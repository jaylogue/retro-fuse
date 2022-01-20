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
 * @file  Implements a modern interface to a 2.9BSD filesystem.
 * 
 * The functions implemented here closely mimic the modern Unix filesystem
 * API, with the notible exception that errors are returned as return
 * values rather than via errno.
 * 
 * While intended to support a FUSE filesystem implementation, these functions 
 * are independent of FUSE code and thus could be used in other contexts.
 */

#define _XOPEN_SOURCE 700
#define _ATFILE_SOURCE 
#define _DARWIN_C_SOURCE
#define _GNU_SOURCE

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

#include "bsd29fs.h"
#include "dskio.h"
#include "idmap.h"

#include "bsd29adapt.h"
#include "bsd29/include/sys/param.h"
#include "bsd29/include/sys/dir.h"
#include "bsd29/include/sys/user.h"
#include "bsd29/include/sys/buf.h"
#include "bsd29/include/sys/systm.h"
#include "bsd29/include/sys/inode.h"
#include "bsd29/include/sys/ino.h"
#include "bsd29/include/sys/file.h"
#include "bsd29/include/sys/filsys.h"
#include "bsd29/include/sys/fblk.h"
#include "bsd29/include/sys/mount.h"
#include "bsd29/include/sys/stat.h"
#include "bsd29/include/sys/conf.h"
#include "bsd29unadapt.h"

enum {
    NIPB = (BSIZE/sizeof(struct bsd29_dinode)), /* from mkfs.c, same as INOPB */
    NDIRECT = (BSIZE/sizeof(struct bsd29_direct)),
#ifndef	CLSIZE
    CLSIZE = 1
#endif
};

static int bsd29fs_initialized = 0;

static struct idmap bsd29fs_uidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = BSD29FS_MAX_UID_GID,
    .defaulthostid = 65534, // nobody
    .defaultfsid = BSD29FS_MAX_UID_GID
};

static struct idmap bsd29fs_gidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = BSD29FS_MAX_UID_GID,
    .defaulthostid = 65534, // nogroup
    .defaultfsid = BSD29FS_MAX_UID_GID
};

static void bsd29fs_initfreelist(struct bsd29_filsys *fp, uint16_t n, uint16_t m);
static void bsd29fs_convertstat(const struct bsd29_stat *fsstatbuf, struct stat *statbuf);
static int bsd29fs_isindir(const char *pathname, int16_t dirnum);
static int bsd29fs_failnonemptydir(const char *entryname, const struct stat *statbuf, void *context);
static int bsd29fs_isdirlinkpath(const char *pathname);
static int bsd29fs_reparentdir(const char *pathname);
static inline int16_t bsd29fs_maphostuid(uid_t hostuid) { return (int16_t)idmap_tofsid(&bsd29fs_uidmap, (uint32_t)hostuid); }
static inline int16_t bsd29fs_maphostgid(gid_t hostgid) { return (int16_t)idmap_tofsid(&bsd29fs_gidmap, (uint32_t)hostgid); }
static inline uid_t bsd29fs_mapfsuid(int16_t fsuid) { return (uid_t)idmap_tohostid(&bsd29fs_uidmap, (uint32_t)fsuid); }
static inline gid_t bsd29fs_mapfsgid(int16_t fsgid) { return (gid_t)idmap_tohostid(&bsd29fs_gidmap, (uint32_t)fsgid); }

/** Initialize the 2.9BSD filesystem.
 * 
 * This function is very similar to the BSD kernel main() function.
 */
int bsd29fs_init(int readonly)
{
    if (bsd29fs_initialized)
        return -EBUSY;

    /* zero kernel data structures and globals */
    bsd29_zerocore();

    /* set the device id for the root device. */
    bsd29_rootdev = bsd29_makedev(0, 0);

    bsd29_u.u_error = 0;

#ifdef	UCB_IHASH
	bsd29_ihinit();
#endif

    /* initialize the buffer pool. */
    bsd29_binit();
    if (bsd29_u.u_error != 0)
        return -bsd29_u.u_error;

    /* mount the root device and read the superblock. */
    bsd29_iinit();
    if (bsd29_u.u_error != 0)
        return -bsd29_u.u_error;

    struct bsd29_filsys *fs = bsd29_getfs(bsd29_rootdev);

    /* if the size of the underlying virtual disk is unknown, or
       is smaller than the size declared in the filesystem superblock
       set the virtual disk size to the size from the superblock. */
    off_t dsksize = dsk_getsize();
    off_t fsize = wswap_int32(fs->s_fsize) * 2; /* adjust to 512-byte blocks */
    if (dsksize == 0 || dsksize > fsize)
        dsk_setsize((off_t)fsize);

    /* mark the filesystem read-only if requested. */
    if (readonly)
        fs->s_ronly = 1;

    /* get the root directory inode. */
    bsd29_rootdir = bsd29_iget(bsd29_rootdev, ROOTINO);
    bsd29_rootdir->i_flag &= ~ILOCK;

    /* set the user's current directory. */
    bsd29_u.u_cdir = bsd29_iget(bsd29_rootdev, ROOTINO);
    bsd29_u.u_cdir->i_flag &= ~ILOCK;
    bsd29_u.u_rdir = NULL;

    bsd29fs_initialized = 1;

    return 0;
}

/** Shutdown the filesystem, sycning any unwritten data.
 */
int bsd29fs_shutdown()
{
    int res = 0;
    if (bsd29fs_initialized) {
        res = bsd29fs_sync();
        bsd29_zerocore();
        bsd29fs_initialized = 0;
    }
    return res;
}

/** Initialize a new filesystem.
 * 
 * Thus function is effectly a simplified re-implementation of the
 * BSD mkfs command.
 */
