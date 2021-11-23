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
 * @file  Implements a modern interface to a Unix v6 filesystem.
 * 
 * The functions implemented here closely mimic the modern Unix filesystem
 * API, with the notible exception that errors are returned as return
 * values rather than via errno.
 * 
 * While intended to support the Unix v6 FUSE filesystem, these functions 
 * are independent of FUSE code and thus could be used in other contexts.
 */

#define _XOPEN_SOURCE 700
#define _ATFILE_SOURCE 
#define _DARWIN_C_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#if defined(__APPLE__)
#include <sys/types.h>
#endif
#if defined(__linux__)
#include <sys/sysmacros.h>
#endif
#include <libgen.h>
#include <sys/statvfs.h>

#include "v6fs.h"
#include "dskio.h"
#include "idmap.h"

#include "v6adapt.h"
#include "param.h"
#include "user.h"
#include "buf.h"
#include "systm.h"
#include "inode.h"
#include "file.h"
#include "filsys.h"
#include "conf.h"
#include "v6unadapt.h"

int v6fs_initialized = 0;

static struct idmap v6fs_uidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = UINT8_MAX,
    .defaulthostid = 65534, // nobody
    .defaultfsid = UINT8_MAX
};

static struct idmap v6fs_gidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = UINT8_MAX,
    .defaulthostid = 65534, // nogroup
    .defaultfsid = UINT8_MAX
};

static void v6fs_initfreelist(struct v6_filsys *fp, uint16_t n, uint16_t m);
static void v6fs_convertstat(const struct v6_stat *v6statbuf, struct stat *statbuf);
static int v6fs_isindir(const char *pathname, int16_t dirnum);
static int v6fs_failnonemptydir(const char *entryname, const struct stat *statbuf, void *context);
static int v6fs_isdirlinkpath(const char *pathname);
static inline char v6fs_maphostuid(uid_t hostuid) { return (char)idmap_tofsid(&v6fs_uidmap, (uint32_t)hostuid); }
static inline char v6fs_maphostgid(gid_t hostgid) { return (char)idmap_tofsid(&v6fs_gidmap, (uint32_t)hostgid); }
static inline uid_t v6fs_mapv6uid(char v6uid) { return (uid_t)idmap_tohostid(&v6fs_uidmap, (uint32_t)v6uid); }
static inline gid_t v6fs_mapv6gid(char v6gid) { return (gid_t)idmap_tohostid(&v6fs_gidmap, (uint32_t)v6gid); }

/** Initialize the Unix v6 filesystem.
 * 
 * This function is very similar to the v6 kernel main() function.
 */
int v6fs_init(int readonly)
{
    if (v6fs_initialized)
        return -EBUSY;

    /* zero kernel data structures and globals */
    v6_zerocore();

    /* set the device id for the root device. */
    v6_rootdev = 0;

    v6_u.u_error = 0;

    /* initialize the buffer pool. */
    v6_binit();
    if (v6_u.u_error != 0)
        return -v6_u.u_error;

    /* mount the root device and read the superblock. */
    v6_iinit();
    if (v6_u.u_error != 0)
        return -v6_u.u_error;

    struct v6_filsys *fs = v6_getfs(v6_rootdev);

    /* if the size of the underlying virtual disk is unknown, or
       is smaller than the size declared in the filesystem superblock
       set the virtual disk size to the size from the superblock. */
    off_t dsksize = dsk_getsize();
    if (dsksize == 0 || dsksize > (uint16_t)fs->s_fsize)
        dsk_setsize((uint16_t)fs->s_fsize);

    /* mark the filesystem read-only if requested. */
    if (readonly)
        fs->s_ronly = 1;

    /* get the root directory inode. */
    v6_rootdir = v6_iget(v6_rootdev, ROOTINO);
    v6_rootdir->i_flag &= ~ILOCK;

    /* set the user's current directory. */
    v6_u.u_cdir = v6_rootdir;

    v6fs_initialized = 1;

    return 0;
}

/** Shutdown the Unix v6 filesystem, sycning any unwritten data.
 */
int v6fs_shutdown()
{
    int res = 0;
    if (v6fs_initialized) {
        res = v6fs_sync();
        v6_zerocore();
        v6fs_initialized = 0;
    }
    return res;
}

/** Initialize a new filesystem.
 * 
 * Thus function is effectly a simplified re-implementation of the
 * v6 mkfs command.
 */
