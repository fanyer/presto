/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef SHORTCUT_LETTER_FINDER
#define SHORTCUT_LETTER_FINDER

#include "modules/util/simset.h"

/// Simple class to find the underlined characters for use as shortcuts in menus.

class MenuShortcutLetterFinder
{
public:
	MenuShortcutLetterFinder();
	~MenuShortcutLetterFinder();

	void Clear();

	/** Looks in str for & and returns the following character. If no & is found,
		it takes the first character which not yet has been used and inserts a & there.
		new_string is the resultstring (With the & added)
	*/
	uni_char CreateMenuString(const uni_char* str, OpString& new_string, const uni_char* default_language_str = NULL);
	
	BOOL IsLetterUsed(uni_char c);

private:
	INT32 GetShortcutLetter(const uni_char* str, INT32* ignored_hardcoded_pos);

	// get the first letter which could be used as a shortcut key
	INT32 GetFirstValidShortcutLetter(const uni_char* str);

	Head used_letters;
	OpString m_valid_letters;
};

#endif // SHORTCUT_LETTER_FINDER
