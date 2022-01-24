#include "bsd211adapt.h"

/*
 * 	@(#) 	ufs_syscalls2.c	  1.6 (2.11BSD) 2019/12/17
 *
 * ufs_syscalls was getting too large.  Various UFS related system calls were
 * relocated to this file.
*/

#include "bsd211/h/param.h"
#include "bsd211/machine/seg.h"
#include "bsd211/h/file.h"
#include "bsd211/h/user.h"
#include "bsd211/h/inode.h"
#include "bsd211/h/buf.h"
#include "bsd211/h/fs.h"
#include "bsd211/h/namei.h"
#include "bsd211/h/mount.h"
#include "bsd211/h/kernel.h"

#if UNUSED
int16_t
statfs()
	{
	register struct a
		{
		char	*path;
		struct	statfs	*buf;
		} *uap = (struct a *)u.u_ap;
	register struct	inode	*ip;
	struct	nameidata nd;
	register struct nameidata *ndp = &nd;
	struct	mount	*mp;

	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, uap->path);
	ip = namei(ndp);
	if	(!ip)
		return(u.u_error);
	mp = (struct mount *)((intptr_t)ip->i_fs - offsetof(struct mount, m_filsys));
	iput(ip);
	u.u_error = statfs1(mp, uap->buf);
	return(u.u_error);
	}
#endif /* UNUSED */

#if UNUSED
fstatfs()
	{
	register struct a
		{
		int	fd;
		struct	statfs *buf;
		} *uap = (struct a *)u.u_ap;
	register struct inode *ip;
	struct	mount *mp;

	ip = getinode(uap->fd);
	if	(!ip)
		return(u.u_error);
	mp = (struct mount *)((int)ip->i_fs - offsetof(struct mount, m_filsys));
	u.u_error = statfs1(mp, uap->buf);
	return(u.u_error);
	}
#endif /* UNUSED */

#if UNUSED
int16_t
statfs1(mp, sbp)
	struct	mount	*mp;
	struct	statfs	*sbp;
	{
	struct	statfs	sfs;
	register struct	statfs	*sfsp;
	struct	xmount	*xmp = (struct xmount *)SEG5;
	struct	fs	*fs = &mp->m_filsys;

	sfsp = &sfs;
	sfsp->f_type = MOUNT_UFS;
	sfsp->f_bsize = MAXBSIZE;
	sfsp->f_iosize = MAXBSIZE;
	sfsp->f_blocks = fs->fs_fsize - fs->fs_isize;
	sfsp->f_bfree = fs->fs_tfree;
	sfsp->f_bavail = fs->fs_tfree;
	sfsp->f_files = (fs->fs_isize - 2) * INOPB;
	sfsp->f_ffree = fs->fs_tinode;

	mapseg5(mp->m_extern, XMOUNTDESC);
	bcopy(xmp->xm_mnton, sfsp->f_mntonname, MNAMELEN);
	bcopy(xmp->xm_mntfrom, sfsp->f_mntfromname, MNAMELEN);
	normalseg5();
	sfsp->f_flags = mp->m_flags & MNT_VISFLAGMASK;
	return(copyout(sfsp, sbp, sizeof (struct statfs)));
	}
#endif /* UNUSED */

#if UNUSED
int16_t
getfsstat()
	{
	register struct a
		{
		struct	statfs	*buf;
		int16_t	bufsize;
		u_int	flags;
		} *uap = (struct a *)u.u_ap;
	register struct	mount *mp;
	caddr_t	sfsp;
	int16_t	count, maxcount, error;

	maxcount = uap->bufsize / sizeof (struct statfs);
	sfsp = (caddr_t)uap->buf;
	count = 0;
	for	(mp = mount; mp < &mount[NMOUNT]; mp++)
		{
		if	(mp->m_inodp == NULL)
			continue;
		if	(count < maxcount)
			{
			if	(error = statfs1(mp, sfsp))
				return(u.u_error = error);
			sfsp += sizeof (struct statfs);
			}
		count++;
		}
	if	(sfsp && count > maxcount)
		u.u_r.r_val1 = maxcount;
	else
		u.u_r.r_val1 = count;
	return(0);
	}
#endif /* UNUSED */

/*
 * 'ufs_sync' is the routine which syncs a single filesystem.  This was
 * created to replace 'update' which 'unmount' called.  It seemed silly to
 * sync _every_ filesystem when unmounting just one filesystem.
*/

