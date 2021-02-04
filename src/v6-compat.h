#ifndef __V6_COMPAT_H__
#define __V6_COMPAT_H__

#include "stdint.h"

struct buf;
struct filsys;
struct inode;
struct file;

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

extern void sleep(void * chan, int16_t pri);
extern void wakeup(void * chan);

/* prf.c */
extern void printf(const char * str, ...);
extern void panic(const char * s);
extern void prdev(const char * str, int16_t dev);

/* subr.c */
extern int16_t bmap(struct inode *ip, int16_t bn);
extern int16_t passc(char c);
extern char cpass();
extern void nodev();
extern void nulldev();
extern void bcopy(void * from, void * to, int16_t count);

/* alloc.c */
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


inline int16_t subyte(char * p, char c) { *p = c; return 0; }
inline char fubyte(char * p) { return *p; }

inline void spl0() { }
inline void spl6() { }
inline void mapfree(struct buf * bp) { }

inline void prele(struct inode *ip) { }

inline int16_t ldiv(uint16_t n, uint16_t b) { return (int16_t)(n / b); }
inline int16_t lrem(uint16_t n, uint16_t b) { return (int16_t)(n % b); }
inline int16_t lshift(uint16_t *n, int16_t s)
{
    uint32_t n32 = ((uint32_t)n[1]) << 16 | n[0];
    n32 = (s >= 0) ? n32 << s : n32 >> -s;
    return (int16_t)n32;
}
inline void dpadd(uint16_t *n, int16_t a)
{
    uint32_t n32 = ((uint32_t)n[1]) << 16 | n[0];
    n32 += a;
    n[0] = (uint16_t)n32;
    n[1] = (uint16_t)(n32 >> 16);
}
inline int16_t dpcmp(uint16_t al, uint16_t ah, uint16_t bl, uint16_t bh)
{
    uint32_t a = ((uint32_t)ah) << 16 | al;
    uint32_t b = ((uint32_t)bh) << 16 | bl;
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

#define todevst(DEV) (*((struct devst *)&(DEV)))

extern struct integ _PS;
#define PS (&_PS)

#endif /* __V6_COMPAT_H__ */