/*
 *	SCCS id	@(#)trap.c	2.1 (Berkeley)	9/4/83
 */

#include "param.h"
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/seg.h>
#ifdef UCB_METER
#include <sys/vm.h>
#endif
#include <sys/trap.h>
#include <sys/inline.h>
#include <sys/iopage.h>

#define	SETD	0170011		/* SETD instruction */
#define	SYS	0104400		/* sys (trap) instruction */
#define	USER	020		/* user-mode flag added to dev */

#ifdef MENLO_KOV
extern	int _ovno;
#endif
#ifdef DIAGNOSTIC
extern	int hasmap;
int	savhasmap;
#endif
mapinfo kernelmap;		/* saved mapping info on kernel-mode trap */

/*
 * In order to stop the system from destroying
 * kernel data space any further, allow only one
 * fatal trap. After once_thru is set, any
 * futher traps will be handled by looping in place.
 */
int	once_thru = 0;

/*
 * Offsets of the user's registers relative to
 * the saved r0.  See sys/reg.h.
 */
char	regloc[] = {
	R0, R1, R2, R3, R4, R5, R6, R7, RPS
};

/*
 * Called from mch.s when a processor trap occurs.
 * The arguments are the words saved on the system stack
 * by the hardware and software during the trap processing.
 * Their order is dictated by the hardware and the details
 * of C's calling sequence. They are peculiar in that
 * this call is not 'by value' and changed user registers
 * get copied back on return.
 * Dev is the kind of trap that occurred.
 */
