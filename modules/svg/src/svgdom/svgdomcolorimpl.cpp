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
#include "modules/svg/src/svgdom/svgdomcolorimpl.h"

SVGDOMCSSPrimitiveValueColorImpl::SVGDOMCSSPrimitiveValueColorImpl(SVGColorObject* color_val, ColorChannel channel) :
		m_color_val(color_val), m_channel(channel)
{
	SVGObject::IncRef(m_color_val);
}
	
SVGDOMCSSPrimitiveValueColorImpl::~SVGDOMCSSPrimitiveValueColorImpl()
{
	SVGObject::DecRef(m_color_val);
}

/* virtual */ const char*
SVGDOMCSSPrimitiveValueColorImpl::GetDOMName()
{
	return "PrimitiveValue";
}

/* virtual */ SVGDOMCSSPrimitiveValue::UnitType
SVGDOMCSSPrimitiveValueColorImpl::GetPrimitiveType()
{
	return SVG_DOM_CSS_NUMBER;
}

/* virtual */ OP_BOOLEAN
SVGDOMCSSPrimitiveValueColorImpl::SetFloatValue(UnitType unit_type, double float_value)
{
	UINT32 colval = m_color_val->GetColor().GetRGBColor();

	UINT8 r = GetRValue(colval);
	UINT8 g = GetGValue(colval);
	UINT8 b = GetBValue(colval);

	switch(m_channel)
	{
		case COLORCHANNEL_RED:
			r = (UINT8)float_value;
			break;
		case COLORCHANNEL_GREEN:
			b = (UINT8)float_value;
			break;
		case COLORCHANNEL_BLUE:
			g = (UINT8)float_value;
			break;
		default:
			OP_ASSERT(!"Not reached");
			break;
	}

	m_color_val->GetColor().SetRGBColor(r, g, b);
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGDOMCSSPrimitiveValueColorImpl::GetFloatValue(UnitType unit_type, double& float_value)
{
	UINT32 colval = m_color_val->GetColor().GetRGBColor();
	switch(m_channel)
	{
		case COLORCHANNEL_RED:
			float_value = GetRValue(colval);
			break;
		case COLORCHANNEL_GREEN:
			float_value = GetGValue(colval);
			break;
		case COLORCHANNEL_BLUE:
			float_value = GetBValue(colval);
			break;
		default:
			OP_ASSERT(!"Not reached");
			break;
	}
	return OpBoolean::IS_TRUE;
}

SVGDOMCSSRGBColorImpl::SVGDOMCSSRGBColorImpl(SVGColorObject* color_val) :
		m_color_val(color_val)
{
	SVGObject::IncRef(m_color_val);
}

SVGDOMCSSRGBColorImpl::~SVGDOMCSSRGBColorImpl()
{
	SVGObject::DecRef(m_color_val);
}

/* virtual */ const char*
SVGDOMCSSRGBColorImpl::GetDOMName()
{
	return "RGBColor";
}

/* virtual */ SVGDOMCSSPrimitiveValue*
SVGDOMCSSRGBColorImpl::GetRed()
{
	return OP_NEW(SVGDOMCSSPrimitiveValueColorImpl, (m_color_val, SVGDOMCSSPrimitiveValueColorImpl::COLORCHANNEL_RED));
}

/* virtual */ SVGDOMCSSPrimitiveValue*
SVGDOMCSSRGBColorImpl::GetGreen()
{
	return OP_NEW(SVGDOMCSSPrimitiveValueColorImpl, (m_color_val, SVGDOMCSSPrimitiveValueColorImpl::COLORCHANNEL_GREEN));
}

/* virtual */ SVGDOMCSSPrimitiveValue*
SVGDOMCSSRGBColorImpl::GetBlue()
{
	return OP_NEW(SVGDOMCSSPrimitiveValueColorImpl, (m_color_val, SVGDOMCSSPrimitiveValueColorImpl::COLORCHANNEL_BLUE));
}

SVGDOMColorImpl::SVGDOMColorImpl(SVGColorObject* color_val) : 
		m_color_val(color_val)
{
	SVGObject::IncRef(m_color_val);
}

/* virtual */
SVGDOMColorImpl::~SVGDOMColorImpl()
{
	SVGObject::DecRef(m_color_val);
}

/* virtual */ const char*
SVGDOMColorImpl::GetDOMName()
{
	return "SVGColor";
}

/* virtual */ SVGDOMColor::ColorType
SVGDOMColorImpl::GetColorType()
{
	return (SVGDOMColor::ColorType)m_color_val->GetColor().GetColorType();
}

/* virtual */ OP_BOOLEAN
SVGDOMColorImpl::GetRGBColor(SVGDOMCSSRGBColor*& rgb_color)
{
	rgb_color = OP_NEW(SVGDOMCSSRGBColorImpl, (m_color_val));
	return rgb_color ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

/* virtual */ OP_BOOLEAN
SVGDOMColorImpl::SetRGBColor(const uni_char* rgb_color)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ OP_BOOLEAN
SVGDOMColorImpl::SetColor(ColorType color_type, const uni_char* rgb_color, uni_char* icc_color)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ OP_STATUS
SVGDOMColorImpl::GetCSSText(TempBuffer *buffer)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ OP_STATUS
SVGDOMColorImpl::SetCSSText(uni_char *buffer)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/* virtual */ SVGDOMCSSValue::UnitType
SVGDOMColorImpl::GetCSSValueType()
{
	return SVG_DOM_CSS_CUSTOM;
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
