struct	vpdevice	{
	short	plbcr;
	short	pbxaddr;
	short	prbcr;
	u_short	pbaddr;
	short	plcsr;
	short	plbuf;
	short	prcsr;
	u_short	prbuf;
};

#define	VP_ERROR	0100000
#define	VP_DTCINTR	0040000
#define	VP_DMAACT	0020000
#define	VP_READY	0000200
#define	VP_IENABLE	0000100
#define	VP_TERMCOM	0000040
#define	VP_FFCOM	0000020
#define	VP_EOTCOM	0000010
#define	VP_CLRCOM	0000004
#define	VP_RESET	0000002
#define	VP_SPP		0000001