int v6fs_mkfs(uint16_t fssize, uint16_t isize, const struct v6fs_flparams *flparams)
{
    struct v6_buf *bp;
    struct v6_filsys *fp;
    struct v6_direntry *dp;
    int16_t rootdirblkno = 0;

    if (v6fs_initialized)
        return -EBUSY;

    /* enforce min/max filesystem size. */
    if (fssize < V6FS_MIN_FS_SIZE || fssize > V6FS_MAX_FS_SIZE)
        return -EINVAL;

    /* if not specified, compute the number of inode blocks based on the
     * filesystem size (based on code in v6 mkfs) */
    if (isize == 0)
        isize = fssize / (43 + (fssize / 1000));

    /* enforce min/max inode table size.  max size is limited by the size of
     * integer used to store inode numbers (uint16_t) and the overall size
     * of the filesystem. */
    if (isize < V6FS_MIN_ITABLE_SIZE || isize > V6FS_MAX_ITABLE_SIZE || isize > (fssize - 4))
        return -EINVAL;

    v6_refreshclock();

    /* zero kernel data structures and globals */
    v6_zerocore();

    /* set the device id for the root device. */
    v6_rootdev = 0;

    /* initialize the buffer pool. */
    v6_binit();

    v6_u.u_error = 0;

    /* initialize the filesystem superblock in memory. 
     * Note that the timestamp for the new filesystem is always set to the Unix
     * epoch. This is because, at boot time, v6 will set the system time to the
     * timestamp in the root filesystem. If this time is happens to be later than
     * about 30 years past the Unix epoch, it will trigger a bug in the v6 date(1)
     * program. Always setting the time to the Unix epoch avoids this. */
	bp = v6_getblk(NODEV, -1);
    v6_clrbuf(bp);
	fp = (struct v6_filsys *)bp->b_addr;
    fp->s_isize = (int16_t)isize;
    fp->s_fsize = (int16_t)fssize;
    fp->s_fmod = 1;

    /* create the mount for the root device. */
	v6_mount[0].m_bufp = bp;
	v6_mount[0].m_dev = v6_rootdev;

    /* initialize the free block list on disk. */
    v6fs_initfreelist(fp, flparams->n, flparams->m);
    if (v6_u.u_error != 0)
        goto exit;

    /* allocate block for root directory and initialize '..' and '.' entries. */
    bp = v6_alloc(v6_rootdev);
    if (bp == NULL)
        goto exit;
    rootdirblkno = bp->b_blkno;
    dp = (struct v6_direntry *)bp->b_addr;
    dp->d_name[0] = '.';
    dp->d_name[1] = '.';
    dp->d_inode = ROOTINO;
    dp++;
    dp->d_name[0] = '.';
    dp->d_inode = ROOTINO;
    v6_bwrite(bp);
    if ((bp->b_flags & B_ERROR) != 0)
        goto exit;

    /* initialize the inode table on disk. setup the first inode to represent
       the root directory. */
    for (uint16_t i = 0; i < isize; i++) {
        bp = v6_getblk(v6_rootdev, i + 2);
        v6_clrbuf(bp);
        if (i == 0) {
            struct v6_inode_dsk * ip = (struct v6_inode_dsk *)bp->b_addr;
            ip->i_mode = (int16_t)(IALLOC|IFDIR|0777);
            ip->i_nlink = 2;
            ip->i_uid = v6_u.u_uid;
            ip->i_gid = v6_u.u_gid;
            ip->i_size0 = 0;
            ip->i_size1 = (DIRSIZ+2)*2; /* size of 2 directory entries */
            ip->i_addr[0] = rootdirblkno;
            ip->i_atime[0] = ip->i_mtime[0] = v6_time[0];
            ip->i_atime[1] = ip->i_mtime[1] = v6_time[1];
        }
        v6_bwrite(bp);
        if ((bp->b_flags & B_ERROR) != 0)
            goto exit;
    }

    /* make the superblock "pristine" by zeroing unused entries in 
       the free table. */
    for (int i = fp->s_nfree; i < 100; i++)
        fp->s_free[i] = 0;

    /* sync the superblock */
    v6_update();

exit:
    v6_zerocore();
    return -v6_u.u_error;
}

/** Open file or directory in the v6 filesystem.
 */
int v6fs_open(const char *name, int flags, mode_t mode)
{
    struct v6_inode *ip;
    int16_t fd;
    int16_t m;

    v6_refreshclock();
    v6_u.u_error = 0;

    switch (flags & O_ACCMODE)
    {
    case O_RDWR:
        m = V6_FREAD | V6_FWRITE;
        break;
    case O_RDONLY:
    default:
        m = V6_FREAD;
        break;
    case O_WRONLY:
        m = V6_FWRITE;
        break;
    }
    v6_u.u_ar0 = &fd;
    v6_u.u_dirp = (char *)name;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL) {
        if (v6_u.u_error != ENOENT)
            return -v6_u.u_error;
        if ((flags & O_CREAT) == 0)
            return -v6_u.u_error;
        ip = v6_maknode(mode & 07777 & (~ISVTX));
        if (ip == NULL)
            return -v6_u.u_error;
        v6_open1(ip, m, 2);
    }
    else {
        if ((flags & O_CREAT) != 0 && (flags & O_EXCL) != 0) {
            v6_iput(ip);
            return -EEXIST;
        }
        v6_open1(ip, m, ((flags & O_TRUNC) != 0) ? 1 : 0);
    }
    if (v6_u.u_error != 0)
        return -v6_u.u_error;
    return fd;
}

