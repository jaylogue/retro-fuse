/*
 *	SCCS id	@(#)sys.c	2.1 (Berkeley)	8/5/83
 */

/*
 *	indirect driver for controlling tty.
 */
#include "param.h"
#include <sys/conf.h>
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/proc.h>


syopen(dev, flag)
dev_t dev;
{
	if(u.u_ttyp == NULL) {
		u.u_error = ENXIO;
		return;
	}
	(*cdevsw[major(u.u_ttyd)].d_open)(u.u_ttyd, flag);
}

syread(dev)
dev_t dev;
{
#ifdef	UCB_NET
	if (u.u_ttyp == NULL) {
		u.u_error = ENXIO;
		return;
	}
#endif
	(*cdevsw[major(u.u_ttyd)].d_read)(u.u_ttyd);
}

sywrite(dev)
dev_t dev;
{
#ifdef	UCB_NET
	if (u.u_ttyp == NULL) {
		u.u_error = ENXIO;
		return;
	}
#endif
	(*cdevsw[major(u.u_ttyd)].d_write)(u.u_ttyd);
}

sysioctl(dev, cmd, addr, flag)
dev_t dev;
caddr_t addr;
{
#ifdef	UCB_NET
	if (cmd == TIOCNOTTY) {
		u.u_ttyp = 0;
		u.u_ttyd = 0;
		u.u_procp->p_pgrp = 0;
		return;
	}
	if (u.u_ttyp == NULL) {
		u.u_error = ENXIO;
		return;
	}
#endif
	(*cdevsw[major(u.u_ttyd)].d_ioctl)(u.u_ttyd, cmd, addr, flag);
}

#ifdef	UCB_NET
syselect(dev, flag)
{
	if (u.u_ttyp == NULL) {
		u.u_error = ENXIO;
		return (0);
	}
	return ((*cdevsw[major(u.u_ttyd)].d_select)(u.u_ttyd, flag));
}
#endif
