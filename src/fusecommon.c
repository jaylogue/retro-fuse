/*
 * Copyright 2021 Jay Logue
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file  Common FUSE filesystem implementation.
 * 
 * This file contains the program's main() function, code to parse options,
 * and the callback functions necessary to implement one of the retro-fuse
 * filesystems.
 */

#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "fusecommon.h"
#include "dskio.h"

#define RETROFUSE_OPT(TEMPLATE, FIELD, VALUE) \
    { TEMPLATE, offsetof(struct retrofuse_config, FIELD), VALUE }

#define RETROFUSE_OPT_KEY_MAPID 100
#define RETROFUSE_OPT_KEY_INITFS 101

static const struct fuse_opt retrofuse_options[] = {
    RETROFUSE_OPT("fssize=%" SCNu32, fssize, 0),
    RETROFUSE_OPT("fsoffset=%" SCNu32, fsoffset, 0),
    FUSE_OPT_KEY("mapuid", RETROFUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapuid=", RETROFUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapgid", RETROFUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapgid=", RETROFUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("initfs", RETROFUSE_OPT_KEY_INITFS),
    FUSE_OPT_KEY("initfs=", RETROFUSE_OPT_KEY_INITFS),
    RETROFUSE_OPT("overwrite", overwrite, 1),
    RETROFUSE_OPT("-r", readonly, 1),
    RETROFUSE_OPT("ro", readonly, 1),
    RETROFUSE_OPT("rw", readonly, 0),
    RETROFUSE_OPT("-f", foreground, 1),
    RETROFUSE_OPT("--foreground", foreground, 1),
    RETROFUSE_OPT("-d", debug, 1),
    RETROFUSE_OPT("--debug", debug, 1),
    RETROFUSE_OPT("-h", showhelp, 1),
    RETROFUSE_OPT("--help", showhelp, 1),
    RETROFUSE_OPT("-V", showversion, 1),
    RETROFUSE_OPT("--version", showversion, 1),

    /* retain these in the fuse_args list to that they
      can be parsed by the FUSE code. */
    FUSE_OPT_KEY("-r", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("ro", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("rw", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("--debug", FUSE_OPT_KEY_KEEP),

    FUSE_OPT_END
};

const char *retrofuse_cmdname;

static int retrofuse_parsemapopt(const char *arg)
{
    long hostid, fsid;
    char idtype, *idname;
    int res;
    int matchend = -1;

    res = sscanf(arg, " map%cid = %ld : %ld %n", &idtype, &hostid, &fsid, &matchend);
    if (res != 3 || arg[matchend] != 0) {
        fprintf(stderr, "%s: ERROR: invalid user/group id mapping option: -o %s\nExpected -o map%cid=<host-id>:<fs-id>\n", 
                retrofuse_cmdname, arg, idtype);
        return -1;
    }

    idname = (idtype == 'u') ? "user" : "group";

    if (hostid < 0 || hostid > INT32_MAX) {
        fprintf(stderr, "%s: ERROR: invalid %s id mapping option: -o %s\nHost %cid value out of range\n", 
                retrofuse_cmdname, idname, arg, idtype);
        return -1;
    }

    if (fsid < 0 || fsid > retrofuse_maxuidgid) {
        fprintf(stderr, "%s: ERROR: invalid %s id mapping option: -o %s\nFilesystem %cid value out of range (expected 0-%d)\n", 
                retrofuse_cmdname, idname, arg, idtype, retrofuse_maxuidgid);
        return -1;
    }

    if (idtype == 'u')
        res = retrofuse_adduidmap((uid_t)hostid, (uint32_t)fsid);
    else
        res = retrofuse_addgidmap((gid_t)hostid, (uint32_t)fsid);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: failed to add %s id mapping: %s\n", 
                retrofuse_cmdname, idname, strerror(-res));
        return -1;
    }

    return 0;
}

static int retrofuse_parseopt(void* data, const char* arg, int key, struct fuse_args* args)
{
    struct retrofuse_config *cfg = (struct retrofuse_config *)data;

    switch (key)
    {
    case FUSE_OPT_KEY_OPT:
        return 1;
    case RETROFUSE_OPT_KEY_MAPID:
        return retrofuse_parsemapopt(arg);
    case RETROFUSE_OPT_KEY_INITFS:
        return retrofuse_parseinitfsopt(cfg, arg);
    case FUSE_OPT_KEY_NONOPT:
        if (cfg->dskfilename == NULL) {
            cfg->dskfilename = arg;
            return 0;
        }
        if (cfg->mountpoint == NULL) {
            cfg->mountpoint = arg;
            return 0;
        }
        fprintf(stderr, "%s: ERROR: Unexpected argument: %s\n", retrofuse_cmdname, arg);
        return -1;
    default:
        fprintf(stderr, "%s: ERROR: Invalid option key: %d\n", retrofuse_cmdname, key);
        return -1;
    }
}

int main(int argc, char *argv[])
{
    int res = EXIT_FAILURE;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct retrofuse_config cfg;
    struct fuse_chan *chan = NULL;
    struct fuse *fuse = NULL;
    struct fuse_session *session = NULL;
    int sighandlersset = 0; 

    memset(&cfg, 0, sizeof(cfg));

    retrofuse_cmdname = strrchr(argv[0], '/');
    if (retrofuse_cmdname == NULL)
        retrofuse_cmdname = argv[0];
    else
        retrofuse_cmdname++;

    /* parse filesystem-specific options and store results in cfg.
       other options are parsed by fuse_mount() and fuse_new() below. */
    if (fuse_opt_parse(&args, &cfg, retrofuse_options, retrofuse_parseopt) == -1)
        goto exit;

    if (cfg.showhelp) {
        retrofuse_showhelp();
        res = EXIT_SUCCESS;
        goto exit;
    }

    if (cfg.showversion) {
        printf("retro-fuse version: %d\n", RETROFUSE_VERSION);
        printf("FUSE library version: %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
        res = EXIT_SUCCESS;
        goto exit;
    }

    if (cfg.dskfilename == NULL) {
        fprintf(stderr, "%s: Please supply the name of the device or disk image to be mounted\n", retrofuse_cmdname);
        goto exit;
    }

    if (cfg.mountpoint == NULL) {
        fprintf(stderr, "%s: Please supply the mount point\n", retrofuse_cmdname);
        goto exit;
    }

    /* Force foreground operation is debugging output requested */
    if (cfg.debug) {
        cfg.foreground = 1;
    }

    /* if requested, call filesystem-specific code to initialize a new filesystem. */
    if (cfg.initfs) {

        /* verify the configuration parameters for initfs are acceptable. */
        if (retrofuse_checkinitfsconfig(&cfg) != 0)
            goto exit;

        /* create a new disk image file or open the underlying block device */
        {
            off_t dsksize = retrofuse_fsblktodskblk(cfg.fssize);
            off_t dskoffset = retrofuse_fsblktodskblk(cfg.fsoffset);
            res = dsk_open(cfg.dskfilename, dsksize, dskoffset, 1, cfg.overwrite, 0);
            if (res != 0) {
                if (res == -EEXIST)
                    fprintf(stderr, 
                            "%s: ERROR: Filesystem image file exists.\n"
                            "To prevent accidents, -o initfs will not overwrite an existing image file.\n"
                            "Use the -o overwrite option to force overwriting.\n",
                            retrofuse_cmdname);
                else if (res == -EINVAL)
                    fprintf(stderr,
                            "%s: ERROR: Missing -o fssize option.\n"
                            "The size of the filesystem must be specified when using -o initfs with an image file.\n",
                            retrofuse_cmdname);
                else
                    fprintf(stderr, "%s: ERROR: Failed to open disk/image file: %s\n", retrofuse_cmdname, strerror(-res));
                return -1;
            }
        }

        /* if the filesystem size wasn't specified on the command line... */
        if (cfg.fssize == 0) {

            /* derrive the filesystem size based on the size of the underlying block device. */
            cfg.fssize = retrofuse_dskblktofsblk(dsk_getsize());

            /* double check that the configuration parameters are sane given the final
             * filesystem size. */
            if (retrofuse_checkinitfsconfig(&cfg) != 0)
                goto exit;
        }

        /* initialize a new filesystem with the specified parameters. */
        if (retrofuse_initfs(&cfg) != 0)
            goto exit;

        /* close the virtual disk containing the new filesystem */
        dsk_close();
    }

    /* verify access to the underlying block device/image file. */
    if (access(cfg.dskfilename, cfg.readonly ? R_OK : R_OK|W_OK) != 0) {
        fprintf(stderr, "%s: ERROR: Unable to access disk/image file: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }

    /* create the FUSE mount point */
    chan = fuse_mount(cfg.mountpoint, &args);
    if (chan == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to mount FUSE filesystem: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }

    /* initialize a new FUSE session */
    fuse = fuse_new(chan, &args, &retrofuse_ops, sizeof(retrofuse_ops), &cfg);
    if (fuse == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to initialize FUSE connection: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }

    /* open the underlying block device/image file */
    {
        off_t dsksize = retrofuse_fsblktodskblk(cfg.fssize);
        off_t dskoffset = retrofuse_fsblktodskblk(cfg.fsoffset);
        int openres = dsk_open(cfg.dskfilename, dsksize, dskoffset, 0, 0, cfg.readonly);
        if (openres != 0) {
            fprintf(stderr, "%s: ERROR: Failed to open disk/image file: %s\n", retrofuse_cmdname, strerror(-openres));
            goto exit;
        }
    }

    /* 'mount' the filesystem */
    if (retrofuse_mount(&cfg) != 0)
        goto exit;

    /* run as a background process, unless told not to */
    if (fuse_daemonize(cfg.foreground) != 0) {
        fprintf(stderr, "%s: ERROR: Failed to daemonize FUSE process: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }

    /* arrange for standard handling of process signals */
    session = fuse_get_session(fuse);
    if (fuse_set_signal_handlers(session) != 0) {
        fprintf(stderr, "%s: ERROR: Failed to set FUSE signal handlers: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }
    sighandlersset = 1;

    /* process filesystem requests until told to stop */
    if (fuse_loop(fuse) != 0) {
        fprintf(stderr, "%s: ERROR: fuse_loop() failed: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }

    res = EXIT_SUCCESS;

exit:
    if (sighandlersset)
        fuse_remove_signal_handlers(session);
    if (chan != NULL)
        fuse_unmount(cfg.mountpoint, chan);
    if (fuse != NULL)
        fuse_destroy(fuse);
    fuse_opt_free_args(&args);
	return res;
}