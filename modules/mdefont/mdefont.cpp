/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */

#include "core/pch.h"

#ifdef MDEFONT_MODULE

#include "modules/mdefont/mdefont.h"
#ifdef MDF_OPENTYPE_SUPPORT
# include "modules/mdefont/mdf_opentype.h"
#endif // MDF_OPENTYPE_SUPPORT

#include "modules/unicode/unicode_stringiterator.h"

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

#include "modules/probetools/probepoints.h"

#ifdef PI_WEBFONT_OPAQUE
# include "modules/display/sfnt_base.h"
#endif // PI_WEBFONT_OPAQUE

MDF_GlyphHandle::MDF_GlyphHandle(MDE_FONT* font)
	: m_font(font)
	, m_has_glyph(FALSE)
#ifndef MDF_FONT_GLYPH_CACHE
	, m_free_glyph(FALSE)
#endif // !MDF_FONT_GLYPH_CACHE
{
	m_glyph.buffer_data = 0;
}
MDF_GlyphHandle::~MDF_GlyphHandle()
{
#ifndef MDF_FONT_GLYPH_CACHE
	if (m_free_glyph)
		MDF_FreeGlyph(m_font, m_glyph);
#endif // !MDF_FONT_GLYPH_CACHE
}
OP_STATUS MDF_GlyphHandle::Advance(short& advance, UnicodePoint up, BOOL isIndex/* = TRUE*/)
{
	RETURN_IF_ERROR(Get(up, FALSE, isIndex));
	OP_ASSERT(m_has_glyph);
	advance = m_glyph.advance;
	return OpStatus::OK;
}
OP_STATUS MDF_GlyphHandle::Get(UnicodePoint up, BOOL needBitmap, BOOL isIndex)
{
	OP_ASSERT(!m_has_glyph);
#ifdef MDF_FONT_GLYPH_CACHE
	RETURN_IF_ERROR(MDF_BorrowGlyph(m_glyph, m_font, up, needBitmap, isIndex));
#else // MDF_FONT_GLYPH_CACHE
	RETURN_IF_ERROR(MDF_GetGlyph(m_glyph, m_font, up, needBitmap, isIndex));
	m_free_glyph = TRUE;
#endif // MDF_FONT_GLYPH_CACHE
	m_has_glyph = TRUE;
	OP_ASSERT(!needBitmap || m_glyph.buffer_data);
	return OpStatus::OK;
}

OP_STATUS MDF_CreateFontEngine(MDF_FontEngine** engine)
{
	OP_ASSERT(engine);

	MDF_FontEngine* e;
	RETURN_IF_ERROR(MDF_FontEngine::Create(&e));
	// probably a good idea to keep a couple of glyphs here from
	// start, to avoid unnecessary initial growth
	OP_STATUS status = e->glyph_buffer.Grow(20);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(e);
		return status;
	}

	*engine = e;
	return OpStatus::OK;
}

void MDF_DestroyFontEngine(MDF_FontEngine* engine)
{
	OP_ASSERT(engine);

#ifdef MDF_OPENTYPE_SUPPORT
	// has to be done before deleting engine, since entries in
	// opentype_cache are deleted using the engine
	// implementation. (doing it from MDF_FontEngine destructor will
	// cause termination due to calling a pure virtual function.)
	engine->opentype_cache.Clear();
#endif // MDF_OPENTYPE_SUPPORT

	OP_DELETE(engine);
}

OP_STATUS MDF_InitFont()
{
	return MDF_CreateFontEngine(&g_opera->mdefont_module.mdf_engine);
}

void MDF_ShutdownFont()
{
	OP_ASSERT(g_opera->mdefont_module.mdf_engine);

	MDF_DestroyFontEngine(g_opera->mdefont_module.mdf_engine);
	g_opera->mdefont_module.mdf_engine = NULL;
}

// utility function to fetch advance of glyph with unicode codepoint
// ucp, so check for advance cache doesn't have to be done all over
// the place
static inline OP_STATUS GlyphAdvance(MDE_FONT* font, const UnicodePoint ucp, short& advance)
{
#ifdef MDF_FONT_ADVANCE_CACHE
	OP_ASSERT(font->m_advance_cache);
	return font->m_advance_cache->Advance(ucp, advance);
#else // MDF_FONT_ADVANCE_CACHE
	MDF_GlyphHandle gh(font);
	return gh.Advance(advance, ucp, FALSE);
#endif // MDF_FONT_ADVANCE_CACHE
}



/**
   applies top and left to all glyphs and computes bounding box
 */
