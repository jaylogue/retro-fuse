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
 * @file  FUSE filesystem implementation for 2.11BSD filesystems.
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
#include "bsd211fs.h"
#include "dskio.h"

struct adddir_context {
    void *buf;
    fuse_fill_dir_t filler;
};

const uint32_t retrofuse_maxuidgid = BSD211FS_MAX_UID_GID;

int retrofuse_parseinitfsopt(struct retrofuse_config *cfg, const char *arg)
{
    cfg->initfs = 1;
    cfg->initfsparams.iratio = 0;
    cfg->initfsparams.n = BSD211FS_DEFAULT_FN;
    cfg->initfsparams.m = BSD211FS_DEFAULT_FM;

    int matchend = -1;
    int scanres = sscanf(arg, " initfs %n= %" SCNu32 " %n: %" SCNu16 " : %" SCNu16 " %n",
                         &matchend, &cfg->initfsparams.iratio, &matchend, 
                         &cfg->initfsparams.n, &cfg->initfsparams.m, &matchend);
    /* match -oinitfs */
    if (scanres == EOF && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<iratio> */
    if (scanres == 1 && arg[matchend] == 0)
        return 0;
    /* match -oinitfs=<iratio>:<n>:<m> */
    if (scanres == 3 && arg[matchend] == 0) {
        if (cfg->initfsparams.n < 1 || cfg->initfsparams.n > BSD211FS_MAX_FN) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter n must be between 1 and %d\n", 
                    retrofuse_cmdname, BSD211FS_MAX_FN);
            return -1;
        }
        if (cfg->initfsparams.m < 1 || cfg->initfsparams.m > cfg->initfsparams.n) {
            fprintf(stderr, "%s: ERROR: invalid initfs option: free list parameter m must be between 1 and the value given for n (%" PRIu16 ")\n", 
                    retrofuse_cmdname, cfg->initfsparams.n);
            return -1;
        }
        return 0;
    }
    fprintf(stderr, "%s: ERROR: invalid initfs option: -o %s\nExpected -o initfs=<iratio>[:<n>:<m>]\n", retrofuse_cmdname, arg);
    return -1;
}

int retrofuse_checkinitfsconfig(const struct retrofuse_config * cfg)
{
    /* if given, verify that the fssize argument is within range. */
    if (cfg->fssize != 0) {
        if (cfg->fssize < BSD211FS_MIN_FS_SIZE) {
            fprintf(stderr, "%s: ERROR: Filesystem size is too small (must be >= %d blocks).\n",
                    retrofuse_cmdname, BSD211FS_MIN_FS_SIZE);
            return -1;
        }
        if (cfg->fssize > BSD211FS_MAX_FS_SIZE) {
            fprintf(stderr, "%s: ERROR: Filesystem size is too big (must be <= %d blocks).\n",
                    retrofuse_cmdname, BSD211FS_MAX_FS_SIZE);
            return -1;
        }
    }

    /* if given, verify that the inode ratio argument is within range. */
    if (cfg->initfsparams.iratio != 0 && cfg->initfsparams.iratio > (cfg->fssize * BSD211FS_BLOCK_SIZE)) {
        fprintf(stderr, "%s: ERROR: Specified inode ratio is too big (must be < filesystem size in bytes).\n", 
                retrofuse_cmdname);
        return -1;
    }

    return 0;
}

int retrofuse_initfs(const struct retrofuse_config * cfg)
{
    int res;

    /* initialize a new filesystem with the specified parameters. */
    struct bsd211fs_flparams flparams = { .n = cfg->initfsparams.n, .m = cfg->initfsparams.m };
    res = bsd211fs_mkfs(cfg->fssize, cfg->initfsparams.iratio, &flparams);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to initialize filesystem: %s\n", retrofuse_cmdname, strerror(-res));
        return -1;
    }

    return 0;
}

int retrofuse_adduidmap(uid_t hostid, uint32_t fsid)
{
    return bsd211fs_adduidmap(hostid, fsid);
}

int retrofuse_addgidmap(gid_t hostid, uint32_t fsid)
{
    return bsd211fs_addgidmap(hostid, fsid);
}

int retrofuse_mount(const struct retrofuse_config * cfg)
{
    int res;

    /* 'mount' the filesystem */
    res = bsd211fs_init(cfg->readonly);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to mount 2.11BSD filesystem: %s\n", retrofuse_cmdname, strerror(-res));
        return -1;
    }

    return 0;
}

void retrofuse_showhelp()
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
        "        The size of the filesystem, in 1K-byte blocks. This is used to\n"
        "        constrain I/O on the underlying device/image file.  Defaults to the\n"
        "        size given in the filesystem superblock.\n"
        "\n"
        "  -o fsoffset=<blocks>\n"
        "        Offset into the device/image at which the filesystem starts, in\n"
        "        1K-byte blocks.  Defaults to 0.\n"
        "\n"
        "  -o initfs\n"
        "  -o initfs=<iratio>\n"
        "  -o initfs=<iratio>:<n>:<m>\n"
        "        Create an empty filesystem on the underlying device/image file before\n"
        "        mounting.  When using the initfs option on an image file, the size of\n"
        "        the filesystem must be specified via the -ofssize option.\n"
        "\n"
        "        iratio controls the total number of inode in the filesystem's inode table.\n"
        "        The value is specified in filesystem bytes per inode. For example, a value\n"
        "        of 1024 will result in one inode per 1K of filesytem size (rounded down).\n"
        "        If not specified, or if 0 is given, a value of 4096 is used.\n"
        "\n"
        "        n and m are the interleave parameters for the initial free block list,\n"
        "        where m is the distance between successive 1K-byte blocks, and n is the\n"
        "        number of blocks before the pattern repeats. Specifying n=1,m=1 produces\n"
        "        no interleave. If not specified, a default of n=100,m=2 is used, which\n"
        "        matches the original BSD mkfs command.\n"
        "\n"
        "        Note that the initfs option does not write a disklabel. If a disklabel is\n"
        "        needed, one must be created using the BSD disklabel(8) command.\n"
        "\n"
        "  -o overwrite\n"
        "        When used with the initfs option, instructs the filesystem handler\n"
        "        to overwrite any existing filesystem image file. Without this option,\n"
        "        the `initfs` option will fail with an error if an image file exists.\n"
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

