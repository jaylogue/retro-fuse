struct	dzdevice
{
	short	dzcsr;		/* control-status register */
	short	dzrbuf;		/* receiver buffer register (read only) */
#define	dzlpr	dzrbuf		/* line parameter register (write only) */
	char	dztcr;		/* transmitter control register */
	char	dzdtr;		/* data terminal ready register */
	char	dztbuf;		/* transmitter buffer (write only) */
#define	dzring	dztbuf;		/* ring register (read only) */
	char	dzbrk;		/* break register (write only) */
#define	dzcar	dzbrk		/* carrier register (read only) */
};

/* Bits in dzcsr */
#define	DZ_MM	0000010		/* Maintenance mode */
#define	DZ_CLR	0000020		/* Clear */
#define	DZ_MSE	0000040		/* Master Scan Enable */
#define	DZ_RIE	0000100		/* Receiver Interrupt Enable */
#define	DZ_RDO	0000200		/* Receiver Done (read only) */
#define	DZ_SAE	0010000		/* Silo Alarm Enable (read/write) */
#define	DZ_SA	0020000		/* Silo Alarm Enable (read only) */
#define	DZ_TIE	0040000		/* Transmit Interrupt Enable */
#define	DZ_TRDY	0100000		/* Transmitter Ready (read only) */
#ifdef	DZ_SILO
#define	DZ_IEN	(DZ_MSE|DZ_RIE|DZ_TIE|DZ_SAE)
#else
#define	DZ_IEN	(DZ_MSE|DZ_RIE|DZ_TIE)
#endif

/* Bits in dzlpr */
#define	BITS7	020
#define	BITS8	030
#define	TWOSB	040
#define	PENABLE	0100
#define	OPAR	0200

/* Bits in dzrbuf */
#define	DZ_PE	010000		/* parity error */
#define	DZ_FE	020000		/* framing error */
#define	DZ_DO	040000		/* overrun */

/* Flags for modem control */
#define	DZ_ON	1
#define	DZ_OFF	0
