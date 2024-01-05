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
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "fusecommon.h"
#include "dskio.h"
#include "fstype.h"

#define RETROFUSE_OPT_KEY_MKFS          100
#define RETROFUSE_OPT_KEY_CONTAINER     101
#define RETROFUSE_OPT_KEY_DSKSIZE       102
#define RETROFUSE_OPT_KEY_DSKOFFSET     103
#define RETROFUSE_OPT_KEY_FSSIZE        104
#define RETROFUSE_OPT_KEY_FSOFFSET      105

static const struct fuse_opt retrofuse_fuseopts_common[];

static int retrofuse_parseopt_common(void *data, const char *arg, int key, struct fuse_args *args);
static int retrofuse_checkconfig_common(struct retrofuse_config *cfg);

int main(int argc, char *argv[])
{
    int res = EXIT_FAILURE;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct retrofuse_config cfg;
    struct fuse_chan *chan = NULL;
    struct fuse *fuse = NULL;
    struct fuse_session *session = NULL;
    int sighandlersset = 0;

    /* setup default configuration values */
    memset(&cfg, 0, sizeof(cfg));
    dsk_initconfig(&cfg.dskcfg);
    retrofuse_initconfig(&cfg);

    /* parse filesystem-specific options and store results in cfg.
     * any options that are not filesystem-specified are left in the args list. */
    if (fuse_opt_parse(&args, &cfg, retrofuse_fuseopts, retrofuse_parseopt) == -1)
        goto exit;

    /* parse common retro-fuse options and store results in cfg. */
    if (fuse_opt_parse(&args, &cfg, retrofuse_fuseopts_common, retrofuse_parseopt_common) == -1)
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

    /* validate the supplied configuration parameters */
    res = retrofuse_checkconfig_common(&cfg);
    if (res != 0) {
        goto exit;
    }
    res = retrofuse_checkconfig(&cfg);
    if (res != 0) {
        goto exit;
    }

    /* open/create the underlying disk device/container
     *
     * if the mkfs parameter isn't given, simply open the specified disk
     * file/block device and fail if it doesn't exist.
     * 
     * if mkfs is given, and the specified disk file doesn't exist, then
     * create and initialize a new disk container file with the given name.
     * 
     * if mkfs is given, and the specified file name refers to an existing
     * disk container file, then open the container file, but only if the
     * overwrite parameter is NOT specified.  if the overwrite parameter
     * is specified, delete the existing container file and create and
     * initialize a new container file with the same name.
     * 
     * if mkfs is given, and the specified file name refers to a block
     * device, then open the block device. if the overwrite parameter is
     * specified, re-write the disk layout information stored on the
     * device (disk geometry, partition table, etc.).
     */
    {
        bool fileexists = (access(cfg.dskfilename, F_OK) == 0);
        bool create = cfg.mkfs && (!fileexists || cfg.overwrite);

        res = dsk_open(cfg.dskfilename, &cfg.dskcfg, create, true, cfg.readonly);
        if (res != 0) {
            fprintf(stderr, "%s: ERROR: Failed to open disk device/container file: %s\n", retrofuse_cmdname, strerror(-res));
            goto exit;
        }
    }

    /* if requested, call filesystem-specific code to initialize a new filesystem. */
    if (cfg.mkfs) {
        res = retrofuse_mkfs(&cfg);
        if (res != 0) {
            goto exit;
        }
    }

    /* create the FUSE mount point */
    chan = fuse_mount(cfg.mountpoint, &args);
    if (chan == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to mount FUSE filesystem: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }

    /* initialize a new FUSE session */
    fuse = fuse_new(chan, &args, &retrofuse_fuseops, sizeof(retrofuse_fuseops), &cfg);
    if (fuse == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to initialize FUSE connection: %s\n", retrofuse_cmdname, strerror(errno));
        goto exit;
    }

    /* mount the actual filesystem */
    if (retrofuse_mountfs(&cfg) != 0)
        goto exit;

    /* run as a background process, unless told not to */
    if (fuse_daemonize(cfg.foreground || cfg.debug) != 0) {
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

exit:
    if (sighandlersset)
        fuse_remove_signal_handlers(session);
    if (chan != NULL)
        fuse_unmount(cfg.mountpoint, chan);
    if (fuse != NULL)
        fuse_destroy(fuse);
    dsk_close();
    fuse_opt_free_args(&args);
	return res == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

int retrofuse_parseopt_dskcontainer(const char *arg, int *outval)
{
    const char *val = strchr(arg, '=') + 1;

    if (strcasecmp(val, "img") == 0)          { *outval = dsk_container_imagefile; }
    else if (strcasecmp(val, "dsk") == 0)     { *outval = dsk_container_imagefile; }
    else if (strcasecmp(val, "dev") == 0)     { *outval = dsk_container_dev; }
    else if (strcasecmp(val, "hdv") == 0)     { *outval = dsk_container_hdv; }
    else if (strcasecmp(val, "vhd") == 0)     { *outval = dsk_container_vhd; }
    else if (strcasecmp(val, "imd") == 0)     { *outval = dsk_container_imd; }
    else if (strcasecmp(val, "drem") == 0)    { *outval = dsk_container_drem; }
    else {
        fprintf(stderr,
                "%s: ERROR: unknown disk container type: %s\n"
                "Expected one of: img, dsk, dev, hdv, vhd, imd or drem\n",
                retrofuse_cmdname, val);
        return -1;
    }
    return 0;
}

int retrofuse_parseopt_mkfs(const char *arg, struct retrofuse_mkfsconfig *mkfscfg)
{
    int matchend = -1;
    int scanres = sscanf(arg, " mkfs %n= %" SCNu32 " %n: %" SCNu16 " : %" SCNu16 " %n",
                         &matchend, &mkfscfg->isize, &matchend, &mkfscfg->n, &mkfscfg->m, &matchend);
    /* match -omkfs */
    if (scanres == EOF && arg[matchend] == 0)
        return 0;
    /* match -omkfs=<isize> */
    if (scanres == 1 && arg[matchend] == 0)
        return 0;
    /* match -omkfs=<isize>:<n>:<m> */
    if (scanres == 3 && arg[matchend] == 0)
        return 0;
    fprintf(stderr, "%s: ERROR: invalid mkfs option: -o %s\nExpected -o mkfs=<isize>[:<n>:<m>]\n", retrofuse_cmdname, arg);
    return -1;
}

int retrofuse_parseopt_idmap(const char *arg, uint32_t maxid, 
                             int (*addidmap)(uid_t hostid, uint32_t fsid))
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

    if (fsid < 0 || fsid > maxid) {
        fprintf(stderr, "%s: ERROR: invalid %s id mapping option: -o %s\nFilesystem %cid value out of range (expected 0-%d)\n", 
                retrofuse_cmdname, idname, arg, idtype, maxid);
        return -1;
    }

    res = addidmap((uid_t)hostid, (uint32_t)fsid);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: failed to add %s id mapping: %s\n", 
                retrofuse_cmdname, idname, strerror(-res));
        return -1;
    }

    return 0;
}

