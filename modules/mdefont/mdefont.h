/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#ifndef MDF_H
#define MDF_H

#ifdef MDEFONT_MODULE

#include "modules/mdefont/mdf_cache.h"
#include "modules/mdefont/processedstring.h"

#ifdef SVG_SUPPORT
# include "modules/svg/svg_path.h"
#endif // SVG_SUPPORT

#ifdef MDF_OPENTYPE_SUPPORT
# include "modules/util/simset.h"
#endif // MDF_OPENTYPE_SUPPORT


// init stuff

/**
   creates an MDF_FontEngine instance. this engine can be passed as a
   handle to the MDF API. caller obtains ownsership, and should pass
   the engine to MDF_DestroyFontEngine when no longer needed - no
   MDF_FONT objects created using the engine must exist at this time.

   @param engine on success, *engine will point to the created engine

   @return OpStatus::OK on success, OpStatus::ERR on error, OpStatus::ERR_NO_MEMORY on OOM
 */
OP_STATUS MDF_CreateFontEngine(MDF_FontEngine** engine);

/**
   destroys an MDF_FontEngine instance. no MDE_FONT objects created
   using the engine must exist at this time.
 */
void MDF_DestroyFontEngine(MDF_FontEngine* engine);

/**
   creates an MDF_FontEngine instance. ownership is transferred to
   MdefontModule. the engine will be deleted when MDF_ShutdownFont is
   called. this method should only be called once, or not at all. if
   this method succeeds, MDF_ShutdownFont must be called before opera
   terminates.

   @return OpStatus::OK on success, OpStatus::ERR on error, OpStatus::ERR_NO_MEMORY on OOM
 */
OP_STATUS MDF_InitFont();

/**
   deletes engine created using MDF_InitFont. no MDE_FONT objects
   created using the engine created by MDF_InitFont must exist when
   this method is called.
 */
void MDF_ShutdownFont();

/**
   Must be called before calls to MDF_CountFonts, and MDF_GetFontInfo.
 */
OP_STATUS MDF_BeginFontEnumeration(MDF_FontEngine* engine = g_mdf_engine);

/**
   Must be called when all calls to MDF_CountFonts, and MDF_GetFontInfo,
   is ready.
 */
void MDF_EndFontEnumeration(MDF_FontEngine* engine = g_mdf_engine);

/**
   Count the number of fonts available in mdf.
 */
int MDF_CountFonts(MDF_FontEngine* engine = g_mdf_engine);


// types

enum MDF_SERIF {
  MDF_SERIF_SANS,
  MDF_SERIF_SERIF,
  MDF_SERIF_UNDEFINED
};

enum MDF_PROCESS_STRING_FLAG {
  MDF_PROCESS_FLAG_NONE = 0,
  MDF_PROCESS_FLAG_NO_ADVANCE = 1 << 0,
  MDF_PROCESS_FLAG_USE_GLYPH_INDICES = 1 << 1
};

struct MDF_FONTINFO {
  const uni_char* font_name;
  MDF_SERIF has_serif;
  BOOL has_normal;
  BOOL has_bold;
  BOOL has_italic;
  BOOL has_bold_italic;
  BOOL is_monospace;
  UINT32 ranges[4];

#ifdef MDF_CODEPAGES_FROM_OS2_TABLE
    UINT32 m_codepages[2];
#endif // MDF_CODEPAGES_FROM_OS2_TABLE
};

/**
   Send in a pointer to a MDF_FONTINFO struct, where
   info about the struct will be written. Notice that
   the char pointer in MDF_FONTINFO is a temporary pointer
   to the name inside mdf, and must be copied.
 */
OP_STATUS MDF_GetFontInfo(int font_nr, MDF_FONTINFO* font_info, MDF_FontEngine* engine = g_mdf_engine);

const uni_char* MDF_GetFontName(struct MDE_FONT* font);
const uni_char* MDF_GetFontFileName(MDE_FONT* font);

struct MDE_FONT {
	// Public data.
	int ascent;
	int descent;
	int height;
	int internal_leading;
	unsigned int max_advance;

	// sometimes needed when syntesizing style and weight (see eg hw
	// glyph cache)
	unsigned int extra_padding;

	// Private data. Store anything you want/need that is specific to the implementation.
	void* private_data;

#ifdef MDF_FONT_ADVANCE_CACHE
	class MDF_AdvanceCache* m_advance_cache;
#endif // MDF_FONT_ADVANCE_CACHE

