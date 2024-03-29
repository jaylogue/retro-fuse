/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)user.h	1.6 (2.11BSD) 1999/9/13
 */

#ifdef KERNEL
#include "bsd211/machine/fperr.h"
#include "bsd211/h/dir.h"
#include "bsd211/h/exec.h"
#include "bsd211/h/time.h"
#include "bsd211/h/resource.h"
#else
#include <machine/fperr.h>
#include <sys/dir.h>
#include <sys/exec.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

/*
 * data that doesn't need to be referenced while the process is swapped.
 * The user block is USIZE*64 bytes long; resides at virtual kernel loc
 * 0140000; contains the system stack (and possibly network stack) per
 * user; is cross referenced with the proc structure for the same process.
 */
#define	MAXCOMLEN	MAXNAMLEN	/* <= MAXNAMLEN, >= sizeof(ac_comm) */

struct	pcb {			/* fake pcb structure */
	int16_t	(*pcb_sigc)();	/* pointer to trampoline code in user space */
};

struct	fps {
	int16_t	u_fpsr;		/* FP status register */
	double	u_fpregs[6];	/* FP registers */
};

struct user {
	struct	pcb u_pcb;
	struct	fps u_fps;
	int16_t	u_fpsaved;		/* FP regs saved for this proc */
	struct	fperr u_fperr;		/* floating point error save */
	struct	proc *u_procp;		/* pointer to proc structure */
	int16_t	*u_ar0;			/* address of users saved R0 */
	char	u_comm[MAXCOMLEN + 1];

/* syscall parameters, results and catches */
	int16_t	u_arg[6];		/* arguments to current system call */
	void	*u_ap;			/* pointer to arglist */
	label_t	u_qsave;		/* for non-local gotos on interrupts */
	union {				/* syscall return values */
		struct	{
			int16_t	R_val1;
			int16_t	R_val2;
		} u_rv;
#define	r_val1	u_rv.R_val1
#define	r_val2	u_rv.R_val2
		int32_t	r_long;
		off_t	r_off;
		time_t	r_time;
	} u_r;
	int16_t	u_error;		/* return error code */
	char	u_dummy0;

/* 1.1 - processes and protection */
	uid_t	u_uid;			/* effective user id */
	uid_t	u_svuid;		/* saved user id */
	uid_t	u_ruid;			/* real user id */
	gid_t	u_svgid;		/* saved group id */
	gid_t	u_rgid;			/* real group id */
	gid_t	u_groups[NGROUPS];	/* groups, 0 terminated */

/* 1.2 - memory management */
	size_t	u_tsize;		/* text size (clicks) */
	size_t	u_dsize;		/* data size (clicks) */
	size_t	u_ssize;		/* stack size (clicks) */
	label_t	u_ssave;		/* label variable for swapping */
	label_t	u_rsave;		/* save info when exchanging stacks */
	int16_t	u_uisa[16];		/* segmentation address prototypes */
	int16_t	u_uisd[16];		/* segmentation descriptor prototypes */
	char	u_sep;			/* flag for I and D separation */
	char	dummy1;			/* room for another char */
					/* overlay information */
	struct	u_ovd {			/* automatic overlay data */
		int16_t	uo_curov;	/* current overlay */
		int16_t	uo_ovbase;	/* base of overlay area, seg. */
		u_short	uo_dbase;	/* start of data, clicks */
		u_short	uo_ov_offst[NOVL+1];	/* overlay offsets in text */
		int16_t	uo_nseg;	/* number of overlay seg. regs. */
	} u_ovdata;

/* 1.3 - signal management */
	int16_t	(*u_signal[NSIG])();	/* disposition of signals */
	int32_t	u_sigmask[NSIG];	/* signals to be blocked */
	int32_t	u_sigonstack;		/* signals to take on sigstack */
	int32_t	u_sigintr;		/* signals that interrupt syscalls */
	int32_t	u_oldmask;		/* saved mask from before sigpause */
	int16_t	u_code;			/* ``code'' to trap */
	char	dummy2;			/* Room for another flags byte */
	char	u_psflags;		/* Process Signal flags */
	struct	sigaltstack u_sigstk;	/* signal stack info */

/* 1.4 - descriptor management */
	struct	file *u_ofile[NOFILE];	/* file structures for open files */
	char	u_pofile[NOFILE];	/* per-process flags of open files */
	int16_t	u_lastfile;		/* high-water mark of u_ofile */
#define	UF_EXCLOSE 	0x1		/* auto-close on exec */
#define	UF_MAPPED 	0x2		/* mapped from device */
	struct	inode *u_cdir;		/* current directory */
	struct	inode *u_rdir;		/* root directory of current process */
	struct	tty *u_ttyp;		/* controlling tty pointer */
	dev_t	u_ttyd;			/* controlling tty dev */
	int16_t	u_cmask;		/* mask for file creation */

/* 1.5 - timing and statistics */
	struct	k_rusage u_ru;		/* stats for this proc */
	struct	k_rusage u_cru;		/* sum of stats for reaped children */
	struct	k_itimerval u_timer[2];	/* profile/virtual timers */
	int32_t	u_start;
	char	u_acflag;
	char	u_dupfd;		/* XXX - see kern_descrip.c/fdopen */

	struct uprof {			/* profile arguments */
		int16_t	*pr_base;	/* buffer base */
		unsigned pr_size;	/* buffer size */
		unsigned pr_off;	/* pc offset */
		unsigned pr_scale;	/* pc scaling */
	} u_prof;

/* 1.6 - resource controls */
	struct	rlimit u_rlimit[RLIM_NLIMITS];
	struct	quota *u_quota;		/* user's quota structure */

/* namei & co. */
	struct	nameicache {		/* last successful directory search */
		off_t nc_prevoffset;	/* offset at which last entry found */
		ino_t nc_inumber;	/* inum of cached directory */
		dev_t nc_dev;		/* dev of cached directory */
	} u_ncache;
	int16_t	u_xxxx[2];		/* spare */
	char	u_login[MAXLOGNAME];	/* setlogin/getlogin */
	int16_t	u_stack[1];		/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
};

#include <bsd211/h/errno.h>

#ifdef KERNEL
extern	struct user u;
#endif
