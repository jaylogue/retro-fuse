/*
 *	SCCS id	@(#)ts.c	2.1 (Berkeley)	8/5/83
 */

/*
 *	TS11 tape driver
 */

#include "ts.h"
#if	NTS > 0
#include "param.h"
#include <sys/buf.h>
#include <sys/dir.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/user.h>
#include <sys/tsreg.h>
#ifdef	TS_IOCTL
#include <sys/mtio.h>
#endif

/*
 * Software state per tape transport:
 *
 * 1. A tape drive is a unique-open device:  we refuse opens when it is already.
 * 2. We keep track of the current position on a block tape and seek
 *    before operations by forward/back spacing if necessary.
 * 3. We remember if the last operation was a write on a tape, so if a tape
 *    is open read write and the last thing done is a write we can
 *    write a standard end of tape mark (two eofs).
 * 4. We remember the status registers after the last command, using
 *    them internally and returning them to the SENSE ioctl.
 */
struct	ts_softc {
	char	sc_openf;
	char	sc_lastiow;
	short	sc_resid;
	daddr_t	sc_blkno;
	daddr_t	sc_nxrec;
	struct	ts_cmd	sc_cmd;
	struct	ts_sts	sc_sts;
	struct	ts_char	sc_char;
} ts_softc[NTS];

struct	buf	tstab;
struct	buf	ctsbuf;
/*
 * Raw tape operations use rtsbuf.  The driver
 * notices when rtsbuf is being used and allows the user
 * program to continue after errors and read records
 * not of the standard length (BSIZE).
 */
struct	buf	rtsbuf;

struct	tsdevice *TSADDR;

#define	INF		((daddr_t) ((u_short) 65535))
#define	T_NOREWIND	0200
#define	TSUNIT(dev)	(minor(dev) & 03)
#define	tswait(r)	while (((TSADDR->(r)) & TS_SSR) == 0)

	/* command code definitions */

/*
 * States for tstab.b_active, the state flag.
 * This is used to sequence control in the driver.
 */
#define SSEEK		1	/* seeking */
#define	SIO		2	/* doing seq. i/o */
#define	SCOM		3	/* sending a control command */
#define	SREW		4	/* sending a drive rewind */

tsattach(addr, unit)
struct tsdevice *addr;
{
	/*
	 * This driver supports only one controller.
	 */
	if (unit == 0) {
		TSADDR = addr;
		return(1);
	}
	return(0);
}

/*
 * Open the device.  Tapes are unique open
 * devices so we refuse if it is already open.
 * We also check that a tape is available and
 * don't block waiting here:  if you want to wait
 * for a tape you should timeout in user code.
 */
tsopen(dev, flag)
dev_t	dev;
{
	register tsunit;
	register struct ts_softc *sc;

	tsunit = TSUNIT(dev);
	if (TSADDR == (struct tsdevice *) NULL || tsunit >= NTS
	    || (sc = &ts_softc[tsunit])->sc_openf) {
		u.u_error = ENXIO;
		return;
	}
	if(tsinit(tsunit)) {
		u.u_error = ENXIO;
		return;
	}
	tstab.b_flags |= B_TAPE;
	tscommand(dev, TS_SENSE, 1);
	if ((sc->sc_sts.s_xs0 & TS_ONL) == 0) {
		uprintf("ts%d: not online\n", tsunit);
		u.u_error = EIO;
		return;
	}
	if ((flag & (FREAD | FWRITE)) == FWRITE
	    && (sc->sc_sts.s_xs0 & TS_WLK)) {
		uprintf("ts%d: no write ring\n", tsunit);
		u.u_error = EIO;
		return;
	}
	sc->sc_openf = 1;
	sc->sc_blkno = (daddr_t) 0;
	sc->sc_nxrec = INF;
	sc->sc_lastiow = 0;
}

/*
 * Close tape device.
 *
 * If tape was open for writing or last operation was
 * a write, then write two EOF's and backspace over the last one.
 * Unless his is a non-rewinding special file, rewind the tape.
 * Make the tape available to others.
 */
