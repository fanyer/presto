/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_STRING_IMPL_H
#define SVG_DOM_STRING_IMPL_H

# ifdef SVG_FULL_11
#include "modules/svg/src/SVGValue.h"

class SVGDOMStringImpl : public SVGDOMString
{
public:
							SVGDOMStringImpl(SVGString* str_val);
	virtual					~SVGDOMStringImpl();

	virtual const char*		GetDOMName();
	virtual OP_BOOLEAN		SetValue(const uni_char* str);
	virtual const uni_char* GetValue();
	virtual SVGObject*		GetSVGObject() { return m_str_val; }

	SVGString*		GetString() { return m_str_val; }
private:
	SVGDOMStringImpl(const SVGDOMStringImpl& X);
	void operator=(const SVGDOMStringImpl& X);

	SVGString* m_str_val;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_STRING_IMPL_H
