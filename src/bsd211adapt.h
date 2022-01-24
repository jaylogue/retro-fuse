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
 * @file  Adaptation code used to help build ancient Unix C source code in
 * modern context.
 */

#ifndef __BSD211ADAPT_H__
#define __BSD211ADAPT_H__

#include "stdint.h"

/* Don't even think about trying to use this code on a big-endian machine. */
#if !defined(__LITTLE_ENDIAN__) && (!defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error Unsupported platform
#endif

/*
 * Allow 2.11BSD code to use certain names that collide with modern
 * preprocessor definitions, while still allowing the modern
 * definitions to be recovered if need be.
 */
#pragma push_macro("NULL")
#undef NULL
#pragma push_macro("EAGAIN")
#undef EAGAIN
#pragma push_macro("EWOULDBLOCK")
#undef EWOULDBLOCK
#pragma push_macro("ELOOP")
#undef ELOOP
#pragma push_macro("EDEADLK")
#undef EDEADLK
#pragma push_macro("EINPROGRESS")
#undef EINPROGRESS
#pragma push_macro("EALREADY")
#undef EALREADY
#pragma push_macro("ENOTSOCK")
#undef ENOTSOCK
#pragma push_macro("EDESTADDRREQ")
#undef EDESTADDRREQ
#pragma push_macro("EMSGSIZE")
#undef EMSGSIZE
#pragma push_macro("EPROTOTYPE")
#undef EPROTOTYPE
#pragma push_macro("ENOPROTOOPT")
#undef ENOPROTOOPT
#pragma push_macro("EPROTONOSUPPORT")
#undef EPROTONOSUPPORT
#pragma push_macro("ESOCKTNOSUPPORT")
#undef ESOCKTNOSUPPORT
#pragma push_macro("EOPNOTSUPP")
#undef EOPNOTSUPP
#pragma push_macro("EPFNOSUPPORT")
#undef EPFNOSUPPORT
#pragma push_macro("EAFNOSUPPORT")
#undef EAFNOSUPPORT
#pragma push_macro("EADDRINUSE")
#undef EADDRINUSE
#pragma push_macro("EADDRNOTAVAIL")
#undef EADDRNOTAVAIL
#pragma push_macro("ENETDOWN")
#undef ENETDOWN
#pragma push_macro("ENETUNREACH")
#undef ENETUNREACH
#pragma push_macro("ENETRESET")
#undef ENETRESET
#pragma push_macro("ECONNABORTED")
#undef ECONNABORTED
#pragma push_macro("ECONNRESET")
#undef ECONNRESET
#pragma push_macro("ENOBUFS")
#undef ENOBUFS
#pragma push_macro("EISCONN")
#undef EISCONN
#pragma push_macro("ENOTCONN")
#undef ENOTCONN
#pragma push_macro("ESHUTDOWN")
#undef ESHUTDOWN
#pragma push_macro("ETOOMANYREFS")
#undef ETOOMANYREFS
#pragma push_macro("ETIMEDOUT")
#undef ETIMEDOUT
#pragma push_macro("ECONNREFUSED")
#undef ECONNREFUSED
#pragma push_macro("ENAMETOOLONG")
#undef ENAMETOOLONG
#pragma push_macro("EHOSTDOWN")
#undef EHOSTDOWN
#pragma push_macro("EHOSTUNREACH")
#undef EHOSTUNREACH
#pragma push_macro("ENOTEMPTY")
#undef ENOTEMPTY
#pragma push_macro("EUSERS")
#undef EUSERS
#pragma push_macro("EDQUOT")
#undef EDQUOT
#pragma push_macro("ESTALE")
#undef ESTALE
#pragma push_macro("EREMOTE")
#undef EREMOTE
#pragma push_macro("ENOLCK")
#undef ENOLCK
#pragma push_macro("ENOSYS")
#undef ENOSYS
#pragma push_macro("ERESTART")
#undef ERESTART
#pragma push_macro("O_ACCMODE")
#undef O_ACCMODE
#pragma push_macro("O_NONBLOCK")
#undef O_NONBLOCK
#pragma push_macro("O_APPEND")
#undef O_APPEND
#pragma push_macro("O_ASYNC")
#undef O_ASYNC
#pragma push_macro("O_FSYNC")
#undef O_FSYNC
#pragma push_macro("O_CREAT")
#undef O_CREAT
#pragma push_macro("O_TRUNC")
#undef O_TRUNC
#pragma push_macro("O_EXCL")
#undef O_EXCL
#pragma push_macro("O_NOCTTY")
#undef O_NOCTTY
#pragma push_macro("XXX")
#undef XXX
#pragma push_macro("XXX")
#undef XXX
#pragma push_macro("XXX")
#undef XXX
#pragma push_macro("FREAD")
#undef FREAD
#pragma push_macro("FWRITE")
#undef FWRITE
#pragma push_macro("F_OK")
#undef F_OK
#pragma push_macro("X_OK")
#undef X_OK
#pragma push_macro("W_OK")
#undef W_OK
#pragma push_macro("R_OK")
#undef R_OK
#pragma push_macro("O_RDONLY")
#undef O_RDONLY
#pragma push_macro("O_WRONLY")
#undef O_WRONLY
#pragma push_macro("O_RDWR")
#undef O_RDWR
#pragma push_macro("major")
#undef major
#pragma push_macro("minor")
#undef minor
#pragma push_macro("makedev")
#undef makedev
#pragma push_macro("st_atime")
#undef st_atime
#define st_atime bsd211_st_atime
#pragma push_macro("st_mtime")
#undef st_mtime
#define st_mtime bsd211_st_mtime
#pragma push_macro("st_ctime")
#undef st_ctime
#define st_ctime bsd211_st_ctime
#pragma push_macro("S_IFMT")
#undef S_IFMT
#pragma push_macro("S_IFDIR")
#undef S_IFDIR
#pragma push_macro("S_IFCHR")
#undef S_IFCHR
#pragma push_macro("S_IFBLK")
#undef S_IFBLK
#pragma push_macro("S_IFREG")
#undef S_IFREG
#pragma push_macro("S_IFLNK")
#undef S_IFLNK
#pragma push_macro("S_ISUID")
#undef S_ISUID
#pragma push_macro("S_ISGID")
#undef S_ISGID
#pragma push_macro("S_ISVTX")
#undef S_ISVTX
#pragma push_macro("S_IREAD")
#undef S_IREAD
#pragma push_macro("S_IWRITE")
#undef S_IWRITE
#pragma push_macro("S_IEXEC")
#undef S_IEXEC
#pragma push_macro("S_IFSOCK")
#undef S_IFSOCK
#pragma push_macro("S_IFIFO")
#undef S_IFIFO
#pragma push_macro("S_IRWXU")
#undef S_IRWXU
#pragma push_macro("S_IRUSR")
#undef S_IRUSR
#pragma push_macro("S_IWUSR")
#undef S_IWUSR
#pragma push_macro("S_IXUSR")
#undef S_IXUSR
#pragma push_macro("S_IRWXG")
#undef S_IRWXG
#pragma push_macro("S_IRGRP")
#undef S_IRGRP
#pragma push_macro("S_IWGRP")
#undef S_IWGRP
#pragma push_macro("S_IXGRP")
#undef S_IXGRP
#pragma push_macro("S_IRWXO")
#undef S_IRWXO
#pragma push_macro("S_IROTH")
#undef S_IROTH
#pragma push_macro("S_IWOTH")
#undef S_IWOTH
#pragma push_macro("S_IXOTH")
#undef S_IXOTH
#pragma push_macro("S_ISDIR")
#undef S_ISDIR
#pragma push_macro("S_ISCHR")
#undef S_ISCHR
#pragma push_macro("S_ISBLK")
#undef S_ISBLK
#pragma push_macro("S_ISREG")
#undef S_ISREG
#pragma push_macro("bcopy")
#undef bcopy
#pragma push_macro("FD_SET")
#undef FD_SET
#pragma push_macro("FD_CLR")
#undef FD_CLR
#pragma push_macro("FD_ISSET")
#undef FD_ISSET
#pragma push_macro("FD_ZERO")
#undef FD_ZERO
#pragma push_macro("ITIMER_REAL")
#undef ITIMER_REAL
#pragma push_macro("ITIMER_VIRTUAL")
#undef ITIMER_VIRTUAL
#pragma push_macro("ITIMER_PROF")
#undef ITIMER_PROF
#pragma push_macro("F_GETOWN")
#undef F_GETOWN
#pragma push_macro("F_SETOWN")
#undef F_SETOWN
#pragma push_macro("NFDBITS")
#undef NFDBITS
#pragma push_macro("timercmp")
#undef timercmp
#pragma push_macro("timerclear")
#undef timerclear
#pragma push_macro("FNDELAY")
#undef FNDELAY
#pragma push_macro("LOCK_SH")
#undef LOCK_SH
#pragma push_macro("LOCK_EX")
#undef LOCK_EX
#pragma push_macro("LOCK_NB")
#undef LOCK_NB
#pragma push_macro("LOCK_UN")
#undef LOCK_UN
#pragma push_macro("L_SET")
#undef L_SET
#pragma push_macro("L_INCR")
#undef L_INCR
#pragma push_macro("L_XTND")
#undef L_XTND
#pragma push_macro("bzero")
#undef bzero
#pragma push_macro("NBBY")
#undef NBBY
#pragma push_macro("ntohs")
#undef ntohs
#pragma push_macro("htons")
#undef htons
#pragma push_macro("ntohl")
#undef ntohl
#pragma push_macro("htonl")
#undef htonl
#pragma push_macro("NSIG")
#undef NSIG
#pragma push_macro("sa_handler")
#undef sa_handler
#pragma push_macro("_SYS_RESOURCE_H_")
#undef _SYS_RESOURCE_H_
#pragma push_macro("_SYS_TIME_H_")
#undef _SYS_TIME_H_
#pragma push_macro("SIG_ERR")
#undef SIG_ERR
#pragma push_macro("SIG_DFL")
#undef SIG_DFL
#pragma push_macro("SIG_IGN")
#undef SIG_IGN
#pragma push_macro("SIGSTKSZ")
#undef SIGSTKSZ
#pragma push_macro("sigmask")
#undef sigmask
#pragma push_macro("RLIMIT_RSS")
#undef RLIMIT_RSS
#pragma push_macro("RLIM_NLIMITS")
#undef RLIM_NLIMITS
#pragma push_macro("RLIM_INFINITY")
#undef RLIM_INFINITY
#pragma push_macro("ELAST")
#undef ELAST
#pragma push_macro("O_SHLOCK")
#undef O_SHLOCK
#pragma push_macro("O_SHLOCK")
#undef O_SHLOCK
#pragma push_macro("SIGIOT")
#undef SIGIOT
#pragma push_macro("UF_SETTABLE")
#undef UF_SETTABLE
#pragma push_macro("UF_NODUMP")
#undef UF_NODUMP
#pragma push_macro("UF_IMMUTABLE")
#undef UF_IMMUTABLE
#pragma push_macro("UF_APPEND")
#undef UF_APPEND
#pragma push_macro("SF_SETTABLE")
#undef SF_SETTABLE
#pragma push_macro("SF_ARCHIVED")
#undef SF_ARCHIVED
#pragma push_macro("SF_IMMUTABLE")
#undef SF_IMMUTABLE
#pragma push_macro("SF_APPEND")
#undef SF_APPEND
#pragma push_macro("SIGABRT")
#undef SIGABRT
#pragma push_macro("MINSIGSTKSZ")
#undef MINSIGSTKSZ
#pragma push_macro("O_EXLOCK")
#undef O_EXLOCK

