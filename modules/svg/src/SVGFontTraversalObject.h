/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#ifndef SVG_FONT_TRAVERSAL_OBJECT_H
#define SVG_FONT_TRAVERSAL_OBJECT_H

#include "modules/svg/src/SVGTraverse.h"

class SVGXMLFontData;

class SVGFontTraversalObject : public SVGSimpleTraversalObject
{
private:
	OpFontInfo*			m_fontinfo;	///< Mode 1: Collect fontinfo
	SVGXMLFontData*		m_fontdata;	///< Mode 2: Create font
	BOOL				m_has_glyphs;

	// Both of the objects above should be owned by their callers

	// We see advance_x/y before units_per_em so we have to wait till
	// later with setting it in the font
	SVGNumber m_last_seen_font_advance_x;
	SVGNumber m_last_seen_font_advance_y;

	OP_STATUS SetupFontInfoForFontFace(HTML_Element* element);

public:
	SVGFontTraversalObject(SVGDocumentContext* doc_ctx, OpFontInfo* fontinfo)
		: SVGSimpleTraversalObject(doc_ctx), m_fontinfo(fontinfo), m_fontdata(NULL), m_has_glyphs(FALSE) {}
	SVGFontTraversalObject(SVGDocumentContext* doc_ctx, SVGXMLFontData* fontdata)
		: SVGSimpleTraversalObject(doc_ctx), m_fontinfo(NULL), m_fontdata(fontdata), m_has_glyphs(FALSE) {}
	virtual ~SVGFontTraversalObject() {}

	virtual BOOL		AllowChildTraverse(HTML_Element* element);
	virtual OP_STATUS	CSSAndEnterCheck(HTML_Element* element, BOOL& traversal_needs_css);
	virtual OP_STATUS	Enter(HTML_Element* element) { return OpStatus::OK; }
	virtual OP_STATUS	DoContent(HTML_Element* element);
	virtual OP_STATUS	Leave(HTML_Element* element) { return OpStatus::OK; }

	BOOL				HasGlyphs() const { return m_has_glyphs; }
};

#endif // SVG_FONT_TRAVERSAL_OBJECT_H
