/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef CSS_SVGFONT_H
#define CSS_SVGFONT_H

#if defined(SVG_SUPPORT)

#include "modules/style/css_webfont.h"
#include "modules/style/css_collection.h"

/**
 * This class represents an SVG font defined in svg markup only. For svgfonts referenced in an
 * @font-face rule see CSS_FontfaceRule.
 */
class CSS_SvgFont : public CSS_WebFont, public CSSCollectionElement
{
public:
	/**
	 * Add an svg font to the css collection. It's added here to get it into the cascading order of web fonts.
	 * This causes a CSS_WebFont object to be created internally which will be returned when the font-family is
	 * matching the one provided in the GetWebFont call.
	 *
	 * @param doc The document where the font is defined.
	 * @param font_element The Markup::SVGE_FONT or Markup::SVGE_FONT_FACE element that describes the SVGFont.
	 * @param fontinfo OpFontInfo structure for the added font.
	 * @return OpStatus::OK if successful, OpStatus::ERR_NO_MEMORY on OOM.
	 */
	static CSS_SvgFont* Make(FramesDocument* doc, HTML_Element* font_element, OpFontInfo* fontinfo);

	virtual ~CSS_SvgFont() { OP_DELETEA(m_familyname); }

	/** CSS_WebFont API */

	virtual void RemovedWebFont(FramesDocument* doc) { /* Removal is handled in Removed() from the CSSCollection API. */ }
	virtual const uni_char* GetFamilyName() { return m_familyname; }
	virtual URL GetSrcURL() { return URL(); }
	virtual Format GetFormat() { return FORMAT_SVG; }
	virtual short GetStyle() { return m_style; }
	virtual short GetWeight() { return m_weight; }
	virtual LoadStatus GetLoadStatus() { return WEBFONT_LOADED; }
	virtual OP_STATUS SetLoadStatus(FramesDocument* doc, LoadStatus status) { OP_ASSERT(FALSE);/* Should not get here with the current design. */ return OpStatus::OK; }
	virtual OP_STATUS Load(FramesDocument* doc) { return OpStatus::OK; }
	virtual BOOL IsLocalFont() { return FALSE; }

	/** CSSCollectionElement API */

	virtual Type GetType() { return SVGFONT; }
	virtual HTML_Element* GetHtmlElement() { return m_font_element; }
	virtual void Added(CSSCollection* collection, FramesDocument* doc) { collection->StyleChanged(CSSCollection::CHANGED_WEBFONT); }
	virtual void Removed(CSSCollection* collection, FramesDocument* doc);

private:

	/** Constructor. It's private and all CSS_SvgFont objects must be created with the static Make method. */
	CSS_SvgFont(HTML_Element* font_element) : m_font_element(font_element), m_style(CSS_VALUE_normal), m_weight(4), m_familyname(NULL) {}

	/** Assign a font number for the font and add it to the webfont font manager.
		Initializes the object with data from fontinfo and sets the assigned font
		number in fontinfo. */
	OP_STATUS Construct(FramesDocument* doc, OpFontInfo* fontinfo);

	/** The svg:font element. */
	HTML_Element* m_font_element;

	/** fontinfo for this font. */
	short m_style;
	short m_weight;
	uni_char* m_familyname;
};

#endif // SVG_SUPPORT

#endif // CSS_SVGFONT_H
