/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2008-2012
 *
 * Web server implementation 
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/url/url2.h"
#include "modules/forms/form.h"
#include "modules/webserver/webserver_openid.h"
#include "modules/windowcommander/src/TransferManager.h"
#include "modules/webserver/webserver_request.h"
#include "modules/formats/uri_escape.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/regexp/include/regexp_api.h"

WebserverAuthenticationId_OpenId::WebserverAuthenticationId_OpenId()
	: m_state(NONE)	 
	, m_transferItem(NULL)
	, m_request_object(NULL)
	, m_contacting_id_server(FALSE)
{
}

WebserverAuthenticationId_OpenId::~WebserverAuthenticationId_OpenId()
{
	if (m_transferItem)
	{
		m_transferItem->SetTransferListener(NULL);
		g_transferManager->ReleaseTransferItem(m_transferItem);		
	}	
}
 
 
 

/* static */ OP_STATUS WebserverAuthenticationId_OpenId::Make(WebserverAuthenticationId_OpenId *&auhtentication_object, const OpStringC &auth_id)
{
	auhtentication_object = NULL;
	OpAutoPtr<WebserverAuthenticationId_OpenId> temp(OP_NEW(WebserverAuthenticationId_OpenId, ()));
	RETURN_OOM_IF_NULL(temp.get());

	RETURN_IF_ERROR(temp->SetAuthId(auth_id.CStr()));
	temp->m_timer.SetTimerListener(temp.get()); 
	
	auhtentication_object = temp.release();
	return OpStatus::OK;
}

/* static */ OP_STATUS WebserverAuthenticationId_OpenId::MakeFromXML(WebserverAuthenticationId_OpenId *&auhtentication_object, XMLFragment &auth_element)
{
	auhtentication_object = NULL;
	OpAutoPtr<WebserverAuthenticationId_OpenId> temp(OP_NEW(WebserverAuthenticationId_OpenId, ()));
	RETURN_OOM_IF_NULL(temp.get());

	temp->m_timer.SetTimerListener(temp.get()); 
	
	auhtentication_object = temp.release();

	return OpStatus::OK;
}

/* virtual */ OP_STATUS WebserverAuthenticationId_OpenId::CheckAuthentication(WebserverRequest *request_object)
{
	m_request_object = request_object;
	if (m_contacting_id_server == TRUE)
	{
		m_state = NONE;
		return CreateSessionAndSendCallback(SERVER_AUTH_STATE_ANOTHER_AGENT_IS_AUTHENTICATING);
	}
	
	if (request_object->GetHttpMethod() == WEB_METH_POST)
	{
		m_openid_return_to_path.Wipe();
		m_state = NONE;		
		return CheckAuthenticationStep1(request_object);
	}
	else
	{
		return CheckAuthenticationStep2(request_object);
	}
}

OP_STATUS WebserverAuthenticationId_OpenId::CheckAuthenticationStep1(WebserverRequest *request_object)
{
	/* FIXME: have proper state handling */
	OP_ASSERT(m_state == NONE);
	m_state = REDIRECT_OPENID_SERVER;
	
	OpString open_id_uri;
	RETURN_IF_ERROR(open_id_uri.Set("http://"));
	RETURN_IF_ERROR(open_id_uri.Append(GetAuthId()));
	unsigned int index;
	request_object->GetItemInQuery(UNI_L("uniteauth.path"), m_openid_return_to_path, index = 0, FALSE);
	
	RETURN_IF_ERROR(SendRequest(HTTP_METHOD_POST, open_id_uri));
	return OpStatus::OK;
}

