/*
 *	SCCS id	@(#)dh.c	2.1 (Berkeley)	11/20/83
 */

#include "dh.h"
#if	NDH > 0
#include "param.h"
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/file.h>
#include <sys/tty.h>
#include <sys/dhreg.h>
#include <sys/uba.h>

#define	q3	tp->t_outq

#if	defined (UNIBUS_MAP) || defined (UCB_CLIST)
extern	ubadr_t	clstaddr;
#ifdef	UCB_CLIST
extern	struct	cblock	*cfree;
#else	UCB_CLIST
extern	struct	cblock	cfree[];
#endif	UCB_CLIST
#define	cpaddr(x)	(clstaddr + (ubadr_t)((x) - cfree))

#else	defined (UNIBUS_MAP) || defined (UCB_CLIST)
#define	cpaddr(x)	(x)
#endif	defined (UNIBUS_MAP) || defined (UCB_CLIST)

#ifdef	DH_SOFTCAR
#define	DHLINE(dev)	(minor(dev) & 0177)
#else
#define	DHLINE(dev)	minor(dev)
#endif

#define	NDHLINE	(NDH * 16)
struct	tty dh11[NDHLINE];
int	ndh11	= NDHLINE; /* only for pstat */
int	dhlowdm	= LOWDM;
int	dhndm	= NDM * 16;
int	dhstart();
int	ttrstrt();
#ifdef	DH_SILO
#define	SILOSCANRATE	(hz / 10)
int	dhchars[NDH];
void	dhtimer();
#endif	DH_SILO

struct	dhdevice *dh_addr[NDH];
#if	NDM > 0
struct	dmdevice *dm_addr[NDM];
#endif	NDM

/*
 * Software copy of last dhbar
 */
int	dhsar[NDH];

dhattach(addr, unit)
struct dhdevice *addr;
{
	if ((unsigned) unit >= NDH)
		return 0;
	dh_addr[unit] = addr;
	return 1;
}

/*
 * Open a DH line.  Turn on this dh if this is
 * the first use of it.  Also do a dmopen to wait for carrier.
 */
/*ARGSUSED*/
dhopen(dev, flag)
dev_t	dev;
{
	register struct tty *tp;
	register unit;
	register struct dhdevice *addr;
#ifdef	DH_SILO
	static	dh_timer;
#endif	DH_SILO

	unit = DHLINE(dev);
	if ((unit >= NDHLINE) || ((addr = dh_addr[unit >> 4]) == 0)) {
		u.u_error = ENXIO;
		return;
	}
	tp = &dh11[unit];
	if (tp->t_state & XCLUDE && u.u_uid != 0) {
		u.u_error = EBUSY;
		return;
	}
	tp->t_addr = (caddr_t) addr;
	tp->t_oproc = dhstart;
	tp->t_iproc = NULL;
	tp->t_state |= WOPEN;

#ifdef	DH_SILO
	if (!dh_timer) {
		dh_timer++;
		timeout(dhtimer, (caddr_t) 0, SILOSCANRATE);
	}
#endif	DH_SILO

	addr->un.dhcsr |= DH_IE;
	/*
	 * If this is first open, initialize tty state to default.
	 */
	if ((tp->t_state & ISOPEN) == 0) {
		ttychars(tp);
		if (tp->t_ispeed == 0) {
			tp->t_ispeed = B300;
			tp->t_ospeed = B300;
			tp->t_flags = ODDP | EVENP | ECHO;
		}
		tp->t_line = DFLT_LDISC;
		dhparam(unit);
	}
#if	NDM > 0
	dmopen(dev);
#else
	tp->t_state |= CARR_ON;
#endif
	ttyopen(dev,tp);
}

/*
 * Close a DH line, turning off the DM11.
 */
