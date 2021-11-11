/*
 *	SCCS id	@(#)tty.c	2.1 (Berkeley)	8/5/83
 */

/*
 * TTY subroutines common to more than one line discipline
 */

#include "param.h"
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/proc.h>
#ifdef	MPX_FILS
#include <sys/mx.h>
#endif
#include <sys/inode.h>
#include <sys/file.h>
#include <sys/reg.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include "bk.h"

extern	char	partab[];

/*
 * Input mapping table-- if an entry is non-zero, when the
 * corresponding character is typed preceded by "\" the escape
 * sequence is replaced by the table value.  Only used for
 * upper-case only terminals.
 */

char	maptab[] ={
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,'|',000,000,000,000,000,'`',
	'{','}',000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,000,000,
	000,000,000,000,000,000,'~',000,
	000,'A','B','C','D','E','F','G',
	'H','I','J','K','L','M','N','O',
	'P','Q','R','S','T','U','V','W',
	'X','Y','Z',000,000,000,000,000,
};

short	tthiwat[16] =
   { 100,100,100,100,100,100,100,200,200,400,400,400,650,650,650,650 };
short	ttlowat[16] =
   {  30, 30, 30, 30, 30, 30, 30, 50, 50,120,120,120,125,125,125,125 };

#define	OBUFSIZ	100

/*
 * routine called on teletype open.
 * establishes a process group for distribution
 * of quits and interrupts from the tty.
 * Then calls open routine for default line discipline.
 */
ttyopen(dev, tp)
dev_t dev;
register struct tty *tp;
{
	register struct proc *pp;

	pp = u.u_procp;
	tp->t_dev = dev;
	if(pp->p_pgrp == 0) {
		u.u_ttyp = tp;
		u.u_ttyd = dev;
		if (tp->t_pgrp == 0)
			tp->t_pgrp = pp->p_pid;
		pp->p_pgrp = tp->t_pgrp;
	}
	tp->t_state &= ~WOPEN;
	tp->t_state |= ISOPEN;
	(*linesw[tp->t_line].l_open)(tp);
}

/*
 * clean tp on last close
 */
ttyclose(tp)
register struct tty *tp;
{
	(*linesw[tp->t_line].l_close)(tp);
	tp->t_pgrp = 0;
	wflushtty(tp);
	tp->t_state = 0;
}
/*
 * set default control characters.
 */
ttychars(tp)
register struct tty *tp;
{

	tun.t_intrc = CINTR;
	tun.t_quitc = CQUIT;
	tun.t_startc = CSTART;
	tun.t_stopc = CSTOP;
	tun.t_eofc = CEOT;
	tun.t_brkc = CBRK;
	tp->t_erase = CERASE;
	tp->t_kill = CKILL;
#ifdef	UCB_NTTY
	tlun.t_suspc = CTRL(z);
	tlun.t_dsuspc = CTRL(y);
	tlun.t_rprntc = CTRL(r);
	tlun.t_flushc = CTRL(o);
	tlun.t_werasc = CTRL(w);
	tlun.t_lnextc = CTRL(v);
	tp->t_local = 0;
	tp->t_lstate = 0;
#endif
}

/*
 * Wait for output to drain, then flush input waiting.
 */
wflushtty(tp)
register struct tty *tp;
{

	(void) _spl5();
	while (tp->t_outq.c_cc && tp->t_state&CARR_ON) {
		(*tp->t_oproc)(tp);
		tp->t_state |= ASLEEP;
		sleep((caddr_t)&tp->t_outq, TTOPRI);
	}
	flushtty(tp, FREAD|FWRITE);
	(void) _spl0();
}

/*
 * flush all TTY queues
 */
flushtty(tp, rw)
register struct tty *tp;
register rw;
{
	register s;

#if	NBK > 0
	if (tp->t_line == NETLDISC)
		return;
#endif
	s = spl6();
	if (rw & FREAD) {
		while (getc(&tp->t_canq) >= 0)
			;
		wakeup((caddr_t)&tp->t_rawq);
	}
	if (rw & FWRITE) {
		tp->t_state &= ~TTSTOP;
		(*cdevsw[major(tp->t_dev)].d_stop)(tp);
		while (getc(&tp->t_outq) >= 0)
			;
		wakeup((caddr_t)&tp->t_outq);
	}
	if (rw & FREAD) {
		while (getc(&tp->t_rawq) >= 0)
			;
		tp->t_delct = 0;
#ifdef	UCB_NTTY
		tp->t_rocount = 0;
		tp->t_rocol = 0;
		tp->t_lstate = 0;
#endif
	}
	splx(s);
}

