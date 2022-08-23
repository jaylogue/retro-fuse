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
 * @file  Implements a modern interface to a 2.11BSD filesystem.
 * 
 * The functions implemented here closely mimic the modern Unix filesystem
 * API, with the notible exception that errors are returned as return
 * values rather than via errno.
 * 
 * While intended to support a FUSE filesystem implementation, these functions 
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

#include "bsd211fs.h"
#include "dskio.h"
#include "idmap.h"

#include "bsd211adapt.h"
#include "bsd211/h/param.h"
#include "bsd211/h/systm.h"
#include "bsd211/h/user.h"
#include "bsd211/h/fs.h"
#include "bsd211/h/file.h"
#include "bsd211/h/buf.h"
#include "bsd211/h/mount.h"
#include "bsd211/h/inode.h"
#include "bsd211/h/stat.h"
#include "bsd211/h/namei.h"
#include "bsd211/h/kernel.h"
#include "bsd211/include/fcntl.h"
#include "bsd211unadapt.h"

static int bsd211fs_initialized = 0;

static struct idmap bsd211fs_uidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = BSD211FS_MAX_UID_GID,
    .defaulthostid = 65534, // nobody
    .defaultfsid = BSD211FS_MAX_UID_GID
};

static struct idmap bsd211fs_gidmap = {
    .entrycount = 0,
    .entries = NULL,
    .maxfsid = BSD211FS_MAX_UID_GID,
    .defaulthostid = 65534, // nogroup
    .defaultfsid = BSD211FS_MAX_UID_GID
};

static void bsd211fs_initfreelist(struct bsd211_fs *fp, uint16_t n, uint16_t m);
static int bsd211fs_initrootdir(struct bsd211_fs *fs);
static void bsd211fs_convertstat(const struct bsd211_stat *fsstatbuf, struct stat *statbuf);
static inline int16_t bsd211fs_maphostuid(uid_t hostuid) { return (int16_t)idmap_tofsid(&bsd211fs_uidmap, (uint32_t)hostuid); }
static inline int16_t bsd211fs_maphostgid(gid_t hostgid) { return (int16_t)idmap_tofsid(&bsd211fs_gidmap, (uint32_t)hostgid); }
static inline uid_t bsd211fs_mapfsuid(int16_t fsuid) { return (uid_t)idmap_tohostid(&bsd211fs_uidmap, (uint32_t)fsuid); }
static inline gid_t bsd211fs_mapfsgid(int16_t fsgid) { return (gid_t)idmap_tohostid(&bsd211fs_gidmap, (uint32_t)fsgid); }

// TODO: add support for getting/setting iflags

/** Initialize the 2.9BSD filesystem.
 * 
 * This function is very similar to the BSD kernel main() function.
 */
int bsd211fs_init(int readonly)
{
    int res;
    struct bsd211_buf *bp;
    struct bsd211_fs *fs;
    enum {
        BSD211_SUPERB = 1,
        BSD211_ROOTINO = 2,
    };

    if (bsd211fs_initialized)
        return -EBUSY;

    /* initialize kernel data structures and globals */
    bsd211_zerocore();
    bsd211_ihinit();
    bsd211_bhinit();
    bsd211_binit();
    bsd211_nchinit();

    /* construct the device id for the root device. */
    bsd211_rootdev = bsd211_makedev(0, 0);

    /* read the superblock */
    bp = bsd211_bread(bsd211_rootdev, BSD211_SUPERB);
    if ((res = -bsd211_geterror(bp)) != 0)
        return res;

    /* mount the root filesystem, marking it read-only if requested. */
    bsd211_mount[0].m_inodp = (struct bsd211_inode *)0; /* as done in main() */
    bsd211_mount[0].m_dev = bsd211_rootdev;
    fs = &bsd211_mount[0].m_filsys;
    memcpy(fs, bp->b_un.b_addr, sizeof(struct bsd211_fs));
    bsd211_brelse(bp);
    fs->fs_ronly = (readonly != 0);
    if (!readonly)
        fs->fs_fmod = 1;
    fs->fs_ilock = 0;
    fs->fs_flock = 0;
    fs->fs_nbehind = 0;
    fs->fs_lasti = 1;
    fs->fs_flags = (readonly != 0) ? MNT_RDONLY : 0;
    if (fs->fs_flags & MNT_CLEAN)
        fs->fs_flags |= MNT_WASCLEAN;
    if (fs->fs_fmod) {
        res = -bsd211_ufs_sync(&bsd211_mount[0]);
        if (res != 0)
            return res;
    }

    /* get the root directory inode. */
    bsd211_rootdir = bsd211_iget(bsd211_rootdev, fs, (ino_t)BSD211_ROOTINO);
    if (bsd211_rootdir == NULL)
        return -EIO;
    bsd211_iunlock(bsd211_rootdir);

    /* set the user's current directory. */
    bsd211_u.u_cdir = bsd211_iget(bsd211_rootdev, fs, (ino_t)BSD211_ROOTINO);
    if (bsd211_u.u_cdir == NULL)
        return -EIO;
    bsd211_iunlock(bsd211_u.u_cdir);
    bsd211_u.u_rdir = NULL;

    /* initialize user groups and limits */
    for (int i = 0; i < NGROUPS; i++)
        bsd211_u.u_groups[i] = NOGROUP;
    for (int i = 0; i < sizeof(bsd211_u.u_rlimit)/sizeof(bsd211_u.u_rlimit[0]); i++)
        bsd211_u.u_rlimit[i].rlim_cur = bsd211_u.u_rlimit[i].rlim_max = BSD211_RLIM_INFINITY;

    bsd211fs_initialized = 1;

    return 0;
}

