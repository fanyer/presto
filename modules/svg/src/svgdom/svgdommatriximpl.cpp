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
#include "modules/svg/src/svgdom/svgdommatriximpl.h"

SVGDOMMatrixImpl::SVGDOMMatrixImpl(SVGMatrixObject* matrix) :
		m_matrix(matrix)
{
	OP_ASSERT(m_matrix);
	SVGObject::IncRef(m_matrix);
}

/* virtual */
SVGDOMMatrixImpl::~SVGDOMMatrixImpl()
{
	SVGObject::DecRef(m_matrix);
}

/* virtual */ OP_BOOLEAN
SVGDOMMatrixImpl::SetValue(int idx, double x)
{
	RETURN_FALSE_IF(m_matrix->matrix[idx] == SVGNumber(x));
	m_matrix->matrix[idx] = SVGNumber(x);
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMMatrixImpl::GetValue(int idx)
{
	return m_matrix->matrix[idx].GetFloatValue();
}

/* virtual */ void
SVGDOMMatrixImpl::GetMatrix(SVGMatrix& matrix) const
{
	matrix = m_matrix->matrix;
}

/* virtual */ void
SVGDOMMatrixImpl::SetMatrix(const SVGMatrix& matrix)
{
	m_matrix->matrix = matrix;
}

#endif // SVG_DOM && SVG_SUPPORT


