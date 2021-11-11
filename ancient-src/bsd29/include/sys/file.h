/*
 * One file structure is allocated
 * for each open/creat/pipe call.
 * Main use is to hold the read/write
 * pointer associated with each open
 * file.
 */
struct	file
{
	char	f_flag;
	char	f_count;	/* reference count */
	struct	inode	*f_inode;	/* pointer to inode structure */
	union {
		off_t	f_offset;	/* read/write character pointer */
		struct	chan *f_chan;	/* mpx channel pointer */
#ifdef	UCB_NET
		struct	socket *f_Socket;
#endif
	} f_un;
};
#ifdef	UCB_NET
#define	f_socket        f_un.f_Socket
#endif

#ifdef	KERNEL
extern	struct file file[];	/* The file table itself */
#endif

/* flags */
#define	FREAD	01
#define	FWRITE	02
#define	FPIPE	04
#ifdef	MPX_FILS
#define	FMPX	010
#define	FMPY	020
#define	FMP	030
#endif
#ifdef	UCB_NET
#define	FSOCKET	040     /* descriptor of a socket */
#endif

/* flags supplied to access call */
#define	FACCESS_EXISTS	0x0	/* does file exist? */
#define	FACCESS_EXECUTE	0x1	/* is it executable by caller? */
#define	FACCESS_WRITE	0x2	/* is it writable by caller? */
#define	FACCESS_READ	0x4	/* is it readable by caller? */
#define	F_OK		FACCESS_EXISTS
#define	X_OK		FACCESS_EXECUTE
#define	W_OK		FACCESS_WRITE
#define	R_OK		FACCESS_READ

/* flags supplies to lseek call */
#define	FSEEK_ABSOLUTE	0x0	/* absolute offset */
#define	FSEEK_RELATIVE	0x1	/* relative to current offset */
#define	FSEEK_EOF	0x2	/* relative to end of file */
#define	L_SET		FSEEK_ABSOLUTE
#define	L_INCR		FSEEK_RELATIVE
#define	L_XTND		FSEEK_EOF

/* flags supplied to open call */
#define	FATT_RDONLY	0x0	/* open for reading only */
#define	FATT_WRONLY	0x1	/* open for writing only */
#define	FATT_RDWR	0x2	/* open for reading and writing */
#define	O_RDONLY	FATT_RDONLY
#define	O_WRONLY	FATT_WRONLY
#define	O_RDWR		FATT_RDWR
