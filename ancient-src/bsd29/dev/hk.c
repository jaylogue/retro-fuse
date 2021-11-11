/*
 *	SCCS id	@(#)hk.c	2.1 (Berkeley)	12/21/83
 */

/*
 * RK611/RK0[67] disk driver
 *
 * This driver mimics the 4.1bsd rk driver.
 * It does overlapped seeks, ECC, and bad block handling.
 *
 * salkind@nyu
 */

#include "hk.h"
#if	NHK > 0
#include "param.h"
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/uba.h>
#ifndef	INTRLVE
#include <sys/inline.h>
#endif
#include <sys/hkreg.h>
#include <sys/dkbad.h>

#define	NHK7CYL	815
#define	NHK6CYL	411
#define	HK_NSECT	22
#define	HK_NTRAC	3
#define	HK_NSPC		(HK_NTRAC*HK_NSECT)

extern	struct size hk_sizes[];
extern	struct hkdevice *HKADDR;

int	hkpip;		/* DEBUG */
int	hknosval;	/* DEBUG */
#ifdef HKDEBUG
int	hkdebug;
#endif

int	hk_offset[] =
{
	HKAS_P400,	HKAS_M400,	HKAS_P400,	HKAS_M400,
	HKAS_P800,	HKAS_M800,	HKAS_P800,	HKAS_M800,
	HKAS_P1200,	HKAS_M1200,	HKAS_P1200,	HKAS_M1200,
	0,		0,		0,		0,
};

int	hk_type[NHK];
int	hk_cyl[NHK];
char	hk_mntflg[NHK];
char	hk_pack[NHK];

struct hk_softc {
	int	sc_softas;
	int	sc_recal;
} hk;

struct	buf	hktab;
struct	buf	hkutab[NHK];
#ifdef	UCB_DBUFS
struct	buf	rhkbuf[NHK];
#else
struct	buf	rhkbuf;
#endif
#ifdef BADSECT
struct	dkbad	hkbad[NHK];
struct	buf	bhkbuf[NHK];
#endif

#define	hkwait(hkaddr)		while ((hkaddr->hkcs1 & HK_CRDY) == 0)
#define	hkncyl(unit)		(hk_type[unit] ? NHK7CYL : NHK6CYL)

#ifdef	INTRLVE
extern	daddr_t	dkblock();
#endif

void
hkroot()
{
	hkattach(HKADDR, 0);
}

hkattach(addr, unit)
struct hkdevice *addr;
{
	if (unit == 0) {
		HKADDR = addr;
		return(1);
	}
	return(0);
}

hkdsel(unit)
register unit;
{
	register struct hkdevice *hkaddr = HKADDR;

	hk_type[unit] = 0;
	hkaddr->hkcs1 = HK_CCLR;
	hkaddr->hkcs2 = unit;
	hkaddr->hkcs1 = HK_DCLR | HK_GO;
	hkwait(hkaddr);
	if((hkaddr->hkcs2&HKCS2_NED) || (hkaddr->hkds&HKDS_SVAL) == 0) {
		hkaddr->hkcs1 = HK_CCLR;
		hkwait(hkaddr);
		return(-1);
	}
	if((hkaddr->hkcs1&HK_CERR) && (hkaddr->hker&HKER_DTYE)) {
		hk_type[unit] = HK_CDT;
		hkaddr->hkcs1 = HK_CCLR;
		hkwait(hkaddr);
	}

	hk_mntflg[unit] = 1;
	hk_cyl[unit] = -1;
	return(0);
}