OP_STATUS WebserverAuthenticationId_OpenId::CheckAuthenticationStep2(WebserverRequest *request_object)
{
	if (m_state != REDIRECT_OPENID_SERVER)
	{
		m_state = NONE;
		return CreateSessionAndSendCallback(SERVER_AUTH_STATE_ANOTHER_AGENT_IS_AUTHENTICATING);
	}
	
	/* FIXME: have proper state handling */
	m_state = CHECK_WITH_OPENID_SERVER;
	
	
	
	OP_ASSERT(m_auth_resource != NULL);
	OpString openid_signed;
	OpString openid_assoc_handle;
	OpString openid_sig;

	unsigned int index = 0;
	RETURN_IF_ERROR(request_object->GetItemInQuery(UNI_L("openid.signed"), openid_signed, index = 0, FALSE));
	RETURN_IF_ERROR(request_object->GetItemInQuery(UNI_L("openid.assoc_handle"), openid_assoc_handle, index = 0, FALSE));
	RETURN_IF_ERROR(request_object->GetItemInQuery(UNI_L("openid.sig"), openid_sig, index = 0, FALSE));

	OpString m_openid_url;
	
	
	/* FIXME : check that all strings are valid */
	RETURN_IF_ERROR(m_openid_url.AppendFormat(UNI_L("%s?openid.mode=check_authentication&openid.signed=%s&openid.assoc_handle=%s&openid.sig=%s"), m_openid_server.CStr(), openid_signed.CStr(), openid_assoc_handle.CStr(), openid_sig.CStr()));
	
	OpAutoStringHashTable<OpAutoStringVector> item_collection;
	RETURN_IF_ERROR(request_object->GetAllItemsInQuery(item_collection, FALSE));	
	BOOL more_signed = TRUE;
	
	/* FIXME : url decode openid_signed */
	
	uni_char *s =  openid_signed.CStr();
	
#ifdef FORMATS_URI_ESCAPE_SUPPORT
	UriUnescape::ReplaceChars(s, UriUnescape::NonCtrlAndEsc);
#else
	int slen = uni_strlen(s);
	
	ReplaceEscapedChars(s, slen);
	s[slen] = '\0';
#endif

	
	const uni_char *c;
	OpString item_name;

	// Adds all arguments (except mode signed, assoc_handle and sig) found in openid.signed given by the openid provider.	 
	while (more_signed == TRUE)
	{
		int slen;
		if ((c = uni_strchr(s,',')))
		{
			slen = c - s;
		}
		else
		{
			more_signed = FALSE;
			slen = uni_strlen(s);
		}
		
		RETURN_IF_ERROR(item_name.Set("openid."));
		RETURN_IF_ERROR(item_name.Append(s, slen));

		if (
				item_name.CompareI(UNI_L("openid.mode")) &&
				item_name.CompareI(UNI_L("openid.signed")) &&
				item_name.CompareI(UNI_L("openid.assoc_handle")) &&
				item_name.CompareI(UNI_L("openid.sig"))
			)
		{
			OpAutoStringVector *item_vector;
			if (OpStatus::IsSuccess(item_collection.GetData(item_name.CStr(), &item_vector)))
			{
				OpString *item_value = item_vector->Get(0);
				if (item_value && item_value->HasContent())
				{			
					m_openid_url.AppendFormat(UNI_L("&%s=%s"), item_vector->GetVectorName(), item_value->CStr());
				}
			}
		}	
		
		s += slen + 1;
	}

	
	RETURN_IF_ERROR(SendRequest(HTTP_METHOD_POST, m_openid_url));
	
	return OpStatus::OK;	
}
	
/* virtual */ OP_STATUS WebserverAuthenticationId_OpenId::CreateAuthXML(XMLFragment &auth_element)
{
	/* Currently nothing is needed here, since the openid itself is saved by the superclass WebserverAuthenticationId. */
	return OpStatus::OK;
}

	// OpTransferListener
