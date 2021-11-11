/*
 *	SCCS id	@(#)vp.c	2.1 (Berkeley)	8/5/83
 */

/*
 * Versatec printer/plotter driver for model c-pdp11(dma) controller
 *
 * Thomas Ferrin, UCSF Computer Graphics Laboratory, April 1979
 *
 * Uses the same user interface as Bell Labs' driver but runs in raw
 * mode, since even the small 11" versatec outputs ~53kbytes/inch!
 */

/* #define NOMESG	/* no console messages if defined */
/* #define VP_TWOSCOMPL	/* use two's-complement bytecount */

#include "vp.h"
#if	NVP > 0
#include "param.h"
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/vcmd.h>

#define	PRINTADR ((struct devregs *)0177514)	/* printer csr */
#define	PLOTADR  ((struct devregs *)0177510)	/* plotter csr */
/*
 * Note that there is really only 1 csr!  Changes to 'PRINTADR'
 * csr are reflected in 'PLOTADR' csr and vice versa. Referencing
 * the particular csr sets the device into that mode of operation.
 */
#define	DMA	((struct dmaregs *)0177500)	/* dma register base */

#define ERROR	0100000
#define DTCENABLE 040000	/* interrupt on data xfer complete */
#define	IENABLE	0100		/* interrupt on ready or error */
#define	READY	0200		/* previous operation complete */
#define REOT	010		/* remote end-of-transmission */
#define RESET	02
#define CMDS	076
#define SPP	01		/* simultaneous print/plot mode */

struct	buf	vpbuf;
#ifndef NOMESG
char *vperr = "\nVersatec needs attention\n";
#endif

struct devregs {
	short	csr;
};

struct dmaregs {
	short	plotbytes;
	short	addrext;
	short	printbytes;
	unsigned short	addr;
};

struct  {
	int	state;
	unsigned bytecount;
	int	alive;
} vp11;

/* states */
#define	PPLOT	0400	/* low 6 bits reserved for hardware functions */
#define	PLOT	0200
#define	PRINT	0100
#define MODE	0700
#define	ERRMSG	01000
#define NOSTART	02000
#define WBUSY	04000

/*
 *  This driver should be converted to use a single VPADDR pointer.
 *  Until then, the attach only marks the unit as alive.
 */
vpattach(addr, unit)
struct vpdevice *addr;
{
	if (((unsigned) unit == 0) || (addr == DMA)) {
		vp11.alive++;
		return 1;
	}
	return 0;
}

/* ARGSUSED */
vpopen(dev, flag)
dev_t dev;
{
	register short *vpp;

	if (!vp11.alive || vp11.state&MODE) {
		u.u_error = ENXIO;
		return;
	}
	vpp = &PRINTADR->csr;
	vp11.state = PRINT;
	vpbuf.b_flags = B_DONE;
	*vpp = IENABLE | RESET;
	/*
	 * Contrary to the implication in the operator's manual, it is an
	 * error to have the bits for both an operation complete interrupt
	 * and a data transmission complete interrupt set at the same time.
	 * Doing so will cause erratic operation of 'READY' interrupts.
	 */
	if (vperror())
		*vpp = vp11.state = 0;
}

/* ARGSUSED */
vpclose(dev, flag)
dev_t dev;
{
	register short *vpp;

	vperror();
	vpp = (vp11.state&PLOT)? &PLOTADR->csr : &PRINTADR->csr;
	*vpp = IENABLE | REOT;	/* page eject past toner bath */
	vperror();
	*vpp = vp11.state = 0;
}

vpwrite(dev)
dev_t dev;
{
	int vpstart();

	if ((vp11.bytecount=u.u_count) == 0)	/* true byte count */
		return;
	if (u.u_count&01)	/* avoid EFAULT error in physio() */
		u.u_count++;
	if (vperror())
		return;
	physio(vpstart, &vpbuf, dev, B_WRITE);
	/* note that newer 1200A's print @ 1000 lines/min = 2.2kbytes/sec */
}

#ifdef V6
vpctl(dev, v)	
dev_t dev;
int *v;
{
	register int ctrl;
	register short *vpp;

	if (v) {
		*v = vp11.state;
		return;
	}
	if (vperror())
		return;
	ctrl = u.u_arg[0];
	vp11.state = (vp11.state&~MODE) | (ctrl&MODE);
	vpp = (ctrl&PLOT)? &PLOTADR->csr : &PRINTADR->csr;
	if (ctrl&CMDS) {
		*vpp = IENABLE | (ctrl&CMDS) | (vp11.state&SPP);
		vperror();
	}
	if (ctrl&PPLOT) {
		vp11.state |= SPP;
		*vpp = DTCENABLE | SPP;
	} else if (ctrl&PRINT) {
		vp11.state &= ~SPP;
		*vpp = IENABLE;
	} else
		*vpp = IENABLE | (vp11.state&SPP);
}
#else