hkstrategy(bp)
register struct buf *bp;
{
	register struct buf *dp;
	register unit;
	int s;
	long bn;
	long sz;

	unit = minor(bp->b_dev) & 077;
	sz = (bp->b_bcount + 511) >> 9;
	if ((unit >= (NHK << 3)) || (HKADDR == (struct hkdevice *) NULL)) {
		bp->b_error = ENXIO;
		goto bad;
	}
	if (bp->b_blkno < 0 || (bn = dkblock(bp))+sz > hk_sizes[unit & 07].nblocks) {
		bp->b_error = EINVAL;
		goto bad;
	}
	bp->b_cylin = bn / HK_NSPC + hk_sizes[unit & 07].cyloff;
	unit = dkunit(bp);
	if (hk_mntflg[unit] == 0) {
		/* SHOULD BE DONE AT BOOT TIME */
		if (hkdsel(unit) < 0)
			goto bad;
	}
#ifdef UNIBUS_MAP
	mapalloc(bp);
#endif
	dp = &hkutab[unit];
	s = spl5();
	disksort(dp, bp);
	if (dp->b_active == 0) {
		hkustart(unit);
		if (hktab.b_active == 0)
			hkstart();
	}
	splx(s);
	return;
bad:
	bp->b_flags |= B_ERROR;
	iodone(bp);
}

hkustart(unit)
	int unit;
{
	register struct hkdevice *hkaddr = HKADDR;
	register struct buf *bp, *dp;
	int didie = 0;

	if (unit >= NHK || hk_mntflg[unit] == 0)
		return(0);
#ifdef	HK_DKN
	dk_busy &= ~(1<<(unit + HK_DKN));
#endif
	if (hktab.b_active) {
		hk.sc_softas |= (1 << unit);
		return(0);
	}

	hkaddr->hkcs1 = HK_CCLR;
	hkaddr->hkcs2 = unit;
	hkaddr->hkcs1 = hk_type[unit] | HK_DCLR | HK_GO;
	hkwait(hkaddr);

	dp = &hkutab[unit];
	if ((bp = dp->b_actf) == NULL)
		return(0);
	if (dp->b_active)
		goto done;
	dp->b_active = 1;
	if ((hkaddr->hkds & HKDS_VV) == 0 || hk_pack[unit] == 0) {
		/* SHOULD WARN SYSTEM THAT THIS HAPPENED */
#ifdef BADSECT
		struct buf *bbp = &bhkbuf[unit];
#endif

		hkaddr->hkcs1 = hk_type[unit]|HK_PACK|HK_GO;
		hk_pack[unit]++;
#ifdef BADSECT
		bbp->b_flags = B_READ|B_BUSY|B_PHYS;
		bbp->b_dev = bp->b_dev;
		bbp->b_bcount = sizeof(struct dkbad);
		bbp->b_un.b_addr = (caddr_t)&hkbad[unit];
		bbp->b_blkno = (long)hkncyl(unit)*HK_NSPC - HK_NSECT;
		bbp->b_cylin = hkncyl(unit) - 1;
#ifdef UNIBUS_MAP
		mapalloc(bbp);
#endif
		dp->b_actf = bbp;
		bbp->av_forw = bp;
		bp = bbp;
#endif
		hkwait(hkaddr);
	}
	if ((hkaddr->hkds & HKDS_DREADY) != HKDS_DREADY)
		goto done;
#ifdef	NHK > 1
	if (bp->b_cylin == hk_cyl[unit])
		goto done;
	hkaddr->hkcyl = bp->b_cylin;
	hk_cyl[unit] = bp->b_cylin;
	hkaddr->hkcs1 = hk_type[unit] | HK_IE | HK_SEEK | HK_GO;
	didie = 1;
#ifdef	HK_DKN
	unit += HK_DKN;
	dk_busy |= 1 << unit;
	dk_numb[unit] += 1;
#endif	HK_DKN
	return (didie);
#endif	NHK > 1

done:
	if (dp->b_active != 2) {
		dp->b_forw = NULL;
		if (hktab.b_actf == NULL)
			hktab.b_actf = dp;
		else
			hktab.b_actl->b_forw = dp;
		hktab.b_actl = dp;
		dp->b_active = 2;
	}
	return (didie);
}

