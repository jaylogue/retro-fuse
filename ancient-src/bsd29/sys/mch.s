/*
 *	SCCS id	@(#)mch.s	2.1 (Berkeley)	12/21/83
 *
 *	Machine language assist.
 *	Uses the C preprocessor to produce suitable code
 *	for various 11's, mainly dependent on definition
 *	of PDP11, MENLO_KOV and NONSEPARATE.
 */ 

#define		LOCORE
#include	"whoami.h"	/* for localopts */

#if PDP11==GENERIC		/* adapt to any 11 at boot */
#	undef	NONSEPARATE			/* support sep I/D */
#	define	SPLHIGH		bis	$HIPRI,PS
#	define	SPL7		bis	$340,PS
#	define	SPLLOW		bic	$HIPRI,PS
#	ifdef	UCB_NET
#		define	SPLNET	bis	$NETPRI,PS
#	endif
#else
#   ifdef	NONSEPARATE			/* 11/40, 34, 23, 24 */
#	define	SPLHIGH		bis	$HIPRI,PS
#	define	SPL7		bis	$340,PS
#	define	SPLLOW		bic	$HIPRI,PS
#	ifdef	UCB_NET
#		define	SPLNET	bis	$NETPRI,PS
#	endif
#	define	mfpd		mfpi
#	define	mtpd		mtpi
#   else					/* 11/44, 11/45, 11/70	*/
#	define	SPLHIGH		spl	HIGH
#	define	SPL7		spl	7
#	define	SPLLOW		spl	0
#	ifdef	UCB_NET
#		define	SPLNET	spl	NET
#	endif
#   endif
#endif

#include	"ht.h"
#include	"tm.h"
#include        "ts.h"
#include	<a.out.h>
#include	<sys/cpu.m>
#include	<sys/trap.h>
#include	<sys/reboot.h>
#include	<sys/iopage.m>
#ifdef	MENLO_KOV
#include	<sys/koverlay.h>
#endif

#define	INTSTK	500.			/* bytes for interrupt stack */

/*
 * non-UNIX instructions
 */
mfpi	= 6500^tst
mtpi	= 6600^tst
#ifndef	NONSEPARATE
mfpd	= 106500^tst
mtpd	= 106600^tst
#endif
stst	= 170300^tst
spl	= 230
ldfps	= 170100^tst
stfps	= 170200^tst
mfpt	= 000007			/ Move from processor - 44s only
mfps	= 106700^tst			/ Move from PS - 11/23,24,34's only
mtps	= 106400^tst			/ Move to PS - 11/23,24,34's only
halt	= 0
wait	= 1
iot	= 4
reset	= 5
rtt	= 6

#ifdef	PROFILE
#define	HIGH	6			/ See also the :splfix files
#define	HIPRI	300			/ Many spl's are done in-line
#else
#define	HIGH	7
#define	HIPRI	340
#endif	PROFILE
#ifdef	UCB_NET
#define	NET	1
#define	NETPRI	40
#endif

/*
 * Mag tape dump
 * Save registers in low core and write core (up to 248K) onto mag tape.
 * Entry is through 044 (physical) with memory management off.
 */
	.globl	dump

#ifndef	KERN_NONSEP
	.data
#endif

dump:
#if     NHT > 0 || NTM > 0 || NTS > 0
	/ save regs r0, r1, r2, r3, r4, r5, r6, KIA6
	/ starting at location 4 (physical)
	inc	$-1			/ check for first time
	bne	1f			/ if not, don't save registers again
	mov	r0,4
	mov	$6,r0
	mov	r1,(r0)+
	mov	r2,(r0)+
	mov	r3,(r0)+
	mov	r4,(r0)+
	mov	r5,(r0)+
	mov	sp,(r0)+
	mov	KDSA6,(r0)+
1:

	/ dump all of core (i.e. to first mt error)
	/ onto mag tape. (9 track or 7 track 'binary')
#if	NHT > 0
HT	= 0172440
HTCS1	= HT+0
HTWC	= HT+2
HTBA	= HT+4
HTFC	= HT+6
HTCS2	= HT+10
HTTC	= HT+32
	mov	$HTCS1,r0
	mov	$40,*$HTCS2
	mov	$2300,*$HTTC
	clr	*$HTBA
	mov	$1,(r0)
1:
	mov	$-512.,*$HTFC
	mov	$-256.,*$HTWC
	movb	$61,(r0)
2:
	tstb	(r0)
	bge	2b
	bit	$1,(r0)
	bne	2b
	bit	$40000,(r0)
	beq	1b
	mov	$27,(r0)
#else

#if	NTM > 0
MTC = 172522
	mov	$MTC,r0
	mov	$60004,(r0)+
	clr	2(r0)
1:
	mov	$-512.,(r0)
	inc	-(r0)
2:
	tstb	(r0)
	bge	2b
	tst	(r0)+
	bge	1b
	reset

	/ end of file and loop
	mov	$60007,-(r0)
#else
#if     NTS > 0
TSDB = 0172520
TSSR = 0172522

	/register usage is as follows

	/reg 0 points to UBMAP register 1 low
	/reg 1 is used to calculate the current memory address
	/ for each 512 byte transfer.
	/reg 2 is used to contain and calculate memory pointer
	/ for UBMAP register 1 low
	/reg 3 is used to contain and calculate memory pointer
	/ for UBMAP register 1 high
	/reg 4 points to the command packet
	/reg 5 is used as an interation counter when mapping is enabled


	cmp     _cputype,$44.   /is a 44 ?
	beq     1f              /yes, skip next
	cmp     _cputype,$70.   /is a 70 ?
	bne     2f              /not a 70 either. no UBMAP
1:
/       tst     _ubmaps         /unibus map present ?
/       beq     2f              /no, skip map init
	/this section of code initializes the Unibus map registers
	/and the memory management registers.
	/UBMAP reg 0 points to low memory for the TS11 command,
	/characteristics, and message buffers.
	/UBMAP reg 1 gets updated to point to the current
	/memory area.
	/Kernel I space 0 points to low memory
	/Kernel I space 7 points to the I/O page.

	inc	setmap		/indicate that UB mapping is needed
	mov	$UBMR0,r0	/point to  map register 0
	clr	r2		/init for low map reg
	clr	r3		/init for high map reg
	clr	(r0)+		/load map reg 0 low
	clr	(r0)+		/load map reg 0 high
	mov	$77406,*$KISD0	/set KISDR0
	mov	$77406,*$KISD7	/set KISDR7
	clr	*$KISA0		/point KISAR0 to low memory
	mov	$IO,*$KISA7	/point KISAR7 to IO page
	inc	*$SSR0		/turn on memory mngt
	mov	$60,*$SSR3	/enable 22 bit mapping
	mov	r2,(r0)		/load map reg 1 low
	mov	r3,2(r0)	/load map reg 1 high
2:
	/this section of code initializes the TS11

	tstb	*$TSSR		/make sure
	bpl	2b		/drive is ready
	mov	$comts,r4	/point to command packet
	add	$2,r4		/set up mod 4
	bic	$3,r4		/alignment
	mov	$140004,(r4)	/write characteristics command
	mov	$chrts,2(r4)	/characteristics buffer
	clr	4(r4)		/clear ext mem addr (packet)
	clr	tsxma		/clear extended memory save loc
	mov	$10,6(r4)	/set byte count for command
	mov	$mests,*$chrts	/show where message buffer is
	clr	*$chrts+2	/clear extended memory bits here too
	mov	$16,*$chrts+4	/set message buffer length
	mov	r4,*$TSDB	/start command
	mov	$20,r5		/set up SOB counter for UBMAP
	clr	r1		/init r1 beginning memory address
1:
	tstb	*$TSSR		/wait for ready
	bpl	1b		/not yet
	mov	*$TSSR,tstcc	/error condition (SC) ?
	bpl	2f		/no error
	bic	$!16,tstcc	/yes error, get TCC
	cmp	tstcc,$10	/recoverable error ?
	bne	8f		/no
	mov	$101005,(r4)	/yes, load write data retry command
	clr	4(r4)		/clear packet ext mem addr
	mov	r4,*$TSDB	/start retry
	br	1b
8:
	bit	$4000,*$TSSR	/is error NXM ?
	beq	.		/no, hang (not sure of good dump)
	mov	$140013,(r4)	/load a TS init command
	mov	r4,*$TSDB	/to clear NXM error
