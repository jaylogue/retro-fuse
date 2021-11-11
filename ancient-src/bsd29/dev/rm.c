/*
 *	SCCS id	@(#)rm.c	2.1 (Berkeley)	9/1/83
 */

/*
 *	RJM02/RWM03 disk driver
 *
 *	For simplicity we use hpreg.h instead of an rmreg.h.  The bits
 *	are the same.  Only the register names have been changed to
 *	protect the innocent.
 */

#include "rm.h"
#if	NRM > 0
#include "param.h"
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/hpreg.h>
#ifndef	INTRLVE
#include <sys/inline.h>
#endif
#ifdef	RM_DUMP
#include <sys/seg.h>
#endif
#include <sys/uba.h>

extern	struct	hpdevice *RMADDR;
extern	struct	size rm_sizes[];

int	rm_offset[] =
{
	HPOF_P400,	HPOF_M400,	HPOF_P400,	HPOF_M400,
	HPOF_P800,	HPOF_M800,	HPOF_P800,	HPOF_M800,
	HPOF_P1200,	HPOF_M1200,	HPOF_P1200,	HPOF_M1200,
	0,		0,		0,		0
};

#define	RM_NSECT	32
#define	RM_NTRAC	5
#define	RM_SDIST	2
#define	RM_RDIST	6

struct	buf	rmtab;
#ifdef	UCB_DBUFS
struct	buf	rrmbuf[NRM];
#else
struct	buf	rrmbuf;
#endif
#if	NRM > 1
struct	buf	rmutab[NRM];
#endif

#ifdef	INTRLVE
extern	daddr_t	dkblock();
#endif
#if	NRM > 1
int	rmcc[NRM]; 	/* Current cylinder */
#endif

void
rmroot()
{
	rmattach(RMADDR, 0);
}

rmattach(addr, unit)
register struct hpdevice *addr;
{
	if (unit != 0)
		return(0);
	if ((addr != (struct hpdevice *) NULL) && (fioword(addr) != -1)) {
		RMADDR = addr;
#if	PDP11 == 70 || PDP11 == GENERIC
		if (fioword(&(addr->hpbae)) != -1)
			rmtab.b_flags |= B_RH70;
#endif
		return(1);
	}
	RMADDR = (struct hpdevice *) NULL;
	return(0);
}

rmstrategy(bp)
register struct	buf *bp;
{
	register struct buf *dp;
	register unit;
	long	bn;

	unit = minor(bp->b_dev) & 077;
	if (unit >= (NRM << 3) || (RMADDR == (struct hpdevice *) NULL)) {
		bp->b_error = ENXIO;
		goto errexit;
	}
	if (bp->b_blkno < 0 ||
	    (bn = dkblock(bp)) + (long) ((bp->b_bcount + 511) >> 9)
	    > rm_sizes[unit & 07].nblocks) {
		bp->b_error = EINVAL;
errexit:
		bp->b_flags |= B_ERROR;
		iodone(bp);
		return;
	}
#ifdef	UNIBUS_MAP
	if ((rmtab.b_flags & B_RH70) == 0)
		mapalloc(bp);
#endif
	bp->b_cylin = bn / (RM_NSECT * RM_NTRAC) + rm_sizes[unit & 07].cyloff;
	unit = dkunit(bp);
#if	NRM > 1
	dp = &rmutab[unit];
#else
	dp = &rmtab;
#endif
	(void) _spl5();
	disksort(dp, bp);
	if (dp->b_active == 0) {
#if	NRM > 1
		rmustart(unit);
		if (rmtab.b_active == 0)
			rmstart();
#else
		rmstart();
#endif
	}
	(void) _spl0();
}

