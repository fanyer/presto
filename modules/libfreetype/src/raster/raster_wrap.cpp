#include "core/pch.h"
#include "modules/libfreetype/include/freetype/config/opera_ftmodules.h"

#if defined(USE_FREETYPE) && defined(FT_INTERNAL_FREETYPE) && defined(FT_USE_RASTER1)

extern "C" {
#if defined(EPOC)
namespace raster_namespace
{
#endif // defined(EPOC)
#include "raster.c"
#if defined(EPOC)
}
#endif // defined(EPOC)
}

#endif // USE_FREETYPE && FT_INTERNAL_FREETYPE && FT_USE_RASTER1
