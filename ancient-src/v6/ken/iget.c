#

#include "v6-adapt.h"

#include "../param.h"
#include "../systm.h"
#include "../user.h"
#include "../inode.h"
#include "../filsys.h"
#include "../conf.h"
#include "../buf.h"

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
iget(int16_t dev, int16_t ino)
{
	register struct inode *p;
	register int16_t *ip2;
	int16_t *ip1;
	register struct mount *mp;
	struct inode *ip;
	struct buf *bp;

loop:
	ip = NULL;
	for(p = &inode[0]; p < &inode[NINODE]; p++) {
		if(dev==p->i_dev && ino==p->i_number) {
			if((p->i_flag&ILOCK) != 0) {
				p->i_flag |= IWANT;
				sleep(p, PINOD);
				goto loop;
			}
			if((p->i_flag&IMOUNT) != 0) {
				for(mp = &mount[0]; mp < &mount[NMOUNT]; mp++)
				if(mp->m_inodp == p) {
					dev = mp->m_dev;
					ino = ROOTINO;
					goto loop;
				}
				panic("no imt");
			}
			p->i_count++;
			p->i_flag |= ILOCK;
			return(p);
		}
		if(ip==NULL && p->i_count==0)
			ip = p;
	}
	if((p=ip) == NULL) {
		printf("Inode table overflow\n");
		u.u_error = ENFILE;
		return(NULL);
	}
	p->i_dev = dev;
	p->i_number = ino;
	p->i_flag = ILOCK;
	p->i_count++;
	p->i_lastr = -1;
	bp = bread(dev, ldiv(ino+31,16));
	/*
	 * Check I/O errors
	 */
	if (bp->b_flags&B_ERROR) {
		brelse(bp);
		iput(p);
		return(NULL);
	}
	ip1 = (int16_t *)(bp->b_addr + 32*lrem(ino+31, 16));
	ip2 = &p->i_mode;
	while(ip2 < &p->i_addr[8])
		*ip2++ = *ip1++;
	brelse(bp);
	return(p);
}

/*
 * Decrement reference count of
 * an inode structure.
 * On the last reference,
 * write the inode out and if necessary,
 * truncate and deallocate the file.
 */
void
iput(struct inode *p)
{
	register struct inode *rp;

	rp = p;
	if(rp->i_count == 1) {
		rp->i_flag |= ILOCK;
		if(rp->i_nlink <= 0) {
			itrunc(rp);
			rp->i_mode = 0;
			ifree(rp->i_dev, rp->i_number);
		}
		iupdat(rp, time);
		prele(rp);
		rp->i_flag = 0;
		rp->i_number = 0;
	}
	rp->i_count--;
	prele(rp);
}

/*
 * Check accessed and update flags on
 * an inode structure.
 * If either is on, update the inode
 * with the corresponding dates
 * set to the argument tm.
 */
void
iupdat(struct inode *p, int16_t *tm)
{
	register struct inode *rp;
	register int16_t *ip1, *ip2;
	struct buf *bp;
	int16_t i;

	rp = p;
	if((rp->i_flag&(IUPD|IACC)) != 0) {
		if(getfs(rp->i_dev)->s_ronly)
			return;
		i = rp->i_number+31;
		bp = bread(rp->i_dev, ldiv(i,16));
		ip1 = (int16_t *)(bp->b_addr + 32*lrem(i, 16));
		ip2 = &rp->i_mode;
		while(ip2 < &rp->i_addr[8])
			*ip1++ = *ip2++;
		if(rp->i_flag&IACC) {
			*ip1++ = time[0];
			*ip1++ = time[1];
		} else
			ip1 += 2;
		if(rp->i_flag&IUPD) {
			*ip1++ = *tm++;
			*ip1++ = *tm;
		}
		bwrite(bp);
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
	register struct inode *rp;
	register struct buf *bp;
	register int16_t *cp;
	struct buf *dp;
	int16_t *ep, *_ip;

	rp = ip;
	if((rp->i_mode&(IFCHR&IFBLK)) != 0)
		return;
	for(_ip = &rp->i_addr[7]; _ip >= &rp->i_addr[0]; _ip--)
	if(*_ip) {
		if((rp->i_mode&ILARG) != 0) {
			bp = bread(rp->i_dev, *_ip);
			for(cp = (int16_t *)(bp->b_addr+512); cp >= (int16_t *)bp->b_addr; cp--)
			if(*cp) {
				if(_ip == &rp->i_addr[7]) {
					dp = bread(rp->i_dev, *cp);
					for(ep = (int16_t *)(dp->b_addr+512); ep >= (int16_t *)dp->b_addr; ep--)
					if(*ep)
						free(rp->i_dev, *ep);
					brelse(dp);
				}
				free(rp->i_dev, *cp);
			}
			brelse(bp);
		}
		free(rp->i_dev, *_ip);
		*_ip = 0;
	}
	rp->i_mode &= ~ILARG;
	rp->i_size0 = 0;
	rp->i_size1 = 0;
	rp->i_flag |= IUPD;
}

/*
 * Make a new file.
 */
struct inode *
maknode(int16_t mode)
{
	register struct inode *ip;

	ip = ialloc(u.u_pdir->i_dev);
	if (ip==NULL)
		return(NULL);
	ip->i_flag |= IACC|IUPD;
	ip->i_mode = mode|IALLOC;
	ip->i_nlink = 1;
	ip->i_uid = u.u_uid;
	ip->i_gid = u.u_gid;
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
	register char *cp1, *cp2;

	u.u_dent.u_ino = ip->i_number;
	cp1 = &u.u_dent.u_name[0];
	for(cp2 = &u.u_dbuf[0]; cp2 < &u.u_dbuf[DIRSIZ];)
		*cp1++ = *cp2++;
	u.u_count = DIRSIZ+2;
	u.u_segflg = 1;
	u.u_base = (char *)&u.u_dent;
	writei(u.u_pdir);
	iput(u.u_pdir);
}
