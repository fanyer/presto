/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "PasswordStrength.h"
#include "commonpasswords.inc"

#include "modules/util/adt/opvector.h"

#define BETWEEN(score, l, r) (score >= l && score < r)

PasswordStrength::Level PasswordStrength::Check(const OpStringC& passwd, int min_length)
{
	if (passwd.Length() < min_length)
		return TOO_SHORT;

	for (unsigned i = 0; i < ARRAY_SIZE(common_passwords); ++i)
		if (passwd.Compare(common_passwords[i]) == 0)
			return VERY_WEAK;

	int lower = 0, upper = 0, digit = 0, special = 0;
	CharacterCount(passwd, lower, upper, digit, special);

	int unique_lower = 0, unique_upper = 0, unique_digit = 0, unique_special = 0;
	UniqueCharacterCount(passwd, unique_lower, unique_upper, unique_digit, unique_special);

	int parts_lower = 0, parts_upper = 0, parts_digit = 0, parts_special = 0;
	CharacterClassesPartsCount(passwd, parts_lower, parts_upper, parts_digit, parts_special);

	// Points for different classes of characters.

	int score = unique_lower * LOWER_W;
	score += unique_upper * UPPER_W;
	score += unique_digit * DIGIT_W;
	score += unique_special * SPECIAL_W;

	// Points for mixing different classes of characters.

	if (parts_lower > 1) 
		score += (parts_lower - 1) * LOWER_MIXED_W;
	if (parts_upper > 1) 
		score += (parts_upper - 1) * UPPER_MIXED_W;
	if (parts_digit > 1) 
		score += (parts_lower - 1) * DIGIT_MIXED_W;
	if (parts_special > 1) 
		score += (parts_special - 1) * SPECIAL_MIXED_W;

	// If there are at least 5 unique characters in the password we
	// also give points for length.

	if (unique_lower + unique_upper + unique_digit + unique_special > 5)
		score += passwd.Length() * LENGTH_W;

	// Normalize the score to [0, 100] range.

	score = MIN(score, 100);

	if (BETWEEN(score, 0, 15)) 
		return VERY_WEAK;
	else if (BETWEEN(score, 15, 30))
		return WEAK;
	else if (BETWEEN(score, 30, 60))
		return MEDIUM;
	else if (BETWEEN(score, 60, 90))
		return STRONG;
	else if (BETWEEN(score, 90, 101))
		return VERY_STRONG;
	
	return NONE;
}

void PasswordStrength::CharacterClassesPartsCount(const OpStringC& str,
		int& lower, int& upper, int& digit, int& special)
{
	lower = upper = digit = special = 0;
	BOOL lower_start = FALSE, upper_start = FALSE, digit_start = FALSE, special_start = FALSE;

	for (int i = 0; i < str.Length(); ++i)
	{
		if (uni_islower(str[i]))
		{
			lower_start = TRUE;
			if (i == str.Length() - 1)
				lower++;
		}
		else if (lower_start)
		{
			lower_start = FALSE;
			lower++;
		}

		if (uni_isupper(str[i]))
		{
			upper_start = TRUE;
			if (i == str.Length() - 1)
				upper++;
		}
		else if (upper_start)
		{
			upper_start = FALSE;
			upper++;
		}

		if (uni_isdigit(str[i]))
		{
			digit_start = TRUE;
			if (i == str.Length() - 1)
				digit++;
		}
		else if (digit_start)
		{
			digit_start = FALSE;
			digit++;
		}

		if (!uni_isalnum(str[i]))
		{
			special_start = TRUE;
			if (i == str.Length() - 1)
				special++;
		}
		else if (special_start)
		{
			special_start = FALSE;
			special++;
		}
	}
}

void PasswordStrength::CharacterCount(const OpStringC& str, 
		int& lower, int& upper, int& digit, int& special)
{
	lower = upper = digit = special = 0;

	for (int i = 0; i < str.Length(); ++i)
	{
		if (uni_isupper(str[i]))
			upper++;
		else if (uni_islower(str[i]))
			lower++;
		else if (uni_isdigit(str[i]))
			digit++;
		else if (!uni_isalnum(str[i]))
			special++;
	}
}

void PasswordStrength::UniqueCharacterCount(const OpStringC& str, 
		int& lower, int& upper, int& digit, int& special)
{
	lower = upper = digit = special = 0;
	OpINT32Vector unique_lower, unique_upper, unique_digit, unique_special;

	for (int i = 0; i < str.Length(); ++i)
	{
		if (uni_islower(str[i]) && unique_lower.Find(str[i]) < 0)
			unique_lower.Add(str[i]);
		else if (uni_isupper(str[i]) && unique_upper.Find(str[i]) < 0)
			unique_upper.Add(str[i]);
		else if (uni_isdigit(str[i]) && unique_digit.Find(str[i]) < 0)
			unique_digit.Add(str[i]);
		else if (!uni_isalnum(str[i]) && unique_special.Find(str[i]) < 0)
			unique_special.Add(str[i]);
	}

	lower = unique_lower.GetCount();
	upper = unique_upper.GetCount();
	digit = unique_digit.GetCount();
	special = unique_special.GetCount();
}
