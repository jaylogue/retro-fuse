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
 * @file  Implements a modern interface to a Unix v7 filesystem.
 * 
 * The functions implemented here closely mimic the modern Unix filesystem
 * API, with the notible exception that errors are returned as return
 * values rather than via errno.
 * 
 * While intended to support the Unix v7 FUSE filesystem, these functions 
 * are independent of FUSE code and thus could be used in other contexts.
 */

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

#include "v7fs.h"
#include "dskio.h"
#include "idmap.h"

#include "v7adapt.h"
#include "h/param.h"
#include "h/dir.h"
#include "h/user.h"
#include "h/buf.h"
#include "h/systm.h"
#include "h/inode.h"
#include "h/ino.h"
#include "h/file.h"
#include "h/filsys.h"
#include "h/fblk.h"
#include "h/mount.h"
#include "h/stat.h"
#include "h/conf.h"
#include "v7unadapt.h"

static int v7fs_initialized = 0;

static struct idmap v7fs_uidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = V7FS_MAX_UID_GID,
    .defaulthostid = 65534, // nobody
    .defaultfsid = V7FS_MAX_UID_GID
};

static struct idmap v7fs_gidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = V7FS_MAX_UID_GID,
    .defaulthostid = 65534, // nogroup
    .defaultfsid = V7FS_MAX_UID_GID
};

static void v7fs_initfreelist(struct v7_filsys *fp, uint16_t n, uint16_t m);
static void v7fs_convertstat(const struct v7_stat *v7statbuf, struct stat *statbuf);
static int v7fs_isindir(const char *pathname, int16_t dirnum);
static int v7fs_failnonemptydir(const char *entryname, const struct stat *statbuf, void *context);
static int v7fs_isdirlinkpath(const char *pathname);
static int v7fs_reparentdir(const char *pathname);
static inline int16_t v7fs_maphostuid(uid_t hostuid) { return (int16_t)idmap_tofsid(&v7fs_uidmap, (uint32_t)hostuid); }
static inline int16_t v7fs_maphostgid(gid_t hostgid) { return (int16_t)idmap_tofsid(&v7fs_gidmap, (uint32_t)hostgid); }
static inline uid_t v7fs_mapv7uid(int16_t v7uid) { return (uid_t)idmap_tohostid(&v7fs_uidmap, (uint32_t)v7uid); }
static inline gid_t v7fs_mapv7gid(int16_t v7gid) { return (gid_t)idmap_tohostid(&v7fs_gidmap, (uint32_t)v7gid); }

/** Initialize the Unix v7 filesystem.
 * 
 * This function is very similar to the v7 kernel main() function.
 */
int v7fs_init(int readonly)
{
    if (v7fs_initialized)
        return -EBUSY;

    /* zero kernel data structures and globals */
    v7_zerocore();

    v7_fsconfig.fstype = fs_type_v7;
    v7_fsconfig.byteorder = fs_byteorder_pdp;
    v7_fsconfig.blocksize = 512;
    v7_fsconfig.nicfree = V7_NICFREE;
    v7_fsconfig.nicinod = V7_NICINOD;

    /* set the device id for the root device. */
    v7_rootdev = v7_makedev(0, 0);

    v7_u.u_error = 0;

    /* initialize the buffer pool. */
    v7_binit();
    if (v7_u.u_error != 0)
        return -v7_u.u_error;

    /* mount the root device and read the superblock. */
    v7_iinit();
    if (v7_u.u_error != 0)
        return -v7_u.u_error;

    struct v7_filsys *fs = v7_getfs(v7_rootdev);

    /* if the size of the underlying virtual disk is unknown, or
       is smaller than the size declared in the filesystem superblock
       set the virtual disk size to the size from the superblock. */
    off_t dsksize = dsk_getsize();
    v7_daddr_t fsize = fs->s_fsize;
    if (dsksize == 0 || dsksize > fsize)
        dsk_setsize((off_t)fsize);

    /* mark the filesystem read-only if requested. */
    if (readonly)
        fs->s_ronly = 1;

    /* get the root directory inode. */
    v7_rootdir = v7_iget(v7_rootdev, ROOTINO);
    v7_rootdir->i_flag &= ~ILOCK;

    /* set the user's current directory. */
    v7_u.u_cdir = v7_iget(v7_rootdev, ROOTINO);
    v7_u.u_cdir->i_flag &= ~ILOCK;
    v7_u.u_rdir = NULL;

    v7fs_initialized = 1;

    return 0;
}

/** Shutdown the Unix v7 filesystem, sycning any unwritten data.
 */
int v7fs_shutdown()
{
    int res = 0;
    if (v7fs_initialized) {
        res = v7fs_sync();
        v7_zerocore();
        v7fs_initialized = 0;
    }
    return res;
}

/** Initialize a new filesystem.
 * 
 * Thus function is effectively a simplified re-implementation of the
 * v7 mkfs command.
 */
