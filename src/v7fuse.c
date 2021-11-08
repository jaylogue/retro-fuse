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
 * @file  FUSE filesystem implementation.
 * 
 * This file contains the program's main() function, code to parse options,
 * and the callback functions necessary to implement a FUSE filesystem.
 */

#define FUSE_USE_VERSION 26
#define _FILE_OFFSET_BITS 64

#define V7FS_VERSION 4

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
#include <fuse.h>

#include <v7fs.h>
#include <dskio.h>

struct v7fuse_config {
    const char *dskfilename;
    uint32_t fssize;
    uint32_t fsoffset;
    const char *mountpoint;
    int readonly;
    int foreground;
    int debug;
    int showversion;
    int showhelp;
    int initfs;
    uint32_t isize;
    struct flparams flparams;
};

#define V7FUSE_OPT(TEMPLATE, FIELD, VALUE) \
    { TEMPLATE, offsetof(struct v7fuse_config, FIELD), VALUE }

#define V7FUSE_OPT_KEY_MAPID 100
#define V7FUSE_OPT_KEY_INITFS 101

static const struct fuse_opt v7fuse_options[] = {
    V7FUSE_OPT("fssize=%" SCNu32, fssize, 0),
    V7FUSE_OPT("fsoffset=%" SCNu32, fsoffset, 0),
    FUSE_OPT_KEY("mapuid", V7FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapuid=", V7FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapgid", V7FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapgid=", V7FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("initfs", V7FUSE_OPT_KEY_INITFS),
    FUSE_OPT_KEY("initfs=", V7FUSE_OPT_KEY_INITFS),
    V7FUSE_OPT("-r", readonly, 1),
    V7FUSE_OPT("ro", readonly, 1),
    V7FUSE_OPT("rw", readonly, 0),
    V7FUSE_OPT("-f", foreground, 1),
    V7FUSE_OPT("--foreground", foreground, 1),
    V7FUSE_OPT("-d", debug, 1),
    V7FUSE_OPT("--debug", debug, 1),
    V7FUSE_OPT("-h", showhelp, 1),
    V7FUSE_OPT("--help", showhelp, 1),
    V7FUSE_OPT("-V", showversion, 1),
    V7FUSE_OPT("--version", showversion, 1),

    /* retain these in the fuse_args list to that they
      can be parsed by the FUSE code. */
    FUSE_OPT_KEY("-r", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("ro", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("rw", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("--debug", FUSE_OPT_KEY_KEEP),

    FUSE_OPT_END
};

static const char *v7fuse_cmdname;

static int v7fuse_parsemapopt(const char *arg)
{
    long hostid, v7id;
    char idtype;
    int res;
    int matchend = -1;

    res = sscanf(arg, " map%cid = %ld : %ld %n", &idtype, &hostid, &v7id, &matchend);
    if (res != 3 || arg[matchend] != 0) {
        fprintf(stderr, "%s: ERROR: invalid user/group mapping option: -o %s\nExpected -o map%cid=<host-id>:<fs-id>\n", 
                v7fuse_cmdname, arg, idtype);
        return -1;
    }

    if (hostid < 0) {
        fprintf(stderr, "%s: ERROR: invalid user/group mapping option: -o %s\nHost id value out of range (expected >= 0)\n", v7fuse_cmdname, arg);
        return -1;
    }

    if (v7id < 0 || v7id > V7FS_MAX_UID_GID) {
        fprintf(stderr, "%s: ERROR: invalid user/group mapping option: -o %s\nFilesystem id value out of range (expected 0-%d)\n", v7fuse_cmdname, arg, V7FS_MAX_UID_GID);
        return -1;
    }

    idtype = tolower(idtype);
    if (idtype == 'u')
        res = v7fs_adduidmap((uid_t)hostid, (int16_t)v7id);
    else
        res = v7fs_addgidmap((uid_t)hostid, (int16_t)v7id);
    switch (res)
    {
    case 0:
        return 0;
    case -EOVERFLOW:
        fprintf(stderr, "%s: ERROR: too many %s mapping entries\n", v7fuse_cmdname, (idtype == 'u') ? "user" : "group");
        return -1;
    default:
        fprintf(stderr, "%s: ERROR: failed to add %s mapping: %s\n", v7fuse_cmdname, (idtype == 'u') ? "user" : "group", strerror(-res));
        return -1;
    }
}

static int v7fuse_parseinitopt(struct v7fuse_config *cfg, const char *arg)
{
    cfg->initfs = 1;
    cfg->isize = 0;
    cfg->flparams.n = 1;
    cfg->flparams.m = 1;

    int matchend = -1;
    int scanres = sscanf(arg, " initfs %n= %" SCNu32 " %n: %" SCNu16 " : %" SCNu16 " %n",
                         &matchend, &cfg->isize, &matchend, &cfg->flparams.n, &cfg->flparams.m, &matchend);
    /* match -oinitfs */
    if (scanres == EOF && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<isize> */
    if (scanres == 1 && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<isize>:<n>:<m> */
    if (scanres == 3 && arg[matchend] == 0) {
        if (cfg->flparams.n < 1 || cfg->flparams.n > V7FS_MAXFN) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter n must be between 1 and %d\n", v7fuse_cmdname, V7FS_MAXFN);
            return -1;
        }
        if (cfg->flparams.m < 1 || cfg->flparams.m > cfg->flparams.n) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter m must be between 1 and the value given for n (%" PRIu16 ")\n", v7fuse_cmdname, cfg->flparams.n);
            return -1;
        }
        return 0;
    }
    fprintf(stderr, "%s: ERROR: invalid initfs option: expected -o initfs=<isize>[:<n>:<m>]\n", v7fuse_cmdname);
    return -1;
}

static int v7fuse_parseopt(void* data, const char* arg, int key, struct fuse_args* args)
{
    struct v7fuse_config *cfg = (struct v7fuse_config *)data;

    switch (key)
    {
    case FUSE_OPT_KEY_OPT:
        return 1;
    case V7FUSE_OPT_KEY_MAPID:
        return v7fuse_parsemapopt(arg);
    case V7FUSE_OPT_KEY_INITFS:
        return v7fuse_parseinitopt(cfg, arg);
    case FUSE_OPT_KEY_NONOPT:
        if (cfg->dskfilename == NULL) {
            cfg->dskfilename = arg;
            return 0;
        }
        if (cfg->mountpoint == NULL) {
            cfg->mountpoint = arg;
            return 0;
        }
        fprintf(stderr, "%s: ERROR: Unexpected argument: %s\n", v7fuse_cmdname, arg);
        return -1;
    default:
        fprintf(stderr, "%s: ERROR: Invalid option key: %d\n", v7fuse_cmdname, key);
        return -1;
    }
}

static void v7fuse_showhelp()
{
    printf(
        "usage: %s [options] <device-or-image-file> <mount-point>\n"
        "\n"
        "OPTIONS\n"
        "  -o ro\n"
        "        Mount the filesystem in read-only mode.\n"
        "\n"
        "  -o rw\n"
        "        Mount the filesystem in read-write mode.  This is the default.\n"
        "\n"
        "  -o allow_root\n"
        "        Allow the root user to access the filesystem, in addition to the\n"
        "        mounting user.  Mutually exclusive with the allow_other option.\n"
        "\n"
        "  -o allow_other\n"
        "        Allow any user to access the filesystem, including root and the\n"
        "        mounting user.  Mutually exclusive with the allow_root option.\n"
        "\n"
        "  -o mapuid=<host-uid>:<fs-uid>\n"
        "  -o mapgid=<host-gid>:<fs-gid>\n"
        "        Map a particular user or group id on the host system to different\n"
        "        id on the mounted filesystem. The id mapping applies both ways.\n"
        "        Specifically, the uid/gid on the host is mapped to the corresponding\n"
        "        id for the purpose of access control checking, and when the id is\n"
        "        stored in an inode. Conversely, the filesystem id is mapped to the\n"
        "        host system id whenever a file is stat()ed or a directory is read.\n"
        "        Multiple map options may be given, up to a limit of 100.\n"
        "\n"
        "  -o fssize=<blocks>\n"
        "        The size of the filesystem, in 512-byte blocks. This is used to\n"
        "        constrain I/O on the underlying device/image file.  Defaults to the\n"
        "        size given in the filesystem superblock.\n"
        "\n"
        "  -o fsoffset=<blocks>\n"
        "        Offset into the device/image at which the filesystem starts, in\n"
        "        512-byte blocks.  Defaults to 0.\n"
        "\n"
        "  -o initfs\n"
        "  -o initfs=<isize>\n"
        "  -o initfs=<isize>:<n>:<m>\n"
        "        Create an empty filesystem on the underlying device/image file before\n"
        "        mounting.  When using the initfs option on an image file the size of\n"
        "        the filesystem must be specified via the -ofssize option.\n"
        "\n"
        "        isize gives the size of the filesystem's inode table in 512-byte\n"
        "        blocks (8 inodes per block).  If not specified, or if 0 is given,\n"
        "        an appropriate inode table size is computed automatically from the size\n"
        "        of the filesystem, using the logic found in the original v7 mkfs command.\n"
        "\n"
        "        n and m are the interleave parameters for the initial free block list.\n"
        "        If not specified, the values n=500,m=3 are used, as per the original v7\n"
        "        mkfs command.  Specify n=1,m=1 for no interleave.\n"
        "\n"
        "  -o <mount-options>\n"
        "        Comma-separated list of standard mount options. See man 8 mount\n"
        "        for further details.\n"
        "\n"
        "  -o <fuse-options>\n"
        "        Comma-separated list of FUSE mount options. see man 8 mount.fuse\n"
        "        for further details.\n"
        "\n"
        "  -f\n"
        "  --foreground\n"
        "        Run in foreground (useful for debugging).\n"
        "\n"
        "  -d\n"
        "  --debug\n"
        "        Enable debug output to stderr (implies -f)\n"
        "\n"
        "  -V\n"
        "  --version\n"
        "        Print version information\n"
        "\n"
        "  -h\n"
        "  --help\n"
        "        Print this help message.\n",
        v7fuse_cmdname);
}

static void v7fuse_showversion()
{
    printf("v7fs version: %d\n", V7FS_VERSION);
    printf("FUSE library version: %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
}

static void v7fuse_setfscontext()
{
    struct fuse_context *context = fuse_get_context();

    /* Set the real and effective uid/gid for v7 filesystem operations
       to the values from the FUSE client process.  */
    v7fs_setreuid(context->uid, context->uid);
    v7fs_setregid(context->gid, context->gid);
}

static void * v7fuse_init(struct fuse_conn_info * conn)
{
    /* Update FUSE configuration based on capabilities of filesystem. */
    conn->want = (conn->capable & (FUSE_CAP_ATOMIC_O_TRUNC|FUSE_CAP_EXPORT_SUPPORT|FUSE_CAP_BIG_WRITES));
    conn->async_read = 0;

    return NULL;
}

static void v7fuse_destroy(void * userdata)
{
    v7fs_shutdown();
    dsk_close();
}

static int v7fuse_getattr(const char *pathname, struct stat *statbuf)
{
    v7fuse_setfscontext();
    return v7fs_stat(pathname, statbuf);
}

static int v7fuse_access(const char *pathname, int mode)
{
    v7fuse_setfscontext();
    return v7fs_access(pathname, mode);
}

struct v7fuse_adddir_context {
    void *buf;
    fuse_fill_dir_t filler;
};

static int v7fuse_filldir(const char *entryname, const struct stat *statbuf, void *_context)
{
    struct v7fuse_adddir_context * context = (struct v7fuse_adddir_context *)_context;
    context->filler(context->buf, entryname, statbuf, 0);
    return 0;
}

static int v7fuse_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct v7fuse_adddir_context context = {
        .buf = buf,
        .filler = filler
    };
    v7fuse_setfscontext();
    return v7fs_enumdir(pathname, v7fuse_filldir, &context);
}

static int v7fuse_open(const char *pathname, struct fuse_file_info *fi)
{
    v7fuse_setfscontext();
    int res = v7fs_open(pathname, fi->flags, 0);
    if (res < 0)
        return res;
    fi->fh = (uint64_t)res;
    return 0;
}

static int v7fuse_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    v7fuse_setfscontext();
    return v7fs_pread((int)fi->fh, buf, size, offset);
}

static int v7fuse_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    v7fuse_setfscontext();
    return v7fs_pwrite((int)fi->fh, buf, size, offset);
}

static int v7fuse_truncate(const char *pathname, off_t length)
{
    v7fuse_setfscontext();
    return v7fs_truncate(pathname, length);
}

static int v7fuse_flush(const char *pathname, struct fuse_file_info *fi)
{
    v7fuse_setfscontext();
    v7fs_sync();
    return 0;
}

static int v7fuse_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
    v7fuse_setfscontext();
    v7fs_sync();
    return 0;
}

static int v7fuse_release(const char *pathname, struct fuse_file_info *fi)
{
    v7fuse_setfscontext();
    return v7fs_close((int)fi->fh);
}

static int v7fuse_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    v7fuse_setfscontext();
    return v7fs_mknod(pathname, mode, dev);
}

