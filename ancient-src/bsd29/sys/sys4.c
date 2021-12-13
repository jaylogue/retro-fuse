#include "bsd29adapt.h"

/*
 *	SCCS id	@(#)sys4.c	2.1 (Berkeley)	9/4/83
 */

#include "bsd29/include/sys/param.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/dir.h>
#include <bsd29/include/sys/user.h>
#include <bsd29/include/sys/reg.h>
#include <bsd29/include/sys/inode.h>
/* UNUSED #include <sys/proc.h> */
/* UNUSED #include <sys/timeb.h> */
/* UNUSED #include <sys/quota.h> */
#ifdef	UCB_AUTOBOOT
#include <sys/reboot.h>
#include <sys/filsys.h>
#endif

/*
 * Everything in this file is a routine implementing a system call.
 */

#if UNUSED
/*
 * return the current time (old-style entry)
 */
gtime()
{
	u.u_r.r_time = time;
}
#endif /* UNUSED */

#if UNUSED
/*
 * New time entry-- return TOD with milliseconds, timezone,
 * DST flag
 */
ftime()
{
	struct a {
		struct	timeb *tp;
	};
	struct timeb t;
	register unsigned ms;

	(void) _spl7();
	t.time = time;
	ms = lbolt;
	(void) _spl0();
	if (ms > hz) {
		ms -= hz;
		t.time++;
	}
	t.millitm = (1000*ms)/hz;
	t.timezone = timezone;
	t.dstflag = dstflag;
	if (copyout((caddr_t)&t, (caddr_t)(((struct a *) u.u_ap)->tp), sizeof(t)) < 0)
		u.u_error = EFAULT;
}
#endif /* UNUSED */

#if UNUSED
/*
 * Set the time
 */
stime()
{
	register struct a {
		time_t	time;
	} *uap;

	if (suser()) {
		uap = (struct a *)u.u_ap;
		bootime += uap->time - time;
		time = uap->time;
	}
}
#endif /* UNUSED */

#if UNUSED
setuid()
{
	register uid;
	struct a {
		int	uid;
	};

	uid = ((struct a *) u.u_ap)->uid;
	if (u.u_ruid == uid || u.u_uid == uid || suser()) {
		u.u_uid = uid;
		u.u_procp->p_uid = uid;
		u.u_ruid = uid;
	}
}
#endif /* UNUSED */

#if UNUSED
getuid()
{
	u.u_r.r_val1 = u.u_ruid;
	u.u_r.r_val2 = u.u_uid;
}
#endif /* UNUSED */

#if UNUSED
setgid()
{
	register gid;
	struct a {
		int	gid;
	};

	gid = ((struct a *) u.u_ap)->gid;
	if (u.u_rgid == gid || u.u_gid == gid || suser()) {
		u.u_gid = gid;
		u.u_rgid = gid;
	}
}
#endif /* UNUSED */

#if UNUSED
getgid()
{
	u.u_r.r_val1 = u.u_rgid;
	u.u_r.r_val2 = u.u_gid;
}
#endif /* UNUSED */

#if UNUSED
getpid()
{
	u.u_r.r_val1 = u.u_procp->p_pid;
	u.u_r.r_val2 = u.u_procp->p_ppid;
}
#endif /* UNUSED */

#if UNUSED
nice()
{
	register oldnice, newnice;
	register struct a {
		int	niceness;
	} *uap;

	uap = (struct a *)u.u_ap;

	oldnice = u.u_procp->p_nice;
	newnice = oldnice + uap->niceness;
	if (newnice >= 2 * NZERO)
		newnice = 2 * NZERO - 1;
	if (newnice < 0)
		newnice = 0;
	if (newnice < oldnice && !suser())	/* tsk tsk */
		newnice = oldnice;
	u.u_procp->p_nice = newnice;
}
#endif /* UNUSED */

/*
 * Unlink system call.
 * Hard to avoid races here, especially
 * in unlinking directories.
 */
