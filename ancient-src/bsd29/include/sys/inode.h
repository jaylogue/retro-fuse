/*
 * The inode is the focus of
 * file activity in unix. There is a unique
 * inode allocated for each active file,
 * each current directory, each mounted-on
 * file, text file, and the root. An inode is 'named'
 * by its dev/inumber pair. (iget/iget.c)
 * Data, from mode on, is read in
 * from permanent inode on volume.
 */

#ifdef	UCB_NKB

#define	NADDR	7
#ifdef	MPX_FILS
#define	NINDEX	6
#endif

#else	UCB_NKB

#define	NADDR	13
#ifdef	MPX_FILS
#define	NINDEX	15
#endif	UCB_NKB

#endif

#ifdef	MPX_FILS
struct group {
	short	g_state;
	char	g_index;
	char	g_rot;
	struct	group	*g_group;
	struct	inode	*g_inode;
	struct	file	*g_file;
	short	g_rotmask;
	short	g_datq;
	struct	chan *g_chans[NINDEX];
};
#endif

struct	inode
{
	int	i_flag;
	int	i_count;	/* reference count */
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* i number, 1-to-1 with device address */
	u_short	i_mode;
	short	i_nlink;	/* directory entries */
	short	i_uid;		/* owner */
	short	i_gid;		/* group of owner */
	off_t	i_size;		/* size of file */
	union {
		struct {
			daddr_t	I_addr[NADDR];	/* if normal file/directory */
			daddr_t	I_lastr;	/* last logical block read (for read-ahead) */
		} i_f;
#define	i_addr	i_f.I_addr
#define	i_lastr	i_f.I_lastr
		struct	{
			daddr_t	I_rdev;		/* i_addr[0] */
#define	i_rdev	i_d.I_rdev
#ifdef	MPX_FILS
			struct	group I_group;	/* multiplexor group file */
#define	i_group	i_d.I_group
#endif
		} i_d;
#ifdef	UCB_QUOTAS
		struct  {
			daddr_t	I_qused;
			daddr_t	I_qmax;
		} i_q;
#define	i_qused	i_q.I_qused
#define	i_qmax	i_q.I_qmax
#endif
	} i_un;
#ifdef	UCB_QUOTAS
	struct inode	*i_quot;/* pointer to quota inode */
#endif
#ifdef	UCB_IHASH
	struct	inode *i_link;	/* link in hash chain (iget/iput/ifind) */
#endif
};


#ifdef	KERNEL
extern struct inode inode[];	/* The inode table itself */
#ifdef	MPX_FILS
struct inode *mpxip;		/* mpx virtual inode */
#endif
#endif

/* flags */
#define	ILOCK	01		/* inode is locked */
#define	IUPD	02		/* file has been modified */
#define	IACC	04		/* inode access time to be updated */
#define	IMOUNT	010		/* inode is mounted on */
#define	IWANT	020		/* some process waiting on lock */
#define	ITEXT	040		/* inode is pure text prototype */
#define	ICHG	0100		/* inode has been changed */
#define	IPIPE	0200		/* inode is a pipe */
#ifdef	UCB_QUOTAS
#define	IQUOT	0400		/* directory that has original quota pointer */
#endif

/* modes */
#define	IFMT	0170000		/* type of file */
#define		IFDIR	0040000	/* directory */
#define		IFCHR	0020000	/* character special */
#define		IFMPC	0030000	/* multiplexed char special */
#define		IFBLK	0060000	/* block special */
#define		IFMPB	0070000	/* multiplexed block special */
#define		IFREG	0100000	/* regular */
#define		IFLNK	0120000	/* symbolic link */
#define		IFQUOT	0140000	/* quota */
#define	ISUID	04000		/* set user id on execution */
#define	ISGID	02000		/* set group id on execution */
#define	ISVTX	01000		/* save swapped text even after use */
#define	IREAD	0400		/* read, write, execute permissions */
#define	IWRITE	0200
#define	IEXEC	0100

#ifdef	UCB_GRPMAST
#define	grpmast()	(u.u_uid == u.u_gid)
#endif
