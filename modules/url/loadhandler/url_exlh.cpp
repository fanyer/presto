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

#ifdef _URL_EXTERNAL_LOAD_ENABLED_

#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/loadhandler/url_exlh.h"
#include "modules/upload/upload.h"
#include "modules/url/url_man.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/olddebug/timing.h"

#ifndef _NO_GLOBALS_
extern URL_Manager* urlManager;
#endif

URL_External_Request_Handler::URL_External_Request_Handler(MessageHandler* msg_handler, URL_External_Request_Manager *mngr)
	: ProtocolComm(msg_handler, NULL,NULL)
{
	manager = mngr;
	url = NULL;
	http = NULL;
	upload_elm = NULL;
	header_string = NULL;
	waiting = FALSE;
}

URL_External_Request_Handler::~URL_External_Request_Handler()
{
	if(InList())
		Out();
	DeleteComm();
	OP_DELETE(upload_elm);
	upload_elm = NULL;
	OP_DELETEA(header_string);
	mh->UnsetCallBacks(this);
}

void URL_External_Request_Handler::DeleteComm()
{
	if(http)
	{
		g_main_message_handler->RemoveCallBacks(this, http->Id());
		g_main_message_handler->UnsetCallBacks(http);

		http->EndLoading();
		SafeDestruction(http);
		http = 0;
	}
}

void URL_External_Request_Handler::EndLoading()
{
	if(http)
		http->EndLoading();
}


const KeywordIndex http_meth_keyword[] = {
	{NULL, HTTP_METHOD_MAX+1},
	{"DELETE", HTTP_METHOD_DELETE},
	{"GET", HTTP_METHOD_GET},
	{"HEAD", HTTP_METHOD_HEAD},
	{"OPTIONS", HTTP_METHOD_OPTIONS},
	{"POST", HTTP_METHOD_POST},
	{"PUT", HTTP_METHOD_PUT},
	{"TRACE", HTTP_METHOD_TRACE}
};

const KeywordIndex http_untrusted_extra_types[] = {
	{NULL, FALSE},
	{"User-Agent",TRUE},
	{"Expect",TRUE},
	{"Cookie",TRUE}
};

CommState URL_External_Request_Handler::Load(
#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
	const uni_char *network_link_name,
#endif
	const char *header, unsigned long header_len, unsigned long body_len,
	const uni_char *proxy, BOOL override_proxy_prefs)

