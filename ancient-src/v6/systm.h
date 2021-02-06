/*
 * Random set of variables
 * used by more than one
 * routine.
 */
extern char	canonb[CANBSIZ];	/* buffer for erase and kill (#@) */
extern int16_t	coremap[CMAPSIZ];	/* space for core allocation */
extern int16_t	swapmap[SMAPSIZ];	/* space for swap allocation */
extern struct inode	*rootdir;		/* pointer to inode of root directory */
extern int16_t	cputype;		/* type of cpu =40, 45, or 70 */
extern int16_t	execnt;			/* number of processes in exec */
extern int16_t	lbolt;			/* time of day in 60th not in time */
extern int16_t	time[2];		/* time in sec from 1970 */
extern int16_t	tout[2];		/* time of day of next sleep */
/*
 * The callout structure is for
 * a routine arranging
 * to be called by the clock interrupt
 * (clock.c) with a specified argument,
 * in a specified amount of time.
 * Used, for example, to time tab
 * delays on teletypes.
 */
extern struct	callo
{
	int16_t	c_time;		/* incremental time */
	int16_t	c_arg;		/* argument to routine */
	int16_t	(*c_func)();	/* routine */
} callout[NCALL];
/*
 * Mount structure.
 * One allocated on every mount.
 * Used to find the super block.
 */
extern struct	mount
{
	int16_t	m_dev;		/* device mounted */
	struct buf	*m_bufp;	/* pointer to superblock */
	struct inode	*m_inodp;	/* pointer to mounted on inode */
} mount[NMOUNT];
extern int16_t	mpid;			/* generic for unique process id's */
extern char	runin;			/* scheduling flag */
extern char	runout;			/* scheduling flag */
extern char	runrun;			/* scheduling flag */
extern char	curpri;			/* more scheduling */
extern int16_t	maxmem;			/* actual max memory per process */
extern int16_t	*lks;			/* pointer to clock device */
extern int16_t	rootdev;		/* dev of root see conf.c */
extern int16_t	swapdev;		/* dev of swap see conf.c */
extern int16_t	swplo;			/* block number of swap space */
extern int16_t	nswap;			/* size of swap space */
extern int16_t	updlock;		/* lock for sync */
extern int16_t	rablock;		/* block to be read ahead */
extern char	regloc[];		/* locs. of saved user registers (trap.c) */
