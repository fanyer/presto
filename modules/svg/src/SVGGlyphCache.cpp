/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/svgpch.h"

#include "modules/svg/src/SVGGlyphCache.h"
#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGFontImpl.h"

#ifdef FONTSWITCHING
class SVGUCRangeChecker
{
public:
	SVGUCRangeChecker(UnicodePoint uc)
	{
		int	block;
		styleManager->GetUnicodeBlockInfo(uc, block, m_block_low, m_block_high);
	}

	BOOL InRange(UnicodePoint low, UnicodePoint high) const
	{
		return m_block_low >= low && m_block_high <= high;
	}

private:
	UnicodePoint m_block_low;
	UnicodePoint m_block_high;
};
#else
class SVGUCRangeChecker
{
public:
	SVGUCRangeChecker(UnicodePoint uc) : m_ch(uc) {}

	BOOL InRange(UnicodePoint low, UnicodePoint high) const
	{
		return m_ch >= low && m_ch <= high;
	}

private:
	UnicodePoint m_ch;
};
#endif // FONTSWITCHING

static BOOL IsNotFullWidthCharacter(UnicodePoint ch)
{
	SVGUCRangeChecker checker(ch);

	// http://www.unicode.org/Public/UNIDATA/Blocks.txt

	if (checker.InRange(0x0000, 0x024F) || // basic latin, latin-1 supplement, latin-1 ext A, latin-1 ext B
		checker.InRange(0x0600, 0x06FF) || // arabic
		checker.InRange(0xFB50, 0xFDFF) || // arabic pres forms A
		checker.InRange(0xFE70, 0xFEFF)) // arabic pres forms B
	{
		return TRUE;
	}
	return FALSE;
}

static inline BOOL ContainsArabic(const uni_char* str, UINT32 len)
{
	for(UINT32 i = 0; i < len; i++)
	{
		if(IsArabic(str[i]))
			return TRUE;
	}

	return FALSE;
}

/* static */
OP_STATUS SVGGlyphCache::GetUncachedGlyphsForWord(OpFont* vectorfont, const uni_char* text, unsigned int len,
												  const SVGGlyphLookupContext& lookup,
												  OpVector<GlyphInfo>& glyphs)
{
	unsigned int last_str_pos = 0;
	for (unsigned char_index = 0; char_index < len;)
	{
		unsigned int next_last_str_pos = char_index;
		GlyphInfo* glyph_info = OP_NEW(GlyphInfo, ());
		if (!glyph_info)
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		SVGPath** path = glyph_info->GetPathPtr();
		SVGNumber& advance = glyph_info->GetAdvanceRef();

		BOOL is_horizontal = lookup.horizontal;
		BOOL fetch_as_opposite_orientation = FALSE;
		SVGNumber& rotation = glyph_info->GetRotationRef();
		if (lookup.horizontal || !lookup.is_auto)
		{
			rotation = SVGNumber(90) * lookup.orientation;
			fetch_as_opposite_orientation = (lookup.orientation == 1 /* 90 */ ||
											 lookup.orientation == 3 /* 270 */);
		}
		else /* vertical and 'auto' */
		{
			/**
			 * 1. Fullwidth ideographic and fullwidth Latin text will be set with a glyph-orientation
			 *    of 0-degrees. Ideographic punctuation and other ideographic characters having
			 *    alternate horizontal and vertical forms will use the vertical form of the glyph.
			 *
			 * 2. Text which is not fullwidth will be set with a glyph-orientation of 90-degrees.
			 *    For more information, see SVG specification 10.7.3 "Glyph orientation within a text run".
			 *
			 * Unicode spec reference for fullwidth characters:
			 *    http://www.unicode.org/Public/4.1.0/ucd/extracted/DerivedEastAsianWidth.txt
			 */
			if (IsNotFullWidthCharacter(text[char_index]))
			{
				rotation = 90;
				fetch_as_opposite_orientation = TRUE;
			}
		}

		if (fetch_as_opposite_orientation)
		{
			// this is a hack to get the correct advance value from GetOutline
			is_horizontal = !is_horizontal;
		}

		UINT8& numchars_ref = glyph_info->GetNumCharsRef();
		UINT32 numchars = char_index;

		OP_STATUS status = vectorfont->GetOutline(text, len, char_index, last_str_pos, is_horizontal,
			advance, path);
		
		// throw away space glyphpaths (if any), we shouldn't display them anyway (see CORE-9950)
		if(len == 1 && text[numchars] == ' ')
		{
			OP_DELETE(*path);
			*path = NULL;
		}

		numchars = char_index - numchars;
		numchars_ref = numchars;

		if (OpStatus::IsError(status) || OpStatus::IsError(status = glyphs.Add(glyph_info)))
		{
			// We have no glyph to paint.
			OP_DELETE(glyph_info);
			if (OpStatus::IsMemoryError(status) || OpStatus::ERR_NOT_SUPPORTED == status)
			{
				return status;
			}
			// The font didn't have that glyph. Should have used another font. :-(
			// Just skip the letter for now.
			char_index++;
		}
		OP_ASSERT(char_index != next_last_str_pos); // Or we have an infinite loop
		last_str_pos = next_last_str_pos;
	}

	return OpStatus::OK;
}

