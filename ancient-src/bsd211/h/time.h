/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)time.h	1.3 (2.11BSD) 2000/4/21
 */

#ifndef	_SYS_TIME_H_
#define	_SYS_TIME_H_

#include <bsd211/h/types.h>

/*
 * Structure returned by gettimeofday(2) system call,
 * and used in other calls.
 */
struct timeval {
	int32_t	tv_sec;		/* seconds */
	int32_t	tv_usec;	/* and microseconds */
};

/*
 * Structure defined by POSIX.4 to be like a timeval but with nanoseconds
 * instead of microseconds.  Silly on a PDP-11 but keeping the names the
 * same makes life simpler than changing the names.
*/
struct timespec {
	time_t tv_sec;		/* seconds */
	int32_t   tv_nsec;		/* and nanoseconds */
};

struct timezone {
	int16_t	tz_minuteswest;	/* minutes west of Greenwich */
	int16_t	tz_dsttime;	/* type of dst correction */
};
#define	DST_NONE	0	/* not on dst */
#define	DST_USA		1	/* USA style dst */
#define	DST_AUST	2	/* Australian style dst */
#define	DST_WET		3	/* Western European dst */
#define	DST_MET		4	/* Middle European dst */
#define	DST_EET		5	/* Eastern European dst */
#define	DST_CAN		6	/* Canada */

/*
 * Operations on timevals.
 *
 * NB: timercmp does not work for >= or <=.
 */
#define	timerisset(tvp)		((tvp)->tv_sec || (tvp)->tv_usec)
#define	timercmp(tvp, uvp, cmp)	\
	((tvp)->tv_sec cmp (uvp)->tv_sec || \
	 (tvp)->tv_sec == (uvp)->tv_sec && (tvp)->tv_usec cmp (uvp)->tv_usec)
#define	timerclear(tvp)		(tvp)->tv_sec = (tvp)->tv_usec = 0

/*
 * Names of the interval timers, and structure
 * defining a timer setting.
 */
#define	ITIMER_REAL	0
#define	ITIMER_VIRTUAL	1
#define	ITIMER_PROF	2

struct	k_itimerval {
	int32_t	it_interval;		/* timer interval */
	int32_t	it_value;		/* current value */
};

struct	itimerval {
	struct	timeval it_interval;	/* timer interval */
	struct	timeval it_value;	/* current value */
};

#ifndef KERNEL
#include <time.h>
#endif

/*
 * Getkerninfo clock information structure
 */
struct clockinfo {
	int16_t	hz;		/* clock frequency */
	int16_t	tick;		/* micro-seconds per hz tick */
	int16_t	stathz;		/* statistics clock frequency */
	int16_t	profhz;		/* profiling clock frequency */
};
#endif	/* !_SYS_TIME_H_ */
