/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_NUMBER_IMPL_H
#define SVG_DOM_NUMBER_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGValue.h"

class SVGDOMNumberImpl : public SVGDOMNumber
{
public:
						SVGDOMNumberImpl(SVGNumberObject* number_val);
	virtual				~SVGDOMNumberImpl();

	virtual const char* GetDOMName();

	virtual OP_BOOLEAN	SetValue(double value);
	virtual double 		GetValue();
	virtual SVGObject*	GetSVGObject() { return m_number_val; }

	SVGNumberObject*		GetNumber() { return m_number_val; }
private:
	SVGDOMNumberImpl(const SVGDOMNumberImpl& X);
	void operator=(const SVGDOMNumberImpl& X);

	SVGNumberObject* m_number_val;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_NUMBER_IMPL_H
