/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; coding: iso-8859-1 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_STRING_SET_H
#define MODULES_UTIL_STRING_SET_H

/**
 * A simple string set class that splits up a string on whitespace. I.e. each entry
 * in the set will be a "word". Entries in the resulting stringset are unique. 
 * The set also has basic set operations to produce new sets.
 * If an OOM occur during any of the operations the resulting set will be set to the
 * empty set.
 */
class OpStringSet
{
public:
	OpStringSet();
	~OpStringSet();

	/**
	 * Initialize the set with a string.
	 *
	 * Entries in the resulting set are unique. String contents is copied.
	 * If an OOM event occurs the size of the set will be set to zero but
	 * some entries may still reside in the array.
	 * 
	 * @param str String of white space delimited "words".
	 */
	void InitL(const OpStringC &str);

	/**
	 * Assign the relative complement of 'b' in 'a' to this set (a - b).
	 * 
	 * Unless the resulting set has the same number of elements as this set a new
	 * array of OpString objects will be allocated. If an OOM event occurs the
	 * size of the resulting set will be set to zero.
	 *
	 * @param a	The starting set
	 * @param b	The set used for comparison
	 * @returns	a pointer to this set
	 */
	OpStringSet &RelativeComplementL(const OpStringSet &a, const OpStringSet &b);

	/* Get the size of the set in number of valid entries. */
	int Size() const { return numStrings; }

	/* Get the Nth element of the set (no order implied) */
	const OpString &GetString(int index) const { OP_ASSERT(0 <= index && index < numStrings); return strings[index]; }

private:
	/**
	 * Helper function that checks if the word "elem" exists earlier in the string
	 * starting at "start".
	 */
	BOOL IsDupeWord(const uni_char *start, const uni_char *elem) const;

	OpString	*strings;	///< Array containing elements of the set
	int			numStrings;	///< Number of valid elements in the array
};

#endif //MODULES_UTIL_STRING_SET_H
