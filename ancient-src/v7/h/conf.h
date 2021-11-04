/*
 * Declaration of block device
 * switch. Each entry (row) is
 * the only link between the
 * main unix code and the driver.
 * The initialization of the
 * device switches is in the
 * file conf.c.
 */
extern struct bdevsw
{
	int16_t	(*d_open)(dev_t dev, int16_t flag);
	int16_t	(*d_close)(dev_t dev, int16_t flag);
	int16_t	(*d_strategy)(struct buf *bp);
	struct buf *d_tab;
} bdevsw[];

/*
 * Character device switch.
 */
extern struct cdevsw
{
	int16_t	(*d_open)();
	int16_t	(*d_close)();
	int16_t	(*d_read)();
	int16_t	(*d_write)();
	int16_t	(*d_ioctl)();
	int16_t	(*d_stop)();
	struct tty *d_ttys;
} cdevsw[];

/*
 * tty line control switch.
 */
extern struct linesw
{
	int16_t	(*l_open)();
	int16_t	(*l_close)();
	int16_t	(*l_read)();
	char	*(*l_write)();
	int16_t	(*l_ioctl)();
	int16_t	(*l_rint)();
	int16_t	(*l_rend)();
	int16_t	(*l_meta)();
	int16_t	(*l_start)();
	int16_t	(*l_modem)();
} linesw[];