int v7fs_mkfs(uint32_t fssize, uint32_t isize, const struct v7fs_flparams *flparams)
{
    struct v7_buf *bp;
    struct v7_filsys *fp;
    struct v7_direct *dp;
    v7_daddr_t rootdirblkno = 0;
    uint16_t fn = flparams->n, fm = flparams->m;

    if (v7fs_initialized)
        return -EBUSY;

    v7_fsconfig.fstype = fs_type_v7;
    v7_fsconfig.byteorder = fs_byteorder_pdp;
    v7_fsconfig.blocksize = 512;
    v7_fsconfig.nicfree = V7_NICFREE;
    v7_fsconfig.nicinod = V7_NICINOD;

    uint32_t NIPB = v7_fsconfig.blocksize / sizeof(struct v7_dinode);

    /* enforce min/max filesystem size.  v7 filesystems are limited to a max of
     * 2^24-1 blocks due to 3 byte block numbers stored in inodes. */
    if (fssize < V7FS_MIN_FS_SIZE || fssize > V7FS_MAX_FS_SIZE)
        return -EINVAL;

    /* if not specified, compute the number of inode blocks based on the
     * filesystem size (based on logic in v7 mkfs) */
    if (isize == 0) {
        isize = fssize / 25;
        if (isize == 0)
            isize = 1;
        if (isize > 65500 / NIPB)
            isize = 65500 / NIPB;
    }

    /* enforce min/max inode table size.  max size is limited by the size of
     * integer used to store inode numbers (uint16_t) and the overall size
     * of the filesystem. */
    if (isize < V7FS_MIN_ITABLE_SIZE || isize > V7FS_MAX_ITABLE_SIZE || isize > (fssize - 4))
        return -EINVAL;

    /* adjust interleave values as needed (based on logic in v7 mkfs) */
    if (fn == 0 || fn >= V7FS_MAXFN)
        fn = V7FS_MAXFN;
    if (fm == 0 || fm > fn)
        fm = 3;

    v7_refreshclock();

    /* zero kernel data structures and globals */
    v7_zerocore();

    /* set the device id for the root device. */
    v7_rootdev = v7_makedev(0, 0);

    /* initialize the buffer pool. */
    v7_binit();

    v7_u.u_error = 0;

    /* initialize the filesystem superblock in memory.
     *
     * NB: While the description of the s_isize field in the filsys
     * structure says the field contains the size of the inode table
     * in blocks, in fact it contains the block number of the first 
     * block *beyond* the end of the table (i.e. the first data
     * block).  Thus, the value includes the count of the boot block
     * and the superblock. This fact can be seen by inspecting the logic
     * in the badblock() function, as well as the code in mkfs.c.
     * 
     * For this reason, the following code adds 2 to the given isize
     * parameter.
     */
    bp = v7_getblk(v7_rootdev, -1);
    v7_clrbuf(bp);
    fp = (struct v7_filsys *)bp->b_un.b_filsys;
    fp->s_isize = isize + 2; /* adjusted to point to the block beyond the i-list */
    fp->s_fsize = fssize;
    fp->s_n = (int16_t)fn;
    fp->s_m = (int16_t)fm;
    fp->s_tfree = 0;
    fp->s_tinode = 0;
    fp->s_time = v7_time;
    fp->s_fmod = 1;

    /* create the mount for the root device. */
    v7_mount[0].m_bufp = bp;
    v7_mount[0].m_dev = v7_rootdev;

    /* initialize the free block list on disk. */
    v7fs_initfreelist(fp, fn, fm);
    if (v7_u.u_error != 0)
        goto exit;

    /* allocate block for root directory and initialize '..' and '.' entries. */
    bp = v7_alloc(v7_rootdev);
    if (bp == NULL)
        goto exit;
    rootdirblkno = bp->b_blkno;
    dp = (struct v7_direct *)bp->b_un.b_addr;
    dp->d_name[0] = '.';
    dp->d_ino = ROOTINO;
    dp++;
    dp->d_name[0] = '.';
    dp->d_name[1] = '.';
    dp->d_ino = ROOTINO;
    v7_bwrite(bp);
    if ((bp->b_flags & B_ERROR) != 0)
        goto exit;

    /* initialize the inode table on disk. setup the first 2 inodes (in inode block 0)
     * to represent the bad block file and the root directory, respectively. */
    for (v7_daddr_t iblk = 0; iblk < isize; iblk++) {
        bp = v7_getblk(v7_rootdev, iblk + 2);
        v7_clrbuf(bp);
        if (iblk == 0) {
            struct v7_dinode * ip = (struct v7_dinode *)bp->b_un.b_addr;
            /* bad block file */
            ip->di_mode = (int16_t)(IFREG);
            ip->di_nlink = 0;
            ip->di_uid = 0;
            ip->di_gid = 0;
            ip->di_size = 0;
            ip->di_atime = ip->di_mtime = ip->di_ctime = fs_htopdp_i32(v7_time);
            /* root directory */
            ip++;
            ip->di_mode = (int16_t)(IFDIR|0777);
            ip->di_nlink = 2;
            ip->di_uid = v7_u.u_uid;
            ip->di_gid = v7_u.u_gid;
            ip->di_size = fs_htopdp_i32(2*sizeof(struct v7_direct)); /* size of 2 directory entries */
            ip->di_addr[0] = (char)(rootdirblkno >> 16);
            ip->di_addr[1] = (char)(rootdirblkno);
            ip->di_addr[2] = (char)(rootdirblkno >> 8);
            ip->di_atime = ip->di_mtime = ip->di_ctime = fs_htopdp_i32(v7_time);
        }
        v7_bwrite(bp);
        if ((bp->b_flags & B_ERROR) != 0)
            goto exit;
    }

    /* make the superblock pristine by zeroing unused entries in 
     * the free table. */
    for (int i = fp->s_nfree; i < NICFREE; i++)
        fp->s_free[i] = 0;

    /* sync the superblock */
    v7_update();

exit:
    v7_zerocore();
    return -v7_u.u_error;
}

/** Open file or directory in the v7 filesystem.
 */
