#ifndef __V6_COMPAT_H__
#define __V6_COMPAT_H__

#include "stdint.h"

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

extern void panic(const char * s);

inline void spl0() { }
inline void spl6() { }
inline void mapfree(struct buf * bp) { }

#define todevst(DEV) (*((struct devst *)&(DEV)))

#endif /* __V6_COMPAT_H__ */
