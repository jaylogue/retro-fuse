/*
 *	SCCS id	@(#)signojcl.c	2.1 (Berkeley)	9/4/83
 */

#include "param.h"
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/inode.h>
#include <sys/reg.h>
#include <sys/text.h>
#include <sys/seg.h>
#ifdef UCB_METER
#include <sys/vm.h>
#endif


/*
 * Priority for tracing
 */
#define	IPCPRI	PZERO

/*
 * Tracing variables.
 * Used to pass trace command from
 * parent to child being traced.
 * This data base cannot be
 * shared and is locked
 * per user.
 */
struct
{
	int	ip_lock;
	int	ip_req;
	int	*ip_addr;
	int	ip_data;
} ipc;

/*
 * Send the specified signal to
 * all processes with 'pgrp' as
 * process group.
 * Called by tty*.c for quits and
 * interrupts.
 */
gsignal(pgrp, sig)
register pgrp;
{
	register struct proc *p;

	if(pgrp == 0)
		return;
	for(p = &proc[0]; p <= maxproc; p++)
		if(p->p_pgrp == pgrp)
			psignal(p, sig);
}

/*
 * Send the specified signal to
 * the specified process.
 */
psignal(p, sig)
register struct proc *p;
register sig;
{

	if((unsigned)sig >= NSIG)
		return;
	if(sig)
		p->p_sig |= 1<<(sig-1);
	if(p->p_pri > PUSER)
		p->p_pri = PUSER;
	if(p->p_stat == SSLEEP && p->p_pri > PZERO)
		setrun(p);
}

/*
 * Returns true if the current
 * process has a signal to process.
 * This is asked at least once
 * each time a process enters the
 * system.
 * A signal does not do anything
 * directly to a process; it sets
 * a flag that asks the process to
 * do something to itself.
 */
issig()
{
	register n;
	register struct proc *p;

	p = u.u_procp;
	while(p->p_sig) {
		n = fsig(p);
		if(((int)u.u_signal[n]&1) == 0 || (p->p_flag&STRC))
			return(n);
		p->p_sig &= ~(1<<(n-1));
	}
	return(0);
}

/*
 * Enter the tracing STOP state.
 * In this state, the parent is
 * informed and the process is able to
 * receive commands from the parent.
 */
stop()
{
	register struct proc *pp, *cp;

loop:
	cp = u.u_procp;
	if(cp->p_ppid != 1)
	for (pp = &proc[0]; pp <= maxproc; pp++)
		if (pp->p_pid == cp->p_ppid) {
			wakeup((caddr_t)pp);
			cp->p_stat = SSTOP;
			swtch();
			if ((cp->p_flag&STRC)==0 || procxmt())
				return;
			goto loop;
		}
	exit(fsig(u.u_procp));
}

/*
 * Perform the action specified by
 * the current signal.
 * The usual sequence is:
 *	if(issig())
 *		psig();
 */
psig()
{
	register n, p;
	register struct proc *rp;

	rp = u.u_procp;
#ifndef	NONFP
	if (u.u_fpsaved==0) {
		savfp(&u.u_fps);
		u.u_fpsaved = 1;
	}
	if (rp->p_flag&STRC)
		stop();
#else
	/* allow normal action on SIGILL even if traced */
	n = fsig(rp);
	if ((rp->p_flag&STRC) && (n!=SIGILL || u.u_signal[SIGILL]==0))
		stop();
#endif
	while(n = fsig(rp)) {
		rp->p_sig &= ~(1<<(n-1));
		if((p=u.u_signal[n]) != 0) {
			u.u_error = 0;
			if (u.u_signal[n] == 1)
				continue;
			if(n != SIGILL && n != SIGTRAP)
				u.u_signal[n] = 0;
			sendsig((caddr_t)p);
		} else {
			switch(n) {

			case SIGQUIT:
			case SIGILL:
			case SIGTRAP:
			case SIGIOT:
			case SIGEMT:
			case SIGFPE:
			case SIGBUS:
			case SIGSEGV:
			case SIGSYS:
				if(core())
					n += 0200;
			}
			exit(n);
		}
	}
}

/*
 * This table defines the order in which signals
 * will be reflected to the user process.  Signals
 * later in the list will be stacked after (processed
 * before) signals earlier in the list.  The value
 * contained in the list is the signal number.
 */

char	siglist[] = {
	SIGKILL,	/* kill				*/
	SIGTERM,	/* terminate			*/
	SIGILL,		/* illegal instruction		*/
	SIGTRAP,	/* trace trap			*/
	SIGIOT,		/* iot				*/
	SIGEMT,		/* emt				*/
	SIGBUS,		/* bus error			*/
	SIGSEGV,	/* segmentation error		*/
	SIGSYS,		/* invalid system call (unused)	*/
	SIGQUIT,	/* quit				*/
	SIGINT,		/* interrupt			*/
	SIGHUP,		/* hangup			*/
	SIGPIPE,	/* broken pipe			*/
	SIGALRM,	/* alarm clock			*/
	SIGFPE,		/* floating exception		*/
	16,		/* unassigned			*/
	 0,		/* end of list			*/
};

