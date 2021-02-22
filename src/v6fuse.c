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

#define V6FS_VERSION 1

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

#include <v6fs.h>
#include <dskio.h>

struct v6fuse_config {
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
    uint16_t isize;
    struct flparams flparams;
};

#define V6FUSE_OPT(TEMPLATE, FIELD, VALUE) \
    { TEMPLATE, offsetof(struct v6fuse_config, FIELD), VALUE }

#define V6FUSE_OPT_KEY_MAPID 100
#define V6FUSE_OPT_KEY_INITFS 101

static const struct fuse_opt v6fuse_options[] = {
    V6FUSE_OPT("fssize=%u", fssize, 0),
    V6FUSE_OPT("fsoffset=%llu", fsoffset, 0),
    FUSE_OPT_KEY("mapuid", V6FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapuid=", V6FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapgid", V6FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("mapgid=", V6FUSE_OPT_KEY_MAPID),
    FUSE_OPT_KEY("initfs", V6FUSE_OPT_KEY_INITFS),
    FUSE_OPT_KEY("initfs=", V6FUSE_OPT_KEY_INITFS),
    V6FUSE_OPT("-r", readonly, 1),
    V6FUSE_OPT("ro", readonly, 1),
    V6FUSE_OPT("rw", readonly, 0),
    V6FUSE_OPT("-f", foreground, 1),
    V6FUSE_OPT("--foreground", foreground, 1),
    V6FUSE_OPT("-d", debug, 1),
    V6FUSE_OPT("--debug", debug, 1),
    V6FUSE_OPT("-h", showhelp, 1),
    V6FUSE_OPT("--help", showhelp, 1),
    V6FUSE_OPT("-V", showversion, 1),
    V6FUSE_OPT("--version", showversion, 1),

    /* retain these in the fuse_args list to that they
      can be parsed by the FUSE code. */
    FUSE_OPT_KEY("-r", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("ro", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("rw", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("-d", FUSE_OPT_KEY_KEEP),
    FUSE_OPT_KEY("--debug", FUSE_OPT_KEY_KEEP),

    FUSE_OPT_END
};

static const char *v6fuse_cmdname;

static int v6fuse_parsemapopt(const char *arg)
{
    long hostid, v6id;
    char idtype;
    int res;
    int matchend = -1;

    res = sscanf(arg, " map%cid = %ld : %ld %n", &idtype, &hostid, &v6id, &matchend);
    if (res != 3 || arg[matchend] != 0) {
        fprintf(stderr, "%s: ERROR: invalid user/group mapping option: -o %s\nExpected -o map%cid=<host-id>:<fs-id>\n", 
                v6fuse_cmdname, arg, idtype);
        return -1;
    }

    if (hostid < 0) {
        fprintf(stderr, "%s: ERROR: invalid user/group mapping option: -o %s\nHost id value out of range (expected >= 0)\n", v6fuse_cmdname, arg);
        return -1;
    }

    if (v6id < 0 || v6id > 255) {
        fprintf(stderr, "%s: ERROR: invalid user/group mapping option: -o %s\nFilesystem id value out of range (expected 0-255)\n", v6fuse_cmdname, arg);
        return -1;
    }

    idtype = tolower(idtype);
    if (idtype == 'u')
        res = v6fs_adduidmap((uid_t)hostid, (char)v6id);
    else
        res = v6fs_addgidmap((uid_t)hostid, (char)v6id);
    switch (res)
    {
    case 0:
        return 0;
    case -EOVERFLOW:
        fprintf(stderr, "%s: ERROR: too many %s mapping entries\n", v6fuse_cmdname, (idtype == 'u') ? "user" : "group");
        return -1;
    default:
        fprintf(stderr, "%s: ERROR: failed to add %s mapping: %s\n", v6fuse_cmdname, (idtype == 'u') ? "user" : "group", strerror(-res));
        return -1;
    }
}

static int v6fuse_parseinitopt(struct v6fuse_config *cfg, const char *arg)
{
    cfg->initfs = 1;
    cfg->isize = 0;
    cfg->flparams.n = 1;
    cfg->flparams.m = 1;

    int matchend = -1;
    int scanres = sscanf(arg, " initfs %n= %" SCNu16 " %n: %" SCNu16 " : %" SCNu16 " %n",
                         &matchend, &cfg->isize, &matchend, &cfg->flparams.n, &cfg->flparams.m, &matchend);
    /* match -oinitfs */
    if (scanres == EOF && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<isize> */
    if (scanres == 1 && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<isize>:<n>:<m> */
    if (scanres == 3 && arg[matchend] == 0) {
        if (cfg->flparams.n < 1 || cfg->flparams.n > 100) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter n must be between 1 and 100\n", v6fuse_cmdname);
            return -1;
        }
        if (cfg->flparams.m < 1 || cfg->flparams.m > 32767) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter m must be between 1 and 32767\n", v6fuse_cmdname);
            return -1;
        }
        return 0;
    }
    fprintf(stderr, "%s: ERROR: invalid initfs option: expected -o initfs=<isize>[:<n>:<m>]\n", v6fuse_cmdname);
    return -1;
}

static int v6fuse_parseopt(void* data, const char* arg, int key, struct fuse_args* args)
{
    struct v6fuse_config *cfg = (struct v6fuse_config *)data;

    switch (key)
    {
    case FUSE_OPT_KEY_OPT:
        return 1;
    case V6FUSE_OPT_KEY_MAPID:
        return v6fuse_parsemapopt(arg);
    case V6FUSE_OPT_KEY_INITFS:
        return v6fuse_parseinitopt(cfg, arg);
    case FUSE_OPT_KEY_NONOPT:
        if (cfg->dskfilename == NULL) {
            cfg->dskfilename = arg;
            return 0;
        }
        if (cfg->mountpoint == NULL) {
            cfg->mountpoint = arg;
            return 0;
        }
        fprintf(stderr, "%s: ERROR: Unexpected argument: %s\n", v6fuse_cmdname, arg);
        return -1;
    default:
        fprintf(stderr, "%s: ERROR: Invalid option key: %d\n", v6fuse_cmdname, key);
        return -1;
    }
}

static void v6fuse_showhelp()
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
        "        limit I/O on the underlying device/image file, e.g. in case the\n"
        "        filesystem contains malformed block pointers.  Defaults to the\n"
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
        "        blocks (32 inodes per block).  If not specified, or if 0 is given,\n"
        "        the size of the inode table is computed automatically from the size\n"
        "        of the filesystem.\n"
        "\n"
        "        n and m are the interleave parameters for the initial free block list.\n"
        "        Traditionally, the v6 mkfs command used n=10,m=4 for rp devices, 24,3\n"
        "        for rk devices, and 1,1 (i.e. no interleave) for all other devices.\n"
        "        If not specified, no interleave is used.\n"
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
        v6fuse_cmdname);
}

static void v6fuse_showversion()
{
    printf("v6fs version: %d\n", V6FS_VERSION);
    printf("FUSE library version: %d.%d\n", FUSE_MAJOR_VERSION, FUSE_MINOR_VERSION);
}

static void v6fuse_setfscontext()
{
    struct fuse_context *context = fuse_get_context();

    /* Set the real and effective uid/gid for v6 filesystem operations
       to the values from the FUSE client process.  */
    v6fs_setreuid(context->uid, context->uid);
    v6fs_setregid(context->gid, context->gid);
}

static void * v6fuse_init(struct fuse_conn_info * conn)
{
    /* Update FUSE configuration based on capabilities of filesystem. */
    conn->want = (conn->capable & (FUSE_CAP_ATOMIC_O_TRUNC|FUSE_CAP_EXPORT_SUPPORT|FUSE_CAP_BIG_WRITES));
    conn->async_read = 0;

    return NULL;
}

static void v6fuse_destroy(void * userdata)
{
    v6fs_shutdown();
    dsk_close();
}

static int v6fuse_getattr(const char *pathname, struct stat *statbuf)
{
    v6fuse_setfscontext();
    return v6fs_stat(pathname, statbuf);
}

static int v6fuse_access(const char *pathname, int mode)
{
    v6fuse_setfscontext();
    return v6fs_access(pathname, mode);
}

struct v6fuse_adddir_context {
    void *buf;
    fuse_fill_dir_t filler;
};

static int v6fuse_filldir(const char *entryname, const struct stat *statbuf, void *_context)
{
    struct v6fuse_adddir_context * context = (struct v6fuse_adddir_context *)_context;
    context->filler(context->buf, entryname, statbuf, 0);
    return 0;
}

static int v6fuse_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct v6fuse_adddir_context context = {
        .buf = buf,
        .filler = filler
    };
    v6fuse_setfscontext();
    return v6fs_enumdir(pathname, v6fuse_filldir, &context);
}

static int v6fuse_open(const char *pathname, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    int res = v6fs_open(pathname, fi->flags, 0);
    if (res < 0)
        return res;
    fi->fh = (uint64_t)res;
    return 0;
}

static int v6fuse_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    return v6fs_pread((int)fi->fh, buf, size, offset);
}

