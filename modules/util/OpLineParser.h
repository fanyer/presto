/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_OPLINEPARSER_H
#define MODULES_UTIL_OPLINEPARSER_H

/**
 * OpLineParser contains functionality that given a string parse out
 * comma separated strings and numbers. A token is given by a collection of
 * characters which are not whitespace unless surrounded by ". An
 * token ends with a ,.
 */
class OpLineParser
{
public:

	OpLineParser(const uni_char* line) {m_current_pos = line;}

	/**
	 * @param token will be set to the next token.
	 *
	 * @return OpStatus::ERR_NO_MEMORY if token string couldn't be
	 * set. OpStatus::OK otherwise.
	 */
	OP_STATUS GetNextToken(OpString& token);
	OP_STATUS GetNextToken8(OpString8& token8);

	/**
	 * Get the next token but remove digits from the end, and stop
	 * when a non-digit character appears.
	 */
	OP_STATUS GetNextTokenWithoutTrailingDigits(OpString8& token);

	/**
	 * If the current character is a " then GetNextString fills
	 * 'string' with all characters until either end of string or
	 * " is found. Then it also skips all characters up to the
	 * next , or to end of the string is found.
	 */
	OP_STATUS GetNextString(OpString& string);

	/**
	 * Read a number and try to look it up using the language
	 * manager and set 'language_string' to the result.
	 *
	 * @param language_string is set to the language string
	 * corresponding to the number read.
	 * 
	 * @param id will be set to the number read.
	 *
	 * @return OpStatus::OK if everything went ok and id not equal
	 * to NULL. If id is equal to NULL OpStatus::ERR will be
	 * returned even if everything went
	 * ok. OpStatus::ERR_NO_MEMORY if not enough memory was
	 * available.
	 */
	OP_STATUS GetNextLanguageString(OpString& language_string, Str::LocaleString* id = NULL);

	/**
	 * Reads in a integer. And then skips all characters up to the
	 * next ,. If the current character is a # then GetNextValue
	 * expects a colorvalue given as three hexadecimal values
	 * representing R, G, B.
	 *
	 * @param value is set to the integer which was read.
	 *
	 * @return Always OpStatus::OK.
	 */
	OP_STATUS GetNextValue(INT32& value);

	/**
	 * Read either a string or a value depending on if the current
	 * character is a " or something else.
	 */
	OP_STATUS GetNextStringOrValue(OpString& string, INT32& value);

	BOOL HasContent() {return m_current_pos && *m_current_pos;}

private:

	void FindFirstCharacter();
	INT32 FindLastCharacter(uni_char end_character, BOOL trim_space = TRUE);

	const uni_char* m_current_pos;
};

/**
 * Write a string consisting of comma separated strings and numbers.
 */
class OpLineString
{
public:

	OpLineString() : m_commas_needed(0) {m_string.Reserve(128);}

	/**
	 * Write a , if needed then write 'token'.
	 *
	 * @return OpStatus::ERR_NO_MEMORY if there was not enough
	 * memory to store the new token in the string.
	 */
	OP_STATUS WriteToken(const uni_char* token);
	OP_STATUS WriteToken8(const char* token8);

	/**
	 * First check if there is a , to be written and then write
	 * 'token' followed by 'value'. If 'token' is either NULL or
	 * empty nothing is written but a , will be written by a
	 * subsequent call to any write function.
	 *
	 * @return OpStatus::ERR_NO_MEMORY if there was not enough
	 * memory to append 'token'.
	 */
	OP_STATUS WriteTokenWithTrailingDigits(const char* token, INT32 value);

	/**
	 * First write a , if needed from a previous write and then
	 * write " followed by 'string' and ".
	 *
	 * @return OpStatus::ERR_NO_MEMORY if there was not enough
	 * memory to write "'string'".
	 */
	OP_STATUS WriteString(const uni_char* string);
	OP_STATUS WriteString8(const char* string);

	/**
	 * First write a , if needed. Then convert 'value' to a ascii string
	 * in base 10 and write that.
	 *
	 * @param value the number to be written.
	 *
	 * @param treat_zero_as_empty when true then value=0 will not
	 * be written. But a , will be written on a later call to a
	 * write function.
	 */
	OP_STATUS WriteValue(INT32 value, BOOL treat_zero_as_empty = TRUE);

	/**
	 * Make sure a , is written on next call to one of the write
	 * functions.
	 */
	OP_STATUS WriteEmpty() {m_commas_needed++; return OpStatus::OK;}

	/**
	 * Write another separator than a ,.
	 */
	OP_STATUS WriteSeparator(const char separator);

	BOOL HasContent() {return m_string.HasContent();}
	const uni_char*	GetString() {return m_string.CStr();}
private:

	OP_STATUS	AppendNeededCommas();

	INT32		m_commas_needed;
	OpString	m_string;
};

#endif // !MODULES_UTIL_OPLINEPARSER_H
