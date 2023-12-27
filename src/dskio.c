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
 * @file  Functions for accessing a virtual disk stored in disk device or
 *        container file.
 */

#include <stdlib.h>
#include <stdint.h>
#include <string.h>
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

struct dsk_geometry {
    off_t size;             /**< Size (in blocks) of disk (<0 if unknown) */
    int cylinders;          /**< Number of cylinders (<0 if unknown) */
    int heads;              /**< Number of heads (<0 if unknown) */
    int sectors;            /**< Number of sectors per track (<0 if unknown) */
};

struct dsk_partinfo {
    int num;                /**< Partition number (<0 if end of list) */
    int type;               /**< Partition type code (<0 if not applicable)*/
    off_t offset;           /**< Offset (in blocks) to the start of partition */
    off_t size;             /**< Size (in blocks) of partition */
};

struct dsk_layoutinfo {
    int layout;             /**< Disk layout id (values from dsk_layout)*/
    bool partitioned;       /**< Flag indicating disk layout is subject to partitioning */
    const struct dsk_geometry *geometry;
                            /**< Size/geometry for disk layouts with fixed size/geometry
                             *   NULL for layouts that store geometry information on disk */
    const struct dsk_partinfo *parttab; 
                            /**< Partition table for disk layouts with static partition tables;
                             *   NULL for layouts that use dynamic (on-disk) partition information */
    int interleave;         /**< Sector interleave factor */
};

static struct dsk_state {
    bool isopen;            /**< Flag indicating a disk device/container is currently open */
    bool creating;          /**< Flag indicating the disk device/container is being created */
    bool ro;                /**< Flag indicating the disk device/container is opened for read only */
    int fd;                 /**< File descriptor for image file (<0 if not set) */
    int container;          /**< Disk container type (values from dsk_container) */
    int layout;             /**< Disk layout/partitioning scheme (values from dsk_layout) */
    off_t offset;           /**< Offset (in bytes) to start of disk image within container (<0 if not set) */
    struct dsk_geometry geometry; /**< Disk geometry */
    int interleave;         /**< Sector interleave factor (1 = no interleave; -1 = not specified) */
    int sector0num;         /**< Logical number of first sector in track (0 or 1; -1 = not specified) */
    off_t fsoffset;         /**< Offset (in blocks) to the start of active filesystem partition (<0 if not set) */
    off_t fssize;           /**< Size (in blocks) of active filesystem partition (<0 if not set; INT64_MAX if unlimited) */
    off_t fslimit;          /**< Limit on the max block number that can be passed to read/write (INT64_MAX if unlimited) */
} dsk_state;

static void dsk_resetstate();
static int dsk_guesscontainer(const char *filename);
static int dsk_guesslayout(const char *filename);
static int dsk_opencontainer(const char *filename, const struct dsk_config *cfg);
static int dsk_opencontainer_imagefile(const char *filename, const struct dsk_config *cfg);
static int dsk_opencontainer_dev(const char *filename, const struct dsk_config *cfg);
static int dsk_createcontainer(const char *filename, const struct dsk_config *cfg);
static int dsk_createcontainer_imagefile(const char *filename, const struct dsk_config *cfg);
static int dsk_getgeometry(const struct dsk_config *cfg);
static int dsk_getgeometry_dev();
static int dsk_getinterleave(const struct dsk_config *cfg);
static int dsk_getfspart(const struct dsk_config *cfg);
static int dsk_getparttab(const struct dsk_partinfo **outparttab);
static int dsk_writelayout(const struct dsk_config *cfg);
static const struct dsk_layoutinfo * dsk_getlayoutinfo(int layout);
static off_t dsk_blktopos(off_t blkno);

static const struct dsk_layoutinfo dsk_layouts[];


/** Open/create a disk backed by a block device or disk container file.
 * 
 * @param[in]   filename    The name of the disk container file or block
 *                          device to be opened.
 * 
 * @param[in]   cfg         dsk_config structure describing the format
 *                          and structure of the disk.
 * 
 * @param[in]   create      If true, create or initialize the disk
 *                          device/container file. If filename refers to
 *                          a container file the specified file must *not*
 *                          exist unless the overwrite flag is set.
 *
 * @param[in]   overwrite   If true, overwrite an existing disk container
 *                          file, if such a file exists. Ignored if the
 *                          create flag is not set, or if filename refers
 *                          to a block device.
 *
 * @param[in]   ro          If true, open the device/container file in read-
 *                          only mode.  Ignored if the create flag is set.
 */
