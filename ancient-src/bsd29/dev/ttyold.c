/*
 *	SCCS id	@(#)ttyold.c	2.1 (Berkeley)	8/5/83
 */

/*
 * general TTY subroutines
 */
#include "param.h"
#ifdef	OLDTTY
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

extern	char	partab[];
char	canonb[CANBSIZ];	/* buffer for erase and kill (#@) */

extern	char	maptab[];	/* see tty.c */

#define	OBUFSIZ	100


/*
 * transfer raw input list to canonical list,
 * doing erase-kill processing and handling escapes.
 * It waits until a full line has been typed in cooked mode,
 * or until any character has been typed in raw mode.
 */
canon(tp)
register struct tty *tp;
{
	register char *bp;
	char *bp1;
	register int c;
	int mc;
	int s;

	if ((tp->t_flags&(RAW|CBREAK))==0 && tp->t_delct==0
	    || (tp->t_flags&(RAW|CBREAK))!=0 && tp->t_rawq.c_cc==0) {
		return(-1);
	}
	s = spl0();
loop:
	bp = &canonb[2];
	while ((c=getc(&tp->t_rawq)) >= 0) {
		if ((tp->t_flags&(RAW|CBREAK))==0) {
			if (c==0377) {
				tp->t_delct--;
				break;
			}
			if (bp[-1]!='\\') {
				if (c==tp->t_erase) {
					if (bp > &canonb[2])
						bp--;
					continue;
				}
				if (c==tp->t_kill)
					goto loop;
				if (c==tun.t_eofc)
					continue;
			} else {
				if (c==tp->t_erase || c==tp->t_kill)
					mc = c;
				else
					mc = maptab[c];
				if (mc && (mc==c || (tp->t_flags&LCASE))) {
					if (bp[-2] != '\\')
						c = mc;
					bp--;
				}
			}
		}
		*bp++ = c;
		if (bp>=canonb+CANBSIZ)
			break;
	}
	bp1 = &canonb[2];
	(void) b_to_q(bp1, bp-bp1, &tp->t_canq);

  
 	/* Unblock output iff:
 	 * is blocked
 	 * AND (input < threshold OR (cooked mode AND delim_count == 0))
 	 * This makes tandem mode usable for line-mode input.
 	 */
	if (tp->t_state&TBLOCK && ((tp->t_rawq.c_cc < TTYHOG/5) ||
	   (tp->t_delct==0 && ((tp->t_flags&(CBREAK|RAW)) == 0)))) {
		if (putc(tun.t_startc, &tp->t_outq)==0) {
			tp->t_state &= ~TBLOCK;
			ttstart(tp);
		}
	}

	splx(s);
	return(0);
}

/*
 * Place a character on raw TTY input queue, putting in delimiters
 * and waking up top half as needed.
 * Also echo if required.
 * The arguments are the character and the appropriate
 * tty structure.
 */
ttyinput(c, tp)
register c;
register struct tty *tp;
{
	register int t_flags;
#ifdef	MPX_FILS
	struct chan *cp;
#endif

	tk_nin += 1;
	c &= 0377;
	t_flags = tp->t_flags;
	if (t_flags&TANDEM)
		ttyblock(tp);
	if ((t_flags&RAW)==0) {
		c &= 0177;
		if (tp->t_state&TTSTOP) {
			if (c==tun.t_startc) {
				tp->t_state &= ~TTSTOP;
				ttstart(tp);
				return;
			}
			if (c==tun.t_stopc)
				return;
			tp->t_state &= ~TTSTOP;
			ttstart(tp);
		} else {
			if (c==tun.t_stopc) {
				tp->t_state |= TTSTOP;
				(*cdevsw[major(tp->t_dev)].d_stop)(tp,0);
				return;
			}
			if (c==tun.t_startc)
				return;
		}
		if (c==tun.t_quitc || c==tun.t_intrc) {
			if ((tp->t_local & LNOFLSH) == 0)
				flushtty(tp, FREAD|FWRITE);
			c = (c==tun.t_intrc) ? SIGINT:SIGQUIT;
#ifdef	MPX_FILS
			if (tp->t_chan)
				scontrol(tp->t_chan, M_SIG, c);
			else
#endif
				gsignal(tp->t_pgrp, c);
			return;
		}
		if (c=='\r' && t_flags&CRMOD)
			c = '\n';
	}
	if (tp->t_rawq.c_cc>TTYHOG) {
		flushtty(tp, FREAD|FWRITE);
		return;
	}
	if (t_flags&LCASE && c>='A' && c<='Z')
		c += 'a'-'A';
	(void) putc(c, &tp->t_rawq);
	if (t_flags&(RAW|CBREAK)||(c=='\n'||c==tun.t_eofc||c==tun.t_brkc)) {
		if ((t_flags&(RAW|CBREAK))==0 && putc(0377, &tp->t_rawq)==0)
			tp->t_delct++;
#ifdef	MPX_FILS
		if ((cp=tp->t_chan)!=NULL)
			(void) sdata(cp);
		else
#endif
			wakeup((caddr_t)&tp->t_rawq);
	}
	if (t_flags&ECHO) {
		ttyoutput(c, tp);
		if (c==tp->t_kill && (t_flags&(RAW|CBREAK))==0)
			ttyoutput('\n', tp);
		ttstart(tp);
	}
}