hkstart()
{
	register struct buf *bp, *dp;
	register struct hkdevice *hkaddr = HKADDR;
	daddr_t bn;
	int sn, tn, cmd, unit;

loop:
	if ((dp = hktab.b_actf) == NULL)
		return(0);
	if ((bp = dp->b_actf) == NULL) {
		hktab.b_actf = dp->b_forw;
		goto loop;
	}
	hktab.b_active++;
	unit = dkunit(bp);
	bn = dkblock(bp);

	sn = bn % HK_NSPC;
	tn = sn / HK_NSECT;
	sn %= HK_NSECT;
retry:
	hkaddr->hkcs1 = HK_CCLR;
	hkaddr->hkcs2 = unit;
	hkaddr->hkcs1 = hk_type[unit] | HK_DCLR | HK_GO;
	hkwait(hkaddr);

	if ((hkaddr->hkds & HKDS_SVAL) == 0) {
		hknosval++;
		goto nosval;
	}
	if (hkaddr->hkds & HKDS_PIP) {
		hkpip++;
		goto retry;
	}
	if ((hkaddr->hkds&HKDS_DREADY) != HKDS_DREADY) {
		printf("hk%d: not ready", unit);
		if ((hkaddr->hkds&HKDS_DREADY) != HKDS_DREADY) {
			printf("\n");
			hkaddr->hkcs1 = hk_type[unit] | HK_DCLR | HK_GO;
			hkwait(hkaddr);
			hkaddr->hkcs1 = HK_CCLR;
			hkwait(hkaddr);
			hktab.b_active = 0;
			hktab.b_errcnt = 0;
			dp->b_actf = bp->av_forw;
			dp->b_active = 0;
			bp->b_flags |= B_ERROR;
			iodone(bp);
			goto loop;
		}
		else
			printf(" (came back!)\n");
	}
nosval:
	hkaddr->hkcyl = bp->b_cylin;
	hk_cyl[unit] = bp->b_cylin;
	hkaddr->hkda = (tn << 8) + sn;
	hkaddr->hkwc = -(bp->b_bcount >> 1);
	hkaddr->hkba = bp->b_un.b_addr;
	cmd = hk_type[unit] | ((bp->b_xmem & 3) << 8) | HK_IE | HK_GO;
	if (bp->b_flags & B_READ)
		cmd |= HK_READ;
	else
		cmd |= HK_WRITE;
	hkaddr->hkcs1 = cmd;
#ifdef	HK_DKN
	dk_busy |= 1 << (HK_DKN + NHK);
	dk_numb[HK_DKN + NHK] += 1;
	dk_wds[HK_DKN + NHK] += bp->b_bcount >> 6;
#endif
	return(1);
}

