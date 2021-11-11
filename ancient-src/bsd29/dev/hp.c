/*
 *	SCCS id	@(#)hp.c	2.1 (Berkeley)	8/31/83
 */

/*
 *	RJP04/RWP04/RJP06/RWP06 disk driver
 */
#include "hp.h"
#if	NHP > 0
#include "param.h"
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/seg.h>
#include <sys/hpreg.h>
#ifndef	INTRLVE
#include <sys/inline.h>
#endif
#include <sys/uba.h>

#define	HP_NSECT	22
#define	HP_NTRAC	19
#define	HP_SDIST	2
#define	HP_RDIST	6

extern	struct	size hp_sizes[];
extern	struct	hpdevice *HPADDR;

int	hp_offset[] =
{
	HPOF_P400,	HPOF_M400,	HPOF_P400,	HPOF_M400,
	HPOF_P800,	HPOF_M800,	HPOF_P800,	HPOF_M800,
	HPOF_P1200,	HPOF_M1200,	HPOF_P1200,	HPOF_M1200,
	0,		0,		0,		0
};

struct	buf	hptab;
#ifdef	UCB_DBUFS
struct	buf	rhpbuf[NHP];
#else
struct	buf	rhpbuf;
#endif
struct	buf	hputab[NHP];

#ifdef	INTRLVE
extern	daddr_t	dkblock();
#endif

void
hproot()
{
	hpattach(HPADDR, 0);
}

hpattach(addr, unit)
register struct hpdevice *addr;
{
	if (unit != 0)
		return(0);
	if ((addr != (struct hpdevice *) NULL) && (fioword(addr) != -1)) {
		HPADDR = addr;
#if	PDP11 == 70 || PDP11 == GENERIC
		if (fioword(&(addr->hpbae)) != -1)
			hptab.b_flags |= B_RH70;
#endif
		return(1);
	}
	HPADDR = (struct hpdevice *) NULL;
	return(0);
}

hpstrategy(bp)
register struct	buf *bp;
{
	register struct buf *dp;
	register unit;
	long	bn;

	unit = minor(bp->b_dev) & 077;
	if (unit >= (NHP << 3) || (HPADDR == (struct hpdevice *) NULL)) {
		bp->b_error = ENXIO;
		goto errexit;
	}
	if (bp->b_blkno < 0 ||
	    (bn = dkblock(bp)) + (long) ((bp->b_bcount + 511) >> 9)
	    > hp_sizes[unit & 07].nblocks) {
		bp->b_error = EINVAL;
errexit:
		bp->b_flags |= B_ERROR;
		iodone(bp);
		return;
	}
#ifdef	UNIBUS_MAP
	if ((hptab.b_flags & B_RH70) == 0)
		mapalloc(bp);
#endif
	bp->b_cylin = bn / (HP_NSECT * HP_NTRAC) + hp_sizes[unit & 07].cyloff;
	unit = dkunit(bp);
	dp = &hputab[unit];
	(void) _spl5();
	disksort(dp, bp);
	if (dp->b_active == 0) {
		hpustart(unit);
		if (hptab.b_active == 0)
			hpstart();
	}
	(void) _spl0();
}

/*
 * Unit start routine.
 * Seek the drive to where the data are
 * and then generate another interrupt
 * to actually start the transfer.
 * If there is only one drive on the controller
 * or we are very close to the data, don't
 * bother with the search.  If called after
 * searching once, don't bother to look
 * where we are, just queue for transfer (to avoid
 * positioning forever without transferring).
 */
