/*
 *	SCCS id	@(#)sys1.c	2.1 (Berkeley)	9/4/83
 */

#include "param.h"
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/reg.h>
#include <sys/inode.h>
#include <sys/seg.h>
#include <sys/acct.h>
#include <sys/file.h>
#include <wait.h>


/*
 * exec system call, with and without environments.
 */
struct execa {
	char	*fname;
	char	**argp;
	char	**envp;
};

exec()
{
	((struct execa *)u.u_ap)->envp = NULL;
	exece();
}

exece()
{
	register nc;
	register char *cp;
	register struct buf *bp;
	register struct execa *uap;
	memaddr	bno;
	int na, ne, ucp, ap, c;
	struct inode *ip;
#ifdef	UCB_SCRIPT
#define	SCRMAG	'#!'
	extern int schar();
	int uid, gid, indir;
#endif

#ifndef UCB_SYMLINKS
	if ((ip = namei(uchar, LOOKUP)) == NULL)
#else
	if ((ip = namei(uchar, LOOKUP, 1)) == NULL)
#endif
		return;
	bno = 0;
	bp = (struct buf *) NULL;
#ifdef	UCB_SCRIPT
	indir = 0;
	uid = u.u_uid;
	gid = u.u_gid;
	if (ip->i_mode&ISUID)
	    uid = ip->i_uid;
	if (ip->i_mode&ISGID)
	    gid = ip->i_gid;
again:
#endif
	if (access(ip, IEXEC))
		goto bad;
	if ((ip->i_mode & IFMT) != IFREG ||
	   (ip->i_mode & (IEXEC | (IEXEC >> 3) | (IEXEC >> 6))) == 0) {
		u.u_error = EACCES;
		goto bad;
	}
#ifdef	UCB_SCRIPT
	/* moved from getxfile() */
	u.u_base = (caddr_t) &u.u_exdata;
	u.u_count = sizeof u.u_exdata;
	u.u_offset = 0;
	u.u_segflg = 1;
	readi(ip);
	u.u_segflg = 0;
	if (u.u_error)
	    goto bad;
	/* check if script.  one level only */
	if (indir == 0
	   && u.u_exdata.ux_mag == SCRMAG
	   && u.u_count < sizeof u.u_exdata - sizeof u.u_exdata.ux_mag)
	{
	    indir++;
	    cp = (char *) &u.u_exdata + sizeof u.u_exdata.ux_mag;
	    while (*cp == ' ' && cp < (char *)&u.u_exdata + sizeof u.u_exdata-1)
		cp++;
	    u.u_dirp = cp;
	    while (cp < (char *) &u.u_exdata + sizeof u.u_exdata - 1
	          && *cp != '\n')
		cp++;
	    *cp = '\0';
	    iput(ip);
#ifndef UCB_SYMLINKS
	    if ((ip = namei(schar, LOOKUP)) == NULL)
#else
	    if ((ip = namei(schar, LOOKUP, 1)) == NULL)
#endif
		return;
	    goto again;
	}
	/*other magic numbers are described in getxfile()*/
#endif
	
	/*
	 * Collect arguments on "file" in swap space.
	 */
	na = 0;
	ne = 0;
	nc = 0;
	uap = (struct execa *)u.u_ap;
#ifndef	UCB_NKB
	if ((bno = malloc(swapmap, (NCARGS + BSIZE - 1) / BSIZE)) == 0)
		panic("Out of swap");
#else	UCB_NKB
	if ((bno = malloc(swapmap, ctod((int) btoc(NCARGS + BSIZE)))) == 0)
		panic("Out of swap");
#endif	UCB_NKB
	if (uap->argp) for (;;) {
		ap = NULL;
#ifdef	UCB_SCRIPT
		/* insert script path name as first arg */
		if (indir && na == 1)
		    ap = uap->fname;
		else
#endif
		if (uap->argp) {
			ap = fuword((caddr_t)uap->argp);
			uap->argp++;
		}
		if (ap==NULL && uap->envp) {
			uap->argp = NULL;
			if ((ap = fuword((caddr_t)uap->envp)) == NULL)
				break;
			uap->envp++;
			ne++;
		}
		if (ap==NULL)
			break;
		na++;
		if (ap == -1)
			u.u_error = EFAULT;
		do {
			if (nc >= NCARGS - 1)
				u.u_error = E2BIG;
			if ((c = fubyte((caddr_t) ap++)) < 0)
				u.u_error = EFAULT;
			if (u.u_error)
				goto bad;
			if ((nc & BMASK) == 0) {
				if (bp) {
					mapout(bp);
					bdwrite(bp);
				}
#ifndef	UCB_NKB
				bp = getblk(swapdev, swplo + bno + (nc >> BSHIFT));
#else
				bp = getblk(swapdev,
				  dbtofsb(clrnd(swplo + bno)) + (nc >> BSHIFT));
#endif
				cp = mapin(bp);
			}
			nc++;
			*cp++ = c;
		} while (c > 0);
	}
	if (bp) {
		mapout(bp);
		bdwrite(bp);
	}
	bp = 0;
	nc = (nc + NBPW - 1) & ~(NBPW - 1);
#ifndef	UCB_SCRIPT
	if (getxfile(ip, (na * NBPW) + nc) || u.u_error)
		goto bad;
#else
	if (getxfile(ip, (na * NBPW) + nc, uid, gid) || u.u_error)
		goto bad;
#endif

	/*
	 * copy back arglist
	 */

	ucp = -nc - NBPW;
	ap = ucp - na * NBPW - 3 * NBPW;
	u.u_ar0[R6] = ap;
	suword((caddr_t)ap, na - ne);
	nc = 0;
	for (;;) {
		ap += NBPW;
		if (na == ne) {
			suword((caddr_t)ap, 0);
			ap += NBPW;
		}
		if (--na < 0)
			break;
		suword((caddr_t)ap, ucp);
		do {
			if ((nc & BMASK) == 0) {
				if (bp) {
					mapout(bp);
					bp->b_flags |= B_AGE;
					brelse(bp);
				}
#ifndef	UCB_NKB
				bp = bread(swapdev, swplo + bno + (nc>>BSHIFT));
#else
				bp = bread(swapdev,
				  dbtofsb(clrnd(swplo + bno)) + (nc >> BSHIFT));
#endif
				bp->b_flags &= ~B_DELWRI;
				cp = mapin(bp);
#ifdef	UCB_SCRIPT
				/* stick in interpreter name for accounting */
				if (indir && nc == 0)
					bcopy(cp, (caddr_t)u.u_dbuf, DIRSIZ);
#endif
			}
			subyte((caddr_t)ucp++, (c = *cp++));
			nc++;
		} while(c & 0377);
	}
	suword((caddr_t) ap, 0);
	suword((caddr_t) (-NBPW), 0);
	if (bp) {
		mapout(bp);
		bp->b_flags |= B_AGE;
		brelse(bp);
		bp = 0;
	}
	setregs();

bad:
	if (bp) {
		mapout(bp);
		bp->b_flags |= B_AGE;
		brelse(bp);
	}
	if (bno)
#ifndef	UCB_NKB
		mfree(swapmap, (NCARGS + BSIZE - 1) / BSIZE, bno);
#else
		mfree(swapmap, ctod((int) btoc(NCARGS + BSIZE)), bno);
#endif
	iput(ip);
}

