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
 * @file  Undo adaptations defined in bsd211adapt.h, restoring access to 
 *        modern constructs, while still prevering access to select
 *        definitions from 2.11BSD code.
 */

#ifndef __BSD211UNADAPT_H__
#define __BSD211UNADAPT_H__

/* Define constructs that provide access to certain conflicting 2.9BSD macros 
 * even after their modern definitions have been restored.
 */

#ifdef _TYPES_
static inline int16_t bsd211_major(dev_t dev) { return major(dev); }
static inline int16_t bsd211_minor(dev_t dev) { return minor(dev); }
static inline dev_t bsd211_makedev(int16_t x, int16_t y) { return makedev(x, y); }
#endif /* _TYPES_ */

#ifdef _SYS_FS_H_
enum {
    BSD211_SUPERB       = SUPERB,
    BSD211_ROOTINO      = ROOTINO,
    BSD211_LOSTFOUNDINO = LOSTFOUNDINO,
};
static inline bsd211_daddr_t bsd211_fsbtodb(bsd211_daddr_t b) { return fsbtodb(b); }
static inline bsd211_daddr_t bsd211_dbtofsb(bsd211_daddr_t b) { return dbtofsb(b); }
#endif /* _SYS_FS_H_ */

#ifdef _STAT_H_
enum {
    BSD211_S_IFMT       = S_IFMT,
    BSD211_S_IFDIR      = S_IFDIR,
    BSD211_S_IFCHR      = S_IFCHR,
    BSD211_S_IFBLK      = S_IFBLK,
    BSD211_S_IFREG      = S_IFREG,
    BSD211_S_IFLNK      = S_IFLNK,
    BSD211_S_ISUID      = S_ISUID,
    BSD211_S_ISGID      = S_ISGID,
    BSD211_S_ISVTX      = S_ISVTX,
    BSD211_S_IREAD      = S_IREAD,
    BSD211_S_IWRITE     = S_IWRITE,
    BSD211_S_IEXEC      = S_IEXEC
};
#endif /* _STAT_H_ */

#ifdef _FCNTL_H_
enum {
    BSD211_FREAD        = FREAD,
    BSD211_FWRITE       = FWRITE,
    BSD211_O_RDONLY     = O_RDONLY,
    BSD211_O_WRONLY     = O_WRONLY,
    BSD211_O_RDWR       = O_RDWR,
    BSD211_O_ACCMODE    = O_ACCMODE,
    BSD211_O_NONBLOCK   = O_NONBLOCK,
    BSD211_O_APPEND     = O_APPEND,
    BSD211_O_SHLOCK     = O_SHLOCK,
    BSD211_O_EXLOCK     = O_EXLOCK,
    BSD211_O_ASYNC      = O_ASYNC,
    BSD211_O_FSYNC      = O_FSYNC,
    BSD211_O_CREAT      = O_CREAT,
    BSD211_O_TRUNC      = O_TRUNC,
    BSD211_O_EXCL       = O_EXCL,
};
#endif /* _FCNTL_H_ */

#ifdef _NAMEI_
enum {
    BSD211_LOOKUP       = LOOKUP,
    BSD211_CREATE       = CREATE,
    BSD211_DELETE       = DELETE,
    BSD211_LOCKPARENT   = LOCKPARENT,
    BSD211_NOCACHE      = NOCACHE,
    BSD211_FOLLOW       = FOLLOW,
    BSD211_NOFOLLOW     = NOFOLLOW,
};
#endif /* _NAMEI_ */

#ifdef _DIR_
static inline size_t BSD211_DIRSIZ(struct bsd211_direct *dp) { return DIRSIZ(dp); }
#endif /* _DIR_ */


/* Undefine name mapping macros
 */
