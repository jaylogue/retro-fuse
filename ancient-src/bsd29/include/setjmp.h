#ifdef	C_OVERLAY
typedef	int	jmp_buf[4];
#else
typedef	int	jmp_buf[3];
#endif
