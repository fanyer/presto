/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGFONTDATA_H
#define SVGFONTDATA_H

#ifdef SVG_SUPPORT

#include "modules/util/adt/opvector.h"
#include "modules/svg/svg_external_types.h"

class OpBpath;
class SVGVector;
class SVGNumberObject;
class SVGDocumentContext;

static inline BOOL IsArabic(uni_char letter)
{
	// From the unicode specification
	// Arabic
	// U+0600 -> U+06FF   (1536->1791)
	return letter >= 1536 && letter <= 1791;
}

#if defined USE_TEXTSHAPER_INTERNALLY && defined TEXTSHAPER_SUPPORT
#define SVG_STORE_SHAPED_GLYPH
extern uni_char GetShapedGlyph(uni_char unicode, SVGArabicForm arabic_form);
#endif // USE_TEXTSHAPER_INTERNALLY && TEXTSHAPER_SUPPORT

typedef UINT16 SVGGlyphIdx;

class SVGGlyphData
{
public:
	enum GlyphDirectionMask
	{
		HORIZONTAL	= 1,
		VERTICAL	= 2,
		BOTH		= 3 // HORIZONTAL | VERTICAL
	};

	struct CompositeGlyph
	{
		uni_char*	unicode;
		UINT32		unicode_len;
	};

private:
	union
	{
		CompositeGlyph composite;
		uni_char simple;
	} m_unicode;
	OpBpath*	m_glyph; // Shared with attribute
	uni_char*	m_glyphname;
	SVGNumber	m_advance_x;
	SVGNumber	m_advance_y;
	SVGGlyphData* m_next;
	SVGVector*	m_lang;
	union
	{
		struct
		{
			unsigned int has_adv_x:1;
			unsigned int has_adv_y:1;
			unsigned int has_path:1;
			unsigned int is_simple:1;
			unsigned int glyph_direction:2;
			unsigned int arabic_form:3; // SVGArabicForm
			unsigned int index:16;
		} packed; /* 25 bits */
		unsigned int packed_init;
	};

public:
	SVGGlyphData() :
		m_glyph(NULL),
		m_glyphname(NULL),
		m_advance_x(0),
		m_advance_y(0),
		m_next(NULL),
		m_lang(NULL),
		packed_init(0)
	{
		m_unicode.composite.unicode = NULL;
		m_unicode.composite.unicode_len = 0;

		packed.arabic_form = SVGARABICFORM_ISOLATED;
		packed.glyph_direction = BOTH;
	}
	~SVGGlyphData();

	SVGGlyphIdx GetIndex() const { return (SVGGlyphIdx)packed.index; }
	void SetIndex(SVGGlyphIdx gi) { packed.index = gi; }

	SVGGlyphData* Suc() const { return m_next; }
	void SetSuc(SVGGlyphData* gd) { m_next = gd; }

	OP_STATUS SetOutline(OpBpath* glyph);
	OpBpath* GetOutline() { return m_glyph; }
	BOOL HasOutline() { return packed.has_path; }

	OP_STATUS SetUnicode(const uni_char* unicode, UINT32 len)
	{
		if (packed.is_simple == 0)
			OP_DELETEA(m_unicode.composite.unicode);
		if (len == 1)
		{
			packed.is_simple = 1;

			m_unicode.simple = unicode[0];
		}
		else
		{
			packed.is_simple = 0;

			m_unicode.composite.unicode = UniSetNewStrN(unicode, len);
			if (!m_unicode.composite.unicode)
				return OpStatus::ERR_NO_MEMORY;
			m_unicode.composite.unicode_len = len;
		}
		return OpStatus::OK;
	}

	OP_STATUS SetGlyphName(const uni_char* glyphname, UINT32 len)
	{
		OP_DELETE(m_glyphname);
		m_glyphname = UniSetNewStrN(glyphname, len);
		if(!m_glyphname)
			return OpStatus::ERR_NO_MEMORY;
		return OpStatus::OK;
	}