/** Close file/directory.
 */
int v6fs_close(int fd)
{
    int16_t ar0;

    v6_refreshclock();
    v6_u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    ar0 = fd;
    v6_u.u_ar0 = &ar0;
    v6_close();
    return 0;
}

/** Seek file/directory.
 */
off_t v6fs_seek(int fd, off_t offset, int whence)
{
    struct v6_file *fp;
    off_t curSize, curPos;

    v6_refreshclock();
    v6_u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    fp = v6_getf((int16_t)fd);
    if (fp == NULL)
        return -v6_u.u_error;
    if (fp->f_flag & FPIPE)
        return -ESPIPE;
    curSize = ((off_t)fp->f_inode->i_size0 & 0377) << 16 | fp->f_inode->i_size1;
    curPos = ((off_t)fp->f_offset[0]) << 16 | fp->f_offset[1];
    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        offset += curPos;
        break;
    case SEEK_END:
        offset += curSize;
        break;
#ifdef SEEK_DATA
    case SEEK_DATA:
        if (offset >= curSize)
            return -ENXIO;
        break;
#endif /* SEEK_DATA */
#ifdef SEEK_HOLE
    case SEEK_HOLE:
        if (offset < curSize)
            offset = curSize;
        break;
#endif /* SEEK_DATA */
    default:
        return -EINVAL;
    }
    if (offset < 0 || offset > curSize)
        return -EINVAL;
    fp->f_offset[1] = (uint16_t)(offset);
    fp->f_offset[0] = (uint16_t)(offset >> 16);
    return offset;
}

/** Create a hard link.
 */
int v6fs_link(const char *oldpath, const char *newpath)
{
    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)oldpath;
    v6_u.u_arg[1] = (intptr_t)newpath;
    v6_link();
    return -v6_u.u_error;
}

/** Create a new file, directory, block or character device node.
 */
int v6fs_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    struct v6_inode *ip;
    int16_t v6mode, v6dev;

    v6_refreshclock();
    v6_u.u_error = 0;

    v6dev = ((int16_t)(major(dev) & 0xFF) << 8) | (int16_t)(minor(dev) & 0xFF);
    switch (mode & S_IFMT)
    {
    case S_IFREG:
        v6mode = 0;
        v6dev = 0;
        break;
    case S_IFBLK:
        v6mode = IFBLK;
        break;
    case S_IFCHR:
        v6mode = IFCHR;
        break;
    default:
        return -EINVAL;
    }
    v6mode |= (mode & 07777);

    v6_u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_uchar, 1);
    if (ip != NULL) {
        v6_iput(ip);
        return -EEXIST;
    }
    if (v6_u.u_error != 0)
        return -v6_u.u_error;
    ip = v6_maknode(v6mode);
    if (ip == NULL)
        return -v6_u.u_error;
    ip->i_addr[0] = (int16_t)v6dev;
    v6_iput(ip);
    return -v6_u.u_error;
}

/** Read data from a file/directory.
 */
ssize_t v6fs_read(int fd, void *buf, size_t count)
{
    ssize_t totalReadSize = 0;
    int16_t ar0;
    
    v6_refreshclock();

    while (count > 0)
    {
        ar0 = fd;
        v6_u.u_ar0 = &ar0;
        v6_u.u_arg[0] = (intptr_t)buf;
        v6_u.u_arg[1] = (count > UINT16_MAX) ? UINT16_MAX : (uint16_t)count;
        v6_u.u_error = 0;
        v6_rdwr(V6_FREAD);

        if (v6_u.u_error != 0)
            return -v6_u.u_error;

        if (ar0 == 0)
            break;

        totalReadSize += (uint16_t)ar0;
        count -= (uint16_t)ar0;
        buf += (uint16_t)ar0;
    }

    return totalReadSize;
}

/** Read data from a file/directory at a specific offset.
 */
ssize_t v6fs_pread(int fd, void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = v6fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return v6fs_read(fd, buf, count);
}

/** Write data to a file/directory.
 */
ssize_t v6fs_write(int fd, const void *buf, size_t count)
{
    uint16_t writeSize;
    ssize_t totalWriteSize = 0;
    int16_t ar0;
    
    v6_refreshclock();

    while (count > 0)
    {
        writeSize = (count > UINT16_MAX) ? UINT16_MAX : (uint16_t)count;

        ar0 = fd;
        v6_u.u_ar0 = &ar0;
        v6_u.u_arg[0] = (intptr_t)buf;
        v6_u.u_arg[1] = writeSize;
        v6_u.u_error = 0;
        v6_rdwr(V6_FWRITE);

        if (v6_u.u_error  != 0)
            return -v6_u.u_error;

        totalWriteSize += (uint16_t)ar0;
        count -= (uint16_t)ar0;
        buf += (uint16_t)ar0;

        if ((uint16_t)ar0 != writeSize)
            break;
    }

    return totalWriteSize;
}

