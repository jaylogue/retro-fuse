#include "bsd29adapt.h"

/*
 *	SCCS id	@(#)iget.c	2.1 (Berkeley)	9/1/83
 */

#include "bsd29/include/sys/param.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/dir.h>
#include <bsd29/include/sys/user.h>
#include <bsd29/include/sys/inode.h>
#include <bsd29/include/sys/ino.h>
#include <bsd29/include/sys/filsys.h>
#include <bsd29/include/sys/mount.h>
#include <bsd29/include/sys/conf.h>
#include <bsd29/include/sys/buf.h>
#ifdef	UCB_QUOTAS
#include <sys/quota.h>
#endif
#include <bsd29/include/sys/inline.h>


#ifdef	UCB_QUOTAS
#define	IFREE(dev, ip, bn)		qfree(ip, bn)
#define	TLOOP(dev, bn, f1, f2, ip)	tloop(dev, bn, f1, f2, ip)
#else
#define	IFREE(dev, ip, bn)		free(dev, bn)
#define	TLOOP(dev, bn, f1, f2, ip)	tloop(dev, bn, f1, f2)
#endif


#ifdef	UCB_IHASH

#ifdef	SMALL
#define	INOHSZ	16		/* must be power of two */
#else
#define	INOHSZ	64		/* must be power of two */
#endif
#define	INOHASH(dev,ino)	(((dev) + (ino)) & (INOHSZ - 1))
struct	inode	*ihash[INOHSZ];
struct	inode	*ifreelist;

/*
 * Initialize hash links for inodes
 * and build inode free list.
 */
void
ihinit()
{
	register struct inode *ip;

	ifreelist = &inode[0];
	for(ip = inode; ip < inodeNINODE - 1; ip++)
		ip->i_link = ip + 1;
}

/*
 * Find an inode if it is incore.
 * This is the equivalent, for inodes,
 * of ``incore'' in bio.c or ``pfind'' in subr.c.
 */
struct inode *
ifind(dev_t dev, ino_t ino)
{
	register struct inode *ip;

	for (ip = ihash[INOHASH(dev,ino)]; ip != NULL;
	    ip = ip->i_link)
		if (ino==ip->i_number && dev==ip->i_dev)
			return (ip);
	return ((struct inode *)NULL);
}

#endif	UCB_IHASH

/*
 * Look up an inode by device,inumber.
 * If it is in core (in the inode structure),
 * honor the locking protocol.
 * If it is not in core, read it in from the
 * specified device.
 * If the inode is mounted on, perform
 * the indicated indirection.
 * In all cases, a pointer to a locked
 * inode structure is returned.
 *
 * printf warning: no inodes -- if the inode
 *	structure is full
 * panic: no imt -- if the mounted file
 *	system is not in the mount table.
 *	"cannot happen"
 */
struct inode *
iget(dev_t dev, ino_t ino)
{
	register struct inode *ip;
	register struct mount *mp;
#ifdef	UCB_IHASH
	register int slot;
#else
	register struct inode *oip;
#endif
	struct buf *bp;
	struct dinode *dp;

loop:
#ifdef	UCB_IHASH
	slot = INOHASH(dev, ino);
	ip = ihash[slot];
	while (ip != NULL)
#else
	oip = NULL;
	for(ip = inode; ip < inodeNINODE; ip++)
#endif
	{
		if(ino == ip->i_number && dev == ip->i_dev) {
	again:
			if((ip->i_flag&ILOCK) != 0) {
				ip->i_flag |= IWANT;
				sleep((caddr_t)ip, PINOD);
				if(ino == ip->i_number && dev == ip->i_dev)
					goto again;
				goto loop;
			}
			if((ip->i_flag&IMOUNT) != 0) {
				for(mp = mount; mp < mountNMOUNT; mp++)
				if(mp->m_inodp == ip) {
					dev = mp->m_dev;
					ino = ROOTINO;
					goto loop;
				}
				panic("no imt");
			}
			ip->i_count++;
			ip->i_flag |= ILOCK;
			return(ip);
		}
#ifdef	UCB_IHASH
		ip = ip->i_link;
#else
		if(oip==NULL && ip->i_count==0)
			oip = ip;
#endif
	}
#ifdef	UCB_IHASH
	if (ifreelist == NULL)
#else
	ip = oip;
	if(ip == NULL)
#endif
	{
		tablefull("inode");
		u.u_error = ENFILE;
		return(NULL);
	}
#ifdef	UCB_IHASH
	ip = ifreelist;
	ifreelist = ip->i_link;
	ip->i_link = ihash[slot];
	ihash[slot] = ip;
#endif
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_flag = ILOCK;
	ip->i_count++;
	ip->i_un.i_lastr = 0;
#ifdef	UCB_QUOTAS
	ip->i_quot = NULL;
#endif

	bp = bread(dev, itod(ino));

	/*
	 * Check I/O errors
	 */
	if (((bp->b_flags&B_ERROR) != 0) || bp->b_resid) {
		brelse(bp);
		/*
		 * This was an iput(ip), but if i_nlink happens to be 0,
		 * the itrunc() in iput() might be successful.
		 */
		ip->i_count = 0;
		ip->i_number = 0;
		ip->i_flag = 0;
#ifdef	UCB_IHASH
		ihash[slot] = ip->i_link;
		ip->i_link = ifreelist;
		ifreelist = ip;
#endif
		return(NULL);
	}
	dp = (struct dinode *) mapin(bp);
	dp += itoo(ino);
	iexpand(ip, dp);
	mapout(bp);
	brelse(bp);
	return(ip);
}

