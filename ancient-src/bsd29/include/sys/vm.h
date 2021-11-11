#ifdef	UCB_METER

/*
 * System totals computed every five seconds
 */
struct	vmrate {
#define	v_first	v_swtch	
	u_short	v_swtch;	/* context switches */
	u_short	v_trap;		/* calls to trap */
	u_short	v_syscall;	/* calls to syscall() */
	u_short	v_intr;		/* device interrupts */
	u_short	v_pdma;		/* pseudo-DMA interrupts */
	u_short	v_ovly;		/* overlay emts */
	u_short	v_pswpin;	/* pages swapped in */
	u_short	v_pswpout;	/* pages swapped out */
	u_short	v_swpin;	/* swapins */
	u_short	v_swpout;	/* swapouts */
#define	v_last	v_swpout	
};

struct	vmsum {
#define	vs_first	vs_swtch
	long	vs_swtch;	/* context switches */
	long	vs_trap;	/* calls to trap */
	long	vs_syscall;	/* calls to syscall() */
	long	vs_intr;	/* device interrupts */
	long	vs_pdma;	/* pseudo-DMA interrupts */
	long	vs_ovly;	/* overlay emts */
	long	vs_pswpin;	/* clicks swapped in */
	long	vs_pswpout;	/* clicks swapped out */
	long	vs_swpin;	/* swapins */
	long	vs_swpout;	/* swapouts */
#define	vs_last	vs_swpout
};

struct	vmtotal {
	u_short	t_rq;		/* length of run queue */
	u_short	t_dw;		/* jobs in ``disk wait'' (neg priority) */
	u_short	t_sl;		/* jobs sleeping in core */
	u_short	t_sw;		/* swapped out runnable/short block jobs */
	long	t_vm;		/* total virtual memory, clicks */
	long	t_avm;		/* active virtual memory, clicks */
	size_t	t_rm;		/* total real memory, clicks */
	size_t	t_arm;		/* active real memory, clicks */
	long	t_vmtxt;	/* virtual memory used by text, clicks */
	long	t_avmtxt;	/* active virtual memory used by text, clicks */
	size_t	t_rmtxt;	/* real memory used by text, clicks */
	size_t	t_armtxt;	/* active real memory used by text, clicks */
	u_short	t_free;		/* free memory, kb */
};

#ifdef	KERNEL
struct	vmrate	cnt, rate;
struct	vmsum	sum;
struct	vmtotal	total;
u_short	avefree;		/* smoothed average free memory, kb */
size_t	freemem;		/* current free, clicks */
#endif	KERNEL

#endif	UCB_METER
