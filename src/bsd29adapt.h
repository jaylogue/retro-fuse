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

#ifndef __BSD29ADAPT_H__
#define __BSD29ADAPT_H__

#include "stdint.h"

/* Don't even think about trying to use this code on a big-endian machine. */
#if !defined(__LITTLE_ENDIAN__) && (!defined(__BYTE_ORDER__) || __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__)
#error Unsupported platform
#endif

/* Allow 2.9BSD code to use certain names that collide with modern
 * preprocessor definitions, while still allowing the modern
 * definitions to be recovered if need be.
 */
#pragma push_macro("NULL")
#undef NULL
#pragma push_macro("EAGAIN")
#undef EAGAIN
#pragma push_macro("ELOOP")
#undef ELOOP
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
#define st_atime bsd29_st_atime
#pragma push_macro("st_mtime")
#undef st_mtime
#define st_mtime bsd29_st_mtime
#pragma push_macro("st_ctime")
#undef st_ctime
#define st_ctime bsd29_st_ctime
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
#pragma push_macro("bcopy")
#undef bcopy
#pragma push_macro("L_SET")
#undef L_SET
#pragma push_macro("L_INCR")
#undef L_INCR
#pragma push_macro("L_XTND")
#undef L_XTND
#pragma push_macro("NSIG")
#undef NSIG

/* General 2.9BSD Configuration Options */
#define KERNEL
#define NSIG 1              /* Number of signals; unused in FUSE code, but necessary for compilation */
#define NOVL 1              /* Number of text overlays; unused in FUSE code, but necessary for compilation */
#define NBUF 32             /* Size of buffer pool */
#define NMOUNT 1            /* Number of mount table entries */
#define NFILE 128           /* Number of open files */
#define NINODE 128          /* Size of inode cache */

/* Forward declarations of 2.9BSD types, suitably name-mangled */
struct bsd29_buf;
struct bsd29_filsys;
struct bsd29_inode;
struct bsd29_dinode;
struct bsd29_file;
struct bsd29_stat;
struct bsd29_devtab;
struct bsd29_direct;
struct bsd29_fblk;

typedef int32_t bsd29_daddr_t;
typedef int16_t bsd29_dev_t;
typedef char * bsd29_caddr_t;
typedef uint16_t bsd29_ino_t;
typedef int32_t bsd29_time_t;
typedef int32_t bsd29_off_t;
typedef int bsd29_segm;

/* Forward declarations of name-mangled 2.9BSD functions */

/* main.c */
extern void bsd29_iinit();
extern void bsd29_binit();

/* machdep.c */
extern bsd29_caddr_t bsd29_mapin(struct bsd29_buf *bp);
extern void bsd29_mapout(struct bsd29_buf *bp);

/* bio.c */
extern struct bsd29_buf * bsd29_bread(bsd29_dev_t dev, bsd29_daddr_t blkno);
extern struct bsd29_buf * bsd29_breada(bsd29_dev_t dev, bsd29_daddr_t blkno, bsd29_daddr_t rablkno);
extern void bsd29_bwrite(struct bsd29_buf *bp);
extern void bsd29_bdwrite(struct bsd29_buf *bp);
extern void bsd29_bawrite(struct bsd29_buf *bp);
extern void bsd29_brelse(struct bsd29_buf *bp);
extern int16_t bsd29_incore(bsd29_dev_t dev, bsd29_daddr_t blkno);
extern struct bsd29_buf * bsd29_getblk(bsd29_dev_t dev, bsd29_daddr_t blkno);
extern struct bsd29_buf * bsd29_geteblk();
extern void bsd29_bunhash(struct bsd29_buf *bp);
extern void bsd29_iowait(struct bsd29_buf *bp);
extern void bsd29_notavail(struct bsd29_buf *bp);
extern void bsd29_iodone(struct bsd29_buf *bp);
extern void bsd29_clrbuf(struct bsd29_buf *bp);
extern void bsd29_bflush(int16_t dev);
extern void bsd29_geterror(struct bsd29_buf *abp);

/* slp.c */
extern void bsd29_sleep(bsd29_caddr_t chan, int16_t pri);
extern void bsd29_wakeup(bsd29_caddr_t chan);

/* prf.c */
extern void bsd29_printf(const char * str, ...);
extern void bsd29_uprintf(const char *fmt, ...);
extern void bsd29_panic(const char * s);
extern void bsd29_tablefull(char *tab);
extern void bsd29_fserr(struct bsd29_filsys *fp, char *cp);

/* subr.c */
extern bsd29_daddr_t bsd29_bmap(struct bsd29_inode *ip, bsd29_daddr_t bn, int16_t rwflg);
extern void bsd29_bcopy(bsd29_caddr_t from, bsd29_caddr_t to, int16_t count);

