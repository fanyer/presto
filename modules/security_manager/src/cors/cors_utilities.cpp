/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef CORS_SUPPORT
#include "modules/security_manager/include/security_manager.h"
#include "modules/security_manager/src/cors/cors_config.h"
#include "modules/security_manager/src/cors/cors_utilities.h"
#include "modules/security_manager/src/cors/cors_request.h"
#ifdef SECMAN_CROSS_ORIGIN_STRICT_FAILURE
#include "modules/cookies/url_cm.h"
#endif // SECMAN_CROSS_ORIGIN_STRICT_FAILURE

/* static */ BOOL
OpCrossOrigin_Utilities::IsSimpleMethod(const uni_char *method)
{
	if (uni_str_eq(method, "GET") || uni_str_eq(method, "HEAD") || uni_str_eq(method, "POST"))
		return TRUE;

	return FALSE;
}

static BOOL
MatchesMIMEType(const char *value, const char *type, unsigned length)
{
	if (op_strnicmp(value, type, length) == 0)
		return value[length] == 0 || value[length] == ';' || op_isspace(value[length]);

	return FALSE;
}

#define STRING_LITERAL(x) x, (sizeof(x) - 1)

/* static */ BOOL
OpCrossOrigin_Utilities::IsSimpleRequestHeader(const uni_char *header, const char *value, BOOL check_name_only)
{
	if (uni_stri_eq(header, "Accept") || uni_stri_eq(header, "Accept-Language") || uni_stri_eq(header, "Content-Language"))
		return TRUE;

	if (uni_stri_eq(header, "Content-Type"))
		if (check_name_only)
			return TRUE;
		else if (value != NULL)
			if (MatchesMIMEType(value, STRING_LITERAL("application/x-www-form-urlencoded")) ||
			    MatchesMIMEType(value, STRING_LITERAL("multipart/form-data")) ||
			    MatchesMIMEType(value, STRING_LITERAL("text/plain")))
				return TRUE;

	return FALSE;
}
#undef STRING_LITERAL

/* static */ BOOL
OpCrossOrigin_Utilities::IsSimpleResponseHeader(const uni_char *header)
{
	if (uni_stri_eq(header, "Cache-Control") ||
	    uni_stri_eq(header, "Content-Language") ||
	    uni_stri_eq(header, "Content-Type") ||
	    uni_stri_eq(header, "Expires") ||
	    uni_stri_eq(header, "Last-Modified") ||
	    uni_stri_eq(header, "Pragma"))
		return TRUE;

	return FALSE;
}

static BOOL
IsRFC2616TokenChar(uni_char ch)
{
	return (ch < 0x7f && ch > 0x20 && !uni_strchr(UNI_L("\t()<>@,;:\\\"/[]?={}"), ch));
}

/* static */ int
OpCrossOrigin_Utilities::CompareASCIICaseInsensitive(const uni_char *str1, const uni_char *str2, unsigned length, BOOL delimited)
{
	const uni_char *end = str2 + (length == UINT_MAX ? uni_strlen(str2) : length);
	while (str2 != end)
	{
		if (*str1 == 0)
			return -1;
		else if (*str1 != *str2)
			if (*str1 >= 0x7f || *str2 >= 0x7f || uni_tolower(*str1) != uni_tolower(*str2))
				return (*str1 < *str2 ? -1 : 1);
		str1++;
		str2++;
	}
	/* If 'delimited' is TRUE, then str1 must be followed by whitespace
	   or a element-delimiter (,) */
	if (delimited)
	{
		BOOL delim1 = *str1 == 0 || uni_isspace(*str1) || *str1 == ',';
		BOOL delim2 = *str2 == 0 || uni_isspace(*str2) || *str2 == ',';
		if (delim1 && delim2)
			return 0;
		else if (delim1)
			return 1;
		else
			return -1;
	}
	else
		return 0;
}

