/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef MDEFONT_MODULE
#ifdef MDF_OPENTYPE_SUPPORT

#include "modules/mdefont/mdf_opentype.h"
#include "modules/mdefont/mdefont.h"

#include "modules/libgogi/mde.h"

#define OT_MAKE_TAG(s) *(UINT32*)(s)

#define RETURN_IF_OUT_OF_BOUNDS_C(a) \
	if ( (a) > m_table_end ) {	OP_ASSERT(!"out of bounds for table"); return FALSE; }
#define RETURN_IF_OUT_OF_BOUNDS(a, b)										\
	if ( (a) > (b) ) {	OP_ASSERT(!"out of bounds for table"); return FALSE; }

static inline UINT16 MDF_GetU16(const UINT8* tab)
{
    return (tab[0] << 8) + tab[1];
}

static inline UINT32 MDF_GetU32(const UINT8* tab)
{
	return (tab[0] << 24) + (tab[1] << 16) + (tab[2] << 8) + tab[3];
}

// fetches table addressed with Offset
// tab should point to the location the offset value is based on
// offs is the offset from tab to the location of the Offset entry
static inline const UINT8* GetTable(const UINT8* tab, const UINT32 offs)
{
	return tab + MDF_GetU16(tab+offs);
}


/**
   many subtables have entries of the format
   * m bytes of "header"
   * n occurrences of: either a struct of some kind, or an offset to another table
   this struct is used to itterate over such entries, and performs bounds-checking
 */
struct OT_ListIt
{
	/**
	   creates itterator for entries of type:
	   * header_size bytes of "header"
	   * count entries of size struct_size
	   Struct will return pointer to current entry

	   @param start address to start of table
	   @param end points to first byte after end of table (used to do bounds-checking)
	   @param header_size size (in bytes) of data before list of entries
	   @param struct_size size (in bytes) of each entry
	*/
	OT_ListIt(const UINT8* start, const UINT8* end,
			  const unsigned int header_size, const unsigned int struct_size)
		: start(start), end(end),
		  header_size(header_size), struct_size(struct_size),
		  count(-1), current(-1)
	{}
	/**
	   creates itterator for entries of type:
	   * header_size bytes of "header"
	   * count entries of type U16, representing an offset (in bytes) from start

	   @param start address to start of table
	   @param end points to first byte after end of table (used to do bounds-checking)
	   @param header_size size (in bytes) of data before list of entries
	*/
	OT_ListIt(const UINT8* start, const UINT8* end,
			  const unsigned int header_size)
		: start(start), end(end),
		  header_size(header_size), struct_size(2),
		  count(-1), current(-1)
	{}

	/**
	   initializes itterator

	   @param count_offset offset (in bytes, relative to start) to number of entries
	   @return FALSE if out of bounds, TRUE otherwize
	 */
	BOOL Init(const unsigned int count_offset)
	{
		OP_ASSERT(count_offset+2 <= header_size);
		RETURN_IF_OUT_OF_BOUNDS(start + count_offset + 2, end);
		count = MDF_GetU16(start+count_offset);
		return start + header_size + count*struct_size <= end;
	}
	/** @return number of entries */
	UINT16 Count() { OP_ASSERT(count >= 0); return count; }
	/** @return current entry */
	UINT16 Current() { OP_ASSERT(current >= 0); return (UINT16)current; }

	/**
	   when initialized current is -1, so Next() should be called once
	   before Table() or Struct() is called. the reason for this
	   behaviour is to allow

	   while (Next()) do stuff with Table() or Struct();

	   @return TRUE if more entries remain, FALSE if no more entries remain
	*/
	BOOL Next()
	{
		OP_ASSERT(count >= 0);
		++current;
		return (UINT16)current < count;
	}
	/**
	   @return a pointer to the start of the current struct
	 */
	const UINT8* Struct()
	{
		OP_ASSERT(count >= 0);
		OP_ASSERT(current >= 0 && current < count);
		return start + header_size + current*struct_size;
	}
	/**
	   @param table (out) pointer to the current subtable, offset bytes from start
	   @return FALSE if out of bounds, TRUE otherwize
	 */
	BOOL Table(const UINT8*& table)
	{
		OP_ASSERT(count >= 0);
		OP_ASSERT(current >= 0 && current < count);
		table = GetTable(start, header_size + 2*current);
		return table < end;
	}

	const UINT8* const start;
	const UINT8* const end;
	const unsigned int header_size, struct_size;
	int count, current;
};



static const UINT8* GetScriptList(const UINT8* tab)
{
    return tab+MDF_GetU16(tab+4);
}
static const UINT8* GetFeatureList(const UINT8* tab)
{
    return tab+MDF_GetU16(tab+6);
}
static const UINT8* GetLookupList(const UINT8* tab)
{
    return tab+MDF_GetU16(tab+8);
}

// returns pointer to table tag (assuming offset from tab)
// format: [offs bytes of whatever] <u16 count> <table entry>(count)
// where table entry is <u32 tag> <u16 offset>
static const UINT8* FindTable(const UINT8* tab, const UINT8* end,
							  const UINT32 tag, const UINT32 offs = 0)
{
	OT_ListIt it(tab, end, offs+2, 6);
	if (!it.Init(offs))
		return 0;

	while (it.Next())
	{
		const UINT8* t = it.Struct();
		if (op_memcmp(&tag, t, sizeof(tag)) == 0)
		{
			const UINT8* r = tab + MDF_GetU16(t+4);
			return r < end ? r : 0;
		}
	}

    return 0;
}

// determines language system from string s (with length l)
// FIXME: implement for relevant scripts
static const UINT8* GetLanguageSystem(const UINT8* ScriptList, const UINT8* end,
									  const UINT32 s, const UINT32 l)
{
	if (!ScriptList)
		return 0;

	// find out script and language system
	UINT32 scriptTag = 0, languageSystemTag = 0;
	// FIXME: determine scripts and language systems here (from s and l)

	if (!scriptTag)
		return 0;

	const UINT8* Script = FindTable(ScriptList, end, scriptTag);
	if (!Script)
		return 0;


#if 0
	// DEBUG: print language systems for current script
	fprintf(stderr, "listing language systems for script\n");
	UINT16 numLangSys = MDF_GetU16(Script+2);
	for (UINT32 i = 0; i < numLangSys; ++i)
		fprintf(stderr, "* %c%c%c%c\n",
				Script + 4 + 6*i + 0,
				Script + 4 + 6*i + 1,
				Script + 4 + 6*i + 2,
				Script + 4 + 6*i + 3);
#endif // 0


	const UINT8* LanguageSystem = FindTable(Script, end, languageSystemTag, 2);
	if (!LanguageSystem) // fall back to default
	{
		LanguageSystem = GetTable(Script, 0);
		if (LanguageSystem == Script)
			return 0;
	}

	return LanguageSystem;
}

// comparison function for format 1 coverage tables
static inline signed char Format1Compare(const UINT8* tab, const UINT16 glyph, const UINT16 pos)
{
	UINT16 g = MDF_GetU16(tab + (pos << 1));
	if (g == glyph)
		return 0;
	return g < glyph ? -1 : 1;
}
// comparison function for format 2 coverage and class tables
static inline signed char Format2Compare(const UINT8* tab, const UINT16 glyph, const UINT16 pos)
{
	UINT16 i = pos*6;
	UINT16 gmin = MDF_GetU16(tab+i);
	UINT16 gmax = MDF_GetU16(tab+i+2);
	if (gmin <= glyph)
	{
		if (gmax >= glyph)
			return 0;
		return -1;
	}
	return 1;
}