int bsd29fs_mkfs(uint32_t fssize, uint32_t isize, const struct bsd29fs_flparams *flparams)
{
    struct bsd29_buf *bp;
    struct bsd29_filsys *fp;
    struct bsd29_dinode rootino, lfino, bbino;
    struct bsd29_direct *dp;
    bsd29_daddr_t bn;
    uint16_t fn = flparams->n, fm = flparams->m;

    enum {
        BADBLOCKINO = ROOTINO - 1,
        LOSTFOUNDINO = ROOTINO + 1
    };

    if (bsd29fs_initialized)
        return -EBUSY;

    /* enforce min/max filesystem size.  2.9BSD filesystems are limited to a max of
     * 2^24-1 blocks due to 3 byte block numbers stored in inodes. */
    if (fssize < BSD29FS_MIN_FS_SIZE || fssize > BSD29FS_MAX_FS_SIZE)
        return -EINVAL;

    /* if not specified, compute the number of inode blocks based on the
     * filesystem size (based on logic in mkfs) */
    if (isize == 0) {
        if (fssize <= 5000/CLSIZE)
            isize = fssize/50;
        else
            isize = fssize/25;
        if(isize <= 0)
            isize = 1;
        if(isize > BSD29FS_MAX_ITABLE_SIZE)
            isize = BSD29FS_MAX_ITABLE_SIZE;
    }

    /* enforce min/max inode table size.  max size is limited by the size of
     * integer used to store inode numbers (uint16_t) and the overall size
     * of the filesystem. */
    if (isize < BSD29FS_MIN_ITABLE_SIZE || isize > BSD29FS_MAX_ITABLE_SIZE || isize > (fssize - 4))
        return -EINVAL;

    /* adjust interleave values as needed (based on logic in mkfs) */
    if (fn == 0) {
        /* use default values */
        fn = 10;
        fm = 5;
    }
    if (fn >= BSD29FS_MAXFN)
        fn = BSD29FS_MAXFN;
    if (fm == 0 || fm > fn)
        fm = 3;

    bsd29_refreshclock();

    /* zero kernel data structures and globals */
    bsd29_zerocore();

    /* set the device id for the root device. */
    bsd29_rootdev = bsd29_makedev(0, 0);

    /* initialize the buffer pool. */
    bsd29_binit();

    bsd29_u.u_error = 0;

    /* create the mount for the root device. */
    bsd29_mount[0].m_dev = bsd29_rootdev;
    bsd29_mount[0].m_inodp = (struct bsd29_inode *) 1; /* as done in iinit() */

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
    fp = &bsd29_mount[0].m_filsys;
    fp->s_isize = isize + 2; /* adjusted to point to the block beyond the i-list */
    fp->s_fsize = wswap_int32(fssize);
    fp->s_n = (int16_t)fn;
    fp->s_m = (int16_t)fm;
    fp->s_tfree = 0;
    fp->s_tinode = 0;
    fp->s_time = wswap_int32(bsd29_time);
    fp->s_fmod = 1;

    /* initialize the free block list on disk. */
    bsd29fs_initfreelist(fp, fn, fm);
    if (bsd29_u.u_error != 0)
        goto exit;

    /* initialize the inode structure for the lost+found directory
     *
     * note that, per the logic in the original BSD mkfs, the size of
     * the lost+found directory is set to 4 blocks (4096 bytes), even
     * though the directory initially contains only two entries. */
    memset(&lfino, 0, sizeof(lfino));
    lfino.di_mode = (int16_t)(IFDIR|ISVTX|0777);
    lfino.di_nlink = 2;
    lfino.di_uid = bsd29_u.u_uid;
    lfino.di_gid = bsd29_u.u_gid;
    lfino.di_size = wswap_int32(4 * BSIZE); /* pre-allocated to 4 blocks */
    lfino.di_atime = lfino.di_mtime = lfino.di_ctime = wswap_int32(bsd29_time);

    /* allocate 5 blocks to the lost+found directory (4 direct and 1
     * single-level indirect). initialize the first block to contain the
     * '.' and '..' directory entries. set the rest to all zeros.
     *
     * the reason for adding the 5th single-level indirect block is unclear
     * and may simply be a bug. it is created by logic in the iput() function
     * in mkfs.c. the behavior is preserved here so that the result of this
     * function can be byte-by-byte compared for correctness against disk
     * images created by the original mkfs. */
    for (int i = 0; i < 5; i++) {
        bp = bsd29_alloc(bsd29_rootdev);
        if (bp == NULL)
            goto exit;
        bn = bsd29_dbtofsb(bp->b_blkno);
        if (i == 0) {
            dp = (struct bsd29_direct *)bp->b_un.b_addr;
            dp->d_name[0] = '.';
            dp->d_ino = LOSTFOUNDINO;
            dp++;
            dp->d_name[0] = '.';
            dp->d_name[1] = '.';
            dp->d_ino = ROOTINO;
        }
        bsd29_bwrite(bp);
        if ((bp->b_flags & B_ERROR) != 0)
            goto exit;
        lfino.di_addr[(i*3) + 0] = (char)(bn >> 16);
        lfino.di_addr[(i*3) + 1] = (char)(bn);
        lfino.di_addr[(i*3) + 2] = (char)(bn >> 8);
    }

    /* initialize the inode structure for the root directory */
    memset(&rootino, 0, sizeof(rootino));
    rootino.di_mode = (int16_t)(IFDIR|0777);
    rootino.di_nlink = 3;
    rootino.di_uid = bsd29_u.u_uid;
    rootino.di_gid = bsd29_u.u_gid;
    rootino.di_size = wswap_int32(3*sizeof(struct bsd29_direct)); /* size of 3 directory entries */
    rootino.di_atime = rootino.di_mtime = rootino.di_ctime = wswap_int32(bsd29_time);

    /* allocate block for root directory and initialize it to
     * contain the '.', '..' and 'lost+found' entries. */
    bp = bsd29_alloc(bsd29_rootdev);
    if (bp == NULL)
        goto exit;
    bn = bsd29_dbtofsb(bp->b_blkno);
    dp = (struct bsd29_direct *)bp->b_un.b_addr;
    dp->d_name[0] = '.';
    dp->d_ino = ROOTINO;
    dp++;
    dp->d_name[0] = '.';
    dp->d_name[1] = '.';
    dp->d_ino = ROOTINO;
    dp++;
    strcpy(dp->d_name, "lost+found");
    dp->d_ino = LOSTFOUNDINO;
    bsd29_bwrite(bp);
    if ((bp->b_flags & B_ERROR) != 0)
        goto exit;
    rootino.di_addr[0] = (char)(bn >> 16);
    rootino.di_addr[1] = (char)(bn);
    rootino.di_addr[2] = (char)(bn >> 8);

    /* initialize the inode structure for the bad block file. */
    memset(&bbino, 0, sizeof(bbino));
    bbino.di_mode = (int16_t)(IFREG);
    bbino.di_nlink = 0;
    bbino.di_uid = 0;
    bbino.di_gid = 0;
    bbino.di_size = 0;
    bbino.di_atime = bbino.di_mtime = bbino.di_ctime = wswap_int32(bsd29_time);

    /* initialize the inode table on disk. setup the first 3 inodes (in inode block 0)
     * to represent the bad block file, the root directory and the lost+found directory,
     * respectively. */
    for (bsd29_daddr_t iblk = 0; iblk < isize; iblk++) {
        bp = bsd29_getblk(bsd29_rootdev, iblk + 2);
        bsd29_clrbuf(bp);
        fp->s_tinode += INOPB;
        if (iblk == 0) {
            struct bsd29_dinode * ip = (struct bsd29_dinode *)bp->b_un.b_addr;
            ip[BADBLOCKINO - 1] = bbino;
            ip[ROOTINO - 1] = rootino;
            ip[LOSTFOUNDINO - 1] = lfino;
            fp->s_tinode -= 3;
        }
        bsd29_bwrite(bp);
        if ((bp->b_flags & B_ERROR) != 0)
            goto exit;
    }

    /* sync the superblock */
    bsd29_update();

exit:
    bsd29_zerocore();
    return -bsd29_u.u_error;
}

