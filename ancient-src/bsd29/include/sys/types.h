typedef	char		bool_t;		/* Boolean */
typedef	char *     	caddr_t;  	/* virtual core address */
typedef	uint16_t	comp_t;		/* "floating pt": 3 bits base 8 exp, 13 bits fraction */
typedef	int32_t       	daddr_t;  	/* disk address */
typedef	int16_t        	dev_t;    	/* device code */
typedef	uint16_t	ino_t;     	/* i-node number */
#ifdef	MENLO_KOV
typedef	short		label_t[7];	/* regs 2-7 and __ovno */
#else
typedef	int16_t        	label_t[6]; 	/* program status */
#endif
typedef uint16_t	memaddr;	/* core or swap address */
typedef	int32_t       	off_t;    	/* offset in file */
typedef	struct {int16_t r[1];}*	physadr;
typedef uint16_t	size_t;		/* size of process segments */
typedef	int32_t       	time_t;   	/* a time */
typedef	int32_t		ubadr_t;	/* UNIBUS address */
typedef	uint16_t	u_short;
/* UNUSED typedef	int16_t		void;		/* Embarassing crock for Ritchie C compiler */
#ifdef	UCB_NET
typedef	unsigned short	u_int;
typedef	long		u_long;		/* watch out! */
typedef	char		u_char;		/* watch out! */
typedef struct  fd_set { long fds_bits[1]; } fd_set;
#endif

	/* selectors and constructor for device code */
#define	major(x)  	(int16_t)(((uint16_t)(x)>>8))
#define	minor(x)  	(int16_t)((x)&0377)
#define	makedev(x,y)	(dev_t)((x)<<8|(y))