/* General 2.11BSD Configuration Options */
#define KERNEL
#define NBUF 32             /* Size of buffer pool */
#define NFILE 128           /* Number of open files */
#define NINODE 128          /* Size of inode cache */

/* Forward declarations of 2.11BSD types, suitably name-mangled */
struct bsd211_buf;
struct bsd211_bufhd;
struct bsd211_dinode;
struct bsd211_direct;
struct bsd211_fblk;
struct bsd211_file;
struct bsd211_fs;
struct bsd211_inode;
struct bsd211_iovec;
struct bsd211_mount;
struct bsd211_namecache;
struct bsd211_nameidata;
struct bsd211_proc;
struct bsd211_sigaltstack;
struct bsd211_stat;
struct bsd211_timeval;
struct bsd211_uio;
struct bsd211_vattr;

enum bsd211_uio_rw;
enum bsd211_uio_seg;

typedef void * bsd211_caddr_t;
typedef int32_t bsd211_daddr_t;
typedef int16_t bsd211_dev_t;
typedef uint16_t bsd211_ino_t;
typedef int32_t bsd211_off_t;
typedef	uint16_t bsd211_size_t;
typedef	int16_t	bsd211_ssize_t;
typedef int32_t bsd211_time_t;
typedef	uint16_t bsd211_uid_t;
typedef	uint16_t bsd211_gid_t;

