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
#include "modules/svg/src/svgdom/svgdommatrixtransformimpl.h"

SVGDOMMatrixTransformImpl::SVGDOMMatrixTransformImpl(SVGTransform* transform) :
		m_transform(transform)
{
	SVGObject::IncRef(m_transform);
}

/* virtual */
SVGDOMMatrixTransformImpl::~SVGDOMMatrixTransformImpl()
{
	SVGObject::DecRef(m_transform);
}

/* virtual */ OP_BOOLEAN
SVGDOMMatrixTransformImpl::SetValue(int idx, double x)
{
	SVGMatrix tmp;
	m_transform->GetMatrix(tmp);

	if (m_transform->GetTransformType() != SVGTRANSFORM_MATRIX)
	{
		m_transform->SetMatrix(tmp);
	}
	else
	{
		RETURN_FALSE_IF(tmp[idx] == SVGNumber(x));
	}

	m_transform->values[idx] = SVGNumber(x);
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMMatrixTransformImpl::GetValue(int idx)
{
	SVGMatrix tmp;
	m_transform->GetMatrix(tmp);
	return tmp[idx].GetFloatValue();
}

/* virtual */ void
SVGDOMMatrixTransformImpl::GetMatrix(SVGMatrix& matrix) const
{
	m_transform->GetMatrix(matrix);
}

/* virtual */ void
SVGDOMMatrixTransformImpl::SetMatrix(const SVGMatrix& matrix)
{
	m_transform->SetMatrix(matrix);
}

#endif // SVG_DOM && SVG_SUPPORT && SVG_FULL_11