tsclose(dev, flag)
register dev_t	dev;
{
	register struct ts_softc *sc = &ts_softc[TSUNIT(dev)];

	if(flag == FWRITE || ((flag & FWRITE) && sc->sc_lastiow)) {
		tscommand(dev, TS_WEOF, 1);
		tscommand(dev, TS_WEOF, 1);
		tscommand(dev, TS_SREV, 1);
	}
	if ((minor(dev) & T_NOREWIND) == 0 )
		/*
		 * 0 count means don't hang waiting for rewind complete.
		 * Rather ctsbuf stays busy until the operation completes
		 * preventing further opens from completing by
		 * preventing a TS_SENSE from completing.
		 */
		tscommand(dev, TS_REV, 0);
	sc->sc_openf = 0;
}

/*
 * Execute a command on the tape drive
 * a specified number of times.
 */
tscommand(dev, com, count)
dev_t	dev;
register u_short count;
{
	register s;
	register struct buf *bp;

	bp = &ctsbuf;
	s = spl5();
	while(bp->b_flags & B_BUSY) {
		/*
		 * This special check is because B_BUSY never
		 * gets cleared in the non-waiting rewind case.
		 */
		if (bp->b_repcnt == 0 && (bp->b_flags & B_DONE))
			break;
		bp->b_flags |= B_WANTED;
		sleep((caddr_t) bp, PRIBIO);
	}
	bp->b_flags = B_BUSY | B_READ;
	splx(s);
	bp->b_dev = dev;
	bp->b_repcnt = -count;
	bp->b_command = com;
	bp->b_blkno = (daddr_t) 0;
	tsstrategy(bp);
	/*
	 * In case of rewind from close, don't wait.
	 * This is the only case where count can be 0.
	 */
	if (count == 0)
		return;
	iowait(bp);
	if(bp->b_flags & B_WANTED)
		wakeup((caddr_t) bp);
	bp->b_flags &= B_ERROR;
}

/*
 * Queue a tape operation.
 */
tsstrategy(bp)
register struct buf *bp;
{
	register s;

#ifdef UNIBUS_MAP
	if (bp != &ctsbuf)
		mapalloc(bp);
#endif
	bp->av_forw = NULL;
	s = spl5();
	if (tstab.b_actf == NULL)
		tstab.b_actf = bp;
	else
		tstab.b_actl->av_forw = bp;
	tstab.b_actl = bp;
	/*
	 * If the controller is not busy, get
	 * it going.
	 */
	if (tstab.b_active == 0)
		tsstart();
	splx(s);
}

