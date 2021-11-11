/*
 * Structure returned by ftime system call
 */
struct	timeb {
	time_t	time;
	u_short	millitm;
	short	timezone;
	short	dstflag;
};