// binary search table
// compare function should return 0 on match, -1 if entry at pos is too low, 1 if entry at pos is too high
static inline
BOOL BSearch(const UINT8* t, const UINT16 count, const UINT16 glyph, UINT16& idx,
			 signed char (*compare)(const UINT8*, const UINT16, const UINT16))
{
	if(!count)
		return FALSE;

	UINT16 begin = 0;
	UINT16 end = count-1;
	while (begin < end)
	{
		const UINT16 mid = (begin + end) >> 1;

        // 0: match, -1: this entry too low, 1: this entry too high
		char comp = compare(t, glyph, mid);
		if (!comp)
		{
			idx = mid;
			return TRUE;
		}
		else if (comp == -1)
			begin = mid + 1;
		else
		{
			if (!mid)
				break;
			end = mid - 1;
		}
	}
	if (!compare(t, glyph, begin))
	{
		idx = begin;
		return TRUE;
	}
	return FALSE;
}

BOOL GetCoverage(const UINT8* Coverage, const UINT8* end,
				 const UINT16 glyph, UINT16& coverageIndex)
{
	RETURN_IF_OUT_OF_BOUNDS(Coverage+4, end);

	UINT16 format = MDF_GetU16(Coverage);
	OP_ASSERT(format == 1 || format == 2);

	UINT16 count = MDF_GetU16(Coverage+2);
	UINT16 idx;
	if (format == 1)
	{
		RETURN_IF_OUT_OF_BOUNDS(Coverage + 4 + 2*count, end);
		if (BSearch(Coverage+4, count, glyph, idx, Format1Compare))
		{
			coverageIndex = idx;
			return TRUE;
		}
	}
	else if (format == 2)
	{
		RETURN_IF_OUT_OF_BOUNDS(Coverage + 4 + 6*count, end);
		if (BSearch(Coverage+4, count, glyph, idx, Format2Compare))
		{
			const UINT8* RangeRecord = Coverage + 4 + 6*idx;
			coverageIndex = MDF_GetU16(RangeRecord + 4) + glyph - MDF_GetU16(RangeRecord);
			return TRUE;
		}
	}
	return FALSE;
}

UINT16 GetClass(const UINT8* ClassTable, const UINT8* end,
				const UINT16 g)
{
	RETURN_IF_OUT_OF_BOUNDS(ClassTable+4, end);

	const UINT16 format = MDF_GetU16(ClassTable);
	if (format < 1 || format > 2)
	{
		OP_ASSERT(!"unknown class definition format");
		return 0;
	}

	if (format == 1)
	{
		RETURN_IF_OUT_OF_BOUNDS(ClassTable+6, end);
		const UINT16 gs = MDF_GetU16(ClassTable+2);
		const UINT16 count = MDF_GetU16(ClassTable+4);
		RETURN_IF_OUT_OF_BOUNDS(ClassTable + 6 + 2*count, end);
		if (g < gs || g-gs >= count)
			return 0;
		return (MDF_GetU16(ClassTable + 6 + 2*(g-gs)));
	}

	UINT16 count = MDF_GetU16(ClassTable+2);
	RETURN_IF_OUT_OF_BOUNDS(ClassTable + 4 + 6*count, end);
	UINT16 idx;
	if (BSearch(ClassTable+4, count, g, idx, Format2Compare))
		return MDF_GetU16(ClassTable + 4 + 6*idx + 4);
	return 0;
}

OTCacheEntry::OTCacheEntry()
	: m_handler(0), m_family_name(0), m_file_name(0)
{}
OTCacheEntry::~OTCacheEntry()
{
	OP_DELETE(m_handler);
	op_free(m_family_name);
	op_free(m_file_name);
}

OTCacheEntry* OTCacheEntry::Create(MDE_FONT* font, const uni_char* family_name, const uni_char* file_name)
{
	// create entry
	OTCacheEntry* entry = OP_NEW(OTCacheEntry, ());
	if (!entry)
		return 0;

	// create handler
	OP_STATUS status = OTHandler::Create(font, entry->m_handler);
	if (OpStatus::IsMemoryError(status))
	{
		OP_DELETE(entry);
		return 0;
	}
	// OpStatus:ERR means no gsub table. we still want to keep the
	// entry so we don't try to create a handler for the same font
	// several times
	OP_ASSERT(OpStatus::IsSuccess(status) || !entry->m_handler);

	// copy family name - used to associate handlers with fonts
	entry->m_family_name = uni_strdup(family_name);
	if (!entry->m_family_name)
	{
		OP_DELETE(entry);
		return 0;
	}

	// copy file name - used to associate handlers with fonts
	entry->m_file_name= uni_strdup(file_name);
	if (!entry->m_file_name)
	{
		OP_DELETE(entry);
		return 0;
	}

	return entry;
}

/**
   fetches an OTHandler from cache, creating a new one if needed. cache size is controlled with TWEAK_MDEFONT_OPENTYPE_CACHE_SIZE
 */
OP_STATUS GetHandler(OTHandler*& handler, MDE_FONT* font)
{
	Head* opentype_cache = &font->engine->opentype_cache;
	const uni_char* family_name = MDF_GetFontName(font);
	const uni_char* file_name = MDF_GetFontFileName(font);
	// check cache for handler - family name is used to associate handlers with fonts
	for (OTCacheEntry* e = (OTCacheEntry*)opentype_cache->First(); e; e = (OTCacheEntry*)e->Suc())
	{
		if (!uni_strcmp(e->m_family_name, family_name) && !uni_strcmp(e->m_file_name, file_name))
		{
			// move requested first, since it's likely to be used
			// several times contiguously
			e->Out();
			e->IntoStart(opentype_cache);
			handler = e->m_handler;
			return OpStatus::OK;
		}
	}

	// cache is full, remove last
	OP_ASSERT(opentype_cache->Cardinal() <= OPENTYPE_CACHE_SIZE);
	if (opentype_cache->Cardinal() == OPENTYPE_CACHE_SIZE)
	{
		OTCacheEntry* e = (OTCacheEntry*)opentype_cache->Last();
		e->Out();
		OP_DELETE(e);
	}

	// handler not in cache, create new
	OTCacheEntry* n = OTCacheEntry::Create(font, family_name, file_name);
	if (!n)
		return OpStatus::ERR_NO_MEMORY;

	// put handler in cache
	n->IntoStart(opentype_cache);
	handler = n->m_handler;
	return OpStatus::OK;
}


