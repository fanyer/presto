/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.	It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGManagerImpl.h"
#include "modules/svg/src/SVGPaint.h"
#include "modules/svg/src/SVGAttributeParser.h"
#include "modules/layout/layoutprops.h"
#include "modules/layout/cascade.h" // LayoutProperties

#include "modules/logdoc/htm_elm.h"

#include "modules/util/tempbuf.h"

SVGPaint::SVGPaint(SVGPaint::PaintType t) :
		m_paint_type(t),
		m_uri(NULL),
		m_color(OP_RGB(0,0,0)),
		m_icccolor(OP_RGB(0,0,0))
{
}

SVGPaint::~SVGPaint()
{
	OP_DELETEA(m_uri);
}

void
SVGPaint::SetRGBColor(UINT8 r, UINT8 g, UINT8 b)
{
	m_color = OP_RGB(r, g, b);
}

UINT32
SVGPaint::GetRGBColor() const
{
	UINT32 color = HTM_Lex::GetColValByIndex(m_color);
	return (OP_GET_A_VALUE(color) << 24) | (OP_GET_B_VALUE(color) << 16) | (OP_GET_G_VALUE(color) << 8) | OP_GET_R_VALUE(color);
}

OP_STATUS SVGPaint::GetCSSDecl(CSS_decl** out_cd, HTML_Element* element, short property, 
						 BOOL is_current_style) const
{
	switch(m_paint_type)
	{
		case SVGPaint::RGBCOLOR_ICCCOLOR:
		case SVGPaint::RGBCOLOR:
			*out_cd = LayoutProperties::GetComputedColorDecl(property, GetColorRef()
#ifdef CURRENT_STYLE_SUPPORT
				, is_current_style
#endif // CURRENT_STYLE_SUPPORT
				);
			break;
		case SVGPaint::NONE:
			*out_cd = OP_NEW(CSS_type_decl, (property, CSS_VALUE_none));
			break;
		case SVGPaint::URI_NONE:
		case SVGPaint::URI_RGBCOLOR:
		case SVGPaint::URI_RGBCOLOR_ICCCOLOR:
			// FIXME: Should the above be handled in a special way?
		case SVGPaint::URI:
			{
				*out_cd = OP_NEW(CSS_string_decl, (property, CSS_string_decl::StringDeclUrl));
				if (*out_cd && OpStatus::IsMemoryError(((CSS_string_decl *)(*out_cd))->CopyAndSetString(m_uri, uni_strlen(m_uri))))
				{
					OP_DELETE(*out_cd);
					*out_cd = NULL;
				}
			}
			break;
		default:
			OP_ASSERT(!"The cascade contains a non-computed value, which is an error.");
	}

	if(!*out_cd)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

/* static */
OP_STATUS SVGPaint::CloneFromCSSDecl(CSS_decl* cp, SVGPropertyReference*& paintref,
									 const SVGPaint* parent_paint)
{
	if(!cp)
		return OpStatus::ERR;

	paintref = cp->GetProperty() == CSS_PROPERTY_fill ? &g_svg_manager_impl->default_fill_prop : &g_svg_manager_impl->default_stroke_prop;

	OP_STATUS status = OpStatus::OK;
	PaintType paint_type = SVGPaint::UNKNOWN;

	if(cp->GetDeclType() == CSS_DECL_LONG || cp->GetDeclType() == CSS_DECL_COLOR)
	{
		paint_type = SVGPaint::RGBCOLOR;
	}
	else if(cp->GetDeclType() == CSS_DECL_TYPE)
	{
		switch(cp->TypeValue())
		{
		case CSS_VALUE_none:
			paint_type = SVGPaint::NONE;
			break;
		case CSS_VALUE_currentColor:
			paint_type = SVGPaint::CURRENT_COLOR;
			break;
		case CSS_VALUE_inherit:
			paint_type = SVGPaint::INHERIT;
			break;
		}
	}
	else if(cp->GetDeclType() == CSS_DECL_STRING)
	{
		paint_type = SVGPaint::URI;
	}

	if (paint_type == SVGPaint::INHERIT && parent_paint)
	{
		paintref = SVGPropertyReference::IncRef(const_cast<SVGPaint*>(parent_paint));
	}
	else
	{
		// Create new paint
		SVGPaint* p = OP_NEW(SVGPaint, ());
		if (!p)
			return OpStatus::ERR_NO_MEMORY;

		paintref = SVGPropertyReference::IncRef(p);

		if (cp->GetDeclType() == CSS_DECL_GEN_ARRAY && cp->ArrayValueLen() == 2)
		{
			const CSS_generic_value* gen_arr = cp->GenArrayValue();

			if (gen_arr[0].GetValueType() == CSS_FUNCTION_URL)
			{
				paint_type = SVGPaint::URI;

				status = p->SetURI(gen_arr[0].GetString(), uni_strlen(gen_arr[0].GetString()));

				if (gen_arr[1].GetValueType() == CSS_IDENT)
				{
					switch (gen_arr[1].GetType())
					{
					case CSS_VALUE_none:
						paint_type = SVGPaint::URI_NONE;
						break;
					case CSS_VALUE_currentColor:
						paint_type = SVGPaint::URI_CURRENT_COLOR;
						break;
					default:
						OP_ASSERT(!"Unexpected value");
						break;
					}
				}
				else if (gen_arr[1].GetValueType() == CSS_DECL_COLOR)
				{
					paint_type = SVGPaint::URI_RGBCOLOR;
					p->m_color = gen_arr[1].value.color;
				}
			}
		}

		p->SetPaintType(paint_type);

		if (paint_type == SVGPaint::RGBCOLOR)
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_LONG ||
					  cp->GetDeclType() == CSS_DECL_COLOR);

			p->SetPaintType(SVGPaint::RGBCOLOR);
			p->m_color = cp->LongValue();
		}
		else if (paint_type == SVGPaint::URI)
		{
			OP_ASSERT(cp->GetDeclType() == CSS_DECL_STRING);

			p->SetPaintType(SVGPaint::URI);
			const uni_char* str_val = cp->StringValue();
			status = p->SetURI(str_val, str_val ? uni_strlen(str_val) : 0);
		}
	}
	return status;
}

