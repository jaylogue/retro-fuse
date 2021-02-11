#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/sysmacros.h>

#include "v6adapt.h"
#include "dskio.h"

#include "param.h"
#include "user.h"
#include "buf.h"
#include "systm.h"
#include "inode.h"
#include "file.h"
#include "conf.h"

static void v6fs_dsk_open(int16_t dev, int16_t flag);
static void v6fs_dsk_close(int16_t dev, int16_t flag);
static void v6fs_dsk_strategy(struct buf *bp);

struct v6_user v6_u;
struct v6_mount v6_mount[NMOUNT];
struct v6_inode v6_inode[NINODE];
struct v6_buf v6_buf[NBUF];
struct v6_file v6_file[NFILE];
struct v6_buf v6_bfreelist;

int16_t	v6_rootdev;         /* root device number (always 0) */
int16_t	v6_nblkdev;         /* number of entries in the block device table */
int16_t	v6_nchrdev;         /* number of entries in the character device table */
struct inode *v6_rootdir;   /* pointer to root directory inode */
int16_t	v6_time[2];         /* time in sec from 1970 */
int16_t	v6_updlock;         /* lock for sync */
int16_t	v6_rablock;         /* block to be read ahead */
struct integ v6_PS;         /* dummy processor status word */

static struct v6_devtab v6fs_rootdsktab = { 0 };

struct v6_bdevsw v6_bdevsw[] = {
    { .d_open = v6fs_dsk_open, .d_close = v6fs_dsk_close, .d_strategy = v6fs_dsk_strategy, .d_tab = &v6fs_rootdsktab },
    { NULL }
};

struct v6_cdevsw v6_cdevsw[1] = {
    { NULL }
};

/**
 * Initialize the Unix v6 kernel.
 * 
 * This function is analogous to the v6 main() routine.
 */
void v6_init_kernel()
{
    /* zero various kernel data structures and globals */
    memset(&v6_u, 0, sizeof(v6_u));
    memset(v6_mount, 0, sizeof(v6_mount));
    memset(v6_inode, 0, sizeof(v6_inode));
    memset(v6_buf, 0, sizeof(v6_buf));
    memset(v6_file, 0, sizeof(v6_file));
    memset(&v6_bfreelist, 0, sizeof(v6_bfreelist));
    v6_updlock = 0;
    v6_rablock = 0;
    v6_nchrdev = 0;
    v6_nblkdev = 0;

    /* set the device id for the root device. */
    v6_rootdev = 0;

    /* initialize the buffer pool. */
    v6_binit();

    /* mount the root device and read the superblock. */
    v6_iinit();

    /* get the root directory inode. */
    v6_rootdir = v6_iget(v6_rootdev, ROOTINO);
    v6_rootdir->i_flag &= ~ILOCK;

    /* set the user's current directory. */
    v6_u.u_cdir = v6_rootdir;
}

/**
 * Unix block device open function
 */
static void v6fs_dsk_open(int16_t dev, int16_t flag)
{
    /* no-op */
}

/**
 * Unix block device close function
 */
static void v6fs_dsk_close(int16_t dev, int16_t flag)
{
    /* no-op */
}

/** Unix block device strategy function
 * 
 * Called by v6 kernel to read/write a block in the filesystem.
 */
static void v6fs_dsk_strategy(struct buf *bp)
{
    int ioRes;
    if ((bp->b_flags&B_READ) != 0)
        ioRes = dsk_read(bp->b_blkno, bp->b_addr, -bp->b_wcount * 2);
    else
        ioRes = dsk_write(bp->b_blkno, bp->b_addr, -bp->b_wcount * 2);
    if (!ioRes)
    {
        bp->b_flags |= B_ERROR;
    }
	v6_iodone(bp);
}

void v6_spl0()
{
    /* no-op */
}

void v6_spl6()
{
    /* no-op */
}

void v6_suword(uint16_t *n, int16_t a)
{
    *n = a;
}

void v6_mapfree(struct buf * bp)
{
    /* no-op */
}

void v6_readp(struct v6_file *fp)
{
    /* never called */
}

void v6_writep(struct v6_file *fp)
{
    /* never called */
}

int16_t v6_ldiv(uint16_t n, uint16_t b)
{
    return (int16_t)(n / b);
}

int16_t v6_lrem(uint16_t n, uint16_t b)
{
    return (int16_t)(n % b);
}

int16_t v6_lshift(uint16_t *n, int16_t s)
{
    uint32_t n32 = ((uint32_t)n[0]) << 16 | n[1];
    n32 = (s >= 0) ? n32 << s : n32 >> -s;
    return (int16_t)n32;
}

void v6_dpadd(uint16_t *n, int16_t a)
{
    uint32_t n32 = ((uint32_t)n[0]) << 16 | n[1];
    n32 += a;
    n[0] = (uint16_t)(n32 >> 16);
    n[1] = (uint16_t)n32;
}

int16_t v6_dpcmp(uint16_t ah, uint16_t al, uint16_t bh, uint16_t bl)
{
    int32_t a = ((int32_t)ah) << 16 | al;
    int32_t b = ((int32_t)bh) << 16 | bl;
    int32_t d = a - b;
    if (d < -512)
        d = -512;
    else if (d > 512)
        d = 512;
    return (int16_t)d;
}

void v6_iomove(struct buf *bp, int16_t o, int16_t an, int16_t flag)
{
    if (flag==B_WRITE)
        memcpy(bp->b_addr + o, u.u_base, an);
    else
        memcpy(u.u_base, bp->b_addr + o, an);
    u.u_base += an;
    v6_dpadd(u.u_offset, an);
    u.u_count -= an;
}

void v6_bcopy(void * from, void * to, int16_t count)
{
    memmove(to, from, (size_t)count * 2);
}

int16_t v6_schar()
{
    if (u.u_dirp != NULL && *u.u_dirp != '\0')
        return *u.u_dirp++;
    u.u_dirp = NULL;
    return '\0';
}

int16_t v6_uchar()
{
    if (u.u_dirp != NULL && *u.u_dirp != '\0')
        return *u.u_dirp++;
    u.u_dirp = NULL;
    return '\0';
}

void v6_sleep(void * chan, int16_t pri)
{
    /* no-op */
}

void v6_wakeup(void * chan)
{
    /* no-op */
}

void v6_panic(const char * s)
{
    // TODO: implement me
}

void v6_prdev(const char * str, int16_t dev)
{
    // TODO: implement me
}

void v6_printf(const char * str, ...)
{

}