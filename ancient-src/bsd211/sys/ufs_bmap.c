#include "bsd211adapt.h"

/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)ufs_bmap.c	1.2 (2.11BSD) 1996/9/19
 */

#include "bsd211/h/param.h"
#include "bsd211/machine/seg.h"

#include "bsd211/h/systm.h"
#include "bsd211/h/conf.h"
#include "bsd211/h/dir.h"
#include "bsd211/h/inode.h"
#include "bsd211/h/user.h"
#include "bsd211/h/buf.h"
#include "bsd211/h/fs.h"
#include "bsd211/h/mount.h"
#include "bsd211/h/uio.h"

/*
 * Bmap defines the structure of file system storage
 * by returning the physical block number on a device given the
 * inode and the logical block number in a file.
 * When convenient, it also leaves the physical
 * block number of the next block of the file in rablock
 * for use in read-ahead.
 */
daddr_t
bmap(ip, bn, rwflg, flags)
	register struct inode *ip;
	daddr_t bn;
	int16_t rwflg, flags;
{
	register int16_t i;
	register struct buf *bp;
	struct buf *nbp;
	int16_t j, sh;
	daddr_t nb, *bap, ra;
	int16_t async = ip->i_fs->fs_flags & MNT_ASYNC;

	if (bn < 0) {
		u.u_error = EFBIG;
		return((daddr_t)0);
	}
	ra = rablock = 0;

	/*
	 * blocks 0..NADDR-4 are direct blocks
	 */
	if (bn < NADDR-3) {
		i = bn;
		nb = ip->i_addr[i];
		if (nb == 0) {
			if (rwflg == B_READ || (bp = balloc(ip, flags)) == NULL)
				return((daddr_t)-1);
			nb = dbtofsb(bp->b_blkno);
/*
 * directory blocks are usually the only thing written synchronously at this
 * point (so they never appear with garbage in them on the disk).  This is
 * overridden if the filesystem was mounted 'async'.
*/
			if (flags & B_SYNC)
				bwrite(bp);
			else
				bdwrite(bp);
			ip->i_addr[i] = nb;
			ip->i_flag |= IUPD|ICHG;
		}
		if (i < NADDR-4)
			rablock = ip->i_addr[i+1];
		return(nb);
	}

	/*
	 * addresses NADDR-3, NADDR-2, and NADDR-1
	 * have single, double, triple indirect blocks.
	 * the first step is to determine
	 * how many levels of indirection.
	 */
	sh = 0;
	nb = 1;
	bn -= NADDR-3;
	for (j = 3;j > 0;j--) {
		sh += NSHIFT;
		nb <<= NSHIFT;
		if (bn < nb)
			break;
		bn -= nb;
	}
	if (j == 0) {
		u.u_error = EFBIG;
		return((daddr_t)0);
	}

	/*
	 * fetch the first indirect block
	 */
	nb = ip->i_addr[NADDR-j];
	if (nb == 0) {
		if (rwflg == B_READ || (bp = balloc(ip, flags | B_CLRBUF)) == NULL)
			return((daddr_t) -1);
		nb = dbtofsb(bp->b_blkno);
		/*
		 * Write synchronously if requested so that indirect blocks
		 * never point at garbage.
		 */
		if (async)
			bdwrite(bp);
		else
			bwrite(bp);
		ip->i_addr[NADDR-j] = nb;
		ip->i_flag |= IUPD|ICHG;
	}

	/*
	 * fetch through the indirect blocks
	 */
	for(;j <= 3;j++) {
		bp = bread(ip->i_dev, nb);
		if ((bp->b_flags & B_ERROR) || bp->b_resid) {
			brelse(bp);
			return((daddr_t)0);
		}
		bap = (daddr_t *) mapin(bp);
		sh -= NSHIFT;
		i = (bn>>sh) & NMASK;
		nb = wswap_int32(bap[i]);
		/*
		 * calculate read-ahead
		 */
		if (i < NINDIR-1)
			ra = wswap_int32(bap[i+1]);
		mapout(bp);
		if (nb == 0) {
			if (rwflg == B_READ || (nbp = balloc(ip, flags | B_CLRBUF)) == NULL) {
				brelse(bp);
				return((daddr_t) -1);
			}
			nb = dbtofsb(nbp->b_blkno);
/*
 * Write synchronously so indirect blocks never point at garbage and blocks
 * in directories never contain garbage.  This check used to be based on the
 * type of inode, if it was a directory then 'sync' writes were done.  See the
 * comments earlier about filesystems being mounted 'async'.
*/
			if (!async && (j < 3 || (flags & B_SYNC)))
				bwrite(nbp);
			else
				bdwrite(nbp);
			bap = (daddr_t *) mapin(bp);
			bap[i] = wswap_int32(nb);
			mapout(bp);
			bdwrite(bp);
		} else
			brelse(bp);
	}
	rablock = ra;
	return(nb);
}