/* alloc.c */
extern struct bsd29_buf * bsd29_alloc(bsd29_dev_t dev);
extern void bsd29_free(bsd29_dev_t dev, bsd29_daddr_t bno);
extern int16_t bsd29_badblock(struct bsd29_filsys *fp, bsd29_daddr_t bn);
extern struct bsd29_inode * bsd29_ialloc(bsd29_dev_t dev);
extern void bsd29_ifree(bsd29_dev_t dev, bsd29_ino_t ino);
extern struct bsd29_filsys * bsd29_getfs(bsd29_dev_t dev);
extern void bsd29_update();

/* iget.c */
extern void bsd29_ihinit();
extern struct bsd29_inode *bsd29_ifind(bsd29_dev_t dev, bsd29_ino_t ino);
extern struct bsd29_inode * bsd29_iget(bsd29_dev_t dev, bsd29_ino_t ino);
extern void bsd29_iexpand(struct bsd29_inode *ip, struct bsd29_dinode *dp);
extern void bsd29_iput(struct bsd29_inode *ip);
extern void bsd29_iupdat(struct bsd29_inode *p, bsd29_time_t *ta, bsd29_time_t *tm, int16_t waitfor);
extern void bsd29_itrunc(struct bsd29_inode *ip);
extern void bsd29_tloop(bsd29_dev_t dev, bsd29_daddr_t bn, int16_t f1, int16_t f2);
extern struct bsd29_inode * bsd29_maknode(int16_t mode);
extern void bsd29_wdir(struct bsd29_inode *ip);

/* pipe.c */
extern void bsd29_prele(struct bsd29_inode *ip);
extern void bsd29_readp(struct bsd29_file *fp);
extern void bsd29_writep(struct bsd29_file *fp);
extern void bsd29_plock(struct bsd29_inode *ip);

/* mch.s */
extern void bsd29_spl0();
extern int16_t bsd29_spl6();
extern int16_t bsd29_copyout(bsd29_caddr_t s, bsd29_caddr_t d, int16_t n);
extern int16_t bsd29_fubyte(const void *base);

/* rdwri.c */
extern void bsd29_readi(struct bsd29_inode *aip);
extern void bsd29_writei(struct bsd29_inode *aip);
extern void bsd29_iomove(bsd29_caddr_t cp, int16_t n, int16_t flag);

/* nami.c */
extern struct bsd29_inode * bsd29_namei(int16_t (*func)(), int16_t flag, int16_t follow);
extern int16_t bsd29_schar();
extern int16_t bsd29_uchar();
extern int16_t bsd29_symchar();

/* fio.c */
extern struct bsd29_file * bsd29_getf(int16_t f);
extern void bsd29_closef(struct bsd29_file *fp);
extern void bsd29_openi(struct bsd29_inode *ip, int16_t rw);
extern int16_t bsd29_access(struct bsd29_inode *aip, int16_t mode);
extern struct bsd29_inode * bsd29_owner(int16_t follow);
extern int16_t bsd29_own();
extern int16_t bsd29_suser();
extern int16_t bsd29_ufalloc();
extern struct bsd29_file * bsd29_falloc();

/* sys2.c */
extern void bsd29_rdwr(int16_t mode);
extern void bsd29_open1(struct bsd29_inode *ip, int16_t mode, int16_t trf);
extern void bsd29_close();
extern void bsd29_link();
extern void bsd29_mknod();

/* sys3.c */
extern void bsd29_stat1(struct bsd29_inode *ip, struct bsd29_stat *ub, bsd29_off_t pipeadj);
extern void bsd29_readlink();
extern void bsd29_symlink();

/* sys4.c */
extern void bsd29_unlink();
extern void bsd29_chmod();

/* text.c */
extern void bsd29_xrele(struct bsd29_inode *ip);

/* Replacements for various macro definitions in 2.9BSD code */

/* param.h */
#define splx(ops) ((void)ops)

/* seg.h */
extern bsd29_caddr_t bsd29_SEG5;
#define	saveseg5(save) {(void)save;}
#define	restorseg5(save) {(void)save;}

/* Utility functions */

extern void bsd29_zerocore();
extern void bsd29_refreshclock();

/* Convert between little-endian and pdp11-endian byte-ordering */
static inline int32_t wswap_int32(int32_t v)
{
    uint32_t uv = (uint32_t)v;
    return (int32_t)((uv << 16) | (uv >> 16));
}

/* Map various names used by BSD code to avoid any conflicts with modern code
 */
