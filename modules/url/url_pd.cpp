/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/hardcore/mh/mh.h"

#include "modules/util/datefun.h"
#include "modules/util/handy.h"

#include "modules/prefs/prefsmanager/prefsmanager.h"
#include "modules/url/url2.h"
#include "modules/url/uamanager/ua.h"
#include "modules/upload/upload.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_pd.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/upload/upload.h"
#include "modules/formats/url_dfr.h"
#include "modules/url/tools/url_util.h"
#include "modules/url/url_link.h"

#ifdef ABSTRACT_CERTIFICATES
#include "modules/pi/network/OpCertificate.h"
#endif

#include "modules/util/cleanse.h"
#include "modules/url/tools/arrays.h"
#include "modules/url/protocols/common.h"

// Url_pdh.cpp

// URL Protocol Data HTTP

#define CONST_KEYWORD_ARRAY(name) PREFIX_CONST_ARRAY(static, KeywordIndex, name, url)
#define CONST_KEYWORD_ENTRY(x,y) CONST_DOUBLE_ENTRY(keyword, x, index, y)
#define CONST_KEYWORD_END(name) CONST_END(name)

CONST_KEYWORD_ARRAY(Untrusted_headers_HTTP)
	CONST_KEYWORD_ENTRY(NULL, FALSE)
	CONST_KEYWORD_ENTRY("Accept-Charset",TRUE)
	CONST_KEYWORD_ENTRY("Accept-Encoding",TRUE)
	//CONST_KEYWORD_ENTRY("Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Connection",TRUE)
	CONST_KEYWORD_ENTRY("Content-Length",TRUE)
	CONST_KEYWORD_ENTRY("Cookie",TRUE)
	CONST_KEYWORD_ENTRY("Cookie2",TRUE)
	CONST_KEYWORD_ENTRY("Date",TRUE)
	CONST_KEYWORD_ENTRY("Expect",TRUE)
	CONST_KEYWORD_ENTRY("Host",TRUE)
	//CONST_KEYWORD_ENTRY("If-Modified-Since",TRUE)
	//CONST_KEYWORD_ENTRY("If-None-Match",TRUE)
	CONST_KEYWORD_ENTRY("If-Range",TRUE)
	CONST_KEYWORD_ENTRY("Keep-Alive",TRUE)
	//CONST_KEYWORD_ENTRY("Proxy-Authorization",TRUE)
	CONST_KEYWORD_ENTRY("Proxy-Connection",TRUE)
	CONST_KEYWORD_ENTRY("Range",TRUE)
	CONST_KEYWORD_ENTRY("Referer",TRUE)
	CONST_KEYWORD_ENTRY("TE",TRUE)
	CONST_KEYWORD_ENTRY("Trailer",TRUE)
	CONST_KEYWORD_ENTRY("Transfer-Encoding",TRUE)
	CONST_KEYWORD_ENTRY("Upgrade",TRUE)
	CONST_KEYWORD_ENTRY("User-Agent",TRUE)
	CONST_KEYWORD_ENTRY("Via",TRUE)
CONST_KEYWORD_END(Untrusted_headers_HTTP)


URL_HTTP_ProtocolData::URL_HTTP_ProtocolData()
{
	InternalInit();
}

