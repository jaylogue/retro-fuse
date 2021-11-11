/*
 * Ioctl.
 */

/*
 *	SCCS id	@(#)ioctl.c	2.1 (Berkeley)	8/5/83
 */
#include "param.h"
#include <sys/systm.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/proc.h>
#ifdef	MPX_FILS
#include <sys/mx.h>
#endif
#include <sys/inode.h>
#include <sys/file.h>
#include <sys/reg.h>
#include <sys/conf.h>

/*
 * stty/gtty writearound
 */
stty()
{
	u.u_arg[2] = u.u_arg[1];
	u.u_arg[1] = TIOCSETP;
	ioctl();
}

gtty()
{
	u.u_arg[2] = u.u_arg[1];
	u.u_arg[1] = TIOCGETP;
	ioctl();
}

/*
 * ioctl system call
 * Check legality, execute common code, and switch out to individual
 * device routine.
 */
ioctl()
{
	register struct file *fp;
	register struct inode *ip;
	register struct a {
		int	fdes;
		int	cmd;
		caddr_t	cmarg;
	} *uap;
	dev_t dev;
	int fmt;

	uap = (struct a *)u.u_ap;
	if ((fp = getf(uap->fdes)) == NULL)
		return;
	if ((fp->f_flag & (FREAD|FWRITE)) == 0) {
		u.u_error = EBADF;
		return;
	}
	if (uap->cmd==FIOCLEX) {
		u.u_pofile[uap->fdes] |= EXCLOSE;
		return;
	}
	if (uap->cmd==FIONCLEX) {
		u.u_pofile[uap->fdes] &= ~EXCLOSE;
		return;
	}
#ifdef  UCB_NET
	if (fp->f_flag & FSOCKET) {
		soioctl(fp->f_socket, uap->cmd, uap->cmarg);
		return;
	}
#endif
	ip = fp->f_inode;
	fmt = ip->i_mode & IFMT;
#ifdef	MPX_FILS
	if (fmt != IFCHR && fmt != IFMPC)
#else
	if (fmt != IFCHR)
#endif
		{
#ifdef	UCB_NTTY
		if (uap->cmd==FIONREAD && (fmt == IFREG || fmt == IFDIR)) {
			off_t nread = ip->i_size - fp->f_un.f_offset;

			if (copyout((caddr_t)&nread, uap->cmarg, sizeof(off_t)))
				u.u_error = EFAULT;
		} else
#endif
#ifdef	UCB_NET
		       if (uap->cmd == FIONBIO || uap->cmd == FIOASYNC)
			return;
		else
#endif
			u.u_error = ENOTTY;
		return;
	}
	dev = ip->i_un.i_rdev;
	u.u_r.r_val1 = 0;
#ifdef	MENLO_JCL
	if ((u.u_procp->p_flag&SNUSIG) && save(u.u_qsav)) {
		u.u_eosys = RESTARTSYS;
		return;
	}
#endif
	(*cdevsw[major(dev)].d_ioctl)(dev, uap->cmd, uap->cmarg, fp->f_flag);
}

/*
 * Do nothing specific version of line
 * discipline specific ioctl command.
 */
/*ARGSUSED*/
nullioctl(tp, cmd, addr, flag)
struct tty *tp;
caddr_t addr;
{
	return (cmd);
}