/** Open file or directory in the filesystem.
 */
int bsd29fs_open(const char *name, int flags, mode_t mode)
{
    struct bsd29_inode *ip;
    int16_t m;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    switch (flags & O_ACCMODE) {
    case O_RDWR:
        m = BSD29_FREAD | BSD29_FWRITE;
        break;
    case O_RDONLY:
    default:
        m = BSD29_FREAD;
        break;
    case O_WRONLY:
        m = BSD29_FWRITE;
        break;
    }
    bsd29_u.u_dirp = (char *)name;
    ip = bsd29_namei(&bsd29_schar, BSD29_LOOKUP, 0);
    if (ip == NULL) {
        if (bsd29_u.u_error != ENOENT)
            return -bsd29_u.u_error;
        if ((flags & O_CREAT) == 0)
            return -bsd29_u.u_error;
        ip = bsd29_maknode(mode & 07777 & (~ISVTX));
        if (ip == NULL)
            return -bsd29_u.u_error;
        bsd29_open1(ip, m, 2);
    }
    else {
        if ((flags & O_CREAT) != 0 && (flags & O_EXCL) != 0) {
            bsd29_iput(ip);
            return -EEXIST;
        }
        bsd29_open1(ip, m, ((flags & O_TRUNC) != 0) ? 1 : 0);
    }
    if (bsd29_u.u_error != 0)
        return -bsd29_u.u_error;
    return bsd29_u.u_r.r_val1;
}

/** Close file/directory.
 */
int bsd29fs_close(int fd)
{
	struct a {
		int16_t	fdes;
	} uap;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    uap.fdes = fd;
    bsd29_u.u_ap = &uap;
    bsd29_close();
    return 0;
}

/** Seek file/directory.
 */
off_t bsd29fs_seek(int fd, off_t offset, int whence)
{
    struct bsd29_file *fp;
    off_t curSize;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    fp = bsd29_getf((int16_t)fd);
    if (fp == NULL)
        return -bsd29_u.u_error;
    if (fp->f_flag & FPIPE)
        return -ESPIPE;
    curSize = (off_t)fp->f_inode->i_size;
    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        offset += (off_t)fp->f_un.f_offset;
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
    fp->f_un.f_offset = (bsd29_off_t)(offset);
    return offset;
}

/** Create a hard link.
 */
int bsd29fs_link(const char *oldpath, const char *newpath)
{
    struct a {
        char    *target;
        char    *linkname;
    } uap;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    uap.target = (char *)oldpath;
    uap.linkname = (char *)newpath;
    bsd29_u.u_ap = &uap;
    bsd29_u.u_dirp = (bsd29_caddr_t)oldpath;
    bsd29_link();
    return -bsd29_u.u_error;
}

/** Create a new file, block or character device node.
 */
int bsd29fs_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    struct bsd29_inode *ip;
    int16_t fsmode;
    bsd29_dev_t fsdev;

    switch (mode & IFMT)
    {
    case 0:
    case S_IFREG:
        fsmode = IFREG;
        fsdev = 0;
        break;
    case S_IFBLK:
    case S_IFCHR:
        if (!bsd29_suser())
            return -EPERM;
        fsmode = (S_ISBLK(mode)) ? IFBLK : IFCHR;
        fsdev = bsd29_makedev(major(dev) & 0xFF, minor(dev) & 0xFF);
        break;
    default:
        return -EEXIST;
    }
    fsmode |= (mode & 07777);

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    bsd29_u.u_dirp = (char *)pathname;
    ip = bsd29_namei(&bsd29_uchar, BSD29_CREATE, 0);
    if (ip != NULL) {
        bsd29_iput(ip);
        return -EEXIST;
    }
    if (bsd29_u.u_error != 0)
        return -bsd29_u.u_error;
    ip = bsd29_maknode(fsmode);
    if (ip == NULL)
        return -bsd29_u.u_error;
    ip->i_un.i_rdev = (bsd29_daddr_t)fsdev;
    bsd29_iput(ip);
    return -bsd29_u.u_error;
}

/** Read data from a file/directory.
 */
