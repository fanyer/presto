#include "core/pch.h"

#if defined(USE_FREETYPE) && defined(FT_INTERNAL_FREETYPE) && defined(FT_USE_LZW_MODULE)

extern "C" {
#include "ftlzw.c"
}

#endif // USE_FREEYYPE && FT_INTERNAL_FREETYPE && FT_USE_LZW_MODULE
