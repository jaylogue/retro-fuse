#ifdef	UCB_QUOTAS
#define	isquot(ip)	(((ip)->i_mode & IFMT) == IFQUOT)
#endif
