/*
 * Copyright 2024 Jay Logue
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
 * @file  Definitions used in the implementation of the disk I/O layer.
 */

#ifndef __DSKIO_INTERNAL_H__
#define __DSKIO_INTERNAL_H__

#include "dskio.h"
#include "fsbyteorder.h"

// TODO: eliminate/move some of these???
#include <string.h>
#include <sys/types.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


/** Represents the logical goemetry of a disk
 */
struct dsk_geometry {
    off_t size;             /**< Overall size (in blocks) of disk (<0 if unknown) */
    int cylinders;          /**< Number of cylinders (<0 if unknown) */
    int heads;              /**< Number of heads (<0 if unknown) */
    int sectors;            /**< Number of sectors per track (<0 if unknown) */
};

/** Represents disk partition information
 */
struct dsk_partinfo {
    int num;                /**< Partition number (<0 if end of list) */
    int type;               /**< Partition type code (<0 if not applicable)*/
    off_t offset;           /**< Offset (in blocks) to the start of partition */
    off_t size;             /**< Size (in blocks) of partition */
};

/** Describes the features associated with a particular logical disk layout
 */
struct dsk_layoutinfo {
    int layout;             /**< Disk layout id (values from dsk_layout)*/
    bool partitioned;       /**< Flag indicating disk layout is subject to partitioning */
    const struct dsk_geometry *geometry;
                            /**< Size/geometry for disk layouts with fixed size/geometry
                             *   NULL for layouts that store geometry information on
                             *   disk, or for which geometry is unavailable */
    const struct dsk_partinfo *parttab; 
                            /**< Partition table for disk layouts with static partition tables;
                             *   NULL for layouts that use dynamic (on-disk) partition information */
    int interleave;         /**< Sector interleave factor */
};

/** Contains the current state of the disk I/O layer
 */
struct dsk_state {
    bool isopen;            /**< Flag indicating a disk device/container is currently open */
    bool creating;          /**< Flag indicating the disk device/container is being created */
    bool ro;                /**< Flag indicating the disk device/container is opened for read only */
    int fd;                 /**< File descriptor for image file (<0 if not set) */
    int container;          /**< Disk container type (values from dsk_container) */
    int layout;             /**< Disk layout/partitioning scheme (values from dsk_layout) */
    off_t offset;           /**< Offset (in bytes) to start of disk image within container */
    struct dsk_geometry geometry; /**< Disk geometry */
    int interleave;         /**< Sector interleave factor (1 = no interleave) */
    int sector0num;         /**< Logical number of first sector in track (0 or 1) */
    off_t fsoffset;         /**< Offset (in blocks) to the start of active filesystem partition */
    off_t fssize;           /**< Size (in blocks) of active filesystem partition (INT64_MAX if unlimited) */
    off_t fslimit;          /**< Limit on the max block number that can be passed to read/write (INT64_MAX if unlimited) */
};

extern struct dsk_state dsk_state;
extern const struct dsk_layoutinfo dsk_layouts[];

/* Internal functions used by the disk I/O layer */

extern void dsk_resetstate();
extern int dsk_guesscontainer(const char *filename);
extern int dsk_guesslayout(const char *filename);
extern int dsk_opencontainer(const char *filename, const struct dsk_config *cfg);
extern int dsk_opencontainer_imagefile(const char *filename, const struct dsk_config *cfg);
extern int dsk_opencontainer_dev(const char *filename, const struct dsk_config *cfg);
extern int dsk_createcontainer(const char *filename, const struct dsk_config *cfg);
extern int dsk_createcontainer_imagefile(const char *filename, const struct dsk_config *cfg);
extern int dsk_getgeometry(const struct dsk_config *cfg);
extern int dsk_getgeometry_dev();
extern int dsk_getgeometry_trsxenix(const struct dsk_config *cfg);
extern int dsk_getinterleave(const struct dsk_config *cfg);
extern int dsk_getfspart(const struct dsk_config *cfg);
extern int dsk_getparttab(const struct dsk_partinfo **outparttab);
extern int dsk_getparttab_trsxenix(const struct dsk_partinfo **outparttab);
extern int dsk_writelayout(const struct dsk_config *cfg);
extern int dsk_writelayout_trsxenix(const struct dsk_config *cfg);
extern const struct dsk_layoutinfo * dsk_getlayoutinfo(int layout);
extern off_t dsk_blktopos(off_t blkno);

#endif /* __DSKIO_INTERNAL_H__ */
