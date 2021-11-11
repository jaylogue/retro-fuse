/*
 * Definitions needed to perform bad sector
 * revectoring ala DEC STD 144.
 *
 * The bad sector information is located in the
 * first 5 even numbered sectors of the last
 * track of the disk pack.  There are five
 * identical copies of the information, described
 * by the dkbad structure.
 *
 * Replacement sectors are allocated starting with
 * the first sector before the bad sector information
 * and working backwards towards the beginning of
 * the disk.  A maximum of MAXBAD bad sectors are supported
 * per pack; MAXBAD may be redefined as conditions require,
 * but may be 126 at most.  It is small to save on the kernel
 * memory required to hold these.
 * The position of the bad sector in the bad sector table
 * determines which replacement sector it corresponds to.
 * Bad sectors in excess of MAXBAD will not be replaced
 * automatically, but the bad-sector file on disk will be correct.
 *
 * The bad sector information and replacement sectors
 * are conventionally only accessible through the
 * 'h' file system partition of the disk.  If that
 * partition is used for a file system, the user is
 * responsible for making sure that it does not overlap
 * the bad sector information or any replacement sectors.
 */

#ifdef	BADSECT

#define	MAXBAD	8

struct dkbad {
	long	bt_csn;			/* cartridge serial number */
	u_short	bt_mbz;			/* unused; should be 0 */
	u_short	bt_flag;		/* -1 => alignment cartridge */
	struct bt_b {
		u_short	bt_cyl;		/* cylinder number of bad sector */
		u_short	bt_trksec;	/* track and sector number */
	} bt_bad[MAXBAD];
};

#if	MAXBAD > 126
ERROR!
#endif
#endif	BADSECT

#define	ECC	0
#define	SSE	1
#define	BSE	2
#define	CONT	3