/* ARGSUSED */
vpioctl(dev, cmd, addr, flag)	
dev_t dev;
caddr_t addr;
{
	register int ctrl;
	register short *vpp;
	register struct buf *bp;

	switch (cmd) {

	case GETSTATE:
		suword(addr, vp11.state);
		return;

	case SETSTATE:
		ctrl = fuword(addr);
		if (ctrl == -1) {
			u.u_error = EFAULT;
			return;
		}
		if (vperror())
			return;
		vp11.state = (vp11.state&~MODE) | (ctrl&MODE);
		vpp = (ctrl&PLOT)? &PLOTADR->csr : &PRINTADR->csr;
		if (ctrl&CMDS) {
			*vpp = IENABLE | (ctrl&CMDS) | (vp11.state&SPP);
			vperror();
		}
		if (ctrl&PPLOT) {
			vp11.state |= SPP;
			*vpp = DTCENABLE | SPP;
		} else if (ctrl&PRINT) {
			vp11.state &= ~SPP;
			*vpp = IENABLE;
		} else
			*vpp = IENABLE | (vp11.state&SPP);
		return;

	case BUFWRITE:
		u.u_base = fuword(addr);
		u.u_count = fuword(addr+NBPW);
		if ((int)u.u_base == -1 || (int)u.u_count == -1) {
			u.u_error = EFAULT;
			return;
		}
		bp = &vpbuf;
		if (vp11.state&WBUSY) {
			/* wait for previous write */
			(void) _spl4();
			while ((bp->b_flags&B_DONE) == 0)
				sleep(bp, PRIBIO);
			u.u_procp->p_flag &= ~SLOCK;
			bp->b_flags &= ~B_BUSY;
			vp11.state &= ~WBUSY;
			(void) _spl0();
			geterror(bp);
			if (u.u_error)
				return;
		}
		if (u.u_count == 0)
			return;
		/* simulate a write without starting i/o */
		(void) _spl4();
		vp11.state |= NOSTART;
		vpwrite(dev);
		vp11.state &= ~NOSTART;
		if (u.u_error) {
			(void) _spl0();
			return;
		}
		/* fake write was ok, now do it for real */
		bp->b_flags = B_BUSY | B_PHYS | B_WRITE;
		u.u_procp->p_flag |= SLOCK;
		vp11.state |= WBUSY;
		vpstart(bp);
		(void) _spl0();
		return;

	default:
		u.u_error = ENOTTY;
	}
}
#endif

vperror()
{
	register int state, err;

	state = vp11.state & PLOT;
	(void) _spl4();
	while ((err=(state? PLOTADR->csr : PRINTADR->csr)&(READY|ERROR))==0)
		sleep((caddr_t)&vp11, PRIBIO);
	(void) _spl0();
	if (err&ERROR) {
		u.u_error = EIO;
#ifndef NOMESG
		if (!(vp11.state&ERRMSG)) {
			printf(vperr);
			vp11.state |= ERRMSG;
		}
#endif
	} else
		vp11.state &= ~ERRMSG;
	return(err&ERROR);
}

vpstart(bp)
register struct buf *bp;
{
	register struct dmaregs *vpp = DMA;

	if (vp11.state&NOSTART) {
		bp->b_flags |= B_DONE;	/* fake it */
		return;
	}
#ifdef	UNIBUS_MAP
	mapalloc(bp);
#endif
	vpp->addr = (short)(bp->b_un.b_addr);
	vpp->addrext = (bp->b_xmem & 03) << 4;
#ifndef	VP_TWOSCOMPL
	if (vp11.state&PLOT)
		vpp->plotbytes = vp11.bytecount;
	else
	 	vpp->printbytes = vp11.bytecount;
#else
	if (vp11.state&PLOT)
		vpp->plotbytes = -vp11.bytecount;
	else
	 	vpp->printbytes = -vp11.bytecount;
#endif
}

vpintr()
{
	register struct buf *bp = &vpbuf;

	/*
	 * Hardware botch.  Interrupts occur a few usec before the 'READY'
	 * bit actually sets.  On fast processors it is unreliable to test
	 * this bit in the interrupt service routine!
	 */
	if (bp->b_flags&B_DONE) {
		wakeup((caddr_t)&vp11);
		return;
	}
	if (((vp11.state&PLOT)? PLOTADR->csr : PRINTADR->csr) < 0) {
#ifndef NOMESG
		if (!(vp11.state&ERRMSG)) {
			printf(vperr);
			vp11.state |= ERRMSG;
		}
#endif
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
		wakeup((caddr_t)&vp11);
	}
	iodone(bp);
}
#endif	NVP
