/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
*/

#include "core/pch.h"
#include "modules/style/css_dom.h"
#include "modules/style/css_media.h"
#include "modules/style/css_decl.h"
#include "modules/style/css_property_list.h"
#include "modules/style/css_style_attribute.h"
#include "modules/style/src/css_rule.h"
#include "modules/style/src/css_ruleset.h"
#include "modules/style/src/css_media_rule.h"
#include "modules/style/src/css_import_rule.h"
#include "modules/style/src/css_charset_rule.h"
#include "modules/style/src/css_namespace_rule.h"
#include "modules/style/src/css_parser.h"
#include "modules/style/src/css_selector.h"
#include "modules/style/src/css_keyframes.h"
#include "modules/style/src/css_aliases.h"
#include "modules/dom/domenvironment.h"
#include "modules/layout/box/tables.h"
#include "modules/layout/cascade.h"
#include "modules/layout/layout_workplace.h"
#include "modules/layout/layoutprops.h"
#include "modules/spatial_navigation/sn_handler.h"
#include "modules/dochand/win.h"

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
#endif // SVG_SUPPORT

static BOOL
CSS_IsShorthandProperty(short property)
{
	switch (property)
	{
	case CSS_PROPERTY_background:
	case CSS_PROPERTY_border:
	case CSS_PROPERTY_border_bottom:
	case CSS_PROPERTY_border_color:
	case CSS_PROPERTY_border_left:
	case CSS_PROPERTY_border_right:
	case CSS_PROPERTY_border_style:
	case CSS_PROPERTY_border_top:
	case CSS_PROPERTY_border_width:
	case CSS_PROPERTY_border_radius:
	case CSS_PROPERTY_font:
	case CSS_PROPERTY_list_style:
	case CSS_PROPERTY_margin:
	case CSS_PROPERTY_outline:
	case CSS_PROPERTY_overflow:
	case CSS_PROPERTY_padding:
	case CSS_PROPERTY_columns:
	case CSS_PROPERTY_column_rule:
	case CSS_PROPERTY_flex:
	case CSS_PROPERTY_flex_flow:
#ifdef CSS_TRANSITIONS
	case CSS_PROPERTY_transition:
	case CSS_PROPERTY_animation:
#endif
		return TRUE;
	}
	return FALSE;
}

static BOOL
CSS_IsDefaultValue(CSS_decl* decl)
{
	short prop = decl->GetProperty();

	switch (prop)
	{
	case CSS_PROPERTY_border_top_color:
	case CSS_PROPERTY_border_right_color:
	case CSS_PROPERTY_border_bottom_color:
	case CSS_PROPERTY_border_left_color:
	case CSS_PROPERTY_background_color:
		if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_transparent)
			return TRUE;
		break;
	case CSS_PROPERTY_outline_color:
		if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_invert)
			return TRUE;
		break;
	case CSS_PROPERTY_border_top_style:
	case CSS_PROPERTY_border_right_style:
	case CSS_PROPERTY_border_bottom_style:
	case CSS_PROPERTY_border_left_style:
	case CSS_PROPERTY_outline_style:
	case CSS_PROPERTY_list_style_image:
		if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_none)
			return TRUE;
		break;
	case CSS_PROPERTY_background_image:
		if (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && decl->ArrayValueLen() == 1 && decl->GenArrayValue()[0].GetType() == CSS_VALUE_none)
			return TRUE;
		break;
	case CSS_PROPERTY_background_repeat:
		if (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && decl->ArrayValueLen() == 1 && decl->GenArrayValue()[0].GetType() == CSS_VALUE_repeat)
			return TRUE;
		break;
	case CSS_PROPERTY_background_attachment:
		if (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && decl->ArrayValueLen() == 1 && decl->GenArrayValue()[0].GetType() == CSS_VALUE_scroll)
			return TRUE;
		break;
	case CSS_PROPERTY_background_position:
		if (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && decl->ArrayValueLen() == 2
			&& decl->GenArrayValue()[0].GetNumber() == 0
			&& decl->GenArrayValue()[1].GetNumber() == 0)
			return TRUE;
		break;
	case CSS_PROPERTY_background_size:
		if (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && decl->ArrayValueLen() == 1
			&& decl->GenArrayValue()[0].GetType() == CSS_VALUE_auto)
			return TRUE;
		break;
	case CSS_PROPERTY_border_top_width:
	case CSS_PROPERTY_border_right_width:
	case CSS_PROPERTY_border_bottom_width:
	case CSS_PROPERTY_border_left_width:
	case CSS_PROPERTY_outline_width:
		if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_medium)
			return TRUE;
		break;
	case CSS_PROPERTY_list_style_type:
		if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_disc)
			return TRUE;
		break;
	case CSS_PROPERTY_list_style_position:
		if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_outside)
			return TRUE;
		break;
	case CSS_PROPERTY_overflow_x:
	case CSS_PROPERTY_overflow_y:
		if (decl->TypeValue() == CSS_VALUE_visible)
			return TRUE;
		break;

	case CSS_PROPERTY_column_width:
	case CSS_PROPERTY_column_count:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_auto;

	case CSS_PROPERTY_column_rule_width:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_medium;

	case CSS_PROPERTY_column_rule_color:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_transparent;

	case CSS_PROPERTY_column_rule_style:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_none;

	case CSS_PROPERTY_flex_grow:
	case CSS_PROPERTY_flex_shrink:
	case CSS_PROPERTY_flex_basis:
		/* We may want to improve this in the future, to get a more compact
		   'flex' shorthand representation. But since the initial values of the
		   longhand properties and their default values when omitted when used
		   with the 'flex' shorthand differ, that's not very trivial. */

		return FALSE;

	case CSS_PROPERTY_flex_direction:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_row;

	case CSS_PROPERTY_flex_wrap:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_nowrap;
	}

	return FALSE;
}

/** Set an array with individual properties that make up a given shorthand.

	Individual properties that make up a given shorthand need to be known when
	constructing computed values or removing shorthand properties from the
	style declaration.

	@param[in] property The shorthand property for which individual properties
		want to be known.
	@param[out] prop_array The array to be set with properties.
	@param[out] prop_array_len The length of the resulting array.
	@param[out] prop_type The type indicating which method should be used for
		serializing computed value. Used by @see GetShorthandPropertyL. */
static void
CSS_GetShorthandProperties(short property, short* prop_array, int& prop_array_len, short& prop_type)
{
	prop_array_len = 0;
	prop_type = -1;

	switch (property)
	{
	case CSS_PROPERTY_border:
		{
			prop_array[0] = CSS_PROPERTY_border_top_color;
			prop_array[1] = CSS_PROPERTY_border_top_style;
			prop_array[2] = CSS_PROPERTY_border_top_width;
			prop_array[3] = CSS_PROPERTY_border_right_color;
			prop_array[4] = CSS_PROPERTY_border_right_style;
			prop_array[5] = CSS_PROPERTY_border_right_width;
			prop_array[6] = CSS_PROPERTY_border_bottom_color;
			prop_array[7] = CSS_PROPERTY_border_bottom_style;
			prop_array[8] = CSS_PROPERTY_border_bottom_width;
			prop_array[9] = CSS_PROPERTY_border_left_color;
			prop_array[10] = CSS_PROPERTY_border_left_style;
			prop_array[11] = CSS_PROPERTY_border_left_width;
			prop_array_len = 12;
			prop_type = 2;
		}
		break;
	case CSS_PROPERTY_border_bottom:
		{
			prop_array[0] = CSS_PROPERTY_border_bottom_width;
			prop_array[1] = CSS_PROPERTY_border_bottom_style;
			prop_array[2] = CSS_PROPERTY_border_bottom_color;
			prop_array_len = 3;
			prop_type = 1;
		}
		break;
	case CSS_PROPERTY_border_color:
		{
			prop_array[0] = CSS_PROPERTY_border_top_color;
			prop_array[1] = CSS_PROPERTY_border_right_color;
			prop_array[2] = CSS_PROPERTY_border_bottom_color;
			prop_array[3] = CSS_PROPERTY_border_left_color;
			prop_array_len = 4;
			prop_type = 4;
		}
		break;
	case CSS_PROPERTY_border_left:
		{
			prop_array[0] = CSS_PROPERTY_border_left_width;
			prop_array[1] = CSS_PROPERTY_border_left_style;
			prop_array[2] = CSS_PROPERTY_border_left_color;
			prop_array_len = 3;
			prop_type = 1;
		}
		break;
	case CSS_PROPERTY_border_right:
		{
			prop_array[0] = CSS_PROPERTY_border_right_width;
			prop_array[1] = CSS_PROPERTY_border_right_style;
			prop_array[2] = CSS_PROPERTY_border_right_color;
			prop_array_len = 3;
			prop_type = 1;
		}
		break;
	case CSS_PROPERTY_column_rule:
		{
			prop_array[0] = CSS_PROPERTY_column_rule_width;
			prop_array[1] = CSS_PROPERTY_column_rule_style;
			prop_array[2] = CSS_PROPERTY_column_rule_color;
			prop_array_len = 3;
			prop_type = 1;
		}
		break;
	case CSS_PROPERTY_border_style:
		{
			prop_array[0] = CSS_PROPERTY_border_top_style;
			prop_array[1] = CSS_PROPERTY_border_right_style;
			prop_array[2] = CSS_PROPERTY_border_bottom_style;
			prop_array[3] = CSS_PROPERTY_border_left_style;
			prop_array_len = 4;
			prop_type = 4;
		}
		break;
	case CSS_PROPERTY_border_top:
		{
			prop_array[0] = CSS_PROPERTY_border_top_width;
			prop_array[1] = CSS_PROPERTY_border_top_style;
			prop_array[2] = CSS_PROPERTY_border_top_color;
			prop_array_len = 3;
			prop_type = 1;
		}
		break;
	case CSS_PROPERTY_border_width:
		{
			prop_array[0] = CSS_PROPERTY_border_top_width;
			prop_array[1] = CSS_PROPERTY_border_right_width;
			prop_array[2] = CSS_PROPERTY_border_bottom_width;
			prop_array[3] = CSS_PROPERTY_border_left_width;
			prop_array_len = 4;
			prop_type = 4;
		}
		break;
	case CSS_PROPERTY_border_radius:
		{
			prop_array[0] = CSS_PROPERTY_border_top_left_radius;
			prop_array[1] = CSS_PROPERTY_border_top_right_radius;
			prop_array[2] = CSS_PROPERTY_border_bottom_right_radius;
			prop_array[3] = CSS_PROPERTY_border_bottom_left_radius;
			prop_array_len = 4;
			prop_type = 8;
		}
		break;
	case CSS_PROPERTY_font:
		{
			prop_array[0] = CSS_PROPERTY_font_style;
			prop_array[1] = CSS_PROPERTY_font_variant;
			prop_array[2] = CSS_PROPERTY_font_weight;
			prop_array[3] = CSS_PROPERTY_font_size;
			prop_array[4] = CSS_PROPERTY_line_height;
			prop_array[5] = CSS_PROPERTY_font_family;
			prop_array_len = 6;
			prop_type = 3;
		}
		break;
	case CSS_PROPERTY_list_style:
		{
			prop_array[0] = CSS_PROPERTY_list_style_type;
			prop_array[1] = CSS_PROPERTY_list_style_position;
			prop_array[2] = CSS_PROPERTY_list_style_image;
			prop_array_len = 3;
			prop_type = 1;
		}
		break;
	case CSS_PROPERTY_margin:
		{
			prop_array[0] = CSS_PROPERTY_margin_top;
			prop_array[1] = CSS_PROPERTY_margin_right;
			prop_array[2] = CSS_PROPERTY_margin_bottom;
			prop_array[3] = CSS_PROPERTY_margin_left;
			prop_array_len = 4;
			prop_type = 4;
		}
		break;
	case CSS_PROPERTY_outline:
		{
			prop_array[0] = CSS_PROPERTY_outline_color;
			prop_array[1] = CSS_PROPERTY_outline_style;
			prop_array[2] = CSS_PROPERTY_outline_width;
			prop_array_len = 3;
			prop_type = 1;
		}
		break;
	case CSS_PROPERTY_padding:
		{
			prop_array[0] = CSS_PROPERTY_padding_top;
			prop_array[1] = CSS_PROPERTY_padding_right;
			prop_array[2] = CSS_PROPERTY_padding_bottom;
			prop_array[3] = CSS_PROPERTY_padding_left;
			prop_array_len = 4;
			prop_type = 4;
		}
		break;
	case CSS_PROPERTY_overflow:
		{
			prop_array[0] = CSS_PROPERTY_overflow_x;
			prop_array[1] = CSS_PROPERTY_overflow_y;
			prop_array_len = 2;
			prop_type = 5;
		}
		break;
    case CSS_PROPERTY_background:
        {
            prop_array[0] = CSS_PROPERTY_background_color;
            prop_array[1] = CSS_PROPERTY_background_image;
            prop_array[2] = CSS_PROPERTY_background_repeat;
            prop_array[3] = CSS_PROPERTY_background_attachment;
            prop_array[4] = CSS_PROPERTY_background_position;
            prop_array[5] = CSS_PROPERTY_background_size;
            prop_array[6] = CSS_PROPERTY_background_origin;
            prop_array[7] = CSS_PROPERTY_background_clip;
            prop_array_len = 8;
            prop_type = 6;
        }
        break;
    case CSS_PROPERTY_columns:
        {
            prop_array[0] = CSS_PROPERTY_column_width;
            prop_array[1] = CSS_PROPERTY_column_count;
            prop_array_len = 2;
            prop_type = 7;
        }
        break;
#ifdef CSS_TRANSITIONS
	case CSS_PROPERTY_transition:
		{
			prop_array[0] = CSS_PROPERTY_transition_property;
			prop_array[1] = CSS_PROPERTY_transition_duration;
			prop_array[2] = CSS_PROPERTY_transition_delay;
			prop_array[3] = CSS_PROPERTY_transition_timing_function;
			prop_array_len = 4;
			prop_type = 9;
		}
		break;
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation:
		{
			prop_array[0] = CSS_PROPERTY_animation_name;
			prop_array[1] = CSS_PROPERTY_animation_duration;
			prop_array[2] = CSS_PROPERTY_animation_delay;
			prop_array[3] = CSS_PROPERTY_animation_timing_function;
			prop_array[4] = CSS_PROPERTY_animation_direction;
			prop_array[5] = CSS_PROPERTY_animation_fill_mode;
			prop_array[6] = CSS_PROPERTY_animation_iteration_count;
			prop_array_len = 7;
			prop_type = 9;
		}
		break;
#endif // CSS_ANIMATIONS
	case CSS_PROPERTY_flex:
		{
			prop_array[0] = CSS_PROPERTY_flex_grow;
			prop_array[1] = CSS_PROPERTY_flex_shrink;
			prop_array[2] = CSS_PROPERTY_flex_basis;
			prop_array_len = 3;
			prop_type = 7;
		}
		break;
	case CSS_PROPERTY_flex_flow:
		{
			prop_array[0] = CSS_PROPERTY_flex_direction;
			prop_array[1] = CSS_PROPERTY_flex_wrap;
			prop_array_len = 2;
			prop_type = 7;
		}
		break;
    }
}

