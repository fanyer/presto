/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

#include "core/pch.h"
#include "modules/url/url2.h"

#include "modules/url/url_dynattr.h"
#include "modules/url/special_attrs.h"

#include "modules/formats/keywordman.h"
#include "modules/url/tools/arrays.h"
#include "modules/network/src/op_request_impl.h"

#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, url)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)

CONST_KEYWORD_ARRAY(HTTP_arbitrary_method_list)
	CONST_KEYWORD_ENTRY(NULL, HTTP_METHOD_String)
	CONST_KEYWORD_ENTRY("CONNECT", HTTP_METHOD_Invalid)
	CONST_KEYWORD_ENTRY("DELETE", HTTP_METHOD_DELETE)
	CONST_KEYWORD_ENTRY("GET", HTTP_METHOD_GET)
	CONST_KEYWORD_ENTRY("HEAD", HTTP_METHOD_HEAD)
	CONST_KEYWORD_ENTRY("OPTIONS", HTTP_METHOD_OPTIONS)
	CONST_KEYWORD_ENTRY("POST", HTTP_METHOD_POST)
	CONST_KEYWORD_ENTRY("PUT", HTTP_METHOD_PUT)
	CONST_KEYWORD_ENTRY("SEARCH", HTTP_METHOD_SEARCH)
	CONST_KEYWORD_ENTRY("TRACE", HTTP_METHOD_Invalid)
	CONST_KEYWORD_ENTRY("TRACK", HTTP_METHOD_Invalid)
CONST_KEYWORD_END(HTTP_arbitrary_method_list)

#define KEYWORD_TABLE_SIZE_HTTP_arbitrary_method_list CONST_DOUBLE_ARRAY_SIZE(url, HTTP_arbitrary_method_list)

OP_STATUS OpRequestImpl::SetCustomHTTPMethod(const char *special_method)
{
	// Check for nontoken chars
	const char *val = special_method;
	while(*val)
	{
		if (op_iscntrl(static_cast<int>(static_cast<unsigned char>(*val))) ||
		   static_cast<unsigned char>(*val) >= 0x80)
		{
			return OpStatus::ERR_PARSING_FAILED;
		}
		val++;
	}

	if (op_strpbrk(special_method, " \t()<>@,;:/[]{}?=\\\"\'"))
	{
		return OpStatus::ERR_PARSING_FAILED;
	}

	HTTP_Request_Method method = (HTTP_Request_Method) CheckKeywordsIndex(special_method, g_HTTP_arbitrary_method_list, KEYWORD_TABLE_SIZE_HTTP_arbitrary_method_list);
	OpStatus::Ignore(SetHTTPMethod(method));

	if (method == HTTP_METHOD_GET || method == HTTP_METHOD_HEAD )
		return OpStatus::OK;

	m_url.SetAttribute(URL::KUnique, (UINT32) TRUE);

	return m_url.SetAttribute(URL::KHTTPSpecialMethodStr, special_method);
}

OP_STATUS HTTP_MethodStrAttrHandler::OnSetValue(URL &url, OpString8 &in_out_value, BOOL &set_value) const
{
	set_value = FALSE;
	if(in_out_value.IsEmpty())
		return OpStatus::ERR_PARSING_FAILED;

	// Check for nontoken chars
	const char *val = in_out_value.CStr();
	while(*val)
	{
		if(op_iscntrl(static_cast<int>(static_cast<unsigned char>(*val))) ||
		   static_cast<unsigned char>(*val) >= 0x80)
		{
			url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_Invalid);
			return OpStatus::ERR_PARSING_FAILED;
		}
		val++;
	}

	if(in_out_value.FindFirstOf(" \t()<>@,;:/[]{}?=\\\"\'") != KNotFound)
	{
		url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_Invalid);
		return OpStatus::ERR_PARSING_FAILED;
	}

	HTTP_Method method = (HTTP_Method) CheckKeywordsIndex(in_out_value.CStr(), g_HTTP_arbitrary_method_list, KEYWORD_TABLE_SIZE_HTTP_arbitrary_method_list);

	url.SetAttribute(URL::KHTTP_Method, method);
	if(method == HTTP_METHOD_Invalid)
		return OpStatus::ERR_PARSING_FAILED; // Illegal values

	if(method == HTTP_METHOD_GET || method == HTTP_METHOD_HEAD )
		return OpStatus::OK;

	g_url_api->MakeUnique(url); // All the methods below are to be made unique
	if(method != HTTP_METHOD_String)
		return OpStatus::OK;

	url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_String);
	set_value = TRUE;
	return OpStatus::OK;
}

OP_STATUS HTTP_MethodStrAttrHandler::OnGetValue(URL &url, OpString8 &in_out_value) const
{
	HTTP_Method method = url.GetAttribute(URL::KHTTP_Method);
	if (method <= HTTP_METHOD_MAX)
		return in_out_value.Set(GetHTTPMethodString(method));

	if (method ==  HTTP_METHOD_Invalid)
	{
		in_out_value.Empty();
		return OpStatus::OK;
	}

	return OpStatus::OK;
}
