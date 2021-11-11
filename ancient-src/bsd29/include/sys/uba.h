/*
 *	Structure to access UNIBUS map registers.
 */
struct	ubmap	{
	short	ub_lo;
	short	ub_hi;
};

#define	UBMAP	((struct ubmap *) 0170200)
#define	UBPAGE	020000			/* size of UNIBUS map segment */

/*
 *	BUF_UBADDR is the UNIBUS address of buffers
 *	if we have a UNIBUS map, as distinguished from bpaddr,
 *	which is the physical address in clicks.
 */
#define	BUF_UBADDR	020000

/*
 *	Bytes to UNIBUS pages.
 */
#define	btoub(b)	((((long)(b)) + ((long)(UBPAGE - 1))) / ((long)UBPAGE))

/*
 *	Number of UNIBUS registers required by n objects of size s.
 */
#define	nubreg(n,s)	btoub(((long) (n)) * ((long) (s)))

/*
 *	Set UNIBUS register r to point at physical address p (in bytes).
 */
#define	setubregno(r,p)		{				\
				UBMAP[r].ub_lo = loint(p);	\
				UBMAP[r].ub_hi = hiint(p);	\
				}

/*
 *	Point the appropriate UNIBUS register at a kernel
 *	virtual data address (in clicks).  V must be less
 *	than btoc(248K) (not currently used).
 */
#define	pointubreg(v,sep)	{					\
					ubadr_t	x;			\
					short	regno;			\
					regno = ((v) >> 7) & 037;	\
					x = (ubadr_t) (v) & ~01;	\
					setubregno(regno, x);		\
				}

#ifdef	KERNEL
extern	bool_t	ubmap;		/* Do we have UNIBUS map registers? */
#ifdef	UCB_NET
int	ub_inited;		/* UNIBUS map initialized yet? */
#endif
#endif