/*
 * Send stop character on input overflow.
 */
ttyblock(tp)
register struct tty *tp;
{
	register x;
	x = tp->t_rawq.c_cc + tp->t_canq.c_cc;
	if (tp->t_rawq.c_cc > TTYHOG) {
		flushtty(tp, FREAD|FWRITE);
		tp->t_state &= ~TBLOCK;
	}
	/*
	 * Block further input iff:
	 * Current input > threshold AND input is available to user program
	 */
	if (x >= TTYHOG/2 && (tp->t_delct>0 || (tp->t_flags&(RAW|CBREAK)))) {
		if (putc(tun.t_stopc, &tp->t_outq)==0) {
			tp->t_state |= TBLOCK;
			ttstart(tp);
		}
	}
}

/*
 * Restart typewriter output following a delay
 * timeout.
 * The name of the routine is passed to the timeout
 * subroutine and it is called during a clock interrupt.
 */
ttrstrt(tp)
register struct tty *tp;
{

#ifdef	DIAGNOSTIC
	if (tp == 0) {
		printf("ttrstrt: arg was 0!\n");
		return;
	}
#endif
	tp->t_state &= ~TIMEOUT;
	ttstart(tp);
}

/*
 * Start output on the typewriter. It is used from the top half
 * after some characters have been put on the output queue,
 * from the interrupt routine to transmit the next
 * character, and after a timeout has finished.
 */
ttstart(tp)
register struct tty *tp;
{
	register s;

	s = spl5();
	if((tp->t_state&(TIMEOUT|TTSTOP|BUSY)) == 0)
		(*tp->t_oproc)(tp);
	splx(s);
}

/*
 * Common code for tty ioctls.
 * Return values:
 *	cmd			cmd is not done (driver must do some/all)
 *	0			cmd is done 
 *	0 (u.u_error set)	cmd was rejected by line disc. code
 */
