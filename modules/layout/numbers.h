/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _NUMBERS_H_
#define _NUMBERS_H_

/** Generate a roman numeral.

	@param number The number to represent as a roman numeral
	@param buf (out) Destination buffer
	@param buf_len Maximum number of characters to put in the buffer,
	NOT including termination NUL character.
	@param upper_case TRUE for upper case letters, FALSE for lower case letters

	@return Number of characters written to the destination buffer, not
	including terminating NUL character */

int MakeRomanStr(int number, uni_char* buf, int buf_len, BOOL upper_case);

/** Control values for
	@see MakeListNumberStr(). */
enum ListNumberStrMode
{
	LIST_NUMBER_STR_MODE_NO_CONTROL,
	LIST_NUMBER_STR_MODE_LTR,
	LIST_NUMBER_STR_MODE_RTL
};

/** Generate list-item marker number string. If SUPPORT_TEXT_DIRECTION
	is defined, the list marker string creates its own embedding level,
	unless the mode param is LIST_NUMBER_STR_MODE_NO_CONTROL.
	@see http://unicode.org/reports/tr9/#Explicit_Directional_Embedding

	@param number The number to represent as string
	@param list_type Value of CSS property list-style-type (enum CSSValue)
	@param add_str Optional string to add to the generated number string. May
	be NULL. Will be put after the number string.
	@param buf[out] Destination buffer. Where to put the number string and the
	optional string. Will always be NUL terminated.
	@param buf_len Maximum number of characters to put in the buffer, NOT
	including terminating NUL character.
	@param mode Controls the text direction embedding.
	Used if SUPPORT_TEXT_DIRECTION is defined. Otherwise behaves as
	LIST_NUMBER_STR_MODE_NO_CONTROL, which means that no directional embedding
	control characters will be put.
	LIST_NUMBER_STR_MODE_LTR - put LRE in front and end the string with PDF.
	LIST_NUMBER_STR_MODE_RTL - put RLE in front and end the string with PDF.

	@return Number of characters written to the destination buffer, not
	including terminating NUL character. */

int MakeListNumberStr(int number, short list_type, const uni_char* add_str, uni_char* buf, int buf_len, ListNumberStrMode mode = LIST_NUMBER_STR_MODE_NO_CONTROL);

#endif /* _NUMBERS_H_ */
