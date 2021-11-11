#include "param.h"
#include "dh.h"
#include "dz.h"
#include <sys/tty.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/seg.h>
#include <sys/map.h>
#include <sys/uba.h>

/*
 *	SCCS id	@(#)prim.c	2.1 (Berkeley)	8/5/83
 */


#ifdef UCB_CLIST
/*
 *  Modification to move clists out of kernel data space.
 *  Clist space is allocated by startup.
 */
memaddr clststrt;		/* Physical click address of clist */
extern unsigned clstdesc;	/* PDR for clist segment when mapped */
struct	cblock *cfree = SEG5;	/* Virt. Addr. of clists (0120000 - 0140000) */

#else UCB_CLIST
extern	struct cblock cfree[];
#endif	UCB_CLIST

struct	cblock	*cfreelist;
int	cbad;

/*
 * Character list get/put
 */
getc(p)
register struct clist *p;
{
	register struct cblock *bp;
	register int c, s;
#ifdef UCB_CLIST
	segm sav5;
#endif

	s = spl6();
#ifdef UCB_CLIST
	saveseg5(sav5);
	mapseg5(clststrt, clstdesc);
#endif
	if (p->c_cc <= 0) {
		c = -1;
		p->c_cc = 0;
		p->c_cf = p->c_cl = NULL;
	} else {
		c = *p->c_cf++ & 0377;
		if (--p->c_cc<=0) {
			bp = (struct cblock *)(p->c_cf-1);
			bp = (struct cblock *) ((int)bp & ~CROUND);
			p->c_cf = NULL;
			p->c_cl = NULL;
			bp->c_next = cfreelist;
			cfreelist = bp;
		} else
			if (((int)p->c_cf & CROUND) == 0){
				bp = (struct cblock *)(p->c_cf);
				bp--;
				p->c_cf = bp->c_next->c_info;
				bp->c_next = cfreelist;
				cfreelist = bp;
			}
	}
#ifdef UCB_CLIST
	restorseg5(sav5);
#endif
	splx(s);
	return(c);
}

#ifdef	MPX_FILS
/*
 * copy clist to buffer.
 * return number of bytes moved.
 */
q_to_b(q, cp, cc)
register struct clist *q;
register char *cp;
{
	register struct cblock *bp;
	register int s;
	char *acp;
#ifdef UCB_CLIST
	segm sav5;
#endif

	if (cc <= 0)
		return(0);
	s = spl6();
	if (q->c_cc <= 0) {
		q->c_cc = 0;
		q->c_cf = q->c_cl = NULL;
		splx(s);
		return(0);
	}
	acp = cp;
	cc++;

#ifdef UCB_CLIST
	saveseg5(sav5);
	mapseg5(clststrt, clstdesc);
#endif
	while (--cc) {
		*cp++ = *q->c_cf++;
		if (--q->c_cc <= 0) {
			bp = (struct cblock *)(q->c_cf-1);
			bp = (struct cblock *)((int)bp & ~CROUND);
			q->c_cf = q->c_cl = NULL;
			bp->c_next = cfreelist;
			cfreelist = bp;
			break;
		}
		if (((int)q->c_cf & CROUND) == 0) {
			bp = (struct cblock *)(q->c_cf);
			bp--;
			q->c_cf = bp->c_next->c_info;
			bp->c_next = cfreelist;
			cfreelist = bp;
		}
	}
#ifdef UCB_CLIST
	restorseg5(sav5);
#endif
	splx(s);
	return(cp-acp);
}
#endif	MPX_FILS


#if	NDH > 0 || (NDZ > 0 && defined(DZ_PDMA))
/*
 * Return count of contiguous characters
 * in clist starting at q->c_cf.
 * Stop counting if flag&character is non-null.
 */
ndqb(q, flag)
register struct clist *q;
{
	register cc;
	int s;
#ifdef UCB_CLIST
	segm sav5;
#endif

	s = spl6();
	if (q->c_cc <= 0) {
		cc = -q->c_cc;
		goto out;
	}
	cc = ((int)q->c_cf + CBSIZE) & ~CROUND;
	cc -= (int)q->c_cf;
	if (q->c_cc < cc)
		cc = q->c_cc;
	if (flag) {
		register char *p, *end;

#ifdef	UCB_CLIST
		saveseg5(sav5);
		mapseg5(clststrt, clstdesc);
#endif
		p = q->c_cf;
		end = p + cc;
		while (p < end) {
			if (*p & flag) {
				cc = (int)p;
				cc -= (int)q->c_cf;
				break;
			}
			p++;
		}
#ifdef	UCB_CLIST
		restorseg5(sav5);
#endif
	}
out:
	splx(s);
	return(cc);
}



