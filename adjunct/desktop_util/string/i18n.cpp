/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */

#include "core/pch.h"

#include "adjunct/desktop_util/string/i18n.h"

#include "modules/locale/oplanguagemanager.h"

OP_STATUS I18n::Format(const FormatArgument* argument, OpString& formatted, const uni_char* format)
{
	const uni_char* src = format;
	while (*src)
	{
		if (*src == '%')
		{
			if (*(src+1) == '%')
			{
				// Append string with one of the two '%' signs, skip the second one
				++src;
				RETURN_IF_ERROR(formatted.Append(format, src - format));
				format = src + 1;
			}
			else if (*(src+1) >= '1' && *(src+1) <= '9')
			{
				// Found a specifier, append string up to the specifier
				RETURN_IF_ERROR(formatted.Append(format, src - format));
				++src;
				int order = *src - '0';

				// Find the specified argument
				const FormatArgument* currentarg = argument;
				for (int count = 1; count < order && currentarg; ++count)
					currentarg = currentarg->next;

				// we assert so that developers can see when the argument is not in the string
				OP_ASSERT(currentarg);
				// but we don't want to punish users of incorrect language files (since errors tend to bubble up)
				if (!currentarg)
					return OpStatus::OK;

				switch (currentarg->format)
				{
					case FORMAT_INT32:
						RETURN_IF_ERROR(formatted.AppendFormat(UNI_L("%d"), currentarg->argument_int));
						break;
					case FORMAT_UINT32:
						RETURN_IF_ERROR(formatted.AppendFormat(UNI_L("%u"), currentarg->argument_uint));
						break;
					case FORMAT_STRING:
						RETURN_IF_ERROR(formatted.Append(currentarg->argument_string));
						break;
					case FORMAT_STRING_ID:
					{
						OpString string;
						RETURN_IF_ERROR(g_languageManager->GetString(Str::LocaleString(currentarg->argument_string_id), string));
						RETURN_IF_ERROR(formatted.Append(string.CStr()));
						break;
					}
				}
				format = src + 1;
			}
			else
			{
				OP_ASSERT(!"Stray '%' character in string, did you mean '%%'?");
			}
		}
		++src;
	}

	// Append the remainder of the string
	return formatted.Append(format, src - format);
}

OP_STATUS I18n::Format(const FormatArgument* argument, OpString& formatted, Str::LocaleString format)
{
	OpString local_format;
	RETURN_IF_ERROR(g_languageManager->GetString(format, local_format));
	return Format(argument, formatted, local_format);
}