{
	if(header == NULL || header_len == 0 || header[header_len-1] != '\0' || 
		header[0] == '\0' || op_isspace((unsigned char) header[0]) )
		return COMM_REQUEST_FAILED;

	if(http)
		return COMM_LOADING;

	OpString8 url_req;
	
	int meth_str_len = strcspn(header, " \t\n\r");

	HTTP_Method method = (HTTP_Method) CheckKeywordsIndex(header, meth_str_len, http_meth_keyword, ARRAY_SIZE(http_meth_keyword));

	if(method == HTTP_METHOD_MAX+1 ||
		(header[meth_str_len] != ' ' && header[meth_str_len] != '\t') )
		return COMM_REQUEST_FAILED;

	const char *url_str = header + meth_str_len + op_strspn(header + meth_str_len, " \t\r\n");
	int url_str_len = op_strcspn(url_str, " \t\r\n");

	const char *header_start = op_strchr(header,'\n');
	
	if(url_str_len == 0 || header_start == NULL)
		return COMM_REQUEST_FAILED;

	header_start ++;

	if (OpStatus::IsError(url_req.Set(url_str, url_str_len)))
		return COMM_REQUEST_FAILED;

	url = urlManager->GetURL(url_req);
	if(url.GetRep() == NULL)
		return COMM_REQUEST_FAILED;

	URLType type = (URLType) url->GetAttribute(URL::KType);
	ServerName *name = (ServerName *) url->GetAttribute(URL::KServerName);

	if(name == NULL || name->Name() == NULL || *(name->Name()) == '\0' ||
		(type != URL_HTTP && type != URL_HTTPS && 
		type != URL_FTP && type != URL_Gopher && type != URL_WAIS ))
		return COMM_REQUEST_FAILED;

#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
	MpSocketAddress *saddr = name->SocketAddress();
	if (saddr)
	{
		OP_STATUS res;
		TRAP_AND_RETURN_VALUE_IF_ERROR(
			res, saddr->SetNetworkLinkNameL(network_link_name),
			COMM_REQUEST_FAILED);
	}
#endif // MULTIPLE_NETWORK_LINKS_SUPPORT

	HTTP_request_st *request = GetServerAndRequest(proxy, override_proxy_prefs);

	if(request == NULL)
		return (waiting ? COMM_REQUEST_WAITING : COMM_REQUEST_FAILED);

	if(request->origin_host == NULL || request->origin_host->Name() == NULL ||
		*(request->origin_host->Name()) == '\0')
	{
		return COMM_REQUEST_FAILED;
	}
	double timeout = url->GetAttribute(URL::KTimeoutMaxTCPConnectionEstablished);
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	http = OP_NEW(HTTP_Request, (mh, method, request, timeout, FALSE,  (type == URL_HTTPS)));
#else
	http = OP_NEW(HTTP_Request, (mh, method, request, timeout, FALSE));
#endif

	if (!http || OpStatus::IsError(http->Construct()))
	{
		OP_DELETE(http);
		http = 0;
		return COMM_REQUEST_FAILED;
	}
	http->SetExternal();
	http->SetSendAcceptEncoding(FALSE);

	BOOL do_not_load = FALSE;

	if(type == URL_HTTP || type == URL_HTTPS)
	{
#ifdef _ASK_COOKIE
		if(urlManager->HandlingCookieForURL(url.GetRep()))
		{
			g_main_message_handler->SetCallBack(this,MSG_HTTP_COOKIE_LOADED,0,0);
			SetProgressInformation(WAITING_FOR_COOKIES,0,0);
			do_not_load = TRUE;
		}
		else
#endif
		{
			BOOL havepw = FALSE;
			BOOL haspw = FALSE;
			int version = 0;
			int max_version = 0;
			const char *cookies = NULL;

			TRAPD(op_err, cookies = urlManager->GetCookiesL(url.GetRep(), version, max_version, FALSE, FALSE, havepw, haspw, url->GetContextId(), TRUE));
			if (OpStatus::IsMemoryError(op_err))
				return COMM_REQUEST_FAILED;

			http->SetCookie(cookies, version, max_version); 
	    }
	}

	HeaderList *headers = OP_NEW(HeaderList, ());
	if (!headers)
	{
		OP_DELETE(http);
		http = 0;
		return COMM_REQUEST_FAILED;
	}
	http->SetExternalHeaders(headers); // ownership over headers taken
	headers->SetValue((char *) header_start);
	HeaderEntry *header_item = headers->First();

	while((header_item = headers->GetHeader("Connection")) != NULL)
	{
		ParameterList parameters;

		parameters.SetValue((char *) header_item->Value(),PARAM_SEP_SEMICOLON| PARAM_NO_ASSIGN);

		for (Parameters *param = parameters.First(); param; param = param->Suc())
		{
			if(param->Name() && 
				op_stricmp(param->Name(), "Keep-Alive") != 0 && 
				op_stricmp(param->Name(), "TE") != 0 && 
				op_stricmp(param->Name(), "close") != 0)
			{
				http->AddConnectionParameter(param->Name());
			}
		}

		OP_DELETE(header_item);
	}

	header_item = headers->GetHeader("Cookie");
	if(header_item)
	{
		http->AddCookies(header_item->Value());
		header_item->Out();
		OP_DELETE(header_item);
	}

	OP_STATUS op_err;
	if(method == HTTP_METHOD_POST && (type == URL_HTTP || type == URL_HTTPS || type == URL_FTP))
		{
		upload_elm = OP_NEW(Upload_AsyncBuffer, ());
		if(!upload_elm)
		return COMM_REQUEST_FAILED;
		TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, upload_elm->InitL(body_len), COMM_REQUEST_FAILED);
		TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, upload_elm->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION), COMM_REQUEST_FAILED);

		http->SetData(upload_elm);
	}

	http->ChangeParent(this);
	http->SetCallbacks(this, this);

	return (!do_not_load ? http->Load() : COMM_LOADING);
}