void
unlink()
{
	register struct inode *ip, *pp;

#ifndef	UCB_SYMLINKS
	pp = namei(uchar, DELETE);
#else
	pp = namei(uchar, DELETE, 0);
#endif
	if (pp == NULL)
		return;
	/*
	 * Check for unlink(".")
	 * to avoid hanging on the iget
	 */
	if (pp->i_number == u.u_dent.d_ino) {
		ip = pp;
		ip->i_count++;
	} else
		ip = iget(pp->i_dev, u.u_dent.d_ino);
	if (ip == NULL)
		goto out1;
#ifdef UCB_QUOTAS
	/*
	 * Only superuser can unlink directories or a quota
	 */
	if (((ip->i_mode & IFMT) == IFDIR || isquot(ip)) && !suser())
#else
	if ((ip->i_mode&IFMT)==IFDIR && !suser())
#endif
		goto out;
	/*
	 * Don't unlink a mounted file.
	 */
	if (ip->i_dev != pp->i_dev) {
		u.u_error = EBUSY;
		goto out;
	}
	if (ip->i_flag&ITEXT)
		xrele(ip);	/* try once to free text */
	if (ip->i_flag&ITEXT && ip->i_nlink==1) {
		u.u_error = ETXTBSY;
		goto out;
	}
	/*
	 * sticky bit on a directory means only
	 * owner or super-user may remove file - for /tmp, etc.
	 */
	if ((pp->i_mode & ISVTX) && !own(ip))
		goto out;
	u.u_offset -= sizeof(struct direct);
	u.u_base = (caddr_t)&u.u_dent;
	u.u_count = sizeof(struct direct);
	u.u_dent.d_ino = 0;
	writei(pp);
	ip->i_nlink--;
#ifdef UCB_QUOTAS
	/*
	 * make unlinks force deallocation of blocks in quota
	 */
	if (ip->i_nlink <= 0)
		qcopy(pp, ip);
#endif
	ip->i_flag |= ICHG;

out:
	iput(ip);
out1:
	iput(pp);
}
#if UNUSED
chdir()
{
	chdirec(&u.u_cdir);
}
#endif /* UNUSED */

#if UNUSED
chroot()
{
	if (suser())
		chdirec(&u.u_rdir);
}
#endif /* UNUSED */

#if UNUSED
chdirec(ipp)
register struct inode **ipp;
{
	register struct inode *ip;

#ifndef	UCB_SYMLINKS
	ip = namei(uchar, LOOKUP);
#else
	ip = namei(uchar, LOOKUP, 1);
#endif
	if (ip == NULL)
		return;
	if ((ip->i_mode&IFMT) != IFDIR) {
		u.u_error = ENOTDIR;
		goto bad;
	}
	if (access(ip, IEXEC))
		goto bad;
	prele(ip);
	if (*ipp) {
		plock(*ipp);
		iput(*ipp);
	}
	*ipp = ip;
	return;

bad:
	iput(ip);
}
#endif /* UNUSED */

void
chmod()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int16_t	fmode;
	} *uap;

#ifndef	UCB_SYMLINKS
	if ((ip = owner()) == NULL)
#else
	if ((ip = owner(1)) == NULL)
#endif
		return;
	ip->i_mode &= ~07777;
	uap = (struct a *)u.u_ap;
	/* relax access controls on setting the sticky bit to
	 * match modern norms. */
#if UNUSED
	if (u.u_uid)
		uap->fmode &= ~ISVTX;
#endif
	ip->i_mode |= uap->fmode & 07777;
	ip->i_flag |= ICHG;
	if (ip->i_flag & ITEXT && (ip->i_mode & ISVTX)==0)
		xrele(ip);
	iput(ip);
}

#if UNUSED
chown()
{
	register struct inode *ip;
	register struct a {
		char	*fname;
		int	uid;
		int	gid;
	} *uap;

#ifndef	UCB_SYMLINKS
	if (!suser() || (ip = namei(uchar, LOOKUP)) == NULL)
#else
	if (!suser() || (ip = namei(uchar, LOOKUP, 0)) == NULL)
#endif
		return;
	uap = (struct a *)u.u_ap;
	ip->i_uid = uap->uid;
	ip->i_gid = uap->gid;
	ip->i_flag |= ICHG;
	iput(ip);
}
#endif /* UNUSED */