/*ARGSUSED*/
dhclose(dev, flag)
dev_t	dev;
int	flag;
{
	register struct tty *tp;
	register unit;

	unit = DHLINE(dev);
	tp = &dh11[unit];
	((struct dhdevice *) (tp->t_addr))->dhbreak &= ~(1 << (unit & 017));
#if	NDM > 0
	if (tp->t_state & HUPCLS)
		dmctl(unit, DML_OFF, DMSET);
#endif
	ttyclose(tp);
}

/*
 * Read from a DH line.
 */
dhread(dev)
dev_t	dev;
{
	register struct tty *tp;

	tp = &dh11[DHLINE(dev)];
	(void) (*linesw[tp->t_line].l_read)(tp);
}

/*
 * Write on a DH line.
 */
dhwrite(dev)
{
	register struct tty *tp;

	tp = &dh11[DHLINE(dev)];
	(void) (*linesw[tp->t_line].l_write)(tp);
}

/*
 * DH11 receiver interrupt.
 */
dhrint(dh)
int	dh;
{
	register struct tty *tp;
	register int c;
	register struct dhdevice *addr;
	struct	tty *tp0;
	int	overrun = 0;

	addr = dh_addr[dh];
	tp0 = &dh11[dh << 4];
	/*
	 * Loop fetching characters from the silo for this
	 * dh until there are no more in the silo.
	 */
	while ((c = addr->dhrcr) < 0) {
		tp = tp0 + ((c >> 8) & 017);
		if (tp >= &dh11[NDHLINE])
			continue;
		if((tp->t_state & ISOPEN) == 0) {
			wakeup((caddr_t)tp);
			continue;
		}
#ifdef	DH_SILO
		dhchars[dh]++;
#endif	DH_SILO
#ifdef  TEXAS_AUTOBAUD
		if (image_mode(tp))
			c &= ~(DH_PE|DH_FE);
#endif
		if (c & DH_PE)
			if ((tp->t_flags & (EVENP | ODDP)) == EVENP
			 || (tp->t_flags & (EVENP | ODDP)) == ODDP)
				continue;
		if ((c & DH_DO) && overrun == 0) {
			printf("dh%d: silo overflow\n", dh);
			overrun = 1;
			}
		if (c & DH_FE)
			/*
			 * At framing error (break) generate
			 * a null (in raw mode, for getty), or an
			 * interrupt (in cooked/cbreak mode).
			 */
			if (tp->t_flags & RAW)
				c = 0;
			else
				c = tun.t_intrc;
		(*linesw[tp->t_line].l_input)(c,tp);
	}
}

/*
 * Ioctl for DH11.
 */
dhioctl(dev, cmd, addr, flag)
dev_t	dev;
caddr_t	addr;
{
	register struct tty *tp;
	register unit = DHLINE(dev);

	tp = &dh11[unit];
	switch (ttioctl(tp, cmd, addr, flag)) {
	    case TIOCSETP:
	    case TIOCSETN:
		dhparam(unit);
		break;
#ifdef	DH_IOCTL
	    case TIOCSBRK:
		((struct dhdevice *)(tp->t_addr))->dhbreak |= 1 << (unit & 017);
		break;
	    case TIOCCBRK:
		((struct dhdevice *)(tp->t_addr))->dhbreak &= ~(1<<(unit&017));
		break;
#if	NDM > 0
	    case TIOCSDTR:
		dmctl (unit, DML_DTR | DML_RTS, DMBIS);
		break;
	    case TIOCCDTR:
		dmctl (unit, DML_DTR | DML_RTS, DMBIC);
		break;
#endif
#endif	DH_IOCTL
	    case 0:
		break;
	    default:
		u.u_error = ENOTTY;
	}
}

/*
 * Set parameters from open or stty into the DH hardware
 * registers.
 */
