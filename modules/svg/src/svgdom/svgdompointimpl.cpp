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
#include "modules/svg/src/svgdom/svgdompointimpl.h"

SVGDOMPointImpl::SVGDOMPointImpl(SVGPoint* point) :
		m_point(point)
{
	SVGObject::IncRef(m_point);
}

/* virtual */
SVGDOMPointImpl::~SVGDOMPointImpl()
{
	SVGObject::DecRef(m_point);
}

/* virtual */ const char*
SVGDOMPointImpl::GetDOMName()
{
	return "SVGPoint";
}

/* virtual */ OP_BOOLEAN
SVGDOMPointImpl::SetX(double x)
{
	RETURN_FALSE_IF(m_point->x == SVGNumber(x));
	m_point->x = x;
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMPointImpl::GetX()
{
	return m_point->x.GetFloatValue();
}

/* virtual */ OP_BOOLEAN
SVGDOMPointImpl::SetY(double y)
{
	RETURN_FALSE_IF(m_point->y == SVGNumber(y));
	m_point->y = y;
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMPointImpl::GetY()
{
	return m_point->y.GetFloatValue();
}

/* virtual */ OP_BOOLEAN
SVGDOMPointImpl::MatrixTransform(SVGDOMMatrix* matrix, SVGDOMPoint* target_point)
{
	OP_ASSERT(matrix != NULL);
	OP_ASSERT(target_point != NULL);

	SVGDOMMatrixBase* matrix_base = static_cast<SVGDOMMatrixBase*>(matrix);

	SVGMatrix svgmatrix;
	matrix_base->GetMatrix(svgmatrix);

	SVGDOMPointImpl* point_impl = static_cast<SVGDOMPointImpl*>(target_point);

	SVGNumberPair point(m_point->x, m_point->y);
	SVGNumberPair tfmpoint = svgmatrix.ApplyToCoordinate(point);
	point_impl->m_point->x = tfmpoint.x;
	point_impl->m_point->y = tfmpoint.y;
	return OpBoolean::IS_TRUE;
}

#endif // SVG_SUPPORT && SVG_DOM