/*ARGSUSED*/
ttioctl(tp, com, addr, flag)
register struct tty *tp;
caddr_t addr;
{
	register dev_t dev;
	unsigned t;
	struct sgttyb iocb;
	struct clist tq;
	extern int nldisp;
	register c;
	int temp;
	extern nodev();

	/*
	 *  First, give the line discipline a chance.
	 *  If it returns 0, either the work is done
	 *  or the request was refused.
	 *  Return 0 to keep the device driver from continuing.
	 */
	if ((com = (*linesw[tp->t_line].l_ioctl)(tp,com,addr,flag)) == 0)
		return(0);
	/*
	 * This is especially so that isatty() will
	 * fail when carrier is gone.
	 */
	if ((tp->t_state&CARR_ON) == 0) {
		u.u_error = EBADF;
		return (0);
	}

	dev = tp->t_dev;
	/*
	 * If the ioctl involves modification,
	 * hang if in the background.
	 */
#ifdef	MENLO_JCL
	switch(com) {

	case TIOCSETD:
	case TIOCSETP:
	case TIOCSETN:
	case TIOCFLUSH:
	case TIOCSETC:
	case TIOCSLTC:
	case TIOCSPGRP:
	case TIOCLBIS:
	case TIOCLBIC:
	case TIOCLSET:
	case TIOCSTI:
		while (tp->t_line == NTTYDISC &&
		   u.u_procp->p_pgrp != tp->t_pgrp && tp == u.u_ttyp &&
#ifdef	VIRUS_VFORK
		   (u.u_procp->p_flag&SVFORK) == 0 &&
#endif
		   u.u_signal[SIGTTOU] != SIG_IGN &&
		   u.u_signal[SIGTTOU] != SIG_HOLD &&
		   (u.u_procp->p_flag&SDETACH)==0) {
			gsignal(u.u_procp->p_pgrp, SIGTTOU);
			sleep((caddr_t)&lbolt, TTOPRI);
		}
		break;
	}
#endif

	/*
	 * Process the ioctl.
	 */
	switch(com) {

	/*
	 * Get discipline number
	 */
	case TIOCGETD:
		t = tp->t_line;
		if (copyout((caddr_t)&t, addr, sizeof(t)))
			u.u_error = EFAULT;
		break;

	/*
	 * Set line discipline
	 */
	case TIOCSETD:
		if (copyin(addr, (caddr_t)&t, sizeof(t))) {
			u.u_error = EFAULT;
			break;
		}
		if ((t >= nldisp) || (linesw[t].l_open == nodev)) {
			u.u_error = ENXIO;
			break;
		}
		if (t != tp->t_line) {
			(void) _spl5();
			(*linesw[tp->t_line].l_close)(tp);
			(*linesw[t].l_open)(tp);
			if (u.u_error==0)
				tp->t_line = t;
			(void) _spl0();
		}
		break;

	/*
	 * Prevent more opens on channel
	 */
	case TIOCEXCL:
		tp->t_state |= XCLUDE;
		break;

	case TIOCNXCL:
		tp->t_state &= ~XCLUDE;
		break;

	/*
	 * Set new parameters
	 */
	case TIOCSETP:
	case TIOCSETN:
		if (copyin(addr, (caddr_t)&iocb, sizeof(iocb))) {
			u.u_error = EFAULT;
			return(0);
		}
		(void) _spl5();
#ifdef	OLDTTY
		if (tp->t_line == OTTYDISC) {
			if (com == TIOCSETP)
				wflushtty(tp);
			while (canon(tp)>=0) 
				;
		}
#endif	OLDTTY
#ifdef	UCB_NTTY
		if (tp->t_line == NTTYDISC) {
			if (tp->t_flags&RAW || iocb.sg_flags&RAW ||
			    com == TIOCSETP)
				wflushtty(tp);
			else if ((tp->t_flags&CBREAK) != (iocb.sg_flags&CBREAK)) {
				if (iocb.sg_flags & CBREAK) {
					catq(&tp->t_rawq, &tp->t_canq);
					tq = tp->t_rawq;
					tp->t_rawq = tp->t_canq;
					tp->t_canq = tq;
				} else {
					tp->t_local |= LPENDIN;
#ifdef	DIAGNOSTIC
					if (tp->t_canq.c_cc)
						panic("ttioctl canq");
#endif
#ifdef MPX_FILS
					if (tp->t_chan)
						(void) sdata(tp->t_chan);
					else
#endif
						wakeup((caddr_t)&tp->t_rawq);
				}
			}
#endif
		}
		if ((tp->t_state&SPEEDS)==0) {
			tp->t_ispeed = iocb.sg_ispeed;
			tp->t_ospeed = iocb.sg_ospeed;
		}
		tp->t_erase = iocb.sg_erase;
		tp->t_kill = iocb.sg_kill;
		tp->t_flags = iocb.sg_flags;
		(void) _spl0();
		return(com);

	/*
	 * Send current parameters to user
	 */
	case TIOCGETP:
		iocb.sg_ispeed = tp->t_ispeed;
		iocb.sg_ospeed = tp->t_ospeed;
		iocb.sg_erase = tp->t_erase;
		iocb.sg_kill = tp->t_kill;
		iocb.sg_flags = tp->t_flags;
		if (copyout((caddr_t)&iocb, addr, sizeof(iocb)))
			u.u_error = EFAULT;
		break;

	/*
	 * Hang up line on last close
	 */
	case TIOCHPCL:
		tp->t_state |= HUPCLS;
		break;

	case TIOCFLUSH: {
		int flags;
		if (copyin(addr, (caddr_t) &flags, sizeof (flags))) {
			u.u_error = EFAULT;
			return (1);
		}
		if (flags == 0)
			flags = FREAD | FWRITE;
		else
			flags &= FREAD | FWRITE;
		flushtty(tp, flags);
		break;
	}

#ifdef	UCB_NET
	case FIONBIO: {
		int nbio;
		if (copyin(addr, (caddr_t)&nbio, sizeof (nbio))) {
			u.u_error = EFAULT;
			return(1);
		}
		if (nbio)
			tp->t_state |= TS_NBIO;
		else
			tp->t_state &= ~TS_NBIO;
		break;
	}

	case FIOASYNC: {
		int async;
		if (copyin(addr, (caddr_t)&async, sizeof (async))) {
			u.u_error = EFAULT;
			return(1);
		}
		if (async)
			tp->t_state |= TS_ASYNC;
		else
			tp->t_state &= ~TS_ASYNC;
		break;
	}
#endif

	/*
	 * Set and fetch special characters
	 */
	case TIOCSETC:
		if (copyin(addr, (caddr_t)&tun, sizeof(struct tchars)))
			u.u_error = EFAULT;
		break;

	case TIOCGETC:
		if (copyout((caddr_t)&tun, addr, sizeof(struct tchars)))
			u.u_error = EFAULT;
		break;

#ifdef	UCB_NTTY
	/*
	 * Set/get local special characters.
	 */
	case TIOCSLTC:
		if (copyin(addr, (caddr_t)&tlun, sizeof (struct ltchars)))
			u.u_error = EFAULT;
		break;

	case TIOCGLTC:
		if (copyout((caddr_t)&tlun, addr, sizeof (struct ltchars)))
			u.u_error = EFAULT;
		break;

	/*
	 * Return number of characters immediately available.
	 */
	case FIONREAD: {
		off_t nread;

		switch (tp->t_line) {

#if	NBK > 0
		case NETLDISC:
			nread = tp->t_rec ? tp->t_inbuf : 0;
			break;
#endif

#ifdef	OLDTTY
		case OTTYDISC:
			(void) _spl5();
			while (canon(tp)>=0)
				;
			(void) _spl0();
			/* fall into ... */
#endif

		case NTTYDISC:
#ifdef  UCB_NET
			nread = ttnread(tp);
#else
			nread = tp->t_canq.c_cc;
			if (tp->t_flags & (RAW|CBREAK))
				nread += tp->t_rawq.c_cc;
#endif
			break;

		}
		if (copyout((caddr_t)&nread, addr, sizeof (off_t)))
			u.u_error = EFAULT;
		break;
		}

	/*
	 * Should allow SPGRP and GPGRP only if tty open for reading.
	 */
	case TIOCSPGRP:
		if (copyin(addr, (caddr_t)&tp->t_pgrp, sizeof (tp->t_pgrp)))
			u.u_error = EFAULT;
		break;

	case TIOCGPGRP:
		if (copyout((caddr_t)&tp->t_pgrp, addr, sizeof(tp->t_pgrp)))
			u.u_error = EFAULT;
		break;

	/*
	 * Modify local mode word.
	 */
	case TIOCLBIS:
		if (copyin(addr, (caddr_t)&temp, sizeof (tp->t_local)))
			u.u_error = EFAULT;
		else
			tp->t_local |= temp;
		break;

	case TIOCLBIC:
		if (copyin(addr, (caddr_t)&temp, sizeof (tp->t_local)))
			u.u_error = EFAULT;
		else
			tp->t_local &= ~temp;
		break;

	case TIOCLSET:
		if (copyin(addr, (caddr_t)&temp, sizeof (tp->t_local)))
			u.u_error = EFAULT;
		else
			tp->t_local = temp;
		break;

	case TIOCLGET:
		if (copyout((caddr_t)&tp->t_local, addr, sizeof(tp->t_local)))
			u.u_error = EFAULT;
		break;

	/*
	 * Return number of characters in
	 * the output.
	 */
	case TIOCOUTQ:
		if (copyout((caddr_t)&tp->t_outq.c_cc, addr, sizeof(tp->t_outq.c_cc)))
			u.u_error = EFAULT;
		break;

	/*
	 * Simulate typing of a character at the terminal.
	 */
	case TIOCSTI:
		c = fubyte(addr);
		if (u.u_uid && u.u_ttyp != tp || c < 0)
			u.u_error = EFAULT;
		else
			(*linesw[tp->t_line].l_input)(c, tp);
		break;
#endif

#ifdef	TEXAS_AUTOBAUD
	case TIOCSIMG:
		tp->t_xflags |= LIMAGE;
		break;

	case TIOCCIMG:
		tp->t_xflags &= ~LIMAGE;
		break;
#endif

	default:
		return(com);
	}
	return(0);
}

#ifdef  UCB_NET
ttnread(tp)
	struct tty *tp;
{
	int nread = 0;

	if (tp->t_local & LPENDIN)
		ttypend(tp);
	nread = tp->t_canq.c_cc;
	if (tp->t_flags & (RAW|CBREAK))
		nread += tp->t_rawq.c_cc;
	return (nread);
}
#endif

#ifdef  UCB_NET
ttselect(dev, rw)
	dev_t dev;
	int rw;
{
	register struct tty *tp = &cdevsw[major(dev)].d_ttys[minor(dev)];
	int nread;
	int s = spl5();

	switch (rw) {

	case FREAD:
		nread = ttnread(tp);
		if (nread > 0)
			goto win;
		if (tp->t_rsel && tp->t_rsel->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_RCOLL;
		else
			tp->t_rsel = u.u_procp;
		break;

	case FWRITE:
		if (tp->t_outq.c_cc <= TTLOWAT(tp))
			goto win;
		if (tp->t_wsel && tp->t_wsel->p_wchan == (caddr_t)&selwait)
			tp->t_state |= TS_WCOLL;
		else
			tp->t_wsel = u.u_procp;
		break;
	}
	splx(s);
	return (0);
win:
	splx(s);
	return (1);
}
#endif
