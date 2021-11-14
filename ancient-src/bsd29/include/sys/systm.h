#ifdef	KERNEL
/*
 * Random set of variables
 * used by more than one
 * routine.
 */
extern	struct	inode	*rootdir;	/* pointer to inode of root directory */
/* UNUSED struct	proc	*runq;		/* head of linked list of running processes */
/* UNUSED struct	proc	*maxproc;	/* current high water mark in proc table */
/* UNUSED extern	int16_t cputype;		/* type of cpu = 40, 44, 45, 60, or 70 */
/* UNUSED int	lbolt;			/* clock ticks since time was last updated */
extern	time_t	time;			/* time in sec from 1970 */
/* UNUSED ubadr_t	clstaddr;		/* UNIBUS virtual address of clists */

/*
 * Nblkdev is the number of entries
 * (rows) in the block switch.
 * Used in bounds checking on major
 * device numbers.
 */
extern	int16_t	nblkdev;

extern	int16_t	nchrdev; 		/* Number of character switch entries. */
/* UNUSED int	mpid;			/* generic for unique process id's */
/* UNUSED bool_t	runin;			/* set when sched wants to swap someone in */
/* UNUSED bool_t	runout;			/* set when sched is out of work */
/* UNUSED bool_t	runrun;			/* reschedule at next opportunity */
/* UNUSED char	curpri;			/* p_pri of current process */
/* UNUSED size_t	maxmem;			/* actual max memory per process */
/* UNUSED u_short	*lks;			/* pointer to clock device */
/* UNUSED extern	daddr_t	swplo;		/* block number of swap space */
/* UNUSED extern	int	nswap;		/* size of swap space */
extern int16_t	updlock;		/* lock for sync */
extern daddr_t	rablock;		/* block to be read ahead */
/* UNUSED extern	char	regloc[];	/* offsets of saved user registers (trap.c) */
/* UNUSED extern	char	msgbuf[];	/* saved "printf" characters */
extern	dev_t	rootdev;	/* device of the root */
/* UNUSED extern	dev_t	swapdev;	/* swapping device */
/* UNUSED extern	dev_t	pipedev;	/* pipe device */
/* UNUSED extern	int	hz;		/* Ticks/second of the clock */
/* UNUSED extern	int	timezone;	/* Minutes westward from Greenwich */
/* UNUSED extern	bool_t	dstflag;	/* Daylight Saving Time applies here */
extern	int16_t	nmount;		/* number of mountable file systems */
/* UNUSED extern	int	nfile;		/* number of in core file structures */
/* UNUSED extern	int	ninode;		/* number of in core inodes */
/* UNUSED extern	int	nproc;		/* maximum number of processes */
/* UNUSED extern	int	ntext;		/* maximum number of shared text segments */
extern	int16_t	nbuf;		/* number of buffers in buffer cache */
extern	int16_t	bsize;		/* size of buffers */
/* UNUSED extern	int	nclist;		/* number of character lists */

/*
 *  Pointers to ends of variable-sized tables
 * (point to first unused entry, e.g. proc[NPROC])
 */
extern	struct	mount	*mountNMOUNT;
extern	struct	file	*fileNFILE;
extern	struct	inode	*inodeNINODE;
/* UNUSED extern	struct	proc	*procNPROC; */
/* UNUSED extern	struct	text	*textNTEXT; */

#ifdef	UCB_AUTOBOOT
/* UNUSED extern	dev_t	dumpdev;	/* device for automatic dump on panic */
/* UNUSED extern	daddr_t	dumplo;		/* starting block on dumpdev */
#endif

/* UNUSED extern	int	icode[];	/* user init code */
/* UNUSED extern	int	szicode;	/* its size */

#ifdef	CGL_RTP
/* UNUSED extern	struct	proc	*rtpp;		/* pointer to real time process entry */
/* UNUSED extern	int	wantrtp;		/* real-time proc is ready to run */
#endif
extern	time_t	bootime;

#ifdef	UCB_AUTOBOOT
/* UNUSED extern	int	bootflags; */
#endif

/* UNUSED extern	bool_t	sep_id;		/* Do we have separate I/D? */

/* UNUSED extern dev_t	getmdev(); */
extern daddr_t	bmap(struct inode *ip, daddr_t bn, int16_t rwflg);
/* UNUSED extern memaddr	malloc(); */
extern struct	inode	*ialloc(dev_t dev);
extern struct	inode	*iget(dev_t dev, ino_t ino);
extern struct	inode	*owner(int16_t follow);
extern struct	inode	*maknode(int16_t mode);
extern struct	inode	*namei(int16_t (*func)(), int16_t flag, int16_t follow);
extern struct	buf	*alloc(dev_t dev);
extern struct	buf	*getblk(dev_t dev, daddr_t blkno);
extern struct	buf	*geteblk();
extern struct	buf	*bread(dev_t dev, daddr_t blkno);
extern struct	buf	*breada(bsd29_dev_t dev, bsd29_daddr_t blkno, bsd29_daddr_t rablkno);
extern struct	filsys	*getfs(dev_t dev);
extern struct	file	*getf(int16_t f);
extern struct	file	*falloc();
extern int16_t	uchar();

extern caddr_t	mapin();

#ifdef	NOKA5
#define	mapout(bp)	/* unused */
#endif

/*
 * Instrumentation
 */

/* UNUSED extern	int	dk_busy;*/
	/*
	 * sy_time contains counters for time in user (0,1), nice (2,3),
	 * system (4,5) and idle (6,7) with no I/O active (even)
	 * or some disc active (odd).
	 */
/* UNUSED extern	long	sy_time[8]; */
/* UNUSED extern	int	ndisk;			/* number of disks monitored */
/* UNUSED extern	long	dk_time[]; */
/* UNUSED extern	long	dk_numb[]; */
/* UNUSED extern	long	dk_wds[]; */

/* UNUSED extern	long	tk_nin; */
/* UNUSED extern	long	tk_nout; */

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
/* UNUSED extern	struct	sysent	sysent[];	/* local system call entry table */
/* UNUSED extern	struct	sysent	syslocal[];	/* local system call entry table */
/* UNUSED extern	int	nlocalsys;		/* number of local syscalls in table */
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