void
iexpand(register struct inode *ip, register struct dinode *dp)
{
	register char *p1;
	char *p2;
	int i;

	ip->i_mode = dp->di_mode;
	ip->i_nlink = dp->di_nlink;
	ip->i_uid = dp->di_uid;
	ip->i_gid = dp->di_gid;
	ip->i_size = dp->di_size;
	p1 = (char *)ip->i_un.i_addr;
	p2 = (char *)dp->di_addr;
	for(i=0; i<NADDR; i++) {
		*p1++ = *p2++;
		*p1++ = 0;
		*p1++ = *p2++;
		*p1++ = *p2++;
	}
}

/*
 * Decrement reference count of
 * an inode structure.
 * On the last reference,
 * write the inode out and if necessary,
 * truncate and deallocate the file.
 */
void
iput(register struct inode *ip)
{
#ifdef	UCB_QUOTAS
	register struct inode *qp;
#endif
#ifdef	UCB_IHASH
	register struct inode *jp;
	register int i;
# endif

#ifdef	UCB_QUOTAS

	/*
	 * Berkeley's quota system is hierarchical
	 * so this loop is necessary to de-reference all
	 * quota nodes still hanging around
	 */
    do {
	qp = NULL;
#endif
	if(ip->i_count == 1) {
		ip->i_flag |= ILOCK;
		if(ip->i_nlink <= 0) {
			itrunc(ip);
			ip->i_mode = 0;
			ip->i_flag |= IUPD|ICHG;
			ifree(ip->i_dev, ip->i_number);
		}
#ifdef	UCB_QUOTAS
		qp = ip->i_quot;
		if (qp != NULL)
			ip->i_quot = NULL;
#endif
#ifdef UCB_FSFIX
		IUPDAT(ip, &time, &time, 0);
#else
		IUPDAT(ip, &time, &time);
#endif
		prele(ip);
#ifdef	UCB_IHASH
		i = INOHASH(ip->i_dev, ip->i_number);
		if (ihash[i] == ip)
			ihash[i] = ip->i_link;
		else {
			for (jp = ihash[i]; jp != NULL; jp = jp->i_link)
				if (jp->i_link == ip) {
					jp->i_link = ip->i_link;
					goto done;
				}
			panic("iput");
		}
done:
		ip->i_link = ifreelist;
		ifreelist = ip;
#endif
		ip->i_flag = 0;
		ip->i_number = 0;
	}
	else
		prele(ip);
	ip->i_count--;
#ifdef	UCB_QUOTAS
	} while ((ip = qp) != NULL);
#endif
}

/*
 * Check accessed and update flags on
 * an inode structure.
 * If any are on, update the inode
 * with the current time.
 */
#ifdef UCB_FSFIX
/*
 * If waitfor set, then must insure
 * i/o order by waiting for the write
 * to complete.
 */

void
iupdat(register struct inode *ip, time_t *ta, time_t *tm, int16_t waitfor)
#else
void
iupdat(register struct inode *ip, time_t *ta, time_t *tm)
#endif
{
	register struct buf *bp;
	register struct dinode *dp;
	struct filsys *fp;
	char *p1, *p2;
	int16_t i;

	if((ip->i_flag&(IUPD|IACC|ICHG)) != 0) {
		if ((fp = getfs(ip->i_dev)) == NULL || fp->s_ronly)
			return;
		bp = bread(ip->i_dev, itod(ip->i_number));
		if ((bp->b_flags & B_ERROR) || bp->b_resid) {
			brelse(bp);
			return;
		}
		dp = (struct dinode *) mapin(bp);
		dp += itoo(ip->i_number);
		dp->di_mode = ip->i_mode;
		dp->di_nlink = ip->i_nlink;
		dp->di_uid = ip->i_uid;
		dp->di_gid = ip->i_gid;
		dp->di_size = ip->i_size;
		p1 = (char *)dp->di_addr;
		p2 = (char *)ip->i_un.i_addr;
		for(i=0; i<NADDR; i++) {
			*p1++ = *p2++;
#ifdef	MPX_FILS
			if(*p2++ != 0 && (ip->i_mode&IFMT)!=IFMPC
			   && (ip->i_mode&IFMT)!=IFMPB)
#else
			if(*p2++ != 0)
#endif
			   printf("iaddr[%d] > 2^24(%D), inum = %d, dev = %d\n",
				i, ip->i_un.i_addr[i], ip->i_number, ip->i_dev);
			*p1++ = *p2++;
			*p1++ = *p2++;
		}
		if(ip->i_flag&IACC)
			dp->di_atime = *ta;
		if(ip->i_flag&IUPD)
			dp->di_mtime = *tm;
		if(ip->i_flag&ICHG)
			dp->di_ctime = time;
		ip->i_flag &= ~(IUPD|IACC|ICHG);
		mapout(bp);
#ifdef UCB_FSFIX
		if (waitfor)
			bwrite(bp);
		else
#endif
			bdwrite(bp);
	}
}