/*ARGSUSED*/
#ifdef MENLO_KOV
trap(dev, sp, r1, ov, nps, r0, pc, ps)
#else
trap(dev, sp, r1, nps, r0, pc, ps)
#endif
int	*pc;
dev_t	dev;
{
	register i;
	register int *a;
	int (*fetch)(), fuword(), fuiword();
	time_t syst;

	if (once_thru) {
		(void) _spl7();
		for(;;);
	}
	if (USERMODE(ps))
		dev |= USER;
	else
		/* guarantee normal kernel mapping */
		savemap(kernelmap);
	syst = u.u_stime;
#ifndef NONFP
	u.u_fpsaved = 0;
#endif
	u.u_ar0 = &r0;
#ifdef UCB_METER
	cnt.v_trap++;
#endif
	switch(minor(dev)) {

	/*
	 * Trap not expected.
	 * Usually a kernel mode bus error.
	 * The numbers printed are used to
	 * find the hardware PS/PC as follows.
	 * (all numbers in octal 18 bits)
	 *	address_of_saved_ps =
	 *		(ka6*0100) + aps - 0140000;
	 *	address_of_saved_pc =
	 *		address_of_saved_ps - 2;
	 */
	default:
		once_thru = 1;
#if	defined(DIAGNOSTIC) && !defined(NOKA5)
		/*
		 * Clear hasmap if we attempt to sync the fs.
		 * Else it might fail if we faulted with a mapped
		 * buffer.
		 */
		savhasmap = hasmap;
		hasmap = 0;
#endif
		printf("ka6 = %o\n", *ka6);
		printf("aps = %o\n", &ps);
		printf("pc = %o ps = %o\n", pc, ps);
#ifdef	MENLO_KOV
		printf("__ovno = %d\n", ov);
#endif
#if	PDP11 == 44 || PDP11 == 70 || PDP11 == GENERIC
		if((cputype == 70) || (cputype == 44))
			printf("cpuerr = %o\n", *CPUERR);
#endif
		printf("trap type %o\n", dev);
		panic("trap");

	case BUSFLT + USER:
		i = SIGBUS;
		break;

	/*
	 * If illegal instructions are not
	 * being caught and the offending instruction
	 * is a SETD, the trap is ignored.
	 * This is because C produces a SETD at
	 * the beginning of every program which
	 * will trap on CPUs without an FP-11.
	 */
	case INSTRAP + USER:
		if(fuiword((caddr_t)(pc-1)) == SETD && u.u_signal[SIGILL] == 0)
			goto out;
		i = SIGILL;
		break;

	case BPTTRAP + USER:
		i = SIGTRAP;
		ps &= ~PS_T;
		break;

	case IOTTRAP + USER:
		i = SIGIOT;
		break;

	case EMTTRAP + USER:
		i = SIGEMT;
		break;

#ifndef	NONFP
	/*
	 * Since the floating exception is an
	 * imprecise trap, a user generated
	 * trap may actually come from kernel
	 * mode. In this case, a signal is sent
	 * to the current process to be picked
	 * up later.
	 */
	case ARITHTRAP:
		stst(&u.u_fperr);	/* save error code and address */
		psignal(u.u_procp, SIGFPE);
		runrun++;
		goto kernelout;

	case ARITHTRAP + USER:
		i = SIGFPE;
		stst(&u.u_fperr);
		break;
#endif

	/*
	 * If the user SP is below the stack segment,
	 * grow the stack automatically.
	 * This relies on the ability of the hardware
	 * to restart a half executed instruction.
	 * On the 11/40 this is not the case and
	 * the routine backup/mch.s may fail.
	 * The classic example is on the instruction
	 *	cmp	-(sp),-(sp)
	 */
	case SEGFLT + USER:
	{
	int	osp;

		osp = sp;
		if(backup(u.u_ar0) == 0)
			if(grow((unsigned)osp))
				goto out;
		i = SIGSEGV;
		break;
	}

#if	PDP11 == 44 || PDP11 == 70 || PDP11 == GENERIC
	/*
	 * The code here is a half-hearted
	 * attempt to do something with all
	 * of the PDP11 parity registers.
	 * In fact, there is little that
	 * can be done.
	 */
	case PARITYFLT:
	case PARITYFLT + USER:
		printf("parity\n");
		if((cputype == 70) || (cputype == 44)) {
			for(i = 0; i < 4; i++)
				printf("%o ", MEMERRLO[i]);
			printf("\n");
			MEMERRLO[2] = -1;
			if(dev & USER) {
				i = SIGBUS;
				break;
			}
		}
		panic("parity");
#endif

	/*
	 * Allow process switch
	 */
	case SWITCHTRAP + USER:
		goto out;

	/*
	 * Whenever possible, locations 0-2
	 * specify this style trap, since
	 * DEC hardware often generates spurious
	 * traps through location 0.  This is a
	 * symptom of hardware problems and may
	 * represent a real interrupt that got
	 * sent to the wrong place.  Watch out
	 * for hangs on disk completion if this message appears.
	 * Uninitialized interrupt vectors may be set to trap here also.
	 */
	case ZEROTRAP:
	case ZEROTRAP + USER:
		printf("Trap to 0: ");
		/*FALL THROUGH*/
	case RANDOMTRAP:
	case RANDOMTRAP + USER:
		printf("Random interrupt ignored\n");
		if ((dev & USER) != 0)
			return;

#if	!defined(NONFP) || !defined(NONSEPARATE)
kernelout:
		restormap(kernelmap);
		return;
#endif
	}

	/*
	 * If there is a trap from user mode and it is caught,
	 * send the signal now.  This prevents user-mode exceptions
	 * from being delayed by other signals, and in addition is
	 * more efficient in the case of SIGILL and floating-point
	 * simulation.
	 */
#ifndef	MENLO_JCL
	if (((int)u.u_signal[i] &~ (int)SIG_IGN) && !(u.u_procp->p_flag & STRC))
		sendsig(u.u_signal[i]);
	else
		psignal(u.u_procp, i);
#else	MENLO_JCL
	{
	long sigmask = 1L << (i - 1);
	if (!(u.u_procp->p_ignsig & sigmask) && (u.u_signal[i] != SIG_DFL)
	    && !(u.u_procp->p_flag & STRC))
		sendsig(u.u_signal[i]);
	else
		psignal(u.u_procp, i);
	}
#endif	MENLO_JCL

out:
#ifndef	MENLO_JCL
	if(issig())
#else
	if(u.u_procp->p_cursig || ISSIG(u.u_procp))
#endif
		psig();
	curpri = setpri(u.u_procp);
	if (runrun)
		qswtch();
	if(u.u_prof.pr_scale)
		addupc((caddr_t) pc, &u.u_prof, (int) (u.u_stime - syst));
#ifndef	NONFP
	if (u.u_fpsaved)
		restfp(&u.u_fps);
#endif
}

/*
 * Called from mch.s when a system call occurs.
 * Dummy[13] (and dummy2 if MENLO_KOV is defined) are
 * necessitated by the values of the R[0-7] ... constants
 * in sys/reg.h (which, in turn, are defined by trap's
 * stack frame).
 */