ssize_t bsd29fs_read(int fd, void *buf, size_t count)
{
    ssize_t totalReadSize = 0;
    
    bsd29_refreshclock();

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
        bsd29_u.u_ap = &uap;
        bsd29_u.u_error = 0;
        bsd29_rdwr(BSD29_FREAD);

        if (bsd29_u.u_error != 0)
            return -bsd29_u.u_error;

        readRes = (uint16_t)bsd29_u.u_r.r_val1;
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
ssize_t bsd29fs_pread(int fd, void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = bsd29fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return bsd29fs_read(fd, buf, count);
}

/** Write data to a file/directory.
 */
ssize_t bsd29fs_write(int fd, const void *buf, size_t count)
{
    ssize_t totalWriteSize = 0;
    
    bsd29_refreshclock();

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
        bsd29_u.u_ap = &uap;
        bsd29_u.u_error = 0;
        bsd29_rdwr(BSD29_FWRITE);

        if (bsd29_u.u_error  != 0)
            return -bsd29_u.u_error;

        writeRes = (uint16_t)bsd29_u.u_r.r_val1;

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
ssize_t bsd29fs_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = bsd29fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return bsd29fs_write(fd, buf, count);
}

/** Truncate file.
 * 
 * NB: Due to limitations in the 2.9BSD kernel, the truncate operation will
 * return an error if length is not 0.
 */
int bsd29fs_truncate(const char *pathname, off_t length)
{
    int res = 0;
    struct bsd29_inode *ip;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    bsd29_u.u_dirp = (char *)pathname;
    ip = bsd29_namei(&bsd29_schar, BSD29_LOOKUP, 0);
    if (ip == NULL)
        return -bsd29_u.u_error;
    if (length == 0) {
        bsd29_itrunc(ip);
        res = -bsd29_u.u_error;
    }
    else
        res =  -EINVAL;
    bsd29_iput(ip);
    return res;
}

/** Get file statistics.
 */
int bsd29fs_stat(const char *pathname, struct stat *statbuf)
{
    struct bsd29_inode *ip;
    struct bsd29_stat fsstatbuf;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    bsd29_u.u_dirp = (char *)pathname;
    ip = bsd29_namei(&bsd29_schar, BSD29_LOOKUP, 0);
    if (ip == NULL)
        return -bsd29_u.u_error;
    bsd29_stat1(ip, &fsstatbuf, 0);
    if (bsd29_u.u_error != 0) {
        bsd29_iput(ip);
        return -bsd29_u.u_error;
    }
    bsd29fs_convertstat(&fsstatbuf, statbuf);
    bsd29_iput(ip);
    return 0;
}

/** Delete a directory entry.
 */
int bsd29fs_unlink(const char *pathname)
{
    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    bsd29_u.u_dirp = (char *)pathname;
    bsd29_unlink();
    return -bsd29_u.u_error;
}

/** Rename or move a file/directory.
 */
int bsd29fs_rename(const char *oldpath, const char *newpath)
{
    struct bsd29_inode *oldip = NULL, *newip = NULL;
    int res = 0;
    int isdir = 0;
    int rmdirnew = 0;
    int unlinknew = 0;
    char preveuid = bsd29_u.u_uid;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    /* get the inode for the source file/dir */
    bsd29_u.u_dirp = (char *)oldpath;
    oldip = bsd29_namei(&bsd29_schar, BSD29_LOOKUP, 0);
    if (oldip == NULL) {
        res = -bsd29_u.u_error;
        goto exit;
    }
    oldip->i_flag &= ~ILOCK;
    isdir = ((oldip->i_mode & IFMT) == IFDIR);

    /* disallow renaming the root directory, or from/to any directory with the
     * name '.' or '..' */
    if (oldip->i_number == ROOTINO || bsd29fs_isdirlinkpath(oldpath) || bsd29fs_isdirlinkpath(newpath)) {
        res = -EBUSY;
        goto exit;
    }

    /* get the inode for the destination file/dir, if it exists */
    bsd29_u.u_dirp = (char *)newpath;
    newip = bsd29_namei(&bsd29_schar, BSD29_LOOKUP, 0);
    if (oldip == NULL && bsd29_u.u_error != ENOENT) {
        res = -bsd29_u.u_error;
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

        bsd29_iput(newip);
        newip = NULL;
    }

    /* if source is a directory ... */
    if (isdir) {
        /* fail if destination is a child of source */
        res = bsd29fs_isindir(newpath, oldip->i_number);
        if (res < 0)
            goto exit;
        if (res) {
            res = -EINVAL;
            goto exit;
        }
    }

    bsd29_iput(oldip);
    oldip = NULL;

    /* if needed, remove an existing directory with the same name as the destination.
     * if the directory is not empty, this will fail. */
    if (rmdirnew) {
        res = bsd29fs_rmdir(newpath);
        if (res < 0)
            goto exit;
    }

    /* if needed, remove an existing file with the same name as the destination. */
    if (unlinknew) {
        res = bsd29fs_unlink(newpath);
        if (res < 0)
            goto exit;
    }

    /* perform the following as "set-uid" root. this allows creating addition temporary
     * links to source directory. */
    bsd29_u.u_uid = 0;

    /* create destination file/dir as a link to the source */
    res = bsd29fs_link(oldpath, newpath);
    if (res < 0)
        goto exit;

    /* remove the source */
    res = bsd29fs_unlink(oldpath);
    if (res < 0)
        goto exit;

    /* if a directory move occured, rewrite the ".." entry in the directory to point
     * to the new parent directory. */
    if (isdir)
        res = bsd29fs_reparentdir(newpath);

exit:
    if (oldip != NULL)
        bsd29_iput(oldip);
    if (newip != NULL)
        bsd29_iput(newip);
    bsd29_u.u_uid = preveuid;
    return res;
}

/** Change permissions of a file.
 */
int bsd29fs_chmod(const char *pathname, mode_t mode)
{
    struct a {
        char *fname;
        int16_t fmode;
    } uap;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    uap.fname = (char *)pathname;
    uap.fmode = (int16_t)(mode & 07777);
    bsd29_u.u_ap = &uap;
    bsd29_u.u_dirp = (bsd29_caddr_t)pathname;
    bsd29_chmod();
    return -bsd29_u.u_error;
}

/** Change ownership of a file.
 */
int bsd29fs_chown(const char *pathname, uid_t owner, gid_t group)
{
    int res = 0;
    struct bsd29_inode *ip;
    int16_t fsuid = bsd29fs_maphostuid(owner);
    int16_t fsgid = bsd29fs_maphostgid(group);

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    bsd29_u.u_dirp = (char *)pathname;
    if ((ip = bsd29_namei(bsd29_uchar, BSD29_LOOKUP, 0)) == NULL)
        return -bsd29_u.u_error;

    if (owner != -1 || group != -1) {
        if (owner != -1 && ip->i_uid != fsuid) {
            if (!bsd29_suser()) {
                res = -EPERM;
                goto exit;
            }
        }
        if (group != -1 && ip->i_gid != fsgid) {
            if (!bsd29_suser() && bsd29_u.u_uid != ip->i_uid) {
                res = -EPERM;
                goto exit;
            }
        }

        if (owner != -1)
            ip->i_uid = fsuid;
        if (group != -1)
            ip->i_gid = fsgid;
        ip->i_flag |= IACC;
    }

exit:
    bsd29_iput(ip);
    return res;
}

/** Change file last access and modification times
 */
int bsd29fs_utimens(const char *pathname, const struct timespec times[2])
{
    int res = 0;
    struct bsd29_inode *ip = NULL;
    bsd29_time_t atime, mtime;
    int setBothNow;

    if (times == NULL) {
        static const struct timespec sNowTimes[] = {
            { 0, UTIME_NOW },
            { 0, UTIME_NOW },
        };
        times = sNowTimes;
    }

    bsd29_refreshclock();

    /* lookup the target file */
    bsd29_u.u_error = 0;
    bsd29_u.u_dirp = (char *)pathname;
    ip = bsd29_namei(&bsd29_schar, BSD29_LOOKUP, 0);
    if (ip == NULL) {
        res = -bsd29_u.u_error;
        goto exit;
    }

    /* exit immediately if nothing to do */
    if (times[0].tv_nsec == UTIME_OMIT && times[1].tv_nsec == UTIME_OMIT)
        goto exit;

    /* both times being set to current time? */
    setBothNow = (times[0].tv_nsec == UTIME_NOW && times[1].tv_nsec == UTIME_NOW);

    /* exit with error if the caller doesn't have the appropriate permission. */
    bsd29_u.u_error = setBothNow ? EACCES : EPERM;
    if (bsd29_u.u_uid != 0 && bsd29_u.u_uid != ip->i_uid &&
        (!setBothNow || bsd29_access(ip, IWRITE))) {
        res = -bsd29_u.u_error;
        goto exit;
    }

    /* update the inode timestamps */
    bsd29_refreshclock();
    atime = (times[0].tv_nsec != UTIME_OMIT && times[0].tv_nsec != UTIME_NOW)
        ? (bsd29_time_t)times[0].tv_sec
        : bsd29_time;
    mtime = (times[1].tv_nsec != UTIME_OMIT && times[1].tv_nsec != UTIME_NOW)
        ? (bsd29_time_t)times[1].tv_sec
        : bsd29_time;
    if (times[0].tv_nsec != UTIME_OMIT)
        ip->i_flag |= IACC;
    if (times[1].tv_nsec != UTIME_OMIT)
        ip->i_flag |= IUPD;
    bsd29_u.u_error = 0;
#ifdef UCB_FSFIX
    bsd29_iupdat(ip, &atime, &mtime, 1);
#else
    bsd29_iupdat(ip, &atime, &mtime);
#endif
    res = -bsd29_u.u_error;
    ip->i_flag &= ~(IACC|IUPD);

exit:
    if (ip != NULL)
        bsd29_iput(ip);
    return res;
}

/** Check user's permissions for a file.
 */
int bsd29fs_access(const char *pathname, int mode)
{
    int res = 0;
    struct bsd29_inode *ip;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    bsd29_u.u_dirp = (char *)pathname;
    ip = bsd29_namei(&bsd29_schar, BSD29_LOOKUP, 0);
    if (ip == NULL)
        return -bsd29_u.u_error;
    if ((mode & R_OK) != 0 && bsd29_access(ip, IREAD) != 0)
        res = -bsd29_u.u_error;
    if ((mode & W_OK) != 0 && bsd29_access(ip, IWRITE) != 0)
        res = -bsd29_u.u_error;
    if ((mode & X_OK) != 0 && bsd29_access(ip, IEXEC) != 0)
        res = -bsd29_u.u_error;
    bsd29_iput(ip);
    return res;
}

/** Read the value of a symbolic link
 */
ssize_t bsd29fs_readlink(const char *pathname, char *buf, size_t bufsiz)
{
    struct a {
        const char *name;
        char *buf;
        int16_t	count;
    } uap;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    uap.name = pathname;
    uap.buf = buf;
    uap.count = (uint16_t)bufsiz;
    bsd29_u.u_ap = &uap;
    bsd29_u.u_dirp = (bsd29_caddr_t)pathname;
    bsd29_readlink();
    if (bsd29_u.u_error == 0)
        return bsd29_u.u_r.r_val1;
    else
        return -bsd29_u.u_error;
}

/** Make a new symbolic link for a file
 */
int bsd29fs_symlink(const char *target, const char *linkpath)
{
    struct a {
        const char *target;
        const char *linkname;
    } uap;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    uap.target = target;
    uap.linkname = linkpath;
    bsd29_u.u_ap = &uap;
    bsd29_u.u_dirp = (bsd29_caddr_t)target;
    bsd29_symlink();
    return -bsd29_u.u_error;
}

/** Create a directory.
 */
int bsd29fs_mkdir(const char *pathname, mode_t mode)
{
    int res;
    struct bsd29_inode *ip;
    char *namebuf;
    char *parentname;
    int dircreated = 0;
    char preveuid = bsd29_u.u_uid;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    namebuf = (char *)malloc(strlen(pathname) + 4);

    strcpy(namebuf, pathname);
    parentname = strdup(dirname(namebuf));

    /* verify that the given pathname does not already exist.
     * in the process, a pointer to the parent directory is
     * set in bsd29_u.u_pdir. */
    bsd29_u.u_dirp = (char *)pathname;
    ip = bsd29_namei(&bsd29_uchar, BSD29_CREATE, 0);
    if (ip != NULL) {
        bsd29_iput(ip);
        res = -EEXIST;
        goto exit;
    }
    if ((res = -bsd29_u.u_error) < 0)
        goto exit;

    /* mkdir is a command that runs set-uid root, 
     * which gives it the priveldge to create links to
     * directory node (in particular the . and .. entries).
     * here we simulate this by switching the effective uid
     * to root. */
    bsd29_u.u_uid = 0;

    /* make the directory node within the parent directory. 
     * temporarily grant access to the root user only. */
    ip = bsd29_maknode(IFDIR|0700);
    if (ip == NULL) {
        res = -bsd29_u.u_error;
        goto exit;
    }
    dircreated = 1;
    bsd29_iput(ip);
    if ((res = -bsd29_u.u_error) < 0)
        goto exit;

    /* create the . directory link. */
    strcpy(namebuf, pathname);
    strcat(namebuf, "/.");
    res = bsd29fs_link(pathname, namebuf);
    if (res < 0)
        goto exit;

    /* create the .. directory link. */
    strcat(namebuf, ".");
    res = bsd29fs_link(parentname, namebuf);
    if (res < 0)
        goto exit;

    /* change the owner of the new directory from root to the
     * current real user id. */
    bsd29_u.u_error = 0;
    bsd29_u.u_dirp = (char *)pathname;
    if ((ip = bsd29_namei(bsd29_uchar, BSD29_LOOKUP, 0)) == NULL) {
        res = -bsd29_u.u_error;
        goto exit;
    }
    ip->i_uid = bsd29_u.u_ruid;
    ip->i_flag |= IUPD;
    bsd29_iput(ip);
    if ((res = -bsd29_u.u_error) < 0)
        goto exit;

    /* set the final access permissions on the new directory. */
    res = bsd29fs_chmod(pathname, mode&07777);

exit:
    if (res != 0 && dircreated) {
        strcpy(namebuf, pathname);
        strcat(namebuf, "/..");
        bsd29fs_unlink(namebuf);

        strcpy(namebuf, pathname);
        strcat(namebuf, "/.");
        bsd29fs_unlink(namebuf);

        bsd29fs_unlink(pathname);
    }
    bsd29_u.u_uid = preveuid;
    free(namebuf);
    free(parentname);
    return res;
}

/** Delete a directory.
 */
int bsd29fs_rmdir(const char *pathname)
{
    struct stat statbuf;
    int res;
    char *dirname;
    char *namebuf;
    char preveuid = bsd29_u.u_uid;

    namebuf = (char *)malloc(strlen(pathname) + 4);

    res = bsd29fs_stat(pathname, &statbuf);
    if (res < 0)
        goto exit;

    if ((statbuf.st_mode & S_IFMT) != S_IFDIR) {
        res = -ENOTDIR;
        goto exit;
    }

    if (statbuf.st_ino == bsd29_rootdir->i_number) {
        res = -EBUSY;
        goto exit;
    }

    /* verify directory is empty */
    res = bsd29fs_enumdir(pathname, bsd29fs_failnonemptydir, NULL);
    if (res < 0)
        goto exit;

    strcpy(namebuf, pathname);
    dirname = basename(namebuf);
    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        res = -EINVAL;
        goto exit;
    }

    /* perform the following as "set-uid" root */
    bsd29_u.u_uid = 0;

    strcpy(namebuf, pathname);
    strcat(namebuf, "/..");
    res = bsd29fs_unlink(namebuf);
    if (res != 0)
        goto exit;

    strcpy(namebuf, pathname);
    strcat(namebuf, "/.");
    res = bsd29fs_unlink(namebuf);
    if (res != 0)
        goto exit;

    res = bsd29fs_unlink(pathname);

exit:
    free(namebuf);
    bsd29_u.u_uid = preveuid;
    return res;
}