hkintr()
{
	register struct hkdevice *hkaddr = HKADDR;
	register struct buf *bp, *dp;
	int unit;
	int as = (hkaddr->hkatt >> 8) | hk.sc_softas;
	int needie = 1;

	hk.sc_softas = 0;
	if (hktab.b_active) {
		dp = hktab.b_actf;
		bp = dp->b_actf;
		unit = dkunit(bp);
#ifdef	HK_DKN
		dk_busy &= ~(1 << (HK_DKN + NHK));
#endif
#ifdef BADSECT
		if (bp->b_flags&B_BAD)
			if (hkecc(bp, CONT))
				return;
#endif
		if (hkaddr->hkcs1 & HK_CERR) {
			int recal;
			u_short ds = hkaddr->hkds;
			u_short cs2 = hkaddr->hkcs2;
			u_short er = hkaddr->hker;
#ifdef HKDEBUG
			if (hkdebug) {
				printf("cs2=%b ds=%b er=%b\n",
				    cs2, HKCS2_BITS, ds, 
				    HKDS_BITS, er, HKER_BITS);
			}
#endif
			if (er & HKER_WLE) {
				printf("hk%d: write locked\n", unit);
				bp->b_flags |= B_ERROR;
			} else if (++hktab.b_errcnt > 28 ||
			    ds&HKDS_HARD || er&HKER_HARD || cs2&HKCS2_HARD) {
hard:
#ifdef	UCB_DEVERR
				harderr(bp, "hk");
				printf("cs2=%b ds=%b er=%b\n",
				    cs2, HKCS2_BITS, ds, 
				    HKDS_BITS, er, HKER_BITS);
#else
				deverror(bp, cs2, er);
#endif
				bp->b_flags |= B_ERROR;
				hk.sc_recal = 0;
			} else if (er & HKER_BSE) {
#ifdef BADSECT
				if (hkecc(bp, BSE))
					return;
				else
#endif
					goto hard;
			} else
				hktab.b_active = 0;
			if (cs2&HKCS2_MDS) {
				hkaddr->hkcs2 = HKCS2_SCLR;
				goto retry;
			}
			recal = 0;
			if (ds&HKDS_DROT || er&(HKER_OPI|HKER_SKI|HKER_UNS) ||
			    (hktab.b_errcnt&07) == 4)
				recal = 1;
#ifdef	UCB_ECC
			if ((er & (HKER_DCK|HKER_ECH)) == HKER_DCK)
				if (hkecc(bp, ECC))
					return;
#endif
			hkaddr->hkcs1 = HK_CCLR;
			hkaddr->hkcs2 = unit;
			hkaddr->hkcs1 = hk_type[unit]|HK_DCLR|HK_GO;
			hkwait(hkaddr);
			if (recal && hktab.b_active == 0) {
				hkaddr->hkcs1 = hk_type[unit]|HK_IE|HK_RECAL|HK_GO;
				hk_cyl[unit] = -1;
				hk.sc_recal = 0;
				goto nextrecal;
			}
		}
retry:
		switch (hk.sc_recal) {

		case 1:
			hkaddr->hkcyl = bp->b_cylin;
			hk_cyl[unit] = bp->b_cylin;
			hkaddr->hkcs1 = hk_type[unit]|HK_IE|HK_SEEK|HK_GO;
			goto nextrecal;
		case 2:
			if (hktab.b_errcnt < 16 ||
			    (bp->b_flags&B_READ) == 0)
				goto donerecal;
			hkaddr->hkatt = hk_offset[hktab.b_errcnt & 017];
			hkaddr->hkcs1 = hk_type[unit]|HK_IE|HK_OFFSET|HK_GO;
			/* fall into ... */
		nextrecal:
			hk.sc_recal++;
			hkwait(hkaddr);
			hktab.b_active = 1;
			return;
		donerecal:
		case 3:
			hk.sc_recal = 0;
			hktab.b_active = 0;
			break;
		}
		if (hktab.b_active) {
			hktab.b_active = 0;
			hktab.b_errcnt = 0;
			hktab.b_actf = dp->b_forw;
			dp->b_active = 0;
			dp->b_errcnt = 0;
			dp->b_actf = bp->av_forw;
			bp->b_resid = -(hkaddr->hkwc << 1);
			iodone(bp);
			if (dp->b_actf)
				if (hkustart(unit))
					needie = 0;
		}
		as &= ~(1<<unit);
	}
	for (unit = 0; as; as >>= 1, unit++)
		if (as & 1) {
			if (unit < NHK && hk_mntflg[unit]) {
				if (hkustart(unit))
					needie = 0;
			} else {
				hkaddr->hkcs1 = HK_CCLR;
				hkaddr->hkcs2 = unit;
				hkaddr->hkcs1 = HK_DCLR | HK_GO;
				hkwait(hkaddr);
				hkaddr->hkcs1 = HK_CCLR;
			}
		}
	if (hktab.b_actf && hktab.b_active == 0)
		if (hkstart())
			needie = 0;
	if (needie)
		hkaddr->hkcs1 = HK_IE;
}

hkread(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NHK)
		u.u_error = ENXIO;
	else
		physio(hkstrategy, &rhkbuf[unit], dev, B_READ);
#else
	physio(hkstrategy, &rhkbuf, dev, B_READ);
#endif
}

hkwrite(dev)
dev_t	dev;
{
#ifdef	UCB_DBUFS
	register int unit = (minor(dev) >> 3) & 07;

	if (unit >= NHK)
		u.u_error = ENXIO;
	else
		physio(hkstrategy, &rhkbuf[unit], dev, B_WRITE);
#else
	physio(hkstrategy, &rhkbuf, dev, B_WRITE);
#endif
}

#ifdef	HK_DUMP
/*
 *  Dump routine for RK06/07
 *  Dumps from dumplo to end of memory/end of disk section for minor(dev).
 *  It uses the UNIBUS map to dump all of memory if there is a UNIBUS map.
 */