OP_STATUS OTHandler::Create(MDE_FONT* font, OTHandler*& handler)
{
	handler = 0;

	if (!font)
	{
		OP_ASSERT(!"must pass a font object");
		return OpStatus::ERR;
	}

	// load table
	UINT32 gsub_len;
	BYTE *gsub_table;

	// seems tag is backwards
 	RETURN_IF_ERROR(MDF_GetTable(font, OT_MAKE_TAG("BUSG"), gsub_table, gsub_len));

	// create gsub object
	handler = OP_NEW(OTHandler, ((const UINT8*)gsub_table, gsub_len));
	if (!handler)
	{
		MDF_FreeTable(gsub_table);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OTHandler::OTHandler(const UINT8* gsub, const UINT32 gsub_len)
	: m_gsub(gsub)
	, m_gsub_end(gsub + gsub_len)
	, m_table_end(0)
	, m_ScriptList(GetScriptList(gsub))
	, m_FeatureList(GetFeatureList(gsub))
	, m_LookupList(0)
	, m_LookupCount(0)
	, m_LookupFunc(0)
	, m_status(OpStatus::OK)
	, m_ubuf(0), m_ibuf(0), m_istr(0), m_ustr(0)
{
	OP_ASSERT(gsub);
	if (!gsub)
		return;

	if (m_FeatureList)
	{
		if (m_FeatureList + 2 > m_gsub_end)
		{
			m_FeatureCount = 0;
		}
		else
		{
			m_FeatureCount = MDF_GetU16(m_FeatureList);
 			if (m_FeatureList + 2 + 6*m_FeatureCount > m_gsub_end)
				m_FeatureCount = 0;
		}
	}
}

OTHandler::~OTHandler()
{
	// shouldn't have locked temp buffers when deleted
	OP_ASSERT(!m_ubuf && !m_ibuf);
	MDF_FreeTable((BYTE*)m_gsub);
}

OTHandler::OTProcessedText::OTProcessedText(const OP_STATUS status/* = OpStatus::OK*/)
	: m_handler(0)
	, m_ustr(0), m_istr(0), m_length(0)
	, m_status(status)
#ifdef DEBUG
	, m_checked_status(FALSE)
#endif // DEBUG
{}
OTHandler::OTProcessedText::OTProcessedText(OTHandler* handler, const uni_char* ustr, const uni_char* istr, const UINT32 length)
	: m_handler(handler)
	, m_ustr(ustr), m_istr(istr), m_length(length)
	, m_status(OpStatus::OK)
#ifdef DEBUG
	, m_checked_status(FALSE)
#endif // DEBUG
{}
OTHandler::OTProcessedText::~OTProcessedText()
{
#ifdef DEBUG
	OP_ASSERT(m_checked_status);
#endif // DEBUG
	if (m_handler)
		m_handler->Unlock();
}

// static
OP_STATUS OTHandler::ProcessString(MDE_FONT* font,
								   ProcessedString& processed_string,
								   const uni_char* str, const unsigned int in_len,
								   const int extra_space/* = 0*/, const short word_width/* = -1*/,
								   const int flags /* = MDF_PROCESS_FLAG_NONE*/)
{
	OTProcessedText pt = GetProcessedText(font, str, in_len);
	OP_STATUS status = pt.Status();
	const UINT32 out_len = pt.Length();
	if (!out_len)
		return OpStatus::IsSuccess(status) ? (OP_STATUS)1 : status;
	const uni_char* istr = pt.IStr();

	RETURN_IF_ERROR(font->engine->glyph_buffer.Grow(in_len));
	ProcessedGlyph* glyphs = font->engine->glyph_buffer.Storage();

	int px = 0; // x position of current glyph - advance accumulates here
	for (UINT32 i = 0; i < out_len; ++i)
	{
		ProcessedGlyph& pg = glyphs[i];
		pg.m_id = istr[i];
		pg.m_pos.Set(px, font->ascent);

		if (!(flags & MDF_PROCESS_FLAG_NO_ADVANCE))
		{
			short advance;
			MDF_GlyphHandle gh(font);
			if (OpStatus::IsSuccess(gh.Advance(advance, pg.m_id, TRUE)))
				px += advance;
		}

		px += extra_space;
	}

	processed_string.m_is_glyph_indices = TRUE;
	processed_string.m_length = out_len;
	processed_string.m_advance = px;
	processed_string.m_processed_glyphs = glyphs;

#ifdef MDF_KERNING
	RETURN_IF_ERROR(MDF_ApplyKerning(font, processed_string));
#endif // MDF_KERNING

	// if requested, adjust glyphs to reach desired width
	if (!(flags & MDF_PROCESS_FLAG_NO_ADVANCE) && in_len > 1 &&
		word_width != -1 && word_width != processed_string.m_advance)
		AdjustToWidth(processed_string, word_width);

	return OpStatus::OK;
}

// static
const OTHandler::OTProcessedText OTHandler::GetProcessedText(MDE_FONT* font, const uni_char* str, const UINT32 len)
{
	if (!len || !NeedsProcessing(str))
		return OTProcessedText();
	OP_ASSERT(font);

	OP_STATUS status;

	// get a handler from cache
	OTHandler* handler;
	status = GetHandler(handler, font);
	if (OpStatus::IsError(status))
		return OTProcessedText(status);
	if (!handler) // no processing needed
		return OTProcessedText();

	// lock buffers
	status = handler->Lock(len);
	if (OpStatus::IsError(status))
		return OTProcessedText(status);

	// process
	status = handler->Process(font, str, len);
	if (OpStatus::IsSuccess(status))
		return OTProcessedText(handler, handler->m_ustr, handler->m_istr, handler->m_len);

	// processing failed, unlock
	handler->Unlock();
	return OTProcessedText(status);
}

BOOL OTHandler::NeedsProcessing(const uni_char* str)
{
	// WONKO: atm we only do anything useful for indic text, so
	// there's no point in passing non-indic text further
	return is_indic(*str);
}

OP_STATUS OTHandler::Lock(const UINT32 len)
{
	// if these trigger, this function's probably called when it shouldn't
	OP_ASSERT(!m_ubuf);
	OP_ASSERT(!m_ibuf);
	OP_ASSERT(!m_ustr);
	OP_ASSERT(!m_istr);

	// obtain buffers
	if (!GetOTBuffers(&m_ubuf,&m_ibuf))
	{
		OP_ASSERT(!"opentype buffers are locked");
		// buffers are locked (wtf?)
		return OpStatus::ERR;
	}

	// grow
	return Grow(len);
}
void OTHandler::Unlock()
{
	ReleaseOTBuffers();
	m_ubuf = m_ibuf = 0;
	m_ustr = m_istr = 0;
}

OP_STATUS OTHandler::Grow(const UINT32 _len)
{
	const UINT32 len = _len +1;
	if (len > m_ubuf->GetCapacity())
	{
		RETURN_IF_ERROR(m_ubuf->Expand(len));
	}
	m_ustr = m_ubuf->GetStorage();
	m_ustr[_len] = 0;

	if (len > m_ibuf->GetCapacity())
	{
		RETURN_IF_ERROR(m_ibuf->Expand(len));
	}
	m_istr = m_ibuf->GetStorage();
	m_istr[_len] = 0;

	return OpStatus::OK;
}

OP_STATUS OTHandler::Process(MDE_FONT* font, const uni_char* str, const UINT32 len)
{
	m_len = len;
	if (len == 0)
		return OpStatus::OK;

	// copy string to internal buffer
	OP_ASSERT(m_len <= m_ubuf->GetCapacity());
	op_memcpy(m_ustr, str, m_len*sizeof(*m_ustr));

	// process string
	OP_STATUS status = OpStatus::OK;
	UINT32 s = 0, sl;
	while (s < m_len)
	{
		status = ProcessGeneric(font, s, sl);
		if (OpStatus::IsError(status))
			break;
		s += sl;
	}

	return status;
}

OP_STATUS OTHandler::ProcessGeneric(MDE_FONT* font, const UINT32 s, UINT32& l)
{
	OP_ASSERT(s < m_len);

	// indic text is special
	IndicGlyphClass::Script script = g_indic_scripts->GetScript(m_ustr[s]);
	if (script != IndicGlyphClass::UNKNOWN)
		return ProcessIndic(script, font, s, l);

	// process rest of string at once
	m_base = s;
	l = m_len - s;

	// translate to char indices
	OP_ASSERT(s+l <= m_len);
	for (UINT32 i = 0; i < l; ++i)
		m_istr[s+i] = MDF_GetCharIndex(font, m_ustr[s+i]);

	const UINT8* LanguageSystem = GetLanguageSystem(m_ScriptList, m_gsub_end, s, l);
	if (!LanguageSystem)
		return OpStatus::OK;

	return ApplyGenericFeatures(LanguageSystem, s, l);
}
OP_STATUS OTHandler::ApplyGenericFeatures(const UINT8* LanguageSystem,
										  const UINT32 s, UINT32& l)
{
	OP_ASSERT(s+l <= m_len);
	OP_ASSERT(LanguageSystem);
	if (!LanguageSystem)
		return OpStatus::ERR;

	if (!m_FeatureList)
		return OpStatus::OK;

	if (LanguageSystem+6 > m_gsub_end)
	{
		OP_ASSERT(!"out of bounds when applying language system substitutions");
		return OpStatus::OK; // error?
	}

	OP_ASSERT(!MDF_GetU16(LanguageSystem));
	// UINT16 requiredFeatureIndex = MDF_GetU16(LanguageSystem+2); // what's this for?
	OT_ListIt it(LanguageSystem, m_gsub_end, 6, 2);
	if (!it.Init(4))
		return OpStatus::OK; // error?

	while (it.Next())
	{
		UINT16 featureIndex = MDF_GetU16(it.Struct());
		if (featureIndex < m_FeatureCount)
		{
#if 0
			// DEBUG
			const UINT8* tag = m_FeatureList + 2 + 6*featureIndex;
			fprintf(stderr, "* applying tag %c%c%c%c\n", tag[0],tag[1],tag[2],tag[3]);
#endif // 0

			UINT32 pos = s, len = l;
			while (pos < s + l)
			{
				UINT32 clen = len;
				if (ApplyGSUBFeature(GetTable(m_FeatureList, 2 + 6*featureIndex + 4), pos, len))
				{
					if (len != clen)
						l += len-clen;
				}
				else
				{
					if (OpStatus::IsError(m_status))
						return m_status;
					++pos;
					--len;
				}
			}
		}
	}
	return m_status;
}
OP_STATUS OTHandler::ProcessIndic(const IndicGlyphClass::Script script,
								  MDE_FONT* font, const UINT32 s, UINT32& l)
{
	OP_ASSERT(s < m_len);
	OP_ASSERT(script != IndicGlyphClass::UNKNOWN);
	m_has_reph = FALSE;
	m_base = s;
	if (indic_find_syllable(script, m_ustr+s, m_len-s, l))
	{
		UINT32 base;
		BOOL found_base = indic_find_base(script, m_ustr+s, l, base);
		if (found_base)
		{
			indic_reorder_syllable(script, m_ustr+s, l, base, m_has_reph);
			m_base = base + s;
		}
		else
		{
			OP_ASSERT(!"we should always be able to find a base if we found a syllable");
			l = 1;
		}
	}
	else
	{
		l = 1;
	}

	// translate to char indices
	OP_ASSERT(s+l <= m_len);
	for (UINT32 i = 0; i < l; ++i)
		m_istr[s+i] = MDF_GetCharIndex(font, m_ustr[s+i]);

	return ApplyIndicFeatures(s, l);
}
OP_STATUS OTHandler::ApplyIndicFeatures(const UINT32 s, UINT32& l)
{
	// ranges are:
	// * 0 - entire syllable
	// * 1 - pre-base
	// * 2 - post-base

	const UINT32 FeatureTags[] =
		{
			// 1. Language forms
			OT_MAKE_TAG("nukt"), 0,
			OT_MAKE_TAG("akhn"), 0,
			OT_MAKE_TAG("rphf"), 0,
			OT_MAKE_TAG("blwf"), 0,
			OT_MAKE_TAG("half"), 1,
			OT_MAKE_TAG("vatu"), 0,
			// 2. Conjuncts and typographical forms
			OT_MAKE_TAG("pres"), 1,
			OT_MAKE_TAG("abvs"), 2,
			OT_MAKE_TAG("blws"), 2,
			OT_MAKE_TAG("psts"), 2,
		};
	const UINT32 numFeatures = sizeof(FeatureTags)/(2*sizeof(*FeatureTags));
	const UINT32 rephTag = OT_MAKE_TAG("rphf");

	// apply ligatures
	for (UINT32 i = 0; i < numFeatures; ++i)
	{
		// only apply reph if needed
 		if (FeatureTags[2*i] == rephTag && !m_has_reph)
 			continue;

		UINT32 range = FeatureTags[2*i + 1];
		UINT32 p = (range == 2 ? m_base+1 : 0) + s;
		UINT32 range_end = range == 1 ? m_base : (l + s);
		while (p < range_end)
		{
			UINT32 dl = range_end-p;
			UINT32 sl = dl;
			if (ApplyGSUBFeature(FeatureTags[2*i], p, sl))
			{
				l -= (INT32)dl - (INT32)sl;
				range_end = range == 1 ? m_base : (l + s); // update, since both base and l can change
			}
			else
			{
				if (OpStatus::IsError(m_status))
					return m_status;
				++p;
			}
		}
	}

	// 3. Halant form
	UINT32 sl = l-(m_base-s);
	UINT32 dl = sl;
	ApplyGSUBFeature(OT_MAKE_TAG("haln"), m_base, sl);
	l -= dl - sl;

	return m_status;
}

BOOL OTHandler::ApplyFeature(LOOKUP_TYPE lookupType, const UINT8* Feature)
{
	OP_ASSERT(!m_LookupFunc);
	OP_ASSERT(!m_LookupList);
	OP_ASSERT(!m_LookupCount);
	OP_ASSERT(!m_table_end);

	switch (lookupType)
	{
	case TYPE_GSUB:
		m_LookupFunc = &OTHandler::ApplyGSUBLookup;
		m_LookupList = GetLookupList(m_gsub);
		m_table_end = m_gsub_end;
		break;
	default:
		OP_ASSERT(!"unknown lookup type");
		return FALSE;
	}
	m_LookupCount = MDF_GetU16(m_LookupList);

	BOOL result = TRUE;

	// not using RETURN_IF_OUT_OF_BOUNDS_C since we want to restore the pointers
	if (Feature+4 > m_table_end)
	{
		OP_ASSERT(!"table out of bounds");
		result = FALSE;
	}
	else
	{
		result = MDF_GetU16(Feature) == 0; // sanity check
		OP_ASSERT(result);
	}

	if (result)
	{
		result = FALSE;

		OT_ListIt it(Feature, m_table_end, 4, 2);
		if (!it.Init(2))
			result = FALSE;
		while(it.Next())
		{
			const UINT8* t = it.Struct();
			const UINT16 lookupListIndex = MDF_GetU16(t);

			// apply the lookup
			if (ApplyLookup(lookupListIndex))
			{
				// FIXME: should we break here?
				// always? sometimes?
				// what about substitutions of different lengths?
				result = TRUE;
				break;
			}
		}
	}

	m_LookupList = 0;
	m_LookupCount = 0;
	m_LookupFunc = 0;
	m_table_end = 0;
	return result;
}

BOOL OTHandler::ApplyLookup(const UINT16 lookupIndex)
{
	// this function should not be called without m_LookupList being set to point to the
	// lookup list for the relevant table and m_ApplyLookup pointing to the relevant
	// function to apply the lookups
	OP_ASSERT(m_LookupList && m_LookupFunc);

    if (lookupIndex >= m_LookupCount)
    {
		OP_ASSERT(!"lookup index out of bounds");
		return FALSE;
    }

    const UINT8* Lookup = GetTable(m_LookupList, 2 + 2*lookupIndex);

	RETURN_IF_OUT_OF_BOUNDS_C(Lookup);

    return (*this.*m_LookupFunc)(Lookup);
}

BOOL OTHandler::ApplyGSUBFeature(const UINT32 featureTag,
								  UINT32& pos, UINT32& slen)
{
    OP_ASSERT(pos+slen <= m_len);

    if (!featureTag)
    {
		OP_ASSERT(!"empty tag passed");
		return FALSE;
    }

    const UINT8* Feature = FindTable(m_FeatureList, m_gsub_end, featureTag);
	return ApplyGSUBFeature(Feature, pos, slen);
}
BOOL OTHandler::ApplyGSUBFeature(const UINT8* Feature,
								  UINT32& pos, UINT32& slen)
{
    OP_ASSERT(pos+slen <= m_len);

    if (Feature)
    {
		m_pos = pos;
		m_slen = slen;
		if (ApplyFeature(TYPE_GSUB, Feature))
		{
			slen = m_slen;
			pos = m_pos + m_advance;
			return TRUE;
		}
    }
    return FALSE;
}

OTHandler::SFNTSubtableFunc OTHandler::GetSubtableFunc(const UINT16 type)
{
	switch (type)
	{
	case 1: return &OTHandler::ApplySingleSubst;               break;
	case 2: return &OTHandler::ApplyMultiSubst;                break;
	case 3: return &OTHandler::ApplyAlternateSubst;            break;
	case 4: return &OTHandler::ApplyLigSubst;                  break;
	case 5: return &OTHandler::ApplyContextSubst;              break;
	case 6: return &OTHandler::ApplyChainingContextSubst;      break;
	case 8: return &OTHandler::ApplyReverseContextSingleSubst; break;

	case 7: // must be dealt with separately
		OP_ASSERT(!"Extension Substitutions must be dealt with separately");
		return NULL;
		break;

	default:
		OP_ASSERT(!"unsupported subtable format");
		return NULL;
	}
}

BOOL OTHandler::ApplyExtensionSubstLookup(const UINT8* Lookup)
{
	OP_ASSERT(MDF_GetU16(Lookup) == 7); // Extension Substitution

	RETURN_IF_OUT_OF_BOUNDS_C(Lookup + 8);

	const UINT16 flags = MDF_GetU16(Lookup+2);
	const UINT16 count  = MDF_GetU16(Lookup+4);
	const UINT16 e_offs = MDF_GetU16(Lookup+6);

	// check count - if i understand the format correctly it should always be one
	if (count != 1)
	{
		OP_ASSERT(!"possible misinterpretation of Extention Substitution format");
		return FALSE;
	}

	// get pointer to Extention Substitution table
	const UINT8* ExtSubstTable = Lookup+e_offs;
	RETURN_IF_OUT_OF_BOUNDS_C(ExtSubstTable + 8);

	// find "real" type
	const UINT16 format = MDF_GetU16(ExtSubstTable+0);
	const UINT16 type   = MDF_GetU16(ExtSubstTable+2);
	const UINT32 r_offs = MDF_GetU32(ExtSubstTable+4);

	if (format != 1)
		return FALSE;

	SFNTSubtableFunc subtable_func = GetSubtableFunc(type);
	if (!subtable_func)
		return FALSE;

	const UINT8* Subtable = ExtSubstTable + r_offs;
	return (*this.*subtable_func)(Subtable, flags);
}

BOOL OTHandler::ApplyGSUBLookup(const UINT8* Lookup)
{
	RETURN_IF_OUT_OF_BOUNDS_C(Lookup + 6);

	const UINT16 type  = MDF_GetU16(Lookup);
	const UINT16 flags = MDF_GetU16(Lookup+2);

	// Extension Substitution - needed when size of subtable exceeds
	// 16-bit limits and is handled separately. gist: find "real" type
	// and offset to subtables.
	if (type == 7)
		return ApplyExtensionSubstLookup(Lookup);

	OT_ListIt it(Lookup, m_table_end, 6);
	if (!it.Init(4))
		return FALSE;

	// all subtables have the same type, so using function pointer to the relevant function
	SFNTSubtableFunc subtable_func = GetSubtableFunc(type);
	if (!subtable_func)
		return FALSE;

	while (it.Next())
	{
		const UINT8* Subtable;
		if (!it.Table(Subtable))
			return FALSE;
		if ((*this.*subtable_func)(Subtable, flags))
			return TRUE;
	}

	return FALSE;

}

BOOL OTHandler::ApplySingleSubst(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 6);

    UINT16 format = MDF_GetU16(Subtable);
    if (format < 1 || format > 2)
    {
		OP_ASSERT(!"unknown single substitution format");
		return FALSE;
    }

    const UINT8* Coverage = GetTable(Subtable, 2);
    UINT16 ch;
    UINT16 coverageIndex;
    if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos], coverageIndex))
		return FALSE;

    if (format == 1)
    {
		INT16 delta = (INT16)MDF_GetU16(Subtable + 4);
		ch = m_istr[m_pos]+delta;
    }
    else
    {
		const UINT16 glyphCount = MDF_GetU16(Subtable + 4);
		if (coverageIndex >= glyphCount)
		{
			OP_ASSERT(!"coverage index out of range");
			return FALSE;
		}
		RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 6 + 2*glyphCount); // could test using coverageIndex
		const UINT16 g = MDF_GetU16(Subtable + 6 + 2*coverageIndex);
		ch = g;
    }

    m_advance = 1;
    SetChar(ch, m_pos);
    return TRUE;
}
BOOL OTHandler::ApplyMultiSubst(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 6);

    UINT16 format = MDF_GetU16(Subtable);

    if (format != 1)
    {
		OP_ASSERT(!"unknown multiple substitution format");
		return FALSE;
    }

    const UINT8* Coverage = GetTable(Subtable, 2);
    UINT16 coverageIndex;
    if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos], coverageIndex))
		return FALSE;

    const UINT16 sequenceCount = MDF_GetU16(Subtable + 4);
    if (coverageIndex >= sequenceCount)
    {
		OP_ASSERT(!"coverage index out of range");
		return FALSE;
    }
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 6 + 2*sequenceCount); // could test on coverageIndex
    const UINT8* Sequence = GetTable(Subtable, 6 + 2*coverageIndex);
	RETURN_IF_OUT_OF_BOUNDS_C(Sequence + 2);
    const UINT16 glyphCount = MDF_GetU16(Sequence);
    if (!glyphCount)
    {
		OP_ASSERT(!"glyph count is 0");
		return FALSE;
    }

    const UINT32 delta = glyphCount - 1;

    // grow the strings if necessary
    UINT32 needed_len = m_len + delta;
    m_status = Grow(needed_len);
    if (OpStatus::IsError(m_status))
		return FALSE;

    UINT32 a = m_pos+1; // first char after target
    // make space for substitution
    if (a < m_len)
    {
		op_memmove(m_ustr+m_pos+glyphCount, m_ustr+m_pos+1, (m_len-a)*sizeof(*m_ustr));
		op_memmove(m_istr+m_pos+glyphCount, m_istr+m_pos+1, (m_len-a)*sizeof(*m_istr));
    }

	OT_ListIt it(Sequence, m_table_end, 2, 2);
	if (!it.Init(0))
		return FALSE;
	while (it.Next())
	{
		UINT16 ch = MDF_GetU16(it.Struct());
		SetChar(ch, m_pos+it.Current());
	}

    m_len += delta;
    m_slen += delta;
    if (m_base < m_pos)
		m_base += delta;
    m_advance = glyphCount;
    return TRUE;
}
BOOL OTHandler::ApplyAlternateSubst(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+6);

    // no point in doing anything unless we know what we want
    return FALSE;

