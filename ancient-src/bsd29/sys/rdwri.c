#include "bsd29adapt.h"

/*
 *	SCCS id	@(#)rdwri.c	2.1 (Berkeley)	8/5/83
 */

#include "bsd29/include/sys/param.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/inode.h>
#include <bsd29/include/sys/dir.h>
#include <bsd29/include/sys/user.h>
#include <bsd29/include/sys/buf.h>
#include <bsd29/include/sys/conf.h>

/*
 * Read the file corresponding to
 * the inode pointed at by the argument.
 * The actual read arguments are found
 * in the variables:
 *	u_base		core address for destination
 *	u_offset	byte offset in file
 *	u_count		number of bytes to read
 *	u_segflg	read to kernel/user/user I
 */
void
readi(register struct inode *ip)
{
	struct buf *bp;
	dev_t dev;
	daddr_t lbn, bn;
	off_t diff;
	register int16_t on, n;
	int16_t type;

	if(u.u_count == 0)
		return;
	if(u.u_offset < 0) {
		u.u_error = EINVAL;
		return;
	}
	ip->i_flag |= IACC;
	dev = (dev_t)ip->i_un.i_rdev;
	type = ip->i_mode&IFMT;
#ifdef	MPX_FILS
	if (type==IFCHR || type==IFMPC)
#else
	if (type==IFCHR)
#endif
		return; /* UNUSED ((*cdevsw[major(dev)].d_read)(dev)); */

	do {
		lbn = bn = u.u_offset >> BSHIFT;
		on = u.u_offset & BMASK;
		n = MIN((unsigned)(BSIZE-on), u.u_count);
#ifdef	MPX_FILS
		if (type!=IFBLK && type!=IFMPB)
#else
		if (type!=IFBLK)
#endif
			{
			diff = ip->i_size - u.u_offset;
			if(diff <= 0)
				return;
			if(diff < n)
				n = diff;
			bn = bmap(ip, bn, B_READ);
			if(u.u_error)
				return;
			dev = ip->i_dev;
		} else
			rablock = bn+1;
		if ((long)bn<0) {
			bp = geteblk();
			clrbuf(bp);
		} else if (ip->i_un.i_lastr+1==lbn)
			bp = breada(dev, bn, rablock);
		else
			bp = bread(dev, bn);
		ip->i_un.i_lastr = lbn;
		n = MIN((unsigned)n, BSIZE-bp->b_resid);
		if (n!=0) {
			iomove(mapin(bp)+on, n, B_READ);
			mapout(bp);
		}
		if (n+on==BSIZE || u.u_offset==ip->i_size) {
			if (ip->i_flag & IPIPE)
				bp->b_flags &= ~B_DELWRI;	/* cancel i/o */
			bp->b_flags |= B_AGE;
		}
		brelse(bp);
	} while(u.u_error==0 && u.u_count!=0 && n>0);
}

/*
 * Write the file corresponding to
 * the inode pointed at by the argument.
 * The actual write arguments are found
 * in the variables:
 *	u_base		core address for source
 *	u_offset	byte offset in file
 *	u_count		number of bytes to write
 *	u_segflg	write to kernel/user/user I
 */
