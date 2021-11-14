/*
 * Declaration of block device
 * switch. Each entry (row) is
 * the only link between the
 * main unix code and the driver.
 * The initialization of the
 * device switches is in the
 * file conf.c.
 */
struct bdevsw	{
	int16_t	(*d_open)(dev_t dev, int16_t flag);
	int16_t	(*d_close)(dev_t dev, int16_t flag);
	int16_t	(*d_strategy)(struct buf *bp);
	void	(*d_root)();		/* root attach routine */
	struct	buf	*d_tab;
};
#ifdef	KERNEL
extern	struct	bdevsw	bdevsw[];
#endif

/*
 * Character device switch.
 */
struct cdevsw	{
	int	(*d_open)();
	int	(*d_close)();
	int	(*d_read)();
	int	(*d_write)();
	int	(*d_ioctl)();
	int	(*d_stop)();
	struct	tty	*d_ttys;
#ifdef	UCB_NET
	int	(*d_select)();
#endif
};
#ifdef	KERNEL
extern	struct	cdevsw	cdevsw[];
#endif

/*
 * tty line control switch.
 */
struct linesw	{
	int	(*l_open)();		/* entry to discipline */
	int	(*l_close)();		/* exit from discipline */
	int	(*l_read)();		/* read routine */
	char	*(*l_write)();		/* write routine */
	int	(*l_ioctl)();		/* ioctl interface to driver */
	int	(*l_input)();		/* received character input */
	int	(*l_output)();		/* character output */
	int	(*l_modem)();		/* modem control notification */
};
#ifdef	KERNEL
extern	struct	linesw	linesw[];
#endif
