/*
 * Each buffer in the pool is usually doubly linked into 2 lists:
 * the device with which it is currently associated (always)
 * and also on a list of blocks available for allocation
 * for other use (usually).
 * The latter list is kept in last-used order, and the two
 * lists are doubly linked to make it easy to remove
 * a buffer from one list when it was found by
 * looking through the other.
 * A buffer is on the available list, and is liable
 * to be reassigned to another disk block, if and only
 * if it is not marked BUSY.  When a buffer is busy, the
 * available-list pointers can be used for other purposes.
 * Most drivers use the forward ptr as a link in their I/O
 * active queue.
 * A buffer header contains all the information required
 * to perform I/O.
 * Most of the routines which manipulate these things
 * are in bio.c.
 */
struct buf
{
	short	b_flags;		/* see defines below */
	struct	buf *b_forw;		/* headed by d_tab of conf.c */
	struct	buf *b_back;		/*  "  */
	struct	buf *av_forw;		/* position on free list, */
	struct	buf *av_back;		/*     if not BUSY*/
	dev_t	b_dev;			/* major+minor device name */
	u_short	b_bcount;		/* transfer count */
	union	{
		caddr_t	b_addr;		/* low order core address */
		short	*b_words;	/* words for clearing */
		struct	filsys	*b_filsys;	/* superblocks */
		struct	dinode	*b_dino;/* ilist */
		daddr_t	*b_daddr;	/* indirect block */
	} b_un;
	daddr_t	b_blkno;		/* block # on device */
	char	b_xmem;			/* high order core address */
	char	b_error;		/* returned after I/O */
#ifdef	UCB_BHASH
	struct	buf *b_link;		/* hash links for buffer cache */
#endif
	u_short	b_resid;		/* words not transferred after error */
};

#ifdef	KERNEL
extern	struct	buf buf[];		/* The buffer pool itself */
extern	struct	buf bfreelist;		/* head of available list */
#endif

/*
 * These flags are kept in b_flags.
 */
#define	B_WRITE		0000000	/* non-read pseudo-flag */
#define	B_READ		0000001	/* read when I/O occurs */
#define	B_DONE		0000002	/* transaction finished */
#define	B_ERROR		0000004	/* transaction aborted */
#define	B_BUSY		0000010	/* not on av_forw/back list */
#define	B_PHYS		0000020	/* Physical IO potentially using UNIBUS map */
#define	B_MAP		0000040	/* This block has the UNIBUS map allocated */
#define	B_WANTED 	0000100	/* issue wakeup when BUSY goes off */
#define	B_AGE		0000200	/* place at head of free list when freed */
#define	B_ASYNC		0000400	/* don't wait for I/O completion */
#define	B_DELWRI 	0001000	/* don't write till block leaves free list */
#define	B_TAPE 		0002000	/* this is a magtape (no bdwrite) */
#define	B_RH70		0004000	/* this device is talking to an RH70 */
#define	B_UBAREMAP	0010000	/* buf's addr is UNIBUS virtual, not physical */
#define	B_BAD		0020000	/* diverted to replacement for bad sector */

/*
 * special redeclarations for
 * the head of the queue per
 * device driver.
 */
#define	b_actf		av_forw
#define	b_actl		av_back
#define	b_active	b_bcount
#define	b_errcnt	b_resid

/*
 * redeclaration used by disksort()
 */
#define	b_cylin		b_resid

/* arguments to chkphys */
#define	WORD	2		/* doing I/O by word */
#define	BYTE	1		/* doing I/O by byte */