void URL_HTTP_ProtocolData::InternalInit()
{
	//sendinfo.connect_host = NULL;
	//sendinfo.connect_port = 0;
	//sendinfo.origin_host  = NULL;
	//sendinfo.origin_port  = 0;

	flags.http_method = HTTP_METHOD_GET;
	flags.http_data_new = FALSE;
	flags.http_data_with_header  = FALSE;
	flags.http10_or_more = FALSE;
	flags.auth_has_credetentials = FALSE;
	flags.auth_credetentials_used = FALSE;
	flags.auth_status = HTTP_AUTH_NOT;
	flags.proxy_auth_status = HTTP_AUTH_NOT;
	flags.connect_auth = FALSE;
	flags.only_if_modified = FALSE;
	flags.proxy_no_cache = FALSE;
	flags.must_revalidate = FALSE;
#ifdef HAS_SET_HTTP_DATA
	sendinfo.upload_data = NULL;
#endif
	flags.enable_accept_encoding = TRUE;
	flags.always_check_never_expired_queryURLs = TRUE;
	flags.checking_redirect = 0;

#ifdef CORS_SUPPORT
	flags.cors_redirects = 0;
#endif // CORS_SUPPORT
	flags.added_conditional_headers = FALSE;

#ifdef URL_ENABLE_HTTP_RANGE_SPEC
	sendinfo.range_start = FILE_LENGTH_NONE;
	sendinfo.range_end = FILE_LENGTH_NONE;
#endif

	flags.used_ua_id = g_uaManager->GetUABaseId();
	sendinfo.redirect_count=0;

	keepinfo.expires = 0;
	keepinfo.age = 0;

	recvinfo.refresh_int = -1;
	recvinfo.response = HTTP_NO_RESPONSE;

	recvinfo.all_headers = NULL;
	flags.header_loaded_sent = FALSE;
	flags.redirect_blocked = FALSE;
	flags.priority = URL_DEFAULT_PRIORITY;
	sendinfo.external_headers = NULL;
#ifdef WEB_TURBO_MODE
	sendinfo.use_proxy_passthrough = FALSE;
#endif // WEB_TURBO_MODE
	recvinfo.response = HTTP_NO_RESPONSE;
	authinfo = NULL;
#ifdef ABSTRACT_CERTIFICATES
	m_certificate = NULL;
#endif
#ifdef HTTP_CONTENT_USAGE_INDICATION
	flags.usage_indication = HTTP_UsageIndication_MainDocument;
#endif // HTTP_CONTENT_USAGE_INDICATION
}

URL_HTTP_ProtocolData::~URL_HTTP_ProtocolData()
{
	InternalDestruct();
}

void URL_HTTP_ProtocolData::InternalDestruct()
{
#ifdef HAS_SET_HTTP_DATA
	OP_DELETE(sendinfo.upload_data);
	sendinfo.upload_data = 0;
#endif
	OP_DELETE(authinfo);
	authinfo = 0;

#ifdef _SECURE_INFO_SUPPORT
	recvinfo.secure_session_info.UnsetURL();
#endif

	OP_DELETE(recvinfo.all_headers);
	recvinfo.all_headers = 0;
	OP_DELETE(sendinfo.external_headers);
	sendinfo.external_headers = NULL;
#ifdef ABSTRACT_CERTIFICATES
	OP_DELETE(m_certificate);
	m_certificate = NULL;
#endif
	ClearHTTPData();
}


void URL_HTTP_ProtocolData::ClearHTTPData()
{
	sendinfo.http_data.Empty();
}

uint32 URL_HTTP_ProtocolData::GetAttribute(URL::URL_Uint32Attribute attr) const
{
	switch(attr)
	{
	case URL::KCachePolicy_MustRevalidate:
		return flags.must_revalidate;
	case URL::KCachePolicy_AlwaysCheckNeverExpiredQueryURLs:
		return flags.always_check_never_expired_queryURLs;
	case URL::KHTTP_Age:
		return keepinfo.age;
	case URL::KHTTP_Method:
		return flags.http_method;
	case URL::KHTTP_Refresh_Interval:
		return recvinfo.refresh_int;
	case URL::KHTTP_10_or_more:
		return flags.http10_or_more;
	case URL::KHTTP_Response_Code:
		return recvinfo.response;
	case URL::KIsHttpRedirect:
		return IsRedirectResponse(recvinfo.response);
	case URL::KRedirectCount:
		return sendinfo.redirect_count;
	case URL::KSendAcceptEncoding:
		return flags.enable_accept_encoding;
	case URL::KHTTP_Priority:
		return flags.priority;
	case URL::KStillSameUserAgent:
		return ((UA_BaseStringId) flags.used_ua_id) == g_uaManager->GetUABaseId();
	case URL::KUsedUserAgentId:
		return flags.used_ua_id;
	case URL::KUsedUserAgentVersion:
		return 0; /* obsolete in core-2 */
#ifdef HTTP_CONTENT_USAGE_INDICATION
	case URL::KHTTP_ContentUsageIndication:
		return flags.usage_indication;
#endif
#ifdef CORS_SUPPORT
	case URL::KFollowCORSRedirectRules:
		return flags.cors_redirects;
#endif // CORS_SUPPORT
	}
	return 0;
}

