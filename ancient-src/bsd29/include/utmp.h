/*
 * Structure of utmp and wtmp files.
 *
 * Assuming the number 8 is unwise.
 */
struct utmp {
	char	ut_line[8];		/* tty name */
	char	ut_name[8];		/* user id */
#ifdef	UCB_NET
	char	ut_host[16];		/* host name, if remote */
#endif
	long	ut_time;		/* time on */
};
