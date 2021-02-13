#ifndef __DSKIO_H__
#define __DSKIO_H__

extern int dsk_open(const char *filename, off_t size, off_t offset, int ro);
extern int dsk_close();
extern int dsk_read(int blkno, void * buf, int byteCount);
extern int dsk_write(int blkno, void * buf, int byteCount);

#endif /* __DSKIO_H__ */