hpustart(unit)
register unit;
{
	register struct	hpdevice *hpaddr = HPADDR;
	register struct buf *dp;
	struct	buf *bp;
	daddr_t	bn;
	int sn, cn, csn;

	hpaddr->hpcs2.w = unit;
	hpaddr->hpcs1.c[0] = HP_IE;
	hpaddr->hpas = 1 << unit;

	if (unit >= NHP)
		return;
#ifdef	HP_DKN
	dk_busy &= ~(1 << (unit + HP_DKN));
#endif	HP_DKN
	dp = &hputab[unit];
	if ((bp = dp->b_actf) == NULL)
		return;
	/*
	 * If we have already positioned this drive,
	 * then just put it on the ready queue.
	 */
	if (dp->b_active)
		goto done;
	dp->b_active++;

	/*
	 * If drive has just come up,
	 * set up the pack.
	 */
	if ((hpaddr->hpds & HPDS_VV) == 0) {
		/* SHOULD WARN SYSTEM THAT THIS HAPPENED */
		hpaddr->hpcs1.c[0] = HP_IE | HP_PRESET | HP_GO;
		hpaddr->hpof = HPOF_FMT22;
	}

#if	NHP >	1
	/*
	 * If drive is offline, forget about positioning.
	 */
	if ((hpaddr->hpds & (HPDS_DPR | HPDS_MOL)) != (HPDS_DPR | HPDS_MOL))
		goto done;

	/*
	 * Figure out where this transfer is going to
	 * and see if we are close enough to justify not searching.
	 */
	bn = dkblock(bp);
	cn = bp->b_cylin;
	sn = bn % (HP_NSECT * HP_NTRAC);
	sn = (sn + HP_NSECT - HP_SDIST) % HP_NSECT;

	if (hpaddr->hpcc != cn)
		goto search;
	csn = (hpaddr->hpla >> 6) - sn + HP_SDIST - 1;
	if (csn < 0)
		csn += HP_NSECT;
	if (csn > HP_NSECT - HP_RDIST)
		goto done;

search:
	hpaddr->hpdc = cn;
	hpaddr->hpda = sn;
	hpaddr->hpcs1.c[0] = HP_IE | HP_SEARCH | HP_GO;
#ifdef	HP_DKN
	/*
	 * Mark unit busy for iostat.
	 */
	unit += HP_DKN;
	dk_busy |= 1 << unit;
	dk_numb[unit]++;
#endif	HP_DKN
	return;
#endif	NHP > 1

done:
	/*
	 * Device is ready to go.
	 * Put it on the ready queue for the controller.
	 */
	dp->b_forw = NULL;
	if (hptab.b_actf == NULL)
		hptab.b_actf = dp;
	else
		hptab.b_actl->b_forw = dp;
	hptab.b_actl = dp;
}

/*
 * Start up a transfer on a drive.
 */
hpstart()
{
	register struct hpdevice *hpaddr = HPADDR;
	register struct buf *bp;
	register unit;
	struct	buf *dp;
	daddr_t	bn;
	int	dn, sn, tn, cn;

loop:
	/*
	 * Pull a request off the controller queue.
	 */
	if ((dp = hptab.b_actf) == NULL)
		return;
	if ((bp = dp->b_actf) == NULL) {
		hptab.b_actf = dp->b_forw;
		goto loop;
	}
	/*
	 * Mark controller busy and
	 * determine destination of this request.
	 */
	hptab.b_active++;
	unit = minor(bp->b_dev) & 077;
	dn = dkunit(bp);
	bn = dkblock(bp);
	cn = bn / (HP_NSECT * HP_NTRAC) + hp_sizes[unit & 07].cyloff;
	sn = bn % (HP_NSECT * HP_NTRAC);
	tn = sn / HP_NSECT;
	sn = sn % HP_NSECT;

	/*
	 * Select drive.
	 */
	hpaddr->hpcs2.w = dn;
	/*
	 * Check that it is ready and online.
	 */
	if ((hpaddr->hpds & (HPDS_DPR | HPDS_MOL)) != (HPDS_DPR | HPDS_MOL)) {
		hptab.b_active = 0;
		hptab.b_errcnt = 0;
		dp->b_actf = bp->av_forw;
		bp->b_flags |= B_ERROR;
		iodone(bp);
		goto loop;
	}
	if (hptab.b_errcnt >= 16 && (bp->b_flags & B_READ)) {
		hpaddr->hpof = hp_offset[hptab.b_errcnt & 017] | HPOF_FMT22;
		hpaddr->hpcs1.w = HP_OFFSET | HP_GO;
		while ((hpaddr->hpds & (HPDS_PIP | HPDS_DRY)) != HPDS_DRY)
			;
	}
	hpaddr->hpdc = cn;
	hpaddr->hpda = (tn << 8) + sn;
	hpaddr->hpba = bp->b_un.b_addr;
#if	PDP11 == 70 || PDP11 == GENERIC
	if (hptab.b_flags & B_RH70)
		hpaddr->hpbae = bp->b_xmem;
#endif
	hpaddr->hpwc = -(bp->b_bcount >> 1);
	/*
	 * Warning:  unit is being used as a temporary.
	 */
	unit = ((bp->b_xmem & 3) << 8) | HP_IE | HP_GO;
	if (bp->b_flags & B_READ)
		unit |= HP_RCOM;
	else
		unit |= HP_WCOM;
	hpaddr->hpcs1.w = unit;

#ifdef	HP_DKN
	dk_busy |= 1 << (HP_DKN + NHP);
	dk_numb[HP_DKN + NHP]++;
	dk_wds[HP_DKN + NHP] += bp->b_bcount >> 6;
#endif	HP_DKN
}

