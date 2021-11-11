/*
 *  Structure to describe pseudo-DMA buffer
 *  used by DZ-11 pseudo-DMA routines.
 *  The offsets in the structure are well-known in dzdma (mch.s).
 */

struct pdma {
	struct	dzdevice *p_addr;	/* address of controlling device */
	char	*p_mem;			/* start of buffer */
	char	*p_end;			/* end of buffer */
	struct	tty *p_arg;		/* tty structure for this line */
};