6:
	tstb	*$TSSR		/wait for ready
	bpl	6b
	mov	$1,6(r4)	/set word count = 1
	mov	$100011,(r4)	/load write EOF command
	mov	r4,*$TSDB	/do write EOF
7:
	tstb	*$TSSR		/wait for ready
	bpl	7b
	halt			/halt after good dump
9:
	br	1b
2:
	/If mapping is needed this section calculates the
	/ base address to be loaded into map reg 1
	/ the algorithm is (!(r5 - 21))*1000) | 20000
	/ the complement is required because an SOB loop
	/ is being used for the counter
	/This loop causes 20000 bytes to be written
	/before the UBMAP is updated.

	tst	setmap		/UBMAP ?
	beq	3f		/no map
	mov	r2,(r0)		/load map reg 1 low
	mov	r3,2(r0)	/load map reg 1 high
	mov	r5,r1		/calculate
	sub	$21,r1		/address for this pass
	com	r1		/based on current
	mul	$1000,r1	/interation
	bis	$20000,r1	/select map register 1
	clr	4(r4)		/clear extended memory bits
3:
	/This section does the write.
	/ if mapping is needed the sob loop comes in play here
	/ when the sob falls through the UBAMP reg will be
	/ updated by 20000 to point to next loop section.

	/ if mapping not needed then just calculate the
	/ next 512 byte address pointer

	mov	r1,2(r4)	/load mem address
	mov	tsxma,4(r4)	/load ext mem address
	mov	$512.,6(r4)	/set byte count
	mov	$100005,(r4)	/set write command
	mov	r4,*$TSDB	/initiate xfer
	tst	setmap		/mapping?
	beq	4f		/branch if not
	sob	r5,9b		/yes continue loop
	mov	$20,r5		/reset loop count
	add	$20000,r2	/bump low map
	adc	r3		/carry to high map
	br	1b		/do some more
4:
	add	$512.,r1	/bump address for no mapping
	adc	tsxma		/carry to extended memory bits
	br	1b		/do again

/ The following TS11 command and message buffers,
/ must be in initialized data space instead of
/ bss space. This allows them to be mapped by the
/ first M/M mapping register, which is the only one
/ used durring a core dump.

tsxma:	0	/ts11 extended memory address bits
setmap:	0	/UB map usage indicator
tstcc:	0	/ts11 temp location for TCC
comts:		/ts11 command packet
	0 ; 0 ; 0 ; 0 ; 0
chrts:		/ts11 characteristics
	0 ; 0 ; 0 ; 0
mests:		/ts11 message buffer
	0 ; 0 ; 0 ; 0 ; 0 ; 0 ; 0
#endif  NTS
#endif	NTM
#endif	NHT
#endif	NHT || NTM
	br	.		/ If no ht and no tm, fall through to here

	.text

#ifdef	UCB_AUTOBOOT
/*
 * routine to call panic, take dump and reboot;
 * entered by manually loading the PC with 040 and continuing
 */
	.globl	_panic, _halt, do_panic, _halt
do_panic:
	mov	$pmesg, -(sp)
	jsr	pc, _panic
	/* NOTREACHED */

	.data
pmesg:	<forced from console\0>
	.text

_halt:
	halt
	/* NOTREACHED */
	rts	pc
#endif	UCB_AUTOBOOT

	.globl	_etext, _main, start
#ifdef	MENLO_KOV
	.globl	ovend, ova, ovd
#endif

start:
	bit	$1,SSR0			/ is memory management enabled ?
	beq	.			/ better be !!!

	/  The following two instructions change the contents of
	/  locations 034-037 to read:
	/ 		syscall; br0+SYSCALL.
	mov	$syscall, 34
	mov	$0+SYSCALL., 36

	/  Turn off write permission on kernel text
	movb	$RO, KISD0

	/  Get a stack pointer
	mov	$usize-1\<8|RW, KDSD6
	mov	$_u+[usize*64.],sp

	/  Clear user block
	mov	$_u,r0
1:
	clr	(r0)+
	cmp	r0,sp
	blo	1b

#ifdef	UCB_AUTOBOOT
	.globl	_bootflags, _checkword

	/  Get bootflags;  the bootstrap leave them in r4.
	/  R2 should be the complement of bootflags.
	mov	r4,_bootflags
	mov	r2,_checkword
#endif

	/  Check out (and if necessary, set up) the hardware.
	jsr	pc, hardprobe

	/  Set up previous mode and call main
	/  on return, enter user mode at 0R
	mov	$30340,PS
	jsr	pc,_main
	mov	$170000,-(sp)
	clr	-(sp)
	rtt

#if	defined(MENLO_OVLY) || (defined(NONFP) && !defined(NONSEPARATE))
/*
 * Emt takes emulator traps, which are normally requests
 * to change the overlay for the current process.
 * Emts are also generated by the separate I/D floating
 * point simulator in fetching instructions from user
 * I space.  In this case, r0 should be -1 and r1 should
 * contain the pc of the instruction to be fetched.
 * If an invalid emt is received, _trap is called.
 */
	.globl	_choverl, emt
emt:
	mov	PS,saveps
	bit	$30000,PS		/ verify that emt is not kernel mode
	beq	bademt
#ifdef	MENLO_OVLY
	tst	_u+U_OVBASE		/ verify that process is overlaid
	beq	fetchi
	cmp	r0,$NOVL		/ verify valid overlay number
	bhi	fetchi
	SPLLOW
	mov	r0,-(sp)
	mov	r1,-(sp)
	mov	r0,_u+U_CUROV
#ifdef UCB_METER
	inc	_cnt+V_OVLY
#endif
	mov	$RO,-(sp)
	jsr	pc,_choverl
	tst	(sp)+
	mov	(sp)+,r1
	mov	(sp)+,r0
	rtt
#endif	MENLO_OVLY

fetchi:
#if	defined(NONFP) && !defined(NONSEPARATE)
	cmp	$-1, r0
	bne	bademt
	mov	$1f,nofault
	mfpi	(r1)			/ get I-space at (r1)
	mov	(sp)+,r0		/ put result in r0
	clr	nofault
	rtt

1:
	clr	nofault
	/* FALL THROUGH */
#endif	defined(NONFP) && !defined(NONSEPARATE)
bademt:
	jsr 	r0, call1; jmp	_trap	/ invalid emt
	/*NOTREACHED*/
#endif	defined(MENLO_OVLY) || (defined(NONFP) && !defined(NONSEPARATE))

	.globl	syscall, trap, buserr, _syscall, _trap, _panic, _panicstr
/*
 *	System call interface to C code.
 */
syscall:
	mov	r0, -(sp)
#ifdef	MENLO_KOV
	cmp	-(sp), -(sp)	/ Dummy arguments.  See trap.c for explanation.
#else
	tst	-(sp)		/ Dummy argument.
#endif
	mov	r1, -(sp)
	mfpd	sp
	tst	-(sp)		/ Dummy argument.
	jsr	pc, _syscall
	tst	(sp)+
	mtpd	sp
	mov	(sp)+, r1
#ifdef	MENLO_KOV
	cmp	(sp)+, (sp)+
#else
	tst	(sp)+
#endif
	mov	(sp)+, r0
	rtt

#ifdef	NONFP
/*
 *	Fast illegal-instruction trap routine for use with interpreted
 *	floating point.  All of the work is done here if SIGILL is caught,
 *	otherwise, trap is called like normal.
 */
	.globl	instrap
instrap:
	mov	PS, saveps
	tst	nofault
	bne	1f			/ branch to trap code
	bit	$30000, PS		/ verify not from kernel mode
	beq	3f
	SPLLOW
	tst	_u+U_SIGILL		/ check whether SIGILL is being caught
	beq	3f			/ is default
	bit	$1, _u+U_SIGILL
	bne	3f			/ is ignored (or held or deferred!)
	mov	r0, -(sp)
	mfpd	sp
#ifdef	UCB_METER
	inc	_cnt+V_TRAP
	inc	_cnt+V_INTR		/ since other traps also count as intr
#endif
	mov	(sp), r0
	sub	$2, r0
	mov	$2f, nofault
	mov	6(sp), -(sp)
	mtpd	(r0)			/ push old PS
	sub	$2, r0
	mov	4(sp), -(sp)
	mtpd	(r0)			/ push old PC
	mov	_u+U_SIGILL, 4(sp)	/ new PC
	bic	$TBIT, 6(sp)		/ new PS
	mov	r0, (sp)
	mtpd	sp			/ set new user SP
	mov	(sp)+, r0
	clr	nofault
	rtt
