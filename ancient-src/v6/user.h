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
struct user
{
	int16_t	u_rsav[2];		/* save r5,r6 when exchanging stacks */
	int16_t	u_fsav[25];		/* save fp registers */
					/* rsav and fsav must be first in structure */
	char	u_segflg;		/* flag for IO; user or kernel space */
	char	u_error;		/* return error code */
	char	u_uid;			/* effective user id */
	char	u_gid;			/* effective group id */
	char	u_ruid;			/* real user id */
	char	u_rgid;			/* real group id */
	int16_t	u_procp;		/* pointer to proc structure */
	char	*u_base;		/* base address for IO */
	uint16_t	u_count;		/* bytes remaining for IO */
	uint16_t	u_offset[2];		/* offset in file for IO */
	int16_t	*u_cdir;		/* pointer to inode of current directory */
	char	u_dbuf[DIRSIZ];		/* current pathname component */
	char	*u_dirp;		/* current pointer to inode */
	struct	{			/* current directory entry */
		int16_t	u_ino;
		char	u_name[DIRSIZ];
	} u_dent;
	struct inode	*u_pdir;		/* inode of parent directory of dirp */
	int16_t	u_uisa[16];		/* prototype of segmentation addresses */
	int16_t	u_uisd[16];		/* prototype of segmentation descriptors */
	int16_t	u_ofile[NOFILE];	/* pointers to file structures of open files */
	int16_t	u_arg[5];		/* arguments to current system call */
	int16_t	u_tsize;		/* text size (*64) */
	int16_t	u_dsize;		/* data size (*64) */
	int16_t	u_ssize;		/* stack size (*64) */
	int16_t	u_sep;			/* flag for I and D separation */
	int16_t	u_qsav[2];		/* label variable for quits and interrupts */
	int16_t	u_ssav[2];		/* label variable for swapping */
	int16_t	u_signal[NSIG];		/* disposition of signals */
	int16_t	u_utime;		/* this process user time */
	int16_t	u_stime;		/* this process system time */
	int16_t	u_cutime[2];		/* sum of childs' utimes */
	int16_t	u_cstime[2];		/* sum of childs' stimes */
	int16_t	*u_ar0;			/* address of users saved R0 */
	int16_t	u_prof[4];		/* profile arguments */
	char	u_intflg;		/* catch intr from sys */
					/* kernel stack per user
					 * extends from u + USIZE*64
					 * backward not to reach here
					 */
} u;

/* u_error codes */
#define	EFAULT	106
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
