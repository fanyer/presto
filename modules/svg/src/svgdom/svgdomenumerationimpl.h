/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_ENUMERATION_IMPL_H
#define SVG_DOM_ENUMERATION_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGValue.h"

class SVGDOMEnumerationImpl : public SVGDOMEnumeration
{
public:
						SVGDOMEnumerationImpl(SVGEnum* enum_val);
	virtual				~SVGDOMEnumerationImpl();

	virtual const char* GetDOMName();

	virtual OP_BOOLEAN  SetValue(unsigned short value);
	virtual unsigned short GetValue();
	virtual SVGObject*		GetSVGObject() { return m_enum_val; }

#if 0
	SVGEnum*		GetEnum();
#endif // 0

private:
	SVGDOMEnumerationImpl(const SVGDOMEnumerationImpl& X);
	void operator=(const SVGDOMEnumerationImpl& X);

	SVGEnum* m_enum_val;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_ENUMERATION_IMPL_H
