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

#ifndef __V7ADAPT_H__
#define __V7ADAPT_H__

#include "stdint.h"
#include "fstype.h"

/** Information about the type of v7-based filesystem being accessed.
 */
struct v7_fsconfig {
    int fstype;                     /* Filesystem type (values from fs_type enum) */
    int byteorder;                  /* Byte ordering on disk (values from fs_byteorder enum) */
    int16_t blocksize;              /* Filesystem block size */
    int16_t nicfree;                /* Size of the free block list table in the superblock */
    int16_t nicinod;                /* Size of the free i-node list table in the superblock */
};

extern struct v7_fsconfig v7_fsconfig;

/* Forward declarations of v7 types, suitably name-mangled */

struct v7_buf;
struct v7_filsys;
struct v7_inode;
struct v7_dinode;
struct v7_file;
struct v7_stat;
struct v7_devtab;
struct v7_direct;
struct v7_fblk;
struct v7_mount;

typedef int32_t v7_daddr_t;
typedef int16_t v7_dev_t;
typedef char * v7_caddr_t;
typedef uint16_t v7_ino_t;
typedef int32_t v7_time_t;
typedef int32_t v7_off_t;

/* Utility functions */

extern void v7_zerocore();
extern void v7_decodesuperblock(v7_caddr_t srcbuf, struct v7_filsys * dest);
extern void v7_encodesuperblock(struct v7_filsys * src, v7_caddr_t destbuf);
extern int16_t v7_htofs_i16(int16_t v);
extern uint16_t v7_htofs_u16(uint16_t v);
extern int32_t v7_htofs_i32(int32_t v);
extern void v7_refreshclock();

/* Forward declarations of name-mangled v7 functions */

/* main.c */
extern void v7_iinit();
extern void v7_binit();

/* machdep.c */
extern void v7_mapfree(struct v7_buf * bp);

/* bio.c */
extern struct v7_buf * v7_bread(v7_dev_t dev, v7_daddr_t blkno);
extern struct v7_buf * v7_breada(v7_dev_t dev, v7_daddr_t blkno, v7_daddr_t rablkno);
extern void v7_bwrite(struct v7_buf *bp);
extern void v7_bdwrite(struct v7_buf *bp);
extern void v7_bawrite(struct v7_buf *bp);
extern void v7_brelse(struct v7_buf *bp);
extern int16_t v7_incore(v7_dev_t dev, v7_daddr_t blkno);
extern struct v7_buf * v7_getblk(v7_dev_t dev, v7_daddr_t blkno);
extern struct v7_buf * v7_geteblk();
extern void v7_iowait(struct v7_buf *bp);
extern void v7_notavail(struct v7_buf *bp);
extern void v7_iodone(struct v7_buf *bp);
extern void v7_clrbuf(struct v7_buf *bp);
extern void v7_bflush(int16_t dev);
extern void v7_geterror(struct v7_buf *abp);

/* slp.c */
extern void v7_sleep(v7_caddr_t chan, int16_t pri);
extern void v7_wakeup(v7_caddr_t chan);

/* prf.c */
extern void v7_printf(const char * str, ...);
extern void v7_panic(const char * s);
extern void v7_prdev(const char * str, int16_t dev);

/* subr.c */
extern v7_daddr_t	v7_bmap(struct v7_inode *ip, v7_daddr_t bn, int16_t rwflg);
extern void v7_bcopy(v7_caddr_t from, v7_caddr_t to, int16_t count);

/* alloc.c */
extern struct v7_buf * v7_alloc(v7_dev_t dev);
extern void v7_free(v7_dev_t dev, v7_daddr_t bno);
extern int16_t v7_badblock(struct v7_filsys *fp, v7_daddr_t bn, v7_dev_t dev);
extern struct v7_inode * v7_ialloc(v7_dev_t dev);
extern void v7_ifree(v7_dev_t dev, v7_ino_t ino);
extern struct v7_filsys * v7_getfs(v7_dev_t dev);
extern void v7_update();

/* iget.c */
extern struct v7_inode * v7_iget(v7_dev_t dev, v7_ino_t ino);
extern void v7_iexpand(struct v7_inode *ip, struct v7_dinode *dp);
extern void v7_iput(struct v7_inode *ip);
extern void v7_iupdat(struct v7_inode *p, v7_time_t *ta, v7_time_t *tm);
extern void v7_itrunc(struct v7_inode *ip);
extern void v7_tloop(v7_dev_t dev, v7_daddr_t bn, int16_t f1, int16_t f2);
extern struct v7_inode * v7_maknode(int16_t mode);
extern void v7_wdir(struct v7_inode *ip);

/* pipe.c */
extern void v7_prele(struct v7_inode *ip);
extern void v7_readp(struct v7_file *fp);
extern void v7_writep(struct v7_file *fp);
extern void v7_plock(struct v7_inode *ip);

/* mch.s */
extern void v7_spl0();
extern int16_t v7_spl6();
extern void v7_splx(int16_t s);
extern void v7_suword(uint16_t *n, int16_t a);
extern int16_t v7_copyout(v7_caddr_t s, v7_caddr_t d, int16_t n);

/* rdwri.c */
extern void v7_readi(struct v7_inode *aip);
extern void v7_writei(struct v7_inode *aip);
extern int16_t v7_max(uint16_t a, uint16_t b);
extern int16_t v7_min(uint16_t a, uint16_t b);
extern void v7_iomove(v7_caddr_t cp, int16_t n, int16_t flag);

/* nami.c */
extern struct v7_inode * v7_namei(int16_t (*func)(), int16_t flag);
extern int16_t v7_schar();
extern int16_t v7_uchar();

