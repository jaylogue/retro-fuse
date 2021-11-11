/*
 * Cary 118c Spectrophotometer Driver
 *
 * WFJ 19-AUG-78
 * wfj 8 mar 80 (rewritten for version 7)
 */

/*
 *	SCCS id	@(#)cary.c	2.1 (Berkeley)	8/5/83
 */

/* #define debug */

#include "param.h"
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>

#define PRI (PZERO+1)			/*user's wait priority */
#define NELEMENTS	32		/* depth of internal buffer q */

/* states */
#define OPEN	1
#define DATAQ	2
#define DATAZ	4
#define OVERRUN 010

/* device bits */
#define DATAINTS 1
#define SWINTS	 2
#define LITE	 4
#define MODE_MSK 0400
#define TENK_MSK 0200
#define START_MSK 01000
#define OVERANGE 02000
#define READY	 04000
#define SIGN_MSK 0100000

#define DEFAULT 200L		/* default sample freq. */

struct cary118
{
	int	state;
	union {
		long	freq;
		int	w[2];
	}	frun;
	long	count;
	int	head,tail;	/* ring buffer pointers */
	int	buffer[NELEMENTS];
#ifdef debug
	unsigned notready;
	unsigned ovflow;
#endif
}	cary;

struct device
{
	int	data;		/* read-only */
	int	csr;		/* no read-modify-write instructions (rmw)*/
};

struct device *CARYADDR = ((struct device*)0160040);
/* note: the carlock board has data & switch interrupts at 270 & 274 */

caryattach(addr, unit)
struct device *addr;
{
	if (unit != 0)
		return(0);
	CARYADDR = addr;
	return(1);
}

caryopen()
{
	if ((CARYADDR == (struct device *) NULL) || cary.state)
	{
		u.u_error = ENXIO;
		return;
	}
	cary.state = OPEN;
	CARYADDR->csr = SWINTS;
	cary.count = 0L;
	cary.frun.freq = DEFAULT;
	cary.head = cary.tail = 0;
}

caryclose()
{
	CARYADDR->csr = 0;
	cary.state = 0;
}

caryioctl(dev,cmd,addr,flag)
dev_t	dev;
int	cmd;
register caddr_t addr;
int	flag;
{
	register x,y;

	switch(cmd)
	{

	/* get status */
	case ('c'<<8)+0:
		suword(addr, CARYADDR->csr);
		addr += 2;
		suword(addr,cary.frun.w[0]);
		addr += 2;
		suword(addr,cary.frun.w[1]);
		addr += 2;
		suword(addr,cary.state);
		return;

	/* set sample frequency/rate */
	case ('c'<<8)+1:
		addr += 2;
		x = fuword(addr);
		addr += 2;
		y = fuword(addr);
		if (x == -1 || y == -1)
		{
			u.u_error = EFAULT;
			return;
		}
		cary.frun.w[0] = x; cary.frun.w[1] = y;
		return;

	default:
		u.u_error = ENOTTY;
		return;
	}
}

caryread()
{
	union
	{
		int w;
		char b[2];
	}	w;

	do
	{
		while (cary.head == cary.tail)
		{
			if ((cary.state & OPEN) == 0)
				return;
			sleep((caddr_t)&cary, PRI);
		}
		w.w = cary.buffer[cary.tail];
		cary.tail = ++cary.tail % NELEMENTS;
	} while ( (passc(w.b[0])>=0) && (passc(w.b[1])>=0));

}

caryswint()
{
	register m;

	if(!cary.state)
	{
		CARYADDR->csr = 0;
		return;
	}
	m = CARYADDR->csr & START_MSK;
	if( m && (cary.state & DATAQ) == 0)
	{	/* allow data accquisition (interrupts) */
		cary.state |= DATAQ;
		CARYADDR->csr = DATAINTS | SWINTS | LITE;
		return;
	}
	if( m == 0 && (cary.state & DATAQ))
	{	/* disable accquisition */
		cary.state &= ~OPEN;
		CARYADDR->csr = 0;
		wakeup((caddr_t)&cary);
		return;
	}
}

carydataint()
{
	register int data, val, x;
	if(--cary.count <= 0)
	{ /* take data */
		if( (cary.state & OPEN) == 0)
		{
			CARYADDR->csr = 0;
			return;
		}
		cary.count = cary.frun.freq;
		data = CARYADDR->data;
		val = 0;
		/*
		 * Convert bcd to binary
		 */

		for(x = 12;x >= 0; x -= 4)
			val = 10 * val + (017 & (data >> x));
		data = CARYADDR->csr;
#ifdef debug
		if (!data&READY)
			cary.notready++;
#endif
		if (data & TENK_MSK)
			val += 10000;
		if(!(data & SIGN_MSK))val = -val;
		cary.buffer[cary.head] = val;
		cary.head = ++cary.head % NELEMENTS;
		if (cary.head == cary.tail) {
			printf("Ring buffer overflow!\n");
			cary.state |= OVERRUN;
#ifdef debug
			cary.ovflow++;
#endif
		}

		/*
		 * This blinks the indicator lamp at the data rate.
		 * (its this complicated cause csr can't do rmw cycles.
		 *  thus no csr ^= LITE...)
		 */

		if (cary.state & DATAZ)
			CARYADDR->csr = SWINTS|DATAINTS|LITE;
		else
			CARYADDR->csr = SWINTS|DATAINTS;
		cary.state ^= DATAZ;
		wakeup((caddr_t)&cary);
	}
}