#if UNUSED
ssig()
{
#ifdef	MENLO_JCL
	register int (*f)();
	register struct a {
		int	signo;
		int	(*fun)();
	} *uap;
	register struct proc *p = u.u_procp;
	register a;
	long sigmask;

	uap = (struct a *)u.u_ap;
	a = uap->signo & SIGNUMMASK;
	f = uap->fun;
	if (a<=0 || a>=NSIG || a==SIGKILL || a==SIGSTOP ||
	    a==SIGCONT && (f == SIG_IGN || f == SIG_HOLD)) {
		u.u_error = EINVAL;
		return;
	}
	if ((uap->signo &~ SIGNUMMASK) || (f != SIG_DFL && f != SIG_IGN &&
	    SIGISDEFER(f)))
		p->p_flag |= SNUSIG;
	/* 
	 * Don't clobber registers if we are to simulate
	 * a ret+rti.
	 */
	if ((uap->signo&SIGDORTI) == 0)
		u.u_r.r_val1 = (int)u.u_signal[a];
	/*
	 * Change setting atomically.
	 */
	(void) _spl6();
	sigmask = 1L << (a-1);
	if (f == SIG_IGN)
		p->p_sig &= ~sigmask;		/* never to be seen again */
	u.u_signal[a] = f;
	if (f != SIG_DFL && f != SIG_IGN && f != SIG_HOLD)
		f = SIG_CATCH;
	if ((int)f & 1)
		p->p_siga0 |= sigmask;
	else
		p->p_siga0 &= ~sigmask;
	if ((int)f & 2)
		p->p_siga1 |= sigmask;
	else
		p->p_siga1 &= ~sigmask;
	(void) _spl0();
	/*
	 * Now handle options.
	 */
	if (uap->signo & SIGDOPAUSE) {
		/*
		 * Simulate a PDP11 style wait instrution which
		 * atomically lowers priority, enables interrupts
		 * and hangs.
		 */
		pause();
		/*NOTREACHED*/
	}
	if (uap->signo & SIGDORTI)
		u.u_eosys = SIMULATERTI;
#else
	register a;
	struct a {
		int	signo;
		int	fun;
	} *uap;

	uap = (struct a *)u.u_ap;
	a = uap->signo;
	if (a<=0 || a>=NSIG || a==SIGKILL) {
		u.u_error = EINVAL;
		return;
	}
	u.u_r.r_val1 = u.u_signal[a];
	u.u_signal[a] = uap->fun;
	u.u_procp->p_sig &= ~(1<<(a-1));
#endif
}
#endif /* UNUSED */