/*
 * Forward declarations of name-mangled 2.11BSD functions
 */

/****** bcmp.s ******/
extern int16_t bsd211_bcmp(bsd211_caddr_t b1, bsd211_caddr_t b2, int16_t length);

/****** bcopy.s ******/
extern void bsd211_bcopy(bsd211_caddr_t from, bsd211_caddr_t to, int16_t count);

/****** bzero.s ******/
extern void bsd211_bzero(bsd211_caddr_t b, uint16_t length);

/****** init_main.c ******/
extern void bsd211_bhinit();
extern void bsd211_binit();

/****** insque.s ******/
extern void bsd211_insque(void * elem, void * pred);

/****** kern_descrip.c ******/
extern void bsd211_close();
extern int16_t bsd211_ufalloc(int16_t i);
extern struct bsd211_file * bsd211_falloc();
extern struct bsd211_file * bsd211_getf(int16_t f);
extern int16_t bsd211_closef(struct bsd211_file *fp);
extern int16_t bsd211_dupfdopen(int16_t indx, int16_t dfd, int16_t mode, int16_t error);

/****** kern_prot.c ******/
extern int16_t bsd211_groupmember(bsd211_gid_t gid);

/****** kern_synch.c ******/
extern void bsd211_sleep(bsd211_caddr_t chan, int16_t pri);
extern void bsd211_wakeup(bsd211_caddr_t chan);

