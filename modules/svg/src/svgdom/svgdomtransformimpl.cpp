/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_FULL_11)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/svgdom/svgdommatrixbase.h"
#include "modules/svg/src/svgdom/svgdommatrixtransformimpl.h"
#include "modules/svg/src/svgdom/svgdomtransformimpl.h"

SVGDOMTransformImpl::SVGDOMTransformImpl(SVGTransform* transform) :
		m_transform(transform)
{
	SVGObject::IncRef(m_transform);
}

/* virtual */
SVGDOMTransformImpl::~SVGDOMTransformImpl()
{
	SVGObject::DecRef(m_transform);
}

/* virtual */ const char*
SVGDOMTransformImpl::GetDOMName()
{
	return "SVGTransform";
}

/* virtual */ SVGDOMTransform::TransformType
SVGDOMTransformImpl::GetType() const
{
	return (SVGDOMTransform::TransformType)m_transform->GetTransformType();
}

/* virtual */ OP_STATUS
SVGDOMTransformImpl::GetMatrix(SVGDOMMatrix*& matrix)
{
	matrix = OP_NEW(SVGDOMMatrixTransformImpl, (m_transform));
	return matrix ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* virtual */ double
SVGDOMTransformImpl::GetAngle()
{
	if (m_transform->GetTransformType() == SVGTRANSFORM_ROTATE ||
		m_transform->GetTransformType() == SVGTRANSFORM_SKEWX ||
		m_transform->GetTransformType() == SVGTRANSFORM_SKEWY)
	{
		return m_transform->values[0].GetFloatValue();
	}
	else
	{
		return 0;
	}
}

/* virtual */ void
SVGDOMTransformImpl::SetMatrix(const SVGDOMMatrix* matrix)
{
	const SVGDOMMatrixBase* matrix_base = static_cast<const SVGDOMMatrixBase*>(matrix);
	SVGMatrix svgmatrix;
	matrix_base->GetMatrix(svgmatrix);
	m_transform->SetMatrix(svgmatrix);
}

/* virtual */ void
SVGDOMTransformImpl::SetTranslate(double x, double y)
{
	m_transform->SetTransformType(SVGTRANSFORM_TRANSLATE);
	m_transform->SetValuesA12(x, y, FALSE);
}

/* virtual */ void
SVGDOMTransformImpl::SetScale(double sx, double sy)
{
	m_transform->SetTransformType(SVGTRANSFORM_SCALE);
	m_transform->SetValuesA12(sx, sy, FALSE);
}

/* virtual */ void
SVGDOMTransformImpl::SetRotate(double angle, double cx, double cy)
{
	m_transform->SetTransformType(SVGTRANSFORM_ROTATE);
	m_transform->SetValuesA123(angle, cx, cy, FALSE);
}

/* virtual */ void
SVGDOMTransformImpl::SetSkewX(double angle)
{
	m_transform->SetTransformType(SVGTRANSFORM_SKEWX);
	m_transform->SetValuesA1(angle);
}

/* virtual */ void
SVGDOMTransformImpl::SetSkewY(double angle)
{
	m_transform->SetTransformType(SVGTRANSFORM_SKEWY);
	m_transform->SetValuesA1(angle);
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
