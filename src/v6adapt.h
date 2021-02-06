#ifndef __V6_ADAPT_H__
#define __V6_ADAPT_H__

#include "stdint.h"


/* ===== Public functions accessable to modern code ===== */

extern void v6_init();


#ifdef ANCIENT_SRC

/* map v6 function and global variable names to avoid conflicts with modern code */

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
#define readi     v6_readi
#define writei    v6_writei
#define max       v6_max
#define min       v6_min
#define iomove    v6_iomove
#define namei     v6_namei
#define schar     v6_schar
#define getf      v6_getf
#define closef    v6_closef
#define closei    v6_closei
#define openi     v6_openi
#define access    v6_access
#define ufalloc   v6_ufalloc
#define falloc    v6_falloc

#define u         v6_u
#define buf       v6_buf
#define bfreelist v6_bfreelist
#define buffers   v6_buffers
#define swbuf     v6_swbuf
#define tmtab     v6_tmtab
#define httab     v6_httab
#define bdevsw    v6_bdevsw
#define cdevsw    v6_cdevsw
#define nblkdev   v6_nblkdev
#define nchrdev   v6_nchrdev
#define mount     v6_mount
#define inode     v6_inode
#define file      v6_file
#define rootdev   v6_rootdev
#define rootdir   v6_rootdir
#define time      v6_time
#define updlock   v6_updlock
#define rablock   v6_rablock

/* forward declare struct names */

struct buf;
struct filsys;
struct inode;
struct file;

/* forward declare v6 functions */

/* bio.h */
extern struct buf * bread(int16_t dev, int16_t blkno);
extern struct buf * breada(int16_t adev, int16_t blkno, int16_t rablkno);
extern void bwrite(struct buf *bp);
extern void bdwrite(struct buf *bp);
extern void bawrite(struct buf *bp);
extern void brelse(struct buf *bp);
extern int16_t incore(int16_t adev, int16_t blkno);
extern struct buf * getblk(int16_t dev, int16_t blkno);
extern void iowait(struct buf *bp);
extern void notavail(struct buf *bp);
extern void iodone(struct buf *bp);
extern void clrbuf(struct buf *bp);
extern void binit();
extern void mapfree(struct buf *bp);
extern void bflush(int16_t dev);
extern void geterror(struct buf *abp);

/* slp.c */
extern void sleep(void * chan, int16_t pri);
extern void wakeup(void * chan);

/* prf.c */
extern void printf(const char * str, ...);
extern void panic(const char * s);
extern void prdev(const char * str, int16_t dev);

/* subr.c */
extern int16_t bmap(struct inode *ip, int16_t bn);
extern void bcopy(void * from, void * to, int16_t count);

/* alloc.c */
extern void iinit();
extern struct buf * alloc(int16_t dev);
extern void free(int16_t dev, int16_t bno);
extern int16_t badblock(struct filsys *afp, int16_t abn, int16_t dev);
extern struct inode * ialloc(int16_t dev);
extern void ifree(int16_t dev, int16_t ino);
extern struct filsys * getfs(int16_t dev);
extern void update();

/* iget.c */
extern struct inode * iget(int16_t dev, int16_t ino);
extern void iput(struct inode *p);
extern void iupdat(struct inode *p, int16_t *tm);
extern void itrunc(struct inode *ip);
extern struct inode * maknode(int16_t mode);
extern void wdir(struct inode *ip);

/* pipe.c */
extern void prele(struct inode *ip);

/* m40.s */
extern int16_t ldiv(uint16_t n, uint16_t b);
extern int16_t lrem(uint16_t n, uint16_t b);
extern int16_t lshift(uint16_t *n, int16_t s);
extern void dpadd(uint16_t *n, int16_t a);
extern int16_t dpcmp(uint16_t al, uint16_t ah, uint16_t bl, uint16_t bh);
extern void spl0();
extern void spl6();

/* rdwri.c */
extern void readi(struct inode *aip);
extern void writei(struct inode *aip);
extern int16_t max(uint16_t a, uint16_t b);
extern int16_t min(uint16_t a, uint16_t b);
extern void iomove(struct buf *bp, int16_t o, int16_t an, int16_t flag);

/* nami.c */
extern struct inode * namei(int16_t (*func)(), int16_t flag);
extern int16_t schar();

/* fio.c */
extern struct file * getf(int16_t f);
extern void closef(struct file *fp);
extern void closei(struct inode *ip, int16_t rw);
extern void openi(struct inode *ip, int16_t rw);
extern int16_t access(struct inode *aip, int16_t mode);
extern int16_t ufalloc();
extern struct file * falloc();

/* alias a 16-bit device id integer as a struct containing device manjor and minor numbers. */
#define todevst(DEV) (*((struct devst *)&(DEV)))

/* replacement for v6 PS (processor status word) define. */
extern struct integ v6_PS;
#define PS (&v6_PS)

/* allow v6 code to redefine NULL without warning */
#undef NULL


#endif /* ANCIENT_SRC */

#endif /* __V6_ADAPT_H__ */