int dsk_open(const char *filename, const struct dsk_config *cfg, bool create, bool overwrite, bool ro)
{
    int res = 0;

    if (dsk_state.isopen) {
        return -EBUSY;
    }

    dsk_resetstate();
    dsk_state.creating = create;
    dsk_state.ro = ro && !create;
    dsk_state.isopen = true;

    /* determine the disk container type and layout. if not expressly specified,
     * attempt to guess each value and fail if either cannot be determined */
    dsk_state.container = cfg->container;
    dsk_state.layout = cfg->layout;
    if (dsk_state.container == dsk_container_notspecified) {
        dsk_state.container = dsk_guesscontainer(filename);
        if (dsk_state.container == dsk_container_notspecified) {
            res = -EINVAL; // TODO: ERROR: Please specify container type
            goto exit;
        }
    }
    if (dsk_state.layout == dsk_layout_notspecified) {
        dsk_state.layout = dsk_guesslayout(filename);
        if (dsk_state.layout == dsk_layout_notspecified) {
            res = -EINVAL; // TODO: ERROR: Please specify disk layout
            goto exit;
        }
    }

    /* if the container file exists and is NOT a block device then fail if
     * creating a new disk and the overwrite flag is not set */
    if (dsk_state.container != dsk_container_dev &&
        access(filename, F_OK) == 0 &&
        create && !overwrite) {
        return -EEXIST;
    }

    /* if creating/initializing a new disk... */
    if (dsk_state.creating) {
        
        /* if the disk container is a block device, open it now. */
        if (dsk_state.container == dsk_container_dev) {
            res = dsk_opencontainer_dev(filename, cfg);
            if (res != 0) {
                goto exit;
            }
        }

        /* determine the size and geometry of the new disk */
        res = dsk_getgeometry(cfg);
        if (res != 0) {
            goto exit;
        }

        /* create the disk container file(s) */
        res = dsk_createcontainer(filename, cfg);
        if (res != 0) {
            goto exit;
        }

        /* write metadata and partition information to the new disk, as
         * needed for the specified disk layout */
        res = dsk_writelayout(cfg);
        if (res != 0) {
            goto exit;
        }
    }

    /* otherwise an existing disk is being opened... */
    else {

        /* open the disk container */
        res = dsk_opencontainer(filename, cfg);
        if (res != 0) {
            goto exit;
        }

        /* fetch the size and geometry of the disk */
        res = dsk_getgeometry(cfg);
        if (res != 0) {
            goto exit;
        }
    }

    /* get the sector interleave to be used */
    res = dsk_getinterleave(cfg);
    if (res != 0) {
        goto exit;
    }

    /* determine the size and location of the filesystem partition */
    res = dsk_getfspart(cfg);
    if (res != 0) {
        goto exit;
    }

exit:
    if (res != 0) {
        dsk_close();
        // TODO: delete if created
    }
    return res;
}

/** Close the virtual disk.
 */
int dsk_close()
{
    if (dsk_state.isopen) {
        if (dsk_state.fd >= 0) {
            close(dsk_state.fd);
        }
        dsk_resetstate();
    }
    return 0;
}

/** Read data from the filesystem partition.
 */
int dsk_read(off_t blkno, void * buf, int count) 
{
    if (!dsk_state.isopen) {
        return -EINVAL;
    }

    /* fail if any of the arguments are obviously invalid */
    if (blkno < 0 || count < 0 || buf == NULL) {
        return -EINVAL;
    }

    while (count) {

        /* fail if the requested block is beyond the end of the filesystem data
         * area or the I/O limit. */
        if (blkno >= dsk_state.fssize || blkno > dsk_state.fslimit) {
            return -EIO;
        }

        /* compute the file offset corresponding to the start of the current block */
        off_t blkpos = dsk_blktopos(blkno);

        /* attempt to read the current block. fail if an error occurs. */
        size_t readlen = (count > DSK_BLKSIZE) ? DSK_BLKSIZE : count;
        ssize_t res = pread(dsk_state.fd, buf, readlen, blkpos);
        if (res < 0) {
            return -errno;
        }

        /* if fewer bytes were read than requested fill in the rest with zeros */
        if (res < readlen) {
            memset(((uint8_t *)buf) + res, 0, readlen - res);
        }

        blkno++;
        buf = ((uint8_t *)buf) + readlen;
        count -= readlen;
    }

    return 0;
}

