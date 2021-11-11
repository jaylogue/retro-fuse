/*
 *	Memory special file
 *	minor device 0 is physical memory
 *	minor device 1 is kernel memory
 *	minor device 2 is EOF/RATHOLE
 */

#include "param.h"
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/conf.h>
#include <sys/seg.h>

/*
 *	SCCS id	@(#)mem.c	2.1 (Berkeley)	8/5/83
 */


mmread(dev)
dev_t dev;
{
	register c, bn, on;
	int a, d;

	if(minor(dev) == 2)
		return;
	on = u.u_count;
	if (minor(dev)==1 && u.u_segflg==0 &&
		((u.u_offset | u.u_base | on) & 01) == 0) {
		c = copyout((caddr_t)u.u_offset, u.u_base, on);
		if (c) {
			u.u_error = EFAULT;
			return;
		}
		u.u_base += on;
		u.u_offset += on;
		u.u_count -= on;
		return;
	}
	do {
		bn = u.u_offset >> 6;
		on = u.u_offset & 077;
		a = UISA[0];
		d = UISD[0];
		UISA[0] = bn;
		UISD[0] = RO;		/* one click, read only */
		if(minor(dev) == 1)
			UISA[0] = ((physadr) ka6-6)->r[(bn>>7)&07] + (bn&0177);
		if ((c = fuibyte((caddr_t)on)) < 0)
			u.u_error = ENXIO;
		UISA[0] = a;
		UISD[0] = d;
	} while(u.u_error==0 && passc(c)>=0);
}

mmwrite(dev)
dev_t dev;
{
	register c, bn, on;
	int a, d;

	if(minor(dev) == 2) {
		u.u_count = 0;
		return;
	}
	for(;;) {
		bn = u.u_offset >> 6;
		on = u.u_offset & 077;
		if ((c=cpass())<0 || u.u_error!=0)
			break;
		a = UISA[0];
		d = UISD[0];
		UISA[0] = bn;
		UISD[0] = RW;		/* one click, read/write */
		if(minor(dev) == 1)
			UISA[0] = ((physadr) ka6-6)->r[(bn>>7)&07] + (bn&0177);
		if (suibyte((caddr_t)on, c) < 0)
			u.u_error = ENXIO;
		UISA[0] = a;
		UISD[0] = d;
	}
}
#ifdef	UCB_ECC
/*
 * Internal versions of mmread(), mmwrite()
 * used by disk driver ecc routines.
 */

int
getmemc(addr)
long addr;
{
	unsigned int bn, on;
	register a, d, s;
	int	c;

	bn = addr >> 6;
	on = addr & 077;
	a = UISA[0];
	d = UISD[0];
	UISA[0] = bn;
	UISD[0] = RO;		/* one click, read only */
	c = fuibyte((caddr_t)on);
	UISA[0] = a;
	UISD[0] = d;
	return(c);
}

putmemc(addr,contents)
long addr;
int contents;
{
	unsigned int bn, on;
	register a, d, s;

	bn = addr >> 6;
	on = addr & 077;
	a = UISA[0];
	d = UISD[0];
	UISA[0] = bn;
	UISD[0] = RW;		/* one click, read/write */
	suibyte((caddr_t)on, contents);
	UISA[0] = a;
	UISD[0] = d;
}
#endif
