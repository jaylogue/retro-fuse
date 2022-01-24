/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)systm.h	1.3 (2.11BSD GTE) 1996/5/9
 */

#ifndef SUPERVISOR

/*
 * The `securelevel' variable controls the security level of the system.
 * It can only be decreased by process 1 (/sbin/init).
 *
 * Security levels are as follows:
 *   -1	permannently insecure mode - always run system in level 0 mode.
 *    0	insecure mode - immutable and append-only flags make be turned off.
 *	All devices may be read or written subject to permission modes.
 *    1	secure mode - immutable and append-only flags may not be changed;
 *	raw disks of mounted filesystems, /dev/mem, and /dev/kmem are
 *	read-only.
 *    2	highly secure mode - same as (1) plus raw disks are always
 *	read-only whether mounted or not. This level precludes tampering 
 *	with filesystems by unmounting them, but also inhibits running
 *	newfs while the system is secured.
 *
 * In normal operation, the system runs in level 0 mode while single user
 * and in level 1 mode while multiuser. If level 2 mode is desired while
 * running multiuser, it can be set in the multiuser startup script
 * (/etc/rc.local) using sysctl(8). If it is desired to run the system
 * in level 0 mode while multiuser, initialize the variable securelevel
 * in /sys/kern/kern_sysctl.c to -1. Note that it is NOT initialized to
 * zero as that would allow the vmunix binary to be patched to -1.
 * Without initialization, securelevel loads in the BSS area which only
 * comes into existence when the kernel is loaded and hence cannot be
 * patched by a stalking hacker.
 */
extern int16_t securelevel;		/* system security level */

extern	char version[];		/* system version */

/*
 * Nblkdev is the number of entries (rows) in the block switch.
 * Used in bounds checking on major device numbers.
 */
extern int16_t	nblkdev;

/*
 * Number of character switch entries.
 */
extern int16_t	nchrdev;

/* UNUSED: int	mpid;			/* generic for unique process id's */
/* UNUSED: char	runin;			/* scheduling flag */
/* UNUSED: char	runout;			/* scheduling flag */
/* UNUSED: int	runrun;			/* scheduling flag */
/* UNUSED: char	curpri;			/* more scheduling */

/* UNUSED: u_int	maxmem;			/* actual max memory per process */

/* UNUSED: u_int	nswap;			/* size of swap space */
extern int16_t	updlock;		/* lock for sync */
extern daddr_t	rablock;		/* block to be read ahead */
extern dev_t	rootdev;		/* device of the root */
/* UNUSED: dev_t	dumpdev;		/* device to take dumps on */
/* UNUSED: long	dumplo;			/* offset into dumpdev */
/* UNUSED: dev_t	swapdev;		/* swapping device */
/* UNUSED: dev_t	pipedev;		/* pipe device */
int16_t	nodev();		/* no device function used in bdevsw/cdevsw */

/* UNUSED: extern	int icode[];		/* user init code */
/* UNUSED: extern	int szicode;		/* its size */

/* UNUSED: daddr_t	bmap(); */

/* UNUSED: ubadr_t	clstaddr;		/* UNIBUS virtual address of clists */

/* UNUSED: extern int	cputype;	/* type of cpu = 40, 44, 45, 60, or 70 */

/*
 * Structure of the system-entry table
 */
#if UNUSED
extern struct sysent
{
	char	sy_narg;		/* total number of arguments */
	int	(*sy_call)();		/* handler */
} sysent[];
#endif

/* UNUSED: int	noproc;			/* no one is running just now */
/* UNUSED: char	*panicstr; */
/* UNUSED: int	boothowto;		/* reboot flags, from boot */
/* UNUSED: int	selwait; */

/* casts to keep lint happy */
#ifdef lint
#define	insque(q,p)	_insque((caddr_t)q,(caddr_t)p)
#define	remque(q)	_remque((caddr_t)q)
#endif

/* UNUSED: extern	bool_t	sep_id;		/* separate I/D */
/* UNUSED: extern	char	regloc[];	/* offsets of saved user registers (trap.c) */
#endif
