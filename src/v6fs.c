#define _XOPEN_SOURCE 700
#define _ATFILE_SOURCE 

#include <fcntl.h>
#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/sysmacros.h>
#include <libgen.h>
#include <sys/statvfs.h>

#include "v6fs.h"
#include "v6adapt.h"

#include "param.h"
#include "user.h"
#include "buf.h"
#include "systm.h"
#include "inode.h"
#include "file.h"
#include "filsys.h"
#include "conf.h"

static void v6fs_convertstat(const struct v6_stat *v6statbuf, struct stat *statbuf);
static int v6fs_isdirlink(const char *entryname, const struct stat *statbuf, void *context);

/** Initialize the Unix v6 filesystem.
 */
void v6fs_init(int readonly)
{
    v6_init_kernel(readonly);
}

/** Open file or directory in the v6 filesystem.
 */
int v6fs_open(const char *name, int flags, mode_t mode)
{
    struct inode *ip;
    int16_t fd;
    int16_t m;

    v6_refreshclock();
    u.u_error = 0;

    switch (flags & O_ACCMODE)
    {
    case O_RDWR:
        m = FREAD | FWRITE;
        break;
    case O_RDONLY:
    default:
        m = FREAD;
        break;
    case O_WRONLY:
        m = FWRITE;
        break;
    }
    u.u_ar0 = &fd;
    u.u_dirp = (char *)name;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL) {
        if (u.u_error != ENOENT)
            return -u.u_error;
        if ((flags & O_CREAT) == 0)
            return -u.u_error;
        ip = v6_maknode(mode & 07777 & (~ISVTX));
        if (ip == NULL)
            return -u.u_error;
        v6_open1(ip, m, 2);
    }
    else {
        if ((flags & O_CREAT) != 0 && (flags & O_EXCL) != 0) {
            v6_iput(ip);
            return -EEXIST;
        }
        v6_open1(ip, m, ((flags & O_TRUNC) != 0) ? 1 : 0);
    }
    if (u.u_error != 0)
        return -u.u_error;
    return fd;
}

/** Close file/directory.
 */
int v6fs_close(int fd)
{
    int16_t ar0;

    v6_refreshclock();
    u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    ar0 = fd;
    u.u_ar0 = &ar0;
    v6_close();
    return 0;
}

/** Seek file/directory.
 */
off_t v6fs_seek(int fd, off_t offset, int whence)
{
    struct file *fp;
    off_t curSize, curPos;

    v6_refreshclock();
    u.u_error = 0;

    if (fd < 0 || fd > INT16_MAX)
        return -EBADF;
    fp = v6_getf((int16_t)fd);
    if (fp == NULL)
        return -u.u_error;
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
    u.u_error = 0;

    u.u_dirp = (char *)oldpath;
    u.u_arg[1] = (intptr_t)newpath;
    v6_link();
    return -u.u_error;
}

/** Create a new file, directory, block or character device node.
 */
