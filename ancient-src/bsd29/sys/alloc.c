#include "bsd29adapt.h"

/*
 *	SCCS id	@(#)alloc.c	2.1 (Berkeley)	8/5/83
 */

#include "bsd29/include/sys/param.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/filsys.h>
#include <bsd29/include/sys/mount.h>
#include <bsd29/include/sys/fblk.h>
#include <bsd29/include/sys/conf.h>
#include <bsd29/include/sys/buf.h>
#include <bsd29/include/sys/inode.h>
#include <bsd29/include/sys/ino.h>
#include <bsd29/include/sys/dir.h>
#include <bsd29/include/sys/user.h>
/* UNUSED #include <sys/quota.h> */

typedef	struct fblk *FBLKP;

#ifdef UCB_QUOTAS
/*
 * Bookkeeping for quota system interfaces to alloc() and free():
 * count one block against the quota of the given inode.
 * If the quota has been exceeded, return NULL without allocating block.
 */
struct buf *
qalloc(ip, d)
register struct inode *ip;
dev_t d;
{
	register struct inode *qp, *rp;
	struct buf *ret;

	for (qp = ip->i_quot; qp != (struct inode *) NULL; qp = qp->i_quot) {
		if (qp->i_un.i_qused >= qp->i_un.i_qmax) {
			/*
			 * A quota was exceeded. Undo the counts.
			 */
			for (rp = ip->i_quot; rp != qp; rp = rp->i_quot)
				rp->i_un.i_qused -= QCOUNT;
			u.u_error = EQUOT;
			return((struct buf *) NULL);
		}
		qp->i_un.i_qused += QCOUNT;
		qp->i_flag |= IUPD;
	}
	if((ret = alloc(d)) != NULL)
		return(ret);
	/*
	 * The block could not be allocated.
	 * Undo the quota count.
	 */
	for (qp = ip->i_quot; qp != (struct inode *) NULL; qp = qp->i_quot)
		if(qp->i_un.i_qused != 0)
			qp->i_un.i_qused -= QCOUNT;
	return((struct buf *) NULL);
}
#endif

/*
 * alloc will obtain the next available
 * free disk block from the free list of
 * the specified device.
 * The super block has up to NICFREE remembered
 * free blocks; the last of these is read to
 * obtain NICFREE more . . .
 *
 * no space on dev x/y -- when
 * the free list is exhausted.
 */
struct buf *
alloc(dev_t dev)
{
	daddr_t bno;
	register struct filsys *fp;
	register struct buf *bp;
	/* UNUSED int16_t	i; */
	register struct fblk *fbp;
	struct filsys *fps;

	if ((fp = getfs(dev)) == NULL)
		goto nofs;
	while(fp->s_flock)
		sleep((caddr_t)&fp->s_flock, PINOD);
	do {
		if(fp->s_nfree <= 0)
			goto nospace;
		if (fp->s_nfree > NICFREE) {
			fserr(fp, "bad free count");
			goto nospace;
		}
		bno = fp->s_free[--fp->s_nfree];
		if(bno == 0)
			goto nospace;
	} while (badblock(fp, bno));
	if(fp->s_nfree <= 0) {
		fp->s_flock++;
		bp = bread(dev, bno);
		if (((bp->b_flags&B_ERROR) == 0) && (bp->b_resid==0)) {
			fbp = (FBLKP) mapin(bp);
			*((FBLKP)&fp->s_nfree) = *fbp;
			mapout(bp);
		}
		brelse(bp);
#ifdef	UCB_FSFIX
		/*
		 * Write the superblock back, synchronously,
		 * so that the free list pointer won't point at garbage.
		 * We can still end up with dups in free if we then
		 * use some of the blocks in this freeblock, then crash
		 * without a sync.
		 */
		bp = getblk(dev, SUPERB);
		fp->s_fmod = 0;
		fp->s_time = time;
		fps = (struct filsys *) mapin(bp);
		*fps = *fp;
		mapout(bp);
		bwrite(bp);

#endif
		fp->s_flock = 0;
		wakeup((caddr_t)&fp->s_flock);
		if (fp->s_nfree <=0)
			goto nospace;
	}
	bp = getblk(dev, bno);
	clrbuf(bp);
	fp->s_fmod = 1;
	fp->s_tfree--;
	return(bp);

nospace:
	fp->s_nfree = 0;
	fp->s_tfree = 0;
	fserr(fp, "file system full");
	/* THIS IS A KLUDGE... */
	/* SHOULD RATHER SEND A SIGNAL AND SUSPEND THE PROCESS IN A */
	/* STATE FROM WHICH THE SYSTEM CALL WILL RESTART */
#ifdef	UCB_UPRINTF
	uprintf("\n%s: write failed, file system is full\n", fp->s_fsmnt);
#endif
#ifdef UNUSED
	for (i = 0; i < 5; i++)
		sleep((caddr_t)&lbolt, PRIBIO);
#endif
nofs:
	u.u_error = ENOSPC;
	return(NULL);
}

