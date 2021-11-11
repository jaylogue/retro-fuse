/*
 *	SCCS id	@(#)rk.c	2.1 (Berkeley)	9/1/83
 */

#include "rk.h"
#if	NRK > 0
#include "param.h"
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/rkreg.h>

#define	NRKBLK	4872	/* Number of blocks per drive */

struct	rkdevice *RKADDR;

struct	buf	rktab;
#ifdef	UCB_DBUFS
struct	buf	rrkbuf[NRK];
#else
struct	buf	rrkbuf;
#endif

rkattach(addr, unit)
struct rkdevice *addr;
{
	if (unit != 0)
		return(0);
	RKADDR = addr;
	return(1);
}

rkstrategy(bp)
register struct buf *bp;
{
	register s;

	if (RKADDR == (struct rkdevice *) NULL) {
		bp->b_error = ENXIO;
		goto errexit;
	}
	if (bp->b_blkno >= NRKBLK) {
		bp->b_error = EINVAL;
errexit:
		bp->b_flags |= B_ERROR;
		iodone(bp);
		return;
	}
#ifdef UNIBUS_MAP
	mapalloc(bp);
#endif
	bp->av_forw = (struct buf *)NULL;
	s = spl5();
	if(rktab.b_actf == NULL)
		rktab.b_actf = bp;
	else
		rktab.b_actl->av_forw = bp;
	rktab.b_actl = bp;
	if(rktab.b_active == NULL)
		rkstart();
	splx(s);
}

rkstart()
{
	register struct	rkdevice *rkaddr = RKADDR;
	register struct buf *bp;
	register com;
	daddr_t bn;
	int dn, cn, sn;

	if ((bp = rktab.b_actf) == NULL)
		return;
	rktab.b_active++;
	bn = bp->b_blkno;
	dn = minor(bp->b_dev);
	cn = bn / 12;
	sn = bn % 12;
	rkaddr->rkda = (dn << 13) | (cn << 4) | sn;
	rkaddr->rkba = bp->b_un.b_addr;
	rkaddr->rkwc = -(bp->b_bcount >> 1);
	com = ((bp->b_xmem & 3) << 4) | RKCS_IDE | RKCS_GO;
	if(bp->b_flags & B_READ)
		com |= RKCS_RCOM;
	else
		com |= RKCS_WCOM;
	rkaddr->rkcs = com;

#ifdef	RK_DKN
	dk_busy |= 1 << RK_DKN;
	dk_numb[RK_DKN]++;
	dk_wds[RK_DKN] += bp->b_bcount >> 6;
#endif	RK_DKN
}

rkintr()
{
	register struct rkdevice *rkaddr = RKADDR;
	register struct buf *bp;

	if (rktab.b_active == NULL)
		return;
#ifdef	RK_DKN
	dk_busy &= ~(1 << RK_DKN);
#endif	RK_DKN
	bp = rktab.b_actf;
	rktab.b_active = NULL;
	if (rkaddr->rkcs & RKCS_ERR) {
		while ((rkaddr->rkcs & RKCS_RDY) == 0)
			;
		if (rkaddr->rker & RKER_WLO)
			/*
			 *	Give up on write locked devices
			 *	immediately.
			 */
			printf("rk%d: write locked\n", minor(bp->b_dev));
		else
			{
#ifdef	UCB_DEVERR
			harderr(bp, "rk");
			printf("er=%b ds=%b\n", rkaddr->rker, RKER_BITS,
				rkaddr->rkds, RK_BITS);
#else
			deverror(bp, rkaddr->rker, rkaddr->rkds);
#endif
			rkaddr->rkcs = RKCS_RESET | RKCS_GO;
			while((rkaddr->rkcs & RKCS_RDY) == 0)
				;
			if (++rktab.b_errcnt <= 10) {
				rkstart();
				return;
			}
		}
		bp->b_flags |= B_ERROR;
	}
	rktab.b_errcnt = 0;
	rktab.b_actf = bp->av_forw;
	bp->b_resid = -(rkaddr->rkwc << 1);
	iodone(bp);
	rkstart();
}

rkread(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NRK)
		u.u_error = ENXIO;
	else
		physio(rkstrategy, &rrkbuf[unit], dev, B_READ);
#else
	physio(rkstrategy, &rrkbuf, dev, B_READ);
#endif
}

rkwrite(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NRK)
		u.u_error = ENXIO;
	else
		physio(rkstrategy, &rrkbuf[unit], dev, B_WRITE);
#else
	physio(rkstrategy, &rrkbuf, dev, B_WRITE);
#endif
}
#endif	NRK
