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
#include "modules/svg/src/svgdom/svgdompaintimpl.h"

SVGDOMCSSPrimitiveValuePaintImpl::SVGDOMCSSPrimitiveValuePaintImpl(SVGPaintObject* paint, PaintChannel channel) :
		m_paint(paint),
		m_channel(channel)
{
	SVGObject::IncRef(m_paint);
}

/* virtual */
SVGDOMCSSPrimitiveValuePaintImpl::~SVGDOMCSSPrimitiveValuePaintImpl()
{
	SVGObject::DecRef(m_paint);
}

/* virtual */ const char* 
SVGDOMCSSPrimitiveValuePaintImpl::GetDOMName()
{
	return "CSSPrimitiveValue";
}

/* virtual */ OP_BOOLEAN
SVGDOMCSSPrimitiveValuePaintImpl::SetFloatValue(UnitType unit_type, double float_value)
{
	UINT32 colval = m_paint->GetPaint().GetRGBColor();

	UINT8 r = GetRValue(colval);
	UINT8 g = GetGValue(colval);
	UINT8 b = GetBValue(colval);

	switch(m_channel)
	{
		case PAINTCHANNEL_RED:
			r = (UINT8)float_value;
			break;
		case PAINTCHANNEL_GREEN:
			b = (UINT8)float_value;
			break;
		case PAINTCHANNEL_BLUE:
			g = (UINT8)float_value;
			break;
		default:
			OP_ASSERT(!"Not reached");
			break;
	}

	m_paint->GetPaint().SetRGBColor(r, g, b);
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGDOMCSSPrimitiveValuePaintImpl::GetFloatValue(UnitType unit_type, double& float_value)
{
	UINT32 colval = m_paint->GetPaint().GetRGBColor();
	switch(m_channel)
	{
		case PAINTCHANNEL_RED:
			float_value = GetRValue(colval);
			break;
		case PAINTCHANNEL_GREEN:
			float_value = GetGValue(colval);
			break;
		case PAINTCHANNEL_BLUE:
			float_value = GetBValue(colval);
			break;
		default:
			OP_ASSERT(!"Not reached");
			break;
	}
	return OpBoolean::IS_TRUE;
}

SVGDOMCSSRGBColorPaintImpl::SVGDOMCSSRGBColorPaintImpl(SVGPaintObject* paint) :
		m_paint(paint)
{
	SVGObject::IncRef(m_paint);
}

/* virtual */
SVGDOMCSSRGBColorPaintImpl::~SVGDOMCSSRGBColorPaintImpl()
{
	SVGObject::DecRef(m_paint);
}

/* virtual */ const char*
SVGDOMCSSRGBColorPaintImpl::GetDOMName()
{
	return "RGBColor";
}

/* virtual */ SVGDOMCSSPrimitiveValue*
SVGDOMCSSRGBColorPaintImpl::GetRed()
{
	return OP_NEW(SVGDOMCSSPrimitiveValuePaintImpl, (m_paint, SVGDOMCSSPrimitiveValuePaintImpl::PAINTCHANNEL_RED));
}

/* virtual */ SVGDOMCSSPrimitiveValue*
SVGDOMCSSRGBColorPaintImpl::GetGreen()
{
	return OP_NEW(SVGDOMCSSPrimitiveValuePaintImpl, (m_paint, SVGDOMCSSPrimitiveValuePaintImpl::PAINTCHANNEL_GREEN));
}

/* virtual */ SVGDOMCSSPrimitiveValue*
SVGDOMCSSRGBColorPaintImpl::GetBlue()
{
	return OP_NEW(SVGDOMCSSPrimitiveValuePaintImpl, (m_paint, SVGDOMCSSPrimitiveValuePaintImpl::PAINTCHANNEL_BLUE));
}

SVGDOMPaintImpl::SVGDOMPaintImpl(SVGPaintObject* paint) :
		m_paint(paint)
{
	SVGObject::IncRef(m_paint);
}

/* virtual */
SVGDOMPaintImpl::~SVGDOMPaintImpl()
{
	SVGObject::DecRef(m_paint);
}

/* virtual */ const char*
SVGDOMPaintImpl::GetDOMName()
{
	return "SVGPaint";
}

/* virtual */ SVGDOMColor::ColorType
SVGDOMPaintImpl::GetColorType()
{
	switch((SVGDOMPaint::PaintType)m_paint->GetPaint().GetPaintType())
	{
		case SVGPaint::RGBCOLOR:
			return SVG_COLORTYPE_RGBCOLOR;
		case SVGPaint::RGBCOLOR_ICCCOLOR:
			return SVG_COLORTYPE_RGBCOLOR_ICCCOLOR;
		case SVGPaint::CURRENT_COLOR:
			return SVG_COLORTYPE_CURRENTCOLOR;
		default:
			// Fallback
			return SVG_COLORTYPE_UNKNOWN;
			break;
	}
}

/* virtual */ OP_BOOLEAN
SVGDOMPaintImpl::GetRGBColor(SVGDOMCSSRGBColor*& color)
{
	color = OP_NEW(SVGDOMCSSRGBColorPaintImpl, (m_paint));
	return OpBoolean::IS_TRUE;
}

/* virtual */ OP_BOOLEAN
SVGDOMPaintImpl::SetRGBColor(const uni_char* rgb_color)
{
	OP_BOOLEAN status = m_paint->GetPaint().SetPaint(SVGPaint::RGBCOLOR,
													 NULL, rgb_color, NULL);
	return status;
}

/* virtual */ OP_BOOLEAN
SVGDOMPaintImpl::SetColor(ColorType color_type, const uni_char* rgb_color, uni_char* icc_color)
{
	OP_BOOLEAN status = OpBoolean::IS_TRUE;
	switch(color_type)
	{
		case SVG_COLORTYPE_RGBCOLOR:
			status = m_paint->GetPaint().SetPaint(SVGPaint::RGBCOLOR, NULL, rgb_color, NULL);
			break;
		case SVG_COLORTYPE_RGBCOLOR_ICCCOLOR:
			status = m_paint->GetPaint().SetPaint(SVGPaint::RGBCOLOR_ICCCOLOR, NULL, rgb_color, icc_color);
			break;
		case SVG_COLORTYPE_CURRENTCOLOR:
			m_paint->GetPaint().SetPaintType(SVGPaint::CURRENT_COLOR);
			break;
	}

	if (OpStatus::IsSuccess(status))
	{
		return OpBoolean::IS_TRUE;
	}
	else
		return status;
}

/* virtual */ OP_STATUS
SVGDOMPaintImpl::GetCSSText(TempBuffer *buffer)
{
	return m_paint->GetPaint().GetStringRepresentation(buffer);
}

/* virtual */ OP_STATUS
SVGDOMPaintImpl::SetCSSText(uni_char* buffer)
{
	// share this with some css helpers?
	OP_ASSERT(!"Not implemented yet");
	return OpSVGStatus::NOT_YET_IMPLEMENTED;
}

/* virtual */ SVGDOMCSSValue::UnitType
SVGDOMPaintImpl::GetCSSValueType()
{
	return SVG_DOM_CSS_CUSTOM;
}

/* virtual */ SVGDOMPaint::PaintType
SVGDOMPaintImpl::GetPaintType()
{
	return (SVGDOMPaint::PaintType)m_paint->GetPaint().GetPaintType();
}

/* virtual */ const uni_char*
SVGDOMPaintImpl::GetURI()
{
	return m_paint->GetPaint().GetURI();
}

/* virtual */ OP_STATUS
SVGDOMPaintImpl::SetURI(const uni_char* uri)
{
	OP_ASSERT(uri);
	RETURN_IF_MEMORY_ERROR(m_paint->GetPaint().SetPaint(SVGPaint::URI_NONE, uri, NULL, NULL));
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
SVGDOMPaintImpl::SetPaint(PaintType type, const uni_char* uri,
						  const uni_char* rgb_color, const uni_char* icc_color)
{
	OP_BOOLEAN status = m_paint->GetPaint().SetPaint((SVGPaint::PaintType)type,
													 uri, rgb_color, icc_color);
	return status;
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_FULL_11