/****** kern_subr.c ******/
extern int16_t bsd211_uiomove(bsd211_caddr_t cp, uint16_t n, struct bsd211_uio *uio);

/****** mch_copy.s ******/
extern int16_t bsd211_fubyte(bsd211_caddr_t base);
extern int16_t bsd211_copyin(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t length);
extern int16_t bsd211_copyout(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t length);
extern int16_t bsd211_copyinstr(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t maxlength, uint16_t *lencopied);

/****** mch_xxx.s ******/
extern void bsd211_clrbuf(struct bsd211_buf *bp);
extern bsd211_caddr_t bsd211_mapin(struct bsd211_buf * bp);
extern void bsd211_mapout(struct bsd211_buf * bp);
extern int16_t bsd211_copystr(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t maxlength, uint16_t *lencopied);
extern void bsd211_vattr_null(struct bsd211_vattr *vp);

/****** mem_clicks.s ******/
extern void bsd211_copy(bsd211_caddr_t src, bsd211_caddr_t dest, int16_t len);

/****** psignal.c ******/
extern void bsd211_psignal(struct bsd211_proc *p, int16_t sig);

/****** remque.s ******/
extern void bsd211_remque(void * elem);

/****** subr_prf.c ******/
extern void bsd211_printf(const char * str, ...);
extern void bsd211_uprintf(const char *fmt, ...);
extern void bsd211_log(int16_t level, const char *fmt, ...);
extern void bsd211_panic(const char * s);
extern void bsd211_tablefull(char *tab);

/****** strlen.s ******/
extern bsd211_size_t bsd211_strlen(const char *s);

/****** sys_generic.c ******/
extern void bsd211_read();
extern void bsd211_write();

/****** sys_inode.c ******/
extern int16_t bsd211_ino_rw(struct bsd211_file *fp, struct bsd211_uio *uio);
extern int16_t bsd211_rdwri(enum bsd211_uio_rw rw, struct bsd211_inode *ip, bsd211_caddr_t base,
	                        int16_t len, bsd211_off_t offset, enum bsd211_uio_seg segflg,
                            int16_t ioflg, int16_t *aresid);
extern int16_t bsd211_rwip(struct bsd211_inode *ip, struct bsd211_uio *uio, int16_t ioflag);
extern int16_t bsd211_ino_stat(struct bsd211_inode *ip, struct bsd211_stat *sb);
extern int16_t bsd211_ino_lock(struct bsd211_file *fp, int16_t cmd);
extern int16_t bsd211_ino_unlock(struct bsd211_file *fp, int16_t kind);