/** Write data to a file/directory at a specific offset.
 */
ssize_t v6fs_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = v6fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return v6fs_write(fd, buf, count);
}

int v6fs_truncate(const char *pathname, off_t length)
{
    int res = 0;
    struct v6_inode *ip;

    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -v6_u.u_error;
    if (length == 0) {
        v6_itrunc(ip);
        res = -v6_u.u_error;
    }
    else
        res =  -EINVAL;
    v6_iput(ip);
    return res;
}

/** Get file statistics.
 */
int v6fs_stat(const char *pathname, struct stat *statbuf)
{
    struct v6_inode *ip;
    struct v6_stat v6statbuf;

    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -v6_u.u_error;
    v6_stat1(ip, &v6statbuf);
    if (v6_u.u_error != 0) {
        v6_iput(ip);
        return -v6_u.u_error;
    }
    v6fs_convertstat(&v6statbuf, statbuf);
    v6_iput(ip);
    return 0;
}

/** Delete a directory entry.
 */
int v6fs_unlink(const char *pathname)
{
    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)pathname;
    v6_unlink();
    return -v6_u.u_error;
}

/** Rename or move a file/directory.
 */
int v6fs_rename(const char *oldpath, const char *newpath)
{
    struct v6_inode *oldip = NULL, *newip = NULL;
    int res = 0;
    int rmdirnew = 0;
    int unlinknew = 0;
    char preveuid = v6_u.u_uid;

    v6_refreshclock();
    v6_u.u_error = 0;

    /* get the inode for the source file/dir */
    v6_u.u_dirp = (char *)oldpath;
    oldip = v6_namei(&v6_schar, 0);
    if (oldip == NULL) {
        res = -v6_u.u_error;
        goto exit;
    }
    oldip->i_flag &= ~ILOCK;

    /* disallow renaming the root directory, or from/to any directory with the
       name '.' or '..' */
    if (oldip->i_number == ROOTINO || v6fs_isdirlinkpath(oldpath) || v6fs_isdirlinkpath(newpath)) {
        res = -EBUSY;
        goto exit;
    }

    /* get the inode for the destination file/dir, if it exists */
    v6_u.u_dirp = (char *)newpath;
    newip = v6_namei(&v6_schar, 0);
    if (oldip == NULL && v6_u.u_error != ENOENT) {
        res = -v6_u.u_error;
        goto exit;
    }

    /* if the destination exists... */
    if (newip != NULL) {
        /* if source and destination refer to the same file/dir, then
           per rename(2) semantics, do nothing */
        if (oldip->i_number == newip->i_number)
            goto exit;
        /* if destination is a directory ... */
        if ((newip->i_mode & IFMT) == IFDIR) {
            /* fail if source is not also a directory ... */
            if ((oldip->i_mode & IFMT) != IFDIR) {
                res = -EISDIR;
                goto exit;
            }
            /* arrange to remove the destination directory before renaming the source. */
            rmdirnew = 1;
        }
        else {
            /* otherwise, destination is a file, so fail if source is a directory */
            if ((oldip->i_mode & IFMT) == IFDIR) {
                res = -ENOTDIR;
                goto exit;
            }
            /* arrange to remove the destination file before renaming the source */
            unlinknew = 1;
        }

        v6_iput(newip);
        newip = NULL;
    }

    /* if source is a directory ... */
    if ((oldip->i_mode & IFMT) == IFDIR) {
        /* fail if destination is a child of source */
        res = v6fs_isindir(newpath, oldip->i_number);
        if (res < 0)
            goto exit;
        if (res) {
            res = -EINVAL;
            goto exit;
        }
    }

    v6_iput(oldip);
    oldip = NULL;

    /* if needed, remove an existing directory with the same name as the destination.
       if the directory is not empty, this will fail. */
    if (rmdirnew) {
        res = v6fs_rmdir(newpath);
        if (res < 0)
            goto exit;
    }

    /* if needed, remove an existing file with the same name as the destination.
     */
    if (unlinknew) {
        res = v6fs_unlink(newpath);
        if (res < 0)
            goto exit;
    }

    /* perform the following as "set-uid" root. this allows creating addition temporary
       links to source directory. */
    v6_u.u_uid = 0;

    /* create destination file/dir as a link to the source */
    res = v6fs_link(oldpath, newpath);
    if (res < 0)
        goto exit;

    /* remove the source */
    res = v6fs_unlink(oldpath);

exit:
    if (oldip != NULL)
        v6_iput(oldip);
    if (newip != NULL)
        v6_iput(newip);
    v6_u.u_uid = preveuid;
    return res;
}

