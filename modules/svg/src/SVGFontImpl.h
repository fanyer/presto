/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVGFONTIMPL_H
#define SVGFONTIMPL_H

#ifdef SVG_SUPPORT

#include "modules/pi/OpFont.h"

#include "modules/util/simset.h"

#include "modules/svg/src/svgfontdata.h"
#include "modules/svg/src/AttrValueStore.h"
#include "modules/svg/src/SVGElementStateContext.h"

class SVGFontImpl : public OpFont, public Link
{
public:
	SVGFontImpl(SVGFontData* fontdata, UINT32 size);
	virtual ~SVGFontImpl();

	virtual OpFontInfo::FontType Type() { return OpFontInfo::SVG_WEBFONT; }

	/** @return the font's ascent */
	virtual UINT32 Ascent();

	/** @return the font's descent */
	virtual UINT32 Descent();

	/** @return the font's internal leading */
	virtual UINT32 InternalLeading();

	/** @return the font's height in pixels */
	virtual UINT32 Height();

	/** @return the average width of characters in the font */
	virtual UINT32 AveCharWidth() { return 0; }

	/** @return the overhang of the font.
		(The width added for Italic fonts. If this is not a italic font, it returns 0)
	*/
	virtual UINT32 Overhang() { return 0; }

	/** Calculates the width of the specified string when drawn with the font.
		@return the width of the specified string
		@param str the string to calculate
		@param len how many characters of the string
	*/
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, INT32 extra_char_spacing = 0
#ifdef PLATFORM_FONTSWITCHING
							   , OpAsyncFontListener* listener = 0
#endif
								);

	/**
	   Calculates the width of the specified string when drawn with
	   the font, on the specified painter.

	   @return the width of the specified string
	   @param painter the painter, whose context to evaluate the width in.
	   @param str the string to calculate
	   @param len how many characters of the string
	*/
	virtual UINT32 StringWidth(const uni_char* str, UINT32 len, OpPainter* painter, INT32 extra_char_spacing = 0
#ifdef PLATFORM_FONTSWITCHING
							   , OpAsyncFontListener* listener = 0
#endif
								)
	{
		return StringWidth(str, len, extra_char_spacing
#ifdef PLATFORM_FONTSWITCHING
						   , listener
#endif
								);
	}

	virtual OP_STATUS GetOutline(const uni_char* in_str, UINT32 in_len, UINT32& io_str_pos, UINT32 in_last_str_pos,
								 BOOL in_writing_direction_horizontal, SVGNumber& out_advance, SVGPath** out_glyph);

#ifdef OPFONT_FONT_DATA
	virtual OP_STATUS GetFontData(UINT8*& font_data, UINT32& data_size) { return OpStatus::ERR; }
	virtual OP_STATUS ReleaseFontData(UINT8* font_data) { return OpStatus::ERR; }
#endif

#ifdef OPFONT_GLYPH_PROPS
	OP_STATUS GetGlyphProps(const UnicodePoint up, GlyphProps* props);
#endif // OPFONT_GLYPH_PROPS

	// =================================================================== //
	// SVG-internal getters/setters

	OP_STATUS CreateFontContents(SVGDocumentContext* doc_ctx)
	{
		if (!IsFontFullyCreated())
		{
			RETURN_IF_ERROR(m_fontdata->CreateFontContents(doc_ctx));

			if (HTML_Element* fontelm = m_fontdata->GetFontElement())
			{
				SVGFontElement* font_elm_ctx = AttrValueStore::GetSVGFontElement(fontelm);
				if (!font_elm_ctx)
					return OpStatus::ERR_NO_MEMORY; // Hmm, strange...

				font_elm_ctx->NotifyFontDataChange();
			}
		}
		return OpStatus::OK;
	}

	void UpdateFontData(SVGFontData* new_fontdata)
	{
		OP_ASSERT(new_fontdata);
		if (new_fontdata != m_fontdata)
		{
			SVGFontData::DecRef(m_fontdata);
			SVGFontData::IncRef(new_fontdata);
			m_fontdata = new_fontdata;
		}

		m_scale = m_size / m_fontdata->GetUnitsPerEm();
	}

	BOOL		HasLigatures() const { return m_fontdata->HasLigatures(); }
	BOOL		HasKerning() const { return m_fontdata->HasKerning(); }
	BOOL        IsFontFullyCreated() const { return m_fontdata->IsFontFullyCreated(); }

	void		SetMatchLang(const uni_char* lang) { m_match_lang = lang; }

	SVGNumber	GetSVGFontAttribute(SVGFontData::ATTRIBUTE attribute)
	{
		return m_fontdata->GetSVGFontAttribute(attribute) * m_scale;
	}

private:
	SVGFontData*		m_fontdata;
	const uni_char*		m_match_lang;
	SVGNumber			m_size;
	SVGNumber			m_scale;
};

#endif // SVG_SUPPORT
#endif // SVGFONTIMPL_H