CSS_DOMStyleDeclaration::CSS_DOMStyleDeclaration(DOM_Environment* environment, HTML_Element* element, CSS_DOMRule* rule, Type type, const uni_char* pseudo_class)
	: m_environment(environment),
	  m_rule(rule),
	  m_element(element),
	  m_type(type),
	  m_pseudo_elm(0)
{
	if (pseudo_class)
	{
		const uni_char* pseudo_start = pseudo_class;
		if (pseudo_start[0] == ':')
		{
			if (pseudo_start[1] == ':')
				pseudo_start += 2;
			else
				pseudo_start++;

			short pseudo = CSS_GetPseudoClass(pseudo_start, uni_strlen(pseudo_start));
			if (pseudo == PSEUDO_CLASS_BEFORE)
				m_pseudo_elm = CSS_PSEUDO_CLASS_BEFORE;
			else if (pseudo == PSEUDO_CLASS_AFTER)
				m_pseudo_elm = CSS_PSEUDO_CLASS_AFTER;
			else if (pseudo == PSEUDO_CLASS_FIRST_LINE)
				m_pseudo_elm = CSS_PSEUDO_CLASS_FIRST_LINE;
			else if (pseudo == PSEUDO_CLASS_FIRST_LETTER)
				m_pseudo_elm = CSS_PSEUDO_CLASS_FIRST_LETTER;
			else if (pseudo == PSEUDO_CLASS_SELECTION && pseudo_start - pseudo_class == 2)
				m_pseudo_elm = CSS_PSEUDO_CLASS_SELECTION;
		}
	}

	if (m_rule)
		m_rule->Ref();
}

CSS_DOMStyleDeclaration::~CSS_DOMStyleDeclaration()
{
	if (m_rule)
		m_rule->Unref();
}

OP_STATUS
CSS_DOMStyleDeclaration::GetPropertyValue(TempBuffer* buffer, const uni_char* property_name)
{
	return GetPropertyValue(buffer, GetAliasedProperty(GetCSS_Property(property_name, uni_strlen(property_name))));
}

OP_STATUS
CSS_DOMStyleDeclaration::GetPropertyValue(TempBuffer* buffer, int property)
{
	if (property != -1 && m_environment->IsEnabled())
	{
		if (CSS_IsShorthandProperty(property))
		{
			TRAPD(status, GetShorthandPropertyL(buffer, property, GetCSS_SerializationFormat()));
			return status;
		}
		else if (CSS_decl* decl = GetDecl(property))
		{
			TRAPD(status, CSS::FormatDeclarationL(buffer, decl, FALSE, GetCSS_SerializationFormat()));
			ReleaseDecl(decl);
			return status;
		}
	}

	return OpStatus::OK;
}

OP_STATUS
CSS_DOMStyleDeclaration::GetPixelValue(double& value, int property)
{
	value = 0.;

	if (m_environment->IsEnabled())
	{
		FramesDocument* doc = m_environment->GetFramesDocument();

		if (doc && doc->GetDocRoot() && doc->GetDocRoot()->IsAncestorOf(m_element))
			if (CSS_decl* decl = GetDecl(property))
			{
				BOOL is_horizontal = property == CSS_PROPERTY_left || property == CSS_PROPERTY_width || property == CSS_PROPERTY_right;
				value = LayoutProperties::CalculatePixelValue(m_element, decl, doc, is_horizontal);
				ReleaseDecl(decl);
			}
	}

	return OpStatus::OK;
}

OP_STATUS
CSS_DOMStyleDeclaration::GetPosValue(double& value, int property)
{
	value = 0.;

	if (m_environment->IsEnabled())
		if (CSS_decl* decl = GetDecl(property))
		{
			value = decl->GetNumberValue(1);
			ReleaseDecl(decl);
		}

	return OpStatus::OK;
}

#ifdef CSS_TRANSFORMS

OP_STATUS
CSS_DOMStyleDeclaration::GetAffineTransform(AffineTransform& transform, const uni_char* str)
{
	if (m_environment->IsEnabled())
	{
		FramesDocument* frames_doc = m_environment->GetFramesDocument();
		HLDocProfile* hld_profile = frames_doc->GetHLDocProfile();

		if (!hld_profile)
			return OpStatus::OK;

		OP_STATUS status;
		CSS_property_list* new_css = CSS::LoadDOMStyleValue(hld_profile, NULL, GetBaseURL(), CSS_PROPERTY_transform, str, uni_strlen(str), status);

		if (!new_css || OpStatus::IsMemoryError(status))
			return OpStatus::ERR_NO_MEMORY;

		if (new_css->GetLength() != 1)
		{
			new_css->Unref();
			return OpStatus::ERR;
		}

		CSS_decl* decl = new_css->GetFirstDecl();

		if (decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_none)
		{
			transform.LoadIdentity();
			new_css->Unref();
			return OpStatus::OK;
		}
		else
			if (decl->GetDeclType() == CSS_DECL_TRANSFORM_LIST)
			{
				CSS_transform_list* transform_list = static_cast<CSS_transform_list*>(decl);
				if (!transform_list->GetAffineTransform(transform))
				{
					new_css->Unref();
					return OpStatus::ERR_NOT_SUPPORTED;
				}
			}
			else
			{
				OP_ASSERT(!"Not reached");
				new_css->Unref();
				return OpStatus::ERR;
			}

		new_css->Unref();
	}

	return OpStatus::OK;
}

#endif // CSS_TRANSFORMS

OP_STATUS
CSS_DOMStyleDeclaration::GetPropertyPriority(TempBuffer* buffer, const uni_char* property_name)
{
	int property = GetAliasedProperty(GetCSS_Property(property_name, uni_strlen(property_name)));
	BOOL important = FALSE;

	if (CSS_IsShorthandProperty(property))
	{
		short prop_array[12]; /* ARRAY OK 2011-04-27 rune */
		int prop_array_len(0);
		short prop_type;

		CSS_GetShorthandProperties(property, prop_array, prop_array_len, prop_type);

		OP_ASSERT(prop_array_len > 0);

		important = TRUE;
		for (int idx = 0; idx < prop_array_len; ++idx)
		{
			CSS_decl* decl = GetDecl(prop_array[idx]);
			if (important && (!decl || !decl->GetImportant()))
				important = FALSE;
			if (decl)
				ReleaseDecl(decl);
		}
	}
	else
	{
		CSS_decl* decl = GetDecl(property);
		if (decl)
		{
			if (decl->GetImportant())
				important = TRUE;
			ReleaseDecl(decl);
		}
	}

	if (important)
		return buffer->Append("important");

	return OpStatus::OK;
}

OP_STATUS
CSS_DOMStyleDeclaration::SetProperty(const uni_char* property_name, const uni_char* value, const uni_char* priority, CSS_DOMException& exception)
{
	return SetProperty(GetCSS_Property(property_name, uni_strlen(property_name)), value, priority, exception);
}

OP_STATUS
CSS_DOMStyleDeclaration::SetProperty(int property, const uni_char* value, CSS_DOMException& exception)
{
	if (*value)
		return SetProperty(property, value, NULL, exception);
	else
		return RemoveProperty(NULL, property, exception);
}

OP_STATUS
CSS_DOMStyleDeclaration::SetPixelValue(int property, const uni_char* value, CSS_DOMException& exception)
{
	TempBuffer buffer;

	RETURN_IF_ERROR(buffer.Append(value));
	RETURN_IF_ERROR(buffer.Append("px"));

	return SetProperty(property, buffer.GetStorage(), NULL, exception);
}

OP_STATUS
CSS_DOMStyleDeclaration::SetPosValue(int property, const uni_char* value, CSS_DOMException& exception)
{
	TempBuffer buffer;
	const char* unit = "px";

	if (CSS_decl* decl = GetDecl(property))
	{
		switch (((CSS_number_decl*)decl)->GetValueType(0))
		{
		case CSS_PERCENTAGE: unit = "%"; break;
		case CSS_CM: unit = "cm"; break;
		case CSS_MM: unit = "mm"; break;
		case CSS_IN: unit = "in"; break;
		case CSS_PC: unit = "pc"; break;
		case CSS_PT: unit = "pt"; break;
		case CSS_EM: unit = "em"; break;
		case CSS_REM: unit = "rem"; break;
		case CSS_EX: unit = "pt"; break;
		case CSS_PX: unit = "px"; break;
		}
		ReleaseDecl(decl);
	}

	RETURN_IF_ERROR(buffer.Append(value));
	RETURN_IF_ERROR(buffer.Append(unit));

	return SetProperty(property, buffer.GetStorage(), NULL, exception);
}

