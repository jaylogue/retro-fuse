/*
 *	SCCS id	@(#)dz.c	2.1 (Berkeley)	8/5/83
 */

#include "dz.h"
#if	NDZ > 0
#include "param.h"
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/file.h>
#include <sys/conf.h>
#include <sys/dzreg.h>
#ifdef	DZ_PDMA
#include <sys/pdma.h>
#endif

#ifdef	DZ_SOFTCAR
#define DZLINE(dev)	(minor(dev) & 0177)
#else
#define	DZLINE(dev)	minor(dev)
#endif

#define	NDZLINE	(NDZ * 8)
int	ndz11	= NDZLINE;		/* Only for pstat */
struct	tty	dz11[NDZLINE];
#ifdef	DZ_PDMA
struct	pdma	dzpdma[NDZLINE];
bool_t	dz_init;
#endif

struct	dzdevice *dz_addr[NDZ];

char	dz_speeds[] = {
	0,	020,	021,	022,	023,	024,	0,	025,
	026,	027,	030,	032,	034,	036,	0,	0,
};

#ifdef	DZ_IOCTL
char	dz_brk[NDZ];		/* software copy of dzbrk */
#endif

dzattach(addr, unit)
struct dzdevice *addr;
{
	if ((unsigned) unit >= NDZ)
		return 0;
	dz_addr[unit] = addr;
	return 1;
}

/*ARGSUSED*/
dzopen(dev, flag)
register dev_t	dev;
{
	register unit;
	register struct tty *tp;
	extern dzstart();
	int s;

#ifdef	DZ_PDMA
	if (dz_init == 0) {
		register struct pdma *dp;

		dz_init = 1;
		tp = dz11;
		dp = dzpdma;
		for (unit = 0; unit < NDZLINE; unit++, dp++) {
			dp->p_addr = dz_addr[unit >> 3];
			dp->p_arg = tp++;
		}
	}
#endif
	unit = DZLINE(dev);
	if (unit >= NDZLINE || (dz_addr[unit >> 3] == 0)) {
		u.u_error = ENXIO;
		return;
	}
	tp = &dz11[unit];
	if (tp->t_state & XCLUDE && u.u_uid != 0) {
		u.u_error = EBUSY;
		return;
	}
	if ((tp->t_state & (ISOPEN | WOPEN)) == 0) {
		tp->t_oproc = dzstart;
		tp->t_iproc = NULL;
		ttychars(tp);
		tp->t_ispeed = B300;
		tp->t_ospeed = B300;
		tp->t_flags = ODDP | EVENP | ECHO;
		tp->t_line = DFLT_LDISC;
#ifdef	DZ_PDMA
		tp->t_addr = &dzpdma[unit];
#endif
		dzparam(unit);
	}
	dzmodem(unit, DZ_ON);
#ifdef	DZ_SOFTCAR
	if (dev & 0200)
		tp->t_state |= CARR_ON;
	else
#endif
	{
		s = spl6();
		while ((tp->t_state & CARR_ON) == 0) {
			tp->t_state |= WOPEN;
			sleep((caddr_t) &tp->t_rawq, TTIPRI);
		}
		splx(s);
	}
	ttyopen(dev, tp);
}

/*ARGSUSED*/
dzclose(dev, flag)
dev_t	dev;
{
	register struct tty *tp;
	register unit;
#ifdef	DZ_IOCTL
	register dz;
#endif

	unit = DZLINE(dev);
	tp = &dz11[unit];
#ifdef	DZ_IOCTL
	dz = unit >> 3;
	dz_addr[dz]->dzbrk = (dz_brk[dz] &= ~(1 << (unit&07)));
#endif
	if (tp->t_state & HUPCLS)
		dzmodem(unit, DZ_OFF);
	ttyclose(tp);
}

dzread(dev)
register dev_t	dev;
{
	register struct tty *tp;

	tp = &dz11[DZLINE(dev)];
	(*linesw[tp->t_line].l_read)(tp);
}

dzwrite(dev)
register dev_t	dev;
{
	register struct tty *tp;

	tp = &dz11[DZLINE(dev)];
	(*linesw[tp->t_line].l_write)(tp);
}

dzioctl(dev, cmd, addr, flag)
dev_t	dev;
int cmd;
caddr_t	addr;
int	flag;
{
	register unit;
	register struct tty *tp;
#ifdef	DZ_IOCTL
	register dz;
#endif

	unit = DZLINE(dev);
	tp = &dz11[unit];
#ifdef	DZ_IOCTL
	dz = unit >> 3;
#endif
	switch (ttioctl(tp, cmd, addr, flag)) {
		case TIOCSETP:
		case TIOCSETN:
			dzparam(unit);
			break;
#ifdef	DZ_IOCTL
		case TIOCSBRK:
			dz_addr[dz]->dzbrk = (dz_brk[dz] |= (1 << (unit&07)));
			break;
		case TIOCCBRK:
			dz_addr[dz]->dzbrk = (dz_brk[dz] &= ~(1 << (unit&07)));
			break;
		case TIOCSDTR:
			dzmodem(unit, DZ_ON);
			break;
		case TIOCCDTR:
			dzmodem(unit, DZ_OFF);
			break;
#endif
		default:
			u.u_error = ENOTTY;
		case 0:
			break;
	}
}