SVGGlyphCache::~SVGGlyphCache()
{
#ifdef _DEBUG
	OP_NEW_DBG("~SVGGlyphCache", "svg_glyphcache");
	OP_DBG(("Glyphcache hits: %d. misses: %d. Hitrate: %g.",
			cache_hits, cache_misses,
			cache_hits+cache_misses > 0 ? (float)cache_hits/(float)(cache_hits+cache_misses) : 0));
#endif // _DEBUG
	Clear();
}

OP_STATUS SVGGlyphCacheElement::Init(const OpVector<GlyphInfo>& glyphs, OpFont* font,
									 const uni_char* text, unsigned text_len,
									 const SVGGlyphLookupContext& props)
{
	RETURN_IF_ERROR(m_string.Set(text, text_len));

	unsigned int glyph_count = glyphs.GetCount();
	for (unsigned int i = 0; i < glyph_count; i++)
	{
		OP_ASSERT(glyphs.Get(i));

		OP_STATUS status = m_glyphs.Add(glyphs.Get(i));
		if (OpStatus::IsError(status))
		{
			if (i > 0)
			{
				m_glyphs.Remove(0, i);
			}
			return status;
		}
	}

	m_font = font;
	m_props = props;
	return OpStatus::OK;
}

SVGGlyphCacheElement* SVGGlyphCache::GetCacheElement(unsigned int alloc_hint,
                                                     BOOL needs_overfull_cache)
{
	SVGGlyphCacheElement* elm;
	if (cached_outlines.Cardinal() == SVGGLYPHCACHESIZE && !needs_overfull_cache)
	{
		// Cache is full - evict least recently used
		elm = cached_outlines.Last();
		elm->Out();

#ifdef SVG_DEBUG_GLYPHCACHE
		OP_NEW_DBG("GetCacheElement", "svg_glyphcache");
		OP_DBG((UNI_L("Evicting elm: font: %p. Glyph: %s(%d)."),
				elm->m_font,
				elm->m_string.CStr(),
				elm->m_string.CStr()[0]));
#endif // SVG_DEBUG_GLYPHCACHE

		elm->Reset();
	}
	else
	{
		elm = OP_NEW(SVGGlyphCacheElement, (alloc_hint));
	}
	return elm;
}

#ifdef _DEBUG
#define RECORD_CACHE_HIT do{ cache_hits++; }while(0)
#define RECORD_CACHE_MISS do{ cache_misses++; }while(0)
#else
#define RECORD_CACHE_HIT do{}while(0)
#define RECORD_CACHE_MISS do{}while(0)
#endif // _DEBUG