/** Write data to the filesystem partition.
 */
int dsk_write(off_t blkno, const void * buf, int count)
{
    if (!dsk_state.isopen) {
        return -EINVAL;
    }

    /* fail if any of the arguments are obviously invalid */
    if (blkno < 0 || count < 0 || buf == NULL) {
        return -EINVAL;
    }

    /* fail if the disk was opened read-only */
    if (dsk_state.ro) {
        return -EACCES;
    }

    while (count) {

        /* fail if the requested block is beyond the end of the filesystem 
         * partition or beyond the read/write limit */
        if (blkno >= dsk_state.fssize || blkno > dsk_state.fslimit) {
            return -EIO;
        }

        /* compute the file offset corresponding to the start of the current block */
        off_t blkpos = dsk_blktopos(blkno);

        /* attempt to write the current block. fail if an error occurs or if the host
         * filesystem has run out of space. */
        size_t writelen = (count > DSK_BLKSIZE) ? DSK_BLKSIZE : count;
        ssize_t res = pwrite(dsk_state.fd, buf, writelen, blkpos);
        if (res < 0) {
            return -errno;
        }
        if (res != writelen) {
            return -ENOSPC;
        }

        blkno++;
        buf = ((uint8_t *)buf) + writelen;
        count -= writelen;
    }

    return 0;
}

/** Flush any outstanding writes to the underlying device/file.
 */
int dsk_flush()
{
    if (fsync(dsk_state.fd) != 0)
        return -errno;
    return 0;
}

/** True of the disk was opened in read-only mode.
 */
bool dsk_readonly()
{
    return dsk_state.ro;
}

/** Returns the size of the disk, in disk blocks.
 * 
 *  A value of INT64_MAX is returned when the disk size is unknown and
 *  effectively unlimited. This occurs, for example, when the underlying
 *  container lacks metadata (as is the case for raw image files) and where
 *  no size information is supplied when the disk is opened.
 */
off_t dsk_size()
{
    return dsk_state.geometry.size;
}

/** Returns the size of the filesystem partition/data area, in disk blocks.
 *
 *  The size and location of the filesystem data area depends on the disk
 *  layout.  If the "rawdisk" layout is used (the default), the filesystem
 *  data area encompasses the entire disk.  If a partitioning scheme is used,
 *  the filesystem data area will be based on the specified partition information.
 * 
 *  A value of INT64_MAX is returned when the size of the filesystem data area
 *  is unknown and effectively unlimited.  This occurs when using the "rawdisk"
 *  layout and the size of the underlying disk is itself unknown/unlimited
 *  (as is the case when using a disk image file container).
 */
off_t dsk_fssize()
{
    return dsk_state.fssize;
}

/** Returns the offset to the start of filesystem partition in disk blocks.
 */
off_t dsk_fsoffset()
{
    return dsk_state.fsoffset;
}

/** Returns the current read/write limit (in blocks)
 *
 *  A value of INT64_MAX (the default) indicates no limit.
 */
off_t dsk_fslimit()
{
    return dsk_state.fslimit;
}

/** Sets the filesystem read/write limit (in blocks) 
 *
 *  The read/write limit governs the range of block numbers that can be passed to
 *  the dsk_read()/dsk_write() functions. When set, it effectively reduces the
 *  accessible area of the filesystem partition to blocks 0 through fslimit-1
 *  (or the size of the filesystem partition, whichever is smaller). Any attempt
 *  to access a block beyond this limit results in an I/O error.
 * 
 *  The read/write limit is intended to be used as a safety feature to ensure that
 *  disk accesses (and especially writes) do not stray outside of the area that
 *  filesystem code considers to be part of the filesystem, even if the underlying
 *  disk partition is larger.
 *
 *  A value of INT64_MAX (the default) disables the read/write limit.
 */