OP_STATUS URL_External_Request_Handler::SetCallbacks(MessageObject* master, MessageObject* parent)
{
    static const int messages[] =
    { 
		MSG_COMM_DATA_READY,
        MSG_COMM_LOADING_FINISHED,
        MSG_COMM_LOADING_FAILED
    };
  
    mh->SetCallBackList(master, Id(), 0, messages, ARRAY_SIZE(messages));

	if (parent && parent != master)
        mh->SetCallBackList(parent, Id(), 0, messages, ARRAY_SIZE(messages));

	return OpStatus::OK;
}

void URL_External_Request_Handler::ProcessReceivedData()
{
	manager->ProcessReceivedData(Id());
}

BOOL URL_External_Request_Handler::RetrieveData(char *&header, unsigned &header_len,
						unsigned char *&body, unsigned long &body_len)
{
	header = NULL;
	header_len = 0;
	body = NULL;
	body_len = 0;

	if(header_string)
	{
		header = header_string;
		header_string = NULL;
		header_len = op_strlen(header);
	}

	if(http)
	{
		unsigned blen = prefsManager->GetNetworkBufferSize() *1024;
		body = OP_NEWA(unsigned char, blen);
		if (body == NULL)
			return FALSE;

		body_len = http->ReadData((char *) body, blen);
	}

	return http ? TRUE : FALSE;
}

void URL_External_Request_Handler::SendData(const void *data, unsigned long data_len)
{
	if (http)
		http->AppendUploadData(data, data_len); // FIXME: OOM
}

void URL_External_Request_Handler::SetProgressInformation(ProgressState progress_level, 
											 unsigned long progress_info1, 
											 const uni_char *progress_info2)
{
	switch(progress_level)
	{
	case HEADER_LOADED :
		HandleHeaderLoaded();
		break;
	case HANDLE_SECONDARY_DATA :
	{
		if(http)
			http->SetReadSecondaryData();
		ProcessReceivedData();
		break;
	}
	case RESTART_LOADING:
		{
			OP_DELETEA(header_string);
			header_string = NULL;
			manager->RestartingRequest(Id());
			/* This URL_External_Request_Handler object may have been deleted
			   at this point - by MURLExternalRequestManager. */
		}
		break;
	case GET_APPLICATIONINFO:
		{
			char **temp = (char **) progress_info2;
			if(temp)
				*temp = SetNewStr(url.Name(PASSWORD_HIDDEN));//FIXME:OOM (unable to report)
		}
		break;
	case UPLOADING_PROGRESS:
		{
			if (manager->GetObserver())
				manager->GetObserver()->UploadProgress(Id(), progress_info1);
		}
		break;
	}
}

void URL_External_Request_Handler::HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
									__DO_START_TIMING(TIMING_COMM_LOAD_HT);
	switch (msg)
	{
	case MSG_COMM_DATA_READY:
	{
		ProcessReceivedData();
		break;
	}
	case MSG_COMM_LOADING_FINISHED:
		HandleLoadingFinished();
		// ProcessReceivedData(); (OGW) should we do this???
		break;
		
	case MSG_COMM_LOADING_FAILED:
		HandleLoadingFailed(par1, par2);
		// ProcessReceivedData(); (OGW) should we do this???
		break;
#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
	case MSG_HTTP_COOKIE_LOADED:
		TRAPD(op_err, CheckForCookiesHandledL());//FIXME:OOM
		break;
#endif		
		/*
		case MSG_HTTP_HEADER_LOADED:
		HandleHeaderLoaded();
		break;
		*/
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD_HT);
}

