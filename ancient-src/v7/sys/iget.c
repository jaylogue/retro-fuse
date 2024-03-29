#include "v7adapt.h"

#include "../h/param.h"
#include "../h/systm.h"
#include "../h/mount.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/inode.h"
#include "../h/ino.h"
#include "../h/filsys.h"
#include "../h/conf.h"
#include "../h/buf.h"

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
	register struct inode *oip;
	register struct buf *bp;
	register struct dinode *dp;

loop:
	oip = NULL;
	for(ip = &inode[0]; ip < &inode[NINODE]; ip++) {
		if(ino == ip->i_number && dev == ip->i_dev) {
			if((ip->i_flag&ILOCK) != 0) {
				ip->i_flag |= IWANT;
				sleep((caddr_t)ip, PINOD);
				goto loop;
			}
			if((ip->i_flag&IMOUNT) != 0) {
				for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
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
		if(oip==NULL && ip->i_count==0)
			oip = ip;
	}
	ip = oip;
	if(ip == NULL) {
		printf("Inode table overflow\n");
		u.u_error = ENFILE;
		return(NULL);
	}
	ip->i_dev = dev;
	ip->i_number = ino;
	ip->i_flag = ILOCK;
	ip->i_count++;
	ip->i_un.i_lastr = 0;
	bp = bread(dev, itod(ino));
	/*
	 * Check I/O errors
	 */
	if((bp->b_flags&B_ERROR) != 0) {
		brelse(bp);
		iput(ip);
		return(NULL);
	}
	dp = bp->b_un.b_dino;
	dp += itoo(ino);
	iexpand(ip, dp);
	brelse(bp);
	return(ip);
}

void
iexpand(struct inode *ip, struct dinode *dp)
{
	register daddr_t *p1;
	uint8_t *p2;
	int16_t i;

	ip->i_mode = dp->di_mode;
	ip->i_nlink = dp->di_nlink;
	ip->i_uid = dp->di_uid;
	ip->i_gid = dp->di_gid;
	ip->i_size = wswap_int32(dp->di_size);
	p1 = (daddr_t *)ip->i_un.i_addr;
	p2 = (uint8_t *)dp->di_addr;
	for(i=0; i<NADDR; i++) {
		*p1    = ((daddr_t)*p2++) << 16;
		*p1   |= ((daddr_t)*p2++);
		*p1++ |= ((daddr_t)*p2++) << 8;
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
iput(struct inode *ip)
{

	if(ip->i_count == 1) {
		ip->i_flag |= ILOCK;
		if(ip->i_nlink <= 0) {
			itrunc(ip);
			ip->i_mode = 0;
			ip->i_flag |= IUPD|ICHG;
			ifree(ip->i_dev, ip->i_number);
		}
		iupdat(ip, &time, &time);
		prele(ip);
		ip->i_flag = 0;
		ip->i_number = 0;
	}
	ip->i_count--;
	prele(ip);
}

/*
 * Check accessed and update flags on
 * an inode structure.
 * If any are on, update the inode
 * with the current time.
 */
void
iupdat(struct inode *ip, time_t *ta, time_t *tm)
{
	register struct buf *bp;
	struct dinode *dp;
	register uint8_t *p1;
	daddr_t *p2;
	int16_t i;

	if((ip->i_flag&(IUPD|IACC|ICHG)) != 0) {
		if(getfs(ip->i_dev)->s_ronly)
			return;
		bp = bread(ip->i_dev, itod(ip->i_number));
		if (bp->b_flags & B_ERROR) {
			brelse(bp);
			return;
		}
		dp = bp->b_un.b_dino;
		dp += itoo(ip->i_number);
		dp->di_mode = ip->i_mode;
		dp->di_nlink = ip->i_nlink;
		dp->di_uid = ip->i_uid;
		dp->di_gid = ip->i_gid;
		dp->di_size = wswap_int32(ip->i_size);
		p1 = (uint8_t *)dp->di_addr;
		p2 = (daddr_t *)ip->i_un.i_addr;
		for(i=0; i<NADDR; i++) {
			*p1++ = (uint8_t)(*p2 >> 16);
			if((*p2 >> 24) != 0 && (ip->i_mode&IFMT)!=IFMPC
			   && (ip->i_mode&IFMT)!=IFMPB)
				printf("iaddress > 2^24\n");
			*p1++ = (uint8_t)(*p2);
			*p1++ = (uint8_t)(*p2++ >> 8);
		}
		if(ip->i_flag&IACC)
			dp->di_atime = wswap_int32(*ta);
		if(ip->i_flag&IUPD)
			dp->di_mtime = wswap_int32(*tm);
		if(ip->i_flag&ICHG)
			dp->di_ctime = wswap_int32(time);
		ip->i_flag &= ~(IUPD|IACC|ICHG);
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
itrunc(struct inode *ip)
{
	register int16_t i;
	uint16_t ui;
	dev_t dev;
	daddr_t bn;

	ui = ip->i_mode & IFMT;
	if (ui!=IFREG && ui!=IFDIR)
		return;
	dev = ip->i_dev;
	for(i=NADDR-1; i>=0; i--) {
		bn = ip->i_un.i_addr[i];
		if(bn == (daddr_t)0)
			continue;
		ip->i_un.i_addr[i] = (daddr_t)0;
		switch(i) {

		default:
			free(dev, bn);
			break;

		case NADDR-3:
			tloop(dev, bn, 0, 0);
			break;

		case NADDR-2:
			tloop(dev, bn, 1, 0);
			break;

		case NADDR-1:
			tloop(dev, bn, 1, 1);
		}
	}
	ip->i_size = 0;
	ip->i_flag |= ICHG|IUPD;
}

void
tloop(dev_t dev, daddr_t bn, int16_t f1, int16_t f2)
{
	register int16_t i;
	register struct buf *bp;
	register daddr_t *bap;
	daddr_t nb;

	bp = NULL;
	for(i=NINDIR-1; i>=0; i--) {
		if(bp == NULL) {
			bp = bread(dev, bn);
			if (bp->b_flags & B_ERROR) {
				brelse(bp);
				return;
			}
			bap = bp->b_un.b_daddr;
		}
		nb = wswap_int32(bap[i]);
		if(nb == (daddr_t)0)
			continue;
		if(f1) {
			brelse(bp);
			bp = NULL;
			tloop(dev, nb, f2, 0);
		} else
			free(dev, nb);
	}
	if(bp != NULL)
		brelse(bp);
	free(dev, bn);
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
	wdir(ip);
	/* Fix for an ancient bug that results in a leaked inode when there's no
	 * room to create the directory entry for a new file. */
	if (u.u_error != 0) {
		ip->i_nlink--;
		iput(ip);
		return(NULL);
	}
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
out:
	iput(u.u_pdir);
}
