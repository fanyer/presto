/* This is only used when compiling within Opera */

#ifdef _MSC_VER /* Visual Studio */
# include "glibconfig.win32.h"
#else /* GCC */
// http://predef.sourceforge.net/prearch.html
# if defined(__LP64__) || defined(_LP64) || defined(__x86_64__) || defined(__ia64__) || defined(__amd64__)
#  include "glibconfig.gnu64.h"
# else
#  include "glibconfig.gnu32.h"
# endif
#endif