#if	NRM > 1
rmustart(unit)
register unit;
{
	register struct hpdevice *rmaddr = RMADDR;
	register struct	buf *dp;
	struct	buf *bp;
	daddr_t	bn;
	int	sn, cn, csn;

	rmaddr->hpcs2.w = unit;
	rmaddr->hpcs1.c[0] = HP_IE;
	rmaddr->hpas = 1 << unit;

	if (unit >= NRM)
		return;
#ifdef	RM_DKN
	dk_busy &= ~(1 << (unit + RM_DKN));
#endif
	dp = &rmutab[unit];
	if ((bp=dp->b_actf) == NULL)
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
	if ((rmaddr->hpds & HPDS_VV) == 0) {
		/* SHOULD WARN SYSTEM THAT THIS HAPPENED */
		rmaddr->hpcs1.c[0] = HP_IE | HP_PRESET | HP_GO;
		rmaddr->hpof = HPOF_FMT22;
	}
	/*
	 * If drive is offline, forget about positioning.
	 */
	if ((rmaddr->hpds & (HPDS_DPR | HPDS_MOL)) != (HPDS_DPR | HPDS_MOL))
		goto done;

	bn = dkblock(bp);
	cn = bp->b_cylin;
	sn = bn % (RM_NSECT * RM_NTRAC);
	sn = (sn + RM_NSECT - RM_SDIST) % RM_NSECT;

	if (rmcc[unit] != cn)
		goto search;
	csn = (rmaddr->hpla >> 6) - sn + RM_SDIST - 1;
	if (csn < 0)
		csn += RM_NSECT;
	if (csn > RM_NSECT - RM_RDIST)
		goto done;

search:
	rmaddr->hpdc = cn;
	rmaddr->hpda = sn;
	rmaddr->hpcs1.c[0] = HP_IE | HP_SEARCH | HP_GO;
	rmcc[unit] = cn;
	/*
	 * Mark unit busy for iostat.
	 */
#ifdef	RM_DKN
	unit += RM_DKN;
	dk_busy |= 1 << unit;
	dk_numb[unit]++;
#endif
	return;

done:
	/*
	 * Device is ready to go.
	 * Put it on the ready queue for the controller.
	 */
	dp->b_forw = NULL;
	if (rmtab.b_actf == NULL)
		rmtab.b_actf = dp;
	else
		rmtab.b_actl->b_forw = dp;
	rmtab.b_actl = dp;
}
#endif

/*
 * Start up a transfer on a drive.
 */
rmstart()
{
	register struct hpdevice *rmaddr = RMADDR;
	register struct	buf *bp;
	struct buf *dp;
	register unit;
	daddr_t	bn;
	int	dn, sn, tn, cn;

loop:
	/*
	 * Pull a request off the controller queue.
	 */
#if	NRM == 1
	dp = &rmtab;
	if ((bp = dp->b_actf) == NULL)
		return;
#else
	if ((dp = rmtab.b_actf) == NULL)
		return;
	if ((bp = dp->b_actf) == NULL) {
		rmtab.b_actf = dp->b_forw;
		goto loop;
	}
#endif
	/*
	 * Mark controller busy and
	 * determine destination of this request.
	 */
	rmtab.b_active++;
	unit = minor(bp->b_dev) & 077;
	dn = dkunit(bp);
	bn = dkblock(bp);
	cn = bn / (RM_NSECT * RM_NTRAC) + rm_sizes[unit & 07].cyloff;
	sn = bn % (RM_NSECT * RM_NTRAC);
	tn = sn / RM_NSECT;
	sn = sn % RM_NSECT;

	/*
	 * Select drive.
	 */
	rmaddr->hpcs2.w = dn;
#if	NRM == 1
	/*
	 * If drive has just come up,
	 * set up the pack.
	 */
	if ((rmaddr->hpds & HPDS_VV) == 0) {
		/* SHOULD WARN SYSTEM THAT THIS HAPPENED */
		rmaddr->hpcs1.c[0] = HP_IE | HP_PRESET | HP_GO;
		rmaddr->hpof = HPOF_FMT22;
	}
#endif
	/*
	 * Check that it is ready and online.
	 */
	if ((rmaddr->hpds & (HPDS_DPR | HPDS_MOL)) != (HPDS_DPR | HPDS_MOL)) {
		rmtab.b_active = 0;
		rmtab.b_errcnt = 0;
		dp->b_actf = bp->av_forw;
		bp->b_flags |= B_ERROR;
		iodone(bp);
		goto loop;
	}
	if (rmtab.b_errcnt >= 16 && (bp->b_flags & B_READ)) {
		rmaddr->hpof = rm_offset[rmtab.b_errcnt & 017] | HPOF_FMT22;
		rmaddr->hpcs1.w = HP_OFFSET | HP_GO;
		while ((rmaddr->hpds & (HPDS_PIP | HPDS_DRY)) != HPDS_DRY)
			;
	}
	rmaddr->hpdc = cn;
	rmaddr->hpda = (tn << 8) + sn;
	rmaddr->hpba = bp->b_un.b_addr;
#if	PDP11 == 70 || PDP11 == GENERIC
	if (rmtab.b_flags & B_RH70)
		rmaddr->hpbae = bp->b_xmem;
#endif
	rmaddr->hpwc = -(bp->b_bcount >> 1);
	/*
	 * Warning:  unit is being used as a temporary.
	 */
	unit = ((bp->b_xmem & 3) << 8) | HP_IE | HP_GO;
	if (bp->b_flags & B_READ)
		unit |= HP_RCOM;
	else
		unit |= HP_WCOM;
	rmaddr->hpcs1.w = unit;

#ifdef	RM_DKN
	dk_busy |= 1 << (RM_DKN + NRM);
	dk_numb[RM_DKN + NRM]++;
	dk_wds[RM_DKN + NRM] += bp->b_bcount >> 6;
#endif
}

