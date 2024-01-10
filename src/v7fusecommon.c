/*
 * Copyright 2023 Jay Logue
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
 * @file  Common FUSE code for filesystems based on Unix v7.
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
#include "fstype.h"
#include "v7fs.h"
#include "dskio.h"

struct adddir_context {
    void *buf;
    fuse_fill_dir_t filler;
};

#define RETROFUSE_OPT_KEY_MKFS      100
#define RETROFUSE_OPT_KEY_MAPUID    101
#define RETROFUSE_OPT_KEY_MAPGID    102
#define RETROFUSE_OPT_KEY_DSKLAYOUT 103

static void * fuseop_init(struct fuse_conn_info * conn);
static void fuseop_destroy(void * userdata);
static int fuseop_getattr(const char *pathname, struct stat *statbuf);
static int fuseop_access(const char *pathname, int mode);
static int fuseop_filldir(const char *entryname, const struct stat *statbuf, void *_context);
static int fuseop_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
static int fuseop_open(const char *pathname, struct fuse_file_info *fi);
static int fuseop_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int fuseop_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
static int fuseop_truncate(const char *pathname, off_t length);
static int fuseop_flush(const char *pathname, struct fuse_file_info *fi);
static int fuseop_fsync(const char *pathname, int datasync, struct fuse_file_info *fi);
static int fuseop_release(const char *pathname, struct fuse_file_info *fi);
static int fuseop_mknod(const char *pathname, mode_t mode, dev_t dev);
static int fuseop_mkdir(const char *pathname, mode_t mode);
static int fuseop_rmdir(const char *pathname);
static int fuseop_link(const char *oldpath, const char *newpath);
static int fuseop_unlink(const char *pathname);
static int fuseop_rename(const char *oldpath, const char *newpath);
static int fuseop_chmod(const char *pathname, mode_t mode);
static int fuseop_chown(const char *pathname, uid_t uid, gid_t gid);
static int fuseop_utimens(const char *pathname, const struct timespec tv[2]);
static int fuseop_statfs(const char *pathname, struct statvfs *statvfsbuf);

const struct fuse_operations retrofuse_fuseops = 
{
    .init       = fuseop_init,
    .destroy    = fuseop_destroy,
    .getattr    = fuseop_getattr,
    .access     = fuseop_access,
    .readdir    = fuseop_readdir,
    .open       = fuseop_open,
    .read       = fuseop_read,
    .write      = fuseop_write,
    .truncate   = fuseop_truncate,
    .flush      = fuseop_flush,
    .fsync      = fuseop_fsync,
    .release    = fuseop_release,
    .mknod      = fuseop_mknod,
    .mkdir      = fuseop_mkdir,
    .rmdir      = fuseop_rmdir,
    .link       = fuseop_link,
    .unlink     = fuseop_unlink,
    .rename     = fuseop_rename,
    .chmod      = fuseop_chmod,
    .chown      = fuseop_chown,
    .utimens    = fuseop_utimens,
    .statfs     = fuseop_statfs,

    .flag_utime_omit_ok = 1
};

int retrofuse_checkconfig_v7(struct retrofuse_config *cfg)
{
    /* if mkfs has been requested... */
    if (cfg->mkfs) {

        /* if the fssize option was given, verify that the value is in range. */
        if (cfg->dskcfg.fssize != -1) {
            if (cfg->dskcfg.fssize < V7FS_MIN_FS_SIZE) {
                fprintf(stderr, "%s: ERROR: Specified filesystem size is too small (min size: %d blocks).\n",
                        retrofuse_cmdname, V7FS_MIN_FS_SIZE);
                return -1;
            }
            if (cfg->dskcfg.fssize > V7FS_MAX_FS_SIZE) {
                fprintf(stderr, "%s: ERROR: Specified filesystem size is too big (max size: %d blocks).\n",
                        retrofuse_cmdname, V7FS_MAX_FS_SIZE);
                return -1;
            }
        }

        /* check the mkfs n and m parameters */
        if (cfg->mkfscfg.n < 1 || cfg->mkfscfg.n > V7FS_MAXFN) {
            fprintf(stderr, "%s: ERROR: invalid mkfs option: free list parameter n must be between 1 and %d\n", 
                    retrofuse_cmdname, V7FS_MAXFN);
            return -1;
        }
        if (cfg->mkfscfg.m < 1 || cfg->mkfscfg.m > cfg->mkfscfg.n) {
            fprintf(stderr, "%s: ERROR: invalid mkfs option: free list parameter m must be between 1 and the value given for n (%" PRIu16 ")\n", 
                    retrofuse_cmdname, cfg->mkfscfg.n);
            return -1;
        }

        /* check the mkfs isize parameter. */
        if (cfg->mkfscfg.isize > 0) {
            uint32_t inodesperblock = fs_blocksize(cfg->fstype) / V7FS_INODE_SIZE;
            uint32_t maxisize = (V7FS_MAX_INODES + (inodesperblock - 1)) / inodesperblock;
            if (cfg->mkfscfg.isize > maxisize) {
                fprintf(stderr, "%s: ERROR: Specified inode table size is too big (must be <= %d blocks).\n",
                        retrofuse_cmdname, maxisize);
                return -1;
            }
        }
    }

    return 0;
}