dhparam(unit)
int	unit;
{
	register struct tty *tp;
	register struct dhdevice *addr;
	int s;
	register lpar;

	tp = &dh11[unit];
	addr = (struct dhdevice *) tp->t_addr;
	/*
	 * Block interrupts so parameters will be set
	 * before the line interrupts.
	 */
	s = spl5();
	addr->un.dhcsrl = (unit & 017) | DH_IE;
	if ((tp->t_ispeed) == 0) {
		tp->t_state |= HUPCLS;
#if	NDM > 0
		dmctl(unit, DML_OFF, DMSET);
#endif
		return;
	}
	lpar = ((tp->t_ospeed) << 10) | ((tp->t_ispeed) << 6);
	if ((tp->t_ispeed) == B134)
		lpar |= BITS6 | PENABLE | HDUPLX;
	else
#ifdef	UCB_NTTY
		if ((tp->t_flags & RAW) || (tp->t_local & LLITOUT))
#else
		if (tp->t_flags & RAW)
#endif
			lpar |= BITS8;
		else
			lpar |= BITS7 | PENABLE;
	if ((tp->t_flags & EVENP) == 0)
		lpar |= OPAR;
	if (tp->t_ospeed == B110)	/* 110 baud */
		lpar |= TWOSB;
	addr->dhlpr = lpar;
	splx(s);
}

/*
 * DH transmitter interrupt.
 * Restart each line which used to be active but has
 * terminated transmission since the last interrupt.
 */
dhxint(dh)
int	dh;
{
	register struct tty *tp;
	register struct dhdevice *addr;
	register unit;
	int ttybit, bar, *sbar;

	addr = dh_addr[dh];
	if (addr->un.dhcsr & DH_NXM) {
		addr->un.dhcsr |= DH_CNI;
		printf("dh%d:  NXM\n", dh);
	}
	sbar = &dhsar[dh];
	bar = *sbar & ~addr->dhbar;
	unit = dh << 4; ttybit = 1;
	addr->un.dhcsr &= ~DH_TI;

	for(; bar; unit++, ttybit <<= 1) {
		if(bar & ttybit) {
			*sbar &= ~ttybit;
			bar &= ~ttybit;
			tp = &dh11[unit];
			tp->t_state &= ~BUSY;
			if (tp->t_state & FLUSH)
				tp->t_state &= ~FLUSH;
			else {
#if	!defined(UCB_CLIST) || defined (UNIBUS_MAP)
				/*
				 * Clists are either:
				 *	1)  in kernel virtual space,
				 *	    which in turn lies in the
				 *	    first 64K of physical memory or
				 *	2)  at UNIBUS virtual address 0.
				 *
				 * In either case, the extension bits are 0.
				 */
				addr->un.dhcsrl	= (unit & 017) | DH_IE;
				ndflush(&q3,
				    (short)(addr->dhcar - cpaddr(q3.c_cf)));
#else	/* defined(UCB_CLIST) && !defined(UNIBUS_MAP) */
				ubadr_t car;
				int count;

				addr->un.dhcsrl	= (unit & 017) | DH_IE;
				car = (ubadr_t) addr->dhcar
				    | (ubadr_t)(addr->dhsilo & 0300) << 10;
				count = car - cpaddr(q3.c_cf);
				ndflush(&q3, count);
#endif
			}
			dhstart(tp);
		}
	}
}

/*
 * Start (restart) transmission on the given DH line.
 */
