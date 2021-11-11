/*
 *	SCCS id	@(#)syslocal.c	2.1 (Berkeley)	9/4/83
 */

#include "param.h"
#include <sys/dir.h>
#include <sys/proc.h>
#include <sys/seg.h>
#include <sys/user.h>
#include <sys/systm.h>
#include <sys/inode.h>
#include <sys/ino.h>
#include <sys/file.h>
#include <sys/conf.h>
#ifdef	UCB_QUOTAS
#include <sys/quota.h>
#include <sys/qstat.h>
#include <sys/buf.h>
#endif
#ifdef	UCB_VHANGUP
#include <sys/tty.h>
#endif
#include <sys/autoconfig.h>
#ifdef  UCB_NET
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/ubavar.h>
#include <sys/map.h>
#include "../net/if.h"
#include "../net/in_systm.h"
#endif

/*
 * These routines implement local system calls
 */

/*
 * nostk -- Set up the process to have no stack segment.
 *	    The process is responsible for the management
 *	    of its own stack, and can thus use the full
 *	    64k byte address space.
 */

nostk()
{
#ifndef	VIRUS_VFORK
	register size;

	size = u.u_procp->p_size - u.u_ssize;
	if(estabur(u.u_tsize, u.u_dsize, 0, u.u_sep, RO))
		return;
	u.u_ssize = 0;
	expand(size);
#else
	if(estabur(u.u_tsize, u.u_dsize, 0, u.u_sep, RO))
		return;
	expand(0,S_STACK);
	u.u_ssize = 0;
#endif
}

#if	!defined(NONSEPARATE) && defined(NONFP)
/*
 * fetchi -- fetch from user I space
 *	     required because the mfpi instruction
 *	     does not work if current and previous
 *	     are both user.
 */

fetchi()
{
	struct a {
		caddr_t iaddr;
	};

	u.u_r.r_val1 = fuiword(((struct a *)u.u_ap)->iaddr);
}
#endif

#ifdef UCB_QUOTAS
/*
 * quota -- establish or change the quota on a directory
 */
quota()
{
	register struct inode *ip;
	register struct a {
		char	*name;
		daddr_t	current;
		daddr_t	max;
	} *uap;

	if (!suser())
		return;
#ifndef	UCB_SYMLINKS
	ip = namei(uchar, LOOKUP);
#else
	ip = namei(uchar, LOOKUP, 1);
#endif
	if (ip == NULL)
		return;
	else
		if ((ip->i_mode & IFMT) != IFQUOT) {
			u.u_error = EACCES;
			return;
		}
	/*
	 * Round the current up to be a multiple of the
	 * counting unit.
	 */
	uap = (struct a *) u.u_ap;
#if	QCOUNT==2
	ip->i_un.i_qused = ((uap->current + QCOUNT - 1) >> 1) << 1;
#else
	ip->i_un.i_qused = (uap->current + QCOUNT - 1) / QCOUNT * QCOUNT;
#endif
	ip->i_un.i_qmax = uap->max;
	ip->i_flag |= IUPD|IACC;
	iput(ip);
}
#endif

#ifdef UCB_QUOTAS
/*
 * the qfstat system call.
 */
qfstat()
{
	register struct file  *fp;
	register struct a {
		int	fdes;
		struct	qstat *sb;
	} *uap;

	uap = (struct a *) u.u_ap;
	fp = getf(uap->fdes);
	if(fp == NULL)
		return;
	qstat1(fp->f_inode, uap->sb);
}

/*
 * the qstat system call.
 */
qstat()
{
	register struct inode *ip;
	struct a {
		char	*fname;
		struct	qstat *sb;
	};

#ifndef	UCB_SYMLINKS
	ip = namei(uchar, LOOKUP);
#else
	ip = namei(uchar, LOOKUP, 0);
#endif
	if(ip == (struct inode *) NULL)
		return;
	qstat1(ip, ((struct a *) u.u_ap)->sb);
	iput(ip);
}

