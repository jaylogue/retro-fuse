/*
 *	SCCS id	@(#)rl.c	2.1 (Berkeley)	11/20/83
 */

/*
 *  RL01/RL02 disk driver
 */

#include "rl.h"
#if	NRL > 0
#include "param.h"
#include <sys/buf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/rlreg.h>

#define	RL01_NBLKS	10240	/* Number of UNIX blocks for an RL01 drive */
#define	RL02_NBLKS	20480	/* Number of UNIX blocks for an RL02 drive */
#define	RL_CYLSZ	10240	/* bytes per cylinder */
#define	RL_SECSZ	256	/* bytes per sector */

#define	rlwait(r)	while (((r)->rlcs & RL_CRDY) == 0)

struct	rldevice *RLADDR;

struct	buf	rrlbuf;
struct	buf	rltab;

struct 	rl_softc {
	short	cn[4];		/* location of heads for each drive */
	short	type[4];	/* parameter dependent on drive type (RL01/2) */
	short	dn;		/* drive number */
	short	com;		/* read or write command word */
	short	chn;		/* cylinder and head number */
	u_short	bleft;		/* bytes left to be transferred */
	u_short	bpart;		/* number of bytes transferred */
	short	sn;		/* sector number */
	union	{
		short	w[2];
		long	l;
	} rl_un;		/* address of memory for transfer */

} rl = {-1,-1,-1,-1, -1,-1,-1,-1}; /* initialize cn[] and type[] */

rlattach(addr, unit)
struct rldevice *addr;
{
	if (unit != 0)
		return(0);
	RLADDR = addr;
	return(1);
}

rlstrategy(bp)
register struct	buf *bp;
{
	register struct rldevice *rp;
	int	s, nblocks, drive, ctr;

	if ((rp = RLADDR) == (struct rldevice *) NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}

	/*
	 * We must determine what type of drive we are talking to in order 
	 * to determine how many blocks are on the device.  The rl.type[]
	 * array has been initialized with -1's so that we may test first
	 * contact with a particular drive and do this determination only once.
	 *
	 * For some unknown reason the RL02 (seems to be
	 * only drive 1) does not return a valid drive status
	 * the first time that a GET STATUS request is issued
	 * for the drive, in fact it can take up to three or more
	 * GET STATUS requests to obtain the correct status.
	 * In order to overcome this "HACK" the driver has been
	 * modified to issue a GET STATUS request, validate the
	 * drive status returned, and then use it to determine the
	 * drive type. If a valid status is not returned after eight
	 * attempts, then an error message is printed.
	 */
	if (rl.type[minor(bp->b_dev)] < 0) {
		drive = minor(bp->b_dev);
		ctr = 0;
		do { /* get status and reset when first touching this drive */
			rp->rlda = RLDA_RESET | RLDA_GS;
			rp->rlcs = (drive << 8) | RL_GETSTATUS;	/* set up csr */
			rlwait(rp);
		} while (((rp->rlmp & 0177477) != 035) && (++ctr < 8));
		if (ctr >= 8) {
			printf("rl%d:  can't get status\n", drive);
			rl.type[drive] = RL02_NBLKS;	/* assume RL02 */
		} else if (rp->rlmp & RLMP_DTYP) {
			rl.type[drive] = RL02_NBLKS;	/* drive is RL02 */
#ifdef	DISTRIBUTION_BINARY
			/*
			 * HACK-- for the 2.9 distribution binary,
			 * patch the swplo value if swapping on an RL02.
			 */
			if ((bdevsw[major(swapdev)].d_strategy == rlstrategy)
			    && (drive == minor(swapdev)) && (swplo == (8000-1)))
				swplo = (daddr_t) (17000 - 1);
#endif
		} else
			rl.type[drive] = RL01_NBLKS;	/* drive RL01 */
	}
	/* determine nblocks based upon which drive this is */
	nblocks = rl.type[minor(bp->b_dev)];
	if(bp->b_blkno >= nblocks) {
		if((bp->b_blkno == nblocks) && (bp->b_flags & B_READ))
			bp->b_resid = bp->b_bcount;
		else
			{
			bp->b_error = ENXIO;
bad:
			bp->b_flags |= B_ERROR;
		}
		iodone(bp);
		return;
	}
#ifdef UNIBUS_MAP
	mapalloc(bp);
#endif

	bp->av_forw = NULL;
	s = spl5();
	if(rltab.b_actf == NULL)
		rltab.b_actf = bp;
	else
		rltab.b_actl->av_forw = bp;
	rltab.b_actl = bp;
	if(rltab.b_active == NULL)
		rlstart();
	splx(s);
}

