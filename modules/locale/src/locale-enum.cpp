/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/hash.h"

#ifdef LOCALE_MAP_OLD_ENUMS

static const struct old_locale_string_map_s
{
	int old_id, new_id;
} g_locale_string_map[] =
{
	#include "modules/locale/locale-map.inc"
};

static int find_old_key(const void *a, const void *b)
{
	return reinterpret_cast<const old_locale_string_map_s *>(a)->old_id -
	       reinterpret_cast<const old_locale_string_map_s *>(b)->old_id;
}

Str::LocaleString::LocaleString(int id)
{
	// Try mapping from old enums to the new ones
	old_locale_string_map_s needle;
	needle.old_id = id;

	old_locale_string_map_s *result =
		reinterpret_cast<old_locale_string_map_s *>
		(op_bsearch(&needle, g_locale_string_map, ARRAY_SIZE(g_locale_string_map),
		            sizeof(needle), find_old_key));
	if (result)
	{
		m_id = result->new_id;
	}
	else
	{
		m_id = id;
	}
}

#endif // LOCALE_MAP_OLD_ENUMS

#ifdef LOCALE_SET_FROM_STRING
// The hash value is interpreted as 32-bit signed int
Str::LocaleString::LocaleString(const uni_char *s) : m_id(static_cast<INT32>(djb2hash(s))) {}
Str::LocaleString::LocaleString(const char *s) : m_id(static_cast<INT32>(djb2hash(s))) {}
#endif // LOCALE_SET_FROM_STRING
