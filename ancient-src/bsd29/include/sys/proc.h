/*
 * One structure allocated per active
 * process. It contains all data needed
 * about the process while the
 * process may be swapped out.
 * Other per process data (user.h)
 * is swapped with the process.
 */
struct	proc {
	char	p_stat;
	char	p_pri;		/* priority, negative is high */
#ifndef	MENLO_JCL
	char	p_flag;
#else
	short	p_flag;
#endif
	char	p_time;		/* resident time for scheduling */
	char	p_cpu;		/* cpu usage for scheduling */
	char	p_nice;		/* nice for cpu usage */
#ifdef	UCB_METER
	char	p_slptime;	/* secs sleeping */
				/* there is room for a char here */
#endif
#ifdef	MENLO_JCL
	char	p_cursig;
	long	p_sig;		/* signals pending to this process */
	long	p_siga0;	/* low bit of 2 bit signal action */
	long	p_siga1;	/* high bit of 2 bit signal action */
#define	p_ignsig	p_siga0	/* ignored signal mask */
#else
	short	p_sig;		/* signals pending to this process */
#endif
#if	defined(MENLO_JCL) || defined(VIRUS_VFORK)
	struct	proc	*p_pptr;/* pointer to parent's process structure */
#endif
	short	p_uid;		/* user id, used to direct tty signals */
	short	p_pgrp;		/* name of process group leader */
	short	p_pid;		/* unique process id */
	short	p_ppid;		/* process id of parent */
		/*
		 * union (supercedes xproc)
		 * to replace part with times
		 * to be passed to parent process
		 * in ZOMBIE state.
		 */
	union {
	    struct {
#ifdef	VIRUS_VFORK
		memaddr	P_addr;		/* address of u. area */
		memaddr	P_daddr;	/* address of data area */
		memaddr	P_saddr;	/* address of stack area */
		size_t	P_dsize;	/* size of data area (clicks) */
		size_t	P_ssize;	/* size of stack segment (clicks) */
#else
		memaddr	P_addr;		/* address of swappable image */
		size_t	P_size;		/* size of swappable image (clicks) */
#endif
		caddr_t	P_wchan;	/* event process is awaiting */
		struct	text	*P_textp;/* pointer to text structure */
		struct	proc	*P_link;/* linked list of running processes */
		short	P_clktim;	/* time to alarm clock signal */
	    } p_p;
#define	p_addr		p_un.p_p.P_addr
#ifdef	VIRUS_VFORK
#define	p_daddr		p_un.p_p.P_daddr
#define	p_saddr		p_un.p_p.P_saddr
#define	p_dsize		p_un.p_p.P_dsize
#define	p_ssize		p_un.p_p.P_ssize
#else
#define	p_size		p_un.p_p.P_size
#endif
#define	p_wchan 	p_un.p_p.P_wchan
#define	p_textp		p_un.p_p.P_textp
#define	p_link		p_un.p_p.P_link
#define	p_clktim 	p_un.p_p.P_clktim
	    struct {
		short	Xp_xstat;	/* Exit status for wait */
		time_t	Xp_utime;	/* user time, this proc */
		time_t	Xp_stime;	/* system time, this proc */
#ifdef	UCB_LOGIN
		short	Xp_login;	/* login flag */
#endif
	    } p_xp;
#define	xp_xstat	p_xp.Xp_xstat
#define	xp_utime	p_xp.Xp_utime
#define	xp_stime	p_xp.Xp_stime
#ifdef	UCB_LOGIN
#define	xp_login	p_xp.Xp_login
#endif
	} p_un;
};

#ifdef	KERNEL
extern struct proc proc[];	/* the proc table itself */
#endif

/* stat codes */
#define	SSLEEP	1		/* awaiting an event */
#define	SWAIT	2		/* (abandoned state) */
#define	SRUN	3		/* running */
#define	SIDL	4		/* intermediate state in process creation */
#define	SZOMB	5		/* intermediate state in process termination */
#define	SSTOP	6		/* process being traced */

/* flag codes */
#define	SLOAD	01		/* in core */
#define	SSYS	02		/* scheduling process */
#define	SLOCK	04		/* process cannot be swapped */
#define	SSWAP	010		/* process is being swapped out */
#define	STRC	020		/* process is being traced */
#define	SWTED	040		/* another tracing flag */
#define	SULOCK	0100		/* user settable lock in core */
#ifdef	MENLO_JCL
#define	SDETACH	0200		/* detached inherited by init */
#define	SNUSIG	0400		/* using new signal mechanism */
#endif
#ifdef	VIRUS_VFORK
#define	SVFORK	01000		/* child in vfork, using parent's data */
#define	SVFPARENT 02000		/* parent in vfork, waiting for child */
#define	SVFDONE	04000		/* parent has relesed child in vfork */
#endif
#ifdef	UCB_NET
#define	SSEL	010000		/* selecting, cleared if rescan needed */
#define	STIMO	020000		/* timing out during sleep */
#endif

#ifdef	VIRUS_VFORK
/* arguments to expand() to expand specified segment */
#define	S_DATA	0
#define	S_STACK	1
#endif
