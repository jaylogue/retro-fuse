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
 * @file  FUSE filesystem implementation for Xenix filesystems.
 */

#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>

#include "fusecommon.h"
#include "fstype.h"
#include "v7fs.h"
#include "dskio.h"

struct adddir_context {
    void *buf;
    fuse_fill_dir_t filler;
};

#define RETROFUSE_OPT_KEY_MKFS      100
#define RETROFUSE_OPT_KEY_MAPUID    101
#define RETROFUSE_OPT_KEY_MAPGID    102
#define RETROFUSE_OPT_KEY_FSTYPE    103
#define RETROFUSE_OPT_KEY_DSKLAYOUT 104

static int parsefstype(const char *arg, struct retrofuse_config *cfg);
static int parsedsklayout(const char *arg, struct retrofuse_config *cfg);

const char *retrofuse_cmdname = "xenixfs";

const struct fuse_opt retrofuse_fuseopts[] = {
    FUSE_OPT_KEY("mapuid", RETROFUSE_OPT_KEY_MAPUID),
    FUSE_OPT_KEY("mapuid=", RETROFUSE_OPT_KEY_MAPUID),
    FUSE_OPT_KEY("mapgid", RETROFUSE_OPT_KEY_MAPGID),
    FUSE_OPT_KEY("mapgid=", RETROFUSE_OPT_KEY_MAPGID),
    FUSE_OPT_KEY("fstype=", RETROFUSE_OPT_KEY_FSTYPE),
    FUSE_OPT_KEY("dsklayout=", RETROFUSE_OPT_KEY_DSKLAYOUT),
    FUSE_OPT_END
};

void retrofuse_initconfig(struct retrofuse_config *cfg)
{
    cfg->fstype = fs_type_unknown;

    /* setup default values for mkfs parameters */
    cfg->mkfscfg.isize = 0; /* 0 = automatically compute isize */
    cfg->mkfscfg.n = 500;
    cfg->mkfscfg.m = 3;
}

int retrofuse_parseopt(void *data, const char *arg, int key, struct fuse_args *args)
{
    struct retrofuse_config *cfg = (struct retrofuse_config *)data;

    switch (key)
    {
    case FUSE_OPT_KEY_OPT:
    case FUSE_OPT_KEY_NONOPT:
        return 1;
    case RETROFUSE_OPT_KEY_FSTYPE:
        return parsefstype(arg, cfg);
    case RETROFUSE_OPT_KEY_DSKLAYOUT:
        return parsedsklayout(arg, cfg);
    case RETROFUSE_OPT_KEY_MAPUID:
        return retrofuse_parseopt_idmap(arg, V7FS_MAX_UID_GID, v7fs_adduidmap);
    case RETROFUSE_OPT_KEY_MAPGID:
        return retrofuse_parseopt_idmap(arg, V7FS_MAX_UID_GID, v7fs_addgidmap);
    default:
        fprintf(stderr, "%s: ERROR: Invalid option key: %d\n", retrofuse_cmdname, key);
        return -1;
    }
}

int retrofuse_checkconfig(struct retrofuse_config *cfg)
{
    /* verify the filesystem type was specified */
    if (cfg->fstype == fs_type_unknown) {
        fprintf(stderr, "%s: ERROR: Please specify the Xenix filesystem type using the -o fstype option.\n",
                retrofuse_cmdname);
        return -1;
    }

    return retrofuse_checkconfig_v7(cfg);
}

