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
#include "modules/svg/src/svgdom/svgdomenumerationimpl.h"

SVGDOMEnumerationImpl::SVGDOMEnumerationImpl(SVGEnum* enum_val) :
		m_enum_val(enum_val)
{
	SVGObject::IncRef(m_enum_val);
}

/* virtual */
SVGDOMEnumerationImpl::~SVGDOMEnumerationImpl()
{
	SVGObject::DecRef(m_enum_val);
}

/* virtual */ const char*
SVGDOMEnumerationImpl::GetDOMName()
{
	return ""; // FIXME: Is this correct?
}

/* virtual */ OP_BOOLEAN
SVGDOMEnumerationImpl::SetValue(unsigned short value)
{
	RETURN_FALSE_IF(m_enum_val->EnumValue() == value);
	m_enum_val->SetEnumValue(value);
	return OpBoolean::IS_TRUE;
}

/* virtual */ unsigned short
SVGDOMEnumerationImpl::GetValue()
{
	return m_enum_val->EnumValue();
}

#if 0
SVGEnum*
SVGDOMEnumerationImpl::GetEnum()
{
	return m_enum_val;
}
#endif // 0

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
