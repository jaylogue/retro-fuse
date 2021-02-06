/*
 * One file structure is allocated
 * for each open/creat/pipe call.
 * Main use is to hold the read/write
 * pointer associated with each open
 * file.
 */
extern struct	file
{
	char	f_flag;
	char	f_count;	/* reference count */
	struct inode	*f_inode;	/* pointer to inode structure */
	uint16_t f_offset[2];	/* read/write character pointer */
} file[NFILE];

/* flags */
#define	FREAD	01
#define	FWRITE	02
#define	FPIPE	04
