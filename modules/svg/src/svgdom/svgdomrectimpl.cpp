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
#include "modules/svg/src/svgdom/svgdomrectimpl.h"

SVGDOMRectImpl::SVGDOMRectImpl(SVGRectObject* rect) :
		m_rect(rect)
{
	SVGObject::IncRef(m_rect);
}

/* virtual */
SVGDOMRectImpl::~SVGDOMRectImpl()
{
	SVGObject::DecRef(m_rect);
}

/* virtual */ const char*
SVGDOMRectImpl::GetDOMName()
{
	return "SVGRect";
}

/* virtual */ OP_BOOLEAN
SVGDOMRectImpl::SetX(double x)
{
	RETURN_FALSE_IF(m_rect->rect.x == SVGNumber(x));
	m_rect->rect.x = x;
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMRectImpl::GetX()
{
	return m_rect->rect.x.GetFloatValue();
}

/* virtual */ OP_BOOLEAN
SVGDOMRectImpl::SetY(double y)
{
	RETURN_FALSE_IF(m_rect->rect.y == SVGNumber(y));
	m_rect->rect.y = y;
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMRectImpl::GetY()
{
	return m_rect->rect.y.GetFloatValue();
}

/* virtual */ OP_BOOLEAN
SVGDOMRectImpl::SetWidth(double width)
{
	RETURN_FALSE_IF(m_rect->rect.width == SVGNumber(width));
	m_rect->rect.width = width;
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMRectImpl::GetWidth()
{
	return m_rect->rect.width.GetFloatValue();
}

/* virtual */ OP_BOOLEAN
SVGDOMRectImpl::SetHeight(double height)
{
	RETURN_FALSE_IF(m_rect->rect.height == SVGNumber(height));
	m_rect->rect.height = height;
	return OpBoolean::IS_TRUE;
}

/* virtual */ double
SVGDOMRectImpl::GetHeight()
{
	return m_rect->rect.height.GetFloatValue();
}

#endif // SVG_SUPPORT && SVG_DOM
