int GrabBacktrace( unsigned int *locs, int nlocs )
{
#if defined WIN32
	/* For each allocation, grab the stack trace at the allocation point. */
	/* Note, this is unsafe, it doesn't know when to stop!   How do we fix that?  */
	unsigned *fp;

	_asm mov fp, ebp;

	for ( int i=0 ; i < nlocs ; i++ )
	{
		locs[i] = fp[1];
		fp = (unsigned*)fp[0];
	}
#else
	return 0;
#endif
}
