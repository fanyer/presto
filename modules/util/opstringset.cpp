/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; coding: iso-8859-1 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/opstringset.h"


OpStringSet::OpStringSet()
: strings(NULL)
, numStrings(0)
{
}

OpStringSet::~OpStringSet()
{
	if (strings)
		OP_DELETEA(strings);
}

BOOL OpStringSet::IsDupeWord(const uni_char *start, const uni_char *elem) const
{
	const uni_char *elemEnd = elem;
	while (*elemEnd && !uni_isspace(*elemEnd))
		++elemEnd;
	BOOL isDupe = FALSE;

	// Check for a dupe earlier in the string.
	while (start != elem)
	{
		while (start != elem && uni_isspace(*start))
			++start;

		if (start != elem && uni_strncmp(elem, start, elemEnd - elem) == 0)
		{
			isDupe = TRUE;
			break;
		}

		while (start != elem && !uni_isspace(*start))
			++start;
	}  
	return isDupe;
}

// Initialize the set with a string of white space delimited words.
void OpStringSet::InitL(const OpStringC &str)
{
	int num = 0;

	if (str.IsEmpty()) return;

	const uni_char *wPtr = str.CStr();

	// Figure out how many whitespace separated strings there are.
	while (*wPtr != 0)
	{
		while (*wPtr != 0 && uni_isspace(*wPtr))
			++wPtr;

		if (*wPtr != 0)
		{
			// Only count words that aren't dupes.
			if (!IsDupeWord(str.CStr(), wPtr))
				++num;
		}

		while (*wPtr != 0 && !uni_isspace(*wPtr))
			++wPtr;
	}

	// Delete old storage.
	if (strings && numStrings < num)
	{
		OP_DELETEA(strings);
		strings = NULL;
	}

	// Set this just in case we leave, then at least this will be an empty set.
	numStrings = 0;

	if (!strings && num > 0)
		strings = OP_NEWA_L(OpString, num);

	if (strings || num == 0)
	{
		if (num > 0)
		{
			// Iterate and extract delimited strings.
			int c = 0;
			wPtr = str.CStr();
			while (*wPtr != 0)
			{
				// Skip past whitespace.
				while (*wPtr != 0 && uni_isspace(*wPtr))
					++wPtr;

				// Find the end of the string, or the first whitespace.
				const uni_char *wEndPtr = wPtr;
				while (*wEndPtr != 0 && !uni_isspace(*wEndPtr))
					++wEndPtr;

				// If we found a string that's not a dupe, copy it and increase the count.
				if (wPtr != wEndPtr && !IsDupeWord(str.CStr(), wPtr))
					strings[c++].SetL(wPtr, wEndPtr - wPtr);

				wPtr = wEndPtr;
			}	
		}
	}

	numStrings = num;
}

// Do the relative complement a - b and return the result in the passed in set.
OpStringSet &OpStringSet::RelativeComplementL(const OpStringSet &a, const OpStringSet &b)
{
	// Calculate the number of elements in the resulting set.
	int num = a.Size();
	for (int ai = 0; ai < a.Size(); ++ai)
		for (int bi = 0; bi < b.Size(); ++bi)
			if (a.GetString(ai) == b.GetString(bi))
			{
				--num;
				break;
			}

	// Don't reallocate if we have the correct amount of elements in array.
	if (strings && numStrings != num)
	{
		OP_DELETEA(strings);
		strings = NULL;
	}

	// Set this just in case we leave, then at least this will be an empty set.
	numStrings = 0;

	if (!strings && num > 0)
		strings = OP_NEWA_L(OpString, num);

	if (strings && num > 0)
	{
		// We're not doing anything fancy here. O(n2).
		int c = 0;
		for (int ai = 0; ai < a.Size(); ++ai)
		{
			BOOL found = FALSE;
			for (int bi = 0; bi < b.Size(); ++bi)
				if (a.GetString(ai) == b.GetString(bi))
				{
					found = TRUE;
					break;
				}
			if (!found)
				strings[c++].SetL(a.GetString(ai));
		}
	}

	numStrings = num;
	return *this;
}

