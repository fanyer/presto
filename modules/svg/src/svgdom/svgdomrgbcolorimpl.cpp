/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#if defined(SVG_SUPPORT) && defined(SVG_DOM) && defined(SVG_TINY_12)

#include "modules/svg/src/svgpch.h"
#include "modules/svg/src/SVGPaint.h"
#include "modules/svg/src/svgdom/svgdomrgbcolorimpl.h"

SVGDOMRGBColorImpl::SVGDOMRGBColorImpl(SVGObject* color) :
	m_color_val(color)
{
	OP_ASSERT(color->Type() == SVGOBJECT_PAINT || color->Type() == SVGOBJECT_COLOR);
	SVGObject::IncRef(m_color_val);
}

/* virtual */
SVGDOMRGBColorImpl::~SVGDOMRGBColorImpl()
{
	SVGObject::DecRef(m_color_val);
}

/* virtual */ const char*
SVGDOMRGBColorImpl::GetDOMName()
{
	return "SVGRGBColor";
}

OP_STATUS
SVGDOMRGBColorImpl::GetRGBColor(UINT32 &rgbcolor)
{
	if(m_color_val->Type() == SVGOBJECT_PAINT)
	{
		SVGPaint& paint = static_cast<SVGPaintObject*>(m_color_val)->GetPaint();
		// It's possible though unlikely that URI_RGBCOLOR, URI_RGBCOLOR_ICCCOLOR should be here too.
		if (paint.GetPaintType() == SVGPaint::RGBCOLOR ||
			paint.GetPaintType() == SVGPaint::RGBCOLOR_ICCCOLOR)
		{
			rgbcolor = paint.GetRGBColor();
		}
		else
			return OpStatus::ERR_OUT_OF_RANGE;
	}
	else if(m_color_val->Type() == SVGOBJECT_COLOR)
	{
		SVGColor& color = static_cast<SVGColorObject*>(m_color_val)->GetColor();
		if (color.GetColorType() == SVGColor::SVGCOLOR_RGBCOLOR ||
			color.GetColorType() == SVGColor::SVGCOLOR_RGBCOLOR_ICCCOLOR)
		{
			rgbcolor = color.GetRGBColor();
		}
		else
			return OpStatus::ERR_OUT_OF_RANGE;
	}
	else
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

/* virtual */ OP_STATUS 
SVGDOMRGBColorImpl::GetComponent(int type, double& val)
{
	if(m_color_val)
	{
		UINT32 rgbcolor;
		RETURN_IF_ERROR(GetRGBColor(rgbcolor));

		switch(type)
		{
			case 0:
				val = GetRValue(rgbcolor);
				break;
			case 1:
				val = GetGValue(rgbcolor);
				break;
			case 2:
				val = GetBValue(rgbcolor);
				break;
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

/* virtual */ OP_STATUS 
SVGDOMRGBColorImpl::SetComponent(int type, double val)
{
	if(m_color_val)
	{
		UINT32 rgbcolor;
		RETURN_IF_ERROR(GetRGBColor(rgbcolor));
		
		UINT8 r = GetRValue(rgbcolor);
		UINT8 g = GetGValue(rgbcolor);
		UINT8 b = GetBValue(rgbcolor);

		switch(type)
		{
			case 0:
				r = (UINT8)val;
				break;
			case 1:
				g = (UINT8)val;
				break;
			case 2:
				b = (UINT8)val;
				break;
		}

		if(m_color_val->Type() == SVGOBJECT_PAINT)
		{
			SVGPaint& paint = static_cast<SVGPaintObject*>(m_color_val)->GetPaint();
			paint.SetRGBColor(r, g, b);
		}
		else if(m_color_val->Type() == SVGOBJECT_COLOR)
		{
			SVGColor& color = static_cast<SVGColorObject*>(m_color_val)->GetColor();
			color.SetRGBColor(r, g, b);
		}

		return OpStatus::OK;
	}
	return OpStatus::ERR;
}

#endif // SVG_SUPPORT && SVG_DOM && SVG_TINY_12