/*
 * The basic routine for qfstat and qstat:
 * get the inode and pass all of it back.
 */
qstat1(ip, ub)
register struct inode *ip;
struct inode *ub;
{
	register struct dinode *dp;
	register struct buf *bp;
	struct qstat qs, *qp;

#ifdef UCB_FSFIX
	iupdat(ip, &time, &time, 0);
#else
	iupdat(ip, &time, &time);
#endif

	/*
	 * first copy from inode table
	 */
	qp = &qs;
	*((struct inode *) qp) = *ip;

	/*
	 * next the dates in the disk
	 */
	bp = bread(ip->i_dev, itod(ip->i_number));
	dp = (struct dinode *) mapin(bp);
	dp += itoo(ip->i_number);
	qs.qs_atime = dp->di_atime;
	qs.qs_mtime = dp->di_mtime;
	qs.qs_ctime = dp->di_ctime;
	mapout(bp);
	brelse(bp);
	if (copyout((caddr_t) & qs, (caddr_t) ub, sizeof qs) < 0)
		u.u_error = EFAULT;
}
#endif

#ifndef MENLO_JCL
/*
 * killpg -- send all processes the specified
 *	     process group the given signal.
 */

killpg()
{
	struct a {
		int	pgrp;
		int	sig;
	};

	killgrp(((struct a *) u.u_ap)->pgrp, ((struct a *) u.u_ap)->sig, 0);
}
#endif

#ifdef UCB_SUBM
/*
 * killbkg -- signal processes in the specified group that
 *	      have not been blessed by a submit call
 */

killbkg()
{
	struct a {
		int	pgrp;
		int	sig;
	};

	killgrp(((struct a *) u.u_ap)->pgrp, ((struct a *) u.u_ap)->sig, SSUBM);
}
#endif

#if	defined(UCB_SUBM) || !defined(MENLO_JCL)
/*
 * common code for killpg and killbkg
 *
 * if mask is non-zero, send signal
 * only to processes whose (p_flag & mask)
 * is zero.
 */

killgrp(pgrp, sig, mask)
register pgrp;
register sig;
{
	register struct proc *p;
	int count;

	if(!suser())
		return;
	count = 0;
	for(p = &proc[2]; p <= maxproc; p++) {
		if(p->p_stat == SZOMB || p->p_pgrp != pgrp)
			continue;

		/*
		 * include following if suser is immune
		 *
		if(p->p_uid == 0)
			continue;
		 *
		 */

		if(mask && (mask & p->p_flag)) {
			continue;
		}
		count++;
		psignal(p, sig);
	}
	if(count == 0)
		u.u_error = ESRCH;
}
#endif

#ifdef	UCB_SUBM
/*
 * submit -- mark the specified process to allow execution after logout
 */

submit()
{
	register struct proc *p;
#ifndef	MENLO_JCL
	register group;
#endif
	register pid;
	struct a {
		int	pid;
	};

	pid = ((struct a *) u.u_ap)->pid;
#ifndef	MENLO_JCL
	group = u.u_procp->p_pgrp;
#endif
	for(p = &proc[2]; p <= maxproc; p++)
		if(p->p_pid == pid) {
#ifndef	MENLO_JCL
			if(p->p_pgrp != group && !suser())
				return;
#else
			if (p->p_uid != u.u_uid && !suser())
				return;
#endif
			p->p_flag |= SSUBM;
			return;
		}
	u.u_error = ESRCH;
}
#endif	UCB_SUBM

#ifdef	UCB_LOGIN
/*
 * login -- mark a process as a login process,
 *	    and record accounting information.
 */
login()
{
	register i;
	struct a {
		int	tslot;
		char	crn[4];
	};

	if (suser()) {
		u.u_login = ((struct a *) u.u_ap)->tslot;
		for (i = 0; i < sizeof u.u_crn; i++)
			u.u_crn[i] = ((struct a *) u.u_ap)->crn[i];
	}
}
#endif

