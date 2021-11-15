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

#include "bsd29adapt.h"
#include "bsd29/include/sys/param.h"
#include "bsd29/include/sys/buf.h"
#include "bsd29/include/sys/dir.h"
#include "bsd29/include/sys/user.h"
#include "bsd29/include/sys/conf.h"
#include "bsd29/include/sys/file.h"
#include "bsd29/include/sys/inode.h"
#include "bsd29/include/sys/filsys.h"
#include "bsd29/include/sys/mount.h"

static int16_t bsd29_dsk_open(dev_t dev, int16_t flag);
static int16_t bsd29_dsk_close(dev_t dev, int16_t flag);
static int16_t bsd29_dsk_strategy(struct buf *bp);
static void bsd29_dsk_root();


/* 2.9BSD global structures and values. */

struct bsd29_user bsd29_u;
struct bsd29_mount bsd29_mount[NMOUNT];
struct bsd29_file bsd29_file[NFILE];
struct bsd29_inode bsd29_inode[NINODE];
struct bsd29_mount *bsd29_mountNMOUNT = &bsd29_mount[NMOUNT];
struct bsd29_file *bsd29_fileNFILE = &bsd29_file[NFILE];
struct bsd29_inode *bsd29_inodeNINODE = &bsd29_inode[NINODE];
char bsd29_buffers[NBUF * (BSIZE+BSLOP)] __attribute__((aligned(4)));
struct bsd29_buf bsd29_buf[NBUF];
struct bsd29_buf bsd29_bfreelist;

dev_t bsd29_rootdev;                                /* root device number (always 0) */
int16_t bsd29_nblkdev = 1;                          /* number of entries in the block device switch table */
int16_t bsd29_nchrdev = 0;                          /* number of entries in the character device table (always 0) */
struct inode *bsd29_rootdir;                        /* pointer to inode of root directory */
bsd29_time_t bsd29_time;                            /* time in sec from 1970 */
int16_t bsd29_updlock;                              /* lock for sync */
daddr_t bsd29_rablock;                              /* block to be read ahead */
int16_t bsd29_nmount = NMOUNT;                      /* number of mountable file systems */
int16_t	bsd29_nbuf = NBUF;                          /* number of buffers in buffer cache */
int16_t	bsd29_bsize = BSIZE + BSLOP;                /* size of buffers */

/* Root device block table */
static struct buf bsd29_rootdsktab = { 0 };

/* Block device switch table */
struct bsd29_bdevsw bsd29_bdevsw[] = {
    {
        .d_open = bsd29_dsk_open, 
        .d_close = bsd29_dsk_close,
        .d_strategy = bsd29_dsk_strategy,
        .d_root = bsd29_dsk_root,
        .d_tab = &bsd29_rootdsktab
    },
    { NULL }
};

/* Character device switch table (always empty) */
struct bsd29_cdevsw bsd29_cdevsw[1] = {
    { NULL }
};

/*
 * Replacements for various 2.9BSD functions that either require significantly
 * different behavior or were originally written in PDP-11 assembly.
 */

bsd29_caddr_t bsd29_SEG5;

bsd29_caddr_t bsd29_mapin(struct bsd29_buf *bp)
{
    SEG5 = bp->b_un.b_addr;
    return bp->b_un.b_addr;
}

void bsd29_mapout(struct bsd29_buf *bp)
{
    SEG5 = NULL;
}

void bsd29_spl0()
{
    /* no-op */
}

int16_t bsd29_spl6()
{
    /* no-op */
    return 0;
}

void bsd29_readp(struct bsd29_file *fp)
{
    /* never called */
}

void bsd29_writep(struct bsd29_file *fp)
{
    /* never called */
}

void bsd29_xrele(struct bsd29_inode *ip)
{
    /* never called */
}

void bsd29_iomove(caddr_t cp, int16_t n, int16_t flag)
{
    if (flag==B_WRITE)
        memcpy(cp, bsd29_u.u_base, n);
    else
        memcpy(u.u_base, cp, n);
    bsd29_u.u_base += n;
    bsd29_u.u_offset += n;
    bsd29_u.u_count -= n;
}

int16_t bsd29_copyout(caddr_t s, caddr_t d, int16_t n)
{
    memcpy(d, s, n);
    return 0;
}

int16_t bsd29_fubyte(const void *base)
{
    return *(const int8_t *)base;
}

void bsd29_bcopy(caddr_t from, caddr_t to, int16_t count)
{
    memmove(to, from, (size_t)count);
}

int16_t bsd29_schar()
{
    if (bsd29_u.u_dirp != NULL && *bsd29_u.u_dirp != '\0')
        return *bsd29_u.u_dirp++;
    bsd29_u.u_dirp = NULL;
    return '\0';
}

int16_t bsd29_uchar()
{
    if (bsd29_u.u_dirp != NULL && *bsd29_u.u_dirp != '\0')
        return *bsd29_u.u_dirp++;
    bsd29_u.u_dirp = NULL;
    return '\0';
}

void bsd29_sleep(caddr_t chan, int16_t pri)
{
    /* no-op */
}

void bsd29_wakeup(caddr_t chan)
{
    /* no-op */
}

void bsd29_panic(const char * s)
{
    fprintf(stderr, "BSD29FS PANIC: %s\n", s);
    abort();
}

void
bsd29_tablefull(char *tab)
{
	fprintf(stderr, "BSD29FS ERROR: %s: table is full\n", tab);
}

void bsd29_fserr(struct filsys *fp, char *cp)
{
	fprintf(stderr, "BSD29FS ERROR: %s: %s\n", fp->s_fsmnt, cp);
}

void bsd29_printf(const char * str, ...)
{
    va_list ap;
    va_start(ap, str);
    vfprintf(stderr, str, ap);
    va_end(ap);
}

void bsd29_uprintf(const char * str, ...)
{
    va_list ap;
    va_start(ap, str);
    vfprintf(stderr, str, ap);
    va_end(ap);
}

/*
 * Root device callback functions.
 */

static int16_t bsd29_dsk_open(dev_t dev, int16_t flag)
{
    /* no-op */
    return 0;
}

static int16_t bsd29_dsk_close(dev_t dev, int16_t flag)
{
    /* no-op */
    return 0;
}

static int16_t bsd29_dsk_strategy(struct bsd29_buf *bp)
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
    bsd29_refreshclock();

    /* tell the kernel that the I/O is complete. */
	bsd29_iodone(bp);

    return 0;
}

static void bsd29_dsk_root()
{
    /* no-op */
}

/*
 * Utility functions
 */

/** Clear kernel data structures and globals.
 */
void bsd29_zerocore()
{
    memset(&bsd29_u, 0, sizeof(bsd29_u));
    memset(bsd29_mount, 0, sizeof(bsd29_mount));
    memset(bsd29_file, 0, sizeof(bsd29_file));
    memset(bsd29_inode, 0, sizeof(bsd29_inode));
    memset(bsd29_buffers, 0, sizeof(bsd29_buffers));
    memset(bsd29_buf, 0, sizeof(bsd29_buf));
    memset(&bsd29_bfreelist, 0, sizeof(bsd29_bfreelist));
    bsd29_rootdev = 0;
    bsd29_rootdir = NULL;
    bsd29_time = 0;
    bsd29_updlock = 0;
    bsd29_rablock = 0;
    memset(&bsd29_rootdsktab, 0, sizeof(bsd29_rootdsktab));
}

#undef time
#undef time_t
#include <time.h>

/** Refresh the kernel's notion of current time.
 */
void bsd29_refreshclock()
{
    bsd29_time = time(NULL);
}