/** Enumerate files in a directory.
 */
int bsd29fs_enumdir(const char *pathname, bsd29fs_enum_dir_funct enum_funct, void *context)
{
    int fd;
    union {
        struct bsd29_direct direntry;
        char buf[sizeof(struct bsd29_direct) + 1];
    } direntrybuf;
    struct bsd29_stat fsstatbuf;
    struct stat statbuf;
    struct bsd29_inode *ip = NULL;
    int readRes;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    memset(&direntrybuf, 0, sizeof(direntrybuf));

    fd = bsd29fs_open(pathname, O_RDONLY, 0);
    if (fd < 0)
        return fd;

    while (1) {
        readRes = bsd29fs_read(fd, &direntrybuf.direntry, sizeof(struct bsd29_direct));
        if (readRes <= 0)
            break;
        if (readRes < sizeof(struct bsd29_direct)) {
            readRes = 0;
            break;
        }

        if (direntrybuf.direntry.d_ino == 0)
            continue;

        bsd29_u.u_error = 0;
        ip = bsd29_iget(bsd29_rootdev, direntrybuf.direntry.d_ino);
        if (ip == NULL) {
            readRes = -bsd29_u.u_error;
            break;
        }

        bsd29_u.u_error = 0;
        bsd29_stat1(ip, &fsstatbuf, 0);
        if (bsd29_u.u_error != 0) {
            readRes = -bsd29_u.u_error;
            break;
        }

        bsd29fs_convertstat(&fsstatbuf, &statbuf);

        readRes = enum_funct(direntrybuf.direntry.d_name, &statbuf, context);
        if (readRes < 0)
            break;

        bsd29_iput(ip);
        ip = NULL;
    }

    if (ip != NULL)
        bsd29_iput(ip);

    bsd29fs_close(fd);

    return readRes;
}

