/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_DOM_MATRIX_BASE_H
#define SVG_DOM_MATRIX_BASE_H

#include "modules/svg/src/SVGTransform.h"

class SVGDOMMatrixBase : public SVGDOMMatrix
{
public:
	virtual void		GetMatrix(SVGMatrix& matrix) const = 0;
	virtual void		SetMatrix(const SVGMatrix& matrix) = 0;

	virtual	const char* GetDOMName();

	virtual void 		Multiply(SVGDOMMatrix* second_matrix, SVGDOMMatrix* target_matrix);
	virtual OP_BOOLEAN	Inverse(SVGDOMMatrix* target_matrix);
	virtual void		Translate(double x, double y, SVGDOMMatrix* target);
	virtual void		Scale(double scale_factor, SVGDOMMatrix* target);
	virtual void		ScaleNonUniform(double scale_factor_x, double scale_factor_y,
										SVGDOMMatrix* target);
	virtual void		Rotate(double angle, SVGDOMMatrix* target);
	virtual OP_BOOLEAN	RotateFromVector(double x, double y,
										 SVGDOMMatrix* target);
    virtual void 		FlipX(SVGDOMMatrix* target);
    virtual void		FlipY(SVGDOMMatrix* target);
	virtual void		SkewX(double angle, SVGDOMMatrix* target);
    virtual void		SkewY(double angle, SVGDOMMatrix* target);
};

#endif // !!SVG_DOM_MATRIX_BASE