/*
 * Read in and set up memory for executed file.
 * Zero return is normal;
 * non-zero means only the text is being replaced
 */
#ifdef	UCB_SCRIPT
getxfile(ip, nargc, uid, gid)
int nargc, uid, gid;
#else
getxfile(ip, nargc)
#endif
register struct inode *ip;
{
	register unsigned ds;
	register sep;
	register unsigned ts, ss;
	register i, overlay;
#ifdef	MENLO_OVLY
	register ovflag,ovmax;
	struct u_ovd sovdata;
	unsigned ovhead[1 + NOVL];
#endif
	long lsize;

#ifndef	UCB_SCRIPT
	/*
	 * read in first few bytes
	 * of file for segment
	 * sizes:
	 * ux_mag = A_MAGIC1/A_MAGIC2/A_MAGIC3/A_MAGIC4
	 *  A_MAGIC1 is plain executable
	 *  A_MAGIC2 is RO text
	 *  A_MAGIC3 is separated ID
	 *  A_MAGIC4 is overlaid text
	 */
#ifdef	MENLO_OVLY
	/*
	 * ux_mag = A_MAGIC1/A_MAGIC2/A_MAGIC3/A_MAGIC4/A_MAGIC5/A_MAGIC6
	 *  A_MAGIC5 is nonseparate auto-overlay
	 *  A_MAGIC6 is separate auto overlay
	 */
#endif

	u.u_base = (caddr_t) &u.u_exdata;
	u.u_count = sizeof(u.u_exdata);
	u.u_offset = 0;
	u.u_segflg = 1;
	readi(ip);
	u.u_segflg = 0;
	if (u.u_error)
		goto bad;
	if (u.u_count != 0) {
		u.u_error = ENOEXEC;
		goto bad;
	}
#endif
	sep = 0;
	overlay = 0;
#ifdef	MENLO_OVLY
	ovflag = 0;
#endif
	if (u.u_exdata.ux_mag == A_MAGIC1) {
		lsize = (long) u.u_exdata.ux_dsize + u.u_exdata.ux_tsize;
		u.u_exdata.ux_dsize = lsize;
		if (lsize != u.u_exdata.ux_dsize) {	/* check overflow */
			u.u_error = ENOMEM;
			goto bad;
		}
		u.u_exdata.ux_tsize = 0;
	} else if (u.u_exdata.ux_mag == A_MAGIC3)
		sep++;
	else if (u.u_exdata.ux_mag == A_MAGIC4)
		overlay++;
#ifdef	MENLO_OVLY
	else if (u.u_exdata.ux_mag == A_MAGIC5)
		ovflag++;
	else if (u.u_exdata.ux_mag == A_MAGIC6) {
		sep++;
		ovflag++;
	}
#endif
	else if (u.u_exdata.ux_mag != A_MAGIC2) {
		u.u_error = ENOEXEC;
		goto bad;
	}
	if (u.u_exdata.ux_tsize!=0 && (ip->i_flag&ITEXT)==0 && ip->i_count!=1) {
		u.u_error = ETXTBSY;
		goto bad;
	}

	/*
	 * find text and data sizes
	 * try them out for possible
	 * overflow of max sizes
	 */
	ts = btoc(u.u_exdata.ux_tsize);
	lsize = (long) u.u_exdata.ux_dsize + u.u_exdata.ux_bsize;
	if (lsize != (unsigned) lsize) {
		u.u_error = ENOMEM;
		goto bad;
	}
	ds = btoc(lsize);
	ss = SSIZE + btoc(nargc);
#ifdef	MENLO_OVLY

	/*
	 * if auto overlay get second header
	 */

	sovdata = u.u_ovdata;
	u.u_ovdata.uo_ovbase = 0;
	u.u_ovdata.uo_curov = 0;
	if (ovflag) {
		u.u_base = (caddr_t) ovhead;
		u.u_count = sizeof(ovhead);
		u.u_offset = sizeof(u.u_exdata);
		u.u_segflg = 1;
		readi(ip);
		u.u_segflg = 0;
		if (u.u_count != 0)
			u.u_error = ENOEXEC;
		if (u.u_error) {
			u.u_ovdata = sovdata;
			goto bad;
		}
		/* set beginning of overlay segment */
		u.u_ovdata.uo_ovbase = ctos(ts);

		/* 0th entry is max size of the overlays */
		ovmax = btoc(ovhead[0]);

		/* set max number of segm. registers to be used */
		u.u_ovdata.uo_nseg = ctos(ovmax);

		/* set base of data space */
		u.u_ovdata.uo_dbase = stoc(u.u_ovdata.uo_ovbase + u.u_ovdata.uo_nseg);
		/*
		 * Set up a table of offsets to each of the
		 * overlay segements. The ith overlay runs
		 * from ov_offst[i-1] to ov_offst[i].
		 */
		u.u_ovdata.uo_ov_offst[0] = ts;
		for (i = 1; i < 1 + NOVL; i++) {
			register t;
			/* check if any overlay is larger than ovmax */
			if ((t=btoc(ovhead[i])) > ovmax) {
				u.u_error = ENOEXEC;
				u.u_ovdata = sovdata;
				goto bad;
			}
			u.u_ovdata.uo_ov_offst[i] =
				t + u.u_ovdata.uo_ov_offst[i - 1];
		}
	}

#endif
	if (overlay) {
		if (u.u_sep == 0 && ctos(ts) != ctos(u.u_tsize) || nargc) {
			u.u_error = ENOMEM;
			goto bad;
		}
		ds = u.u_dsize;
		ss = u.u_ssize;
		sep = u.u_sep;
		xfree();
		xalloc(ip);
		u.u_ar0[PC] = u.u_exdata.ux_entloc & ~01;
	} else {
		if (estabur(ts, ds, ss, sep, RO)) {
#ifdef	MENLO_OVLY
			u.u_ovdata = sovdata;
#endif
			goto bad;
		}

		/*
		 * allocate and clear core
		 * at this point, committed
		 * to the new image
		 */
	
		u.u_prof.pr_scale = 0;
#ifdef	VIRUS_VFORK
		if (u.u_procp->p_flag & SVFORK)
			endvfork();
		else
			xfree();
		expand(ds, S_DATA);
		clear(u.u_procp->p_daddr, ds);
		expand(ss,S_STACK);
		clear(u.u_procp->p_saddr, ss);
#else
		xfree();
		i = USIZE + ds + ss;
		expand(i);
		clear(u.u_procp->p_addr + USIZE, i - USIZE);
#endif
		xalloc(ip);
	
		/*
		 * read in data segment
		 */
		estabur((unsigned)0, ds, (unsigned)0, 0, RO);
		u.u_base = 0;
#ifndef	MENLO_OVLY
		u.u_offset = sizeof(u.u_exdata) + u.u_exdata.ux_tsize;
#else
		u.u_offset = sizeof(u.u_exdata);
		if (ovflag) {
			u.u_offset += sizeof(ovhead);
			u.u_offset += (((long)u.u_ovdata.uo_ov_offst[NOVL]) << 6);
		}
		else
			u.u_offset += u.u_exdata.ux_tsize;
#endif
		u.u_count = u.u_exdata.ux_dsize;
		readi(ip);

		/*
		 * set SUID/SGID protections, if no tracing
		 */
		if ((u.u_procp->p_flag & STRC) == 0) {
#ifndef	UCB_SCRIPT
			if (ip->i_mode & ISUID)
				if (u.u_uid != 0) {
					u.u_uid = ip->i_uid;
					u.u_procp->p_uid = ip->i_uid;
				}
			if (ip->i_mode&ISGID)
				u.u_gid = ip->i_gid;
#else
			u.u_uid = uid;
			u.u_procp->p_uid = uid;
			u.u_gid = gid;
#endif
		} else
			psignal(u.u_procp, SIGTRAP);
	}
	u.u_tsize = ts;
	u.u_dsize = ds;
	u.u_ssize = ss;
	u.u_sep = sep;
	estabur(ts, ds, ss, sep, RO);
bad:
	return(overlay);
}

