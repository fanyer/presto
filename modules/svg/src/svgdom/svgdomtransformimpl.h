/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_TRANSFORM_IMPL_H
#define SVG_DOM_TRANSFORM_IMPL_H

# ifdef SVG_FULL_11
#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGTransform.h"

class SVGDOMTransformImpl : public SVGDOMTransform
{
public:
						SVGDOMTransformImpl(SVGTransform* transform);
	virtual				~SVGDOMTransformImpl();

	virtual	const char*	GetDOMName();
	virtual SVGObject*	GetSVGObject() { return m_transform; }
	virtual TransformType GetType() const;
	virtual void		UpdateMatrix(SVGDOMMatrix* matrix) { /* empty, depricated */ }
	virtual OP_STATUS	GetMatrix(SVGDOMMatrix*& matrix);
	virtual double		GetAngle();
	virtual void 		SetMatrix(const SVGDOMMatrix* matrix);
	virtual void		SetTranslate(double x, double y);
	virtual void        SetScale(double sx, double sy);
	virtual void        SetRotate(double angle, double cx, double cy);
	virtual void		SetSkewX(double angle);
	virtual void		SetSkewY(double angle);

	SVGTransform*		GetTransform() { return m_transform; }
	const SVGTransform*	GetTransform() const { return m_transform; }

private:
	SVGTransform*		m_transform;
};

# endif // SVG_FULL_11
#endif // !SVG_DOM_TRANSFORM_IMPL_H
