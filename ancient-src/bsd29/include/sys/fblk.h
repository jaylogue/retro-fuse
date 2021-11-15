struct fblk
{
	int16_t	df_nfree;
	daddr_t	df_free[NICFREE];
} __attribute__((packed));