OP_STATUS
SVGPaint::Copy(const SVGPaint &other)
{
	SetPaintType(other.GetPaintType());
	m_color = other.m_color;
	SetICCColor(other.GetICCColor());
	const uni_char* uri = other.GetURI();
	return SetURI(uri, uri ? uni_strlen(uri) : 0);
}

#ifdef SVG_DOM
OP_BOOLEAN
SVGPaint::SetPaint(SVGPaint::PaintType type, const uni_char* uri, const uni_char* rgb_color, const uni_char* icc_color)
{
	OP_BOOLEAN status = OpBoolean::IS_TRUE;
	switch(type)
	{
		case RGBCOLOR_ICCCOLOR:
			OP_ASSERT(rgb_color);
			// Ignore icc_color and set rgb_color only, but we keep
			// ICCCOLOR in the type so we can warn about it, if it's
			// used.
			status = SetPaintRGBColor(SVGPaint::RGBCOLOR_ICCCOLOR, rgb_color);
			break;
		case RGBCOLOR:
			status = SetPaintRGBColor(SVGPaint::RGBCOLOR, rgb_color);
			break;
		case NONE:
			OP_ASSERT(rgb_color == NULL);
			OP_ASSERT(uri == NULL);
			OP_ASSERT(icc_color == NULL);
			SetPaintType(SVGPaint::NONE);
			break;
		case CURRENT_COLOR:
			OP_ASSERT(rgb_color == NULL);
			OP_ASSERT(uri == NULL);
			OP_ASSERT(icc_color == NULL);
			SetPaintType(SVGPaint::CURRENT_COLOR);
			break;
		case URI_RGBCOLOR_ICCCOLOR:
			// Ignore icc_color and set rgb_color only, but we keep
			// ICCCOLOR in the type so we can warn about it, if it's
			// used.
			OP_ASSERT(rgb_color != NULL);
			OP_ASSERT(uri != NULL);
			status = SetPaintURIRGBColor(SVGPaint::URI_RGBCOLOR_ICCCOLOR, uri, rgb_color);
			break;
		case URI_RGBCOLOR:
			status = SetPaintURIRGBColor(SVGPaint::URI_RGBCOLOR, uri, rgb_color);
			break;
		case URI_NONE:
			OP_ASSERT(uri);
			status = SetPaintURI(SVGPaint::URI_NONE, uri);
			break;
 		case URI_CURRENT_COLOR:
			OP_ASSERT(uri);
			status = SetPaintURI(SVGPaint::URI_CURRENT_COLOR, uri);
			break;
		case URI:
			OP_ASSERT(uri);
			status = SetPaintURI(SVGPaint::URI, uri);
			break;
	}
	return status;
}

