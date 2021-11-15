#include	<bsd29/include/errno.h>
#include	<bsd29/include/sys/fperr.h>
/* UNUSED #include	<a.out.h> */

/*
 * The user structure.
 * One allocated per process.
 * Contains all per process data
 * that doesn't need to be referenced
 * while the process is swapped.
 * The user block is USIZE*64 bytes
 * long; resides at virtual kernel
 * loc 0140000; contains the system
 * stack per user; is cross referenced
 * with the proc structure for the
 * same process.
 */

#define	EXCLOSE	01

struct	user
{
	label_t	u_rsav;			/* save info when exchanging stacks */
	int16_t	u_dummy;		/* historical dreg; u_fperr is below */
	int16_t	u_fpsaved;		/* FP regs saved for this proc */
	struct {
		int16_t	u_fpsr;		/* FP status register */
		int16_t	u_fpregs[6];	/* FP registers */
	} u_fps;
	char	u_segflg;		/* IO flag: 0:user D; 1:system; 2:user I */
	char	u_error;		/* return error code */
	int16_t	u_uid;			/* effective user id */
	int16_t	u_gid;			/* effective group id */
	int16_t	u_ruid;			/* real user id */
	int16_t	u_rgid;			/* real group id */
	struct	proc	*u_procp;	/* pointer to proc structure */
	void	*u_ap;			/* pointer to arglist */
	union {				/* syscall return values */
		struct	{
			int16_t	r_val1;
			int16_t	r_val2;
		};
		off_t	r_off;
		time_t	r_time;
	} u_r;
	caddr_t	u_base;			/* base address for IO */
	uint16_t	u_count;		/* bytes remaining for IO */
	off_t	u_offset;		/* offset in file for IO */
	struct	inode	*u_cdir;	/* inode of current directory */
	struct	inode	*u_rdir;	/* root directory of current process */
	char	u_dbuf[DIRSIZ];		/* current pathname component */
	caddr_t	u_dirp;			/* pathname pointer */
	struct	direct	u_dent;		/* current directory entry */
	struct	inode	*u_pdir;	/* inode of parent directory of dirp */
	int16_t	u_uisa[16];		/* segmentation address prototypes */
	int16_t	u_uisd[16];		/* segmentation descriptor prototypes */
	struct	file	*u_ofile[NOFILE];/* pointers to file structures of open files */
	char	u_pofile[NOFILE];	/* per-process flags of open files */
	int16_t	u_arg[5];		/* arguments to current system call */
	size_t	u_tsize;		/* text size (clicks) */
	size_t	u_dsize;		/* data size (clicks) */
	size_t	u_ssize;		/* stack size (clicks) */
	label_t	u_qsav;			/* saved regs for interrupted syscall */
	label_t	u_ssav;			/* saved regs for newproc/expand */
	int16_t	(*u_signal[NSIG])();	/* disposition of signals */
	time_t	u_utime;		/* this process's user time */
	time_t	u_stime;		/* this process's system time */
	time_t	u_cutime;		/* sum of children's utimes */
	time_t	u_cstime;		/* sum of children's stimes */
	int16_t	*u_ar0;			/* address of user's saved R0 */
	struct {			/* profile arguments */
		int16_t	*pr_base;	/* buffer base */
		uint16_t	pr_size;	/* buffer size */
		uint16_t	pr_off;		/* pc offset */
		uint16_t	pr_scale;	/* pc scaling */
	} u_prof;
	char	u_intflg;		/* catch intr from sys */
	char	u_sep;			/* flag for I and D separation */
	struct	tty	*u_ttyp;	/* controlling tty pointer */
	dev_t	u_ttyd;			/* controlling tty dev */
	struct {			/* header of executable file */
		int16_t	ux_mag;		/* magic number */
		uint16_t	ux_tsize;	/* text size */
		uint16_t	ux_dsize;	/* data size */
		uint16_t	ux_bsize;	/* bss size */
		uint16_t	ux_ssize;	/* symbol table size */
		uint16_t	ux_entloc;	/* entry location */
		uint16_t	ux_unused;
		uint16_t	ux_relflg;
	} u_exdata;
	char	u_comm[DIRSIZ];
	time_t	u_start;
	char	u_acflag;
	int16_t	u_fpflag;		/* unused now, will be later */
	int16_t	u_cmask;		/* mask for file creation */
#ifdef	UCB_LOGIN
	int16_t	u_login;		/* login flag: 0 or ttyslot */
	char	u_crn[4];
#endif
#ifdef	MENLO_OVLY
	struct	u_ovd	{		/* automatic overlay data */
		int16_t	uo_curov;	/* current overlay */
		int16_t	uo_ovbase;	/* base of overlay area, seg. */
		uint16_t	uo_dbase;	/* start of data, clicks */
		uint16_t	uo_ov_offst[1+NOVL];	/* overlay offsets in text */
		int16_t	uo_nseg;	/* number of overlay seg. regs. */
	}	u_ovdata;
#endif
	struct	fperr	u_fperr;	/* floating point error save */
#ifdef	MENLO_JCL
	char	u_eosys;		/* action on syscall termination */
#endif
#ifdef	UCB_SYMLINKS
	struct	buf *u_sbuf;		/* Buffer cache of symbolic name */
	int16_t	u_slength;		/* Length of symbolic name */
	int16_t	u_soffset;		/* Pointer into buffer */
#endif
	int16_t	u_stack[1];
					/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
};

#ifdef	KERNEL
extern	struct	user	u;
#endif

#ifdef	MENLO_JCL
/* u_eosys values */
#define	JUSTRETURN	0
#define	RESTARTSYS	1
#define	SIMULATERTI	2
#endif