int v7fs_open(const char *name, int flags, mode_t mode)
{
    struct v7_inode *ip;
    int16_t m;

    v7_refreshclock();
    v7_u.u_error = 0;

    switch (flags & O_ACCMODE) {
    case O_RDWR:
        m = V7_FREAD | V7_FWRITE;
        break;
    case O_RDONLY:
    default:
        m = V7_FREAD;
        break;
    case O_WRONLY:
        m = V7_FWRITE;
        break;
    }
    v7_u.u_dirp = (char *)name;
    ip = v7_namei(&v7_schar, 0);
    if (ip == NULL) {
        if (v7_u.u_error != ENOENT)
            return -v7_u.u_error;
        if ((flags & O_CREAT) == 0)
            return -v7_u.u_error;
        ip = v7_maknode(mode & 07777 & (~ISVTX));
        if (ip == NULL)
            return -v7_u.u_error;
        v7_open1(ip, m, 2);
    }
    else {
        if ((flags & O_CREAT) != 0 && (flags & O_EXCL) != 0) {
            v7_iput(ip);
            return -EEXIST;
        }
        v7_open1(ip, m, ((flags & O_TRUNC) != 0) ? 1 : 0);
    }
    if (v7_u.u_error != 0)
        return -v7_u.u_error;
    return v7_u.u_r.r_val1;
}

/** Close file/directory.
 */
int v7fs_close(int fd)
{
	struct a {
		int16_t	fdes;
	} uap;

    v7_refreshclock();
    v7_u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    uap.fdes = fd;
    v7_u.u_ap = &uap;
    v7_close();
    return 0;
}

/** Seek file/directory.
 */
off_t v7fs_seek(int fd, off_t offset, int whence)
{
    struct v7_file *fp;
    off_t curSize, curPos;

    v7_refreshclock();
    v7_u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    fp = v7_getf((int16_t)fd);
    if (fp == NULL)
        return -v7_u.u_error;
    if (fp->f_flag & (FPIPE|FMP))
        return -ESPIPE;
    curSize = (off_t)fp->f_inode->i_size;
    curPos = (off_t)fp->f_un.f_offset;
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
        if (offset >= curSize)
            return -ENXIO;
        offset = curSize;
        break;
#endif /* SEEK_DATA */
    default:
        return -EINVAL;
    }
    if (offset < 0 || offset > INT32_MAX)
        return -EINVAL;
    fp->f_un.f_offset = (v7_off_t)(offset);
    return offset;
}

/** Create a hard link.
 */
int v7fs_link(const char *oldpath, const char *newpath)
{
    struct a {
        char    *target;
        char    *linkname;
    } uap;

    v7_refreshclock();
    v7_u.u_error = 0;

    uap.target = (char *)oldpath;
    uap.linkname = (char *)newpath;
    v7_u.u_ap = &uap;
    v7_u.u_dirp = (v7_caddr_t)oldpath;
    v7_link();
    return -v7_u.u_error;
}

/** Create a new file, directory, block or character device node.
 */
int v7fs_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    struct v7_inode *ip;
    int16_t fsmode;
    v7_dev_t fsdev;

    switch (mode & IFMT)
    {
    case 0:
    case S_IFREG:
        fsmode = IFREG;
        fsdev = 0;
        break;
    case S_IFBLK:
    case S_IFCHR:
        if (!v7_suser())
            return -EPERM;
        fsmode = (S_ISBLK(mode)) ? IFBLK : IFCHR;
        fsdev = v7_makedev(major(dev) & 0xFF, minor(dev) & 0xFF);
        break;
    default:
        return -EEXIST;
    }
    fsmode |= (mode & 07777);

    v7_refreshclock();
    v7_u.u_error = 0;

    v7_u.u_dirp = (char *)pathname;
    ip = v7_namei(&v7_uchar, 1);
    if (ip != NULL) {
        v7_iput(ip);
        return -EEXIST;
    }
    if (v7_u.u_error != 0)
        return -v7_u.u_error;
    ip = v7_maknode(fsmode);
    if (ip == NULL)
        return -v7_u.u_error;
    ip->i_un.i_rdev = (v7_daddr_t)fsdev;
    v7_iput(ip);
    return -v7_u.u_error;
}

/** Read data from a file/directory.
 */
ssize_t v7fs_read(int fd, void *buf, size_t count)
{
    ssize_t totalReadSize = 0;
    
    v7_refreshclock();

    while (count > 0) {
        struct a {
            int16_t    fdes;
            char    *cbuf;
            uint16_t count;
        } uap;
        uint16_t readRes;

        uap.fdes = (int16_t)fd;
        uap.cbuf = (char *)buf;
        uap.count = (count > UINT16_MAX) ? UINT16_MAX : (uint16_t)count;
        v7_u.u_ap = &uap;
        v7_u.u_error = 0;
        v7_rdwr(V7_FREAD);

        if (v7_u.u_error != 0)
            return -v7_u.u_error;

        readRes = (uint16_t)v7_u.u_r.r_val1;
        if (readRes == 0)
            break;

        totalReadSize += readRes;
        count -= readRes;
        buf += readRes;
    }

    return totalReadSize;
}

/** Read data from a file/directory at a specific offset.
 */
ssize_t v7fs_pread(int fd, void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = v7fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return v7fs_read(fd, buf, count);
}

/** Write data to a file/directory.
 */
ssize_t v7fs_write(int fd, const void *buf, size_t count)
{
    ssize_t totalWriteSize = 0;
    
    v7_refreshclock();

    while (count > 0) {
        struct a {
            int16_t    fdes;
            char    *cbuf;
            uint16_t count;
        } uap;
        uint16_t writeSize, writeRes;

        writeSize = (count > UINT16_MAX) ? UINT16_MAX : (uint16_t)count;

        uap.fdes = (int16_t)fd;
        uap.cbuf = (char *)buf;
        uap.count = writeSize;
        v7_u.u_ap = &uap;
        v7_u.u_error = 0;
        v7_rdwr(V7_FWRITE);

        if (v7_u.u_error  != 0)
            return -v7_u.u_error;

        writeRes = (uint16_t)v7_u.u_r.r_val1;

        totalWriteSize += writeRes;
        count -= writeRes;
        buf += writeRes;

        if (writeRes != writeSize)
            break;
    }

    return totalWriteSize;
}