/* virtual */ void WebserverAuthenticationId_OpenId::OnProgress(OpTransferItem* transferItem, TransferStatus status)
{
	switch (status) // Switch on the various status messages.
	{
		case TRANSFER_DONE:
		{
			m_contacting_id_server = FALSE;

			URL* url = transferItem->GetURL();
			if (url)
			{
				OpString data;
				BOOL more = TRUE;
				OpAutoPtr<URL_DataDescriptor> url_data_desc(url->GetDescriptor(NULL, URL::KFollowRedirect));

				if(!url_data_desc.get())
				{
					more = FALSE;
				}
				
				while (more)
				{
					TRAPD(err, url_data_desc->RetrieveDataL(more));
					if (OpStatus::IsError(err))
					{
						if (OpStatus::IsMemoryError(err))
						{
							/* FIXME: handle memory error */
						}
						break;
					}
					
					if (OpStatus::IsError(data.Append((uni_char *)url_data_desc->GetBuffer(), UNICODE_DOWNSIZE(url_data_desc->GetBufSize()))))
					{
						/* FIXME: handle memory error */
						break;
					}
					url_data_desc->ConsumeData(UNICODE_SIZE(UNICODE_DOWNSIZE(url_data_desc->GetBufSize())));
				}
				
				if (m_auth_resource && data.HasContent())
				{
					if (m_state == CHECK_WITH_OPENID_SERVER)
					{
						AuthState is_authenticated =  uni_strni_eq(data.CStr(), UNI_L("is_valid:true"), 13) ?  SERVER_AUTH_STATE_SUCCESS : SERVER_AUTH_STATE_FAILED;
						m_state = NONE;
						OpStatus::Ignore(CreateSessionAndSendCallback(is_authenticated));


					}
					else if (m_state == REDIRECT_OPENID_SERVER)
					{
						OpStatus::Ignore(RedirectToOpenidProvider(data));
					}
				}
			}
			
			break;
		}
		
		case TRANSFER_PROGRESS:
		{
			m_contacting_id_server = TRUE;
			return;
		}
		
		case TRANSFER_ABORTED:
		case TRANSFER_FAILED:
		default:
		{
			m_contacting_id_server = FALSE;
			break;
		}
	}	
	
}

/* virtual */ void WebserverAuthenticationId_OpenId::OnReset(OpTransferItem* transferItem)
{
	
	
}

/* virtual */ void WebserverAuthenticationId_OpenId::OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to)
{
	
	
}

	
/* virtual */ void WebserverAuthenticationId_OpenId::OnTimeOut(OpTimer* timer)
{
	if (m_contacting_id_server == TRUE) /* If we got a timeout while in progress, this was a upload timeout. The upload is restarted. */
	{
		m_contacting_id_server = FALSE;

		URL *url = m_transferItem->GetURL();
		if(url && url->GetAttribute(URL::KLoadStatus) == URL_LOADING)
		{
			url->StopLoading(g_main_message_handler);

			m_transferItem->SetTransferListener(NULL);
			g_transferManager->ReleaseTransferItem(m_transferItem);
			m_transferItem = NULL;
		}

		m_state = NONE;	
		OpStatus::Ignore(CreateSessionAndSendCallback(SERVER_AUTH_STATE_AUTH_SERVER_ERROR));
	}	
	
}

OP_STATUS WebserverAuthenticationId_OpenId::SendRequest(HTTP_Method method, const OpStringC &url)
{
 	OpTransferItem *found = 0;
	URL *server_url = NULL;

	OP_STATUS status;
	g_transferManager->GetTransferItem(&found, url.CStr());

	if (found)
	{
		m_transferItem = found;
		server_url = m_transferItem->GetURL();
		status = OpStatus::ERR_NO_MEMORY;
		if (server_url == NULL || OpStatus::IsError(status = server_url->SetHTTP_Data("", TRUE)) ||
				OpStatus::IsError(status = server_url->SetHTTP_ContentType(FormUrlEncodedString)))
		{
			m_transferItem->SetTransferListener(NULL);
			g_transferManager->ReleaseTransferItem(m_transferItem);
			m_transferItem = NULL;
			return status;
		}
		
		server_url->SetHTTP_Method(method);
		
		m_transferItem->SetTransferListener(this);

		URL tmp;
		server_url->Load(g_main_message_handler, tmp, TRUE, FALSE); /* Error handling? */
	
		m_timer.Start(30000); /* We set a timeout for this upload */
		m_contacting_id_server = TRUE;
		
	}	

	return OpStatus::OK;
}

