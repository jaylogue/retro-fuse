/*
 * Accounting structures;
 * these use a comp_t type which is a 3 bits base 8
 * exponent, 13 bit fraction ``floating point'' number.
 */

struct	acct
{
	char	ac_comm[10];		/* Accounting command name */
	comp_t	ac_utime;		/* Accounting user time */
	comp_t	ac_stime;		/* Accounting system time */
	comp_t	ac_etime;		/* Accounting elapsed time */
	time_t	ac_btime;		/* Beginning time */
	short	ac_uid;			/* Accounting user ID */
	short	ac_gid;			/* Accounting group ID */
	short	ac_mem;			/* average memory usage */
	comp_t	ac_io;			/* number of disk IO blocks */
	dev_t	ac_tty;			/* control typewriter */
	char	ac_flag;		/* Accounting flag */
#ifdef	UCB_LOGIN
	char	ac_crn[4];		/* Accounting charge number */
	short	ac_magic;		/* Magic number for synchronization */
#endif
};

#if	defined(KERNEL) && defined(ACCT)
extern	struct	acct	acctbuf;
extern	struct	inode	*acctp;		/* inode of accounting file */
#endif

#define	AFORK	01			/* has executed fork, but no exec */
#define	ASU	02			/* used super-user privileges */

#ifdef	UCB_SUBM
#define	ASUBM	04			/* submitted process */
#endif

#ifdef	UCB_LOGIN
#define	AMAGIC	052525			/* magic number */
#endif
