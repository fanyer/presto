/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_POINT_IMPL_H
#define SVG_DOM_POINT_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGPoint.h"

class SVGDOMPointImpl : public SVGDOMPoint
{
public:
						SVGDOMPointImpl(SVGPoint* point);
	virtual 			~SVGDOMPointImpl();

	virtual const char* GetDOMName();
	virtual SVGObject*	GetSVGObject() { return m_point; }
	virtual OP_BOOLEAN	SetX(double x);
	virtual double 		GetX();
	virtual OP_BOOLEAN	SetY(double y);
	virtual double 		GetY();
	virtual OP_BOOLEAN  MatrixTransform(SVGDOMMatrix* matrix, SVGDOMPoint* target_point);

	SVGPoint*		GetNumberPair() { return m_point; }
private:
	SVGPoint*		m_point;
};

#endif // SVG_DOM_POINT_IMPL_H
