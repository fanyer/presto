/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2002 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "MenuShortcutLetterFinder.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#include <ctype.h> // toupper

class UsedLetter : public Link
{
public:
	UsedLetter(uni_char letter) : c(letter) {}
	uni_char c;
};

// == MenuShortcutLetterFinder ==========================================

MenuShortcutLetterFinder::MenuShortcutLetterFinder()
{
	g_languageManager->GetString(Str::S_VALID_SHORTCUT_LETTERS, m_valid_letters);
	if(!m_valid_letters.HasContent())
		m_valid_letters.Append(UNI_L("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"));
	m_valid_letters.Append(UNI_L("1234567890"));
}

MenuShortcutLetterFinder::~MenuShortcutLetterFinder()
{
	Clear();
}

void MenuShortcutLetterFinder::Clear()
{
	while(used_letters.First())
	{
		UsedLetter* letter = (UsedLetter*) used_letters.First();
		letter->Out();
		OP_DELETE(letter);
	}
}

// get the first letter which could be used as a shortcut key
INT32 MenuShortcutLetterFinder::GetFirstValidShortcutLetter(const uni_char* str)
{
	INT32 i, len = uni_strlen(str);

	for(i = 0; i < len ; i++)
	{
		if (m_valid_letters.FindFirstOf(str[i]) != KNotFound)
		{
			return i;
		}
	}
	return -1;
}


INT32 MenuShortcutLetterFinder::GetShortcutLetter(const uni_char* str, INT32* ignored_hardcoded_pos)
{
	if (str == NULL)
		return -1;

	INT32 i, len = uni_strlen(str);

	// Check for hardcoded &
	for(i = 0; i < len - 1; i++)
	{
		if (str[i] == '&')
		{
			if (str[i+1] == '&')
			{
				i++; // the & was escaped
				continue;
			}

			if (!IsLetterUsed(str[i+1]))
			{
				UsedLetter* letter = OP_NEW(UsedLetter, (str[i + 1]));
				if (letter)
					letter->Into(&used_letters);
#ifndef ALLOW_MENU_SHARE_SHORTCUT
				return i + 1;
#endif
			}
#ifdef ALLOW_MENU_SHARE_SHORTCUT
			// use hard coded shortcut key no matter whether it has been used if shared shortcut key is allowed.
			return i+1;
#endif

#ifndef ALLOW_MENU_SHARE_SHORTCUT
			if (ignored_hardcoded_pos)
				*ignored_hardcoded_pos = i;
#endif

		}
	}

	// Select the first unused character
	for(i = 0; i < len; i++)
	{
		if ( m_valid_letters.FindFirstOf(str[i]) != KNotFound   && !IsLetterUsed(str[i]))
		{
			UsedLetter* letter = OP_NEW(UsedLetter, (str[i]));
			if (letter)
				letter->Into(&used_letters);
			return i;
		}
	}
	return -1;
}

uni_char MenuShortcutLetterFinder::CreateMenuString(const uni_char* str, OpString& new_string, const uni_char* default_language_str)
{
	if (str == NULL)
		return 0;

	INT32 ignored_hardcoded_pos = -1;

	INT32 letter_pos = GetShortcutLetter(str, &ignored_hardcoded_pos);
	uni_char letter = 0;
	BOOL append = FALSE; // whether the shortcut key is appended as "(X)"

	if(letter_pos != -1)
		letter = uni_toupper(str[letter_pos]);

#ifdef ALLOW_MENU_SHARE_SHORTCUT
	// Allow multiple menu items to share a common shortcut key
	// presing the key will switch highlight among those items.
	// platforms define this Macro when they get able to handle this

	// Try to get a shortcut letter the menu string contains
	if(letter_pos == -1)
	{
		letter_pos = GetFirstValidShortcutLetter(str);
		if( letter_pos != -1)
		{
			letter = uni_toupper(str[letter_pos]);
		} 
	}
#endif 
	
	// Try to get the English letter 
	if (letter_pos == -1 && default_language_str)
	{
		letter_pos = GetShortcutLetter(default_language_str, 0);
		if (letter_pos != -1)
		{
			letter = uni_toupper(default_language_str[letter_pos]);
			append = TRUE;
		}
	}

	// Try to get any unused letter
	if (letter_pos == -1)
	{
		letter_pos = GetShortcutLetter(m_valid_letters.CStr(), 0);
		if (letter_pos != -1)
		{
			letter = m_valid_letters[letter_pos];
			append = TRUE;
		}
	}

	if(letter_pos == -1)
	{
		new_string.Set(str);
		return 0;
	}

	// construct the menu string
	if(append)
	{
		new_string.Set(str);
		OpString access_key;

		access_key.Set(UNI_L("(&"));
		access_key.Append(&letter,1);
		access_key.Append(UNI_L(")"));
		
		INT32 insert_pos = new_string.Find(UNI_L("..."));
		if (insert_pos != KNotFound)
		{
			new_string.Insert(insert_pos, access_key);
		}
		else
		{
			new_string.Append(access_key);
		}

		if (ignored_hardcoded_pos != -1)
		{
			new_string.Delete(ignored_hardcoded_pos,1);
		}

	}
	else if (letter_pos != 0 && str[letter_pos-1] == '&')
	{
		new_string.Append(str);	
	}
	else
	{
		new_string.Append(str, letter_pos);
		new_string.Append(UNI_L("&"));
		new_string.Append(&str[letter_pos]);

		if (ignored_hardcoded_pos != -1)
		{
			if (ignored_hardcoded_pos>letter_pos)
				ignored_hardcoded_pos++;
			new_string.Delete(ignored_hardcoded_pos,1);
		}

	}

	return letter;
}

BOOL MenuShortcutLetterFinder::IsLetterUsed(uni_char c)
{
	UsedLetter* letter = (UsedLetter*) used_letters.First();
	while(letter)
	{
		if (uni_toupper(letter->c) == uni_toupper(c))
			return TRUE;
		letter = (UsedLetter*) letter->Suc();
	}
	return FALSE;
}