OP_STATUS MDF_FontEngine::LayoutString(MDE_FONT* font, ProcessedString& processed_string, OpRect& box)
{
	OP_ASSERT(!processed_string.m_top_left_positioned);
	ProcessedGlyph* glyphs = processed_string.m_processed_glyphs;
	RECT bb = { 0, 0, 0, 0 };
	for (size_t i = 0; i < processed_string.m_length; ++i)
	{
		ProcessedGlyph& pg = glyphs[i];
		MDF_GlyphHandle gh(font);
		RETURN_IF_ERROR(gh.Get(pg.m_id, TRUE, processed_string.m_is_glyph_indices));
		MDF_GLYPH& g = gh.Glyph();
		OP_ASSERT(g.buffer_data);

		pg.m_pos.x += g.bitmap_left;
		pg.m_pos.y -= g.bitmap_top;
		OpPoint p = pg.m_pos;

		if (p.x < bb.left)
			bb.left = p.x;
		if (p.y < bb.top)
			bb.top = p.y;

		p.x += g.buffer_data->w;
		p.y += g.buffer_data->h;

		if (p.x > bb.right)
			bb.right = p.x;
		if (p.y > bb.bottom)
			bb.bottom = p.y;
	}
	processed_string.m_top_left_positioned = TRUE;
	box.Set(bb.left, bb.top, bb.right - bb.left + 1, bb.bottom - bb.top + 1);
	return OpStatus::OK;
}


OP_STATUS MDF_ProcessedGlyphBuffer::Grow(const size_t size, BOOL copy_contents/* = FALSE*/)
{
	if (m_size >= size)
		return OpStatus::OK;

	ProcessedGlyph* tmp = OP_NEWA(ProcessedGlyph, size);
	if (!tmp)
		return OpStatus::ERR_NO_MEMORY;

	if (copy_contents)
		op_memcpy(tmp, m_buffer, m_size * sizeof(*m_buffer));

	OP_DELETEA(m_buffer);
	m_size = size;
	m_buffer = tmp;
	return OpStatus::OK;
}

// default implementation - if anyone wants to override, go ahead
// virtual
OP_STATUS MDF_FontEngine::ProcessString(MDE_FONT* font,
										ProcessedString& processed_string,
										const uni_char* str, const unsigned int in_len,
										const int extra_space/* = 0*/,
										const short word_width/* = -1*/,
										const int flags/* = MDF_PROCESS_FLAG_NONE*/)
{
	OP_PROBE5(OP_PROBE_MDEFONT_PROCESS_STRING);

	OP_ASSERT(font);

	processed_string.m_top_left_positioned = FALSE;

#ifdef MDF_OPENTYPE_SUPPORT
	OP_STATUS status = OTHandler::ProcessString(font, processed_string, str, in_len, extra_space, word_width, flags);
	if (status != (OP_STATUS)1) // 1 means no processing necessary
		return status;
#endif // MDF_OPENTYPE_SUPPORT

	RETURN_IF_ERROR(font->engine->glyph_buffer.Grow(in_len));
	ProcessedGlyph* glyphs = font->engine->glyph_buffer.Storage();

	size_t out_len = 0; // number of glyphs in output string
	UnicodePoint ucp; // current unicode codepoint
	int px = 0; // x position of current glyph - advance accumulates here
	short advance; // advance of current glyph
	UnicodeStringIterator it(str, 0, in_len);
	while (!it.IsAtEnd())
	{
		ucp = it.At();
		ProcessedGlyph& pg = glyphs[out_len];
		pg.m_id = ucp;
		pg.m_pos.Set(px, font->ascent);

		if (!(flags & MDF_PROCESS_FLAG_NO_ADVANCE))
		{
			RETURN_IF_ERROR(GlyphAdvance(font, ucp, advance));
			px += advance;
		}
		px += extra_space;

		OP_ASSERT(out_len < font->engine->glyph_buffer.Capacity());
		++out_len;

		it.Next();
	}

	processed_string.m_is_glyph_indices = FALSE;
	processed_string.m_length = out_len;
	processed_string.m_advance = px;
	processed_string.m_processed_glyphs = glyphs;

#ifdef MDF_KERNING
	RETURN_IF_ERROR(UCPToGID(font, processed_string));
	RETURN_IF_ERROR(ApplyKerning(font, processed_string));
#else
	if (flags & MDF_PROCESS_FLAG_USE_GLYPH_INDICES)
	{
		RETURN_IF_ERROR(UCPToGID(font, processed_string));
	}
#endif // MDF_KERNING

	// if requested, adjust glyphs to reach desired width
	if (!(flags & MDF_PROCESS_FLAG_NO_ADVANCE) && in_len > 1 &&
		word_width != -1 && word_width != processed_string.m_advance)
	{
#ifdef DEBUG
		BOOL strictly_adjusted =
#endif //DEBUG
		AdjustToWidth(processed_string, word_width);

#ifdef DEBUG
		if (strictly_adjusted)
		{
			// verify that adjusted width matches requested
			ProcessedGlyph& pg = glyphs[out_len-1];
			short advance;
			MDF_GlyphHandle gh(font);
			if (OpStatus::IsSuccess(gh.Advance(advance, pg.m_id, processed_string.m_is_glyph_indices)))
				OP_ASSERT(pg.m_pos.x + advance + extra_space == word_width);
		}
#endif // DEBUG
	}

	return OpStatus::OK;
}

