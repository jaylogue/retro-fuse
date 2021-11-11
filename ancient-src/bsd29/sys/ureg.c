#include "param.h"
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/text.h>
#include <sys/seg.h>

/*
 *	SCCS id	@(#)ureg.c	2.1 (Berkeley)	8/29/83
 */

/*
 * Load the user hardware segmentation
 * registers from the software prototype.
 * The software registers must have
 * been setup prior by estabur.
 */
sureg()
{
	register *udp, *uap, *rdp;
	int *rap, *limudp;
	int taddr, daddr;
#ifdef	VIRUS_VFORK
	int saddr;
#endif
	struct text *tp;

#ifdef	VIRUS_VFORK
	taddr = daddr = u.u_procp->p_daddr;
	saddr = u.u_procp->p_saddr;
#else
	taddr = daddr = u.u_procp->p_addr;
#endif
	if ((tp=u.u_procp->p_textp) != NULL)
		taddr = tp->x_caddr;
#ifndef NONSEPARATE
	if (sep_id)
		limudp = &u.u_uisd[16];
	else
#endif
		limudp = &u.u_uisd[8];
	rap = (int *) UISA;
	rdp = (int *) UISD;
	uap = &u.u_uisa[0];
	for (udp = &u.u_uisd[0]; udp < limudp;) {
#ifdef	VIRUS_VFORK
		*rap++ = *uap++ +
		    (*udp & TX? taddr: (*udp&ED? saddr: (*udp&ABS? 0: daddr)));
#else
		*rap++ = *uap++ + (*udp & TX?  taddr: (*udp & ABS?  0: daddr));
#endif
		*rdp++ = *udp++;
	}
#ifdef	MENLO_OVLY
	/*
	 *  Since software prototypes are not maintained for overlay
	 *  segments, force overlay change.  The test for TX is because
	 *  there is no text if called from core().
	 */
	if (u.u_ovdata.uo_ovbase && (u.u_uisd[0] & TX))
		choverlay(u.u_uisd[0] & ACCESS);
#endif
}

/*
 * Set up software prototype segmentation
 * registers to implement the 3 pseudo
 * text,data,stack segment sizes passed
 * as arguments.
 * The argument sep specifies if the
 * text and data+stack segments are to
 * be separated.
 * The last argument determines whether the text
 * segment is read-write or read-only.
 */