/* NOTE: if OOM occurs at SetURI the rgbcolor may be set without the
 * painttype updated. */
OP_BOOLEAN
SVGPaint::SetPaintURIRGBColor(SVGPaint::PaintType type, const uni_char* uri, const uni_char* rgb_color)
{
	SVGColorObject* col;
	OP_STATUS status = SVGAttributeParser::ParseColorValue(rgb_color, uni_strlen(rgb_color), &col);
	RETURN_IF_MEMORY_ERROR(status);
	if (OpStatus::IsSuccess(status))
	{
		SetColorRef(col->GetColor().GetColorRef());
		OP_DELETE(col);
	}
	else
	{
		return OpBoolean::IS_FALSE;
	}

	return SetPaintURI(type, uri);
}

OP_BOOLEAN
SVGPaint::SetPaintRGBColor(SVGPaint::PaintType type, const uni_char* rgb_color)
{
	return SetPaintURIRGBColor(type, NULL, rgb_color);
}

OP_BOOLEAN
SVGPaint::SetPaintURI(SVGPaint::PaintType type, const uni_char* uri)
{
	RETURN_IF_MEMORY_ERROR(SetURI(uri, uri ? uni_strlen(uri) : 0));
	SetPaintType(type);
	return OpBoolean::IS_TRUE;
}
#endif // SVG_DOM

OP_STATUS
SVGPaint::GetStringRepresentation(TempBuffer* buffer) const
{
	OP_STATUS result = OpStatus::OK;

	BOOL hasURI	=		m_paint_type == SVGPaint::URI_NONE ||
						m_paint_type == SVGPaint::URI_RGBCOLOR ||
						m_paint_type == SVGPaint::URI_RGBCOLOR_ICCCOLOR ||
						m_paint_type == SVGPaint::URI_CURRENT_COLOR ||
						m_paint_type == SVGPaint::URI;

	BOOL hasRGBcolor =	m_paint_type == SVGPaint::URI_RGBCOLOR ||
						m_paint_type == SVGPaint::RGBCOLOR ||
						m_paint_type == SVGPaint::RGBCOLOR_ICCCOLOR ||
						m_paint_type == SVGPaint::URI_RGBCOLOR ||
						m_paint_type == SVGPaint::URI_RGBCOLOR_ICCCOLOR;

	BOOL hasNone =		m_paint_type == SVGPaint::URI_NONE ||
						m_paint_type == SVGPaint::NONE;

	BOOL hasICCColor =	m_paint_type == SVGPaint::URI_RGBCOLOR_ICCCOLOR ||
						m_paint_type == SVGPaint::RGBCOLOR_ICCCOLOR;

	BOOL hasCurrentColor =	m_paint_type == SVGPaint::CURRENT_COLOR ||
							m_paint_type == SVGPaint::URI_CURRENT_COLOR;

	if(hasURI && m_uri)
	{
		const char* url_func_str = "url(";
		result = buffer->Append(url_func_str, op_strlen(url_func_str));
		if (OpStatus::IsSuccess(result))
		{
			result = buffer->Append(m_uri, uni_strlen(m_uri));
			if (OpStatus::IsSuccess(result))
				result = buffer->Append(')');
		}
	}

	if(hasRGBcolor && OpStatus::IsSuccess(result))
	{
		// Check if m_color has the CSS_COLOR_KEYWORD_TYPE_named flag
		// set.  For extra caution, check that it hasn't got ANY bits
		// set outside the named flag and the index bitmask.
		BOOL has_named_color = ((m_color & CSS_COLOR_KEYWORD_TYPE_named) &&
								!(m_color &
								  ~(CSS_COLOR_KEYWORD_TYPE_index |
									CSS_COLOR_KEYWORD_TYPE_x_color |
									CSS_COLOR_KEYWORD_TYPE_ui_color)));

 		if(has_named_color)
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
				result = buffer->Append(colorstr);
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
			result = buffer->Append(a.CStr(), a.Length());
		}
	}

	if(hasICCColor && OpStatus::IsSuccess(result))
	{
		REPORT_NOTICE(NULL, UNI_L("Truncation in string conversion because lack of ICCColor-support"));
	}

	if(hasNone && OpStatus::IsSuccess(result))
	{
		const char* none_str = "none";
		result = buffer->Append(none_str, op_strlen(none_str));
	}

	if(hasCurrentColor && OpStatus::IsSuccess(result))
	{
		const char* curcol_str = "currentColor";
		result = buffer->Append(curcol_str, op_strlen(curcol_str));
	}

	if((m_paint_type == SVGPaint::INHERIT //|| Flag(SVGOBJECTFLAG_INHERIT)
		) && OpStatus::IsSuccess(result))
	{
		const char* inherit_str = "inherit";
		result = buffer->Append(inherit_str, op_strlen(inherit_str));
	}

	return result;
}

