/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
**
** Implementations of functions and classes in svg_external_types.h
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/svg_external_types.h"
#include "modules/svg/src/SVGCanvasState.h" // For color unpacking macros
#include "modules/layout/cascade.h" // LayoutProperties
#include "modules/logdoc/htm_elm.h"

/* static */
SVGPropertyReference*
SVGPropertyReference::IncRef(SVGPropertyReference* propref)
{
	if (propref && !propref->m_refdata.protect)
		++propref->m_refdata.refcount;

	return propref;
}

/* static */
void SVGPropertyReference::DecRef(SVGPropertyReference* propref)
{
#ifdef _DEBUG
	if (propref)
	{
		OP_ASSERT(propref->m_refdata.refcount >= 0);
	}
#endif // _DEBUG

	if (propref && !propref->m_refdata.protect && --propref->m_refdata.refcount == 0)
		OP_DELETE(propref);
}

/* static */
SVGPropertyReference*
SVGPropertyReference::Protect(SVGPropertyReference* propref)
{
	if (propref)
		propref->m_refdata.protect = 1;

	return propref;
}

SVGColor::SVGColor(ColorType type, COLORREF color, UINT32 icccolor) :
			m_color_type(type),
			m_color(color),
			m_icccolor(icccolor)
{
}

BOOL
SVGColor::operator==(const SVGColor& other) const
{
	return (m_color == other.m_color &&
			m_color_type == other.m_color_type &&
			m_icccolor == other.m_icccolor);
}

void
SVGColor::Copy(const SVGColor& c)
{
	m_color = c.m_color;
	m_color_type = c.m_color_type;
	m_icccolor = c.m_icccolor;
}

void
SVGColor::SetRGBColor(UINT8 r, UINT8 g, UINT8 b)
{
	m_color = OP_RGB(r, g, b);
}

void
SVGColor::SetRGBColor(UINT32 color)
{
	m_color = OP_RGBA(GetSVGColorRValue(color),
					  GetSVGColorGValue(color),
					  GetSVGColorBValue(color),
					  GetSVGColorAValue(color));
}

UINT32
SVGColor::GetRGBColor() const
{
	UINT32 color = HTM_Lex::GetColValByIndex(m_color);
	return (OP_GET_A_VALUE(color) << 24) | (OP_GET_B_VALUE(color) << 16) | (OP_GET_G_VALUE(color) << 8) | OP_GET_R_VALUE(color);
}

OP_STATUS
SVGColor::GetStringRepresentation(TempBuffer* buffer) const
{
	if(m_color_type == SVGCOLOR_RGBCOLOR)
	{
		if (m_color & CSS_COLOR_KEYWORD_TYPE_named &&
			!(m_color & ~(CSS_COLOR_KEYWORD_TYPE_named |
						  CSS_COLOR_KEYWORD_TYPE_index)))
		{
			const uni_char* colorstr = NULL;
			if((m_color & CSS_COLOR_KEYWORD_TYPE_ui_color) == CSS_COLOR_KEYWORD_TYPE_ui_color)
			{
				colorstr = CSS_GetKeywordString((short)(m_color & CSS_COLOR_KEYWORD_TYPE_index));
			}
			else if(m_color & CSS_COLOR_KEYWORD_TYPE_x_color)
			{
				colorstr = HTM_Lex::GetColNameByIndex(m_color & CSS_COLOR_KEYWORD_TYPE_index);
			}
#ifdef _DEBUG
			else
			{
				OP_ASSERT(!"Unknown color type");
			}
#endif			
			if(colorstr)
			{
				return buffer->Append(colorstr);
			}
		}
		else
		{
			OpString8 a;
			if(OP_GET_A_VALUE(m_color) != 255)
			{
				RETURN_IF_ERROR(a.AppendFormat("rgba(%d,%d,%d,%d)",
					OP_GET_R_VALUE(m_color),
					OP_GET_G_VALUE(m_color),
					OP_GET_B_VALUE(m_color),
					OP_GET_A_VALUE(m_color)));
			}
			else
			{
				RETURN_IF_ERROR(a.AppendFormat("#%02x%02x%02x",
					GetRValue(m_color),
					GetGValue(m_color),
					GetBValue(m_color)));
			}
			return buffer->Append(a.CStr(), a.Length());
		}
	}
	else if(m_color_type == SVGCOLOR_CURRENT_COLOR)
	{
		return buffer->Append("currentColor");
	}
	else if(m_color_type == SVGCOLOR_RGBCOLOR_ICCCOLOR)
	{
		// FIXME: Generate this string
	}
	else if(m_color_type == SVGCOLOR_NONE)
	{
		return buffer->Append("none");
	}

	OP_ASSERT(!"Unknown or unsupported color type");
	return OpStatus::ERR;
}