2:					/ back out on error
	clr	nofault
	tst	(sp)+			/ pop user's sp
	mov	(sp)+, r0
3:
	jsr	r0, call1; jmp _trap	/ call trap on bad SIGILL
	/*NOTREACHED*/
#endif
buserr:
	mov	PS,saveps
	bit	$30000,PS
	bne	2f	/ if previous mode != kernel
	tst	nofault
	bne	1f
	tst	sp
	bne	3f
	/ Kernel stack invalid.
	tst	_panicstr
	beq	4f
	br	.		/ Already paniced, don't overwrite anything
4:
	/ Find a piece of stack so we can panic.
	mov	$intstk+[INTSTK\/2], sp
	mov	$redstak, -(sp)
	jsr	pc, _panic
	/*NOTREACHED*/
	.data
redstak: <kernel red stack violation\0>
	.text

/*
 *	Traps without specialized catchers get vectored here.
 */
trap:
	mov	PS,saveps
2:
	tst	nofault
	bne	1f
3:
	mov	SSR0,ssr
#ifndef	KERN_NONSEP
	mov	SSR1,ssr+2
#endif
	mov	SSR2,ssr+4
	mov	$1,SSR0
	jsr	r0, call1; jmp _trap
	/*NOTREACHED*/
1:
	mov	$1,SSR0
	mov	nofault,(sp)
	rtt

	.globl	call, _runrun
call1:
	mov	saveps, -(sp)
	SPLLOW
	br	1f

call:
	mov	PS, -(sp)
1:
#ifdef	MENLO_KOV
	mov	__ovno,-(sp)		/ save overlay number
#endif
	mov	r1,-(sp)
	mfpd	sp
#ifdef UCB_METER
	.globl	_cnt
	inc	_cnt+V_INTR		/ count device interrupts
#endif UCB_METER
#ifdef	MENLO_KOV
	mov	6(sp), -(sp)
#else
	mov	4(sp), -(sp)
#endif
	bic	$!37,(sp)
	bit	$30000,PS
	beq	1f
	jsr	pc,(r0)+
#ifdef	UCB_NET
	jsr	pc,checknet
#endif
	tstb	_runrun
	beq	2f
	mov	$SWITCHTRAP.,(sp)	/ give up cpu
	jsr	pc,_trap
2:
	tst	(sp)+
	mtpd	sp
	br	2f
1:
	bis	$30000,PS
	jsr	pc,(r0)+
#ifdef	UCB_NET
	jsr	pc,checknet
#endif
	cmp	(sp)+,(sp)+
2:
	mov	(sp)+,r1
#ifdef	MENLO_KOV
	/ Restore previous overlay number and mapping.
	mov	(sp)+,r0		/ get saved overlay number
	cmp	r0,__ovno
	beq	1f
	mov	PS,-(sp)		/ save PS
	SPL7
	mov	r0,__ovno
	asl	r0
	mov	ova(r0), OVLY_PAR
	mov	ovd(r0), OVLY_PDR
	mov	(sp)+,PS		/ restore PS, unmask interrupts
1:
#endif	MENLO_KOV
	tst	(sp)+
	mov	(sp)+,r0
	rtt

#ifdef	UCB_AUTOBOOT
/*
 *  Power fail handling.  On power down, we just set up for the next trap.
 */
#define	PVECT	24			/* power fail vector */
	.globl	powrdown, powrup, _panicstr, _rootdev, hardboot
	.text
powrdown:
	mov	$powrup,PVECT
	SPL7
	br	.

	/*
	 *  Power back on... wait a bit, then attempt a reboot.
	 *  Can't do much since memory management is off
	 *  (hence we are in "data" space).
	 */
#ifndef	KERN_NONSEP
	.data
#endif
powrup:
	mov	$-1,r0
1:
	dec	r0
	bne	1b

	mov	$RB_POWRFAIL, r4
	mov	_rootdev,r3
	jsr	pc,hardboot
	/* NOTREACHED */
	.text
#endif	UCB_AUTOBOOT

#ifdef	UCB_NET
	.globl  _netisr,_netintr
checknet:
	mov	PS,-(sp)
	SPL7
	tst     _netisr                 / net requesting soft interrupt
	beq     3f
#ifdef	MENLO_KOV
	bit	$340,18.(sp)
#else
	bit     $340,16.(sp)
#endif
	bne     3f                      / if prev spl not 0
	SPLNET
	jsr	pc,*$_netintr
3:
	mov	(sp)+,PS
	rts     pc
#endif

#include "dz.h"
#if	NDZ > 0 && defined(DZ_PDMA)
	/*
	 *  DZ-11 pseudo-DMA interrupt routine.
	 *  Called directly from the interrupt vector;
	 *  the device number is in the low bits of the PS.
	 *  Calls dzxint when the end of the buffer is reached.
	 *  The pdma structure is known to be:
	 *	struct pdma {
	 *		struct	dzdevice *p_addr;
	 *		char	*p_mem;
	 *		char	*p_end;
	 *		struct	tty *p_arg;
	 *	};
	 */
	.globl	dzdma, _dzpdma, _dzxint
dzdma:
	mov	PS, -(sp)
	mov	r0, -(sp)
#ifdef	MENLO_KOV
	mov	__ovno, -(sp)
#endif
	mov	r1, -(sp)
	mov	r2, -(sp)
	mov	r3, -(sp)
#ifdef	MENLO_KOV
	mov	12(sp), r3		/ new PS
#else
	mov	10(sp), r3		/ new PS
#endif
#ifdef UCB_METER
	inc	_cnt+V_PDMA		/ count pseudo-DMA interrupts
#endif UCB_METER
	bic	$!37, r3		/ extract device number
	ash	$3+3, r3		/ get offset into dzpdma
	add	$_dzpdma, r3		/ 	for first line
	mov	(r3)+, r2		/ dzaddr in r2; r3 points to p_mem 

#ifdef	UCB_CLIST
	.globl	_clststrt, _clstdesc
	mov	KDSA5, -(sp)		/ save previous mapping
	mov	KDSD5, -(sp)
	mov	_clststrt, KDSA5	/ map in clists
	mov	_clstdesc, KDSD5
#endif	UCB_CLIST

	/ loop until no line is ready
1:
	movb	1(r2), r1		/ dzcsr high byte
	bge	3f			/ test TRDY; branch if none
	bic	$!7, r1			/ extract line number
	ash	$3, r1			/ convert to pdma offset
	add	r3, r1			/ r1 is pointer to pdma.p_mem for line
	mov	(r1)+, r0		/ pdma->p_mem
	cmp	r0, (r1)+		/ cmp p_mem to p_end
	bhis	2f			/ if p_mem >= p_end
	movb	(r0)+, 6(r2)		/ dztbuf = *p_mem++
	mov	r0, -4(r1)		/ update p_mem
	br	1b

	/ buffer is empty; call dzxint
2:
	mov	(r1), -(sp)		/ p_arg
	jsr	pc, _dzxint		/ r0, r1 are modified!
	tst	(sp)+
	br	1b

	/ no more lines ready; return
3:
#ifdef	UCB_CLIST
	mov	(sp)+, KDSD5
	mov	(sp)+, KDSA5		/ restore previous mapping
#endif	UCB_CLIST
	mov	(sp)+, r3
	mov	(sp)+, r2
	mov	(sp)+, r1
#ifdef	MENLO_KOV
	SPL7
	mov	(sp)+, r0
	cmp	r0, __ovno
	beq	1f
	mov	r0, __ovno
	asl	r0
	mov	ova(r0), OVLY_PAR
	mov	ovd(r0), OVLY_PDR
1:
#endif
	mov	(sp)+, r0
	tst	(sp)+
	rtt
#endif	DZ_PDMA

#ifndef	NONFP
	.globl	_savfp, _restfp, _stst
_savfp:
	tst	fpp
	beq	9f			/ No FP hardware
	mov	2(sp),r1
	stfps	(r1)+
	setd
	movf	fr0,(r1)+
	movf	fr1,(r1)+
	movf	fr2,(r1)+
	movf	fr3,(r1)+
	movf	fr4,fr0
	movf	fr0,(r1)+
	movf	fr5,fr0
	movf	fr0,(r1)+
