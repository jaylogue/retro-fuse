/*
 * Location of the users' stored
 * registers relative to R0.
 * Usage is u.u_ar0[XX].
 */
#ifdef	MENLO_KOV
#define	R0	(0)
#define	R1	(-3)
#define	R2	(-11)
#define	R3	(-10)
#define	R4	(-9)
#define	R5	(-7)
#define	R6	(-4)
#else
#define	R0	(0)
#define	R1	(-2)
#define	R2	(-9)
#define	R3	(-8)
#define	R4	(-7)
#define	R5	(-6)
#define	R6	(-3)
#endif
#define	R7	(1)
#define	PC	(1)
#define	RPS	(2)