/****** ufs_alloc.c ******/
extern struct bsd211_buf * bsd211_balloc(struct bsd211_inode *ip, int16_t flags);
extern struct bsd211_inode * bsd211_ialloc(struct bsd211_inode *pip);
extern void bsd211_free(struct bsd211_inode *ip, bsd211_daddr_t bno);
extern void bsd211_ifree(struct bsd211_inode *ip, bsd211_ino_t ino);
extern void bsd211_fserr(struct bsd211_fs *fp, char *cp);

/****** ufs_bio.c ******/
extern struct bsd211_buf * bsd211_bread(bsd211_dev_t dev, bsd211_daddr_t blkno);
extern struct bsd211_buf * bsd211_breada(bsd211_dev_t dev, bsd211_daddr_t blkno, bsd211_daddr_t rablkno);
extern void bsd211_bwrite(struct bsd211_buf *bp);
extern void bsd211_bdwrite(struct bsd211_buf *bp);
extern void bsd211_brelse(struct bsd211_buf *bp);
extern int16_t bsd211_incore(bsd211_dev_t dev, bsd211_daddr_t blkno);
extern struct bsd211_buf * bsd211_getblk(bsd211_dev_t dev, bsd211_daddr_t blkno);
extern struct bsd211_buf * bsd211_geteblk();
extern struct bsd211_buf * bsd211_getnewbuf();
extern void bsd211_biowait(struct bsd211_buf *bp);
extern void bsd211_biodone(struct bsd211_buf *bp);
extern void bsd211_blkflush(bsd211_dev_t dev, bsd211_daddr_t blkno);
extern void bsd211_bflush(int16_t dev);
extern int16_t bsd211_geterror(struct bsd211_buf *abp);
extern void bsd211_binval(bsd211_dev_t dev);

/****** ufs_bmap.c ******/
extern bsd211_daddr_t bsd211_bmap(struct bsd211_inode *ip, bsd211_daddr_t bn, int16_t rwflg, int16_t flags);

/****** ufs_fio.c ******/
extern int16_t bsd211_access(struct bsd211_inode *ip, int16_t mode);
extern int16_t bsd211_suser();
extern int16_t bsd211_ufs_setattr(struct bsd211_inode *ip, struct bsd211_vattr *vap);
extern int16_t bsd211_ufs_mountedon(bsd211_dev_t dev);

/****** ufs_inode.c ******/
extern void bsd211_ihinit();
extern struct bsd211_inode *bsd211_ifind(bsd211_dev_t dev, bsd211_ino_t ino);
extern struct bsd211_inode * bsd211_iget(bsd211_dev_t dev, struct bsd211_fs *fs, bsd211_ino_t ino);
extern void bsd211_igrab(struct bsd211_inode *ip);
extern void bsd211_iput(struct bsd211_inode *ip);
extern void bsd211_irele(struct bsd211_inode *ip);
extern void bsd211_iupdat(struct bsd211_inode *p, struct bsd211_timeval *ta, struct bsd211_timeval *tm, int16_t waitfor);
extern void bsd211_itrunc(struct bsd211_inode *oip, uint32_t length, int16_t	ioflags);
extern void bsd211_indirtrunc(struct bsd211_inode *ip, bsd211_daddr_t bn, bsd211_daddr_t lastbn, int16_t level, int16_t aflags);
extern int16_t bsd211_iflush(bsd211_dev_t dev);
extern void bsd211_ilock(struct bsd211_inode *ip);
extern void bsd211_iunlock(struct bsd211_inode *ip);

/****** ufs_nami.c ******/
extern struct bsd211_inode * bsd211_namei(struct bsd211_nameidata *ndp);
extern void bsd211_dirbad(struct bsd211_inode *ip, bsd211_off_t offset, char *how);
extern int16_t bsd211_dirbadentry(struct bsd211_direct *ep, int16_t entryoffsetinblock);
extern int16_t bsd211_direnter(struct bsd211_inode *ip, struct bsd211_nameidata *ndp);
extern int16_t bsd211_dirremove(struct bsd211_nameidata *ndp);
extern void bsd211_dirrewrite(struct bsd211_inode *dp, struct bsd211_inode *ip, struct bsd211_nameidata *ndp);
extern int16_t bsd211_dirempty(struct bsd211_inode *ip, bsd211_ino_t parentino);
extern int16_t bsd211_checkpath(struct bsd211_inode *source, struct bsd211_inode *target);
extern void bsd211_nchinit();
extern void bsd211_nchinval(bsd211_dev_t dev); // TODO: not sure if this will be used
extern void bsd211_cacheinvalall(); // TODO: not sure if this will be used

