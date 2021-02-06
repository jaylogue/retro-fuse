#ifndef __DSKIO_H__
#define __DSKIO_H__

extern int dsk_open(const char * fn);
extern int dsk_close();
extern int dsk_read(int blkno, void * buf, int byteCount);
extern int dsk_write(int blkno, void * buf, int byteCount);

#endif /* __DSKIO_H__ */