#ifdef UCB_QUOTAS
/*
 *	decrement the count for quotas
 */
qfree(ip, b)
register struct inode *ip;
daddr_t b;
{
	register struct inode *qp;

	for (qp = ip->i_quot; qp != NULL; qp = qp->i_quot)
		if (qp->i_un.i_qused) {
			qp->i_un.i_qused -= QCOUNT;
			qp->i_flag |= IUPD;
		}
	free(ip->i_dev, b);
}
#endif

/*
 * place the specified disk block
 * back on the free list of the
 * specified device.
 */
void
free(dev_t dev, daddr_t bno)
{
	register struct filsys *fp;
	register struct buf *bp;
	register struct fblk *fbp;

	if ((fp = getfs(dev)) == NULL)
		return;
	if (badblock(fp, bno))
		return;
	while(fp->s_flock)
		sleep((caddr_t)&fp->s_flock, PINOD);
	if(fp->s_nfree <= 0) {
		fp->s_nfree = 1;
		fp->s_free[0] = 0;
	}
	if(fp->s_nfree >= NICFREE) {
		fp->s_flock++;
		bp = getblk(dev, bno);
		fbp = (FBLKP) mapin(bp);
		*fbp = *((FBLKP) &fp->s_nfree);
		mapout(bp);
		fp->s_nfree = 0;
		bwrite(bp);
		fp->s_flock = 0;
		wakeup((caddr_t)&fp->s_flock);
	}
	fp->s_free[fp->s_nfree++] = bno;
	fp->s_tfree++;
	fp->s_fmod = 1;
}

/*
 * Check that a block number is in the
 * range between the I list and the size
 * of the device.
 * This is used mainly to check that a
 * garbage file system has not been mounted.
 *
 * bad block on dev x/y -- not in range
 */
int16_t
badblock(register struct filsys *fp, daddr_t bn)
{

	if (bn < fp->s_isize || bn >= fp->s_fsize) {
		fserr(fp, "bad block");
		return(1);
	}
	return(0);
}

/*
 * Allocate an unused I node
 * on the specified device.
 * Used with file creation.
 * The algorithm keeps up to
 * NICINOD spare I nodes in the
 * super block. When this runs out,
 * a linear search through the
 * I list is instituted to pick
 * up NICINOD more.
 */
struct inode *
ialloc(dev_t dev)
{
	register struct filsys *fp;
	register struct buf *bp;
	register struct inode *ip;
	int16_t i;
	struct dinode *dp;
	ino_t ino;
	daddr_t adr;
	ino_t inobas;
	int16_t first;

	if ((fp = getfs(dev)) == NULL)
		goto nofs;
	while(fp->s_ilock)
		sleep((caddr_t)&fp->s_ilock, PINOD);
loop:
	if(fp->s_ninode > 0) {
		ino = fp->s_inode[--fp->s_ninode];
		if (ino <= ROOTINO)
			goto loop;
		ip = iget(dev, ino);
		if(ip == NULL)
			return(NULL);
		if(ip->i_mode == 0) {
			for (i=0; i<NADDR; i++)
				ip->i_un.i_addr[i] = 0;
			fp->s_fmod = 1;
			fp->s_tinode--;
			return(ip);
		}
		/*
		 * Inode was allocated after all.
		 * Look some more.
		 */
		iput(ip);
		goto loop;
	}
	fp->s_ilock++;
	if (fp->s_nbehind < 4 * NICINOD) {
		first = 1;
		ino = fp->s_lasti;
#ifdef	DIAGNOSTIC
		if (itoo(ino))
			panic("ialloc");
#endif
		adr = itod(ino);
	} else {
fromtop:
		first = 0;
		ino = 1;
		adr = SUPERB+1;
		fp->s_nbehind = 0;
	}
	for(; adr < fp->s_isize; adr++) {
		inobas = ino;
		bp = bread(dev, adr);
		if ((bp->b_flags & B_ERROR) || bp->b_resid) {
			brelse(bp);
			ino += INOPB;
			continue;
		}
		dp = (struct dinode *) mapin(bp);
		for(i=0; i<INOPB; i++) {
			if(dp->di_mode != 0)
				goto cont;
#ifdef	UCB_IHASH
			if(ifind(dev, ino))
				goto cont;
#else
			for(ip = inode; ip < inodeNINODE; ip++)
				if(dev==ip->i_dev && ino==ip->i_number)
					goto cont;
#endif
			fp->s_inode[fp->s_ninode++] = ino;
			if(fp->s_ninode >= NICINOD)
				break;
		cont:
			ino++;
			dp++;
		}
		mapout(bp);
		brelse(bp);
		if(fp->s_ninode >= NICINOD)
			break;
	}
	if (fp->s_ninode < NICINOD && first) {
		goto fromtop;
	}
	fp->s_lasti = inobas;
	fp->s_ilock = 0;
	wakeup((caddr_t)&fp->s_ilock);
	if(fp->s_ninode > 0)
		goto loop;
	fserr(fp, "out of inodes");
#ifdef	UCB_UPRINTF
	uprintf("\n%s:  create failed, no inodes free\n", fp->s_fsmnt);
#endif
nofs:
	u.u_error = ENOSPC;
	return(NULL);
}

