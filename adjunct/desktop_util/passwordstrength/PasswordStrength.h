/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef PASSWORD_STRENGTH_H
#define PASSWORD_STRENGTH_H

namespace PasswordStrength
{
	static const int LOWER_W = 1;
	static const int DIGIT_W = 4;
	static const int UPPER_W = 4;
	static const int SPECIAL_W = 8; 

	static const int LOWER_MIXED_W = 5;
	static const int DIGIT_MIXED_W = 20;
	static const int UPPER_MIXED_W = 20;
	static const int SPECIAL_MIXED_W = 40;

	static const float LENGTH_W = 1.7f;

	enum Level
	{
		NONE,
		TOO_SHORT,
		VERY_WEAK,
		WEAK,
		MEDIUM,
		STRONG,
		VERY_STRONG
	};

   /**
	* Since it is possible to create an account on our Opera servers 
	* through Opera (the client) we need a function that checks for
	* the user if password that he provides is strong enough.
	*
	* This function is *pure* heuristics!
	*
	* @param passwd Password (input from user).
	* @param min_length If length(passwd) < min_len tell the user that password is too short.
	* @return Password strength.
	*/
	Level Check(const OpStringC& passwd, int min_length);

	void UniqueCharacterCount(const OpStringC& str, 
			int& lower, int& upper, int& digit, int& special);
	void CharacterCount(const OpStringC& str, 
			int& lower, int& upper, int& digit, int& special);
	void CharacterClassesPartsCount(const OpStringC& str,
			int& lower, int& upper, int& digit, int& special);
}

#endif // !PASSWORD_STRENGTH