#if defined(MDF_SFNT_TABLES) && defined(_GLYPHTESTING_SUPPORT_)
static OP_STATUS GetFontTable(OpFontInfo* fi, unsigned long tab_tag, BYTE*& tab, UINT32& size)
{
	OP_STATUS status;
	MDE_FONT* font = NULL;
	if(fi->GetWebFont())
	{
		font = MDF_GetWebFont((MDF_WebFontBase*)fi->GetWebFont(), 4);
	}
	else
	{
		font = MDF_GetFont(fi->GetFontNumber(), 4, false, false);
	}

	if(!font)
	{
		status = OpStatus::ERR;
	}
	else
	{
		status = MDF_GetTable(font, tab_tag, tab, size);
		MDF_ReleaseFont(font);
	}

	return status;
}

template<typename T, int SIZE>
static T encodeValue(const BYTE* bp)
{
	const int BYTE_BITS = 8;

	T val = 0;
	for (int i = 0; i < SIZE; ++i)
	{
		val = (val << BYTE_BITS) | bp[i];
	}
	return val;
}
// format 12 microsoft (platform ID 3) UCS-4 (encoding ID 10) sfnt subtable
static OP_STATUS ParseFormat12Table(const BYTE* cmap, const UINT32 offs, const UINT32 size, OpFontInfo* fontinfo)
{
	if (offs + 16 > size) // subtable (way) too small
		return OpStatus::ERR;

	const BYTE* tab = cmap + offs;

	const  UINT16 format = encodeValue<UINT16, 2>(tab);
	if (format != 12)
	{
		// unsupported format
		return OpStatus::ERR;
	}

	/* Skip fields "language" and "reservePad" which should both typically be zero.
	 * const UINT16 reserved = encodeValue<UINT16, 2>(tab+2);
	 * const UINT32 language = encodeValue<UINT32, 4>(tab + 8);
	 */

	const UINT32 length = encodeValue<UINT32, 4>(tab + 4);
	const UINT32 ngroups = encodeValue<UINT32, 4>(tab + 12);

	if (offs + length > size // size of subtable exceeds that of table
		|| 16 + 12*ngroups > length) // header + entries exceed recorded length
		return OpStatus::ERR;

	const UINT8* group = tab + 16;
	for (UINT32 i = 0; i < ngroups; ++i)
	{
		const UINT32 start = encodeValue<UINT32, 4>(group + 0);
		UINT32 end = encodeValue<UINT32, 4>(group + 4);
		// const UINT32 glyph = encodeValue<UINT32, 4>(group + 8);

		// currently glyph testing is only supported for BMP SMP and SIP
		const UnicodePoint highest = 0x2ffff;
		if (start > highest)
			break;
		if (end > highest)
			end = highest;

		if (end < start)
			return OpStatus::ERR;

		for (UnicodePoint uc = start; uc <= end; ++uc)
			RETURN_IF_ERROR(fontinfo->SetGlyph(uc));

		group += 12;
	}
	OP_ASSERT(group <= tab + length);

	return OpStatus::OK;
}
// format 4 microsoft (platform ID 3) unicode (encoding ID 1) sfnt subtable
static OP_STATUS ParseFormat4Table(const BYTE* cmap, const UINT32 offs, const UINT32 size, OpFontInfo* fontinfo)
{
	if (offs + 14 > size) // subtable (way) too small
		return OpStatus::ERR;

	const BYTE* tab = cmap + offs;

	const  UINT16 format = encodeValue<UINT16, 2>(tab);
	if (format != 4)
	{
		// unsupported format
		return OpStatus::ERR;
	}

	const UINT16 length = encodeValue<UINT16, 2>(tab + 2);
	if (offs + length > size) // size of subtable exceeds that of table
		return OpStatus::ERR;
	const BYTE* tabEnd = tab + length;

	const UINT16 segCount2 = encodeValue<UINT16, 2>(tab + 6);
	if (length < 16 + 4*segCount2) // subtable not big enough to hold data
		return OpStatus::ERR;

	/* Skip fields "language" and "reservePad" which should both typically be zero.
	 * const UINT16 language = encodeValue<UINT16, 2>(tab + 4);
	 * const UINT16 reservePad = encodeValue<UINT16, 2>(tab + 14 + segCount2);
	 */

	const BYTE* endCount    = tab + 14;
	const BYTE* startCount  = endCount + 2 + segCount2;
	const BYTE* idDelta     = startCount + segCount2;
	const BYTE* rangeOffset = idDelta + segCount2;

	// loop over segments
	for (unsigned int seg2 = 0; seg2 < segCount2; seg2 += 2)
	{
		const UINT16 end    = encodeValue<UINT16, 2>(endCount    + seg2);
		const UINT16 start  = encodeValue<UINT16, 2>(startCount  + seg2);
		const INT16  delta  = encodeValue<INT16,  2>(idDelta     + seg2);
		const UINT16 offset = encodeValue<UINT16, 2>(rangeOffset + seg2);
		for (unsigned int u = start; u <= end; ++u)
		{
			// determine glyph id
			UINT16 g_id;
			if (!offset	||
				offset == 0xffff) // (at least) AkrutiMal1 uses this to mean missing glyph
				g_id = delta + u;
			else
			{
				const BYTE* glyphId = rangeOffset + seg2 + /*mod 65536*/(UINT16)(offset + 2*(u - start));
				g_id = /* bounds check */(glyphId + 2 > tabEnd) ? 0 : encodeValue<UINT16, 2>(glyphId);
				if (g_id)
					g_id += delta;
			}
			if (g_id)
				RETURN_IF_ERROR(fontinfo->SetGlyph(u));
			if (u == 0xffff)
				break;
		}
		if (end == 0xffff)
		{
			RETURN_IF_ERROR(fontinfo->ClearGlyph(end));
			break;
		}
	}
	return OpStatus::OK;
}
OP_STATUS MDF_UpdateGlyphMask(OpFontInfo* fontinfo, MDF_FontEngine* engine)
{
	BYTE* cmap;
	UINT32 size;
	const unsigned long tag = (((unsigned long)'c'<<24)|((unsigned long)'m'<<16)|((unsigned long)'a'<<8)|'p');
	RETURN_IF_ERROR(GetFontTable(fontinfo, tag, cmap, size));
	if (size < 4) // not enough room for header
	{
		MDF_FreeTable(cmap, engine);
		return OpStatus::ERR;
	}

#ifdef DEBUG_ENABLE_OPASSERT
	const UINT16 version = encodeValue<UINT16, 2>(cmap + 0);
	OP_ASSERT(version == 0); // format says this should be 0
#endif // DEBUG_ENABLE_OPASSERT
	const UINT16 count = encodeValue<UINT16, 2>(cmap + 2);
	if (size < 8u * count) // not enough room for records
	{
		MDF_FreeTable(cmap, engine);
		return OpStatus::ERR;
	}


	// parse subtables
	// currently only support for Microsoft
	// * format 4 - Unicode 'Segment mapping to delta values'. "All
	//   Microsoft Unicode encodings (Platform ID = 3, Encoding ID = 1)
	//   must provide at least a Format 4 'cmap' subtable". for BMP
	//   unicode points.
	// * format 12 - UCS-4 'Segmented coverage'. for non-BMP unicode
	//   points.

	const UINT16 platformMS   = 3; // microsoft
	const UINT16 encodingUni  = 1; // unicode
	const UINT16 encodingUCS4 = 10; // UCS-4
	const UINT16 format4  =  4; // segment mapping to delta values
	const UINT16 format12 = 12; // segmented coverage
	BOOL read_format4  = FALSE;
	BOOL read_format12 = FALSE;

	const BYTE* p = cmap + 4;
	OP_STATUS status = OpStatus::OK;
	for (UINT32 i = 0; i < count; ++i, p += 8)
		if (encodeValue<UINT16, 2>(p + 0) == platformMS)
		{
			const UINT16 encoding = encodeValue<UINT16, 2>(p + 2);
			const UINT32 offset = encodeValue<UINT32, 4>(p + 4);

			if (size < offset + 2) // this subtable falls outside cmap table
				continue;

			const UINT16 format = encodeValue<UINT16, 2>(cmap + offset);
			if (!read_format4 && encoding == encodingUni && format == format4)
			{
				status = ParseFormat4Table(cmap, offset, size, fontinfo);
				if (OpStatus::IsError(status))
					break;
				read_format4 = TRUE;
				// both formats read - no need to take this further
				if (read_format12)
					break;
			}
			else if (!read_format12 && encoding == encodingUCS4 && format == format12)
			{
				status = ParseFormat12Table(cmap, offset, size, fontinfo);
				if (OpStatus::IsError(status))
					break;
				read_format12 = TRUE;
				// both formats read - no need to take this further
				if (read_format4)
					break;
			}
		}

	if (!read_format4 && !read_format12) // no appropriate table found
		status = OpStatus::ERR;

	MDF_FreeTable(cmap, engine);
	return status;
}
#endif // MDF_SFNT_TABLES && _GLYPHTESTING_SUPPORT_

