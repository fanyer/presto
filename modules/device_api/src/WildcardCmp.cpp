/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "modules/util/opautoptr.h"
#include "modules/device_api/WildcardCmp.h"

static const int WILDCARD_CMP_STACK_BUFFER_SIZE = 1024;

/**
 * Splits pattern buffer into subpatterns in place. Example:
 * "*Some * cool \* pattern*" = > "","Some "," cool * pattern",""
 * @return number of subpatterns
 */
static
unsigned int UnescapeAndSplitSubPatterns(uni_char* buf, int buffer_length, uni_char wildcard_char, uni_char escape_char)
{
	const uni_char* current_input_char = buf;
	uni_char* current_output_char = buf;
	const uni_char* end_of_buf = buf + buffer_length;

	BOOL escape_next_char = FALSE;
	unsigned int subpatterns_num = 1;
	for (; current_input_char < end_of_buf; ++current_input_char)
	{
		if (escape_next_char)
		{
			escape_next_char = FALSE;
			*current_output_char = *current_input_char;
			++current_output_char;
			continue;
		}

		if (*current_input_char == escape_char)
		{
			escape_next_char = TRUE;
			continue;
		}

		if (*current_input_char == wildcard_char)
		{
			*current_output_char = 0;
			++current_output_char;
			++subpatterns_num;
			continue;
		}

		*current_output_char = *current_input_char;
		++current_output_char;

	}
	if (current_output_char < current_input_char)	// we had at least one escaped char
		*current_output_char = 0;					// so we need to add a terminating zero
	return subpatterns_num;
}

/**
 * Gets the next subpattern.
 */
static const uni_char*
GetNextSubPattern(const uni_char* pattern)
{
	return pattern + uni_strlen(pattern) + 1;
}

OP_BOOLEAN
WildcardCmp(const uni_char* pattern, const uni_char* string, BOOL case_sensitive, uni_char escape, uni_char wildcard)
{
	OP_ASSERT(pattern);
	OP_ASSERT(string);

	int pattern_length = static_cast<int>(uni_strlen(pattern));
	uni_char* pattern_buf;
	uni_char stack_pattern_buf[WILDCARD_CMP_STACK_BUFFER_SIZE]; /* ARRAY OK 2010-05-11 msimonides */
	OpAutoArray<uni_char> heap_buffer_deleter;
	if (pattern_length - 1 > WILDCARD_CMP_STACK_BUFFER_SIZE)
	{
		pattern_buf = UniSetNewStr(pattern);
		if (!pattern_buf)
			return OpStatus::ERR_NO_MEMORY;
		heap_buffer_deleter.reset(pattern_buf);
	}
	else
	{
		uni_strcpy(stack_pattern_buf, pattern);
		pattern_buf = stack_pattern_buf;
	}
#define STRING_COMPARE(arg1, arg2, size) (case_sensitive ? uni_strncmp(arg1, arg2, size) : uni_strnicmp(arg1, arg2, size))
#define SUBSTRING_FIND(haystack, needle) (case_sensitive ? uni_strstr(haystack, needle) : uni_stristr(haystack, needle))
	size_t num_subpatterns = UnescapeAndSplitSubPatterns(pattern_buf, pattern_length, wildcard, escape);

	size_t i;
	const uni_char* current_subpattern = pattern_buf;
	const uni_char* current_string_part = string;
	for (i = 0; i < num_subpatterns; ++i, current_subpattern = GetNextSubPattern(current_subpattern))
	{
		size_t subpattern_length = uni_strlen(current_subpattern);
		if (i == 0 || i == num_subpatterns - 1)
		{
			if (i == 0 && i == num_subpatterns - 1) // we have only one subpattern, so it must match the whole string
			{
				if (STRING_COMPARE(current_subpattern, current_string_part, subpattern_length) != 0 || uni_strlen(current_string_part) != subpattern_length)
					return OpBoolean::IS_FALSE;
			}
			else if (i == 0)// match at the beginning
			{
				if (STRING_COMPARE(current_subpattern, current_string_part, subpattern_length) != 0)
					return OpBoolean::IS_FALSE; // beginning of the string doesnt match
			}
			else if (i == num_subpatterns - 1)// match at the end
			{
				size_t remaining_string_length = uni_strlen(current_string_part);
				if (remaining_string_length < subpattern_length)
					return OpBoolean::IS_FALSE; // not enough chars left to match ending
				if (STRING_COMPARE(current_subpattern, current_string_part + remaining_string_length - subpattern_length, subpattern_length) != 0)
					return OpBoolean::IS_FALSE; // ending of the string doesnt match
			}
			current_string_part += subpattern_length;
		}
		else
		{
			const uni_char * subpattern_found = SUBSTRING_FIND(current_string_part, current_subpattern);
			if (!subpattern_found)
				return OpBoolean::IS_FALSE; // Subpattern not found
			current_string_part = subpattern_found + subpattern_length;
		}

	}
	return OpBoolean::IS_TRUE;

#undef STRING_COMPARE
#undef SUBSTRING_FIND
}

#ifdef DAPI_JIL_SUPPORT
OP_BOOLEAN
JILWildcardCmp(const uni_char* pattern, const uni_char* string)
{
	OP_ASSERT(pattern);

	// "*" needs to match even NULL. Otherwise NULL isn't matched by anything else.
	if (string == NULL)
		return ((pattern[0] == '*') && (pattern[1] == '\0')) ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
	return WildcardCmp(pattern, string, FALSE, UNI_L("\\")[0], UNI_L("*")[0]);
}
#endif // DAPI_JIL_SUPPORT