/** Commit filesystem changes to disk.
 */
int bsd29fs_sync()
{
    bsd29_refreshclock();
    bsd29_u.u_error = 0;
    bsd29_update();
    int res = dsk_flush();
    if (bsd29_u.u_error != 0)
        return -bsd29_u.u_error;
    return res;
}

/** Get filesystem statistics.
 */
int bsd29fs_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    struct bsd29_filsys *fp;

    bsd29_refreshclock();
    bsd29_u.u_error = 0;

    bsd29_update();

    memset(statvfsbuf, 0, sizeof(struct statvfs));

    fp = bsd29_getfs(bsd29_rootdev);

    statvfsbuf->f_bsize = BSIZE;
    statvfsbuf->f_frsize = BSIZE;
    statvfsbuf->f_blocks = wswap_int32(fp->s_fsize);
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
        bsd29_daddr_t nextfblk = wswap_int32(fp->s_free[0]);
        bsd29_daddr_t freelistlen = 0;
        while (1) {
            if (nfree < 0 || nfree > NICFREE) {
                fprintf(stderr, "BSD29FS ERROR: bad free count in free list\n");
                return -EIO;
            }
            if (nfree > 0) {
                statvfsbuf->f_bfree += (nextfblk != 0) ? nfree : (nfree - 1);
            }
            if (nfree == 0 || nextfblk == 0)
                break;
            freelistlen++;
            if (freelistlen > wswap_int32(fp->s_fsize)) {
                fprintf(stderr, "BSD29FS ERROR: loop detected in free list\n");
                return -EIO;
            }
            if (bsd29_badblock(fp, nextfblk)) {
                fprintf(stderr, "BSD29FS ERROR: invalid block number on free list\n");
                return -EIO;
            }
            struct bsd29_buf * bp = bsd29_bread(bsd29_rootdev, nextfblk);
            bsd29_geterror(bp);
            if (bsd29_u.u_error != 0)
                return -bsd29_u.u_error;
            struct bsd29_fblk * fb = (struct bsd29_fblk *)bp->b_un.b_addr;
            nfree = fb->df_nfree;
            nextfblk = wswap_int32(fb->df_free[0]);
            bsd29_brelse(bp);
        }
    }
    statvfsbuf->f_bavail = statvfsbuf->f_bfree;

    /* count the number of free inodes. */
    for (bsd29_daddr_t blk = SUPERB+1; blk < fp->s_isize; blk++) {
        struct bsd29_buf * bp = bsd29_bread(bsd29_rootdev, blk);
        bsd29_geterror(bp);
        if (bsd29_u.u_error != 0)
            return -bsd29_u.u_error;
        for (int16_t i = 0; i < INOPB; i++) {
            if (bp->b_un.b_dino[i].di_mode == 0)
                statvfsbuf->f_ffree++;
        }
        bsd29_brelse(bp);
    }

    return 0;
}

