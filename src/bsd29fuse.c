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
 * @file  FUSE filesystem implementation for 2.9BSD filesystems.
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
#include "bsd29fs.h"
#include "dskio.h"

struct adddir_context {
    void *buf;
    fuse_fill_dir_t filler;
};

const uint32_t retrofuse_maxuidgid = BSD29FS_MAX_UID_GID;

int retrofuse_parseinitfsopt(struct retrofuse_config *cfg, const char *arg)
{
    cfg->initfs = 1;
    cfg->initfsparams.isize = 0;
    cfg->initfsparams.n = 1;
    cfg->initfsparams.m = 1;

    int matchend = -1;
    int scanres = sscanf(arg, " initfs %n= %" SCNu32 " %n: %" SCNu16 " : %" SCNu16 " %n",
                         &matchend, &cfg->initfsparams.isize, &matchend, 
                         &cfg->initfsparams.n, &cfg->initfsparams.m, &matchend);
    /* match -oinitfs */
    if (scanres == EOF && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<isize> */
    if (scanres == 1 && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<isize>:<n>:<m> */
    if (scanres == 3 && arg[matchend] == 0) {
        if (cfg->initfsparams.n < 1 || cfg->initfsparams.n > BSD29FS_MAXFN) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter n must be between 1 and %d\n", 
                    retrofuse_cmdname, BSD29FS_MAXFN);
            return -1;
        }
        if (cfg->initfsparams.m < 1 || cfg->initfsparams.m > cfg->initfsparams.n) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter m must be between 1 and the value given for n (%" PRIu16 ")\n", 
                    retrofuse_cmdname, cfg->initfsparams.n);
            return -1;
        }
        return 0;
    }
    fprintf(stderr, "%s: ERROR: invalid initfs option: -o %s\nExpected -o initfs=<isize>[:<n>:<m>]\n", retrofuse_cmdname, arg);
    return -1;
}

static int checkfssize(uint32_t fssize)
{
    if (fssize < BSD29FS_MIN_FS_SIZE) {
        fprintf(stderr, "%s: ERROR: Filesystem size is too small (must be >= %d blocks).\n",
                retrofuse_cmdname, BSD29FS_MIN_FS_SIZE);
        return -1;
    }
    if (fssize > BSD29FS_MAX_FS_SIZE) {
        fprintf(stderr, "%s: ERROR: Filesystem size is too big (must be <= %d blocks).\n",
                retrofuse_cmdname, BSD29FS_MAX_FS_SIZE);
        return -1;
    }
    return 0;
}

static int checkisize(uint32_t isize, uint32_t fssize)
{
    if (isize < BSD29FS_MIN_ITABLE_SIZE) {
        fprintf(stderr, "%s: ERROR: Specified inode table size is too small (must be >= %d blocks).\n", 
                retrofuse_cmdname, BSD29FS_MIN_ITABLE_SIZE);
        return -1;
    }
    if (isize > BSD29FS_MAX_ITABLE_SIZE) {
        fprintf(stderr, "%s: ERROR: Specified inode table size is too big (must be <= %d blocks).\n",
                retrofuse_cmdname, BSD29FS_MAX_ITABLE_SIZE);
        return -1;
    }
    if (fssize != 0 && isize > (fssize - 4)) {
        fprintf(stderr, "%s: ERROR: Specified inode table size too big (must be <= filesystem size - 4 blocks).\n",
                retrofuse_cmdname);
        return -1;
    }
    return 0;
}

int retrofuse_initfs(const struct retrofuse_config * cfg)
{
    int res;

    /* if given, verify that the fssize argument is within range. */
    if (cfg->fssize != 0 && checkfssize(cfg->fssize) != 0)
        return -1;

    /* if given, verify that the inode table size argument is within range. */
    if (cfg->initfsparams.isize != 0 && checkisize(cfg->initfsparams.isize, cfg->fssize) != 0)
        return -1;

    /* get the final filesystem size.  Note that this value may have been derrived from the 
     * the size of the underlying block device. */
    off_t fssize = dsk_getsize();

    /* double check the final filesystem size is sane and does not exceed the capabilities of the v6 code. */
    if (checkfssize(fssize) != 0)
        return -1;

    /* double check the inode table size. */
    if (cfg->initfsparams.isize != 0 && checkisize(cfg->initfsparams.isize, fssize) != 0)
        return -1;

    /* initialize the new filesystem with the specified parameters. */
    struct bsd29fs_flparams flparams = { .n = cfg->initfsparams.n, .m = cfg->initfsparams.m };
    res = bsd29fs_mkfs(fssize, cfg->initfsparams.isize, &flparams);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to initialize filesystem: %s\n", retrofuse_cmdname, strerror(-res));
        return -1;
    }

    /* close the virtual disk containing the new filesystem */
    dsk_close();

    return 0;
}

int retrofuse_adduidmap(uid_t hostid, uint32_t fsid)
{
    return bsd29fs_adduidmap(hostid, fsid);
}

int retrofuse_addgidmap(gid_t hostid, uint32_t fsid)
{
    return bsd29fs_addgidmap(hostid, fsid);
}

int retrofuse_mount(const struct retrofuse_config * cfg)
{
    int res;

    /* 'mount' the filesystem */
    res = bsd29fs_init(cfg->readonly);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to mount 2.9BSD filesystem: %s\n", retrofuse_cmdname, strerror(-res));
        return -1;
    }

    return 0;
}

