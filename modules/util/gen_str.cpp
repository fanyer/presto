/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/str.h"
#include "modules/util/handy.h"

#include "modules/util/gen_str.h"
#include "modules/util/gen_math.h"

void StrFilterOutChars(uni_char *target, const uni_char* charsToRemove)
{
	if( target && *target && charsToRemove && *charsToRemove)
	{
		int i = 0;
		int j = 0;

		while( target[i])
		{
			if( !uni_strchr( charsToRemove, target[i]))
				target[j++] = target[i++];
			else
				i++;
		}

		target[j] = 0;
	}
}

void StrFilterOutChars(char *target, const char* charsToRemove)
{
	if( target && *target && charsToRemove && *charsToRemove)
	{
		int i = 0;
		int j = 0;

		while( target[i])
		{
			if( !op_strchr( charsToRemove, (BYTE) target[i]))
				target[j++] = target[i++];
			else
				i++;
		}

		target[j] = 0;
	}
}

const char* StrFileExt(const char* fName)
{
	if( !fName)
		return NULL;

	char *ext = (char*) op_strrchr( fName, '.');
	if(	!ext					// '.' not found
		||	(ext==fName)		// A filename must be preceding the '.'
		|| !*(ext+1)			// An extension must follow the '.'
		|| (*(ext-1) == '.'))	// Not '..'
		return NULL;

	return ext+1;
}

const uni_char* StrFileExt(const uni_char* fName)
{
	if( !fName)
		return NULL;

	uni_char *ext = (uni_char*) uni_strrchr( fName, '.');
	if(	!ext					// '.' not found
		||	(ext==fName)		// A filename must be preceding the '.'
		|| !*(ext+1)			// An extension must follow the '.'
		|| (*(ext-1) == '.'))	// Not '..'
		return NULL;

	return ext+1;
}

int GetStrTokens(uni_char *str, const uni_char *tokenSep, const uni_char *stripChars,
				 uni_char *tokens[], UINT maxTokens, BOOL strip_trailing_tokens)
{
	if (0 == maxTokens || NULL == str)
		return 0;

	// Zero out tokens
	for (size_t i = 0; i < maxTokens; i++)
		tokens[i] = NULL;

	UINT tokensFound = 0;
	tokens[tokensFound++] = str; // First token is always on the beginning

	// Find subsequent separators and add tokens behind them
	uni_char *strEnd = str + uni_strlen(str);
	while (tokensFound <= maxTokens)
	{
		size_t nextSeparator = uni_strcspn(str, tokenSep);

		/*	Normally, change the found separator to \0 to terminate
			the previous token. Don't do this on the last token,
			unless we were asked to strip trailing tokens.*/
		if (tokensFound < maxTokens || strip_trailing_tokens)
			str[nextSeparator] = '\0';
		if (tokensFound == maxTokens || strEnd <= str + nextSeparator)
			break;

		str += nextSeparator;
		tokens[tokensFound++] = ++str; // Token starts right after separator;
	}

	// Strip trailing and leading chars
	for (UINT i = 0; i < tokensFound; i++)
	{
		uni_char *content = tokens[i];

		// Keep null-terminating from the back while the last char is in trimChars
		for (uni_char *end = tokens[i] + uni_strlen(content) ;
			end > content && uni_strchr(stripChars, *end) ;
			--end)
		{
			*end = '\0';
		}

		// Skip leading chars
		tokens[i] = content + uni_strspn(content, stripChars);
	}

	return tokensFound;
}