#undef _cinvall
#undef access
#undef balloc
#undef badblock
#undef bcmp
#undef bcopy
#undef bdevsw
#undef bdwrite
#undef bflush
#undef bfreelist
#undef binval
#undef biodone
#undef biowait
#undef blkatoff
#undef blkflush
#undef bmap
#undef bread
#undef breada
#undef brelse
#undef buf
#undef bufhash
#undef bufhd
#undef bwrite
#undef bzero
#undef caddr_t
#undef cdevsw
#undef checkpath
#undef chmod
#undef chmod1
#undef chown1
#undef clean
#undef close
#undef closef
#undef clrbuf
#undef copy
#undef copyin
#undef copyinstr
#undef copyout
#undef copystr
#undef daddr_t
#undef dev_t
#undef dinode
#undef dirchk
#undef dirbad
#undef dirbadentry
#undef direct
#undef dirempty
#undef direnter
#undef dirremove
#undef dirrewrite
#undef dirtemplate
#undef dupfdopen
#undef falloc
#undef fblk
#undef fd_set
#undef file
#undef fileNFILE
#undef Fops
#undef free
#undef fs
#undef fserr
#undef fstat
#undef fsync
#undef ftruncate
#undef fubyte
#undef getblk
#undef geteblk
#undef geterror
#undef getf
#undef getfs
#undef getfsstat
#undef getinode
#undef getnewbuf
#undef gid_t
#undef groupmember
#undef ialloc
#undef ifind
#undef iflush
#undef ifree
#undef ifreeh
#undef ifreet
#undef iget
#undef igrab
#undef ihead
#undef ihinit
#undef ilock
#undef incore
#undef indirtrunc
#undef ino_rw
#undef ino_stat
#undef ino_t
#undef inode
#undef inodeNINODE
#undef inodeops
#undef iovec
#undef iput
#undef irele
#undef insque
#undef itimerval
#undef itrunc
#undef iunlock
#undef iupdat
#undef label_t
#undef lastf
#undef link
#undef log
#undef maknode
#undef malloc
#undef mapin
#undef mapout
#undef mastertemplate
#undef mkdir
#undef mknod
#undef mode_t
#undef mount
#undef namecache
#undef namei
#undef nameidata
#undef nblkdev
#undef nbuf
#undef nchash
#undef nchhead
#undef nchtail
#undef nchinit
#undef nchinval
#undef nchrdev
#undef nchsize
#undef nchstats
#undef nextinodeid
#undef ninode
#undef nmidesc
#undef off_t
#undef open
#undef panic
#undef pid_t
#undef printf
#undef proc
#undef psignal
#undef rablock
#undef rdwri
#undef read
#undef readlink
#undef remque
#undef rename
#undef rmdir
#undef rootdev
#undef rootdir
#undef rwip
#undef rwuio
#undef saccess
#undef securelevel
#undef SEG5
#undef segm
#undef sigaction
#undef sigaltstack
#undef sigset_t
#undef size_t
#undef sleep
#undef ssize_t
#undef stat
#undef stat1
#undef strlen
#undef suser
#undef symlink
#undef sync
#undef syncinodes
#undef syncip
#undef tablefull
#undef time
#undef time_t
#undef timespec
#undef timeval
#undef timezone
#undef trsingle
#undef truncate
#undef u
#undef u_char
#undef u_short
#undef u_int
#undef u_long
#undef ufalloc
#undef ufs_mountedon
#undef ufs_setattr
#undef ufs_sync
#undef uid_t
#undef uio
#undef uio_rw
#undef uio_seg
#undef uiomove
#undef unlink
#undef updlock
#undef uprintf
#undef user
#undef ushort
#undef utimes
#undef vattr
#undef vattr_null
#undef vn_open
#undef vn_closefile
#undef wakeup
#undef write


/* Restore modern definitions of macros that collide with 2.11BSD code.
 */
