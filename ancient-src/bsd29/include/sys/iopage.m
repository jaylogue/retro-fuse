/*
 *	Access abilities (from seg.h)
 */
#define	RO	2
#define	RW	6

/*
 *	ENABLE/34 registers
 *
 *	All information relevant to the ENABLE/34 is supplied with
 *	the permission of ABLE Computer and may not be disclosed in
 *	any manner to sites not licensed by the University of California
 *	for the Second Berkeley Software Distribution.
 *
 */
#ifdef	ENABLE34
#   define	ENABLE_UISA	0163720
#   define	DEC_UISA	0177640
#   ifdef	NONSEPARATE
#	define	ENABLE_UDSA	ENABLE_UISA
#	define	DEC_UDSA	DEC_UISA
#   else
#	define	ENABLE_UDSA	0163740
#	define	DEC_UDSA	0177660
#   endif	NONSEPARATE
#   define	ENABLE_KISA0	0163700
#   define	ENABLE_KISA6	0163714
#   define	DEC_KISA0	0172340
#   define	DEC_KISA6	0172354
#   ifdef	KERN_NONSEP
#	define	ENABLE_KDSA1	0163702
#	define	ENABLE_KDSA2	0163704
#	define	ENABLE_KDSA5	0163712
#	define	ENABLE_KDSA6	0163714
#	define	DEC_KDSA1	0172342
#	define	DEC_KDSA2	0172344
#	define	DEC_KDSA5	0172352
#	define	DEC_KDSA6	0172354
#   else
#	define	ENABLE_KDSA1	0163762
#	define	ENABLE_KDSA2	0163764
#	define	ENABLE_KDSA5	0163772
#	define	ENABLE_KDSA6	0163774
#	define	DEC_KDSA1	0172362
#	define	DEC_KDSA2	0172364
#	define	DEC_KDSA5	0172372
#	define	DEC_KDSA6	0172374
#   endif	KERN_NONSEP
#   define	ENABLE_SSR4	0163674
#   define	ENABLE_SSR3	0163676
#endif	ENABLE34

/*
 *	DEC registers
 */
#define	SISD0	0172200
#define	SISD1	0172202
#define	SISD2	0172204
#define	SISD3	0172206
#define	SDSD0	0172220
#define	SISA0	0172240
#define	SISA1	0172242
#define	SISA2	0172244
#define	SISA7	0172256
#define	SDSA0	0172260

#define	KISD0	0172300
#define	KISD1	0172302
#define	KISD2	0172304
#define	KISD4	0172310
#define	KISD5	0172312
#define	KISD6	0172314
#define	KISD7	0172316
#ifdef	KERN_NONSEP
#   define	KDSD0	KISD0
#   define	KDSD5	KISD5
#   define	KDSD6	KISD6
#else
#   define	KDSD0	0172320
#   define	KDSD5	0172332
#   define	KDSD6	0172334
#endif

#ifdef	ENABLE34
#   define	KISA0	*_KISA0
#else
#   define	KISA0	0172340
#endif
#define	KISA1	0172342
#define	KISA2	0172344
#define	KISA4	0172350
#define	KISA5	0172352
#ifdef	ENABLE34
#   define	KISA6	*_KISA6
#else
#   define	KISA6	0172354
#endif
#define	KISA7	0172356
#ifdef	KERN_NONSEP
#   define	KDSA0	KISA0
#   ifdef	ENABLE34
#	define	KDSA1	*_KDSA1
#	define	KDSA2	*_KDSA2
#	define	KDSA5	*_KDSA5
#	define	KDSA6	*_KDSA6
#   else
#	define	KDSA1	KISA1
#	define	KDSA2	KISA2
#	define	KDSA5	KISA5
#	define	KDSA6	KISA6
#   endif
#define	KDSA7	KISA7
#else	KERN_NONSEP
#   define	KDSA0	0172360
#   ifdef	ENABLE34
#	define	KDSA1	*_KDSA1
#	define	KDSA2	*_KDSA2
#	define	KDSA5	*_KDSA5
#	define	KDSA6	*_KDSA6
#   else
#	define	KDSA1	0172362
#	define	KDSA2	0172364
#	define	KDSA5	0172372
#	define	KDSA6	0172374
#   endif
#define	KDSA7	0172376
#endif	KERN_NONSEP

#define	SSR3	0172516
#define	CCSR	0172540
#define	CCSB	0172542
#define	CSW	0177570
#define	SSR0	0177572
#define	SSR1	0177574
#define	SSR2	0177576
#ifdef	ENABLE34
#   define	UISA	*_UISA
#   define	UDSA	*_UDSA
#else
#   define	UISA	0177640
#   define	UDSA	0177660
#endif

#define	STACKLIM 0177774
#define	PS	0177776
#define TBIT	020		/* trace bit in PS */
