/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGEMBEDDEDSYSTEMFONT_H
#define SVGEMBEDDEDSYSTEMFONT_H

#if defined(SVG_SUPPORT) && defined(SVG_SUPPORT_EMBEDDED_FONTS)

#include "modules/svg/src/svgfontdata.h"
#include "modules/svg/src/OpBpath.h"

#include "modules/util/opfile/opfile.h"

/**
 * These classes implements system fonts by using our own font-format
 * for easy parsing during drawing.
 */

class SVGFontFile
{
public:
	SVGFontFile(OpFileDescriptor* filedesc) : m_filedesc(filedesc) {}
	~SVGFontFile() { OP_DELETE(m_filedesc); }

	OP_STATUS GetFilePos(OpFileLength& pos) const
	{
		return m_filedesc->GetFilePos(pos);
	}
	OP_STATUS SetFilePos(OpFileLength pos, OpSeekMode seek_mode=SEEK_FROM_START)
	{
		return m_filedesc->SetFilePos(pos, seek_mode);
	}

	void ReadL(INT16& result);
	void ReadL(INT32& result);
	void ReadL(float& result);
	void ReadL(OpString& str);

	BOOL Eof() { return m_filedesc->Eof(); }

private:
	OpFileDescriptor* m_filedesc;
};

class SVGSystemFontCache
{
public:
	SVGSystemFontCache();
	~SVGSystemFontCache();

	OP_STATUS			Init(SVGFontFile* in_file, OpFileLength glyph_table_offset);
	SVGGlyphData*		GetGlyph(uni_char uc);

private:
	void				ReadIndexL(OpFileLength glyph_table_offset);
	SVGGlyphData*		ReadGlyphL(uni_char uc);

	struct IndexItem
	{
		IndexItem() : position(0) {}
		OpFileLength position;
		uni_char uc;
	};

	enum { CacheSize = 40 }; // Somewhat bigger than the english alphabet.

	int					m_misses;
	int					m_requests;
	UINT32				m_cache_idx;
	SVGGlyphData**		m_cache; // linear, O(n), TODO. Could make this into a binary search
	IndexItem*			m_index;
	INT16				m_glyphs;
	SVGFontFile*		m_font_file;
};

class SVGEmbeddedFontData;

class SVGSystemFontManager
{
public:
	SVGSystemFontManager();
    ~SVGSystemFontManager();

	BOOL					IsCreated() { return m_create_called; }
	BOOL					HasFonts() { return m_hasfonts; }
	OP_STATUS				Create();
	SVGEmbeddedFontData*	GetMatchingSystemFont(FontAtt &fontatt);

private:
	OP_STATUS				LoadFont(const uni_char* filename, SVGEmbeddedFontData** font);

	SVGEmbeddedFontData*	m_monospace_font;
	SVGEmbeddedFontData*	m_serif_font;
	SVGEmbeddedFontData*	m_sans_serif_font;

	SVGEmbeddedFontData*	m_bold_monospace_font;
	SVGEmbeddedFontData*	m_bold_serif_font;
	SVGEmbeddedFontData*	m_bold_sans_serif_font;

	BOOL					m_create_called;
	BOOL					m_hasfonts;
};

class SVGEmbeddedFontData : public SVGFontData
{
public:
	SVGEmbeddedFontData();
	~SVGEmbeddedFontData();

	virtual OP_STATUS	FetchGlyphData(const uni_char* in_str, UINT32 in_len,
									   UINT32& io_str_pos, BOOL in_writing_direction_horizontal,
									   const uni_char* lang,
									   SVGGlyphData** out_data);

	OP_STATUS			Load(SVGFontFile* font_file);

	const uni_char*		GetFontName() { return m_family_name.CStr(); }

private:
	void				LoadL(SVGFontFile* font_file);

	OpString			m_family_name;
	OpString			m_style_name;

	SVGGlyphData		m_unknown_glyph;
	SVGSystemFontCache	m_cache;
	SVGFontFile*		m_font_file;
};

#endif // SVG_SUPPORT && SVG_SUPPORT_EMBEDDED_FONTS
#endif // SVGEMBEDDEDSYSTEMFONT_H
