/*
 * Copyright (c) 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)types.h	1.4.1 (2.11BSD) 2000/2/28
 */

#ifndef _TYPES_
#define	_TYPES_

/*
 * Basic system types and major/minor device constructing/busting macros.
 */

/* major part of a device */
#define	major(x)	((int16_t)(((int16_t)(x)>>8)&0377))

/* minor part of a device */
#define	minor(x)	((int16_t)((x)&0377))

/* make a device number */
#define	makedev(x,y)	((dev_t)(((x)<<8) | (y)))

typedef	unsigned char	u_char;
typedef	uint16_t	u_short;
typedef	uint16_t	u_int;
typedef uint32_t	u_long;		/* see this! unsigned longs at last! */
typedef	uint16_t	ushort;		/* sys III compat */

#ifdef pdp11
typedef	struct	_physadr { short r[1]; } *physadr;
typedef	struct	label_t	{
	int16_t	val[8];			/* regs 2-7, __ovno and super SP */
} label_t;
#endif
typedef	struct	_quad { int32_t val[2]; } quad;
typedef	int32_t	daddr_t;
typedef	void *	caddr_t;
typedef	u_short	ino_t;
typedef	int32_t	swblk_t;
typedef	u_int	size_t;
typedef	int16_t	ssize_t;
typedef	int32_t	time_t;
typedef	int16_t	dev_t;
typedef	int32_t	off_t;
typedef	u_short	uid_t;
typedef	u_short	gid_t;
typedef	int16_t	pid_t;
typedef	u_short	mode_t;

#define	NBBY	8		/* number of bits in a byte */

#ifndef howmany
#define	howmany(x, y)	(((x)+((y)-1))/(y))
#endif

#include <bsd211/h/select.h>

typedef char	bool_t;		/* boolean */
typedef u_int	memaddr;	/* core or swap address */
typedef int32_t	ubadr_t;	/* unibus address */

#endif
