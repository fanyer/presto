/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#ifdef ACCESSIBILITY_EXTENSION_SUPPORT

#include "modules/accessibility/acc_utils.h"

void CopyStringClean(OpString& dest, const uni_char* src, int len)
{
	OP_ASSERT(src);

	int dest_len = dest.Length();
	BOOL term_space = (dest_len > 0) ? uni_isspace(dest.CStr()[dest_len - 1]) : FALSE;
	BOOL first = TRUE;
	BOOL lead_space = FALSE;
	uni_char c;
	if (len < 0)
		len = uni_strlen(src);

	dest.Reserve(dest_len + len + 1);
	while (len)
	{
		if (!first)
			--len;
		c = *src;
		while (len && uni_isspace(c)) {
			// skip whitespace
			c = *++src;
			--len;
			lead_space = TRUE;
		}
		if (len)
		{
			const uni_char* end = src + 1;
			c = *end;
			--len;
			while (len && !uni_isspace(c))
			{
				c = *++end;
				--len;
			}

			if (first && (term_space || !lead_space || !dest_len))
			{
				// Append word straight only if this is the very first word we append,
				// or if the source string did not start with whitespace.
				// or if the destination already had a trailing whitespace
				dest.Append(src, end-src);
			}
			else
			{
				// Add a space before the word.
				dest.Append(UNI_L(" "));
				dest.Append(src, end-src);
			}
			 // if len is positivie, end points at whitespace.
			 // No sense checking that again when looping.
			src = end + 1;
		}
		else
		{
			// The string ended in whitespace.
			dest.Append(UNI_L(" "));
		}
		first = FALSE;
	}
}

#endif // ACCESSIBILITY_EXTENSION_SUPPORT