#ifndef MENLO_JCL
/*
 * establish a new process group
 */
setpgrp()
{
	register struct proc *pp;

	if (suser()) {
		pp = u.u_procp;
		pp->p_pgrp = pp->p_pid;
	}
}
#endif

#ifdef	UCB_LOAD
/*
 * gldav -- get the load averages
 */
gldav()
{
	extern short avenrun[];
	struct a {
		short	*ptr;
	};

	if (copyout( (caddr_t) avenrun, (caddr_t) (((struct a *) u.u_ap)->ptr),
	    3 * sizeof(short)) < 0)
		u.u_error = EFAULT;
}
#endif

#ifndef NONFP
/*
 * fperr - return floating point error registers
 */
fperr()
{
	u.u_r.r_val1 = u.u_fperr.f_fec;
	u.u_r.r_val2 = u.u_fperr.f_fea;
}
#endif

#ifdef	UCB_VHANGUP
/*
 * Revoke access to the current tty by all processes.
 * Used only by the super-user in init
 * to give ``clean'' terminals at login.
 */
vhangup()
{
	if (suser()) {
		if (u.u_ttyp == NULL)
			return;
		forceclose(u.u_ttyd);
		if ((u.u_ttyp->t_state) & ISOPEN)
			gsignal(u.u_ttyp->t_pgrp, SIGHUP);
	}
}

forceclose(dev)
dev_t	dev;
{
	register struct file *fp;
	register struct inode *ip;
	register int n = 0;

	for (fp = file; fp < fileNFILE; fp++) {
#ifdef  UCB_NET
		if (fp->f_flag & FSOCKET)
			continue;
#endif
		if (fp->f_count == 0)
			continue;
		ip = fp->f_inode;
		if ((ip->i_mode & IFMT) != IFCHR)
			continue;
		if (ip->i_un.i_rdev != dev)
			continue;
		fp->f_flag &= ~(FREAD | FWRITE);
		n++;
	}
	return (n);
}
#endif	UCB_VHANGUP

#ifdef	UCB_RENICE
/*
 * renice -- change the nice value of a process
 */
renice()
{
	register struct proc *p;
	register struct a {
		int pid;
		int nice;
	} *uap;

	uap = (struct a *) u.u_ap;

	for (p = &proc[2]; p <= maxproc; p++)
		if (p->p_pid == uap->pid) {
			if (suser()) {
				u.u_r.r_val1 = p->p_nice;
				p->p_nice = MAX(uap->nice, -127);
			}
			else if (p->p_uid == u.u_uid) {
				u.u_r.r_val1 = p->p_nice;
				p->p_nice = MIN(MAX(p->p_nice, uap->nice), 127);
				u.u_error = 0;
			}
			return;
		}
	u.u_error = ESRCH;
}
#endif	UCB_RENICE

int conf_int = CONF_MAGIC; /* Used to pass result from int service to probe() */

/*
 * Routine to allow user level code to call various internal
 * functions; in configuration it calls for the probe and
 * attach functions of the various drivers.
 */
ucall()
{
	register struct a {
		int priority;
		int (*routine)();
		int arg0;
		int arg1;
	} *uap;

	if (suser()) {
		uap = (struct a *) u.u_ap;
		(void) splx(uap->priority);
		u.u_r.r_val1 = (*uap->routine)(uap->arg0, uap->arg1);
		(void) _spl0();
	}
}

#ifdef  UCB_NET

extern	u_long LocalAddr;	/* Generic local net address	*/

int	nlbase;         /* net error log area in clicks */
int	nlsize = 01000;
int	nlclick, nlbyte;

int	netoff = 0;
int	protoslow;
int	protofast;
int	ifnetslow;
int	nselcoll;

/*
 * Initialize network code.  Called from main().
 */
