/*
 * Structure of the floating point error register save/return
 */

struct fperr
{
	short	f_fec;
	caddr_t	f_fea;
};

/*
 * Bits in the floating point status register
 */
#define	FPS_FER		0100000		/* floating error */
#define	FPS_FID		0040000		/* interrupt disable */
/* bit 13-12 are unused */
#define	FPS_FIUV	0004000		/* interrupt on undefined variable */
#define	FPS_FIU		0002000		/* interrupt on underflow */
#define	FPS_FIV		0001000		/* interrupt on overflow */
#define	FPS_FIC		0000400		/* interrupt on integer conversion error */
#define	FPS_FD		0000200		/* floating double precision mode */
#define	FPS_FL		0000100		/* floating long integer mode */
#define	FPS_FT		0000040		/* floating truncate mode */
#define	FPS_FMM		0000020		/* floating maintenance mode */
#define	FPS_FN		0000010		/* floating negative */
#define	FPS_FZ		0000004		/* floating zero */
#define	FPS_FV		0000002		/* floating overflow */
#define	FPS_FC		0000001		/* floating carry */

#define	FPS_BITS	\
"\10\20FER\17FID\14FIUV\13FIU\12FIV\11FIC\10FD\7FL\6FT\5FMM\4FN\3FZ\2FV\1FC"

/*
 * Floating point error register codes
 */
#define	FEC_FMMT	0000016		/* maintenance mode trap */
#define	FEC_FUV		0000014		/* floating undefined variable */
#define	FEC_FU		0000012		/* floating underflow */
#define	FEC_FV		0000010		/* floating overflow */
#define	FEC_FC		0000006		/* floating integer conversion error */
#define	FEC_FDZ		0000004		/* floating divide by zero */
#define	FEC_FOPER	0000002		/* floating op code error */
