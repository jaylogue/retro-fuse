/*
 * Text structure.
 * One allocated per pure
 * procedure on swap device.
 * Manipulated by text.c
 */
struct text
{
	short	x_daddr;	/* segment's disk address (relative to swplo) */
	short	x_caddr;	/* core address, if loaded */
	short	x_size;		/* size (clicks) */
	struct	inode *x_iptr;	/* inode of prototype */
	char	x_count;	/* reference count */
	char	x_ccount;	/* number of loaded references */
	char	x_flag;		/* traced, written flags */
};

#ifdef	KERNEL
extern	struct	text	text[];
#endif

#define	XTRC	01		/* Text may be written, exclusive use */
#define	XWRIT	02		/* Text written into, must swap out */
#define	XLOAD	04		/* Currently being read from file */
#define	XLOCK	010		/* Being swapped in or out */
#define	XWANT	020		/* Wanted for swapping */

/* arguments to xswap: */
#define	X_OLDSIZE	(-1)	/* the old size is the same as current */
#define	X_DONTFREE	0	/* save core image (for parent in newproc) */
#define	X_FREECORE	01	/* free core space after swap */
