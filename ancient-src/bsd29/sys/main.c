#include "bsd29adapt.h"

/*
 *	SCCS id	@(#)main.c	2.1 (Berkeley)	8/29/83
 */

#include "bsd29/include/sys/param.h"
#include <bsd29/include/sys/systm.h>
#include <bsd29/include/sys/dir.h>
#include <bsd29/include/sys/user.h>
#include <bsd29/include/sys/filsys.h>
#include <bsd29/include/sys/mount.h>
/* UNUSED #include <sys/map.h> */
/* UNUSED #include <sys/proc.h> */
#include <bsd29/include/sys/inode.h>
/* UNUSED #include <sys/seg.h> */
#include <bsd29/include/sys/conf.h>
#include <bsd29/include/sys/buf.h>


#ifdef	UCB_FRCSWAP
/*
 *	If set, allow incore forks and expands.
 *	Set before idle(), cleared in clock.c.
 *	Set to 1 here because the init creation
 *	must not cause a swap.
 */
int	idleflg	= 1;
#endif

#if UNUSED
/*
 * Initialization code.
 * Called from cold start routine as
 * soon as a stack and segmentation
 * have been established.
 * Functions:
 *	clear and free user core
 *	turn on clock
 *	hand craft 0th process
 *	call all initialization routines
 *	fork - process 0 to schedule
 *	     - process 1 execute bootstrap
 */
main()
{
	extern char version[];

	printf("\n%s", version);
	startup();

	/*
	 * set up system process
	 */
	proc[0].p_addr = *ka6;
#ifndef	VIRUS_VFORK
	proc[0].p_size = USIZE;
#endif
	proc[0].p_stat = SRUN;
	proc[0].p_flag |= SLOAD|SSYS;
	proc[0].p_nice = NZERO;
	u.u_procp = &proc[0];
	u.u_cmask = CMASK;

	/*
	 * Initialize devices and
	 * set up 'known' i-nodes
	 */

#ifdef	UCB_IHASH
	ihinit();
#endif
	cinit();
	binit();
#ifdef	UNIBUS_MAP
	(void) ubinit();
#endif	UNIBUS_MAP
#ifdef	UCB_NET
	netinit();
#endif
	clkstart();
	iinit();
	rootdir = iget(rootdev, (ino_t)ROOTINO);
	rootdir->i_flag &= ~ILOCK;
	u.u_cdir = iget(rootdev, (ino_t)ROOTINO);
	u.u_cdir->i_flag &= ~ILOCK;

	/*
	 * make init process
	 * enter scheduling loop
	 * with system process
	 */
#ifdef	VIRUS_VFORK
	if(newproc(0))
#else
	if(newproc())
#endif
		{
#ifdef	VIRUS_VFORK
		expand((int)btoc(szicode),S_DATA);
#else
		expand(USIZE + (int)btoc(szicode));
#endif
		estabur((unsigned)0, btoc(szicode), (unsigned)0, 0, RO);
		copyout((caddr_t)icode, (caddr_t)0, szicode);
		/*
		 * Return goes to loc. 0 of user init
		 * code just copied out.
		 */
		return;
	}
	else
		sched();
}
#endif /* UNUSED */

/*
 * Iinit is called once (from main)
 * very early in initialization.
 * It reads the root's super block
 * and initializes the current date
 * from the last modified date.
 *
 * panic: iinit -- cannot read the super
 * block (usually because of an IO error).
 */
void
iinit()
{
	register struct buf /* UNUSED *cp, */ *bp;
	register struct filsys *fp;
	register int16_t i;

	(*bdevsw[major(rootdev)].d_open)(rootdev, B_READ);
	/* UNUSED (*bdevsw[major(swapdev)].d_open)(swapdev, B_READ); */
	bp = bread(rootdev, SUPERB);
	if(u.u_error)
		panic("iinit");
	fp = &mount[0].m_filsys;
	bcopy(mapin(bp), (caddr_t)fp, sizeof(struct filsys));
	mapout(bp);
	mount[0].m_inodp = (struct inode *) 1;
	brelse(bp);
	mount[0].m_dev = rootdev;
	fp->s_flock = 0;
	fp->s_ilock = 0;
	fp->s_ronly = 0;
#ifdef	UCB_IHASH
	fp->s_lasti = 1;
	fp->s_nbehind = 0;
#endif
	fp->s_fsmnt[0] = '/';
	for (i = 1; i < sizeof(fp->s_fsmnt); i++)
		fp->s_fsmnt[i] = 0;
	time = fp->s_time;
	/* UNUSED bootime = time; */
}

#ifdef UNUSED
memaddr bpaddr;		/* physical click-address of buffers */
#endif /* UNUSED */

extern char buffers[];

/*
 * Initialize the buffer I/O system by freeing
 * all buffers and setting all device buffer lists to empty.
 */
void
binit()
{
	register struct buf *bp;
	register struct buf *dp;
	register int16_t i;
	struct bdevsw *bdp;
	caddr_t paddr;

	bfreelist.b_forw = bfreelist.b_back =
	    bfreelist.av_forw = bfreelist.av_back = &bfreelist;
	paddr = buffers;
	for (i=0; i<nbuf; i++) {
		bp = &buf[i];
		bp->b_dev = NODEV;
		bp->b_un.b_addr = paddr;
		bp->b_xmem = 0;
		paddr += bsize;
		bp->b_back = &bfreelist;
		bp->b_forw = bfreelist.b_forw;
		bfreelist.b_forw->b_back = bp;
		bfreelist.b_forw = bp;
		bp->b_flags = B_BUSY;
		brelse(bp);
	}
	for (bdp = bdevsw; bdp < bdevsw + nblkdev; bdp++) {
		dp = bdp->d_tab;
		if(dp) {
			dp->b_forw = dp;
			dp->b_back = dp;
		}
		(void) (*bdp->d_root)();
	}
}