int retrofuse_parseopt_size(const char *arg, const char *optname, uint32_t unitSize, off_t *outval)
{
    int res, matchend;
    char suffix;
    uint64_t v, m;

    res = sscanf(arg, "%*[A-Za-z-]=%" SCNu64 " %1c %n", &v, &suffix, &matchend);
    if (res < 1) {
        goto syntaxerr;
    }

    if (res == 2) {
        if (arg[matchend] != 0) {
            goto syntaxerr;
        }

        switch (toupper(suffix)) {
        case 'K': m = 1024;           break;
        case 'M': m = 1024*1024;      break;
        case 'G': m = 1024*1024*1024; break;
        default:
            goto syntaxerr;
        }

        if (v > INT64_MAX / m) {
            fprintf(stderr, "%s: ERROR: value for %s option too large\n",
                    retrofuse_cmdname, optname);
            return -1;
        }

        v = (v * m) / unitSize;
    }

    *outval = (off_t)v;
    
    return 0;

syntaxerr:
    if (unitSize == 1) {
        fprintf(stderr,
                "%s: ERROR: invalid option: %s\n"
                "Expected -o %s=<bytes>[<multiplier>], where <multiplier> is one of:\n"
                "  K=1024, M=1024*1024, G=1024*1024*1024\n",
                retrofuse_cmdname, arg, optname);
    }
    else {
        fprintf(stderr,
                "%s: ERROR: invalid option: %s\n"
                "Expected -o %s=<blocks> or -o %s=<bytes><multiplier>, where <multiplier> is one of:\n"
                "  K=1024, M=1024*1024, G=1024*1024*1024\n",
                retrofuse_cmdname, arg, optname, optname);
    }

    return -1;
}

/* ========== Private Functions/Data ========== */

/** Option description table for parsing options that are common
 *  to all filesystem handlers.
 */
