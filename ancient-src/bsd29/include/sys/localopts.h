/*
 * local system options - included by whoami.h
 */

/*
 *	System Changes Which May Have User Visible Effects
 */

/* #define	CGL_RTP		/* Allow one real time process */
#define	DISKMON			/* Iostat disk monitoring */
/* #define	UCB_GRPMAST		/* Group master accounts */
/* #define	UCB_LOGIN		/* login sys call is available */
/* #define	UCB_PGRP		/* Count process limit by process group */
/* #define	UCB_QUOTAS		/* Dynamic file system quotas */
/* #define	UCB_SCRIPT		/* Shell scripts can specify interpreter */
/* #define	UCB_SUBM		/* "submit" processing */
/* #define	UCB_SYMLINKS		/* Symbolic links */
#define	TEXAS_AUTOBAUD	/* tty image mode to support autobauding */

#define	UCB_AUTOBOOT		/* System is able to reboot itself */
#define	UCB_UPRINTF		/* Send error messages to user */
#define	UCB_VHANGUP		/* Revoke control tty access when user leaves */
#define	UCB_LOAD		/* load average and uptime */
#define	UCB_METER		/* vmstat performance metering */
/* #define	OLDTTY		/* old line discipline */
#define	UCB_NTTY		/* new tty driver */
#define	MENLO_JCL		/* Job Control */
#define	MENLO_OVLY		/* process text overlays */
#define	VIRUS_VFORK		/* vfork system call */
#define	UCB_RENICE		/* renice system call */

/*
 * Internal changes
 * It should not be necessary to use these in user products.
 * Note: The UCB_NKB flag requires changes to UNIX boot pgms
 *	 as well as changes to dump, restore, icheck, dcheck, ncheck, mkfs.
 *	 It includes the options previously known as UCB_SMINO (smaller
 *	 inodes, NADDR = 7) and UCB_MOUNT (multiple superblocks per internal
 * 	 buffer).
 */


#define	NOKA5			/* KA5 not used except for buffers and clists */
				/* (_end must be before 0120000) */
/* #define	UCB_FRCSWAP		/* Force swap on expand/fork */
#define	UCB_BHASH		/* hashed buffer accessing */
/* #define	UCB_CLIST		/* Clists moved out of kernel data space */
#define	UCB_DEVERR		/* Print device errors in mnemonics */
#define	UCB_ECC			/* Disk drivers should do ECC if possible */
#define	BADSECT			/* Bad-sector forwarding */
/* #define	UCB_DBUFS		/* Use one raw buffer per drive */
#define	UCB_FSFIX		/* Crash resistant filesystems */
#define	UCB_IHASH		/* hashed inode table */
#define	UCB_ISRCH		/* circular inode search */
#define	UCB_NKB		1	/* "n" KB byte system buffers (not just bool) */
#define	UNFAST			/* Don't use inline.h macro expansion speedups*/
#define	SMALL			/* for small sys: smaller hash queues, etc. */
/*
 * Options determined by machine type:
 *	machine type set in whoami.h
 */
#if	PDP11 == GENERIC
#	define	MENLO_KOV
#	define	KERN_NONSEP		/* kernel is not separate I/D */
#else
#   if	PDP11 <= 40 || PDP11 == 60
#	define	MENLO_KOV
#	define	NONSEPARATE
#	define	KERN_NONSEP		/* kernel is not separate I/D */
#   endif
#endif

#if	PDP11 == 44 || PDP11 == 70 || PDP11 == 24 || PDP11 == GENERIC
#	define	UNIBUS_MAP
#endif

/*
 * Standard Bell V7 features you may or may not want
 *
 */
/* #define	MPX_FILS		/* hooks for multiplexed files */
/* #define	ACCT			/* process accounting */
#define	INSECURE		/* don't clear setuid, setgid bits on write */
#define	DIAGNOSTIC		/* misc. diagnostic loops and checks */
/* #define	DISPLAY		/* 11/70 or 45 display routine */
/*
 *	Note: to enable profiling, the :splfix script must be changed
 *	to use spl6 instead of spl7 (see conf/:splfix.profile).
 */
/* #define	PROFILE		/* System profiling w/KW11P clock */

/*
 *  UCB_NET requires that the additional files in /usr/net/sys
 *  be merged in here-- only the hooks are installed on this ifdef.
 */
/* #define	UCB_NET		/* UCB TCP/IP Kernel */