#pragma pop_macro("NULL")
#pragma pop_macro("EAGAIN")
#pragma pop_macro("EWOULDBLOCK")
#pragma pop_macro("ELOOP")
#pragma pop_macro("EDEADLK")
#pragma pop_macro("EINPROGRESS")
#pragma pop_macro("EALREADY")
#pragma pop_macro("ENOTSOCK")
#pragma pop_macro("EDESTADDRREQ")
#pragma pop_macro("EMSGSIZE")
#pragma pop_macro("EPROTOTYPE")
#pragma pop_macro("ENOPROTOOPT")
#pragma pop_macro("EPROTONOSUPPORT")
#pragma pop_macro("ESOCKTNOSUPPORT")
#pragma pop_macro("EOPNOTSUPP")
#pragma pop_macro("EPFNOSUPPORT")
#pragma pop_macro("EAFNOSUPPORT")
#pragma pop_macro("EADDRINUSE")
#pragma pop_macro("EADDRNOTAVAIL")
#pragma pop_macro("ENETDOWN")
#pragma pop_macro("ENETUNREACH")
#pragma pop_macro("ENETRESET")
#pragma pop_macro("ECONNABORTED")
#pragma pop_macro("ECONNRESET")
#pragma pop_macro("ENOBUFS")
#pragma pop_macro("EISCONN")
#pragma pop_macro("ENOTCONN")
#pragma pop_macro("ESHUTDOWN")
#pragma pop_macro("ETOOMANYREFS")
#pragma pop_macro("ETIMEDOUT")
#pragma pop_macro("ECONNREFUSED")
#pragma pop_macro("ENAMETOOLONG")
#pragma pop_macro("EHOSTDOWN")
#pragma pop_macro("EHOSTUNREACH")
#pragma pop_macro("ENOTEMPTY")
#pragma pop_macro("EUSERS")
#pragma pop_macro("EDQUOT")
#pragma pop_macro("ESTALE")
#pragma pop_macro("EREMOTE")
#pragma pop_macro("ENOLCK")
#pragma pop_macro("ENOSYS")
#pragma pop_macro("ERESTART")
#pragma pop_macro("O_ACCMODE")
#pragma pop_macro("O_NONBLOCK")
#pragma pop_macro("O_APPEND")
#pragma pop_macro("O_ASYNC")
#pragma pop_macro("O_FSYNC")
#pragma pop_macro("O_CREAT")
#pragma pop_macro("O_TRUNC")
#pragma pop_macro("O_EXCL")
#pragma pop_macro("O_NOCTTY")
#pragma pop_macro("XXX")
#pragma pop_macro("XXX")
#pragma pop_macro("XXX")
#pragma pop_macro("FREAD")
#pragma pop_macro("FWRITE")
#pragma pop_macro("F_OK")
#pragma pop_macro("X_OK")
#pragma pop_macro("W_OK")
#pragma pop_macro("R_OK")
#pragma pop_macro("O_RDONLY")
#pragma pop_macro("O_WRONLY")
#pragma pop_macro("O_RDWR")
#pragma pop_macro("major")
#pragma pop_macro("minor")
#pragma pop_macro("makedev")
#pragma pop_macro("st_atime")
#pragma pop_macro("st_mtime")
#pragma pop_macro("st_ctime")
#pragma pop_macro("S_IFMT")
#pragma pop_macro("S_IFDIR")
#pragma pop_macro("S_IFCHR")
#pragma pop_macro("S_IFBLK")
#pragma pop_macro("S_IFREG")
#pragma pop_macro("S_IFLNK")
#pragma pop_macro("S_ISUID")
#pragma pop_macro("S_ISGID")
#pragma pop_macro("S_ISVTX")
#pragma pop_macro("S_IREAD")
#pragma pop_macro("S_IWRITE")
#pragma pop_macro("S_IEXEC")
#pragma pop_macro("S_IFSOCK")
#pragma pop_macro("S_IFIFO")
#pragma pop_macro("S_IRWXU")
#pragma pop_macro("S_IRUSR")
#pragma pop_macro("S_IWUSR")
#pragma pop_macro("S_IXUSR")
#pragma pop_macro("S_IRWXG")
#pragma pop_macro("S_IRGRP")
#pragma pop_macro("S_IWGRP")
#pragma pop_macro("S_IXGRP")
#pragma pop_macro("S_IRWXO")
#pragma pop_macro("S_IROTH")
#pragma pop_macro("S_IWOTH")
#pragma pop_macro("S_IXOTH")
#pragma pop_macro("S_ISDIR")
#pragma pop_macro("S_ISCHR")
#pragma pop_macro("S_ISBLK")
#pragma pop_macro("S_ISREG")
#pragma pop_macro("bcopy")
#pragma pop_macro("FD_SET")
#pragma pop_macro("FD_CLR")
#pragma pop_macro("FD_ISSET")
#pragma pop_macro("FD_ZERO")
#pragma pop_macro("ITIMER_REAL")
#pragma pop_macro("ITIMER_VIRTUAL")
#pragma pop_macro("ITIMER_PROF")
#pragma pop_macro("NFDBITS")
#pragma pop_macro("timercmp")
#pragma pop_macro("timerclear")
#pragma pop_macro("FNDELAY")
#pragma pop_macro("LOCK_SH")
#pragma pop_macro("LOCK_EX")
#pragma pop_macro("LOCK_NB")
#pragma pop_macro("LOCK_UN")
#pragma pop_macro("L_SET")
#pragma pop_macro("L_INCR")
#pragma pop_macro("L_XTND")

#endif /* __BSD211UNADAPT_H__ */