/** Write data to a file/directory at a specific offset.
 */
ssize_t v7fs_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = v7fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return v7fs_write(fd, buf, count);
}

/** Truncate file.
 * 
 * NB: Due to limitations in the v7 kernel, the truncate operation will
 * return an error if length is not 0.
 */
int v7fs_truncate(const char *pathname, off_t length)
{
    int res = 0;
    struct v7_inode *ip;

    v7_refreshclock();
    v7_u.u_error = 0;

    v7_u.u_dirp = (char *)pathname;
    ip = v7_namei(&v7_schar, 0);
    if (ip == NULL)
        return -v7_u.u_error;
    if (length == 0) {
        v7_itrunc(ip);
        res = -v7_u.u_error;
    }
    else
        res =  -EINVAL;
    v7_iput(ip);
    return res;
}

/** Get file statistics.
 */
int v7fs_stat(const char *pathname, struct stat *statbuf)
{
    struct v7_inode *ip;
    struct v7_stat v7statbuf;

    v7_refreshclock();
    v7_u.u_error = 0;

    v7_u.u_dirp = (char *)pathname;
    ip = v7_namei(&v7_schar, 0);
    if (ip == NULL)
        return -v7_u.u_error;
    v7_stat1(ip, &v7statbuf, 0);
    if (v7_u.u_error != 0) {
        v7_iput(ip);
        return -v7_u.u_error;
    }
    v7fs_convertstat(&v7statbuf, statbuf);
    v7_iput(ip);
    return 0;
}

/** Delete a directory entry.
 */
int v7fs_unlink(const char *pathname)
{
    v7_refreshclock();
    v7_u.u_error = 0;

    v7_u.u_dirp = (char *)pathname;
    v7_unlink();
    return -v7_u.u_error;
}

/** Rename or move a file/directory.
 */
int v7fs_rename(const char *oldpath, const char *newpath)
{
    struct v7_inode *oldip = NULL, *newip = NULL;
    int res = 0;
    int isdir = 0;
    int rmdirnew = 0;
    int unlinknew = 0;
    char preveuid = v7_u.u_uid;

    v7_refreshclock();
    v7_u.u_error = 0;

    /* get the inode for the source file/dir */
    v7_u.u_dirp = (char *)oldpath;
    oldip = v7_namei(&v7_schar, 0);
    if (oldip == NULL) {
        res = -v7_u.u_error;
        goto exit;
    }
    oldip->i_flag &= ~ILOCK;
    isdir = ((oldip->i_mode & IFMT) == IFDIR);

    /* disallow renaming the root directory, or from/to any directory with the
     * name '.' or '..' */
    if (oldip->i_number == ROOTINO || v7fs_isdirlinkpath(oldpath) || v7fs_isdirlinkpath(newpath)) {
        res = -EBUSY;
        goto exit;
    }

    /* get the inode for the destination file/dir, if it exists */
    v7_u.u_dirp = (char *)newpath;
    newip = v7_namei(&v7_schar, 0);
    if (oldip == NULL && v7_u.u_error != ENOENT) {
        res = -v7_u.u_error;
        goto exit;
    }

    /* if the destination exists... */
    if (newip != NULL) {
        /* if source and destination refer to the same file/dir, then
         * per rename(2) semantics, do nothing */
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
            if (isdir) {
                res = -ENOTDIR;
                goto exit;
            }
            /* arrange to remove the destination file before renaming the source */
            unlinknew = 1;
        }

        v7_iput(newip);
        newip = NULL;
    }

    /* if source is a directory ... */
    if (isdir) {
        /* fail if destination is a child of source */
        res = v7fs_isindir(newpath, oldip->i_number);
        if (res < 0)
            goto exit;
        if (res) {
            res = -EINVAL;
            goto exit;
        }
    }

    v7_iput(oldip);
    oldip = NULL;

    /* if needed, remove an existing directory with the same name as the destination.
     * if the directory is not empty, this will fail. */
    if (rmdirnew) {
        res = v7fs_rmdir(newpath);
        if (res < 0)
            goto exit;
    }

    /* if needed, remove an existing file with the same name as the destination. */
    if (unlinknew) {
        res = v7fs_unlink(newpath);
        if (res < 0)
            goto exit;
    }

    /* perform the following as "set-uid" root. this allows creating additional temporary
     * links to source directory. */
    v7_u.u_uid = 0;

    /* create destination file/dir as a link to the source */
    res = v7fs_link(oldpath, newpath);
    if (res < 0)
        goto exit;

    /* remove the source */
    res = v7fs_unlink(oldpath);
    if (res < 0)
        goto exit;

    /* if a directory move occured, rewrite the ".." entry in the directory to point
     * to the new parent directory. */
    if (isdir)
        res = v7fs_reparentdir(newpath);

exit:
    if (oldip != NULL)
        v7_iput(oldip);
    if (newip != NULL)
        v7_iput(newip);
    v7_u.u_uid = preveuid;
    return res;
}

/** Change permissions of a file.
 */
int v7fs_chmod(const char *pathname, mode_t mode)
{
    struct a {
        char *fname;
        int16_t fmode;
    } uap;

    v7_refreshclock();
    v7_u.u_error = 0;

    uap.fname = (char *)pathname;
    uap.fmode = (int16_t)(mode & 07777);
    v7_u.u_ap = &uap;
    v7_u.u_dirp = (v7_caddr_t)pathname;
    v7_chmod();
    return -v7_u.u_error;
}

