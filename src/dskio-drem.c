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
 * @file  Disk I/O Layer: Internal functions for accessing DREM disk containers
 */

#include "dskio-internal.h"

#include <stdio.h>
#include "ini.h"

#define DREM_CYLINDERS_MIN 1
#define DREM_CYLINDERS_MAX 2048
#define DREM_HEADS_MIN 1
#define DREM_HEADS_MAX 16
#define DREM_SECTORS_MIN 1
#define DREM_SECTORS_MAX 36
#define DREM_SECTORS_DEFAULT 17
#define DREM_SECTORSIZE_MIN 1
#define DREM_SECTORSIZE_MAX 65535

static int cfghandler(void* user, const char* section, const char* name, const char* value, int lineno);

int dsk_opencontainer_drem(const struct dsk_config *cfg)
{
    int res = 0;
    int oflags = dsk_state.ro ? O_RDONLY : O_RDWR;
    FILE *cfgfile = NULL;
    char *dskfilename = NULL;
    const char *ext;

    /* open the config file. fail if an error occurs. */
    cfgfile = fopen(dsk_state.filename, "r");
    if (cfgfile == NULL) {
        res = -errno; // TODO: file I/O error
        goto exit;
    }

    /* parse the contents of the config file as a sanity check */
    {
        struct dsk_geometry g = DSK_EMPTY_GEOMETRY;
        res = ini_parse_file(cfgfile, cfghandler, &g);
        if (res != 0) {
            res = -EINVAL; // translate error
            goto exit;
        }
        /* verify that required config fields were found */
        if (g.cylinders == -1 || g.heads == -1 || g.sectors == -1) {
            res = -EINVAL;
            goto exit;
        }
    }

    /* construct the name of the .dsk file */
    dskfilename = strdup(dsk_state.filename);
    ext = strrchr(dskfilename, '.');
    strcpy(dskfilename + (ext - dskfilename), ".dsk");

    /* open the .dsk file. fail if an error occurs. */
    dsk_state.fd = open(dskfilename, oflags, 0666);
    if (dsk_state.fd < 0) {
        res = -errno; // TODO: file I/O error
        goto exit;
    }

exit:
    if (cfgfile != NULL) {
        fclose(cfgfile);
    }
    if (res != 0 && dsk_state.fd >= 0) {
        close(dsk_state.fd);
        dsk_state.fd = -1;
    }
    if (dskfilename != NULL) {
        free(dskfilename);
    }
    return res;
}

int dsk_createcontainer_drem(const struct dsk_config *cfg)
{
    FILE *cfgfile = NULL;
    char *dskfilename = NULL;
    const char *ext;

    /* verify cylinders, heads, sectors-per-track are set and within limits */
    if (dsk_state.geometry.cylinders < 0 || 
        dsk_state.geometry.heads < 0 || 
        dsk_state.geometry.sectors < 0) {
        return -EINVAL; // TODO: Disk geometry required
    }
    if (dsk_state.geometry.cylinders < DREM_CYLINDERS_MIN ||
        dsk_state.geometry.cylinders > DREM_CYLINDERS_MAX) {
        return -EINVAL; // TODO: Invalid number of cylinders
    }
    if (dsk_state.geometry.heads < DREM_HEADS_MIN || 
        dsk_state.geometry.heads > DREM_HEADS_MAX) {
        return -EINVAL; // TODO: Invalid number of heads
    }
    if (dsk_state.geometry.sectors < DREM_SECTORS_MIN ||
        dsk_state.geometry.sectors > DREM_SECTORS_MAX) {
        return -EINVAL; // TODO: Invalid sectors-per-track
    }

    /* create/truncate the config file and write its contents.
     * fail if an error occurs. */
    cfgfile = fopen(dsk_state.filename, "w");
    if (cfgfile == NULL) {
        return -errno; // TODO: file I/O error
    }
    fprintf(cfgfile, 
            "# DREM config file created by retro-fuse\n"
            "[DSK]\n"
            "Format=WDC\n"
            "Encoding=MFM\n"
            "RPM=3600\n"
            "Tracks=%d\n"
            "Sides=%d\n"
            "Sectors=%d\n"
            "Sector Size=%d\n"
            "First Sector ID=1\n"
            "Interleave=1\n"
            "Data ECC=NO\n",
            dsk_state.geometry.cylinders,
            dsk_state.geometry.heads,
            dsk_state.geometry.sectors,
            DSK_BLKSIZE);
    if (fclose(cfgfile) == EOF) {
        return -errno; // TODO: file I/O error
    }

    /* construct the name of the .dsk file */
    dskfilename = strdup(dsk_state.filename);
    ext = strrchr(dskfilename, '.');
    strcpy(dskfilename + (ext - dskfilename), ".dsk");

    /* create/truncate the .dsk file */
    dsk_state.fd = open(dskfilename, O_CREAT|O_TRUNC|O_RDWR, 0666);
    if (dsk_state.fd < 0)
        return -errno;

    /* extend the .dsk file to the size of the disk. note that on filesystems
     * that support it, this will create a sparse file that does not actually
     * consume any space on the host disk. */
    if (ftruncate(dsk_state.fd, dsk_state.geometry.size) != 0) {
        int res = -errno;
        close(dsk_state.fd);
        dsk_state.fd = -1;
        unlink(dskfilename);
        return res;
    }

    return 0;
}