/*
 * Handle a disk interrupt.
 */
hpintr()
{
	register struct hpdevice *hpaddr = HPADDR;
	register struct buf *dp;
	register unit;
	struct	buf *bp;
	int	as, i, j;

	as = hpaddr->hpas & 0377;
	if (hptab.b_active) {
#ifdef	HP_DKN
		dk_busy &= ~(1 << (HP_DKN + NHP));
#endif	HP_DKN
		/*
		 * Get device and block structures.  Select the drive.
		 */
		dp = hptab.b_actf;
		bp = dp->b_actf;
		unit = dkunit(bp);
		hpaddr->hpcs2.c[0] = unit;
		/*
		 * Check for and process errors.
		 */
		if (hpaddr->hpcs1.w & HP_TRE) {
			while ((hpaddr->hpds & HPDS_DRY) == 0)
				;
			if (hpaddr->hper1 & HPER1_WLE) {
				/*
				 *	Give up on write locked devices
				 *	immediately.
				 */
				printf("hp%d: write locked\n", unit);
				bp->b_flags |= B_ERROR;
			} else {
				/*
				 * After 28 retries (16 without offset and
				 * 12 with offset positioning), give up.
				 */
				if (++hptab.b_errcnt > 28) {
				    bp->b_flags |= B_ERROR;
#ifdef	UCB_DEVERR
				    harderr(bp, "hp");
				    printf("cs2=%b er1=%b\n", hpaddr->hpcs2.w,
					HPCS2_BITS, hpaddr->hper1, HPER1_BITS);
#else
				    deverror(bp, hpaddr->hpcs2.w, hpaddr->hper1);
#endif
				} else
				    hptab.b_active = 0;
			}
#ifdef UCB_ECC
			/*
			 * If soft ecc, correct it (continuing
			 * by returning if necessary).
			 * Otherwise, fall through and retry the transfer.
			 */
			if((hpaddr->hper1 & (HPER1_DCK|HPER1_ECH)) == HPER1_DCK)
				if (hpecc(bp))
					return;
#endif
			hpaddr->hpcs1.w = HP_TRE | HP_IE | HP_DCLR | HP_GO;
			if ((hptab.b_errcnt & 07) == 4) {
				hpaddr->hpcs1.w = HP_RECAL | HP_IE | HP_GO;
				while ((hpaddr->hpds & (HPDS_PIP | HPDS_DRY)) != HPDS_DRY)
					;
			}
		}
		if (hptab.b_active) {
			if (hptab.b_errcnt) {
				hpaddr->hpcs1.w = HP_RTC | HP_GO;
				while ((hpaddr->hpds & (HPDS_PIP | HPDS_DRY)) != HPDS_DRY)
					;
			}
			hptab.b_active = 0;
			hptab.b_errcnt = 0;
			hptab.b_actf = dp->b_forw;
			dp->b_active = 0;
			dp->b_actf = bp->av_forw;
			bp->b_resid = - (hpaddr->hpwc << 1);
			iodone(bp);
			hpaddr->hpcs1.w = HP_IE;
			if (dp->b_actf)
				hpustart(unit);
		}
		as &= ~(1 << unit);
	} else
		{
		if (as == 0)
			hpaddr->hpcs1.w = HP_IE;
		hpaddr->hpcs1.c[1] = HP_TRE >> 8;
	}
	for (unit = 0; unit < NHP; unit++)
		if (as & (1 << unit))
			hpustart(unit);
	hpstart();
}

