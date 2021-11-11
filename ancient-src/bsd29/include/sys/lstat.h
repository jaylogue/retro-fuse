#ifdef	UCB_QUOTAS
struct lstat
{
	char	ls_flag;
	char	ls_count;	/* reference count */
	dev_t	ls_dev;		/* device where inode resides */
	ino_t	ls_number;	/* i number, 1-to-1 with device address */
	u_short	ls_mode;
	short	ls_nlink;	/* directory entries */
	short	ls_uid;		/* owner */
	short	ls_gid;		/* group of owner */
	off_t	ls_size;	/* size of file */
	union {
		struct {
			daddr_t	ls_addr[NADDR];	/* if normal file/directory */
			daddr_t	ls_lastr;	/* last logical block read  */
		};
		struct {
			daddr_t	ls_rdev;		/* ls_addr[0] */
#ifdef	MPX_FILS
			struct group ls_group;	/* multiplexer group file */
#endif
		};
		struct {
			daddr_t	ls_qused;
			daddr_t	ls_qmax;
		};
	} ls_un;
	time_t	ls_atime;	/* access time */
	time_t	ls_mtime;	/* modification time */
	time_t	ls_ctime;	/* creation time */
};
#endif	UCB_QUOTAS