int retrofuse_mountfs(struct retrofuse_config *cfg)
{
    int res;
    off_t fssize, parsize;

    /* 'mount' the v7 filesystem */
    res = v7fs_init(cfg->fstype, cfg->readonly);
    if (res != 0) {
        fprintf(stderr, "%s: ERROR: Failed to mount v7 filesystem: %s\n", retrofuse_cmdname, strerror(-res));
        return res;
    }

    /* get the size of the filesystem, as recorded in the superblock, and convert
     * to disk blocks. */
    fssize = v7fs_fssize();
    fssize = fssize * (fs_blocksize(cfg->fstype) / DSK_BLKSIZE);

    /* compare the size of the filesystem with the size of the underlying disk
     * partition, if known.  issue a warning if the filesystem size is bigger
     * than the partition size. */
    parsize = dsk_fssize();
    if (parsize >= 0 && fssize > parsize) {
        fprintf(stderr, "%s: WARNING: Filesystem size exceeds size of underlying disk/partition: %s\n", retrofuse_cmdname, strerror(-res));
    }

    /* as a safety measure, limit disk I/O to only those blocks that are
     * considered to be part of the filesystem */
    dsk_setfslimit(fssize);

    return 0;
}

int retrofuse_mkfs(struct retrofuse_config *cfg)
{
    int res = -1;
    off_t fssize;

    /* get the size of the filesystem partition on the disk. */
    fssize = dsk_fssize();

    /* fail if the size of the filesystem partition is not known. */
    if (fssize == INT64_MAX) {
        fprintf(stderr,
                "%s: ERROR: Cannot derive the filesystem size from the size of the underlying disk/partition.\n"
                "Please specify the filesystem size using the -o fssize option.\n", retrofuse_cmdname);
        return -1;
    }

    /* convert the size to units of filesystem blocks */
    fssize = fssize / (fs_blocksize(cfg->fstype) / DSK_BLKSIZE);

    /* fail if the partition is too small to hold a filesystem */
    if (fssize < V7FS_MIN_FS_SIZE) {
        fprintf(stderr, "%s: ERROR: The underlying disk/partition size is too small to hold the filesystem (min size: %d blocks).\n",
                retrofuse_cmdname, V7FS_MIN_FS_SIZE);
        return -1;
    }

    /* if the partition is larger that the max filesystem size,
     * silently reduced it to the maximum. */
    if (fssize > V7FS_MAX_FS_SIZE) {
        fssize = V7FS_MAX_FS_SIZE;
    }

    // fail if the specified inode table size is too big relative to the filesystem size */
    if (cfg->mkfscfg.isize > (fssize - (V7FS_MIN_FS_SIZE - 1))) {
        fprintf(stderr, "%s: ERROR: Specified inode table size too big (must be <= filesystem size - %d blocks).\n",
                retrofuse_cmdname, (V7FS_MIN_FS_SIZE-1));
        return -1;
    }

    /* as a safety measure, limit reading and writing to the area of the disk
     * that is actually occupied by the filesystem */
    dsk_setfslimit(fssize * (fs_blocksize(cfg->fstype) / DSK_BLKSIZE));

    /* initialize the new filesystem with the specified parameters. */
    {
        struct v7fs_flparams flparams = { .n = cfg->mkfscfg.n, .m = cfg->mkfscfg.m };
        res = v7fs_mkfs(cfg->fstype, fssize, cfg->mkfscfg.isize, &flparams);
        if (res != 0) {
            fprintf(stderr, "%s: ERROR: Failed to initialize filesystem: %s\n", retrofuse_cmdname, strerror(-res));
            return -1;
        }
    }

    return 0;
}

