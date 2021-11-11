/*
 *	SCCS id	@(#)text.c	2.1 (Berkeley)	8/29/83
 */

#include "param.h"
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/text.h>
#include <sys/inode.h>
#include <sys/buf.h>
#include <sys/seg.h>

/*
 * Swap out process p.
 * The ff flag causes its core to be freed--
 * it may be off when called to create an image for a
 * child process in newproc.
 *
 * panic: out of swap space
 */
#ifndef	VIRUS_VFORK
/*
 * Os is the old size of the data area of the process,
 * and is supplied during core expansion swaps.
 */
xswap(p, ff, os)
register struct proc *p;
#else
/*
 * Odata and ostack are the old data and stack sizes,
 * supplied during core expansion swaps.
 */
xswap(p,ff,odata,ostack)
register struct proc *p;
unsigned odata, ostack;
#endif
{

#ifdef	VIRUS_VFORK
	unsigned a[3];

	if (odata == X_OLDSIZE)
		odata = p->p_dsize;
	if (ostack == X_OLDSIZE)
		ostack = p->p_ssize;
	if (malloc3(swapmap,ctod(p->p_dsize),ctod(p->p_ssize),
	    ctod(USIZE),a) == NULL)
		panic("out of swap space");
	p->p_flag |= SLOCK;
	xccdec(p->p_textp);
	if (odata) {
		swap(a[0], p->p_daddr, odata, B_WRITE);
		if (ff == X_FREECORE)
			mfree(coremap, odata, p->p_daddr);
	}
	if (ostack) {
		swap(a[1], p->p_saddr, ostack, B_WRITE);
		if(ff == X_FREECORE)
			mfree(coremap, ostack, p->p_saddr);
	}
	swap(a[2], p->p_addr, USIZE, B_WRITE);
	if(ff == X_FREECORE)
		mfree(coremap, USIZE, p->p_addr);
	p->p_daddr = a[0];
	p->p_saddr = a[1];
	p->p_addr = a[2];
	p->p_flag &= ~(SLOAD|SLOCK);
	p->p_time = 0;
	if(runout) {
		runout = 0;
		wakeup((caddr_t)&runout);
	}

#else	VIRUS_VFORK
	register a;

	if(os == X_OLDSIZE)
		os = p->p_size;
	a = malloc(swapmap, ctod(p->p_size));
	if(a == NULL)
		panic("out of swap space");
	p->p_flag |= SLOCK;
	xccdec(p->p_textp);
	swap(a, p->p_addr, os, B_WRITE);
	if(ff == X_FREECORE)
		mfree(coremap, os, p->p_addr);
	p->p_addr = a;
	p->p_flag &= ~(SLOAD|SLOCK);
	p->p_time = 0;
	if(runout) {
		runout = 0;
		wakeup((caddr_t)&runout);
	}
#endif	VIRUS_VFORK
}

/*
 * relinquish use of the shared text segment
 * of a process.
 */
xfree()
{
	register struct text *xp;
	register struct inode *ip;
	register struct proc *p = u.u_procp;

	if((xp=p->p_textp) == NULL)
		return;
	xlock(xp);
	ip = xp->x_iptr;
	if(--xp->x_count == 0 && (ip->i_mode & ISVTX) == 0) {
		/*
		 * Only safe to mfree first if we are guaranteed
		 * not to swap with locked text.
		 */
		mfree(swapmap, ctod(xp->x_size), xp->x_daddr);
		mfree(coremap, xp->x_size, xp->x_caddr);
		ip->i_flag &= ~ITEXT;
		if (ip->i_flag & ILOCK)
			ip->i_count--;
		else
			iput(ip);
		xp->x_iptr = NULL;
		xunlock(xp);
	} else {
		xp->x_flag &= ~XLOCK;
		xccdec(xp);
	}
	p->p_textp = NULL;
}

/*
 * Attach to a shared text segment.
 * If there is no shared text, just return.
 * If there is, hook up to it:
 * if it is not currently being used, it has to be read
 * in from the inode (ip); the written bit is set to force it
 * to be written out as appropriate.
 * If it is being used, but is not currently in core,
 * a swap has to be done to get it back.
 */