#if 0 // this code applies an alernate substitution, but we don't know what substitution to do
    const UINT16 format = MDF_GetU16(Subtable);
    if (format != 1)
    {
		OP_ASSERT(!"unknown alternate substitution format");
		return FALSE;
    }

    const UINT8* Coverage = GetTable(Subtable, 2);
    UINT16 coverageIndex;
    if (!Coverage(Coverage, m_istr[pos], coverageIndex))
		return FALSE;

    UINT16 alternateSetCount = MDF_GetU16(Subtable + 4);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 6 + 2*alternateSetCount);
    const UINT8* AlternateSet = GetTable(Subtable, 6 + 2*coverageIndex);
	RETURN_IF_OUT_OF_BOUNDS_C(AlternateSet + 2);

    const UINT16 glyphCount = MDF_GetU16(AlternateSet);
	RETURN_IF_OUT_OF_BOUNDS_C(AlternateSet + 2 + 2*glyphCount);
    // wantedAlt is the requested alternative - but who is going to call this?
    m_istr [pos] = MDF_GetU16(AlternateSet + 2 + 2*wantedAlt);
    m_advance = 1;
    return TRUE;
#endif
}
BOOL OTHandler::ApplyLigSubst(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+6);

    UINT16 format = MDF_GetU16(Subtable);
    if (format != 1)
    {
		OP_ASSERT(!"unknown ligature substitution format");
		return FALSE;
    }

    UINT16 coverageIndex;
    const UINT8* Coverage = GetTable(Subtable, 2);
    if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos], coverageIndex))
		return FALSE;

    const UINT16 ligSetCount = MDF_GetU16(Subtable+4);
    if (coverageIndex >= ligSetCount)
    {
		OP_ASSERT(!"got converage index greater than number of ligatures: bailing");
		return FALSE;
    }
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 6 + 2*ligSetCount);

    const UINT8* LigatureSet = GetTable(Subtable, 6 + 2*coverageIndex);

	OT_ListIt it(LigatureSet, m_table_end, 2);
	if (!it.Init(0))
		return FALSE;
	while (it.Next())
	{
		const UINT8* Ligature;
		if (!it.Table(Ligature))
			return FALSE;
		const UINT16 lig = MDF_GetU16(Ligature);
		const UINT16 n = MDF_GetU16(Ligature+2);
		RETURN_IF_OUT_OF_BOUNDS_C(Ligature + 4 + 2*(n-1));
		if (n > m_slen)
			continue;
		int c;
		for (c = 0; c < n-1; ++c)
			if (m_istr[m_pos+c+1] != MDF_GetU16(Ligature + 4 + 2*c))
				break;
		if (c < n-1) // partial match
			continue;

		// MATCH

		// apply the ligature
		ApplySubst(lig, n);
		m_advance = 1;
		return TRUE;
    }
    return FALSE;
}
BOOL OTHandler::ApplyContextSubst(const UINT8* Subtable, const UINT16 flags)
{
    return ApplyContextSubst(Subtable, flags, FALSE);
}
BOOL OTHandler::ApplyChainingContextSubst(const UINT8* Subtable, const UINT16 flags)
{
    return ApplyContextSubst(Subtable, flags, TRUE);
}
BOOL OTHandler::ApplyReverseContextSingleSubst(const UINT8* Subtable, const UINT16 flags)
{
	OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+6);

    const UINT16 format = MDF_GetU16(Subtable);
    if (format != 1)
    {
		OP_ASSERT(!"unknown reverse chaining contextual single substitution format");
		return FALSE;
    }

    BOOL match = FALSE;
    const UINT8* Coverage = GetTable(Subtable, 2);

	// backtrack
	const UINT16 backtrackCount = MDF_GetU16(Subtable+4);

    // lookahead
    const UINT32 lookaheadOffset = 6 + 2*backtrackCount;
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + lookaheadOffset + 2);
    const UINT16 lookaheadCount = MDF_GetU16(Subtable + lookaheadOffset);

    // subst
    const UINT32 substOffset = lookaheadOffset + 2 + 2*lookaheadCount;
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + substOffset + 2);
    const UINT16 glyphCount = MDF_GetU16(Subtable+substOffset);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + substOffset + 2 + 2*glyphCount);
    for (INT32 i = glyphCount-1; i >= 0; --i) // apply reversed
    {
		UINT16 coverageIndex;
		if (!GetCoverage(Coverage, m_table_end, m_istr[i], coverageIndex) || coverageIndex > glyphCount)
			continue;
		
		// backtrack
		OT_ListIt back_it(Subtable, m_table_end, 6);
		if (!back_it.Init(4))
			return FALSE;
		if (back_it.Count() > m_pos+i)
			continue; // not enough backtrack
		while (back_it.Next())
		{
			const UINT8* Coverage;
			if (!back_it.Table(Coverage))
				return FALSE;
			UINT16 ci;
			if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos+i-1-back_it.Count()], ci))
				break; // backtrack mismatch
		}
		if (back_it.Current() != back_it.Count())
			continue;

		// lookahead
		if (m_pos+i+1+lookaheadCount > m_len)
			continue; // not enough lookahead

		OT_ListIt it(Subtable, m_table_end, lookaheadOffset+2);
		if (!it.Init(lookaheadOffset))
			return FALSE;
		while (it.Next())
		{
			const UINT8* Coverage;
			if (!it.Table(Coverage))
				return FALSE;
			UINT16 ci;
			if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos+i+1+it.Current()], ci))
				break; // lookahead mismatch
		}
		if (it.Current() != it.Count())
			continue;

		// apply
		match = TRUE;
		SetChar(MDF_GetU16(Subtable+substOffset+2+coverageIndex), m_pos+i);
    }

    if (match)
    {
		m_advance = m_slen;
		return TRUE;
    }
    return FALSE;
}