9:
	rts	pc

_restfp:
	tst	fpp
	beq	9f
	mov	2(sp),r1
	mov	r1,r0
	setd
	add	$8.+2.,r1
	movf	(r1)+,fr1
	movf	(r1)+,fr2
	movf	(r1)+,fr3
	movf	(r1)+,fr0
	movf	fr0,fr4
	movf	(r1)+,fr0
	movf	fr0,fr5
	movf	2(r0),fr0
	ldfps	(r0)
9:
	rts	pc

/*
 * Save floating poing error registers.
 * The argument is a pointer to a two word
 * structure.
 */
_stst:
	tst	fpp
	beq	9f
	stst	*2(sp)
9:
	rts	pc

#endif	NONFP

	.globl	_addupc
_addupc:
	mov	r2,-(sp)
	mov	6(sp),r2		/ base of prof with base,leng,off,scale
	mov	4(sp),r0		/ pc
	sub	4(r2),r0		/ offset
	clc
	ror	r0
	mov	6(r2),r1
	clc
	ror	r1
	mul	r1,r0			/ scale
	ashc	$-14.,r0
	inc	r1
	bic	$1,r1
	cmp	r1,2(r2)		/ length
	bhis	1f
	add	(r2),r1			/ base
	mov	nofault,-(sp)
	mov	$2f,nofault
	mfpd	(r1)
	add	12.(sp),(sp)
	mtpd	(r1)
	br	3f
2:
	clr	6(r2)
3:
	mov	(sp)+,nofault
1:
	mov	(sp)+,r2
	rts	pc

#ifdef	DISPLAY
	.globl	_display
_display:
	dec	dispdly
	bge	2f
	clr	dispdly
	mov	PS,-(sp)
	SPLHIGH
	mov	CSW,r1
	bit	$1,r1
	beq	1f
	bis	$30000,PS
	dec	r1
1:
	jsr	pc,fuword
	mov	r0,CSW
	mov	(sp)+,PS
	cmp	r0,$-1
	bne	2f
	mov	$120.,dispdly		/ 2 second delay after CSW fault
2:
	rts	pc
#endif

	.globl	_regloc, _backup
#ifndef	KERN_NONSEP
/*
 *  Backup routine for use with machines with ssr2.
 */
_backup:
	mov	2(sp),r0
	movb	ssr+2,r1
	jsr	pc,1f
	movb	ssr+3,r1
	jsr	pc,1f
	movb	_regloc+7,r1
	asl	r1
	add	r0,r1
	mov	ssr+4,(r1)
	clr	r0
2:
	rts	pc
1:
	mov	r1,-(sp)
	asr	(sp)
	asr	(sp)
	asr	(sp)
	bic	$!7,r1
	movb	_regloc(r1),r1
	asl	r1
	add	r0,r1
	sub	(sp)+,(r1)
	rts	pc

#else	KERN_NONSEP
/*
 * 11/40 version of backup, for use with no SSR2
 */
_backup:
	mov	2(sp),ssr+2
	mov	r2,-(sp)
	jsr	pc,backup
	mov	r2,ssr+2
	mov	(sp)+,r2
	movb	jflg,r0
	bne	2f
	mov	2(sp),r0
	movb	ssr+2,r1
	jsr	pc,1f
	movb	ssr+3,r1
	jsr	pc,1f
	movb	_regloc+7,r1
	asl	r1
	add	r0,r1
	mov	ssr+4,(r1)
	clr	r0
2:
	rts	pc
1:
	mov	r1,-(sp)
	asr	(sp)
	asr	(sp)
	asr	(sp)
	bic	$!7,r1
	movb	_regloc(r1),r1
	asl	r1
	add	r0,r1
	sub	(sp)+,(r1)
	rts	pc

	/ Hard part:  simulate the ssr2 register missing on 11/40.
backup:
	clr	r2			/ backup register ssr1
	mov	$1,bflg			/ clrs jflg
	clrb	fflg
	mov	ssr+4,r0
	jsr	pc,fetch
	mov	r0,r1
	ash	$-11.,r0
	bic	$!36,r0
	jmp	*0f(r0)
0:		t00; t01; t02; t03; t04; t05; t06; t07
		t10; t11; t12; t13; t14; t15; t16; t17

t00:
	clrb	bflg

t10:
	mov	r1,r0
	swab	r0
	bic	$!16,r0
	jmp	*0f(r0)
0:		u0; u1; u2; u3; u4; u5; u6; u7

u6:					/ single op, m[tf]pi, sxt, illegal
	bit	$400,r1
	beq	u5			/ all but m[tf], sxt
	bit	$200,r1
	beq	1f			/ mfpi
	bit	$100,r1
	bne	u5			/ sxt

	/ Simulate mtpi with double (sp)+,dd.
	bic	$4000,r1		/ turn instr into (sp)+
	br	t01

	/ Simulate mfpi with double ss,-(sp).
1:
	ash	$6,r1
	bis	$46,r1			/ -(sp)
	br	t01

u4:					/ jsr
	mov	r1,r0
	jsr	pc,setreg		/ assume no fault
	bis	$173000,r2		/ -2 from sp
	rts	pc

t07:					/ EIS
	clrb	bflg

u0:					/ jmp, swab
u5:					/ single op
#ifndef	NONFP
f5:					/ movei, movfi
ff1:					/ ldfps
ff2:					/ stfps
ff3:					/ stst
#endif
	mov	r1,r0
	br	setreg

t01:					/ mov
t02:					/ cmp
t03:					/ bit
t04:					/ bic
t05:					/ bis
t06:					/ add
t16:					/ sub
	clrb	bflg

t11:					/ movb
t12:					/ cmpb
t13:					/ bitb
t14:					/ bicb
t15:					/ bisb
	mov	r1,r0
	ash	$-6,r0
	jsr	pc,setreg
	swab	r2
	mov	r1,r0
	jsr	pc,setreg

	/ If delta(dest) is zero, no need to fetch source.
	bit	$370,r2
	beq	1f

	/ If mode(source) is R, no fault is possible.
	bit	$7000,r1
	beq	1f

	/ If reg(source) is reg(dest), too bad.
	mov	r2,-(sp)
	bic	$174370,(sp)
	cmpb	1(sp),(sp)+
	beq	u7

	/ Start source cycle.  Pick up value of reg.
	mov	r1,r0
	ash	$-6,r0
	bic	$!7,r0
	movb	_regloc(r0),r0
	asl	r0
	add	ssr+2,r0
	mov	(r0),r0

	/ If reg has been incremented, must decrement it before fetch.
	bit	$174000,r2
	ble	2f
	dec	r0
	bit	$10000,r2
	beq	2f
	dec	r0
2:

	/ If mode is 6,7 fetch and add X(R) to R.
	bit	$4000,r1
	beq	2f
	bit	$2000,r1
	beq	2f
	mov	r0,-(sp)
	mov	ssr+4,r0
	add	$2,r0
	jsr	pc,fetch
	add	(sp)+,r0
2:

	/ Fetch operand. If mode is 3, 5, or 7, fetch *.
	jsr	pc,fetch
	bit	$1000,r1
	beq	1f
	bit	$6000,r1
	bne	fetch
1:
	rts	pc

t17:					/ floating point instructions
#ifndef	NONFP
	clrb	bflg
	mov	r1,r0
	swab	r0
	bic	$!16,r0
	jmp	*0f(r0)
0:		f0; f1; f2; f3; f4; f5; f6; f7

f0:
	mov	r1,r0
	ash	$-5,r0
	bic	$!16,r0
	jmp	*0f(r0)
0:		ff0; ff1; ff2; ff3; ff4; ff5; ff6; ff7

f1:					/ mulf, modf
f2:					/ addf, movf
f3:					/ subf, cmpf
f4:					/ movf, divf
ff4:					/ clrf
ff5:					/ tstf
ff6:					/ absf
ff7:					/ negf
	inc	fflg
	mov	r1,r0
	br	setreg

f6:
	bit	$400,r1
	beq	f1			/ movfo
	br	f5			/ movie

f7:
	bit	$400,r1
	beq	f5			/ movif
	br	f1			/ movof

ff0:					/ cfcc, setf, setd, seti, setl
#endif
u1:					/ br
u2:					/ br
u3:					/ br
u7:					/ illegal
	incb	jflg
	rts	pc