/*
 * find the highest-priority signal in
 * bit-position representation in p_sig.
 */
fsig(p)
struct proc *p;
{
	register char *cp;
	register psig;

	psig = p->p_sig;
	for(cp=siglist; *cp; cp++)
		if(psig & (1 << (*cp-1)))
			return(*cp);
	return(0);
}

/*
 * Create a core image on the file "core"
 * If you are looking for protection glitches,
 * there are probably a wealth of them here
 * when this occurs to a suid command.
 *
 * It writes USIZE block of the
 * user.h area followed by the entire
 * data+stack segments.
 */
core()
{
	register struct inode *ip;
	register unsigned s;
	extern schar();

	u.u_error = 0;

	/*
	 * Disallow dump if setuid/setgid.
	 */
	if (u.u_gid != u.u_rgid || u.u_uid != u.u_ruid)
		return(0);

	u.u_dirp = "core";
#ifndef	UCB_SYMLINKS
	ip = namei(schar, CREATE);
#else
	ip = namei(schar, CREATE, 1);
#endif
	if(ip == NULL) {
		if(u.u_error)
			return(0);
		ip = maknode(0666);
		if (ip == NULL)
			return(0);
	}
	/*
	 * allow dump only if permissions allow, "core" is
	 * regular file and has exactly one link
	 */
	if (!access(ip, IWRITE) &&
	   (ip->i_mode&IFMT) == IFREG &&
	   ip->i_nlink == 1) {
		itrunc(ip);
		u.u_offset = 0;
		u.u_base = (caddr_t)&u;
		u.u_count = ctob(USIZE);
		u.u_segflg = 1;
		writei(ip);
#ifdef	VIRUS_VFORK
		s = u.u_dsize;
		estabur((unsigned)0, s, u.u_ssize, 0, RO);
		u.u_base = 0;
		u.u_count = ctob(s);
		u.u_segflg = 0;
		writei(ip);
		s = u.u_ssize;
		u.u_base = -(ctob(s));
		u.u_count = ctob(s);
		writei(ip);
#else
		s = u.u_procp->p_size - USIZE;
		estabur((unsigned)0, s, (unsigned)0, 0, RO);
		u.u_base = 0;
		u.u_count = ctob(s);
		u.u_segflg = 0;
		writei(ip);
#endif
	}
	iput(ip);
	return(u.u_error==0);
}

/*
 * grow the stack to include the SP
 * true return if successful.
 */

grow(sp)
unsigned sp;
{
	register si;
#ifndef	VIRUS_VFORK
	register i;
#endif
	register struct proc *p;
	register a;

	if(sp >= -ctob(u.u_ssize))
		return(0);
	si = (-sp)/ctob(1) - u.u_ssize + SINCR;
	/*
	 * Round the increment back to a segment boundary if necessary.
	 */
	if (ctos(si + u.u_ssize) > ctos(((-sp)+ctob(1)-1) / ctob(1)))
		si = stoc(ctos(((-sp)+ctob(1)-1) / ctob(1))) - u.u_ssize;
	if(si <= 0)
		return(0);
	if(estabur(u.u_tsize, u.u_dsize, u.u_ssize+si, u.u_sep, RO))
		return(0);
	p = u.u_procp;
#ifdef	VIRUS_VFORK
	/*
	 *  expand will put the stack in the right place;
	 *  no copy required here.
	 */
	expand(u.u_ssize+si,S_STACK);
	u.u_ssize += si;
	clear(u.u_procp->p_saddr, si);

#else	VIRUS_VFORK
	expand(p->p_size+si);
	a = u.u_procp->p_addr + USIZE + u.u_dsize;
	i = u.u_ssize;
	while (i >= si) {
		i -= si;
		copy(a+i, a+i+si, si);
	}
	copy(a, a+si, i);
	clear(a, si);
	u.u_ssize += si;
#endif	VIRUS_VFORK
	return(1);
}

/*
 * sys-trace system call.
 */
