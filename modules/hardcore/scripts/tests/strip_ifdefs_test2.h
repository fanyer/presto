// These are tests for the pike script (strip_ifdefs.pike) that removes ifdefs from a header file.
// After stripping there should be no FAIL in the output.

// This define was set in the other file, we just make sure it's not still set here
#ifdef BARBAPAPA_WILL_RETURN
FAIL
#else
OK
#endif
#ifndef BARBAPAPA_WILL_RETURN
OK
#else
FAIL
#endif