static int v6fuse_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    return v6fs_pwrite((int)fi->fh, buf, size, offset);
}

static int v6fuse_truncate(const char *pathname, off_t length)
{
    v6fuse_setfscontext();
    return v6fs_truncate(pathname, length);
}

static int v6fuse_flush(const char *pathname, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    v6fs_sync();
    return 0;
}

static int v6fuse_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    v6fs_sync();
    return 0;
}

static int v6fuse_release(const char *pathname, struct fuse_file_info *fi)
{
    v6fuse_setfscontext();
    return v6fs_close((int)fi->fh);
}

static int v6fuse_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    v6fuse_setfscontext();
    return v6fs_mknod(pathname, mode, dev);
}

static int v6fuse_mkdir(const char *pathname, mode_t mode)
{
    v6fuse_setfscontext();
    return v6fs_mkdir(pathname, mode);
}

static int v6fuse_rmdir(const char *pathname)
{
    v6fuse_setfscontext();
    return v6fs_rmdir(pathname);
}

static int v6fuse_link(const char *oldpath, const char *newpath)
{
    v6fuse_setfscontext();
    return v6fs_link(oldpath, newpath);
}

static int v6fuse_unlink(const char *pathname)
{
    v6fuse_setfscontext();
    return v6fs_unlink(pathname);
}

