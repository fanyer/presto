/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

/** @file SVGGlyphCache.h
 *
 * Cached glyph outlines class prototype.
 * Based on FontCache, with minor modifications.
 */

#ifndef SVG_GLYPH_CACHE_H
#define SVG_GLYPH_CACHE_H

#include "modules/util/simset.h"
#include "modules/util/adt/opvector.h"

#include "modules/svg/src/svgpch.h"
#include "modules/svg/svg_path.h"

struct SVGGlyphLookupContext
{
	unsigned orientation:2;
	unsigned is_auto:1;
	unsigned horizontal:1;

	BOOL Match(const SVGGlyphLookupContext& props) const
	{
		if (horizontal != props.horizontal)
			return FALSE; // Differing orientation

		if (!horizontal)
		{
			// Both are vertical
			if (is_auto != props.is_auto)
				return FALSE; // Differing autoness (could potentially match though...)

			if (is_auto)
				return TRUE;
		}
		return orientation == props.orientation;
	}
};

class GlyphInfo
{
	SVGPath* svg_path;
	SVGNumber advance;
	SVGNumber rotation;
	UINT8 num_chars;
public:
	~GlyphInfo() { OP_DELETE(svg_path); }
	GlyphInfo() : svg_path(NULL) {}
	SVGPath** GetPathPtr() { return &svg_path; }
	SVGNumber& GetAdvanceRef() { return advance; }
	SVGNumber& GetRotationRef() { return rotation; }
	UINT8& GetNumCharsRef() { return num_chars; }
	const SVGNumber& GetAdvance() const { return advance; }
	const SVGPath* GetSVGPath() const { return svg_path; }
	const SVGNumber& GetRotation() const { return rotation; }
	const UINT8& GetNumChars() const { return num_chars; }
};

class GlyphRun
{
public:
	GlyphRun(unsigned int prealloc_count) : m_glyphs(prealloc_count), m_cached_char_count(0) {}

	OP_STATUS MapToGlyphs(OpFont* font, const uni_char* text, unsigned int textlen,
						  const SVGGlyphLookupContext& lookup_cxt);

	int FindGlyphContaining(int char_idx);
	void Truncate(unsigned int from_idx)
	{
		if (from_idx >= m_glyphs.GetCount())
			return;
		m_cached_char_count = 0; // Reset char. count
		m_glyphs.Remove(from_idx, m_glyphs.GetCount() - from_idx);
	}
	OP_STATUS Append(OpVector<GlyphInfo>& glyphs);

	SVGNumber GetExtent(unsigned int start_idx, unsigned int num_glyphs,
						SVGNumber letter_spacing, SVGNumber font_to_user_scale,
						BOOL skip_last_ls = TRUE);

	const GlyphInfo* Get(unsigned int i) const { return m_glyphs.Get(i); }
	unsigned int GetGlyphCount() const { return m_glyphs.GetCount(); }
	unsigned int GetCharCount()
	{
		if (m_cached_char_count == 0)
			m_cached_char_count = CountChars();
		return m_cached_char_count;
	}

private:
	unsigned int CountChars();

	OpVector<GlyphInfo> m_glyphs;
	unsigned int m_cached_char_count;
};

// We cache whole words so the cache has to be bigger to get a high number of cache hits.
#define SVGGLYPHCACHESIZE 128

class OpFont;

/**
 * SVGGlyphCacheElement's are stored in the glyph cache.
 * The elements are ordered in LRU order.
 */
class SVGGlyphCacheElement : public CountedListElement<SVGGlyphCacheElement>
{
public:
	SVGGlyphCacheElement(int glyph_count) : m_glyphs(glyph_count) {}

	OP_STATUS Init(const OpVector<GlyphInfo>& glyphs, OpFont* font,
				   const uni_char* text, unsigned text_len, const SVGGlyphLookupContext& props);
	void Reset() { m_glyphs.DeleteAll(); }

	BOOL IsMatching(OpFont* font, const uni_char* text, unsigned int len,
					const SVGGlyphLookupContext& props)
	{
		return
			m_font == font &&  // This will break if another font has
							   // been allocated at the same memory
							   // address as the old font
			(unsigned int)m_string.Length() == len &&
			m_string.Compare(text, len) == 0 &&
			m_props.Match(props);
	}
	BOOL IsMatchingSingleGlyph(OpFont* font, uni_char ch,
							   const SVGGlyphLookupContext& props)
	{
		return
			m_font == font &&  // This will break if another font has
							   // been allocated at the same memory
							   // address as the old font
			m_string.Length() == 1 &&
			m_string.CStr()[0] == ch &&
			m_props.Match(props);
	}

	BOOL Uses(OpFont* font) { return m_font == font; }

	OP_STATUS CopyGlyphsToVector(OpVector<GlyphInfo>& glyphs) const;
	OP_STATUS AddGlyphsToVector(OpVector<GlyphInfo>& glyphs) const;

#ifndef SVG_DEBUG_GLYPHCACHE
private:
#endif // SVG_DEBUG_GLYPHCACHE
	OpAutoVector<GlyphInfo> m_glyphs;
	OpFont* m_font;
	OpString m_string;
	SVGGlyphLookupContext m_props;
};

/**
 * Glyph Cache
 */
class SVGGlyphCache
{
public:
	SVGGlyphCache()
#ifdef _DEBUG
		: cache_hits(0), cache_misses(0)
#endif // _DEBUG
	{}
	~SVGGlyphCache();

	/**
	 * Clears the cache.
	 */
	void Clear() { cached_outlines.Clear(); }

	void HandleFontDeletion(OpFont* font_about_to_be_deleted);

	OP_STATUS GetGlyphsForWord(OpFont* vectorfont, const uni_char* text, unsigned int len, const SVGGlyphLookupContext& props, OpVector<GlyphInfo>& glyphs);

private:
	static OP_STATUS GetUncachedGlyphsForWord(OpFont* vectorfont, const uni_char* text, unsigned int len, const SVGGlyphLookupContext& props, OpVector<GlyphInfo>& glyphs);

	/**
	 * 'needs_overfull_cache' is set to TRUE for extra glyphs in words
	 * containing more than SVGGLYPHCACHESIZE unique glyphs. This lets the
	 * cache temporarily grow so that all the returned cache references will be
	 * valid. The cache returns to a size of SVGGLYPHCACHESIZE at the next
	 * word.
	 *
	 * A nicer solution would be glyph IDs.
	 */
	SVGGlyphCacheElement* GetCacheElement(unsigned int alloc_hint,
	                                      BOOL needs_overfull_cache);

	CountedList<SVGGlyphCacheElement> cached_outlines;

#ifdef _DEBUG
	UINT32 cache_hits;
	UINT32 cache_misses;
#endif // _DEBUG
};

#endif // SVG_GLYPH_CACHE_H