int v6fs_mknod(const char *pathname, mode_t mode, dev_t dev)
{
    struct inode *ip;
    int16_t v6mode, v6dev;

    v6_refreshclock();
    u.u_error = 0;

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

    u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_uchar, 1);
    if (ip != NULL) {
        v6_iput(ip);
        return -EEXIST;
    }
    if (u.u_error != 0)
        return -u.u_error;
    ip = v6_maknode(v6mode);
    if (ip == NULL)
        return -u.u_error;
    ip->i_addr[0] = (int16_t)v6dev;
    v6_iput(ip);
    return -u.u_error;
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
        u.u_ar0 = &ar0;
        u.u_arg[0] = (intptr_t)buf;
        u.u_arg[1] = (count > UINT16_MAX) ? UINT16_MAX : (uint16_t)count;
        u.u_error = 0;
        v6_rdwr(FREAD);

        if (u.u_error != 0)
            return -u.u_error;

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
        u.u_ar0 = &ar0;
        u.u_arg[0] = (intptr_t)buf;
        u.u_arg[1] = writeSize;
        u.u_error = 0;
        v6_rdwr(FWRITE);

        if (u.u_error  != 0)
            return -u.u_error;

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
    struct inode *ip;

    v6_refreshclock();
    u.u_error = 0;

    u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -u.u_error;
    if (length == 0) {
        v6_itrunc(ip);
        res = -u.u_error;
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
    struct inode *ip;
    struct v6_stat v6statbuf;

    v6_refreshclock();
    u.u_error = 0;

    u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -u.u_error;
    v6_stat1(ip, &v6statbuf);
    if (u.u_error != 0) {
        v6_iput(ip);
        return -u.u_error;
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
    u.u_error = 0;

    u.u_dirp = (char *)pathname;
    v6_unlink();
    return -u.u_error;
}

/** Change permissions of a file.
 */
int v6fs_chmod(const char *pathname, mode_t mode)
{
    v6_refreshclock();
    u.u_error = 0;

    u.u_dirp = (char *)pathname;
    u.u_arg[1] = (int16_t)(mode & 07777);
    v6_chmod();
    return -u.u_error;
}

/** Change ownership of a file.
 */
int v6fs_chown(const char *pathname, uid_t owner, gid_t group)
{
    int res = 0;
    struct inode *ip;

    v6_refreshclock();
    u.u_error = 0;

    if ((owner != (uid_t)-1 && owner > 0xFF) || 
        (group != (gid_t)-1 && group > 0xFF))
        return -EINVAL;

    u.u_dirp = (char *)pathname;
	if ((ip = v6_namei(v6_uchar, 0)) == NULL)
		return -u.u_error;

    if (owner != -1 || group != -1) {
        if (owner != -1 && ip->i_uid != (char)owner) {
            if (!v6_suser()) {
                res = -EPERM;
                goto exit;
            }
        }
        if (group != -1 && ip->i_gid != (char)group) {
            if (!v6_suser() && u.u_uid != ip->i_uid) {
                res = -EPERM;
                goto exit;
            }
        }

        if (owner != -1)
            ip->i_uid = (char)owner;
        if (group != -1)
            ip->i_gid = (char)group;
        ip->i_flag |= IUPD;
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
    struct inode *ip;
    int16_t curTime[2], mtime[2];

    v6_refreshclock();
    u.u_error = 0;

    u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -u.u_error;

    if (times[0].tv_nsec != UTIME_OMIT || times[1].tv_nsec != UTIME_OMIT) {
        
        if (u.u_uid != ip->i_uid && v6_access(ip, IWRITE) != 0) {
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
        res = -u.u_error;

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
    struct inode *ip;

    v6_refreshclock();
    u.u_error = 0;

    u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_schar, 0);
    if (ip == NULL)
        return -u.u_error;
    if ((mode & R_OK) != 0 && v6_access(ip, IREAD) != 0)
        res = -u.u_error;
    if ((mode & W_OK) != 0 && v6_access(ip, IWRITE) != 0)
        res = -u.u_error;
    if ((mode & X_OK) != 0 && v6_access(ip, IEXEC) != 0)
        res = -u.u_error;
    v6_iput(ip);
    return res;
}

/** Create a directory.
 */
int v6fs_mkdir(const char *pathname, mode_t mode)
{
    int res;
    struct inode *ip;
    char *namebuf;
    char *parentname;

    v6_refreshclock();
    u.u_error = 0;

    namebuf = (char *)malloc(strlen(pathname) + 4);

    strcpy(namebuf, pathname);
    parentname = strdup(dirname(namebuf));

    u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_uchar, 1);
    if (ip != NULL) {
        v6_iput(ip);
        res = -EEXIST;
        goto exit;
    }
    if ((res = -u.u_error) < 0)
        goto exit;
    ip = v6_maknode(IFDIR|0700);
    if (ip == NULL) {
        res = -u.u_error;
        goto exit;
    }
    v6_iput(ip);
    if ((res = -u.u_error) < 0)
        goto exit;

    strcpy(namebuf, pathname);
    strcat(namebuf, "/.");
    res = v6fs_link(pathname, namebuf);
    if (res < 0)
        goto exit;

    strcat(namebuf, ".");
    res = v6fs_link(parentname, namebuf);
    if (res < 0)
        goto exit;

    res = v6fs_chmod(pathname, mode&07777);

exit:
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
    res = v6fs_enumdir(pathname, v6fs_isdirlink, NULL);
    if (res < 0)
        goto exit;

    strcpy(namebuf, pathname);
    dirname = basename(namebuf);
    if (strcmp(dirname, ".") == 0 || strcmp(dirname, "..") == 0) {
        res = -EINVAL;
        goto exit;
    }

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
    u.u_error = 0;

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

        u.u_error = 0;
        ip = v6_iget(v6_rootdev, direntrybuf.direntry.d_inode);
        if (ip == NULL) {
            readRes = -u.u_error;
            break;
        }

        u.u_error = 0;
        v6_stat1(ip, &v6statbuf);
        if (u.u_error != 0) {
            readRes = -u.u_error;
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

/** Commit filesystem caches to disk.
 */
void v6fs_sync()
{
    v6_refreshclock();
    u.u_error = 0;

    v6_update();
}

/** Get filesystem statistics.
 */
int v6fs_statfs(const char *pathname, struct statvfs *statvfsbuf)
{
    struct v6_filsys *fp;
    struct buf *bp = NULL;

    v6_refreshclock();
    u.u_error = 0;

    v6_update();

    memset(statvfsbuf, 0, sizeof(struct statvfs));

    fp = v6_getfs(v6_rootdev);

    statvfsbuf->f_bsize = 512;
    statvfsbuf->f_frsize = 512;
    statvfsbuf->f_blocks = fp->s_fsize;
    statvfsbuf->f_files = fp->s_isize * 16;
    statvfsbuf->f_namemax = DIRSIZ;

    /* count the number of free blocks. */
    {    
        int16_t *nfree = &fp->s_nfree;
        int16_t *freetab = fp->s_free;
        while (1) {
            statvfsbuf->f_bfree += *nfree;
            int16_t nextblk = freetab[0];
            if (bp != NULL)
                v6_brelse(bp);
            if (nextblk == 0) {
                statvfsbuf->f_bfree--;
                break;
            }
            if (v6_badblock(fp, nextblk, v6_rootdev))
                return -EIO;
            bp = v6_bread(v6_rootdev, nextblk);
            v6_geterror(bp);
            if (u.u_error != 0)
                return -u.u_error;
            nfree = (int16_t *)bp->b_addr;
            freetab = nfree + 1;
        }
    }
    statvfsbuf->f_bavail = statvfsbuf->f_bfree;

    /* count the number of free inodes. */
    for (int16_t blk = 2; blk < fp->s_isize + 2; blk++) {
        bp = v6_bread(v6_rootdev, blk);
        v6_geterror(bp);
        if (u.u_error != 0)
            return -u.u_error;
        int16_t *ip = (int16_t *)bp->b_addr;
        for (int i = 0; i < 16; i++, ip += 16) {
            if ((*ip & IALLOC) == 0)
                statvfsbuf->f_ffree++;
        }
        v6_brelse(bp);
    }

    return 0;
}

/** Set the effective user id for filesystem operations.
 */
void v6fs_seteuid(uid_t uid)
{
    if (uid > 0xFF)
        uid = 0xFF;
    u.u_uid = (char)uid;
}

/** Set the effective group id for filesystem operations.
 */
void v6fs_setegid(gid_t gid)
{
    if (gid > 0xFF)
        gid = 0xFF;
    u.u_gid = (char)gid;
}

static void v6fs_convertstat(const struct v6_stat *v6statbuf, struct stat *statbuf)
{
    memset(statbuf, 0, sizeof(*statbuf));
    statbuf->st_dev = v6statbuf->dev;
    statbuf->st_ino = v6statbuf->inumber;
    statbuf->st_mode = v6statbuf->mode ;
    statbuf->st_nlink = v6statbuf->nlinks;
    statbuf->st_uid = v6statbuf->uid;
    statbuf->st_gid = v6statbuf->gid;
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

static int v6fs_isdirlink(const char *entryname, const struct stat *statbuf, void *context)
{
    if ((statbuf->st_mode & S_IFMT) == S_IFDIR && (strcmp(entryname, ".") == 0 || strcmp(entryname, "..") == 0))
        return 0;
    return -ENOTEMPTY;
}