/** Change permissions of a file.
 */
int v6fs_chmod(const char *pathname, mode_t mode)
{
    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)pathname;
    v6_u.u_arg[1] = (int16_t)(mode & 07777);
    v6_chmod();
    return -v6_u.u_error;
}

/** Change ownership of a file.
 */
int v6fs_chown(const char *pathname, uid_t owner, gid_t group)
{
    int res = 0;
    struct v6_inode *ip;
    char v6uid = v6fs_maphostuid(owner);
    char v6gid = v6fs_maphostgid(group);

    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)pathname;
	if ((ip = v6_namei(v6_uchar, 0)) == NULL)
		return -v6_u.u_error;

    if (owner != -1 || group != -1) {
        if (owner != -1 && ip->i_uid != v6uid) {
            if (!v6_suser()) {
                res = -EPERM;
                goto exit;
            }
        }
        if (group != -1 && ip->i_gid != v6gid) {
            if (!v6_suser() && v6_u.u_uid != ip->i_uid) {
                res = -EPERM;
                goto exit;
            }
        }

        if (owner != -1)
            ip->i_uid = v6uid;
        if (group != -1)
            ip->i_gid = v6gid;
        ip->i_flag |= IACC;
    }

exit:
    v6_iput(ip);
    return res;
}

/** Change file last access and modification times
 */
int v6fs_utimens(const char *pathname, const struct timespec times[2])
{
    int res = 0;
    struct v6_inode *ip;
    int16_t curTime[2], mtime[2];

    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -v6_u.u_error;

    if (times[0].tv_nsec != UTIME_OMIT || times[1].tv_nsec != UTIME_OMIT) {
        
        if (v6_u.u_uid != ip->i_uid && v6_access(ip, IWRITE) != 0) {
            res = -EPERM;
            goto exit;
        }
        
        curTime[0] = mtime[0] = v6_time[0]; 
        curTime[1] = mtime[1] = v6_time[1];

        if (times[0].tv_nsec != UTIME_OMIT) {
            ip->i_flag |= IACC;
            if (times[0].tv_nsec != UTIME_NOW) {
                v6_time[0] = (int16_t)(times[0].tv_sec >> 16);
                v6_time[1] = (int16_t)(times[0].tv_sec);
            }
        }

        if (times[1].tv_nsec != UTIME_OMIT) {
            ip->i_flag |= IUPD;
            if (times[1].tv_nsec != UTIME_NOW) {
                mtime[0] = (int16_t)(times[1].tv_sec >> 16);
                mtime[1] = (int16_t)(times[1].tv_sec);
            }
        }

        v6_iupdat(ip, mtime);
        res = -v6_u.u_error;

        ip->i_flag &= ~(IACC|IUPD);

        v6_time[0] = curTime[0];
        v6_time[1] = curTime[1];
    }

exit:
    v6_iput(ip);
    return res;
}

/** Check user's permissions for a file.
 */
int v6fs_access(const char *pathname, int mode)
{
    int res = 0;
    struct v6_inode *ip;

    v6_refreshclock();
    v6_u.u_error = 0;

    v6_u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -v6_u.u_error;
    if ((mode & R_OK) != 0 && v6_access(ip, IREAD) != 0)
        res = -v6_u.u_error;
    if ((mode & W_OK) != 0 && v6_access(ip, IWRITE) != 0)
        res = -v6_u.u_error;
    if ((mode & X_OK) != 0 && v6_access(ip, IEXEC) != 0)
        res = -v6_u.u_error;
    v6_iput(ip);
    return res;
}

/** Create a directory.
 */
int v6fs_mkdir(const char *pathname, mode_t mode)
{
    int res;
    struct v6_inode *ip;
    char *namebuf;
    char *parentname;
    int dircreated = 0;
    char preveuid = v6_u.u_uid;

    v6_refreshclock();
    v6_u.u_error = 0;

    namebuf = (char *)malloc(strlen(pathname) + 4);

    strcpy(namebuf, pathname);
    parentname = strdup(dirname(namebuf));

    /* verify that the given pathname does not already exist.
       in the process, a pointer to the parent directory is
       set in v6_u.u_pdir. */
    v6_u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_uchar, 1);
    if (ip != NULL) {
        v6_iput(ip);
        res = -EEXIST;
        goto exit;
    }
    if ((res = -v6_u.u_error) < 0)
        goto exit;

    /* in v6, mkdir is a command that runs set-uid root, 
       which gives it the priveldge to create links to
       directory node (in particular the . and .. entries).
       here we simulate this by switching the effective uid
       to root. */
    v6_u.u_uid = 0;

    /* make the directory node within the parent directory. 
       temporarily grant access to the root user only. */
    ip = v6_maknode(IFDIR|0700);
    if (ip == NULL) {
        res = -v6_u.u_error;
        goto exit;
    }
    dircreated = 1;
    v6_iput(ip);
    if ((res = -v6_u.u_error) < 0)
        goto exit;

    /* create the . directory link. */
    strcpy(namebuf, pathname);
    strcat(namebuf, "/.");
    res = v6fs_link(pathname, namebuf);
    if (res < 0)
        goto exit;

    /* create the /. directory link. */
    strcat(namebuf, ".");
    res = v6fs_link(parentname, namebuf);
    if (res < 0)
        goto exit;

    /* change the owner of the new directory from root to the
       current real user id. */
    v6_u.u_error = 0;
    v6_u.u_dirp = (char *)pathname;
	if ((ip = v6_namei(v6_uchar, 0)) == NULL) {
		res = -v6_u.u_error;
        goto exit;
    }
    ip->i_uid = v6_u.u_ruid;
    ip->i_flag |= IUPD;
    v6_iput(ip);
    if ((res = -v6_u.u_error) < 0)
        goto exit;

    /* set the final access permissions on the new directory. */
    res = v6fs_chmod(pathname, mode&07777);