OP_STATUS MDF_AddFontFile(const uni_char* filename, MDF_FontEngine* engine)
{
	OP_STATUS status = engine->AddFontFile(filename);
	return status;
}

void AutoFaceNameHashTable::Delete(void* data)
{
	uni_char* name = reinterpret_cast<uni_char*>(data);
	OP_DELETEA(name);
}

/**
   adds name pointed to by 'it' to hash
   @return
   OpStatus::OK if name was successfully fetched, and either added to hash or already present
   OpStatus::ERR if name was in an unsupported format
   OpStatus::ERR_NO_MEMORY on OOM
 */
static
OP_STATUS AddNameToHash(SFNTNameIterator& it, AutoFaceNameHashTable& hash)
{
	uni_char* name;
	// OpStatus::ERR means unsupported encoding
	RETURN_IF_ERROR(it.GetUnicodeName(name));

	// add to hash
	OP_STATUS status = hash.Add(name, name);
	if (OpStatus::IsError(status))
	{
		OP_DELETEA(name);
		// name already in hash - no need to report this
		if (!OpStatus::IsMemoryError(status))
			status = OpStatus::OK;
	}
	return status;
}

/**
   adds first (fetchable) name entry matching nameID to hash.
   @return
   OpStatus::OK on success (first entry added or already in hash)
   OpStatus::ERR on data error
   OpStatus::ERR_NO_MEMORY on OOM
 */