dzparam(unit)
int	unit;
{
	register struct tty *tp;
	register struct dzdevice *addr;
	register lpr;
	static	dz_timer;
	void	dzscan();
#ifdef	DZ_SILO
	void	dzrscan();
#endif

	tp = &dz11[unit];
	addr = dz_addr[unit >> 3];
	addr->dzcsr = DZ_IEN;
	if (dz_timer == 0) {
		dz_timer++;
		timeout(dzscan, (caddr_t) 0, 1);
#ifdef	DZ_SILO
		timeout(dzrscan, (caddr_t)0, SILOSCANRATE);
#endif
	}
	if (tp->t_ispeed == 0) {	/* Hang up line */
		dzmodem(unit, DZ_OFF);
		return;
	}
	lpr = (dz_speeds[tp->t_ispeed] << 8) | (unit & 07);
#ifdef	UCB_NTTY
	if ((tp->t_local & LLITOUT) || (tp->t_flags & RAW))
#else
	if (tp->t_flags & RAW)
#endif
		lpr |= BITS8;
	else
		lpr |= BITS7 | PENABLE;
	if ((tp->t_flags & EVENP) == 0)
		lpr |= OPAR;
	if (tp->t_ispeed == B110)
		lpr |= TWOSB;
	addr->dzlpr = lpr;
}

dzrint(dz)
int dz;
{
	register struct tty *tp;
	register c;
	register struct dzdevice *addr;
	struct	tty *tp0;
	int	overrun = 0;
#ifdef	DZ_SILO
	int s;

	s = spl6();
#endif
	if ((unsigned) dz >= NDZ)
		return;
	addr = dz_addr[dz];
	tp0 = &dz11[dz << 3];
	while ((c = addr->dzrbuf) < 0) {	/* char. present */
		tp = tp0 + ((c >> 8) & 07);
		if (tp >= &dz11[NDZLINE])
			continue;
		if((tp->t_state & ISOPEN) == 0) {
			wakeup((caddr_t) &tp->t_rawq);
			continue;
		}
#ifdef  TEXAS_AUTOBAUD
		if (image_mode(tp))
			c &= ~(DZ_FE|DZ_PE);
#endif
		if (c & DZ_FE)
			if (tp->t_flags & RAW)
				c = 0;
			else
				c = tun.t_intrc;
		if (c & DZ_DO && overrun == 0) {
			printf("dz%d: silo overflow\n", dz);
			overrun = 1;
		}
		if (c & DZ_PE)
			if ((tp->t_flags & (EVENP|ODDP)) == EVENP
			 || (tp->t_flags & (EVENP|ODDP)) == ODDP)
				continue;
		(*linesw[tp->t_line].l_input)(c, tp);
	}
#ifdef	DZ_SILO
	splx(s);
#endif
}

#ifdef	DZ_PDMA
/*
 * dzxint is called from dzdma after the last of the characters set up
 * has been sent.
 */
dzxint(tp)
register struct tty *tp;
{
	register struct pdma *dp;

	dp = (struct pdma *) tp->t_addr;
	tp->t_state &= ~BUSY;
	if (tp->t_state & FLUSH)
		tp->t_state &= ~FLUSH;
	else
		ndflush(&tp->t_outq, dp->p_mem - tp->t_outq.c_cf);
	dzstart(tp);
	if ((tp->t_outq.c_cc == 0) || (tp->t_state & BUSY) == 0)
		dp->p_addr->dztcr &= ~(1 << (DZLINE(tp->t_dev) & 07));
}

#else	DZ_PDMA
dzxint(dz)
int dz;
{
	register struct tty *tp, *tp0;
	register struct dzdevice *addr;

	addr = dz_addr[dz];
	tp0 = &dz11[dz << 3];
	while(addr->dzcsr < 0) {
		tp = tp0 + ((addr->dzcsr >> 8) & 07);
		addr->dztbuf = tp->t_char;
		tp->t_state &= ~BUSY;
		dzstart(tp);
	}
}
#endif	DZ_PDMA

