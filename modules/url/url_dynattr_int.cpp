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
#include "modules/url/url_man.h"

#include "modules/url/url_ds.h"

#include "modules/url/url_dynattr.h"
#include "modules/url/url_dynattr_int.h"

OP_STATUS URL_DynamicUIntAttributeDescriptor::Construct(URL_DynamicUIntAttributeHandler *hndlr)
{
	handler = hndlr; 
	if(handler.get() != NULL)
	{
		OP_ASSERT(handler->GetModuleId() <= USHRT_MAX); // If we ever get to 65K modules we have a !serious! design problem
		OP_ASSERT(handler->GetTagID() <= USHRT_MAX); // If we ever get to 65K tags in a module we have another serious problem: feature creep.

		if(handler->GetModuleId() > USHRT_MAX || handler->GetTagID() > USHRT_MAX)
			return OpStatus::ERR_OUT_OF_RANGE;

		store_in_cache_index = handler->GetCachable();

		RETURN_IF_ERROR(SetIsFlag(handler->GetIsFlag()));
	}

	return OpStatus::OK;
}

OP_STATUS URL_DynamicUIntAttributeDescriptor::SetIsFlag(BOOL is_flg)
{
	is_flag = is_flg;
	if(is_flag)
		RETURN_IF_ERROR(urlManager->GetNewFlagAttribute(flag_attribute_id, flag_mask));

	return OpStatus::OK;
}

OP_STATUS URL_DynamicUIntAttributeDescriptor::OnSetValue(URL &url, URL_DataStorage *url_ds, uint32 &in_out_value, BOOL &set_value) const
{
	set_value = FALSE; 
	if(handler.get() != NULL)
	{
		OP_STATUS op_err = handler->OnSetValue(url, in_out_value, set_value);

		if(OpStatus::IsSuccess(op_err) && is_flag && set_value)
		{
			uint32 mask_set = 0;
			if(in_out_value != 0)
			{
				in_out_value = TRUE;
				mask_set = flag_mask;
			}

			uint32 current_flags;
#ifdef SELFTEST
			// Hack to allow selftests to test directly */
			if(url_ds == NULL)
				current_flags = url.GetAttribute(flag_attribute_id);
			else
#endif
				current_flags = url_ds->GetAttribute(flag_attribute_id);

			current_flags = (current_flags & (~flag_mask)) | mask_set;

#ifdef SELFTEST
			// Hack to allow selftests to test directly */
			if(url_ds == NULL)
				op_err = url.SetAttribute(flag_attribute_id, current_flags);
			else
#endif
				op_err = url_ds->SetAttribute(flag_attribute_id, current_flags);
			set_value = FALSE;
		}

		return op_err;
	}

	return OpStatus::OK;
}

OP_STATUS URL_DynamicUIntAttributeDescriptor::OnGetValue(URL &url, uint32 &in_out_value) const
{
	if(handler.get() != NULL)
	{
		if(is_flag)
		{
			uint32 current_flags = url.GetAttribute(flag_attribute_id);

			in_out_value = ((current_flags & flag_mask) != 0 ? TRUE : FALSE);
		}

		OP_STATUS op_err =  handler->OnGetValue(url, in_out_value);

		if(OpStatus::IsSuccess(op_err) && is_flag && in_out_value)
			in_out_value = TRUE;

		return op_err;
	}
	return OpStatus::OK;
}


OP_STATUS URL_DynamicStringAttributeDescriptor::Construct(URL_DynamicStringAttributeHandler *hndlr)
{
	handler = hndlr; 
	if(handler.get() != NULL)
	{
		OP_ASSERT(handler->GetModuleId() <= USHRT_MAX); // If we ever get to 65K modules we have a !serious! design problem
		OP_ASSERT(handler->GetTagID() <= USHRT_MAX); // If we ever get to 65K tags in a module we have another serious problem: feature creep.

		if(handler->GetModuleId() > USHRT_MAX || handler->GetTagID() > USHRT_MAX)
			return OpStatus::ERR_OUT_OF_RANGE;

		module_identifier = handler->GetModuleId();
		identifier = handler->GetTagID();
		store_in_cache_index = handler->GetCachable();
#if defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
		header_name_is_prefix = handler->GetIsPrefixHeader();
#endif
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER
		send_http = handler->GetSendHTTP();
#endif
#if defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
		get_http= handler->GetReceiveHTTP();
#endif
#if defined URL_ENABLE_DYNATTR_SEND_HEADER || defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
		return http_header_name.Set(handler->GetHTTPHeaderName());
#endif
	}
	return OpStatus::OK;
}

#if defined URL_ENABLE_DYNATTR_SEND_HEADER
OP_STATUS URL_DynamicStringAttributeDescriptor::OnSendHTTPHeader(URL &url, OpString8 &in_out_value, BOOL &send) const
{
	send = FALSE; 
	if(handler.get() != NULL)
	{
		RETURN_IF_ERROR(handler->OnSendHTTPHeader(url, in_out_value, send));
		if(in_out_value.FindFirstOf("\n\r") != KNotFound)
		{
			in_out_value.Empty();
			send = FALSE;
		}
	}
	return OpStatus::OK;
}
#endif

#ifdef URL_ENABLE_DYNATTR_SEND_MULTIHEADER
BOOL DynAttrCheckBlockedHeader(const OpStringC8 &hdr);
OP_STATUS URL_DynamicStringAttributeDescriptor::GetSendHTTPHeaders(URL &url, const char * const*&send_headers, int &count) const
{
	count = NULL;
	send_headers = NULL;
	if(header_name_is_prefix && handler.get() != NULL)
	{
		RETURN_IF_ERROR(handler->GetSendHTTPHeaders(url, send_headers, count));

		int hdr_name_len = http_header_name.Length();
		for(int i = 0; i< count; i++)
		{
			{
				OpStringC8 hdrname(send_headers[i]);
				if(hdrname.IsEmpty() ||
					hdrname.CompareI(http_header_name.CStr(), hdr_name_len) != 0  ||
					hdrname.SpanOf("abcdefghijklmnopqrstuvwxyzABCDEFGHILJKLMNOPQRSTUVWXYZ0123456789+-_") !=hdrname.Length() ||
					DynAttrCheckBlockedHeader(hdrname) )
				{
					count = 0;
					send_headers = NULL;
					break;
				}
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS URL_DynamicStringAttributeDescriptor::OnSendPrefixHTTPHeader(URL &url, const OpStringC8 &header, OpString8 &in_out_value, BOOL &send) const
{
	send = FALSE; 
	if(handler.get() != NULL)
	{
		RETURN_IF_ERROR(handler->OnSendPrefixHTTPHeader(url, header, in_out_value, send));
		if(in_out_value.FindFirstOf("\n\r") != KNotFound)
		{
			in_out_value.Empty();
			send = FALSE;
		}
	}
	return OpStatus::OK;
}
#endif