static
OP_STATUS AddFirstEntryToHash(const UINT8* name_table, size_t name_table_size,
                                  AutoFaceNameHashTable& hash, INT32 nameID)
{
	SFNTNameIterator it;
	if (!it.Init(name_table, name_table_size))
		return OpStatus::ERR;
	while (it.Next(-1, -1, -1, nameID))
	{
		const OP_STATUS status = AddNameToHash(it, hash);
		// OpStatus::ERR can be ignored, since it means the name in
		// question was in an unsupported format
		RETURN_IF_MEMORY_ERROR(status);
		if (OpStatus::IsSuccess(status))
			break;
	}
	return OpStatus::OK;
}

OP_STATUS MDF_GetFaceNames(const UINT8* name_table, size_t name_table_size,
						   AutoFaceNameHashTable& hash)
{
	SFNTNameIterator it;
	if (!it.Init(name_table, name_table_size))
		return OpStatus::ERR;

	// look up relevant name entries - see local()-part of http://dev.w3.org/csswg/css3-fonts/#src-desc

	while (it.Next(-1, -1, -1, -1, TRUE))
	{
		// "full font name" and "postscript name"
		if (it.m_rec.nameID != 4 && it.m_rec.nameID != 6)
			continue;

		// add to hash
		// OpStatus::ERR can be ignored, since it means the name in
		// question was in an unsupported format
		RETURN_IF_MEMORY_ERROR(AddNameToHash(it, hash));
	}

	// "... or the first localization when a US English full font name is not available"
	if (!hash.GetCount())
	{
		RETURN_IF_ERROR(AddFirstEntryToHash(name_table, name_table_size, hash, 4));
		RETURN_IF_ERROR(AddFirstEntryToHash(name_table, name_table_size, hash, 6));
	}

	return OpStatus::OK;
}

OP_STATUS MDF_AddFontFile(const uni_char* filename, MDF_WebFontBase*& webfont, MDF_FontEngine* engine)
{
	RETURN_IF_ERROR(engine->AddWebFont(filename, webfont));
	webfont->engine = engine;
	return OpStatus::OK;
}

OP_STATUS MDF_RemoveFont(MDF_WebFontBase* webfont)
{
	return webfont->engine->RemoveFont(webfont);
}

OP_STATUS MDF_BeginFontEnumeration(MDF_FontEngine* engine)
{
	return engine->BeginFontEnumeration();
}

void MDF_EndFontEnumeration(MDF_FontEngine* engine)
{
	engine->EndFontEnumeration();
}

int MDF_CountFonts(MDF_FontEngine* engine)
{
	return engine->CountFonts();
}

OP_STATUS MDF_GetFontInfo(int font_nr, MDF_FONTINFO* font_info, MDF_FontEngine* engine)
{
	return engine->GetFontInfo(font_nr, font_info);
}

OP_STATUS MDF_GetLocalFont(MDF_WebFontBase*& localfont, const uni_char* facename, MDF_FontEngine* engine)
{
	RETURN_IF_ERROR(engine->GetLocalFont(localfont, facename));
	if (localfont)
		localfont->engine = engine;
	return OpStatus::OK;
}

OP_STATUS MDF_GetWebFontInfo(MDF_WebFontBase* webfont, MDF_FONTINFO* font_info)
{
	return webfont->engine->GetWebFontInfo(webfont, font_info);
}

OpFontInfo::FontType MDF_GetWebFontType(MDF_WebFontBase* webfont)
{
	return webfont->engine->GetWebFontType(webfont);
}

const uni_char* MDF_GetFontName(MDE_FONT* font)
{
	OP_ASSERT(font);
	return font->engine->GetFontName(font);
}

const uni_char* MDF_GetFontFileName(MDE_FONT* font)
{
	OP_ASSERT(font);
	return font->engine->GetFontFileName(font);
}

BOOL MDF_HasGlyph(MDE_FONT* font, uni_char c)
{
	return font->engine->HasGlyph(font, c);
}

BOOL MDF_IsScalable(MDE_FONT* font)
{
	return font->engine->IsScalable(font);
}

#ifdef OPFONT_GLYPH_PROPS
OP_STATUS MDF_GetGlyphProps(MDE_FONT* font, const UnicodePoint up, OpFont::GlyphProps* props)
{
	return font->engine->GetGlyphProps(font, up, props);
}
#endif // OPFONT_GLYPH_PROPS

