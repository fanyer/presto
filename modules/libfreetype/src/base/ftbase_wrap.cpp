#include "core/pch.h"

#if defined(USE_FREETYPE) && defined(FT_INTERNAL_FREETYPE)

extern "C" {
#include "ftbase.c"

// it appears freetype doesn't include this file. it contains code
// governed by patents, but this is made clear in
// OPERA_FT_USE_SUBPIXEL_RENDERING.
#ifdef OPERA_FT_USE_SUBPIXEL_RENDERING
# include "ftlcdfil.c"
#endif // OPERA_FT_USE_SUBPIXEL_RENDERING
}

#endif // USE_FREETYPE && FT_INTERNAL_FREETYPE