BOOL OTHandler::ApplyContextSubst(const UINT8* Subtable, const UINT16 flags, BOOL chaining)
{
	OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+6);

    const UINT16 format = MDF_GetU16(Subtable);

    UINT16 coverageIndex = 0;
    if (format == 1 || format == 2)
    {
		const UINT8* Coverage = GetTable(Subtable, 2);
		if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos], coverageIndex))
			return FALSE;
    }

    switch (format)
    {
    case 1:
		return chaining ?
			ApplyChainContextSubst1(Subtable, flags, coverageIndex) :
			ApplyContextSubst1(Subtable, flags, coverageIndex);
		break;
    case 2:
		return chaining ?
			ApplyChainContextSubst2(Subtable, flags) :
			ApplyContextSubst2(Subtable, flags);
		break;
    case 3:
		return chaining ? 
			ApplyChainContextSubst3(Subtable, flags) :
			ApplyContextSubst3(Subtable, flags);
		break;	
    default:
		OP_ASSERT(!"unsupported contextual substitution format");
		return FALSE;
		break;
    }
}
BOOL OTHandler::ApplyContextSubst1(const UINT8* Subtable, const UINT16 flags, const UINT16 coverageIndex)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+6);
    OP_ASSERT(MDF_GetU16(Subtable) == 1);

    const UINT16 subRuleSetCount = MDF_GetU16(Subtable+4);
    if (coverageIndex >= subRuleSetCount)
    {
		OP_ASSERT(!"coverage index out of bounds");
		return FALSE;
    }
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 6 + 2*subRuleSetCount);

    // get the subrule set for the coverage index
    const UINT8* SubRuleSet = GetTable(Subtable, 6 + 2*coverageIndex);

	OT_ListIt it(SubRuleSet, m_table_end, 2);
	if (!it.Init(0))
		return FALSE;
	while (it.Next())
	{
		const UINT8* SubRule;
		if (!it.Table(SubRule))
			return FALSE;
		RETURN_IF_OUT_OF_BOUNDS_C(SubRule + 4);
		const UINT16 glyphCount = MDF_GetU16(SubRule);
		const UINT16 substCount = MDF_GetU16(SubRule + 2);
		OP_ASSERT(glyphCount);
		RETURN_IF_OUT_OF_BOUNDS_C(SubRule + 4 + 2*(glyphCount-1) + 2*substCount);

		// cannot possibly match subrule
		if (glyphCount > m_slen || glyphCount < 1)
			continue;

		// compare string with subrule
		UINT32 j;
		for (j = 0; j < (UINT32)(glyphCount-1); ++j)
			if (MDF_GetU16(SubRule + 4 + 2*j) != m_istr[m_pos+j+1])
				break;
		// mismatch
		if (j < (UINT32)(glyphCount-1))
			continue;

		// apply all substitutions for this subrule
		m_advance = glyphCount;
		return ApplyContextSubstitutions(SubRule + 4 + 2*(glyphCount-1), substCount);
    }

    // no matching subrule found
    return FALSE;
}
BOOL OTHandler::ApplyContextSubst2(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+8);
    OP_ASSERT(MDF_GetU16(Subtable) == 2);

    const UINT8* ClassDef = GetTable(Subtable, 4);
    const UINT16 subClassSetCount = MDF_GetU16(Subtable+6);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 8 + 2*subClassSetCount);

    // character class of first character
    const UINT16 firstClass = GetClass(ClassDef, m_table_end, m_istr[m_pos]);
    if (!firstClass || firstClass > subClassSetCount) // character not in any class, or out of range
		return FALSE;

    // SubClassSet for the class of the first character
    const UINT8* SubClassSet = GetTable(Subtable, 8 + 2*firstClass);

    if (SubClassSet == Subtable) // offset == 0 - no substitution starts with a char from this class
		return FALSE;

	RETURN_IF_OUT_OF_BOUNDS_C(SubClassSet+2);

    // find subclass to apply
	OT_ListIt it(SubClassSet, m_table_end, 2);
	if (!it.Init(0))
		return FALSE;
	while (it.Next())
	{
		const UINT8* SubClassRule;
		if (!it.Table(SubClassRule))
			return FALSE;
		RETURN_IF_OUT_OF_BOUNDS_C(SubClassRule+4);
		const UINT16 glyphCount = MDF_GetU16(SubClassRule);

		// cannot possibly match
		if (glyphCount > m_slen)
			continue;

		RETURN_IF_OUT_OF_BOUNDS_C(SubClassRule + 4 + 2*glyphCount);
		// compare string with subrule
		int j;
		for (j = 0; j < glyphCount-1; ++j)
		{
			UINT16 wanted = MDF_GetU16(SubClassRule + 4 + 2*j);
			if (!wanted || wanted > subClassSetCount)
			{
				OP_ASSERT(!"wanted class out of bounds");
				return FALSE;
			}
			if (wanted != GetClass(ClassDef, m_table_end, m_istr[m_pos+j+1]))
				break;
		}
		// mismatch
		if (j != glyphCount-1)
			continue;

		// apply all substitutions for this subrule
		const UINT16 substCount = MDF_GetU16(SubClassRule+2);
		m_advance = glyphCount;
		return ApplyContextSubstitutions(SubClassRule + 4 + 2*(glyphCount-1), substCount);
    }

    // no matching subrule found
    return FALSE;
}
BOOL OTHandler::ApplyContextSubst3(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+6);
    OP_ASSERT(MDF_GetU16(Subtable) == 3);

	OT_ListIt it(Subtable, m_table_end, 6);
	if (!it.Init(2))
		return FALSE;
    // cannot possibly match
	if (m_slen < it.Count())
		return FALSE;
    // check sequence for coverage
	while (it.Next())
	{
		const UINT8* Coverage;
		if (!it.Table(Coverage))
			return FALSE;
		UINT16 ci;
		if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos+it.Current()], ci))
			return FALSE;
    }

    // apply all substitutions
    m_advance = it.Count();
    const UINT16 substCount = MDF_GetU16(Subtable + 4);
    return ApplyContextSubstitutions(Subtable+8, substCount);
}
BOOL OTHandler::ApplyChainContextSubst1(const UINT8* Subtable, const UINT16 flags, const UINT16 coverageIndex)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+6);
    OP_ASSERT(MDF_GetU16(Subtable) == 1);

    const UINT16 chainSubRuleSetCount = MDF_GetU16(Subtable+4);
    if (coverageIndex > chainSubRuleSetCount)
    {
		OP_ASSERT(!"coverage index out of bounds");
		return FALSE;
    }
    const UINT8* ChainSubRuleSet = GetTable(Subtable, 6 + 2*coverageIndex);

	OT_ListIt chain_it(ChainSubRuleSet, m_table_end, 2);
	if (!chain_it.Init(0))
		return FALSE;
	while (chain_it.Next())
	{
		const UINT8* ChainSubRule;
		if (!chain_it.Table(ChainSubRule))
			return FALSE;

		// backtrack
		OT_ListIt back_it(ChainSubRule, m_table_end, 2, 2);
		if (!back_it.Init(0))
			return FALSE;
		if (back_it.Count() > m_pos)
			continue; // not enough backtrack
		while (back_it.Next())
			if (m_istr[m_pos-1-back_it.Current()] != MDF_GetU16(back_it.Struct()))
				break;
		if (back_it.Current() != back_it.Count())
			continue; // backtrack mismatch

		// input
		const UINT32 inputOffs = 2 + 2*back_it.Count();
		RETURN_IF_OUT_OF_BOUNDS_C(ChainSubRule + inputOffs + 2);
		const UINT16 inputGlyphCount = MDF_GetU16(ChainSubRule + inputOffs);
		if (inputGlyphCount < 1)
			return FALSE; // nothing to do (wtf?)
		if (inputGlyphCount > m_slen)
			continue; // not enough input
		RETURN_IF_OUT_OF_BOUNDS_C(ChainSubRule + inputOffs + 2 + 2*(inputGlyphCount-1));
		UINT32 j;
		for (j = 0; j < (UINT32)(inputGlyphCount-1); ++j)
			if (m_istr[m_pos+1+j] != MDF_GetU16(ChainSubRule + inputOffs + 2 + 2*j))
				break;
		if (j != (UINT32)(inputGlyphCount-1))
			continue; // input mismatch

		// lookahead
		const UINT32 lookaheadOffs = inputOffs + 2 + 2*(inputGlyphCount-1);
		OT_ListIt ahead_it(ChainSubRule, m_table_end, lookaheadOffs+2, 2);
		if (!ahead_it.Init(lookaheadOffs))
			return FALSE;
		if (m_pos + inputGlyphCount + ahead_it.Count() > m_len)
			continue; // not enough lookahead
		while (ahead_it.Next())
			if (m_istr[m_pos+inputGlyphCount+ahead_it.Current()] != MDF_GetU16(ahead_it.Struct()))
				break;
		if (ahead_it.Current() != ahead_it.Count())
			continue;


		// match found - apply
		m_advance = inputGlyphCount;
		const UINT32 substOffs = lookaheadOffs + 2 + 2*ahead_it.Count();
		return ApplyContextSubstitutions(ChainSubRule + substOffs + 2, MDF_GetU16(ChainSubRule + substOffs));
    }
    // no match
    return FALSE;
}
BOOL OTHandler::ApplyChainContextSubst2(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+12);
    OP_ASSERT(MDF_GetU16(Subtable) == 2);

    const UINT8* BacktrackClassDef = GetTable(Subtable, 4);
    const UINT8* InputClassDef = GetTable(Subtable, 6);
    const UINT8* LookaheadClassDef = GetTable(Subtable, 8);

    const UINT16 chainSubClassSetCount = MDF_GetU16(Subtable+10);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable + 12 + 2*chainSubClassSetCount);

    // character class of first character
    const UINT16 firstClass = GetClass(InputClassDef, m_table_end, m_istr[m_pos]);
    if (!firstClass || firstClass > chainSubClassSetCount) // character not in any class, or out of range
		return FALSE;

    // SubClassSet for the class of the first character
    const UINT8* ChainSubClassSet = GetTable(Subtable, 12 + 2*firstClass);

    if (ChainSubClassSet == Subtable) // offset == 0 - no substitution starts with a char from this class
		return FALSE;

    // find subclass to apply
	OT_ListIt it(ChainSubClassSet, m_table_end, 2);
	if (!it.Init(0))
		return FALSE;
	while (it.Next())
	{
		const UINT8* ChainSubClassRule;
		if (!it.Table(ChainSubClassRule))
			return FALSE;

		// backtrack
		OT_ListIt back_it(ChainSubClassRule, m_table_end, 2, 2);
		if (!back_it.Init(0))
			return FALSE;
		if (back_it.Count() > m_pos)
			continue; // not enough backtrack
		while (back_it.Next())
			if (GetClass(BacktrackClassDef, m_table_end, m_istr[m_pos-1-back_it.Current()] != MDF_GetU16(back_it.Struct())))
				break;
		if (back_it.Current() != back_it.Count())
			continue; // backtrack mismatch

		// input
		const UINT32 inputOffset = 2 + 2*back_it.Count();
		RETURN_IF_OUT_OF_BOUNDS_C(ChainSubClassRule + inputOffset + 2);
		const UINT16 inputCount = MDF_GetU16(ChainSubClassRule + inputOffset);
		if (inputCount > m_slen || !inputCount)
			continue; // not enough input
		UINT32 j;
		for (j = 0; j < (UINT32)(inputCount-1); ++j)
			if (GetClass(InputClassDef, m_table_end, m_istr[m_pos+1+j]) != MDF_GetU16(ChainSubClassRule + inputOffset + 2 + 2*j))
				break;
		if (j != (UINT32)(inputCount-1))
			continue; // input mismatch

		// lookahead
		const UINT32 lookaheadOffset = inputOffset + 2 + 2*(inputCount-1);
		OT_ListIt ahead_it(ChainSubClassRule, m_table_end, lookaheadOffset+2, 2);
		if (!ahead_it.Init(lookaheadOffset))
			return FALSE;
		if (m_pos + m_slen + ahead_it.Count() > m_len)
			continue; // not enough lookahead
		while (ahead_it.Next())
			if (GetClass(LookaheadClassDef, m_table_end, m_istr[m_pos+m_slen+ahead_it.Current()] != MDF_GetU16(ahead_it.Struct())))
				break;
		if (ahead_it.Current() != ahead_it.Count())
			continue; // input mismatch

		// apply all substitutions for this subrule
		const UINT32 substOffset = lookaheadOffset + 2 + 2*ahead_it.Count();
		m_advance = inputCount;
		return ApplyContextSubstitutions(ChainSubClassRule+substOffset+2, MDF_GetU16(ChainSubClassRule + substOffset));
    }

    // no matching subrule found
    return FALSE;
}
BOOL OTHandler::ApplyChainContextSubst3(const UINT8* Subtable, const UINT16 flags)
{
    OP_ASSERT(Subtable);
	RETURN_IF_OUT_OF_BOUNDS_C(Subtable+4);
    OP_ASSERT(MDF_GetU16(Subtable) == 3);

    // backtrack
	OT_ListIt back_it(Subtable, m_table_end, 4);
	if (!back_it.Init(2))
		return FALSE;
	if (back_it.Count() > m_pos)
		return FALSE; // not enough backtrack
	while (back_it.Next())
	{
		const UINT8* Coverage;
		if (!back_it.Table(Coverage))
			return FALSE;
		UINT16 ci;
		if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos-1-back_it.Current()], ci))
			return FALSE; // backtrack mismatch
	}

    // input
	const UINT32 inputOffset = 4 + 2*back_it.Count();
	OT_ListIt input_it(Subtable, m_table_end, inputOffset+2);
	if (!input_it.Init(inputOffset))
		return FALSE;
	if (m_slen < input_it.Count())
		return FALSE; // not enough input
	while (input_it.Next())
	{
		const UINT8* Coverage;
		if (!input_it.Table(Coverage))
			return FALSE;
		UINT16 ci;
		if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos+input_it.Current()], ci))
			return FALSE; // input mismatch
    }

    const UINT32 lookaheadOffset = inputOffset + 2 + 2*input_it.Count();
	OT_ListIt ahead_it(Subtable, m_table_end, lookaheadOffset+2);
	if (!ahead_it.Init(lookaheadOffset))
		return FALSE;
	if (m_pos+m_slen+ahead_it.Count() > m_len)
		return FALSE; // not enough lookahead
	while (ahead_it.Next())
	{
		const UINT8* Coverage;
		if (!ahead_it.Table(Coverage))
			return FALSE;
		UINT16 ci;
		if (!GetCoverage(Coverage, m_table_end, m_istr[m_pos+m_slen+ahead_it.Current()], ci))
			return FALSE; // lookahead mismatch
    }

    // apply all substitutions
    UINT32 substOffset = lookaheadOffset + 2 + 2*ahead_it.Count();
    m_advance = input_it.Count();
    return ApplyContextSubstitutions(Subtable+substOffset+2, MDF_GetU16(Subtable+substOffset));
}

