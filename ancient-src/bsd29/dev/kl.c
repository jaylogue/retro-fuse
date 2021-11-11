/*
 *   KL/DL-11 driver
 */
#include "kl.h"
#include "param.h"
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/klreg.h>

/*
 *	SCCS id	@(#)kl.c	2.1 (Berkeley)	9/1/83
 */

extern	struct	dldevice *KLADDR;
/*
 * Normal addressing:
 * minor 0 addresses KLADDR
 * minor 1 thru n-1 address from KLBASE (0176600),
 *    where n is the number of additional KL11's
 * minor n on address from DLBASE (0176500)
 */

struct	tty kl11[NKL];
int	nkl11	= NKL;		/* for pstat */
int	klstart();
int	ttrstrt();
extern	char	partab[];

klattach(addr, unit)
struct dldevice *addr;
{
	if ((unsigned) unit <= NKL) {
		kl11[unit].t_addr = addr;
		return(1);
	}
	return(0);
}

/*ARGSUSED*/
klopen(dev, flag)
dev_t	dev;
{
	register struct dldevice *addr;
	register struct tty *tp;
	register d;

	d = minor(dev);
	tp = &kl11[d];
	if ((d == 0) && (tp->t_addr == 0))
		tp->t_addr = KLADDR;
	if ((d >= NKL) || ((addr = tp->t_addr) == 0)) {
		u.u_error = ENXIO;
		return;
	}
	tp->t_oproc = klstart;
	if ((tp->t_state & ISOPEN) == 0) {
		tp->t_state = ISOPEN | CARR_ON;
		tp->t_flags = EVENP | ECHO | XTABS | CRMOD;
		tp->t_line = DFLT_LDISC;
		ttychars(tp);
	} else if (tp->t_state & XCLUDE && u.u_uid != 0) {
		u.u_error = EBUSY;
		return;
	}
	addr->dlrcsr |= DL_RIE | DL_DTR | DL_RE;
	addr->dlxcsr |= DLXCSR_TIE;
	ttyopen(dev, tp);
}

/*ARGSUSED*/
klclose(dev, flag)
dev_t	dev;
int	flag;
{
	ttyclose(&kl11[minor(dev)]);
}

klread(dev)
dev_t	dev;
{
	register struct tty *tp;

	tp = &kl11[minor(dev)];
	(*linesw[tp->t_line].l_read)(tp);
}

klwrite(dev)
dev_t	dev;
{
	register struct tty *tp;

	tp = &kl11[minor(dev)];
	(*linesw[tp->t_line].l_write)(tp);
}

klxint(dev)
dev_t	dev;
{
	register struct tty *tp;

	tp = &kl11[minor(dev)];
	ttstart(tp);
	if (tp->t_state & ASLEEP && tp->t_outq.c_cc <= TTLOWAT(tp))
#ifdef	MPX_FILS
		if (tp->t_chan)
			mcstart(tp->t_chan, (caddr_t) &tp->t_outq);
		else
#endif
			wakeup((caddr_t) &tp->t_outq);
}

klrint(dev)
dev_t	dev;
{
	register int c;
	register struct dldevice *addr;
	register struct tty *tp;

	tp = &kl11[minor(dev)];
	addr = (struct dldevice *) tp->t_addr;
	c = addr->dlrbuf;
	addr->dlrcsr |= DL_RE;
	(*linesw[tp->t_line].l_input)(c, tp);
}

klioctl(dev, cmd, addr, flag)
caddr_t	addr;
dev_t	dev;
{
	switch (ttioctl(&kl11[minor(dev)], cmd, addr, flag)) {
		case TIOCSETN:
		case TIOCSETP:
		case 0:
			break;
		default:
			u.u_error = ENOTTY;
	}
}

klstart(tp)
register struct tty *tp;
{
	register c;
	register struct dldevice *addr;

	addr = (struct dldevice *) tp->t_addr;
	if ((addr->dlxcsr & DLXCSR_TRDY) == 0)
		return;
	if ((c=getc(&tp->t_outq)) >= 0) {
		if (tp->t_flags & RAW)
			addr->dlxbuf = c;
		else if (c<=0177)
			addr->dlxbuf = c | (partab[c] & 0200);
		else {
			timeout(ttrstrt, (caddr_t)tp, (c & 0177) + DLDELAY);
			tp->t_state |= TIMEOUT;
		}
	}
}

char *msgbufp = msgbuf;		/* Next saved printf character */
/*
 * Print a character on console (or users terminal if touser).
 * Attempts to save and restore device
 * status.
 * If the last character input from the console was a break
 * (null or del), all printing is inhibited.
 *
 * Whether or not printing is inhibited,
 * the last MSGBUFS characters
 * are saved in msgbuf for inspection later.
 */

#ifdef	UCB_UPRINTF
putchar(c, touser)
#else
putchar(c)
#endif
register c;
{
	register s;
	register struct	dldevice *kladdr = KLADDR;
	long	timo;
	extern	char *panicstr;

#ifdef	UCB_UPRINTF
	if (touser) {
		register struct tty *tp = u.u_ttyp;
		if (tp && (tp->t_state & CARR_ON)) {
			register s = spl6();

			if (c == '\n')
				(*linesw[tp->t_line].l_output)('\r', tp);
			(*linesw[tp->t_line].l_output)(c, tp);
			ttstart(tp);
			splx(s);
		}
		return;
	}
#endif
	if (c != '\0' && c != '\r' && c != 0177) {
		*msgbufp++ = c;
		if (msgbufp >= &msgbuf[MSGBUFS])
			msgbufp	= msgbuf;
	}

	/*
	 *  If last char was a break or null, don't print
	 */
	if (panicstr == (char *) 0)
		if ((kladdr->dlrbuf & 0177) == 0)
			return;
	timo = 60000L;
	/*
	 * Try waiting for the console tty to come ready,
	 * otherwise give up after a reasonable time.
	 */
	while((kladdr->dlxcsr & DLXCSR_TRDY) == 0)
		if (--timo == 0L)
			break;
	if (c == 0)
		return;
	s = kladdr->dlxcsr;
	kladdr->dlxcsr = 0;
	kladdr->dlxbuf = c;
	if (c == '\n') {
#ifdef	UCB_UPRINTF
		putchar('\r', 0);
		putchar(0177, 0);
		putchar(0177, 0);
#else
		putchar('\r');
		putchar(0177);
		putchar(0177);
#endif
	}
#ifdef	UCB_UPRINTF
	putchar(0, 0);
#else
	putchar(0);
#endif
	kladdr->dlxcsr = s;
}
