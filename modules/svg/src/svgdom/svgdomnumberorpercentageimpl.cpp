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
#include "modules/svg/src/svgdom/svgdomnumberorpercentageimpl.h"

SVGDOMNumberOrPercentageImpl::SVGDOMNumberOrPercentageImpl(SVGLengthObject* length) :
		m_length(length)
{
	SVGObject::IncRef(m_length);
}

/* virtual */
SVGDOMNumberOrPercentageImpl::~SVGDOMNumberOrPercentageImpl()
{
	SVGObject::DecRef(m_length);
}

/* virtual */ const char*
SVGDOMNumberOrPercentageImpl::GetDOMName()
{
	return "SVGNumber";
}

/* virtual */ OP_BOOLEAN
SVGDOMNumberOrPercentageImpl::SetValue(double value)
{
	// What to do here? It is not known wheter value is in percentage
	// or in the interval (0..1)
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ double
SVGDOMNumberOrPercentageImpl::GetValue()
{
	return m_length->GetScalar().GetFloatValue();
}

#endif // SVG_DOM && SVG_SUPPORT && SVG_FULL_11