/*
 * Update clist to show that cc characters
 * were removed.  It is assumed that cc < CBSIZE.
 */
ndflush(q, cc)
register struct clist *q;
register cc;
{
	register s;
#ifdef UCB_CLIST
	segm sav5;
#endif

	s = spl6();
	if (q->c_cc < 0) {
		if (q->c_cf != NULL) {
			q->c_cc += cc;
			q->c_cf += cc;
			goto out;
		}
		q->c_cc = 0;
		goto out;
	}
	if (q->c_cc == 0)
		goto out;
	if (cc > CBSIZE || cc <= 0) {
		cbad++;
		goto out;
	}
	q->c_cc -= cc;
	q->c_cf += cc;
	if (((int)q->c_cf & CROUND) == 0) {
		register struct cblock *bp;
#ifdef UCB_CLIST
		saveseg5(sav5);
		mapseg5(clststrt, clstdesc);
#endif

		bp = (struct cblock *)(q->c_cf) -1;
		if (bp->c_next)
			q->c_cf = bp->c_next->c_info;
		else
			q->c_cf = q->c_cl = NULL;
		bp->c_next = cfreelist;
		cfreelist = bp;
#ifdef UCB_CLIST
		restorseg5(sav5);
#endif
	} else
		if (q->c_cc == 0) {
			register struct cblock *bp;
#ifdef UCB_CLIST
			saveseg5(sav5);
			mapseg5(clststrt, clstdesc);
#endif
			q->c_cf = (char *)((int)q->c_cf & ~CROUND);
			bp = (struct cblock *)(q->c_cf);
			bp->c_next = cfreelist;
			cfreelist = bp;
			q->c_cf = q->c_cl = NULL;
#ifdef UCB_CLIST
			restorseg5(sav5);
#endif
		}
out:
	splx(s);
}
#endif	NDH || (NDZ && defined(DZ_PDMA))

putc(c, p)
register struct clist *p;
{
	register struct cblock *bp;
	register char *cp;
	int s;
#ifdef UCB_CLIST
	segm sav5;
#endif

	s = spl6();
#ifdef UCB_CLIST
	saveseg5(sav5);
	mapseg5(clststrt, clstdesc);
#endif
	if ((cp = p->c_cl) == NULL || p->c_cc < 0 ) {
		if ((bp = cfreelist) == NULL) {
#ifdef UCB_CLIST
			restorseg5(sav5);
#endif
			splx(s);
			return(-1);
		}
		cfreelist = bp->c_next;
		bp->c_next = NULL;
		p->c_cf = cp = bp->c_info;
	} else
		if (((int)cp & CROUND) == 0) {
			bp = (struct cblock *)cp - 1;
			if ((bp->c_next = cfreelist) == NULL) {
#ifdef UCB_CLIST
				restorseg5(sav5);
#endif
				splx(s);
				return(-1);
			}
			bp = bp->c_next;
			cfreelist = bp->c_next;
			bp->c_next = NULL;
			cp = bp->c_info;
		}
	*cp++ = c;
	p->c_cc++;
	p->c_cl = cp;
#ifdef UCB_CLIST
	restorseg5(sav5);
#endif
	splx(s);
	return(0);
}



/*
 * copy buffer to clist.
 * return number of bytes not transfered.
 */
b_to_q(cp, cc, q)
register char *cp;
struct clist *q;
register int cc;
{
	register char *cq;
	struct cblock *bp;
	int s, acc;
#ifdef UCB_CLIST
	segm sav5;
#endif

	if (cc <= 0)
		return(0);
	acc = cc;


	s = spl6();
#ifdef UCB_CLIST
	saveseg5(sav5);
	mapseg5(clststrt, clstdesc);
#endif
	if ((cq = q->c_cl) == NULL || q->c_cc < 0) {
		if ((bp = cfreelist) == NULL) 
			goto out;
		cfreelist = bp->c_next;
		bp->c_next = NULL;
		q->c_cf = cq = bp->c_info;
	}

	while (cc) {
		if (((int)cq & CROUND) == 0) {
			bp = (struct cblock *) cq - 1;
			if ((bp->c_next = cfreelist) == NULL) 
				goto out;
			bp = bp->c_next;
			cfreelist = bp->c_next;
			bp->c_next = NULL;
			cq = bp->c_info;
		}
		*cq++ = *cp++;
		cc--;
	}
out:
	q->c_cl = cq;
	q->c_cc += acc-cc;
#ifdef UCB_CLIST
	restorseg5(sav5);
#endif
	splx(s);
	return(cc);
}

