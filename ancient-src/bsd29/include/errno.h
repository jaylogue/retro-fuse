/*	errno.h	4.1	82/12/28	*/

/*
 * Error codes
 */

#define	EPERM		1		/* Not owner */
#define	ENOENT		2		/* No such file or directory */
#define	ESRCH		3		/* No such process */
#define	EINTR		4		/* Interrupted system call */
#define	EIO		5		/* I/O error */
#define	ENXIO		6		/* No such device or address */
#define	E2BIG		7		/* Arg list too long */
#define	ENOEXEC		8		/* Exec format error */
#define	EBADF		9		/* Bad file number */
#define	ECHILD		10		/* No children */
#define	EAGAIN		11		/* No more processes */
#define	ENOMEM		12		/* Not enough core */
#define	EACCES		13		/* Permission denied */
#define	EFAULT		14		/* Bad address */
#define	ENOTBLK		15		/* Block device required */
#define	EBUSY		16		/* Exclusive use facility busy */
#define	EEXIST		17		/* File exists */
#define	EXDEV		18		/* Cross-device link */
#define	ENODEV		19		/* No such device */
#define	ENOTDIR		20		/* Not a directory*/
#define	EISDIR		21		/* Is a directory */
#define	EINVAL		22		/* Invalid argument */
#define	ENFILE		23		/* File table overflow */
#define	EMFILE		24		/* Too many open files */
#define	ENOTTY		25		/* Inappropriate ioctl for device */
#define	ETXTBSY		26		/* Text file busy */
#define	EFBIG		27		/* File too large */
#define	ENOSPC		28		/* No space left on device */
#define	ESPIPE		29		/* Illegal seek */
#define	EROFS		30		/* Read-only file system */
#define	EMLINK		31		/* Too many links */
#define	EPIPE		32		/* Broken pipe */

/* math software */
#define	EDOM		33		/* Argument too large */
#define	ERANGE		34		/* Result too large */

/* disk quotas */
#define	EQUOT		35		/* Disk quota exceeded */

/* symbolic links */
#define	ELOOP		36		/* Too many levels of symbolic links */

#ifdef UNUSED
/* non-blocking and interrupt i/o */
#define	EWOULDBLOCK	37		/* Operation would block */

/*
 *	The following errors relate specifically to the
 *	TCP/IP implementation (UCB_NET).
 */

#define	EINPROGRESS	38		/* Operation now in progress */
#define	EALREADY	39		/* Operation already in progress */
/* ipc/network software */

	/* argument errors */
#define	ENOTSOCK	40		/* Socket operation on non-socket */
#define	EDESTADDRREQ	41		/* Destination address required */
#define	EMSGSIZE	42		/* Message too long */
#define	EPROTOTYPE	43		/* Protocol wrong type for socket */
#define	ENOPROTOOPT	44		/* Protocol not available */
#define	EPROTONOSUPPORT	45		/* Protocol not supported */
#define	ESOCKTNOSUPPORT	46		/* Socket type not supported */
#define	EOPNOTSUPP	47		/* Operation not supported on socket */
#define	EPFNOSUPPORT	48		/* Protocol family not supported */
#define	EAFNOSUPPORT	49		/* Address family not supported by protocol family */
#define	EADDRINUSE	50		/* Address already in use */
#define	EADDRNOTAVAIL	51		/* Can't assign requested address */

	/* operational errors */
#define	ENETDOWN	52		/* Network is down */
#define	ENETUNREACH	53		/* Network is unreachable */
#define	ENETRESET	54		/* Network dropped connection on reset */
#define	ECONNABORTED	55		/* Software caused connection abort */
#define	ECONNRESET	56		/* Connection reset by peer */
#define	ENOBUFS		57		/* No buffer space available */
#define	EISCONN		58		/* Socket is already connected */
#define	ENOTCONN	59		/* Socket is not connected */
#define	ESHUTDOWN	60		/* Can't send after socket shutdown */
#define	ETOOMANYREFS	61		/* Too many references: can't splice */
#define	ETIMEDOUT	62		/* Connection timed out */
#define	ECONNREFUSED	63		/* Connection refused */

	/* */
#define	ENAMETOOLONG	64		/* File name too long */

/* should be rearranged */
#define	EHOSTDOWN	65		/* Host is down */
#define	EHOSTUNREACH	66		/* No route to host */
#endif /* UNUSED */