setreg:
	mov	r0,-(sp)
	bic	$!7,r0
	bis	r0,r2
	mov	(sp)+,r0
	ash	$-3,r0
	bic	$!7,r0
	movb	0f(r0),r0
	tstb	bflg
	beq	1f
	bit	$2,r2
	beq	2f
	bit	$4,r2
	beq	2f
1:
	cmp	r0,$20
	beq	2f
	cmp	r0,$-20
	beq	2f
	asl	r0
2:
#ifndef	NONFP
	tstb	fflg
	beq	3f
	asl	r0
	stfps	r1
	bit	$200,r1
	beq	3f
	asl	r0
3:
#endif
	bisb	r0,r2
	rts	pc

0:	.byte	0,0,10,20,-10,-20,0,0

fetch:
	bic	$1,r0
	mov	nofault,-(sp)
	mov	$1f,nofault
	mfpi	(r0)
	mov	(sp)+,r0
	mov	(sp)+,nofault
	rts	pc

1:
 	mov	(sp)+,nofault
	clrb	r2			/ clear out dest on fault
	mov	$-1,r0
	rts	pc

	.bss
bflg:	.=.+1
jflg:	.=.+1
fflg:	.=.+1
	.text
#endif	KERN_NONSEP

	.globl	_fuibyte, _fubyte, _suibyte, _subyte
_fuibyte:
#ifndef	NONSEPARATE
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,giword
	br	2f
#endif

_fubyte:
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,gword

2:
	cmp	r1,2(sp)
	beq	1f
	swab	r0
1:
	bic	$!377,r0
	rts	pc

_suibyte:
#ifndef	NONSEPARATE
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,giword
	mov	r0,-(sp)
	cmp	r1,4(sp)
	beq	1f
	movb	6(sp),1(sp)
	br	2f
1:
	movb	6(sp),(sp)
2:
	mov	(sp)+,r0
	jsr	pc,piword
	clr	r0
	rts	pc
#endif

_subyte:
	mov	2(sp),r1
	bic	$1,r1
	jsr	pc,gword
	mov	r0,-(sp)
	cmp	r1,4(sp)
	beq	1f
	movb	6(sp),1(sp)
	br	2f
1:
	movb	6(sp),(sp)
2:
	mov	(sp)+,r0
	jsr	pc,pword
	clr	r0
	rts	pc

	.globl	_fuiword, _fuword, _suiword, _suword
_fuiword:
#ifndef	NONSEPARATE
	mov	2(sp),r1
fuiword:
	jsr	pc,giword
	rts	pc
#endif

_fuword:
	mov	2(sp),r1
fuword:
	jsr	pc,gword
	rts	pc

gword:
#ifndef	NONSEPARATE
	mov	PS,-(sp)
#ifdef	UCB_NET
	bis	$30000,PS		/ just in case; dgc; %%%%
#endif
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mfpd	(r1)
	mov	(sp)+,r0
	br	1f

giword:
#endif
	mov	PS,-(sp)
#ifdef	UCB_NET
	bis	$30000,PS		/ just in case; dgc; %%%%
#endif
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mfpi	(r1)
	mov	(sp)+,r0
	br	1f

_suiword:
#ifndef	NONSEPARATE
	mov	2(sp),r1
	mov	4(sp),r0
suiword:
	jsr	pc,piword
	rts	pc
#endif

_suword:
	mov	2(sp),r1
	mov	4(sp),r0
suword:
	jsr	pc,pword
	rts	pc

pword:
#ifndef	NONSEPARATE
	mov	PS,-(sp)
#ifdef	UCB_NET
	bis	$30000,PS		/ just in case; dgc; %%%%
#endif
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mov	r0,-(sp)
	mtpd	(r1)
	br	1f

piword:
#endif
	mov	PS,-(sp)
#ifdef	UCB_NET
	bis	$30000,PS		/ just in case; dgc; %%%%
#endif
	SPLHIGH
	mov	nofault,-(sp)
	mov	$err,nofault
	mov	r0,-(sp)
	mtpi	(r1)
1:
	mov	(sp)+,nofault
	mov	(sp)+,PS
	rts	pc

err:
	mov	(sp)+,nofault
	mov	(sp)+,PS
	tst	(sp)+
	mov	$-1,r0
	rts	pc

	.globl	_copyin, _copyiin, _copyout, _copyiout
_copyin:
#ifndef	NONSEPARATE
	jsr	pc,copsu
1:
	mfpd	(r0)+
	mov	(sp)+,(r1)+
	sob	r2,1b
	br	2f
#endif

_copyiin:
	jsr	pc,copsu
1:
	mfpi	(r0)+
	mov	(sp)+,(r1)+
	sob	r2,1b
	br	2f

_copyout:
#ifndef	NONSEPARATE
	jsr	pc,copsu
1:
	mov	(r0)+,-(sp)
	mtpd	(r1)+
	sob	r2,1b
	br	2f
#endif

_copyiout:
	jsr	pc,copsu
1:
	mov	(r0)+,-(sp)
	mtpi	(r1)+
	sob	r2,1b
2:
	mov	(sp)+,nofault
	mov	(sp)+,r2
	clr	r0
	rts	pc

copsu:
#ifdef	UCB_NET
	bis	$30000,PS		/ make sure that we copy to/from user space
					/ this is a test - dgc - %%%%
#endif
	mov	(sp)+,r0
	mov	r2,-(sp)
	mov	nofault,-(sp)
	mov	r0,-(sp)
	mov	10(sp),r0
	mov	12(sp),r1
	mov	14(sp),r2
	asr	r2
	mov	$1f,nofault
	rts	pc

1:
	mov	(sp)+,nofault
	mov	(sp)+,r2
	mov	$-1,r0
	rts	pc

	.globl	_idle, _waitloc
_idle:
	mov	PS,-(sp)
	SPLLOW
	wait
waitloc:
	mov	(sp)+,PS
	rts	pc

	.data
_waitloc:
	waitloc
	.text
#if	defined(PROFILE) && !defined(ENABLE34)
/ These words are to insure that times reported for _save
/ do not include those spent while in idle mode, when
/ statistics are gathered for system profiling.
/
	rts	pc
	rts	pc
	rts	pc
#endif	defined(PROFILE) && !defined(ENABLE34)

	.globl	_save, _resume
_save:
	mov	(sp)+,r1
	mov	(sp),r0
	mov	r2,(r0)+
	mov	r3,(r0)+
	mov	r4,(r0)+
	mov	r5,(r0)+
	mov	sp,(r0)+
#ifdef	MENLO_KOV
	mov	__ovno,(r0)+
#endif
	mov	r1,(r0)+
	clr	r0
	jmp	(r1)

_resume:
	mov	2(sp),r0		/ new process
	mov	4(sp),r1		/ new stack
	SPL7
	mov	r0,KDSA6		/ In new process
	mov	(r1)+,r2
	mov	(r1)+,r3
	mov	(r1)+,r4
	mov	(r1)+,r5
	mov	(r1)+,sp
#ifdef	MENLO_KOV
	mov	(r1)+,r0
	cmp	r0,__ovno
	beq	1f
	mov	r0,__ovno
	asl	r0
	mov	ova(r0), OVLY_PAR
	mov	ovd(r0), OVLY_PDR
1:
#endif	MENLO_KOV
	mov	$1,r0
	SPLLOW
	jmp	*(r1)+

/*
 *	Note that in the Berkeley system, calls to spl's except splx
 *	are substituted in line in the assembly code on machines
 *	with the spl instruction or mtps/mfps.  Splx is done by macros
 *	in param.h. See the makefile, :splfix.spl, :splfix.mtps,
 *	:splfix.movb and param.h.  Calls to __spl# (_spl# in C)
 *	are always expanded in-line and do not return the previous priority.
 */

#if	defined(KERN_NONSEP) && PDP11 != 34 && PDP11 != 23 && PDP11 != 24
	/  Spl's for machines (like 11/40) without spl or m[tf]ps instructions.
	.globl	_spl0, _spl1, _spl4, _spl5, _spl6, _spl7
_spl0:
	movb	PS,r0
	clrb	PS
	rts	pc
_spl1:
	movb	PS,r0
	movb	$40, PS
	rts	pc
_spl4:
	movb	PS,r0
	movb	$200, PS
	rts	pc
_spl5:
	movb	PS,r0
	movb	$240, PS
	rts	pc
