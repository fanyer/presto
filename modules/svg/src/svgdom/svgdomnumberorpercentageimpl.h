/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_NUMBER_OR_PERCENTAGE_IMPL_H
#define SVG_DOM_NUMBER_OR_PERCENTAGE_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGLength.h"

class SVGDOMNumberOrPercentageImpl : public SVGDOMNumber
{
public:
						SVGDOMNumberOrPercentageImpl(SVGLengthObject* length);
	virtual				~SVGDOMNumberOrPercentageImpl();

	virtual const char* GetDOMName();

	virtual OP_BOOLEAN	SetValue(double value);
	virtual double 		GetValue();
	virtual SVGObject*	GetSVGObject() { return m_length; }

private:
	SVGDOMNumberOrPercentageImpl(const SVGDOMNumberOrPercentageImpl& X);
	void operator=(const SVGDOMNumberOrPercentageImpl& X);

	SVGLengthObject* m_length;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_NUMBER_OR_PERCENTAGE_IMPL_H