/** Shutdown the filesystem, sycning any unwritten data.
 */
int bsd211fs_shutdown()
{
    int res = 0;
    if (bsd211fs_initialized) {
        res = bsd211_ufs_sync(&bsd211_mount[0]);
        bsd211_zerocore();
        bsd211fs_initialized = 0;
    }
    return res;
}

/** Initialize a new filesystem.
 * 
 * Thus function is effectly a simplified re-implementation of the
 * BSD mkfs command.
 */
int bsd211fs_mkfs(uint32_t fssize, uint32_t iratio, const struct bsd211fs_flparams *flparams)
{
    int res = 0;
    struct bsd211_fs *fs;
    struct bsd211_buf *bp;
    uint32_t isize;
    uint16_t fn = flparams->n, fm = flparams->m;

    if (bsd211fs_initialized)
        return -EBUSY;

    /* enforce min/max filesystem size. */
    if (fssize < BSD211FS_MIN_FS_SIZE || fssize > BSD211FS_MAX_FS_SIZE)
        return -EINVAL;

    /* compute the number of inode blocks based on the filesystem size and the given
     * inode ratio value (i.e. filesystem bytes/inode).  based on logic in mkfs. */
    if (iratio == 0)
        iratio = 4096;
    isize = (dbtob(fssize) / iratio) / INOPB;
    if	(isize < BSD211FS_MIN_ITABLE_SIZE)
        isize = BSD211FS_MIN_ITABLE_SIZE;
    if	(isize > BSD211FS_MAX_ITABLE_SIZE)
        isize = BSD211FS_MAX_ITABLE_SIZE;

    /* enforce min/max inode table size based on the overall size of the filesystem. */
    if (isize > (fssize - 4))
        return -EINVAL;

    /* adjust interleave values as needed (based on logic in mkfs) */
    if (fn == 0) {
        /* use default values */
        fn = 100;
        fm = 2;
    }
    if (fn >= BSD211FS_MAX_FN)
        fn = BSD211FS_MAX_FN;
    if (fm == 0 || fm > fn)
        fm = 3;

    /* initialize kernel data structures and globals */
    bsd211_zerocore();
    bsd211_ihinit();
    bsd211_bhinit();
    bsd211_binit();
    bsd211_nchinit();

    /* construct the device id for the root device. */
    bsd211_rootdev = bsd211_makedev(0, 0);

    bsd211_refreshclock();
    bsd211_u.u_error = 0;

    /* create the mount for the filesystem. */
    bsd211_mount[0].m_inodp = (struct bsd211_inode *)0; /* as done in main() */
    bsd211_mount[0].m_dev = bsd211_rootdev;

    /* initialize the filesystem superblock in memory. */
    fs = &bsd211_mount[0].m_filsys;
    memset(fs, 0, sizeof(*fs));
    fs->fs_fsize = wswap_int32(fssize);
    fs->fs_isize = isize + 2; /* adjusted to point to the block beyond the i-list */
	fs->fs_step = fm;
	fs->fs_cyl = fn;
    fs->fs_time = wswap_int32(bsd211_time.tv_sec);
    fs->fs_fmod = 1;

    /* clear the inode table on disk. */
    for (bsd211_daddr_t iblk = 2; iblk < fs->fs_isize; iblk++) {
        bp = bsd211_getblk(bsd211_rootdev, iblk);
        bsd211_clrbuf(bp);
        bsd211_bwrite(bp);
        if (bsd211_u.u_error != 0)
            return -bsd211_u.u_error;
        fs->fs_tinode += INOPB;
    }

    /* initialize the free block list on disk. */
    bsd211fs_initfreelist(fs, fn, fm);

    /* create the initial directory structure. */
    res = bsd211fs_initrootdir(fs);
    if (res != 0)
        goto exit;

    /* sync the filesystem to disk */
    res = bsd211fs_sync();

exit:
    bsd211_zerocore();
    return res;
}

