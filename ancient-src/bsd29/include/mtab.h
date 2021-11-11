#define	MAXMOUNT	16
#define	PATHSIZE	32
#define	DEVSIZE		32
#define	MOUNTTABLE	"/etc/mtab"
struct	mtab	{
	dev_t	m_dev;		 	/* major/minor device numbers */
	char	m_dname[DEVSIZE];	/* block device pathname */
	char	m_name[PATHSIZE];	/* mounted on pathname */
};
