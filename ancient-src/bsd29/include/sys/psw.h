/*
 * PDP 11 program status word definitions
 */
#define	PS_C		0000001	/* carry bit */
#define	PS_V		0000002	/* overflow bit */
#define	PS_Z		0000004	/* zero bit */
#define	PS_N		0000010	/* negative bit */
#define	PS_ALLCC	0000017	/* all condition code bits on (unlikely) */
#define	PS_T		0000020	/* trace enable bit */
#define	PS_CURMOD	0140000	/* current mode  ( all on is user ) */
#define	PS_PRVMOD	0030000	/* previous mode ( all on is user ) */	
#define	PS_IPL		0000340	/* interrupt priority */
#define	PS_USERCLR	0000340	/* bits that must be clear in user mode */
#define	PS_BR0		0000000	/* bus request level 0 */
#define	PS_BR1		0000040	/* bus request level 1 */
#define	PS_BR2		0000100	/* bus request level 2 */
#define	PS_BR3		0000140	/* bus request level 3 */
#define	PS_BR4		0000200	/* bus request level 4 */
#define	PS_BR5		0000240	/* bus request level 5 */
#define	PS_BR6		0000300	/* bus request level 6 */
#define	PS_BR7		0000340	/* bus request level 7 */

#define	BASEPRI(ps)	(((ps) & PS_IPL) == 0)
#define	USERMODE(ps)	(((ps) & (PS_CURMOD|PS_PRVMOD)) == (PS_CURMOD|PS_PRVMOD))