static int v7fuse_mkdir(const char *pathname, mode_t mode)
{
    v7fuse_setfscontext();
    return v7fs_mkdir(pathname, mode);
}

static int v7fuse_rmdir(const char *pathname)
{
    v7fuse_setfscontext();
    return v7fs_rmdir(pathname);
}

static int v7fuse_link(const char *oldpath, const char *newpath)
{
    v7fuse_setfscontext();
    return v7fs_link(oldpath, newpath);
}

static int v7fuse_unlink(const char *pathname)
{
    v7fuse_setfscontext();
    return v7fs_unlink(pathname);
}

static int v7fuse_rename(const char *oldpath, const char *newpath)
{
    v7fuse_setfscontext();
    return v7fs_rename(oldpath, newpath);
}

static int v7fuse_chmod(const char *pathname, mode_t mode)
{
    v7fuse_setfscontext();
    return v7fs_chmod(pathname, mode);
}

static int v7fuse_chown(const char *pathname, uid_t uid, gid_t gid)
{
    v7fuse_setfscontext();
    return v7fs_chown(pathname, uid, gid);
}

static int v7fuse_utimens(const char *pathname, const struct timespec tv[2])
{
    v7fuse_setfscontext();
    return v7fs_utimens(pathname, tv);
}

static int v7fuse_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    v7fuse_setfscontext();
    return v7fs_statfs(pathname, statvfsbuf);
}