/*
 * Start activity on a ts controller.
 */
 tsstart()
 {
	daddr_t	blkno;
	int	cmd, s, tsunit;
	register struct ts_softc *sc;
	register struct ts_cmd *tc;
	register struct buf *bp;

	/*
	 * Start the controller if there is something for it to do.
	 */
loop:
	if ((bp = tstab.b_actf) == NULL)
		return;
	tsunit = TSUNIT(bp->b_dev);
	sc = &ts_softc[tsunit];
	tc = &sc->sc_cmd;
	/*
	 * Default is that last command was NOT a write command;
	 * if we do a write command we will notice this in tsintr().
	 */
	sc->sc_lastiow = 0;
	if (sc->sc_openf < 0 || (TSADDR->tssr & TS_OFL)) {
		/*
		 * Have had a hard error on a non-raw tape
		 * or the tape unit is now unavailable
		 * (e.g. taken off line).
		 */
		bp->b_flags |= B_ERROR;
		goto next;
	}
	if (bp == &ctsbuf) {
		/*
		 * Execute control operation with the specified count.
		 */
		tstab.b_active = bp->b_command == TS_REW ?  SREW : SCOM;
		tc->c_repcnt = bp->b_repcnt;
		goto dobpcmd;
	}
	/*
	 * The following checks handle boundary cases for operation
	 * on non-raw tapes.  On raw tapes the initialization of
	 * sc->sc_nxrec by tsphys causes them to be skipped normally
	 * (except in the case of retries).
	 */
	if(dbtofsb(bp->b_blkno) > sc->sc_nxrec) {
		/*
		 * Can't read past known end-of-file.
		 */
		bp->b_flags |= B_ERROR;
		bp->b_error = ENXIO;
		goto next;
	}
	if(dbtofsb(bp->b_blkno) == sc->sc_nxrec && bp->b_flags & B_READ) {
		/*
		 * Reading at end of file returns 0 bytes.
		 * Buffer will be cleared (if written) in writei.
		 */
		bp->b_resid = bp->b_bcount;
		goto next;
	}
	if((bp->b_flags & B_READ) == 0)
		sc->sc_nxrec = dbtofsb(bp->b_blkno) + 1;
	/*
	 * If the data transfer command is in the correct place,
	 * set up all registers and do the transfer.
	 */
	if ((blkno = sc->sc_blkno) == dbtofsb(bp->b_blkno)) {
		tc->c_size = bp->b_bcount;
		if ((bp->b_flags & B_READ) == 0)
			cmd = TS_WCOM;
		else
			cmd = TS_RCOM;
		if (tstab.b_errcnt)
			cmd |= TS_RETRY;
		tstab.b_active = SIO;
		tc->c_cmd = TS_ACK | TS_CVC | TS_IE | cmd;
		TSADDR->tsdb = &sc->sc_cmd.c_cmd;
		return;
	}
	/*
	 * Tape positioned incorrectly;
	 * set to seek forward or backward to the correct spot.
	 * This happens for raw tapes only on error retries.
	 */
	tstab.b_active = SSEEK;
	if(blkno < dbtofsb(bp->b_blkno)) {
		bp->b_command = TS_SFORW;
		bp->b_repcnt = dbtofsb(bp->b_blkno) - blkno;
	} else
		{
		bp->b_command = TS_SREV;
		bp->b_repcnt = blkno - dbtofsb(bp->b_blkno);
	}

dobpcmd:
	/*
	 * Do the command in bp.
	 */
	tc->c_cmd = TS_ACK | TS_CVC | TS_IE | bp->b_command;
	TSADDR->tsdb = &sc->sc_cmd.c_cmd;
	return;

next:
	tstab.b_errcnt = 0;
	tstab.b_actf = bp->av_forw;
	iodone(bp);
	goto loop;
}

/*
 * TS interrupt routine
 */