void retrofuse_showhelp()
{
    // TODO: fix this
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
        "        of the filesystem, using the logic found in the original BSD mkfs command.\n"
        "\n"
        "        n and m are the interleave parameters for the initial free block list.\n"
        "        If not specified, the values n=500,m=3 are used, as per the original BSD\n"
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
        retrofuse_cmdname);
}

static inline void setfscontext()
{
    struct fuse_context *context = fuse_get_context();

    /* Set the real and effective uid/gid for filesystem operations
       to the values from the FUSE client process.  */
    bsd29fs_setreuid(context->uid, context->uid);
    bsd29fs_setregid(context->gid, context->gid);
}

static void * retrofuse_init(struct fuse_conn_info * conn)
{
    /* Update FUSE configuration based on capabilities of filesystem. */
    conn->want = (conn->capable & (FUSE_CAP_ATOMIC_O_TRUNC|FUSE_CAP_EXPORT_SUPPORT|FUSE_CAP_BIG_WRITES));
    conn->async_read = 0;

    return NULL;
}

static void retrofuse_destroy(void * userdata)
{
    bsd29fs_shutdown();
    dsk_close();
}

static int retrofuse_getattr(const char *pathname, struct stat *statbuf)
{
    setfscontext();
    return bsd29fs_stat(pathname, statbuf);
}

static int retrofuse_access(const char *pathname, int mode)
{
    setfscontext();
    return bsd29fs_access(pathname, mode);
}

static int retrofuse_filldir(const char *entryname, const struct stat *statbuf, void *_context)
{
    struct adddir_context * context = (struct adddir_context *)_context;
    context->filler(context->buf, entryname, statbuf, 0);
    return 0;
}

static int retrofuse_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct adddir_context context = {
        .buf = buf,
        .filler = filler
    };
    setfscontext();
    return bsd29fs_enumdir(pathname, retrofuse_filldir, &context);
}

static int retrofuse_open(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    int res = bsd29fs_open(pathname, fi->flags, 0);
    if (res < 0)
        return res;
    fi->fh = (uint64_t)res;
    return 0;
}

static int retrofuse_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    setfscontext();
    return bsd29fs_pread((int)fi->fh, buf, size, offset);
}

static int retrofuse_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    setfscontext();
    return bsd29fs_pwrite((int)fi->fh, buf, size, offset);
}

static int retrofuse_truncate(const char *pathname, off_t length)
{
    setfscontext();
    return bsd29fs_truncate(pathname, length);
}

static int retrofuse_flush(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    bsd29fs_sync();
    return 0;
}

static int retrofuse_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
    setfscontext();
    bsd29fs_sync();
    return 0;
}

static int retrofuse_release(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    return bsd29fs_close((int)fi->fh);
}

static int retrofuse_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    setfscontext();
    return bsd29fs_mknod(pathname, mode, dev);
}

static int retrofuse_mkdir(const char *pathname, mode_t mode)
{
    setfscontext();
    return bsd29fs_mkdir(pathname, mode);
}

static int retrofuse_rmdir(const char *pathname)
{
    setfscontext();
    return bsd29fs_rmdir(pathname);
}

static int retrofuse_link(const char *oldpath, const char *newpath)
{
    setfscontext();
    return bsd29fs_link(oldpath, newpath);
}

static int retrofuse_unlink(const char *pathname)
{
    setfscontext();
    return bsd29fs_unlink(pathname);
}

static int retrofuse_rename(const char *oldpath, const char *newpath)
{
    setfscontext();
    return bsd29fs_rename(oldpath, newpath);
}

static int retrofuse_chmod(const char *pathname, mode_t mode)
{
    setfscontext();
    return bsd29fs_chmod(pathname, mode);
}

static int retrofuse_chown(const char *pathname, uid_t uid, gid_t gid)
{
    setfscontext();
    return bsd29fs_chown(pathname, uid, gid);
}

static int retrofuse_utimens(const char *pathname, const struct timespec tv[2])
{
    setfscontext();
    return bsd29fs_utimens(pathname, tv);
}

static int retrofuse_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    setfscontext();
    return bsd29fs_statfs(pathname, statvfsbuf);
}

const struct fuse_operations retrofuse_ops = 
{
    .init       = retrofuse_init,
    .destroy    = retrofuse_destroy,
    .getattr    = retrofuse_getattr,
    .access     = retrofuse_access,
    .readdir    = retrofuse_readdir,
    .open       = retrofuse_open,
    .read       = retrofuse_read,
    .write      = retrofuse_write,
    .truncate   = retrofuse_truncate,
    .flush      = retrofuse_flush,
    .fsync      = retrofuse_fsync,
    .release    = retrofuse_release,
    .mknod      = retrofuse_mknod,
    .mkdir      = retrofuse_mkdir,
    .rmdir      = retrofuse_rmdir,
    .link       = retrofuse_link,
    .unlink     = retrofuse_unlink,
    .rename     = retrofuse_rename,
    .chmod      = retrofuse_chmod,
    .chown      = retrofuse_chown,
    .utimens    = retrofuse_utimens,
    .statfs     = retrofuse_statfs,

    .flag_utime_omit_ok = 1
};