estabur(nt, nd, ns, sep, xrw)
unsigned nt, nd, ns;
{
	register a, *ap, *dp;
#ifdef	MENLO_OVLY
	unsigned ts;
#endif

#ifdef	MENLO_OVLY
	if (u.u_ovdata.uo_ovbase && nt)
		ts = u.u_ovdata.uo_dbase;
	else
		ts = nt;
#endif
	if(sep) {
#ifndef	NONSEPARATE
		if(!sep_id)
			goto err;
#ifdef	MENLO_OVLY
		if(ctos(ts) > 8 || ctos(nd)+ctos(ns) > 8)
#else
		if(ctos(nt) > 8 || ctos(nd)+ctos(ns) > 8)
#endif
#endif	NONSEPARATE
			goto err;
	} else
#ifdef	MENLO_OVLY
		if(ctos(ts) + ctos(nd) + ctos(ns) > 8)
			goto err;
	if (u.u_ovdata.uo_ovbase && nt)
		ts = u.u_ovdata.uo_ov_offst[NOVL];
	if(ts + nd + ns + USIZE > maxmem)
		goto err;
#else
		if(ctos(nt) + ctos(nd) + ctos(ns) > 8)
			goto err;
	if(nt + nd + ns + USIZE > maxmem)
		goto err;
#endif
	a = 0;
	ap = &u.u_uisa[0];
	dp = &u.u_uisd[0];
	while(nt >= 128) {
		*dp++ = (127 << 8) | xrw | TX;
		*ap++ = a;
		a += 128;
		nt -= 128;
	}
	if(nt) {
		*dp++ = ((nt - 1) << 8) | xrw | TX;
		*ap++ = a;
	}
#ifdef	MENLO_OVLY
#ifdef	NONSEPARATE
	if (u.u_ovdata.uo_ovbase && ts)
#else
	if ((u.u_ovdata.uo_ovbase && ts) && !sep)
#endif
		{
		/*
		 * overlay process, adjust accordingly.
		 * The overlay segment's registers will be set by
 		 * choverlay() from sureg().
		 */ 
		register novlseg;
		for(novlseg = 0; novlseg < u.u_ovdata.uo_nseg; novlseg++) {
			*ap++ = 0;
			*dp++ = 0;
		}
	}
#endif
#ifndef NONSEPARATE
	if(sep)
		while(ap < &u.u_uisa[8]) {
			*ap++ = 0;
			*dp++ = 0;
		}
#endif

#ifdef	VIRUS_VFORK
	a = 0;
#else
	a = USIZE;
#endif
	while(nd >= 128) {
		*dp++ = (127 << 8) | RW;
		*ap++ = a;
		a += 128;
		nd -= 128;
	}
	if(nd) {
		*dp++ = ((nd - 1) << 8) | RW;
		*ap++ = a;
		a += nd;
	}
	while(ap < &u.u_uisa[8]) {
		if(*dp & ABS) {
			dp++;
			ap++;
			continue;
		}
		*dp++ = 0;
		*ap++ = 0;
	}
#ifndef NONSEPARATE
	if(sep)
		while(ap < &u.u_uisa[16]) {
			if(*dp & ABS) {
				dp++;
				ap++;
				continue;
			}
			*dp++ = 0;
			*ap++ = 0;
		}
#endif
#ifdef	VIRUS_VFORK
	a = ns;
	while(ns >= 128) {
		a -= 128;
		ns -= 128;
		*--dp = (0 << 8) | RW | ED;
		*--ap = a;
	}
#else
	a += ns;
	while(ns >= 128) {
		a -= 128;
		ns -= 128;
		*--dp = (127 << 8) | RW;
		*--ap = a;
	}
#endif
	if(ns) {
		*--dp = ((128 - ns) << 8) | RW | ED;
		*--ap = a - 128;
	}
#ifndef NONSEPARATE
	if(!sep) {
		ap = &u.u_uisa[0];
		dp = &u.u_uisa[8];
		while(ap < &u.u_uisa[8])
			*dp++ = *ap++;
		ap = &u.u_uisd[0];
		dp = &u.u_uisd[8];
		while(ap < &u.u_uisd[8])
			*dp++ = *ap++;
	}
#endif
	sureg();
	return(0);

err:
	u.u_error = ENOMEM;
	return(-1);
}

#ifdef	MENLO_OVLY
/*
 * Routine to change overlays.
 * Only hardware registers are changed;
 * must be called from sureg
 * since the software prototypes will be out of date.
 */
choverlay(xrw)
{
	register int *rap, *rdp;
	register nt;
	int addr, *limrdp;

	rap = &(UISA[u.u_ovdata.uo_ovbase]);
	rdp = &(UISD[u.u_ovdata.uo_ovbase]);
	limrdp = &(UISD[u.u_ovdata.uo_ovbase + u.u_ovdata.uo_nseg]);
	if (u.u_ovdata.uo_curov) {
		addr = u.u_ovdata.uo_ov_offst[u.u_ovdata.uo_curov - 1];
		nt = u.u_ovdata.uo_ov_offst[u.u_ovdata.uo_curov] - addr;
		addr += u.u_procp->p_textp->x_caddr;
		while (nt >= 128) {
			*rap++ = addr;
			*rdp++ = (127 << 8) | xrw;
			addr += 128;
			nt -= 128;
		}
		if (nt) {
			*rap++ = addr;
			*rdp++ = ((nt-1) << 8) | xrw;
		}
	}
	while (rdp < limrdp) {
		*rap++ = 0;
		*rdp++ = 0;
	}
#ifndef	NONSEPARATE
	/*
	 * This section copies the UISA/UISD registers to the
	 * UDSA/UDSD registers.  It is only needed for data fetches
	 * on the overlaid segment, which normally don't happen.
	 */
	if (!u.u_sep && sep_id) {
		rdp = &(UISD[u.u_ovdata.uo_ovbase]);
		rap = rdp + 8;
		/* limrdp is still correct */
		while (rdp < limrdp)
			*rap++ = *rdp++;
		rdp = &(UISA[u.u_ovdata.uo_ovbase]);
		rap = rdp + 8;
		limrdp = &(UISA[u.u_ovdata.uo_ovbase + u.u_ovdata.uo_nseg]);
		while (rdp < limrdp)
			*rap++ = *rdp++;
	}
#endif
}
#endif	MENLO_OVLY
