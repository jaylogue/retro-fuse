/*
 * Random set of variables
 * used by more than one
 * routine.
 */
extern char	canonb[CANBSIZ];	/* buffer for erase and kill (#@) */
extern struct inode *rootdir;		/* pointer to inode of root directory */
extern struct proc *runq;		/* head of linked list of running processes */
extern int16_t	cputype;		/* type of cpu =40, 45, or 70 */
extern int16_t	lbolt;			/* time of day in 60th not in time */
extern time_t	time;			/* time in sec from 1970 */

/*
 * Nblkdev is the number of entries
 * (rows) in the block switch. It is
 * set in binit/bio.c by making
 * a pass over the switch.
 * Used in bounds checking on major
 * device numbers.
 */
extern int16_t	nblkdev;

/*
 * Number of character switch entries.
 * Set by cinit/tty.c
 */
extern int16_t	nchrdev;

extern int16_t	mpid;			/* generic for unique process id's */
extern char	runin;			/* scheduling flag */
extern char	runout;			/* scheduling flag */
extern char	runrun;			/* scheduling flag */
extern char	curpri;			/* more scheduling */
extern int16_t	maxmem;			/* actual max memory per process */
extern physadr	lks;			/* pointer to clock device */
extern daddr_t	swplo;			/* block number of swap space */
extern int16_t	nswap;			/* size of swap space */
extern int16_t	updlock;		/* lock for sync */
extern daddr_t	rablock;		/* block to be read ahead */
extern char	regloc[];	/* locs. of saved user registers (trap.c) */
extern char	msgbuf[MSGBUFS];	/* saved "printf" characters */
extern dev_t	rootdev;		/* device of the root */
extern dev_t	swapdev;		/* swapping device */
extern dev_t	pipedev;		/* pipe device */
extern int16_t	icode[];	/* user init code */
extern int16_t	szicode;	/* its size */

extern dev_t getmdev();
extern daddr_t	bmap(struct inode *ip, daddr_t bn, int16_t rwflg);
extern struct inode *ialloc(dev_t dev);
extern struct inode *iget(dev_t dev, ino_t ino);
extern struct inode *owner();
extern struct inode *maknode(int16_t mode);
extern struct inode *namei(int16_t (*func)(), int16_t flag);
extern struct buf *alloc(dev_t dev);
extern struct buf *getblk(dev_t dev, daddr_t blkno);
extern struct buf *geteblk();
extern struct buf *bread(dev_t dev, daddr_t blkno);
extern struct buf *breada(dev_t dev, daddr_t blkno, daddr_t rablkno);
extern struct filsys *getfs(dev_t dev);
extern struct file *getf(int16_t f);
extern struct file *falloc();
extern int16_t	uchar();
/*
 * Instrumentation
 */
extern int16_t	dk_busy;
extern int32_t	dk_time[32];
extern int32_t	dk_numb[3];
extern int32_t	dk_wds[3];
extern int32_t	tk_nin;
extern int32_t	tk_nout;

/*
 * Structure of the system-entry table
 */
extern struct sysent {
	char	sy_narg;		/* total number of arguments */
	char	sy_nrarg;		/* number of args in registers */
	int16_t	(*sy_call)();		/* handler */
} sysent[];