	MDF_FontEngine* engine;
};

/**
   a 256 level alpha map of the glyph
 */
struct MDF_GLYPH_BUFFER {
    void* data;
    unsigned int w, h;
};
struct MDF_GLYPH {
    MDF_GLYPH_BUFFER* buffer_data;
    short bitmap_left;
    short bitmap_top;
    short advance;
};

// handle for glyphs, so checks for cache etc need not be made all
// over the place - does its own clean-up
class MDF_GlyphHandle
{
public:
	MDF_GlyphHandle(MDE_FONT* font);
	~MDF_GlyphHandle();
	OP_STATUS Advance(short& advance, UnicodePoint up, BOOL isIndex = TRUE);
	OP_STATUS Get(UnicodePoint up, BOOL needBitmap = TRUE, BOOL isIndex = TRUE);
	MDF_GLYPH& Glyph() { OP_ASSERT(m_has_glyph); return m_glyph; }
private:
	MDE_FONT* m_font;
	BOOL m_has_glyph;
#ifndef MDF_FONT_GLYPH_CACHE
	BOOL m_free_glyph;
#endif // !MDF_FONT_GLYPH_CACHE
	MDF_GLYPH m_glyph;
};

struct MDF_WebFontBase
{
	MDF_FontEngine* engine;
};

OP_STATUS MDF_GetLocalFont(MDF_WebFontBase*& localfont, const uni_char* facename, MDF_FontEngine* engine = g_mdf_engine);
OP_STATUS MDF_GetWebFontInfo(MDF_WebFontBase* webfont, MDF_FONTINFO* font_info);
OpFontInfo::FontType MDF_GetWebFontType(MDF_WebFontBase* webfont);

/**
   implementation of hash table suitable for storing font
   names. useful eg for local webfonts, since name matching will
   otherwise take a lot of time.
*/
class AutoFaceNameHashTable : public OpStringHashTable<uni_char>
{
public:
	virtual ~AutoFaceNameHashTable() { DeleteAll(); }
protected:
	virtual void Delete(void* data);
};
/**
   utility function to add all english "full font name" and
   "postscript name" entries from an sfnt name table into a hash
   table. on failure, any names successfully added before error will
   remain in hash.

   @param name_table pointer to the raw name table data for the font
   @param name_table_size the size of the name table, in bytes
   @param hash target table for found names
   @return OpStatus::OK if all went well, OpStatus::ERR on data error, OpStatus::ERR_NO_MEMORY on OOM
*/
OP_STATUS MDF_GetFaceNames(const UINT8* name_table, size_t name_table_size,
                           AutoFaceNameHashTable& hash);

/**
   processes string, converting surrogate pairs, handles compex text
   and applies hinting. processed_string.m_processed_glyphs is only
   valid until next call to MDF_ProcessString.
   @param processed_string (out) the processed string
   @param str the string to process
   @param len the length of the string
   @param extra_space any extra space per glyph to be added
   @param word_width desired word width - position of individual glyphs will be adjusted
   @param no_advance do not accumulate advance. this is useful for
   platforms where obtaining advance can be done cheaper outside of
   mdefont (eg using VEGAOpPainter without advance and glyph
   caches). will ignore word_width, since there's no point in
   adjusting the position of individual glyphs before advance has been
   applied. position of individual glyphs as well as m_advance will
   have to be updated by caller in order to make sense. note that
   extra_space is still accumulated.
   @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR on error,
   OpStatus::OK on success. contents of processed_string is only to be
   trusted on success.
 */
OP_STATUS MDF_ProcessString(MDE_FONT* font,
							ProcessedString& processed_string,
							const uni_char* str, const unsigned int len,
							const int extra_space = 0,
							const short word_width = -1,
							const int flags = MDF_PROCESS_FLAG_NONE);

/**
   applies top and left to glyphs in string, and computes bounding
   box. bounding box y is relative to ascent, not baseline.
 */
OP_STATUS MDF_LayoutString(MDE_FONT* font,
						   ProcessedString& processed_string,
						   OpRect& box);

#ifdef MDF_KERNING
/**
   applies kerning to processed string, which must contain glyph
   indices, not unicode codepoints.
 */
OP_STATUS MDF_ApplyKerning(MDE_FONT* font,
						   ProcessedString& processed_string);
#endif // MDF_KERNING

