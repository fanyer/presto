/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_GEN_STR_H
#define MODULES_UTIL_GEN_STR_H

/** Remove all characters in @a target found in @a charsToRemove.
 *
 * @param target   String to modify by removing characters
 * @param charsToRemove   Set of characters to remove
 */
void StrFilterOutChars(uni_char *target, const uni_char* charsToRemove);

/**
 * @overload
 */
void StrFilterOutChars(char *target, const char* charsToRemove);

/** Parse a string into an array of tokens.
 *
 * Splits a string (which is modified) on specified separators and optionally
 * strips unwanted characters from the start and end of each token thus
 * delimited. Stores a pointer to the starts of up to @a maxTokens stripped
 * tokens in the supplied array. When @a strip_trailing_tokens is true, any
 * further tokens are ignored. When it is false, separators after the start
 * of the last token stored are ignored (treated as normal characters, not
 * separators); the tail of the text belongs to this last token, which is
 * stripped in the usual way. (If a separator character also appears as a
 * strip character, this is the one case in which it may be stripped.)
 *
 * @param str String to be parsed; is modified.
 * @param tokenSep Each character in this string is a separator.
 * @param stripChars All characters present in this string shall be
 *        stripped from the start and end of each token.
 * @param tokens Array in which to store the start address of each token.
 * @param maxTokens Upper bound on number of addresses to store.
 * @param strip_trailing_tokens TRUE to discard excess tokens, FALSE to not
 *        split on excess separators.
 * @return Number of entries written to @a tokens.
 *
 * EXAMPLES:
 *
 * @code
 * GetStrTokens("::", ":", "", t,5)			// t[0]="", t[1]="" and t[2]=""
 * GetStrTokens("a::b", ":", "", t,5)			// t[0]="a", t[1]="" and t[2]="b"
 * GetStrTokens("a%:b:c%%", ":", "%", t,5)		// t[0]="a", t[1]="b" and t[2]="c"
 * GetStrTokens("aa:bb:cc%%, ":", "%", t, 2)		// t[0]="aa", t[1]="bb:cc"
 * GetStrTokens("aa:bb:cc%%, ":", "%", t, 2, TRUE)	// t[0]="aa", t[1]="bb"
 * @endcode
 */
int GetStrTokens(uni_char *str, const uni_char *tokenSep, const uni_char *stripChars,
				 uni_char *tokens[], UINT maxTokens, BOOL strip_trailing_tokens = FALSE);

/** Parse extension from file-name.
 *
 * The extension, if any, is the text following the last '.' in the file-name.
 * However, the file is deemed to have no extension, even if there is a '.'
 * in it, if the last '.' is:
 * - the first or last character of the file-name
 * - part of a sequence of more than one '.'
 * In these cases, or if no '.' is found, NULL is returned.
 *
 * @param fName Name of a file, without path.
 * @return Pointer to the first character after the last '.', or NULL.
 */
const char* StrFileExt(const char* fName);

/**
 * @overload
 */
const uni_char* StrFileExt(const uni_char* fName);

#endif // !MODULES_UTIL_GEN_STR_H