int16_t
ufs_sync(mp)
	register struct mount *mp;
	{
	register struct fs *fs;
	struct	buf *bp;
	int16_t	error = 0;

	fs = &mp->m_filsys;
	if	(fs->fs_fmod && (mp->m_flags & MNT_RDONLY))
		{
		printf("fs = %s\n", fs->fs_fsmnt);
		panic("sync: rofs");
		}
	syncinodes(fs);		/* sync the inodes for this filesystem */
	bflush(mp->m_dev);	/* flush dirty data blocks */
#ifdef	QUOTA
	qsync(mp->m_dev);	/* sync the quotas */
#endif

/*
 * And lastly the superblock, if the filesystem was modified.
 * Write back modified superblocks. Consistency check that the superblock
 * of each file system is still in the buffer cache.
*/
	if	(fs->fs_fmod)
		{
		bp =  getblk(mp->m_dev, SUPERB);
		fs->fs_fmod = 0;
		bcopy(fs, mapin(bp), sizeof (struct fs));
		mapout(bp);
		bwrite(bp);
		error = geterror(bp);
		}
	return(error);
	}

/*
 * This is somewhat inefficient in that the inode table is scanned for each
 * filesystem but it didn't seem worth a page or two of code on something
 * which only happens every 30 seconds.
*/
void
syncinodes(fs)
	struct	fs *fs;
	{
	register struct inode *ip;

	/*
	 * Write back each (modified) inode.
	 */
	for	(ip = inode; ip < inodeNINODE; ip++)
		{
/*
 * Attempt to reduce the overhead by short circuiting the scan if the
 * inode is not for the filesystem being processed.
*/
		if	(ip->i_fs != fs)
			continue;
		if ((ip->i_flag & ILOCKED) != 0 || ip->i_count == 0 ||
			   (ip->i_flag & (IMOD|IACC|IUPD|ICHG)) == 0)
			continue;
		ip->i_flag |= ILOCKED;
		ip->i_count++;
		iupdat(ip, &time, &time, 0);
		iput(ip);
		}
	}

#if UNUSED
/*
 * mode mask for creation of files
 */
umask()
{
	register struct a {
		int	mask;
	} *uap = (struct a *)u.u_ap;

	u.u_r.r_val1 = u.u_cmask;
	u.u_cmask = uap->mask & 07777;
}
#endif /* UNUSED */

#if UNUSED
/*
 * Seek system call
 */
void
lseek()
{
	register struct file *fp;
	register struct a {
		int16_t	fd;
		off_t	off;
		int16_t	sbase;
	} *uap = (struct a *)u.u_ap;

	if ((fp = getf(uap->fd)) == NULL)
		return;
	if (fp->f_type != DTYPE_INODE) {
		u.u_error = ESPIPE;
		return;
	}
	switch (uap->sbase) {

	case L_INCR:
		fp->f_offset += uap->off;
		break;
	case L_XTND:
		fp->f_offset = uap->off + ((struct inode *)fp->f_data)->i_size;
		break;
	case L_SET:
		fp->f_offset = uap->off;
		break;
	default:
		u.u_error = EINVAL;
		return;
	}
	u.u_r.r_off = fp->f_offset;
}
#endif /* UNUSED */

/*
 * Synch an open file.
 */
void
fsync()
{
	register struct a {
		int16_t	fd;
	} *uap = (struct a *)u.u_ap;
	register struct inode *ip;

	if ((ip = getinode(uap->fd)) == NULL)
		return;
	ilock(ip);
	syncip(ip);
	iunlock(ip);
}

void
utimes()
{
	register struct a {
		char	*fname;
		struct	timeval *tptr;
	} *uap = (struct a *)u.u_ap;
	register struct inode *ip;
	struct	nameidata nd;
	register struct nameidata *ndp = &nd;
	struct timeval tv[2];
	struct vattr vattr;

	VATTR_NULL(&vattr);
	if (uap->tptr == NULL) {
		tv[0].tv_sec = tv[1].tv_sec = time.tv_sec;
		vattr.va_vaflags |= VA_UTIMES_NULL;
	} else if (u.u_error = copyin((caddr_t)uap->tptr,(caddr_t)tv,sizeof(tv)))
		return;
	NDINIT(ndp, LOOKUP, FOLLOW, UIO_USERSPACE, uap->fname);
	if ((ip = namei(ndp)) == NULL)
		return;
	vattr.va_atime = tv[0].tv_sec;
	vattr.va_mtime = tv[1].tv_sec;
	u.u_error = ufs_setattr(ip, &vattr);
	iput(ip);
}