/** Open file or directory in the filesystem.
 */
int bsd211fs_open(const char *name, int flags, mode_t mode)
{
    struct a {
        const char *fname;
        int16_t mode;
        int16_t crtmode;
    } uap;

    uap.fname = name;
    uap.mode = (int16_t)(flags & O_ACCMODE);
    uap.crtmode = (int16_t)(mode & 07777);

    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_open();
    if (bsd211_u.u_error != 0)
        return -bsd211_u.u_error;
    return bsd211_u.u_r.r_val1;
}

/** Close file/directory.
 */
int bsd211fs_close(int fd)
{
    struct a {
        int16_t	fdes;
    } uap;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    uap.fdes = fd;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_close();
    return -bsd211_u.u_error;
}

/** Seek file/directory.
 */
off_t bsd211fs_seek(int fd, off_t offset, int whence)
{
    struct bsd211_file *fp;
    off_t curSize;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    if ((fp = bsd211_getf((int16_t)fd)) == NULL)
        return -bsd211_u.u_error;
    if (fp->f_type != DTYPE_INODE) {
        return -ESPIPE;
    }
    curSize = (off_t)(((struct bsd211_inode *)fp->f_data)->i_size);
    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        offset += (off_t)fp->f_offset;
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
    fp->f_offset = (bsd211_off_t)(offset);
    return offset;
}

/** Create a hard link.
 */
int bsd211fs_link(const char *oldpath, const char *newpath)
{
    struct a {
        char    *target;
        char    *linkname;
    } uap;

    uap.target = (char *)oldpath;
    uap.linkname = (char *)newpath;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_link();
    return -bsd211_u.u_error;
}

/** Create a new file, block or character device node.
 */
int bsd211fs_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    struct bsd211_inode *ip;
    struct bsd211_nameidata nd;
    struct bsd211_nameidata *ndp = &nd;
    int16_t fsmode;

    if (!S_ISREG(mode) && !bsd211_suser())
        return -EPERM;

    switch (mode & S_IFMT) {
    case S_IFREG:
    case 0:
        fsmode = IFREG;
        break;
    case S_IFBLK:
        fsmode = IFBLK;
        break;
    case S_IFCHR:
        fsmode = IFCHR;
        break;
    default:
        return -EPERM;
    }
    fsmode |= (mode & 07777);

    bsd211_refreshclock();
    bsd211_u.u_error = 0;

    NDINIT(ndp, CREATE, NOFOLLOW, UIO_USERSPACE, (char *)pathname);
    ip = bsd211_namei(ndp);
    if (ip != NULL) {
        bsd211_iput(ip);
        return -EEXIST;
    }
    if (bsd211_u.u_error)
        return -bsd211_u.u_error;
    ip = bsd211_maknode(fsmode, ndp);
    if (ip == NULL)
        return -bsd211_u.u_error;
    switch (ip->i_mode & IFMT) {
    case IFCHR:
    case IFBLK:
        ip->i_rdev = bsd211_makedev(major(dev) & 0xFF, minor(dev) & 0xFF);
        ip->i_dummy = 0;
        ip->i_flag |= IACC|IUPD|ICHG;
        break;
    }
    bsd211_iput(ip);
    return 0;
}

/** Read data from a file/directory.
 */