void URL_External_Request_Handler::HandleLoadingFinished()
{
	HeaderList	*headerlist;

#ifdef DEBUG_HTTP_REQDUMP
		PrintfTofile("redir.txt","Handling LHH finished for %sn",url.Name());
#endif
	HeaderInfo *hinfo = http->GetHeader();

	headerlist = (hinfo->info.has_trailer ? &hinfo->trailingheaders : &hinfo->headers);

	switch(hinfo->response)
	{
		case HTTP_EXPECTATION_FAILED:
			http->SetMasterHTTP_Continue(FALSE);
			http->Clear();
			http->Load();
			break;
#ifdef SUPPORT_HTTP_305_USE_PROXY_REDIRECTS
		case HTTP_USE_PROXY:
			if(!http->info.proxy_request)
			{
				HeaderEntry *hdr = headerlist->GetHeader("Location");

				if(hdr && hdr->Value() && *hdr->Value())
				{
					char *tmp = (char *) hdr->Value();
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

					URL_Rep proxy_url(hdr->Value());
					if(proxy_url.GetType() == URL_HTTP && proxy_url.GetServerName())
					{
						manager->RestartingRequest(Id());
						http->request->connect_host = proxy_url.GetServerName();
						http->request->connect_port = proxy_url.GetServerPort();
						http->Clear();
						http->Load();
						OP_DELETEA(header_string);
						header_string = NULL;
						return;
					}
				}
			}
#endif // SUPPORT_HTTP_305_USE_PROXY_REDIRECTS
		default:
			if(header_string)
			{
				ProcessReceivedData();
			}
			else
				mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);
			DeleteComm();
	}

}

void URL_External_Request_Handler::HandleLoadingFailed(MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	DeleteComm();
	mh->PostMessage(MSG_COMM_LOADING_FAILED,Id(),par2);
}

void URL_External_Request_Handler::HandleHeaderLoaded()
{
#ifdef DEBUG_URL_COMM
	{
		FILE *fptemp = fopen("c:\\klient\\url.txt","a");
		fprintf(fptemp,"[%d] URL: %s - HTTP_HEADER_LOADED\n", id, name);
		fclose(fptemp);
	}
#endif

	HeaderInfo *hinfo = http->GetHeader();
	HeaderEntry *header;
	HeaderList	*headerlist;

	if (!hinfo)
		return;

	headerlist = (hinfo->info.has_trailer ? &hinfo->trailingheaders : &hinfo->headers);

	/*
	if(prefsManager->CookiesEnabled() != COOKIE_NONE)
	{
		HeaderEntry *cookie;

#ifdef _USE_VERSION_2_COOKIES
		cookie = headerlist->GetHeader("Set-Cookie2",NULL, HEADER_RESOLVE);
		if(!cookie)
#endif
			cookie = headerlist->GetHeader("Set-Cookie",NULL, HEADER_RESOLVE);

		while(cookie)
		{
			cookie->DuplicateInto(&hptr->recvinfo.cookie_headers);
			cookie = headerlist->GetHeader(NULL, cookie,HEADER_NO_RESOLVE);
		}
		if(!hptr->recvinfo.cookie_headers.Empty())
			urlManager->HandleCookies(url);
	}
	*/

	unsigned long new_header_len = 0;
	header = headerlist->First();
	while(header != NULL)
	{
		if(header->Name() == NULL || header->Value() == NULL)
		{
			HeaderEntry *temp = header;
			header = header->Suc();

			temp->Out();
			OP_DELETE(temp);
			continue;
		}
		new_header_len += op_strlen(header->Name())+ 2 + op_strlen(header->Value())+ 2;
		header = header->Suc();
	}

	if(!hinfo->info.has_trailer)
	{
		new_header_len += 20;
		if(hinfo->response_text)
			new_header_len += op_strlen(hinfo->response_text);
	}

	if(header_string)
		new_header_len += op_strlen(header_string);


	char *new_header = OP_NEWA(char, new_header_len+1);//FIXME:OOM (unable to report)
	if(new_header)
	{
		if(header_string)
			op_strcpy(new_header, header_string);
		else
			new_header[0] = '\0';

		if(!hinfo->info.has_trailer)
		{
			StrCatSprintf(new_header,"HTTP/1.1 %u %s\r\n", hinfo->response, (hinfo->response_text ? hinfo->response_text : ""));
		}

		header = headerlist->First();
		while(header != NULL)
		{
			StrMultiCat(new_header, header->Name(), ": ", header->Value(), "\r\n");
			header = header->Suc();
		}
	}

	OP_DELETE(header_string);

	header_string = new_header;

	ProcessReceivedData();
}

