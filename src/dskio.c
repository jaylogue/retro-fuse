#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "dskio.h"

int dskfd = -1;
off_t dsksize = -1;
off_t dskoffset = -1;
int readonly = 0;

#define BLKSIZE 512

int dsk_open(const char *filename, off_t size, off_t offset, int ro)
{
    dskfd = open(filename, (ro) ? O_RDONLY : O_RDWR);
    if (dskfd < 0)
        return 0;
    dsksize = size * BLKSIZE;
    if (dsksize <= 0) {
        dsksize = lseek(dskfd, 0, SEEK_END);
        if (dsksize < 0)
        {
            dsk_close();
            return 0;
        }
    }
    dskoffset = offset * BLKSIZE;
    return 1;
}

int dsk_close()
{
    if (dskfd >= 0)
    {
        close(dskfd);
        dskfd = -1;
    }
    return 1;
}

int dsk_read(int blkno, void * buf, int count) 
{
    off_t blkoff = blkno * BLKSIZE;
    if (blkoff < 0 || count < 0 || (blkoff + count) > dsksize)
        return 0;
    blkoff += dskoffset;
    ssize_t res = pread(dskfd, buf, count, blkoff);
    return (res == count);
}

int dsk_write(int blkno, void * buf, int count) 
{
    off_t blkoff = blkno * BLKSIZE;
    if (blkoff < 0 || count < 0 || (blkoff + count) > dsksize)
        return 0;
    blkoff += dskoffset;
    ssize_t res = pwrite(dskfd, buf, count, blkoff);
    return (res == count);
}

