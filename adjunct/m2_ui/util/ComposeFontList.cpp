/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/m2_ui/util/ComposeFontList.h"
#include "modules/util/OpHashTable.h"

/***********************************************************************************
**
**	AddFont
**
***********************************************************************************/

OP_STATUS ComposeFontList::AddFont(const uni_char * font_name, INT32& index)
{
	FontDropdownInfo *font_info = OP_NEW(FontDropdownInfo,());
	if (!font_info)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(font_info->font_name.Set(font_name)))
	{
		OP_DELETE(font_info);
		return OpStatus::ERR;
	}
	font_info->index = index;

	return m_font_list.Add(font_info->font_name.CStr(), font_info);
}

/***********************************************************************************
**
**	GetFontIndex
**
***********************************************************************************/

INT32 ComposeFontList::GetFontIndex(const uni_char * font_name) const
{
	FontDropdownInfo * font_info;
	if (OpStatus::IsSuccess(m_font_list.GetData(font_name, &font_info)))
		return font_info->index;
	else
	{
		// maybe it's a list of fonts like "Helvetica,Arial,Roman, "Times New Roman", sans-serif"
		// attempt to iterate through that list and select the first one
		OpString font;
		while ((font_name = StringUtils::SplitWord(font_name, ',', font)))
		{
			OpString separated_font;
			if (OpStatus::IsSuccess(StripAndUnquote(font.CStr(), separated_font)) &&
				OpStatus::IsSuccess(m_font_list.GetData(separated_font.CStr(), &font_info)))
				return font_info->index;
		}
	}
	return -1;
}

/***********************************************************************************
**
**	StripAndUnquote
**
***********************************************************************************/

OP_STATUS ComposeFontList::StripAndUnquote(const uni_char * input, OpString& string_output) const
{
	OpString string_input;
	RETURN_IF_ERROR(string_input.Set(input));

	// remove any whitespace from it
	string_input.Strip();

	// remove leading and trailing ' or "
	if ((string_input.CStr()[0] == '\'' || string_input.CStr()[0] == '\"') &&
		(string_input.CStr()[string_input.Length()-1] == '\'' || string_input.CStr()[string_input.Length()-1] == '\"'))
		RETURN_IF_ERROR(string_output.Set(string_input.SubString(1).CStr(), string_input.Length()-2));
	else
		RETURN_IF_ERROR(string_output.Set(string_input.CStr()));
	
	return OpStatus::OK;
}
#endif //M2_SUPPORT
