/*
 *	sig.c isn't ifdef'ed for MENLO_JCL;
 *	it would be impossible to read.
 */

/*
 *	SCCS id	@(#)sig.c	2.1 (Berkeley)	8/5/83
 */

#include "whoami.h"

#ifdef	MENLO_JCL
#  include	"sigjcl.c"
#else
#  include	"signojcl.c"
#endif