#define access      bsd29_access
#define alloc       bsd29_alloc
#define badblock    bsd29_badblock
#define bawrite     bsd29_bawrite
#define bcopy       bsd29_bcopy
#define bdevsw      bsd29_bdevsw
#define bdwrite     bsd29_bdwrite
#define bflush      bsd29_bflush
#define bfreelist   bsd29_bfreelist
#define bhash       bsd29_bhash
#define binit       bsd29_binit
#define bmap        bsd29_bmap
#define bread       bsd29_bread
#define breada      bsd29_breada
#define brelse      bsd29_brelse
#define bsize       bsd29_bsize
#define buf         bsd29_buf
#define buffers     bsd29_buffers
#define bunhash     bsd29_bunhash
#define bwrite      bsd29_bwrite
#define caddr_t     bsd29_caddr_t
#define callo       bsd29_callo
#define callout     bsd29_callout
#define cdevsw      bsd29_cdevsw
#define chmod       bsd29_chmod
#define close       bsd29_close
#define closef      bsd29_closef
#define clrbuf      bsd29_clrbuf
#define copyout     bsd29_copyout
#define daddr_t     bsd29_daddr_t
#define dev_t       bsd29_dev_t
#define devtab      bsd29_devtab
#define dinode      bsd29_dinode
#define direct      bsd29_direct
#define falloc      bsd29_falloc
#define fblk        bsd29_fblk
#define file        bsd29_file
#define fileNFILE   bsd29_fileNFILE
#define filsys      bsd29_filsys
#define free        bsd29_free
#define fserr       bsd29_fserr
#define fubyte      bsd29_fubyte
#define getblk      bsd29_getblk
#define geteblk     bsd29_geteblk
#define geterror    bsd29_geterror
#define getf        bsd29_getf
#define getfs       bsd29_getfs
#define ialloc      bsd29_ialloc
#define iexpand     bsd29_iexpand
#define ifind       bsd29_ifind
#define ifree       bsd29_ifree
#define ifreelist   bsd29_ifreelist
#define iget        bsd29_iget
#define ihash       bsd29_ihash
#define ihinit      bsd29_ihinit
#define iinit       bsd29_iinit
#define incore      bsd29_incore
#define ino_t       bsd29_ino_t
#define inode       bsd29_inode
#define inodeNINODE bsd29_inodeNINODE
#define iodone      bsd29_iodone
#define iomove      bsd29_iomove
#define iowait      bsd29_iowait
#define iput        bsd29_iput
#define itrunc      bsd29_itrunc
#define iupdat      bsd29_iupdat
#define lastf       bsd29_lastf
#define link        bsd29_link
#define maknode     bsd29_maknode
#define mapin       bsd29_mapin
#define mapout      bsd29_mapout
#define mknod       bsd29_mknod
#define mount       bsd29_mount
#define mountNMOUNT bsd29_mountNMOUNT
#define namei       bsd29_namei
#define nblkdev     bsd29_nblkdev
#define nbuf        bsd29_nbuf
#define nchrdev     bsd29_nchrdev
#define nmount      bsd29_nmount
#define notavail    bsd29_notavail
#define off_t       bsd29_off_t
#define open1       bsd29_open1
#define openi       bsd29_openi
#define own         bsd29_own
#define owner       bsd29_owner
#define panic       bsd29_panic
#define plock       bsd29_plock
#define prele       bsd29_prele
#define printf      bsd29_printf
#define rablock     bsd29_rablock
#define rdwr        bsd29_rdwr
#define readi       bsd29_readi
#define readlink    bsd29_readlink
#define readp       bsd29_readp
#define rootdev     bsd29_rootdev
#define rootdir     bsd29_rootdir
#define schar       bsd29_schar
#define SEG5        bsd29_SEG5
#define segm        bsd29_segm
#define size_t      bsd29_size_t
#define sleep       bsd29_sleep
#define spl0        bsd29_spl0
#define spl6        bsd29_spl6
#define _spl0       bsd29_spl0
#define _spl6       bsd29_spl6
#define stat        bsd29_stat
#define stat1       bsd29_stat1
#define suser       bsd29_suser
#define swbuf       bsd29_swbuf
#define symchar     bsd29_symchar
#define symlink     bsd29_symlink
#define tablefull   bsd29_tablefull
#define time        bsd29_time
#define time_t      bsd29_time_t
#define tloop       bsd29_tloop
#define u           bsd29_u
#define uchar       bsd29_uchar
#define ufalloc     bsd29_ufalloc
#define unlink      bsd29_unlink
#define update      bsd29_update
#define updlock     bsd29_updlock
#define uprintf     bsd29_uprintf
#define user        bsd29_user
#define wakeup      bsd29_wakeup
#define wdir        bsd29_wdir
#define writei      bsd29_writei
#define writep      bsd29_writep
#define xrele       bsd29_xrele

#endif /* __BSD29ADAPT_H__ */
