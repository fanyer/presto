/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

#include "core/pch.h"

#include "modules/url/url2.h"

#include "modules/url/url_dynattr.h"

OP_STATUS URL_DynamicUIntAttributeHandler::OnSetValue(URL &url, uint32 &in_out_value, BOOL &set_value) const
{
	set_value = TRUE;
	return OpStatus::OK;
}

OP_STATUS URL_DynamicUIntAttributeHandler::OnGetValue(URL &url, uint32 &in_out_value) const
{
	return OpStatus::OK;
}

#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_RECV_HEADER
OP_STATUS	URL_DynamicStringAttributeHandler::SetHTTPInfo(const OpStringC8 &hdr_name, BOOL send, BOOL recv)
{
	if(hdr_name.IsEmpty())
	{
#if defined URL_ENABLE_DYNATTR_SEND_HEADER
		send_http = FALSE;
#endif
#if defined URL_ENABLE_DYNATTR_RECV_HEADER
		get_http = FALSE;
#endif
		http_header_name.Empty();
		return OpStatus::OK;
	}
#if defined URL_ENABLE_DYNATTR_SEND_HEADER
	send_http = send;
#endif
#if defined URL_ENABLE_DYNATTR_RECV_HEADER
	get_http = recv;
#endif
	if(hdr_name.SpanOf("abcdefghijklmnopqrstuvwxyzABCDEFGHILJKLMNOPQRSTUVWXYZ0123456789+-_") !=hdr_name.Length())
		return OpStatus::ERR; 

	return http_header_name.Set(hdr_name);
}
#endif

#if defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
OP_STATUS URL_DynamicStringAttributeHandler::SetPrefixHTTPInfo(const OpStringC8 &hdr_name, BOOL send, BOOL recv)
{
	if(hdr_name.IsEmpty())
	{
#ifdef URL_ENABLE_DYNATTR_SEND_MULTIHEADER
		send_http = FALSE;
#endif
#ifdef URL_ENABLE_DYNATTR_RECV_MULTIHEADER
		get_http = FALSE;
#endif
		http_header_name.Empty();
		return OpStatus::OK;
	}
	header_name_is_prefix = TRUE;
#ifdef URL_ENABLE_DYNATTR_SEND_MULTIHEADER
	send_http = send;
#endif
#ifdef URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	get_http = recv;
#endif
	if(hdr_name.SpanOf("abcdefghijklmnopqrstuvwxyzABCDEFGHILJKLMNOPQRSTUVWXYZ0123456789+-_") !=hdr_name.Length())
		return OpStatus::ERR; 

	return http_header_name.Set(hdr_name);
}
#endif

OP_STATUS URL_DynamicStringAttributeHandler::OnSetValue(URL &url, OpString8 &in_out_value, BOOL &set_value) const
{
	set_value = TRUE;
	return OpStatus::OK;
}

OP_STATUS URL_DynamicStringAttributeHandler::OnGetValue(URL &url, OpString8 &in_out_value) const
{
	return OpStatus::OK;
}

#ifdef URL_ENABLE_DYNATTR_SEND_HEADER
OP_STATUS URL_DynamicStringAttributeHandler::OnSendHTTPHeader(URL &url, OpString8 &in_out_value, BOOL &send) const
{
	send = TRUE;
	return OpStatus::OK;
}
#endif

#if defined URL_ENABLE_DYNATTR_RECV_HEADER
OP_STATUS URL_DynamicStringAttributeHandler::OnRecvHTTPHeader(URL &url, OpString8 &in_out_value, BOOL &set) const
{
	set = TRUE;
	return OpStatus::OK;
}
#endif

#ifdef URL_ENABLE_DYNATTR_SEND_MULTIHEADER
OP_STATUS URL_DynamicStringAttributeHandler::GetSendHTTPHeaders(URL &url, const char * const*&send_headers, int &count) const
{
	send_headers = NULL;
	count = 0;
	return OpStatus::OK;
}

OP_STATUS URL_DynamicStringAttributeHandler::OnSendPrefixHTTPHeader(URL &url, const OpStringC8 &header, OpString8 &in_out_value, BOOL &send) const
{
	send = TRUE;
	return OpStatus::OK;
}
#endif

#ifdef URL_ENABLE_DYNATTR_RECV_MULTIHEADER
OP_STATUS URL_DynamicStringAttributeHandler::OnRecvPrefixHTTPHeader(URL &url, const OpStringC8 &header, OpString8 &in_out_value, BOOL &set) const
{
	set = TRUE;
	return OpStatus::OK;
}
#endif

OP_STATUS URL_DynamicURLAttributeHandler::OnSetValue(URL &url, URL &in_out_value, BOOL &set_value) const
{
	set_value = TRUE;
	return OpStatus::OK;
}

OP_STATUS URL_DynamicURLAttributeHandler::OnGetValue(URL &url, URL &in_out_value) const
{
	return OpStatus::OK;
}