tsintr()
{
	register state;
	register struct buf *bp;
	register struct ts_softc *sc;
	int	tsunit, err;

	if((bp = tstab.b_actf) == NULL)
		return;
	tsunit = TSUNIT (bp->b_dev);

	/*
	 * If last command was a rewind, and tape is still
	 * rewinding, wait for the rewind complete interrupt.
	 *
	 * SHOULD NEVER GET AN INTERRUPT IN THIS STATE.
	 */
	if (tstab.b_active == SREW) {
		tstab.b_active = SCOM;
		if ((TSADDR->tssr & TS_SSR) == 0)
			return;
	}

	/*
	 * An operation completed... record status
	 */
	sc = &ts_softc[tsunit];
	if ((bp->b_flags & B_READ) == 0)
		sc->sc_lastiow = 1;
	state = tstab.b_active;
	tstab.b_active = 0;

	/*
	 * Check for errors.
	 */
	if(TSADDR->tssr & TS_SC) {
		switch (TSADDR->tssr & TS_TC) {
			case TS_UNREC:	/* unrecoverable */
			case TS_FATAL:	/* fatal error */
			case TS_ATTN:	/* attention (shouldn't happen) */
			case TS_RECNM:	/* recoverable, no motion */
				break;

			case TS_SUCC:	/* successful termination */
				goto ignoreerr;
				/*NOTREACHED*/
			
			case TS_ALERT:	/* tape status alert */
				/*
				 * If we hit the end of the tape file,
				 * update our position.
				 */
				if (sc->sc_sts.s_xs0 & (TS_TMK | TS_EOT)) {
					tsseteof(bp);	/* set blkno and nxrec */
					state = SCOM;	/* force completion */
					/*
					 * Stuff bc so it will be unstuffed
					 * correctly later to get resid.
					 */
					sc->sc_sts.s_rbpcr = bp->b_bcount;
					goto opdone;
					/*NOTREACHED*/
				}
				/*
				 * If we were reading raw tape and the record
				 * was too long or too short, then we don't
				 * consider this an error.
				 */
				if (bp == &rtsbuf && (bp->b_flags & B_READ)
				    && sc->sc_sts.s_xs0 & (TS_RLS | TS_RLL))
					goto ignoreerr;
					/*NOTREACHED*/

			case TS_RECOV:	/* recoverable, tape moved */
				/*
				 * If this was an i/o operation,
				 * retry up to 8 times.
				 */
				if (state == SIO) {
					if (++tstab.b_errcnt < 7)
						goto opcont;
					else
						sc->sc_blkno++;
				} else
					{
					/*
					 * Non-i/o errors on non-raw tape
					 * cause it to close.
					 */
					if (sc->sc_openf > 0 && bp != &rtsbuf)
						sc->sc_openf = -1;
				}
				break;
			case TS_REJECT:
				if (state == SIO && sc->sc_sts.s_xs0 & TS_WLE)
					printf("ts%d: no write ring\n", tsunit);
				if ((sc->sc_sts.s_xs0 & TS_ONL) == 0)
					printf("ts%d: not online\n", tsunit);
				break;
		}
		/*
		 * Couldn't recover error.
		 */
#ifdef	UCB_DEVERR
		printf("ts%d: hard error bn%d xs0=%b", TSUNIT(bp->b_dev),
		     bp->b_blkno, sc->sc_sts.s_xs0, TSXS0_BITS);
		if (sc->sc_sts.s_xs1)
			printf(" xs1=%b", sc->sc_sts.s_xs1, TSXS1_BITS);
		if (sc->sc_sts.s_xs2)
			printf(" xs2=%b", sc->sc_sts.s_xs2, TSXS2_BITS);
		if (sc->sc_sts.s_xs3)
			printf(" xs3=%b\n", sc->sc_sts.s_xs3, TSXS3_BITS);
#else
		deverror(bp, sc->sc_sts.s_xs0, sc->sc_sts.s_xs1);
		printf("%o,%o\n", sc->sc_sts.s_xs2, sc->sc_sts.s_xs3);
#endif
		bp->b_flags |= B_ERROR;
		goto opdone;
		/*NOTREACHED*/
	}
	/*
	 * Advance tape control finite state machine.
	 */
ignoreerr:
	switch (state) {
		case SIO:
			/*
			 * Read/write increments tape block number.
			 */
			sc->sc_blkno++;
			goto opdone;
			/*NOTREACHED*/

		case SCOM:
			/*
			 * For forward/backward space record
			 * update current position.
			 */
			if (bp == &ctsbuf)
				switch (bp->b_command) {
					case TS_SFORW:
						sc->sc_blkno += bp->b_repcnt;
						break;

					case TS_SREV:
						sc->sc_blkno -= bp->b_repcnt;
						break;
				}
			goto opdone;
			/*NOTREACHED*/

		case SSEEK:
			sc->sc_blkno = dbtofsb(bp->b_blkno);
			goto opcont;
			/*NOTREACHED*/
		
		default:
			panic("tsintr");
			/*NOTREACHED*/
	}

opdone:
	/*
	 * Reset error count and remove
	 * from device queue.
	 */
	tstab.b_errcnt = 0;
	tstab.b_actf = bp->av_forw;
	bp->b_resid = sc->sc_sts.s_rbpcr;
	iodone(bp);

opcont:
	tsstart();
}

tsseteof(bp)
register struct buf *bp;
{
	register tsunit = TSUNIT(bp->b_dev);
	register struct ts_softc *sc = &ts_softc[tsunit];

	if (bp == &ctsbuf) {
		if (sc->sc_blkno > dbtofsb(bp->b_blkno)) {
			/* reversing */
			sc->sc_nxrec = dbtofsb(bp->b_blkno) - sc->sc_sts.s_rbpcr;
			sc->sc_blkno = sc->sc_nxrec;
		}
		else
			{
			/* spacing forward */
			sc->sc_blkno = dbtofsb(bp->b_blkno) - sc->sc_sts.s_rbpcr;
			sc->sc_nxrec = sc->sc_blkno - 1;
		}
		return;
	}
	else
		/* eof on read */
		sc->sc_nxrec = dbtofsb(bp->b_blkno);
}

/*
 * Initialize the TS11.
 */
