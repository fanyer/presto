/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_RECT_IMPL_H
#define SVG_DOM_RECT_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGRect.h"

class SVGDOMRectImpl : public SVGDOMRect
{
public:
						SVGDOMRectImpl(SVGRectObject* rect);
	virtual				~SVGDOMRectImpl();

	virtual const char* GetDOMName();
	virtual SVGObject*	GetSVGObject() { return m_rect; }
	virtual OP_BOOLEAN	SetX(double x);
	virtual double 		GetX();
	virtual OP_BOOLEAN  SetY(double y);
	virtual double		GetY();
	virtual OP_BOOLEAN  SetWidth(double width);
	virtual double		GetWidth();
	virtual OP_BOOLEAN  SetHeight(double height);
	virtual double		GetHeight();

	SVGRectObject*		GetRect() { return m_rect; }
private:
	SVGRectObject*		m_rect;
};

#endif // !SVG_DOM_RECT_IMPL_H
