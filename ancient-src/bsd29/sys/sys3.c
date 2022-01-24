#include "bsd29adapt.h"

/*
 *	SCCS id	@(#)sys3.c	2.1 (Berkeley)	9/4/83
 */

#include "bsd29/include/sys/param.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/ino.h>
#include <bsd29/include/sys/reg.h>
#include <bsd29/include/sys/buf.h>
#include <bsd29/include/sys/filsys.h>
#include <bsd29/include/sys/mount.h>
#include <bsd29/include/sys/dir.h>
#include <bsd29/include/sys/user.h>
#include <bsd29/include/sys/inode.h>
#include <bsd29/include/sys/file.h>
#include <bsd29/include/sys/conf.h>
#include <bsd29/include/sys/stat.h>
#include <bsd29/include/sys/inline.h>


#if UNUSED
/*
 * the fstat system call.
 */
fstat()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		struct stat *sb;
	} *uap;

	uap = (struct a *)u.u_ap;
	fp = getf(uap->fdes);
	if (fp == NULL)
		return;
#ifdef  UCB_NET
	if (fp->f_flag & FSOCKET)
		u.u_error = sostat(fp->f_socket, uap->sb);
	else
#endif
		stat1(fp->f_inode, uap->sb, fp->f_flag & FPIPE?  fp->f_un.f_offset: (off_t) 0);
}
#endif /* UNUSED */

#if UNUSED
/*
 * the stat system call.
 */
stat()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		struct stat *sb;
	} *uap;

	uap = (struct a *)u.u_ap;
#ifndef	UCB_SYMLINKS
	ip = namei(uchar, LOOKUP);
#else
	ip = namei(uchar, LOOKUP, 1);
#endif
	if (ip == NULL)
		return;
	stat1(ip, uap->sb, (off_t)0);
	iput(ip);
}
#endif /* UNUSED */

#if UNUSED
#ifdef	UCB_SYMLINKS
/*
 * Lstat system call; like stat but doesn't follow links.
 */
lstat()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		struct stat *sb;
	} *uap;

	uap = (struct a *)u.u_ap;
	ip = namei(uchar, 0, 0);
	if (ip == NULL)
		return;
	stat1(ip, uap->sb, (off_t)0);
	iput(ip);
}
#endif
#endif /* UNUSED */

/*
 * The basic routine for fstat and stat:
 * get the inode and pass appropriate parts back.
 */
void
stat1(register struct inode *ip, struct stat *ub, off_t pipeadj)
{
	register struct dinode *dp;
	register struct buf *bp;
	struct stat ds;

#ifdef UCB_FSFIX
	IUPDAT(ip, &time, &time, 0);
#else
	IUPDAT(ip, &time, &time);
#endif
	/*
	 * first copy from inode table
	 */
	ds.st_dev = ip->i_dev;
	ds.st_ino = ip->i_number;
	ds.st_mode = ip->i_mode;
	ds.st_nlink = ip->i_nlink;
	ds.st_uid = ip->i_uid;
	ds.st_gid = ip->i_gid;
	ds.st_rdev = (dev_t)ip->i_un.i_rdev;
	ds.st_size = ip->i_size - pipeadj;
	/*
	 * next the dates in the disk
	 */
	bp = bread(ip->i_dev, itod(ip->i_number));
	dp = (struct dinode *) mapin(bp);
	dp += itoo(ip->i_number);
	ds.st_atime = wswap_int32(dp->di_atime);
	ds.st_mtime = wswap_int32(dp->di_mtime);
	ds.st_ctime = wswap_int32(dp->di_ctime);
	mapout(bp);
	brelse(bp);
	if (copyout((caddr_t)&ds, (caddr_t)ub, sizeof(ds)) < 0)
		u.u_error = EFAULT;
}

#if UNUSED
/*
 * the dup system call.
 */
dup()
{
	register struct file *fp;
	register struct a {
		int	fdes;
		int	fdes2;
	} *uap;
	register i, m;

	uap = (struct a *)u.u_ap;
	m = uap->fdes & ~077;
	uap->fdes &= 077;
	fp = getf(uap->fdes);
	if (fp == NULL)
		return;
	if ((m&0100) == 0) {
		if ((i = ufalloc()) < 0)
			return;
	} else {
		i = uap->fdes2;
		if (i<0 || i>=NOFILE) {
			u.u_error = EBADF;
			return;
		}
		u.u_r.r_val1 = i;
	}
	if (i!=uap->fdes) {
		if (u.u_ofile[i]!=NULL)
#ifndef UCB_NET
			closef(u.u_ofile[i]);
#else
			closef(u.u_ofile[i],0);
#endif
		u.u_ofile[i] = fp;
		fp->f_count++;
	}
}
#endif /* UNUSED */

#if UNUSED
/*
 * the mount system call.
 */
