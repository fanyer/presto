#ifndef PCF_FONT_H
#define PCF_FONT_H

#ifdef MDEFONT_MODULE

#ifdef MDF_FREETYPE_SUPPORT

#ifdef FT_INTERNAL_FREETYPE
# include "modules/libfreetype/include/ft2build.h"
# include "modules/libfreetype/include/freetype/tttables.h"
#else // FT_INTERNAL_FREETYPE
# include <ft2build.h>
# include <freetype/tttables.h>
#endif // FT_INTERNAL_FREETYPE

#include FT_FREETYPE_H

/**
 * Get the unicode blocks from the PCF font. Freetype does not support
 * this.
 *
 * @param ranges is an array of length 4.
 * @param face is the font face object.
 *
 * @return true if operation was successful, false otherwise.
 */
bool GetPcfUnicodeRanges(const FT_Face& face, unsigned int* ranges);

/**
 * Extract the metrics info from font file and put it in face. Freetype 
 * does not support this.
 *
 * @param face is the font face object.
 */
void GetPcfMetrics(FT_Face& face);

/**
 * Returns true if the face object refers to a PCF font.
 */
bool IsPcfFont(const FT_Face& face);


#endif // MDF_FREETYPE_SUPPORT

#endif // MDEFONT_MODULE

#endif // PCF_FONT_H