#ifdef	UNIBUS_MAP
#define	DBSIZE	(UBPAGE/PGSIZE)		/* unit of transfer, one UBPAGE */
#else
#define DBSIZE	16			/* unit of transfer, same number */
#endif

hkdump(dev)
	dev_t dev;
{
	register struct hkdevice *hkaddr = HKADDR;
	daddr_t	bn, dumpsize;
	long paddr;
	register count;
#ifdef	UNIBUS_MAP
	extern bool_t ubmap;
	register struct ubmap *ubp;
#endif
	int com, cn, tn, sn, unit;

	unit = minor(dev) >> 3;
	if ((bdevsw[major(dev)].d_strategy != hkstrategy)	/* paranoia */
	    || unit >= NHK)
		return(EINVAL);
	dumpsize = hk_sizes[minor(dev)&07].nblocks;
	if ((dumplo < 0) || (dumplo >= dumpsize))
		return(EINVAL);
	dumpsize -= dumplo;

	hkaddr->hkcs1 = HK_CCLR;
	hkwait(hkaddr);
	hkaddr->hkcs2 = unit;
	hkaddr->hkcs1 = hk_type[unit] | HK_DCLR | HK_GO;
	hkwait(hkaddr);
	if ((hkaddr->hkds & HKDS_VV) == 0) {
		hkaddr->hkcs1 = hk_type[unit]|HK_IE|HK_PACK|HK_GO;
		hkwait(hkaddr);
	}
#ifdef	UNIBUS_MAP
	ubp = &UBMAP[0];
#endif
	for (paddr = 0L; dumpsize > 0; dumpsize -= count) {
		count = dumpsize>DBSIZE? DBSIZE: dumpsize;
		bn = dumplo + (paddr >> PGSHIFT);
		cn = (bn/HK_NSPC) + hk_sizes[minor(dev)&07].cyloff;
		sn = bn%HK_NSPC;
		tn = sn/HK_NSECT;
		sn = sn%HK_NSECT;
		hkaddr->hkcyl = cn;
		hkaddr->hkda = (tn << 8) | sn;
		hkaddr->hkwc = -(count << (PGSHIFT-1));
		com = hk_type[unit]|HK_GO|HK_WRITE;
#ifdef	UNIBUS_MAP
		/*
		 *  If UNIBUS_MAP exists, use the map.
		 */
		if (ubmap) {
			ubp->ub_lo = loint(paddr);
			ubp->ub_hi = hiint(paddr);
			hkaddr->hkba = 0;
		} else {
#endif
			/* non UNIBUS map */
			hkaddr->hkba = loint(paddr);
			com |= ((paddr >> 8) & (03 << 8));
#ifdef	UNIBUS_MAP
		}
#endif
		hkaddr->hkcs2 = unit;
		hkaddr->hkcs1 = com;
		hkwait(hkaddr);
		if (hkaddr->hkcs1 & HK_CERR) {
			if (hkaddr->hkcs2 & HKCS2_NEM)
				return(0);	/* made it to end of memory */
			return(EIO);
		}
		paddr += (DBSIZE << PGSHIFT);
	}
	return(0);		/* filled disk minor dev */
}
#endif	HK_DUMP

#ifdef	UCB_ECC
#define	exadr(x,y)	(((long)(x) << 16) | (unsigned)(y))

/*
 * Correct an ECC error and restart the i/o to complete
 * the transfer if necessary.  This is quite complicated because
 * the transfer may be going to an odd memory address base
 * and/or across a page boundary.
 */
