/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef SVG_TOKENIZER_H
#define SVG_TOKENIZER_H

#ifdef SVG_SUPPORT

struct SVGTokenizer
{
    void		Reset(const uni_char *input_string, unsigned input_string_length);
	/**< Set the start state of the tokenizer */

	/**
	 * Rule struct for parsing lists of string.
	 */
	struct StringRules
	{
		BOOL allow_quoting; // allow quotes ('), (") to override
							// separators and backslashes (\) to write
							// quoted charaters
		BOOL wsp_separator; // use white-space as a separator
		BOOL comma_separator; // use the comma sign as a separator
		BOOL semicolon_separator; // use semicolon as a separator
		uni_char end_condition;

		void Init(BOOL allow_quoting, BOOL wsp_separator, BOOL comma_separator,
				  BOOL semicolon_separator, uni_char end_condition);
	};

	BOOL		ScanNumber(double& d);
	/**< Scan for a number as defined by the SVG specification.
	   See <number> in XML-attributes in
	   <URL:http://www.w3.org/TR/SVG11/types.html> */

	BOOL		Scan(const char *pattern) { return state.Scan(pattern); }
	/**< Scan for a single-byte character string. Returns TRUE for success,
	 * FALSE otherwise.*/

	BOOL		Scan(char c) { return state.Scan(c); }
	/**< Scan for a single-byte character. Returns TRUE for success,
	 * FALSE otherwise.*/

	BOOL		ScanAlphabetic() { return state.ScanAlphabetic(); }
	/**< Scan an alphabetic character as recognized by the unicode
	   module. Returns TRUE for success, FALSE otherwise. */

	BOOL		ScanURI(const uni_char *&uri, unsigned &uri_length);
	/**< Scan for a unquoted URI-string (For a quoted string, use
	  ScanString with a proper ruleset). An unquoted URI requires
	  quoting of certain characters like '(' and ')' as noted in the
	  CSS 2.1 specification
	  <URL:http://www.w3.org/TR/CSS21/syndata.html#uri>	 */

	BOOL		ScanURL(const uni_char *&url_string, unsigned &url_string_length);
	/**< Scan for a url(<uri>) pattern and return TRUE of if <uri> is
	 * saved in url_string/url_string_length. Returns FALSE if the
	 * pattern wasn't found or was malformed. */

	BOOL 		ScanString(StringRules &rules,
						   const uni_char *&sub_string,
						   unsigned &sub_string_length);
	/**< Scan for a string using a ruleset. Eats leading whitespace
	  and only includes it in the result if the white-space is
	  quoted. Returns TRUE if a result is found and saved to
	  sub_string/sub_string_length. Returns FALSE if a string couldn't be
	  found or it was malformed. */

	unsigned	CurrentCharacter() const { return state.current_char; }
	const uni_char*	CurrentString() const { return state.CurrentString(); }
	unsigned		CurrentStringLength() const { return state.CurrentStringLength(); }

	unsigned	Shift() { return state.Shift(); }
	/**< Throw away the current character and shift in a new one from
	   the rest of the string. Returns the NEW current character. */

	unsigned	Shift(unsigned& counter) { return state.Shift(counter); }
	/**< Throw away the current character and shift in a new one from
	   the rest of the string. This version also increments counter
	   with one. Returns the NEW current character. */

	void		EatWsp() { state.EatWsp(); }
	BOOL		IsEnd() { return state.current_char == '\0'; }
	void		EatWspSeparatorWsp(char separator) { state.EatWspSeparator(separator); }
	unsigned	MatchNumber() { return state.MatchNumber(); }
	unsigned	ScanHexDigits(unsigned& v) { return state.ScanHexDigits(v); }

	OP_STATUS	ReturnStatus(OP_STATUS status);
	/**< The point of this function is to return uniform status codes
	  from parsing functions back to the attribute parser in
	  SVGAttributeParser.cpp (that returns them to the
	  SVGManager).

	  The tokenizer must have come to the end (except whitespace) of
	  the buffer to return OK (IsEnd() must return TRUE) so trailing
	  separators before calling this function to return.

	  The function currently leaves OK and OOM-errors as
	  is and transforms all other error codes into
	  OpSVGStatus::ATTRIBUTE_ERROR. Usually the sub-parsers only uses
	  OpStatus::ERR to signal that a error has been detected in the
	  input, and that is then translated into
	  OpSVGStatus::ATTRIBUTE_ERROR upon return. */

    const uni_char *
				input_string;
    unsigned		input_string_length;

	static BOOL ShouldQuote(StringRules &r, unsigned c) { return r.allow_quoting && (c == '\'' || c == '\"'); }
	static BOOL ShouldEscape(StringRules &r, unsigned c) { return r.allow_quoting && c == '\\'; }
	static BOOL ShouldSeparate(StringRules &r, unsigned c);

	struct State {
		State();

		unsigned	Shift();
		unsigned	Shift(unsigned &counter);
		void		MoveAscii(unsigned offset);
		BOOL		Shift(uni_char *p);
		void		EatWsp();
		void		EatWspSeparator(char separator);

		BOOL		Scan(char c);
		BOOL		Scan(const char *pattern);
		BOOL		ScanAlphabetic();

		unsigned		ScanHexDigits(unsigned& v);
		unsigned		MatchNumber();
		const uni_char*	CurrentString() const;
		unsigned		CurrentStringLength() const;

		unsigned	current_char;
		const uni_char * rest_string;
		unsigned		rest_string_length;
	};

	State state;
};

#endif // SVG_SUPPORT
#endif // !SVG_TOKENIZER_H