/** Change ownership of a file.
 */
int v7fs_chown(const char *pathname, uid_t owner, gid_t group)
{
    int res = 0;
    struct v7_inode *ip;
    char v7uid = v7fs_maphostuid(owner);
    char v7gid = v7fs_maphostgid(group);

    v7_refreshclock();
    v7_u.u_error = 0;

    v7_u.u_dirp = (char *)pathname;
    if ((ip = v7_namei(v7_uchar, 0)) == NULL)
        return -v7_u.u_error;

    if (owner != -1 || group != -1) {
        if (owner != -1 && ip->i_uid != v7uid) {
            if (!v7_suser()) {
                res = -EPERM;
                goto exit;
            }
        }
        if (group != -1 && ip->i_gid != v7gid) {
            if (!v7_suser() && v7_u.u_uid != ip->i_uid) {
                res = -EPERM;
                goto exit;
            }
        }

        if (owner != -1)
            ip->i_uid = v7uid;
        if (group != -1)
            ip->i_gid = v7gid;
        ip->i_flag |= IACC;
    }

exit:
    v7_iput(ip);
    return res;
}

/** Change file last access and modification times
 */
int v7fs_utimens(const char *pathname, const struct timespec times[2])
{
    int res = 0;
    struct v7_inode *ip = NULL;
    v7_time_t atime, mtime;
    int setBothNow;

    if (times == NULL) {
        static const struct timespec sNowTimes[] = {
            { 0, UTIME_NOW },
            { 0, UTIME_NOW },
        };
        times = sNowTimes;
    }

    v7_refreshclock();

    /* lookup the target file */
    v7_u.u_error = 0;
    v7_u.u_dirp = (char *)pathname;
    ip = v7_namei(&v7_schar, 0);
    if (ip == NULL) {
        res = -v7_u.u_error;
        goto exit;
    }

    /* exit immediately if nothing to do */
    if (times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT)
        goto exit;

    /* both times being set to current time? */
    setBothNow = (times[0].tv_nsec == UTIME_NOW && times[1].tv_nsec == UTIME_NOW);

    /* exit with error if the caller doesn't have the appropriate permission. */
    v7_u.u_error = setBothNow ? EACCES : EPERM;
    if (v7_u.u_uid != 0 && v7_u.u_uid != ip->i_uid &&
        (!setBothNow || v7_access(ip, IWRITE))) {
        res = -v7_u.u_error;
        goto exit;
    }

    /* update the inode timestamps */
    v7_refreshclock();
    atime = (times[0].tv_nsec != UTIME_OMIT && times[0].tv_nsec != UTIME_NOW)
        ? (v7_time_t)times[0].tv_sec
        : v7_time;
    mtime = (times[1].tv_nsec != UTIME_OMIT && times[1].tv_nsec != UTIME_NOW)
        ? (v7_time_t)times[1].tv_sec
        : v7_time;
    if (times[0].tv_nsec != UTIME_OMIT)
        ip->i_flag |= IACC;
    if (times[1].tv_nsec != UTIME_OMIT)
        ip->i_flag |= IUPD;
    v7_u.u_error = 0;
    v7_iupdat(ip, &atime, &mtime);
    res = -v7_u.u_error;
    ip->i_flag &= ~(IACC|IUPD);

exit:
    if (ip != NULL)
        v7_iput(ip);
    return res;
}

/** Check user's permissions for a file.
 */
int v7fs_access(const char *pathname, int mode)
{
    int res = 0;
    struct v7_inode *ip;

    v7_refreshclock();
    v7_u.u_error = 0;

    v7_u.u_dirp = (char *)pathname;
    ip = v7_namei(&v7_schar, 0);
    if (ip == NULL)
        return -v7_u.u_error;
    if ((mode & R_OK) != 0 && v7_access(ip, IREAD) != 0)
        res = -v7_u.u_error;
    if ((mode & W_OK) != 0 && v7_access(ip, IWRITE) != 0)
        res = -v7_u.u_error;
    if ((mode & X_OK) != 0 && v7_access(ip, IEXEC) != 0)
        res = -v7_u.u_error;
    v7_iput(ip);
    return res;
}

/** Create a directory.
 */
