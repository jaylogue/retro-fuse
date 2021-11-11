struct _utmp {
	char	_ut_line[8];		/* tty name */
	char	_ut_name[8];		/* user id */
	long	_ut_time;		/* time on */
/*
 *	The rest of the fields will be taken out.
 *	For now, they are here as place holders.
 */
	short	_ut_jobno;		/* charging job number */
	short	_ut_uid;			/* current user id */
	short	_ut_gid;			/* current group id */
	char	_ut_chuid;		/* user has su'd */
	char	_ut_chgid;		/* user has newgrp'd */
};
