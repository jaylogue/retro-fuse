struct	lpdevice
{
	short	lpcs;
	short	lpdb;
};

struct	lxdevice
{
	short	lxcs;
	short	lxdb;
};

/* bits in lpcs */
#define	LP_ERR		0100000		/* error */
/* bits 14-8 are unused */
#define	LP_RDY		0000200		/* ready */
#define	LP_IE		0000100		/* interrupt enable */
/* bits 5-0 are unused */

/* bits in lxcs */
#define	LX_ERR		0100000		/* error */
/* bits 14-8 are unused */
#define	LX_RDY		0000200		/* ready */
#define	LX_IE		0000100		/* interrupt enable */
/* bits 5-0 are unused */

/* bits in lxdb */
/* bits 15-8 are unused */
#define	LXDB_PI		0000200		/* paper instruction */
/* bits 6-0 are data */