/*
 * Initialize clist by freeing all character blocks.
 */
cinit()
{
	register int ccp;
	register struct cblock *cp;
	register struct cdevsw *cdp;

	ccp = (int)cfree;
#ifdef UCB_CLIST
	mapseg5(clststrt, clstdesc);
#else
	ccp = (ccp+CROUND) & ~CROUND;
#endif
	for(cp=(struct cblock *)ccp; cp <= &cfree[nclist-1]; cp++) {
		cp->c_next = cfreelist;
		cfreelist = cp;
	}
#ifdef UCB_CLIST
	normalseg5();
#endif
}

#ifdef	unneeded
/*
 * integer (2-byte) get/put
 * using clists
 */
getw(p)
register struct clist *p;
{
	register int s;

	if (p->c_cc <= 1)
		return(-1);
	s = getc(p);
	return(s | (getc(p)<<8));
}
#endif	unneeded

#ifdef	MPX_FILS
putw(c, p)
register struct clist *p;
{
	register s;

	s = spl6();
	if (cfreelist==NULL) {
		splx(s);
		return(-1);
	}
	putc(c, p);
	putc(c>>8, p);
	splx(s);
	return(0);
}
#endif


#ifdef UCB_NTTY
/*
 * Given a non-NULL pointer into the list (like c_cf which
 * always points to a real character if non-NULL) return the pointer
 * to the next character in the list or return NULL if no more chars.
 *
 * Callers must not allow getc's to happen between nextc's so that the
 * pointer becomes invalid.  Note that interrupts are NOT masked.
 */
char *
nextc(p, cp)
register struct clist *p;
register char *cp;
{
	register char *rcp;

#ifdef UCB_CLIST
	segm sav5;

	saveseg5(sav5);
	mapseg5(clststrt, clstdesc);
#endif
	if (p->c_cc && ++cp != p->c_cl) {
		if (((int)cp & CROUND) == 0)
			rcp = ((struct cblock *)cp)[-1].c_next->c_info;
		else
			rcp = cp;
	}
	else
		rcp = (char *) NULL;
#ifdef UCB_CLIST
	restorseg5(sav5);
#endif
	return rcp;
}

#ifdef	UCB_CLIST
/*
 * Lookc returns the character pointed at by cp, which is a nextc-style
 * (e.g. possibly mapped out) char pointer.
 */
lookc(cp)
register char *cp;
{
	register char rc;
	segm sav5;

	saveseg5(sav5);
	mapseg5(clststrt, clstdesc);
	rc = *cp;
	restorseg5(sav5);
	return rc;
}
#endif

/*
 * Remove the last character in the list and return it.
 */
unputc(p)
register struct clist *p;
{
	register struct cblock *bp;
	register int c, s;
	struct cblock *obp;
#ifdef UCB_CLIST
	segm sav5;
#endif

	s = spl6();
#ifdef UCB_CLIST
	saveseg5(sav5);
	mapseg5(clststrt, clstdesc);
#endif
	if (p->c_cc <= 0)
		c = -1;
	else {
		c = *--p->c_cl;
		if (--p->c_cc <= 0) {
			bp = (struct cblock *)p->c_cl;
			bp = (struct cblock *)((int)bp & ~CROUND);
			p->c_cl = p->c_cf = NULL;
			bp->c_next = cfreelist;
			cfreelist = bp;
		} else
			if (((int)p->c_cl & CROUND) == sizeof(bp->c_next)) {
				p->c_cl = (char *)((int)p->c_cl & ~CROUND);
				bp = (struct cblock *)p->c_cf;
				bp = (struct cblock *)((int)bp & ~CROUND);
				while (bp->c_next != (struct cblock *)p->c_cl)
					bp = bp->c_next;
				obp = bp;
				p->c_cl = (char *)(bp + 1);
				bp = bp->c_next;
				bp->c_next = cfreelist;
				cfreelist = bp;
				obp->c_next = NULL;
			}
	}
#ifdef UCB_CLIST
	restorseg5(sav5);
#endif
	splx(s);
	return (c);
}

/*
 * Put the chars in the ``from'' queue
 * on the end of the ``to'' queue.
 */
catq(from, to)
struct clist *from, *to;
{
	register c;

	while ((c = getc(from)) >= 0)
		 putc(c, to);
}
#endif	UCB_NTTY