int v7fs_mkdir(const char *pathname, mode_t mode)
{
    int res;
    struct v7_inode *ip;
    char *namebuf;
    char *parentname;
    int dircreated = 0;
    char preveuid = v7_u.u_uid;

    v7_refreshclock();
    v7_u.u_error = 0;

    namebuf = (char *)malloc(strlen(pathname) + 4);

    strcpy(namebuf, pathname);
    parentname = strdup(dirname(namebuf));

    /* verify that the given pathname does not already exist.
     * in the process, a pointer to the parent directory is
     * set in v7_u.u_pdir. */
    v7_u.u_dirp = (char *)pathname;
    ip = v7_namei(&v7_uchar, 1);
    if (ip != NULL) {
        v7_iput(ip);
        res = -EEXIST;
        goto exit;
    }
    if ((res = -v7_u.u_error) < 0)
        goto exit;

    /* in v7, mkdir is a command that runs set-uid root, 
     * which gives it the priveldge to create links to
     * directory node (in particular the . and .. entries).
     * here we simulate this by switching the effective uid
     * to root. */
    v7_u.u_uid = 0;

    /* make the directory node within the parent directory. 
     * temporarily grant access to the root user only. */
    ip = v7_maknode(IFDIR|0700);
    if (ip == NULL) {
        res = -v7_u.u_error;
        goto exit;
    }
    dircreated = 1;
    v7_iput(ip);
    if ((res = -v7_u.u_error) < 0)
        goto exit;

    /* create the . directory link. */
    strcpy(namebuf, pathname);
    strcat(namebuf, "/.");
    res = v7fs_link(pathname, namebuf);
    if (res < 0)
        goto exit;

    /* create the .. directory link. */
    strcat(namebuf, ".");
    res = v7fs_link(parentname, namebuf);
    if (res < 0)
        goto exit;

    /* change the owner of the new directory from root to the
     * current real user id. */
    v7_u.u_error = 0;
    v7_u.u_dirp = (char *)pathname;
    if ((ip = v7_namei(v7_uchar, 0)) == NULL) {
        res = -v7_u.u_error;
        goto exit;
    }
    ip->i_uid = v7_u.u_ruid;
    ip->i_flag |= IUPD;
    v7_iput(ip);
    if ((res = -v7_u.u_error) < 0)
        goto exit;

    /* set the final access permissions on the new directory. */
    res = v7fs_chmod(pathname, mode&07777);

exit:
    if (res != 0 && dircreated) {
        strcpy(namebuf, pathname);
        strcat(namebuf, "/..");
        v7fs_unlink(namebuf);

        strcpy(namebuf, pathname);
        strcat(namebuf, "/.");
        v7fs_unlink(namebuf);

        v7fs_unlink(pathname);
    }
    v7_u.u_uid = preveuid;
    free(namebuf);
    free(parentname);
    return res;
}

/** Delete a directory.
 */
int v7fs_rmdir(const char *pathname)
{
    struct stat statbuf;
    int res;
    char *dirname;
    char *namebuf;
    char preveuid = v7_u.u_uid;

    namebuf = (char *)malloc(strlen(pathname) + 4);

    res = v7fs_stat(pathname, &statbuf);
    if (res < 0)
        goto exit;

    if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
        res = -ENOTDIR;
        goto exit;
    }

    if (statbuf.st_ino == v7_rootdir->i_number) {
        res = -EBUSY;
        goto exit;
    }

    /* verify directory is empty */
    res = v7fs_enumdir(pathname, v7fs_failnonemptydir, NULL);
    if (res < 0)
        goto exit;

    strcpy(namebuf, pathname);
    dirname = basename(namebuf);
    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        res = -EINVAL;
        goto exit;
    }

    /* perform the following as "set-uid" root */
    v7_u.u_uid = 0;

    strcpy(namebuf, pathname);
    strcat(namebuf, "/..");
    res = v7fs_unlink(namebuf);
    if (res != 0)
        goto exit;

    strcpy(namebuf, pathname);
    strcat(namebuf, "/.");
    res = v7fs_unlink(namebuf);
    if (res != 0)
        goto exit;

    res = v7fs_unlink(pathname);

exit:
    free(namebuf);
    v7_u.u_uid = preveuid;
    return res;
}

/** Enumerate files in a directory.
 */
int v7fs_enumdir(const char *pathname, v7fs_enum_dir_funct enum_funct, void *context)
{
    int fd;
    union {
        struct v7_direct direntry;
        char buf[sizeof(struct v7_direct) + 1];
    } direntrybuf;
    struct v7_stat v7statbuf;
    struct stat statbuf;
    struct v7_inode *ip = NULL;
    int readRes;

    v7_refreshclock();
    v7_u.u_error = 0;

    memset(&direntrybuf, 0, sizeof(direntrybuf));

    fd = v7fs_open(pathname, O_RDONLY, 0);
    if (fd < 0)
        return fd;

    while (1) {
        readRes = v7fs_read(fd, &direntrybuf.direntry, sizeof(struct v7_direct));
        if (readRes <= 0)
            break;
        if (readRes < sizeof(struct v7_direct)) {
            readRes = 0;
            break;
        }

        if (direntrybuf.direntry.d_ino == 0)
            continue;

        v7_u.u_error = 0;
        ip = v7_iget(v7_rootdev, direntrybuf.direntry.d_ino);
        if (ip == NULL) {
            readRes = -v7_u.u_error;
            break;
        }

        v7_u.u_error = 0;
        v7_stat1(ip, &v7statbuf, 0);
        if (v7_u.u_error != 0) {
            readRes = -v7_u.u_error;
            break;
        }

        v7fs_convertstat(&v7statbuf, &statbuf);

        readRes = enum_funct(direntrybuf.direntry.d_name, &statbuf, context);
        if (readRes < 0)
            break;

        v7_iput(ip);
        ip = NULL;
    }

    if (ip != NULL)
        v7_iput(ip);

    v7fs_close(fd);

    return readRes;
}

/** Commit filesystem changes to disk.
 */
int v7fs_sync()
{
    v7_refreshclock();
    v7_u.u_error = 0;
    v7_update();
    int res = dsk_flush();
    if (v7_u.u_error != 0)
        return -v7_u.u_error;
    return res;
}

/** Get filesystem statistics.
 */
