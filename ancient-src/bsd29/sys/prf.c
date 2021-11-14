/*
 *	SCCS id	@(#)prf.c	2.1 (Berkeley)	8/5/83
 */

#include "bsd29/include/sys/aram.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/filsys.h>
#include <bsd29/include/sys/mount.h>
/* UNUSED #include <sys/seg.h> */
#include <bsd29/include/sys/buf.h>
#include <bsd29/include/sys/conf.h>
#include <bsd29/include/sys/inline.h>
#ifdef	UCB_AUTOBOOT
#include <sys/reboot.h>
#endif

/*
 * In case console is off,
 * panicstr contains argument to last
 * call to panic.
 */

char	*panicstr;

/*
 * Scaled down version of C Library printf.
 * Only %c %s %u %d (==%u) %o %x %D %O are recognized.
 * Used to print diagnostic information directly on console tty.
 * Since it is not interrupt driven, all system activities are
 * suspended.  Printf should not be used for chit-chat.
 *
 */
#ifdef	UCB_DEVERR
/*
 * One additional format: %b is supported to decode error registers.
 * Usage is:
 *	printf("reg=%b\n", regval, "<base><arg>*");
 * Where <base> is the output base expressed as a control character,
 * e.g. \10 gives octal; \20 gives hex.  Each arg is a sequence of
 * characters, the first of which gives the bit number to be inspected
 * (origin 1), and the next characters (up to a control character, i.e.
 * a character <= 32), give the name of the register.  Thus
 *	printf("reg=%b\n", 3, "\10\2BITTWO\1BITONE\n");
 * would produce output:
 *	reg=3<BITTWO,BITONE>
 */
#endif	UCB_DEVERR

/*VARARGS1*/
printf(fmt, x1)
register char *fmt;
unsigned x1;
{
	prf(fmt, &x1, 0);
}

#ifdef	UCB_UPRINTF
/*
 *	Uprintf prints to the current user's terminal,
 *	guarantees not to sleep (so could be called by interrupt routines;
 *	but prints on the tty of the current process)
 *	and does no watermark checking - (so no verbose messages).
 *	NOTE: with current kernel mapping scheme, the user structure is
 *	not guaranteed to be accessible at interrupt level (see seg.h);
 *	a savemap/restormap would be needed here or in putchar if uprintf
 *	was to be used at interrupt time.
 */
/*VARARGS1*/
uprintf(fmt, x1)
char	*fmt;
unsigned x1;
{
	prf(fmt, &x1, 2);
}
#else

/*VARARGS1*/
uprintf(fmt, x1)
char *fmt;
unsigned x1;
{
	prf(fmt, &x1, 0);
}

#endif	UCB_PRINTF

prf(fmt, adx, touser)
register char *fmt;
register unsigned int *adx;
{
	register c;
	char *s;
#ifdef	UCB_DEVERR
	int	i, any;
	unsigned b;
#endif

loop:
	while((c = *fmt++) != '%') {
		if(c == '\0')
			return;
		putchar(c, touser);
	}
	c = *fmt++;
	if(c == 'd' || c == 'u' || c == 'o' || c == 'x')
		printn((long)*adx, c=='o'? 8: (c=='x'? 16:10), touser);
	else if(c == 'c')
		putchar(*adx, touser);
	else if(c == 's') {
		s = (char *)*adx;
		while(c = *s++)
			putchar(c, touser);
	} else if (c == 'D' || c == 'O' || c == 'X') {
		printn(*(long *)adx, c=='D'?10:c=='O'?8:16, touser);
		adx += (sizeof(long) / sizeof(int)) - 1;
	}
#ifdef	UCB_DEVERR
	else if (c == 'b') {
		b = *adx++;
		s = (char *) *adx;
		printn((long) b, *s++, touser);
		any = 0;
		if (b) {
			putchar('<', touser);
			while (i = *s++) {
				if (b & (1 << (i - 1))) {
					if (any)
						putchar(',', touser);
					any = 1;
					for (; (c = *s) > 32; s++)
						putchar(c, touser);
				} else
					for (; *s > 32; s++)
						;
			}
			putchar('>', touser);
		}
	}
#endif
	adx++;
	goto loop;
}

/*
 * Print an unsigned integer in base b, avoiding recursion.
 */
printn(n, b, touser)
long n;
{
	long a;
	char prbuf[12];
	register char *cp;

	if (b == 10 && n < 0) {
		putchar('-', touser);
		n = -n;
	}
	cp = prbuf;
	do {
		*cp++ = "0123456789ABCDEF"[(int)(n%b)];
	} while (n = n/b);	/* Avoid  n /= b, since that requires alrem */
	do
		putchar(*--cp, touser);
	while (cp > prbuf);
}

/*
 * Panic is called on fatal errors.
 * It prints "panic: mesg", syncs, and then reboots if possible.
 * If we are called twice, then we avoid trying to
 * sync the disks as this often leads to recursive panics.
 */
panic(s)
register char *s;
{
#ifdef	UCB_AUTOBOOT
	int	bootopt = (panicstr == NULL) ?  RB_DUMP : (RB_DUMP | RB_NOSYNC);

	if (panicstr == NULL)
		panicstr = s;
	printf("panic: %s\n", s);
	(void) _spl0();
	boot(rootdev,bootopt);
	/*NOTREACHED*/
#else
	printf("panic: %s\n", s);
	if (panicstr == NULL) {
		panicstr = s;
		if (bfreelist.b_forw)
			update();
	}
	for(;;)
		idle();
#endif
}

/*
 * Warn that a system table is full.
 */
tablefull(tab)
char	*tab;
{
	printf("%s: table is full\n", tab);
}

/*
 * Hard error is the preface to plaintive error messages
 * about failing disk tranfers.
 */
harderr(bp, cp)
struct buf *bp;
char *cp;
{
	printf("%s%d%c: hard error bn %D ", cp,
	     dkunit(bp), 'a' + (minor(bp->b_dev) & 07), bp->b_blkno);
}

/*
 * Fserr prints the name of a file system
 * with an error diagnostic, in the form
 *	filsys:  error message
 */
fserr(fp, cp)
struct	filsys *fp;
char	*cp;
{
	printf("%s: %s\n", fp->s_fsmnt, cp);
}

#ifndef	UCB_DEVERR
/*
 * deverr prints a diagnostic from
 * a device driver.
 * It prints the device, block number,
 * and 2 octal words (usually some error
 * status registers) passed as argument.
 */
deverror(bp, o1, o2)
register struct buf *bp;
{
	register struct mount *mp;
	register struct filsys *fp;
	
	for(mp = mount; mp < mountNMOUNT; mp++)
		if(mp->m_inodp != NULL)
			if(bp->b_dev == mp->m_dev) {
				fp = &mp->m_filsys;
				fserr(fp, "err");
			}
	printf("err on dev %u/%u\n", major(bp->b_dev), minor(bp->b_dev));
	printf("bn=%D er=%o,%o\n", bp->b_blkno, o1, o2);
}
#endif	UCB_DEVERR