smount()
{
	dev_t dev;
	char *cp;
	register struct inode *ip;
	register struct mount *mp;
	struct mount *smp;
	register struct filsys *fp;
	struct buf *bp;
	register struct a {
		char	*fspec;
		char	*freg;
		int	ronly;
	} *uap;

	uap = (struct a *)u.u_ap;
	dev = getmdev();
	if (u.u_error || !suser())
		return;
	u.u_dirp = (caddr_t)uap->freg;
#ifndef	UCB_SYMLINKS
	ip = namei(uchar, LOOKUP);
#else
	ip = namei(uchar, LOOKUP, 1);
#endif
	if (ip == NULL)
		return;
	if (ip->i_count != 1 || (ip->i_mode & (IFBLK & IFCHR)) != 0)
		goto out;
	smp = NULL;
	for(mp = mount; mp < mountNMOUNT; mp++) {
		if (mp->m_inodp != NULL) {
			if (dev == mp->m_dev)
				goto out;
		} else
			if (smp == NULL)
				smp = mp;
	}
	mp = smp;
	if (mp == NULL)
		goto out;
	(*bdevsw[major(dev)].d_open)(dev, !uap->ronly);
	if (u.u_error)
		goto out;
	bp = bread(dev, SUPERB);
	if (u.u_error) {
		brelse(bp);
		goto out1;
	}
	mp->m_inodp = ip;
	mp->m_dev = dev;
	fp = &mp->m_filsys;
	bcopy((caddr_t) mapin(bp), (caddr_t)fp, sizeof(struct filsys));
	mapout(bp);
	fp->s_ilock = 0;
	fp->s_flock = 0;
	fp->s_ronly = uap->ronly & 1;
#ifdef	UCB_IHASH
	fp->s_nbehind = 0;
	fp->s_lasti = 1;
#endif
	u.u_dirp = uap->freg;
	for (cp = fp->s_fsmnt; cp < &fp->s_fsmnt[sizeof(fp->s_fsmnt) - 1];)
		if ((*cp++ = uchar()) == 0)
			u.u_dirp--;		/* get 0 again */
	*cp = 0;
	brelse(bp);
	ip->i_flag |= IMOUNT;
	prele(ip);
	return;

out:
	u.u_error = EBUSY;
out1:
	iput(ip);
}
#endif /* UNUSED */

#if UNUSED
/*
 * the umount system call.
 */
sumount()
{
	dev_t dev;
	register struct inode *ip;
	register struct mount *mp;
	register struct buf *bp;
	struct buf *dp;

	dev = getmdev();
	if (u.u_error || !suser())
		return;
	xumount(dev);	/* remove unused sticky files from text table */
	update();
	for (mp = mount; mp < mountNMOUNT; mp++)
		if (mp->m_inodp != NULL && dev == mp->m_dev) {
			for(ip = inode; ip < inodeNINODE; ip++)
				if (ip->i_number != 0 && dev == ip->i_dev) {
					u.u_error = EBUSY;
					return;
				}
			(*bdevsw[major(dev)].d_close)(dev, 0);
			dp = bdevsw[major(dev)].d_tab;
			for (bp = dp->b_forw; bp != dp; bp = bp->b_forw) {
				(void) _spl6();
				if (bp->b_dev == dev) {
#ifdef UCB_BHASH
					bunhash(bp);
#endif
					bp->b_dev = NODEV;
				}
				(void) _spl0();
			}
			ip = mp->m_inodp;
			ip->i_flag &= ~IMOUNT;
			plock(ip);
			iput(ip);
			mp->m_inodp = NULL;
			return;
		}

	u.u_error = EINVAL;
}
#endif /* UNUSED */

#if UNUSED
/*
 * Common code for mount and umount.
 * Check that the user's argument is a reasonable
 * thing on which to mount, and return the device number if so.
 */
dev_t
getmdev()
{
	register dev_t dev;
	register struct inode *ip;

#ifndef	UCB_SYMLINKS
	ip = namei(uchar, LOOKUP);
#else
	ip = namei(uchar, LOOKUP, 1);
#endif
	if (ip == NULL)
		return(NODEV);
	if ((ip->i_mode&IFMT) != IFBLK)
		u.u_error = ENOTBLK;
	dev = (dev_t)ip->i_un.i_rdev;
	if (major(dev) >= nblkdev)
		u.u_error = ENXIO;
	iput(ip);
	return(dev);
}
#endif /* UNUSED */

#ifdef	UCB_SYMLINKS
/*
 * Return target name of a symbolic link
 */
void
readlink()
{
	register struct inode *ip;
	register struct a {
		char	*name;
		char	*buf;
		int16_t	count;
	} *uap;

	ip = namei(uchar, 0, 0);
	if (ip == NULL)
		return;
	if ((ip->i_mode&IFMT) != IFLNK) {
		u.u_error = ENXIO;
		goto out;
	}
	uap = (struct a *)u.u_ap;
	u.u_offset = 0;
	u.u_base = uap->buf;
	u.u_count = uap->count;
	u.u_segflg = 0;
	readi(ip);
	u.u_r.r_val1 = uap->count - u.u_count; /* fix uninitialized use error */
out:
	iput(ip);
}

/*
 * symlink -- make a symbolic link
 */
void
symlink()
{
	register struct a {
		char	*target;
		char	*linkname;
	} *uap;
	register struct inode *ip;
	register char *tp;
	register int16_t c, nc;

	uap = (struct a *)u.u_ap;
	tp = uap->target;
	nc = 0;
	while ((c = fubyte(tp)) != 0) {
		if (c < 0) {
			u.u_error = EFAULT;
			return;
		}
		tp++;
		nc++;
	}
	u.u_dirp = uap->linkname;
	ip = namei(uchar, 1, 0);
	if (ip) {
		iput(ip);
		u.u_error = EEXIST;
		return;
	}
	if (u.u_error)
		return;
	ip = maknode((int16_t)IFLNK | 0777);
	if (ip == NULL)
		return;
	u.u_base = uap->target;
	u.u_count = nc;
	u.u_offset = 0;
	u.u_segflg = 0;
	writei(ip);
	iput(ip);
}
#endif	UCB_SYMLINKS
