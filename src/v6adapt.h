#ifndef __V6ADAPT_H__
#define __V6ADAPT_H__

#include "stdint.h"

/* forward declarations of v6 struct types */

struct v6_buf;
struct v6_filsys;
struct v6_inode;
struct v6_file;


/* Structure returned by v6 stat/fstat system calls. */
struct v6_stat {
   int16_t dev;        /* +0: major/minor device of i-node */
   int16_t inumber;    /* +2: i number, 1-to-1 with device address */
   int16_t mode;       /* +4: mode and type */
   char nlinks;        /* +6: number of links to file */
   char uid;           /* +7: user ID of owner */
   char gid;           /* +8: group ID of owner */
   char size0;         /* +9: high byte of 24-bit size */
   int16_t size1;      /* +10: low word of 24-bit size */
   int16_t addr[8];    /* +12: block numbers or device number */
   int16_t actime[2];  /* +28: time of last access */
   int16_t modtime[2]; /* +32: time of last modification */
};

/* Structure of v6 directory entry */
struct v6_direntry {
   int16_t d_inode;
   char d_name[14];
};

extern void v6_init_kernel(int readonly);
extern void v6_refreshclock();

/* forward declarations of v6 functions */

/* bio.c */
extern struct v6_buf * v6_bread(int16_t dev, int16_t blkno);
extern struct v6_buf * v6_breada(int16_t adev, int16_t blkno, int16_t rablkno);
extern void v6_bwrite(struct v6_buf *bp);
extern void v6_bdwrite(struct v6_buf *bp);
extern void v6_bawrite(struct v6_buf *bp);
extern void v6_brelse(struct v6_buf *bp);
extern int16_t v6_incore(int16_t adev, int16_t blkno);
extern struct v6_buf * v6_getblk(int16_t dev, int16_t blkno);
extern void v6_iowait(struct v6_buf *bp);
extern void v6_notavail(struct v6_buf *bp);
extern void v6_iodone(struct v6_buf *bp);
extern void v6_clrbuf(struct v6_buf *bp);
extern void v6_binit();
extern void v6_mapfree(struct v6_buf *bp);
extern void v6_bflush(int16_t dev);
extern void v6_geterror(struct v6_buf *abp);

/* slp.c */
extern void v6_sleep(void * chan, int16_t pri);
extern void v6_wakeup(void * chan);

/* prf.c */
extern void v6_printf(const char * str, ...);
extern void v6_panic(const char * s);
extern void v6_prdev(const char * str, int16_t dev);

/* subr.c */
extern int16_t v6_bmap(struct v6_inode *ip, int16_t bn);
extern void v6_bcopy(void * from, void * to, int16_t count);

/* alloc.c */
extern void v6_iinit();
extern struct v6_buf * v6_alloc(int16_t dev);
extern void v6_free(int16_t dev, int16_t bno);
extern int16_t v6_badblock(struct v6_filsys *afp, int16_t abn, int16_t dev);
extern struct v6_inode * v6_ialloc(int16_t dev);
extern void v6_ifree(int16_t dev, int16_t ino);
extern struct v6_filsys * v6_getfs(int16_t dev);
extern void v6_update();

/* iget.c */
extern struct v6_inode * v6_iget(int16_t dev, int16_t ino);
extern void v6_iput(struct v6_inode *p);
extern void v6_iupdat(struct v6_inode *p, int16_t *tm);
extern void v6_itrunc(struct v6_inode *ip);
extern struct v6_inode * v6_maknode(int16_t mode);
extern void v6_wdir(struct v6_inode *ip);

/* pipe.c */
extern void v6_prele(struct v6_inode *ip);
extern void v6_readp(struct v6_file *fp);
extern void v6_writep(struct v6_file *fp);

/* m40.s */
extern int16_t v6_ldiv(uint16_t n, uint16_t b);
extern int16_t v6_lrem(uint16_t n, uint16_t b);
extern int16_t v6_lshift(uint16_t *n, int16_t s);
extern void v6_dpadd(uint16_t *n, int16_t a);
extern int16_t v6_dpcmp(uint16_t al, uint16_t ah, uint16_t bl, uint16_t bh);
extern void v6_spl0();
extern void v6_spl6();
extern void v6_suword(uint16_t *n, int16_t a);

/* rdwri.c */
extern void v6_readi(struct v6_inode *aip);
extern void v6_writei(struct v6_inode *aip);
extern int16_t v6_max(uint16_t a, uint16_t b);
extern int16_t v6_min(uint16_t a, uint16_t b);
extern void v6_iomove(struct v6_buf *bp, int16_t o, int16_t an, int16_t flag);

/* nami.c */
extern struct v6_inode * v6_namei(int16_t (*func)(), int16_t flag);
extern int16_t v6_schar();
extern int16_t v6_uchar();