rlstart()
{
	register struct rl_softc *rlp = &rl;
	register struct buf *bp;

	if ((bp = rltab.b_actf) == NULL)
		return;
	rltab.b_active++;
	rlp->dn = minor(bp->b_dev);
	rlp->chn = bp->b_blkno / 20;
	rlp->sn = (bp->b_blkno % 20) << 1;
	rlp->bleft = bp->b_bcount;
	rlp->rl_un.w[0] = bp->b_xmem & 3;
	rlp->rl_un.w[1] = (int) bp->b_un.b_addr;
	rlp->com = (rlp->dn << 8) | RL_IE;
	if (bp->b_flags & B_READ)
		rlp->com |= RL_RCOM;
	else
		rlp->com |= RL_WCOM;
	rlio();
}

rlintr()
{
	register struct buf *bp;
	register struct rldevice *rladdr = RLADDR;
	register status;

	if (rltab.b_active == NULL)
		return;
	bp = rltab.b_actf;
#ifdef	RL_DKN
	dk_busy &= ~(1 << RL_DKN);
#endif	RL_DKN
	if (rladdr->rlcs & RL_CERR) {
		if (rladdr->rlcs & RL_HARDERR) {
			if(rltab.b_errcnt > 2) {
#ifdef	UCB_DEVERR
				harderr(bp, "rl");
				printf("cs=%b da=%b\n", rladdr->rlcs, RL_BITS,
					rladdr->rlda, RLDA_BITS);
#else
				deverror(bp, rladdr->rlcs, rladdr->rlda);
#endif	UCB_DEVERR
			}
		}
		if (rladdr->rlcs & RL_DRE) {
			rladdr->rlda = RLDA_GS;
			rladdr->rlcs = (rl.dn <<  8) | RL_GETSTATUS;
			rlwait(rladdr);
			status = rladdr->rlmp;
			if(rltab.b_errcnt > 2) {
#ifdef	UCB_DEVERR
				harderr(bp, "rl");
				printf("mp=%b da=%b\n", status, RLMP_BITS,
					rladdr->rlda, RLDA_BITS);
#else
				deverror(bp, status, rladdr->rlda);
#endif
			}
			rladdr->rlda = RLDA_RESET | RLDA_GS;
			rladdr->rlcs = (rl.dn << 8) | RL_GETSTATUS;
			rlwait(rladdr);
			if(status & RLMP_VCHK) {
				rlstart();
				return;
			}
		}
		if (++rltab.b_errcnt <= 10) {
			rl.cn[rl.dn] = -1;
			rlstart();
			return;
		}
		else
			{
			bp->b_flags |= B_ERROR;
			rl.bpart = rl.bleft;
		}
	}

	if ((rl.bleft -= rl.bpart) > 0) {
		rl.rl_un.l += rl.bpart;
		rl.sn=0;
		rl.chn++;
		rlio();
		return;
	}
	bp->b_resid = 0;
	rltab.b_active = NULL;
	rltab.b_errcnt = 0;
	rltab.b_actf = bp->av_forw;
	iodone(bp);
	rlstart();
}

rlio()
{

	register struct rldevice *rladdr = RLADDR;
	register dif;
	register head;

#ifdef	RL_DKN
	dk_busy |= 1 << RL_DKN;
	dk_numb[RL_DKN]++;
	head = rl.bpart >> 6;
	dk_wds[RL_DKN] += head;
#endif	RL_DKN

	/*
	 * When the device is first touched, find where the heads are and
	 * what type of drive it is.
	 * rl.cn[] is initialized as -1 and this indicates that we have not 
	 * touched this drive before.
	 */
	if (rl.cn[rl.dn] < 0) {
		/* find where the heads are */
		rladdr->rlcs = (rl.dn << 8) | RL_RHDR;
		rlwait(rladdr);
		rl.cn[rl.dn] = ((rladdr->rlmp) >> 6) & 01777;
	}
	dif =(rl.cn[rl.dn] >> 1) - (rl.chn >> 1);
	head = (rl.chn & 1) << 4;
	if (dif < 0)
		rladdr->rlda = (-dif << 7) | RLDA_SEEKHI | head;
	else
		rladdr->rlda = (dif << 7) | RLDA_SEEKLO | head;
	rladdr->rlcs = (rl.dn << 8) | RL_SEEK;
	rl.cn[rl.dn] = rl.chn;
	if (rl.bleft < (rl.bpart = RL_CYLSZ - (rl.sn * RL_SECSZ)))
		rl.bpart = rl.bleft;
	rlwait(rladdr);
	rladdr->rlda = (rl.chn << 6) | rl.sn;
	rladdr->rlba = rl.rl_un.w[1];
	rladdr->rlmp = -(rl.bpart >> 1);
	rladdr->rlcs = rl.com | rl.rl_un.w[0] << 4;
}

rlread(dev)
dev_t	dev;
{
	physio(rlstrategy, &rrlbuf, dev, B_READ);
}

rlwrite(dev)
dev_t	dev;
{
	physio(rlstrategy, &rrlbuf, dev, B_WRITE);
}
#endif	NRL