static int v6fuse_rename(const char *oldpath, const char *newpath)
{
    v6fuse_setfscontext();
    return v6fs_rename(oldpath, newpath);
}

static int v6fuse_chmod(const char *pathname, mode_t mode)
{
    v6fuse_setfscontext();
    return v6fs_chmod(pathname, mode);
}

static int v6fuse_chown(const char *pathname, uid_t uid, gid_t gid)
{
    v6fuse_setfscontext();
    return v6fs_chown(pathname, uid, gid);
}

static int v6fuse_utimens(const char *pathname, const struct timespec tv[2])
{
    v6fuse_setfscontext();
    return v6fs_utimens(pathname, tv);
}

static int v6fuse_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    v6fuse_setfscontext();
    return v6fs_statfs(pathname, statvfsbuf);
}

static const struct fuse_operations v6fuse_ops = 
{
    .init       = v6fuse_init,
    .destroy    = v6fuse_destroy,
    .getattr    = v6fuse_getattr,
    .access     = v6fuse_access,
    .readdir    = v6fuse_readdir,
    .open       = v6fuse_open,
    .read       = v6fuse_read,
    .write      = v6fuse_write,
    .truncate   = v6fuse_truncate,
    .flush      = v6fuse_flush,
    .fsync      = v6fuse_fsync,
    .release    = v6fuse_release,
    .mknod      = v6fuse_mknod,
    .mkdir      = v6fuse_mkdir,
    .rmdir      = v6fuse_rmdir,
    .link       = v6fuse_link,
    .unlink     = v6fuse_unlink,
    .rename     = v6fuse_rename,
    .chmod      = v6fuse_chmod,
    .chown      = v6fuse_chown,
    .utimens    = v6fuse_utimens,
    .statfs     = v6fuse_statfs,

    .flag_utime_omit_ok = 1
};

