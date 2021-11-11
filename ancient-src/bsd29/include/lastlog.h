struct lastlog {
	time_t	ll_time;
	char	ll_line[8];
#ifdef	UCB_NET
	char	ll_host[16];		/* same as in utmp */
#endif
};