OP_STATUS
SVGPaint::SetURI(const uni_char* uri, int strlen, BOOL add_hashsep /* = FALSE */)
{
	OP_DELETEA(m_uri);
	m_uri = NULL;

	if (uri != NULL && strlen > 0)
	{
		if (add_hashsep && uri[0] != '#')
		{
			m_uri = OP_NEWA(uni_char, strlen + 2);
			if (!m_uri)
				return OpStatus::ERR_NO_MEMORY;
			m_uri[0] = '#';
			uni_strncpy(m_uri+1, uri, strlen);
			m_uri[strlen+1] = '\0';
			return OpStatus::OK;
		}
		return UniSetStrN(m_uri, uri, strlen);
	}
	return OpStatus::OK;
}

BOOL
SVGPaint::IsEqual(const SVGPaint& other) const
{
	if (m_paint_type == other.m_paint_type)
	{
		switch (m_paint_type)
		{
		case SVGPaint::INHERIT:
		case SVGPaint::NONE:
			return TRUE;
		case SVGPaint::RGBCOLOR:
			return m_color == other.m_color;
		case SVGPaint::RGBCOLOR_ICCCOLOR:
			return m_icccolor == other.m_icccolor;
		default:
			if (GetURI() && other.GetURI())
			{
				return uni_str_eq(GetURI(), other.GetURI());
			}
			// FIXME: Support the other types as well
		}
	}
	return FALSE;
}

///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////

/* virtual */ SVGObject *
SVGPaintObject::Clone() const
{
	SVGPaintObject *paint_object = OP_NEW(SVGPaintObject, ());
	if (paint_object)
	{
		paint_object->CopyFlags(*this);
		if (OpStatus::IsMemoryError(paint_object->m_paint.Copy(m_paint)))
		{
			OP_DELETE(paint_object);
			return NULL;
		}
	}
	return paint_object;
}

/* virtual */
OP_STATUS
SVGPaintObject::LowLevelGetStringRepresentation(TempBuffer* buffer) const
{
	return m_paint.GetStringRepresentation(buffer);
}

/* virtual */
BOOL
SVGPaintObject::IsEqual(const SVGObject& other) const
{
	if (other.Type() == SVGOBJECT_PAINT)
	{
		const SVGPaintObject& other_paint =
			static_cast<const SVGPaintObject&>(other);
		return m_paint.IsEqual(other_paint.m_paint);
	}

	return FALSE;
}

/* virtual */ BOOL
SVGPaintObject::InitializeAnimationValue(SVGAnimationValue &animation_value)
{
	animation_value.reference_type = SVGAnimationValue::REFERENCE_SVG_PAINT;
	animation_value.reference.svg_paint = &m_paint;
	return TRUE;
}

#endif // SVG_SUPPORT
