/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/svgdom/svgdommatrixbase.h"

/* virtual */ const char*
SVGDOMMatrixBase::GetDOMName()
{
	return "SVGMatrix";
}

/* virtual */ void
SVGDOMMatrixBase::Multiply(SVGDOMMatrix* second_matrix, SVGDOMMatrix* target_matrix)
{
	OP_ASSERT(target_matrix != NULL);
	OP_ASSERT(second_matrix != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	SVGMatrix work;
	SVGDOMMatrixBase* second_base = static_cast<SVGDOMMatrixBase*>(second_matrix);
	second_base->GetMatrix(work);
	
	work.Multiply(tmp);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target_matrix);
	target_base->SetMatrix(work);
}

/* virtual */ OP_BOOLEAN
SVGDOMMatrixBase::Inverse(SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix work;
	GetMatrix(work);
	if (work.Invert())
	{
		SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
		target_base->SetMatrix(work);
		return OpBoolean::IS_TRUE;
	}
	else
	{
		return OpBoolean::IS_FALSE;
	}
}

/* virtual */ void
SVGDOMMatrixBase::Translate(double x, double y, SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	tmp.PostMultTranslation(x,y);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(tmp);
}

/* virtual */ void
SVGDOMMatrixBase::Scale(double scale_factor, SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	tmp.PostMultScale(scale_factor, scale_factor);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(tmp);
}

/* virtual */ void
SVGDOMMatrixBase::ScaleNonUniform(double scale_factor_x,
										   double scale_factor_y,
										   SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	tmp.PostMultScale(scale_factor_x, scale_factor_y);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(tmp);
}

/* virtual */ void
SVGDOMMatrixBase::Rotate(double angle, SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	SVGMatrix work;
	work.LoadRotation(angle);
	work.Multiply(tmp);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(work);
}

/* virtual */ OP_BOOLEAN
SVGDOMMatrixBase::RotateFromVector(double x, double y,
											SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	SVGNumber theta = SVGNumber(x).atan2(SVGNumber(y));

	SVGMatrix work;
	work.LoadRotation(theta * 180 / SVGNumber::pi());
	work.Multiply(tmp);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(work);
	return OpBoolean::IS_TRUE;
}

/* virtual */ void
SVGDOMMatrixBase::FlipX(SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	tmp.PostMultScale(-1, 1);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(tmp);
}

/* virtual */ void
SVGDOMMatrixBase::FlipY(SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	tmp.PostMultScale(1, -1);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(tmp);
}

/* virtual */ void
SVGDOMMatrixBase::SkewX(double angle, SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	SVGMatrix work;
	work.LoadSkewX(angle);
	work.Multiply(tmp);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(work);
}

/* virtual */ void
SVGDOMMatrixBase::SkewY(double angle, SVGDOMMatrix* target)
{
	OP_ASSERT(target != NULL);

	SVGMatrix tmp;
	GetMatrix(tmp);

	SVGMatrix work;
	work.LoadSkewY(angle);
	work.Multiply(tmp);

	SVGDOMMatrixBase* target_base = static_cast<SVGDOMMatrixBase*>(target);
	target_base->SetMatrix(work);
}

#endif // SVG_DOM && SVG_SUPPORT
