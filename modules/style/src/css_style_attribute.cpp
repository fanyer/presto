/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#include "modules/style/css_style_attribute.h"
#include "modules/style/css_property_list.h"
#include "modules/style/css.h"

OP_STATUS StyleAttribute::CreateCopy(ComplexAttr** copy_to)
{
	TRAPD(ret, CreateCopyL(copy_to));
	return ret;
}

void StyleAttribute::CreateCopyL(ComplexAttr** copy_to)
{
	CSS_property_list* new_props = m_prop_list->GetCopyL();
	OpStackAutoPtr<CSS_property_list> props_auto_ptr(new_props);

	uni_char* new_string = NULL;
	LEAVE_IF_ERROR(UniSetStr(new_string, m_original_string));
	ANCHOR_ARRAY(uni_char, new_string);

	*copy_to = OP_NEW_L(StyleAttribute, (new_string, new_props));

	ANCHOR_ARRAY_RELEASE(new_string);
	props_auto_ptr.release();
	new_props->Ref();
}

StyleAttribute::~StyleAttribute()
{
	OP_DELETEA(m_original_string);
	m_prop_list->Unref();
}

/* virtual */
OP_STATUS StyleAttribute::ToString(TempBuffer* buffer)
{
	if (m_original_string)
	{
		return buffer->Append(m_original_string);
	}

	TRAPD(status, m_prop_list->AppendPropertiesAsStringL(buffer));
	return status;
}

#ifdef SVG_SUPPORT
OP_STATUS StyleAttribute::ReparseStyleString(HLDocProfile* hld_profile, const URL& base_url)
{
	// If m_original_string is NULL, it means that the style object has been modified
	// and the original string is not the correct representation anyway.
	// The internal font handling in Opera needs to be fixed so that we can
	// store font names instead of font ids in the cases where fonts are not loaded.
	// Then, this method will be superfluous.
	if (m_original_string)
	{
		CSS_PARSE_STATUS ret_stat;
		CSS_property_list* new_css = CSS::LoadHtmlStyleAttr(m_original_string, uni_strlen(m_original_string), base_url, hld_profile, ret_stat);
		if (OpStatus::IsError(ret_stat))
		{
			if (ret_stat >= CSSParseStatus::SYNTAX_ERROR)
				return OpStatus::ERR;
			else
				return ret_stat;
		}
		else if (new_css)
		{
			m_prop_list->Unref();
			m_prop_list = new_css;
		}
	}
	return OpStatus::OK;
}
#endif // SVG_SUPPORT