dzstart(tp)
register struct tty *tp;
{
#ifdef	DZ_PDMA
	register struct pdma *dp;
	register cc;
	int s;
	struct dzdevice *addr;
	extern ttrstrt();

	dp = (struct pdma *) tp->t_addr;
	addr = dp->p_addr;
	s = spl5();
	if (tp->t_state & (TIMEOUT | BUSY | TTSTOP))
		goto out;
	if (tp->t_outq.c_cc<=TTLOWAT(tp)) {
		if (tp->t_state & ASLEEP) {
			tp->t_state &= ~ASLEEP;
#if	MPX_FILS
			if (tp->t_chan)
				mcstart(tp->t_chan, (caddr_t) &tp->t_outq);
			else
#endif
			wakeup((caddr_t) &tp->t_outq);
		}
#ifdef UCB_NET
		if (tp->t_wsel) {
			selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
			tp->t_wsel = 0;
			tp->t_state &= ~TS_WCOLL;
		}
#endif
	}
	if (tp->t_outq.c_cc == 0)
		goto out;
	if ((tp->t_flags & RAW)
#ifdef	UCB_NTTY
	    || (tp->t_local & LLITOUT)
#endif
	    )
		cc = ndqb(&tp->t_outq, 0);
	else {
		cc = ndqb(&tp->t_outq, 0200);
		if (cc == 0) {
			cc = getc(&tp->t_outq);
			timeout(ttrstrt, (caddr_t) tp, (cc & 0177) + 6);
			tp->t_state |= TIMEOUT;
			goto out;
		}
	}
	tp->t_state |= BUSY;
	dp->p_end = dp->p_mem = tp->t_outq.c_cf;
	dp->p_end += cc;
	addr->dztcr |= 1 << ((DZLINE(tp->t_dev) & 07));
out:
	splx(s);


#else	DZ_PDMA
	register unit, c;
	int s;
	struct dzdevice *addr;
	extern ttrstrt();

	unit = (int) (tp - dz11);
	addr = dz_addr[unit >> 3];
	unit = 1 << (unit & 07);
	s = spl5();
	if (tp->t_state & (TIMEOUT | BUSY))
		goto out;
	if (tp->t_state & TTSTOP) {
		addr->dztcr &= ~unit;
		goto out;
	}
	if ((c=getc(&tp->t_outq)) >= 0) {
		if (c >= 0200 && (tp->t_flags & RAW) == 0
#ifdef	UCB_NTTY
		    && (tp->t_local & LLITOUT) == 0)
#endif
		    {
			addr->dztcr &= ~unit;
			tp->t_state |= TIMEOUT;
			timeout(ttrstrt, (caddr_t) tp, (c & 0177) + 6);
		} else
			{
			tp->t_char = c;
			tp->t_state |= BUSY;
			addr->dztcr |= unit;
		}
		if (tp->t_outq.c_cc<=TTLOWAT(tp)) {
			if (tp->t_state & ASLEEP) {
				tp->t_state &= ~ASLEEP;
#if	MPX_FILS
				if (tp->t_chan)
					mcstart(tp->t_chan, (caddr_t) &tp->t_outq);
				else
#endif
				wakeup((caddr_t) &tp->t_outq);
			}
#ifdef UCB_NET
			if (tp->t_wsel) {
				selwakeup(tp->t_wsel, tp->t_state & TS_WCOLL);
				tp->t_wsel = 0;
				tp->t_state &= ~TS_WCOLL;
			}
#endif
		}
	} else
		addr->dztcr &= ~unit;
out:
	splx(s);
#endif	DZ_PDMA
}

#ifdef	DZ_PDMA
/*ARGSUSED*/
dzstop(tp, flag)
register struct tty *tp;
{
	register struct pdma *dp;
	register int s;

	dp = (struct pdma *) tp->t_addr;
	s = spl5();
	if (tp->t_state & BUSY) {
		dp->p_end = dp->p_mem;
		if ((tp->t_state & TTSTOP) == 0)
			tp->t_state |= FLUSH;
	}
	splx(s);
}
#endif

dzmodem(unit, flag)
register unit;
{
	register struct dzdevice *addr;
	register bit;

	addr = dz_addr[unit >> 3];
	bit = 1 << (unit & 07);
	if (flag == DZ_OFF)
		addr->dzdtr &= ~bit;
	else
		addr->dzdtr |= bit;
}

dzscan()
{
	register unit;
	register struct dzdevice *addr;
	register struct tty *tp;
	char bit;

	for (unit = 0; unit < NDZLINE; unit++) {
		addr = dz_addr[unit >> 3];
		tp = &dz11[unit];
		bit = 1 << (unit & 07);
		if (addr->dzcar & bit) {
			/* carrier present */
			if ((tp->t_state & CARR_ON) == 0) {
				wakeup((caddr_t) &tp->t_rawq);
				tp->t_state |= CARR_ON;
			}
		} else
			{
			if ((tp->t_state & CARR_ON)
#ifdef	DZ_SOFTCAR
			    && ((tp->t_dev & 0200) == 0)
#endif
#ifdef	UCB_NTTY
			    && ((tp->t_local & LNOHANG) == 0)
#endif
			    ) {
				/* carrier lost */
				if (tp->t_state & ISOPEN) {
					gsignal(tp->t_pgrp, SIGHUP);
					addr->dzdtr &= ~bit;
					flushtty(tp, FREAD | FWRITE);
				}
				tp->t_state &= ~CARR_ON;
			}
		}
	}
	timeout(dzscan, (caddr_t) 0, 2 * hz);
}

#ifdef	DZ_SILO
dzrscan()
{
	register dz;
	register struct dzdevice *addr;

	for (dz = 0; dz < NDZ; dz++) {
		addr = dz_addr[dz];
		if (addr->dzcsr & DZ_RDO)
			dzrint(dz);
	}
	timeout(dzrscan, (caddr_t)0, SILOSCANRATE);
};
#endif	DZ_SILO
#endif	NDZ