#ifdef _ASK_COOKIE
void URL_External_Request_Handler::CheckForCookiesHandledL()
{
	if(urlManager->HandlingCookieForURL(url.GetRep()))
		return;

	g_main_message_handler->UnsetCallBack(this,MSG_HTTP_COOKIE_LOADED);
			BOOL have_password = FALSE;
			BOOL have_authentication= FALSE;
	int version = 0;
	int max_version = 0;
	const char *cookies = urlManager->GetCookiesL(url.GetRep(), version, max_version,
				FALSE,
				FALSE,
				have_password,
				have_authentication,
				url.GetContextId(),
				TRUE
                );
	http->SetCookie(cookies, version, max_version);

	http->Load();
}
#endif


#undef SUPPORT_AUTO_PROXY_CONFIGURATION
HTTP_request_st *URL_External_Request_Handler::GetServerAndRequest(const uni_char *use_proxy, BOOL override_proxy_prefs)
{
	HTTP_request_st *req = OP_NEW(HTTP_request_st, ());

	URL_Name *url_name = url.GetRep()->GetURL_Name();
	URLType type = url_name->GetType();

	waiting = FALSE;
	if(req == NULL || url_name == NULL)
	{
		OP_DELETE(req);
		return NULL;
	}

	req->connect_host = req->origin_host = url_name->GetServerName();
	req->connect_port = req->origin_port = url_name->Port() ? url_name->Port() : (url.Type() == URL_HTTPS ? 443 : 80);

	const uni_char *proxy = NULL;
	const char *request = NULL;

	if (override_proxy_prefs)
	{
		proxy = use_proxy;
	}
	else
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	if (prefsManager->IsAutomaticProxyOn())
#endif
	{
		switch (type)
		{
			case URL_HTTP : proxy = prefsManager->GetHTTPProxyServer();
							break;
			case URL_HTTPS : proxy = prefsManager->GetHTTPSProxyServer();
							break;
#ifndef NO_FTP_SUPPORT
			case URL_FTP : proxy = prefsManager->GetFTPProxyServer();
							break;
#endif // NO_FTP_SUPPORT
#ifdef	GOPHER_SUPPORT
			case URL_Gopher : proxy = prefsManager->GetGopherProxyServer();
							break;
#endif
#ifdef	WAIS_SUPPORT
			case URL_WAIS : proxy = prefsManager->GetWAISProxyServer();
							break;
#endif
#ifdef	SOCKS_SUPPORT
			case URL_SOCKS : proxy = prefsManager->GetSOCKSProxyServer();
							break;
#endif
		}
		if (proxy)
		{
			OpString origin_name;
			if (OpStatus::IsError(origin_name.Set(req->origin_host->Name())))
			{
				OP_DELETE(req);
				return NULL;
	    	}
			if (!prefsManager->UseProxyOnServer(origin_name, req->origin_port))
			proxy = NULL;
     	}
	}

	if(proxy)
	{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		if(hptr->sendinfo.using_apc_proxy)
		{
			ParameterList proxylist;
			ServerName *candidate;
			unsigned short candidate_port;
			
			proxylist.SetValue(proxy, PARAM_SEP_SEMICOLON | PARAM_NO_ASSIGN | PARAM_ONLY_SEP);
			
			Parameters *param = proxylist.First();
			PROXY_type current_type = PROXY_HTTP;
			ProxyListItem  *item = NULL;
			
			while(param && current_type != NO_PROXY)
			{
				ParameterList apc_item;
				
				apc_item.SetValue(param->Name(),  PARAM_SEP_WHITESPACE | PARAM_NO_ASSIGN | PARAM_ONLY_SEP);
				Parameter *apc_param = apc_item.First();
				switch(CheckKeywords(apc_param->Name(),"DIRECT","PROXY","SOCKS"))
				{
				case 1:
					item = OP_NEW(ProxyListItem, ());//FIXME:OOM (unable to report)
					if (item)
					{
						item->type = current_type = NO_PROXY;
						item->proxy_candidate = req->origin_host;
						item->port = req->origin_port;
						item->Into(&req->connect_list);
						item = NULL;
					}
					break;
				case 2: 
					current_type = PROXY_HTTP;
					apc_param = apc_param->Suc();
					
					if(apc_param != NULL)
					{
						candidate = urlManager->GetServerName(apc_param->Name(), candidate_port, TRUE);//FIXME:OOM (unable to report)
						if(candidate && candidate->MayBeUsedAsProxy(port))
						{
							item = OP_NEW(ProxyListItem, ());//FIXME:OOM (unable to report)
							if (item)
							{
								item->type = current_type;
								item->proxy_candidate = candidate;
								item->port = candidate_port;
								item->Into(&req->connect_list);
							}
						}
					}
					break;
#ifdef SOCKS_SUPPORT
				case 3: 
					current_type = PROXY_SOCKS;
					apc_param = apc_param->Suc();
					
					if(apc_param != NULL)
					{
						candidate = urlManager->GetServerName(apc_param->Name(), candidate_port, TRUE);//FIXME:OOM (unable to report)
						if(candidate && candidate->MayBeUsedAsProxy(port))
						{
							item = OP_NEW(ProxyListItem, ());//FIXME:OOM (unable to report)
							if (item)
							{
								item->type = current_type;
								item->proxy_candidate = candidate;
								item->port = candidate_port;
								item->Into(&req->connect_list);
							}
						}
					}
					break;
#endif	
				}
			}

			if(req->connect_list.Cardinal() == 1)
			{
				item = (ProxyListItem *) req->connect_list.First();
				item->Out();
				req->connect_host = item->proxy_candidate;
				req->connect_port = item->port;
				req->proxy = item->type;
				OP_DELETE(item);
			}
			else if(!req->connect_list.Empty())
			{
				req->current_connect_host = item = (ProxyListItem *) req->connect_list.First();
				req->connect_host = item->proxy_candidate;
				req->connect_port = item->port;
				req->proxy = item->type;
			}
			if(type != URL_HTTPS && req->proxy != NO_PROXY)
				request = url->GetAttribute(URL::KName_Escaped).CStr();
		}
		else
#endif	
		{
			req->connect_host = urlManager->GetServerName(proxy, req->connect_port, TRUE);//FIXME:OOM (unable to report)
#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
			if (req->connect_host && req->connect_host->SocketAddress())
			{
				const uni_char *linkname = 0;
				if (url.GetServerName() &&
					url.GetServerName()->SocketAddress())
				{
					linkname = url.GetServerName()->SocketAddress()->GetNetworkLinkName();
				}
				TRAPD(res, req->connect_host->SocketAddress()->SetNetworkLinkNameL(linkname));
			}
#endif // MULTIPLE_NETWORK_LINKS_SUPPORT
			if(type != URL_HTTPS)
				request = url->GetAttribute(URL::KName_Escaped).CStr();
			req->proxy = TRUE;
		}

	}
	else if(type != URL_HTTP && type != URL_HTTPS)
	{
		OP_DELETE(req);
		return NULL;
	}

	OpString8 temp_req;
	if(request == NULL)
	{
		TRAPD(op_err, url->GetAttributeL(URL::KPathAndQuery_L,temp_req));
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(req);
			return NULL;
		}
		request = temp_req.CStr();
	}

	SetStr(req->request, (request ? request : "/"));//FIXME:OOM (unable to report)

	if(proxy && type != URL_HTTPS)
	{
		req->path = op_strstr(req->request,"://");
		if(req->path)
			req->path = op_strchr(req->path+3,'/');
	}
	else
		req->path = req->request;

	return req;
}




