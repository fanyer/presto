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
#include "modules/svg/src/svgdom/svgdompreserveaspectratioimpl.h"

SVGDOMPreserveAspectRatioImpl::SVGDOMPreserveAspectRatioImpl(SVGAspectRatio* aspect_val)
	: m_aspect_val(aspect_val)
{
	SVGObject::IncRef(m_aspect_val);
}

/* virtual */
SVGDOMPreserveAspectRatioImpl::~SVGDOMPreserveAspectRatioImpl()
{
	SVGObject::DecRef(m_aspect_val);
}

/* virtual */ const char*
SVGDOMPreserveAspectRatioImpl::GetDOMName()
{
	return "SVGPreserveAspectRatio";
}

/* virtual */ OP_BOOLEAN
SVGDOMPreserveAspectRatioImpl::SetAlign(unsigned short value)
{
	RETURN_FALSE_IF(m_aspect_val->align == (SVGAlignType)value);
	m_aspect_val->align = (SVGAlignType)value;
	return OpBoolean::IS_TRUE;
}

/* virtual */ unsigned short
SVGDOMPreserveAspectRatioImpl::GetAlign()
{
	return m_aspect_val->align;
}

/* virtual */ OP_BOOLEAN
SVGDOMPreserveAspectRatioImpl::SetMeetOrSlice(unsigned short value)
{
	RETURN_FALSE_IF(m_aspect_val->mos == (SVGMeetOrSliceType)value);
	m_aspect_val->mos = (SVGMeetOrSliceType)value;
	return OpBoolean::IS_TRUE;
}

/* virtual */ unsigned short
SVGDOMPreserveAspectRatioImpl::GetMeetOrSlice()
{
	return m_aspect_val->mos;
}

#if 0
SVGAspectRatio*
SVGDOMPreserveAspectRatioImpl::GetAspectRatio()
{
	return m_aspect_val;
}
#endif //0

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
