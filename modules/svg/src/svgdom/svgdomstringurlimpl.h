/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_STRING_URL_IMPL_H
#define SVG_DOM_STRING_URL_IMPL_H

# ifdef SVG_FULL_11
#include "modules/svg/src/SVGValue.h"

class SVGDOMStringUrlImpl : public SVGDOMString
{
public:
							SVGDOMStringUrlImpl(SVGURL* url);
	virtual					~SVGDOMStringUrlImpl();

	virtual const char*		GetDOMName();
	virtual SVGObject*		GetSVGObject() { return m_url; }
	virtual OP_BOOLEAN		SetValue(const uni_char* str);
	virtual const uni_char* GetValue();

private:
	SVGDOMStringUrlImpl(const SVGDOMStringUrlImpl& X);
	void operator=(const SVGDOMStringUrlImpl& X);

	SVGURL* m_url;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_STRING_URL_IMPL_H