/* fio.c */
extern struct v6_file * v6_getf(int16_t f);
extern void v6_closef(struct v6_file *fp);
extern void v6_closei(struct v6_inode *ip, int16_t rw);
extern void v6_openi(struct v6_inode *ip, int16_t rw);
extern int16_t v6_access(struct v6_inode *aip, int16_t mode);
extern struct v6_inode * v6_owner();
extern int16_t v6_suser();
extern int16_t v6_ufalloc();
extern struct v6_file * v6_falloc();

/* sys2.c */
extern void v6_rdwr(int16_t mode);
extern void v6_open1(struct v6_inode *ip, int16_t mode, int16_t trf);
extern void v6_close();
extern void v6_link();
extern void v6_mknod();

/* sys3.c */
extern void v6_stat1(struct v6_inode *ip, void *ub);

/* sys4.c */
extern void v6_unlink();
extern void v6_chmod();
extern void v6_chown();


/* map struct and global variable names to avoid conflicts with modern code */

#define bdevsw    v6_bdevsw
#define bfreelist v6_bfreelist
#define buf       v6_buf
#define buffers   v6_buffers
#define callo     v6_callo
#define callout   v6_callout
#define cdevsw    v6_cdevsw
#define devtab    v6_devtab
#define file      v6_file
#define file      v6_file
#define filsys    v6_filsys
#define hilo      v6_hilo
#define httab     v6_httab
#define inode     v6_inode
#define integ     v6_integ
#define mount     v6_mount
#define nblkdev   v6_nblkdev
#define nchrdev   v6_nchrdev
#define rablock   v6_rablock
#define rootdev   v6_rootdev
#define rootdir   v6_rootdir
#define swbuf     v6_swbuf
#define time      v6_time
#define tmtab     v6_tmtab
#define u         v6_u
#define updlock   v6_updlock
#define user      v6_user


/* map function names to avoid conflicts with modern code, 
   but only when compiling v6 source. */

#ifdef ANCIENT_SRC

#define bread     v6_bread
#define breada    v6_breada
#define bwrite    v6_bwrite
#define bdwrite   v6_bdwrite
#define bawrite   v6_bawrite
#define brelse    v6_brelse
#define incore    v6_incore
#define getblk    v6_getblk
#define iowait    v6_iowait
#define notavail  v6_notavail
#define iodone    v6_iodone
#define clrbuf    v6_clrbuf
#define binit     v6_binit
#define mapfree   v6_mapfree
#define bflush    v6_bflush
#define geterror  v6_geterror
#define sleep     v6_sleep
#define wakeup    v6_wakeup
#define printf    v6_printf
#define panic     v6_panic
#define prdev     v6_prdev
#define bmap      v6_bmap
#define bcopy     v6_bcopy
#define iinit     v6_iinit
#define alloc     v6_alloc
#define free      v6_free
#define badblock  v6_badblock
#define ialloc    v6_ialloc
#define ifree     v6_ifree
#define getfs     v6_getfs
#define update    v6_update
#define iget      v6_iget
#define iput      v6_iput
#define iupdat    v6_iupdat
#define itrunc    v6_itrunc
#define maknode   v6_maknode
#define wdir      v6_wdir
#define prele     v6_prele
#define ldiv      v6_ldiv
#define lrem      v6_lrem
#define lshift    v6_lshift
#define dpadd     v6_dpadd
#define dpcmp     v6_dpcmp
#define spl0      v6_spl0
#define spl6      v6_spl6
#define suword    v6_suword
#define readi     v6_readi
#define writei    v6_writei
#define max       v6_max
#define min       v6_min
#define iomove    v6_iomove
#define namei     v6_namei
#define schar     v6_schar
#define uchar     v6_uchar
#define getf      v6_getf
#define closef    v6_closef
#define closei    v6_closei
#define openi     v6_openi
#define access    v6_access
#define owner     v6_owner
#define suser     v6_suser
#define ufalloc   v6_ufalloc
#define falloc    v6_falloc
#define rdwr      v6_rdwr
#define open1     v6_open1
#define close     v6_close
#define link      v6_link
#define mknod     v6_mknod
#define stat1     v6_stat1
#define readp     v6_readp
#define writep    v6_writep
#define unlink    v6_unlink
#define chmod     v6_chmod
#define chown     v6_chown


#endif /* ANCIENT_SRC */

/* alias a 16-bit device id integer to a struct containing device major and minor numbers. 
   this replaces the archaic 'int dev; ...  dev.d_major' idiom. */
#define todevst(DEV) (*((struct devst *)&(DEV)))

/* replacement for v6 PS (processor status word) define. */
extern struct v6_integ v6_PS;
#define PS (&v6_PS)

/* allow v6 code to redefine NULL without warning */
#undef NULL

#endif /* __V6ADAPT_H__ */
