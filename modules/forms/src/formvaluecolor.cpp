/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/forms/formvaluecolor.h"

#include "modules/forms/piforms.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_lex.h"
#include "modules/logdoc/logdoc_util.h"


/* static */
OP_STATUS FormValueColor::ConstructFormValueColor(HTML_Element* he,
													FormValue*& out_value)
{
	OP_ASSERT(he->Type() == HE_INPUT);
	FormValueColor *color_value = OP_NEW(FormValueColor, ());
	if (!color_value)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	const uni_char* html_value = he->GetStringAttr(ATTR_VALUE);
	if (html_value)
	{
		OP_STATUS status = color_value->SetValueFromText(he, html_value);
		if (OpStatus::IsMemoryError(status))
		{
			OP_DELETE(color_value);
			return status;
		}
	}
	out_value = color_value;
	return OpStatus::OK;
}

/* virtual */
FormValue* FormValueColor::Clone(HTML_Element *he)
{
	FormValueColor* clone = OP_NEW(FormValueColor, ());
	if (clone)
	{
		if (he && IsValueExternally())
		{
			m_color = GetWidgetColor(he->GetFormObject());
		}

		clone->m_color = m_color;
		OP_ASSERT(!clone->IsValueExternally());
	}
	return clone;
}

/* virtual */
OP_STATUS FormValueColor::GetValueAsText(HTML_Element* he,
									 OpString& out_value) const
{
	COLORREF color = m_color;
	if (IsValueExternally())
	{
		color = GetWidgetColor(he->GetFormObject());
	}

	if (!out_value.Reserve(10))
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	HTM_Lex::GetRGBStringFromVal(color, out_value.CStr(), TRUE);
	return OpStatus::OK;
}

static COLORREF ParseSimpleColor(const uni_char* value)
{
	if (value)
	{
		while (IsHTML5Space(*value))
		{
			value++;
		}

		size_t len = uni_strlen(value);
		while (len > 0 && IsHTML5Space(value[len-1]))
		{
			len--;
		}

		BOOL valid = len == 7 && value[0] == '#';
		if (valid)
		{
			int r = HexToInt(value + 1, 2, TRUE);
			int g = HexToInt(value + 3, 2, TRUE);
			int b = HexToInt(value + 5, 2, TRUE);
			if (r != -1 && g != -1 && b != -1)
			{
				return OP_RGB(r, g, b);
			}
		}
	}

	// Fallback to #000000.
	return OP_RGB(0, 0, 0);
}

/* virtual */
OP_STATUS FormValueColor::SetValueFromText(HTML_Element* he,
											const uni_char* in_value)
{
	m_color = ParseSimpleColor(in_value);
	if (IsValueExternally())
	{
		SetWidgetColor(he->GetFormObject(), m_color);
	}
	return OpStatus::OK;
}

/* virtual */
OP_STATUS FormValueColor::ResetToDefault(HTML_Element* he)
{
	OP_ASSERT(he->GetFormValue() == this);

	ResetChangedFromOriginal();

	const uni_char* default_value = he->GetStringAttr(ATTR_VALUE);
	return SetValueFromText(he, default_value);
}

/* virtual */
BOOL FormValueColor::Externalize(FormObject* form_object_to_seed)
{
	if (!FormValue::Externalize(form_object_to_seed))
	{
		return FALSE; // It was wrong to call Externalize
	}
	OpStatus::Ignore(SetWidgetColor(form_object_to_seed, m_color));
	return TRUE;
}

/* virtual */
void FormValueColor::Unexternalize(FormObject* form_object_to_replace)
{
	if (IsValueExternally())
	{
		m_color = GetWidgetColor(form_object_to_replace);
		FormValue::Unexternalize(form_object_to_replace);
	}
}

/* static */
OP_STATUS FormValueColor::SetWidgetColor(FormObject* form_object,
										 COLORREF color)
{
	OP_ASSERT(OP_GET_A_VALUE(color) == 255 || !"Will generate the wrong string that might not fit the buffer");
	uni_char color_string[8]; // ARRAY OK bratell 2010-01-03.
	HTM_Lex::GetRGBStringFromVal(color, color_string, TRUE);
	return form_object->SetFormWidgetValue(color_string);
}

/* static */
COLORREF FormValueColor::GetWidgetColor(FormObject* form_object)
{
	OpString val_as_text;
	OP_STATUS status = form_object->GetFormWidgetValue(val_as_text);
	if (OpStatus::IsSuccess(status))
	{
		return ParseSimpleColor(val_as_text.CStr());
	}
	return OP_RGB(0, 0, 0);
}