/* fio.c */
extern struct v7_file * v7_getf(int16_t f);
extern void v7_closef(struct v7_file *fp);
extern void v7_closei(struct v7_inode *ip, int16_t rw);
extern void v7_openi(struct v7_inode *ip, int16_t rw);
extern int16_t v7_access(struct v7_inode *aip, int16_t mode);
extern struct v7_inode * v7_owner();
extern int16_t v7_suser();
extern int16_t v7_ufalloc();
extern struct v7_file * v7_falloc();

/* sys2.c */
extern void v7_rdwr(int16_t mode);
extern void v7_open1(struct v7_inode *ip, int16_t mode, int16_t trf);
extern void v7_close();
extern void v7_link();
extern void v7_mknod();

/* sys3.c */
extern void v7_stat1(struct v7_inode *ip, struct v7_stat *ub, v7_off_t pipeadj);

/* sys4.c */
extern void v7_unlink();
extern void v7_chmod();
extern void v7_chown();

/* text.c */
extern void v7_xrele(struct v7_inode *ip);

/* Allow v7 code to use certain names that collide with modern
 * preprocessor definitions, while still allowing the modern
 * definitions to be recovered if need be.
 */
#pragma push_macro("NULL")
#undef NULL
#pragma push_macro("NSIG")
#undef NSIG
#pragma push_macro("SIGIOT")
#undef SIGIOT
#pragma push_macro("SIGABRT")
#undef SIGABRT
#pragma push_macro("EAGAIN")
#undef EAGAIN
#pragma push_macro("FREAD")
#undef FREAD
#pragma push_macro("FWRITE")
#undef FWRITE
#pragma push_macro("major")
#undef major
#pragma push_macro("minor")
#undef minor
#pragma push_macro("makedev")
#undef makedev
#pragma push_macro("st_atime")
#undef st_atime
#define st_atime v7_st_atime
#pragma push_macro("st_mtime")
#undef st_mtime
#define st_mtime v7_st_mtime
#pragma push_macro("st_ctime")
#undef st_ctime
#define st_ctime v7_st_ctime
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

/* Map various names used by v7 code to avoid any conflicts with modern code
 */
#define access    v7_access
#define alloc     v7_alloc
#define badblock  v7_badblock
#define bawrite   v7_bawrite
#define bcopy     v7_bcopy
#define bdevsw    v7_bdevsw
#define bdwrite   v7_bdwrite
#define bflush    v7_bflush
#define bfreelist v7_bfreelist
#define binit     v7_binit
#define bmap      v7_bmap
#define bread     v7_bread
#define breada    v7_breada
#define brelse    v7_brelse
#define buf       v7_buf
#define buffers   v7_buffers
#define bwrite    v7_bwrite
#define caddr_t   v7_caddr_t
#define callo     v7_callo
#define callout   v7_callout
#define cdevsw    v7_cdevsw
#define chmod     v7_chmod
#define chown     v7_chown
#define close     v7_close
#define closef    v7_closef
#define clrbuf    v7_clrbuf
#define copyout   v7_copyout
#define daddr_t   v7_daddr_t
#define dev_t     v7_dev_t
#define devtab    v7_devtab
#define dinode    v7_dinode
#define direct    v7_direct
#define falloc    v7_falloc
#define fblk      v7_fblk
#define file      v7_file
#define filsys    v7_filsys
#define free      v7_free
#define getblk    v7_getblk
#define geteblk   v7_geteblk
#define geterror  v7_geterror
#define getf      v7_getf
#define getfs     v7_getfs
#define hilo      v7_hilo
#define httab     v7_httab
#define ialloc    v7_ialloc
#define iexpand   v7_iexpand
#define ifree     v7_ifree
#define iget      v7_iget
#define iinit     v7_iinit
#define incore    v7_incore
#define ino_t     v7_ino_t
#define inode     v7_inode
#define integ     v7_integ
#define iodone    v7_iodone
#define iomove    v7_iomove
#define iowait    v7_iowait
#define iput      v7_iput
#define itrunc    v7_itrunc
#define iupdat    v7_iupdat
#define link      v7_link
#define maknode   v7_maknode
#define mapfree   v7_mapfree
#define max       v7_max
#define min       v7_min
#define mknod     v7_mknod
#define mount     v7_mount
#define mpxip     v7_mpxip
#define namei     v7_namei
#define nblkdev   v7_nblkdev
#define nchrdev   v7_nchrdev
#define notavail  v7_notavail
#define off_t     v7_off_t
#define open1     v7_open1
#define openi     v7_openi
#define owner     v7_owner
#define panic     v7_panic
#define plock     v7_plock
#define prdev     v7_prdev
#define prele     v7_prele
#define printf    v7_printf
#define rablock   v7_rablock
#define rdwr      v7_rdwr
#define readi     v7_readi
#define readp     v7_readp
#define rootdev   v7_rootdev
#define rootdir   v7_rootdir
#define schar     v7_schar
#define sleep     v7_sleep
#define spl0      v7_spl0
#define spl6      v7_spl6
#define splx      v7_splx
#define stat      v7_stat
#define stat1     v7_stat1
#define suser     v7_suser
#define suword    v7_suword
#define swbuf     v7_swbuf
#define time      v7_time
#define time_t    v7_time_t
#define tloop     v7_tloop
#define tmtab     v7_tmtab
#define u         v7_u
#define uchar     v7_uchar
#define ufalloc   v7_ufalloc
#define unlink    v7_unlink
#define update    v7_update
#define updlock   v7_updlock
#define user      v7_user
#define wakeup    v7_wakeup
#define wdir      v7_wdir
#define writei    v7_writei
#define writep    v7_writep
#define xrele     v7_xrele

#endif /* __V7ADAPT_H__ */
