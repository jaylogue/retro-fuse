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
 * @file  Disk I/O Layer: Internal functions for accessing block device containers
 */

#include "dskio-internal.h"

#if defined(__linux__)
#include <linux/fs.h>
#endif
#if defined(__APPLE__)
#include <sys/disk.h>
#endif

int dsk_opencontainer_dev(const char *filename, const struct dsk_config *cfg)
{
    struct stat statbuf;
    int oflags = dsk_state.ro ? O_RDONLY : O_RDWR;

    /* fail if the file isn't a block device */
    if (stat(filename, &statbuf) == 0 && !S_ISBLK(statbuf.st_mode)) {
        return -ENOTBLK;
    }

    /* fail if the given disk offset is anything other than zero */
    if (cfg->offset > 0) {
        return -EINVAL; // TODO: ERROR: Disk offset for block device must be 0
    }

    // open the device file. fail if an error occurs.
    dsk_state.fd = open(filename, oflags, 0666);
    if (dsk_state.fd < 0)
        return -errno;

    return 0;
}

int dsk_getgeometry_dev()
{
    uint64_t blkdevsize;

    /* fetch the size of the block device from the kernel */
#if defined(__linux__)
    if (ioctl(dsk_state.fd, BLKGETSIZE64, &blkdevsize) < 0) {
        return -errno;
    }
    blkdevsize /= DSK_BLKSIZE;
#elif defined(__APPLE__)
    uint32_t blksize;
    uint64_t blkcount;
    if (ioctl(dsk_fd, DKIOCGETblksize, &blksize) < 0 ||
        ioctl(dsk_fd, DKIOCGETblkcount, &blkdevsize) < 0) {
        return -errno;
    }
    blkdevsize = (blkdevsize * blksize) / DSK_BLKSIZE;
#endif

    dsk_state.geometry.size = (off_t)blkdevsize;

    return 0;
}