/** Set the real and/or effective user ids for filesystem operations.
 */
int bsd29fs_setreuid(uid_t ruid, uid_t euid)
{
    if (ruid != -1)
        bsd29_u.u_ruid = bsd29fs_maphostuid(ruid);
    if (euid != -1)
        bsd29_u.u_uid = bsd29fs_maphostuid(euid);
    return 0;
}

/** Set the real and/or effective group ids for filesystem operations.
 */
int bsd29fs_setregid(gid_t rgid, gid_t egid)
{
    if (rgid != -1)
        bsd29_u.u_rgid = bsd29fs_maphostgid(rgid);
    if (egid != -1)
        bsd29_u.u_gid = bsd29fs_maphostgid(egid);
    return 0;
}

/** Add an entry to the uid mapping table.
 */
int bsd29fs_adduidmap(uid_t hostuid, uint32_t fsuid)
{
    return idmap_addidmap(&bsd29fs_uidmap, (uint32_t)hostuid, fsuid);
}

/** Add an entry to the gid mapping table.
 */
int bsd29fs_addgidmap(uid_t hostgid, uint32_t fsgid)
{
    return idmap_addidmap(&bsd29fs_gidmap, (uint32_t)hostgid, fsgid);
}

/** Construct the initial free block list for a new filesystem.
 *
 * This function is effectively identical to the bflist() function in the
 * BSD mkfs command.  However, unlike the original, this adaptation omits
 * code for creating the bad block file, which is handled in bsd29fs_mkfs()
 * instead.
 * 
 * n and m are values used to create an initial interleave in the order
 * that blocks appear on the free list.  The BSD mkfs command used default
 * values of n=500,m=3 for all devices unless overriden on the command line.
 */
static void bsd29fs_initfreelist(struct bsd29_filsys *fp, uint16_t n, uint16_t m)
{
    uint8_t flg[BSD29FS_MAXFN];
    int16_t adr[BSD29FS_MAXFN];
    int16_t i, j;
    bsd29_daddr_t f, d;
    
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

    d = wswap_int32(fp->s_fsize)-1;
    while (d%n)
        d++;
    for (; d > 0; d -= n)
        for (i=0; i<n; i++) {
            f = d - adr[i];
            if (f < wswap_int32(fp->s_fsize) && f >= fp->s_isize)
                bsd29_free(bsd29_rootdev, f);
        }
}

