/*
 * Structure of the super-block
 */
/* This structure now represents the superblock as it resides in memory,
 * with all integer values stored in the host's native byte order.
 *
 * See below for the on-disk forms of superblocks for various versions
 * of the v7 filesystem.
 */
struct	filsys {
	uint16_t s_isize;	/* size in blocks of i-list */
	daddr_t	s_fsize;   	/* size in blocks of entire volume */
	short  	s_nfree;   	/* number of addresses in s_free */
	daddr_t	s_free[MAX_NICFREE];/* free block list */
	int16_t	s_ninode;  	/* number of i-nodes in s_inode */
	ino_t  	s_inode[MAX_NICINOD];/* free i-node list */
	char   	s_flock;   	/* lock during free list manipulation */
	char   	s_ilock;   	/* lock during i-list manipulation */
	char   	s_fmod;    	/* super block modified flag */
	char   	s_ronly;   	/* mounted read-only flag */
	time_t 	s_time;    	/* last super block update */
	/* remainder not maintained by this version of the system */
	daddr_t	s_tfree;   	/* total free blocks*/
	ino_t  	s_tinode;  	/* total free inodes */
	int16_t	s_m;       	/* interleave factor */
	int16_t	s_n;       	/* " " */
	char   	s_fname[6];	/* file system name */
	char   	s_fpack[6];	/* file system pack name */
};

/** On-disk form of original v7 superblock (as used on the PDP-11)
 */
struct v7_superblock {

#define V7_NICFREE 50
#define V7_NICINOD 100

    uint16_t    s_isize;
    int32_t     s_fsize;
    int16_t     s_nfree;
    int32_t     s_free[V7_NICFREE];
    int16_t     s_ninode;
    uint16_t    s_inode[V7_NICINOD];
    char        s_flock;
    char        s_ilock;
    char        s_fmod;
    char        s_ronly;
    int32_t     s_time;
    int32_t     s_tfree;
    uint16_t    s_tinode;
    int16_t     s_m;
    int16_t     s_n;
    char        s_fname[6];
    char        s_fpack[6];
} __attribute__((packed));

/** On-disk form of standard Unix System III superblock (as used on the PDP-11 and VAX)
 */
struct v7_superblock_sys3 {

#define V7_SYS3_NICFREE 50
#define V7_SYS3_NICINOD 100

    uint16_t    s_isize;
    int32_t     s_fsize;
    int16_t     s_nfree;
    int32_t     s_free[V7_SYS3_NICFREE];
    int16_t     s_ninode;
    uint16_t    s_inode[V7_SYS3_NICINOD];
    char        s_flock;
    char        s_ilock;
    char        s_fmod;
    char        s_ronly;
    int32_t     s_time;
	int16_t     s_dinfo[4];
    int32_t     s_tfree;
    uint16_t    s_tinode;
    char        s_fname[6];
    char        s_fpack[6];
} __attribute__((packed));

/* On-disk form of Xenix 2.x superblock
 */
struct v7_superblock_xenix2 {

#define V7_XENIX2_NICFREE 50
#define V7_XENIX2_NICINOD 100
#define V7_XENIX2_CLEAN_MARKER 0106

    uint16_t    s_isize;
    int32_t     s_fsize;
    int16_t     s_nfree;
    int32_t     s_free[V7_XENIX2_NICFREE];
    int16_t     s_ninode;
    uint16_t    s_inode[V7_XENIX2_NICINOD];
    char        s_flock;
    char        s_ilock;
    char        s_fmod;
    char        s_ronly;
    int32_t     s_time;
    int32_t     s_tfree;
    uint16_t    s_tinode;
    int16_t     s_m;
    int16_t     s_n;
    char        s_fname[6];
    char        s_fpack[6];
    char        s_clean;
} __attribute__((packed));

/* On-disk form of Xenix 3.x superblock
 */
struct v7_superblock_xenix3 {

#define V7_XENIX3_NICFREE 50
#define V7_XENIX3_NICINOD 100
#define V7_XENIX3_MAGIC 0x2b5544
#define V7_XENIX3_B512 1
#define V7_XENIX3_B1024 2
#define V7_XENIX3_NBSFILL 51

    uint16_t    s_isize;
    int32_t     s_fsize;
    int16_t     s_nfree;
    int32_t     s_free[V7_XENIX3_NICFREE];
    int16_t     s_ninode;
    uint16_t    s_inode[V7_XENIX3_NICINOD];
    char        s_flock;
    char        s_ilock;
    char        s_fmod;
    char        s_ronly;
    int32_t     s_time;
    int32_t     s_tfree;
    uint16_t    s_tinode;
    int16_t     s_dinfo[4];
    char        s_fname[6];
    char        s_fpack[6];
    char        s_clean;
    char        s_fill[V7_XENIX3_NBSFILL];
    int32_t     s_magic;
    int32_t     s_type;
} __attribute__((packed));

/* On-disk form of IBM PC Xenix 1.x/2.x superblock
 */
struct v7_superblock_ibmpcxenix {

#define V7_IBMPCXENIX_NICFREE 100
#define V7_IBMPCXENIX_NICINOD 100
#define V7_IBMPCXENIX_MAGIC 0x2b5544
#define V7_IBMPCXENIX_B512 1
#define V7_IBMPCXENIX_B1024 2
#define V7_IBMPCXENIX_NBSFILL 370

    uint16_t    s_isize;
    int32_t     s_fsize;
    int16_t     s_nfree;
    int32_t     s_free[V7_IBMPCXENIX_NICFREE];
    int16_t     s_ninode;
    uint16_t    s_inode[V7_IBMPCXENIX_NICINOD];
    char        s_flock;
    char        s_ilock;
    char        s_fmod;
    char        s_ronly;
    int32_t     s_time;
    int32_t     s_tfree;
    uint16_t    s_tinode;
    int16_t     s_dinfo[4];
    char        s_fname[6];
    char        s_fpack[6];
    char        s_clean;
    char        s_fill[V7_IBMPCXENIX_NBSFILL];
    int32_t     s_magic;
    int32_t     s_type;
} __attribute__((packed));