int dsk_getgeometry_drem(const struct dsk_config *cfg, struct dsk_geometry *geometry)
{
    int res = 0;
    FILE *cfgfile = NULL;

    *geometry = DSK_EMPTY_GEOMETRY;

    /* open the config file and extract the geometry information. fail if an error occurs. */
    cfgfile = fopen(dsk_state.filename, "r");
    if (cfgfile == NULL) {
        return -errno; // TODO: file I/O error
    }
    res = ini_parse_file(cfgfile, cfghandler, geometry);
    if (res != 0) {
        res = -EINVAL; // translate error
    }
    fclose(cfgfile);
    
    return res;
}

/* ========== Private Functions ========== */

int cfghandler(void* user, const char* section, const char* key, const char* value, int lineno)
{
    struct dsk_geometry *geometry = (struct dsk_geometry *)user;

    /* ignore any keys not in the "DSK" section */
    if (strcasecmp(section, "DSK") != 0) {
        return 1;
    }

    /* attempt to parse the value as an unsigned integer.  this may fail
     * for keys that we aren't interested in */
    char *end;
    unsigned long val = strtoul(value, &end, 10);

    /* capture the cylinders, heads and sectors-per-track values, checking
     * that each is within the expected range */
    if (strcasecmp(key, "Tracks") == 0) {
        if (val < DREM_CYLINDERS_MIN || val > DREM_CYLINDERS_MAX) {
            return 0; // TODO: error: invalid DREM configuration
        }
        geometry->cylinders = val;
    }
    else if (strcasecmp(key, "Sides") == 0) {
        if (val < DREM_HEADS_MIN || val > DREM_HEADS_MAX) {
            return 0; // TODO: error: invalid DREM configuration
        }
        geometry->heads = val;
    }
    else if (strcasecmp(key, "Sectors") == 0) {
        if (val < DREM_SECTORS_MIN || val > DREM_SECTORS_MAX) {
            return 0; // TODO: error: invalid DREM configuration
        }
        geometry->sectors = val;
    }

    /* verify that the sector size matches the block size */
    else if (strcasecmp(key, "Sector Size") == 0) {
        if (val < DREM_SECTORSIZE_MIN || val > DREM_SECTORSIZE_MAX) {
            return 0; // TODO: error: invalid DREM configuration
        }
        if (val != DSK_BLKSIZE) {
            return -EINVAL; // TODO: error: unsupported bytes per sector
        }
    }

    /* ignore any keys that aren't recoganized */
    else {
        return 1;
    }

    /* verify that the value string contained no extraneous characteres */
    if (*end != 0) {
        return 0; // TODO: error: invalid DREM configuration
    }

    return 1;
}