OP_STATUS WebserverAuthenticationId_OpenId::RedirectToOpenidProvider(const OpStringC &openid_provider_xml)
{
	m_openid_server.Wipe();

	/* We parse the html page from the openid provider to get the openid.server from the tag <link rel="openid.server" href="http://some.server.com/a/path" /> */ 
	OpRegExp *regexp;
	OpREFlags flags;
	flags.multi_line = TRUE;
	flags.case_insensitive = TRUE;
	flags.ignore_whitespace = TRUE;
	
	RETURN_IF_ERROR(OpRegExp::CreateRegExp(&regexp, UNI_L("<\\s*?link\\s.*?rel\\s*?=\\s*?\"\\s*?openid.server\".*?/\\s*?>"), &flags));
	OpAutoPtr<OpRegExp> regexp_deleter(regexp);
	
	OpREMatchLoc location;
	RETURN_IF_ERROR(regexp->Match(openid_provider_xml.CStr(), &location));

	if (location.matchloc != REGEXP_NO_MATCH && location.matchlen != REGEXP_NO_MATCH)
	{
		const uni_char *end = openid_provider_xml.CStr() + location.matchloc + location.matchlen;
		uni_char *pos = uni_strstr(openid_provider_xml.CStr() + location.matchloc, UNI_L("href"));
		if (pos && pos < end)
		{
			pos += 4; 
			while (*pos == ' ' ) pos++;
			while (*pos == '=' ) pos++;
			while (*pos == ' ') pos++;
			while (*pos == '\"') pos++;
			while (*pos == '\'') pos++;
			while (*pos == ' ') pos++;
			
			const uni_char *end_str = uni_strchr(pos, '\"');
			if (end_str && end_str < end)
			{
				RETURN_IF_ERROR(m_openid_server.Set(pos, end_str - pos));
			}
		}
	}
		
		
	if (m_openid_server.HasContent())
	{
		HeaderList *header_list = m_request_object->GetClientHeaderList();
		
		HeaderEntry *host_header = header_list->GetHeader("host");
		
		if (host_header)
		{
			OpString escape_persent;
			
			
	#ifdef FORMATS_URI_ESCAPE_SUPPORT
			RETURN_IF_ERROR(UriEscape::AppendEscaped(escape_persent, m_openid_return_to_path.CStr(), UriEscape::Percent));
	#else
			RETURN_IF_ERROR(WebResource::EscapeItemSplitters(m_openid_return_to_path, UNI_L("%"), escape_persent));
	#endif
			
			OpString host;
			RETURN_IF_ERROR(host.SetFromUTF8(host_header->Value()));
			OpString location;

			RETURN_IF_ERROR(location.AppendFormat(UNI_L("%s?openid.mode=checkid_setup&openid.identity=%s&openid.return_to=http://%s/_root/authresource/?uniteauth.type%%3D%s%%26uniteauth.path%%3D%s"), m_openid_server.CStr(), GetAuthId(), host.CStr(), WebserverAuthenticationId::AuthTypeToString(WEB_AUTH_TYPE_HTTP_OPEN_ID), escape_persent.CStr()));
					
			m_auth_resource->AddResponseHeader(UNI_L("location"), location);
			m_auth_resource->SetResponseCode(302);
			m_auth_resource->StartServingData();
			return OpStatus::OK;
		}
	}
	
	m_state = NONE;	
	RETURN_IF_ERROR(CreateSessionAndSendCallback(SERVER_AUTH_STATE_FAILED));

	return OpStatus::ERR;
}
	

#endif // WEBSERVER_SUPPORT