BOOL OTHandler::ApplyContextSubstitutions(const UINT8* SubstLookupRecord, const UINT16 substCount)
{
    OP_ASSERT(substCount);
	RETURN_IF_OUT_OF_BOUNDS_C(SubstLookupRecord + 4*substCount);
    for (int i = 0; i < substCount; ++i)
    {
		const UINT16 sequenceIndex = MDF_GetU16(SubstLookupRecord);
		if (sequenceIndex >= m_slen)
		{
			OP_ASSERT(!"sequence index out of bounds");
			return FALSE;
		}
		UINT32 old_pos = m_pos;
		m_pos += sequenceIndex;

		// apply
		const UINT16 lookupIndex = MDF_GetU16(SubstLookupRecord+2);
		BOOL r = ApplyLookup(lookupIndex);
		
		m_pos = old_pos;

		if (i && !r)
		{
			OP_ASSERT(!"a substitution failed - string was partially converted");
			return FALSE;
		}
			
		SubstLookupRecord += 4;
    }
    return TRUE;
}

void OTHandler::SetChar(const UINT16 ch, const UINT32 idx)
{
    UINT16 uch = 0;
	OP_ASSERT(idx < m_len);
	OP_ASSERT(idx < m_ibuf->GetCapacity());
	OP_ASSERT(idx < m_ubuf->GetCapacity());
    m_istr[idx] = ch;
    m_ustr[idx] = uch;
}

