#define _XOPEN_SOURCE 700
#define _ATFILE_SOURCE 

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/time.h>
#include <string.h>
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

struct IDMapEntry {
    uint32_t hostid;
    char v6id;
};

#define V6FS_MAX_ID_MAP_ENTRIES 100
static size_t v6fs_uidmapcount = 0;
static struct IDMapEntry v6fs_uidmap[V6FS_MAX_ID_MAP_ENTRIES];
static size_t v6fs_gidmapcount = 0;
static struct IDMapEntry v6fs_gidmap[V6FS_MAX_ID_MAP_ENTRIES];

static void v6fs_convertstat(const struct v6_stat *v6statbuf, struct stat *statbuf);
static int v6fs_isdirlink(const char *entryname, const struct stat *statbuf, void *context);
static char v6fs_maphostid(uint32_t hostid, const struct IDMapEntry *table, size_t count);
static uint32_t v6fs_mapv6id(char v6id, const struct IDMapEntry *table, size_t count);
static inline char v6fs_maphostuid(uid_t hostuid) { return v6fs_maphostid((uint32_t)hostuid, v6fs_uidmap, v6fs_uidmapcount); }
static inline char v6fs_maphostgid(gid_t hostgid) { return v6fs_maphostid((uint32_t)hostgid, v6fs_gidmap, v6fs_gidmapcount); }
static inline uid_t v6fs_mapv6uid(char v6uid) { return (uid_t)v6fs_mapv6id(v6uid, v6fs_uidmap, v6fs_uidmapcount); }
static inline gid_t v6fs_mapv6gid(char v6gid) { return (gid_t)v6fs_mapv6id(v6gid, v6fs_gidmap, v6fs_gidmapcount); }

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
    char v6uid = v6fs_maphostuid(owner);
    char v6gid = v6fs_maphostgid(group);

    v6_refreshclock();
    u.u_error = 0;

    u.u_dirp = (char *)pathname;
	if ((ip = v6_namei(v6_uchar, 0)) == NULL)
		return -u.u_error;

    if (owner != -1 || group != -1) {
        if (owner != -1 && ip->i_uid != v6uid) {
            if (!v6_suser()) {
                res = -EPERM;
                goto exit;
            }
        }
        if (group != -1 && ip->i_gid != v6gid) {
            if (!v6_suser() && u.u_uid != ip->i_uid) {
                res = -EPERM;
                goto exit;
            }
        }

        if (owner != -1)
            ip->i_uid = v6uid;
        if (group != -1)
            ip->i_gid = v6gid;
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
    int dircreated = 0;

    v6_refreshclock();
    u.u_error = 0;

    namebuf = (char *)malloc(strlen(pathname) + 4);

    strcpy(namebuf, pathname);
    parentname = strdup(dirname(namebuf));

    /* verify that the given pathname does not already exist.
       in the process, a pointer to the parent directory is
       set in u.u_pdir. */
    u.u_dirp = (char *)pathname;
    ip = v6_namei(&v6_uchar, 1);
    if (ip != NULL) {
        v6_iput(ip);
        res = -EEXIST;
        goto exit;
    }
    if ((res = -u.u_error) < 0)
        goto exit;

    /* in v6, mkdir is a command that runs set-uid root, 
       which gives it the priveldge to create links to
       directory node (in particular the . and .. entries).
       here we simulate this by switching the effective uid
       to root. */
    u.u_uid = 0;

    /* make the directory node within the parent directory. 
       temporarily grant access to the root user only. */
    ip = v6_maknode(IFDIR|0700);
    if (ip == NULL) {
        res = -u.u_error;
        goto exit;
    }
    dircreated = 1;
    v6_iput(ip);
    if ((res = -u.u_error) < 0)
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
    u.u_error = 0;
    u.u_dirp = (char *)pathname;
	if ((ip = v6_namei(v6_uchar, 0)) == NULL) {
		res = -u.u_error;
        goto exit;
    }
    ip->i_uid = u.u_ruid;
    ip->i_flag |= IUPD;
    v6_iput(ip);
    if ((res = -u.u_error) < 0)
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
    u.u_uid = u.u_ruid;
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

    /* count the number of free blocks.
     
       The freelist consist of a table of 100 block numbers located in the
       filesystem superblock (fp->s_free).  Each entry in this table can
       contain the block number of a free block.  The number of entries that
       actually contain free block numbers (starting at index 0 in the table)
       is denoted by the integer fp->s_nfree.
       
       The first block number in the table is special.  If there are more than
       99 free blocks, fp->s_free[0] is the number of a free block that also
       contains pointers to other free blocks.  This is referred to as an
       "index" block.  In cases where there are 99 or fewer free blocks, 
       fp->s_free[0] will always contain the NULL block number (0), indicating
       that no index block exists.

       Like the superblock, an index block also contains a table of 100 block
       numbers (starting at offset 2) and a count of the number table entries
       that actually contain free blocks (located at offset 0).  Similarly, the
       first poistion in the table is also used to point to a subsequent index
       block in cases where there are more than 99 additional free blocks. Thus
       index blocks are strung together to form a chain of groups of 100 free
       blocks.

       NOTE that there are *two* distinct states possible when the filesystem
       reaches the point of being completely full:
           1) fp->s_nfree == 0
           2) fp->s_nfree == 1 and fp->s_free[0] == 0
       The second state is entered when the last free block is allocated by the
       alloc() function.  The first state is entered when alloc() is called again
       *after* the last block has been allocated (i.e. when the first allocation
       failure occurs).
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
            if (idxblkcount > fp->s_fsize) {
                fprintf(stderr, "V6FS ERROR: loop detected in free list");
                return -EIO;
            }
            if (v6_badblock(fp, nextidxblk, v6_rootdev))
                return -EIO;
            bp = v6_bread(v6_rootdev, nextidxblk);
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

/** Set the real and/or effective user ids for filesystem operations.
 */
int v6fs_setreuid(uid_t ruid, uid_t euid)
{
    if (ruid != -1)
        u.u_ruid = v6fs_maphostuid(ruid);
    if (euid != -1)
        u.u_uid = v6fs_maphostuid(euid);
    return 0;
}

/** Set the real and/or effective group ids for filesystem operations.
 */
int v6fs_setregid(gid_t rgid, gid_t egid)
{
    if (rgid != -1)
        u.u_rgid = v6fs_maphostgid(rgid);
    if (egid != -1)
        u.u_gid = v6fs_maphostgid(egid);
    return 0;
}

/** Add an entry to the uid mapping table.
 */
int v6fs_adduidmap(uid_t hostuid, char fsuid)
{
    if (v6fs_uidmapcount >= V6FS_MAX_ID_MAP_ENTRIES)
        return -EOVERFLOW;
    v6fs_uidmap[v6fs_uidmapcount].hostid = (uint32_t)hostuid;
    v6fs_uidmap[v6fs_uidmapcount++].v6id = fsuid;
    return 0;
}

/** Add an entry to the gid mapping table.
 */
int v6fs_addgidmap(uid_t hostgid, char fsgid)
{
    if (v6fs_gidmapcount >= V6FS_MAX_ID_MAP_ENTRIES)
        return -EOVERFLOW;
    v6fs_gidmap[v6fs_gidmapcount].hostid = (uint32_t)hostgid;
    v6fs_gidmap[v6fs_gidmapcount++].v6id = fsgid;
    return 0;
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

static int v6fs_isdirlink(const char *entryname, const struct stat *statbuf, void *context)
{
    if ((statbuf->st_mode & S_IFMT) == S_IFDIR && (strcmp(entryname, ".") == 0 || strcmp(entryname, "..") == 0))
        return 0;
    return -ENOTEMPTY;
}

static char v6fs_maphostid(uint32_t hostid, const struct IDMapEntry *table, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (table[i].hostid == hostid)
            return table[i].v6id;
    /* map any ids larger than what will fit in a v6 id to 255 */
    if ((uint32_t)hostid > 0xFF)
        return (char)255;
    return (char)hostid;
}

static uint32_t v6fs_mapv6id(char v6id, const struct IDMapEntry *table, size_t count)
{
    for (size_t i = 0; i < count; i++)
        if (table[i].v6id == v6id)
            return table[i].hostid;
    /* map the v6 id 255 to the standard nobody/nogroup host ids */
    if (v6id == (char)255)
        return (uint32_t)65534;
    return (uint32_t)v6id;
}