void
writei(register struct inode *ip)
{
	struct buf *bp;
	dev_t dev;
	daddr_t bn;
	register int16_t n, on;
	register int16_t type;
#ifdef	UCB_FSFIX
	struct	direct	*dp;
#endif

#ifndef	INSECURE
	/*
	 * clear set-uid/gid on any write
	 */
	ip->i_mode &= ~(ISUID|ISGID);
#endif
	if(u.u_offset < 0) {
		u.u_error = EINVAL;
		return;
	}
	dev = (dev_t)ip->i_un.i_rdev;
	type = ip->i_mode&IFMT;
#ifdef	MPX_FILS
	if (type==IFCHR || type==IFMPC)
#else
	if (type==IFCHR)
#endif
		{
		ip->i_flag |= IUPD|ICHG;
		(*cdevsw[major(dev)].d_write)(dev);
		return;
	}
	if (u.u_count == 0)
		return;
	do {
		bn = u.u_offset >> BSHIFT;
		on = u.u_offset & BMASK;
		n = MIN((unsigned)(BSIZE-on), u.u_count);
#ifdef	MPX_FILS
		if (type!=IFBLK && type!=IFMPB)
#else
		if (type!=IFBLK)
#endif
			{
#ifdef UCB_QUOTAS
			if ((bn = bmap(ip, bn, B_WRITE)) == 0)
				return;
#else
			bn = bmap(ip, bn, B_WRITE);
#endif
			if (bn < 0)
				return;
			dev = ip->i_dev;
		}
		if (n == BSIZE) 
			bp = getblk(dev, bn);
		else {
			bp = bread(dev, bn);
			/*
			 * Tape drivers don't clear buffers on end-of-tape
			 * any longer (clrbuf can't be called from interrupt).
			 */
			if (bp->b_resid == BSIZE)
				clrbuf(bp);
		}
#ifdef	UCB_FSFIX
		iomove((caddr_t)(dp = (struct direct *)((char *)mapin(bp)+on)), n, B_WRITE);
#else
		iomove(mapin(bp)+on, n, B_WRITE);
		mapout(bp);
#endif
		if(u.u_error != 0) {
#ifdef	UCB_FSFIX
			mapout(bp);
#endif
			brelse(bp);
		} else {
#ifdef	UCB_FSFIX
			if ((ip->i_mode&IFMT) == IFDIR && (dp->d_ino == 0)) {
				mapout(bp);
				/*
				 * Writing to clear a directory entry.
				 * Must insure the write occurs before
				 * the inode is freed, or may end up
				 * pointing at a new (different) file
				 * if inode is quickly allocated again
				 * and system crashes.
				 */
				bwrite(bp);
			} else {
				mapout(bp);
				if (((u.u_offset & BMASK) == 0)
				   && (ip->i_flag & IPIPE) == 0) {
					bp->b_flags |= B_AGE;
					bawrite(bp);
				} else
					bdwrite(bp);
			}
#else	UCB_FSFIX

			if (((u.u_offset & BMASK) == 0)
			   && (ip->i_flag & IPIPE) == 0)
				bp->b_flags |= B_AGE;
			bdwrite(bp);
#endif	UCB_FSFIX
		}
#ifndef	UCB_SYMLINKS
		if (u.u_offset > ip->i_size && (type == IFDIR || type == IFREG))
#else
		if (u.u_offset > ip->i_size && (type == IFDIR || type == IFREG || type == IFLNK))
#endif
			ip->i_size = u.u_offset;
		ip->i_flag |= IUPD|ICHG;
	} while(u.u_error == 0 && u.u_count != 0);
}


#if UNUSED
/*
 * Move n bytes at byte location cp
 * to/from (flag) the user/kernel (u.segflg)
 * area starting at u.base.
 * Update all the arguments by the number
 * of bytes moved.
 *
 * There are 2 algorithms,
 * if source address, dest address and count
 * are all even in a user copy,
 * then the machine language copyin/copyout
 * is called.
 * If not, its done byte by byte with
 * cpass and passc.
 */
iomove(cp, n, flag)
register caddr_t cp;
register n;
{
	register t;

	if (n == 0)
		return;
	if(u.u_segflg != 1 && ((n | (int)cp | (int)u.u_base) & (NBPW-1)) == 0) {
		if (flag == B_WRITE)
			if (u.u_segflg == 0)
				t = copyin(u.u_base, (caddr_t)cp, n);
			else
				t = copyiin(u.u_base, (caddr_t)cp, n);
		else
			if (u.u_segflg == 0)
				t = copyout((caddr_t)cp, u.u_base, n);
			else
				t = copyiout((caddr_t)cp, u.u_base, n);
		if (t) {
			u.u_error = EFAULT;
			return;
		}
		u.u_base += n;
		u.u_offset += n;
		u.u_count -= n;
		return;
	}
	if (flag == B_WRITE) {
		do {
			if ((t = cpass()) < 0)
				return;
			*cp++ = t;
		} while (--n);
	} else
		do {
			if(passc(*cp++) < 0)
				return;
		} while (--n);
}
#endif /* UNUSED */