/*
 * Clear registers on exec
 */
setregs()
{
#ifdef	MENLO_JCL
	register int (**rp)();
	long sigmask;
#else
	register int *rp;
#endif
	register char *cp;
	register i;

#ifndef	MENLO_JCL
	for(rp = &u.u_signal[0]; rp < &u.u_signal[NSIG]; rp++)
		if ((*rp & 1) == 0)
			*rp = 0;
#else
	u.u_procp->p_flag &= ~SNUSIG;
	for(rp = &u.u_signal[1], sigmask = 1L; rp < &u.u_signal[NSIG];
	    sigmask <<= 1, rp++) {
		switch (*rp) {

		case SIG_HOLD:
			u.u_procp->p_flag |= SNUSIG;
			continue;
		case SIG_IGN:
		case SIG_DFL:
			continue;

		default:
			/*
			 * Normal or deferring catch; revert to default.
			 */
			(void) _spl6();
			*rp = SIG_DFL;
			if ((int)SIG_DFL & 1)
				u.u_procp->p_siga0 |= sigmask;
			else
				u.u_procp->p_siga0 &= ~sigmask;
			if ((int)SIG_DFL & 2)
				u.u_procp->p_siga1 |= sigmask;
			else
				u.u_procp->p_siga1 &= ~sigmask;
			(void) _spl0();
			continue;
		}
	}
#endif
	for(cp = &regloc[0]; cp < &regloc[6];)
		u.u_ar0[*cp++] = 0;
	u.u_ar0[PC] = u.u_exdata.ux_entloc & ~01;
#ifndef	NONFP
	for(rp = (int *)&u.u_fps; rp < (int *)&u.u_fps.u_fpregs[6];)
		*rp++ = 0;
#endif
	for(i=0; i<NOFILE; i++) {
		if (u.u_pofile[i]&EXCLOSE) {
#ifndef UCB_NET
			closef(u.u_ofile[i]);
#else
			closef(u.u_ofile[i],1);
#endif
			u.u_ofile[i] = NULL;
			u.u_pofile[i] &= ~EXCLOSE;
		}
	}
#ifdef	ACCT
	u.u_acflag &= ~AFORK;
#endif
	/*
	 * Remember file name.
	 */
	bcopy((caddr_t)u.u_dbuf, (caddr_t)u.u_comm, DIRSIZ);
}

