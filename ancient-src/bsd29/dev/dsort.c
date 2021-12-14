/*
 *	SCCS id	@(#)dsort.c	2.1 (Berkeley)	8/5/83
 */

/*
 * generalized seek sort for disk
 */

#include "param.h"
#include <sys/systm.h>
#include <sys/buf.h>

disksort(dp, bp)
register struct buf *dp, *bp;
{
	register struct buf *ap;
	struct buf *tp;

	ap = dp->b_actf;
	if(ap == NULL) {
		dp->b_actf = bp;
		dp->b_actl = bp;
		bp->av_forw = NULL;
		return;
	}
	tp = NULL;
	for(; ap != NULL; ap = ap->av_forw) {
		if ((bp->b_flags&B_READ) && (ap->b_flags&B_READ) == 0) {
			if (tp == NULL)
				tp = ap;
			break;
		}
		if ((bp->b_flags&B_READ) == 0 && (ap->b_flags&B_READ))
			continue;
		if(ap->b_cylin <= bp->b_cylin)
			if(tp == NULL || ap->b_cylin >= tp->b_cylin)
				tp = ap;
	}
	if(tp == NULL)
		tp = dp->b_actl;
	bp->av_forw = tp->av_forw;
	tp->av_forw = bp;
	if(tp == dp->b_actl)
		dp->b_actl = bp;
}
