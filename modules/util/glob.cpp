/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef OP_GLOB_SUPPORT

#include "modules/util/glob.h"

/* \author Lars T Hansen
 * \author Tord Akerbæk 
 */
BOOL OpGlob( const uni_char *str, const uni_char *pat, BOOL slash_is_special/*=TRUE*/, BOOL brackets_enabled/*=TRUE*/ )
{
	int complement=0;
	int match = 0;
	uni_char c;

	while (*pat != 0)
	{
		switch (c = *pat++)
		{
		case '*' :
			while ('*' == *pat)
				++pat;
			if (*pat == 0)
			{
				while (*str != 0 && (!slash_is_special || *str != '/'))
					str++;
				return *str == 0;
			}
			else if (!*str)
				return FALSE;

			do
			{
				if (OpGlob( str, pat, slash_is_special ))
					return TRUE;
				if (slash_is_special && *str == '/')
					return FALSE;
				++str;
			} while (*str != 0);
			return FALSE;
		case '?':
			if (slash_is_special && *str == '/' || *str == '\0')
				return FALSE;
			++str;
			break;
		case '\\':
			if (*pat == 0 || *pat != *str)
				return FALSE;
			++pat;
			++str;
			break;
		case '[':
			if (brackets_enabled)
			{
				complement = 0;
				if (*pat == 0)
					return FALSE;
				if (*pat == '!') 
				{
					complement = 1;
					++pat;
					if (*pat == 0)
						return FALSE;
				}
				do
				{
					match = 0;
					uni_char c1, c2;

					OP_ASSERT( *pat != 0 );

					c1 = *pat;
					++pat;
					if (*pat == 0 || (slash_is_special && *pat == '/'))
						return FALSE;
					if (*pat == '-')
					{
						++pat;
						c2 = *pat;
						if (c2 == 0)
							return FALSE;
						if (c2 == ']')
						{
							--pat;
							match = (c1 == *str);
						}
						else
						{
							++pat;
							match = (c1 <= *str && *str <= c2);
						}
					}
					else
						match = (c1 == *str);
					if ((!slash_is_special || *str != '/') && match)
						while (*pat && *pat != ']')
							++pat;
				} while (*pat && *pat != ']');
				if(match == complement)
					return FALSE;
				++str;
				if (*pat != ']')
					return FALSE;
				++pat;
				break;
			}
			// fallthrough if !brackets_enabled
		default:
			if (c != *str)
				return FALSE;
			str++;
			break;
		}
	}
	return *str == 0;
}

#endif // OP_GLOB_SUPPORT