OP_STATUS MDF_StringWidth(int& width, MDE_FONT* font, const uni_char* str, int len, int extra_space)
{
	OP_PROBE4(OP_PROBE_MDEFONT_STRINGWIDTH);

	if (!len)
		return OpStatus::ERR;

#ifndef MDF_UNPROCESSED_STRING_API
	width = 0;
	ProcessedString processed_string;
	RETURN_IF_ERROR(MDF_ProcessString(font, processed_string, str, len, extra_space));
	width = processed_string.m_advance;
	return OpStatus::OK;
#else
	return font->engine->StringWidth(width, font, str, len, extra_space);
#endif // !MDF_UNPROCESSED_STRING_API
}

#ifdef MDF_DRAW_TO_MDE_BUFFER
OP_STATUS MDF_DrawString(MDE_FONT* font, const uni_char* str, unsigned int len, int x, int y, int extra, MDE_BUFFER* dst, short word_width/* = -1*/)
{
	OP_PROBE4(OP_PROBE_MDEFONT_DRAWSTRING);

	if (!len)
		return OpStatus::ERR;

#ifndef MDF_UNPROCESSED_STRING_API
	ProcessedString processed_string;
	RETURN_IF_ERROR(MDF_ProcessString(font, processed_string, str, len, extra, word_width));
	if (extra < 0 && processed_string.m_advance < 0)
		RETURN_IF_ERROR(MDF_ProcessString(font, processed_string, str, len, 0, word_width));

	return font->engine->DrawString(font, processed_string, x, y, dst);
#else
	return font->engine->DrawString(font, str, len, x, y, extra, dst, word_width);
#endif // !MDF_UNPROCESSED_STRING_API
}

/**
   default implementataion - feel free to override
 */
# include "modules/libgogi/mde.h"
OP_STATUS MDF_FontEngine::DrawString(MDE_FONT* font, const ProcessedString& processed_string, const int x, const int y, MDE_BUFFER* dst)
{
	OP_ASSERT(!processed_string.m_top_left_positioned);

    MDE_BUFFER_INFO buffer_info = {
        NULL, dst->col, 0,
        MDE_METHOD_COLOR, MDE_FORMAT_INDEX8,
#ifdef MDE_BILINEAR_BLITSTRETCH
		MDE_DEFAULT_STRETCH_METHOD,
#endif // MDE_BILINEAR_BLITSTRETCH
    };

#ifdef MDF_FONT_SMOOTHING
	OP_ASSERT(!HasComponentRendering() || !"Font smoothing not implemented!");
#endif

	const BOOL isIndex = processed_string.m_is_glyph_indices;

	OP_STATUS status = OpStatus::OK;
    for (unsigned int i = 0; i < processed_string.m_length; ++i)
    {
		const ProcessedGlyph& g = processed_string.m_processed_glyphs[i];
		MDF_GlyphHandle handle(font);
		OP_STATUS s = handle.Get(g.m_id, TRUE, isIndex);
        if (OpStatus::IsSuccess(s))
        {
			MDF_GLYPH glyph = handle.Glyph();
			MDE_RECT dst_rect = MDE_MakeRect(x + g.m_pos.x + glyph.bitmap_left,
											 y + g.m_pos.y - glyph.bitmap_top,
											 glyph.buffer_data->w,
											 glyph.buffer_data->h);

			MDE_BUFFER_DATA glyph_buf;
			glyph_buf.data = glyph.buffer_data->data;
			glyph_buf.w = glyph.buffer_data->w;
			glyph_buf.h = glyph.buffer_data->h;
			MDE_DrawBufferData(&glyph_buf, &buffer_info, glyph_buf.w, dst_rect, 0, 0, dst);
        }
		else if (!OpStatus::IsMemoryError(status)) // OOM takes precedence
			status = s;
    }

	return status;
}
#endif // MDF_DRAW_TO_MDE_BUFFER

#ifdef MDF_DRAW_TO_BITMAP
OP_STATUS MDF_DrawString(OpBitmap*& bitmap, short& top, short& left, MDE_FONT* font, const uni_char* str, unsigned int len, int extra, unsigned int color, short word_width/* = -1*/)
{
	bitmap = 0;
	if (!len)
		return OpStatus::ERR;
	ProcessedString processed_string;
	RETURN_IF_ERROR(MDF_ProcessString(font, processed_string, str, len, extra, word_width));
	if (extra < 0 && processed_string.m_advance < 0)
		RETURN_IF_ERROR(MDF_ProcessString(font, processed_string, str, len, 0, word_width));
	return font->engine->DrawString(bitmap, top, left, font, processed_string, color);
}