exit:
    if (res != 0 && dircreated) {
        strcpy(namebuf, pathname);
        strcat(namebuf, "/..");
        v6fs_unlink(namebuf);

        strcpy(namebuf, pathname);
        strcat(namebuf, "/.");
        v6fs_unlink(namebuf);

        v6fs_unlink(pathname);
    }
    v6_u.u_uid = preveuid;
    free(namebuf);
    free(parentname);
    return res;
}

/** Delete a directory.
 */
int v6fs_rmdir(const char *pathname)
{
    struct stat statbuf;
    int res;
    char *dirname;
    char *namebuf;
    char preveuid = v6_u.u_uid;

    namebuf = (char *)malloc(strlen(pathname) + 4);
    
    res = v6fs_stat(pathname, &statbuf);
    if (res < 0)
        goto exit;

    if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
        res = -ENOTDIR;
        goto exit;
    }

    if (statbuf.st_ino == v6_rootdir->i_number) {
        res = -EBUSY;
        goto exit;
    }

    /* verify directory is empty */
    res = v6fs_enumdir(pathname, v6fs_failnonemptydir, NULL);
    if (res < 0)
        goto exit;

    strcpy(namebuf, pathname);
    dirname = basename(namebuf);
    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        res = -EINVAL;
        goto exit;
    }

    /* perform the following as "set-uid" root */
    v6_u.u_uid = 0;

    strcpy(namebuf, pathname);
    strcat(namebuf, "/..");
    res = v6fs_unlink(namebuf);
    if (res != 0)
        goto exit;

    strcpy(namebuf, pathname);
    strcat(namebuf, "/.");
    res = v6fs_unlink(namebuf);
    if (res != 0)
        goto exit;

    res = v6fs_unlink(pathname);

exit:
    free(namebuf);
    v6_u.u_uid = preveuid;
    return res;
}

/** Enumerate files in a directory.
 */
int v6fs_enumdir(const char *pathname, v6fs_enum_dir_funct enum_funct, void *context)
{
    int fd;
    union {
        struct v6_direntry direntry;
        char buf[sizeof(struct v6_direntry) + 1];
    } direntrybuf;
    struct v6_stat v6statbuf;
    struct stat statbuf;
    struct v6_inode *ip = NULL;
    int readRes;

    v6_refreshclock();
    v6_u.u_error = 0;

    memset(&direntrybuf, 0, sizeof(direntrybuf));

    fd = v6fs_open(pathname, O_RDONLY, 0);
    if (fd < 0)
        return fd;

    while (1) {
        readRes = v6fs_read(fd, &direntrybuf.direntry, sizeof(struct v6_direntry));
        if (readRes <= 0)
            break;
        if (readRes < sizeof(struct v6_direntry)) {
            readRes = 0;
            break;
        }

        if (direntrybuf.direntry.d_inode == 0)
            continue;

        v6_u.u_error = 0;
        ip = v6_iget(v6_rootdev, direntrybuf.direntry.d_inode);
        if (ip == NULL) {
            readRes = -v6_u.u_error;
            break;
        }

        v6_u.u_error = 0;
        v6_stat1(ip, &v6statbuf);
        if (v6_u.u_error != 0) {
            readRes = -v6_u.u_error;
            break;
        }

        v6fs_convertstat(&v6statbuf, &statbuf);

        readRes = enum_funct(direntrybuf.direntry.d_name, &statbuf, context);
        if (readRes < 0)
            break;

        v6_iput(ip);
        ip = NULL;
    }

    if (ip != NULL)
        v6_iput(ip);

    v6fs_close(fd);

    return readRes;
}

/** Commit filesystem changes to disk.
 */
int v6fs_sync()
{
    v6_refreshclock();
    v6_u.u_error = 0;
    v6_update();
    int res = dsk_flush();
    if (v6_u.u_error != 0)
        return -v6_u.u_error;
    return res;
}