OP_STATUS
CSS_DOMStyleDeclaration::RemoveProperty(TempBuffer* buffer, const uni_char* property_name, CSS_DOMException& exception)
{
	return RemoveProperty(buffer, GetAliasedProperty(GetCSS_Property(property_name, uni_strlen(property_name))), exception);
}

unsigned int
CSS_DOMStyleDeclaration::GetLength()
{
	if (m_type == NORMAL)
		if (CSS_property_list* css = GetPropertyList())
			return css->GetLength();

	/* FIXME: what about computed style? */
	return 0;
}

OP_STATUS
CSS_DOMStyleDeclaration::GetItem(TempBuffer* buffer, unsigned int index)
{
	if (CSS_property_list* css = GetPropertyList())
		if (index < css->GetLength())
		{
			CSS_decl* decl = css->GetFirstDecl();
			while (index-- > 0)
				decl = decl->Suc();

			RETURN_IF_ERROR(buffer->Append(GetCSS_PropertyName(decl->GetProperty())));
			uni_strlwr(buffer->GetStorage());
		}

	return OpStatus::OK;
}

OP_STATUS
CSS_DOMStyleDeclaration::SetText(const uni_char* text, CSS_DOMException& exception)
{
	if (m_rule && m_environment->IsEnabled())
	{
		CSS_PARSE_STATUS stat;
		FramesDocument* frames_doc = m_environment->GetFramesDocument();
		HLDocProfile* hld_profile = frames_doc ? frames_doc->GetHLDocProfile() : NULL;
		if (!hld_profile)
			return OpStatus::OK;

		CSS_property_list* prop_list = CSS::LoadHtmlStyleAttr(text, uni_strlen(text), GetBaseURL(), hld_profile, stat);
		if (stat == CSSParseStatus::SYNTAX_ERROR)
		{
			exception = CSS_DOMEXCEPTION_SYNTAX_ERR;
			return OpStatus::ERR;
		}
		else if (stat == CSSParseStatus::HIERARCHY_ERROR)
		{
			exception = CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR;
			return OpStatus::ERR;
		}
		else if (stat == CSSParseStatus::NAMESPACE_ERROR)
		{
			exception = CSS_DOMEXCEPTION_NAMESPACE_ERR;
			return OpStatus::ERR;
		}
		else
		{
			if (OpStatus::IsError(stat))
			{
				OP_ASSERT(stat == OpStatus::ERR_NO_MEMORY);
				return stat;
			}
			if (prop_list)
			{
				m_rule->SetPropertyList(prop_list);
				prop_list->Unref();
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS
CSS_DOMStyleDeclaration::GetText(TempBuffer* buffer)
{
	OP_STATUS status = OpStatus::OK;
	if (CSS_property_list* css = GetPropertyList())
	{
		TRAP(status, css->AppendPropertiesAsStringL(buffer));
	}
	return status;
}

CSS_property_list*
CSS_DOMStyleDeclaration::GetPropertyList()
{
	return m_rule ? m_rule->GetPropertyList() : m_element->GetCSS_Style();
}

OP_STATUS
CSS_DOMStyleDeclaration::SetProperty(short property, const uni_char* value, const uni_char* priority, CSS_DOMException& exception)
{
	if (m_type != NORMAL)
	{
		exception = CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR;
		return OpStatus::ERR;
	}
	else if (property == -1)
	{
		exception = CSS_DOMEXCEPTION_NONE;
		return OpStatus::ERR;
	}
	else if (m_environment->IsEnabled())
	{
		FramesDocument* frames_doc = m_environment->GetFramesDocument();
		HLDocProfile* hld_profile = frames_doc ? frames_doc->GetHLDocProfile() : NULL;

		if (!hld_profile)
			return OpStatus::OK;

		HTML_Element* root = hld_profile->GetRoot();
		BOOL in_document = root && root->IsAncestorOf(m_element);
		BOOL need_update = FALSE;

		CSS_property_list* css = GetPropertyList();

		if (value && *value)
		{
			OP_STATUS status = OpStatus::OK;
			if (!css)
			{
				OP_ASSERT(!m_rule);

				css = OP_NEW(CSS_property_list, ());
				if (!css)
					return OpStatus::ERR_NO_MEMORY;

				css->Ref();
				status = m_element->SetCSS_Style(css);
				if (OpStatus::IsMemoryError(status))
				{
					css->Unref();
					return status;
				}
			}

			CSS_property_list* new_css = CSS::LoadDOMStyleValue(hld_profile, m_rule, GetBaseURL(), property, value, uni_strlen(value), status);
			RETURN_IF_ERROR(status);

			if (new_css)
			{
				BOOL important = priority && uni_stri_eq(priority, "IMPORTANT");
				need_update = css->AddList(new_css, important);
				new_css->Unref();
			}
		}
		else if (css)
		{
			if (CSS_decl* decl = css->RemoveDecl(property))
			{
				decl->Unref();
				need_update = TRUE;
			}
		}

		if (need_update)
		{
			if (!m_rule && m_element)
			{
				StyleAttribute* attr = m_element->GetStyleAttribute();
				if (attr)
					attr->SetIsModified();
			}
			if (in_document)
				return Update(property);
		}
	}

	return OpStatus::OK;
}

OP_STATUS
CSS_DOMStyleDeclaration::RemoveProperty(TempBuffer* buffer, short property, CSS_DOMException& exception)
{
	if (m_type != NORMAL)
	{
		exception = CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR;
		return OpStatus::ERR;
	}
	else if (property == -1)
	{
		exception = CSS_DOMEXCEPTION_NONE;
		return OpStatus::ERR;
	}
	else if (CSS_IsShorthandProperty(property))
	{
		OP_STATUS status = OpStatus::OK;
		if (buffer)
			TRAP(status, GetShorthandPropertyL(buffer, property, CSS_FORMAT_CSSOM_STYLE));

		short prop_array[12]; /* ARRAY OK 2009-02-12 rune */
		int prop_array_len(0);
		short prop_type;
		CSS_GetShorthandProperties(property, prop_array, prop_array_len, prop_type);

		for (int idx = 0; idx < prop_array_len; ++idx)
			RETURN_IF_ERROR(RemoveProperty(NULL, prop_array[idx], exception));
		return status;
	}
	else if (CSS_property_list* css = GetPropertyList())
	{
		if (CSS_decl* decl = css->RemoveDecl(property))
		{
			// Use anchored auto_ptr to avoid 'clobbered by vfork' problem with TRAP.
			CSS_DeclStackAutoPtr decl_ptr(decl);
			if (buffer)
			{
				OP_ASSERT(m_type != COMPUTED || !"prefer_original_string = TRUE in the call below is based on m_type == COMPUTED being handled elsewhere.");
				TRAPD(status, CSS::FormatDeclarationL(buffer, decl, FALSE, CSS_FORMAT_CSSOM_STYLE));
				RETURN_IF_ERROR(status);
			}

			if (!m_rule && m_element)
			{
				StyleAttribute* attr = m_element->GetStyleAttribute();
				if (attr)
					attr->SetIsModified();
			}

			FramesDocument* doc = m_environment->GetFramesDocument();
			if (doc && doc->GetHLDocProfile())
			{
				HTML_Element* root = doc->GetHLDocProfile()->GetRoot();
				if (m_environment->IsEnabled() && root && root->IsAncestorOf(m_element))
					RETURN_IF_ERROR(Update(property));
			}
		}
	}

	return OpStatus::OK;
}

void
CSS_DOMStyleDeclaration::GetShorthandPropertyL(TempBuffer* buffer, short property, CSS_SerializationFormat format_mode)
{
	CSS_property_list* css = NULL;

	if (m_type == NORMAL)
	{
		css = GetPropertyList();
		if (!css)
			return;
	}

	buffer->ExpandL(32);

	CSS_decl* prop_values[] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };

	short prop_array[12];
	int prop_array_len;
	short prop_type;

	CSS_GetShorthandProperties(property, prop_array, prop_array_len, prop_type);

	OP_ASSERT(prop_array_len > 0);

	for (int index = 0; index < prop_array_len; ++index)
		prop_values[index] = GetDecl(prop_array[index]);

	BOOL is_set = FALSE;
	BOOL default_dropped = FALSE;

	do
	{
		// The DOM specification states that default values should be removed from
		// the shorthand string returned for CSS properties. If all values are default
		// values all the values should be added to the return string
		// When all values are default nothing will be added to the buffer on the first
		// loop. The second time around all_default will be set since default_dropped
		// can only be TRUE after the first loop and we will only take a second loop
		// if we have nothing set because of dropped default values. (stighal, 2002-05-15)

		BOOL all_default = default_dropped;

		switch (prop_type)
		{
		case 1: // border-top, border-right, border-bottom, border-left, outline
			{
				BOOL first = TRUE;
				for (int i = 0; i < prop_array_len; i++)
				{
					if (prop_values[i])
					{
						if (all_default || !CSS_IsDefaultValue(prop_values[i]))
						{
							CSS::FormatDeclarationL(buffer, prop_values[i], !first, format_mode);
							first = FALSE;
						}
						else
							default_dropped = TRUE;
					}
				}
				is_set = !first;
			}
			break;

		case 2: // border
			{
				BOOL first = TRUE;

				// width
				if (prop_values[2] && prop_values[5] && prop_values[8] && prop_values[11] &&
					prop_values[2]->IsEqual(prop_values[5]) && prop_values[2]->IsEqual(prop_values[8]) &&
					prop_values[2]->IsEqual(prop_values[11]))
				{
					if (all_default || !CSS_IsDefaultValue(prop_values[2]))
					{
						CSS::FormatDeclarationL(buffer, prop_values[2], FALSE, format_mode);
						first = FALSE;
					}
					else
						default_dropped = TRUE;
				}

				// style
				if (prop_values[1] && prop_values[4] && prop_values[7] && prop_values[10] &&
					prop_values[1]->IsEqual(prop_values[4]) && prop_values[1]->IsEqual(prop_values[7]) &&
					prop_values[1]->IsEqual(prop_values[10]))
				{
					if (all_default || !CSS_IsDefaultValue(prop_values[1]))
					{
						CSS::FormatDeclarationL(buffer, prop_values[1], !first, format_mode);
						first = FALSE;
					}
					else
						default_dropped = TRUE;
				}

				// color
				if (prop_values[0] && prop_values[3] && prop_values[6] && prop_values[9] &&
					prop_values[0]->IsEqual(prop_values[3]) && prop_values[0]->IsEqual(prop_values[6]) &&
					prop_values[0]->IsEqual(prop_values[9]))
				{
					if (all_default || !CSS_IsDefaultValue(prop_values[0]))
					{
						CSS::FormatDeclarationL(buffer, prop_values[0], !first, format_mode);
						first = FALSE;
					}
					else
						default_dropped = TRUE;
				}

				is_set = !first;
			}
			break;

		case 3: // font
			{
				int first = 0; // set to one above the index of the last non-null property
				short system_font = -1;
				int i;
				for (i = 0; i < 4; i++)
				{
					if (prop_values[i])
					{
						if (!all_default && prop_values[i]->GetDeclType() == CSS_DECL_TYPE && CSS_is_font_system_val(prop_values[i]->TypeValue()))
							system_font = prop_values[i]->TypeValue();
						else
						{
							if (all_default || !CSS_IsDefaultValue(prop_values[i]))
							{
								CSS::FormatDeclarationL(buffer, prop_values[i], first != 0, format_mode);
								first = i + 1;
							}
							else
								default_dropped = TRUE;
						}
					}
				}

				if (system_font == -1 && first == 4 && prop_values[i]) // add a '/' if we have both font-size and line-height
					buffer->AppendL("/");

				for (; i < prop_array_len; i++)
				{
					if (prop_values[i])
					{
						if (!all_default && prop_values[i]->GetDeclType() == CSS_DECL_TYPE && CSS_is_font_system_val(prop_values[i]->TypeValue()))
							system_font = prop_values[i]->TypeValue();
						else
						{
							if (all_default || !CSS_IsDefaultValue(prop_values[i]))
							{
								CSS::FormatDeclarationL(buffer, prop_values[i], first > 4, format_mode); // no space after '/'
								first = i + 1;
							}
							else
								default_dropped = TRUE;
						}
					}
				}

				if (system_font != -1)
				{
					if (first == 0)
						buffer->AppendL(CSS_GetKeywordString(system_font));
					else
						buffer->Clear();
				}

				is_set = (first != 0 || system_font != -1);
			}
			break;

		case 4: // margin, padding, border-style, border-color
			if (prop_values[0] && prop_values[1] && prop_values[2] && prop_values[3])
			{
				int level = 4;

				if (prop_values[1]->IsEqual(prop_values[3]))
				{
					level--;
					if (prop_values[0]->IsEqual(prop_values[2]))
					{
						level--;
						if (prop_values[0]->IsEqual(prop_values[1]))
							level--;
					}
				}

				for (int i = 0; i < level; i++)
					CSS::FormatDeclarationL(buffer, prop_values[i], i > 0, format_mode);

				is_set = TRUE;
			}
			break;

		case 5: // overflow
			{
				int i = 0;
				if (prop_values[0] && prop_values[1])
				{
					if (prop_values[0]->IsEqual(prop_values[1]))
						CSS::FormatDeclarationL(buffer, prop_values[0], FALSE, format_mode);
				}
				else if (prop_values[0] && CSS_IsDefaultValue(prop_values[0]) ||
						 ++i && prop_values[1] && CSS_IsDefaultValue(prop_values[1]))
				{
					CSS::FormatDeclarationL(buffer, prop_values[i], FALSE, format_mode);
				}

				is_set = TRUE;
			}
			break;

		case 6: // background
			{
				GetBackgroundShorthandPropertyL(buffer, prop_values, prop_array_len, format_mode);
				is_set = TRUE;
			}
			break;

		case 7: // columns, flex, flex-flow
			if (prop_values[0] && prop_values[1] && (prop_array_len <= 2 || prop_values[2]))
			{
				int decl_count = 0;

				for (int i=0; i<prop_array_len; i++)
					if (all_default || !CSS_IsDefaultValue(prop_values[i]))
					{
						CSS::FormatDeclarationL(buffer, prop_values[i], decl_count > 0, format_mode);
						++ decl_count;

						if (all_default)
							break;
					}

				if (decl_count == 0)
				{
					default_dropped = TRUE;
					break;
				}
			}
			is_set = TRUE;
			break;

		case 8: // border-radius
			{
				if (!prop_values[0] || !prop_values[1] || !prop_values[2] || !prop_values[3])
					break;

				/* There can be at most four values on each side of the slash (/)
				   in the border-radius shorthand (eight in total). This array
				   keeps track of how many values it is necessary to emit on each
				   side.

				   level[0] indicates how many values needed for the horizontal
				   part, and level[1] indicates how many values needed for the
				   vertical part. */
				char level[2] = { 4, 4 }; // ARRAY OK 2010-12-29 andersr

				// Figure out if the vertical radius is different for one (or more) of the values.
				BOOL vertical_radii = (prop_values[0]->GetNumberValue(0) != prop_values[0]->GetNumberValue(1)) ||
					(prop_values[1]->GetNumberValue(0) != prop_values[1]->GetNumberValue(1)) ||
					(prop_values[2]->GetNumberValue(0) != prop_values[2]->GetNumberValue(1)) ||
					(prop_values[3]->GetNumberValue(0) != prop_values[3]->GetNumberValue(1));

				const char num_radii = vertical_radii ? 2 : 1;

				/* Figure out how many values we need to output
				   (shorten the shorthand as much as possible). */
				for (int i = 0; i < num_radii; ++i)
				{
					/* border-bottom-left-radius (3) can be omitted if it's
					   equal to border-top-right-radius [1]. */
					if (prop_values[1]->GetNumberValue(i) == prop_values[3]->GetNumberValue(i))
					{
						level[i]--;
						/* border-bottom-right-radius (2) can be omitted if it's
						   equal to border-top-left-radius [0]. */
						if (prop_values[0]->GetNumberValue(i) == prop_values[2]->GetNumberValue(i))
						{
							level[i]--;
							/* If border-top-right-radius [1] is equal to
							   border-top-right-radius [0] at this point, then
							   we only need to emit at most one value. */
							if (prop_values[0]->GetNumberValue(i) == prop_values[1]->GetNumberValue(i))
								level[i]--;
						}
					}
				}

				// Output the horizontal part of the border-radius shorthand.
				for (int i = 0; i < level[0]; i++)
					CSS::FormatCssNumberL(prop_values[i]->GetNumberValue(0), prop_values[i]->GetValueType(0), buffer, i != 0);

				// Output the vertical part of the border-radius shorthand, if necessary.
				if (vertical_radii && level[1] > 0)
				{
					buffer->AppendL(level[0] > 0 ? " /" : "/");

					for (int i = 0; i < level[1]; i++)
						CSS::FormatCssNumberL(prop_values[i]->GetNumberValue(1), prop_values[i]->GetValueType(1), buffer, TRUE);
				}

				is_set = (level[0] > 0) || (vertical_radii && level[1] > 0);
			}
			break;

#ifdef CSS_TRANSITIONS
		case 9: // transitions/animations
			GetComplexShorthandPropertyL(buffer, prop_values, prop_array_len, format_mode);
			is_set = TRUE;
			break;
#endif // CSS_TRANSITIONS

		}
	} while (!is_set && default_dropped);

	for (int i=0; i < prop_array_len; i++)
		if (prop_values[i])
			ReleaseDecl(prop_values[i]);

	if (!is_set)
		buffer->Clear();
}

#ifdef CSS_TRANSITIONS

/** Check if a subset of a declaration is a default value, index given
	by 'i'. Only applicable for complex list properties for
	transitions and animations. */

static BOOL
CSS_IsSubsetDefaultValue(CSS_decl* decl, CSS_decl* suc, int i)
{
    short prop = decl->GetProperty();

    switch (prop)
    {
	case CSS_PROPERTY_transition_duration:
# ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_duration:
# endif // CSS_ANIMATIONS
		if (suc && !CSS_IsSubsetDefaultValue(suc, NULL, i))
			/* 'duration' and 'delay' have linked default values. If
			   delay is non-default, duration must be explicit no
			   matter what the duration is, or the values won't
			   round-trip through the parser. */

			return FALSE;

		// fall-through is intended
	case CSS_PROPERTY_transition_delay:
# ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_delay:
# endif // CSS_ANIMATIONS
        return (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && i < decl->ArrayValueLen() &&
				decl->GenArrayValue()[i].GetNumber() == 0);

	case CSS_PROPERTY_transition_timing_function:
# ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_timing_function:
# endif // CSS_ANIMATIONS
        return ((decl->GetDeclType() == CSS_DECL_GEN_ARRAY && i*4 < decl->ArrayValueLen() &&
				 decl->GenArrayValue()[i*4].GetReal() == 0.25f &&
				 decl->GenArrayValue()[i*4+1].GetReal() == 0.1f  &&
				 decl->GenArrayValue()[i*4+2].GetReal() == 0.25f &&
				 decl->GenArrayValue()[i*4+3].GetReal() == 1.0f));

	case CSS_PROPERTY_transition_property:
		return decl->GetDeclType() == CSS_DECL_TYPE && decl->TypeValue() == CSS_VALUE_all;

# ifdef CSS_ANIMATIONS
	case CSS_PROPERTY_animation_name:
        return (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && i < decl->ArrayValueLen() &&
				uni_str_eq(decl->GenArrayValue()[i].GetString(), UNI_L("none")));

	case CSS_PROPERTY_animation_direction:
        return (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && i < decl->ArrayValueLen() &&
				decl->GenArrayValue()[i].GetType() == CSS_VALUE_normal);

	case CSS_PROPERTY_animation_fill_mode:
        return (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && i < decl->ArrayValueLen() &&
				decl->GenArrayValue()[i].GetType() == CSS_VALUE_none);

	case CSS_PROPERTY_animation_iteration_count:
        return (decl->GetDeclType() == CSS_DECL_GEN_ARRAY && i < decl->ArrayValueLen() &&
				decl->GenArrayValue()[i].GetReal() == 1.0);
# endif // CSS_ANIMATIONS
    }

    return FALSE;
}

void
CSS_DOMStyleDeclaration::GetComplexShorthandPropertyL(TempBuffer* buffer,
													  CSS_decl** prop_values,
													  int n_prop_values,
													  CSS_SerializationFormat format_mode)
{
	BOOL all_inherit = TRUE;
	for (int i=0; i < n_prop_values; i++)
	{
		if (!prop_values[i] ||
			prop_values[i]->GetDeclType() != CSS_DECL_TYPE ||
			prop_values[i]->TypeValue() != CSS_VALUE_inherit)
			all_inherit = FALSE;
	}

	/* If all values are set to inherit, return 'inherit'. Typically
	   happens when the shorthand was set to 'inherit'. */

	if (all_inherit)
	{
		buffer->Append("inherit");
		return;
	}

	BOOL allow_default = FALSE;
	int n_layers = MAX(1, prop_values[0] ? prop_values[0]->ArrayValueLen() : 0);

again_with_all_defaults:
	BOOL space_before = FALSE;

	for (int layer_idx = 0; layer_idx < n_layers; layer_idx++)
	{
		if (layer_idx > 0 && buffer->Length() > 0)
			buffer->AppendL(',');

		for (int i=0; i < n_prop_values; i++)
		{
			if (prop_values[i])
			{
				CSS_decl* suc = i + 1 < n_prop_values ? prop_values[i + 1] : NULL;

				if (!allow_default && CSS_IsSubsetDefaultValue(prop_values[i], suc, layer_idx))
					continue;

				const int arr_len = prop_values[i]->ArrayValueLen();

				switch (prop_values[i]->GetProperty())
				{
				case CSS_PROPERTY_transition_property:
					{
						short* array = prop_values[i]->ArrayValue();

						if (array && arr_len)
							CSS::FormatCssValueL((void*)(INTPTR)array[layer_idx % arr_len],
												 CSS::CSS_VALUE_TYPE_property, buffer, space_before || layer_idx > 0);
						else
							CSS::FormatDeclarationL(buffer, prop_values[i], space_before, format_mode);
					}
					break;

				case CSS_PROPERTY_transition_delay:
				case CSS_PROPERTY_transition_duration:
# ifdef CSS_ANIMATIONS
				case CSS_PROPERTY_animation_name:
				case CSS_PROPERTY_animation_delay:
				case CSS_PROPERTY_animation_duration:
				case CSS_PROPERTY_animation_direction:
				case CSS_PROPERTY_animation_iteration_count:
				case CSS_PROPERTY_animation_fill_mode:
# endif // CSS_ANIMATIONS
					{
						const CSS_generic_value& gen_val = prop_values[i]->GenArrayValue()[layer_idx % arr_len];

						CSS::FormatCssGenericValueL(CSSProperty(prop_values[i]->GetProperty()),
													gen_val,
													buffer,
													space_before || layer_idx > 0,
													format_mode);
					}
					break;

				case CSS_PROPERTY_transition_timing_function:
# ifdef CSS_ANIMATIONS
				case CSS_PROPERTY_animation_timing_function:
# endif // CSS_ANIMATIONS
					{
						const CSS_generic_value* gen_val =
							&prop_values[i]->GenArrayValue()[(layer_idx * 4) % arr_len];

						CSS::FormatCssTimingValueL(gen_val, buffer, space_before || layer_idx > 0);
					}
					break;
				}

				space_before = TRUE;
			}
		}
	}

	if (!allow_default && buffer->Length() == 0)
	{
		allow_default = TRUE;
		goto again_with_all_defaults;
	}
}

#endif // CSS_TRANSITIONS

void
CSS_DOMStyleDeclaration::GetBackgroundShorthandPropertyL(TempBuffer* buffer,
														 CSS_decl** prop_values,
														 int n_prop_values,
														 CSS_SerializationFormat format_mode)
{
	short keyword;
	OP_ASSERT(n_prop_values == 8);

	// The following depends on the order of properties setup in CSS_GetShorthandProperties

	const BOOL background_color_is_inherit = (prop_values[0] && prop_values[0]->GetDeclType() == CSS_DECL_TYPE &&
											  prop_values[0]->TypeValue() == CSS_VALUE_inherit);

	const CSS_generic_value* background_images = prop_values[1] ? prop_values[1]->GenArrayValue() : NULL;
	const int n_background_images = prop_values[1] ? prop_values[1]->ArrayValueLen() : 0;
	const BOOL background_image_is_inherit = (prop_values[1] && prop_values[1]->GetDeclType() == CSS_DECL_TYPE &&
											  prop_values[1]->TypeValue() == CSS_VALUE_inherit);

	const CSS_generic_value* background_repeat = prop_values[2] ? prop_values[2]->GenArrayValue() : NULL;
	const int n_background_repeat = prop_values[2] ? prop_values[2]->ArrayValueLen() : 0;
	const BOOL background_repeat_is_inherit = (prop_values[2] && prop_values[2]->GetDeclType() == CSS_DECL_TYPE &&
											   prop_values[2]->TypeValue() == CSS_VALUE_inherit);

	const CSS_generic_value* background_attachment = prop_values[3] ? prop_values[3]->GenArrayValue() : NULL;
	const int n_background_attachment = prop_values[3] ? prop_values[3]->ArrayValueLen() : 0;
	const BOOL background_attachement_is_inherit = (prop_values[3] && prop_values[3]->GetDeclType() == CSS_DECL_TYPE &&
													prop_values[3]->TypeValue() == CSS_VALUE_inherit);

	const CSS_generic_value* background_position = prop_values[4] ? prop_values[4]->GenArrayValue() : NULL;
	const int n_background_position = prop_values[4] ? prop_values[4]->ArrayValueLen() : 0;
	const BOOL background_position_is_inherit = (prop_values[4] && prop_values[4]->GetDeclType() == CSS_DECL_TYPE &&
												 prop_values[4]->TypeValue() == CSS_VALUE_inherit);

	const CSS_generic_value* background_size = prop_values[5] ? prop_values[5]->GenArrayValue() : NULL;
	const int n_background_size = prop_values[5] ? prop_values[5]->ArrayValueLen() : 0;
	const BOOL background_size_is_inherit = (prop_values[5] && prop_values[5]->GetDeclType() == CSS_DECL_TYPE &&
											 prop_values[5]->TypeValue() == CSS_VALUE_inherit);

	const CSS_generic_value* background_origin = prop_values[6] ? prop_values[6]->GenArrayValue() : NULL;
	const int n_background_origin = prop_values[6] ? prop_values[6]->ArrayValueLen() : 0;
	const BOOL background_origin_is_inherit = (prop_values[6] && prop_values[6]->GetDeclType() == CSS_DECL_TYPE &&
											   prop_values[6]->TypeValue() == CSS_VALUE_inherit);

	const CSS_generic_value* background_clip = prop_values[7] ? prop_values[7]->GenArrayValue() : NULL;
	const int n_background_clip = prop_values[7] ? prop_values[7]->ArrayValueLen() : 0;
	const BOOL background_clip_is_inherit = (prop_values[7] && prop_values[7]->GetDeclType() == CSS_DECL_TYPE &&
											 prop_values[7]->TypeValue() == CSS_VALUE_inherit);

	/* If all values is set to inherit, return 'inherit'. Typically
	   the background shorthand was set to 'inherit'. */

	if (background_color_is_inherit && background_image_is_inherit && background_repeat_is_inherit &&
		background_attachement_is_inherit && background_position_is_inherit && background_size_is_inherit &&
		background_origin_is_inherit && background_clip_is_inherit)
	{
		buffer->Append("inherit");
		return;
	}

	BOOL allow_default = FALSE;
	int n_layers = MAX(1, n_background_images);

again_with_all_defaults:
	BOOL layer_has_bg_position = FALSE;
	BOOL space_before = FALSE;
	int layer_idx = 0;
	int background_size_idx = 0;
	int background_repeat_idx = 0;
	int background_position_idx = 0;

	for (; layer_idx < n_layers; layer_idx++)
	{
		if (layer_idx > 0)
			buffer->AppendL(',');

		/* background-color */

		const BOOL last_layer = (layer_idx + 1 == n_layers);
		if (last_layer && prop_values[0] && (allow_default || !CSS_IsDefaultValue(prop_values[0])))
		{
			CSS::FormatDeclarationL(buffer, prop_values[0], space_before, format_mode);
			space_before = TRUE;
		}

		/* background-image */

		if (layer_idx < n_background_images)
			if (background_images[layer_idx].value_type == CSS_FUNCTION_URL)
			{
				CSS::FormatCssValueL((void*)background_images[layer_idx].value.string,
									 CSS::CSS_VALUE_TYPE_func_url, buffer, space_before);
				space_before = TRUE;
			}
#ifdef CSS_GRADIENT_SUPPORT
			else if (background_images[layer_idx].value_type == CSS_FUNCTION_LINEAR_GRADIENT ||
				background_images[layer_idx].value_type == CSS_FUNCTION_WEBKIT_LINEAR_GRADIENT ||
				background_images[layer_idx].value_type == CSS_FUNCTION_O_LINEAR_GRADIENT ||
				background_images[layer_idx].value_type == CSS_FUNCTION_REPEATING_LINEAR_GRADIENT)
			{
				CSS::FormatCssValueL((void*)background_images[layer_idx].value.gradient,
									 CSS::CSS_VALUE_TYPE_func_linear_gradient, buffer, space_before);
				space_before = TRUE;
			}

			else if (background_images[layer_idx].value_type == CSS_FUNCTION_RADIAL_GRADIENT ||
				background_images[layer_idx].value_type == CSS_FUNCTION_REPEATING_RADIAL_GRADIENT)
			{
				CSS::FormatCssValueL((void*)background_images[layer_idx].value.gradient,
									 CSS::CSS_VALUE_TYPE_func_radial_gradient, buffer, space_before);
				space_before = TRUE;
			}
#endif // CSS_GRADIENT_SUPPORT
			else if (allow_default)
			{
				CSS::FormatCssValueL((void*)(long)CSS_VALUE_none, CSS::CSS_VALUE_TYPE_keyword, buffer, space_before);
				space_before = TRUE;
			}

		/* background-repeat */

		if (n_background_repeat)
		{
			while (background_repeat_idx < n_background_repeat &&
				   background_repeat[background_repeat_idx].value_type != CSS_COMMA)
			{
				OP_ASSERT(background_repeat[background_repeat_idx].value_type == CSS_IDENT);
				keyword = background_repeat[background_repeat_idx].value.type;
				if (keyword != CSS_VALUE_repeat || allow_default)
				{
					CSS::FormatCssValueL((void*)(long)keyword, CSS::CSS_VALUE_TYPE_keyword, buffer, space_before);
					space_before = TRUE;
				}
				background_repeat_idx++;
			}

			if (background_repeat_idx == n_background_repeat)
				background_repeat_idx = 0;
			else
				background_repeat_idx++;
		}

		/* background-attachment */

		if (n_background_attachment)
		{
			OP_ASSERT(background_attachment[layer_idx % n_background_attachment].value_type == CSS_IDENT);
			keyword = background_attachment[layer_idx % n_background_attachment].value.type;
			if (keyword != CSS_VALUE_scroll || allow_default)
			{
				CSS::FormatCssValueL((void*)(long)keyword, CSS::CSS_VALUE_TYPE_keyword, buffer, space_before);
				space_before = TRUE;
			}
		}

		/* background-position */

		if (n_background_position)
		{
			while (background_position_idx < n_background_position &&
				   background_position[background_position_idx].value_type != CSS_COMMA)
			{
				if (background_position[background_position_idx].value_type == CSS_IDENT)
				{
					keyword = background_position[background_position_idx].value.type;
					CSS::FormatCssValueL((void*)(long)keyword, CSS::CSS_VALUE_TYPE_keyword, buffer, space_before);
					layer_has_bg_position = TRUE;
					space_before = TRUE;
				}
				else
				{
					float val = background_position[background_position_idx].value.real;
					short typ = background_position[background_position_idx].value_type;

					if (val != 0 || allow_default)
					{
						CSS::FormatCssNumberL(val, typ, buffer, space_before);
						layer_has_bg_position = TRUE;
						space_before = TRUE;
					}
				}

				background_position_idx++;
			}

			if (background_position_idx == n_background_position)
				background_position_idx = 0;
			else
				background_position_idx++;
		}

		/* background-size */

		if (n_background_size)
		{
			BOOL first = TRUE;
			BOOL slash_outputted = FALSE;

			while (background_size_idx < n_background_size &&
				   background_size[background_size_idx].value_type != CSS_COMMA)
			{
				if (background_size[background_size_idx].value_type == CSS_IDENT)
				{
					keyword = background_size[background_size_idx].value.type;

					BOOL is_default = first && keyword == CSS_VALUE_auto &&
						(background_size_idx + 1 < n_background_size && background_size[background_size_idx+1].value_type == CSS_COMMA ||
						 background_size_idx + 1 == n_background_size);

					if (allow_default || !is_default)
					{
						if (!slash_outputted)
						{
							if (space_before)
								buffer->AppendL(" ");

							if (!layer_has_bg_position)
								buffer->AppendL("0% 0% /");
							else
								buffer->AppendL("/");
						}

						slash_outputted = TRUE;

						CSS::FormatCssValueL((void*)(long)keyword, CSS::CSS_VALUE_TYPE_keyword, buffer, TRUE);
						space_before = TRUE;
					}
				}
				else
				{
					if (!slash_outputted)
						if (!layer_has_bg_position)
							buffer->AppendL(" 0% 0% /");
						else
							buffer->AppendL(" /");
					slash_outputted = TRUE;

					float val = background_size[background_size_idx].value.real;
					short typ = background_size[background_size_idx].value_type;
					CSS::FormatCssNumberL(val, typ, buffer, TRUE);
					space_before = TRUE;
				}

				first = FALSE;
				background_size_idx++;
			}

			if (background_size_idx == n_background_size)
				background_size_idx = 0;
			else
				background_size_idx++;
		}

		/* background-origin */

		if (n_background_origin)
		{
			OP_ASSERT(background_origin[layer_idx % n_background_origin].value_type == CSS_IDENT);
			keyword = background_origin[layer_idx % n_background_origin].value.type;
			if (keyword != CSS_VALUE_padding_box || allow_default)
			{
				CSS::FormatCssValueL((void*)(long)keyword, CSS::CSS_VALUE_TYPE_keyword, buffer, space_before);
				space_before = TRUE;
			}
		}

		/* background-clip */

		if (n_background_clip)
		{
			OP_ASSERT(background_clip[layer_idx % n_background_clip].value_type == CSS_IDENT);
			keyword = background_clip[layer_idx % n_background_clip].value.type;
			if (keyword != CSS_VALUE_border_box || allow_default)
			{
				CSS::FormatCssValueL((void*)(long)keyword, CSS::CSS_VALUE_TYPE_keyword, buffer, space_before);
				space_before = TRUE;
			}
		}

		layer_has_bg_position = FALSE;
	}

	if (!allow_default && buffer->Length() == 0)
	{
		allow_default = TRUE;
		goto again_with_all_defaults;
	}
}

CSS_decl*
CSS_DOMStyleDeclaration::GetDecl(short property)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_profile = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_profile)
	{
		if (m_type != NORMAL)
		{
			/* We can't compute style in inactive documents, because it might
			   require a reflow, and reflowing an inactive document is dangerous
			   and wrong. */
			if (doc->IsCurrentDoc())
			{
				HTML_Element* root = hld_profile->GetRoot();
				if (root && root->IsAncestorOf(m_element))
				{
#ifdef CURRENT_STYLE_SUPPORT
					if (m_type == CURRENT)
						return LayoutProperties::GetCurrentStyleDecl(m_element, property, m_pseudo_elm, hld_profile);
					else
#endif
						return LayoutProperties::GetComputedDecl(m_element, property, m_pseudo_elm, hld_profile);
				}
			}
		}
		else
		{
			CSS_property_list* css = GetPropertyList();
			if (css && property >= 0)
			{
				CSS_decl* decl = css->GetLastDecl();
				CSS_decl* ret_decl = NULL;
				while (decl)
				{
					if (decl->GetProperty() == property)
					{
						if (decl->GetImportant())
							return decl;
						else if (!ret_decl)
							ret_decl = decl;
					}
					decl = decl->Pred();
				}
				return ret_decl;
			}
		}
	}

	return NULL;
}

void
CSS_DOMStyleDeclaration::ReleaseDecl(CSS_decl* decl)
{
	if (m_type != NORMAL)
		OP_DELETE(decl);
}

OP_STATUS
CSS_DOMStyleDeclaration::Update(short property)
{
	FramesDocument* frames_doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_profile = frames_doc ? frames_doc->GetHLDocProfile() : NULL;

	if (!hld_profile)
		return OpStatus::OK;

	if (m_rule)
	{
		m_rule->PropertyListChanged();
		CSS* sheet = m_element->GetCSS();
		OP_ASSERT(sheet);
		sheet->SetModified();
		// Should really only need to do MarkAffectedElementsDirty
		// for the selector of this rule.
		sheet->Added(hld_profile->GetCSSCollection(), frames_doc);
		return OpStatus::OK;
	}

	return hld_profile->GetLayoutWorkplace()->ApplyPropertyChanges(m_element, 0, FALSE);
}

URL CSS_DOMStyleDeclaration::GetBaseURL()
{
	if (m_rule)
		return m_element->GetCSS()->GetBaseURL();
	else
	{
		FramesDocument* frames_doc = m_environment->GetFramesDocument();
		HLDocProfile* hld_profile = frames_doc ? frames_doc->GetHLDocProfile() : NULL;
		if (hld_profile)
		{
			URL* base_url = hld_profile->BaseURL();
			if (base_url)
				return *base_url;
		}
		return URL();
	}
}

CSS_DOMRule::Type CSS_DOMRule::GetType()
{
	if (m_rule)
	{
		switch (m_rule->GetType())
		{
		case CSS_Rule::UNKNOWN: return UNKNOWN;
		case CSS_Rule::STYLE: return STYLE;
		case CSS_Rule::CHARSET: return CHARSET;
		case CSS_Rule::IMPORT: return IMPORT;
		case CSS_Rule::SUPPORTS: return SUPPORTS;
		case CSS_Rule::MEDIA: return MEDIA;
		case CSS_Rule::FONT_FACE: return FONT_FACE;
		case CSS_Rule::PAGE: return PAGE;
		case CSS_Rule::KEYFRAME: return KEYFRAME;
		case CSS_Rule::KEYFRAMES: return KEYFRAMES;
		case CSS_Rule::NAMESPACE: return NAMESPACE;
		case CSS_Rule::VIEWPORT: return VIEWPORT;
		}
	}
	return UNKNOWN;
}

OP_STATUS
CSS_DOMRule::GetText(TempBuffer* buffer)
{
	if (m_rule)
		return m_rule->GetCssText(m_element->GetCSS(), buffer);
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::SetText(const uni_char* text, CSS_DOMException& exception)
{
	FramesDocument* frames_doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_profile = frames_doc ? frames_doc->GetHLDocProfile() : NULL;

	if (hld_profile && m_rule)
	{
		CSS* sheet = m_element->GetCSS();
		CSS_PARSE_STATUS stat = m_rule->SetCssText(hld_profile, sheet, text, uni_strlen(text));
		if (stat == CSSParseStatus::SYNTAX_ERROR)
		{
			exception = CSS_DOMEXCEPTION_SYNTAX_ERR;
			return OpStatus::ERR;
		}
		else if (stat == CSSParseStatus::HIERARCHY_ERROR)
		{
			exception = CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR;
			return OpStatus::ERR;
		}
		else
		{
			OP_ASSERT(sheet);
			sheet->SetModified();
			return stat;
		}
	}
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::GetSelectorText(TempBuffer* buffer)
{
	if (m_rule && (m_rule->GetType() == CSS_Rule::STYLE || m_rule->GetType() == CSS_Rule::PAGE))
		return static_cast<CSS_StyleRule*>(m_rule)->GetSelectorText(m_element->GetCSS(), buffer);
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::SetSelectorText(const uni_char* text, CSS_DOMException& exception)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_prof && m_rule && (m_rule->GetType() == CSS_Rule::STYLE || m_rule->GetType() == CSS_Rule::PAGE))
	{
		CSS* sheet = m_element->GetCSS();
		CSS_PARSE_STATUS stat = static_cast<CSS_StyleRule*>(m_rule)->SetSelectorText(hld_prof, sheet, text, uni_strlen(text));
		if (stat == CSSParseStatus::SYNTAX_ERROR)
		{
			exception = CSS_DOMEXCEPTION_SYNTAX_ERR;
			return OpStatus::ERR;
		}
		else
		{
			OP_ASSERT(sheet);
			sheet->SetModified();
			return stat;
		}
	}
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::GetStyle(CSS_DOMStyleDeclaration*& styledeclaration)
{
	styledeclaration = OP_NEW(CSS_DOMStyleDeclaration, (m_environment, m_element, this, CSS_DOMStyleDeclaration::NORMAL));
	return styledeclaration ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

OP_STATUS
CSS_DOMRule::GetParentRule(CSS_DOMRule*& parent)
{
	parent = NULL;
	if (m_rule)
	{
		CSS_Rule* parent_rule = NULL;

		if (m_rule->GetType() == CSS_Rule::STYLE ||
			m_rule->GetType() == CSS_Rule::PAGE ||
			m_rule->GetType() == CSS_Rule::FONT_FACE ||
			m_rule->GetType() == CSS_Rule::VIEWPORT ||
#ifdef CSS_ANIMATIONS
			m_rule->GetType() == CSS_Rule::KEYFRAMES ||
#endif // CSS_ANIMATIONS
			m_rule->GetType() == CSS_Rule::SUPPORTS ||
			m_rule->GetType() == CSS_Rule::MEDIA
			)
		{
			parent_rule = static_cast<CSS_BlockRule*>(m_rule)->GetConditionalRule();
		}
#ifdef CSS_ANIMATIONS
		else if (m_rule->GetType() == CSS_Rule::KEYFRAME)
		{
			parent_rule = static_cast<CSS_KeyframeRule*>(m_rule)->GetKeyframesRule();
		}
#endif // CSS_ANIMATIONS

		if (parent_rule)
		{
			parent = parent_rule->GetDOMRule();
			if (!parent)
			{
				parent = OP_NEW(CSS_DOMRule, (m_environment, parent_rule, m_element));
				if (!parent)
					return OpStatus::ERR_NO_MEMORY;
				parent_rule->SetDOMRule(parent);
			}
		}
	}
	return OpStatus::OK;
}

#ifdef STYLE_PROPNAMEANDVALUE_API

unsigned int
CSS_DOMRule::GetPropertyCount()
{
	CSS_property_list* props = GetPropertyList();
	if (props)
		return props->GetLength();
	else
		return 0;
}

OP_STATUS
CSS_DOMRule::GetPropertyNameAndValue(unsigned int idx, TempBuffer* name, TempBuffer* value, BOOL& important)
{
	CSS_property_list* props = GetPropertyList();
	if (props)
		return props->GetPropertyNameAndValue(idx, name, value, important);
	else
		return OpStatus::OK;
}

#endif // STYLE_PROPNAMEANDVALUE_API

#ifdef SCOPE_CSS_RULE_ORIGIN
unsigned
CSS_DOMRule::GetLineNumber() const
{
	if (m_rule)
		if (m_rule->GetType() == CSS_Rule::STYLE)
			return static_cast<CSS_StyleRule*>(m_rule)->GetLineNumber();
		else if (m_rule->GetType() == CSS_Rule::IMPORT)
			return static_cast<CSS_ImportRule*>(m_rule)->GetLineNumber();

	return 0;
}
#endif // SCOPE_CSS_RULE_ORIGIN

CSS_property_list*
CSS_DOMRule::GetPropertyList()
{
	if (m_rule)
	{
		CSS_Rule::Type type = m_rule->GetType();

#ifdef CSS_ANIMATIONS
		if (type == CSS_Rule::KEYFRAME)
			return static_cast<CSS_KeyframeRule*>(m_rule)->GetPropertyList();
		else
#endif // CSS_ANIMATIONS
			if (type == CSS_Rule::STYLE ||
				type == CSS_Rule::PAGE ||
				type == CSS_Rule::FONT_FACE ||
				type == CSS_Rule::VIEWPORT)
				return static_cast<CSS_DeclarationBlockRule*>(m_rule)->GetPropertyList();
	}
	return NULL;
}

void
CSS_DOMRule::SetPropertyList(CSS_property_list* new_props)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_prof && m_rule)
	{
		CSS_Rule::Type type = m_rule->GetType();
		if (type == CSS_Rule::STYLE ||
			type == CSS_Rule::PAGE ||
			type == CSS_Rule::FONT_FACE ||
			type == CSS_Rule::VIEWPORT)
		{
			static_cast<CSS_DeclarationBlockRule*>(m_rule)->SetPropertyList(hld_prof, new_props, FALSE);
			CSS* sheet = m_element->GetCSS();
			OP_ASSERT(sheet);
			sheet->SetModified();
			// Should really only need to do MarkAffectedElementsDirty
			// for the selector of this rule.
			sheet->Added(hld_prof->GetCSSCollection(), doc);
		}
	}
}

// Methods for conditional rules.
unsigned int
CSS_DOMRule::GetRuleCount()
{
	if (m_rule)
	{
		if (m_rule->GetType() == CSS_Rule::MEDIA ||
		    m_rule->GetType() == CSS_Rule::SUPPORTS)
			return static_cast<CSS_ConditionalRule*>(m_rule)->GetRuleCount();
#ifdef CSS_ANIMATIONS
		else if (m_rule->GetType() == CSS_Rule::KEYFRAMES)
			return static_cast<CSS_KeyframesRule*>(m_rule)->GetRuleCount();
#endif // CSS_ANIMATIONS
	}

	return 0;
}

OP_STATUS
CSS_DOMRule::GetRule(CSS_DOMRule*& rule, unsigned int index, CSS_DOMException& exception)
{
	OP_ASSERT(m_rule && (m_rule->GetType() == CSS_Rule::SUPPORTS ||
	                     m_rule->GetType() == CSS_Rule::MEDIA ||
	                     m_rule->GetType() == CSS_Rule::KEYFRAMES));

	CSS_Rule* css_rule = NULL;

	if (m_rule->GetType() == CSS_Rule::SUPPORTS ||
	    m_rule->GetType() == CSS_Rule::MEDIA)
	{
		css_rule = static_cast<CSS_ConditionalRule*>(m_rule)->GetRule(index);
		if (!css_rule)
		{
			exception = CSS_DOMEXCEPTION_INDEX_SIZE_ERR;
			return OpStatus::ERR;
		}
		rule = css_rule->GetDOMRule();
	}
#ifdef CSS_ANIMATIONS
	else if (m_rule->GetType() == CSS_Rule::KEYFRAMES)
	{
		css_rule = static_cast<CSS_KeyframesRule*>(m_rule)->GetRule(index);
		if (!css_rule)
		{
			exception = CSS_DOMEXCEPTION_INDEX_SIZE_ERR;
			return OpStatus::ERR;
		}
		rule = css_rule->GetDOMRule();
	}
#endif // CSS_ANIMATIONS

	if (!rule)
	{
		rule = OP_NEW(CSS_DOMRule, (m_environment, css_rule, m_element));
		if (!rule)
			return OpStatus::ERR_NO_MEMORY;
		css_rule->SetDOMRule(rule);
	}
	return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::InsertRule(const uni_char* rule, unsigned int index, CSS_DOMException& exception)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_prof && m_rule && (m_rule->GetType() == CSS_Rule::SUPPORTS ||
	                           m_rule->GetType() == CSS_Rule::MEDIA))
	{
		CSS_ConditionalRule* conditional_rule = static_cast<CSS_ConditionalRule*>(m_rule);
		if (index <= conditional_rule->GetRuleCount())
		{
			CSS* sheet = m_element->GetCSS();
			OP_ASSERT(sheet);
			CSS_PARSE_STATUS stat = conditional_rule->InsertRule(hld_prof, sheet, rule, uni_strlen(rule), index);
			if (stat == CSSParseStatus::SYNTAX_ERROR)
			{
				exception = CSS_DOMEXCEPTION_SYNTAX_ERR;
				return OpStatus::ERR;
			}
			else if (stat == CSSParseStatus::HIERARCHY_ERROR)
			{
				exception = CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR;
				return OpStatus::ERR;
			}
			else
			{
				if (stat == OpStatus::OK)
				{
					sheet->SetModified();
					sheet->Added(hld_prof->GetCSSCollection(), doc);
				}
				return stat;
			}
		}
		else
		{
			exception = CSS_DOMEXCEPTION_INDEX_SIZE_ERR;
			return OpStatus::ERR;
		}
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
CSS_DOMRule::DeleteRule(unsigned int index, CSS_DOMException& exception)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_prof && m_rule && (m_rule->GetType() == CSS_Rule::SUPPORTS ||
	                           m_rule->GetType() == CSS_Rule::MEDIA))
	{
		CSS* sheet = m_element->GetCSS();
		BOOL deleted = static_cast<CSS_ConditionalRule*>(m_rule)->DeleteRule(hld_prof, sheet, index);
		if (!deleted)
		{
			exception = CSS_DOMEXCEPTION_INDEX_SIZE_ERR;
			return OpStatus::ERR;
		}
		else
		{
			OP_ASSERT(sheet);
			sheet->SetModified();
			hld_prof->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_ALL);
			return OpStatus::OK;
		}
	}
	else
		return OpStatus::ERR;
}

