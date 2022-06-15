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

#include <stdlib.h>
#include <stdint.h>
#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#if defined(__linux__)
#include <linux/fs.h>
#endif
#if defined(__APPLE__)
#include <sys/disk.h>
#endif

#include "dskio.h"

static int dsk_fd = -1;
static off_t dsk_size = -1;     /* size of disk, in 512-byte blocks */
static off_t dsk_start = -1;    /* logical start of disk, in bytes */
static off_t dsk_end = -1;      /* logical end of disk, in bytes */
static int dsk_isblkdev = 0;    /* underlying storage is block device */

/** Open/create a virtual disk backed by a block device or image file.
 * 
 * @param[in]   filename    The name of the device or image file to be 
 *                          opened.
 * 
 * @param[in]   size        Size of the virtual disk, in 512-byte blocks.
 *                          Access to the underlying device/file will be
 *                          limited to blocks within the range 0 to size.
 *                          If size <= 0, and the underlying storage is a
 *                          block device, size is inferred from the size
 *                          of the block device.
 * 
 * @param[in]   offset      Offset (in 512-byte blocks) into the underlying
 *                          device/image file at which the virtual disk
 *                          starts. This value is added to the blkno argument
 *                          to dsk_read()/dsk_write() to determine the actual
 *                          read/write position.
 * 
 * @param[in]   create      If != 0, create a new disk image file. If
 *                          specified, the target file must *not* exist, unless
 *                          overwrite != 0.
 *                          Ignored if filename specifies a block device.
 * 
 * @param[in]   overwrite   If != 0, overwrite an existing disk image file, if
 *                          such a file exists.
 *                          Ignored if create == 0 or filename specifies a block
 *                          device.
 * 
 * @param[in]   ro          If != 0, open the device/image file in read-only
 *                          mode.  Ignored if create != 0.
 */
int dsk_open(const char *filename, off_t size, off_t offset, int create, int overwrite, int ro)
{
    struct stat statbuf;
    int oflags = (ro && !create) ? O_RDONLY : O_RDWR;

    if (stat(filename, &statbuf) == 0) {
        dsk_isblkdev = S_ISBLK(statbuf.st_mode);
        
        /* only allow block devices and regular files */
        if (!S_ISREG(statbuf.st_mode) && !dsk_isblkdev)
            return -ENOTBLK;
        
        /* image file must not exist when creating, unless overwrite != 0 */
        if (!dsk_isblkdev && create) {
            if (!overwrite)
                return -EEXIST;
            oflags |= O_TRUNC;
        }
    }

    else {
        /* fail if there's an error accessing the file, or if the file doesn't
         * exist and we're not creating. */
        if (errno != ENOENT || !create)
            return -errno;

        dsk_isblkdev = 0;
        oflags |= O_CREAT|O_EXCL;
    }

    /* must specify disk size when creating a disk image file. */
    if (!dsk_isblkdev && create && size <= 0)
        return -EINVAL;

    dsk_fd = open(filename, oflags, 0666);
    if (dsk_fd < 0)
        return -errno;

    if (dsk_isblkdev && size <= 0) {
#if defined(__linux__)
        uint64_t devsize;
        if (ioctl(dsk_fd, BLKGETSIZE64, &devsize) >= 0) {
            size = (devsize / DSK_BLKSIZE) - offset;
        }
#elif defined(__APPLE__)
		uint32_t blocksize;
        uint64_t blockcount;
		if (ioctl(dsk_fd, DKIOCGETBLOCKSIZE, &blocksize) >= 0 &&
            ioctl(dsk_fd, DKIOCGETBLOCKCOUNT, &blockcount) >= 0) {
            size = ((blockcount * blocksize) / DSK_BLKSIZE) - offset;
		}
#endif
    }

    dsk_start = offset * DSK_BLKSIZE;
    dsk_setsize(size);

    /* if creating an image file, enlarge it to the size of the
       virtual disk */
    if (!dsk_isblkdev && create) {
        if (ftruncate(dsk_fd, dsk_end) != 0) {
            int res = -errno;
            dsk_close();
            unlink(filename);
            return res;
        }
    }

    return 0;
}

/** Close the virtual disk.
 */
int dsk_close()
{
    if (dsk_fd >= 0) {
        close(dsk_fd);
        dsk_fd = -1;
    }
    return 0;
}

/** Read data from the virtual disk.
 */
int dsk_read(off_t blkno, void * buf, int count) 
{
    off_t blkoff = dsk_start + blkno * DSK_BLKSIZE;
    if (count < 0 || blkoff < dsk_start || (blkoff + count) > dsk_end)
        return 0;
    ssize_t res = pread(dsk_fd, buf, count, blkoff);
    if (res < 0)
        return -errno;
    return (res == count) ? 0 : -EIO;
 }

/** Write data to the virtual disk.
 */
int dsk_write(off_t blkno, void * buf, int count) 
{
    off_t blkoff = dsk_start + blkno * DSK_BLKSIZE;
    if (count < 0 || blkoff < dsk_start || (blkoff + count) > dsk_end)
        return 0;
    ssize_t res = pwrite(dsk_fd, buf, count, blkoff);
    if (res < 0)
        return -errno;
    return (res == count) ? 0 : -EIO;
}

/** Flush any outstanding writes to the underlying device/image file.
 */
int dsk_flush()
{
    if (fsync(dsk_fd) != 0)
        return -errno;
    return 0;
}

/** Return the logical size of the virtual disk, in 512-byte blocks.
 * 
 * A value of 0 is returned if the size is unknown.
 */
off_t dsk_getsize()
{
    return dsk_size;
}

/** Set the logical size of the virtual disk, in 512-byte blocks.
 * 
 * A value of 0 indicates the size is unknown.
 */
void dsk_setsize(off_t size)
{
    dsk_size = size;
    if (size > 0)
        dsk_end = dsk_start + (size * DSK_BLKSIZE);
    else
        dsk_end = LLONG_MAX;
}