/* static */ BOOL
OpCrossOrigin_Utilities::IsEqualASCIICaseInsensitive(const uni_char *str1, const uni_char *str2, unsigned length, BOOL delimited)
{
	return CompareASCIICaseInsensitive(str1, str2, length, delimited) == 0;
}

/* static */ BOOL
OpCrossOrigin_Utilities::StripLWS(const uni_char *&input)
{
	while (*input == ' ' || *input == '\t' || *input == '\r')
	{
		/* Line folding. */
		if (*input == '\r')
			if (input[1] == '\n' && (input[2] == ' ' || input[2] == '\t'))
				input += 2;
			else
				return FALSE;

		input++;
	}

	return TRUE;
}

/* static */ BOOL
OpCrossOrigin_Utilities::HasSingleFieldValue(const uni_char *field_value, BOOL recognize_comma)
{
	const uni_char *input = field_value;
	if (!input || !StripLWS(input))
		return FALSE;

	while (*input && !uni_isspace(*input) && (!recognize_comma || *input != ','))
		input++;

	if (!StripLWS(input))
		return FALSE;

	return !*input;
}

/* static */ BOOL
OpCrossOrigin_Utilities::HasFieldValue(const uni_char *field_value, const uni_char *value, unsigned length)
{
	const uni_char *input = field_value;
	if (!StripLWS(input))
		return FALSE;

	length = length != UINT_MAX ? length : uni_strlen(value);
	if (uni_strncmp(input, value, length) == 0)
	{
		input += length;
		while (*input)
		{
			if (*input != ' ' && *input != '\t' && *input != '\r')
				return FALSE;
			if (*input == '\r')
				if (input[1] != '\n')
					return FALSE;
				else
					input++;

			input++;
		}

		return TRUE;
	}
	else
		return FALSE;
}

/* static */ OP_STATUS
OpCrossOrigin_Utilities::GetHeaderValue(const URL &url, const char *header, OpString &value)
{
	OpString8 value8;
	RETURN_IF_ERROR(value8.Set(header));
	RETURN_IF_ERROR(url.GetAttribute(URL::KHTTPSpecificResponseHeadersL, value8));
	return value.Set(value8.CStr());
}

static const uni_char *
ConsumeWhitespace(const uni_char *input)
{
	while (*input == ' ')
		input++;
	while (*input == '\r' && input[1] == '\n')
	{
		input += 2;
		while (*input == ' ')
			input++;
	}
	return input;
}

/* static */ OP_STATUS
OpCrossOrigin_Utilities::ParseValueList(const uni_char *input, ListElementHandler &handler, BOOL optional_comma)
{
	if (!input)
		return OpStatus::OK;

	while (*input)
	{
		input = ConsumeWhitespace(input);
		if (*input == ',')
			input++;
		else
		{
			const uni_char *start = input;
			while (IsRFC2616TokenChar(*input))
				input++;

			if (input != start)
				RETURN_IF_ERROR(handler.HandleElement(start, input - start));
			else
				return OpStatus::ERR;

			input = ConsumeWhitespace(input);
			if (!optional_comma && *input && *input != ',')
				return OpStatus::ERR;
		}
	}
	return OpStatus::OK;
}

class CORS_ProcessAccessMethodElement
	: public OpCrossOrigin_Utilities::ListElementHandler
{
public:
	CORS_ProcessAccessMethodElement(const OpCrossOrigin_Request &r, OpVector<OpString> &methods)
		: request(r)
		, methods(methods)
		, match_result(FALSE)
	{
	}

	const OpCrossOrigin_Request &request;
	OpVector<OpString> &methods;
	BOOL match_result;

	virtual OP_STATUS HandleElement(const uni_char *start, unsigned length)
	{
		if (match_result || uni_strncmp(request.GetMethod(), start, length) == 0 && (start[length] == 0 || uni_isspace(start[length]) ||  start[length] == ','))
			match_result = TRUE;
		OpString *s = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(s);
		RETURN_IF_ERROR(s->Set(start, length));
		RETURN_IF_ERROR(methods.Add(s));
		return OpStatus::OK;
	}
};