_spl6:
	movb	PS,r0
	movb	$300, PS
	rts	pc
_spl7:
	movb	PS,r0
	movb	$HIPRI, PS
	rts	pc
#endif

	.globl	_copy, _clear, _kdsa6
#ifdef	CGL_RTP
	.globl	_copyu, _wantrtp, _runrtp
#endif

/*
 * Copy count clicks from src to dst.
 * Uses KDSA5 and 6 to copy with mov instructions.
 * Interrupt routines must restore segmentation registers if needed;
 * see seg.h.
 * Note that if CGL_RTP is defined, it checks whether
 * the real-time process is runnable once each loop,
 * and preempts the current process if necessary
 * (which must not swap before this finishes!).
 *
 * copy(src, dst, count)
 * memaddr src, dst;
 * int count;
 */
_copy:
	jsr	r5, csv
#ifdef	UCB_NET
	mov	PS,-(sp)		/ have to lock out interrupts...
	SPL7
	mov	KDSA5,-(sp)		/ save seg5
	mov	KDSD5,-(sp)		/ save seg5
#endif
	mov	10(r5),r3		/ count
	beq	3f
	mov	4(r5),KDSA5		/ point KDSA5 at source
	mov	$RO,KDSD5		/ 64 bytes, read-only
	mov	sp,r4
	mov	$eintstk,sp		/ switch to intstk
	mov	KDSA6,_kdsa6
	mov	6(r5),KDSA6		/ point KDSA6 at destination
	mov	$RW,KDSD6		/ 64 bytes, read-write
1:
#ifdef	CGL_RTP
	tst	_wantrtp
	bne	preempt
9:
#endif
	mov	$5*8192.,r0
	mov	$6*8192.,r1
#if	PDP11==70
	mov	$4.,r2			/ copy one click (4*16)
#else
	mov	$8.,r2			/ copy one click (8*8)
#endif
2:
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
#if	PDP11==70
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
#endif
	sob	r2,2b

	inc	KDSA5			/ next click
	inc	KDSA6
	dec	r3
	bne	1b
	mov	_kdsa6,KDSA6
	mov	$usize-1\<8|RW, KDSD6
	clr	_kdsa6
#ifndef	NOKA5
	.globl	_seg5
	mov	_seg5+SE_DESC, KDSD5	/ normalseg5();
	mov	_seg5+SE_ADDR, KDSA5	/ (restore all mapping)
#endif	NOKA5
	mov	r4,sp			/ back to normal stack
3:
#ifdef	UCB_NET
	mov	(sp)+,KDSD5		/ restore seg5
	mov	(sp)+,KDSA5		/ restore seg5
	mov	(sp)+,PS		/ back to normal priority
#endif
	jmp	cret

#ifdef	CGL_RTP
/*
 * Save our state and restore enough context for a process switch.
 */
preempt:
	mov	KDSA6, r0
	mov	_kdsa6,KDSA6		/ back to our u.
	mov	$usize-1\<8|RW, KDSD6
	clr	_kdsa6
	mov	r4, sp			/ back to normal stack
	mov	KDSA5, -(sp)
	mov	r0, -(sp)		/ KDSA6
#ifndef	NOKA5
	mov	_seg5+SE_DESC, KDSD5	/ normalseg5();
	mov	_seg5+SE_ADDR, KDSA5	/ (restore all mapping)
#endif	NOKA5
	jsr	pc, _runrtp		/ switch context and run rtpp

	/ Now continue where we left off.
	mov	(sp)+, r0		/ KDSA6
	mov	(sp)+, KDSA5
	mov	$RO,KDSD5		/ 64 bytes, read-only
	mov	KDSA6,_kdsa6
	mov	$eintstk, sp
	mov	r0, KDSA6
	mov	$RW,KDSD6		/ 64 bytes, read-write
	br	9b

/*
 * Copy the u. to dst; not preemptable.
 * Uses KDSA5 to copy with mov instructions.
 * Interrupt routines must restore segmentation registers if needed;
 * see seg.h.
 *
 * copyu(dst)
 * memaddr dst;
 */
_copyu:
	jsr	r5, csv
	mov	4(r5),KDSA5		/ point KDSA5 at dst.
	mov	$usize-1\<8.|RW,KDSD5
	mov	$6*8192.,r0
	mov	$5*8192.,r1
#if	PDP11==70
	mov	$4.*usize,r2		/ copy 4*16 bytes per click
#else
	mov	$8.*usize,r2		/ copy 8*8 bytes per click
#endif
2:
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
#if	PDP11==70
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
	mov	(r0)+,(r1)+
#endif
	sob	r2,2b

#ifndef	NOKA5
	mov	_seg5+SE_DESC, KDSD5	/ normalseg5();
	mov	_seg5+SE_ADDR, KDSA5	/ (restore all mapping)
#endif	NOKA5
	jmp	cret
#endif	CGL_RTP

/*
 * Clear count clicks at dst.
 * Uses KDSA5.
 * Interrupt routines must restore segmentation registers if needed;
 * see seg.h.
 *
 * clear(dst, count)
 * memaddr dst;
 * int count;
 */
_clear:
	jsr	r5, csv
#ifdef	UCB_NET
	mov	KDSA5,-(sp)		/ save seg5
	mov	KDSD5,-(sp)		/ save seg5
#endif
	mov	4(r5),KDSA5		/ point KDSA5 at source
	mov	$RW,KDSD5		/ 64 bytes, read-write
	mov	6(r5),r3		/ count
	beq	3f
1:
#ifdef	CGL_RTP
	tst	_wantrtp
	bne	clrpreempt
9:
#endif
	mov	$5*8192.,r0
	mov	$8.,r2			/ clear one click (8*8)
2:
	clr	(r0)+
	clr	(r0)+
	clr	(r0)+
	clr	(r0)+
	sob	r2,2b

	inc	KDSA5			/ next click
	dec	r3
	bne	1b
3:
#ifndef	NOKA5
	mov	_seg5+SE_DESC, KDSD5	/ normalseg5();
	mov	_seg5+SE_ADDR, KDSA5	/ (restore all mapping)
#endif	NOKA5
#ifdef	UCB_NET
	mov	(sp)+,KDSD5		/ restore seg5
	mov	(sp)+,KDSA5		/ restore seg5
#endif
	jmp	cret

#ifdef	CGL_RTP
clrpreempt:
	mov	KDSA5, -(sp)
#ifndef	NOKA5
	mov	_seg5+SE_DESC, KDSD5	/ normalseg5();
	mov	_seg5+SE_ADDR, KDSA5	/ (restore all mapping)
#endif	NOKA5
	jsr	pc, _runrtp		/ switch context and run rtpp
	/*
	 * Now continue where we left off.
	 */
	mov	(sp)+, KDSA5
	mov	$RW,KDSD5		/ 64 bytes, read-write
	br	9b
#endif	CGL_RTP

#ifdef	UCB_NET
/*
 *	copyv(fromaddr,toaddr,count)
 *	virtual_addr fromaddr,toaddr;
 *	unsigned count;
 *
 *	Copy two arbitrary pieces of PDP11 virtual memory from one location
 *	to another.  Up to 8K bytes can be copied at one time.
 *
 *	A PDP11 virtual address is a two word value; a 16 bit "click" that
 *	defines the start in physical memory of an 8KB segment and an offset.
 */

	.globl	_copyv

	.data
copyvsave: 0			/ saved copy of KDSA6
	.text

_copyv:	jsr	r5,csv

	tst	14(r5)		/* if (count == 0)		*/
	jeq	copyvexit	/* 	return;			*/
	cmp	$20000,14(r5)	/* if (count >= 8192)		*/
	jlos	copyvexit	/*	return;			*/

	mov	PS,-(sp)	/* Lock out interrupts. sigh... */
	SPL7

	mov	KDSA5,-(sp)	/* save seg5			*/
	mov	KDSD5,-(sp)

	mov	4(r5),KDSA5	/* seg5 = fromclick		*/
	mov	$128.-1\<8.|RO,KDSD5
	mov	10(r5),r1	/* click = toclick		*/
	mov	6(r5),r2	/* foff = fromoffset		*/
	add	$5*8192.,r2	/* foff = virtual addr (page 5)	*/
	mov	12(r5),r3	/* toff = tooffset		*/
	add	$6*8192.,r3	/* toff = virtual addr (page 6)	*/
	mov	14(r5),r0	/* count = count		*/

	/* Note: the switched stack is only for use of a fatal	*/
	/* kernel trap occurring during the copy; otherwise we	*/
	/* might conflict with the other copy routine		*/
	mov	sp,r4		/* switch stacks		*/
	mov	$eintstk,sp
	mov	KDSA6,copyvsave

	mov	r1,KDSA6	/* seg6 = click			*/
	mov	$128.-1\<8.|RW,KDSD6

	/****** Finally do the copy 			   ******/
	mov	r3,r1		/* Odd addresses or count?	*/
	bis	r2,r1
	bis	r0,r1
	bit	$1,r1
	bne	copyvodd	/* Branch if odd		*/

	asr	r0		/* Copy a word at a time	*/