dhstart(tp)
register struct tty *tp;
{
	register struct dhdevice *addr;
	register nch;
	int s, unit;

	unit = (int) (tp - dh11);
	addr = (struct dhdevice *) tp->t_addr;

	/*
	 * Must hold interrupts in following code to prevent
	 * state of the tp from changing.
	 */
	s = spl5();

	/*
	 * If it's currently active, or delaying, no need to do anything.
	 */
	if (tp->t_state & (TIMEOUT | BUSY | TTSTOP))
		goto out;
	/*
	 * If there are sleepers, and the output has drained below low
	 * water mark, wake up the sleepers.
	 */
	if (tp->t_outq.c_cc<=TTLOWAT(tp)) {
		if (tp->t_state&ASLEEP) {
			tp->t_state &= ~ASLEEP;
#ifdef	MPX_FILS
			if (tp->t_chan)
				mcstart(tp->t_chan, (caddr_t)&tp->t_outq);
			else
#endif
				wakeup((caddr_t)&tp->t_outq);
		}
#ifdef	UCB_NET
		if (tp->t_wsel) {
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
#endif
	}

	/*
	 * Now restart transmission unless the output queue is
	 * empty.
	 */
	if (tp->t_outq.c_cc == 0)
		goto out;
#ifdef	UCB_NTTY
	if ((tp->t_flags & RAW) || (tp->t_local & LLITOUT))
#else
	if (tp->t_flags & RAW)
#endif
		nch = ndqb(&tp->t_outq, 0);
	else {
		nch = ndqb(&tp->t_outq, 0200);
		/*
		 * If first thing on queue is a delay, process it.
		 */
		if (nch == 0) {
			nch = getc(&tp->t_outq);
			timeout(ttrstrt, (caddr_t) tp, (nch & 0177) + 6);
			tp->t_state |= TIMEOUT;
			goto out;
		}
	}
	/*
	 * If characters to transmit, restart transmission.
	 */
	if (nch) {
#if	!defined(UCB_CLIST) || defined (UNIBUS_MAP)
		addr->un.dhcsrl = (unit & 017) | DH_IE;
		addr->dhcar = (short)cpaddr(tp->t_outq.c_cf);
#else	/* defined(UCB_CLIST) && !defined(UNIBUS_MAP) */
		ubadr_t uba;

		uba = cpaddr(tp->t_outq.c_cf);
		addr->un.dhcsrl	= (unit&017) | DH_IE | ((hiint(uba) << 4) & 060);
		addr->dhcar = loint(uba);
#endif
		addr->dhbcr = -nch;
		nch = 1 << (unit & 017);
		addr->dhbar |= nch;
		dhsar[unit >> 4] |= nch;
		tp->t_state |= BUSY;
	}
    out:
	splx(s);
}


/*
 * Stop output on a line, e.g. for ^S/^Q or output flush.
 */
/*ARGSUSED*/
dhstop(tp, flag)
register struct tty *tp;
{
	register struct dhdevice *addr;
	register unit;
	int s;

	addr = (struct dhdevice *)tp->t_addr;
	/*
	 * Block input/output interrupts while messing with state.
	 */
	s = spl6();
	if (tp->t_state & BUSY) {
		/*
		 * Device is transmitting; stop output
		 * by selecting the line and setting the byte
		 * count to -1.  We will clean up later
		 * by examining the address where the dh stopped.
		 */
		unit = DHLINE(tp->t_dev);
		addr->un.dhcsrl = (unit & 017) | DH_IE;
		if ((tp->t_state & TTSTOP) == 0) {
			tp->t_state |= FLUSH;
		}
		addr->dhbcr = -1;
	}
	splx(s);
}

#ifdef	DH_SILO
void
dhtimer(dev)
dev_t	dev;
{
	register dh, cc;
	register struct dhdevice *addr;

	dh = 0;
	do {
		addr = dh_addr[dh];
		cc = dhchars[dh];
		dhchars[dh] = 0;
		if (cc > 50)
			cc = 32;
		else
			if (cc > 16)
				cc = 16;
			else
				cc = 0;
		addr->dhsilo = cc;
		dhrint(dh++);
	} while (dh < NDH);
	timeout(dhtimer, (caddr_t) 0, SILOSCANRATE);
}
#endif	DH_SILO

#if	NDM > 0

dmattach(addr, unit)
struct device *addr;
{
	if ((unsigned) unit >= NDM)
		return 0;
	dm_addr[unit] = addr;
	return 1;
}

/*
 * Turn on the line associated with the dh device dev.
 */
dmopen(dev)
dev_t	dev;
{
	register struct tty *tp;
	register struct dmdevice *addr;
	register unit;
	int s;

	unit = DHLINE(dev);
	tp = &dh11[unit];
	if ((unit < dhlowdm) || (unit >= dhlowdm + dhndm)
	   || ((addr = dm_addr[(unit - dhlowdm) >> 4]) == 0)
#ifdef	DH_SOFTCAR
	   || (dev & 0200)
#endif
	   ) {
		tp->t_state |= CARR_ON;
		return;
	}
	s = spl5();
	addr->dmcsr &= ~DM_SE;
	while (addr->dmcsr & DM_BUSY)
		;
	addr->dmcsr = unit & 017;
	addr->dmlstat = DML_ON;
	if (addr->dmlstat & DML_CAR)
		tp->t_state |= CARR_ON;
	addr->dmcsr = DM_IE | DM_SE;
	while ((tp->t_state & CARR_ON)==0)
		sleep((caddr_t) &tp->t_rawq, TTIPRI);
	addr->dmcsr = unit & 017;
	if (addr->dmlstat & DML_SR) {
		tp->t_ispeed = B1200;
		tp->t_ospeed = B1200;
		dhparam(unit);
	}
	addr->dmcsr = DM_IE | DM_SE;
	splx(s);
}

/*
 * Dump control bits into the DM registers.
 */
dmctl(unit, bits, how)
register unit;
{
	register struct dmdevice *addr;
	register s;

	if(unit < dhlowdm || unit >= dhlowdm + dhndm)
		return;
	addr = dm_addr[(unit - dhlowdm) >> 4];
	s = spl5();
	addr->dmcsr &= ~DM_SE;
	while (addr->dmcsr & DM_BUSY)
		;
	addr->dmcsr = unit & 017;
	switch (how) {
		case DMSET:
			addr->dmlstat = bits;
			break;
		case DMBIS:
			addr->dmlstat |= bits;
			break;
		case DMBIC:
			addr->dmlstat &= ~bits;
			break;
		}
	addr->dmcsr = DM_IE | DM_SE;
	splx(s);
}

/*
 * DM interrupt; deal with carrier transitions.
 */
dmintr(dm)
register dm;
{
	register struct tty *tp;
	register struct dmdevice *addr;

	addr = dm_addr[dm];
	if (addr->dmcsr & DM_DONE) {
		if (addr->dmcsr & DM_CF) {
			tp = &dh11[(dm << 4) + (addr->dmcsr & 017)];
			tp += dhlowdm;
			if (tp < &dh11[dhlowdm + dhndm]) {
				wakeup((caddr_t)&tp->t_rawq);
#ifdef	UCB_NTTY
				if ((tp->t_state & WOPEN) == 0
				    && (tp->t_local & LMDMBUF)) {
					if ((addr->dmlstat & DML_CAR)) {
						tp->t_state &= ~TTSTOP;
						ttstart(tp);
					} else if ((tp->t_state&TTSTOP) == 0) {
						tp->t_state |= TTSTOP;
						dhstop(tp, 0);
					}
				} else
#endif
				if ((addr->dmlstat & DML_CAR) == 0) {
				    if ((tp->t_state & WOPEN)==0
#ifdef	UCB_NTTY
					&& (tp->t_local & LNOHANG)==0
#endif
#ifdef	DH_SOFTCAR
					&& (tp->t_dev & 0200)==0
#endif
					) {
					    gsignal(tp->t_pgrp, SIGHUP);
					    addr->dmlstat = 0;
					    flushtty(tp, FREAD|FWRITE);
				    }
				    tp->t_state &= ~CARR_ON;
				} else
					tp->t_state |= CARR_ON;
			}
		}
		addr->dmcsr = DM_IE | DM_SE;
	}
}
#endif	NDM
#endif	NDH