OP_STATUS SVGGlyphCache::GetGlyphsForWord(OpFont* vectorfont, const uni_char* text, unsigned int len,
										  const SVGGlyphLookupContext& props,
										  OpVector<GlyphInfo>& glyphs)
{
	OP_NEW_DBG("GetGlyphsForWord", "svg_glyphcache");
	BOOL cache_individual_glyphs = TRUE;

	// Trim the cache down to SVGGLYPHCACHESIZE elements in case it is overfull
	// from a previous word with more than SVGGLYPHCACHESIZE unique glyphs
	while (cached_outlines.Cardinal() > SVGGLYPHCACHESIZE)
	{
		SVGGlyphCacheElement* elm = cached_outlines.Last();
		elm->Out();
		OP_DELETE(elm);
	}

	OP_ASSERT(cached_outlines.Cardinal() <= SVGGLYPHCACHESIZE);

	BOOL is_svgfont = vectorfont->Type() == OpFontInfo::SVG_WEBFONT;
	if (is_svgfont)
	{
		cache_individual_glyphs = !ContainsArabic(text, len) &&
			!((SVGFontImpl*)vectorfont)->HasLigatures() &&
			!((SVGFontImpl*)vectorfont)->HasKerning();
	}

	if (cache_individual_glyphs)
	{
		unsigned int num_new_cache_entries = 0;

		for (unsigned int i = 0; i < len; ++i)
		{
			SVGGlyphCacheElement* elm = cached_outlines.First();

			while (elm)
			{
				if (elm->IsMatchingSingleGlyph(vectorfont, text[i], props))
					break;

				elm = elm->Suc();
			}

			if (elm)
			{
				RECORD_CACHE_HIT;

				elm->Out();
			}
			else
			{
				RECORD_CACHE_MISS;
				OP_DBG(("Font: %p. Missed: %d.",
						vectorfont, text[i]));

				// See docstring for GetCacheElements()
				++num_new_cache_entries;

				OpVector<GlyphInfo> one_glyph;
				RETURN_IF_ERROR(GetUncachedGlyphsForWord(vectorfont, &text[i], 1, props, one_glyph));

				elm = GetCacheElement(1, num_new_cache_entries > SVGGLYPHCACHESIZE);
				if (!elm || OpStatus::IsError(elm->Init(one_glyph, vectorfont, &text[i], 1, props)))
				{
					OP_ASSERT(!elm || !elm->InList());

					OP_DELETE(elm);
					one_glyph.DeleteAll();
					return OpStatus::ERR_NO_MEMORY;
				}
			}

			OP_ASSERT(elm);
			elm->IntoStart(&cached_outlines);
			RETURN_IF_ERROR(elm->AddGlyphsToVector(glyphs));
		}
	}
	else
	{
		SVGGlyphCacheElement* elm = cached_outlines.First();

		while (elm)
		{
			if (elm->IsMatching(vectorfont, text, len, props))
				break;

			elm = elm->Suc();
		}

		if (elm)
		{
			RECORD_CACHE_HIT;

			elm->Out();
			RETURN_IF_ERROR(elm->CopyGlyphsToVector(glyphs));
		}
		else
		{
			RECORD_CACHE_MISS;

			RETURN_IF_ERROR(GetUncachedGlyphsForWord(vectorfont, text, len, props, glyphs));

			elm = GetCacheElement(glyphs.GetCount(), FALSE);
			if (!elm || OpStatus::IsError(elm->Init(glyphs, vectorfont, text, len, props)))
			{
				OP_ASSERT(!elm || !elm->InList());

				OP_DELETE(elm);
				glyphs.DeleteAll();

				// If we hit OOM here, the operation still succeeded,
				// although we failed to cache the result
				return OpStatus::ERR_NO_MEMORY;
			}
		}

		OP_ASSERT(elm);
		elm->IntoStart(&cached_outlines);
	}
	return OpStatus::OK;
}

OP_STATUS SVGGlyphCacheElement::CopyGlyphsToVector(OpVector<GlyphInfo>& glyphs) const
{
	OP_ASSERT(glyphs.GetCount() == 0); // Or they'll leak
	return glyphs.DuplicateOf(m_glyphs);
}

OP_STATUS SVGGlyphCacheElement::AddGlyphsToVector(OpVector<GlyphInfo>& glyphs) const
{
	// Never add NULL GlyphInfo to the vector, that will crash
	if (m_glyphs.Get(0))
		return glyphs.Add(m_glyphs.Get(0));
	else
		return OpStatus::OK;
}

void SVGGlyphCache::HandleFontDeletion(OpFont* font_about_to_be_deleted)
{
	SVGGlyphCacheElement* elm = cached_outlines.First();
	while (elm)
	{
		SVGGlyphCacheElement* next_elm = elm->Suc();
		if (elm->Uses(font_about_to_be_deleted))
		{
			elm->Out();
			OP_DELETE(elm);
		}

		elm = next_elm;
	}
}

#endif // SVG_SUPPORT
