#ifdef	KERNEL
/*
 * Random set of variables
 * used by more than one
 * routine.
 */
struct	inode	*rootdir;	/* pointer to inode of root directory */
struct	proc	*runq;		/* head of linked list of running processes */
struct	proc	*maxproc;	/* current high water mark in proc table */
extern	cputype;		/* type of cpu = 40, 44, 45, 60, or 70 */
int	lbolt;			/* clock ticks since time was last updated */
time_t	time;			/* time in sec from 1970 */
ubadr_t	clstaddr;		/* UNIBUS virtual address of clists */

/*
 * Nblkdev is the number of entries
 * (rows) in the block switch.
 * Used in bounds checking on major
 * device numbers.
 */
int	nblkdev;

int	nchrdev; 		/* Number of character switch entries. */
int	mpid;			/* generic for unique process id's */
bool_t	runin;			/* set when sched wants to swap someone in */
bool_t	runout;			/* set when sched is out of work */
bool_t	runrun;			/* reschedule at next opportunity */
char	curpri;			/* p_pri of current process */
size_t	maxmem;			/* actual max memory per process */
u_short	*lks;			/* pointer to clock device */
extern	daddr_t	swplo;		/* block number of swap space */
extern	int	nswap;		/* size of swap space */
int	updlock;		/* lock for sync */
daddr_t	rablock;		/* block to be read ahead */
extern	char	regloc[];	/* offsets of saved user registers (trap.c) */
extern	char	msgbuf[];	/* saved "printf" characters */
extern	dev_t	rootdev;	/* device of the root */
extern	dev_t	swapdev;	/* swapping device */
extern	dev_t	pipedev;	/* pipe device */
extern	int	hz;		/* Ticks/second of the clock */
extern	int	timezone;	/* Minutes westward from Greenwich */
extern	bool_t	dstflag;	/* Daylight Saving Time applies here */
extern	int	nmount;		/* number of mountable file systems */
extern	int	nfile;		/* number of in core file structures */
extern	int	ninode;		/* number of in core inodes */
extern	int	nproc;		/* maximum number of processes */
extern	int	ntext;		/* maximum number of shared text segments */
extern	int	nbuf;		/* number of buffers in buffer cache */
extern	int	bsize;		/* size of buffers */
extern	int	nclist;		/* number of character lists */

/*
 *  Pointers to ends of variable-sized tables
 * (point to first unused entry, e.g. proc[NPROC])
 */
extern	struct	mount	*mountNMOUNT;
extern	struct	file	*fileNFILE;
extern	struct	inode	*inodeNINODE;
extern	struct	proc	*procNPROC;
extern	struct	text	*textNTEXT;

#ifdef	UCB_AUTOBOOT
extern	dev_t	dumpdev;	/* device for automatic dump on panic */
extern	daddr_t	dumplo;		/* starting block on dumpdev */
#endif

extern	int	icode[];	/* user init code */
extern	int	szicode;	/* its size */

#ifdef	CGL_RTP
struct	proc	*rtpp;		/* pointer to real time process entry */
int	wantrtp;		/* real-time proc is ready to run */
#endif
time_t	bootime;

#ifdef	UCB_AUTOBOOT
int	bootflags;
#endif

extern	bool_t	sep_id;		/* Do we have separate I/D? */

dev_t	getmdev();
daddr_t	bmap();
memaddr	malloc();
struct	inode	*ialloc();
struct	inode	*iget();
struct	inode	*owner();
struct	inode	*maknode();
struct	inode	*namei();
struct	buf	*alloc();
struct	buf	*getblk();
struct	buf	*geteblk();
struct	buf	*bread();
struct	buf	*breada();
struct	filsys	*getfs();
struct	file	*getf();
struct	file	*falloc();
int	uchar();

caddr_t	mapin();

#ifdef	NOKA5
#define	mapout(bp)	/* unused */
#endif

/*
 * Instrumentation
 */

int	dk_busy;
	/*
	 * sy_time contains counters for time in user (0,1), nice (2,3),
	 * system (4,5) and idle (6,7) with no I/O active (even)
	 * or some disc active (odd).
	 */
long	sy_time[8];
extern	int	ndisk;			/* number of disks monitored */
extern	long	dk_time[];
extern	long	dk_numb[];
extern	long	dk_wds[];

long	tk_nin;
long	tk_nout;

#endif	KERNEL

#ifdef	DISKMON
struct	ioinfo	{
	long	nread;
	long	nreada;
	long	ncache;
	long	nwrite;
};
#endif

/*
 * Structure of the system-entry table
 */
struct	sysent	{			/* system call entry table */
	char	sy_narg;		/* total number of arguments */
	char	sy_nrarg;		/* number of args in registers */
	int	(*sy_call)();		/* handler */
};

#ifdef	KERNEL
extern	struct	sysent	sysent[];	/* local system call entry table */
extern	struct	sysent	syslocal[];	/* local system call entry table */
extern	int	nlocalsys;		/* number of local syscalls in table */
#endif	KERNEL

#define	SYSINDIR	0		/* ordinal of indirect sys call */
#define	SYSLOCAL	58		/* ordinal of local indirect call */

/*
 * Structure of the disk driver partition tables
 */
struct	size	{
	daddr_t	nblocks;
	short	cyloff;
};

/* operations performed by namei */
#define	LOOKUP		0	/* perform name lookup only */
#define	CREATE		1	/* setup for file creation */
#define	DELETE		2	/* setup for file deletion */