/**
   copies contents of buffer to glyph.
   if mustRender is TRUE, bitmap data is copied - caller obtains ownership and should delete after use.
 */
OP_STATUS MDF_GetGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender = TRUE, BOOL isIndex = FALSE);
void MDF_FreeGlyph(MDE_FONT* font, MDF_GLYPH& glyph);
/**
   copies contents of buffer to glyph - glyph.buffer->data is used as
   destination buffer, glyph.buffer->w/h are the buffer boundaries. if
   glyph is bigger than w/h, it will be clipped accordingly, without
   signalling error.
 */
OP_STATUS MDF_FillGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, const UINT32 stride, BOOL isIndex = FALSE);
#ifdef MDF_FONT_GLYPH_CACHE
/**
   borrows contents of buffer to glyph - data may no longer be valid after further calls to MDF
 */
OP_STATUS MDF_BorrowGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender = TRUE, BOOL isIndex = FALSE);
#endif // MDF_FONT_GLYPH_CACHE

#ifdef SVG_SUPPORT
OP_STATUS MDF_GetOutline(MDE_FONT* font, const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph);
#endif // SVG_SUPPORT

// == FONT FUNCTIONS ==========================================================

#if defined(MDF_SFNT_TABLES) && defined(_GLYPHTESTING_SUPPORT_)
OP_STATUS MDF_UpdateGlyphMask(OpFontInfo* fontinfo, MDF_FontEngine* engine = g_mdf_engine);
#endif // MDF_SFNT_TABLES && _GLYPHTESTING_SUPPORT_

/**
   Adds a font file to mdf. The function returns OpStatus::OK, if everything
   was ok, OpStatus::ERR, if there was something wrong with the file, or
   OpStatus::ERR_NO_MEMORY if memory allocation failed.
   Everything allocated in MDF_AddFont will be released when MDF_Shutdown is called.

   @param filename the name of the font file, utf-8 encoded
 */
OP_STATUS MDF_AddFontFile(const uni_char* filename, MDF_FontEngine* engine = g_mdf_engine);
OP_STATUS MDF_AddFontFile(const uni_char* filename, MDF_WebFontBase*& webfont, MDF_FontEngine* engine = g_mdf_engine);

/**
	Removes a font from mdf. This function returns OpStatus::OK, if everything was ok,
	OpStatus::ERR_NO_MEMORY if memory allocation failed (unlikely),
	OpStatus::ERR, if there was no such font or there was an other error.
*/
OP_STATUS MDF_RemoveFont(MDF_WebFontBase* webfont);

/**
   Tells us if a specific glyph is part of a font.
 */
BOOL MDF_HasGlyph(MDE_FONT* font, uni_char c);
/**
   Tells us if font is scalable
 */
BOOL MDF_IsScalable(MDE_FONT* font);

#ifdef OPFONT_GLYPH_PROPS
OP_STATUS MDF_GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props);
#endif // OPFONT_GLYPH_PROPS

/**
   Returns the width of a string using a specific font.
 */
OP_STATUS MDF_StringWidth(int& width, MDE_FONT* font, const uni_char* str, int len, int extra_space);

/**
   Gets a reference to a font the the specified name.
   Will return NULL if the font does not exist, or if
   we get an OOM.
 */
MDE_FONT* MDF_GetFont(int font_nr, int size, BOOL bold, BOOL italic, MDF_FontEngine* engine = g_mdf_engine);

/**
	Gets a reference to a webfont with the given size.
	Will return NULL if there was an error.
 */
MDE_FONT* MDF_GetWebFont(MDF_WebFontBase* webfont, int size);
MDE_FONT* MDF_GetWebFont(MDF_WebFontBase* webfont, UINT8 weight, BOOL italic, int size);

/**
 * Release a reference to a font.
 */
void MDF_ReleaseFont(MDE_FONT* font);

/**
   Returns a reference to a structure of some kind that can be used to get character outlines.
 */
void* MDF_GetFontImplementation(MDE_FONT* font);

UINT32 MDF_GetCharIndex(MDE_FONT* font, const UnicodePoint ch);
UINT32 MDF_GetCharIndex(MDE_FONT* font, const uni_char* str, const UINT32 len, UINT32& pos);