void dsk_setfslimit(off_t fslimit)
{
    dsk_state.fslimit = fslimit;
}

void dsk_initconfig(struct dsk_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    cfg->container = dsk_container_notspecified;
    cfg->offset = -1;
    cfg->size = -1;
    cfg->cylinders = -1;
    cfg->heads = -1;
    cfg->sectors = -1;
    cfg->interleave = -1;
    cfg->sector0num = -1;
    cfg->layout = dsk_layout_notspecified;
    cfg->partnum = -1;
    cfg->parttype = -1;
    cfg->fsoffset = -1;
    cfg->fssize = -1;
}

/* ========== Private Functions/Data ========== */

static void dsk_resetstate()
{
    dsk_state = (struct dsk_state) {
        .fd = -1,
        .container = dsk_container_notspecified,
        .layout = dsk_layout_notspecified,
        .fslimit = INT64_MAX
    };
}

static int dsk_guesscontainer(const char *filename)
{
    struct stat statbuf;
    const char * ext;
    bool fileexists;

    /* if the file exists and is a block device then container type is 'dev' */
    fileexists = (stat(filename, &statbuf) == 0);
    if (fileexists && S_ISBLK(statbuf.st_mode)) {
        return dsk_container_dev;
    }

    /* attempt to determine the container type from the file extension */
    ext = strrchr(filename, '.');
    if (ext != NULL) {
        if (strcasecmp(ext, ".img") == 0) { return dsk_container_imagefile; }
        if (strcasecmp(ext, ".hdv") == 0) { return dsk_container_hdv; }
        if (strcasecmp(ext, ".vhd") == 0) { return dsk_container_vhd; }
        if (strcasecmp(ext, ".imd") == 0) { return dsk_container_imd; }
        if (strcasecmp(ext, ".cfg") == 0) { return dsk_container_drem; }

        /* a .dsk file could be an raw image file or a drem file. if a drem file
         * then a like-named .cfg file will also exist. */
        if (strcasecmp(ext, ".dsk") == 0) {
            if (fileexists) {
                char * cfgname = strdup(filename);
                strcpy(cfgname + (ext - filename), ".cfg");
                fileexists = (access(cfgname, F_OK) == 0);
                free(cfgname);
                if (fileexists) {
                    return dsk_container_drem;
                }
            }
            return dsk_container_imagefile;
        }
    }

    return dsk_container_notspecified;
}

static int dsk_guesslayout(const char *filename)
{
    // TODO: finish this

    if (dsk_state.container == dsk_container_hdv) {
        return dsk_layout_trsxenix;
    }

    if (dsk_state.container == dsk_container_imagefile) {
        return dsk_layout_rawdisk;
    }

    return dsk_layout_notspecified;
}

static int dsk_opencontainer(const char *filename, const struct dsk_config *cfg)
{
    /* based on the container type, open the underlying disk file */
    switch (dsk_state.container) {
    case dsk_container_imagefile:
        return dsk_opencontainer_imagefile(filename, cfg);
    case dsk_container_dev:
        return dsk_opencontainer_dev(filename, cfg);
    default:
        return -EINVAL; // TODO: unsupported container type
    }
}

static int dsk_opencontainer_imagefile(const char *filename, const struct dsk_config *cfg)
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

static int dsk_opencontainer_dev(const char *filename, const struct dsk_config *cfg)
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

static int dsk_createcontainer(const char *filename, const struct dsk_config *cfg)
{
    /* based on the container type, create the underlying disk file(s) */
    switch (dsk_state.container) {
    case dsk_container_imagefile:
        return dsk_createcontainer_imagefile(filename, cfg);
    case dsk_container_dev:
        /* nothing to do */
        return 0;
    default:
        return -EINVAL; // TODO: unsupported container type
    }
}

static int dsk_createcontainer_imagefile(const char *filename, const struct dsk_config *cfg)
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

