#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "dskio.h"

int dskFD = -1;
off_t dskSize = -1;

#define BLKSIZE 512

int dsk_open(const char *fn)
{
    dskFD = open(fn, O_RDWR);
    if (dskFD < 0)
        return 0;
    dskSize = lseek(dskFD, 0, SEEK_END);
    if (dskSize < 0)
    {
        dsk_close();
        return 0;
    }
    return 1;
}

int dsk_close()
{
    if (dskFD != -1)
    {
        close(dskFD);
        dskFD = -1;
    }
    return 1;
}

int dsk_read(int blkno, void * buf, int byteCount) 
{
    off_t blkoff = blkno * BLKSIZE;
    if (blkoff < 0 || byteCount < 0 || (blkoff + byteCount) > dskSize)
        return 0;
    ssize_t res = pread(dskFD, buf, byteCount, blkoff);
    return (res == byteCount);
}

int dsk_write(int blkno, void * buf, int byteCount) 
{
    off_t blkoff = blkno * BLKSIZE;
    if (blkoff < 0 || byteCount < 0 || (blkoff + byteCount) > dskSize)
        return 0;
    ssize_t res = pwrite(dskFD, buf, byteCount, blkoff);
    return (res == byteCount);
}