netinit()
{
	extern struct uba_device ubdinit[];
	register struct uba_driver *udp;
	register struct uba_device *ui = &ubdinit;

	if (netoff)
		return;
	nlbase = nlclick = malloc(coremap, nlsize);  /* net error log */
	MAPSAVE();
	mbinit();
	for (ui = &ubdinit ; udp = ui->ui_driver ; ui++) {
		if (badaddr(ui->ui_addr, 2))
			continue;
		ui->ui_alive = 1;
		udp->ud_dinfo[ui->ui_unit] = ui;
		(*udp->ud_attach)(ui);
	}
#ifdef INET
	loattach();			/* XXX */
	ifinit();
	pfinit();			/* must follow interfaces */
#endif
	MAPREST();
}

/*
 * Entered via software interrupt vector at spl1.  Check netisr bit array
 * for tasks requesting service.
 */
netintr()
{
	int onetisr;
	mapinfo map;

	savemap(map);
	while (spl7(), (onetisr = netisr)) {
		netisr = 0;
		splnet();
		if (onetisr & (1 << NETISR_RAW))
			rawintr();
		if (onetisr & (1 << NETISR_IP))
			ipintr();
		if (protofast <= 0) {
			protofast = hz / PR_FASTHZ;
			pffasttimo();
		}
		if (protoslow <= 0) {
			protoslow = hz / PR_SLOWHZ;
			pfslowtimo();
		}
		if (ifnetslow <= 0) {
			ifnetslow = hz / IFNET_SLOWHZ;
			if_slowtimo();
		}
	}
	restormap(map);
}

int	nprint = 0;            /* enable nprintf */

/*
 * net printf.  prints to net log area in memory (nlbase, nlsize).
 */
nprintf(fmt, x1)
char *fmt;
unsigned x1;
{
	if (enprint)
		prf(fmt, &x1, 4);
}

/*
 * Select system call.
 */
select()
{
	register struct uap  {
		int	nfd;
		fd_set	*rp, *wp;
		long    timo;
	} *ap = (struct uap *)u.u_ap;
	fd_set rd, wr;
	int nfds = 0;
	long selscan();
	long readable = 0, writeable = 0;
	time_t t = time;
	int s, tsel, ncoll, rem;

	if (ap->nfd > NOFILE)
		ap->nfd = NOFILE;
	if (ap->nfd < 0) {
		u.u_error = EBADF;
		return;
	}
	if (ap->rp && copyin((caddr_t)ap->rp, (caddr_t)&rd, sizeof(fd_set)))
		return;
	if (ap->wp && copyin((caddr_t)ap->wp, (caddr_t)&wr, sizeof(fd_set)))
		return;

retry:
	ncoll = nselcoll;
	u.u_procp->p_flag |= SSEL;
	if (ap->rp)
		readable = selscan(ap->nfd, rd, &nfds, FREAD);
	if (ap->wp)
		writeable = selscan(ap->nfd, wr, &nfds, FWRITE);
	if (u.u_error)
		goto done;
	if (readable || writeable)
		goto done;
	rem = (ap->timo + 999) / 1000 - (time - t);
	if (ap->timo == 0 || rem <= 0)
		goto done;
	s = spl6();
	if ((u.u_procp->p_flag & SSEL) == 0 || nselcoll != ncoll) {
		u.u_procp->p_flag &= ~SSEL;
		splx(s);
		goto retry;
	}
	u.u_procp->p_flag &= ~SSEL;
	tsel = tsleep((caddr_t)&selwait, PZERO + 1, rem);
	splx(s);
	switch (tsel) {

		case TS_OK:
			goto retry;

		case TS_SIG:
			u.u_error = EINTR;
			return;

		case TS_TIME:
			break;
	}
done:
	rd.fds_bits[0] = readable;
	wr.fds_bits[0] = writeable;
	s = sizeof (fd_set);
	if (s * NBBY > ap->nfd)
		s = (ap->nfd + NBBY - 1) / NBBY;
	u.u_r.r_val1 = nfds;
	if (ap->rp)
		(void) copyout((caddr_t)&rd, (caddr_t)ap->rp, sizeof(fd_set));
	if (ap->wp)
		(void) copyout((caddr_t)&wr, (caddr_t)ap->wp, sizeof(fd_set));
}