/*
 * exit system call:
 * pass back caller's arg
 */
rexit()
{
	register struct a {
		int	rval;
	} *uap;

	uap = (struct a *)u.u_ap;
	exit((uap->rval & 0377) << 8);
}

/*
 * Release resources.
 * Save u. area for parent to look at.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
exit(rv)
{
	register int i;
	register struct proc *p, *q;
	register struct file *f;

	p = u.u_procp;
	p->p_flag &= ~(STRC|SULOCK);
	p->p_clktim = 0;
#ifdef	CGL_RTP
	/*
	 * if this a "real time" process that is dying
	 * remove the rtpp flag.
	 */
	if (rtpp != NULL && rtpp == p)
		rtpp = NULL;
#endif
#ifdef	MENLO_JCL
	(void) _spl6();
	if ((int)SIG_IGN & 1)
		p->p_siga0 = ~0L;
	else
		p->p_siga0 = 0L;
	if ((int)SIG_IGN & 2)
		p->p_siga1 = ~0L;
	else
		p->p_siga1 = 0L;
	(void) _spl0();
#endif
	for(i=0; i<NSIG; i++)
		u.u_signal[i] = SIG_IGN;
	for(i=0; i<NOFILE; i++) {
		f = u.u_ofile[i];
		u.u_ofile[i] = NULL;
#ifndef UCB_NET
		closef(f);
#else
		closef(f,1);
#endif
	}
	plock(u.u_cdir);
	iput(u.u_cdir);
	if (u.u_rdir) {
		plock(u.u_rdir);
		iput(u.u_rdir);
	}
