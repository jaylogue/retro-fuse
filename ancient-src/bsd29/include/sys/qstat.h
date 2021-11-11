struct qstat
{
	char	qs_flag;
	char	qs_count;	/* reference count */
	dev_t	qs_dev;		/* device where inode resides */
	ino_t	qs_number;	/* i number, 1-to-1 with device address */
	u_short	qs_mode;
	short	qs_nlink;	/* directory entries */
	short	qs_uid;		/* owner */
	short	qs_gid;		/* group of owner */
	off_t	qs_size;	/* size of file */
	union {
		struct {
			daddr_t	qs_addr[NADDR];	/* if normal file/directory */
			daddr_t	qs_lastr;	/* last logical block read  */
		};
		struct {
			daddr_t	qs_rdev;		/* qs_addr[0] */
#ifdef	MPX_FILS
			struct group qs_group;	/* multiplexer group file */
#endif
		};
		struct {
			daddr_t	qs_qused;
			daddr_t	qs_qmax;
		};
	} qs_un;
	time_t	qs_atime;	/* access time */
	time_t	qs_mtime;	/* modification time */
	time_t	qs_ctime;	/* creation time */
};
