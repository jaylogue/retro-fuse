struct fblk
{
	int16_t    	df_nfree;
	daddr_t	df_free[MAX_NICINOD];
} __attribute__((packed));