#ifdef	ACCT
	acct();
#endif	ACCT
#ifdef	VIRUS_VFORK
	if (p->p_flag & SVFORK) {
		endvfork();
	} else {
		xfree();
		mfree(coremap, p->p_dsize, p->p_daddr);
		mfree(coremap, p->p_ssize, p->p_saddr);
	}
	mfree(coremap, USIZE, p->p_addr);
#else	VIRUS_VFORK
	xfree();
	mfree(coremap, p->p_size, p->p_addr);
#endif	VIRUS_VFORK
	p->p_stat = SZOMB;
	if (p->p_pid == 1) {
		/*
		 * If /etc/init is not found by the icode,
		 * the stack size will still be zero when it exits.
		 * Don't panic: we're unlikely to find init after a reboot,
		 * either.
		 */
		if (u.u_ssize == 0) {
			printf("Can't exec /etc/init\n");
			for (;;)
				idle();
		}
		else
			panic("init died");
	}
	p->p_un.xp_xstat = rv;
	p->p_un.xp_utime = u.u_cutime + u.u_utime;
	p->p_un.xp_stime = u.u_cstime + u.u_stime;
#ifdef	UCB_LOGIN
	p->p_un.xp_login = u.u_login;
#endif
#ifdef	MENLO_JCL
	for(q = &proc[0]; q <= maxproc; q++)
		if (q->p_pptr == p) {
			q->p_pptr = &proc[1];
			q->p_ppid = 1;
			wakeup((caddr_t)&proc[1]);
			/*
			 * Traced processes are killed
			 * since their existence means someone is screwing up.
			 * Stopped processes are sent a hangup and a continue.
			 * This is designed to be ``safe'' for setuid
			 * processes since they must be willing to tolerate
			 * hangups anyways.
			 */
			if (q->p_flag&STRC) {
				q->p_flag &= ~STRC;
				psignal(q, SIGKILL);
			} else if (q->p_stat == SSTOP) {
				psignal(q, SIGHUP);
				psignal(q, SIGCONT);
			}
			/*
			 * Protect this process from future
			 * tty signals, clear TSTP/TTIN/TTOU if pending,
			 * and set SDETACH bit on procs.
			 */
			spgrp(q, -1);
		}
	wakeup((caddr_t)p->p_pptr);
	psignal(p->p_pptr, SIGCHLD);