hpread(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NHP)
		u.u_error = ENXIO;
	else
		physio(hpstrategy, &rhpbuf[unit], dev, B_READ);
#else
	physio(hpstrategy, &rhpbuf, dev, B_READ);
#endif
}

hpwrite(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NHP)
		u.u_error = ENXIO;
	else
		physio(hpstrategy, &rhpbuf[unit], dev, B_WRITE);
#else
	physio(hpstrategy, &rhpbuf, dev, B_WRITE);
#endif
}

#ifdef	UCB_ECC
#define	exadr(x,y)	(((long)(x) << 16) | (unsigned)(y))

/*
 * Correct an ECC error and restart the i/o to complete
 * the transfer if necessary.  This is quite complicated because
 * the correction may be going to an odd memory address base
 * and the transfer may cross a sector boundary.
 */
hpecc(bp)
register struct	buf *bp;
{
	register struct hpdevice *hpaddr = HPADDR;
	register unsigned byte;
	ubadr_t	bb, addr;
	long	wrong;
	int	bit, wc;
	unsigned ndone, npx;
	int	ocmd;
	int	cn, tn, sn;
	daddr_t	bn;
#ifdef	UNIBUS_MAP
	struct	ubmap *ubp;
#endif

	/*
	 *	ndone is #bytes including the error
	 *	which is assumed to be in the last disk page transferred.
	 */
	wc = hpaddr->hpwc;
	ndone = (wc * NBPW) + bp->b_bcount;
	npx = ndone / PGSIZE;
	printf("hp%d%c: soft ecc bn %D\n",
		dkunit(bp), 'a' + (minor(bp->b_dev) & 07),
		bp->b_blkno + (npx - 1));
	wrong = hpaddr->hpec2;
	if (wrong == 0) {
		hpaddr->hpof = HPOF_FMT22;
		hpaddr->hpcs1.w |= HP_IE;
		return (0);
	}

	/*
	 *	Compute the byte/bit position of the err
	 *	within the last disk page transferred.
	 *	Hpec1 is origin-1.
	 */
	byte = hpaddr->hpec1 - 1;
	bit = byte & 07;
	byte >>= 3;
	byte += ndone - PGSIZE;
	bb = exadr(bp->b_xmem, bp->b_un.b_addr);
	wrong <<= bit;

	/*
	 *	Correct until mask is zero or until end of transfer,
	 *	whichever comes first.
	 */
	while (byte < bp->b_bcount && wrong != 0) {
		addr = bb + byte;
#ifdef	UNIBUS_MAP
		if (bp->b_flags & (B_MAP|B_UBAREMAP)) {
			/*
			 * Simulate UNIBUS map if UNIBUS transfer.
			 */
			ubp = UBMAP + ((addr >> 13) & 037);
			addr = exadr(ubp->ub_hi, ubp->ub_lo) + (addr & 017777);
		}
#endif
		putmemc(addr, getmemc(addr) ^ (int) wrong);
		byte++;
		wrong >>= 8;
	}

	hptab.b_active++;
	if (wc == 0)
		return (0);

	/*
	 * Have to continue the transfer.  Clear the drive
	 * and compute the position where the transfer is to continue.
	 * We have completed npx sectors of the transfer already.
	 */
	ocmd = (hpaddr->hpcs1.w & ~HP_RDY) | HP_IE | HP_GO;
	hpaddr->hpcs2.w = dkunit(bp);
	hpaddr->hpcs1.w = HP_TRE | HP_DCLR | HP_GO;

	bn = dkblock(bp);
	cn = bp->b_cylin - bn / (HP_NSECT * HP_NTRAC);
	bn += npx;
	addr = bb + ndone;

	cn += bn / (HP_NSECT * HP_NTRAC);
	sn = bn % (HP_NSECT * HP_NTRAC);
	tn = sn / HP_NSECT;
	sn %= HP_NSECT;

	hpaddr->hpdc = cn;
	hpaddr->hpda = (tn << 8) + sn;
	hpaddr->hpwc = ((int)(ndone - bp->b_bcount)) / NBPW;
	hpaddr->hpba = (int) addr;
#if	PDP11 == 70 || PDP11 == GENERIC
	if (hptab.b_flags & B_RH70)
		hpaddr->hpbae = (int) (addr >> 16);
#endif
	hpaddr->hpcs1.w = ocmd;
	return (1);
}
#endif	UCB_ECC