/*ARGSUSED*/
#ifdef	MENLO_KOV
syscall(dummy1, sp, r1, dummy2, dummy3, r0, pc, ps)
#else
syscall(dummy1, sp, r1, dummy3, r0, pc, ps)
#endif
int *pc;
{
	register i;
	register int *a;
	register struct sysent *callp;
	int (*fetch)(), fuword(), fuiword();
	time_t syst;
#ifdef	MENLO_JCL
	int *opc;	/* save original pc in case we must restart syscall */
#endif

	if (!USERMODE(ps))
		panic("syscall");
	syst = u.u_stime;
#ifndef NONFP
	u.u_fpsaved = 0;
#endif
	u.u_ar0 = &r0;
#ifdef UCB_METER
	cnt.v_syscall++;
#endif
	u.u_error = 0;
#ifdef	MENLO_JCL
	opc = pc - 1;		/* opc now points at syscall */
	i = fuiword((caddr_t) opc);
	callp = &sysent[i & 077];
	if (i & 0200) {		/* are argument(s) on stack ? */
		a = sp;
		fetch = fuword;
	} else
#endif
		{
		a = pc;
		fetch = fuiword;
#ifndef	MENLO_JCL
		callp = &sysent[fuiword((caddr_t) (a - 1)) & 077];
#endif
		pc += callp->sy_narg - callp->sy_nrarg;
	}
	if (callp == &sysent[SYSINDIR]) { /* indirect */
		a = (int *) (*fetch)((caddr_t) (a));
		i = fuword((caddr_t) a);
		a++;
		if ((i & ~077) != SYS)
			i = 077;	/* illegal */
		callp = &sysent[i & 077];
		fetch = fuword;
	} else
		if (callp == &sysent[SYSLOCAL]) { /* local */
			a = (int *) (*fetch)((caddr_t) (a));
			i = fuword((caddr_t) a);
			a++;
			if (((i & ~077) != SYS) || ((i &= 077) > nlocalsys))
				i = 0;		/* illegal */
			callp = &syslocal[i];
			fetch = fuword;
		} 
	for (i = 0; i < callp->sy_nrarg; i++)
		u.u_arg[i] = u.u_ar0[regloc[i]];
	for(; i < callp->sy_narg; i++)
		u.u_arg[i] = (*fetch)((caddr_t)a++);
	u.u_dirp = (caddr_t) u.u_arg[0];
	u.u_r.r_val1 = u.u_ar0[R0];
	u.u_r.r_val2 = u.u_ar0[R1];
	u.u_ap = u.u_arg;
#ifndef	MENLO_JCL
	if (save(u.u_qsav)) {
		if (u.u_error==0)
			u.u_error = EINTR;
	} else {
		(*callp->sy_call)();
#if	defined(DIAGNOSTIC) && !defined(NOKA5)
		if(hasmap != (struct buf *) NULL)
			panic("hasmap");
#endif
	}
#else	MENLO_JCL
	if (save(u.u_qsav)) {
		if (u.u_error == 0 && u.u_eosys == JUSTRETURN)
			u.u_error = EINTR;
	} else {
		u.u_eosys = JUSTRETURN;
		(*callp->sy_call)();
#if	defined(DIAGNOSTIC) && !defined(NOKA5)
		if(hasmap != (struct buf *) NULL)
			panic("hasmap");
#endif
	}
	if (u.u_eosys == RESTARTSYS)
		pc = opc;	/* back up pc to restart syscall */
	else if (u.u_eosys == SIMULATERTI)
		dorti(fuiword((caddr_t)opc) & 0200 ?
		      callp->sy_narg - callp->sy_nrarg : 0);
	else
#endif	MENLO_JCL
		if (u.u_error) {
			ps |= PS_C;
			u.u_ar0[R0] = u.u_error;
		} else {
			ps &= ~PS_C;
			u.u_ar0[R0] = u.u_r.r_val1;
			u.u_ar0[R1] = u.u_r.r_val2;
		}
#ifndef	MENLO_JCL
	if(issig())
#else
	if(u.u_procp->p_cursig || ISSIG(u.u_procp))
#endif
		psig();
	curpri = setpri(u.u_procp);
	if (runrun)
		qswtch();
	if(u.u_prof.pr_scale)
		addupc((caddr_t)pc, &u.u_prof, (int)(u.u_stime - syst));
#ifndef NONFP
	if (u.u_fpsaved)
		restfp(&u.u_fps);
#endif
}

/*
 * nonexistent system call-- set fatal error code.
 */
nosys()
{
	u.u_error = EINVAL;
}