#else
	for(q = &proc[0]; q <= maxproc; q++)
		if (q->p_ppid == p->p_pid) {
			wakeup((caddr_t)&proc[1]);
			q->p_ppid = 1;
			if (q->p_stat==SSTOP)
				setrun(q);
		}
	for(q = &proc[0]; q <= maxproc; q++)
		if (p->p_ppid == q->p_pid) {
			wakeup((caddr_t)q);
			swtch();
			/* no return */
		}
#endif
	swtch();
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 * Look also for stopped (traced) children,
 * and pass back status from them.
 */
wait()
{
	register f;
	register struct proc *p;
#ifdef	MENLO_JCL
	register options;

	options = (u.u_ar0[RPS] & PS_ALLCC) == PS_ALLCC ?  u.u_ar0[R0] : 0;
#endif
	f = 0;

loop:
	for(p = &proc[0]; p <= maxproc; p++)
#ifdef	MENLO_JCL
	if (p->p_pptr == u.u_procp)
#else
	if (p->p_ppid == u.u_procp->p_pid)
#endif
		{
		f++;
		if (p->p_stat == SZOMB) {
			u.u_r.r_val1 = p->p_pid;
			u.u_r.r_val2 = p->p_un.xp_xstat;
#ifdef	MENLO_JCL
			p->p_un.xp_xstat = 0;
			p->p_pptr = 0;
			p->p_siga0 = 0L;
			p->p_siga1 = 0L;
			p->p_cursig = 0;
#endif
			u.u_cutime += p->p_un.xp_utime;
			u.u_cstime += p->p_un.xp_stime;
			p->p_pid = 0;
			p->p_ppid = 0;
			p->p_pgrp = 0;
			p->p_sig = 0;
			p->p_flag = 0;
			p->p_wchan = 0;
			p->p_stat = NULL;
			if (p == maxproc)
				while (maxproc->p_stat == NULL)
					maxproc--;
			return;
		}
		if (p->p_stat == SSTOP && (p->p_flag & SWTED) == 0 &&
		   (p->p_flag & STRC
#ifdef	MENLO_JCL
				|| options & WUNTRACED
#endif
					)){
			p->p_flag |= SWTED;
			u.u_r.r_val1 = p->p_pid;
#ifdef	MENLO_JCL
			u.u_r.r_val2 = (p->p_cursig << 8) | 0177;
#else
			u.u_r.r_val2 = (fsig(p) << 8) | 0177;
#endif
			return;
		}
	}
	if (f) {
#ifdef	MENLO_JCL
		if (options & WNOHANG) {
			u.u_r.r_val1 = 0;
			return;
		} else {
			if ((u.u_procp->p_flag & SNUSIG) && save(u.u_qsav)) {
				u.u_eosys = RESTARTSYS;
				return;
			}
			sleep((caddr_t)u.u_procp, PWAIT);
			goto loop;
		}
#else
		sleep((caddr_t) u.u_procp, PWAIT);
		goto loop;
#endif
	}
	u.u_error = ECHILD;
}

/*
 * fork system call.
 */
#ifdef	VIRUS_VFORK
fork()
{
	fork1(0);
}

/*
 * vfork system call
 */
vfork()
{
	fork1(1);
}

