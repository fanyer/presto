/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef CORS_UTILITIES_H
#define CORS_UTILITIES_H

#ifdef CORS_SUPPORT
#include "modules/security_manager/src/cors/cors_config.h"

class OpCrossOrigin_Utilities
{
public:
	static BOOL IsSimpleMethod(const uni_char *method);
	/**< Predicate for checking if method is simple [CORS: Section 3].

	     @param method The method name; MUST not be NULL.
	     @return TRUE if method is simple, FALSE if not. */

	static BOOL IsSimpleRequestHeader(const uni_char *header, const char *value, BOOL check_name_only);
	/**< Check if given request header name and value
	     is a 'simple' header [CORS: Section 3]

	     @param header The header name, must not be NULL.
	     @param value The header value, can be NULL.
	     @param check_name_only If TRUE, disregard the value and
	            only consider the header name for simplicity.
	     @return TRUE if header,value pair is simple, FALSE if not. */

	static BOOL IsSimpleResponseHeader(const uni_char *header);
	/**< Check if given response header constitutes as 'simple' [CORS: Section 3]

	     @param header The header name; MUST not be NULL.
	     @return TRUE if header is simple, FALSE if not. */

	static int CompareASCIICaseInsensitive(const uni_char *str1, const uni_char *str2, unsigned length = UINT_MAX, BOOL delimited = FALSE);
	/**< Header name comparison using an ASCII case-insensitive predicate.

	     @param str1 a zero-terminated string.
	     @param str2 the string to compare against.
	     @param length number of codepoints from str2 to at most
	            compare str1 against.
	     @param delimited If TRUE, then both strings must be delimited
	            by whitespace, NUL or comma. If equal upto the
	            delimiting character, the inputs are considered equal
	            if both have a delimiter (the actual delimiting
	            character can be different.) If not, then the
	            string without a delimiter "wins" the comparison.
	     @return (-1) if str1 is a prefix of str2, or equal upto
	             an index I, where str1 has a codepoint less than
	             str2 _or_ has a lesser US-ASCII lower-case value
	             at I.
	             0 if string equality (using same predicate)
	             upto length 'length'.
	             1 if an equal prefix (<length), but diverges
	             using the negated predicate used for (-1). */

	static BOOL IsEqualASCIICaseInsensitive(const uni_char *str1, const uni_char *str2, unsigned length = UINT_MAX, BOOL delimited = FALSE);
	/**< Test header name equality, using an ASCII case-insensitive predicate.

	     @param str1 a zero-terminated string.
	     @param str2 the string to compare equality against.
	     @param length number of codepoints from str2 to at most
	            compare str1 against.
	     @param delimited If TRUE, then both inputs must be followed
	            by a delimiting character (either whitespace or comma)
	            to be equal.
	     @return TRUE if str1 is element-wise equal to str2 upto
	             length 'length', where equality at index I holds
	             if str1 has a lower-case US-ASCII codepoint equal
	             to that of str2 at index I, or if not in the
	             US-ASCII range, has an equal codepoint. */

	static BOOL HasSingleFieldValue(const uni_char *field_value, BOOL recognize_comma);
	/**< Determine if 'field_value' just has a single value.

	     @param field_value Field value string to check.
	     @param recognize_comma TRUE if comma should be considered a separator
	            (along with whitespace.)
	     @return TRUE if response has exactly one header entry. */

	static BOOL HasFieldValue(const uni_char *field_value, const uni_char *value, unsigned length = UINT_MAX);
	/**< Test header field value for being equal to 'value', ignoring whitespace (before and after.) */

	static OP_STATUS GetHeaderValue(const URL &url, const char *header, OpString &value);
	/**< From the response header values in 'url', return the header field
	     value for 'header'.

	     @param url The URL having the response headers included.
	     @param header Name of the header; must not be NULL.
	     @param [out] value The field value result; the empty
	            string if not found.
	     @return OpStatus::OK if header list successfully scanned,
	             'value' holding the outcome. OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS ParseAndMatchMethodList(const OpCrossOrigin_Request &request, const uni_char *input, OpVector<OpString> &methods, BOOL &matched, BOOL optional_comma = FALSE);
	/**< Parse the given input list of HTTP methods and check if the request
	     method is is a member. Used to verify if the request method is allowed
	     by the cross-origin resource (Access-Control-Allow-Methods.)

	     @param request The cross-origin request being verified.
	     @param input The header field value, should not be NULL.
	     @param [out] methods The parsed list of method verbs.
	     @param [out] matched TRUE if the request method was found
	            to be a member of the method list. FALSE if not.
	     @return OpStatus::OK on successful parse, OpStatus::ERR if
	             syntactically invalid. OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_BOOLEAN ParseAndMatchHeaderList(const OpCrossOrigin_Request &request, const uni_char *input, OpVector<OpString> &headers, BOOL &matched, BOOL optional_comma = FALSE);
	/**< Parse the given input list of HTTP header names and check if the author
	     requested header names in 'request' are members. Used to verify if the
	     request's headers are allowed by the cross-origin resource
	     (Access-Control-Allow-Headers.)

	     @param request The cross-origin request being verified.
	     @param input The header field value, should not be NULL.
	     @param [out] methods The parsed list of method verbs.
	     @param [out] matched TRUE if the request method was found
	            to be a member of the method list. FALSE if not.
	     @return OpStatus::OK on successful parse, OpStatus::ERR if
	             syntactically invalid. OpStatus::ERR_NO_MEMORY on OOM. */

	/** Function/callback class that ParseValueList() is parameterised over. */
	class ListElementHandler
	{
	public:
		virtual OP_STATUS HandleElement(const uni_char *start, unsigned length) = 0;
	};

	static BOOL StripLWS(const uni_char *&input);
	/**< Strip out OLWS from header value strings; returns FALSE if an ill-formed
	     field value. */

#ifdef SECMAN_CROSS_ORIGIN_STRICT_FAILURE
	static OP_STATUS RemoveCookiesByURL(URL &url);
	/**< Remove all named cookies that the given URL has supplied.
	     Used to revert the cookie data that an actual response returned,
	     but one that proceeded to fail a CORS access check.

	     @param url The URL to cleanse the cookies it supplied.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */
#endif // SECMAN_CROSS_ORIGIN_STRICT_FAILURE

private:
	static OP_STATUS ParseValueList(const uni_char *input, ListElementHandler &handler,BOOL optional_comma = FALSE);
	/**< Internal mini parser of comma-delimited value lists. The 'handler' is passed each
	     element encountered.

	     @param input the field value input string.
	     @param handler the callback function.
	     @return OpStatus::OK if list was successfully parsed. OpStatus::ERR if the
	             the list was syntactically invalid or the handler returned OpStatus::ERR
	             while processing an element. OpStatus::ERR_NO_MEMORY on OOM. */
};

#endif // CORS_SUPPORT
#endif // CORS_UTILITIES_H
