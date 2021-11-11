/*
 * Possible exit codes of the autoconfiguration program
 */
#define	AC_OK		0	/* Everything A-OK for system go */
#define	AC_SETUP	1	/* Error in initializing autoconfig program */
#define	AC_SINGLE	2	/* Non-serious error; come up single user */

/*
 * Magic number to verify that autoconfig runs only once
 */
#define	CONF_MAGIC	0x1960

/*
 * Return values for probes
 */
#define	ACP_NXDEV	-1
#define	ACP_IFINTR	0
#define	ACP_EXISTS	1

/*
 * Return values from interrupt routines
 */
#define	ACI_BADINTR	-1
#define	ACI_NOINTR	0
#define	ACI_GOODINTR	1
