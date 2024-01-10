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
 * @file  Disk I/O Layer: Internal functions for trsxenix layout
 */

#include "dskio-internal.h"

#define TRSXENIX_BOOTTRACKS 2
#define TRSXENIX_DEFAULTALTTRACKS 24

#define TRSXENIX_DISKPARAM_BLKNO 0
#define TRSXENIX_DISKPARAM_TABLEMARKER ".pvh"
#define TRSXENIX_DISKPARAM_MINCYLINDERS 31
#define TRSXENIX_DISKPARAM_MAXCYLINDERS 2048
#define TRSXENIX_DISKPARAM_MINHEADS 2
#define TRSXENIX_DISKPARAM_MAXHEADS 16
#define TRSXENIX_DISKPARAM_SECTORS 17
#define TRSXENIX_DISKPARAM_SECTORSSIZE 512

#define TRSXENIX_BADTRACK_BLKNO 1
#define TRSXENIX_BADTRACK_TABLEMARKER ".bad"
#define TRSXENIX_BADTRACK_MAXBAD 102


/** Represents the TRS-XENIX disk parameter table (a.k.a. ".pvh" table) */
struct trsxenix_diskparm {
    char marker[4];             /**< table marker (".pvh") */
    uint16_t length;            /**< table length; value: 16 */
    uint16_t cylinders;         /**< number of cylinders*/
    uint16_t heads;             /**< number of heads */
    uint16_t sectors;           /**< number of sectors per track */
    uint16_t sectorsize;        /**< number of bytes per sector */
    uint16_t precomp;           /**< (presumed) write-precomp cylinder number */
} __attribute__((packed));

/** Represents the TRS-XENIX bad track table (a.k.a. ".bad" table)*/
struct trsxenix_badtrack {
    char marker[4];             /**< table marker (".bad") */
    uint16_t alttracks;         /**< number of alternate tracks at end of disk */
    uint16_t badtracks;         /**< number of bad tracks recorded in badtrack table */
    struct {
        uint16_t cylinder;      /**< cylinder of bad track */
        uint16_t head;          /**< head of bad track */
    } badtrack[TRSXENIX_BADTRACK_MAXBAD];
} __attribute__((packed));

int dsk_getgeometry_trsxenix(const struct dsk_config *cfg)
{
    union {
        struct trsxenix_diskparm dskparm;
        uint8_t blkdata[DSK_BLKSIZE];
    } buf;
    int res;

    /* if creating a new disk... */
    if (dsk_state.creating) {

        /* verify cylinders and heads parameters given and within limits */
        if (cfg->cylinders < 0 || cfg->heads < 0) {
            return -EINVAL; // TODO: Disk geometry required
        }
        if (cfg->cylinders < TRSXENIX_DISKPARAM_MINCYLINDERS ||
            cfg->cylinders > TRSXENIX_DISKPARAM_MAXCYLINDERS) {
            return -EINVAL; // TODO: Invalid number of cylinders
        }
        if (cfg->heads < TRSXENIX_DISKPARAM_MINHEADS ||
            cfg->heads > TRSXENIX_DISKPARAM_MAXHEADS) {
            return -EINVAL; // TODO: Invalid number of heads
        }
        dsk_state.geometry.cylinders = cfg->cylinders;
        dsk_state.geometry.heads = cfg->heads;

        /* if sectors not specified, assume default */
        dsk_state.geometry.sectors = (cfg->sectors > 0) ? cfg->sectors : TRSXENIX_DISKPARAM_SECTORS;
    }

    /* otherwise read geometery from the disk... */
    else {

        /* attempt to read the block containing the Xenix disk parameter table */
        res = dsk_read(TRSXENIX_DISKPARAM_BLKNO, &buf, DSK_BLKSIZE);
        if (res < 0) {
            return res;
        }

        /* check for the table marker and verify the table length. fail if not correct. */
        if (strncmp(buf.dskparm.marker, TRSXENIX_DISKPARAM_TABLEMARKER, sizeof(buf.dskparm.marker)) != 0 ||
            fs_htobe_u16(buf.dskparm.length) != sizeof(struct trsxenix_diskparm)) {
            return -EINVAL; // TODO: Error Xenix disk parameter table not found
        }

        /* read the disk geometry from the parameter table */
        dsk_state.geometry.cylinders = fs_htobe_u16(buf.dskparm.cylinders);
        dsk_state.geometry.heads = fs_htobe_u16(buf.dskparm.heads);
        dsk_state.geometry.sectors = fs_htobe_u16(buf.dskparm.sectors);
    }

    return 0;
}

