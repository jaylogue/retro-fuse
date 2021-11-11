/*
 *	SCCS id	@(#)clock.c	2.1 (Berkeley)	9/1/83
 */

#include "param.h"
#include <sys/systm.h>
#include <sys/callout.h>
#include <sys/seg.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/reg.h>
#ifdef UCB_METER
#include <sys/text.h>
#include <sys/vm.h>
#endif
#ifdef UCB_NET
	/*
	 * shifted to keep "make depend" from finding these for now...
	 */
#	include <sys/protosw.h>
#	include <sys/socket.h>
#	include "../net/if.h"
#	include "../net/in_systm.h"
#endif

#ifdef UCB_FRCSWAP
extern int	idleflg;	/* If set, allow incore forks and expands */
				/* Set before idle(), cleared in clock.c */
#endif

#ifdef  UCB_NET
/*
 * Protoslow is like lbolt, but for slow protocol timeouts, counting
 * up to (hz/PR_SLOWHZ), then causing a pfslowtimo().
 * Protofast is like lbolt, but for fast protocol timeouts, counting
 * up to (hz/PR_FASTHZ), then causing a pffasttimo().
 */
int	protoslow;
int	protofast;
int	ifnetslow;
int     netoff;
#endif

#define	SCHMAG	8/10

/*
 * clock is called straight from
 * the real time clock interrupt.
 *
 * Functions:
 *	reprime clock
 *	copy *switches to display
 *	implement callouts
 *	maintain user/system times
 *	maintain date
 *	profile
 *	lightning bolt wakeup (every second)
 *	alarm clock signals
 *	jab the scheduler
 */

/*ARGSUSED*/
#ifdef	MENLO_KOV
clock(dev, sp, r1, ov, nps, r0, pc, ps)
#else
clock(dev, sp, r1, nps, r0, pc, ps)
#endif
dev_t dev;
caddr_t pc;
{
	register a;
	mapinfo	map;
	extern caddr_t waitloc;
	extern char *panicstr;

	/*
	 * restart clock
	 */
	if(lks)
		*lks = 0115;
	/*
	 * ensure normal mapping of kernel data
	 */
	savemap(map);

#ifdef	DISPLAY
	/*
	 * display register
	 */
	display();
#endif

	/*
	 * callouts
	 * never after panics
	 * if none, just continue
	 * else update first non-zero time
	 */
	if (panicstr == (char *) 0) {
		register struct callout *p1, *p2;

		if(callout[0].c_func == NULL)
			goto out;
		p2 = &callout[0];
		while(p2->c_time<=0 && p2->c_func!=NULL)
			p2++;
		p2->c_time--;

#ifdef  UCB_NET
		/*
		 * Time moves on for protocols.
		 */
		if(!netoff) {
			--protoslow; --protofast; --ifnetslow;
			if(protoslow<=0 || protofast<=0 || ifnetslow<=0)
				schednetisr(NETISR_CLOCK);
		}
#endif

		/*
		 * if ps is high, just return
		 */
		if (!BASEPRI(ps))
			goto out;

		/*
		 * callout
		 */
		if(callout[0].c_time <= 0) {
			p1 = &callout[0];
			while(p1->c_func != 0 && p1->c_time <= 0) {
				(*p1->c_func)(p1->c_arg);
				p1++;
			}
			p2 = &callout[0];
			while(p2->c_func = p1->c_func) {
				p2->c_time = p1->c_time;
				p2->c_arg = p1->c_arg;
				p1++;
				p2++;
			}
		}
	}
out:

	a = (dk_busy != 0);
	if (USERMODE(ps)) {
		u.u_utime++;
		if(u.u_prof.pr_scale)
			addupc(pc, &u.u_prof, 1);
		if(u.u_procp->p_nice > NZERO)
			a += 2;
	} else {
		a += 4;
		if (pc == waitloc)
			a += 2;
		u.u_stime++;
	}
	sy_time[a] += 1;
	dk_time[dk_busy & ((1 << ndisk) - 1)] += 1;
	{
	  register struct proc *pp;
	  pp = u.u_procp;
	  if(++pp->p_cpu == 0)
		pp->p_cpu--;
	}
	/*
	 * lightning bolt time-out
	 * and time of day
	 */
	if ((++lbolt >= hz) && (BASEPRI(ps))) {
		register struct proc *pp;

		lbolt -= hz;
#ifdef	UCB_FRCSWAP
		idleflg = 0;
#endif
		++time;
		(void) _spl1();
#if defined(UCB_LOAD) || defined(UCB_METER)
		meter();
#endif
		runrun++;
		wakeup((caddr_t)&lbolt);
		for(pp = &proc[0]; pp <= maxproc; pp++)
		if (pp->p_stat && pp->p_stat<SZOMB) {
			if(pp->p_time != 127)
				pp->p_time++;
#ifdef	UCB_METER
			if (pp->p_stat == SSLEEP || pp->p_stat == SSTOP)
				if (pp->p_slptime != 127)
					pp->p_slptime++;
#endif	UCB_METER
			if (pp->p_clktim && --pp->p_clktim == 0)
#ifdef	UCB_NET
				/*
				 * If process has clock counting down, and it
				 * expires, set it running (if this is a
				 * tsleep()), or give it an SIGALRM (if the user
				 * process is using alarm signals.
				 */
				if (pp->p_flag & STIMO) {
					a = spl6();
					switch (pp->p_stat) {

					case SSLEEP:
						setrun(pp);
						break;

					case SSTOP:
						unsleep(pp);
						break;
					}
					pp->p_flag &= ~STIMO;
					splx(a);
				} else
#endif
					psignal(pp, SIGALRM);
			a = (pp->p_cpu & 0377) * SCHMAG + pp->p_nice - NZERO;
			if(a < 0)
				a = 0;
			if(a > 255)
				a = 255;
			pp->p_cpu = a;
			if(pp->p_pri >= PUSER)
				setpri(pp);
		}
		if(runin!=0) {
			runin = 0;
			wakeup((caddr_t)&runin);
		}
	}
	restormap(map);
}

