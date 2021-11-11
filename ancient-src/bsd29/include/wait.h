/*
 * this file holds definitions relevent to the (new) wait2 system call
 * this system call is similar to the standard "wait" system call
 * except that it has a second argument whose bits indicate certain
 * options to the behavior of the system call
 */

/*
 * structure of the int returned in the "int" pointed to by the
 * first argument to the system call.  two substructures are distinguished
 * by a test: if (w_stopval == WSTOPPED) then the second one applies
 * other wise the first one applies.
 */

union	wait	{
	int	w_status;		/* to keep lint happy - used
					   (with &) in actual syscall*/
	struct {			/* to keep the code readable? */
		short	w_Termsig:7;	/* termination signal */
		short	w_Coredump:1;	/* core dump indicator */
		short	w_Retcode:8;	/* program return code via exit
					   valid if w_termsig == 0 */
	} w_T;				/* terminated process status */
	struct {
		short	w_Stopval:8;	/* stop value flag */
		short	w_Stopsig:8;	/* signal that stopped us */
	} w_S;				/* stopped process status */
};
#define	w_termsig	w_T.w_Termsig
#define	w_coredump	w_T.w_Coredump
#define	w_retcode	w_T.w_Retcode
#define	w_stopval	w_S.w_Stopval
#define	w_stopsig	w_S.w_Stopsig


#define	WSTOPPED	0177	/* value of s.stopval if process is stopped */

#define	WNOHANG		1	/* option bit indicating that the caller should
				   not hang if no child processes have stopped
				   or terminated, but rather return zero as th
				   process id */
#define	WUNTRACED	2	/* option bit indicating that the caller should
				   receive status about untraced children that
				   stop due to signals. The default is that
				   the caller never sees status for such
				   processes, thus they appear to stay in the
				   running state even though they are in fact
				   stopped. */
#define	WIFSTOPPED(x)	((x).w_stopval == WSTOPPED)
#define	WIFSIGNALED(x)	((x).w_stopval != WSTOPPED && (x).w_termsig != 0)
#define	WIFEXITED(x)	((x).w_stopval != WSTOPPED && (x).w_termsig == 0)