void retrofuse_showhelp()
{
    printf(
        "usage: %s [options] <device-or-image-file> <mount-point>\n"
        "\n"
        "OPTIONS\n"
        "  -o ro\n"
        "        Mount the filesystem in read-only mode.\n"
        "\n"
        "  -o rw\n"
        "        Mount the filesystem in read-write mode.  This is the default.\n"
        "\n"
        "  -o allow_root\n"
        "        Allow the root user to access the filesystem, in addition to the\n"
        "        mounting user.  Mutually exclusive with the allow_other option.\n"
        "\n"
        "  -o allow_other\n"
        "        Allow any user to access the filesystem, including root and the\n"
        "        mounting user.  Mutually exclusive with the allow_root option.\n"
        "\n"
        "  -o mapuid=<host-uid>:<fs-uid>\n"
        "  -o mapgid=<host-gid>:<fs-gid>\n"
        "        Map a particular user or group id on the host system to a different\n"
        "        id on the mounted filesystem. The id mapping applies both ways.\n"
        "        Specifically, the uid/gid on the host is mapped to the corresponding\n"
        "        id for the purpose of access control checking, and when the id is\n"
        "        stored in an inode. Conversely, the filesystem id is mapped to the\n"
        "        host system id whenever a file is stat()ed or a directory is read.\n"
        "        Multiple map options may be given, up to a limit of 100.\n"
        "\n"
        "  -o fssize=<blocks>\n"
        "        The size of the filesystem, in 512-byte blocks. This is used to\n"
        "        constrain I/O on the underlying device/image file.  Defaults to the\n"
        "        size given in the filesystem superblock.\n"
        "\n"
        "  -o fsoffset=<blocks>\n"
        "        Offset into the device/image at which the filesystem starts, in\n"
        "        512-byte blocks.  Defaults to 0.\n"
        "\n"
        "  -o mkfs\n"
        "  -o mkfs=<isize>\n"
        "  -o mkfs=<isize>:<n>:<m>\n"
        "        Create an empty filesystem on the underlying device/image file before\n"
        "        mounting.\n"
        "\n"
        "        isize gives the size of the filesystem's inode table in 512-byte\n"
        "        blocks (8 inodes per block).  If not specified, or if 0 is given,\n"
        "        an appropriate inode table size is computed automatically from the size\n"
        "        of the filesystem, using the logic found in the original v7 mkfs command.\n"
        "\n"
        "        n and m are the interleave parameters for the initial free block list.\n"
        "        Specifying n=1,m=1 produces no interleave. The default is n=500,m=3,\n"
        "        which is the same as used in the original v7 mkfs command.\n"
        "\n"
        "  -o overwrite\n"
        "        When used with the mkfs option, instructs the filesystem handler\n"
        "        to overwrite any existing filesystem image file. Without this option,\n"
        "        the mkfs option will fail with an error if an image file exists.\n"
        "\n"
        "  -o <mount-options>\n"
        "        Comma-separated list of standard mount options. See man 8 mount\n"
        "        for further details.\n"
        "\n"
        "  -o <fuse-options>\n"
        "        Comma-separated list of FUSE mount options. see man 8 mount.fuse\n"
        "        for further details.\n"
        "\n"
        "  -f\n"
        "  --foreground\n"
        "        Run in foreground (useful for debugging).\n"
        "\n"
        "  -d\n"
        "  --debug\n"
        "        Enable debug output to stderr (implies -f)\n"
        "\n"
        "  -V\n"
        "  --version\n"
        "        Print version information\n"
        "\n"
        "  -h\n"
        "  --help\n"
        "        Print this help message.\n",
        retrofuse_cmdname);
}

/* ========== Private Functions/Data ========== */

static int parsefstype(const char *arg, struct retrofuse_config *cfg)
{
    static const struct {
        const char * name;
        int fstype;
        int layout;
    } fstypes[] = {
        { "xenix2le",   fs_type_msxenix2_le, dsk_layout_notspecified },
        { "xenix2be",   fs_type_msxenix2_be, dsk_layout_notspecified },
        { "xenix3le",   fs_type_msxenix3_le, dsk_layout_notspecified },
        { "xenix3be",   fs_type_msxenix3_be, dsk_layout_notspecified },

        { "trsxenix1",  fs_type_msxenix2_be, dsk_layout_trsxenix     },
        { "trsxenix3",  fs_type_msxenix3_be, dsk_layout_trsxenix     },

        { "ibmxenix",   fs_type_ibmpcxenix,  dsk_layout_mbr          },

        { NULL },
    };

    const char *val = strchr(arg, '=') + 1;

    for (int i = 0; fstypes[i].name != NULL; i++) {
        if (strcasecmp(val, fstypes[i].name) == 0) {
            cfg->fstype = fstypes[i].fstype;
            if (cfg->dskcfg.layout == dsk_layout_notspecified) {
                cfg->dskcfg.layout = fstypes[i].layout;
            }
            return 0;
        }
    }

    fprintf(stderr, "%s: ERROR: unknown filesystem type: %s\n", retrofuse_cmdname, val);
    return -1;
}

static int parsedsklayout(const char *arg, struct retrofuse_config *cfg)
{
    static const struct {
        const char * name;
        int id;
    } layouts[] = {
        { "raw",        dsk_layout_rawdisk   },
        { "rawdisk",    dsk_layout_rawdisk   },
        { "mbr",        dsk_layout_mbr       },
        { "trs-xenix",  dsk_layout_trsxenix  },
        { "trsxenix",   dsk_layout_trsxenix  },
        { NULL },
    };

    const char *val = strchr(arg, '=') + 1;

    for (int i = 0; layouts[i].name != NULL; i++) {
        if (strcasecmp(val, layouts[i].name) == 0) {
            cfg->dskcfg.layout = layouts[i].id;
            return 0;
        }
    }

    fprintf(stderr, "%s: ERROR: unknown disk layout: %s\n", retrofuse_cmdname, val);
    return -1;
}