	void SetLang(SVGVector* lang_vec);

	// Compare c to the first char of unicode
	int Compare(unsigned c)
	{
		if (packed.is_simple)
			return (int)m_unicode.simple - (int)c;
		else if (m_unicode.composite.unicode_len > 1)
			return (int)m_unicode.composite.unicode[0] - (int)c;
		else
			return -(int)c;
	}

	BOOL MatchUnicode(const uni_char* unicode, UINT32 len) const
	{
		OP_ASSERT(len > 0);
		if (packed.is_simple)
			return unicode[0] == m_unicode.simple;

		return m_unicode.composite.unicode &&
			(len >= m_unicode.composite.unicode_len &&
			 uni_strncmp(unicode, m_unicode.composite.unicode, m_unicode.composite.unicode_len) == 0);
	}

	BOOL MatchGlyphName(const uni_char* unicode, UINT32 len) const
	{
		return m_glyphname &&
			len == uni_strlen(m_glyphname) &&
			uni_strncmp(unicode, m_glyphname, len) == 0;
	}

	BOOL MatchLang(const uni_char* lang, unsigned int lang_len) const;

	/** Returns if this glyph can be used to draw the start of the unicode string */
	BOOL IsMatch(const uni_char* unicode, UINT32 len, BOOL isGlyphName = FALSE) const
	{
		if (!isGlyphName)
			return MatchUnicode(unicode, len);
		else
			return MatchGlyphName(unicode, len);
	}

	void SetDirection(int direction) { packed.glyph_direction = direction; }
	BOOL MatchDirection(int direction_mask) const
	{
		return (packed.glyph_direction & direction_mask) != 0;
	}

// 	const uni_char* GetUnicode() { return m_unicode; }
	UINT32 GetUnicodeLen() { return packed.is_simple ? 1 : m_unicode.composite.unicode_len; }
// 	const uni_char* GetGlyphName() { return m_glyphname; }
	SVGArabicForm GetArabicForm() { return (SVGArabicForm)packed.arabic_form; }
	void SetArabicForm(SVGArabicForm arabic_form)
	{
		packed.arabic_form = arabic_form;
#ifdef SVG_STORE_SHAPED_GLYPH
		if (packed.is_simple)
			// Only do shaping for simple glyphs
			if (uni_char shaped = GetShapedGlyph(m_unicode.simple, arabic_form))
				m_unicode.simple = shaped;
#endif // SVG_STORE_SHAPED_GLYPH
	}

	void SetAdvanceX(SVGNumber x);
	void SetAdvanceY(SVGNumber y);
	SVGNumber GetAdvanceX() { return m_advance_x; }
	SVGNumber GetAdvanceY() { return m_advance_y; }
	BOOL HasAdvanceX() { return packed.has_adv_x; }
	BOOL HasAdvanceY() { return packed.has_adv_y; }
};

class SVGFontData
{
public:
	SVGFontData() : m_refcount(0), m_packed_init(0) {}
	virtual ~SVGFontData() {}

	static SVGFontData* IncRef(SVGFontData* fdata);
	static void			DecRef(SVGFontData* fdata);

	BOOL HasKerning() const { return m_packed.has_kerning; }
	BOOL HasLigatures() const { return m_packed.has_ligatures; }
	BOOL IsFontFullyCreated() const { return m_packed.is_fully_created; }

	virtual OP_STATUS GetKern(SVGGlyphData* current_glyph, SVGGlyphData* last_glyph,
							  BOOL horizontal, SVGNumber &kern) { return OpStatus::ERR; }

	virtual OP_STATUS FetchGlyphData(const uni_char* in_str, UINT32 in_len,
									 UINT32& io_str_pos, BOOL in_writing_direction_horizontal,
									 const uni_char* lang,
									 SVGGlyphData** out_data) = 0;

