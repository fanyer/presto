#include "core/pch.h"

#if defined(USE_FREETYPE) && defined(FT_INTERNAL_FREETYPE) && defined(FT_USE_GZIP_MODULE)

extern "C" {
#include "ftgzip.c"
// modules/libfreetype/src/gzip/zutil.h #define:s 'local' as 'static',
// so undefine the symbol again to avoid compile errors with
// jumbo-compile (e.g. in modules/libfreetype/src/psnames/psmodule.c,
// which uses a local variable called 'local'):
#undef local
}

#endif // USE_FREETYPE && FT_INTERNAL_FREETYPE && FT_USE_GZIP_MODULE