/****** ufs_subr.c ******/
extern void bsd211_syncip(struct bsd211_inode *ip);
extern void bsd211_clean(); // TODO: not sure if this will be used
extern int16_t bsd211_badblock(struct bsd211_fs *fp, bsd211_daddr_t bn);
extern struct bsd211_fs * bsd211_getfs(bsd211_dev_t dev);

/****** ufs_syscalls.c ******/
extern void bsd211_open();
extern void bsd211_mknod();
extern void bsd211_link();
extern void bsd211_symlink();
extern void bsd211_unlink();
extern void bsd211_saccess();
extern void bsd211_stat1(int16_t follow);
extern void bsd211_readlink();
extern void bsd211_chmod();
extern int16_t bsd211_chmod1(struct bsd211_inode *ip, int16_t mode);
extern int16_t bsd211_chown1(struct bsd211_inode *ip, int16_t uid, int16_t gid);
extern void bsd211_truncate();
extern void bsd211_ftruncate();
extern void bsd211_rename();
extern struct bsd211_inode * bsd211_maknode(int16_t mode, struct bsd211_nameidata *ndp);
extern void bsd211_mkdir(); // TODO: not sure if this will be used
extern void bsd211_rmdir(); // TODO: not sure if this will be used
extern struct bsd211_inode * bsd211_getinode(int16_t fdes);

/****** ufs_syscalls2.c ******/
extern int16_t bsd211_ufs_sync(struct bsd211_mount *mp);
extern void bsd211_syncinodes(struct bsd211_fs *fs); // TODO: not sure if this will be used
extern void bsd211_fsync(); // TODO: not sure if this will be used
extern void bsd211_utimes(); // TODO: not sure if this will be used

/****** vfs_vnops.c ******/
extern int16_t bsd211_vn_open(struct bsd211_nameidata *ndp, int16_t fmode, int16_t cmode);
extern int16_t bsd211_vn_closefile(struct bsd211_file *fp);

// TODO: remove
#if UNUSED

/* main.c */
// TODO: not needed: extern void bsd211_iinit();
// TODO: not needed: extern void bsd211_binit();

/* rdwri.c */
extern void bsd211_readi(struct bsd211_inode *aip);
extern void bsd211_writei(struct bsd211_inode *aip);
extern void bsd211_iomove(bsd211_caddr_t cp, int16_t n, int16_t flag);

/* nami.c */
extern int16_t bsd211_schar();
extern int16_t bsd211_uchar();
extern int16_t bsd211_symchar();

/* fio.c */
extern struct bsd211_file * bsd211_getf(int16_t f);
extern void bsd211_closef(struct bsd211_file *fp);
extern struct bsd211_inode * bsd211_owner(int16_t follow);
extern int16_t bsd211_own();
extern int16_t bsd211_ufalloc();
extern struct bsd211_file * bsd211_falloc();

/* sys2.c */
extern void bsd211_rdwr(int16_t mode);
extern void bsd211_open1(struct bsd211_inode *ip, int16_t mode, int16_t trf);
extern void bsd211_close();

/* sys3.c */

/* sys4.c */

/* text.c */
extern void bsd211_xrele(struct bsd211_inode *ip);

#endif

/*
 * Replacements for select definitions in types.h
 */
typedef struct bsd211_label_t { } bsd211_label_t;

/*
 * Replacements for definitions in seg.h
 */
#define _SEG_ /* suppress inclusion of sec.h */
typedef struct bsd211_segm_reg {
	uint16_t	se_desc;
	void *	se_addr;
} bsd211_segm;
extern bsd211_caddr_t bsd211_SEG5;
#define saveseg5(save) { (void)(save); }
#define restorseg5(save) { (void)(save); }
#define mapseg5(addr,desc) { bsd211_SEG5 = (void *)(intptr_t)(addr); (void)(desc); }
#define normalseg5() { bsd211_SEG5 = NULL; }
#define RW 0

/* Replacement for splbio/splx functions */
#define splbio() (0)
#define splx(v) ((void)v)

/* Replacements for definitions in text.h/text.c */
struct bsd211_text { };
#define xuntext(x) ((void)(x))

/* Replacements for definitions in acct.h */
#define ASU 0

/*
 * Utility functions
 */

extern void bsd211_refreshclock();
extern void bsd211_zerocore();