static void bsd29fs_convertstat(const struct bsd29_stat *fsstatbuf, struct stat *statbuf)
{
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_dev = fsstatbuf->st_dev;
    statbuf->st_ino = fsstatbuf->st_ino;
    statbuf->st_nlink = fsstatbuf->st_nlink;
    statbuf->st_uid = bsd29fs_mapfsuid(fsstatbuf->st_uid);
    statbuf->st_gid = bsd29fs_mapfsgid(fsstatbuf->st_gid);
    statbuf->st_size = (off_t)fsstatbuf->st_size;
    statbuf->st_blocks = (statbuf->st_size + (BSIZE-1)) / BSIZE;
    switch (fsstatbuf->st_mode & BSD29_S_IFMT) {
    case BSD29_S_IFDIR:
        statbuf->st_mode = S_IFDIR;
        break;
    case BSD29_S_IFCHR:
    case BSD29_S_IFMPC:
        statbuf->st_mode = S_IFCHR;
        statbuf->st_rdev = fsstatbuf->st_rdev;
        break;
    case BSD29_S_IFBLK:
    case BSD29_S_IFMPB:
        statbuf->st_mode = S_IFBLK;
        statbuf->st_rdev = fsstatbuf->st_rdev;
        break;
    case BSD29_S_IFLNK:
        statbuf->st_mode = S_IFLNK;
        break;
    default:
        statbuf->st_mode = S_IFREG;
        break;
    }
    statbuf->st_mode |= (fsstatbuf->st_mode & 07777);
    statbuf->st_atime = fsstatbuf->bsd29_st_atime;
    statbuf->st_mtime = fsstatbuf->bsd29_st_mtime;
    statbuf->st_ctime = fsstatbuf->bsd29_st_ctime;
}

static int bsd29fs_isindir(const char *pathname, int16_t dirnum)
{
    char *pathnamecopy, *p;
    int res = 0;

    pathnamecopy = strdup(pathname);
    if (pathnamecopy == NULL)
        return 1;

    p = pathnamecopy;
    do {
        p = dirname(p);
        bsd29_u.u_dirp = p;
        struct bsd29_inode *ip = bsd29_namei(&bsd29_uchar, BSD29_LOOKUP, 0);
        if (ip != NULL) {
            res = (ip->i_number == dirnum);
            bsd29_iput(ip);
        }
        else if (bsd29_u.u_error != ENOENT)
            res = -bsd29_u.u_error;
    } while (!res && strcmp(p, "/") != 0 && strcmp(p, ".") != 0);

    free(pathnamecopy);
    return res;
}

static int bsd29fs_failnonemptydir(const char *entryname, const struct stat *statbuf, void *context)
{
    if ((statbuf->st_mode & S_IFMT) == S_IFDIR && (strcmp(entryname, ".") == 0 || strcmp(entryname, "..") == 0))
        return 0;
    return -ENOTEMPTY;
}

static int bsd29fs_isdirlinkpath(const char *pathname)
{
    const char * p = strrchr(pathname, '/');
    if (p == NULL)
        p = pathname;
    else
        p++;
    return (strcmp(p, ".") == 0 || strcmp(p, "..") == 0);
}

static int bsd29fs_reparentdir(const char *pathname)
{
    int res = 0;
    char * pathnamecopy = strdup(pathname);
    char * newparentname = dirname(pathnamecopy);
    struct bsd29_inode *newparentip = NULL;
    struct bsd29_inode *oldparentip = NULL;
    int fd = -1;
    struct bsd29_direct direntry;

    /* get inode of new parent directory */
    bsd29_u.u_dirp = newparentname;
    newparentip = bsd29_namei(&bsd29_uchar, BSD29_LOOKUP, 0);
    if (newparentip == NULL) {
        res = -bsd29_u.u_error;
        goto exit;
    }
    newparentip->i_flag &= ~ILOCK;

    /* open the target directory */
    res = fd = bsd29fs_open(pathname, O_RDONLY, 0);
    if (res < 0)
        goto exit;
    bsd29_u.u_ofile[fd]->f_flag |= BSD29_FWRITE; /* bypass error check in open1() */

    /* scan for the ".." entry... */
    while (1) {
        res = bsd29fs_read(fd, &direntry, sizeof(struct bsd29_direct));
        if (res < 0)
            goto exit;
        if (res < sizeof(struct bsd29_direct)) {
            res = -EIO; /* ".." entry not found! */
            goto exit;
        }
        if (direntry.d_name[0] == '.' && direntry.d_name[1] == '.' && direntry.d_name[2] == 0)
            break;
    }
    res = 0;

    /* if the ".." entry does *not* contain the inode number of the new parent directory... */
    if (direntry.d_ino != newparentip->i_number) {

        /* get the inode for the old parent directory. */
        oldparentip = bsd29_iget(newparentip->i_dev, direntry.d_ino);
        if (oldparentip == NULL) {
            res = -EIO; /* old parent not found */
            goto exit;
        }

        /* decrement the link count on the old parent directory. */
        oldparentip->i_nlink--;
        oldparentip->i_flag |= ICHG;
#ifdef UCB_FSFIX
        bsd29_iupdat(oldparentip, &bsd29_time, &bsd29_time, 1);
#else
        bsd29_iupdat(oldparentip, &bsd29_time, &bsd29_time);
#endif
        res = -bsd29_u.u_error;
        if (res != 0)
            goto exit;

        /* rewrite the ".." directory entry to contain the inode of
         * the new parent directory. */
        direntry.d_ino = newparentip->i_number;
        res = bsd29fs_seek(fd, -sizeof(struct bsd29_direct), SEEK_CUR);
        if (res < 0)
            goto exit;
        res = bsd29fs_write(fd, &direntry, sizeof(struct bsd29_direct));
        if (res < 0)
            goto exit;
        if (res < sizeof(struct bsd29_direct)) {
            res = -EIO;
            goto exit;
        }

        /* increment the link count on the new parent directory. */
        newparentip->i_nlink++;
        newparentip->i_flag |= ICHG;
#ifdef UCB_FSFIX
        bsd29_iupdat(newparentip, &bsd29_time, &bsd29_time, 1);
#else
        bsd29_iupdat(newparentip, &bsd29_time, &bsd29_time);
#endif
        res = -bsd29_u.u_error;
        if (res != 0)
            goto exit;
    }

exit:
    if (fd != -1)
        bsd29fs_close(fd);
    if (newparentip != NULL)
        bsd29_iput(newparentip);
    if (oldparentip != NULL)
        bsd29_iput(oldparentip);
    free(pathnamecopy);
    return res;
}