#ifdef _GLYPHTESTING_SUPPORT_
#define B2LENDIAN(x) (((x & 0xff) << 8) | ((x & 0xff00) >> 8))
#define B2LENDIAN32(x) (((x & 0xff) << 24) | ((x & 0xff00) << 8) | ((x & 0xff0000) >> 8) | ((x & 0xff000000) >> 24))
#endif //_GLYPHTESTING_SUPPORT_

#ifdef MDF_SFNT_TABLES
// deprecated - tables have a size, and we can no longer assume
// they're correct
BYTE DEPRECATED(*MDF_GetTable(MDE_FONT* font, unsigned long tab_tag));
/**
   Gets a reference to the specified font table in the supplied
   font. The reference must be free:d by calling MDF_FreeTable().
   @param font The font to extract the table from.
   @param tab_tag A four byte character tag of the table.
   @param tab (out) handle to table - on success tab will point
    to allocated memory containing the table (to be free:d with
    MDF_FreeTable)
   @param size (out) the size (bytes) of the table
*/
OP_STATUS MDF_GetTable(MDE_FONT* font, unsigned long tab_tag, BYTE*& tab, UINT32& size);
/**
   Frees a font table
   @param tab is a reference to the table to be free:d.
*/
void MDF_FreeTable(MDE_FONT* font, BYTE* tab);
void MDF_FreeTable(BYTE* tab, MDF_FontEngine* engine = g_mdf_engine);
#endif // MDF_SFNT_TABLES



////////// Drawing strings ///////////

#ifdef MDF_DRAW_TO_MDE_BUFFER
OP_STATUS MDF_DrawString(MDE_FONT* font, const uni_char* str, const unsigned int len, const int x, const int y, const int extra, struct MDE_BUFFER* dst, short word_width = -1);
#endif // MDF_DRAW_TO_MDE_BUFFER
#ifdef MDF_DRAW_TO_BITMAP
/**
   returns (OP_STATUS)1 for no width/height bitmap
 */
OP_STATUS MDF_DrawString(OpBitmap*& bitmap, short& left, short& top, MDE_FONT* font, const uni_char* str, const unsigned int len, const int extra, const unsigned int color, short word_width = -1);
#endif // MDF_DRAW_TO_BITMAP



/**
   growable buffer used to hold a processed string
 */
class MDF_ProcessedGlyphBuffer
{
public:
	MDF_ProcessedGlyphBuffer() : m_size(0), m_buffer(0) {}
	~MDF_ProcessedGlyphBuffer();
	/**
	   grows the buffer
	   @param size the number of ProcessedGlyph elements to grow to - noop if Capacity >= size
	   @param copy_contents if TRUE, copy contents of old buffer if buffer grew
	   @return
	   OpStatus::OK if buffer growth succeeded or was not necessary - Capacity will be at least size from now on
	   OpStatus::ERR_NO_MEMORY on OOM
	 */
	OP_STATUS Grow(const size_t size, BOOL copy_contents = FALSE);
	/**
	   @return the number of ProcessedGlyphs the buffer can hold
	 */
	size_t Capacity() const { return m_size; }
	/**
	   @return a pointer to the storage
	 */
	struct ProcessedGlyph* Storage() { return m_buffer; }

private:
	size_t m_size;
	struct ProcessedGlyph* m_buffer;
};



////////// Font interface to be implemented by fontengines //////////

class MDF_FontEngine
{
public:
#ifdef DEBUG_ENABLE_OPASSERT
	MDF_FontEngine() : m_created_font_count(0) {}
	int m_created_font_count;
#endif // DEBUG_ENABLE_OPASSERT

	virtual ~MDF_FontEngine()
	{
#ifdef DEBUG_ENABLE_OPASSERT
		OP_ASSERT(!m_created_font_count);
#endif // DEBUG_ENABLE_OPASSERT
	}

	static OP_STATUS Create(MDF_FontEngine** engine);

	/**
	   Do initialization. This function is called during module initialization
	*/
	virtual OP_STATUS Init() = 0;

	/**
	   Adds a font file to mdf. The function returns OpStatus::OK, if everything
	   was ok, OpStatus::ERR, if there was something wrong with the file, or
	   OpStatus::ERR_NO_MEMORY if memory allocation failed.
	   Everything allocated in MDF_AddFont will be released when MDF_Shutdown is called.
	 */
	virtual OP_STATUS AddFontFile(const uni_char* filename) = 0;