#if UNUSED
kill()
{
#ifdef	MENLO_JCL
	register struct proc *p;
	register a, sig;
	register struct a {
		int	pid;
		int	signo;
	} *uap;
	int f, priv;

	f = 0;
	priv = 0;
	uap = (struct a *)u.u_ap;
	a = uap->pid;
	sig = uap->signo;
	if (sig < 0)
		/*
		 * A negative signal means send to process group.
		 */
		uap->signo = -uap->signo;
	if (uap->signo == 0 || uap->signo > NSIG) {
		u.u_error = EINVAL;
		return;
	}
	if (a > 0 && sig > 0) {
		for(p = &proc[0]; p <= maxproc; p++)
			if ( p->p_pid == a)
				goto found;
		p = 0;
found:
		if (p == 0 || u.u_uid && u.u_uid != p->p_uid) {
			u.u_error = ESRCH;
			return;
		}
		psignal(p, uap->signo);
		return;
	}
	if (a==-1 && u.u_uid==0) {
		priv++;
		a = 0;
		sig = -1;		/* like sending to pgrp */
	} else if (a==0) {
		/*
		 * Zero process id means send to my process group.
		 */
		sig = -1;
		a = u.u_procp->p_pgrp;
		if (a == 0) {
			u.u_error = EINVAL;
			return;
		}
	}
	for(p = &proc[0]; p <= maxproc; p++) {
		if (p->p_stat == NULL)
			continue;
		if (sig > 0) {
			if (p->p_pid != a)
				continue;
		} else if (p->p_pgrp!=a && priv==0 || p<=&proc[1] ||
		    (priv && p==u.u_procp))
			continue;
		if (u.u_uid != 0 && u.u_uid != p->p_uid &&
		    (uap->signo != SIGCONT || !inferior(p)))
			continue;
		f++;
		psignal(p, uap->signo);
	}
#else
	register struct proc *p, *q;
	register a;
	register struct a {
		int	pid;
		int	signo;
	} *uap;
	int f, priv;

	f = 0;
	uap = (struct a *)u.u_ap;
	a = uap->pid;
	priv = 0;
	if (a==-1 && u.u_uid==0) {
		priv++;
		a = 0;
	}
	q = u.u_procp;
	for(p = &proc[0]; p <= maxproc; p++) {
		if (p->p_stat == NULL)
			continue;
		if (a != 0 && p->p_pid != a)
			continue;
		if (a==0 && ((p->p_pgrp!=q->p_pgrp&&priv==0) || p<=&proc[1]))
			continue;
		if (priv && p==u.u_procp)
			continue;
		if (u.u_uid != 0 && u.u_uid != p->p_uid)
			continue;
		f++;
		psignal(p, uap->signo);
	}
#endif
	if (f == 0)
		u.u_error = ESRCH;
}
#endif /* UNUSED */

#if UNUSED
times()
{
	register struct a {
		time_t	(*times)[4];
	} *uap;

	uap = (struct a *)u.u_ap;
	if (copyout((caddr_t)&u.u_utime, (caddr_t)uap->times,
	    sizeof(*uap->times)) < 0)
		u.u_error = EFAULT;
}
#endif /* UNUSED */

#if UNUSED
profil()
{
	register struct a {
		short	*bufbase;
		unsigned bufsize;
		unsigned pcoffset;
		unsigned pcscale;
	} *uap;

	uap = (struct a *)u.u_ap;
	u.u_prof.pr_base = uap->bufbase;
	u.u_prof.pr_size = uap->bufsize;
	u.u_prof.pr_off = uap->pcoffset;
	u.u_prof.pr_scale = uap->pcscale;
}
#endif /* UNUSED */

#if UNUSED
/*
 * alarm clock signal
 */
alarm()
{
	register struct proc *p;
	register c;
	struct a {
		int	deltat;
	};

	p = u.u_procp;
	c = p->p_clktim;
	p->p_clktim = ((struct a *) u.u_ap)->deltat;
	u.u_r.r_val1 = c;
}
#endif /* UNUSED */

#if UNUSED
/*
 * indefinite wait.
 * no one should wakeup(&u)
 */
pause()
{

	for(;;)
		sleep((caddr_t)&u, PSLEP);
}
#endif /* UNUSED */

#if UNUSED
/*
 * mode mask for creation of files
 */
umask()
{
	struct a {
		int	mask;
	};

	u.u_r.r_val1 = u.u_cmask;
	u.u_cmask = ((struct a *)u.u_ap)->mask & 0777;
}
#endif /* UNUSED */

#if UNUSED
/*
 * Set IUPD and IACC times on file.
 * Can't set ICHG.
 */
utime()
{
	struct a {
		char	*fname;
		time_t	*tptr;
	};
	register struct inode *ip;
	time_t tv[2];

#ifndef	UCB_SYMLINKS
	if ((ip = owner()) == NULL)
#else
	if ((ip = owner(1)) == NULL)
#endif
		return;
	if (copyin((caddr_t)((struct a *) u.u_ap)->tptr, (caddr_t)tv, sizeof(tv))) {
		u.u_error = EFAULT;
	} else {
		ip->i_flag |= IACC|IUPD|ICHG;
#ifdef UCB_FSFIX
		iupdat(ip, &tv[0], &tv[1], 0);
#else
		iupdat(ip, &tv[0], &tv[1]);
#endif
	}
	iput(ip);
}
#endif /* UNUSED */

