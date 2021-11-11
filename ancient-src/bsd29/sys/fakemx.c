/*
 * Fake multiplexor routines to satisfy references
 * if you don't want it.
 */

#include "param.h"
#include <sys/dir.h>
#include <sys/user.h>
#include <sys/tty.h>
#include <sys/inode.h>
#include <sys/mx.h>

/*
 *	SCCS id	@(#)fakemx.c	2.1 (Berkeley)	8/5/83
 */


sdata(cp)
struct chan *cp;
{
}

mcttstart(tp)
struct tty *tp;
{
}

mpxchan()
{
	u.u_error = EINVAL;
}

mcstart(p, q)
struct chan *p;
caddr_t q;
{
}

scontrol(chan, s, c)
struct chan *chan;
{
}