int main(int argc, char *argv[])
{
	int res = 0;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct v6fuse_config cfg;
    struct fuse_chan *chan = NULL;
    struct fuse *fuse = NULL;
    struct fuse_session *session = NULL;
    int sighandlersset = 0; 

    memset(&cfg, 0, sizeof(cfg));

    v6fuse_cmdname = rindex(argv[0], '/');
    if (v6fuse_cmdname == NULL)
        v6fuse_cmdname = argv[0];
    else
        v6fuse_cmdname++;

    /* parse options specific to v6fs and store results in cfg.
       other options are parsed by fuse_mount() and fuse_new() below. */
    if (fuse_opt_parse(&args, &cfg, v6fuse_options, v6fuse_parseopt) == -1)
        goto exit;

    if (cfg.showhelp) {
        v6fuse_showhelp();
        goto exit;
    }

    if (cfg.showversion) {
        v6fuse_showversion();
        goto exit;
    }

    if (cfg.dskfilename == NULL) {
        fprintf(stderr, "%s: Please supply the name of the device or disk image to be mounted\n", v6fuse_cmdname);
        goto exit;
    }

    if (cfg.mountpoint == NULL) {
        fprintf(stderr, "%s: Please supply the mount point\n", v6fuse_cmdname);
        goto exit;
    }

    /* if initializing a new filesystem ... */
    if (cfg.initfs) {

        if (cfg.fssize > UINT16_MAX) {
            fprintf(stderr, "%s: ERROR: Specified filesystem size is too big (must be <= 65535)\n", v6fuse_cmdname);
            goto exit;
        }

        /* create a new disk image file or open the underlying block device */
        res = dsk_open(cfg.dskfilename, cfg.fssize, cfg.fsoffset, 1, 0);
        if (res != 0) {
            if (res == -EEXIST)
                fprintf(stderr, "%s: ERROR: Filesystem image file exists: %s\nTo prevent accidents, the filesystem image file must NOT exist when using -oinitfs.\n", v6fuse_cmdname, cfg.dskfilename);
            else if (res == -EINVAL)
                fprintf(stderr, "%s: ERROR: Missing -o fssize option\nThe size of the filesystem must be specified when using -oinitfs\n", v6fuse_cmdname);
            else
                fprintf(stderr, "%s: ERROR: Failed to open disk/image file: %s\n", v6fuse_cmdname, strerror(-res));
            goto exit;
        }

        /* get the final filesystem size.  this value may have been determined by the 
           the size of the underlying block device. */
        off_t fssize = dsk_getsize();

        /* verify the filesystem size is sane and does not exceed the capabilities of the v6 code. */
        if (fssize < 5) {
            fprintf(stderr, "%s: ERROR: Filesystem size is too small.  Use -o fssize option to specify a size >= 5 blocks.\n", v6fuse_cmdname);
            goto exit;
        }
        if (fssize > INT16_MAX) {
            fprintf(stderr, "%s: ERROR: Filesystem size is too big.  Use -o fssize option to specify a size <= 32767 blocks.\n", v6fuse_cmdname);
            goto exit;
        }

        /* verify the request number of inode blocks is sane. */
        if (cfg.isize > (fssize - 4)) {
            fprintf(stderr, "%s: ERROR: Specified inode table size too big.  Must be <= filesystem size - 4 blocks.\n", v6fuse_cmdname);
            goto exit;
        }

        /* initialize the new filesystem with the specified parameters. */
        res = v6fs_mkfs((uint16_t)fssize, (uint16_t)cfg.isize, &cfg.flparams);
        if (res != 0) {
            fprintf(stderr, "%s: ERROR: Failed to initialize filesystem: %s\n", v6fuse_cmdname, strerror(-res));
            goto exit;
        }

        /* close the virtual disk containing the new filesystem */
        dsk_close();
    }

    /* verify access to the underlying block device/image file. */
    if (access(cfg.dskfilename, cfg.readonly ? R_OK : R_OK|W_OK) != 0) {
        fprintf(stderr, "%s: ERROR: Unable to access disk/image file: %s\n%s\n", v6fuse_cmdname, cfg.dskfilename, strerror(errno));
        goto exit;
    }

    /* create the FUSE mount point */
    chan = fuse_mount(cfg.mountpoint, &args);
    if (chan == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to mount FUSE filesystem: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    /* initialize a new FUSE session */
    fuse = fuse_new(chan, &args, &v6fuse_ops, sizeof(v6fuse_ops), &cfg);
    if (fuse == NULL) {
        fprintf(stderr, "%s: ERROR: Failed to initialize FUSE connection: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    /* open the underlying block device/image file */
    res = dsk_open(cfg.dskfilename, cfg.fssize, cfg.fsoffset, 0, cfg.readonly);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to open disk/image file: %s\n", v6fuse_cmdname, strerror(-res));
        goto exit;
    }

    /* 'mount' the v6 filesystem */
    res = v6fs_init(cfg.readonly);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to mount v6 filesystem: %s\n", v6fuse_cmdname, strerror(-res));
        goto exit;
    }

    /* run as a background process, unless told not to */
    if (fuse_daemonize(cfg.foreground) != 0) {
        fprintf(stderr, "%s: ERROR: Failed to daemonize FUSE process: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    /* arrange for standard handling of process signals */
    session = fuse_get_session(fuse);
    if (fuse_set_signal_handlers(session) != 0) {
        fprintf(stderr, "%s: ERROR: Failed to set FUSE signal handlers: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }
    sighandlersset = 1;

    /* process filesystem requests until told to stop */
    if (fuse_loop(fuse) != 0) {
        fprintf(stderr, "%s: ERROR: fuse_loop() failed: %s\n", v6fuse_cmdname, strerror(errno));
        goto exit;
    }

    res = 0;

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