	/**
		Adds the webfont to mdf. The function returns OpStatus::OK, if everything
	    was ok, OpStatus::ERR, if there was something wrong with the file, or
	    OpStatus::ERR_NO_MEMORY if memory allocation failed.
	    Everything allocated in MDF_AddFont will be released when MDF_Shutdown is called.
	 */
	virtual OP_STATUS AddWebFont(const uni_char* filename, MDF_WebFontBase*& webfont) = 0;

	virtual OP_STATUS GetWebFontInfo(MDF_WebFontBase* webfont, MDF_FONTINFO* font_info) = 0;
	
	virtual OpFontInfo::FontType GetWebFontType(MDF_WebFontBase* webfont) = 0;

	/**
		Removes the font with the given font number from the font engine.
	 */
	virtual OP_STATUS RemoveFont(MDF_WebFontBase* webfont) = 0;

	/**
	   Must be called before calls to MDF_CountFonts, and MDF_GetFontInfo.
	 */
	virtual OP_STATUS BeginFontEnumeration() = 0;

	/**
	   Must be called when all calls to MDF_CountFonts, and MDF_GetFontInfo,
	   is ready.
	 */
	virtual void EndFontEnumeration() = 0;

	/**
	   Count the number of fonts available in mdf.
	 */
	virtual int CountFonts() = 0;

#ifdef MDF_FONT_SMOOTHING
	/**
	   Return whether this font engine uses component rendering (font smoothing)
	*/
	virtual BOOL HasComponentRendering() = 0;
#endif // MDF_FONT_SMOOTHING

	/**
	   Send in a pointer to a MDF_FONTINFO struct, where
	   info about the struct will be written. Notice that
	   the char pointer in MDF_FONTINFO is a temporary pointer
	   to the name inside mdf, and must be copied.
	 */
	virtual OP_STATUS GetFontInfo(int font_nr, MDF_FONTINFO* font_info) = 0;

	virtual const uni_char* GetFontName(MDE_FONT* font) = 0;

	virtual const uni_char* GetFontFileName(MDE_FONT* font) = 0;

	/**
	   Tells us if a specific glyph is part of a font.
	 */
	virtual BOOL HasGlyph(MDE_FONT* font, uni_char c) = 0;
	/**
	   Tells us if font is scalable
	 */
	virtual BOOL IsScalable(MDE_FONT* font) = 0;

	/**
	   Tells us if font is bold
	 */
	virtual BOOL IsBold(MDE_FONT* font) = 0;

	/**
	   Tells us if font is italic
	 */
	virtual BOOL IsItalic(MDE_FONT* font) = 0;

#ifdef MDF_KERNING
	virtual OP_STATUS ApplyKerning(MDE_FONT* font, ProcessedString& processed_string) = 0;
#endif // MDF_KERNING

#ifdef OPFONT_GLYPH_PROPS
        virtual OP_STATUS GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props) = 0;
#endif // OPFONT_GLYPH_PROPS


	// default implementation provided in mdefont.cpp - feel free to override if necessary
	/**
	   processes str and stores position of each (output) glyph in
	   processed_string. takes care of surrogate pairs, complex text,
	   hinting etc. for string drawing, position has to be adjusted
	   with top and left of glyphs (since no rasterization is
	   done). output is glyph id:s or unicode points depending on
	   string, passed parameters and configuration. (see
	   ProcessedString::m_is_glyph_indices.)

	   NOTE: output data is in a shared buffer, so a new call to
	   ProcessString will overrite data from last call.

	   @param font the font to use
	   @param processed_string (out) handle to processed string
	   @param str input string
	   @param len length of str
	   @param extra_space the ammount of extra (horizontal) space to add to each (output) glyph
	   @param word_width requested output word width, used by truezoom
	   @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM
	 */
	virtual OP_STATUS ProcessString(MDE_FONT* font,
									ProcessedString& processed_string,
									const uni_char* str, const unsigned int len,
									const int extra_space = 0,
									const short word_width = -1,
									const int flags = MDF_PROCESS_FLAG_NONE);

#ifdef MDF_UNPROCESSED_STRING_API
	virtual OP_STATUS StringWidth(int& width, MDE_FONT* font, const uni_char* str, int len, int extra_space = 0) = 0;
