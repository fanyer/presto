/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SVG_SUPPORT

#include "modules/svg/src/SVGInternalEnum.h"
#include "modules/svg/src/SVGInternalEnumTables.h"
#include "modules/svg/src/SVGManagerImpl.h"

/* static */ const char*
SVGEnumUtils::GetEnumName(SVGEnumType type, int enum_val)
{
	OP_NEW_DBG("SVGEnumUtils::GetEnumValue", "svg_enum");

	if (type < 0 || type >= SVGENUM_TYPE_COUNT)
	{
		OP_DBG(("Request for unknown type (dec: %d)", type));
		return NULL;
	}

	int start_idx = svg_enum_lookup[type];

	if (start_idx < 0 || start_idx > SVG_ENUM_ENTRIES_COUNT)
	{
		OP_DBG(("Start index out of range (dec: %d) for %s", start_idx, g_svg_enum_type_strings[type]));
		return NULL;
	}

	int end_idx = (type == (SVGENUM_TYPE_COUNT - 1)) ? SVG_ENUM_ENTRIES_COUNT : svg_enum_lookup[type+1];

	if (end_idx < 0 || end_idx > SVG_ENUM_ENTRIES_COUNT)
	{
		OP_DBG(("End index out of range (dec: %d) for %s", start_idx, g_svg_enum_type_strings[type]));
		return NULL;
	}

	int i=start_idx;
	while(i<end_idx && g_svg_enum_entries[i].val != enum_val)
		++i;

	if (i<end_idx)
		return g_svg_enum_entries[i].str;
	else
	{
		OP_DBG(("Failed to convert enum (%s,%s) to enumeration value",
				g_svg_enum_type_strings[type],
				g_svg_enum_name_strings[enum_val]));
		return NULL;
	}
}

/* static */ int
SVGEnumUtils::GetEnumValue(SVGEnumType type, const uni_char* str, unsigned str_len)
{
	OP_NEW_DBG("SVGEnumUtils::GetEnumValue", "svg_enum");
	if (type < 0 || type >= SVGENUM_TYPE_COUNT)
	{
		OP_DBG(("Request for unknown type (dec: %d)", type));
		return -1;
	}

	int start_idx = svg_enum_lookup[type];

	if (start_idx < 0 || start_idx > SVG_ENUM_ENTRIES_COUNT)
	{
		OP_DBG(("Start index out of range (dec: %d) for %s", start_idx, g_svg_enum_type_strings[type]));
		return -1;
	}

	int end_idx = (type == (SVGENUM_TYPE_COUNT - 1)) ? SVG_ENUM_ENTRIES_COUNT : svg_enum_lookup[type+1];

	if (end_idx < 0 || end_idx > SVG_ENUM_ENTRIES_COUNT)
	{
		OP_DBG(("End index out of range (dec: %d) for %s", end_idx, g_svg_enum_type_strings[type]));
		return -1;
	}

	int i=start_idx;
	for (; i<end_idx; ++i)
	{
		if (str_len == g_svg_enum_entries[i].str_len &&
			(uni_strncmp(str, g_svg_enum_entries[i].str, str_len) == 0))
			break;
	}

	if (i<end_idx)
	{
#ifdef _DEBUG
		OpString8 char_text;
		if (str_len > 0 && OpStatus::IsSuccess(char_text.Set(str, str_len)))
		{
			OP_DBG(("Translating %s to %s (%s) in interval (%s-%s)",
					char_text.CStr(),
					g_svg_enum_name_strings[i],
					g_svg_enum_type_strings[type],
					g_svg_enum_name_strings[start_idx],
					g_svg_enum_name_strings[end_idx-1]));
		}
#endif // _DEBUG
		return g_svg_enum_entries[i].val;
	}
	else
	{
#ifdef _DEBUG
		OpString8 char_text;
		if (str_len > 0 && OpStatus::IsSuccess(char_text.Set(str, str_len)))
		{
			OP_DBG(("Failed translating %s (%s) in interval (%s-%s)",
					char_text.CStr(),
					g_svg_enum_type_strings[type],
					g_svg_enum_name_strings[start_idx],
					g_svg_enum_name_strings[end_idx-1]));
		}
#endif // _DEBUG
		return -1;
	}
}

#endif // SVG_SUPPORT