int v7fs_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    struct v7_filsys *fp;

    v7_refreshclock();
    v7_u.u_error = 0;

    v7_update();

    memset(statvfsbuf, 0, sizeof(struct statvfs));

    fp = v7_getfs(v7_rootdev);

    statvfsbuf->f_bsize = 512;
    statvfsbuf->f_frsize = 512;
    statvfsbuf->f_blocks = fp->s_fsize;
    statvfsbuf->f_files = (fp->s_isize - 2) * INOPB;
    statvfsbuf->f_namemax = DIRSIZ;

    /* count the number of free blocks.
     *
     * The freelist consist of a table of NICFREE (50) block numbers located
     * in the filesystem superblock (fp->s_free).  Each entry in this table
     * can contain the block number of a free block.  The number of entries
     * that actually contain free block numbers (starting at index 0 in the
     * table) is denoted by the integer fp->s_nfree.
     * 
     * The first block number in the table is special.  If there are more than
     * 49 free blocks, fp->s_free[0] is the number of a free block that also
     * contains pointers to other free blocks.  This is referred to as an
     * "fblk".  In cases where there are 49 or fewer free blocks, fp->s_free[0]
     * will always contain the NULL block number (0), indicating that no fblks
     * exist.

     * Like the superblock, an fblk also contains a table of 50 block numbers
     * (starting at offset 2) and a count of the number table entries that
     * actually contain free blocks (located at offset 0).  Similarly, the
     * first position in the table is also used to point to a subsequent index
     * block in cases where there are more than 49 additional free blocks. Thus
     * fblks are strung together to form a chain of groups of 50 free blocks.

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
        int16_t nfree = fp->s_nfree;
        v7_daddr_t nextfblk = fp->s_free[0];
        v7_daddr_t freelistlen = 0;
        while (1) {
            if (nfree < 0 || nfree > NICFREE) {
                fprintf(stderr, "V7FS ERROR: bad free count in free list\n");
                return -EIO;
            }
            if (nfree > 0) {
                statvfsbuf->f_bfree += (nextfblk != 0) ? nfree : (nfree - 1);
            }
            if (nfree == 0 || nextfblk == 0)
                break;
            freelistlen++;
            if (freelistlen > fp->s_fsize) {
                fprintf(stderr, "V7FS ERROR: loop detected in free list\n");
                return -EIO;
            }
            if (v7_badblock(fp, nextfblk, v7_rootdev)) {
                fprintf(stderr, "V7FS ERROR: invalid block number on free list\n");
                return -EIO;
            }
            struct v7_buf * bp = v7_bread(v7_rootdev, nextfblk);
            v7_geterror(bp);
            if (v7_u.u_error != 0)
                return -v7_u.u_error;
            struct v7_fblk * fb = (struct v7_fblk *)bp->b_un.b_addr;
            nfree = fb->df_nfree;
            nextfblk = fs_htopdp_i32(fb->df_free[0]);
            v7_brelse(bp);
        }
    }
    statvfsbuf->f_bavail = statvfsbuf->f_bfree;

    /* count the number of free inodes. */
    for (v7_daddr_t blk = 2; blk < fp->s_isize; blk++) {
        struct v7_buf * bp = v7_bread(v7_rootdev, blk);
        v7_geterror(bp);
        if (v7_u.u_error != 0)
            return -v7_u.u_error;
        for (int16_t i = 0; i < INOPB; i++) {
            if (bp->b_un.b_dino[i].di_mode == 0)
                statvfsbuf->f_ffree++;
        }
        v7_brelse(bp);
    }

    return 0;
}

/** Set the real and/or effective user ids for filesystem operations.
 */
int v7fs_setreuid(uid_t ruid, uid_t euid)
{
    if (ruid != -1)
        v7_u.u_ruid = v7fs_maphostuid(ruid);
    if (euid != -1)
        v7_u.u_uid = v7fs_maphostuid(euid);
    return 0;
}

/** Set the real and/or effective group ids for filesystem operations.
 */
int v7fs_setregid(gid_t rgid, gid_t egid)
{
    if (rgid != -1)
        v7_u.u_rgid = v7fs_maphostgid(rgid);
    if (egid != -1)
        v7_u.u_gid = v7fs_maphostgid(egid);
    return 0;
}

/** Add an entry to the uid mapping table.
 */
int v7fs_adduidmap(uid_t hostuid, uint32_t fsuid)
{
    return idmap_addidmap(&v7fs_uidmap, (uint32_t)hostuid, fsuid);
}

/** Add an entry to the gid mapping table.
 */
int v7fs_addgidmap(uid_t hostgid, uint32_t fsgid)
{
    return idmap_addidmap(&v7fs_gidmap, (uint32_t)hostgid, fsgid);
}

/** Construct the initial free block list for a new filesystem.
 *
 * This function is effectively identical to the bflist() function in the
 * v7 mkfs command.  However, unlike the original, this adaptation omits
 * code for creating the bad block file, which is handled in v7fs_mkfs()
 * instead.
 * 
 * n and m are values used to create an initial interleave in the order
 * that blocks appear on the free list.  The v7 mkfs command used default
 * values of n=500,m=3 for all devices unless overriden on the command line.
 */
static void v7fs_initfreelist(struct v7_filsys *fp, uint16_t n, uint16_t m)
{
    uint8_t flg[V7FS_MAXFN];
    int16_t adr[V7FS_MAXFN];
    int16_t i, j;
    v7_daddr_t f, d;
    
    for (i = 0; i < n; i++)
        flg[i] = 0;
    i = 0;
    for (j = 0; j < n; j++) {
        while (flg[i])
            i = (i+1)%n;
        adr[j] = i+1;
        flg[i]++;
        i = (i+m)%n;
    }

    d = fp->s_fsize-1;
    while (d%n)
        d++;
    for (; d > 0; d -= n)
        for (i=0; i<n; i++) {
            f = d - adr[i];
            if (f < fp->s_fsize && f >= fp->s_isize)
                v7_free(v7_rootdev, f);
        }
}

