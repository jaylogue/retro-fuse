#include "bsd29adapt.h"

/*
 *	SCCS id	@(#)subr.c	2.1 (Berkeley)	9/4/83
 */

#include "bsd29/include/sys/param.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/conf.h>
#include <bsd29/include/sys/inode.h>
#include <bsd29/include/sys/dir.h>
#include <bsd29/include/sys/user.h>
#include <bsd29/include/sys/buf.h>
/* UNUSED #include <sys/quota.h> */

#ifndef	UCB_QUOTAS
#define	qalloc(x,y)	alloc(y)
#endif

/*
 * Bmap defines the structure of file system storage
 * by returning the physical block number on a device given the
 * inode and the logical block number in a file.
 * When convenient, it also leaves the physical
 * block number of the next block of the file in rablock
 * for use in read-ahead.
 */
daddr_t
bmap(struct inode *ip, daddr_t bn, int16_t rwflg)
{
	register int16_t i;
	register struct buf *bp, *nbp;
	int16_t j, sh;
	daddr_t nb, *bap, ra;
	dev_t dev;

	if(bn < 0) {
		u.u_error = EFBIG;
		return((daddr_t)0);
	}
	dev = ip->i_dev;
	ra = rablock = 0;

	/*
	 * blocks 0..NADDR-4 are direct blocks
	 */
	if(bn < NADDR-3) {
		i = bn;
		nb = ip->i_un.i_addr[i];
		if (nb == 0) {
			if (rwflg == B_READ || (bp = qalloc(ip, dev)) == NULL)
				return((daddr_t)-1);
			nb = dbtofsb(bp->b_blkno);
#ifdef UCB_FSFIX
			if ((ip->i_mode&IFMT) == IFDIR)
				/*
				 * Write directory blocks synchronously
				 * so they never appear with garbage in
				 * them on the disk.
				 */
				bwrite(bp);
			else
#endif
				bdwrite(bp);
			ip->i_un.i_addr[i] = nb;
			ip->i_flag |= IUPD|ICHG;
		}
		if(i < NADDR-4)
			rablock = ip->i_un.i_addr[i+1];
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
	for(j=3; j>0; j--) {
		sh += NSHIFT;
		nb <<= NSHIFT;
		if(bn < nb)
			break;
		bn -= nb;
	}
	if(j == 0) {
		u.u_error = EFBIG;
		return((daddr_t)0);
	}

	/*
	 * fetch the first indirect block
	 */
	nb = ip->i_un.i_addr[NADDR-j];
	if(nb == 0) {
		if (rwflg == B_READ || (bp = qalloc(ip, dev)) == NULL)
			return((daddr_t) -1);
		nb = dbtofsb(bp->b_blkno);
#ifdef UCB_FSFIX
		/*
		 * Write synchronously so that indirect blocks
		 * never point at garbage.
		 */
		bwrite(bp);
#else
		bdwrite(bp);
#endif
		ip->i_un.i_addr[NADDR-j] = nb;
		ip->i_flag |= IUPD|ICHG;
	}

	/*
	 * fetch through the indirect blocks
	 */
	for(; j<=3; j++) {
		bp = bread(dev, nb);
		if((bp->b_flags & B_ERROR) || bp->b_resid) {
			brelse(bp);
			return((daddr_t)0);
		}
		bap = (daddr_t *) mapin(bp);
		sh -= NSHIFT;
		i = (bn>>sh) & NMASK;
		nb = bap[i];
		/*
		 * calculate read-ahead
		 */
		if(i < NINDIR-1)
			ra = bap[i+1];
		mapout(bp);
		if(nb == 0) {
			if(rwflg == B_READ || (nbp = qalloc(ip, dev)) == NULL) {
				brelse(bp);
				return((daddr_t) -1);
			}
			nb = dbtofsb(nbp->b_blkno);
#ifdef UCB_FSFIX
			if (j < 3 || (ip->i_mode&IFMT) == IFDIR)
				/*
				 * Write synchronously so indirect blocks
				 * never point at garbage and blocks
				 * in directories never contain garbage.
				 */
				bwrite(nbp);
			else
#endif
				bdwrite(nbp);
			bap = (daddr_t *) mapin(bp);
			bap[i] = nb;
			mapout(bp);
			bdwrite(bp);
		} else
			brelse(bp);
	}

	rablock = ra;
	return(nb);
}

#if UNUSED
/*
 * Pass back  c  to the user at his location u_base;
 * update u_base, u_count, and u_offset.  Return -1
 * on the last character of the user's read.
 * u_base is in the user address space unless u_segflg is 1.
 */
passc(c)
register c;
{
	register id;

	if((id = u.u_segflg) == 1)
		*u.u_base = c;
	else
		if(id?suibyte(u.u_base, c):subyte(u.u_base, c) < 0) {
			u.u_error = EFAULT;
			return(-1);
		}
	u.u_count--;
	u.u_offset++;
	u.u_base++;
	return(u.u_count == 0? -1: 0);
}
#endif /* UNUSED */

#if UNUSED
/*
 * Pick up and return the next character from the user's
 * write call at location u_base;
 * update u_base, u_count, and u_offset.  Return -1
 * when u_count is exhausted.  u_base is in the user's
 * address space unless u_segflg is 1.
 */
cpass()
{
	register c, id;

	if(u.u_count == 0)
		return(-1);
	if((id = u.u_segflg) == 1)
		c = *u.u_base;
	else
		if((c = id==0?fubyte(u.u_base):fuibyte(u.u_base)) < 0) {
			u.u_error = EFAULT;
			return(-1);
		}
	u.u_count--;
	u.u_offset++;
	u.u_base++;
	return(c&0377);
}
#endif /* UNUSED */

#if UNUSED
/*
 * Routine which sets a user error; placed in
 * illegal entries in the bdevsw and cdevsw tables.
 */
nodev()
{

	u.u_error = ENODEV;
}
#endif /* UNUSED */

#if UNUSED
/*
 * copy count bytes from from to to.
 */
bcopy(from, to, count)
register caddr_t from, to;
register count;
{
	if ((from|to|count) & 1)	 /* copy by bytes */
		while (count--)
			*to++ = *from++;
	else {				/* copy by words */
		count >>= 1;
		while (count--)
			*((short *)to)++ = *((short *)from)++;
	}
}
#endif /* UNUSED */