static inline void set_color_map(unsigned int* col_map, unsigned int color)
{
	for (unsigned int i = 0; i < 256; ++i)
		col_map[i] =
#ifdef USE_PREMULTIPLIED_ALPHA
			((color & 0xff) * (i+1) >> 8) |
			((((color >> 8)  & 0xff) * (i+1) >> 8) << 8) |
			((((color >> 16)  & 0xff) * (i+1) >> 8) << 16) |
#else // USE_PREMULTIPLIED_ALPHA
			(color & 0x00ffffff) |
#endif // USE_PREMULTIPLIED_ALPHA
			(((i * (((color >> 24) & 0xff) + 1)) >> 8) << 24);
}
/**
   default implementataion - feel free to override
 */
#include "modules/pi/OpBitmap.h"
OP_STATUS MDF_FontEngine::DrawString(OpBitmap*& bitmap, short& top, short& left, MDE_FONT* font, ProcessedString& processed_string, const unsigned int color)
{
	OP_ASSERT(!processed_string.m_top_left_positioned);

	OpRect box;
	RETURN_IF_ERROR(LayoutString(font, processed_string, box));
	const int bitmap_width = box.width;
	const int bitmap_height = box.height;
	top = box.y;
	left = box.x;

	if (!bitmap_width || !bitmap_height)
		return (OP_STATUS)1;

	OP_ASSERT(bitmap_width > 0);
	OP_ASSERT(bitmap_height > 0);


	// create bitmap
	UINT32* data = 0;
	bitmap = 0;
	OP_STATUS status = OpBitmap::Create(&bitmap, bitmap_width, bitmap_height);
	RETURN_IF_ERROR(status);
	if (!bitmap->Supports(OpBitmap::SUPPORTS_POINTER))
	{
		OP_ASSERT(!"drawing to bitmap requires GetPointer support");
		status = OpStatus::ERR;
	}
	if (OpStatus::IsSuccess(status))
	{
		data = (UINT32*)bitmap->GetPointer(OpBitmap::ACCESS_WRITEONLY);
		if (!data)
			status = OpStatus::ERR_NO_MEMORY;
	}

	const BOOL isIndex = processed_string.m_is_glyph_indices;

	if (OpStatus::IsSuccess(status))
	{
		UINT32* data_end = data + bitmap_width*bitmap_height;

		// FIXME: share this, since it's not likely to change between words
		unsigned int color_map[256];
		set_color_map(color_map, color);

		// clear bitmap
		UINT32* target = data;
		while (target < data_end)
			*target++ = color_map[0];

		for (unsigned int i = 0; i < processed_string.m_length; ++i)
		{
			MDF_GlyphHandle handle(font);
			status = handle.Get(processed_string.m_processed_glyphs[i].m_id, TRUE, isIndex);
			if (OpStatus::IsError(status))
				break;
			MDF_GLYPH& glyph = handle.Glyph();

			const int start_y = processed_string.m_processed_glyphs[i].m_pos.y/* - glyph.bitmap_top*/;
			const int end_y = start_y + glyph.buffer_data->h;
			OP_ASSERT(start_y >= 0);
			OP_ASSERT(start_y <= end_y);
			OP_ASSERT(end_y <= bitmap_height);

			int px = processed_string.m_processed_glyphs[i].m_pos.x/* + glyph.bitmap_left*/ - left;
			const int width = glyph.buffer_data->w;
			OP_ASSERT(px >= 0);
			OP_ASSERT(px + width <= bitmap_width);

			target = data + start_y*bitmap_width + px;
			unsigned char* glyph_data = (unsigned char*)glyph.buffer_data->data;
			// glyph
			for (int py = start_y; py < end_y; ++py)
			{
				// color within the lines!
				OP_ASSERT(target >= data + py*bitmap_width);

				// one glyph line
				for (int x = 0; x < width; ++x)
					*target++ = color_map[*glyph_data++];

				// color within the lines!
				OP_ASSERT(target <= data + (py+1)*bitmap_width);

				target += bitmap_width - width;
			}
			// make sure we've drawn the entire glyph
			OP_ASSERT((unsigned char*)glyph.buffer_data->data + glyph.buffer_data->w*glyph.buffer_data->h == glyph_data);
		}
		bitmap->ReleasePointer(TRUE);
	}

	if (OpStatus::IsError(status))
		OP_DELETE(bitmap);

	return status;
}
#endif // MDF_DRAW_TO_BITMAP

static MDE_FONT*
GetFontInt(MDE_FONT* font, MDF_FontEngine* engine)
{
	if (!font)
		return 0;

	font->engine = engine;
	OP_ASSERT(font->engine);

#ifdef MDF_FONT_ADVANCE_CACHE
	font->m_advance_cache = 0;
	MDF_AdvanceCache* advance_cache;
	// possibly, size of advance cache should be made to vary
	// depending on script likely to be used, since eg CJK text
	// contains much more characters than eg latin.
	if (OpStatus::IsError(MDF_AdvanceCache::Create(&advance_cache, font, MDF_ADVANCE_CACHE_SIZE, MDF_ADVANCE_CACHE_HASH_SIZE)))
	{
		MDF_ReleaseFont(font);
		return 0;
	}
	font->m_advance_cache = advance_cache;
#endif // MDF_FONT_ADVANCE_CACHE

#ifdef DEBUG_ENABLE_OPASSERT
	++ font->engine->m_created_font_count;
#endif // DEBUG_ENABLE_OPASSERT

	return font;
}