	virtual OP_STATUS CreateFontContents(SVGDocumentContext* doc_ctx) { return OpStatus::OK; }

	virtual HTML_Element* GetFontElement() const { return NULL; }

	enum ATTRIBUTE
	{
		// Used to index an array of SVGNumbers
		ASCENT,
		DESCENT,

		ADVANCE_X,
		ADVANCE_Y,

		UNDERLINE_POSITION,
		OVERLINE_POSITION,
		STRIKETHROUGH_POSITION,
		UNDERLINE_THICKNESS,
		OVERLINE_THICKNESS,
		STRIKETHROUGH_THICKNESS,

		// Baseline offsets
		ALPHABETIC_OFFSET,
		HANGING_OFFSET,
		IDEOGRAPHIC_OFFSET,
		MATHEMATICAL_OFFSET,
		V_ALPHABETIC_OFFSET,
		V_HANGING_OFFSET,
		V_IDEOGRAPHIC_OFFSET,
		V_MATHEMATICAL_OFFSET,

		FONT_ATTRIBUTE_COUNT
	};

	SVGNumber GetSVGFontAttribute(ATTRIBUTE attribute)
	{
// 		OP_ASSERT(IsFontFullyCreated());
		OP_ASSERT((unsigned)attribute < ARRAY_SIZE(m_line_attributes));
		SVGNumber res = m_line_attributes[attribute];
		return res;
	}
	void SetSVGFontAttribute(ATTRIBUTE attribute, SVGNumber value)
	{
		OP_ASSERT((unsigned)attribute < ARRAY_SIZE(m_line_attributes));
		m_line_attributes[attribute] = value;
	}

	/**
	 * Should be the first to be set, since this dictates how the rest should be interpreted.
	 */
	void SetUnitsPerEm(SVGNumber upem);
	SVGNumber GetUnitsPerEm() const { return m_units_per_em; }

	/**
	 * Must be called after units-per-em is set, but before any custom attributes have been set.
	 */
	void SetDefaultAttributes();

protected:
	/**
	 * Attributes stored as in the backend.
	 */
	SVGNumber m_line_attributes[FONT_ATTRIBUTE_COUNT];
	SVGNumber m_units_per_em;

	int m_refcount;

	union
	{
		struct
		{
			unsigned int has_kerning : 1;
			unsigned int has_ligatures : 1;
			unsigned int is_fully_created : 1;
		} m_packed;
		UINT8 m_packed_init;
	};
};

/*
 * Backend for tree-backed fonts AKA 'SVG fonts'
 */
class SVGKernData
{
private:
	SVGVector*	m_g1; // Shared with attribute
	SVGVector*	m_g2; // Shared with attribute
	SVGVector*	m_u1; // Shared with attribute
	SVGVector*	m_u2; // Shared with attribute
	SVGNumber	m_kerning;

public:
	SVGKernData() : m_g1(0), m_g2(0), m_u1(0), m_u2(0), m_kerning(0) {}
	~SVGKernData();

	const SVGNumber& GetKerningValue() const { return m_kerning; }
	const SVGVector* GetG1() const { return m_g1; }
	const SVGVector* GetG2() const { return m_g2; }
	const SVGVector* GetU1() const { return m_u1; }
	const SVGVector* GetU2() const { return m_u2; }

	static BOOL IsMatch(const SVGVector* v1, SVGGlyphData* g1, BOOL isGlyphNameVector = FALSE);

	OP_STATUS SetData(SVGVector* g1, SVGVector* g2, SVGVector* u1, SVGVector* u2, SVGNumber kerning);
};

#define INDEXBASED_KERN_TABLES

#ifdef INDEXBASED_KERN_TABLES
class SVGKerningTable
{
public:
	SVGKerningTable() : m_pairs(NULL), m_count(0), m_allocated(0) {}
	~SVGKerningTable() { OP_DELETEA(m_pairs); }

