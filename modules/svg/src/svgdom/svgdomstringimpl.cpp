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
#include "modules/svg/src/svgdom/svgdomstringimpl.h"

SVGDOMStringImpl::SVGDOMStringImpl(SVGString* str_val) :
		m_str_val(str_val)
{
	SVGObject::IncRef(m_str_val);
}

SVGDOMStringImpl::~SVGDOMStringImpl()
{
	SVGObject::DecRef(m_str_val);
}

/* virtual */ const char*
SVGDOMStringImpl::GetDOMName()
{
	return "SVGString";
}

/* virtual */ OP_BOOLEAN
SVGDOMStringImpl::SetValue(const uni_char* str)
{
// 	RETURN_FALSE_IF(uni_strcmp(str, m_str_val->GetString()) == 0);
	if (OpStatus::IsMemoryError(m_str_val->SetString(str, uni_strlen(str))))
		return OpStatus::ERR_NO_MEMORY;
	return OpBoolean::IS_TRUE;
}

/* virtual */ const uni_char *
SVGDOMStringImpl::GetValue()
{
	return m_str_val->GetString();
}

#endif // SVG_DOM && SVG_SUPPORT && SVG_FULL_11