1:	mov	(r2)+,(r3)+
	sob	r0,1b
	br	copyvdone

copyvodd: movb	(r2)+,(r3)+	/* Copy a byte at a time	*/
	sob	r0,copyvodd
/	br	copyvdone

copyvdone:
	mov	copyvsave,KDSA6	/* remap in the stack		*/
	mov	$usize-1\<8.|RW,KDSD6
	mov	r4,sp
	mov	(sp)+,KDSD5	/* restore seg5			*/
	mov	(sp)+,KDSA5
	mov	(sp)+,PS	/* unlock interrupts		*/

copyvexit:
	clr	r0
	clr	r1
	jmp	cret
#endif	UCB_NET

hardprobe:
#ifndef	NONFP
	/ Test for floating point capability.
fptest:
	mov	$fpdone, nofault
	setd
	inc	fpp
fpdone:
#endif	NONFP

	/ Test for SSR3 and UNIBUS map capability.  If there is no SSR3,
	/ the first test of SSR3 will trap and we skip past septest.
ubmaptest:
	mov	$cputest, nofault
#ifdef	UNIBUS_MAP
	bit	$20, SSR3
	beq	septest
	incb	_ubmap
#endif

	/ Test for separate I/D capability.
septest:
#ifdef	NONSEPARATE
	/ Don't attempt to determine whether we've got separate I/D
	/ (but just in case we do, we must force user unseparated
	/ because boot will have turned on separation if possible).
	bic	$1, SSR3
#else
	/ Test for user I/D separation (not kernel).
	bit	$1, SSR3
	beq	cputest
	incb	_sep_id
#endif	NONSEPARATE

	/ Try to find out what kind of cpu this is.
	/ Defaults are 40 for nonseparate and 45 for separate.
	/ Cputype will be one of: 24, 40, 60, 45, 44, 70.
cputest:
#ifndef	NONSEPARATE
	tstb	_sep_id
	beq	nonsepcpu

	tstb	_ubmap
	beq	cpudone

	mov	$1f, nofault
	mfpt
	mov	$44., _cputype
	/ Disable cache parity interrupts.
	bis	$CCR_DCPI, *$PDP1144_CCR
	br	cpudone
1:
	mov	$70., _cputype
	/ Disable UNIBUS and nonfatal traps.
	bis	$CCR_DUT|CCR_DT, *$PDP1170_CCR
	br	cpudone

nonsepcpu:
#endif	NONSEPARATE
	tstb	_ubmap
	beq	1f
	mov	$24., _cputype
	br	cpudone
1:
	mov	$cpudone, nofault
	tst	PDP1160_MSR
	mov	$60., _cputype
	/ Disable cache parity error traps.
	bis	$CCR_DT, *$PDP1160_CCR

cpudone:
	/ Test for stack limit register; set it if present.
	mov	$1f, nofault
	mov	$intstk-256., STACKLIM
1:

#ifdef	ENABLE34
	/ Test for an ENABLE/34.  We are very cautious since
	/ the ENABLE's PARs  are in the range of the floating
	/ addresses.
	tstb	_ubmap
	bne	2f
	mov	$2f, nofault
	mov	32., r0
	mov	$ENABLE_KISA0, r1
1:
	tst	(r1)+
	sob	r0, 1b

	tst	*$PDP1170_LEAR
	tst	*$ENABLE_SSR3
	tst	*$ENABLE_SSR4
	incb	_enable34
	incb	_ubmap

	/ Turn on an ENABLE/34.  Enableon() is a C routine
	/ which does a PAR shuffle and turns mapping on.
	.globl	_enableon
	.globl	_UISA, _UDSA, _KISA0, _KISA6, _KDSA1, _KDSA2, _KDSA5, _KDSA6

	.data
_UISA:	DEC_UISA
_UDSA:	DEC_UDSA
_KISA0:	DEC_KISA0
_KISA6:	DEC_KISA6
_KDSA1:	DEC_KDSA1
_KDSA2:	DEC_KDSA2
_KDSA5:	DEC_KDSA5
_KDSA6:	DEC_KDSA6
	.text

	mov	$ENABLE_UISA, _UISA
	mov	$ENABLE_UDSA, _UDSA
	mov	$ENABLE_KISA0, _KISA0
	mov	$ENABLE_KISA6, _KISA6
	mov	$ENABLE_KDSA1, _KDSA1
	mov	$ENABLE_KDSA2, _KDSA2
	mov	$ENABLE_KDSA5, _KDSA5
	mov	$ENABLE_KDSA6, _KDSA6
	mov	$ENABLE_KDSA6, _ka6
	jsr	pc, _enableon

2:
#endif	ENABLE34

	clr	nofault
	rts	pc

/*
 * Long quotient
 */
	.globl	ldiv, lrem
ldiv:
	jsr	r5,csv
	mov	10.(r5),r3
	sxt	r4
	bpl	1f
	neg	r3
1:
	cmp	r4,8.(r5)
	bne	hardldiv
	mov	6.(r5),r2
	mov	4.(r5),r1
	bge	1f
	neg	r1
	neg	r2
	sbc	r1
	com	r4
1:
	mov	r4,-(sp)
	clr	r0
	div	r3,r0
	mov	r0,r4			/high quotient
	mov	r1,-(sp)		/ Stash interim result
	mov	r1,r0
	mov	r2,r1
	div	r3,r0
	bvc	1f
	mov	(sp),r0			/ Recover interim result.
	mov	r2,r1			/ (Regs may be clobbered by failed div.)
	sub	r3,r0			/ this is the clever part
	div	r3,r0
	tst	r1
	sxt	r1
	add	r1,r0			/ cannot overflow!
1:
	tst	(sp)+			/ Pop temp off stack.
	mov	r0,r1
	mov	r4,r0
	tst	(sp)+
	bpl	9f
	neg	r0
	neg	r1
	sbc	r0
9:
	jmp	cret

hardldiv:
	iot				/ ``Cannot happen''

/*
 * Long remainder
 */
lrem:
	jsr	r5,csv
	mov	10.(r5),r3
	sxt	r4
	bpl	1f
	neg	r3
1:
	cmp	r4,8.(r5)
	bne	hardlrem
	mov	6.(r5),r2
	mov	4.(r5),r1
	mov	r1,r4
	bge	1f
	neg	r1
	neg	r2
	sbc	r1
1:
	clr	r0
	div	r3,r0
	mov	r1,-(sp)		/ Stash interim result.
	mov	r1,r0
	mov	r2,r1
	div	r3,r0
	bvc	1f
	mov	(sp),r0			/ Recover interim result.
	mov	r2,r1			/ (Regs may be clobbered by failed div.)
	sub	r3,r0
	div	r3,r0
	tst	r1
	beq	9f
	add	r3,r1
1:
	tst	(sp)+			/ Pop temp off stack.
	tst	r4
	bpl	9f
	neg	r1
9:
	sxt	r0
	jmp	cret

hardlrem:
	iot				/ ``Cannot happen''

	.globl	lmul
lmul:
	mov	r2,-(sp)
	mov	r3,-(sp)
	mov	8(sp),r2
	sxt	r1
	sub	6(sp),r1
	mov	12.(sp),r0
	sxt	r3
	sub	10.(sp),r3
	mul	r0,r1
	mul	r2,r3
	add	r1,r3
	mul	r2,r0
	sub	r3,r0
	mov	(sp)+,r3
	mov	(sp)+,r2
	rts	pc

#ifdef	UCB_NET
	.globl  _htonl,_htons,_ntohl,_ntohs
_htonl:
_ntohl:
	mov     2(sp),r0
	mov     4(sp),r1
	swab    r0
	swab    r1
	rts     pc

