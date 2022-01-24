/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)conf.h	1.3 (2.11BSD Berkeley) 12/23/92
 */

/*
 * Declaration of block device
 * switch. Each entry (row) is
 * the only link between the
 * main unix code and the driver.
 * The initialization of the
 * device switches is in the
 * file conf.c.
 */
struct bdevsw
{
	int16_t	(*d_open)(dev_t dev, int16_t flag, int16_t mode);
	int16_t	(*d_close)(dev_t dev, int16_t flag, int16_t mode);
	int16_t	(*d_strategy)(struct buf *bp);
	int16_t	(*d_root)(caddr_t addr);		/* XXX root attach routine */
	daddr_t	(*d_psize)(dev_t dev);
	int16_t	d_flags;
};

#if defined(KERNEL) && !defined(SUPERVISOR)
extern struct	bdevsw bdevsw[];
#endif

/*
 * Character device switch.
 */
struct cdevsw
{
	int16_t	(*d_open)();
	int16_t	(*d_close)();
	int16_t	(*d_read)();
	int16_t	(*d_write)();
	int16_t	(*d_ioctl)();
	int16_t	(*d_stop)();
	struct tty *d_ttys;
	int16_t	(*d_select)();
	int16_t	(*d_strategy)();
};
#if defined(KERNEL) && !defined(SUPERVISOR)
extern struct	cdevsw cdevsw[];
#endif

/*
 * tty line control switch.
 */
struct linesw
{
	int16_t	(*l_open)();
	int16_t	(*l_close)();
	int16_t	(*l_read)();
	int16_t	(*l_write)();
	int16_t	(*l_ioctl)();
	int16_t	(*l_rint)();
	int16_t	(*l_rend)();
	int16_t	(*l_meta)();
	int16_t	(*l_start)();
	int16_t	(*l_modem)();
};
#if defined(KERNEL) && !defined(SUPERVISOR)
extern struct	linesw linesw[];
#endif
