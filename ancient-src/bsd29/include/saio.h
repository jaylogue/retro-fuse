/*
 * header file for standalone package
 */

/*
 * io block: includes an
 * inode, cells for the use of seek, etc,
 * and a buffer.
 */
struct	iob {
	char	i_flgs;
	struct	inode	i_ino;
	int	i_unit;
	daddr_t	i_boff;
	daddr_t	i_cyloff;
	off_t	i_offset;
	daddr_t	i_bn;
	char	*i_ma;
	int	i_cc;
#ifndef	UCB_NKB
	char	i_buf[512];
#else
	char	i_buf[BSIZE];
#endif
};

#define	F_READ	01
#define	F_WRITE	02
#define	F_ALLOC	04
#define	F_FILE	010
#define	READ	F_READ
#define	WRITE	F_WRITE

/*
 * device switch
 */
struct	devsw {
	char	*dv_name;
	int	(*dv_strategy)();
	int	(*dv_open)();
	int	(*dv_close)();
};

struct	devsw	devsw[];

#define	NBUFS	4


#ifndef	UCB_NKB
char	b[NBUFS][512];
#else
char	b[NBUFS][BSIZE];
#endif
daddr_t	blknos[NBUFS];

#define	NFILES	4
struct	iob	iob[NFILES];

/*
 * Set to which 64Kb segment the code is physically running in.
 * Must be set by the user's main (or thereabouts).
 */
int	segflag;
