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

#ifndef __DSKIO_H__
#define __DSKIO_H__

enum {
    DSK_BLKSIZE = 512
};

extern int dsk_open(const char *filename, off_t size, off_t offset, int create, int overwrite, int ro);
extern int dsk_close();
extern int dsk_read(off_t blkno, void * buf, int byteCount);
extern int dsk_write(off_t blkno, void * buf, int byteCount);
extern int dsk_flush();
extern off_t dsk_getsize();
extern void dsk_setsize(off_t size);

#endif /* __DSKIO_H__ */