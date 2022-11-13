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
 * @file  Adaptation code used to help build ancient Unix C source code in
 * modern context.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#if defined(__APPLE__)
#include <sys/types.h>
#endif
#if defined(__linux__)
#include <sys/sysmacros.h>
#endif
#include <stdarg.h>

#include "dskio.h"

#include "v7adapt.h"
#include "h/param.h"
#include "h/buf.h"
#include "h/dir.h"
#include "h/user.h"
#include "h/conf.h"
#include "h/file.h"
#include "h/inode.h"
#include "h/filsys.h"
#include "h/mount.h"

static int16_t v7_dsk_open(dev_t dev, int16_t flag);
static int16_t v7_dsk_close(dev_t dev, int16_t flag);
static int16_t v7_dsk_strategy(struct buf *bp);

/* Global values describing the v7-based filesystem being accessed */
struct v7_fsconfig v7_fsconfig;

/* Unix v7 global structures and values. */

struct v7_user v7_u;
struct v7_mount v7_mount[NMOUNT];
struct v7_inode v7_inode[NINODE];
struct v7_buf v7_buf[NBUF];
struct v7_file v7_file[NFILE];
struct v7_buf v7_bfreelist;
struct v7_mount v7_mount[NMOUNT];

dev_t	v7_rootdev;                             /* root device number (always 0) */
int16_t	v7_nblkdev;                             /* number of entries in the block device switch table */
int16_t	v7_nchrdev;                             /* number of entries in the character device table (always 0) */
struct v7_inode *v7_rootdir;                    /* pointer to root directory inode */
v7_time_t	v7_time;                            /* time in sec from 1970 */
int16_t	v7_updlock;                             /* lock for sync */
daddr_t	v7_rablock;                             /* block to be read ahead */
struct v7_inode *v7_mpxip;                      /* mpx virtual inode (unused) */

/* Root device block table */
static struct buf v7_rootdsktab = { 0 };

/* Block device switch table */
struct v7_bdevsw v7_bdevsw[] = {
    { .d_open = v7_dsk_open, .d_close = v7_dsk_close, .d_strategy = v7_dsk_strategy, .d_tab = &v7_rootdsktab },
    { NULL }
};

/* Character device switch table (alway empty) */
struct v7_cdevsw v7_cdevsw[1] = {
    { NULL }
};

/*
 * Replacements for various Unix v7 functions that either require significantly
 * different behavior or were originally written in PDP-11 assembly.
 */

void v7_spl0()
{
    /* no-op */
}

int16_t v7_spl6()
{
    /* no-op */
    return 0;
}

void v7_splx(int16_t s)
{
    /* no-op */
}

void v7_suword(uint16_t *n, int16_t a)
{
    *n = a;
}

void v7_mapfree(struct buf * bp)
{
    /* no-op */
}

void v7_readp(struct v7_file *fp)
{
    /* never called */
}

void v7_writep(struct v7_file *fp)
{
    /* never called */
}

void v7_plock(struct v7_inode *ip)
{
    /* never called */
}

void v7_xrele(struct v7_inode *ip)
{
    /* never called */
}

void v7_iomove(caddr_t cp, int16_t n, int16_t flag)
{
    if (flag==B_WRITE)
        memcpy(cp, u.u_base, n);
    else
        memcpy(u.u_base, cp, n);
    u.u_base += n;
    u.u_offset += n;
    u.u_count -= n;
}

int16_t v7_copyout(caddr_t s, caddr_t d, int16_t n)
{
    memcpy(d, s, n);
    return 0;
}

void v7_bcopy(caddr_t from, caddr_t to, int16_t count)
{
    memmove(to, from, (size_t)count);
}

int16_t v7_schar()
{
    if (u.u_dirp != NULL && *u.u_dirp != '\0')
        return *u.u_dirp++;
    u.u_dirp = NULL;
    return '\0';
}

int16_t v7_uchar()
{
    if (u.u_dirp != NULL && *u.u_dirp != '\0')
        return *u.u_dirp++;
    u.u_dirp = NULL;
    return '\0';
}

void v7_sleep(caddr_t chan, int16_t pri)
{
    /* no-op */
}

void v7_wakeup(caddr_t chan)
{
    /* no-op */
}

void v7_panic(const char * s)
{
    fprintf(stderr, "V7FS PANIC: %s\n", s);
    abort();
}

void v7_prdev(const char * str, int16_t dev)
{
    fprintf(stderr, "V7FS ERROR: %s\n", str);
}

void v7_printf(const char * str, ...)
{
    va_list ap;
    va_start(ap, str);
    vfprintf(stderr, str, ap);
    va_end(ap);
}

/*
 * Root device callback functions.
 */

static int16_t v7_dsk_open(dev_t dev, int16_t flag)
{
    /* no-op */
    return 0;
}

static int16_t v7_dsk_close(dev_t dev, int16_t flag)
{
    /* no-op */
    return 0;
}

static int16_t v7_dsk_strategy(struct v7_buf *bp)
{
    /* perform a synchronous read/write of the underlying device/image file
       by forwarding the operation to the dskio layer. */
    int ioRes;
    if ((bp->b_flags & B_READ) != 0)
        ioRes = dsk_read(bp->b_blkno, bp->b_un.b_addr, bp->b_bcount);
    else
        ioRes = dsk_write(bp->b_blkno, bp->b_un.b_addr, bp->b_bcount);
    if (ioRes != 0)
        bp->b_flags |= B_ERROR;

    /* refresh the kernel's notion of time in case the I/O took awhile. */
    v7_refreshclock();

    /* tell the v7 kernel that the I/O is complete. */
	v7_iodone(bp);

    return 0;
}

/*
 * Utility functions
 */