/*
 * timeout is called to arrange that
 * fun(arg) is called in tim/hz seconds.
 * An entry is sorted into the callout
 * structure. The time in each structure
 * entry is the number of hz's more
 * than the previous entry.
 * In this way, decrementing the
 * first entry has the effect of
 * updating all entries.
 *
 * The panic is there because there is nothing
 * intelligent to be done if an entry won't fit.
 */
timeout(fun, arg, tim)
int (*fun)();
caddr_t arg;
{
	register struct callout *p1, *p2;
	register int t;
	int s;

	t = tim;
	p1 = &callout[0];
	s = spl7();
	while(p1->c_func != 0 && p1->c_time <= t) {
		t -= p1->c_time;
		p1++;
	}
	p1->c_time -= t;
	p2 = p1;
	while(p2->c_func != 0)
		p2++;
	if (p2 >= callNCALL)
		panic("Timeout table overflow");
	while(p2 >= p1) {
		(p2+1)->c_time = p2->c_time;
		(p2+1)->c_func = p2->c_func;
		(p2+1)->c_arg = p2->c_arg;
		p2--;
	}
	p1->c_time = t;
	p1->c_func = fun;
	p1->c_arg = arg;
	splx(s);
	return;
}

#if defined(UCB_LOAD) || defined(UCB_METER)
/*
 * Count up various things once a second
 */
short avenrun[3];	/* internal load average in psuedo-floating point */
#define ave(smooth,new,time)  (smooth) = (((time)-1) * (smooth) + (new))/ (time)

#define	ctok(cliks)	(((cliks) >> 4) & 07777)	/* clicks to KB */

meter()
{
#ifdef UCB_METER
	register unsigned *cp, *rp;
	register long *sp;

	ave(avefree, ctok(freemem), 5);
#endif

	if (time % 5 == 0) {
		vmtotal();
#ifdef UCB_METER
		cp = &cnt.v_first; rp = &rate.v_first; sp = &sum.vs_first;
		while (cp <= &cnt.v_last) {
			*rp = *cp;
			*sp += *cp;
			*cp = 0;
			rp++, cp++, sp++;
		}
#endif
	}
}