/*
 * Handle a disk interrupt.
 */
rmintr()
{
	register struct hpdevice *rmaddr = RMADDR;
	register struct	buf *bp, *dp;
	short	unit;
	int	as, i, j;

	as = rmaddr->hpas & 0377;
	if (rmtab.b_active) {
#ifdef	RM_DKN
		dk_busy &= ~(1 << (RM_DKN + NRM));
#endif
		/*
		 * Get device and block structures.  Select the drive.
		 */
#if	NRM > 1
		dp = rmtab.b_actf;
#else
		dp = &rmtab;
#endif
		bp = dp->b_actf;
		unit = dkunit(bp);
		rmaddr->hpcs2.c[0] = unit;
		/*
		 * Check for and process errors.
		 */
		if (rmaddr->hpcs1.w & HP_TRE) {		/* error bit */
			while ((rmaddr->hpds & HPDS_DRY) == 0)
				;
			if (rmaddr->hper1 & HPER1_WLE) {
				/*
				 *	Give up on write locked devices
				 *	immediately.
				 */
				printf("rm%d: write locked\n", unit);
				bp->b_flags |= B_ERROR;
			} else {
				/*
				 * After 28 retries (16 without offset and
				 * 12 with offset positioning), give up.
				 */
				if (++rmtab.b_errcnt > 28) {
				    bp->b_flags |= B_ERROR;
#ifdef	UCB_DEVERR
				    harderr(bp, "rm");
				    printf("cs2=%b er1=%b\n", rmaddr->hpcs2.w,
					HPCS2_BITS, rmaddr->hper1, HPER1_BITS);
#else
				    deverror(bp, rmaddr->hpcs2.w, rmaddr->hper1);
#endif
				} else
				    rmtab.b_active = 0;
			}
#ifdef	UCB_ECC
			/*
			 * If soft ecc, correct it (continuing
			 * by returning if necessary).
			 * Otherwise, fall through and retry the transfer.
			 */
			if((rmaddr->hper1 & (HPER1_DCK|HPER1_ECH)) == HPER1_DCK)
				if (rmecc(bp))
					return;
#endif
			rmaddr->hpcs1.w = HP_TRE | HP_IE | HP_DCLR | HP_GO;
			if ((rmtab.b_errcnt & 07) == 4) {
				rmaddr->hpcs1.w = HP_RECAL | HP_IE | HP_GO;
				while ((rmaddr->hpds & (HPDS_PIP | HPDS_DRY)) != HPDS_DRY)
					;
			}
#if	NRM > 1
			rmcc[unit] = -1;
#endif
		}
		if (rmtab.b_active) {
			if (rmtab.b_errcnt) {
				rmaddr->hpcs1.w = HP_RTC | HP_GO;
				while ((rmaddr->hpds & (HPDS_PIP | HPDS_DRY)) != HPDS_DRY)
					;
			}
			rmtab.b_errcnt = 0;
#if	NRM > 1
			rmtab.b_active = 0;
			rmtab.b_actf = dp->b_forw;
			rmcc[unit] = bp->b_cylin;
#endif
			dp->b_active = 0;
			dp->b_actf = bp->b_actf;
			bp->b_resid = - (rmaddr->hpwc << 1);
			iodone(bp);
			rmaddr->hpcs1.w = HP_IE;
#if	NRM > 1
			if (dp->b_actf)
				rmustart(unit);
#endif
		}
#if	NRM > 1
		as &= ~(1 << unit);
#endif
	} else {
		if (as == 0)
			rmaddr->hpcs1.w	= HP_IE;
		rmaddr->hpcs1.c[1] = HP_TRE >> 8;
	}
#if	NRM > 1
	for(unit = 0; unit < NRM; unit++)
		if (as & (1 << unit))
			rmustart(unit);
#endif
	rmstart();
}

