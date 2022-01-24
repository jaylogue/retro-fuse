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

#include "bsd211adapt.h"
#include "bsd211/h/param.h"
#include "bsd211/h/buf.h"
#include "bsd211/h/dir.h"
#include "bsd211/h/user.h"
#include "bsd211/h/conf.h"
#include "bsd211/h/file.h"
#include "bsd211/h/inode.h"
#include "bsd211/h/fs.h"
#include "bsd211/h/mount.h"
#include "bsd211/h/namei.h"
#include "bsd211/h/uio.h"
#include "bsd211/h/kernel.h"

static int16_t bsd211_dsk_strategy(struct buf *bp);


/* 2.11BSD global structures and values.
 */
struct bsd211_user bsd211_u;
struct bsd211_mount bsd211_mount[NMOUNT];
struct bsd211_mount *bsd211_mountNMOUNT = &bsd211_mount[NMOUNT];
struct bsd211_file bsd211_file[NFILE];
struct bsd211_file *bsd211_fileNFILE = &bsd211_file[NFILE];
struct bsd211_inode bsd211_inode[NINODE];
struct bsd211_inode *bsd211_inodeNINODE = &bsd211_inode[NINODE];
char bsd211_buffers[NBUF][MAXBSIZE] __attribute__((aligned(4)));
struct bsd211_buf bsd211_buf[NBUF];
struct bsd211_buf bsd211_bfreelist[BQUEUES];
struct bsd211_bufhd bsd211_bufhash[BUFHSZ];
struct bsd211_namecache bsd211_namecachebuf[8192 / sizeof(struct bsd211_namecache)];
struct bsd211_namecache * bsd211_namecache = bsd211_namecachebuf;
bsd211_segm bsd211_nmidesc;                          /* dummy segement register pair for namecache */

dev_t bsd211_rootdev;                                /* root device number (always 0) */
int16_t bsd211_nblkdev = 1;                          /* number of entries in the block device switch table */
int16_t bsd211_nchrdev = 0;                          /* number of entries in the character device table (always 0) */
struct inode *bsd211_rootdir;                        /* pointer to inode of root directory */
struct bsd211_timeval bsd211_time;                   /* current time */
int16_t bsd211_updlock;                              /* lock for sync */
daddr_t bsd211_rablock;                              /* block to be read ahead */
int16_t bsd211_nmount = NMOUNT;                      /* number of mountable file systems */
int16_t bsd211_nbuf = NBUF;                          /* number of buffers in buffer cache */
int16_t bsd211_ninode = NINODE;                      /* number of inodes in in-memory inode table */
bsd211_caddr_t bsd211_SEG5;                          /* address currently mapped to segment 5 */
int16_t bsd211_securelevel = -1;                     /* system security level (-1 = permannently insecure) */
uint16_t bsd211_nextinodeid;                         /* inode cache unique id generator */
int16_t bsd211_nchsize =                             /* number of entries in namecache */
    sizeof(bsd211_namecachebuf) / sizeof(bsd211_namecachebuf[0]);

/* Block device switch table
 */
struct bsd211_bdevsw bsd211_bdevsw[] = {
    {
        .d_open = NULL, 
        .d_close = NULL,
        .d_strategy = bsd211_dsk_strategy,
        .d_root = NULL,
        .d_psize = NULL,
        .d_flags = 0
    },
    { NULL }
};

/* Character device switch table (always empty)
 */
struct bsd211_cdevsw bsd211_cdevsw[] = {
    { NULL }
};

/* Root device I/O function.
 */
static int16_t bsd211_dsk_strategy(struct bsd211_buf *bp)
{
    /* perform a synchronous read/write of the underlying device/image file
       by forwarding the operation to the dskio layer. */
    int ioRes;
    if ((bp->b_flags & B_READ) != 0)
        ioRes = dsk_read(bp->b_blkno, bp->b_un.b_addr, bp->b_bcount);
    else
        ioRes = dsk_write(bp->b_blkno, bp->b_un.b_addr, bp->b_bcount);
    if (ioRes != 0) {
        bp->b_error = ioRes;
        bp->b_flags |= B_ERROR;
    }

    /* refresh the kernel's notion of time in case the I/O took a while. */
    bsd211_refreshclock();

    /* tell the kernel that the I/O is complete. */
    bsd211_biodone(bp);

    return 0;
}