ssize_t bsd211fs_read(int fd, void *buf, size_t count)
{
    ssize_t totalReadSize = 0;

    while (count > 0) {
        struct a {
            int16_t  fdes;
            char    *cbuf;
            uint16_t count;
        } uap;
        uint16_t readRes;

        uap.fdes = (int16_t)fd;
        uap.cbuf = (char *)buf;
        uap.count = (count > UINT16_MAX) ? UINT16_MAX : (uint16_t)count;
        bsd211_u.u_ap = &uap;
        bsd211_u.u_error = 0;
        bsd211_refreshclock();
        bsd211_read();

        if (bsd211_u.u_error != 0)
            return -bsd211_u.u_error;

        readRes = (uint16_t)bsd211_u.u_r.r_val1;
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
ssize_t bsd211fs_pread(int fd, void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = bsd211fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return bsd211fs_read(fd, buf, count);
}

/** Write data to a file/directory.
 */
ssize_t bsd211fs_write(int fd, const void *buf, size_t count)
{
    ssize_t totalWriteSize = 0;

    while (count > 0) {
        struct a {
            int16_t   fdes;
            char     *cbuf;
            uint16_t  count;
        } uap;
        uint16_t writeSize, writeRes;

        writeSize = (count > UINT16_MAX) ? UINT16_MAX : (uint16_t)count;

        uap.fdes = (int16_t)fd;
        uap.cbuf = (char *)buf;
        uap.count = writeSize;
        bsd211_u.u_ap = &uap;
        bsd211_u.u_error = 0;
        bsd211_refreshclock();
        bsd211_write();

        if (bsd211_u.u_error  != 0)
            return -bsd211_u.u_error;

        writeRes = (uint16_t)bsd211_u.u_r.r_val1;

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
ssize_t bsd211fs_pwrite(int fd, const void *buf, size_t count, off_t offset)
{
    off_t seekRes;
    
    seekRes = bsd211fs_seek(fd, offset, SEEK_SET);
    if (seekRes < 0)
        return seekRes;

    return bsd211fs_write(fd, buf, count);
}

/** Truncate a file to a specified length
 */
int bsd211fs_truncate(const char *pathname, off_t length)
{
    struct a {
        const char *fname;
        bsd211_off_t length;
    } uap;

    if (length < 0 || length > INT32_MAX)
        return -EINVAL;

    uap.fname = pathname;
    uap.length = (bsd211_off_t)length;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_truncate();
    return -bsd211_u.u_error;
}

/** Truncate an open file to a specified length
 */
int bsd211fs_ftruncate(int fd, off_t length)
{
    struct a {
        int16_t	fd;
        off_t	length;
    } uap;

    if (fd < 0 || fd > INT16_MAX)
        return -EINVAL;
    if (length < 0 || length > INT32_MAX)
        return -EINVAL;

    uap.fd = fd;
    uap.length = (bsd211_off_t)length;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_ftruncate();
    return -bsd211_u.u_error;
}

/** Get file statistics.
 */
int bsd211fs_stat(const char *pathname, struct stat *statbuf)
{
    struct bsd211_stat fsstatbuf;
    struct a {
        const char *fname;
        struct bsd211_stat *ub;
    } uap;

    uap.fname = pathname;
    uap.ub = &fsstatbuf;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_stat1(BSD211_FOLLOW);
    if (bsd211_u.u_error  != 0)
        return -bsd211_u.u_error;
    bsd211fs_convertstat(&fsstatbuf, statbuf);
    return 0;
}

/** Delete a directory entry.
 */
int bsd211fs_unlink(const char *pathname)
{
    struct a {
        const char *fname;
    } uap;

    uap.fname = pathname;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_unlink();
    return -bsd211_u.u_error;
}

/** Rename or move a file/directory.
 */
int bsd211fs_rename(const char *oldpath, const char *newpath)
{
    struct a {
        const char *from;
        const char *to;
    } uap;

    uap.from = oldpath;
    uap.to = newpath;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_rename();
    return -bsd211_u.u_error;
}

/** Change permissions of a file.
 */
int bsd211fs_chmod(const char *pathname, mode_t mode)
{
    struct a {
        const char *fname;
        int16_t fmode;
    } uap;

    uap.fname = pathname;
    uap.fmode = (int16_t)(mode & 07777);
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_chmod();
    return -bsd211_u.u_error;
}

/** Change ownership of a file.
 */
int bsd211fs_chown(const char *pathname, uid_t owner, gid_t group)
{
    int16_t fsuid = bsd211fs_maphostuid(owner);
    int16_t fsgid = bsd211fs_maphostgid(group);
    struct bsd211_inode *ip;
    struct bsd211_nameidata nd;
    struct bsd211_nameidata *ndp = &nd;

    bsd211_refreshclock();

    bsd211_u.u_error = 0;
    NDINIT(ndp, LOOKUP, NOFOLLOW, UIO_USERSPACE, (char *)pathname);
    ip = bsd211_namei(ndp);
    if (ip == NULL)
        return -bsd211_u.u_error;

    if (owner != -1 || group != -1) {
        if (owner != -1 && ip->i_uid != fsuid) {
            if (!bsd211_suser())
                goto exit;
        }
        if (group != -1 && ip->i_gid != fsgid) {
            if ((bsd211_u.u_uid != ip->i_uid || !bsd211_groupmember(fsgid)) && !bsd211_suser())
                goto exit;
        }
        if (owner != -1)
            ip->i_uid = fsuid;
        if (group != -1)
            ip->i_gid = fsgid;
    }

exit:
    bsd211_iput(ip);
    return -bsd211_u.u_error;
}

/** Change file last access and modification times
 */
int bsd211fs_utimens(const char *pathname, const struct timespec times[2])
{
    int res = 0;
    struct bsd211_inode *ip;
    struct bsd211_nameidata nd;
    struct bsd211_nameidata *ndp = &nd;
    struct bsd211_timeval atimeval, mtimeval;

    static const struct timespec sDefaultTimes[] = {
        { 0, UTIME_NOW },
        { 0, UTIME_NOW },
    };

    if (times == NULL)
        times = sDefaultTimes;

    bsd211_refreshclock();

    bsd211_u.u_error = 0;
    NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, (char *)pathname);
    if ((ip = bsd211_namei(ndp)) == NULL)
        return -bsd211_u.u_error;

    if (times[0].tv_nsec != UTIME_OMIT || times[1].tv_nsec != UTIME_OMIT) {
        
        int setBothNow = (times[0].tv_nsec == UTIME_NOW && times[1].tv_nsec == UTIME_NOW);

        if (bsd211_u.u_uid == 0 || bsd211_u.u_uid == ip->i_uid ||
            setBothNow || (res = bsd211_access(ip, IWRITE)) == 0) {

            bsd211_refreshclock();

            atimeval.tv_sec = (times[0].tv_nsec != UTIME_OMIT && times[0].tv_nsec != UTIME_NOW)
                ? (bsd211_time_t)times[0].tv_sec
                : bsd211_time.tv_sec;
            mtimeval.tv_sec = (times[1].tv_nsec != UTIME_OMIT && times[1].tv_nsec != UTIME_NOW)
                ? (bsd211_time_t)times[1].tv_sec
                : bsd211_time.tv_sec;

            if (times[0].tv_nsec != UTIME_OMIT && !(ip->i_fs->fs_flags & MNT_NOATIME))
                ip->i_flag |= IACC;

            if (times[1].tv_nsec != UTIME_OMIT)
                ip->i_flag |= (IUPD|ICHG);

            bsd211_iupdat(ip, &atimeval, &mtimeval, 1);
            res = -bsd211_u.u_error;
        }
    }

    bsd211_iput(ip);
    return res;
}

/** Check user's permissions for a file.
 */
int bsd211fs_access(const char *pathname, int mode)
{
    struct a {
        const char *fname;
        int16_t fmode;
    } uap;

    uap.fname = pathname;
    uap.fmode = (int16_t)mode;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_saccess();
    return -bsd211_u.u_error;
}

/** Read the value of a symbolic link
 */
ssize_t bsd211fs_readlink(const char *pathname, char *buf, size_t bufsiz)
{
    struct a {
        const char *name;
        char *buf;
        int16_t	count;
    } uap;

    uap.name = pathname;
    uap.buf = buf;
    uap.count = (uint16_t)bufsiz;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_readlink();
    if (bsd211_u.u_error == 0)
        return bsd211_u.u_r.r_val1;
    else
        return -bsd211_u.u_error;
}

/** Make a new symbolic link for a file
 */
int bsd211fs_symlink(const char *target, const char *linkpath)
{
    struct a {
        const char *target;
        const char *linkname;
    } uap;

    uap.target = target;
    uap.linkname = linkpath;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_symlink();
    return -bsd211_u.u_error;
}

/** Create a directory.
 */
int bsd211fs_mkdir(const char *pathname, mode_t mode)
{
    struct a {
        const char *name;
        int16_t dmode;
    } uap;

    uap.name = pathname;
    uap.dmode = (int16_t)mode;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_mkdir();
    return -bsd211_u.u_error;
}

/** Delete a directory.
 */
int bsd211fs_rmdir(const char *pathname)
{
    struct a {
        const char *name;
    } uap;

    uap.name = pathname;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_rmdir();
    return -bsd211_u.u_error;
}

int bsd211fs_enumdir(const char *pathname, bsd211fs_enum_dir_funct enum_funct, void *context)
{
    int fd;
    uint8_t dirbuf[DIRBLKSIZ];
    struct bsd211_direct *direntry;
    struct bsd211_direct * const dirbufend = (struct bsd211_direct *)&dirbuf[DIRBLKSIZ];
    struct bsd211_stat fsstatbuf;
    struct stat statbuf;
    struct bsd211_inode *ip = NULL;
    int readRes;

    bsd211_refreshclock();
    bsd211_u.u_error = 0;

    fd = bsd211fs_open(pathname, O_RDONLY, 0);
    if (fd < 0)
        return fd;

    direntry = dirbufend;

    while (1) {
        if (direntry >= dirbufend) {
            readRes = bsd211fs_read(fd, dirbuf, DIRBLKSIZ);
            if (readRes <= 0)
                break;
            if (readRes < DIRBLKSIZ) {
                readRes = 0;
                break;
            }
            direntry = (struct bsd211_direct *)&dirbuf[0];
        }

        if (direntry->d_ino != 0) {

            bsd211_u.u_error = 0;
            ip = bsd211_iget(bsd211_rootdev, &bsd211_mount[0].m_filsys, direntry->d_ino);
            if (ip == NULL) {
                readRes = -bsd211_u.u_error;
                break;
            }

            readRes = bsd211_ino_stat(ip, &fsstatbuf);
            if (readRes < 0)
                break;

            bsd211fs_convertstat(&fsstatbuf, &statbuf);

            readRes = enum_funct(direntry->d_name, &statbuf, context);
            if (readRes < 0)
                break;

            bsd211_iput(ip);
            ip = NULL;
        }

        direntry = (struct bsd211_direct *)(((uint8_t *)direntry) + direntry->d_reclen);
    }

    if (ip != NULL)
        bsd211_iput(ip);

    bsd211fs_close(fd);

    return readRes;
}

/** Commit filesystem changes to disk.
 */
int bsd211fs_sync()
{
    struct bsd211_fs *fs = &bsd211_mount[0].m_filsys;
    int syncRes, flushRes;

    if (fs->fs_fmod == 0)
        return 0;
    bsd211_refreshclock();
    fs->fs_time = wswap_int32(bsd211_time.tv_sec);
    syncRes = -bsd211_ufs_sync(&bsd211_mount[0]);
    flushRes = dsk_flush();
    return (syncRes != 0) ? syncRes : flushRes;
}

/** Synchronize a file's in-core state to disk.
 */
int bsd211fs_fsync(int fd)
{
	struct a {
		int16_t	fd;
	} uap;

    if (fd < 0 || fd > INT16_MAX)
        return -EINVAL;
    uap.fd = fd;
    bsd211_u.u_ap = &uap;
    bsd211_u.u_error = 0;
    bsd211_refreshclock();
    bsd211_fsync();
    return -bsd211_u.u_error;
}

/** Get filesystem statistics.
 */
int bsd211fs_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    memset(statvfsbuf, 0, sizeof(struct statvfs));

	statvfsbuf->f_bsize = MAXBSIZE;
	statvfsbuf->f_frsize = MAXBSIZE;
	statvfsbuf->f_blocks = wswap_int32(bsd211_mount[0].m_filsys.fs_fsize);
	statvfsbuf->f_bfree = wswap_int32(bsd211_mount[0].m_filsys.fs_tfree);
	statvfsbuf->f_bavail = statvfsbuf->f_bfree;
	statvfsbuf->f_files = (bsd211_mount[0].m_filsys.fs_isize - 2) * INOPB;
	statvfsbuf->f_ffree = bsd211_mount[0].m_filsys.fs_tinode;
    statvfsbuf->f_favail = bsd211_mount[0].m_filsys.fs_tinode;
    statvfsbuf->f_namemax = MAXNAMLEN;

    return 0;
}

/** Set the real and/or effective user ids for filesystem operations.
 */
int bsd211fs_setreuid(uid_t ruid, uid_t euid)
{
    if (ruid != -1)
        bsd211_u.u_ruid = bsd211fs_maphostuid(ruid);
    if (euid != -1)
        bsd211_u.u_uid = bsd211fs_maphostuid(euid);
    return 0;
}

/** Set the real and/or effective group ids for filesystem operations.
 */
int bsd211fs_setregid(gid_t rgid, gid_t egid)
{
    if (rgid != -1)
        bsd211_u.u_rgid = bsd211fs_maphostgid(rgid);
    if (egid != -1)
        bsd211_u.u_groups[0] = bsd211fs_maphostgid(egid); /* effective gid is u_groups[0] */
    return 0;
}

/** Set the supplementary group IDs for filesystem operations.
 */
int bsd211fs_setgroups(size_t size, const gid_t *list)
{
    int i;

    /* note that u_groups[0] is reserved for the effective group id.
     * thus only NGROUPS-1 supplementary group ids can be set via
     * this API.
     */
    if (size > NGROUPS-1)
        return -EINVAL;
    for (i = 1; i < NGROUPS; i++) {
        if (i <= size)
            bsd211_u.u_groups[i] = bsd211fs_maphostgid(list[i-1]);
        else
            bsd211_u.u_groups[i] = NOGROUP;
    }
    return 0;
}

/** Called by 2.11BSD kernel to refresh supplementary groups before an access check.
 * 
 * This function is intended to be overriden by users of the bsd211fs API as needed.
 * The design allows the API user to delay the loading of user supplementary groups
 * until the point they are needed by kernel, thereby avoiding the potentially expensive
 * operation of enumerating a user's groups for operations that don't require it.
 */
extern void bsd211fs_refreshgroups() __attribute__((weak));
void bsd211fs_refreshgroups()
{
    /* do nothing by default */
}

/** Add an entry to the uid mapping table.
 */
int bsd211fs_adduidmap(uid_t hostuid, uint32_t fsuid)
{
    return idmap_addidmap(&bsd211fs_uidmap, (uint32_t)hostuid, fsuid);
}

/** Add an entry to the gid mapping table.
 */
int bsd211fs_addgidmap(uid_t hostgid, uint32_t fsgid)
{
    return idmap_addidmap(&bsd211fs_gidmap, (uint32_t)hostgid, fsgid);
}

/** Construct the initial free block list for a new filesystem.
 *
 * This function is effectively identical to the bflist() function in the
 * BSD mkfs command.  However, unlike the original, this adaptation omits
 * code for reserving inode 1 (historically the bad block file), which is
 * handled in bsd211fs_mkfs() instead.
 * 
 * n and m are values used to create an initial interleave in the order
 * that blocks appear on the free list.
 */
static void bsd211fs_initfreelist(struct bsd211_fs *fs, uint16_t n, uint16_t m)
{
    struct bsd211_inode in;
    uint8_t flg[BSD211FS_MAX_FN];
    int16_t adr[BSD211FS_MAX_FN];
    int16_t i, j;
    bsd211_daddr_t f, d;
    
    /* setup dummy inode for use when calling bsd211_free() */
    memset(&in, 0, sizeof(in));
    in.i_number = 1;
    in.i_mode = IFREG;
    in.i_fs = fs;
    in.i_dev = bsd211_rootdev;

    memset(flg, 0, sizeof(flg));
    i = 0;
    for (j = 0; j < n; j++) {
        while (flg[i])
            i = (i+1)%n;
        adr[j] = i+1;
        flg[i]++;
        i = (i+m)%n;
    }

    d = wswap_int32(fs->fs_fsize)-1;
    while (d%n)
        d++;
    for (; d > 0; d -= n)
        for (i=0; i<n; i++) {
            f = d - adr[i];
            if (f < wswap_int32(fs->fs_fsize) && f >= fs->fs_isize)
                bsd211_free(&in, f);
        }
}

/** Initialize root directory structure for new filesystem.
 * 
 * This function is roughly equivalent to the fsinit() function in
 * the BSD mkfs command.
 */
static int bsd211fs_initrootdir(struct bsd211_fs *fs)
{
    struct bsd211_inode in;
    struct bsd211_dinode rootin, lfin, reservedin, *ip;
    struct bsd211_buf *bp;
    struct bsd211_direct *dp;

    enum {
        UMASK = 0777,   /* default permissions for directories */
        RESERVEDINO = 1 /* reserved inode number */
    };

#define NEXT_DIRECT(DP) (struct bsd211_direct *)((uint8_t *)(DP) + (DP)->d_reclen);
#define REMAINING_DIR_SIZE(START, END) (DIRBLKSIZ - ((uint8_t *)(END) - (uint8_t *)(START)))

    /* setup dummy inode structure for use when calling bsd211_balloc() */
    memset(&in, 0, sizeof(in));
    in.i_number = 1;
    in.i_mode = IFREG;
    in.i_fs = fs;
    in.i_dev = bsd211_rootdev;

    bsd211_u.u_error = 0;

    /* initialize lost+found directory and inode */
    memset(&lfin, 0, sizeof(lfin));
    lfin.di_mode = (uint16_t)(IFDIR|UMASK);
    lfin.di_nlink = 2;
    lfin.di_size = wswap_int32(DIRBLKSIZ * 2);
    lfin.di_atime = (bsd211_time_t)wswap_int32(bsd211_time.tv_sec);
    lfin.di_mtime = (bsd211_time_t)wswap_int32(bsd211_time.tv_sec);
    lfin.di_ctime = (bsd211_time_t)wswap_int32(bsd211_time.tv_sec);
    bp = bsd211_balloc(&in, B_CLRBUF);
    if (bp == NULL)
        return -bsd211_u.u_error;
    lfin.di_addr[0] = wswap_int32(bsd211_dbtofsb(bp->b_blkno));
    dp = (struct bsd211_direct *)bp->b_un.b_addr;
    dp->d_ino = LOSTFOUNDINO;
    dp->d_name[0] = '.';
    dp->d_namlen = 1;
    dp->d_reclen = BSD211_DIRSIZ(dp);
    dp = NEXT_DIRECT(dp);
    dp->d_ino = ROOTINO;
    dp->d_name[0] = '.';
    dp->d_name[1] = '.';
    dp->d_namlen = 2;
    dp->d_reclen = REMAINING_DIR_SIZE(bp->b_un.b_addr, dp); /* remaining space in 1st dir block */
    dp = NEXT_DIRECT(dp);
    dp->d_reclen = DIRBLKSIZ; /* 2nd dir block empty */
    bsd211_bwrite(bp);
    if (bsd211_u.u_error != 0)
        return -bsd211_u.u_error;

    /* initialize root directory and inode */
    memset(&rootin, 0, sizeof(rootin));
    rootin.di_mode = (uint16_t)(IFDIR|UMASK);
    rootin.di_nlink = 3;
    rootin.di_size = wswap_int32(DIRBLKSIZ);
    rootin.di_atime = (bsd211_time_t)wswap_int32(bsd211_time.tv_sec);
    rootin.di_mtime = (bsd211_time_t)wswap_int32(bsd211_time.tv_sec);
    rootin.di_ctime = (bsd211_time_t)wswap_int32(bsd211_time.tv_sec);
    bp = bsd211_balloc(&in, B_CLRBUF);
    if (bp == NULL)
        return -bsd211_u.u_error;
    rootin.di_addr[0] = wswap_int32(bsd211_dbtofsb(bp->b_blkno));
    dp = (struct bsd211_direct *)bp->b_un.b_addr;
    dp->d_ino = ROOTINO;
    dp->d_name[0] = '.';
    dp->d_namlen = 1;
    dp->d_reclen = BSD211_DIRSIZ(dp);
    dp = NEXT_DIRECT(dp);
    dp->d_ino = ROOTINO;
    dp->d_name[0] = '.';
    dp->d_name[1] = '.';
    dp->d_namlen = 2;
    dp->d_reclen = BSD211_DIRSIZ(dp);
    dp = NEXT_DIRECT(dp);
    dp->d_ino = LOSTFOUNDINO;
    strcpy(dp->d_name, "lost+found");
    dp->d_namlen = 10;
    dp->d_reclen = REMAINING_DIR_SIZE(bp->b_un.b_addr, dp); /* remaining space in dir block */
    bsd211_bwrite(bp);
    if (bsd211_u.u_error != 0)
        return -bsd211_u.u_error;

    /* initialize the reserved inode */
    memset(&reservedin, 0, sizeof(reservedin));
    reservedin.di_mode = IFREG;

    /* write the inodes to the inode table */
    bp = bsd211_bread(bsd211_rootdev, 2);
    if (bsd211_u.u_error != 0)
        return -bsd211_u.u_error;
    ip = (struct bsd211_dinode *)bp->b_un.b_addr;
    ip[RESERVEDINO - 1] = reservedin;
    ip[ROOTINO - 1] = rootin;
    ip[LOSTFOUNDINO - 1] = lfin;
    fs->fs_tinode -= 3;
    bsd211_bwrite(bp);
    if (bsd211_u.u_error != 0)
        return -bsd211_u.u_error;

    return 0;
}

static void bsd211fs_convertstat(const struct bsd211_stat *fsstatbuf, struct stat *statbuf)
{
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_dev = fsstatbuf->st_dev;
    statbuf->st_ino = fsstatbuf->st_ino;
    statbuf->st_nlink = fsstatbuf->st_nlink;
    statbuf->st_uid = bsd211fs_mapfsuid(fsstatbuf->st_uid);
    statbuf->st_gid = bsd211fs_mapfsgid(fsstatbuf->st_gid);
    statbuf->st_size = (off_t)fsstatbuf->st_size;
    statbuf->st_blksize = fsstatbuf->st_blksize;
    statbuf->st_blocks = fsstatbuf->st_blocks;
    switch (fsstatbuf->st_mode & BSD211_S_IFMT) {
    case BSD211_S_IFDIR:
        statbuf->st_mode = S_IFDIR;
        break;
    case BSD211_S_IFCHR:
        statbuf->st_mode = S_IFCHR;
        statbuf->st_rdev = fsstatbuf->st_rdev;
        break;
    case BSD211_S_IFBLK:
        statbuf->st_mode = S_IFBLK;
        statbuf->st_rdev = fsstatbuf->st_rdev;
        break;
    case BSD211_S_IFLNK:
        statbuf->st_mode = S_IFLNK;
        break;
    default:
        statbuf->st_mode = S_IFREG;
        break;
    }
    statbuf->st_mode |= (fsstatbuf->st_mode & 07777);
    statbuf->st_atime = fsstatbuf->bsd211_st_atime;
    statbuf->st_mtime = fsstatbuf->bsd211_st_mtime;
    statbuf->st_ctime = fsstatbuf->bsd211_st_ctime;
}
