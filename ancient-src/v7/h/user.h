/*
 * The user structure.
 * One allocated per process.
 * Contains all per process data
 * that doesn't need to be referenced
 * while the process is swapped.
 * The user block is USIZE*64 bytes
 * long; resides at virtual kernel
 * loc 140000; contains the system
 * stack per user; is cross referenced
 * with the proc structure for the
 * same process.
 */

#define	EXCLOSE	01

struct	user
{
	label_t	u_rsav;			/* save info when exchanging stacks */
	int16_t	u_fper;			/* FP error register */
	int16_t	u_fpsaved;		/* FP regs saved for this proc */
	struct {
		int16_t	u_fpsr;		/* FP status register */
		double	u_fpregs[6];	/* FP registers */
	} u_fps;
	char	u_segflg;		/* IO flag: 0:user D; 1:system; 2:user I */
	char	u_error;		/* return error code */
	int16_t	u_uid;			/* effective user id */
	int16_t	u_gid;			/* effective group id */
	int16_t	u_ruid;			/* real user id */
	int16_t	u_rgid;			/* real group id */
	struct proc *u_procp;		/* pointer to proc structure */
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
	uint16_t u_count;		/* bytes remaining for IO */
	off_t	u_offset;		/* offset in file for IO */
	struct inode *u_cdir;		/* pointer to inode of current directory */
	struct inode *u_rdir;		/* root directory of current process */
	char	u_dbuf[DIRSIZ];		/* current pathname component */
	caddr_t	u_dirp;			/* pathname pointer */
	struct direct u_dent;		/* current directory entry */
	struct inode *u_pdir;		/* inode of parent directory of dirp */
	int16_t	u_uisa[16];		/* prototype of segmentation addresses */
	int16_t	u_uisd[16];		/* prototype of segmentation descriptors */
	struct file *u_ofile[NOFILE];	/* pointers to file structures of open files */
	char	u_pofile[NOFILE];	/* per-process flags of open files */
	intptr_t	u_arg[5];		/* arguments to current system call */
	uint16_t u_tsize;		/* text size (clicks) */
	uint16_t u_dsize;		/* data size (clicks) */
	uint16_t u_ssize;		/* stack size (clicks) */
	label_t	u_qsav;			/* label variable for quits and interrupts */
	label_t	u_ssav;			/* label variable for swapping */
	int16_t	u_signal[NSIG];		/* disposition of signals */
	time_t	u_utime;		/* this process user time */
	time_t	u_stime;		/* this process system time */
	time_t	u_cutime;		/* sum of childs' utimes */
	time_t	u_cstime;		/* sum of childs' stimes */
	int16_t	*u_ar0;			/* address of users saved R0 */
	struct {			/* profile arguments */
		int16_t	*pr_base;	/* buffer base */
		uint16_t pr_size;	/* buffer size */
		uint16_t pr_off;	/* pc offset */
		uint16_t pr_scale;	/* pc scaling */
	} u_prof;
	char	u_intflg;		/* catch intr from sys */
	char	u_sep;			/* flag for I and D separation */
	struct tty *u_ttyp;		/* controlling tty pointer */
	dev_t	u_ttyd;			/* controlling tty dev */
	struct {			/* header of executable file */
		int16_t	ux_mag;		/* magic number */
		uint16_t ux_tsize;	/* text size */
		uint16_t ux_dsize;	/* data size */
		uint16_t ux_bsize;	/* bss size */
		uint16_t ux_ssize;	/* symbol table size */
		uint16_t ux_entloc;	/* entry location */
		uint16_t ux_unused;
		uint16_t ux_relflg;
	} u_exdata;
	char	u_comm[DIRSIZ];
	time_t	u_start;
	char	u_acflag;
	int16_t	u_fpflag;		/* unused now, will be later */
	int16_t	u_cmask;		/* mask for file creation */
	int16_t	u_stack[1];
					/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
};

extern struct user u;

/* u_error codes */
#define	EPERM	1
#define	ENOENT	2
#define	ESRCH	3
#define	EINTR	4
#define	EIO	5
#define	ENXIO	6
#define	E2BIG	7
#define	ENOEXEC	8
#define	EBADF	9
#define	ECHILD	10
#define	EAGAIN	11
#define	ENOMEM	12
#define	EACCES	13
#define	EFAULT	14
#define	ENOTBLK	15
#define	EBUSY	16
#define	EEXIST	17
#define	EXDEV	18
#define	ENODEV	19
#define	ENOTDIR	20
#define	EISDIR	21
#define	EINVAL	22
#define	ENFILE	23
#define	EMFILE	24
#define	ENOTTY	25
#define	ETXTBSY	26
#define	EFBIG	27
#define	ENOSPC	28
#define	ESPIPE	29
#define	EROFS	30
#define	EMLINK	31
#define	EPIPE	32
#define	EDOM	33
#define	ERANGE	34