rmread(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NRM)
		u.u_error = ENXIO;
	else
		physio(rmstrategy, &rrmbuf[unit], dev, B_READ);
#else
	physio(rmstrategy, &rrmbuf, dev, B_READ);
#endif
}

rmwrite(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NRM)
		u.u_error = ENXIO;
	else
		physio(rmstrategy, &rrmbuf[unit], dev, B_WRITE);
#else
	physio(rmstrategy, &rrmbuf, dev, B_WRITE);
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
rmecc(bp)
register struct	buf *bp;
{
	register struct hpdevice *rmaddr = RMADDR;
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
	wc = rmaddr->hpwc;
	ndone = (wc * NBPW) + bp->b_bcount;
	npx = ndone / PGSIZE;
	printf("rm%d%c:  soft ecc bn %D\n",
		dkunit(bp), 'a' + (minor(bp->b_dev) & 07),
		bp->b_blkno + (npx - 1));
	wrong = rmaddr->hpec2;
	if (wrong == 0) {
		rmaddr->hpof = HPOF_FMT22;
		rmaddr->hpcs1.w |= HP_IE;
		return (0);
	}

	/*
	 *	Compute the byte/bit position of the err
	 *	within the last disk page transferred.
	 *	Hpec1 is origin-1.
	 */
	byte = rmaddr->hpec1 - 1;
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
			 * Simulate UNIBUS map if UNIBUS transfer
			 */
			ubp = UBMAP + ((addr >> 13) & 037);
			addr = exadr(ubp->ub_hi, ubp->ub_lo) + (addr & 017777);
		}
#endif
		putmemc(addr, getmemc(addr) ^ (int) wrong);
		byte++;
		wrong >>= 8;
	}

	rmtab.b_active++;
	if (wc == 0)
		return (0);

	/*
	 * Have to continue the transfer.  Clear the drive
	 * and compute the position where the transfer is to continue.
	 * We have completed npx sectors of the transfer already.
	 */
	ocmd = (rmaddr->hpcs1.w & ~HP_RDY) | HP_IE | HP_GO;
	rmaddr->hpcs2.w = dkunit(bp);
	rmaddr->hpcs1.w = HP_TRE | HP_DCLR | HP_GO;

	bn = dkblock(bp);
	cn = bp->b_cylin - bn / (RM_NSECT * RM_NTRAC);
	bn += npx;
	addr = bb + ndone;

	cn += bn / (RM_NSECT * RM_NTRAC);
	sn = bn % (RM_NSECT * RM_NTRAC);
	tn = sn / RM_NSECT;
	sn %= RM_NSECT;

	rmaddr->hpdc = cn;
	rmaddr->hpda = (tn << 8) + sn;
	rmaddr->hpwc = ((int)(ndone - bp->b_bcount)) / NBPW;
	rmaddr->hpba = (int) addr;
#if	PDP11 == 70 || PDP11 == GENERIC
	if (rmtab.b_flags & B_RH70)
		rmaddr->hpbae = (int) (addr >> 16);
