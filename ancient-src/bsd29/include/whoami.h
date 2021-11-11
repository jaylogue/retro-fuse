#define _WHOAMI			/* so param.h won't include us again */

#define UNKNOWN
#define PDP11	GENERIC
/* #define	NONFP		/* if no floating point unit */
/* #define	ENABLE34		/* support the ENABLE/34 board */

#ifdef	KERNEL
#    include "localopts.h"
#else
#    include <sys/localopts.h>
#endif