// Methods for MEDIA rules.
CSS_MediaObject*
CSS_DOMRule::GetMediaObject(BOOL create)
{
	CSS_MediaObject* media_object = NULL;
	if (m_rule)
	{
		if (m_rule->GetType() == CSS_Rule::MEDIA)
			media_object = static_cast<CSS_MediaRule*>(m_rule)->GetMediaObject(create);
		else if (m_rule->GetType() == CSS_Rule::IMPORT)
			media_object = static_cast<CSS_ImportRule*>(m_rule)->GetMediaObject(create);
	}
	return media_object;
}

HTML_Element*
CSS_DOMRule::GetImportLinkElm()
{
	if (m_rule && m_rule->GetType() == CSS_Rule::IMPORT)
		return static_cast<CSS_ImportRule*>(m_rule)->GetElement();
	else
		return NULL;
}

const uni_char*
CSS_DOMRule::GetHRef()
{
	if (m_rule && m_rule->GetType() == CSS_Rule::IMPORT)
	{
		HTML_Element* elm = static_cast<CSS_ImportRule*>(m_rule)->GetElement();
		if (elm)
			return elm->GetStringAttr(Markup::HA_HREF);
	}
	return NULL;
}

const uni_char*
CSS_DOMRule::GetEncoding()
{
	if (m_rule && m_rule->GetType() == CSS_Rule::CHARSET)
		return static_cast<CSS_CharsetRule*>(m_rule)->GetCharset();
	else
		return NULL;
}

