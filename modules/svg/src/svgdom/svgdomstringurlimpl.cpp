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
#include "modules/svg/src/svgdom/svgdomstringurlimpl.h"

SVGDOMStringUrlImpl::SVGDOMStringUrlImpl(SVGURL* url) :
		m_url(url)
{
	SVGObject::IncRef(m_url);
}

SVGDOMStringUrlImpl::~SVGDOMStringUrlImpl()
{
	SVGObject::DecRef(m_url);
}

/* virtual */ const char*
SVGDOMStringUrlImpl::GetDOMName()
{
	return "SVGString";
}

/* virtual */ OP_BOOLEAN
SVGDOMStringUrlImpl::SetValue(const uni_char* str)
{
	if (uni_strcmp(str, m_url->GetAttrString()) != 0)
	{
		RETURN_IF_ERROR(m_url->SetURL(str, URL()));
		return OpBoolean::IS_TRUE;
	}
	return OpBoolean::IS_FALSE;
}

/* virtual */ const uni_char* 
SVGDOMStringUrlImpl::GetValue()
{
	return m_url->GetAttrString();
}

#endif // SVG_DOM && SVG_SUPPORT && SVG_FULL_11