void URL_HTTP_ProtocolData::SetAttribute(URL::URL_Uint32Attribute attr, uint32 value)
{
	BOOL flag = (value ? TRUE : FALSE);

	switch(attr)
	{
	case URL::KCachePolicy_MustRevalidate:
		flags.must_revalidate = flag;
		break;
	case URL::KCachePolicy_AlwaysCheckNeverExpiredQueryURLs:
		flags.always_check_never_expired_queryURLs = flag;
		break;
	case URL::KHTTP_Age:
		keepinfo.age = value;
		break;
	case URL::KHTTP_Method:
		flags.http_method = value;
		break;
	case URL::KHTTP_10_or_more:
		flags.http10_or_more = (unsigned short) value;
		break;
	case URL::KHTTP_Response_Code:
		recvinfo.response = (unsigned short) value;
		break;
	case URL::KRedirectCount:
		sendinfo.redirect_count = (short) value;
		break;
	case URL::KSendAcceptEncoding:
		flags.enable_accept_encoding = flag;
		break;
	case URL::KHTTP_Priority:
		flags.priority = value;
		break;
	case URL::KHTTP_Refresh_Interval:
		recvinfo.refresh_int = (int) value;
		break;
#ifdef HTTP_CONTENT_USAGE_INDICATION
	case URL::KHTTP_ContentUsageIndication:
		OP_ASSERT(value < HTTP_UsageIndication_max_value);
		flags.usage_indication = (HTTP_ContentUsageIndication) value;
		break;
#endif
#ifdef CORS_SUPPORT
	case URL::KFollowCORSRedirectRules:
		OP_ASSERT(value <= URL::CORSRedirectMaxValue);
		flags.cors_redirects = value;
		break;
#endif // CORS_SUPPORT
	}
}

const OpStringC8 URL_HTTP_ProtocolData::GetAttribute(URL::URL_StringAttribute attr) const
{
	switch(attr)
	{
	case URL::KHTTPUsername:
		return sendinfo.username;
	case URL::KHTTPPassword:
		return sendinfo.password;
	case URL::KHTTPContentLanguage:
		return recvinfo.content_language;
	case URL::KHTTPContentLocation:
		return recvinfo.content_location;
	case URL::KHTTP_ContentType:
		return sendinfo.http_content_type;
	case URL::KHTTP_Date:
		return keepinfo.load_date;
	case URL::KHTTP_EntityTag:
		return keepinfo.entitytag;
	case URL::KHTTPEncoding:
		return keepinfo.encoding;
	case URL::KHTTPFrameOptions:
		return keepinfo.frame_options;
	case URL::KHTTP_LastModified:
		return keepinfo.last_modified;
	case URL::KHTTP_Location:
		return recvinfo.location;
	case URL::KHTTPRefreshUrlName:
		return recvinfo.refresh_url;
	case URL::KHTTPResponseText:
		return recvinfo.response_text;
	}
	OpStringC8 empty_ret;
	return empty_ret;
}