#ifdef CSS_ANIMATIONS

const uni_char*
CSS_DOMRule::GetKeyframesName() const
{
	if (m_rule && m_rule->GetType() == CSS_Rule::KEYFRAMES)
		return static_cast<CSS_KeyframesRule*>(m_rule)->GetName();
	else
		return NULL;
}

OP_STATUS
CSS_DOMRule::SetKeyframesName(const uni_char* name)
{
	if (m_rule && m_rule->GetType() == CSS_Rule::KEYFRAMES)
		return static_cast<CSS_KeyframesRule*>(m_rule)->SetName(name);
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::GetKeyframeKeyText(TempBuffer* buf) const
{
	if (m_rule && m_rule->GetType() == CSS_Rule::KEYFRAME)
		return static_cast<CSS_KeyframeRule*>(m_rule)->GetKeyText(buf);
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::SetKeyframeKeyText(const uni_char* key_text, unsigned key_text_length)
{
	if (m_rule && m_rule->GetType() == CSS_Rule::KEYFRAME)
		return static_cast<CSS_KeyframeRule*>(m_rule)->SetKeyText(key_text, key_text_length);
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMRule::AppendKeyframeRule(const uni_char* rule, unsigned rule_length)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_prof && m_rule && m_rule->GetType() == CSS_Rule::KEYFRAMES)
	{
		CSS_KeyframesRule* keyframes_rule = static_cast<CSS_KeyframesRule*>(m_rule);

		CSS* sheet = m_element->GetCSS();
		OP_ASSERT(sheet);
		CSS_PARSE_STATUS stat = keyframes_rule->AppendRule(hld_prof, sheet, rule, rule_length);
		if (stat == CSSParseStatus::SYNTAX_ERROR)
			return OpStatus::ERR;

		return stat;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
CSS_DOMRule::DeleteKeyframeRule(const uni_char* key_text, unsigned key_text_length)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_prof && m_rule && m_rule->GetType() == CSS_Rule::KEYFRAMES)
	{
		CSS_KeyframesRule* keyframes_rule = static_cast<CSS_KeyframesRule*>(m_rule);

		return keyframes_rule->DeleteRule(key_text, key_text_length);
	}
	else
		return OpStatus::ERR;
}

OP_STATUS
CSS_DOMRule::FindKeyframeRule(CSS_DOMRule *&rule, const uni_char* key_text, unsigned key_text_length)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	if (hld_prof && m_rule && m_rule->GetType() == CSS_Rule::KEYFRAMES)
	{
		CSS_KeyframesRule* keyframes_rule = static_cast<CSS_KeyframesRule*>(m_rule);

		CSS_KeyframeRule* keyframe_rule;
		RETURN_IF_ERROR(keyframes_rule->FindRule(keyframe_rule, key_text, key_text_length));

		if (keyframe_rule)
		{
			rule = keyframe_rule->GetDOMRule();
			if (!rule)
			{
				rule = OP_NEW(CSS_DOMRule, (m_environment, keyframe_rule, m_element));
				if (!rule)
					return OpStatus::ERR_NO_MEMORY;
				keyframe_rule->SetDOMRule(rule);
			}
		}
		else
			rule = NULL;

		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

#endif // CSS_ANIMATIONS

const uni_char*
CSS_DOMRule::GetNamespacePrefix()
{
	if (m_rule && m_rule->GetType() == CSS_Rule::NAMESPACE)
		return static_cast<CSS_NamespaceRule*>(m_rule)->GetPrefix();
	else
		return NULL;
}

const uni_char*
CSS_DOMRule::GetNamespaceURI()
{
	if (m_rule && m_rule->GetType() == CSS_Rule::NAMESPACE)
		return static_cast<CSS_NamespaceRule*>(m_rule)->GetURI();
	else
		return NULL;
}

void
CSS_DOMRule::Unref()
{
	OP_ASSERT(m_ref_count > 0);
	if (--m_ref_count == 0)
	{
		if (m_rule)
			m_rule->SetDOMRule(NULL);
		OP_DELETE(this);
	}
}

// CSSStylesheet

CSS_DOMStyleSheet::CSS_DOMStyleSheet(DOM_Environment* environment, HTML_Element* element)
	: m_environment(environment),
	  m_element(element)
{
}

unsigned int
CSS_DOMStyleSheet::GetRuleCount()
{
	CSS* stylesheet = m_element->GetCSS();
	if (stylesheet)
		return stylesheet->GetRuleCount();
	else
		return 0;
}

OP_STATUS
CSS_DOMStyleSheet::GetRule(CSS_DOMRule*& rule, unsigned int index, CSS_DOMException& exception)
{
	CSS* stylesheet = m_element->GetCSS();
	CSS_Rule* css_rule;

	if (!stylesheet || !(css_rule = stylesheet->GetRule(index)))
	{
		exception = CSS_DOMEXCEPTION_INDEX_SIZE_ERR;
		return OpStatus::ERR;
	}

	rule = css_rule->GetDOMRule();
	if (!rule)
	{
		rule = OP_NEW(CSS_DOMRule, (m_environment, css_rule, m_element));
		if (!rule)
			return OpStatus::ERR_NO_MEMORY;
		css_rule->SetDOMRule(rule);
	}
	return OpStatus::OK;
}

OP_STATUS
CSS_DOMStyleSheet::InsertRule(const uni_char* rule, unsigned int index, CSS_DOMException& exception)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;
	CSS* stylesheet = m_element->GetCSS();

	if (!hld_prof || !stylesheet)
	{
		exception = CSS_DOMEXCEPTION_NO_MODIFICATION_ALLOWED_ERR;
		return OpStatus::ERR;
	}

	if (index <= stylesheet->GetRuleCount())
	{
		CSS_PARSE_STATUS stat = stylesheet->ParseAndInsertRule(hld_prof, stylesheet->GetRule(index), NULL, NULL, FALSE, CSS_TOK_DOM_RULE, rule, uni_strlen(rule));
		if (stat == CSSParseStatus::SYNTAX_ERROR)
		{
			exception = CSS_DOMEXCEPTION_SYNTAX_ERR;
			return OpStatus::ERR;
		}
		else if (stat == CSSParseStatus::HIERARCHY_ERROR)
		{
			exception = CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR;
			return OpStatus::ERR;
		}
		else
		{
			if (stat == OpStatus::OK)
				stylesheet->SetModified();

			return stat;
		}
	}
	else
	{
		exception = CSS_DOMEXCEPTION_INDEX_SIZE_ERR;
		return OpStatus::ERR;
	}
}

OP_STATUS
CSS_DOMStyleSheet::DeleteRule(unsigned int index, CSS_DOMException& exception)
{
	CSS* stylesheet = m_element->GetCSS();
	if (stylesheet)
	{
		FramesDocument* doc = m_environment->GetFramesDocument();
		HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;
		BOOL deleted = stylesheet->DeleteRule(hld_prof, index);
		if (!deleted)
		{
			exception = CSS_DOMEXCEPTION_INDEX_SIZE_ERR;
			return OpStatus::ERR;
		}
		else
		{
			stylesheet->SetModified();
			if (hld_prof)
				hld_prof->GetCSSCollection()->StyleChanged(CSSCollection::CHANGED_ALL);
		}
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS
CSS_DOMStyleSheet::GetImportRule(CSS_DOMRule*& import_rule, HTML_Element* sheet_elm, DOM_Environment* dom_env)
{
	OP_ASSERT(sheet_elm->IsCssImport());

	HTML_Element* parent_sheet_elm = sheet_elm->Parent();
	CSS* parent_css = parent_sheet_elm->GetCSS();
	CSS_ImportRule* css_import_rule = parent_css->FindImportRule(sheet_elm);

	CSS_DOMRule* dom_rule = css_import_rule->GetDOMRule();
	if (!dom_rule)
	{
		dom_rule = OP_NEW(CSS_DOMRule, (dom_env, css_import_rule, parent_sheet_elm));
		if (!dom_rule)
			return OpStatus::ERR_NO_MEMORY;
		css_import_rule->SetDOMRule(dom_rule);
	}

	import_rule = dom_rule;
	return OpStatus::OK;
}

// CSSMediaList
CSS_DOMMediaList::CSS_DOMMediaList(DOM_Environment* environment, HTML_Element* sheet_elm)
	: m_environment(environment),
	  m_element(sheet_elm),
	  m_rule(NULL)
{
}

CSS_DOMMediaList::CSS_DOMMediaList(DOM_Environment* environment, CSS_DOMRule* media_rule)
	: m_environment(environment),
	  m_element(NULL),
	  m_rule(media_rule)
{
	m_rule->Ref();
}

CSS_DOMMediaList::~CSS_DOMMediaList()
{
	if (m_rule)
		m_rule->Unref();
}

unsigned int
CSS_DOMMediaList::GetMediaCount()
{
	CSS_MediaObject* media_object = GetMediaObject();
	if (media_object)
		return media_object->GetLength();
	else
		return 0;
}

OP_STATUS
CSS_DOMMediaList::GetMedia(TempBuffer* buffer)
{
	CSS_MediaObject* media_object = GetMediaObject();
	if (media_object)
	{
		TRAP_AND_RETURN(ret, media_object->GetMediaStringL(buffer));
	}
	return OpStatus::OK;
}

OP_STATUS
CSS_DOMMediaList::SetMedia(const uni_char* text, CSS_DOMException& exception)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	CSS_MediaObject* media_object = GetMediaObject(TRUE);

	if (hld_prof && media_object)
	{
		BOOL eval_before = (media_object->EvaluateMediaTypes(doc) & (doc->GetMediaType()|CSS_MEDIA_TYPE_ALL)) != 0;
		CSS_PARSE_STATUS stat = media_object->SetMediaText(text, uni_strlen(text), FALSE);
		if (stat == CSSParseStatus::SYNTAX_ERROR)
		{
			exception = CSS_DOMEXCEPTION_SYNTAX_ERR;
			return OpStatus::ERR;
		}
		else if (stat == CSSParseStatus::HIERARCHY_ERROR)
		{
			exception = CSS_DOMEXCEPTION_HIERARCHY_REQUEST_ERR;
			return OpStatus::ERR;
		}
		else
		{
			if (stat == OpStatus::OK)
			{
				CSS* sheet = GetCSS();
				if (sheet)
				{
					sheet->SetModified();
					RETURN_IF_ERROR(media_object->AddQueryLengths(sheet));
					hld_prof->SetHasMediaStyle(media_object->GetMediaTypes());

					if (eval_before != ((media_object->EvaluateMediaTypes(doc) & (doc->GetMediaType()|CSS_MEDIA_TYPE_ALL)) != 0))
					{
						if (eval_before)
							sheet->Removed(hld_prof->GetCSSCollection(), doc);
						else
							sheet->Added(hld_prof->GetCSSCollection(), doc);

						sheet->MediaAttrChanged();
					}
				}
			}
			return stat;
		}
	}
	return OpStatus::OK;
}

OP_STATUS
CSS_DOMMediaList::GetMedium(TempBuffer* buffer, unsigned int index, CSS_DOMException& exception)
{
	CSS_MediaObject* media_object = GetMediaObject();
	if (media_object)
	{
		return media_object->GetItemString(buffer, index);
	}
	else
		return OpStatus::OK;
}

OP_STATUS
CSS_DOMMediaList::AppendMedium(const uni_char* medium, CSS_DOMException& exception)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	CSS_MediaObject* media_object = GetMediaObject(TRUE);

	if (hld_prof && media_object)
	{
		BOOL deleted;
		BOOL eval_before = (media_object->EvaluateMediaTypes(doc) & (doc->GetMediaType()|CSS_MEDIA_TYPE_ALL)) != 0;
		CSS_PARSE_STATUS stat = media_object->AppendMedium(medium, uni_strlen(medium), deleted);
		if (stat == CSSParseStatus::SYNTAX_ERROR)
		{
			exception = CSS_DOMEXCEPTION_INVALID_CHARACTER_ERR; /* Not necessarily correct. */
			return OpStatus::ERR;
		}
		else
		{
			if (stat == OpStatus::OK)
			{
				CSS* sheet = GetCSS();
				if (sheet)
				{
					sheet->SetModified();
					RETURN_IF_ERROR(media_object->AddQueryLengths(sheet));
					hld_prof->SetHasMediaStyle(media_object->GetMediaTypes());

					if (eval_before != ((media_object->EvaluateMediaTypes(doc) & (doc->GetMediaType()|CSS_MEDIA_TYPE_ALL)) != 0))
					{
						if (eval_before)
							sheet->Removed(hld_prof->GetCSSCollection(), doc);
						else
							sheet->Added(hld_prof->GetCSSCollection(), doc);

						sheet->MediaAttrChanged();
					}
				}
			}
			return stat;
		}
	}
	return OpStatus::OK;
}

OP_STATUS
CSS_DOMMediaList::DeleteMedium(const uni_char* medium, CSS_DOMException& exception)
{
	FramesDocument* doc = m_environment->GetFramesDocument();
	HLDocProfile* hld_prof = doc ? doc->GetHLDocProfile() : NULL;

	CSS_MediaObject* media_object = GetMediaObject();

	if (hld_prof && media_object)
	{
		BOOL deleted;
		BOOL eval_before = (media_object->EvaluateMediaTypes(doc) & (doc->GetMediaType()|CSS_MEDIA_TYPE_ALL)) != 0;
		CSS_PARSE_STATUS stat = media_object->DeleteMedium(medium, uni_strlen(medium), deleted);
		if (stat == CSSParseStatus::SYNTAX_ERROR)
		{
			exception = CSS_DOMEXCEPTION_INVALID_CHARACTER_ERR;
			return OpStatus::ERR;
		}
		else if (stat == OpStatus::OK && !deleted)
		{
			exception = CSS_DOMEXCEPTION_NOT_FOUND_ERR;
			return OpStatus::ERR;
		}
		else
		{
			if (stat == OpStatus::OK)
			{
				CSS* sheet = GetCSS();
				if (sheet)
				{
					sheet->SetModified();
					if (eval_before != ((media_object->EvaluateMediaTypes(doc) & (doc->GetMediaType()|CSS_MEDIA_TYPE_ALL)) != 0))
					{
						if (eval_before)
							sheet->Removed(hld_prof->GetCSSCollection(), doc);
						else
							sheet->Added(hld_prof->GetCSSCollection(), doc);

						sheet->MediaAttrChanged();
					}
				}
			}
			return stat;
		}
	}
	else
	{
		exception = CSS_DOMEXCEPTION_NOT_FOUND_ERR;
		return OpStatus::ERR;
	}
}

CSS_MediaObject*
CSS_DOMMediaList::GetMediaObject(BOOL create)
{
	CSS_MediaObject* media_object = NULL;
	if (m_element)
	{
		media_object = m_element->GetLinkStyleMediaObject();
		if (!media_object && create)
		{
			media_object = OP_NEW(CSS_MediaObject, ());
			if (media_object)
				m_element->SetLinkStyleMediaObject(media_object);
		}
	}
	else
	{
		OP_ASSERT(m_rule);
		media_object = m_rule->GetMediaObject(create);
	}
	return media_object;
}

CSS*
CSS_DOMMediaList::GetCSS()
{
	if (m_element)
		return m_element->GetCSS();
	else
	{
		OP_ASSERT(m_rule);
		return m_rule->GetCSS();
	}
}

#ifdef STYLE_DOM_SELECTORS_API

OP_STATUS CSS_QuerySelector(HLDocProfile* hld_profile,
							const uni_char* selectors,
							HTML_Element* root,
							int query_flags,
							CSS_QuerySelectorListener* listener,
							CSS_DOMException& exception)
{
	// Parse selector.
	OP_STATUS stat = OpStatus::OK;
	CSS_StyleRule* OP_MEMORY_VAR dummy_rule = OP_NEW(CSS_StyleRule, ());
	CSS_Buffer* css_buf = OP_NEW(CSS_Buffer, ());

	if (dummy_rule && css_buf && css_buf->AllocBufferArrays(1))
	{
		css_buf->AddBuffer(selectors, uni_strlen(selectors));

		URL base_url;
		CSS_Parser* parser = OP_NEW(CSS_Parser, (NULL, css_buf, base_url, hld_profile, 1));
		if (parser)
		{
			parser->SetNextToken(CSS_TOK_DOM_SELECTOR);
			parser->SetDOMRule(dummy_rule, TRUE);
			CSS_PARSE_STATUS parse_stat = OpStatus::OK;
			TRAP(parse_stat, parser->ParseL());
			if (parse_stat == OpStatus::ERR_NO_MEMORY)
				stat = OpStatus::ERR_NO_MEMORY;
			else if (OpStatus::IsError(parse_stat) || dummy_rule->FirstSelector() == NULL)
			{
				exception = CSS_DOMEXCEPTION_SYNTAX_ERR;
				stat = OpStatus::ERR;
			}
		}
		else
			stat = OpStatus::ERR_NO_MEMORY;

		OP_DELETE(parser);
	}
	else
		stat = OpStatus::ERR_NO_MEMORY;

	OP_DELETE(css_buf);

	if (OpStatus::IsError(stat))
	{
		OP_DELETE(dummy_rule);
		return stat;
	}

	// Match using the dummy rule.

	HTML_Element* stop_elm = static_cast<HTML_Element*>(root->NextSibling());
	HTML_Element* cur_elm = root;

	unsigned short spec;

	CSS_MatchContext* match_context = hld_profile->GetCSSCollection()->GetMatchContext();

	CSS_MatchContext::Operation context_op(match_context, hld_profile->GetFramesDocument(), CSS_MatchContext::TOP_DOWN, CSS_MEDIA_TYPE_NONE, FALSE, FALSE);
	stat = match_context->InitTopDownRoot(cur_elm);

	while (cur_elm && stat == OpStatus::OK)
	{
		if (!(query_flags & CSS_QUERY_MATCHES_SELECTOR))
		{
			cur_elm = context_op.Next(cur_elm);

			if (cur_elm == stop_elm)
				break;
		}

		if (cur_elm && Markup::IsRealElement(cur_elm->Type()) && cur_elm->IsIncludedActual())
		{
			CSS_MatchContextElm* context_elm = match_context->GetLeafElm(cur_elm);
			if (!context_elm)
				stat = OpStatus::ERR_NO_MEMORY;
			else
			{
				BOOL match = dummy_rule->Match(match_context, context_elm, spec, spec);
				if (match)
					stat = listener->MatchElement(cur_elm);

				if (match && !(query_flags & CSS_QUERY_ALL) ||
					(query_flags & CSS_QUERY_MATCHES_SELECTOR))
					break;
			}
		}
	}

	// Clean up.
	OP_DELETE(dummy_rule);

	return stat;
}

#endif // STYLE_DOM_SELECTORS_API