fork1(isvfork)
#else	VIRUS_VFORK
fork()
#endif
{
#ifdef	UCB_PGRP
	register struct proc *p1;
	struct proc *p2;
	register a, pg;
#else
	register struct proc *p1, *p2;
	register a;
#endif

	/*
	 * Make sure there's enough swap space for max
	 * core image, thus reducing chances of running out
	 */
	if ((a = malloc(swapmap, ctod(maxmem))) == 0) {
		u.u_error = ENOMEM;
		goto out;
	}
	mfree(swapmap, ctod(maxmem), a);
	a = 0;
#ifdef	UCB_PGRP
	pg = u.u_procp->p_pgrp;	/* process group number */
#endif
	p2 = NULL;
	for(p1 = proc; p1 < procNPROC; p1++) {
		if (p1->p_stat==NULL && p2==NULL)
			p2 = p1;
		else {
			/*
			 * Exempt low positive uids (0-15) for users like uucp
			 * and network, which shouldn't lose limits.
			 */
#ifdef	UCB_PGRP
			if (p1->p_pgrp==pg && (unsigned) u.u_uid>=16
			    && p1->p_stat!=NULL)
				a++;
#else
			if ((p1->p_uid==u.u_uid && p1->p_stat!=NULL)
			    && ((unsigned) u.u_uid >= 16))
				a++;
#endif
		}
	}
	/*
	 * Disallow if
	 *  No processes at all;
	 *  not su and too many procs owned (or in pgrp, if UCB_PGRP set);
	 *  or not su and would take last slot.
	 */
	if (p2 == NULL)
		tablefull("proc");
	if (p2 == NULL || (u.u_uid != 0 && (p2 == procNPROC-1 || a > MAXUPRC))){
		u.u_error = EAGAIN;
		goto out;
	}
	p1 = u.u_procp;
#ifdef	VIRUS_VFORK
	if (newproc(isvfork))
#else
	if (newproc())
#endif
		{
		u.u_r.r_val1 = p1->p_pid;
		u.u_start = time;
		u.u_cstime = 0;
		u.u_stime = 0;
		u.u_cutime = 0;
		u.u_utime = 0;
#ifdef	UCB_LOGIN
		u.u_login = 0;
#endif
#ifdef	ACCT
		u.u_acflag = AFORK;
#endif
		return;
	}
	u.u_r.r_val1 = p2->p_pid;

out:
	u.u_ar0[R7] += NBPW;
}

/*
 * break system call.
 *  -- bad planning: "break" is a dirty word in C.
 */
sbreak()
{
	struct a {
		char	*nsiz;
	};
	register a, n, d;
	int i;

	/*
	 * set n to new data size
	 * set d to new-old
	 * set n to new total size
	 */

	n = btoc((int) ((struct a *) u.u_ap)->nsiz);
	if (!u.u_sep)
#ifdef	MENLO_OVLY
		if (u.u_ovdata.uo_ovbase)
			n -= u.u_ovdata.uo_dbase;
		else
			n -= ctos(u.u_tsize) * stoc(1);
#else
		n -= ctos(u.u_tsize) * stoc(1);
#endif
	if (n < 0)
		n = 0;
#ifdef	VIRUS_VFORK
	d = n - u.u_dsize;
	if (estabur(u.u_tsize, n, u.u_ssize, u.u_sep, RO))
		return;
	expand(n,S_DATA);
	if (d > 0)
		clear(u.u_procp->p_daddr + u.u_dsize, d);
	u.u_dsize = n;

#else	VIRUS_VFORK
	d = n - u.u_dsize;
	n += USIZE+u.u_ssize;
	if (estabur(u.u_tsize, u.u_dsize+d, u.u_ssize, u.u_sep, RO))
		return;
	u.u_dsize += d;
	if (d > 0)
		goto bigger;
	a = u.u_procp->p_addr + n - u.u_ssize;
	copy(a-d, a, u.u_ssize);	/* d is negative */
	expand(n);
	return;

bigger:
	expand(n);
	a = u.u_procp->p_addr + n - u.u_ssize - d;
	n = u.u_ssize;
	while (n >= d) {
		n -= d;
		copy(a+n, a+n+d, d);
	}
	copy(a, a+d, n);
	clear(a, d);
#endif	VIRUS_VFORK
}