OP_STATUS URL_HTTP_ProtocolData::GetAttribute(URL::URL_StringAttribute attr, OpString8 &val) const
{
	switch(attr)
	{
	case URL::KHTTPSpecificResponseHeaderL:
		// NOTE: val contains the requested header.
		if (recvinfo.all_headers)
		{
			HeaderEntry* cur_head = recvinfo.all_headers->GetHeader(val.CStr());
			if(cur_head)
				return val.Set(cur_head->GetValue());
			else
				val.Empty();
		}
		else
			val.Empty();
		break;

	case URL::KHTTPSpecificResponseHeadersL:
		// NOTE: val contains the concatenated values with
		// the (incoming) val as name. Separate by \r\n.
		if (recvinfo.all_headers)
		{
			OpString8 result;
			HeaderEntry* cur_head = recvinfo.all_headers->First();
			BOOL first = TRUE;
			while (cur_head)
			{
				if (val.CompareI(cur_head->GetName()) == 0)
				{
					if (!first)
						RETURN_IF_ERROR(result.Append("\r\n"));
					RETURN_IF_ERROR(result.Append(cur_head->GetValue()));
					first = FALSE;
				}
				cur_head = cur_head->Suc();
			}
			return val.Set(result);
		}
		else
			val.Empty();
		break;

	case URL::KHTTPAllResponseHeadersL:
		val.Empty();
		if (recvinfo.all_headers)
		{
			HeaderEntry* cur_head = recvinfo.all_headers->First();
			while (cur_head)
			{
				RETURN_IF_ERROR(val.Append(cur_head->GetName()));
				RETURN_IF_ERROR(val.Append(": "));
				RETURN_IF_ERROR(val.Append(cur_head->GetValue()));
				//find all subsequent headers with same name and append them with ", " to value according to WhatWG XmlHttpRequest specification.
				HeaderEntry* temp_head = (cur_head->GetName().CompareI("Set-Cookie") != 0 ?  cur_head : NULL); // Set-Cookie cannot be concatenated
				while (temp_head)
				{
					temp_head = recvinfo.all_headers->GetHeader(cur_head->Name(), temp_head);
					if (temp_head)
					{
						RETURN_IF_ERROR(val.Append(", "));
						RETURN_IF_ERROR(val.Append(temp_head->GetValue()));
						HeaderEntry* temp_next_head = temp_head->Pred();
						if(temp_next_head != cur_head)
						{
							temp_head->Out();
							temp_head->Follow(cur_head);
							temp_head = temp_next_head;
						}
						else
							cur_head = temp_head;
					}
				}
				RETURN_IF_ERROR(val.Append("\r\n"));
				cur_head = cur_head->Suc();
			}
			return val.Append("\r\n");
		}
		break;
	case URL::KHTTP_MovedToURL_Name:
		{
			if(recvinfo.moved_to_url.IsValid())
				return recvinfo.moved_to_url.GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI, val); // Needed for referencing // But should redirect to password be forgotten, too?
			else
				return val.Set(recvinfo.moved_to_url_name);
			break;
		}
	default:
		return val.Set(GetAttribute(attr));
		break;
	}

	return OpStatus::OK;
}

OP_STATUS URL_HTTP_ProtocolData::SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	OpStringS8 *target = NULL;

	switch(attr)
	{
	case URL::KHTTPContentLanguage:
		target  = &recvinfo.content_language;
		break;
	case URL::KHTTPContentLocation:
		target  = &recvinfo.content_location;
		break;
	case URL::KHTTP_ContentType:
		target  = &sendinfo.http_content_type;
		break;;
	case URL::KHTTP_Date:
		target = &keepinfo.load_date;
		break;
	case URL::KHTTPEncoding:
		target  = &keepinfo.encoding;
		break;
	case URL::KHTTP_EntityTag:
		target  = &keepinfo.entitytag;
		break;
	case URL::KHTTPFrameOptions:
		target = &keepinfo.frame_options;
		break;
	case URL::KHTTP_LastModified:
		target = &keepinfo.last_modified;
		break;
	case URL::KHTTP_Location:
#if URL_REDIRECTION_LENGTH_LIMIT >0
		if(string.Length() > URL_REDIRECTION_LENGTH_LIMIT)
		{
			recvinfo.location.Empty();
			return OpStatus::OK;
		}
#endif
		target = &recvinfo.location;
		break;
	case URL::KHTTP_MovedToURL_Name:
		recvinfo.moved_to_url = URL();
		target = &recvinfo.moved_to_url_name;
		break;
	case URL::KHTTPRefreshUrlName:
		target  = &recvinfo.refresh_url;
		break;
	case URL::KHTTPResponseText:
		target  = &recvinfo.response_text;
		break;
	case URL::KHTTPUsername:
		target = &sendinfo.username;
		if (string.CStr())
		{
			flags.auth_has_credetentials = TRUE;
			flags.auth_credetentials_used = FALSE;
		}
		break;

	case URL::KHTTPPassword:
		target = &sendinfo.password;
		if (string.CStr())
		{
			flags.auth_has_credetentials = TRUE;
			flags.auth_credetentials_used = FALSE;
		}
		break;
	}

	if(target)
		return target->Set(string);

	return OpStatus::OK;
}