long
selscan(nfd, fds, nfdp, flag)
	int nfd;
	fd_set fds;
	int *nfdp, flag;
{
	struct file *fp;
	struct inode *ip;
	long bits,res = 0;
	int i, able;
		
	bits = fds.fds_bits[0];
	while (i = ffs(bits)) {
		if (i >= nfd)
			break;
		bits &= ~(1L << (i - 1));
		fp = u.u_ofile[i - 1];
		if (fp == NULL) {
			u.u_error = EBADF;
			return (0);
		}
		if (fp->f_flag & FSOCKET)
			able = soselect(fp->f_socket, flag);
		else {
			ip = fp->f_inode;
			switch (ip->i_mode & IFMT) {

				case IFCHR:
					able = (*cdevsw[major(ip->i_un.i_rdev)].d_select)
						((int)ip->i_un.i_rdev, flag);
					break;

				case IFBLK:
				case IFREG:
				case IFDIR:
					able = 1;
					break;
			}
		}
		if (able) {
			res |= (1L << (i - 1));
			(*nfdp)++;
		}
	}
	return (res);
}

ffs(mask)
	long mask;
{
	register int i;
	register imask;

	if (mask == 0)
		return (0);

	imask = loint(mask);
	for (i = 1; i < 16; i++) {
		if (imask & 1)
			return (i);
		imask >>= 1;
	}
	imask = hiint(mask);
	for(; i <= 32; i++) {
		if (imask & 1)
			return (i);
		imask >>= 1;
	}
	return (0);     /* can't get here anyway! */
}

/*ARGSUSED*/
seltrue(dev, flag)
	dev_t dev;
	int flag;
{
	return (1);
}

selwakeup(p, coll)
	register struct proc *p;
	int coll;
{
	int s;

	if (coll) {
		nselcoll++;
		wakeup((caddr_t) &selwait);
	}
	s = spl6();
	if (p)
		if (p->p_wchan == (caddr_t) &selwait)
			setrun(p);
		else
			if (p->p_flag & SSEL)
				p->p_flag &= ~SSEL;
	splx(s);
}

char	hostname[32] = "hostnameunknown";
int	hostnamelen = 16;

gethostname()
{
	register struct a {
		char	*hostname;
		int	len;
	} *uap = (struct a *) u.u_ap;
	register int len;

	len = uap->len;
	if (len > hostnamelen)
		len = hostnamelen;
	if (copyout((caddr_t) hostname, (caddr_t) uap->hostname, len))
		u.u_error = EFAULT;
}

sethostname()
{
	register struct a {
		char	*hostname;
		int	len;
	} *uap = (struct a *) u.u_ap;

	if (suser()) {
		if (uap->len > sizeof (hostname) - 1) {
			u.u_error = EINVAL;
			return;
		}
		hostnamelen = uap->len;
		if (copyin((caddr_t) uap->hostname, hostname, uap->len + 1))
			u.u_error = EFAULT;
	}
}

/*
 * Sleep on chan at pri.
 * Return in no more than the indicated number of seconds.
 * (If seconds==0, no timeout implied)
 * Return	TS_OK if chan was awakened normally
 *		TS_TIME if timeout occurred
 *		TS_SIG if asynchronous signal occurred
 *
 * SHOULD HAVE OPTION TO SLEEP TO ABSOLUTE TIME OR AN
 * INCREMENT IN MILLISECONDS!
 */