	OP_STATUS Prepare(unsigned int size_hint);
	OP_STATUS Append(SVGGlyphIdx gi1, SVGGlyphIdx gi2, SVGNumber value);
	OP_STATUS Finalize();

	OP_STATUS Lookup(SVGGlyphIdx gi1, SVGGlyphIdx gi2, SVGNumber& value);

private:
	OP_STATUS Resize(unsigned int new_size);
	static int Compare(const void* a, const void* b);

	enum { GROWTH_AMOUNT = 16 };

	struct KernPair
	{
		UINT32 SearchValue() const { return (first << 16) | second; }

		SVGNumber value;
		SVGGlyphIdx first;
		SVGGlyphIdx second;
	};

	KernPair* m_pairs;
	unsigned int m_count;
	unsigned int m_allocated;
};
#endif // INDEXBASED_KERN_TABLES

class SVGXMLFontData : public SVGFontData
{
public:
	SVGXMLFontData();

	/**
	 * Creates the font. Traverses the 'font' subtree and picks up relevant data.
	 */
	OP_STATUS Construct(HTML_Element* fontelm);

	virtual OP_STATUS GetKern(SVGGlyphData* current_glyph, SVGGlyphData* last_glyph,
							  BOOL horizontal, SVGNumber &kern);

	virtual OP_STATUS FetchGlyphData(const uni_char* in_str, UINT32 in_len,
									 UINT32& io_str_pos, BOOL in_writing_direction_horizontal,
									 const uni_char* lang,
									 SVGGlyphData** out_data);

	virtual OP_STATUS CreateFontContents(SVGDocumentContext* doc_ctx);

	OP_STATUS AddGlyph(const uni_char* str, const uni_char* glyphname,
					   OpBpath* path, SVGNumberObject* advX, SVGNumberObject* advY,
					   SVGArabicForm arabic_form, SVGGlyphData::GlyphDirectionMask gdir_mask,
					   SVGVector* lang_vector);
	OP_STATUS SetMissingGlyph(OpBpath* path, SVGNumberObject* advX, SVGNumberObject* advY);
	OP_STATUS SetPathAndAdvance(SVGGlyphData* data, OpBpath* path, SVGNumberObject* advX, SVGNumberObject* advY);

	OP_STATUS AddKern(BOOL horizontal, SVGVector* g1, SVGVector* g2, SVGVector* u1, SVGVector* u2, SVGNumber k);

	HTML_Element* GetFontElement() const { return m_fontelm; }
	void DetachFontElement() { m_fontelm = NULL; }

private:
	OP_STATUS InsertIntoCMap(unsigned c, SVGGlyphData* data);
	unsigned int FindCMapIndex(unsigned c);
	SVGGlyphData* FindInCMap(unsigned c);

	OpAutoVector<SVGGlyphData> m_cmap;
	SVGGlyphData m_missing_glyph;

#ifdef INDEXBASED_KERN_TABLES
	SVGGlyphIdx UnicodeToIdx(const uni_char* str, unsigned int str_len);
	SVGGlyphIdx GlyphNameToIdx(const uni_char* name, unsigned int name_len);
	OP_STATUS MapAndAppendIndices(const SVGVector* v, OpINT32Vector& indices, BOOL map_names);
	OP_STATUS BuildKernTable(OpAutoVector<SVGKernData>& kerndatatable,
							 SVGKerningTable& kerntab);

	SVGKerningTable m_kern_pairs_h;
	SVGKerningTable m_kern_pairs_v;
#endif // INDEXBASED_KERN_TABLES

	OpAutoVector<SVGKernData> m_hkern;
	OpAutoVector<SVGKernData> m_vkern;

	HTML_Element* m_fontelm;
	unsigned int m_current_glyph_idx;
	BOOL m_kern_tables_built;
};

#endif // SVG_SUPPORT
#endif // SVGFONTDATA_H
