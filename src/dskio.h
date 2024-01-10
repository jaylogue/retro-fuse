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
 * @file  Disk I/O Layer: API for interacting with virtual disks
 */

#ifndef __DSKIO_H__
#define __DSKIO_H__

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

enum {
    DSK_BLKSIZE = 512
};

/** Type of disk container 
 * 
 *  Identifies the type of file or device in which a disk image is stored.
 */
enum dsk_container {
    dsk_container_notspecified = -1,
    dsk_container_imagefile,        /**< Raw disk image file */
    dsk_container_dev,              /**< Host block device */
    dsk_container_hdv,              /**< TRS-80 FreHD disk file */
    dsk_container_vhd,              /**< VHD hard disk file */
    dsk_container_imd,              /**< ImageDisk floppy image file */
    dsk_container_drem,             /**< DREM floppy/hard disk image file */
};

/** Disk layout
 * 
 *  Describes the physical qualities of a disk and the organization of data
 *  and metadata stored on the disk. A disk layout can encompass any of the
 *  following properties:
 * 
 *      - disk size
 *      - number of cylinders/heads/sectors
 *      - location/format of disk metadata
 *      - static partition information
 *      - location/format of dynamic partition information
 *      - location/format of bad block information
 *      - sector interleave scheme
 */
enum dsk_layout {
    dsk_layout_notspecified = -1,
    dsk_layout_rawdisk,             /**< Raw disk with no partitioning scheme */
    dsk_layout_mbr,                 /**< PC Master Boot Record */
    dsk_layout_trsxenix,            /**< TRS-XENIX "PVH" table */
    dsk_layout_rk05,                /**< DEC RK05, whole disk */
    dsk_layout_rl01,                /**< DEC RL01, whole disk */
    dsk_layout_rl02,                /**< DEC RL02, whole disk */
    dsk_layout_rp03_v6,             /**< DEC RP03, Unix v6 partitioning scheme */
    dsk_layout_rp03_v7,             /**< DEC RP03, Unix v7 partitioning scheme */
    dsk_layout_rp03_29bsd,          /**< DEC RP03, 2.9BSD partitioning scheme */
    dsk_layout_rp06_v6,             /**< DEC RP04/RP05/RP06, Unix v6 partitioning scheme */
    dsk_layout_rp06_v7,             /**< DEC RP04/RP05/RP06, Unix v7 partitioning scheme */
    dsk_layout_rp06_29bsd,          /**< DEC RP04/RP05/RP06, 2.9BSD partitioning scheme */
    dsk_layout_211bsd_disklabel,    /**< 2.11BSD disk label */
};

/** Parameters to the dsk_open() call 
 */
struct dsk_config {
    int container;                  /**< Disk image container type (values from dsk_container) */
    off_t size;                     /**< Size (in blocks) of disk (-1 if not specified) */
    off_t offset;                   /**< Offset (in bytes) to start of disk image within container (-1 if not specified) */
    int cylinders;                  /**< Number of cylinders (-1 if not specified) */
    int heads;                      /**< Number of heads (-1 if not specified) */
    int sectors;                    /**< Number of sectors per track (-1 if not specified) */
    int interleave;                 /**< Sector interleave factor (1 = no interleave; -1 = not specified) */
    int sector0num;                 /**< Logical number of first sector in track (0 or 1; -1 = not specified) */
    int layout;                     /**< Disk layout/partitioning scheme (values from dsk_layout) */
    int partnum;                    /**< Filesystem partition number (-1 = not specified) */
    int parttype;                   /**< Filesystem partition type (-1 = not specified) */
    off_t fsoffset;                 /**< Offset (in blocks) to the start of the filesystem area (-1 if not specified) */
    off_t fssize;                   /**< Size (in blocks) of filesystem area (-1 if not specified) */
};

/** Error codes returned by the DiskIO API functions
 */
enum dsk_error {
    DSK_ERR_BASE            = -1000000,

    DSK_ERR
};

extern int dsk_open(const char *filename, const struct dsk_config *cfg, bool create, bool overwrite, bool ro);
extern int dsk_close();
extern int dsk_read(off_t blkno, void * buf, int byteCount);
extern int dsk_write(off_t blkno, const void * buf, int byteCount);
extern int dsk_flush();
extern bool dsk_readonly();
extern off_t dsk_size();
extern off_t dsk_fssize();
extern off_t dsk_fsoffset();
extern off_t dsk_fslimit();
extern void dsk_setfslimit(off_t blks);
extern void dsk_initconfig(struct dsk_config *cfg);

#endif /* __DSKIO_H__ */