off_t retrofuse_fsblktodskblk(off_t fsblk)
{
    return fsblk * (BSD211FS_BLOCK_SIZE / DSK_BLKSIZE);
}

off_t retrofuse_dskblktofsblk(off_t dskblk)
{
    return dskblk / (BSD211FS_BLOCK_SIZE / DSK_BLKSIZE);
}

static inline void setfscontext()
{
    struct fuse_context *context = fuse_get_context();
    gid_t gids[BSD211FS_NGROUPS_MAX];
    int res;

    /* Set the real and effective uid/gid for filesystem operations
       to the values from the FUSE client process.  */
    bsd211fs_setreuid(context->uid, context->uid);
    bsd211fs_setregid(context->gid, context->gid);

    /* Set the supplementary group ids to the values from the client
       process */
    res = fuse_getgroups(BSD211FS_NGROUPS_MAX, gids);
    if (res >= 0) {
        if (res > BSD211FS_NGROUPS_MAX)
            res = BSD211FS_NGROUPS_MAX;
        bsd211fs_setgroups(res, gids);
    }
    else {
        bsd211fs_setgroups(0, NULL);
        /* in some FUSE contexts (e.g. during a release() call), fuse_getgroups()
           will return EIO. presumably this means the supplementary groups are
           unavailable in this context, in which case we simply ignore the error. */
        if (res != -EIO)
            fprintf(stderr, "%s: ERROR: fuse_getgroups() failed: %s\n", retrofuse_cmdname, strerror(-res));
    }
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
    bsd211fs_shutdown();
    dsk_close();
}

static int retrofuse_getattr(const char *pathname, struct stat *statbuf)
{
    setfscontext();
    return bsd211fs_stat(pathname, statbuf);
}

static int retrofuse_access(const char *pathname, int mode)
{
    setfscontext();
    return bsd211fs_access(pathname, mode);
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
    return bsd211fs_enumdir(pathname, retrofuse_filldir, &context);
}

static int retrofuse_open(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    int res = bsd211fs_open(pathname, fi->flags, 0);
    if (res < 0)
        return res;
    fi->fh = (uint64_t)res;
    return 0;
}

static int retrofuse_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    setfscontext();
    return bsd211fs_pread((int)fi->fh, buf, size, offset);
}

static int retrofuse_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    setfscontext();
    return bsd211fs_pwrite((int)fi->fh, buf, size, offset);
}

static int retrofuse_truncate(const char *pathname, off_t length)
{
    setfscontext();
    return bsd211fs_truncate(pathname, length);
}

static int retrofuse_flush(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    bsd211fs_sync();
    return 0;
}

static int retrofuse_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
    // TODO: fix this
    setfscontext();
    bsd211fs_sync();
    return 0;
}

static int retrofuse_release(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    return bsd211fs_close((int)fi->fh);
}

static int retrofuse_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    setfscontext();
    return bsd211fs_mknod(pathname, mode, dev);
}

static int retrofuse_mkdir(const char *pathname, mode_t mode)
{
    setfscontext();
    return bsd211fs_mkdir(pathname, mode);
}

static int retrofuse_rmdir(const char *pathname)
{
    setfscontext();
    return bsd211fs_rmdir(pathname);
}

static int retrofuse_link(const char *oldpath, const char *newpath)
{
    setfscontext();
    return bsd211fs_link(oldpath, newpath);
}

static int retrofuse_unlink(const char *pathname)
{
    setfscontext();
    return bsd211fs_unlink(pathname);
}

static int retrofuse_rename(const char *oldpath, const char *newpath)
{
    setfscontext();
    return bsd211fs_rename(oldpath, newpath);
}

static int retrofuse_chmod(const char *pathname, mode_t mode)
{
    setfscontext();
    return bsd211fs_chmod(pathname, mode);
}

static int retrofuse_chown(const char *pathname, uid_t uid, gid_t gid)
{
    setfscontext();
    return bsd211fs_chown(pathname, uid, gid);
}

static int retrofuse_utimens(const char *pathname, const struct timespec tv[2])
{
    setfscontext();
    return bsd211fs_utimens(pathname, tv);
}

static int retrofuse_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    setfscontext();
    return bsd211fs_statfs(pathname, statvfsbuf);
}

static int retrofuse_readlink(const char *pathname, char *buf, size_t bufsiz)
{
    setfscontext();
    ssize_t res = bsd211fs_readlink(pathname, buf, bufsiz);
    if (res >= 0) {
        if (res > (bufsiz-1))
            res = (bufsiz-1);
        buf[res] = 0;
        res = 0;
    }
    return (int)res;
}

int retrofuse_symlink(const char *target, const char *linkpath)
{
    setfscontext();
    return bsd211fs_symlink(target, linkpath);
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
    .readlink   = retrofuse_readlink,
    .symlink    = retrofuse_symlink,

    .flag_utime_omit_ok = 1
};