OP_STATUS
SVGColor::SetFromCSSDecl(CSS_decl* cp, SVGColor* parent_color)
{
	if(!cp)
		return OpStatus::ERR;

	if(cp->GetDeclType() == CSS_DECL_LONG || cp->GetDeclType() == CSS_DECL_COLOR)
	{
		m_color = cp->LongValue();
		SetColorType(SVGCOLOR_RGBCOLOR);
	}
	else if(cp->GetDeclType() == CSS_DECL_TYPE)
	{
		switch(cp->TypeValue())
		{
			case CSS_VALUE_currentColor:
				SetColorType(SVGCOLOR_CURRENT_COLOR);
				break;
			case CSS_VALUE_inherit:
				SetColorType(SVGCOLOR_UNKNOWN);
				if(parent_color)
				{
					Copy(*parent_color);
				}
				else
				{
					//SetFlag(SVGOBJECTFLAG_INHERIT);
				}
				break;
		}
	}
	return OpStatus::OK;
}

OP_STATUS SVGColor::GetCSSDecl(CSS_decl** out_cd, HTML_Element* element, short property, 
						 BOOL is_current_style) const
{
	switch(m_color_type)
	{
		case SVGColor::SVGCOLOR_RGBCOLOR:
		case SVGColor::SVGCOLOR_RGBCOLOR_ICCCOLOR:
			*out_cd = LayoutProperties::GetComputedColorDecl(property, GetColorRef()
#ifdef CURRENT_STYLE_SUPPORT
				, is_current_style
#endif // CURRENT_STYLE_SUPPORT
				);
			break;
		case SVGColor::SVGCOLOR_NONE:
			*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			break;
		default:
			OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
	}

	if(!*out_cd)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

#ifdef SVG_SUPPORT_FILTERS
SVGEnableBackground::SVGEnableBackground(SVGEnableBackground::EnableBackgroundType t /* = SVGENABLEBG_ACCUMULATE */) :
	m_eb_info_init(0)
{
	SetType(t);
}

SVGEnableBackground::SVGEnableBackground(SVGEnableBackground::EnableBackgroundType t,
										 SVGNumber x, SVGNumber y,
										 SVGNumber width, SVGNumber height) :
	m_eb_info_init(0)
{
	Set(t, x, y, width, height);
}

void
SVGEnableBackground::Copy(const SVGEnableBackground& other)
{
	m_eb_info_init = other.m_eb_info_init;
	m_x = other.m_x;
	m_y = other.m_y;
	m_width = other.m_width;
	m_height = other.m_height;
}

BOOL
SVGEnableBackground::operator==(const SVGEnableBackground& other) const
{
	return m_x == other.m_x &&
		m_y == other.m_y &&
		m_width == other.m_width &&
		m_height == other.m_height &&
		info.desc == other.info.desc &&
		info.bgrect_valid == other.info.bgrect_valid;
}

void
SVGEnableBackground::Set(SVGEnableBackground::EnableBackgroundType type, SVGNumber x, SVGNumber y, SVGNumber width, SVGNumber height)
{
	SetType(type);
	m_x = x;
	m_y = y;
	m_width = width;
	m_height = height;
	info.bgrect_valid = 1;
}

OP_STATUS
SVGEnableBackground::GetStringRepresentation(TempBuffer* buffer) const
{
	if((EnableBackgroundType)info.desc == SVGENABLEBG_ACCUMULATE)
	{
		return buffer->Append("accumulate");
	}
	else
	{
		RETURN_IF_ERROR(buffer->Append("new"));
		if (IsRectValid())
		{
			RETURN_IF_ERROR(buffer->Append(' '));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(m_x.GetFloatValue()));
			RETURN_IF_ERROR(buffer->Append(' '));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(m_y.GetFloatValue()));
			RETURN_IF_ERROR(buffer->Append(' '));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(m_width.GetFloatValue()));
			RETURN_IF_ERROR(buffer->Append(' '));
			RETURN_IF_ERROR(buffer->AppendDoubleToPrecision(m_height.GetFloatValue()));
		}
		return OpStatus::OK;
	}
}
#endif // SVG_SUPPORT_FILTERS

OP_STATUS
SVGBaselineShift::GetStringRepresentation(TempBuffer* buffer) const
{
	if (m_shift_type == SVGBASELINESHIFT_SUB)
	{
		return buffer->Append("sub");
	}
	else if (m_shift_type == SVGBASELINESHIFT_SUPER)
	{
		return buffer->Append("super");
	}
	else if (m_shift_type == SVGBASELINESHIFT_VALUE)
	{
		return m_value.GetStringRepresentation(buffer);
	}
	else /* m_shift_type == SVGBASELINESHIFT_BASELINE */
	{
		return buffer->Append("baseline");
	}
}

BOOL
SVGBaselineShift::operator==(const SVGBaselineShift &other) const
{
	if (m_shift_type != other.m_shift_type)
		return FALSE;

	if (m_shift_type == SVGBASELINESHIFT_VALUE)
	{
		return m_value == other.m_value;
	}
	else
	{
		return TRUE;
	}
}

#endif // SVG_SUPPORT