tsinit(tsunit)
{
	register struct tsdevice *tsaddr = TSADDR;
	struct	ts_softc *sc = &ts_softc[tsunit];
	register struct ts_cmd *tcmd = &sc->sc_cmd;
	register struct ts_char *tchar = &sc->sc_char;

	if (tsaddr->tssr & (TS_NBA | TS_OFL)) {
		tsaddr->tssr = 0;		/* subsystem initialize */
		tswait(tssr);
		if (((u_short) tcmd) & 03) {
			printf("ts%d: addr mod 4 != 0\n", tsunit);
			return (1);
		}
		tchar->char_bptr = &sc->sc_sts;
		tchar->char_bae = 0;
		tchar->char_size = sizeof(struct ts_sts);
		tchar->char_mode = TS_ESS;
		tcmd->c_cmd = TS_ACK | TS_SETCHR;
		tcmd->c_loba = tchar;
		tcmd->c_hiba = 0;
		tcmd->c_size = sizeof(struct ts_char);
		tsaddr->tsdb = tcmd;
		tswait(tssr);
		if (tsaddr->tssr & TS_NBA)
			return (1);
	}
	else
		return(0);
}

tsread(dev)
register dev_t	dev;
{
	tsphys(dev);
	bphysio(tsstrategy, &rtsbuf, dev, B_READ);
}

tswrite(dev)
register dev_t	dev;
{
	tsphys(dev);
	bphysio(tsstrategy, &rtsbuf, dev, B_WRITE);
}

tsphys(dev)
dev_t	dev;
{
	register struct ts_softc *sc;
	daddr_t	a;

	sc = &ts_softc[TSUNIT(dev)];
	a = dbtofsb(u.u_offset >> 9);
	sc->sc_blkno = a;
	sc->sc_nxrec = a + 1;
}

#ifdef	TS_IOCTL
/*ARGSUSED*/
tsioctl(dev, cmd, addr, flag)
dev_t	dev;
caddr_t	addr;
{
	register struct ts_softc *sc = &ts_softc[TSUNIT(dev)];
	register struct buf *bp = &ctsbuf;
	register callcount;
	u_short	fcount;
	struct	mtop mtop;
	struct	mtget mtget;
	/* we depend on the values and order of the MT codes here */
	static tsops[] = {TS_WEOF,TS_SFORW,TS_SREV,TS_SFORW,TS_SREV,TS_REW,TS_OFFL,TS_SENSE};

	switch (cmd) {

	case MTIOCTOP:	/* tape operation */
		if (copyin((caddr_t)addr, (caddr_t)&mtop, sizeof(mtop))) {
			u.u_error = EFAULT;
			return;
		}
		switch(mtop.mt_op) {
		case MTWEOF:
			callcount = mtop.mt_count;
			fcount = 1;
			break;
		case MTFSF:
		case MTBSF:
		case MTFSR:
		case MTBSR:
			callcount = 1;
			fcount = mtop.mt_count;
			break;
		case MTREW:
		case MTOFFL:
		case MTNOP:
			callcount = 1;
			fcount = 1;
			break;
		default:
			u.u_error = ENXIO;
			return;
		}
		if (callcount <= 0 || fcount <= 0) {
			u.u_error = ENXIO;
			return;
		}
		while (--callcount >= 0) {
			tscommand(dev, tsops[mtop.mt_op], fcount);
			if ((mtop.mt_op == MTFSR || mtop.mt_op == MTBSR) &&
			    bp->b_resid) {
				u.u_error = EIO;
				break;
			}
			if ((bp->b_flags & B_ERROR) || sc->sc_sts.s_xs0 & TS_BOT)
				break;
		}
		geterror(bp);
		return;
	case MTIOCGET:
		mtget.mt_dsreg = 0;
		mtget.mt_erreg = sc->sc_sts.s_xs0;
		mtget.mt_resid = sc->sc_resid;
		mtget.mt_type = MT_ISTS;
		if (copyout((caddr_t)&mtget, addr, sizeof(mtget)))
			u.u_error = EFAULT;
		return;
	default:
		u.u_error = ENXIO;
	}
}
#endif	TS_IOCTL
#endif	NTS