/** Get filesystem statistics.
 */
int v6fs_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    struct v6_filsys *fp;
    struct v6_buf *bp = NULL;

    v6_refreshclock();
    v6_u.u_error = 0;

    v6_update();

    memset(statvfsbuf, 0, sizeof(struct statvfs));

    fp = v6_getfs(v6_rootdev);

    statvfsbuf->f_bsize = 512;
    statvfsbuf->f_frsize = 512;
    statvfsbuf->f_blocks = (uint16_t)fp->s_fsize;
    statvfsbuf->f_files = fp->s_isize * 16;
    statvfsbuf->f_namemax = DIRSIZ;

    /* count the number of free blocks.
     *
     * The freelist consist of a table of 100 block numbers located in the
     * filesystem superblock (fp->s_free).  Each entry in this table can
     * contain the block number of a free block.  The number of entries that
     * actually contain free block numbers (starting at index 0 in the table)
     * is denoted by the integer fp->s_nfree.
     * 
     * The first block number in the table is special.  If there are more than
     * 99 free blocks, fp->s_free[0] is the number of a free block that also
     * contains pointers to other free blocks.  This is referred to as an
     * "index" block.  In cases where there are 99 or fewer free blocks, 
     * fp->s_free[0] will always contain the NULL block number (0), indicating
     * that no index block exists.

     * Like the superblock, an index block also contains a table of 100 block
     * numbers (starting at offset 2) and a count of the number table entries
     * that actually contain free blocks (located at offset 0).  Similarly, the
     * first position in the table is also used to point to a subsequent index
     * block in cases where there are more than 99 additional free blocks. Thus
     * index blocks are strung together to form a chain of groups of 100 free
     * blocks.

     * NOTE that there are *two* distinct states possible when the filesystem
     * reaches the point of being completely full:
     *     1) fp->s_nfree == 0
     *     2) fp->s_nfree == 1 and fp->s_free[0] == 0
     * The second state is entered when the last free block is allocated by the
     * alloc() function.  The first state is entered when alloc() is called again
     * *after* the last block has been allocated (i.e. when the first allocation
     * failure occurs).
     */
    {    
        int16_t *nfree = &fp->s_nfree;
        int16_t *freetab = fp->s_free;
        uint16_t idxblkcount = 0;
        while (1) {
            int16_t nextidxblk;
            if (*nfree > 0) {
                nextidxblk = freetab[0];
                statvfsbuf->f_bfree += (nextidxblk != 0) ? *nfree : *nfree - 1;
            }
            else
                nextidxblk = 0;
            if (bp != NULL)
                v6_brelse(bp);
            if (nextidxblk == 0)
                break;
            idxblkcount++;
            if (idxblkcount > (uint16_t)fp->s_fsize) {
                fprintf(stderr, "V6FS ERROR: loop detected in free list\n");
                return -EIO;
            }
            if (v6_badblock(fp, nextidxblk, v6_rootdev))
                return -EIO;
            bp = v6_bread(v6_rootdev, nextidxblk);
            v6_geterror(bp);
            if (v6_u.u_error != 0)
                return -v6_u.u_error;
            nfree = (int16_t *)bp->b_addr;
            freetab = nfree + 1;
        }
    }
    statvfsbuf->f_bavail = statvfsbuf->f_bfree;

    /* count the number of free inodes. */
    for (int16_t blk = 2; blk < fp->s_isize + 2; blk++) {
        bp = v6_bread(v6_rootdev, blk);
        v6_geterror(bp);
        if (v6_u.u_error != 0)
            return -v6_u.u_error;
        int16_t *ip = (int16_t *)bp->b_addr;
        for (int i = 0; i < V6FS_INODES_PER_BLOCK; i++, ip += 16) {
            if ((*ip & IALLOC) == 0)
                statvfsbuf->f_ffree++;
        }
        v6_brelse(bp);
    }

    return 0;
}

/** Set the real and/or effective user ids for filesystem operations.
 */
int v6fs_setreuid(uid_t ruid, uid_t euid)
{
    if (ruid != -1)
        v6_u.u_ruid = v6fs_maphostuid(ruid);
    if (euid != -1)
        v6_u.u_uid = v6fs_maphostuid(euid);
    return 0;
}

/** Set the real and/or effective group ids for filesystem operations.
 */
int v6fs_setregid(gid_t rgid, gid_t egid)
{
    if (rgid != -1)
        v6_u.u_rgid = v6fs_maphostgid(rgid);
    if (egid != -1)
        v6_u.u_gid = v6fs_maphostgid(egid);
    return 0;
}

/** Add an entry to the uid mapping table.
 */
int v6fs_adduidmap(uid_t hostuid, uint32_t fsuid)
{
    return idmap_addidmap(&v6fs_uidmap, (uint32_t)hostuid, fsuid);
}