// ============== MANAGER ====================

URL_External_Request_Manager::URL_External_Request_Manager(MURLExternalRequestManager *aObserver)
{
	iObserver = aObserver;
}


URL_External_Request_Manager::~URL_External_Request_Manager()
{
	requests.Clear();
	iObserver = NULL;
}

URL_External_Request_Handler *URL_External_Request_Manager::FindRequest(unsigned long id)
{
	URL_External_Request_Handler *req = (URL_External_Request_Handler *) requests.First();

	while(req)
	{
		if(req->Id() == id)
			break;

		req = (URL_External_Request_Handler *) req->Suc();
	}

	return req;
}


unsigned long URL_External_Request_Manager::LoadRequest(
#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
	const uni_char *network_link_name,
#endif
	const char *header, unsigned long header_len, unsigned long body_len,
	const uni_char *proxy, BOOL override_proxy_prefs)
{
	if(iObserver == NULL || header == NULL || header_len == 0)
		return 0;

	URL_External_Request_Handler *req = OP_NEW(URL_External_Request_Handler, (g_main_message_handler, this));
	if (req == NULL)
		return 0;

	CommState stat = req->Load(
#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
		network_link_name,
#endif
		header, header_len, body_len, proxy, override_proxy_prefs);

	if(stat == COMM_REQUEST_FAILED)
	{
		OP_DELETE(req);
		req = NULL;
	}
	else
	{
		req->SetCallbacks(this,this);
		req->Into(&requests);
	}

	return (req ? req->Id() : 0);
}

