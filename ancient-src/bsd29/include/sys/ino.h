/*
 * Inode structure as it appears on
 * a disk block.
 */
struct dinode
{
	uint16_t	di_mode;     	/* mode and type of file */
	int16_t	di_nlink;    	/* number of links to file */
	int16_t	di_uid;      	/* owner's user id */
	int16_t	di_gid;      	/* owner's group id */
	off_t	di_size;     	/* number of bytes in file */
	char  	di_addr[40];	/* disk block addresses */
	time_t	di_atime;   	/* time last accessed */
	time_t	di_mtime;   	/* time last modified */
	time_t	di_ctime;   	/* time created */
} __attribute__((packed));

#ifndef	UCB_NKB
#define	INOPB	8	/* 8 inodes per block */
/*
 *	39 of the address bytes are used;
 *	13 addresses of 3 bytes each.
 */
#endif

#if	UCB_NKB == 1
#define	INOPB	16	/* 16 inodes per BSIZE block */
/*
 *	21 of the address bytes are used;
 *	7 addresses of 3 bytes each.
 */
#endif
