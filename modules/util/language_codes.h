/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef UTIL_LANGUAGE_CODES_H
#define UTIL_LANGUAGE_CODES_H

class LanguageCodeUtils
{
public:

	/**
	 * Converts a string beginning with a (2 or 3 letter) language code to 3 letter (IS0 639-2) code.
	 *
	 * @param[out] three_letter_code Set to the ISO 639-2 code corresponding to the input value.
	 * @param[in] input Input.
	 */
	static OP_STATUS ConvertTo3LetterCode(OpString * three_letter_code, const uni_char * input);

	/**
	 * Converts a string beginning with a (2 or 3 letter) language code to 3 letter (IS0 639-2) code.
	 *
	 * @param[out] three_letter_code Set to the ISO 639-2 code corresponding to the input value.
	 * @param[in] input Input.
	 */
	static OP_STATUS ConvertTo3LetterCode(OpString8 * three_letter_code, const char * input);
private:
	static OP_STATUS Convert2To3LetterCode(OpString8 * three_letter_code, const char * two_letter_code);
};
#endif // UTIL_LANGUAGE_CODES_H
