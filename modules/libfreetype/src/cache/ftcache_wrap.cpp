#include "core/pch.h"

#if defined(USE_FREETYPE) && defined(FT_INTERNAL_FREETYPE) && defined(FT_USE_CACHE_MODULE)

extern "C" {
#include "ftcache.c"
}

#endif // USE_FREETYPE && FT_INTERNAL_FREETYPE && FT_USE_CACHE_MODULE
