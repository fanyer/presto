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
#include "modules/svg/src/svgdom/svgdomnumberimpl.h"

SVGDOMNumberImpl::SVGDOMNumberImpl(SVGNumberObject* val) :
		m_number_val(val)
{
	SVGObject::IncRef(m_number_val);
}

/* virtual */
SVGDOMNumberImpl::~SVGDOMNumberImpl()
{
	SVGObject::DecRef(m_number_val);
}

/* virtual */ const char*
SVGDOMNumberImpl::GetDOMName()
{
	return "SVGNumber";
}

/* virtual */ OP_BOOLEAN
SVGDOMNumberImpl::SetValue(double value)
{
	RETURN_FALSE_IF(m_number_val->value == SVGNumber(value));
	m_number_val->value = value;
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMNumberImpl::GetValue()
{
	return m_number_val->value.GetFloatValue();
}

#if 0
SVGNumberObject*
SVGDOMNumberImpl::GetNumber()
{
	return m_number_val;
}
#endif //0

#endif // SVG_DOM && SVG_SUPPORT && SVG_FULL_11