void OTHandler::ApplySubst(const UINT16 ch, const UINT16 l)
{
    OP_ASSERT(m_istr);
    OP_ASSERT(m_ustr);
    OP_ASSERT(m_pos+l <= m_len);

    SetChar(ch, m_pos);

    if (l > 1)
    {
		UINT32 a = m_pos+l; // first char after ligature string

		// move everything after the ligature
		if (a < m_len)
		{
			op_memmove(m_istr+m_pos+1, m_istr+a, (m_len-a)*sizeof(*m_istr));
			op_memmove(m_ustr+m_pos+1, m_ustr+a, (m_len-a)*sizeof(*m_ustr));
		}

		// update base
		if (m_base > m_pos)
		{
			// base is after the ligature
			if (m_base >= a)
				m_base -= l-1;
			// base was part of the ligature string, make ligature base
			else
				m_base = m_pos;
		}

		// reduce string length
		m_len -= l-1;
		m_slen -= l-1;
    }
}

// static
BOOL OTHandler::GetOTBuffers(class TempBuffer** u, class TempBuffer** i)
{
	if ( g_opentype_buflock )
		return FALSE;
	g_opentype_buflock = TRUE;
	*u = g_opentype_ubuf;
	*i = g_opentype_ibuf;
	return TRUE;
}

// static
void OTHandler::ReleaseOTBuffers()
{
	g_opentype_buflock = FALSE;
}

#endif // MDF_OPENTYPE_SUPPORT
#endif // MDEFONT_MODULE