/*
 * Replacements for various 2.11BSD functions that either require significantly
 * different behavior or were originally written in PDP-11 assembly.
 */

/****** bcmp.s ******/

int16_t bsd211_bcmp(bsd211_caddr_t b1, bsd211_caddr_t b2, int16_t length)
{
    return memcmp(b1, b2, (size_t)length);
}

/****** bcopy.s ******/

void bsd211_bcopy(bsd211_caddr_t from, bsd211_caddr_t to, int16_t count)
{
    memmove(to, from, (size_t)count);
}

/****** bzero.s ******/

void bsd211_bzero(bsd211_caddr_t b, uint16_t length)
{
    memset(b, 0, (size_t)length);
}

/****** init_main.c ******/

void bsd211_bhinit()
{
    int i;
    struct bsd211_bufhd *bp;

    for (bp = bsd211_bufhash, i = 0; i < BUFHSZ; i++, bp++)
        bp->b_forw = bp->b_back = (struct bsd211_buf *)bp;
}

void bsd211_binit()
{
    struct bsd211_buf *bp;
    int i;

    for (bp = bsd211_bfreelist; bp < &bsd211_bfreelist[BQUEUES]; bp++)
        bp->b_forw = bp->b_back = bp->av_forw = bp->av_back = bp;
    for (i = 0; i < bsd211_nbuf; i++) {
        bp = &bsd211_buf[i];
        bp->b_dev = NODEV;
        bp->b_bcount = 0;
        bp->b_un.b_addr = (bsd211_caddr_t)bsd211_buffers[i];
        binshash(bp, &bsd211_bfreelist[BQ_AGE]);
        bp->b_flags = B_BUSY|B_INVAL;
        bsd211_brelse(bp);
    }
}

/****** insque.s ******/

void bsd211_insque(void * _elem, void * _pred)
{
    struct qelem
    {
        struct qelem * q_forw;
        struct qelem * q_back;
        char q_data[];
    };
    struct qelem * elem = (struct qelem *)_elem;
    struct qelem * pred = (struct qelem *)_pred;

    elem->q_back = pred;
    elem->q_forw = pred->q_forw;
    pred->q_forw = elem;
    elem->q_forw->q_back = elem;
}

/****** kern_prot.c ******/

int16_t bsd211_groupmember(bsd211_gid_t gid)
{
    bsd211_gid_t *gp;

    for (gp = bsd211_u.u_groups; gp < &bsd211_u.u_groups[NGROUPS] && *gp != NOGROUP; gp++)
        if (*gp == gid)
            return 1;
    return 0;
}

/****** kern_synch.c ******/

void bsd211_sleep(bsd211_caddr_t chan, int16_t pri)
{
    /* no-op */
}

void bsd211_wakeup(bsd211_caddr_t chan)
{
    /* no-op */
}

/****** kern_subr.c ******/

int16_t bsd211_uiomove(bsd211_caddr_t cp, uint16_t n, struct bsd211_uio *uio)
{
    while (n > 0 && uio->uio_resid) {
        struct bsd211_iovec *iov = uio->uio_iov;
        uint16_t cnt = iov->iov_len;
        if (cnt == 0) {
            uio->uio_iov++;
            uio->uio_iovcnt--;
            continue;
        }
        if (cnt > n)
            cnt = n;
        if (uio->uio_rw == UIO_READ)
            memmove(iov->iov_base, cp, cnt);
        else
            memmove(cp, iov->iov_base, cnt);
        iov->iov_base += cnt;
        iov->iov_len -= cnt;
        uio->uio_resid -= cnt;
        uio->uio_offset += cnt;
        cp += cnt;
        n -= cnt;
    }
    return 0;
}

/****** mch_copy.s ******/

int16_t bsd211_fubyte(bsd211_caddr_t base)
{
    return *(const int8_t *)base;
}

int16_t bsd211_copyin(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t length)
{
    memcpy(toaddr, fromaddr, length);
    return 0;
}

int16_t bsd211_copyout(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t length)
{
    memcpy(toaddr, fromaddr, length);
    return 0;
}

int16_t bsd211_copyinstr(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t maxlength, uint16_t *lencopied)
{
    return bsd211_copystr(fromaddr, toaddr, maxlength, lencopied);
}

/****** mch_xxx.s ******/

void bsd211_clrbuf(struct bsd211_buf *bp)
{
    memset(bp->b_un.b_addr, 0, MAXBSIZE);
}

