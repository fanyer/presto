/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/style/css_svgfont.h"
#include "modules/display/webfont_manager.h"

/* static */ CSS_SvgFont*
CSS_SvgFont::Make(FramesDocument* doc, HTML_Element* font_element, OpFontInfo* fontinfo)
{
	CSS_SvgFont* new_font = OP_NEW(CSS_SvgFont, (font_element));
	if (new_font)
	{
		OP_STATUS status = new_font->Construct(doc, fontinfo);
		if (OpStatus::IsError(status))
		{
			OP_DELETE(new_font);
			new_font = NULL;
		}
	}
	return new_font;
}

/* virtual */ void
CSS_SvgFont::Removed(CSSCollection* collection, FramesDocument* doc)
{
	if (g_webfont_manager)
		g_webfont_manager->RemoveCSSWebFont(doc, m_font_element, URL());

	collection->StyleChanged(CSSCollection::CHANGED_WEBFONT);
}

OP_STATUS
CSS_SvgFont::Construct(FramesDocument* doc, OpFontInfo* fontinfo)
{
	int fn = styleManager->GetFontNumber(fontinfo->GetFace());

	BOOL create_fontnumber = fn < 0;

	if (create_fontnumber)
	{
		RETURN_IF_ERROR(styleManager->CreateFontNumber(fontinfo->GetFace(), fn));
	}

	fontinfo->SetFontNumber(fn);

	if (fontinfo)
	{
		for (int i = 0; i < 9; i++)
		{
			if (fontinfo->HasWeight(i))
				m_weight = i;
		}

		if (fontinfo->HasOblique())
			m_style = CSS_VALUE_oblique;
		else if (fontinfo->HasItalic())
			m_style = CSS_VALUE_italic;
		else
			m_style = CSS_VALUE_normal;
	}

	m_familyname = UniSetNewStr(fontinfo->GetFace());

	OP_STATUS err = m_familyname ? g_webfont_manager->AddCSSWebFont(GetSrcURL(), doc, m_font_element, fontinfo) : OpStatus::ERR_NO_MEMORY;

	// release fontnumber if we failed, but only if we allocated a new fontnumber
	if (OpStatus::IsError(err) && create_fontnumber)
		styleManager->ReleaseFontNumber(fn);

	return err;
}

#endif // SVG_SUPPORT