/*
 * Free all the disk blocks associated
 * with the specified inode structure.
 * The blocks of the file are removed
 * in reverse order. This FILO
 * algorithm will tend to maintain
 * a contiguous free list much longer
 * than FIFO.
 */
void
itrunc(register struct inode *ip)
{
	register int16_t i;
	register dev_t dev;
	daddr_t bn;
#ifdef UCB_FSFIX
	struct inode itmp;
#endif

	i = ip->i_mode & IFMT;
#ifndef UCB_SYMLINKS
	if (i!=IFREG && i!=IFDIR)
#else
	if (i!=IFREG && i!=IFDIR && i!=IFLNK)
#endif
		return;

#ifdef UCB_FSFIX
	/*
	 * Clean inode on disk before freeing blocks
	 * to insure no duplicates if system crashes.
	 */
	itmp = *ip;
	itmp.i_size = 0;
	for (i = 0; i < NADDR; i++)
		itmp.i_un.i_addr[i] = 0;
	itmp.i_flag |= ICHG|IUPD;
	iupdat(&itmp, &time, &time, 1);
	ip->i_flag &= ~(IUPD|IACC|ICHG);

	/*
	 * Now return blocks to free list... if machine
	 * crashes, they will be harmless MISSING blocks.
	 */
#endif

	dev = ip->i_dev;
	for(i=NADDR-1; i>=0; i--) {
		bn = ip->i_un.i_addr[i];
		if(bn == (daddr_t)0)
			continue;
		ip->i_un.i_addr[i] = (daddr_t)0;
		switch(i) {

		default:
			IFREE(dev, ip, bn);
			break;

		case NADDR-3:
			TLOOP(dev, bn, 0, 0, ip);
			break;

		case NADDR-2:
			TLOOP(dev, bn, 1, 0, ip);
			break;

		case NADDR-1:
			TLOOP(dev, bn, 1, 1, ip);
		}
	}
	ip->i_size = 0;
#ifndef UCB_FSFIX
	ip->i_flag |= ICHG|IUPD;
#else
	/* Inode was written and flags updated above.
	 * No need to modify flags here.
	 */
#endif

}

#ifdef	UCB_QUOTAS
void
tloop(dev_t dev, daddr_t bn, int16_t f1, int16_t f2, struct inode	*ip)
#else
void
tloop(dev_t dev, daddr_t bn, int16_t f1, int16_t f2)
#endif
{
	register int16_t i;
	register struct buf *bp;
	register daddr_t *bap;
	daddr_t nb;

	bp = NULL;
	for(i=NINDIR-1; i>=0; i--) {
		if(bp == NULL) {
			bp = bread(dev, bn);
			if ((bp->b_flags & B_ERROR) || bp->b_resid) {
				brelse(bp);
				return;
			}
		}
		bap = (daddr_t *) mapin(bp);
		nb = bap[i];
		mapout(bp);
		if(nb == (daddr_t)0)
			continue;
		if(f1) {
			brelse(bp);
			bp = NULL;
			TLOOP(dev, nb, f2, 0, ip);
		} else
			IFREE(dev, ip, nb);
	}
	if(bp != NULL)
		brelse(bp);
	IFREE(dev, ip, bn);
}

/*
 * Make a new file.
 */
struct inode *
maknode(int16_t mode)
{
	register struct inode *ip;

	ip = ialloc(u.u_pdir->i_dev);
	if(ip == NULL) {
		iput(u.u_pdir);
		return(NULL);
	}
	ip->i_flag |= IACC|IUPD|ICHG;
	if((mode&IFMT) == 0)
		mode |= IFREG;
	ip->i_mode = mode & ~u.u_cmask;
	ip->i_nlink = 1;
	ip->i_uid = u.u_uid;
	ip->i_gid = u.u_gid;

#ifdef UCB_FSFIX
	/*
	 * Make sure inode goes to disk before directory entry.
	 */
	iupdat(ip, &time, &time, 1);
#endif

	wdir(ip);
	return(ip);
}

/*
 * Write a directory entry with
 * parameters left as side effects
 * to a call to namei.
 */
void
wdir(struct inode *ip)
{

	if (u.u_pdir->i_nlink <= 0) {
		u.u_error = ENOTDIR;
		goto out;
	}
	u.u_dent.d_ino = ip->i_number;
	bcopy((caddr_t)u.u_dbuf, (caddr_t)u.u_dent.d_name, DIRSIZ);
	u.u_count = sizeof(struct direct);
	u.u_segflg = 1;
	u.u_base = (caddr_t)&u.u_dent;
	writei(u.u_pdir);
#ifdef	UCB_QUOTAS
	/*
	 * Copy quota for new file
	 */
	if (!u.u_error)
		qcopy(u.u_pdir, ip);
#endif
out:
	iput(u.u_pdir);
}