_htons:
_ntohs:
	mov     2(sp),r0
	swab    r0
	rts     pc

	.globl  _badaddr
_badaddr:
	mov     2(sp),r0
	mov     nofault,-(sp)
	mov     $1f,nofault
	tst     (r0)
	clr     r0
	br      2f
1:
	mov     $-1,r0
2:
	mov     (sp)+,nofault
	rts     pc

	.globl  _bzero
_bzero:
	mov     2(sp),r0
	beq	3f		/ error checking...  dgc
	mov     4(sp),r1
	beq	3f		/ error checking ... dgc
	bit     $1,r0
	bne     1f
	bit     $1,r1
	bne     1f
	asr     r1
2:      clr     (r0)+
	sob     r1,2b
	rts     pc

1:      clrb    (r0)+
	sob     r1,1b
3:
	rts     pc
#endif  UCB_NET

	.globl	csv, cret
#ifndef	MENLO_KOV
csv:
	mov	r5,r0
	mov	sp,r5
	mov	r4,-(sp)
	mov	r3,-(sp)
	mov	r2,-(sp)
	jsr	pc,(r0)

cret:
	mov	r5,r2
	mov	-(r2),r4
	mov	-(r2),r3
	mov	-(r2),r2
	mov	r5,sp
	mov	(sp)+,r5
	rts	pc

#else	MENLO_KOV
	.globl  __ovno
	.data
__ovno:	0
	.text
/*
 * Inter-overlay calls call thunk which calls ovhndlr to
 * save registers.  Intra-overlay calls may call function
 * directly which calls csv to save registers.
 */
csv:
	mov	r5,r1
	mov	sp,r5
	mov	__ovno,-(sp)		/ overlay is extra (first) word in mark
	mov	r4,-(sp)
	mov	r3,-(sp)
	mov	r2,-(sp)
	jsr	pc,(r1)			/ jsr part is sub $2,sp

cret:
	mov	r5,r2
	/ Get the overlay out of the mark, and if it is non-zero
	/ make sure it is the currently loaded one.
	mov	-(r2),r4
	bne	1f			/ zero is easy
2:
	mov	-(r2),r4
	mov	-(r2),r3
	mov	-(r2),r2
	mov	r5,sp
	mov	(sp)+,r5
	rts	pc

	/ Not returning to base segment, so check that the right
	/ overlay is mapped in, and if not change the mapping.
1:
	cmp	r4,__ovno
	beq	2b			/ lucked out!

	/ If return address is in base segment, then nothing to do.
	cmp	2(r5),$_etext
	blos	2b

	/ Returning to wrong overlay --- do something!
	mov	PS,-(sp)		/ save PS 
	SPL7
	mov	r4,__ovno
	asl	r4
	mov	ova(r4), OVLY_PAR
	mov	ovd(r4), OVLY_PDR
	mov	(sp)+,PS			/ restore PS, unmask interrupts
	/ Could measure switches[ovno][r4]++ here.
	jmp	2b

/*
 * Ovhndlr1 through ovhndlr7  are called from the thunks,
 * after the address to return to is put in r1.  This address is
 * that after the call to csv, which will be skipped.
 */
	.globl	ovhndlr1, ovhndlr2, ovhndlr3, ovhndlr4 
	.globl	ovhndlr5, ovhndlr6, ovhndlr7
ovhndlr1:
	mov	$1,r0
	br	ovhndlr
ovhndlr2:
	mov	$2,r0
	br	ovhndlr
ovhndlr3:
	mov	$3,r0
	br	ovhndlr
ovhndlr4:
	mov	$4,r0
	br	ovhndlr
ovhndlr5:
	mov	$5,r0
	br	ovhndlr
ovhndlr6:
	mov	$6,r0
	br	ovhndlr
ovhndlr7:
	mov	$7,r0
	br	ovhndlr
/*
 * Ovhndlr makes the argument (in r0) be the current overlay,
 * saves the registers ala csv (but saves the previous overlay number),
 * and then jmp's to the function, skipping the function's initial
 * call to csv.
 */
ovhndlr:
	mov	sp,r5
	mov	__ovno,-(sp)		/ save previous overlay number
	cmp	r0,__ovno		/ correct overlay mapped?
	bne	2f
1:	mov	r4,-(sp)
	mov	r3,-(sp)
	mov	r2,-(sp)
	jsr	pc,(r1)			/ skip function's call to csv

2:	mov	PS,-(sp)		/ save PS
	SPL7
	mov	r0,__ovno		/ set new overlay number
	asl	r0
	mov	ova(r0), OVLY_PAR
	mov	ovd(r0), OVLY_PDR
	mov	(sp)+,PS		/ restore PS, unmask interrupts
	jbr	1b
#endif	MENLO_KOV

/*
 * Save regs r0, r1, r2, r3, r4, r5, r6, K[DI]SA6
 * starting at data location 0300, in preparation for dumping core.
 */
	.globl	_saveregs
_saveregs:
#ifdef	KERN_NONSEP
	movb	$RW, KISD0		/ write enable
#endif
	mov	r0,300
	mov	$302,r0
	mov	r1,(r0)+
	mov	r2,(r0)+
	mov	r3,(r0)+
	mov	r4,(r0)+
	mov	r5,(r0)+
	mov	sp,(r0)+
	mov	KDSA6,(r0)+
	mov	KDSA5,(r0)+
	rts	pc

	.globl _nulldev, _nullsys

_nulldev:		/ placed in insignificant entries in bdevsw and cdevsw
_nullsys:		/ ignored system call
	rts	pc

	.globl	_u
_u	= 140000
#ifndef UCB_NET
usize	= 16.
#else
usize   = 32.
#endif

	.data
	.globl	_ka6, _cputype, _sep_id, _ubmap
#ifdef	ENABLE34
	.globl	_enable34
_enable34: .byte 0
#endif
_ubmap:	.byte 0
_sep_id: .byte 0
	.even

#ifdef	KERN_NONSEP
#ifdef	ENABLE34
_ka6:	DEC_KISA6
#else
_ka6:	KISA6
#endif
_cputype:40.

#else	KERN_NONSEP
#ifdef	ENABLE34
_ka6:	DEC_KDSA6
#else
_ka6:	KDSA6
#endif
_cputype:45.
#endif	KERN_NONSEP

	.bss
	.even
intstk:	.=.+INTSTK		/ temporary stack while KDSA6 is repointed
eintstk: .=.+2			/ initial top of intstk

	.data
nofault:.=.+2
#ifndef	NONFP
fpp:	.=.+2
#endif
ssr:	.=.+6
#ifdef	DISPLAY
dispdly:.=.+2
#endif
saveps:	.=.+2

#if	defined(PROFILE) && !defined(ENABLE34)
	.text
/*
 * System profiler
 *
 * Expects to have a KW11-P in addition to the line-frequency
 * clock, and it should be set to BR7.
 * Uses supervisor I space register 2 and 3 (040000 - 0100000)
 * to maintain the profile.
 */

CCSB	= 172542
CCSR	= 172540
_probsiz = 37777

	.globl	_isprof, _sprof, _probsiz, _mode
/
/ Enable clock interrupts for system profiling
/
_isprof:
	mov	$1f,nofault
	mov	$_sprof,104		/ interrupt
	mov	$340,106		/ pri
	mov	$100.,CCSB		/ count set = 100
	mov	$113,CCSR		/ count down, 10kHz, repeat
1:
	clr	nofault
	rts	pc

/
/ Process profiling clock interrupts
/
_sprof:
	mov	r0,-(sp)
	mov	PS,r0
	ash	$-10.,r0
	bic	$!14,r0			/ mask out all but previous mode
	add	$1,_mode+2(r0)
	adc	_mode(r0)
	cmp	r0,$14			/ user
	beq	done
	mov	2(sp),r0		/ pc
	asr	r0
	asr	r0
	bic	$140001,r0
	cmp	r0,$_probsiz
	blo	1f
	inc	_outside
	br	done
1:
	mov	$10340,PS		/ Set previous mode to supervisor
	mfpi	40000(r0)
	inc	(sp)
	mtpi	40000(r0)
done:
	mov	(sp)+,r0
	mov	$113,CCSR
	rtt

	.data
_mode:	.=.+16.
_outside: .=.+2
#endif	defined(PROFILE) && !defined(ENABLE34)
