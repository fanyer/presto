/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_RGBCOLOR_IMPL_H
#define SVG_DOM_RGBCOLOR_IMPL_H

# ifdef SVG_TINY_12

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGValue.h"

class SVGDOMRGBColorImpl : public SVGDOMRGBColor
{
public:
	SVGDOMRGBColorImpl(SVGObject* paint_or_color_val);
	virtual ~SVGDOMRGBColorImpl();

	virtual const char*		GetDOMName();

	/**
	 * type: 0 = red, 1 = green, 2 = blue
	 */
	virtual OP_STATUS		GetComponent(int type, double& val);

	/**
	 * type: 0 = red, 1 = green, 2 = blue
	 */
	virtual OP_STATUS		SetComponent(int type, double val);

	virtual SVGObject*		GetSVGObject() { return m_color_val; }

	OP_STATUS				GetRGBColor(UINT32 &color);
private:
	SVGObject*				m_color_val;
};

# endif // SVG_TINY_12
#endif // SVG_DOM_RGBCOLOR_IMPL_H