/* static */ OP_STATUS
OpCrossOrigin_Utilities::ParseAndMatchMethodList(const OpCrossOrigin_Request &request, const uni_char *input, OpVector<OpString> &methods, BOOL &matched, BOOL optional_comma)
{
	CORS_ProcessAccessMethodElement method_handler(request, methods);

	OP_STATUS status = ParseValueList(input, method_handler, optional_comma);
	if (OpStatus::IsError(status))
	{
		methods.DeleteAll();
		return status;
	}
	matched = method_handler.match_result;
	return OpStatus::OK;
}

class CORS_ProcessAccessHeaderElement
	: public OpCrossOrigin_Utilities::ListElementHandler
{
public:
	const OpCrossOrigin_Request &request;
	OpVector<OpString> &headers;
	OpINT32Vector matches_seen;
	BOOL match_result;

	CORS_ProcessAccessHeaderElement(const OpCrossOrigin_Request &r, OpVector<OpString> &headers)
		: request(r)
		, headers(headers)
		, match_result(FALSE)
	{
	}

	OP_STATUS Construct()
	{
		for (unsigned i = 0; i < request.GetHeaders().GetCount(); i++)
			RETURN_IF_ERROR(matches_seen.Add(0));

		return OpStatus::OK;
	}

	virtual OP_STATUS HandleElement(const uni_char *start, unsigned length)
	{
		unsigned count = request.GetHeaders().GetCount();
		for (unsigned i = 0; i < count; i++)
			if (OpCrossOrigin_Utilities::IsEqualASCIICaseInsensitive(request.GetHeaders().Get(i), start, length, TRUE))
				RETURN_IF_ERROR(matches_seen.Replace(i, 1));

		OpString *s = OP_NEW(OpString, ());
		RETURN_OOM_IF_NULL(s);
		RETURN_IF_ERROR(s->Set(start, length));
		RETURN_IF_ERROR(headers.Add(s));
		return OpStatus::OK;
	}
};

/* static */ OP_STATUS
OpCrossOrigin_Utilities::ParseAndMatchHeaderList(const OpCrossOrigin_Request &request, const uni_char *input, OpVector<OpString> &headers, BOOL &matched, BOOL optional_comma)
{
	CORS_ProcessAccessHeaderElement header_handler(request, headers);
	RETURN_IF_ERROR(header_handler.Construct());

	OP_STATUS status = ParseValueList(input, header_handler, optional_comma);
	if (OpStatus::IsError(status))
	{
		headers.DeleteAll();
		return status;
	}

	matched = TRUE;
	for (unsigned i = 0; i < request.GetHeaders().GetCount(); i++)
		if (header_handler.matches_seen.Get(i) == 0 && !OpCrossOrigin_Utilities::IsSimpleRequestHeader(request.GetHeaders().Get(i), NULL, TRUE))
		{
			matched = FALSE;
			break;
		}

	return OpStatus::OK;
}

#ifdef SECMAN_CROSS_ORIGIN_STRICT_FAILURE
/* static */ OP_STATUS
OpCrossOrigin_Utilities::RemoveCookiesByURL(URL &url)
{
	int count = 0;
	RETURN_IF_ERROR(g_url_api->BuildCookieList(NULL, &count, url));
	if (count > 0)
	{
		Cookie **cookie_entries = OP_NEWA(Cookie *, count);
		OpAutoArray<Cookie *> anchor_cookies(cookie_entries);

		count = 0;
		RETURN_IF_ERROR(g_url_api->BuildCookieList(cookie_entries, &count, url));

		for (int i = 0; i < count; i++)
		{
			OpString str;
			RETURN_IF_ERROR(str.Set(cookie_entries[i]->Name()));
			RETURN_IF_ERROR(g_url_api->RemoveCookieList(NULL, NULL, str.CStr()));
		}
	}
	return OpStatus::OK;
}
#endif // SECMAN_CROSS_ORIGIN_STRICT_FAILURE

#endif // CORS_SUPPORT