static const struct fuse_opt retrofuse_fuseopts_common[] = {
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
    FUSE_OPT_KEY("mkfs", RETROFUSE_OPT_KEY_MKFS),
    FUSE_OPT_KEY("mkfs=", RETROFUSE_OPT_KEY_MKFS),
    RETROFUSE_OPT("overwrite", overwrite, 1),
    FUSE_OPT_KEY("container=", RETROFUSE_OPT_KEY_CONTAINER),
    FUSE_OPT_KEY("dsksize=", RETROFUSE_OPT_KEY_DSKSIZE),
    FUSE_OPT_KEY("dskoffset=", RETROFUSE_OPT_KEY_DSKOFFSET),
    RETROFUSE_OPT("cylinders=%d", dskcfg.cylinders, 0),
    RETROFUSE_OPT("cyls=%d", dskcfg.cylinders, 0),
    RETROFUSE_OPT("heads=%d", dskcfg.heads, 0),
    RETROFUSE_OPT("sectors=%d", dskcfg.sectors, 0),
    RETROFUSE_OPT("secs=%d", dskcfg.sectors, 0),
    RETROFUSE_OPT("interleave=%d", dskcfg.interleave, 0),
    RETROFUSE_OPT("partnum=%d", dskcfg.partnum, 0),
    RETROFUSE_OPT("parttype=%d", dskcfg.parttype, 0),
    FUSE_OPT_KEY("fssize=", RETROFUSE_OPT_KEY_FSSIZE),
    FUSE_OPT_KEY("fsoffset=", RETROFUSE_OPT_KEY_FSOFFSET),

    /* retain these in the fuse_args list so that they
     * can be parsed by the FUSE code. */
    FUSE_OPT_KEY("-r", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("ro", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("rw", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("--debug", FUSE_OPT_KEY_KEEP),

    FUSE_OPT_END
};

static int retrofuse_parseopt_common(void *data, const char *arg, int key, struct fuse_args *args)
{
    struct retrofuse_config *cfg = (struct retrofuse_config *)data;

    switch (key)
    {
    case FUSE_OPT_KEY_OPT:
        return 1;
    case RETROFUSE_OPT_KEY_MKFS:
        // TODO: fail if mkfs already set???
        cfg->mkfs = 1;
        return retrofuse_parseopt_mkfs(arg, &cfg->mkfscfg);
    case RETROFUSE_OPT_KEY_CONTAINER:
        return retrofuse_parseopt_dskcontainer(arg, &cfg->dskcfg.container);
    case RETROFUSE_OPT_KEY_DSKSIZE:
        return retrofuse_parseopt_size(arg, "dsksize", DSK_BLKSIZE, &cfg->dskcfg.size);
    case RETROFUSE_OPT_KEY_DSKOFFSET:
        return retrofuse_parseopt_size(arg, "dskoffset", 1, &cfg->dskcfg.offset);
    case RETROFUSE_OPT_KEY_FSSIZE:
        return retrofuse_parseopt_size(arg, "fssize", fs_blocksize(cfg->fstype), &cfg->dskcfg.fssize);
    case RETROFUSE_OPT_KEY_FSOFFSET:
        return retrofuse_parseopt_size(arg, "fsoffset", DSK_BLKSIZE, &cfg->dskcfg.fsoffset);
    case FUSE_OPT_KEY_NONOPT:
        if (cfg->dskfilename == NULL) {
            cfg->dskfilename = strdup(arg);
            return 0;
        }
        if (cfg->mountpoint == NULL) {
            cfg->mountpoint = strdup(arg);
            return 0;
        }
        fprintf(stderr, "%s: ERROR: Unexpected argument: %s\n", retrofuse_cmdname, arg);
        return -1;
    default:
        fprintf(stderr, "%s: ERROR: Invalid option key: %d\n", retrofuse_cmdname, key);
        return -1;
    }
}

static int retrofuse_checkconfig_common(struct retrofuse_config *cfg)
{
    if (cfg->dskfilename == NULL) {
        fprintf(stderr, "%s: Please supply the name of the device or disk image to be mounted\n", retrofuse_cmdname);
        return -1;
    }

    if (cfg->mountpoint == NULL) {
        fprintf(stderr, "%s: Please supply the mount point\n", retrofuse_cmdname);
        return -1;
    }

    if (cfg->readonly && cfg->mkfs) {
        fprintf(stderr, "%s: Cannot request read-only access when -o mkfs option given\n", retrofuse_cmdname);
        return -1;
    }

    return 0;
}
