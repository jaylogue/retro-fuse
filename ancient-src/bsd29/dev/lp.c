/*
 *	SCCS id	@(#)lp.c	2.1 (Berkeley)	10/6/83
 */

#include "lp.h"
#if NLP > 0
#include "param.h"
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/lpreg.h>

/*
 * LP-11 Line printer driver
 *
 * This driver has been modified to work on printers where
 * leaving LP_IE set would cause continuous interrupts.
 */


#define	LPPRI	(PZERO + 8)
#define	LPLWAT	40
#define	LPHWAT	400

#define MAXCOL	132
#define CAP	1

#define LPUNIT(dev) (minor(dev) >> 3)

struct lp_softc	{
	struct	clist sc_outq;
	int	sc_state;
	int	sc_physcol;
	int	sc_logcol;
	int	sc_physline;
	char	sc_flags;
	int	sc_lpchar;
} lp_softc[NLP];

/* bits for state */
#define	OPEN		1	/* device is open */
#define	TOUT		2	/* timeout is active */
#define	MOD		4	/* device state has been modified */
#define	ASLP		8	/* awaiting draining of printer */

extern	lbolt;
struct	lpdevice *lp_addr[NLP];
int	lptout();

lpattach(addr, unit)
struct lpdevice *addr;
{
	if ((unsigned) unit >= NLP)
		return(0);
	lp_addr[unit] = addr;
	return(1);
}

/*ARGSUSED*/
lpopen(dev, flag)
dev_t	dev;
int	flag;
{
	register int	unit;
	register struct	lp_softc *sc;

	if (((unit = LPUNIT(dev)) >= NLP) || (lp_addr[unit] == 0)) {
		u.u_error = ENXIO;
		return;
	}
	else
		if ((sc = &lp_softc[unit])->sc_state & OPEN) {
			u.u_error = EBUSY;
			return;
		}
	if (lp_addr[unit]->lpcs & LP_ERR) {
		u.u_error = EIO;
		return;
	}
	sc->sc_state |= OPEN;
	sc->sc_flags = minor(dev) & 07;
	(void) _spl4();
	if ((sc->sc_state & TOUT) == 0) {
		sc->sc_state |= TOUT;
		timeout(lptout, (caddr_t)dev, 10 * hz);
	}
	(void) _spl0();
	lpcanon(dev, '\f');
}

/*ARGSUSED*/
lpclose(dev, flag)
dev_t	dev;
int	flag;
{
	register struct	lp_softc *sc = &lp_softc[LPUNIT(dev)];

	lpcanon(dev, '\f');
	sc->sc_state &= ~OPEN;
}

lpwrite(dev)
dev_t	dev;
{
	register char c;

	while (u.u_count) {
		c = fubyte(u.u_base++);
		if (c < 0) {
			u.u_error = EFAULT;
			break;
		}
		u.u_count--;
		lpcanon(dev, c);
	}
}

lpcanon(dev, c)
dev_t	dev;
register c;
{
	register logcol, physcol;
	register struct lp_softc *sc = &lp_softc[LPUNIT(dev)];

	if (sc->sc_flags & CAP) {
		register c2;

		if (c >= 'a' && c <= 'z')
			c += 'A'-'a'; else
		switch (c) {

			case '{':
				c2 = '(';
				goto esc;

			case '}':
				c2 = ')';
				goto esc;

			case '`':
				c2 = '\'';
				goto esc;

			case '|':
				c2 = '!';
				goto esc;

			case '~':
				c2 = '^';

			esc:
				lpcanon(dev, c2);
				sc->sc_logcol--;
				c = '-';
		}
	}
	logcol = sc->sc_logcol;
	physcol = sc->sc_physcol;
	if (c == ' ')
		logcol++;
	else switch(c) {

		case '\t':
			logcol = (logcol + 8) & ~7;
			break;

		case '\f':
			if (sc->sc_physline == 0 && physcol == 0)
				break;
			/* fall into ... */

		case '\n':
			lpoutput(dev, c);
			if (c == '\f')
				sc->sc_physline = 0;
			else
				sc->sc_physline++;
			physcol = 0;
			/* fall into ... */

		case '\r':
			logcol = 0;
			(void) _spl4();
			lpintr(LPUNIT(dev));
			(void) _spl0();
			break;

		case '\b':
			if (logcol > 0)
				logcol--;
			break;

		default:
			if (logcol < physcol) {
				lpoutput(dev, '\r');
				physcol = 0;
			}
			if (logcol < MAXCOL) {
				while (logcol > physcol) {
					lpoutput(dev, ' ');
					physcol++;
				}
				lpoutput(dev, c);
				physcol++;
			}
			logcol++;
	}
	if (logcol > 1000)	/* ignore long lines  */
		logcol = 1000;
	sc->sc_logcol = logcol;
	sc->sc_physcol = physcol;
}

lpoutput(dev, c)
dev_t	dev;
int	c;
{
	register struct	lp_softc *sc = &lp_softc[LPUNIT(dev)];

	if (sc->sc_outq.c_cc >= LPHWAT) {
		(void) _spl4();
		lpintr(LPUNIT(dev));				/* unchoke */
		while (sc->sc_outq.c_cc >= LPHWAT) {
			sc->sc_state |= ASLP;		/* must be LP_ERR */
			sleep((caddr_t)sc, LPPRI);
		}
		(void) _spl0();
	}
	while (putc(c, &sc->sc_outq))
		sleep((caddr_t)&lbolt, LPPRI);
}

lpintr(lp11)
int	lp11;
{
	register n;
	register struct	lp_softc *sc = &lp_softc[lp11];
	register struct	lpdevice *lpaddr;

	lpaddr = lp_addr[lp11];
	lpaddr->lpcs &= ~LP_IE;
	n = sc->sc_outq.c_cc;
	if (sc->sc_lpchar < 0)
		sc->sc_lpchar = getc(&sc->sc_outq);
	while ((lpaddr->lpcs & LP_RDY) && sc->sc_lpchar >= 0) {
		lpaddr->lpdb = sc->sc_lpchar;
		sc->sc_lpchar = getc(&sc->sc_outq);
	}
	sc->sc_state |= MOD;
	if (sc->sc_outq.c_cc > 0 && (lpaddr->lpcs & LP_ERR) == 0)
		lpaddr->lpcs |= LP_IE;	/* ok and more to do later */
	if (n > LPLWAT && sc->sc_outq.c_cc <= LPLWAT && sc->sc_state & ASLP) {
		sc->sc_state &= ~ASLP;
		wakeup((caddr_t)sc);		/* top half should go on */
	}
}

lptout(dev)
dev_t	dev;
{
	register struct	lp_softc *sc;
	register struct	lpdevice *lpaddr;

	lpaddr = lp_addr[LPUNIT(dev)];
	sc = &lp_softc[LPUNIT(dev)];
	if ((sc->sc_state & MOD) != 0) {
		sc->sc_state &= ~MOD;		/* something happened */
		timeout(lptout, (caddr_t)dev, 2 * hz);	/* so don't sweat */
		return;
	}
	if ((sc->sc_state & OPEN) == 0) {
		sc->sc_state &= ~TOUT;		/* no longer open */
		lpaddr->lpcs = 0;
		return;
	}
	if (sc->sc_outq.c_cc && (lpaddr->lpcs & LP_RDY)
	    && (lpaddr->lpcs & LP_ERR) == 0)
		lpintr(LPUNIT(dev));			/* ready to go */
	timeout(lptout, (caddr_t)dev, 10 * hz);
}
