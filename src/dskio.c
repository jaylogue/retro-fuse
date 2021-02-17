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
 * @file  Functions for accessing the underlying device/image file.
 */

#define _XOPEN_SOURCE 500

#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#include "dskio.h"

#define BLKSIZE 512

static int dskfd = -1;
static off_t dsksize = -1;
static off_t dskoffset = -1;

/** Open a filesystem device/image file.
 * 
 * @param[in]   filename    The name of the device or image file to be 
 *                          opened.
 * 
 * @param[in]   size        Size (in 512-byte blocks) of the filesystem.
 *                          Access to the underlying file will be limited
 *                          to blocks within this range.
 * 
 * @param[in]   offset      Offset (in blocks) into the device/image file
 *                          at which the filesystem starts.  This value is
 *                          added to the blkno arguemtn to dsk_read()/dsk_write().
 * 
 * @param[in]   ro          If != 0, open the device/file in read-only mode.
 */
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

/** Close the filesystem device/image file.
 */
int dsk_close()
{
    if (dskfd >= 0)
    {
        close(dskfd);
        dskfd = -1;
    }
    return 1;
}

/** Read data from the filesystem device/image file.
 */
int dsk_read(int blkno, void * buf, int count) 
{
    off_t blkoff = blkno * BLKSIZE;
    if (blkoff < 0 || count < 0 || (blkoff + count) > dsksize)
        return 0;
    blkoff += dskoffset;
    ssize_t res = pread(dskfd, buf, count, blkoff);
    return (res == count);
}

/** Write data from the filesystem device/image file.
 */
int dsk_write(int blkno, void * buf, int count) 
{
    off_t blkoff = blkno * BLKSIZE;
    if (blkoff < 0 || count < 0 || (blkoff + count) > dsksize)
        return 0;
    blkoff += dskoffset;
    ssize_t res = pwrite(dskfd, buf, count, blkoff);
    return (res == count);
}

