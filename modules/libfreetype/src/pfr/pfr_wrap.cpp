#include "core/pch.h"
#include "modules/libfreetype/include/freetype/config/opera_ftmodules.h"

#if defined(USE_FREETYPE) && defined(FT_INTERNAL_FREETYPE) && defined(FT_USE_PFR_FONTS)

extern "C" {
#include "pfr.c"
}

#endif // USE_FREETYPE && FT_INTERNAL_FREETYPE && FT_USE_PFR_FONTS