xalloc(ip)
register struct inode *ip;
{
	register struct text *xp;
	register unsigned ts;
	struct text *xp1;

	if(u.u_exdata.ux_tsize == 0)
		return;
again:
	xp1 = NULL;
	for (xp = text; xp < textNTEXT; xp++) {
		if(xp->x_iptr == NULL) {
			if(xp1 == NULL)
				xp1 = xp;
			continue;
		}
		if(xp->x_iptr == ip) {
			if (xp->x_flag & XLOCK) {
				/*
				 *  Wait for text entry to be unlocked,
				 *  then start over in case this text was
				 *  xfree'ed.
				 */
				xlock(xp);
				xunlock(xp);
				goto again;
			}
			xlock(xp);
			xp->x_count++;
			u.u_procp->p_textp = xp;
			if (xp->x_ccount == 0)
				xexpand(xp);
			else
				xp->x_ccount++;
			xunlock(xp);
			return;
		}
	}
	if((xp=xp1) == NULL) {
		tablefull("text");
		psignal(u.u_procp, SIGKILL);
		return;
	}
	xp->x_flag = XLOAD|XLOCK;
	xp->x_count = 1;
	xp->x_ccount = 0;
	xp->x_iptr = ip;
	ip->i_flag |= ITEXT;
	ip->i_count++;
#ifdef MENLO_OVLY
	ts = btoc(u.u_exdata.ux_tsize);
	if (u.u_ovdata.uo_ovbase)
		xp->x_size = u.u_ovdata.uo_ov_offst[NOVL];
	else
		xp->x_size = ts;
	if((xp->x_daddr = malloc(swapmap, (int)ctod(xp->x_size))) == NULL)
#else
	ts = btoc(u.u_exdata.ux_tsize);
	xp->x_size = ts;
	if((xp->x_daddr = malloc(swapmap, (int)ctod(ts))) == NULL)
#endif
		panic("out of swap space");
	u.u_procp->p_textp = xp;
	xexpand(xp);
	estabur(ts, (unsigned)0, (unsigned)0, 0, RW);
	u.u_count = u.u_exdata.ux_tsize;
	u.u_offset = sizeof(u.u_exdata);
#ifdef MENLO_OVLY
	if (u.u_ovdata.uo_ovbase)
		u.u_offset += (1 + NOVL) * sizeof(unsigned);
#endif
	u.u_base = 0;
	u.u_segflg = 2;
	u.u_procp->p_flag |= SLOCK;
	readi(ip);
#ifdef MENLO_OVLY
	/* read in overlays if necessary */

	if (u.u_ovdata.uo_ovbase) {
		register i;
		for (i = 1; i < 1 + NOVL; i++) {
			u.u_ovdata.uo_curov = i;
			u.u_count = ctob(u.u_ovdata.uo_ov_offst[i]
				  - u.u_ovdata.uo_ov_offst[i-1]);
			u.u_base = ctob(stoc(u.u_ovdata.uo_ovbase));
			if( u.u_count != 0) {
				choverlay(RW);
				readi(ip);
			}
		}
	}
	u.u_ovdata.uo_curov = 0;
#endif
	u.u_procp->p_flag &= ~SLOCK;
	u.u_segflg = 0;
	xp->x_flag = XWRIT;
}

/*
 * Assure core for text segment
 * Text must be locked to keep someone else from
 * freeing it in the meantime.
 * x_ccount must be 0.
 */
xexpand(xp)
register struct text *xp;
{
	if ((xp->x_caddr = malloc(coremap, xp->x_size)) != NULL) {
		if ((xp->x_flag&XLOAD) == 0)
			swap(xp->x_daddr, xp->x_caddr, xp->x_size, B_READ);
		xp->x_ccount++;
		xunlock(xp);
		return;
	}
	if (save(u.u_ssav)) {
		sureg();
		return;
	}
#ifdef	VIRUS_VFORK
	(void) xswap(u.u_procp, X_FREECORE, X_OLDSIZE, X_OLDSIZE);
#else
	(void) xswap(u.u_procp, X_FREECORE, X_OLDSIZE);
#endif
	xunlock(xp);
	u.u_procp->p_flag |= SSWAP;
#ifdef	MENLO_JCL
	swtch();
#else
	qswtch();
#endif
	/* NOTREACHED */
}

/*
 * Lock and unlock a text segment from swapping
 */
xlock(xp)
register struct text *xp;
{

	while(xp->x_flag&XLOCK) {
		xp->x_flag |= XWANT;
		sleep((caddr_t)xp, PSWP);
	}
	xp->x_flag |= XLOCK;
}

xunlock(xp)
register struct text *xp;
{

	if (xp->x_flag&XWANT)
		wakeup((caddr_t)xp);
	xp->x_flag &= ~(XLOCK|XWANT);
}

/*
 * Decrement the in-core usage count of a shared text segment.
 * When it drops to zero, free the core space.
 */
xccdec(xp)
register struct text *xp;
{

	if (xp == NULL || xp->x_ccount == 0)
		return;
	xlock(xp);
	if (--xp->x_ccount == 0) {
		if (xp->x_flag&XWRIT) {
			swap(xp->x_daddr, xp->x_caddr, xp->x_size, B_WRITE);
			xp->x_flag &= ~XWRIT;
		}
		mfree(coremap, xp->x_size, xp->x_caddr);
	}
	xunlock(xp);
}

/*
 * Free the swap image of all unused saved-text text segments
 * which are from device dev (used by umount system call).
 * Free all unused text segments if dev == NODEV (from malloc).
 */
xumount(dev)
register dev_t dev;
{
	register struct text *xp;

	for (xp = text; xp < textNTEXT; xp++) 
		if (xp->x_iptr!=NULL && (dev==xp->x_iptr->i_dev || dev==NODEV))
			xuntext(xp);
}

/*
 * remove a shared text segment from the text table, if possible.
 */
xrele(ip)
register struct inode *ip;
{
	register struct text *xp;

	if ((ip->i_flag&ITEXT) == 0)
		return;
	for (xp = text; xp < textNTEXT; xp++) 
		if (ip == xp->x_iptr)
			xuntext(xp);
}

/*
 * remove text image from the text table.
 * the use count must be zero.
 */
xuntext(xp)
register struct text *xp;
{
	register struct inode *ip;

	xlock(xp);
	if (xp->x_count) {
		xunlock(xp);
		return;
	}
	ip = xp->x_iptr;
	xp->x_flag &= ~XLOCK;
	xp->x_iptr = NULL;
	mfree(swapmap, ctod(xp->x_size), xp->x_daddr);
	ip->i_flag &= ~ITEXT;
	if (ip->i_flag&ILOCK)
		ip->i_count--;
	else
		iput(ip);
}