const OpStringC URL_HTTP_ProtocolData::GetAttribute(URL::URL_UniStringAttribute attr) const
{
	switch(attr)
	{
	case URL::KHTTP_SuggestedFileName:
	case URL::KSuggestedFileName_L:
		return recvinfo.suggested_filename;
	}
	OpStringC empty_ret;
	return empty_ret;
}

OP_STATUS URL_HTTP_ProtocolData::GetAttribute(URL::URL_UniStringAttribute attr, OpString &val) const
{
	return val.Set(GetAttribute(attr));
}

OP_STATUS URL_HTTP_ProtocolData::SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string)
{
	switch(attr)
	{
	case URL::KHTTP_SuggestedFileName:
	case URL::KSuggestedFileName_L:
		return recvinfo.suggested_filename.Set(string);
	}

	return OpStatus::OK;
}

const void *URL_HTTP_ProtocolData::GetAttribute(URL::URL_VoidPAttribute  attr, const void *param) const
{
	switch(attr)
	{
	case URL::KHTTPLinkRelations:
		return recvinfo.link_relations.First();
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
	case URL::KHTTPRangeEnd:
		if(param)
				*((OpFileLength *) param) = sendinfo.range_end;
		break;
	case URL::KHTTPRangeStart:
		if(param)
			*((OpFileLength *) param) = sendinfo.range_start;
		break;
#endif
	case URL::KVLastModified:
		if(!param)
			return NULL;

		*((time_t *) param) = 0;
		if(keepinfo.last_modified.HasContent())
			*((time_t *) param) = GetDate(keepinfo.last_modified.CStr());
		return param;
	case URL::KVHTTP_ExpirationDate:
		if(!param)
			return NULL;

		*((time_t *) param) = keepinfo.expires;
		return param;
#ifdef ABSTRACT_CERTIFICATES
	case URL::KRequestedCertificate:
		return m_certificate;
#endif
	case URL::KHTTP_Upload_TotalBytes:
		if(!param)
			return NULL;

		*((OpFileLength *) param) = sendinfo.upload_data != NULL ? sendinfo.upload_data->PayloadLength() : 0;
		return param;
	}

	return NULL;
}

OP_STATUS URL_HTTP_ProtocolData::SetAttribute(URL::URL_VoidPAttribute  attr, const void *param)
{
	switch(attr)
	{
	case URL::KVHTTP_ExpirationDate:
		keepinfo.expires = (param ? *((time_t *) param) : 0);
		break;
	case URL::KAddHTTPHeader:
		if (param)
		{
			const URL_Custom_Header *header = (const URL_Custom_Header *) param;
			if(header->name.IsEmpty() || header->value.IsEmpty())
				return OpStatus::OK;

			if(header->name.FindFirstOf(":\r\n \t") != KNotFound)
				return OpStatus::OK;

			if(header->value.FindFirstOf("\r\n") != KNotFound)
				return OpStatus::OK;

			if(header->bypass_security || (flags.http_method ==HTTP_METHOD_SEARCH && header->name.CompareI("Range") == 0) || // Range header update supported for the SEARCH method, currently no syntax limitation
				(!CheckKeywordsIndex(header->name.CStr(), g_Untrusted_headers_HTTP, CONST_ARRAY_SIZE(url,Untrusted_headers_HTTP)) &&
					header->name.CompareI("Proxy-", STRINGLENGTH("Proxy-")) != 0 &&
					header->name.CompareI("Sec-", STRINGLENGTH("Sec-")) != 0)
				)
			{
				//add the new header
				if(!sendinfo.external_headers)
				{
					sendinfo.external_headers = OP_NEW(Header_List, ());
					if(!sendinfo.external_headers)
						return OpStatus::ERR_NO_MEMORY;
				}

				if(sendinfo.external_headers)
				{
					Header_Item *item;
					if (sendinfo.upload_data && header->name.CompareI("Content-Type") == 0)
					{
						item = sendinfo.upload_data->Headers.FindHeader("Content-Type", FALSE);
						if(item)
						{
							item->Out();
							OP_DELETE(item);
						}
					}
                    OpAutoPtr<Header_Item> new_item(OP_NEW(Header_Item, (SEPARATOR_SEMICOLON, TRUE)));
					if (new_item.get() == NULL)
						return OpStatus::ERR_NO_MEMORY;

                    TRAP_AND_RETURN(op_err, new_item->InitL(header->name));
                    TRAP_AND_RETURN(op_err, new_item->AddParameterL(header->value));
                    sendinfo.external_headers->InsertHeader(new_item.get());
                    new_item.release();
				}
			}
		}
		else
		{
			//not allowed to add headers according to condition.
			OP_ASSERT(0);
		}
		break;
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
	case URL::KHTTPRangeEnd:
		sendinfo.range_end = (param	? *((OpFileLength *) param): 0) ;
		break;
	case URL::KHTTPRangeStart:
		sendinfo.range_start = (param	? *((OpFileLength *) param): 0) ;
		break;
#endif
#ifdef ABSTRACT_CERTIFICATES
	case URL::KRequestedCertificate:
		OP_DELETE(m_certificate);
		m_certificate = (OpCertificate*) param;
		break;
#endif
	}

	return OpStatus::OK;
}