tsleep(chan, pri, seconds)
	caddr_t chan;
	int pri, seconds;
{
	label_t lqsav;
	register struct proc *pp;
	register sec, n, rval;

	pp = u.u_procp;
	n = spl7();
	sec = 0;
	rval = 0;
	if (pp->p_clktim && pp->p_clktim < seconds)
		seconds = 0;
	if (seconds) {
		pp->p_flag |= STIMO;
		sec = pp->p_clktim - seconds;
		pp->p_clktim = seconds;
	}
	bcopy((caddr_t) u.u_qsav, (caddr_t) lqsav, sizeof (label_t));
	if (save(u.u_qsav))
		rval = TS_SIG;
	else {
		sleep(chan, pri);
		if ((pp->p_flag & STIMO) == 0 && seconds)
			rval = TS_TIME;
		else
			rval = TS_OK;
	}
	pp->p_flag &= ~STIMO;
	bcopy((caddr_t) lqsav, (caddr_t) u.u_qsav, sizeof (label_t));
	if (sec > 0)
		pp->p_clktim += sec;
	else
		pp->p_clktim = 0;
	splx(n);
	return (rval);
}

/*
 * Provide about n microseconds of delay
 */
delay(n)
long n;
{
	register hi,low;

	low = (n & 0177777);
	hi = n >> 16;
	if (hi == 0)
		hi = 1;
	do {
		do { } while (--low);
	} while(--hi);
}

/*
 * compare bytes; same result as VAX cmpc3.
 */
bcmp(s1, s2, n)
register char *s1, *s2;
register n;
{
	do
		if (*s1++ != *s2++)
			break;
	while (--n);

	return(n);
}

struct	vaxque {		/* queue format expected by VAX queue instr's */
	struct	vaxque	*vq_next;
	struct	vaxque	*vq_prev;
};

/*
 * Insert an entry onto queue.
 */
_insque(e,prev)
	register struct vaxque *e,*prev;
{
	e->vq_prev = prev;
	e->vq_next = prev->vq_next;
	prev->vq_next->vq_prev = e;
	prev->vq_next = e;
}

/*
 * Remove an entry from queue.
 */
_remque(e)
	register struct vaxque *e;
{
	e->vq_prev->vq_next = e->vq_next;
	e->vq_next->vq_prev = e->vq_prev;
}

setreuid()
{
	struct a {
		int	ruid;
		int	euid;
	} *uap;
	register int ruid, euid;

	uap = (struct a *) u.u_ap;
	ruid = uap->ruid;
	if (ruid == -1)
		ruid = u.u_ruid;
	if (u.u_ruid != ruid && u.u_uid != ruid && !suser())
		return;
	euid = uap->euid;
	if (euid == -1)
		euid = u.u_uid;
	if (u.u_ruid != euid && u.u_uid != euid && !suser())
		return;
	/*
	 * Everything's okay, do it.
	 */
	u.u_procp->p_uid = ruid;
	u.u_ruid = ruid;
	u.u_uid = euid;
}

setregid()
{
	register struct a {
		int	rgid;
		int	egid;
	} *uap;
	register int rgid, egid;

	uap = (struct a *) u.u_ap;
	rgid = uap->rgid;
	if (rgid == -1)
		rgid = u.u_rgid;
	if (u.u_rgid != rgid && u.u_gid != rgid && !suser())
		return;
	egid = uap->egid;
	if (egid == -1)
		egid = u.u_gid;
	if (u.u_rgid != egid && u.u_gid != egid && !suser())
		return;
	if (u.u_rgid != rgid)
		u.u_rgid = rgid;
	if (u.u_gid != egid)
		u.u_gid = egid;
}

/*
 *	Get/Set our local internet address.
 *	Names changed from Vax code because of PDP11 length restrictions
 */
gethostid()
{
	u.u_r.r_off = (off_t) LocalAddr;
}

sethostid()
{
	struct a {
		u_long	hostid;
	} *uap = (struct a *) u.u_ap;

	if (suser())
		LocalAddr = uap->hostid;
}
#endif	UCB_NET