MDE_FONT* MDF_GetFont(int font_nr, int size, BOOL bold, BOOL italic, MDF_FontEngine* engine)
{
	MDE_FONT* font = engine->GetFont(font_nr, size, bold, italic);
	return GetFontInt(font, engine);
}

MDE_FONT* MDF_GetWebFont(MDF_WebFontBase* webfont, int size)
{
	MDE_FONT* font = webfont->engine->GetWebFont(webfont, size);
	return GetFontInt(font, webfont->engine);
}
MDE_FONT* MDF_GetWebFont(MDF_WebFontBase* webfont, UINT8 weight, BOOL italic, int size)
{
	MDE_FONT* font = webfont->engine->GetWebFont(webfont, weight, italic, size);
	return GetFontInt(font, webfont->engine);
}

void MDF_ReleaseFont(MDE_FONT* font)
{
#ifdef DEBUG_ENABLE_OPASSERT
	-- font->engine->m_created_font_count;
#endif // DEBUG_ENABLE_OPASSERT

#ifdef MDF_FONT_ADVANCE_CACHE
	OP_DELETE(font->m_advance_cache);
#endif // MDF_FONT_ADVANCE_CACHE
	font->engine->ReleaseFont(font);
}

UINT32 MDF_GetCharIndex(MDE_FONT* font, const UINT32 ch)
{
	return font->engine->GetCharIndex(font, ch);
}
UINT32 MDF_GetCharIndex(MDE_FONT* font, const uni_char* str, const UINT32 len, UINT32& pos)
{
	if (!str || pos >= len || !len)
		return 0;
	return font->engine->GetCharIndex(font, str, len, pos);
}


void* MDF_GetFontImplementation(MDE_FONT* font)
{
	return font->engine->GetFontImplementation(font);
}

#ifdef MDF_SFNT_TABLES
OP_STATUS MDF_GetTable(MDE_FONT* font, unsigned long tab_tag, BYTE*& tab, UINT32& size)
{
	return font->engine->GetTable(font, tab_tag, tab, size);
}
BYTE* MDF_GetTable(MDE_FONT* font, unsigned long tab_tag)
{
	BYTE* tab;
	UINT32 size;
	OP_STATUS status = MDF_GetTable(font, tab_tag, tab, size);
 	return OpStatus::IsSuccess(status) ? tab : 0;
}

void MDF_FreeTable(BYTE* tab, MDF_FontEngine* engine)
{
	engine->FreeTable(tab);
}
void MDF_FreeTable(MDE_FONT* font, BYTE* tab)
{
	font->engine->FreeTable(tab);
}
#endif // MDF_SFNT_TABLES

OP_STATUS MDF_GetGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, BOOL mustRender, BOOL isIndex)
{
	return font->engine->GetGlyph(glyph, font, ch, mustRender, isIndex);
}
void MDF_FreeGlyph(MDE_FONT* font, MDF_GLYPH& glyph)
{
	font->engine->FreeGlyph(glyph);
}
OP_STATUS MDF_FillGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, const UINT32 stride, BOOL isIndex)
{
	OP_ASSERT(glyph.buffer_data);
	OP_ASSERT(glyph.buffer_data->data);
	OP_ASSERT(glyph.buffer_data->w <= stride);
	return font->engine->FillGlyph(glyph, font, ch, stride, isIndex);
}
#ifdef MDF_FONT_GLYPH_CACHE
OP_STATUS MDF_BorrowGlyph(MDF_GLYPH& glyph, MDE_FONT* font, const UINT32 ch, BOOL mustRender, BOOL isIndex)
{
	return font->engine->BorrowGlyph(glyph, font, ch, mustRender, isIndex);
}
#endif // MDF_FONT_GLYPH_CACHE

#ifdef SVG_SUPPORT
OP_STATUS MDF_GetOutline(MDE_FONT* font, const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos, BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph)
{
	if (!out_glyph)
		return OpStatus::OK;
	if (!in_str || io_str_pos >= in_len || !in_len)
		return OpStatus::ERR_OUT_OF_RANGE;

	SVGPath *go;
	RETURN_IF_ERROR(g_svg_manager->CreatePath(&go));

	const UINT32 g_id = MDF_GetCharIndex(font, in_str, in_len, io_str_pos);
	const UINT32 next_g_id = io_str_pos < in_len ? MDF_GetCharIndex(font, in_str, in_len, in_last_str_pos = io_str_pos) : 0;
	OP_STATUS status = font->engine->GetOutline(font, g_id, next_g_id, in_writing_direction_horizontal, out_advance, go);
	if (OpStatus::IsError(status))
	{
		// cleanup goes here
	}

	*out_glyph = go;
	return status;
}
#endif // SVG_SUPPORT

#endif // MDEFONT_MODULE