static int dsk_getgeometry(const struct dsk_config *cfg)
{
    const struct dsk_layoutinfo *layoutinfo;
    int res = 0;

    dsk_state.geometry.size = -1;
    dsk_state.geometry.cylinders = -1;
    dsk_state.geometry.heads = -1;
    dsk_state.geometry.sectors = -1;

    /* attempt to determine the geometry of the disk using information
     * from the specified disk layout and/or the disk container. in some
     * cases, the chosen disk layout implies a fixed geometry (e.g. the
     * DEC RL02 layout). in other cases, geometry information is stored
     * on the disk itself (trsxenix, disklabel) or stored in metadata
     * associated with the container (hdv, vhd, drem). finally, in some
     * cases (e.g. rawdisk layout stored in an imagefile container) there
     * is no geometry information available. */
    switch (dsk_state.layout) {
    case dsk_layout_rawdisk:
    case dsk_layout_mbr:
        /* these layouts do not have fixed geometries and do not store
         * geometry information on the disk. so attempt to determine the
         * geometry from metadata in the container */
        switch (dsk_state.container) {
        case dsk_container_dev:
            res = dsk_getgeometry_dev();
            break;
        case dsk_container_drem:
            res = -EINVAL; // TODO: finish this
            break;
        case dsk_container_hdv:
            res = -EINVAL; // TODO: finish this
            break;
        case dsk_container_imagefile:
            /* no geometry information available */
            break;
        case dsk_container_imd:
            res = -EINVAL; // TODO: finish this
            break;
        case dsk_container_vhd:
            res = -EINVAL; // TODO: finish this
            break;
        }
        break;
    case dsk_layout_trsxenix:
        res = -EINVAL; // TODO: finish this
        break;
    case dsk_layout_211bsd_disklabel:
        res = -EINVAL; // TODO: finish this
        break;
    default:
        /* if the disk layout implies a fixed geometry, use that. */
        layoutinfo = dsk_getlayoutinfo(dsk_state.layout);
        if (layoutinfo->geometry != NULL) {
            dsk_state.geometry = *layoutinfo->geometry;
        }
        break;
    }
    if (res != 0) {
        return res;
    }

    /* for each value of disk size, cylinder count, head count and sectors-per
     * track: if a value was given in the config paramters, use it if a
     * corresponding value couldn't be determined from the layout/container;
     * otherwise, ensure that the value given in the config parameters matches
     * exactly the value derived from the layout/container. */
    if (cfg->size >= 0) {
        if (dsk_state.geometry.size < 0) {
            dsk_state.geometry.size = cfg->size;
        }
        else if (cfg->size != dsk_state.geometry.size) {
            return -EINVAL; // ERROR: size parameter does not match layout/container
        }
    }
    if (cfg->cylinders >= 0) {
        if (dsk_state.geometry.cylinders < 0) {
            dsk_state.geometry.cylinders = cfg->cylinders;
        }
        else if (cfg->cylinders != dsk_state.geometry.cylinders) {
            return -EINVAL; // ERROR: cylinders argument does not match layout/container
        }
    }
    if (cfg->heads >= 0) {
        if (dsk_state.geometry.heads < 0) {
            dsk_state.geometry.heads = cfg->heads;
        }
        else if (cfg->heads != dsk_state.geometry.heads) {
            return -EINVAL; // ERROR: heads argument does not match layout/container
        }
    }
    if (cfg->sectors >= 0) {
        if (dsk_state.geometry.sectors < 0) {
            dsk_state.geometry.sectors = cfg->sectors;
        }
        else if (cfg->sectors != dsk_state.geometry.sectors) {
            return -EINVAL; // ERROR: sectors argument does not match layout/container
        }
    }

    /* if the disk cylinder count, head count and sectors-per-track are all known... */
    if (dsk_state.geometry.cylinders >= 0 &&
        dsk_state.geometry.heads >= 0 &&
        dsk_state.geometry.sectors >= 0) {

        /* compute the total size from the CHS values */
        off_t chssize = dsk_state.geometry.cylinders * dsk_state.geometry.heads * dsk_state.geometry.sectors;

        /* if the disk size is known, make sure the two size values match */
        if (dsk_state.geometry.size >= 0) {
            if (dsk_state.geometry.size != chssize) {
                return -EINVAL; // ERROR: specified disk geometry does not match size
            }
        }

        /* otherwise set the disk size to the computed CHS size */
        else {
            dsk_state.geometry.size = chssize;
        }
    }

    /* if at this point the disk size is still unknown, and the container is an image
     * file, then set the disk size to "unlimited" (INT64_MAX) */
    if (dsk_state.geometry.size < 0 && dsk_state.container == dsk_container_imagefile) {
        dsk_state.geometry.size = INT64_MAX;
    }

    return 0;
}