/* ========== Private Functions/Data ========== */

static inline void setfscontext()
{
    struct fuse_context *context = fuse_get_context();

    /* Set the real and effective uid/gid for v7 filesystem operations
       to the values from the FUSE client process.  */
    v7fs_setreuid(context->uid, context->uid);
    v7fs_setregid(context->gid, context->gid);
}

static void * fuseop_init(struct fuse_conn_info * conn)
{
    /* Update FUSE configuration based on capabilities of filesystem. */
    conn->want = (conn->capable & (FUSE_CAP_ATOMIC_O_TRUNC|FUSE_CAP_EXPORT_SUPPORT|FUSE_CAP_BIG_WRITES));
    conn->async_read = 0;

    return NULL;
}

static void fuseop_destroy(void * userdata)
{
    v7fs_shutdown();
    dsk_close();
}

static int fuseop_getattr(const char *pathname, struct stat *statbuf)
{
    setfscontext();
    return v7fs_stat(pathname, statbuf);
}

static int fuseop_access(const char *pathname, int mode)
{
    setfscontext();
    return v7fs_access(pathname, mode);
}

static int fuseop_filldir(const char *entryname, const struct stat *statbuf, void *_context)
{
    struct adddir_context * context = (struct adddir_context *)_context;
    context->filler(context->buf, entryname, statbuf, 0);
    return 0;
}

static int fuseop_readdir(const char *pathname, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    struct adddir_context context = {
        .buf = buf,
        .filler = filler
    };
    setfscontext();
    return v7fs_enumdir(pathname, fuseop_filldir, &context);
}

static int fuseop_open(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    int res = v7fs_open(pathname, fi->flags, 0);
    if (res < 0)
        return res;
    fi->fh = (uint64_t)res;
    return 0;
}

static int fuseop_read(const char *pathname, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    setfscontext();
    return v7fs_pread((int)fi->fh, buf, size, offset);
}

static int fuseop_write(const char *pathname, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    setfscontext();
    return v7fs_pwrite((int)fi->fh, buf, size, offset);
}

static int fuseop_truncate(const char *pathname, off_t length)
{
    setfscontext();
    return v7fs_truncate(pathname, length);
}

static int fuseop_flush(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    v7fs_sync();
    return 0;
}

static int fuseop_fsync(const char *pathname, int datasync, struct fuse_file_info *fi)
{
    setfscontext();
    v7fs_sync();
    return 0;
}

static int fuseop_release(const char *pathname, struct fuse_file_info *fi)
{
    setfscontext();
    return v7fs_close((int)fi->fh);
}

static int fuseop_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    setfscontext();
    return v7fs_mknod(pathname, mode, dev);
}

static int fuseop_mkdir(const char *pathname, mode_t mode)
{
    setfscontext();
    return v7fs_mkdir(pathname, mode);
}

static int fuseop_rmdir(const char *pathname)
{
    setfscontext();
    return v7fs_rmdir(pathname);
}

static int fuseop_link(const char *oldpath, const char *newpath)
{
    setfscontext();
    return v7fs_link(oldpath, newpath);
}

static int fuseop_unlink(const char *pathname)
{
    setfscontext();
    return v7fs_unlink(pathname);
}

static int fuseop_rename(const char *oldpath, const char *newpath)
{
    setfscontext();
    return v7fs_rename(oldpath, newpath);
}

static int fuseop_chmod(const char *pathname, mode_t mode)
{
    setfscontext();
    return v7fs_chmod(pathname, mode);
}

static int fuseop_chown(const char *pathname, uid_t uid, gid_t gid)
{
    setfscontext();
    return v7fs_chown(pathname, uid, gid);
}

static int fuseop_utimens(const char *pathname, const struct timespec tv[2])
{
    setfscontext();
    return v7fs_utimens(pathname, tv);
}

static int fuseop_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    setfscontext();
    return v7fs_statfs(pathname, statvfsbuf);
}
