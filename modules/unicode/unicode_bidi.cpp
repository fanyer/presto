/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_TEXT_DIRECTION
#include "modules/unicode/unicode.h"
#include "modules/unicode/tables/bidiclass.inl"
#include "modules/util/handy.h"

BidiCategory Unicode::GetBidiCategory(UnicodePoint c)
{
	int plane = GetUnicodePlane(c);

	// Invalid|empty plane
	if (plane > 0xE || bidi_planes[plane][0] == -1)
		return BIDI_UNDEFINED; // BIDI_ON??

	uni_char cp = static_cast<uni_char>(c);

	// Sentinel
	if (cp == 0xffff)
		return BIDI_ON;
	
	// Check cache
	int last_block = g_opera->unicode_module.m_last_bidi_block;
	int last_plane = g_opera->unicode_module.m_last_bidi_plane;
	if (plane == last_plane && bidi_chars[last_block] <= cp && bidi_chars[last_block + 1] > cp)
		return static_cast<BidiCategory>(bidi_data[last_block]);
	
	// Binary search for the code point in the data table
	// BIDI_LAST_255 contains the last index for the characters
	// up to U+00FF to speed up search for Basic Latin and Latin-1
	// characters.
	size_t high = (c < 256 ? BIDI_LAST_255 : bidi_planes[plane][1]);
	size_t low = bidi_planes[plane][0];

	while (1)
	{
		size_t middle = (high + low) >> 1;
		if (bidi_chars[middle] <= cp)
		{
			if (bidi_chars[middle + 1] > cp)
			{
				// Codepoint range found, return the category value
				g_opera->unicode_module.m_last_bidi_block = middle;
				g_opera->unicode_module.m_last_bidi_plane = plane;
				return static_cast<BidiCategory>(bidi_data[middle]);
			}
			else
			{
				low = middle;
			}
		}
		else
		{
			high = middle;
		}
	}
	/* NOT REACHED */
}

#include "modules/unicode/tables/mirror.inl"
BOOL Unicode::IsMirrored(UnicodePoint c)
{
	return GetMirrorChar(c) != 0;
}

UnicodePoint Unicode::GetMirrorChar(UnicodePoint c)
{
	uni_char res = 0;

	if (c > 0xff63)
		return 0;

	// Check for cached lookup first, since we call GetMirrorChar()
	// also for IsMirrored().
	uni_char *last_mirror = g_opera->unicode_module.m_last_mirror;
	if (c == last_mirror[0])
	{
		return last_mirror[1];
	}
	if (c == last_mirror[1])
	{
		return last_mirror[0];
	}
	last_mirror[0] = c;

	size_t high = (c < 256 ? MIRROR_LAST_255 : ARRAY_SIZE(mirror_table)), low = 0, middle;

	// First try to find the code point in the sorted mapping table.
	// This table contains mirrored characters sorted by code point.
	while (1)
	{
		middle = (high + low) >> 1;
		if (mirror_table[middle] == c)
		{
			if (middle & 1)
				res = mirror_table[middle - 1];
			else
				res = mirror_table[middle + 1];
			break;
		}
		if (mirror_table[middle] < c)
		{
			if (low == middle)
				break;
			low = middle;
		}
		else
		{
			if (high == middle)
				break;
			high = middle;
		}
	}

	// If that failed, and the code point we are looking for is in the
	// range where mappings are overlapping, we linear-search through the
	// second table.
	if (c >= MIRROR_OTHER_MIN && c <= MIRROR_OTHER_MAX)
	{
		for (unsigned int i = 0; i < ARRAY_SIZE(mirror_table_other); i++)
		{
			if (c == mirror_table_other[i])
			{
				if (i & 1)
					res = mirror_table_other[i - 1];
				else
					res = mirror_table_other[i + 1];
				break;
			}
		}
	}

	// Cache result
	last_mirror[1] = res;
	return res;
}

#endif