/*
 * put character on TTY output queue, adding delays,
 * expanding tabs, and handling the CR/NL bit.
 * It is called both from the top half for output, and from
 * interrupt level for echoing.
 * The arguments are the character and the tty structure.
 */
ttyoutput(c, tp)
register c;
register struct tty *tp;
{
	register char *colp;
	int ctype;

	/*
	 * Ignore EOT in normal mode to avoid hanging up
	 * certain terminals.
	 * In raw mode dump the char unchanged.
	 */
	if ((tp->t_flags&RAW)==0) {
		c &= 0177;
		if ((tp->t_flags&CBREAK)==0 && c==CEOT)
			return;
	} else {
		tk_nout++;
		(void) putc(c, &tp->t_outq);
		return;
	}

	/*
	 * Turn tabs to spaces as required
	 */
	if (c=='\t' && (tp->t_flags&TBDELAY)==XTABS) {
		c = 8 - (tp->t_col & 7);
		(void) b_to_q("        ", c, &tp->t_outq);
		tp->t_col += c;
		tk_nout += c;
		return;
	}
	tk_nout++;
	/*
	 * for upper-case-only terminals,
	 * generate escapes.
	 */
	if (tp->t_flags&LCASE) {
		colp = "({)}!|^~'`";
		while(*colp++)
			if(c == *colp++) {
				ttyoutput('\\', tp);
				c = colp[-2];
				break;
			}
		if ('a'<=c && c<='z')
			c += 'A' - 'a';
	}
	/*
	 * turn <nl> to <cr><lf> if desired.
	 */
	if (c=='\n' && tp->t_flags&CRMOD)
		ttyoutput('\r', tp);
	(void) putc(c, &tp->t_outq);
	/*
	 * Calculate delays.
	 * The numbers here represent clock ticks
	 * and are not necessarily optimal for all terminals.
	 * The delays are indicated by characters above 0200.
	 * In raw mode there are no delays and the
	 * transmission path is 8 bits wide.
	 */
	colp = &tp->t_col;
	ctype = partab[c];
	c = 0;
	switch (ctype&077) {

	case ORDINARY:
		(*colp)++;

	case CONTROL:
		break;

	case BACKSPACE:
		if (*colp)
			(*colp)--;
		break;

	case NEWLINE:
		ctype = (tp->t_flags >> 8) & 03;
		if(ctype == 1) { /* tty 37 */
			if (*colp)
				c = MAX(((unsigned)*colp>>4) + 3, (unsigned)6);
		} else
		if(ctype == 2) { /* vt05 */
			c = 6;
		}
		*colp = 0;
		break;

	case TAB:
		ctype = (tp->t_flags >> 10) & 03;
		if(ctype == 1) { /* tty 37 */
			c = 1 - (*colp | ~07);
			if(c < 5)
				c = 0;
		}
		*colp |= 07;
		(*colp)++;
		break;

	case VTAB:
		if(tp->t_flags & VTDELAY) /* tty 37 */
			c = 0177;
		break;

	case RETURN:
		ctype = (tp->t_flags >> 12) & 03;
		if(ctype == 1) { /* tn 300 */
			c = 5;
		} else if(ctype == 2) { /* ti 700 */
			c = 10;
		} else if(ctype == 3) { /* concept 100 */
			int i;
			if ((i = *colp) >= 0)
				for (; i<9; i++)
					(void) putc(0177, &tp->t_outq);
		}
		*colp = 0;
	}
	if(c)
		(void) putc(c|0200, &tp->t_outq);
}