static const struct fuse_operations v7fuse_ops = 
{
    .init       = v7fuse_init,
    .destroy    = v7fuse_destroy,
    .getattr    = v7fuse_getattr,
    .access     = v7fuse_access,
    .readdir    = v7fuse_readdir,
    .open       = v7fuse_open,
    .read       = v7fuse_read,
    .write      = v7fuse_write,
    .truncate   = v7fuse_truncate,
    .flush      = v7fuse_flush,
    .fsync      = v7fuse_fsync,
    .release    = v7fuse_release,
    .mknod      = v7fuse_mknod,
    .mkdir      = v7fuse_mkdir,
    .rmdir      = v7fuse_rmdir,
    .link       = v7fuse_link,
    .unlink     = v7fuse_unlink,
    .rename     = v7fuse_rename,
    .chmod      = v7fuse_chmod,
    .chown      = v7fuse_chown,
    .utimens    = v7fuse_utimens,
    .statfs     = v7fuse_statfs,

    .flag_utime_omit_ok = 1
};

int main(int argc, char *argv[])
{
    int res = EXIT_FAILURE;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct v7fuse_config cfg;
    struct fuse_chan *chan = NULL;
    struct fuse *fuse = NULL;
    struct fuse_session *session = NULL;
    int sighandlersset = 0; 

    memset(&cfg, 0, sizeof(cfg));

    v7fuse_cmdname = rindex(argv[0], '/');
    if (v7fuse_cmdname == NULL)
        v7fuse_cmdname = argv[0];
    else
        v7fuse_cmdname++;

    /* parse options specific to v7fs and store results in cfg.
       other options are parsed by fuse_mount() and fuse_new() below. */
    if (fuse_opt_parse(&args, &cfg, v7fuse_options, v7fuse_parseopt) == -1)
        goto exit;

    if (cfg.showhelp) {
        v7fuse_showhelp();
        goto exit;
    }

    if (cfg.showversion) {
        v7fuse_showversion();
        goto exit;
    }

    if (cfg.debug) {
        cfg.foreground = 1;
    }

    if (cfg.dskfilename == NULL) {
        fprintf(stderr, "%s: Please supply the name of the device or disk image to be mounted\n", v7fuse_cmdname);
        goto exit;
    }

    if (cfg.mountpoint == NULL) {
        fprintf(stderr, "%s: Please supply the mount point\n", v7fuse_cmdname);
        goto exit;
    }

    /* if initializing a new filesystem ... */
    if (cfg.initfs) {

        /* verify the specified filesystem size is sane and does not exceed the capabilities of the v7 code. */
        if (cfg.fssize > 0 && cfg.fssize < V7FS_MIN_FS_SIZE) {
            fprintf(stderr, "%s: ERROR: Specified filesystem size is too small (must be >= %" PRIu32 " blocks).\n", v7fuse_cmdname, V7FS_MIN_FS_SIZE);
            goto exit;
        }
        if (cfg.fssize > V7FS_MAX_FS_SIZE) {
            fprintf(stderr, "%s: ERROR: Specified filesystem size is too big (must be <= %" PRIu32 " blocks).\n", v7fuse_cmdname, V7FS_MAX_FS_SIZE);
            goto exit;
        }

        /* verify the specified inode table size. */
        if (cfg.isize > V7FS_MAX_ITABLE_SIZE) {
            fprintf(stderr, "%s: ERROR: Specified inode table size is too big (must be <= %" PRIu32 " blocks).\n", v7fuse_cmdname, V7FS_MAX_ITABLE_SIZE);
            goto exit;
        }

        /* create a new disk image file or open the underlying block device */
        res = dsk_open(cfg.dskfilename, cfg.fssize, cfg.fsoffset, 1, 0);
        if (res != 0) {
            if (res == -EEXIST)
                fprintf(stderr, "%s: ERROR: Filesystem image file exists. To prevent accidents, the filesystem image file must NOT exist when using -oinitfs.\n", v7fuse_cmdname);
            else if (res == -EINVAL)
                fprintf(stderr, "%s: ERROR: Missing -o fssize option. The size of the filesystem must be specified when using -oinitfs\n", v7fuse_cmdname);
            else
                fprintf(stderr, "%s: ERROR: Failed to open disk/image file: %s\n", v7fuse_cmdname, strerror(-res));
            res = EXIT_FAILURE;
            goto exit;
        }

        /* get the final filesystem size.  this value may have been determined by the 
         * the size of the underlying block device. */
        off_t fssize = dsk_getsize();

        /* double check the filesystem size. */
        if (fssize < V7FS_MIN_FS_SIZE) {
            fprintf(stderr, "%s: ERROR: Filesystem size is too small. Use -o fssize option to specify a size >= %" PRIu32 " blocks.\n", v7fuse_cmdname, V7FS_MIN_FS_SIZE);
            goto exit;
        }
        if (fssize > V7FS_MAX_FS_SIZE) {
            fprintf(stderr, "%s: ERROR: Filesystem size is too big. Use -o fssize option to specify a size <= %" PRIu32 " blocks.\n", v7fuse_cmdname, V7FS_MAX_FS_SIZE);
            goto exit;
        }

        /* verify the request number of inode blocks is sane relative to the filesystem size. */
        if (cfg.isize > (fssize - 4)) {
            fprintf(stderr, "%s: ERROR: Specified inode table size too big. Must be <= filesystem size - 4 blocks.\n", v7fuse_cmdname);
            goto exit;
        }

        /* initialize the new filesystem with the specified parameters. */
        res = v7fs_mkfs(fssize, cfg.isize, &cfg.flparams);
        if (res != 0) {
            fprintf(stderr, "%s: ERROR: Failed to initialize filesystem: %s\n", v7fuse_cmdname, strerror(-res));
            res = EXIT_FAILURE;
            goto exit;
        }

        /* close the virtual disk containing the new filesystem */
        dsk_close();
    }

    /* verify access to the underlying block device/image file. */
    if (access(cfg.dskfilename, cfg.readonly ? R_OK : R_OK|W_OK) != 0) {
        fprintf(stderr, "%s: ERROR: Unable to access disk/image file: %s\n", v7fuse_cmdname, strerror(errno));
        goto exit;
    }

    /* create the FUSE mount point */
    chan = fuse_mount(cfg.mountpoint, &args);
    if (chan == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to mount FUSE filesystem: %s\n", v7fuse_cmdname, strerror(errno));
        goto exit;
    }

    /* initialize a new FUSE session */
    fuse = fuse_new(chan, &args, &v7fuse_ops, sizeof(v7fuse_ops), &cfg);
    if (fuse == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to initialize FUSE connection: %s\n", v7fuse_cmdname, strerror(errno));
        goto exit;
    }

    /* open the underlying block device/image file */
    res = dsk_open(cfg.dskfilename, cfg.fssize, cfg.fsoffset, 0, cfg.readonly);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to open disk/image file: %s\n", v7fuse_cmdname, strerror(-res));
        res = EXIT_FAILURE;
        goto exit;
    }

    /* 'mount' the v7 filesystem */
    res = v7fs_init(cfg.readonly);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to mount v7 filesystem: %s\n", v7fuse_cmdname, strerror(-res));
        res = EXIT_FAILURE;
        goto exit;
    }

    /* run as a background process, unless told not to */
    if (fuse_daemonize(cfg.foreground) != 0) {
        fprintf(stderr, "%s: ERROR: Failed to daemonize FUSE process: %s\n", v7fuse_cmdname, strerror(errno));
        goto exit;
    }

    /* arrange for standard handling of process signals */
    session = fuse_get_session(fuse);
    if (fuse_set_signal_handlers(session) != 0) {
        fprintf(stderr, "%s: ERROR: Failed to set FUSE signal handlers: %s\n", v7fuse_cmdname, strerror(errno));
        goto exit;
    }
    sighandlersset = 1;

    /* process filesystem requests until told to stop */
    if (fuse_loop(fuse) != 0) {
        fprintf(stderr, "%s: ERROR: fuse_loop() failed: %s\n", v7fuse_cmdname, strerror(errno));
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