URL URL_HTTP_ProtocolData::GetAttribute(URL::URL_URLAttribute  attr, URL_Rep *url)
{
	switch(attr)
	{
	case URL::KMovedToURL:
		return MovedToUrl(url);
	case URL::KHTTPContentLocationURL:
		{
			if(recvinfo.content_location.IsEmpty())
				break;

			URL this_url(url, (const char*)NULL);
			URL ret;

			ret = urlManager->GetURL(this_url, recvinfo.content_location);

			if(!url->GetAttribute(URL::KIsGeneratedByOpera) && ret.IsValid() &&
				((URLType) ret.GetAttribute(URL::KType) != (URLType) this_url.GetAttribute(URL::KType) ||
				(ServerName *) ret.GetAttribute(URL::KServerName, (const void*) 0) != (ServerName *) this_url.GetAttribute(URL::KServerName, (const void*) 0) ||
				ret.GetAttribute(URL::KServerPort) != this_url.GetAttribute(URL::KServerPort)))
			{
				recvinfo.content_location.Empty();
				break;
			}

			return ret;
		}
	case URL::KReferrerURL:
		return sendinfo.referer_url;
#ifdef _SECURE_INFO_SUPPORT
	case URL::KSecurityInformationURL:
		return recvinfo.secure_session_info;
#endif
	}

	return URL();
}

OP_STATUS URL_HTTP_ProtocolData::SetAttribute(URL::URL_URLAttribute  attr, const URL &param)
{
	switch(attr)
	{
	case URL::KMovedToURL:
		recvinfo.moved_to_url_name.Empty();
		recvinfo.moved_to_url = param;
		break;
	case URL::KReferrerURL:
		sendinfo.referer_url = param;
		break;
#ifdef _SECURE_INFO_SUPPORT
	case URL::KSecurityInformationURL:
		{
			URL param2(param);
			recvinfo.secure_session_info.SetURL(param2);
		}
		break;
#endif
	}
	return OpStatus::OK;
}



#ifdef _OPERA_DEBUG_DOC_
unsigned long URL_HTTP_ProtocolData::GetMemUsed()
{
	unsigned long len = sizeof(*this);

	len += sendinfo.http_content_type.Length();
	len += sendinfo.http_data.Length();
	len += UNICODE_SIZE(sendinfo.proxyname.Length());
	// Upload and autoproxy not included

	len += keepinfo.load_date.Length();
	len += keepinfo.last_modified.Length();
	//len += keepinfo.mime_type.Length();
	len += keepinfo.encoding.Length();
	len += keepinfo.entitytag.Length();

	len += recvinfo.moved_to_url_name.Length();
	len += recvinfo.location.Length();
	len += recvinfo.refresh_url.Length();
	len += recvinfo.response_text.Length();
	len += recvinfo.suggested_filename.Length();
	// some member not included

	if(authinfo)
	{
		len += sizeof(*authinfo);
	}

	return len;
}
#endif

