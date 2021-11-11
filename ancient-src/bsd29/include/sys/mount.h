/*
 * Mount structure.
 * One allocated on every mount.
 * Used to hold the super block.
 */
struct	mount
{
	dev_t	m_dev;			/* device mounted */
	struct	inode	*m_inodp;	/* pointer to mounted on inode */
	struct	filsys	m_filsys;	/* superblock data */
};

# ifdef KERNEL
extern	struct	mount	mount[];
# endif