/** Clear kernel data structures and globals.
 */
void v7_zerocore()
{
    memset(&v7_u, 0, sizeof(v7_u));
    memset(v7_mount, 0, sizeof(v7_mount));
    memset(v7_inode, 0, sizeof(v7_inode));
    memset(v7_buf, 0, sizeof(v7_buf));
    memset(v7_file, 0, sizeof(v7_file));
    memset(&v7_bfreelist, 0, sizeof(v7_bfreelist));
    memset(&v7_time, 0, sizeof(v7_time));
    v7_rootdev = 0;
    v7_nblkdev = 0;
    v7_nchrdev = 0;
    v7_rootdir = NULL;
    v7_updlock = 0;
    v7_rablock = 0;
}

/** Decode the on-disk form of the filesystem's superblock
 */
void v7_decodesuperblock(v7_caddr_t srcbuf, struct v7_filsys * dest)
{
    if (v7_fsconfig.fstype == fs_type_v7) {
        struct v7_superblock * src = (struct v7_superblock *)srcbuf;
        memset(src, 0, sizeof(struct v7_superblock));
        dest->s_isize = fs_htopdp_u16(src->s_isize);
        dest->s_fsize = fs_htopdp_i32(src->s_fsize);
        dest->s_nfree = fs_htopdp_i16(src->s_nfree);
        for (int i = 0; i < V7_NICFREE; i++)
            dest->s_free[i] = fs_htopdp_i32(src->s_free[i]);
        dest->s_ninode = fs_htopdp_i16(src->s_ninode);
        for (int i = 0; i < V7_NICINOD; i++)
            dest->s_inode[i] = fs_htopdp_u16(src->s_inode[i]);
        dest->s_flock = src->s_flock;
        dest->s_ilock = src->s_ilock;
        dest->s_fmod = src->s_fmod;
        dest->s_ronly = src->s_ronly;
        dest->s_time = fs_htopdp_i32(src->s_time);
        dest->s_tfree = fs_htopdp_i32(src->s_tfree);
        dest->s_tinode = fs_htopdp_u16(src->s_tinode);
        dest->s_m = fs_htopdp_i16(src->s_m);
        dest->s_n = fs_htopdp_i16(src->s_n);
        memcpy(dest->s_fname, src->s_fname, sizeof(src->s_fname));
        memcpy(dest->s_fpack, src->s_fpack, sizeof(src->s_fpack));
    }
    else {
        /* v7_fsconfig.fstype set incorrectly */
        abort();
    }
}

/** Encode the on-disk form of the filesystem's superblock
 */
void v7_encodesuperblock(struct v7_filsys * src, v7_caddr_t destbuf)
{
    if (v7_fsconfig.fstype == fs_type_v7) {
        struct v7_superblock * dest = (struct v7_superblock *)destbuf;
        memset(dest, 0, sizeof(struct v7_superblock));
        dest->s_isize = fs_htopdp_u16(src->s_isize);
        dest->s_fsize = fs_htopdp_i32(src->s_fsize);
        dest->s_nfree = fs_htopdp_i16(src->s_nfree);
        for (int i = 0; i < V7_NICFREE; i++)
            dest->s_free[i] = fs_htopdp_i32(src->s_free[i]);
        dest->s_ninode = fs_htopdp_i16(src->s_ninode);
        for (int i = 0; i < V7_NICINOD; i++)
            dest->s_inode[i] = fs_htopdp_u16(src->s_inode[i]);
        dest->s_flock = src->s_flock;
        dest->s_ilock = src->s_ilock;
        dest->s_fmod = src->s_fmod;
        dest->s_ronly = src->s_ronly;
        dest->s_time = fs_htopdp_i32(src->s_time);
        dest->s_tfree = fs_htopdp_i32(src->s_tfree);
        dest->s_tinode = fs_htopdp_u16(src->s_tinode);
        dest->s_m = fs_htopdp_i16(src->s_m);
        dest->s_n = fs_htopdp_i16(src->s_n);
        memcpy(dest->s_fname, src->s_fname, sizeof(dest->s_fname));
        memcpy(dest->s_fpack, src->s_fpack, sizeof(dest->s_fpack));
    }
    else {
        /* v7_fsconfig.fstype set incorrectly */
        abort();
    }
}

int16_t v7_htofs_i16(int16_t v)
{
    switch (v7_fsconfig.byteorder) {
    case fs_byteorder_le:
        return fs_htole_i16(v);
    case fs_byteorder_be:
        return fs_htobe_i16(v);
    case fs_byteorder_pdp:
        return fs_htopdp_i16(v);
    default:
        /* v7_fsconfig.byteorder set incorrectly */
        abort();
    }
}

uint16_t v7_htofs_u16(uint16_t v)
{
    switch (v7_fsconfig.byteorder) {
    case fs_byteorder_le:
        return fs_htole_u16(v);
    case fs_byteorder_be:
        return fs_htobe_u16(v);
    case fs_byteorder_pdp:
        return fs_htopdp_u16(v);
    default:
        /* v7_fsconfig.byteorder set incorrectly */
        abort();
    }
}

int32_t v7_htofs_i32(int32_t v)
{
    switch (v7_fsconfig.byteorder) {
    case fs_byteorder_le:
        return fs_htole_i32(v);
    case fs_byteorder_be:
        return fs_htobe_i32(v);
    case fs_byteorder_pdp:
        return fs_htopdp_i32(v);
    default:
        /* v7_fsconfig.byteorder set incorrectly */
        abort();
    }
}

#undef time
#undef time_t
#include <time.h>

/** Refresh the kernel's notion of current time.
 */
void v7_refreshclock()
{
    v7_time = time(NULL);
}