vmtotal()
{
	extern	char	counted[];
	register struct proc *p;
	register struct text *xq;
	register nrun = 0;
#ifdef UCB_METER
	int nt;

	total.t_vmtxt = 0;
	total.t_avmtxt = 0;
	total.t_rmtxt = 0;
	total.t_armtxt = 0;
	for (xq = text; xq < textNTEXT; xq++) {
		counted[xq-text]=0;
		if (xq->x_iptr) {
			total.t_vmtxt += xq->x_size;
			if (xq->x_ccount)
				total.t_rmtxt += xq->x_size;
		}
	}
	total.t_vm = 0;
	total.t_avm = 0;
	total.t_rm = 0;
	total.t_arm = 0;
	total.t_rq = 0;
	total.t_dw = 0;
	total.t_sl = 0;
	total.t_sw = 0;
#endif
	for (p = &proc[1]; p <= maxproc; p++) {
		if (p->p_stat) {
#ifdef UCB_METER
#ifndef	VIRUS_VFORK
			total.t_vm += p->p_size;
			if (p->p_flag & SLOAD)
				total.t_rm += p->p_size;
#else
			total.t_vm += p->p_dsize + p->p_ssize + USIZE;
			if (p->p_flag & SLOAD)
				total.t_rm += p->p_dsize + p->p_ssize + USIZE;
#endif
#endif
			switch (p->p_stat) {

			case SSLEEP:
			case SSTOP:
				if (p->p_pri <= PZERO)
					nrun++;
#ifdef UCB_METER
				if (p->p_flag & SLOAD) {
					if (p->p_pri <= PZERO)
						total.t_dw++;
					else if (p->p_slptime < MAXSLP)
						total.t_sl++;
				} else if (p->p_slptime < MAXSLP)
					total.t_sw++;
				if (p->p_slptime < MAXSLP)
					goto active;
#endif
				break;

			case SRUN:
			case SIDL:
				nrun++;
#ifdef UCB_METER
				if (p->p_flag & SLOAD)
					total.t_rq++;
				else
					total.t_sw++;
active:
#ifndef	VIRUS_VFORK
				total.t_avm += p->p_size;
				if (p->p_flag & SLOAD)
					total.t_arm += p->p_size;
#else
				total.t_avm += p->p_dsize + p->p_ssize + USIZE;
				if (p->p_flag & SLOAD)
					total.t_arm +=
						p->p_dsize + p->p_ssize + USIZE;
#endif
				if (p->p_textp) {
					total.t_avmtxt += p->p_textp->x_size;
					nt = p->p_textp-text;
					if (counted[nt]==0) {
						counted[nt]=1;
						if (p->p_textp->x_ccount)
							total.t_armtxt +=
							    p->p_textp->x_size;
					}
				}
#endif
				break;
			}
		}
	}
#ifdef UCB_METER
	total.t_vm += total.t_vmtxt;
	total.t_avm += total.t_avmtxt;
	total.t_rm += total.t_rmtxt;
	total.t_arm += total.t_armtxt;
	total.t_free = avefree;
#endif
	loadav(avenrun, nrun);
}

/*
 * Compute Tenex style load average.  This code is adapted from similar
 * code by Bill Joy on the Vax system.  The major change is that we
 * avoid floating point since not all pdp-11's have it.  This makes
 * the code quite hard to read - it was derived with some algebra.
 *
 * "floating point" numbers here are stored in a 16 bit short, with
 * 8 bits on each side of the decimal point.  Some partial products
 * will have 16 bits to the right.
 */

	/*
	 * The Vax algorithm is:
	 *
	 * /*
	 *  * Constants for averages over 1, 5, and 15 minutes
	 *  * when sampling at 5 second intervals.
	 *  * /
	 * double	cexp[3] = {
	 * 	0.9200444146293232,	/* exp(-1/12) * /
	 * 	0.9834714538216174,	/* exp(-1/60) * /
	 * 	0.9944598480048967,	/* exp(-1/180) * /
	 * };
	 * 
	 * /*
	 *  * Compute a tenex style load average of a quantity on
	 *  * 1, 5 and 15 minute intervals.
	 *  * /
	 * loadav(avg, n)
	 * 	register double *avg;
	 * 	int n;
	 * {
	 * 	register int i;
	 * 
	 * 	for (i = 0; i < 3; i++)
	 * 		avg[i] = cexp[i] * avg[i] + n * (1.0 - cexp[i]);
	 * }
	 */

long cexp[3] = {
	0353,	/* 256*exp(-1/12) */
	0373,	/* 256*exp(-1/60) */
	0376,	/* 256*exp(-1/180) */
};

loadav(avg, n)
	register short *avg;
	register n;
{
	register int i;

	for (i = 0; i < 3; i++)
		avg[i] = (cexp[i] * (avg[i]-(n<<8)) + (((long)n)<<16)) >> 8;
}
#endif