URL URL_HTTP_ProtocolData::MovedToUrl(URL_Rep *url)
{
	if (flags.redirect_blocked)
		return URL();
	if (recvinfo.moved_to_url.IsEmpty() && recvinfo.moved_to_url_name.HasContent())
	{
		char *tmp = recvinfo.moved_to_url_name.CStr();
		while (*tmp != '\0' && *tmp != '#')
			tmp++;

		char *href_rel = 0;
		if (*tmp == '#')
		{
			*tmp = '\0';
			if (*(tmp+1) != '\0')
			{
				href_rel = tmp+1;
			}
		}
		else
			tmp = 0;

		URL tmp_url(url, (char *) NULL);
		recvinfo.moved_to_url = urlManager->GetURL(tmp_url, recvinfo.moved_to_url_name, OpStringC8(href_rel));
		if (tmp)
			*tmp = '#';

		if (recvinfo.moved_to_url.GetRep() == url)
		{
			OP_ASSERT(!"Opera has allowed the url to redirect to itself. This must never be allowed, instead an error message should have shown.");

			// We set the redirect URL to empty to prevent stack overflow crash that will happen if we do not break it of here.
			flags.redirect_blocked = TRUE;
			recvinfo.moved_to_url_name.Empty();
			recvinfo.moved_to_url = URL();
		}

		//OP_ASSERT(!recvinfo.moved_to_url->GetRep() || recvinfo.moved_to_url->GetRep() != url);
		//recvinfo.moved_to_url = new URL(urlManager->GetURL(recvinfo.moved_to_url_name));
		recvinfo.moved_to_url_name.Empty();
	}
	return recvinfo.moved_to_url;
}

BOOL URL_HTTP_ProtocolData::CheckAuthData()
{
	if(!authinfo)
		authinfo = OP_NEW(authdata_st, ());

	if (authinfo && authinfo->auth_headers)
		return TRUE;

	OP_DELETE(authinfo);
	authinfo = 0;

	return FALSE;
}

URL_Mailto_ProtocolData::URL_Mailto_ProtocolData()
{
#ifdef HAS_SET_HTTP_DATA
	upload_data = NULL;
#endif
}

URL_Mailto_ProtocolData::~URL_Mailto_ProtocolData()
{
#ifdef HAS_SET_HTTP_DATA
	OP_DELETE(upload_data);
#endif
}


HTTP_Link_Relations::HTTP_Link_Relations()
{
}

HTTP_Link_Relations::~HTTP_Link_Relations()
{
}

void HTTP_Link_Relations::Clear()
{
	original_string.Empty();
	link_uri.Empty();
	parameters.Clear();

}

void HTTP_Link_Relations::InitL(OpStringC8 link_header)
{
	Clear();

	original_string.SetL(link_header);

	InitInternalL();
}

#ifdef DISK_CACHE_SUPPORT
void HTTP_Link_Relations::InitL(DataFile_Record *link_record)
{
	Clear();

	if(link_record == NULL)
		return;

	link_record->GetStringL(original_string);

	InitInternalL();
}
#endif // DISK_CACHE_SUPPORT

void HTTP_Link_Relations::InitInternalL()
{
	const char *uri_start = NULL;
	const char *pos = original_string.CStr();

	if(!pos)
		return;

	if(*pos == '<')
	{
		pos ++;
		uri_start = pos;
		while(*pos != '\0' && *pos != '>')
			pos++;

		if(*pos)
		{
			link_uri.SetL(uri_start, pos-uri_start);
			pos++;
			while(*pos != '\0' && *pos != ';')
				pos++;
			pos++;
		}
		else
			pos = original_string.CStr();
	}

	parameters.SetValueL(pos, PARAM_COPY_CONTENT | PARAM_SEP_SEMICOLON | PARAM_ONLY_SEP |PARAM_STRIP_ARG_QUOTES);
}


const OpStringC8 URL_FTP_ProtocolData::GetAttribute(URL::URL_StringAttribute attr) const
{
	switch(attr)
	{
	case URL::KFTP_MDTM_Date:
		return MDTM_date;
	}
	OpStringC8 tmpRet;
	return tmpRet;
}


OP_STATUS URL_FTP_ProtocolData::SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	switch(attr)
	{
	case URL::KFTP_MDTM_Date:
		return MDTM_date.Set(string);
	}

	return OpStatus::OK;
}


#if 0
const OpStringC8 URL_Mailto_ProtocolData::GetAttribute(URL::URL_StringAttribute attr) const
{
	switch(attr)
	{
	}
	OpStringC8 tmpRet;
	return tmpRet;
}
#endif

