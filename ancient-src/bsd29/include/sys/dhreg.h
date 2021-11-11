struct dhdevice
{
	union {
		short	dhcsr;		/* control-status register */
		char	dhcsrl;		/* low byte for line select */
	} un;
	short	dhrcr;			/* receive character register */
	short	dhlpr;			/* line parameter register */
	char	*dhcar;			/* current address register */
	short	dhbcr;			/* byte count register */
	u_short	dhbar;			/* buffer active register */
	short	dhbreak;		/* break control register */
	short	dhsilo;			/* silo status register */
};

/* Bits in dhcsr */
#define	DH_TI	0100000		/* transmit interrupt */
#define	DH_SI	0040000		/* storage interrupt */
#define	DH_TIE	0020000		/* transmit interrupt enable */
#define	DH_SIE	0010000		/* storage interrupt enable */
#define	DH_MC	0004000		/* master clear */
#define	DH_NXM	0002000		/* non-existent memory */
#define	DH_MM	0001000		/* maintenance mode */
#define	DH_CNI	0000400		/* clear non-existent memory interrupt */
#define	DH_RI	0000200		/* receiver interrupt */
#define	DH_RIE	0000100		/* receiver interrupt enable */

/* Bits in dhlpr */
#define	BITS6	01
#define	BITS7	02
#define	BITS8	03
#define	TWOSB	04
#define	PENABLE	020
/* Some DEC manuals incorrectly say this bit causes generation of even parity. */
#define	OPAR	040
#define	HDUPLX	040000

#ifdef	DH_SILO
#define	DH_IE	(DH_TIE|DH_SIE|DH_RIE)
#else
#define	DH_IE	(DH_TIE|DH_RIE)
#endif

/* Bits in dhrcr */
#define	DH_PE		0010000		/* parity error */
#define	DH_FE		0020000		/* framing error */
#define	DH_DO		0040000		/* data overrun */

struct dmdevice
{
	short	dmcsr;		/* control status register */
	short	dmlstat;	/* line status register */
	short	dmpad1[2];
};

/* bits in dm csr */
#define	DM_RF		0100000		/* ring flag */
#define	DM_CF		0040000		/* carrier flag */
#define	DM_CTS		0020000		/* clear to send */
#define	DM_SRF		0010000		/* secondary receive flag */
#define	DM_CS		0004000		/* clear scan */
#define	DM_CM		0002000		/* clear multiplexor */
#define	DM_MM		0001000		/* maintenance mode */
#define	DM_STP		0000400		/* step */
#define	DM_DONE		0000200		/* scanner is done */
#define	DM_IE		0000100		/* interrupt enable */
#define	DM_SE		0000040		/* scan enable */
#define	DM_BUSY		0000020		/* scan busy */

/* bits in dm lsr */
#define	DML_RNG		0000200		/* ring */
#define	DML_CAR		0000100		/* carrier detect */
#define	DML_CTS		0000040		/* clear to send */
#define	DML_SR		0000020		/* secondary receive */
#define	DML_ST		0000010		/* secondary transmit */
#define	DML_RTS		0000004		/* request to send */
#define	DML_DTR		0000002		/* data terminal ready */
#define	DML_LE		0000001		/* line enable */

#define	DML_ON		(DML_DTR|DML_RTS|DML_LE)
#define	DML_OFF		(DML_LE)

/* dmctl actions */
#define	DMSET		0
#define	DMBIS		1
#define	DMBIC		2
