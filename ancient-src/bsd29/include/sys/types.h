typedef	char		bool_t;		/* Boolean */
typedef	char *     	caddr_t;  	/* virtual core address */
typedef	unsigned short	comp_t;		/* "floating pt": 3 bits base 8 exp, 13 bits fraction */
typedef	long       	daddr_t;  	/* disk address */
typedef	short        	dev_t;    	/* device code */
typedef	unsigned short	ino_t;     	/* i-node number */
#ifdef	MENLO_KOV
typedef	short		label_t[7];	/* regs 2-7 and __ovno */
#else
typedef	short        	label_t[6]; 	/* program status */
#endif
typedef unsigned	memaddr;	/* core or swap address */
typedef	long       	off_t;    	/* offset in file */
typedef	struct {short r[1];}*	physadr;
typedef unsigned	size_t;		/* size of process segments */
typedef	long       	time_t;   	/* a time */
typedef	long		ubadr_t;	/* UNIBUS address */
typedef	unsigned short	u_short;
typedef	short		void;		/* Embarassing crock for Ritchie C compiler */
#ifdef	UCB_NET
typedef	unsigned short	u_int;
typedef	long		u_long;		/* watch out! */
typedef	char		u_char;		/* watch out! */
typedef struct  fd_set { long fds_bits[1]; } fd_set;
#endif

	/* selectors and constructor for device code */
#define	major(x)  	(short)(((unsigned)(x)>>8))
#define	minor(x)  	(short)((x)&0377)
#define	makedev(x,y)	(dev_t)((x)<<8|(y))