/*
 * Called from device's read routine after it has
 * calculated the tty-structure given as argument.
 */
ttread(tp)
register struct tty *tp;
{
register s;

	if ((tp->t_state&CARR_ON)==0)
		return(-1);
	s = spl5();
	if (tp->t_canq.c_cc==0) {
		while ((canon(tp)<0) && (tp->t_state&CARR_ON)) {
			if ((tp->t_state&CARR_ON)==0 ||
#ifdef	MPX_FILS
			    (tp->t_chan!=NULL) ||
#endif
#ifdef	UCB_NET
			    (tp->t_state&TS_NBIO)
#endif
			    ) {
				splx(s);
				return(0);
			}
			sleep((caddr_t)&tp->t_rawq, TTIPRI); 
		}
	}
	splx(s);
	while (tp->t_canq.c_cc && passc(getc(&tp->t_canq))>=0)
			;
	return(tp->t_rawq.c_cc+tp->t_canq.c_cc);
}

/*
 * Called from the device's write routine after it has
 * calculated the tty-structure given as argument.
 */
caddr_t
ttwrite(tp)
register struct tty *tp;
{
	register char *cp;
	register int ce;
	int cc, i;
	char obuf[OBUFSIZ];
	int hiwat = TTHIWAT(tp);

	if ((tp->t_state&CARR_ON)==0)
		return(NULL);
	while (u.u_count) {
		cc = MIN(u.u_count, OBUFSIZ);
		cp = obuf;
		iomove(cp, (unsigned)cc, B_WRITE);
		if (u.u_error)
			break;
		(void) _spl5();
		while (tp->t_outq.c_cc > hiwat) {
			ttstart(tp);
#ifdef	UCB_NET
			if (tp->t_state&TS_NBIO) {
				u.u_error = EWOULDBLOCK;
				return(NULL);
			}
#endif
			tp->t_state |= ASLEEP;
#ifdef	MPX_FILS
			if (tp->t_chan) {
				u.u_base -= cc;
				u.u_offset -= cc;
				u.u_count += cc;
				(void) _spl0();
				return((caddr_t)&tp->t_outq);
			}
#endif
			sleep((caddr_t)&tp->t_outq, TTOPRI);
		}
		(void) _spl0();
		if (tp->t_flags&LCASE) {
			while (cc--)
				ttyoutput(*cp++,tp);
			continue;
		}
		while (cc) {
			if (tp->t_flags&RAW)
				ce=cc;
			else {
				ce=0;
				while(((partab[(*(cp+ce))&0177]&077)==0)&&(ce<cc))
					ce++;
				if (ce==0) {
					ttyoutput(*cp++,tp);
					cc--;
					goto check;
				}
			}
			i=b_to_q(cp,ce,&tp->t_outq);
			ce-=i;
			tk_nout+=ce;
			tp->t_col+=ce;
			cp+=ce;
			cc-=ce;
check:
			if (tp->t_outq.c_cc > hiwat) {
				(void) _spl5();
				while (tp->t_outq.c_cc > hiwat) {
					ttstart(tp);
#ifdef	UCB_NET
					if (tp->t_state&TS_NBIO) {
						u.u_error = EWOULDBLOCK;
						return(NULL);
					}
#endif
					tp->t_state |= ASLEEP;
					sleep((caddr_t)&tp->t_outq, TTOPRI);
				}
				(void) _spl0();
			}
		}
	}
	ttstart(tp);
	return(NULL);
}

#endif	OLDTTY