static int dsk_getgeometry_dev()
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

static int dsk_getinterleave(const struct dsk_config *cfg)
{
    const struct dsk_layoutinfo *layoutinfo = dsk_getlayoutinfo(dsk_state.layout);

    /* if an interleave parameter was given... */
    if (cfg->interleave >= 0) {

        /* fail if the value is invalid */
        if (cfg->interleave == 0) {
            return -EINVAL; // TODO: ERROR: Invalid interleave value
        }

        /* fail if the layout specifies an interleave and the given
         * interleave parameter doesn't match */
        if (layoutinfo->interleave >= 1 && cfg->interleave != layoutinfo->interleave) {
            return -EINVAL; // TODO: ERROR: Invalid interleave value
        }

        /* set the interleave to the given value */
        dsk_state.interleave = cfg->interleave;
    }

    /* otherwise, use the interleave defined by the layout (if any) */
    else {
        dsk_state.interleave = layoutinfo->interleave;
    }

    /* if no interleave defined by either the layout or config, 
     * default the interleave to 1 (i.e. no interleave) */
    if (dsk_state.interleave < 0) {
        dsk_state.interleave = 1;
    }

    /* fail if an interleave has been specified, but the number of
     * sectors-per-track is not known. */
    if (dsk_state.interleave > 1 && dsk_state.geometry.sectors < 0) {
        return -EINVAL; // TODO: ERROR: Sectors per track required when interleave > 1 is used
    }

    return 0;
}

static int dsk_getfspart(const struct dsk_config *cfg)
{
    int res;
    const struct dsk_layoutinfo *layoutinfo = dsk_getlayoutinfo(dsk_state.layout);
    const struct dsk_partinfo *parttab = NULL, *p = NULL;

    dsk_state.fsoffset = -1;
    dsk_state.fssize = -1;

    /* if the disk layout supports partitioning attempt to fetch the
     * partition table; return if an error occurs. */
    if (layoutinfo->partitioned) {
        res = dsk_getparttab(&parttab);
        if (res != 0) {
            return res;
        }
    }

    /* if the partnum and/or parttype parameters were given...*/
    if (cfg->partnum >= 0 || cfg->parttype >= 0) {

        /* if a partition table wasn't found, fail with an error */
        if (parttab == NULL) {
            return -EINVAL; // TODO: ERROR: partition table not found
        }

        /* if the partnum parameter was given, search the partition table
         * for a partition with a matching number and fail if it isn't found. */
        if (cfg->partnum >= 0) {
            for (p = parttab; p->num >= 0 && p->num != cfg->partnum; p++);
            if (p->num < 0) {
                return -EINVAL; // TODO: ERROR: partition not found
            }

            /* if parttype was also given, fail if the type of the selected
             * partition doesn't match. */
            if (cfg->parttype >= 0 && p->type != cfg->parttype) {
                return -EINVAL; // ERROR: partition type does not match selected partition
            }
        }

        /* Otherwise, only parttype parameter specified, so search for the first
         * partition of that type and fail if no matching partition exists. */
        else {
            for (p = parttab; p->num >= 0 && p->type != cfg->parttype; p++);
            if (p->num < 0) {
                return -EINVAL; // ERROR: partition type not found
            }
        }

        /* setup the selected partition as the active filesystem partition */
        dsk_state.fsoffset = p->offset;
        dsk_state.fssize = p->size;

        /* if the fsoffset parameter was also given, verify that the
         * given specified exactly matches the offset of the selected partition */
        if (cfg->fsoffset >= 0 && cfg->fsoffset != p->offset) {
            return -EINVAL; // TODO: ERROR: fsoffset must match start offset of selected partition
        }

        /* if the fssize parameter was given, use the value as the size
         * of the active partition as long as the value is equal or smaller
         * than the size of the selected partition. */
        if (cfg->fssize >= 0) {
            if (cfg->fssize > p->size) {
                return -EINVAL; // TODO: ERROR: fssize must be <= size of selected partition
            }
            dsk_state.fssize = cfg->fssize;
        }
    }

    /* otherwise, if only fsoffset and/or fssize parameters given... */
    else if (cfg->fsoffset >= 0 || cfg->fssize >= 0) {

        /* fail if a filesystem size was not given */
        if (cfg->fssize < 0) {
            return -EINVAL; // TODO: ERROR: Please specified size of the filesystem using fssize parameter
        }

        dsk_state.fsoffset = (cfg->fsoffset >= 0) ? cfg->fsoffset : 0;
        dsk_state.fssize = cfg->fssize;

        /* fail if the specified offset+size exceeds the disk size */
        if (dsk_state.fsoffset + dsk_state.fssize > dsk_state.geometry.size) {
            return -EINVAL; // TODO: ERROR: Size and location of filesystem exceeds the size of the disk
        }
    }

    /* otherwise, none of partnum, parttype, fsoffset and fssize given... */
    else {

        /* if the disk contains a single partition, select it by default */
        if (parttab != NULL && parttab[0].num >= 0 && parttab[1].num < 0) {
            dsk_state.fsoffset = parttab[0].offset;
            dsk_state.fssize = parttab[0].size;
        }

        /* if the disk does not support partitioning, use the whole disk by default */
        else if (!layoutinfo->partitioned) {
            dsk_state.fsoffset = 0;
            dsk_state.fssize = dsk_state.geometry.size;
        }

        /* otherwise fail with an error */
        else {
            return -EINVAL; // TODO: ERROR: Please specify location of filesystem using partnum, parttype, fsoffset and/or fssize parameters
        }
    }

    return 0;
}