#if 0
OP_STATUS URL_Mailto_ProtocolData::SetAttribute(URL::URL_StringAttribute attr, const OpStringC8 &string)
{
	OpStringS8 *target = NULL;

	switch(attr)
	{
	}

	if(target)
		return target->Set(string);

	return OpStatus::OK;
}
#endif

#ifdef HAS_SET_HTTP_DATA
const void *URL_Mailto_ProtocolData::GetAttribute(URL::URL_VoidPAttribute attr, const void *param) const
{
	switch(attr)
	{
	case URL::KMailTo_UploadData:
		return upload_data;
	}
	return NULL;
}
#endif

#ifdef _MIME_SUPPORT_
#ifdef NEED_URL_MIME_DECODE_LISTENERS
uint32 URL_MIME_ProtocolData::GetAttribute(URL::URL_Uint32Attribute attr) const
{
	switch(attr)
	{
#ifdef NEED_URL_MIME_DECODE_LISTENERS
	case URL::KMIME_HasAttachmentListener:
		{
			OpListeners<MIMEDecodeListener>& listeners = const_cast<OpListeners<MIMEDecodeListener>&>(m_mimedecode_listeners);
			OpListenersIterator iterator(listeners);
			return listeners.HasNext(iterator);
		}
#endif
	}
	return 0;
}
#endif

#if 0
void URL_MIME_ProtocolData::SetAttribute(URL::URL_Uint32Attribute attr, uint32 value)
{
	switch(attr)
	{
	}
}
#endif

const OpStringC URL_MIME_ProtocolData::GetAttribute(URL::URL_UniStringAttribute attr) const
{
	switch(attr)
	{
	case URL::KBodyCommentString:
	case URL::KBodyCommentString_Append:
		return body_comment_string;
	}
	OpStringC empty_ret;
	return empty_ret;
}


OP_STATUS URL_MIME_ProtocolData::SetAttribute(URL::URL_UniStringAttribute attr, const OpStringC &string)
{
	switch(attr)
	{
	case URL::KBodyCommentString:
		body_comment_string.Empty();
		// fall-through
	case URL::KBodyCommentString_Append:
		RETURN_IF_ERROR(body_comment_string.Append(string));
		if (body_comment_string.HasContent())
			RETURN_IF_ERROR(body_comment_string.Append(UNI_L("\r\n")));
		break;
	}
	return OpStatus::OK;
}

URL URL_MIME_ProtocolData::GetAttribute(URL::URL_URLAttribute  attr)
{
	switch(attr)
	{
	case URL::KBaseAliasURL:
		return content_base_location_alias;
	case URL::KAliasURL:
		return content_location_alias;
	}
	URL empty_ret;
	return empty_ret;
}

OP_STATUS URL_MIME_ProtocolData::SetAttribute(URL::URL_URLAttribute  attr, const URL &param)
{
	switch(attr)
	{
	case URL::KBaseAliasURL:
		content_base_location_alias = param;
		break;
	case URL::KAliasURL:
		content_location_alias = param;
		break;

#if defined(NEED_URL_MIME_DECODE_LISTENERS)
	case URL::KMIME_SignalAttachmentListeners:
		{
			URL attachment(param);

			for (OpListenersIterator iterator(m_mimedecode_listeners); m_mimedecode_listeners.HasNext(iterator);)
			{
				m_mimedecode_listeners.GetNext(iterator)->OnMessageAttachmentReady(attachment);
			}
		}
		break;
#endif
	}

	return OpStatus::OK;
}

#if defined(NEED_URL_MIME_DECODE_LISTENERS)
OP_STATUS URL_MIME_ProtocolData::SetAttribute(URL::URL_VoidPAttribute  attr, const void *param)
{
	switch (attr)
	{
	case URL::KMIME_AddAttachmentListener:
		return m_mimedecode_listeners.Add((MIMEDecodeListener *) param);
	case URL::KMIME_RemoveAttachmentListener:
		return m_mimedecode_listeners.Remove((MIMEDecodeListener *) param);
	}
	return OpStatus::OK;
}
#endif
#endif // _MIME_SUPPORT_
