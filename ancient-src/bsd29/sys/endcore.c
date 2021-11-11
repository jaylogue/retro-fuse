
/*
 *	SCCS id @(#)endcore.c	2.1 (Berkeley) 8/5/83
 *
 *  endcore.c:  declarations of structures loaded last and allowed to
 *  reside in the 0120000-140000 range (where buffers and clists are
 *  mapped).  These structures must be extern everywhere else,
 *  and the asm output of cc is edited to move these structures
 *  from comm to bss (which is last) (see the script :comm-to-bss).
 */

#include "param.h"
#include <sys/proc.h>
#include <sys/text.h>
#include <sys/file.h>
#include <sys/inode.h>
#include <sys/dir.h>

int	remap_area;	/* start of possibly mapped area; must be first */
struct	proc	proc[NPROC];
struct	text	text[NTEXT];
struct	file	file[NFILE];