/* Convert between little-endian and pdp11-endian byte-ordering */
static inline uint32_t wswap_uint32(uint32_t v)
{
    return ((v << 16) | (v >> 16));
}
static inline int32_t wswap_int32(int32_t v)
{
    return (int32_t)wswap_uint32((uint32_t)v);
}

/* Map various names used by BSD code to avoid any conflicts with modern code
 */
#define _cinvall    bsd211_cacheinvalall
#define access      bsd211_access
#define balloc      bsd211_balloc
#define badblock    bsd211_badblock
#define bcmp        bsd211_bcmp
#define bcopy       bsd211_bcopy
#define bdevsw      bsd211_bdevsw
#define bdwrite     bsd211_bdwrite
#define bflush      bsd211_bflush
#define bfreelist   bsd211_bfreelist
#define binval      bsd211_binval
#define biodone     bsd211_biodone
#define biowait     bsd211_biowait
#define blkatoff    bsd211_blkatoff
#define blkflush    bsd211_blkflush
#define bmap        bsd211_bmap
#define bread       bsd211_bread
#define breada      bsd211_breada
#define brelse      bsd211_brelse
#define buf         bsd211_buf
#define bufhash     bsd211_bufhash
#define bufhd       bsd211_bufhd
#define bwrite      bsd211_bwrite
#define bzero       bsd211_bzero
#define caddr_t     bsd211_caddr_t
#define cdevsw      bsd211_cdevsw
#define checkpath   bsd211_checkpath
#define chmod       bsd211_chmod
#define chmod1      bsd211_chmod1
#define chown1      bsd211_chown1
#define clean       bsd211_clean
#define clockinfo   bsd211_clockinfo
#define close       bsd211_close
#define closef      bsd211_closef
#define clrbuf      bsd211_clrbuf
#define copy        bsd211_copy
#define copyin      bsd211_copyin
#define copyinstr   bsd211_copyinstr
#define copyout     bsd211_copyout
#define copystr     bsd211_copystr
#define daddr_t     bsd211_daddr_t
#define dev_t       bsd211_dev_t
#define dinode      bsd211_dinode
#define dirchk      bsd211_dirchk
#define dirbad      bsd211_dirbad
#define dirbadentry bsd211_dirbadentry
#define direct      bsd211_direct
#define dirempty    bsd211_dirempty
#define direnter    bsd211_direnter
#define dirremove   bsd211_dirremove
#define dirrewrite  bsd211_dirrewrite
#define dirtemplate bsd211_dirtemplate
#define dupfdopen   bsd211_dupfdopen
#define falloc      bsd211_falloc
#define fblk        bsd211_fblk
#define fd_set      bsd211_fd_set
#define file        bsd211_file
#define fileNFILE   bsd211_fileNFILE
#define Fops        bsd211_Fops
#define free        bsd211_free
#define fs          bsd211_fs
#define fserr       bsd211_fserr
#define fsync       bsd211_fsync
#define ftruncate   bsd211_ftruncate
#define fubyte      bsd211_fubyte
#define getblk      bsd211_getblk
#define geteblk     bsd211_geteblk
#define geterror    bsd211_geterror
#define getf        bsd211_getf
#define getfs       bsd211_getfs
#define getfsstat   bsd211_getfsstat
#define getinode    bsd211_getinode
#define getnewbuf   bsd211_getnewbuf
#define gid_t       bsd211_gid_t
#define groupmember bsd211_groupmember
#define ialloc      bsd211_ialloc
#define ifind       bsd211_ifind
#define iflush      bsd211_iflush
#define ifree       bsd211_ifree
#define ifreeh      bsd211_ifreeh
#define ifreet      bsd211_ifreet
#define iget        bsd211_iget
#define igrab       bsd211_igrab
#define ihead       bsd211_ihead
#define ihinit      bsd211_ihinit
#define ilock       bsd211_ilock
#define incore      bsd211_incore
#define indirtrunc  bsd211_indirtrunc
#define ino_rw      bsd211_ino_rw
#define ino_stat    bsd211_ino_stat
#define ino_t       bsd211_ino_t
#define inode       bsd211_inode
#define inodeNINODE bsd211_inodeNINODE
#define inodeops    bsd211_inodeops
#define iovec       bsd211_iovec
#define iput        bsd211_iput
#define irele       bsd211_irele
#define insque      bsd211_insque
#define itimerval   bsd211_itimerval
#define itrunc      bsd211_itrunc
#define iunlock     bsd211_iunlock
#define iupdat      bsd211_iupdat
#define label_t     bsd211_label_t
#define lastf       bsd211_lastf
#define link        bsd211_link
#define log         bsd211_log
#define maknode     bsd211_maknode
#define malloc      bsd211_malloc
#define mapin       bsd211_mapin
#define mapout      bsd211_mapout
#define mastertemplate bsd211_mastertemplate
#define mkdir       bsd211_mkdir
#define mknod       bsd211_mknod
#define mode_t      bsd211_mode_t
#define mount       bsd211_mount
#define namecache   bsd211_namecache
#define namei       bsd211_namei
#define nameidata   bsd211_nameidata
#define nblkdev     bsd211_nblkdev
#define nbuf        bsd211_nbuf
#define nchash      bsd211_nchash
#define nchhead     bsd211_nchhead
#define nchtail     bsd211_nchtail
#define nchinit     bsd211_nchinit
#define nchinval    bsd211_nchinval
#define nchrdev     bsd211_nchrdev
#define nchsize     bsd211_nchsize
#define nchstats    bsd211_nchstats
#define nextinodeid bsd211_nextinodeid
#define ninode      bsd211_ninode
#define nmidesc     bsd211_nmidesc
#define off_t       bsd211_off_t
#define open        bsd211_open
#define panic       bsd211_panic
#define pid_t       bsd211_pid_t
#define printf      bsd211_printf
#define proc        bsd211_proc
#define psignal     bsd211_psignal
#define rablock     bsd211_rablock
#define rdwri       bsd211_rdwri
#define read        bsd211_read
#define readlink    bsd211_readlink
#define remque      bsd211_remque
#define rename      bsd211_rename
#define rlimit      bsd211_rlimit
#define rmdir       bsd211_rmdir
#define rootdev     bsd211_rootdev
#define rootdir     bsd211_rootdir
#define rusage      bsd211_rusage
#define rwip        bsd211_rwip
#define rwuio       bsd211_rwuio
#define saccess     bsd211_saccess
#define securelevel bsd211_securelevel
#define SEG5        bsd211_SEG5
#define segm        bsd211_segm
#define sig_t       bsd211_sig_t
#define sigaction   bsd211_sigaction
#define sigaltstack bsd211_sigaltstack
#define sigset_t    bsd211_sigset_t
#define sigstack    bsd211_sigstack
#define sigvec      bsd211_sigvec
#define size_t      bsd211_size_t
#define sleep       bsd211_sleep
#define ssize_t     bsd211_ssize_t
#define stat        bsd211_stat
#define stat1       bsd211_stat1
#define strlen      bsd211_strlen
#define suser       bsd211_suser
#define symlink     bsd211_symlink
#define syncinodes  bsd211_syncinodes
#define syncip      bsd211_syncip
#define tablefull   bsd211_tablefull
#define time        bsd211_time
#define time_t      bsd211_time_t
#define timespec    bsd211_timespec
#define timeval     bsd211_timeval
#define timezone    bsd211_timezone
#define trsingle    bsd211_trsingle
#define truncate    bsd211_truncate
#define u           bsd211_u
#define u_char      bsd211_u_char
#define u_short     bsd211_u_short
#define u_int       bsd211_u_int
#define u_long      bsd211_u_long
#define ufalloc     bsd211_ufalloc
#define ufs_mountedon bsd211_ufs_mountedon
#define ufs_setattr bsd211_ufs_setattr
#define ufs_sync    bsd211_ufs_sync
#define uid_t       bsd211_uid_t
#define uio         bsd211_uio
#define uio_rw      bsd211_uio_rw
#define uio_seg     bsd211_uio_seg
#define uiomove     bsd211_uiomove
#define unlink      bsd211_unlink
#define updlock     bsd211_updlock
#define uprintf     bsd211_uprintf
#define user        bsd211_user
#define ushort      bsd211_ushort
#define utimes      bsd211_utimes
#define vattr       bsd211_vattr
#define vattr_null  bsd211_vattr_null
#define vn_open     bsd211_vn_open
#define vn_closefile bsd211_vn_closefile
#define wakeup      bsd211_wakeup
#define write       bsd211_write

#endif /* __BSD211ADAPT_H__ */