/** Add an entry to the gid mapping table.
 */
int v6fs_addgidmap(uid_t hostgid, uint32_t fsgid)
{
    return idmap_addidmap(&v6fs_gidmap, (uint32_t)hostgid, fsgid);
}

/** Construct the initial free block list for a new filesystem.
 *
 * This function is effectively identical to the bflist() function in the
 * v6 mkfs command.
 * 
 * n and m are values used to create an initial interleave in the order
 * that blocks appear on the free list.  The reasoning behind this
 * algorithm is unclear.  It's possible its purpose was to opimize
 * seeking in some way.  The v6 mkfs command used values of n=23,m=3
 * for rk devices, 10,4 for rp devices and 1,1 (i.e. no interleave)
 * for all others.
 */
static void v6fs_initfreelist(struct v6_filsys *fp, uint16_t n, uint16_t m)
{
    uint8_t flg[100], adr[100];
    uint16_t i, j, low, high;
    
    if (n > 100)
        n = 100;
    for (i = 0; i < n; i++)
        flg[i] = 0;
    i = 0;
    for (j = 0; j < n; j++) {
        while (flg[i])
            i = (i+1)%n;
        adr[j] = i;
        flg[i]++;
        i = (i+m)%n;
    }

    high = fp->s_fsize-1;
    low = fp->s_isize+2;
    /* The following code was a bit of cleverness used to
       set the NULL terminator for the free block list. It
       is unnecessary in this implementation. */
    /* free(0); */
    for (i = high; ((i+1) % n) != 0; i--) {
        if (i < low)
            break;
        v6_free(v6_rootdev, i);
    }
    for (; i >= low+n; i -= n)
        for (j=0; j < n; j++)
            v6_free(v6_rootdev, i-adr[j]);
    for (; i >= low; i--)
        v6_free(v6_rootdev, i);
}

static void v6fs_convertstat(const struct v6_stat *v6statbuf, struct stat *statbuf)
{
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_dev = v6statbuf->dev;
    statbuf->st_ino = v6statbuf->inumber;
    statbuf->st_mode = v6statbuf->mode ;
    statbuf->st_nlink = v6statbuf->nlinks;
    statbuf->st_uid = v6fs_mapv6uid(v6statbuf->uid);
    statbuf->st_gid = v6fs_mapv6gid(v6statbuf->gid);
    statbuf->st_size = ((off_t)(uint16_t)v6statbuf->size0) << 16 | (uint16_t)v6statbuf->size1;
    statbuf->st_blocks = (statbuf->st_size + 511) >> 9;
    switch (v6statbuf->mode & IFMT)
    {
    case IFDIR:
        statbuf->st_mode = S_IFDIR;
        break;
    case IFCHR:
        statbuf->st_mode = S_IFCHR;
        statbuf->st_rdev = v6statbuf->addr[0];
        break;
    case IFBLK:
        statbuf->st_mode = S_IFBLK;
        statbuf->st_rdev = v6statbuf->addr[0];
        break;
    default:
        statbuf->st_mode = S_IFREG;
        break;
    }
    statbuf->st_mode |= (v6statbuf->mode & 07777);
    statbuf->st_atime = ((time_t)(uint16_t)v6statbuf->actime[0]) << 16 | (uint16_t)v6statbuf->actime[1];
    statbuf->st_mtime = ((time_t)(uint16_t)v6statbuf->modtime[0]) << 16 | (uint16_t)v6statbuf->modtime[1];
}

static int v6fs_isindir(const char *pathname, int16_t dirnum)
{
    char *pathnamecopy, *p;
    int res = 0;

    pathnamecopy = strdup(pathname);
    if (pathnamecopy == NULL)
        return 1;

    p = pathnamecopy;
    do {
        p = dirname(p);
        v6_u.u_dirp = p;
        struct v6_inode *ip = v6_namei(&v6_uchar, 1);
        if (ip != NULL) {
            res = (ip->i_number == dirnum);
            v6_iput(ip);
        }
        else if (v6_u.u_error != ENOENT)
            res = -v6_u.u_error;
    } while (!res && strcmp(p, "/") != 0 && strcmp(p, ".") != 0);

    free(pathnamecopy);
    return res;
}

static int v6fs_failnonemptydir(const char *entryname, const struct stat *statbuf, void *context)
{
    if ((statbuf->st_mode & S_IFMT) == S_IFDIR && (strcmp(entryname, ".") == 0 || strcmp(entryname, "..") == 0))
        return 0;
    return -ENOTEMPTY;
}

static int v6fs_isdirlinkpath(const char *pathname)
{
    const char * p = strrchr(pathname, '/');
    if (p == NULL)
        p = pathname;
    else
        p++;
    return (strcmp(p, ".") == 0 || strcmp(p, "..") == 0);
}