#endif
	rmaddr->hpcs1.w = ocmd;
	return (1);
}
#endif	UCB_ECC

#if	defined(RM_DUMP) && defined(UCB_AUTOBOOT)
/*
 *  Dump routine for RM02/RM03.
 *  Dumps from dumplo to end of memory/end of disk section for minor(dev).
 *  It uses the UNIBUS map to dump all of memory if there is a UNIBUS map
 *  and this isn't an RM03.  This depends on UNIBUS_MAP being defined.
 *  If there is no UNIBUS map, it will work with any definitions.
 */

#ifdef	UNIBUS_MAP
#define	DBSIZE	(UBPAGE/PGSIZE)		/* unit of transfer, one UBPAGE */
#else
#define DBSIZE	16			/* unit of transfer, same number */
#endif

rmdump(dev)
dev_t	dev;
{
	register struct hpdevice *rmaddr = RMADDR;
	daddr_t	bn, dumpsize;
	long	paddr;
	register sn;
	register count;
#ifdef	UNIBUS_MAP
	extern	bool_t ubmap;
	register struct ubmap *ubp;
#endif

	if ((bdevsw[major(dev)].d_strategy != rmstrategy)	/* paranoia */
	    || ((dev=minor(dev)) > (NRM << 3)))
		return(EINVAL);
	dumpsize = rm_sizes[dev & 07].nblocks;
	if ((dumplo < 0) || (dumplo >= dumpsize))
		return(EINVAL);
	dumpsize -= dumplo;

	rmaddr->hpcs2.w = dev >> 3;
	if ((rmaddr->hpds & HPDS_VV) == 0) {
		rmaddr->hpcs1.w = HP_DCLR | HP_GO;
		rmaddr->hpcs1.w = HP_PRESET | HP_GO;
		rmaddr->hpof = HPOF_FMT22;
	}
	if ((rmaddr->hpds & (HPDS_DPR | HPDS_MOL)) != (HPDS_DPR | HPDS_MOL))
		return(EFAULT);
	dev &= 07;
#ifdef	UNIBUS_MAP
	ubp = &UBMAP[0];
#endif
	for (paddr = 0L; dumpsize > 0; dumpsize -= count) {
		count = dumpsize>DBSIZE? DBSIZE: dumpsize;
		bn = dumplo + (paddr >> PGSHIFT);
		rmaddr->hpdc = bn / (RM_NSECT*RM_NTRAC) + rm_sizes[dev].cyloff;
		sn = bn % (RM_NSECT * RM_NTRAC);
		rmaddr->hpda = ((sn / RM_NSECT) << 8) | (sn % RM_NSECT);
		rmaddr->hpwc = -(count << (PGSHIFT - 1));
		/*
		 *  If UNIBUS_MAP exists, use
		 *  the map, unless on an 11/70 with RM03.
		 */
#ifdef	UNIBUS_MAP
		if (ubmap && ((rmtab.b_flags & B_RH70) == 0)) {
			ubp->ub_lo = loint(paddr);
			ubp->ub_hi = hiint(paddr);
			rmaddr->hpba = 0;
			rmaddr->hpcs1.w = HP_WCOM | HP_GO;
		}
		else
#endif
			{
			/*
			 *  Non-UNIBUS map, or 11/70 RM03 (MASSBUS)
			 */
			rmaddr->hpba = loint(paddr);
#if	PDP11 == 70 || PDP11 == GENERIC
			if (rmtab.b_flags & B_RH70)
				rmaddr->hpbae = hiint(paddr);
#endif
			rmaddr->hpcs1.w = HP_WCOM | HP_GO | ((paddr >> 8) & (03 << 8));
		}
		while (rmaddr->hpcs1.w & HP_GO)
			;
		if (rmaddr->hpcs1.w & HP_TRE) {
			if (rmaddr->hpcs2.w & HPCS2_NEM)
				return(0);	/* made it to end of memory */
			return(EIO);
		}
		paddr += (DBSIZE << PGSHIFT);
	}
	return(0);		/* filled disk minor dev */
}
#endif	RM_DUMP
#endif	NRM