#endif // MDF_UNPROCESSED_STRING_API

	/**
	   applies top and left to each glyph and computes bounding
	   box. bounding box y is relative to ascent, not baseline.
	 */
	virtual OP_STATUS LayoutString(MDE_FONT* font,
								   ProcessedString& processed_string,
								   OpRect& box);

	/**
	   Gets a reference to a font the the specified name.
	   Will return NULL if the font does not exist, or if
	   we get an OOM.
	 */
	virtual MDE_FONT* GetFont(int font_nr, int size, BOOL bold, BOOL italic) = 0;
	virtual OP_STATUS GetLocalFont(MDF_WebFontBase*& localfont, const uni_char* facename) = 0;
	virtual MDE_FONT* GetWebFont(MDF_WebFontBase* webfont, int size) = 0;
	virtual MDE_FONT* GetWebFont(MDF_WebFontBase* webfont, UINT8 weight, BOOL italic, int size) = 0;

	/**
	 * Release a reference to a font.
	 */
	virtual void ReleaseFont(MDE_FONT* font) = 0;

	/**
	   translates the glyphs in processed_string from unicode
	   codepoints to glyph indices - assumes processed_string to
	   contain unicode codepoints.
	 */
	virtual OP_STATUS UCPToGID(MDE_FONT* font, ProcessedString& processed_string) = 0;

	/**
	 * translates a unicode point into the corrisponding character index for the font.
	 */
	virtual UINT32 GetCharIndex(MDE_FONT* font, const UnicodePoint ch) = 0;
	virtual UINT32 GetCharIndex(MDE_FONT* font, const uni_char* str, const UINT32 len, UINT32& pos) = 0;

	/**
	   Returns a reference to a structure of some kind that can be used to get character outlines.
	 */
	virtual void* GetFontImplementation(MDE_FONT* font) = 0;

	virtual OP_STATUS GetGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender, BOOL isIndex) = 0;
	virtual void FreeGlyph(MDF_GLYPH& glyph) = 0;
	virtual OP_STATUS FillGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, const UINT32 stride, BOOL isIndex) = 0;
#ifdef MDF_FONT_GLYPH_CACHE
	virtual OP_STATUS BorrowGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UnicodePoint ch, BOOL mustRender, BOOL isIndex) = 0;
#endif // MDF_FONT_GLYPH_CACHE

#ifdef SVG_SUPPORT
	virtual OP_STATUS GetOutline(MDE_FONT* font, const UINT32 g_id, const UINT32 prev_g_id, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath* out_glyph) = 0;
#endif // SVG_SUPPORT

#ifdef MDF_DRAW_TO_MDE_BUFFER
#ifdef MDF_UNPROCESSED_STRING_API
	virtual OP_STATUS DrawString(MDE_FONT* font, const uni_char* str, const unsigned int len, const int x, const int y, const int extra, MDE_BUFFER* dst, short word_width = -1) = 0;
#endif // MDF_UNPROCESSED_STRING_API
	virtual OP_STATUS DrawString(MDE_FONT* font, const ProcessedString& processed_string, const int x, const int y, MDE_BUFFER* dst);
#endif // MDF_DRAW_TO_MDE_BUFFER

#ifdef MDF_DRAW_TO_BITMAP
	virtual OP_STATUS DrawString(OpBitmap*& bitmap, short& top, short& left, MDE_FONT* font, ProcessedString& processed_string, const unsigned int color);
#endif // MDF_DRAW_TO_BITMAP


#ifdef MDF_SFNT_TABLES
	/**
	   Gets a reference to the specified font table in the supplied
	   font. The reference must be free:d by calling MDF_FreeTable().
	   @param font The font to extract the table from.
	   @param tab_tag A four byte character tag of the table.
	   @param tab (out) if succsessful, point to start of table
	   @param size (out) size of table, in bytes
	   @return OpStatus::ERR_NO_MEMORY on OOM, OpStatus::ERR on error, OpStatus::OK otherwise
	*/
 	virtual OP_STATUS GetTable(MDE_FONT* font, unsigned long tab_tag, BYTE*& tab, UINT32& size) = 0;

	/**
	   Frees a font table
	   @param tab is a reference to the table to be free:d.
	*/
	virtual void FreeTable(BYTE* tab) = 0;
#endif // MDF_SFNT_TABLES

#ifdef MDF_FONT_FACE_CACHE
	/** Load a font face. This functions is only needed if the face cache is used.
	  * If a font face cannot be found in the cache it is loaded using this function instead.
	  * If *face is not NULL then the existing fontface must be reused.
	  * @param face variable which will contain the face if the function returns OpStatus::OK.
	  * @param font_name name of the font to load.
	  * @param index index fo the face in the font file to load. This is required to support collection fonts. */
	virtual OP_STATUS LoadFontFace(class MDF_FontFace** face, const uni_char* font_name, int index) = 0;