// Buffers are allocated and must be freed by caller
// Returns TRUE if more data (even 0 bytes) are expected;
BOOL URL_External_Request_Manager::RetrieveData(unsigned long id, char *&header, unsigned &header_len,
						unsigned char *&body, unsigned long &body_len)
{
	URL_External_Request_Handler *req;
	req = FindRequest(id);
	// no such request, return TRUE anyway...
	if (!req) {
		body_len = header_len = 0;
		return FALSE;
	}
	// retrieve data on request, delete it if no data is present
	if (!req->RetrieveData(header,header_len, body, body_len)) {
		OP_DELETE(req);
		body_len = header_len = 0;
        return FALSE;
	}
	// return TRUE
	return TRUE;
}

void URL_External_Request_Manager::SendData(
	unsigned long id, const void *data, unsigned long data_len)
{
	URL_External_Request_Handler *req;
	req = FindRequest(id);
	if (!req)
		return;
	req->SendData(data, data_len);
}

void URL_External_Request_Manager::EndLoading(unsigned long id)
{
	URL_External_Request_Handler *req;

	req = FindRequest(id);

	if(req)
	{
		req->EndLoading();
		OP_DELETE(req);
	}
}

void URL_External_Request_Manager::ProcessReceivedData(unsigned long id)
{
	if(iObserver)
		iObserver->DataReady(id);
}

void URL_External_Request_Manager::RestartingRequest(unsigned long id)
{
	if(iObserver)
		iObserver->DataCorrupt(id);
}

void URL_External_Request_Manager::HandleCallback(int msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	__DO_START_TIMING(TIMING_COMM_LOAD_HT);
	switch (msg)
	{
	case MSG_COMM_DATA_READY:
		ProcessReceivedData(par1);
		break;

	case MSG_COMM_LOADING_FAILED:
	case MSG_COMM_LOADING_FINISHED:
		{
			URL_External_Request_Handler *req;

			req = FindRequest(par1);

			if(req)
			{
				req->EndLoading();
				OP_DELETE(req);
			}

			if(msg == MSG_COMM_LOADING_FAILED)
			{
				iObserver->LoadFailed(par1, par2);
			}
			else
				ProcessReceivedData(par1);
		}	
		break;
	}
	__DO_STOP_TIMING(TIMING_COMM_LOAD_HT);
}

#endif
