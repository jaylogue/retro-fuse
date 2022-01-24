/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)trace.h	7.1 (Berkeley) 6/4/86
 */

/*
 * File system buffer tracing points; all trace <pack(dev, size), bn>
 *
 * Slipped into 2.10 in about 15 minutes.  Bears only a casual
 * relationship to the 4.X code, we just used the same names so
 * someone could do it right if they felt like it.  Note that
 * the #defines aren't even sequentially numbered.
 */
#define	TR_BREADHIT	0	/* buffer read found in cache */
#define	TR_BREADMISS	1	/* buffer read not in cache */
#define	TR_BWRITE	2	/* buffer written */
#define	TR_BREADHITRA	3	/* buffer read-ahead found in cache */
#define	TR_BREADMISSRA	4	/* buffer read-ahead not in cache */
#define	TR_BRELSE	5	/* brelse */
#define	TR_SWAPIO	6	/* swap i/o request arrives */
#define	TR_NUM_210	7	/* size of array for 2.10BSD

#define	TR_XFODMISS	5	/* exe fod read */
#define	TR_XFODHIT	6	/* exe fod read */
#define	TR_BREALLOC	8	/* expand/contract a buffer */

/*
 * Memory allocator trace points; all trace the amount of memory involved
 */
#define	TR_MALL		10	/* memory allocated */

/*
 * Paging trace points: all are <vaddr, pid>
 */
#define	TR_INTRANS	20	/* page intransit block */
#define	TR_EINTRANS	21	/* page intransit wait done */
#define	TR_FRECLAIM	22	/* reclaim from free list */
#define	TR_RECLAIM	23	/* reclaim from loop */
#define	TR_XSFREC	24	/* reclaim from free list instead of drum */
#define	TR_XIFREC	25	/* reclaim from free list instead of fsys */
#define	TR_WAITMEM	26	/* wait for memory in pagein */
#define	TR_EWAITMEM	27	/* end memory wait in pagein */
#define	TR_ZFOD		28	/* zfod page fault */
#define	TR_EXFOD	29	/* exec fod page fault */
#define	TR_VRFOD	30	/* vread fod page fault */
#define	TR_CACHEFOD	31	/* fod in file system cache */
#define	TR_SWAPIN	32	/* drum page fault */
#define	TR_PGINDONE	33	/* page in done */

/*
 * System call trace points.
 */
#define	TR_VADVISE	40	/* vadvise occurred with <arg, pid> */

/*
 * Miscellaneous
 */
#define	TR_STAMP	45	/* user said vtrace(VTR_STAMP, value); */

/*
 * This defines the size of the trace flags array.
 */
#define	TR_NFLAGS	100	/* generous */

#define	TRCSIZ		4096

/*
 * Specifications of the vtrace() system call, which takes one argument.
 */
#define	VTRACE		64+51

#define	VTR_DISABLE	0		/* set a trace flag to 0 */
#define	VTR_ENABLE	1		/* set a trace flag to 1 */
#define	VTR_VALUE	2		/* return value of a trace flag */
#define	VTR_UALARM	3		/* set alarm to go off (sig 16) */
					/* in specified number of hz */
#define	VTR_STAMP	4		/* user specified stamp */

#if defined(KERNEL) && defined(UCB_METER) && !defined(SUPERVISOR)
u_long tracebuf[TR_NUM_210];
#define	trace(a)	tracebuf[a]++;
#else
#define	trace(a)	;
#endif