hkecc(bp, flag)
register struct	buf *bp;
{
	register struct	hkdevice *hkaddr = HKADDR;
	ubadr_t	addr;
	int npx, wc;
	int cn, tn, sn;
	daddr_t	bn;
	unsigned ndone;
	int cmd;
	int unit;

#ifdef BADSECT
	if (flag == CONT) {
		npx = bp->b_error;
		ndone = npx * PGSIZE;
		wc = ((int)(ndone - bp->b_bcount)) / NBPW;
	} else
#endif
		{
		wc = hkaddr->hkwc;
		ndone = (wc * NBPW) + bp->b_bcount;
		npx = ndone / PGSIZE;
		}
	unit = dkunit(bp);
	bn = dkblock(bp);
	cn = bp->b_cylin - bn / HK_NSPC;
	bn += npx;
	cn += bn / HK_NSPC;
	sn = bn % HK_NSPC;
	tn = sn / HK_NSECT;
	sn %= HK_NSECT;
	hktab.b_active++;

	switch (flag) {
	case ECC:
		{
		register byte;
		int bit;
		long mask;
		ubadr_t bb;
		unsigned o;
#ifdef	UNIBUS_MAP
		struct ubmap *ubp;
#endif
		printf("hk%d%c:  soft ecc sn %D\n",
			unit, 'a' + (minor(bp->b_dev) & 07),
			bp->b_blkno + npx - 1);

		mask = hkaddr->hkecpt;
		byte = hkaddr->hkecps - 1;
		bit = byte & 07;
		byte >>= 3;
		mask <<= bit;
		o = (ndone - PGSIZE) + byte;
		bb = exadr(bp->b_xmem, bp->b_un.b_addr);
		bb += o;
#ifdef	UNIBUS_MAP
		if (bp->b_flags & (B_MAP|B_UBAREMAP))	{
			ubp = UBMAP + ((bb >> 13) & 037);
			bb = exadr(ubp->ub_hi, ubp->ub_lo) + (bb & 017777);
		}
#endif
		/*
		 * Correct until mask is zero or until end of
		 * sector or transfer, whichever comes first.
		 */
		while (byte < PGSIZE && o < bp->b_bcount && mask != 0) {
			putmemc(bb, getmemc(bb) ^ (int)mask);
			byte++;
			o++;
			bb++;
			mask >>= 8;
		}
		if (wc == 0)
			return(0);
		break;
	}

#ifdef BADSECT
	case BSE:
#ifdef HKDEBUG
		if (hkdebug)
			printf("hkecc, BSE: bn %D cn %d tn %d sn %d\n",
				bn, cn, tn, sn);
#endif
		if ((bn = isbad(&hkbad[unit], cn, tn, sn)) < 0)
			return(0);
		bp->b_flags |= B_BAD;
		bp->b_error = npx + 1;
		bn = (long)hkncyl(unit)*HK_NSPC - HK_NSECT - 1 - bn;
		cn = bn/HK_NSPC;
		sn = bn%HK_NSPC;
		tn = sn/HK_NSECT;
		sn %= HK_NSECT;
#ifdef HKDEBUG
		if (hkdebug)
			printf("revector to cn %d tn %d sn %d\n", cn, tn, sn);
#endif
		wc = -(512 / NBPW);
		break;

	case CONT:
		bp->b_flags &= ~B_BAD;
		if (wc == 0)
			return(0);
#ifdef HKDEBUG
		if (hkdebug)
			printf("hkecc, CONT: bn %D cn %d tn %d sn %d\n",
				bn, cn, tn, sn);
#endif
		break;
#endif	BADSECT
	}
	/*
	 * Have to continue the transfer.  Clear the drive
	 * and compute the position where the transfer is to continue.
	 * We have completed npx sectors of the transfer already.
	 */
	hkaddr->hkcs1 = HK_CCLR;
	hkwait(hkaddr);
	hkaddr->hkcs2 = unit;
	hkaddr->hkcs1 = hk_type[unit] | HK_DCLR | HK_GO;
	hkwait(hkaddr);

	addr = exadr(bp->b_xmem, bp->b_un.b_addr);
	addr += ndone;
	hkaddr->hkcyl = cn;
	hkaddr->hkda = (tn << 8) + sn;
	hkaddr->hkwc = wc;
	hkaddr->hkba = (int)addr;
	cmd = hk_type[unit] | ((hiint(addr) & 3) << 8) | HK_IE | HK_GO;
	if (bp->b_flags & B_READ)
		cmd |= HK_READ;
	else
		cmd |= HK_WRITE;
	hkaddr->hkcs1 = cmd;
	hktab.b_errcnt = 0;	/* error has been corrected */
	return (1);
}
#endif	UCB_ECC
#endif	NHK > 0