static void v7fs_convertstat(const struct v7_stat *v7statbuf, struct stat *statbuf)
{
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_dev = v7statbuf->st_dev;
    statbuf->st_ino = v7statbuf->st_ino;
    statbuf->st_nlink = v7statbuf->st_nlink;
    statbuf->st_uid = v7fs_mapv7uid(v7statbuf->st_uid);
    statbuf->st_gid = v7fs_mapv7gid(v7statbuf->st_gid);
    statbuf->st_size = (off_t)v7statbuf->st_size;
    statbuf->st_blocks = (statbuf->st_size + 511) >> 9;
    switch (v7statbuf->st_mode & V7_S_IFMT) {
    case V7_S_IFDIR:
        statbuf->st_mode = S_IFDIR;
        break;
    case V7_S_IFCHR:
    case V7_S_IFMPC:
        statbuf->st_mode = S_IFCHR;
        statbuf->st_rdev = v7statbuf->st_rdev;
        break;
    case V7_S_IFBLK:
    case V7_S_IFMPB:
        statbuf->st_mode = S_IFBLK;
        statbuf->st_rdev = v7statbuf->st_rdev;
        break;
    default:
        statbuf->st_mode = S_IFREG;
        break;
    }
    statbuf->st_mode |= (v7statbuf->st_mode & 07777);
    statbuf->st_atime = v7statbuf->v7_st_atime;
    statbuf->st_mtime = v7statbuf->v7_st_mtime;
    statbuf->st_ctime = v7statbuf->v7_st_ctime;
}

static int v7fs_isindir(const char *pathname, int16_t dirnum)
{
    char *pathnamecopy, *p;
    int res = 0;

    pathnamecopy = strdup(pathname);
    if (pathnamecopy == NULL)
        return 1;

    p = pathnamecopy;
    do {
        p = dirname(p);
        v7_u.u_dirp = p;
        struct v7_inode *ip = v7_namei(&v7_uchar, 1);
        if (ip != NULL) {
            res = (ip->i_number == dirnum);
            v7_iput(ip);
        }
        else if (v7_u.u_error != ENOENT)
            res = -v7_u.u_error;
    } while (!res && strcmp(p, "/") != 0 && strcmp(p, ".") != 0);

    free(pathnamecopy);
    return res;
}

static int v7fs_failnonemptydir(const char *entryname, const struct stat *statbuf, void *context)
{
    if ((statbuf->st_mode & S_IFMT) == S_IFDIR && (strcmp(entryname, ".") == 0 || strcmp(entryname, "..") == 0))
        return 0;
    return -ENOTEMPTY;
}

static int v7fs_isdirlinkpath(const char *pathname)
{
    const char * p = strrchr(pathname, '/');
    if (p == NULL)
        p = pathname;
    else
        p++;
    return (strcmp(p, ".") == 0 || strcmp(p, "..") == 0);
}

static int v7fs_reparentdir(const char *pathname)
{
    int res = 0;
    char * pathnamecopy = strdup(pathname);
    char * newparentname = dirname(pathnamecopy);
    struct v7_inode *newparentip = NULL;
    struct v7_inode *oldparentip = NULL;
    int fd = -1;
    struct v7_direct direntry;

    /* get inode of new parent directory */
    v7_u.u_dirp = newparentname;
    newparentip = v7_namei(&v7_uchar, 0);
    if (newparentip == NULL) {
        res = -v7_u.u_error;
        goto exit;
    }
    newparentip->i_flag &= ~ILOCK;

    /* open the target directory */
    res = fd = v7fs_open(pathname, O_RDONLY, 0);
    if (res < 0)
        goto exit;
    v7_u.u_ofile[fd]->f_flag |= V7_FWRITE; /* bypass error check in open1() */

    /* scan for the ".." entry... */
    while (1) {
        res = v7fs_read(fd, &direntry, sizeof(struct v7_direct));
        if (res < 0)
            goto exit;
        if (res < sizeof(struct v7_direct)) {
            res = -EIO; /* ".." entry not found! */
            goto exit;
        }
        if (direntry.d_name[0] == '.' && direntry.d_name[1] == '.' && direntry.d_name[2] == 0)
            break;
    }
    res = 0;

    /* if the ".." entry does *not* contain the inode number of the
     * new parent directory... */
    if (direntry.d_ino != newparentip->i_number) {

        /* get the inode for the old parent directory. */
        oldparentip = v7_iget(newparentip->i_dev, direntry.d_ino);
        if (oldparentip == NULL) {
            res = -EIO; /* old parent not found */
            goto exit;
        }

        /* decrement the link count on the old parent directory. */
        oldparentip->i_nlink--;
        oldparentip->i_flag |= IACC;
        v7_iupdat(oldparentip, &v7_time, &v7_time);
        res = -v7_u.u_error;
        if (res != 0)
            goto exit;

        /* rewrite the ".." directory entry to contain the inode of
         * the new parent directory. */
        direntry.d_ino = newparentip->i_number;
        res = v7fs_seek(fd, -((off_t)sizeof(struct v7_direct)), SEEK_CUR);
        if (res < 0)
            goto exit;
        res = v7fs_write(fd, &direntry, sizeof(struct v7_direct));
        if (res < 0)
            goto exit;
        if (res < sizeof(struct v7_direct)) {
            res = -EIO;
            goto exit;
        }

        /* increment the link count on the new parent directory. */
        newparentip->i_nlink++;
        newparentip->i_flag |= IACC;
        v7_iupdat(newparentip, &v7_time, &v7_time);
        res = -v7_u.u_error;
        if (res != 0)
            goto exit;
    }

exit:
    if (fd != -1)
        v7fs_close(fd);
    if (newparentip != NULL)
        v7_iput(newparentip);
    if (oldparentip != NULL)
        v7_iput(oldparentip);
    free(pathnamecopy);
    return res;
}