static int dsk_getparttab(const struct dsk_partinfo **outparttab)
{
    const struct dsk_layoutinfo *layoutinfo;
    int res = 0;

    switch (dsk_state.layout) {
    case dsk_layout_mbr:
        res = -EINVAL; // TODO: implement this
        break;
    case dsk_layout_trsxenix:
        res = -EINVAL; // TODO: implement this
        break;
    case dsk_layout_211bsd_disklabel:
        res = -EINVAL; // TODO: implement this
        break;
    default:
        /* if the layout implies a fixed partitioning scheme, 
         * return the associated partition table. */
        layoutinfo = dsk_getlayoutinfo(dsk_state.layout);
        if (layoutinfo->parttab != NULL) {
            *outparttab = layoutinfo->parttab;
        }
        else {
            res = -EINVAL; // TODO: ERROR: Layout does not support partitioning
        }
        break;
    }
    return res;
}

static int dsk_writelayout(const struct dsk_config *cfg)
{
    switch (dsk_state.layout) {
    case dsk_layout_mbr:
        return -EINTR; // TODO: implement this
    case dsk_layout_trsxenix:
        return -EINTR; // TODO: implement this
    case dsk_layout_211bsd_disklabel:
        return -EINTR; // TODO: implement this
    default:
        /* nothing to do for all other layouts */
        return 0;
    }
}

static const struct dsk_layoutinfo * dsk_getlayoutinfo(int layout)
{
    const struct dsk_layoutinfo *layoutinfo;

    for (layoutinfo = dsk_layouts; 
         layoutinfo->layout != dsk_layout_notspecified && layoutinfo->layout != layout;
         layoutinfo++);

    return (layoutinfo->layout != dsk_layout_notspecified) ? layoutinfo : NULL;
}

