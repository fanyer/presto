/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_MATRIX_TRANSFORM_IMPL_H
#define SVG_DOM_MATRIX_TRANSFORM_IMPL_H

# ifdef SVG_FULL_11

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/svgdom/svgdommatrixbase.h"

class SVGDOMMatrixTransformImpl : public SVGDOMMatrixBase
{
public:
						SVGDOMMatrixTransformImpl(SVGTransform* transform);
	virtual 			~SVGDOMMatrixTransformImpl();
	virtual OP_BOOLEAN	SetValue(int idx, double x);
	virtual double 		GetValue(int idx);
	virtual SVGObject*	GetSVGObject() { return m_transform; }

	virtual void		GetMatrix(SVGMatrix& matrix) const;
	virtual void		SetMatrix(const SVGMatrix& matrix);
private:
	SVGDOMMatrixTransformImpl(const SVGDOMMatrixTransformImpl& X);
	void operator=(const SVGDOMMatrixTransformImpl& X);

	SVGTransform* m_transform;
};

# endif // SVG_FULL_11
#endif // SVG_DOM_MATRIX_TRANSFORM_IMPL_H
