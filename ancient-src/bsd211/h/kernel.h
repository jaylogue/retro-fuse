/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)kernel.h	1.3 (2.11BSD GTE) 1997/2/14
 */

/*
 * Global variables for the kernel
 */

#ifdef SUPERVISOR
/* UNUSED: long	startnet;			/* start of network data space */
#else
memaddr	malloc();

/* 1.1 */
/* UNUSED: long	hostid; */
/* UNUSED: char	hostname[MAXHOSTNAMELEN]; */
/* UNUSED: int	hostnamelen; */

/* 1.2 */
#include <bsd211/h/time.h>

/* UNUSED: struct	timeval boottime; */
extern struct	timeval time;
/* UNUSED: struct	timezone tz;			/* XXX */
/* UNUSED: int	adjdelta; */
/* UNUSED: int	hz; */
/* UNUSED: int	mshz;				/* # milliseconds per hz */
/* UNUSED: int	lbolt;				/* awoken once a second */
/* UNUSED: int	realitexpire(); */

/* UNUSED: short	avenrun[3]; */
#endif
