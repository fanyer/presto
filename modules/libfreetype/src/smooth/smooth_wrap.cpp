#include "core/pch.h"

#if defined(USE_FREETYPE) && defined(FT_INTERNAL_FREETYPE)

// causes problems with ADS compiler - patch CORE-29151
#undef ROUND

extern "C" {
#include "smooth.c"
}

#endif // USE_FREETYPE && FT_INTERNAL_FREETYPE