#if UNUSED
#ifdef CGL_RTP
/*
 * make/unmake this process into a real time one.
 */
rtp()
{
	register struct proc *p;
	struct a {
		int flag;
	};

	if (u.u_gid != 0 && !suser()) {
		u.u_error = EPERM;
		return;
	}
	p = u.u_procp;
	if (((struct a *) u.u_ap)->flag) {
		if (rtpp != NULL) {
			u.u_error = EBUSY;	/* exclusive use error */
			return;
		}
		p->p_flag |= SULOCK;
		rtpp = p;
	} else {
		p->p_flag &= ~SULOCK;
		rtpp = NULL;
	}
}
#endif
#endif /* UNUSED */

#if UNUSED
#ifdef	MENLO_JCL
/*
 * Setpgrp on specified process and its descendants.
 * Pid of zero implies current process.
 * Pgrp of zero is getpgrp system call returning
 * current process group.
 */
setpgrp()
{
	register struct proc *top;
	register struct a {
		int	pid;
		int	pgrp;
	} *uap;

	uap = (struct a *)u.u_ap;
	if (uap->pid==0)
		top = u.u_procp;
	else {
		for(top = &proc[0]; top <= maxproc; top++)
			if ( uap->pid == top->p_pid)
				goto found1;
		top = 0;
found1:
		if (top == 0) {
			u.u_error = ESRCH;
			return;
		}
	}
	if (uap->pgrp <= 0) {
		u.u_r.r_val1 = top->p_pgrp;
		return;
	}
	if (top->p_uid != u.u_uid && u.u_uid && !inferior(top))
		u.u_error = EPERM;
	else
		top->p_pgrp = uap->pgrp;
}
#endif /* UNUSED */

#if UNUSED
spgrp(top, npgrp)
register struct proc *top;
{
	register struct proc *pp, *p;
	int f = 0;

	for (p = top; npgrp == -1 || u.u_uid == p->p_uid ||
	    !u.u_uid || inferior(p); p = pp) {
		if (npgrp == -1) {
#define	bit(a)	(1L<<(a-1))
			p->p_sig &= ~(bit(SIGTSTP)|bit(SIGTTIN)|bit(SIGTTOU));
			p->p_flag |= SDETACH;
		} else
			p->p_pgrp = npgrp;
		f++;
		/*
		 * Search for children.
		 */
		for (pp = &proc[0]; pp <= maxproc; pp++)
			if (pp->p_pptr == p)
				goto cont;
		/*
		 * Search for siblings.
		 */
		for (; p != top; p = p->p_pptr)
			for (pp = p + 1; pp <= maxproc; pp++)
				if (pp->p_pptr == p->p_pptr)
					goto cont;
		break;
	cont:
		;
	}
	return (f);
}
#endif /* UNUSED */

#if UNUSED
/*
 * Is p an inferior of the current process?
 */
inferior(p)
register struct proc *p;
{

	for (; p != u.u_procp; p = p->p_pptr)
		if (p <= &proc[1])
			return (0);
	return (1);
}
#endif /* UNUSED */

#endif

#if UNUSED
#ifdef	UCB_AUTOBOOT
reboot()
{
	register struct filsys *fp;
	struct a {
		dev_t	dev;
		int	opt;
	};

	if (suser()) {
		/*
		 *  Force the root filesystem's superblock to be updated,
		 *  so the date will be as current as possible after
		 *  rebooting.
		 */
		if (fp = getfs(rootdev))
			fp->s_fmod = 1;
		boot((struct a *)u.u_ap->dev,(struct a *)u.u_ap->opt);
	}
}
#endif
#endif /* UNUSED */
