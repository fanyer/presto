/** @mainpage The mdefont Module

@section Overview
'mdefont' (Multiplatform Desktop Environment Font Engine) is a breakout of the font parts of libgogi. it handles font loading, glyph rasterization, (optional) font and glyph caching, string measurement and drawing. it provides an abstract interface for font rasterization engines. currently implemented are wrappers for freetype and itype. caution - those are both third party libraries. it also provides (currently limited) support for complex indic scripts, via opentype.

@section api API & internal functions

the API is a set of function with the prefix MDF. they almost always take an MDE_FONT handle, which represents the font to be used. data in objects might be public for easy *read* access, but should not be written to from outside.

@section memory Memory, globals & heap usage

the ammount of memory required for mdefont depends on the implemenation of the wrapper for the rasterization engine that do parts of its work. in all current implementations there's some data stored about loaded fonts. apart from implementation-specific memory, there are (optional) caches for rasterized glyphs and fast access to fonts. if support for complex indic scripts is enabled, there'll also be a cache storing handles to deal with gsub tables, as well as the tables themselves.

@subsection glyphcache Glyph cache
the size of the (optional) glyph cache is currently hard coded (yes, this should change) to 100 glyphs. the size of each entry is dependent on the dimensions of the glyph, and therefore font size. there will be one glyph cache for each MDE_FONT handle fetched by calling MDF_GetFont, and the cash will live until the handle is released via MDF_ReleaseFont.

@subsection facecache Face cache
the size of the (optional) face cache is controlled with TWEAK_MDEFONT_FONT_FACE_CACHE_SIZE. the size of the cached data is dependent on the implementation used.

@subsection opentypecache Opentype cache
if support for complex indic scripts is enabled, a cache containing handles to objects that deal with the opentype gsub table, as well as the table itself, will be stored for a number of recently used fonts that required such processing. the number of opentype handles stored is controlled with TWEAK_MDEFONT_OPENTYPE_CACHE_SIZE. there's also some objects that deal with categorizing indic glyphs, and two buffers for storing processed text that grow to encompass the strings passed.

@subsection advancecache Advance cache
in order to speed up layout, there's an (optional) advance cache (that's currently disabled by default). this cache stores advances for a number of glyphs, and works in the same manner as the glyph cache. the number of advances stored is controlled with TWEAK_MDEFONT_FONT_ADVANCE_CACHE_SIZE.

*/
