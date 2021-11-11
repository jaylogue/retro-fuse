#define	PDP1144_CMER	0177744
#define	PDP1144_CCR	0177746
#define	PDP1144_CMR	0177750
#define	PDP1144_CHR	0177752
#define	PDP1144_CDR	0177754

/* bits in cache memory error register */
#define	CME_CMPE	0100000		/* cache memory parity error */
/* bits 14-8 are unused */
#define	CME_PEHI	0000200		/* parity error high byte */
#define	CME_PELO	0000100		/* parity error low byte */
#define	CME_TPE		0000040		/* tag parity error */
/* bits 4-0 are unused */

/* bits in cache control register */
/* bits 15-14 are unused */
#define	CCR_VSIU	0020000		/* valid store in use (read only) */
#define	CCR_VCIP	0010000		/* valid clear in progress (read only) */
/* bit 11 is unused */
#define	CCR_WWPT	0002000		/* write wrong parity tag */
#define	CCR_UCB		0001000		/* unconditional cache bypass */
#define	CCR_FC		0000400		/* flush cache (write only) */
#define	CCR_PEA		0000200		/* parity error abort */
#define	CCR_WWPD	0000100		/* write wrong parity data */
/* bits 5-4 are unused */
#define	CCR_FMHI	0000010		/* force miss high */
#define	CCR_FMLO	0000004		/* force miss low */
/* bit 1 is unused */
#define	CCR_DCPI	0000001		/* disable cache parity interrupt */

/* bits in cache maintenance register */
#define	CMR_CMP1	0100000		/* compare 1 (write only) */
#define	CMR_CMP2	0040000		/* compare 2 (write only) */
#define	CMR_CMP3	0020000		/* compare 3 (write only) */
#define	CMR_V		0010000		/* valid (write only) */
#define	CMR_HPB		0004000		/* high parity bit (write only) */
#define	CMR_LPB		0002000		/* low parity bit (write only) */
#define	CMR_TPB		0001000		/* tag parity bit */
#define	CMR_HIT		0000400		/* hit */
/* bits 7-5 are unused */
#define	CMR_ESA		0000020		/* enable stop action */
#define	CMR_AM		0000010		/* address matched */
#define	CMR_EHA		0000004		/* enable halt action */
#define	CMR_HODO	0000002		/* hit on destination only */
#define	CMR_TDAR	0000001		/* tag data from address match register */

#define	PDP1160_MSR	0177744
#define	PDP1160_CCR	0177746
#define	PDP1160_HMR	0177752

/* bits in memory system register */
#define	MSR_CPUAB	0100000		/* cpu abort
/* bits 14-8 are unused */
#define	MSR_PEHI	0000200		/* high byte parity error */
#define	MSR_PELO	0000100		/* low byte parity error */
#define	MSR_TPE		0000040		/* tag parity error */
/* bits 4-0 are unused */

/* bits in cache control register */
/* bits 15-8 are unused */
#define	CCR_CPEA	0000200		/* cache parity error abort */
#define	CCR_WWP		0000100		/* write wrong parity */
/* bits 5-4 are unused */
#define	CCR_FM1		0000010		/* force miss 1 */
#define	CCR_FM2		0000004		/* force miss 2 */
/* bit 1 is unused */
#define	CCR_DT		0000001		/* disable traps */

#define	PDP1170_LEAR	0177740
#define	PDP1170_HEAR	0177742
#define	PDP1170_MSER	0177744
#define	PDP1170_CCR	0177746
#define	PDP1170_CMR	0177750
#define	PDP1170_HMR	0177752
#define	PDP1170_LSR	0177760
#define	PDP1170_USR	0177762
#define	PDP1170_SID	0177764
#define	PDP1170_CPUER	0177766
#define	PDP1170_MBR	0177770

/* bits in memory system error register */
#define	MSER_CPUAB	0100000		/* cpu abort */
#define	MSER_CPUABAE	0040000		/* cpu abort after error */
#define	MSER_UPE	0020000		/* UNIBUS parity error */
#define	MSER_UMPE	0010000		/* UNIBUS multiple parity error */
#define	MSER_CPUER	0004000		/* cpu error */
#define	MSER_UE		0002000		/* UNIBUS error */
#define	MSER_CPUUA	0001000		/* cpu UNIBUS abort */
#define	MSER_EM		0000400		/* error in maintenance */
#define	MSER_DMG1	0000200		/* data memory group 1 */
#define	MSER_DMG0	0000100		/* data memory group 0 */
#define	MSER_AMG1	0000040		/* address memory group 1 */
#define	MSER_AMG0	0000020		/* address memory group 0 */
#define	MSER_MMOW	0000010		/* main memory odd word */
#define	MSER_MMEW	0000004		/* main memory even word */
#define	MSER_MMAPE	0000002		/* main memory address parity error */
#define	MSER_MMT	0000001		/* main memory timeout */

/* bits in cache control register */
/* bits 15-6 are unused */
#define	CCR_FRG1	0000040		/* force replacement group 1 */
#define	CCR_FRG0	0000020		/* force replacement group 0 */
#define	CCR_FMG1	0000010		/* force miss group 1 */
#define	CCR_FMG0	0000004		/* force miss group 0 */
#define	CCR_DUT		0000002		/* disable UNIBUS traps */
#define	CCR_DT		0000001		/* disable traps */

/* bits in cpu error register */
/* bits 15-8 are unused */
#define	CPUER_ILH	0000200		/* illegal halt */
#define	CPUER_OAE	0000100		/* odd address error */
#define	CPUER_NXM	0000040		/* nonexistent memory */
#define	CPUER_UTIMO	0000020		/* UNIBUS timeout */
#define	CPUER_YZSL	0000010		/* yellow zone stack limit */
#define	CPUER_RZSL	0000004		/* red zone stack limit */