/*
 * Free the specified I node
 * on the specified device.
 * The algorithm stores up
 * to NICINOD I nodes in the super
 * block and throws away any more.
 */
void
ifree(dev_t dev, ino_t ino)
{
	register struct filsys *fp;

	if ((fp = getfs(dev)) == NULL)
		return;
	fp->s_tinode++;
	if(fp->s_ilock)
		return;
	if(fp->s_ninode >= NICINOD) {
		if (fp->s_lasti > ino)
			fp->s_nbehind++;
		return;
	}
	fp->s_inode[fp->s_ninode++] = ino;
	fp->s_fmod = 1;
}

/*
 * getfs maps a device number into
 * a pointer to the incore super
 * block.
 * The algorithm is a linear
 * search through the mount table.
 * A consistency check of the
 * in core free-block and i-node
 * counts.
 *
 * bad count on dev x/y -- the count
 *	check failed. At this point, all
 *	the counts are zeroed which will
 *	almost certainly lead to "no space"
 *	diagnostic
 * The previous ``panic:  no fs'', which occurred
 * when a device was not found in the mount table,
 * has been replaced with a warning message.  This
 * is because a panic would occur if one forgot to
 * mount the pipedev.
 * NULL is now returned instead, and routines which
 * previously thought that they were guaranteed a
 * valid pointer from getfs (alloc, free, ialloc,
 * ifree, access, iupdat) have been modified to
 * detect this.
 */
struct filsys *
getfs(dev_t dev)
{
	register struct mount *mp;
	register struct filsys *fp;

	for(mp = mount; mp < mountNMOUNT; mp++)
	if(mp->m_inodp != NULL && mp->m_dev == dev) {
		fp = &mp->m_filsys;
		if(fp->s_nfree > NICFREE || fp->s_ninode > NICINOD) {
			fserr(fp, "bad count");
			fp->s_nfree = 0;
			fp->s_ninode = 0;
		}
		return(fp);
	}
	printf("no fs on dev %u/%u\n", major(dev), minor(dev));
	return((struct filsys *) NULL);
}

/*
 * Update is the internal name of
 * 'sync'. It goes through the disk
 * queues to initiate sandbagged IO;
 * goes through the I nodes to write
 * modified nodes; and it goes through
 * the mount table to initiate modified
 * super blocks.
 */
void
update()
{
	register struct inode *ip;
	register struct mount *mp;
	register struct buf *bp;
	struct filsys *fp,*fpdst;

	if(updlock)
		return;
	updlock++;
	for(mp = mount; mp < mountNMOUNT; mp++)
		if(mp->m_inodp != NULL) {
			fp = &mp->m_filsys;
			if(fp->s_fmod==0 || fp->s_ilock!=0 ||
			   fp->s_flock!=0 || fp->s_ronly!=0)
				continue;
			bp = getblk(mp->m_dev, SUPERB);
			if (bp->b_flags & B_ERROR)
				continue;
			fp->s_fmod = 0;
			fp->s_time = time;
			fpdst = (struct filsys *) mapin(bp);
			*fpdst = *fp;
			mapout(bp);
			bwrite(bp);
		}
	for(ip = inode; ip < inodeNINODE; ip++)
		if((ip->i_flag&ILOCK)==0 && ip->i_count) {
			ip->i_flag |= ILOCK;
			ip->i_count++;
#ifdef	UCB_FSFIX
			iupdat(ip, &time, &time, 0);
#else
			iupdat(ip, &time, &time);
#endif
			iput(ip);
		}
	updlock = 0;
	bflush(NODEV);
}
