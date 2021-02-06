/*
 * Used to dissect integer device code
 * into major (driver designation) and
 * minor (driver parameter) parts.
 */
struct devst
{
	char	d_minor;
	char	d_major;
};

/*
 * Declaration of block device
 * switch. Each entry (row) is
 * the only link between the
 * main unix code and the driver.
 * The initialization of the
 * device switches is in the
 * file conf.c.
 */
extern struct	bdevsw
{
	void	(*d_open)(int16_t dev, int16_t flag);
	void	(*d_close)(int16_t dev, int16_t flag);
	void	(*d_strategy)(struct buf *bp);
	struct devtab	*d_tab;
} bdevsw[];

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
 * Character device switch.
 */
extern struct	cdevsw
{
	int16_t	(*d_open)();
	int16_t	(*d_close)();
	int16_t	(*d_read)();
	int16_t	(*d_write)();
	int16_t	(*d_sgtty)();
} cdevsw[];

/*
 * Number of character switch entries.
 * Set by cinit/tty.c
 */
extern int16_t	nchrdev;
