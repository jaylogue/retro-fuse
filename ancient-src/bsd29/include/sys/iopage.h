/*
 * I/O page addresses
 */

#define	PS		((u_short *) 0177776)
#define	SL		((u_short *) 0177774)
#define	PIR		((u_short *) 0177772)
#if	PDP11 == 44 || PDP11 == 70 || PDP11 == GENERIC
#define	CPUERR		((u_short *) 0177766)
#endif
#if	PDP11 == 70 || PDP11 == GENERIC
#define	SYSSIZEHI	((u_short *) 0177762)
#define	SYSSIZELO	((u_short *) 0177760)
#endif
#if	PDP11 == 44 || PDP11 == 70 || PDP11 == GENERIC
#define	MEMSYSCTRL	((u_short *) 0177746)
#define	MEMSYSERR	((u_short *) 0177744)
#define	MEMERRHI	((u_short *) 0177742)
#define	MEMERRLO	((u_short *) 0177740)
#endif
#define	CSW		((u_short *) 0177570)
#define	LKS		((u_short *) 0177546)
#define	KW11P_CSR	((u_short *) 0172540)