bsd211_caddr_t bsd211_mapin(struct bsd211_buf * bp)
{
    return bp->b_un.b_addr;
}

void bsd211_mapout(struct bsd211_buf * bp)
{
    /* no-op */
}

int16_t bsd211_copystr(bsd211_caddr_t fromaddr, bsd211_caddr_t toaddr, uint16_t maxlength, uint16_t *lencopied)
{
    const char * from = (const char *)fromaddr;
    char * to = (char *)toaddr;

    while (maxlength > 0) {
        char ch = (*to++ = *from++);
        if (ch == 0) {
            if (lencopied != NULL)
                *lencopied = (uint16_t)(to - (char *)toaddr);
            return 0;
        }
        maxlength--;
    }
    return ENOENT;
}

void bsd211_vattr_null(struct bsd211_vattr *vp)
{
    vp->va_mode = VNOVAL;
    vp->va_uid = VNOVAL;
    vp->va_gid = VNOVAL;
    vp->va_size = VNOVAL;
    vp->va_atime = VNOVAL;
    vp->va_mtime = VNOVAL;
    vp->va_flags = (bsd211_u_short)VNOVAL;
    vp->va_vaflags = 0;
}

/****** mem_clicks.s ******/

void bsd211_copy(bsd211_caddr_t src, bsd211_caddr_t dest, int16_t len)
{
    memmove(dest, src, (size_t)ctob(len));
}

/****** psignal.c ******/

void bsd211_psignal(struct bsd211_proc *p, int16_t sig)
{
    /* no-op */
}

/****** remque.s ******/

void bsd211_remque(void * _elem)
{
    struct qelem
    {
        struct qelem * q_forw;
        struct qelem * q_back;
        char q_data[];
    } * elem = (struct qelem *)_elem;

    struct qelem * fw = elem->q_forw;
    struct qelem * bk = elem->q_back;
    elem->q_back->q_forw = fw;
    elem->q_forw->q_back = bk;
}

/****** subr_prf.c ******/

void bsd211_printf(const char * str, ...)
{
    va_list ap;
    va_start(ap, str);
    vfprintf(stderr, str, ap);
    va_end(ap);
}

void bsd211_uprintf(const char * str, ...)
{
    va_list ap;
    va_start(ap, str);
    vfprintf(stderr, str, ap);
    va_end(ap);
}

void bsd211_log(int16_t level, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

void bsd211_panic(const char * s)
{
    fprintf(stderr, "BSD211FS PANIC: %s\n", s);
    abort();
}

void bsd211_tablefull(char *tab)
{
    fprintf(stderr, "BSD211FS ERROR: %s: table is full\n", tab);
}

/****** strlen.s ******/

bsd211_size_t bsd211_strlen(const char *s)
{
#undef strlen
    return strlen(s);
}

/****** ufs_alloc.c ******/

void bsd211_fserr(struct fs *fp, char *cp)
{
    fprintf(stderr, "BSD211FS ERROR: %s\n", cp);
}

/*
 * Utility functions
 */

/** Clear kernel data structures and globals.
 */
void bsd211_zerocore()
{
    memset(&bsd211_u, 0, sizeof(bsd211_u));
    memset(bsd211_mount, 0, sizeof(bsd211_mount));
    memset(bsd211_file, 0, sizeof(bsd211_file));
    memset(bsd211_inode, 0, sizeof(bsd211_inode));
    memset(bsd211_buffers, 0, sizeof(bsd211_buffers));
    memset(bsd211_buf, 0, sizeof(bsd211_buf));
    memset(&bsd211_bfreelist, 0, sizeof(bsd211_bfreelist));
    memset(&bsd211_bufhash, 0, sizeof(bsd211_bufhash));
    memset(bsd211_namecachebuf, 0, sizeof(bsd211_namecachebuf));
    memset(&bsd211_nmidesc, 0, sizeof(bsd211_nmidesc));
    memset(&bsd211_time, 0, sizeof(bsd211_time));
    bsd211_rootdev = 0;
    bsd211_rootdir = NULL;
    bsd211_updlock = 0;
    bsd211_rablock = 0;
    bsd211_SEG5 = NULL;
    bsd211_nextinodeid = 0;
}

#undef time
#undef time_t
#undef timespec
#undef timeval
#include <time.h>

/** Refresh the kernel's notion of current time.
 */
void bsd211_refreshclock()
{
    bsd211_time.tv_sec = time(NULL);
}