int dsk_getparttab_trsxenix(const struct dsk_partinfo **outparttab)
{
    union {
        struct trsxenix_badtrack badtrack;
        uint8_t blkdata[DSK_BLKSIZE];
    } buf;
    int res;

    static struct dsk_partinfo parttab[] = {
        { .num = 1, .type = -1 },
        { .num = -1 }
    };

    /* attempt to read the block containing the Xenix bad track table */
    res = dsk_read(TRSXENIX_BADTRACK_BLKNO, &buf, DSK_BLKSIZE);
    if (res < 0) {
        return res;
    }

    /* check for the table marker. fail if not found... */
    if (strncmp(buf.badtrack.marker, TRSXENIX_BADTRACK_TABLEMARKER, sizeof(buf.badtrack.marker)) != 0) {
        return -EINVAL; // TODO: Error Xenix bad track table not found
    }

    /* fetch the number of alternate tracks */
    uint16_t alttracks = fs_htobe_u16(buf.badtrack.alttracks);

    /* compute the number of tracks in the filesystem partition. fail if the alttracks
     * value is bad. */
    off_t parttracks = (dsk_state.geometry.cylinders * dsk_state.geometry.heads)
                       - TRSXENIX_BOOTTRACKS
                       - alttracks;
    if (parttracks < 1) {
        return -EINVAL; // TODO: Invalid value in Xenix bad track table
    }

    /* setup the offset and size (in blocks) of the filesystem partition */
    parttab[0].offset = ((off_t)TRSXENIX_BOOTTRACKS) * dsk_state.geometry.sectors;
    parttab[0].size = parttracks * dsk_state.geometry.sectors;

    *outparttab = parttab;

    return 0;
}

int dsk_writelayout_trsxenix(const struct dsk_config *cfg)
{
    union {
        struct trsxenix_diskparm dskparm;
        struct trsxenix_badtrack badtrack;
        uint8_t blkdata[DSK_BLKSIZE];
    } buf;
    int res;
    uint16_t alttracks;

    memset(&buf, 0, sizeof(buf));
    memcpy(buf.dskparm.marker, TRSXENIX_DISKPARAM_TABLEMARKER, sizeof(buf.dskparm.marker));
    buf.dskparm.length = fs_htobe_u16(sizeof(struct trsxenix_diskparm));
    buf.dskparm.cylinders = fs_htobe_u16(dsk_state.geometry.cylinders);
    buf.dskparm.heads = fs_htobe_u16(dsk_state.geometry.heads);
    buf.dskparm.sectors = fs_htobe_u16(dsk_state.geometry.sectors);
    buf.dskparm.sectorsize = fs_htobe_u16(TRSXENIX_DISKPARAM_SECTORSSIZE);
    buf.dskparm.precomp = fs_htobe_u16(dsk_state.geometry.cylinders / 2);
    res = dsk_write(TRSXENIX_DISKPARAM_BLKNO, &buf, DSK_BLKSIZE);
    if (res < 0) {
        return res;
    }

    /* if the filesystem partition size parameter was given... */
    if (cfg->fssize > 0) {

        /* calculate the requested fs partition size in terms of whole tracks,
         * rounding down if necessary. fail if the specified size exceeds the
         * available disk space */
        off_t parttracks = cfg->fssize / TRSXENIX_DISKPARAM_SECTORS;
        off_t maxparttracks = (cfg->cylinders * cfg->heads) - TRSXENIX_BOOTTRACKS;
        if (parttracks > maxparttracks) {
            return -EINVAL; // TODO: Filesystem partition size too big
        }

        /* set the number of alternate tracks equal to the remaining number of
         * tracks beyond the requested end of the filesystem partition */
        alttracks = (uint16_t)(maxparttracks - parttracks);
    }

    /* otherwise, automatically reserve a default set alternate tracks
     * and use the remainder of the disk for the filesystem. */
    else {
        alttracks = TRSXENIX_DEFAULTALTTRACKS;
    }

    // TODO: write copyright/id string to offset 0x1a0 in badtrack block???

    /* write an empty badtrack table to disk */
    memset(&buf, 0, sizeof(buf));
    memcpy(buf.badtrack.marker, TRSXENIX_BADTRACK_TABLEMARKER, sizeof(buf.badtrack.marker));
    buf.badtrack.alttracks = fs_htobe_u16(alttracks);
    res = dsk_write(TRSXENIX_BADTRACK_BLKNO, &buf, DSK_BLKSIZE);
    if (res < 0) {
        return res;
    }

    return 0;
}
