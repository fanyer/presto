/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_MATRIX_IMPL_H
#define SVG_DOM_MATRIX_IMPL_H

#include "modules/svg/svg_dominterfaces.h"
#include "modules/svg/src/SVGTransform.h"
#include "modules/svg/src/svgdom/svgdommatrixbase.h"

class SVGDOMMatrixImpl : public SVGDOMMatrixBase
{
public:
						SVGDOMMatrixImpl(SVGMatrixObject* matrix);
	virtual 			~SVGDOMMatrixImpl();
	virtual OP_BOOLEAN	SetValue(int idx, double x);
	virtual double 		GetValue(int idx);
	virtual SVGObject*	GetSVGObject() { return m_matrix; }

	void				GetMatrix(SVGMatrix& matrix) const;
	void				SetMatrix(const SVGMatrix& matrix);
private:
	SVGDOMMatrixImpl(const SVGDOMMatrixImpl& X);
	void operator=(const SVGDOMMatrixImpl& X);

	SVGMatrixObject* m_matrix;
};

#endif // !SVG_DOM_MATRIX_TRANSFORM_IMPL_H