static off_t dsk_blktopos(off_t blkno)
{
    off_t blkpos;

    // TODO: add alternate block/trace support

    /* if there's no interleave... */
    if (dsk_state.interleave == 1) {

        /* compute the file position for the block using a simple linear arrangement:
         *     offset to start of disk image
         *     + offset to start of filesystem partition within disk
         *     + offset to start of block within filesystem partition */
        blkpos = dsk_state.offset
               + (dsk_state.fsoffset * DSK_BLKSIZE)
               + (blkno * DSK_BLKSIZE);
    }

    /* otherwise, handle sector interleave... */
    else {

        /* compute the desired block number relative to the start of the disk */
        blkno += dsk_state.fsoffset;

        /* compute the track number and sector number for the given block number */
        off_t trackno = blkno / dsk_state.geometry.sectors;
        off_t secno = blkno % dsk_state.geometry.sectors;

        /* if the container type numbers its sectors starting with 1, adjust the 
         * sector number accordingly */
        if (dsk_state.sector0num == 1) {
            secno++;
        }

        /* convert the logical sector number to an index based on the interleave factor */
        off_t secindex = (secno * dsk_state.interleave) % dsk_state.geometry.sectors;

        /* compute the file position for the block:
         *     offset to start of disk image
         *     + offset to start of track within disk
         *     + offset to start of sector within track */
        blkpos = dsk_state.offset
               + (trackno * dsk_state.geometry.sectors * DSK_BLKSIZE)
               + (secindex * DSK_BLKSIZE);
    }

    return blkpos;
}

static const struct dsk_layoutinfo dsk_layouts[] = {
    {
        .layout = dsk_layout_rawdisk,
        .partitioned = false,
        .interleave = -1,
        .geometry = NULL,
        .parttab = NULL,
    },
    {
        .layout = dsk_layout_rk05,
        .partitioned = false,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 4872,
            .cylinders = 203,
            .heads = 2,
            .sectors = 12,
        },
        .parttab = NULL,
    },
    {
        .layout = dsk_layout_rl01,
        .partitioned = false,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 10240,
            .cylinders = 256,
            .heads = 2,
            .sectors = 20,
        },
        .parttab = NULL
    },
    {
        .layout = dsk_layout_rl02,
        .partitioned = false,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 20480,
            .cylinders = 512,
            .heads = 2,
            .sectors = 20,
        },
        .parttab = NULL
    },
    {
        .layout = dsk_layout_rp03_v6,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 81200,
            .cylinders = 406,
            .heads = 20,
            .sectors = 10,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,     .size = 40600 },
            { .num = 1, .offset = 40600, .size = 40600 },
            { .num = 2, .offset = 0,     .size = 9200  },
            { .num = 3, .offset = 72000, .size = 9200  },
            { .num = 4, .offset = 0,     .size = 65535 },
            { .num = 5, .offset = 15600, .size = 65535 },
            { .num = 6, .offset = 0,     .size = 15600 },
            { .num = 7, .offset = 65600, .size = 15600 },
            { .num = -1 }
        }
    },
    {
        .layout = dsk_layout_rp03_v7,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 81200,
            .cylinders = 406,
            .heads = 20,
            .sectors = 10,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,     .size = 81000 },
            { .num = 1, .offset = 0,     .size = 5000  },
            { .num = 2, .offset = 5000,  .size = 2000  },
            { .num = 3, .offset = 7000,  .size = 74000 },
            { .num = -1 }
        }
    },
    {
        .layout = dsk_layout_rp06_v6,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 340670,
            .cylinders = 815,
            .heads = 19,
            .sectors = 22,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,      .size = 9614  },
            { .num = 1, .offset = 18392,  .size = 65535 },
            { .num = 2, .offset = 48018,  .size = 65535 },
            { .num = 3, .offset = 149644, .size = 20900 },
            { .num = 4, .offset = 0,      .size = 40600 },
            { .num = 5, .offset = 41800,  .size = 40600 },
            { .num = 6, .offset = 83600,  .size = 40600 },
            { .num = 7, .offset = 125400, .size = 40600 },
            { .num = -1 }
        }
    },
    {
        .layout = dsk_layout_rp06_v7,
        .partitioned = true,
        .interleave = 1,
        .geometry = &(const struct dsk_geometry) {
            .size = 340670,
            .cylinders = 815,
            .heads = 19,
            .sectors = 22,
        },
        .parttab = (const struct dsk_partinfo[]) {
            { .num = 0, .offset = 0,      .size = 9614   },
            { .num = 1, .offset = 9614,   .size = 8778   },
            { .num = 4, .offset = 18392,  .size = 161348 },
            { .num = 5, .offset = 179740, .size = 160930 },
            { .num = 6, .offset = 18392,  .size = 153406 },
            { .num = 7, .offset = 18392,  .size = 322278 },
            { .num = -1 }
        }
    },
    { .layout = dsk_layout_notspecified }
};