#if	defined(HP_DUMP) && defined(UCB_AUTOBOOT)
/*
 *  Dump routine for RP04/05/06.
 *  Dumps from dumplo to end of memory/end of disk section for minor(dev).
 *  It uses the UNIBUS map to dump all of memory if there is a UNIBUS map
 *  and this isn't an RH70.  This depends on UNIBUS_MAP being defined.
 */

#ifdef	UNIBUS_MAP
#define	DBSIZE	(UBPAGE/PGSIZE)		/* unit of transfer, one UBPAGE */
#else
#define DBSIZE	16			/* unit of transfer, same number */
#endif

hpdump(dev)
dev_t	dev;
{
	register struct hpdevice *hpaddr = HPADDR;
	daddr_t	bn, dumpsize;
	long	paddr;
	register sn;
	register count;
#ifdef	UNIBUS_MAP
	extern	bool_t ubmap;
	register struct ubmap *ubp;
#endif

	if ((bdevsw[major(dev)].d_strategy != hpstrategy)	/* paranoia */
	    || ((dev=minor(dev)) > (NHP << 3)))
		return(EINVAL);
	dumpsize = hp_sizes[dev & 07].nblocks;
	if ((dumplo < 0) || (dumplo >= dumpsize))
		return(EINVAL);
	dumpsize -= dumplo;

	hpaddr->hpcs2.w = dev >> 3;
	if ((hpaddr->hpds & HPDS_VV) == 0) {
		hpaddr->hpcs1.w = HP_DCLR | HP_GO;
		hpaddr->hpcs1.w = HP_PRESET | HP_GO;
		hpaddr->hpof = HPOF_FMT22;
	}
	if ((hpaddr->hpds & (HPDS_DPR | HPDS_MOL)) != (HPDS_DPR | HPDS_MOL))
		return(EFAULT);
	dev &= 07;
#ifdef	UNIBUS_MAP
	ubp = &UBMAP[0];
#endif
	for (paddr = 0L; dumpsize > 0; dumpsize -= count) {
		count = dumpsize>DBSIZE? DBSIZE: dumpsize;
		bn = dumplo + (paddr >> PGSHIFT);
		hpaddr->hpdc = bn / (HP_NSECT*HP_NTRAC) + hp_sizes[dev].cyloff;
		sn = bn % (HP_NSECT * HP_NTRAC);
		hpaddr->hpda = ((sn / HP_NSECT) << 8) | (sn % HP_NSECT);
		hpaddr->hpwc = -(count << (PGSHIFT - 1));
#ifdef	UNIBUS_MAP
		/*
		 *  If UNIBUS_MAP exists, use
		 *  the map, unless on an 11/70 with RH70.
		 */
		if (ubmap && ((hptab.b_flags & B_RH70) == 0)) {
			ubp->ub_lo = loint(paddr);
			ubp->ub_hi = hiint(paddr);
			hpaddr->hpba = 0;
			hpaddr->hpcs1.w = HP_WCOM | HP_GO;
		}
		else
#endif
			{
			/*
			 *  Non-UNIBUS map, or 11/70 RH70 (MASSBUS)
			 */
			hpaddr->hpba = loint(paddr);
#if	PDP11 == 70 || PDP11 == GENERIC
			if (hptab.b_flags & B_RH70)
				hpaddr->hpbae = hiint(paddr);
#endif
			hpaddr->hpcs1.w = HP_WCOM | HP_GO | ((paddr >> 8) & (03 << 8));
		}
		while (hpaddr->hpcs1.w & HP_GO)
			;
		if (hpaddr->hpcs1.w & HP_TRE) {
			if (hpaddr->hpcs2.w & HPCS2_NEM)
				return(0);	/* made it to end of memory */
			return(EIO);
		}
		paddr += (DBSIZE << PGSHIFT);
	}
	return(0);		/* filled disk minor dev */
}
#endif	HP_DUMP
#endif	NHP
