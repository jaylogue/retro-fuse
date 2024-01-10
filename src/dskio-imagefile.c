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
 * @file  Disk I/O Layer: Internal functions for accessing imagefile containers
 */

#include "dskio-internal.h"

int dsk_opencontainer_imagefile(const char *filename, const struct dsk_config *cfg)
{
    struct stat statbuf;
    int oflags = dsk_state.ro ? O_RDONLY : O_RDWR;

    /* only allow regular files */
    if (stat(filename, &statbuf) == 0 && !S_ISREG(statbuf.st_mode))
        return -EPERM;

    /* open the image file. fail if an error occurs. */
    dsk_state.fd = open(filename, oflags, 0666);
    if (dsk_state.fd < 0) {
        return -errno;
    }

    /* setup offset into imagefile for start of disk image. */
    dsk_state.offset =  (cfg->offset >= 0) ? cfg->offset : 0;

    return 0;
}

int dsk_createcontainer_imagefile(const char *filename, const struct dsk_config *cfg)
{
    /* open the image file, creating or truncating it in the process */
    dsk_state.fd = open(filename, O_CREAT|O_TRUNC|O_RDWR, 0666);
    if (dsk_state.fd < 0)
        return -errno;

    /* if the size of the disk is known and is not "unlimited" extend the file to the
     * size of the disk. note that on filesystems that support it, this will create a
     * sparse file that does not actually consume any space on the host disk. */
    if (dsk_state.geometry.size >= 0 && dsk_state.geometry.size != INT64_MAX) {
        if (ftruncate(dsk_state.fd, dsk_state.geometry.size) != 0) {
            int res = -errno;
            close(dsk_state.fd);
            dsk_state.fd = -1;
            unlink(filename);
            return res;
        }
    }

    return 0;
}