#endif // MDF_FONT_FACE_CACHE

#ifdef MDF_FONT_GLYPH_CACHE
	/** Load a glyph from the font. This function is only needed if the glyph cach is used.
	  * If a glyph is not found in the cache this function is called. If a glyph is rendered when
	  * mustRender is set to false, the flag MDF_GLYPH_RENDERED must be set.
	  * @param glyph the glyph to load. The 'c' member is set to the glyph which should be loaded.
	  * All other fields (except next) should be updated by this function.
	  * @param font the font to use for loading the glyph.
	  * @param mustRender if true the glyph must be rendered, if false only advance needs to be set.
	  * @returns OpStatus::OK on success, an error otherwise. */
	virtual OP_STATUS LoadFontGlyph(struct MDF_FontGlyph* glyph, MDE_FONT* font, BOOL mustRender) = 0;
	/** Free a previously loaded glyph.
	 * @param glyph the glyph to free. */
	virtual void FreeFontGlyph(struct MDF_FontGlyph* glyph) = 0;
	/** Change the size of the glyph cache (if font engine supports it). */
	virtual OP_STATUS ResizeGlyphCache(MDE_FONT* font, int new_size)
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	virtual unsigned short GetGlyphCacheSize(MDE_FONT* font) const = 0;
	virtual BOOL GlyphCacheFull(MDE_FONT* font) const = 0;
	virtual unsigned short GetGlyphCacheFill(const MDE_FONT* font) const = 0;
#endif // MDF_FONT_GLYPH_CACHE

	// used as storage area from ProcessString
	MDF_ProcessedGlyphBuffer glyph_buffer;
#ifdef MDF_OPENTYPE_SUPPORT
	Head opentype_cache;
#endif // MDF_OPENTYPE_SUPPORT

#ifdef _DEBUG
private:
	virtual const uni_char* DebugGetFaceName(MDE_FONT* font) { return 0; }
	virtual const uni_char* DebugGetFileName(MDE_FONT* font) { return 0; }
#endif // _DEBUG
};

inline OP_STATUS MDF_ProcessString(MDE_FONT* font,
								   ProcessedString& processed_string,
								   const uni_char* str, const unsigned int len,
								   const int extra_space/* = 0*/,
								   const short word_width/* = -1*/,
								   const int flags/* = MDF_PROCESS_FLAG_NONE*/)
{
	return font->engine->ProcessString(font, processed_string, str, len, extra_space, word_width, flags);
}
inline OP_STATUS MDF_LayoutString(MDE_FONT* font,
								  ProcessedString& processed_string,
								  OpRect& box)
{
	return font->engine->LayoutString(font, processed_string, box);
}
#ifdef MDF_FONT_GLYPH_CACHE
/** Enlarge the font's glyph cache.
 *
 *  The hash size will remain the same. Contents will be kept from the old cache.
 *  @param new_size The desired new size of the cache. Must be larger than the cache's current size
 */
inline OP_STATUS MDF_ResizeGlyphCache(MDE_FONT* font, int new_size)
{
	return font->engine->ResizeGlyphCache(font, new_size);
}
/** Return the current size of the font's glyph cache.
 */
inline unsigned short MDF_GetGlyphCacheSize(MDE_FONT* font)
{
	return font->engine->GetGlyphCacheSize(font);
}
/** Check if the font's glyph cache has been filled to its capacity.
 */
inline BOOL MDF_GlyphCacheFull(MDE_FONT* font)
{
	return font->engine->GlyphCacheFull(font);
}
inline unsigned short MDF_GetGlyphCacheFill(const MDE_FONT* font)
{
	return g_mdf_engine->GetGlyphCacheFill(font);
}
#endif // MDF_FONT_GLYPH_CACHE
#ifdef MDF_KERNING
inline OP_STATUS MDF_ApplyKerning(MDE_FONT* font, ProcessedString& processed_string)
{
	if (!processed_string.m_is_glyph_indices)
		return OpStatus::ERR;
	return font->engine->ApplyKerning(font, processed_string);
}
#endif // MDF_KERNING

#endif // MDEFONT_MODULE

#endif // MDF_H