ptrace()
{
	register struct proc *p;
	register struct a {
		int	data;
		int	pid;
		int	*addr;
		int	req;
	} *uap;

	uap = (struct a *)u.u_ap;
	if (uap->req <= 0) {
		u.u_procp->p_flag |= STRC;
		return;
	}
	for (p = proc; p <= maxproc; p++) 
		if (p->p_stat == SSTOP && p->p_pid == uap->pid
		    && p->p_ppid == u.u_procp->p_pid) {
		while (ipc.ip_lock)
			sleep((caddr_t)&ipc, IPCPRI);
		ipc.ip_lock = p->p_pid;
		ipc.ip_data = uap->data;
		ipc.ip_addr = uap->addr;
		ipc.ip_req = uap->req;
		p->p_flag &= ~SWTED;
		setrun(p);
		while (ipc.ip_req > 0)
			sleep((caddr_t)&ipc, IPCPRI);
		u.u_r.r_val1 = ipc.ip_data;
		if (ipc.ip_req < 0)
			u.u_error = EIO;
		ipc.ip_lock = 0;
		wakeup((caddr_t)&ipc);
		return;
	}

	u.u_error = ESRCH;
}

/*
 * Code that the child process
 * executes to implement the command
 * of the parent process in tracing.
 */
procxmt()
{
	register int i;
	register *p;
	register struct text *xp;

	if (ipc.ip_lock != u.u_procp->p_pid)
		return(0);
#ifdef UCB_METER
	u.u_procp->p_slptime = 0;
#endif
	i = ipc.ip_req;
	ipc.ip_req = 0;
	wakeup((caddr_t)&ipc);
	switch (i) {

	/* read user I */
	case 1:
#ifndef	NONSEPARATE		/* I and D are the same if nonsep */
		if (fuibyte((caddr_t)ipc.ip_addr) == -1)
			goto error;
		ipc.ip_data = fuiword((caddr_t)ipc.ip_addr);
		break;

#endif
	/* read user D */
	case 2:
		if (fubyte((caddr_t)ipc.ip_addr) == -1)
			goto error;
		ipc.ip_data = fuword((caddr_t)ipc.ip_addr);
		break;

	/* read u */
	case 3:
		i = (int)ipc.ip_addr;
		if (i<0 || i >= ctob(USIZE))
			goto error;
		ipc.ip_data = ((physadr)&u)->r[i>>1];
		break;

	/* write user I */
	/* Must set up to allow writing */
	case 4:
		/*
		 * If text, must assure exclusive use
		 */
		if (xp = u.u_procp->p_textp) {
			if (xp->x_count!=1 || xp->x_iptr->i_mode&ISVTX)
				goto error;
			xp->x_iptr->i_flag &= ~ITEXT;
		}
		estabur(u.u_tsize, u.u_dsize, u.u_ssize, u.u_sep, RW);
		i = suiword((caddr_t)ipc.ip_addr, 0);
		suiword((caddr_t)ipc.ip_addr, ipc.ip_data);
		estabur(u.u_tsize, u.u_dsize, u.u_ssize, u.u_sep, RO);
		if (i<0)
			goto error;
		if (xp)
			xp->x_flag |= XWRIT;
		break;

	/* write user D */
	case 5:
		if (suword((caddr_t)ipc.ip_addr, 0) < 0)
			goto error;
		suword((caddr_t)ipc.ip_addr, ipc.ip_data);
		break;

	/* write u */
	case 6:
		i = (int)ipc.ip_addr;
		p = (int *)&((physadr)&u)->r[i>>1];
#ifndef	NONFP
		if (p >= (int *)&u.u_fps && p < (int *)&u.u_fps.u_fpregs[6])
			goto ok;
#endif
		for (i=0; i<8; i++)
			if (p == &u.u_ar0[regloc[i]])
				goto ok;
		if (p == &u.u_ar0[RPS]) {
			ipc.ip_data |= PS_CURMOD | PS_PRVMOD;	/* user space */
			ipc.ip_data &= ~PS_USERCLR;		/* priority 0 */
			goto ok;
		}
#ifdef	MENLO_OVLY
		if ((p == &u.u_ovdata.uo_curov) && ((ipc.ip_data >= 0) &&
		    (ipc.ip_data <= NOVL) && u.u_ovdata.uo_ovbase)) {
			*p = ipc.ip_data;
			choverlay(RO);
			break;
		}
#endif
		goto error;

	ok:
		*p = ipc.ip_data;
		break;

	/* set signal and continue */
	/*  one version causes a trace-trap */
	case 9:
		u.u_ar0[RPS] |= PS_T;
		/* FALL THROUGH TO ... */
	case 7:
		if ((int)ipc.ip_addr != 1)
			u.u_ar0[PC] = (int)ipc.ip_addr;
		u.u_procp->p_sig = 0;
		if (ipc.ip_data)
			psignal(u.u_procp, ipc.ip_data);
		return(1);

	/* force exit */
	case 8:
		exit(fsig(u.u_procp));
		/*NOTREACHED*/

	default:
	error:
		ipc.ip_req = -1;
	}
	return(0);
}
