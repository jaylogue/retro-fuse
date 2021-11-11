/*
 *	SCCS id	@(#)dn.c	2.1 (Berkeley)	8/5/83
 */

/*
 * DN-11 ACU interface
 */

#include "dn.h"
#if	NDN > 0
#include "param.h"
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/dnreg.h>

#define	DNPRI	(PZERO+5)

struct	dndevice *dn_addr[NDN];

dnattach(addr, unit)
struct dndevice *addr;
{
	if ((unsigned) unit >= NDN)
		return 0;
	dn_addr[unit] = addr;
	return 1;
}

/*ARGSUSED*/
dnopen(dev, flag)
register dev_t	dev;
{
	register struct dndevice *dp;

	dev = minor(dev);
	if (dev >= NDN << 2
	    || (dp = dn_addr[dev >> 2]) == NULL
	    || (dp->dnisr[dev & 03] & (DN_PWI | DN_FDLO | DN_FCRQ)))
		u.u_error = ENXIO;
	else
		{
		dp->dnisr[0] |= DN_MINAB;
		dp->dnisr[dev & 03] = DN_INTENB | DN_MINAB | DN_FCRQ;
	}
}

dnclose(dev)
register dev_t	dev;
{
	dev = minor(dev);
	dn_addr[dev >> 2]->dnisr[dev & 03] = DN_MINAB;
}

dnwrite(dev)
register dev_t	dev;
{
	int s;
	register c;
	register *dp;
	extern lbolt;

	dev = minor(dev);
	dp = &(dn_addr[dev >> 2]->dnisr[dev & 03]);
	while ((*dp & (DN_PWI | DN_ACR | DN_DSS)) == 0) {
		s = spl4();
		if ((*dp & DN_FPND) == 0 || u.u_count == 0 || (c = cpass()) < 0)
			sleep((caddr_t) dp, DNPRI);
		else if (c == '-') {
			sleep((caddr_t) &lbolt, DNPRI);
			sleep((caddr_t) &lbolt, DNPRI);
		} else
			{
			*dp = (c << 8) | DN_INTENB|DN_MINAB|DN_FDPR|DN_FCRQ;
			sleep((caddr_t) dp, DNPRI);
		}
		splx(s);
	}
	if (*dp & (DN_PWI | DN_ACR))
		u.u_error = EIO;
}

dnint(dn11)
{
	register *dp, *ep;

	dp = &(dn_addr[dn11]->dnisr[0]);
	*dp &= ~DN_MINAB;
	for (ep = dp; ep < dp + 4; ep++)
		if (*ep & DN_DONE) {
			*ep &= ~DN_DONE;
			wakeup((caddr_t)ep);
		}
	*dp |= DN_MINAB;
}
#endif	NDN
