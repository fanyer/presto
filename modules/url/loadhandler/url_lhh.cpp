/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/formats/uri_escape.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/util/timecache.h"
#include "modules/url/url2.h"
#include "modules/url/url_tools.h"
#ifdef _MIME_SUPPORT_
//#include "modules/mime/mimedec2.h"
#endif // _MIME_SUPPORT_
#include "modules/util/datefun.h"
#include "modules/util/handy.h"
#include "modules/util/str.h"
#include "modules/url/uamanager/ua.h"

#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/network/OpSocketAddress.h"

#include "modules/util/timecache.h"

#include "modules/encodings/decoders/utf8-decoder.h"
#include "modules/encodings/encoders/utf8-encoder.h"
#include "modules/encodings/detector/charsetdetector.h"
#include "modules/encodings/utility/charsetnames.h"
#include "modules/encodings/utility/createinputconverter.h"
#include "modules/upload/upload.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"

#include "modules/olddebug/timing.h"

#include "modules/cache/url_cs.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_pd.h"
//#include "connman.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
//#include "comm.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"

#include "modules/auth/auth_elm.h"

#include "modules/url/url_link.h"
#include "modules/util/htmlify.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"
#include "modules/about/opredirectpage.h"

#include "modules/util/cleanse.h"

#ifdef SCOPE_RESOURCE_MANAGER
# include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER

#include "modules/url/url_dynattr_int.h"

#ifdef WEB_TURBO_MODE
#include "modules/obml_comm/obml_config.h"
#endif // WEB_TURBO_MODE

#ifdef _DEBUG
#ifdef YNP_WORK
//#define _DEBUG_RED
#endif
#endif

#include "modules/olddebug/tstdump.h"

#include "modules/url/protocols/common.h"

// Url_lhh.cpp

// URL Load Handler HTTP
int UU_encode (unsigned char* bufin, unsigned int nbytes, char* bufcoded);

#ifdef _DEBUG
#ifdef YNP_WORK
//#define DEBUG_HTTP_REQDUMP
//#define _DEBUG_DD1
#endif
#endif

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/application_cache_manager.h"
#include "modules/applicationcache/manifest.h"
#endif // APPLICATION_CACHE_SUPPORT


extern void HeaderLoaded(const OpStringC& url, const OpStringC8& headers);

URL_HTTP_LoadHandler::URL_HTTP_LoadHandler(URL_Rep *url_rep, MessageHandler *msgh)
	: URL_LoadHandler(url_rep, msgh)
{
	req = NULL;

	info.waiting = FALSE;
	info.name_completion_modus = FALSE;
	info.use_name_completion = FALSE;
#ifdef HTTP_DIGEST_AUTH
	info.check_auth_info = FALSE;
	info.check_proxy_auth_info = FALSE;
#endif // HTTP_DIGEST_AUTH
#ifdef STRICT_CACHE_LIMIT
	ramcacheextra=0;
	tryagain = TRUE;
#endif // STRICT_CACHE_LIMIT
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	info.load_direct = FALSE;
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
	info.predicted = FALSE;
	predicted_depth = 0;
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEBSERVER_SUPPORT
	info.unite_connection = FALSE;
	info.unite_admin_connection = FALSE;
#endif // WEBSERVER_SUPPORT
}

URL_HTTP_LoadHandler::~URL_HTTP_LoadHandler()
{
	urlManager->StopAuthentication(url);
	DeleteComm();
	mh->UnsetCallBacks(this);
#ifdef STRICT_CACHE_LIMIT
	urlManager->SubFromRamCacheSizeExtra(ramcacheextra);
	ramcacheextra=0;
#endif // STRICT_CACHE_LIMIT
}


void URL_HTTP_LoadHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
    // this is coming from sink object (object below, in this case req request)
									__DO_START_TIMING(TIMING_COMM_LOAD_HT);
	NormalCallCount blocker(this);
	switch (msg)
	{
	case MSG_COMM_DATA_READY:
		ProcessReceivedData();
		break;
	case MSG_COMM_LOADING_FINISHED:
		HandleLoadingFinished();
		break;

	case MSG_COMM_LOADING_FAILED:
		HandleLoadingFailed(par1, par2);
		break;
#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
	case MSG_HTTP_COOKIE_LOADED:
		TRAPD(op_err, CheckForCookiesHandledL());//FIXME:OOM
		break;
#endif // _ASK_COOKIE
		/*
		case MSG_HTTP_HEADER_LOADED:
		HandleHeaderLoaded();
		break;
		*/
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD_HT);
}

void URL_HTTP_LoadHandler::HandleLoadingFinished()
{
	URL_DataStorage *url_ds = url->GetDataStorage();
	if(!url_ds)
		return;

	URL_HTTP_ProtocolData *hptr = url_ds->GetHTTPProtocolData();
	if(hptr == NULL)
	{
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		req->Stop();
		DeleteComm();
		url->OnLoadFinished(URL_LOAD_FAILED);
		return;
	}

	HeaderInfo *hinfo = req->GetHeader();

#ifdef DEBUG_HTTP_REQDUMP
		PrintfTofile("redir.txt","Handling LHH finished for %sn",url->Name());
#endif // DEBUG_HTTP_REQDUMP
	if (!hinfo)
		return;

#ifdef HTTP_DIGEST_AUTH
	HeaderList *headerlist = (hinfo->info.has_trailer ? &hinfo->trailingheaders : &hinfo->headers);
#endif
	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	if (server_name)
		url_ds->SetAttribute(URL::KLoadedFromNetType, server_name->GetNetType());
	URL_LoadHandler::SetProgressInformation(REQUEST_FINISHED,0,
		server_name && server_name->UniName() ? server_name->UniName() : UNI_L(""));

#ifdef OPERA_PERFORMANCE
	http_Manager->OnResponseFinished(req->GetRequestNumber(), hinfo->content_length, hptr->keepinfo.time_request_created);
#endif // OPERA_PERFORMANCE

#ifdef HTTP_DIGEST_AUTH
	if(info.check_auth_info)
	{
		BOOL failed = FALSE;
		info.check_auth_info = !CheckAuthentication(headerlist, HTTP_Header_Authentication_Info, FALSE, TRUE, URL_ERRSTR(SI, ERR_HTTP_AUTH_FAILED), TRUE, failed);
		if(failed)
			return;
	}

	if(info.check_proxy_auth_info)
	{
		BOOL failed = FALSE;
		info.check_proxy_auth_info = !CheckAuthentication(headerlist, HTTP_Header_Proxy_Authentication_Info, TRUE, TRUE, URL_ERRSTR(SI, ERR_HTTP_PROXY_AUTH_FAILED), TRUE, failed);
		if(failed)
			return;
	}
#endif // HTTP_DIGEST_AUTH

	uint32 response = url_ds->GetAttribute(URL::KHTTP_Response_Code);


	switch(hptr->recvinfo.response)
	{
#ifdef _ENABLE_AUTHENTICATE
		case HTTP_UNAUTHORIZED:
			if(url->GetAttribute(URL::KSpecialRedirectRestriction))
				goto handle_auth_failure;
			if (((HTTPAuth) hptr->flags.auth_status) != HTTP_AUTH_NOT)
			{
				NormalCallCount blocker(this);
				if(!urlManager->HandleAuthentication(url, ((HTTPAuth) hptr->flags.auth_status) == HTTP_AUTH_HAS))
					goto handle_auth_failure;
			}
			else
				goto handle_auth_failure;
			break;

		case HTTP_PROXY_UNAUTHORIZED:
			if (((HTTPAuth) hptr->flags.proxy_auth_status) != HTTP_AUTH_NOT)
			{
				NormalCallCount blocker(this);

				if(!urlManager->HandleAuthentication(url, ((HTTPAuth) hptr->flags.proxy_auth_status) == HTTP_AUTH_HAS))
					goto handle_auth_failure;
			}
			else
				goto handle_auth_failure;
			break;
#endif // _ENABLE_AUTHENTICATE
		case HTTP_EXPECTATION_FAILED:
			{
				if(!req || !req->info.send_expect_100_continue)
					goto finish_document;
				req->SetMasterHTTP_Continue(FALSE);
				req->Clear();
				TRAPD(op_err, req->ResetL());
				OpStatus::Ignore(op_err);
				req->Load();
				hptr->flags.always_check_never_expired_queryURLs = TRUE;
			}
			break;
#ifdef SUPPORT_HTTP_305_USE_PROXY_REDIRECTS
		case HTTP_USE_PROXY:
			hptr->flags.always_check_never_expired_queryURLs = TRUE;
			if(!req->info.proxy_request && hptr->recvinfo.location.HasContent())
			{
				HTTP_request_st *req = req->request;
				char *tmp = hptr->recvinfo.location.CStr();

				if(op_strchr(tmp,'/') == NULL)
				{
					req->connect_host = urlManager->GetServerName(tmp, req->connect_port, TRUE);//FIXME:OOM (unable to report)
					if(req->connect_host == NULL)
						break;// exit switch, end loading

#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
					if (req->connect_host->SocketAddress())
					{
						TRAPD(res, req->connect_host->SocketAddress()->SetNetworkLinkNameL(0)); // FIXME: OOM
					}
#endif // MULTIPLE_NETWORK_LINKS_SUPPORT
				}
				else
				{
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
					URL proxy_url = urlManager->GetURL(tmp_url,hptr->recvinfo.location, href_rel, TRUE);
					if(!proxy_url.GetServerName())
						break; // exit switch, end loading
					req->connect_host = proxy_url.GetServerName();
					req->connect_port = proxy_url.GetServerPort();
				}

				if(req->connect_port == 0)
					req->connect_port = 80;
				req->proxy = TRUE;

				if(OpStatus::IsError(req->SetUpMaster()))
					break;// exit switch, end loading

				const char *request_str = url->Name(PASSWORD_NOTHING);
				if(OpStatus::IsError(SetStr(req->request, (request_str ? request_str : "/"))))//FIXME:OOM (unable to report)
					break; // exit switch, end loading
				req->path = op_strstr(req->request,"://");
				if(req->path)
					req->path = op_strchr(req->path+3,'/');
				else
					req->path = req->request;

#if defined(_ENABLE_AUTHENTICATE) && defined(_USE_PREAUTHENTICATION_)
				if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowPreSendAuthentication))
				{
					AuthElm *auth = req->connect_host->Get_Auth(NULL /*request->use_proxy_realm*/,
						req->connect_port, NULL, type, AUTH_SCHEME_HTTP_PROXY);

					if(auth)
					{
						char *auth_str = NULL;
						if(OpStatus::IsError(auth->GetAuthString(&auth_str, url, http)))
							break;  // exit switch, end loading
						req->SetProxyAuthorization(auth_str);
						req->SetProxyAuthorizationId(auth->GetId());
#ifdef HTTP_DIGEST_AUTH
						if(auth->GetScheme() == AUTH_SCHEME_HTTP_PROXY_DIGEST)
							info.check_proxy_auth_info = TRUE;
#endif // HTTP_DIGEST_AUTH
						hptr->flags.proxy_auth_status = HTTP_AUTH_HAS;
					}

				}
#endif // _ENABLE_AUTHENTICATE && _USE_PREAUTHENTICATION_
				url_ds->MoveCacheToOld();
				url_ds->SetHeaderLoaded(FALSE);

				req->Clear();
				req->Load();
				return;
			}
#endif // SUPPORT_HTTP_305_USE_PROXY_REDIRECTS
		default:
handle_auth_failure: // finish document
finish_document:
			if (IsRedirectResponse(response) &&
				(!hinfo->info.received_content_length ||  hinfo->content_length == 0))
			{
				OpFileLength registered_len=0;
				url->GetAttribute(URL::KContentLoaded, &registered_len);

				if(registered_len == 0 && !GenerateRedirectHTML(TRUE))
				{
					mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);
					DeleteComm();
					// No need to call LoadFinished here as it is handled in GenerateRedirectHTML
					return;
				}
			}
#ifndef SUPPORT_HTTP_305_USE_PROXY_REDIRECTS
			else if(response == HTTP_USE_PROXY && !hinfo->info.received_content_length)
			{
				OpFileLength registered_len=0;
				url->GetAttribute(URL::KContentLoaded, &registered_len);

				URLContentType cnt_type = (URLContentType) url->GetAttribute(URL::KContentType);

				if(registered_len == 0 && (cnt_type == URL_HTML_CONTENT || cnt_type == URL_UNDETERMINED_CONTENT))
				{
					OP_STATUS op_err;
					//RAISE_IF_ERROR(op_err = hptr->keepinfo.mime_type.Set("text/html"));
					RAISE_IF_ERROR(op_err = url_ds->SetAttribute(URL::KMIME_Type, "text/html"));
					if(OpStatus::IsMemoryError(op_err))
					{
						//FIXME:OOM - Check this OOM handling.
						mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
						req->Stop();
						DeleteComm();
						url->OnLoadFinished(URL_LOAD_FAILED);
						return;
					}
					OpStringC8 location(url_ds->GetAttribute(URL::KHTTP_Location));
					url_ds->SetAttribute(URL::KContentType, URL_HTML_CONTENT);
					url_ds->WriteDocumentData(URL::KNormal, "<HTML>\r\n<HEAD>\r\n<TITLE>Redirect to alternative proxy</TITLE>\r\n</HEAD>\r\n<BODY>\r\n"
						"The server tried to redirect Opera to the alternative proxy \"");
					url_ds->WriteDocumentData(URL::KHTMLify, location);
					url_ds->WriteDocumentData(URL::KNormal, "\".\r\nFor security reasons this is no longer supported.\r\n<BR><BR><HR>Generated by Opera &copy;\r\n</BODY>\r\n</HTML>");
					url_ds->WriteDocumentDataFinished();
				}
			}
#endif
			{
				hptr->flags.always_check_never_expired_queryURLs = TRUE;
				uint32 response = url_ds->GetAttribute(URL::KHTTP_Response_Code);
				OpFileLength registered_len = 0;
				url->GetAttribute(URL::KContentLoaded, &registered_len);

				if((!url_ds->GetAttribute(URL::KHeaderLoaded)|| !hptr->flags.header_loaded_sent) &&
					!(url_ds->GetCacheStorage() && url_ds->GetCacheStorage()->GetIsMultipart()) &&
					(URLContentType) url_ds->GetAttribute(URL::KContentType) != URL_UNDETERMINED_CONTENT &&
					!(registered_len == 0 && response >= 400)&&
					!IsRedirectResponse(response))
				{
					url_ds->SetAttribute(URL::KHeaderLoaded,TRUE);
					url_ds->BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
					hptr->flags.header_loaded_sent = TRUE;
					url->Access(FALSE);
					mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);
				}
				else
				{
					if(response >= 400 && registered_len == 0 && req->method != HTTP_METHOD_HEAD)
					{
						mh->PostMessage(MSG_COMM_LOADING_FAILED,Id(),urlManager->ConvertHTTPResponseToErrorCode(response));
					}
					else
						mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);
				}
				DeleteComm();
			}
	}
}

void URL_HTTP_LoadHandler::HandleLoadingFailed(MH_PARAM_1 par1, MH_PARAM_2 par2)
{
#ifdef OPERA_PERFORMANCE
	URL_DataStorage *url_ds = url->GetDataStorage();
	if(url_ds)
	{
		URL_HTTP_ProtocolData *hptr = url_ds->GetHTTPProtocolData();
		http_Manager->OnResponseFinished(req->GetRequestNumber(), 0, hptr->keepinfo.time_request_created);
	}
#endif // OPERA_PERFORMANCE

	// Str::SI_ERR_HTTP_100CONTINUE_ERROR is Non - fatal error (Hardcoded)
	if(par2 != URL_ERRSTR(SI, ERR_HTTP_100CONTINUE_ERROR))
	{
		DeleteComm();
		ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
		URL_LoadHandler::SetProgressInformation(REQUEST_FINISHED,0,
			server && server->UniName() ? server->UniName() : UNI_L(""));
	}
	mh->PostMessage(MSG_COMM_LOADING_FAILED,Id(),par2); // this must be called if an error is received from the HTTP stack
#ifdef SCOPE_RESOURCE_MANAGER
	SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE_FAILED, req);
#endif // SCOPE_RESOURCE_MANAGER
	url->OnLoadFinished(URL_LOAD_FAILED);
}

/** Null terminated list */
struct HTTP_HeaderTo_URL_Attribute {
	int header_name_tag;
	URL::URL_StringAttribute attribute;
	BOOL all;
};

static const HTTP_HeaderTo_URL_Attribute http_header_to_attribute_list[] = {
	{HTTP_Header_Date, URL::KHTTP_Date, FALSE},
	{HTTP_Header_Age, URL::KHTTPAgeHeader, FALSE},
	{HTTP_Header_Cache_Control, URL::KHTTPCacheControl_HTTP, TRUE},
	{HTTP_Header_Pragma, URL::KHTTPPragma, FALSE},
	{HTTP_Header_Expires, URL::KHTTPExpires, FALSE},
	{HTTP_Header_ETag, URL::KHTTP_EntityTag, FALSE},
	{HTTP_Header_Last_Modified, URL::KHTTP_LastModified, FALSE},
	{HTTP_Header_Content_Location, URL::KHTTPContentLocation, FALSE},
	{HTTP_Header_Content_Language, URL::KHTTPContentLanguage, FALSE},
	{HTTP_Header_Location, URL::KHTTP_Location, FALSE},
	{HTTP_Header_Content_Encoding, URL::KHTTPEncoding, FALSE},
	{HTTP_Header_X_Frame_Options, URL::KHTTPFrameOptions, FALSE},
	// Termination
	{0, URL::KNoStr, FALSE}
};

void SetAttributesFromHeadersL(URL_DataStorage *url_ds, HeaderList *headers, const HTTP_HeaderTo_URL_Attribute *list)
{
	if(!url_ds || !list || !headers)
		return;

	while(list->header_name_tag)
	{
		HeaderEntry *item = headers->GetHeaderByID(list->header_name_tag);
		while(item)
		{
			url_ds->SetAttributeL(list->attribute, item->Value());
			item = (list->all ? headers->GetHeaderByID(0, item) : NULL);
		}
		list++;
	}
}

void URL_HTTP_LoadHandler::HandleHeaderLoadedL()
{
	URL_DataStorage *url_ds = url->GetDataStorage();
	if(!url_ds)
		return;

	URL_HTTP_ProtocolData *hptr = url_ds->GetHTTPProtocolDataL();
	HeaderInfo *hinfo = req->GetHeader();
	if(!hptr)
		return;
	if (!hinfo)
		return;
	HandleHeaderLoadedStage2L(hptr, hinfo);
}

/** Helper function to introduce a layer that will prevent trampling of local variables. */
static OP_STATUS GetParameters(ParameterList *&refresh_parameters, HeaderEntry *header, unsigned flags, Prepared_KeywordIndexes index = KeywordIndex_None)
{
	TRAPD(err, refresh_parameters = header->GetParametersL(flags, index));
	return err;
}

void URL_HTTP_LoadHandler::HandleHeaderLoadedStage2L(URL_HTTP_ProtocolData *hptr, HeaderInfo *hinfo)
{
	OP_NEW_DBG("URL_HTTP_LoadHandler::HandleHeaderLoadedL", "url.loadhandler");
#ifdef _DEBUG_RED
	PrintfTofile("redir1.txt","%s HeaderLoaded\n", url->Name());
#endif // _DEBUG_RED
/* start: get variables */
	URL_DataStorage *url_ds = url->GetDataStorage();
	if(!url_ds)
		return;

#ifdef APPLICATION_CACHE_SUPPORT
	if (hinfo->response >= 400 && hinfo->response < 600)
	{
		BOOL had_application_cache_fallback;

		/* Check if this url is loaded from an application cache, and check if it had a fallback url*/
		LEAVE_IF_ERROR(url->CheckAndSetApplicationCacheFallback(had_application_cache_fallback));

		/* If it had an fallback url, CheckAndSetApplicationCacheFallback sets fallback url and broadcasts URL_LOADED message. */
		if (had_application_cache_fallback)
			return;
	}
#endif // APPLICATION_CACHE_SUPPORT

	URL this_url(url, (char *) NULL);
	ANCHOR(URL, this_url);
	URL_InUse url_blocker(this_url);
	ANCHOR(URL_InUse, url_blocker);

/* end: get variables */

/* start: get headers */
	BOOL x_mixed_replace = FALSE;
	HeaderEntry *header;
	HeaderList	*headerlist;

	if (!hinfo)
		return;

	url_ds->SetAttributeL(URL::KVLocalTimeLoaded, &hinfo->loading_started);
#ifdef USE_SPDY
	url_ds->SetAttributeL(URL::KLoadedWithSpdy, hinfo->info.spdy);
#endif // USE_SPDY

	if(hinfo->original_method == HTTP_METHOD_CONNECT)
	{
		if(hinfo->response != HTTP_PROXY_UNAUTHORIZED)
		{
			// documents served from the CONNECT response must not execute scripts
			url->SetAttribute(URL::KSuppressScriptAndEmbeds, MIME_Remote_Script_Disable);
		}
		else
			hptr->flags.connect_auth = TRUE;

		if(hinfo->response < HTTP_BAD_REQUEST)
		{
			// Any 2XX and 3XX response coming from a CONNECT request is treated as a bad gateway error
			hinfo->response = HTTP_BAD_GATEWAY;
		}
	}
	else if(hinfo->response != HTTP_NOT_MODIFIED) // No change for not modified
			url->SetAttribute(URL::KSuppressScriptAndEmbeds, MIME_No_ScriptEmbed_Restrictions);

#ifdef STRICT_CACHE_LIMIT
	/* Abort download if content-length is greater than free memory */
/*
	printf("Examine %s for content-length (http) (%s, %s)\n", url->Name(),
		   url->GetAttribute(URL::KLoadAsInline) ? "inline" : "main-ignored",
		   (!url->GetDataStorage() || url->GetDataStorage()->GetCacheType()!=URL_CACHE_USERFILE) ? "other storetype" : "storetype userfile-ignored");
*/
	if (url->GetAttribute(URL::KCacheType)!=URL_CACHE_USERFILE &&
		hinfo->info.received_content_length &&
		urlManager->GetMemoryLimit() < hinfo->content_length + urlManager->GetUsedUnderMemoryLimit())
	{
		urlManager->MakeRoomUnderMemoryLimit(hinfo->content_length);

		if (urlManager->GetMemoryLimit() < hinfo->content_length + urlManager->GetUsedUnderMemoryLimit())
		{
//			printf("Download of %s aborted due to content-length (http)\n", url->Name());
			Stop();
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(D,ERR_LOADING_ABORTED_OOM));
			url->LoadFinished(URL_LOAD_FAILED);
		};
	};
#endif // STRICT_CACHE_LIMIT

	headerlist = (hinfo->info.has_trailer ? &hinfo->trailingheaders : &hinfo->headers);
/* end: get headers */

/* start: undo redirection */ /* reloading url that was previously redirected and now we get a different response */
	if(!hinfo->info.has_trailer && hinfo->response != HTTP_NOT_MODIFIED)
	{
		hptr->flags.redirect_blocked = FALSE;
		URL moved_to_url = url_ds->GetAttribute(URL::KMovedToURL);
		if((URLStatus) moved_to_url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
		{
			MsgHndlList		*msg_hndlrs = url_ds->GetMessageHandlerList();
			if(msg_hndlrs)
			{
				MsgHndlList::Iterator itr;
				for(itr = msg_hndlrs->Begin(); itr != msg_hndlrs->End(); ++itr)
				{
					if((*itr)->GetMessageHandler())
						moved_to_url.StopLoading((*itr)->GetMessageHandler());
				}
			}
		}
		url_ds->SetAttributeL(URL::KHTTP_MovedToURL_Name, NULL);
	}
	url_ds->SetAttributeL(URL::KHTTP_Location, NULL);
/* end: undo redirection */

/* start: reloadsametarget */
	BOOL reload_same = !!url_ds->GetAttribute(URL::KReloadSameTarget);
	if(reload_same || hptr->flags.only_if_modified)
	{
		if(hinfo->response == HTTP_PARTIAL_CONTENT)
		{
			url_ds->UnsetCacheFinished();
		}
		else
		{
			if(reload_same  &&
				url_ds->GetAttribute(URL::KHTTP_Response_Code) < 300)
				url_ds->ResetCache();
			else if(reload_same)
				url_ds->MoveCacheToOld();
		}
	}
/* end: reloadsametarget */

/* start: Store all headers */
	if (hinfo->response != HTTP_NOT_MODIFIED
#ifdef NEED_URL_STORE_HEADERS_IF_UNDETERMINED_TYPE
		|| (URLContentType) url->GetAttribute(URL::KContentType) == URL_UNDETERMINED_CONTENT
#endif //NEED_URL_STORE_HEADERS_IF_UNDETERMINED_TYPE
		)
	{
		OP_DELETE(hptr->recvinfo.all_headers);
		hptr->recvinfo.all_headers = NULL;
		hptr->recvinfo.all_headers = headerlist->DuplicateL();

		HeaderEntry *hdr;
		while((hdr = hptr->recvinfo.all_headers->GetHeaderByID(HTTP_Header_Set_Cookie)) != NULL)
		{
			hdr->Out();
			OP_DELETE(hdr);
		}
		while((hdr = hptr->recvinfo.all_headers->GetHeaderByID(HTTP_Header_Set_Cookie2)) != NULL)
		{
			hdr->Out();
			OP_DELETE(hdr);
		}

	}
/* end: storeallheaders */

/* start: cookies */
#ifdef _ASK_COOKIE
	ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	COOKIE_MODES cmode_site = (server ? server->GetAcceptCookies(TRUE) :COOKIE_DEFAULT);
#endif
	COOKIE_MODES cmode_global = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	COOKIE_MODES cmode;

#ifdef _ASK_COOKIE
	cmode = cmode_site;

	if(cmode == COOKIE_DEFAULT)
#endif
		cmode = cmode_global;

	if(cmode != COOKIE_NONE &&
		!url_ds->GetAttribute(URL::KDisableProcessCookies) &&
		(!url_ds->GetAttribute(URL::KIsThirdParty) || cmode != COOKIE_SEND_NOT_ACCEPT_3P)
		)
	{

		HeaderList cookie_headers;
		ANCHOR(HeaderList, cookie_headers);

		cookie_headers.SetKeywordList(*headerlist);
		headerlist->DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie2);
		headerlist->DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie);

		if(!cookie_headers.Empty())
		{
			urlManager->HandleCookiesL(url, cookie_headers, url->GetContextId());
		}
	}
#ifdef COOKIE_WARNING_SUPPORT
	else
	{
		HeaderList cookie_headers;
		ANCHOR(HeaderList, cookie_headers);

		cookie_headers.SetKeywordList(*headerlist);
		headerlist->DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie2);
		headerlist->DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie);

		if(!cookie_headers.Empty())
		{
			url_ds->BroadcastMessage(MSG_COOKIE_SET, url->GetID(), 0, MH_LIST_ALL);
		}
	}
#endif // COOKIE_WARNING_SUPPORT

/* end: cookies */
/* start: cache policy 1*/
	time_t old_expiration_time = 0;
	time_t new_expiration_time = 0;
	if(hinfo->response != HTTP_NOT_MODIFIED)
	{
		if(!url_ds->GetAttribute(URL::KHeaderLoaded))
		{
			// Reset a number of things when the header is loaded
			static const URL_DataStorage::URL_UintAttributeEntry http_reset_protocol_flags[] = {
				{URL::KCachePolicy_AlwaysVerify, FALSE},
				{URL::KCachePolicy_MustRevalidate, FALSE},
				{URL::KCachePolicy_NoStore, FALSE},
				{URL::KContentType, URL_UNDETERMINED_CONTENT},
				{URL::KHTTP_Refresh_Interval, (uint32) (-1)},
				{URL::KUntrustedContent, FALSE},
#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
				{URL::KUsedContentDispositionAttachment, FALSE},
#endif
				{URL::KNoInt, FALSE},
			};
			static const URL_DataStorage::URL_StringAttributeEntry http_reset_protocolstr[] = {
				{URL::KHTTP_EntityTag, 0},
				{URL::KMIME_CharSet, 0},
				{URL::KMIME_Type, 0},
				{URL::KHTTP_Date, 0},
				{URL::KHTTP_LastModified, 0},
				{URL::KHTTPRefreshUrlName, 0},
				{URL::KHTTP_Location, 0},
				{URL::KHTTP_MovedToURL_Name, 0},
				{URL::KHTTPEncoding, 0},
				{URL::KHTTPContentLocation, 0},
				{URL::KHTTPContentLanguage, 0},

				{URL::KNoStr, NULL},
			};

			LEAVE_IF_ERROR(url_ds->SetAttribute(http_reset_protocol_flags));
			LEAVE_IF_ERROR(url_ds->SetAttribute(http_reset_protocolstr));

			time_t expiration_time = 0;
			url_ds->SetAttributeL(URL::KVHTTP_ExpirationDate, &expiration_time);

			hptr->recvinfo.link_relations.Clear();
		}
	}
	else
	{
		url_ds->GetAttribute(URL::KVHTTP_ExpirationDate, &old_expiration_time);
		url_ds->SetAttributeL(URL::KVHTTP_ExpirationDate, &new_expiration_time);
	}

	SetAttributesFromHeadersL(url_ds, headerlist, http_header_to_attribute_list);

#if defined URL_ENABLE_DYNATTR_RECV_HEADER || defined URL_ENABLE_DYNATTR_RECV_MULTIHEADER
	URL_DynamicStringAttributeDescriptor *attr = NULL;

	urlManager->GetFirstAttributeDescriptor(&attr);

	while(attr)
	{
		if(attr->GetReceiveHTTP() && attr->GetHTTPHeaderName().HasContent())
		{
#ifdef URL_ENABLE_DYNATTR_RECV_MULTIHEADER
			if(attr->GetIsPrefixHeader())
			{
				OpStringC8 hdr_prefix(attr->GetHTTPHeaderName());
				int prefix_len = hdr_prefix.Length();

				HeaderEntry *hdr = headerlist->First();
				while(hdr)
				{
					if(hdr->GetName().CompareI(hdr_prefix.CStr(), prefix_len)==0)
					{
						OpString8 value;
						ANCHOR(OpString8, value);
						BOOL recv = FALSE;

						value.SetL(hdr->GetValue());

						LEAVE_IF_ERROR(attr->OnRecvPrefixHTTPHeader(this_url,hdr->GetName(), value,recv));

						if(recv)
							url_ds->SetAttributeL(attr->GetAttributeID(), value);
					}
				}
			}
#ifdef URL_ENABLE_DYNATTR_RECV_HEADER
			else
#endif
#endif
#ifdef URL_ENABLE_DYNATTR_RECV_HEADER
			{
				OpString8 value;
				ANCHOR(OpString8, value);
				BOOL recv = FALSE;
				HeaderEntry *hdr = headerlist->GetHeader(attr->GetHTTPHeaderName().CStr());

				while(hdr)
				{
					if(value.HasContent())
						value.AppendL(",");
					value.AppendL(hdr->GetValue());

					hdr = headerlist->GetHeader(attr->GetHTTPHeaderName().CStr(),hdr);
				}

				LEAVE_IF_ERROR(attr->OnRecvHTTPHeader(this_url,value,recv));

				if(recv)
					url_ds->SetAttributeL(attr->GetAttributeID(), value);
			}
#endif
		}
		attr = attr->Suc();
	}
#endif


	if(hinfo->response == HTTP_NOT_MODIFIED )
	{
		url_ds->GetAttribute(URL::KVHTTP_ExpirationDate, &new_expiration_time);
		if (!new_expiration_time)
			url_ds->SetAttributeL(URL::KVHTTP_ExpirationDate, &old_expiration_time);
	}

#ifdef HTTP_BENCHMARK
	//if(bench_active)
	//	urlManager->MakeUnique(url);
#endif
/* start: cachepolicy 3*/
	{
		time_t expires = 0;

		url_ds->GetAttribute(URL::KVHTTP_ExpirationDate, &expires);
		if(expires &&
			g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysCheckNeverExpireGetQueries)  &&
			url_ds->GetAttribute(URL::KCachePolicy_AlwaysCheckNeverExpiredQueryURLs) &&
			(req && (req->method == HTTP_METHOD_GET || req->method == HTTP_METHOD_HEAD)) &&
			url->GetAttribute(URL::KQuery).HasContent())
			url_ds->SetAttributeL(URL::KCachePolicy_AlwaysVerify, TRUE);; // to comply with RFC 2616 sec 13.9
	}
	/* end: cachepolicy 3*/

#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	HandleStrictTransportSecurityHeader(headerlist);
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
	/* stat: refresh*/
	header = headerlist->GetHeaderByID(HTTP_Header_Refresh);
	if (header
#if URL_REDIRECTION_LENGTH_LIMIT >0
		&& header->GetValue().Length() < URL_REDIRECTION_LENGTH_LIMIT
#endif
		)
	{
		ParameterList *refresh_parameters=NULL;

		OP_STATUS op_err = GetParameters(refresh_parameters, header, PARAM_SEP_SEMICOLON | PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES, KeywordIndex_HTTP_General_Parameters);

		// CORE-29651: tolerate broken refresh header
		if(refresh_parameters && OpStatus::IsSuccess(op_err))
		{
			Parameters *item = refresh_parameters->First();
			if(item)
			{
				url_ds->SetAttributeL(URL::KHTTP_Refresh_Interval, item->GetUnsignedName());

				item = refresh_parameters->GetParameterByID(HTTP_General_Tag_URL, PARAMETER_ASSIGNED_ONLY);
				if(item)
					url_ds->SetAttributeL(URL::KHTTPRefreshUrlName, item->Value());
			}
		}
	}
/* end: refresh*/
/* start: content-disposition*/
	OpString8 original_suggested_name;
	ANCHOR(OpString8, original_suggested_name);
	OpString8 original_suggested_name_charset;
	ANCHOR(OpString8, original_suggested_name_charset);
	header = headerlist->GetHeaderByID(HTTP_Header_Content_Disposition);
	while(header)
	{
		ParameterList *disposition_parameters = NULL;

		OP_STATUS op_err = GetParameters(disposition_parameters, header, PARAM_SEP_SEMICOLON | PARAM_ONLY_SEP | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters);
		if(OpStatus::IsError(op_err))
		{
			if(OpStatus::IsMemoryError(op_err))
				LEAVE(op_err);
			disposition_parameters = NULL;
		}

		if(disposition_parameters && !disposition_parameters->Empty())
		{
#ifdef CONTENT_DISPOSITION_ATTACHMENT_FLAG
			if(disposition_parameters->First()->GetNameID()!= HTTP_General_Tag_Inline &&
				!disposition_parameters->First()->AssignedValue())
				url_ds->SetAttribute(URL::KUsedContentDispositionAttachment, TRUE);
#endif
			Parameters *filename = disposition_parameters->GetParameterByID(HTTP_General_Tag_Filename, PARAMETER_ASSIGNED_ONLY);
			if(filename)
			{
				original_suggested_name.SetL(filename->Value());
				original_suggested_name_charset.Set(filename->Charset());
			}
		}

		header = headerlist->GetHeaderByID(0, header);
	}
	//remove trailing dots from suggested filename for windows.
	if(original_suggested_name.Length() && original_suggested_name[original_suggested_name.Length()-1]=='.')
		for (;original_suggested_name.Length() && original_suggested_name[original_suggested_name.Length()-1]=='.';)
			original_suggested_name[original_suggested_name.Length()-1]=0;

	if(original_suggested_name.IsEmpty())
		url_ds->GetAttributeL(URL::KHTTPContentLocation, original_suggested_name);

/* end: content-disposition*/
/* start: partial content*/

	URL_Resumable_Status resumable = (hinfo->response == HTTP_PARTIAL_CONTENT ? Probably_Resumable : Resumable_Unknown);
	header = headerlist->GetHeaderByID(HTTP_Header_Accept_Ranges);
	while(header)
	{
		ParameterList *params = NULL;
		OP_STATUS op_err = GetParameters(params, header, PARAM_SEP_COMMA | PARAM_STRIP_ARG_QUOTES, KeywordIndex_HTTP_General_Parameters);
		if(OpStatus::IsMemoryError(op_err))
			LEAVE(op_err);

		if(params && OpStatus::IsSuccess(op_err))
		{
			if(params->ParameterExists(HTTP_General_Tag_Bytes))
			{
				resumable = Probably_Resumable;
				break;
			}

			if(params->ParameterExists(HTTP_General_Tag_None))
			{
				resumable = Not_Resumable;
				break;
			}
		}

		header = headerlist->GetHeaderByID(0,header);
	}
	url_ds->SetAttributeL(URL::KResumeSupported, resumable);

/* end: partial content*/
/* start: location*/
	if(url_ds->GetAttribute(URL::KHTTP_Location).HasContent())
	{
		if(!IsRedirectResponse(hinfo->response) && hinfo->response != HTTP_USE_PROXY)
		{
			// only let this be set by redirect responses
			url_ds->SetAttributeL(URL::KHTTP_Location, NULL);
		}
#ifdef DEBUG_HTTP_REQDUMP
		else
		{
			PrintfTofile("redir.txt","Received Redirect of %s to\n",url->Name());
			PrintfTofile("redir.txt","            %s \n",hptr->recvinfo.location);
		}
#endif // DEBUG_HTTP_REQDUMP
	}
/* end: location*/
/* start: digest authentication */
#ifdef HTTP_DIGEST_AUTH
	if(info.check_auth_info)
	{
		BOOL failed = FALSE;
		info.check_auth_info = !CheckAuthentication(headerlist, HTTP_Header_Authentication_Info, FALSE, hinfo->info.has_trailer, URL_ERRSTR(SI, ERR_HTTP_AUTH_FAILED), FALSE, failed);
		if(failed)
			return;
	}

	if(info.check_proxy_auth_info)
	{
		BOOL failed = FALSE;
		info.check_proxy_auth_info= !CheckAuthentication(headerlist, HTTP_Header_Proxy_Authentication_Info, TRUE, hinfo->info.has_trailer, URL_ERRSTR(SI, ERR_HTTP_PROXY_AUTH_FAILED), FALSE, failed);
		if(failed)
			return;
	}
#endif // HTTP_DIGEST_AUTH
/* end: digest authentication */

/* start: handle http 1.0 or better*/
		url_ds->SetAttributeL(URL::KHTTP_10_or_more, hinfo->info.http_10_or_more);
/* end: handle http 1.0 or better*/

/* start: handle response code*/

	if(hinfo->response != HTTP_NOT_MODIFIED)
		url_ds->SetAttributeL(URL::KHTTP_Response_Code, hinfo->response);

	int resp = hinfo->response;

	switch(resp)
	{
	case HTTP_UNAUTHORIZED:
		if(req->method == HTTP_METHOD_HEAD)
			break; // Only used to detect precense of server

		if(url->GetAttribute(URL::KSpecialRedirectRestriction))
			break; // Authentication not permitted for special URLs

		{
			HTTPAuth auth_status = (HTTPAuth) hptr->flags.auth_status;
			if(!SetupAuthenticationDataL(headerlist,HTTP_Header_WWW_Authenticate, hptr, hinfo, auth_status, resp))
				goto the_default_handler;
			hptr->flags.auth_status = auth_status;
		}
		break;

	case HTTP_PROXY_UNAUTHORIZED:
		{
			HTTP_Header_Tags hdr = HTTP_Header_Proxy_Authenticate;

			HTTPAuth auth_status = (HTTPAuth) hptr->flags.proxy_auth_status;
			if(!SetupAuthenticationDataL(headerlist, hdr, hptr, hinfo, auth_status, resp))
				goto the_default_handler;
			hptr->flags.proxy_auth_status = auth_status;
		}
		break;
	case HTTP_NOT_MODIFIED:
		{
			if (((HTTPAuth) hptr->flags.auth_status) != HTTP_AUTH_NOT)
				hptr->flags.auth_status = HTTP_AUTH_NOT;

			url_ds->UseOldCache();
#ifdef _DEBUG_DD1
			PrintfTofile("urldd1.txt","\nLHH HTTP_NOT_MODIFIED- %s\n",
				(url->Name() ? url->Name() : ""));
#endif // _DEBUG_DD1
			url_ds->SetAttributeL(URL::KLoadStatus, URL_LOADED);
			url_ds->SetAttributeL(URL::KReloading, FALSE);

			hptr->flags.redirect_blocked = FALSE;
			if(url_ds->GetAttribute(URL::KMovedToURL).IsValid() &&
				(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientPull, static_cast<const ServerName *>(url->GetAttribute(URL::KServerName, NULL))) ||
					url->GetAttribute(URL::KOverrideRedirectDisabled))
				&& !url->GetAttribute(URL::KSpecialRedirectRestriction))
			{
				// Redirect is not supported for restricted URLs
				if(OpStatus::IsSuccess(url_ds->ExecuteRedirect_Stage2()))
					break;
			}

			ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
			URL_LoadHandler::SetProgressInformation(REQUEST_FINISHED,0,
				server && server->UniName() ? server->UniName() : UNI_L(""));

#ifdef _PLUGIN_SUPPORT_
			// this will check if plugin and send the proper message
			URLContentType cnt_type;
			OpString path;
			ANCHOR(OpString, path);

			LEAVE_IF_ERROR(url->GetAttribute(URL::KUniPath, 0, path));

			LEAVE_IF_ERROR(url_ds->FindContentType(cnt_type, url_ds->GetAttribute(URL::KMIME_Type).CStr(),0, path.CStr()));

#endif // _PLUGIN_SUPPORT_

			url_ds->SendMessages(NULL, FALSE, MSG_NOT_MODIFIED, 0);
			url_ds->SetAttribute(URL::KHeaderLoaded,TRUE);
			hptr->flags.header_loaded_sent = TRUE;

			return;
		}
	case HTTP_MOVED:
	case HTTP_FOUND:
	case HTTP_SEE_OTHER:
	case HTTP_TEMPORARY_REDIRECT:
	case HTTP_USE_PROXY:
	case HTTP_OK:
	case HTTP_PARTIAL_CONTENT:
	case HTTP_MULTIPLE_CHOICES:
		if (((HTTPAuth) hptr->flags.auth_status) != HTTP_AUTH_NOT)
			hptr->flags.auth_status = HTTP_AUTH_NOT;
		if (((HTTPAuth) hptr->flags.proxy_auth_status) != HTTP_AUTH_NOT)
			hptr->flags.proxy_auth_status = HTTP_AUTH_NOT;
		break;

	default:
		{
the_default_handler:;
			if (((HTTPAuth) hptr->flags.auth_status) != HTTP_AUTH_NOT)
				hptr->flags.auth_status = HTTP_AUTH_NOT;
			if (((HTTPAuth) hptr->flags.proxy_auth_status) != HTTP_AUTH_NOT)
				hptr->flags.proxy_auth_status = HTTP_AUTH_NOT;
			if (resp == HTTP_EXPECTATION_FAILED && req->info.send_expect_100_continue)
				 break;
			 if(hinfo->response != HTTP_NO_CONTENT &&  hinfo->response != HTTP_RESET_CONTENT &&
				hinfo->response >= 100 && hinfo->response<400)
				break;
			if (hinfo->response == HTTP_NO_CONTENT)
			{
				if (hinfo->info.has_trailer)
				{
					headerlist->DuplicateHeadersL(&hinfo->headers,0);
 					hinfo->info.has_trailer = FALSE;
				}

				DeleteComm();

				mh->UnsetCallBacks(this);
				mh->UnsetCallBack(url_ds, MSG_COMM_DATA_READY, Id());
#ifdef _DEBUG_DD1
				PrintfTofile("urldd1.txt","\nLHH failure- %s\n",
					(url->Name() ? url->Name() : ""));
#endif // _DEBUG_DD1
				url_ds->SetAttribute(URL::KLoadStatus,(hinfo->response == HTTP_NO_CONTENT || hinfo->response == HTTP_RESET_CONTENT ?
														URL_LOADED : URL_LOADING_FAILURE));

				url_ds->SetAttributeL(URL::KHTTPResponseText, hinfo->response_text.CStr());
				int err = urlManager->ConvertHTTPResponseToErrorCode(url_ds->GetAttribute(URL::KHTTP_Response_Code));

				url_ds->SendMessages(NULL, TRUE, MSG_URL_LOADING_FAILED, err);
				url_ds->CleanMessageHandlers();

				return;
			}
		}
	}

	uint32 response = url_ds->GetAttribute(URL::KHTTP_Response_Code);

	url_ds->SetAttributeL(URL::KHTTPResponseText, hinfo->response_text.CStr());

/* end: handle response codes*/
/* start: content type */
	// BEGIN processed
	BOOL create_streamcache = FALSE;
	header = headerlist->GetHeaderByID(HTTP_Header_Content_Type);
	if (header)
	{
		// TODO: can this be improved?
		HeaderEntry *header1 = NULL;
		do{
			header1 = headerlist->GetHeaderByID(0,header);
			if(header1 != NULL)
				header = header1;
		}while(header1 != NULL);

		url_ds->SetAttribute(URL::KServerMIME_Type, header->GetValue());

		ParameterList *content_type = NULL;
		OP_STATUS op_err = GetParameters(content_type, header, PARAM_SEP_SEMICOLON | PARAM_SEP_COMMA | PARAM_STRIP_ARG_QUOTES | PARAM_NO_ASSIGN_FIRST_ONLY, KeywordIndex_HTTP_General_Parameters);

		if(OpStatus::IsError(op_err))
		{
			if(OpStatus::IsMemoryError(op_err))
				LEAVE(op_err);
			url_ds->SetAttributeL(URL::KMIME_Type, "application/octet-stream");
			content_type = NULL;
		}

		if(content_type && !content_type->Empty())
		{
			Parameters *item = content_type->First();
			URLContentType tmp_url_ct;
			OP_STATUS op_err;

			item->SetNameID(HTTP_General_Tag_Unknown);

			OpStringC8 stripped_name(item->GetName());
			BOOL illegal_name = FALSE;		  // TRUE if the Content type illegal (but "null" is considered legal)
			BOOL illegal_name_xhr = FALSE;	  // TRUE if the Content type illegal for XHR ("null" is considered illegal)

			// Bug CORE-23450: Only accept Content-Type of the form: token "/" token  (RFC 2616 2.2)
			// Check the type (first token)
			int i = (int)op_strspn(stripped_name.CStr(), "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~");

			if (stripped_name[i]!='/')
			{
				illegal_name_xhr = TRUE;		   // XHR consider it empty

				if(stripped_name.CompareI("null")) // "Content-Type: null" is allowed outside of XHR, because of CT-2273 and CORE-6605
					illegal_name = TRUE;
			}
			else
			{
				// Check the subtype (second token)
				int j = (int)op_strspn(stripped_name.CStr()+i+1, "!#$%&'*+-.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz|~");

				if (stripped_name[i+j+1]!=0)
				{
					illegal_name = TRUE;
					illegal_name_xhr = TRUE;
				}
			}

			url_ds->SetAttributeL(URL::KMIME_Type, (illegal_name? "application/octet-stream" : item->Name()));
			url_ds->SetAttributeL(URL::KMIME_Type_Was_NULL, illegal_name_xhr || item->GetName().IsEmpty());

			item = content_type->GetParameterByID(HTTP_General_Tag_Charset, PARAMETER_ASSIGNED_ONLY);
			if(item)
				url_ds->SetAttributeL(URL::KMIME_CharSet, item->Value());

			OpStringC8 content_type_temp(header->GetValue());
			if(
				g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TrustServerTypes) == FALSE &&
				(content_type_temp.Compare("text/plain") == 0 ||
				content_type_temp.Compare("text/plain; charset=iso-8859-1") == 0 ||
				content_type_temp.Compare("text/plain; charset=ISO-8859-1") == 0 ) )
			{
				tmp_url_ct = URL_UNDETERMINED_CONTENT;
			}
			else
			{
				OpString name;
				ANCHOR(OpString, name);
				OpString fileext;
				ANCHOR(OpString, fileext);

				LEAVE_IF_ERROR(url->GetAttribute(URL::KUniPath, 0, name));
				url->GetAttributeL(URL::KSuggestedFileNameExtension_L, fileext);


				RAISE_IF_ERROR(op_err = url_ds->FindContentType(tmp_url_ct,
					url_ds->GetAttribute(URL::KMIME_Type).CStr(),
					fileext.CStr(), name.CStr(), TRUE));
			}

			url_ds->SetAttributeL(URL::KContentType, tmp_url_ct);
// END Processed

#if MAX_PLUGIN_CACHE_LEN>0
			if(hinfo->content_length > MAX_PLUGIN_CACHE_LEN &&
				url_ds->GetAttribute(URL::KUseStreamCache)) // bug #272105 (don't stream swf-files) and CORE-25492
			{
				create_streamcache = TRUE;
			}
			else
#endif
			if( tmp_url_ct == URL_X_MIXED_REPLACE_CONTENT
#ifdef WBMULTIPART_MIXED_SUPPORT
			 || tmp_url_ct == URL_X_MIXED_BIN_REPLACE_CONTENT
#endif // WBMULTIPART_MIXED_SUPPORT
				)
			{
				HeaderEntry *boundary1 = headerlist->GetHeaderByID(HTTP_Header_Boundary,header);
				OpStringC8 boundary_header(boundary1 ? boundary1->Value() : NULL);
				url_ds->CreateMultipartCacheL(content_type, boundary_header);
				x_mixed_replace = TRUE;
			}
		}
		else
			url_ds->SetAttributeL(URL::KMIME_Type_Was_NULL, TRUE);
	}
	else
	{
#if MAX_PLUGIN_CACHE_LEN>0
		if(hinfo->content_length > MAX_PLUGIN_CACHE_LEN &&
			url_ds->GetAttribute(URL::KUseStreamCache)) // bug #272105 (don't stream swf-files) and CORE-25492
		{
			create_streamcache = TRUE;
		}
#endif
	}
	if(create_streamcache)
	{
		// Plugins and applets get everything this long as a stream, without any caching or processing
		if(OpStatus::IsError(url_ds->CreateStreamCache()))
		{
			mh->PostMessage(MSG_COMM_LOADING_FAILED, req->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			url->OnLoadFinished(URL_LOAD_FAILED);
			return;
		}
		urlManager->MakeUnique(url);
		OP_ASSERT(url->GetRefCount() > 0);
	}
/* end: content type */
/* start: suggested file name */
	if(original_suggested_name.HasContent())
	{
		OpString processed_name;
		ANCHOR(OpString, processed_name);

		// May need a pointer to the window
		Window *window = url_ds->GetFirstMessageHandler()
			? url_ds->GetFirstMessageHandler()->GetWindow()
			: NULL;

		// May need charset of referrer url
		URL referrer_url = url_ds->GetAttribute(URL::KReferrerURL);
		OpStringC8 referrer_url_charset = referrer_url.IsValid() ? referrer_url.GetAttribute(URL::KMIME_CharSet) : OpStringC8();

		ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);

		if(OpStatus::IsError(guessEncodingFromContextAndConvertString(
			&original_suggested_name,						// original_suggested_input
			&original_suggested_name_charset,				// original_suggested_input_charset
			url_ds->GetAttribute(URL::KMIME_CharSet).CStr(),// document_charset
			window,											// window
			server,											// Servername object
			referrer_url_charset.CStr(),					// referrer_url_charset
			g_pcdisplay->GetDefaultEncoding(),				// default_encoding
			g_op_system_info->GetSystemEncodingL(),			// system_encoding
			processed_name									// result_string
		)))
		{
			// Currently we try to continue with the result we got even if error.
			// This is better then stopping.
		}

		{
			// In case the string is utf8 escaped
			if (UriUnescape::ContainsValidEscapedUtf8(processed_name.CStr()))
				UriUnescape::ReplaceChars(processed_name.CStr(), UriUnescape::ConvertUtf8 | UriUnescape::Safe | UriUnescape::ExceptUnsafeHigh);

			const uni_char *val = processed_name.CStr();
			const uni_char *val1 = val + uni_strcspn(val,UNI_L(":/\\"));

			while(*val1)
			{
				val = val1 + 1;
				val1 = val + uni_strcspn(val,UNI_L(":/\\"));
			}

			RAISE_IF_ERROR(hptr->recvinfo.suggested_filename.Set(val, val1 - val));

			int idx = hptr->recvinfo.suggested_filename.FindFirstOf('?');
			if(idx != KNotFound)
				hptr->recvinfo.suggested_filename.Delete(idx);
		}
	}
/* end: suggested file name */
	// TODO: can this be improved?
/* start: content_length 1 */
	OpFileLength registered_len = 0;
	url_ds->GetAttribute(URL::KContentSize, &registered_len);
	if (!hinfo->content_length || registered_len != hinfo->content_length)
	{
		url_ds->SetAttributeL(URL::KContentSize, &hinfo->content_length);
	}

#ifdef WEB_TURBO_MODE
	url_ds->SetAttributeL(URL::KTurboTransferredBytes, &hinfo->turbo_transferred_bytes);
	url_ds->SetAttributeL(URL::KTurboOriginalTransferredBytes, &hinfo->turbo_orig_transferred_bytes);
#endif // WEB_TURBO_MODE
/* end: content_length 1 */
/* start: content encoding */
	header = headerlist->GetHeaderByID(HTTP_Header_Link);
	while(header)
	{
		ParameterList *params = NULL;
		OP_STATUS op_err = GetParameters(params, header, PARAM_SEP_COMMA| PARAM_NO_ASSIGN | PARAM_ONLY_SEP);
		if (OpStatus::IsMemoryError(op_err))
			LEAVE(op_err);

		if(params && OpStatus::IsSuccess(op_err))
		{
			Parameters *param = params->First();

			while(param)
			{
				OpStackAutoPtr<HTTP_Link_Relations> link(OP_NEW_L(HTTP_Link_Relations, ()));

				link->InitL(param->Name());
				link->Into(&hptr->recvinfo.link_relations);
				link.release();

				param = param->Suc();
			}
		}

		header = headerlist->GetHeaderByID(0, header);
	}

	if (url_ds->GetAttribute(URL::KReloading))
	{
		if (((HTTPAuth) hptr->flags.auth_status) == HTTP_AUTH_NOT &&
			((HTTPAuth) hptr->flags.proxy_auth_status) == HTTP_AUTH_NOT)
		{ // 20/06/97
			url_ds->SetAttribute(URL::KReloading, FALSE);
		}
	}

#if defined __OEM_EXTENDED_CACHE_MANAGEMENT && defined __OEM_OPERATOR_CACHE_MANAGEMENT
	if(response >= 200 && response< 400 &&
		(ServerName *) url->GetAttribute(URL::KServerName, NULL) &&
		((ServerName *) url->GetAttribute(URL::KServerName, NULL))->TestAndSetNeverFlushTrust() == NeverFlush_Trusted)
		url->SetAttribute(URL::KNeverFlush, TRUE); // For this mode; if a server is in the trusted list, the file is always neverflush
#endif

	// TODO: can this be improved?
	BOOL wait_for_err_msg_finished = (
		response != HTTP_OK &&
		response != HTTP_PARTIAL_CONTENT &&
		response != HTTP_FOUND &&
		response != HTTP_TEMPORARY_REDIRECT &&
		response != HTTP_MULTIPLE_CHOICES &&
		response != HTTP_MOVED); // not OK or redirected

	if ((req && req->method == HTTP_METHOD_HEAD)
		|| (url_ds->GetAttribute(URL::KIsResuming) && response < 300)
		|| ((URLContentType) url_ds->GetAttribute(URL::KContentType) != URL_UNDETERMINED_CONTENT
			&&
#ifdef _MIME_SUPPORT_
			(URLContentType) url_ds->GetAttribute(URL::KContentType) != URL_MIME_CONTENT
			&&
#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
			(URLContentType) url_ds->GetAttribute(URL::KContentType) != URL_MHTML_ARCHIVE
			&&
#endif
#endif // _MIME_SUPPORT_
			!x_mixed_replace
			&& !url_ds->GetAttribute(URL::KHeaderLoaded)
			&& !wait_for_err_msg_finished
			&& ((HTTPAuth) hptr->flags.auth_status) == HTTP_AUTH_NOT
			&& ((HTTPAuth) hptr->flags.proxy_auth_status) == HTTP_AUTH_NOT
			&& !(
				(!req || req->method != HTTP_METHOD_HEAD)
				&& g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientPull, static_cast<const ServerName *>(url->GetAttribute(URL::KServerName, NULL)))
				&& IsRedirectResponse(response)
				&& url_ds->GetAttribute(URL::KHTTP_Location).HasContent()
				)
			)
		)
	{
		url_ds->BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
		url->SetAttributeL(URL::KIsFollowed, TRUE);
		hptr->flags.header_loaded_sent = TRUE;
	}
	url_ds->SetAttributeL(URL::KHeaderLoaded, TRUE);

#ifdef WEB_TURBO_MODE
		if (response == HTTP_UNAUTHORIZED && wait_for_err_msg_finished && headerlist->GetHeader("X-OA", 0, HEADER_RESOLVE))
		{
			//Turbo 401s are handled entirely in the http layer, so data in url_ds is not reset correctly. We do that now instead.
			url_ds->SetAttribute(URL::KHeaderLoaded, FALSE);
		}
#endif // WEB_TURBO_MODE

	// Do redirect immediately, before content is loaded;
	// TODO: can this be improved?
	if( (!req || req->method != HTTP_METHOD_HEAD) &&
		IsRedirectResponse(response) &&
		url_ds->GetAttribute(URL::KHTTP_Location).HasContent())
	{
		BOOL perform_redirect = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableClientPull, static_cast<const ServerName *>(url->GetAttribute(URL::KServerName, NULL)));

		if((perform_redirect ||
			url->GetAttribute(URL::KOverrideRedirectDisabled)) &&
			!url->GetAttribute(URL::KSpecialRedirectRestriction))
		{
			// Redirect is not supported for restricted URLs
			OP_STATUS op_err;
			NormalCallCount blocker(this);
			RAISE_IF_ERROR(op_err = url_ds->ExecuteRedirect());
			if( OpStatus::IsError(op_err) )
			{
				mh->PostMessage(MSG_COMM_LOADING_FAILED, req->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				url->OnLoadFinished(URL_LOAD_FAILED);
				return;
			}
		}
	}

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	if( url_ds->GetAttribute(URL::KUsedForFileDownload) )
	{
		SetIsFileDownload(TRUE);
	}
#endif
}

#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
void URL_HTTP_LoadHandler::CheckForCookiesHandledL()
{
	CommState load_state;

	load_state = UpdateCookieL();

	if(load_state == COMM_REQUEST_WAITING)
		return;

	g_main_message_handler->UnsetCallBack(this,MSG_HTTP_COOKIE_LOADED);

	req->Load();
}
#endif // _ASK_COOKIE


HTTP_request_st *URL_HTTP_LoadHandler::GetServerAndRequestL()
{
	OpStackAutoPtr<HTTP_request_st> req(OP_NEW_L(HTTP_request_st, ()));

	URLType type = (URLType) url->GetAttribute(URL::KType);

	info.waiting = FALSE;

	req->connect_host = req->origin_host = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
	if (req->connect_host == NULL)
		return NULL;

	req->connect_port = req->origin_port = (unsigned short) url->GetAttribute(URL::KResolvedPort);
	req->userid = url->GetAttribute(URL::KUserName).CStr();
	req->password = url->GetAttribute(URL::KPassWord).CStr();

	const uni_char *proxy;
	const char *request = NULL;

	URL_HTTP_ProtocolData *hptr = url->GetDataStorage()->GetHTTPProtocolDataL();

	proxy = hptr->sendinfo.proxyname.CStr();

	if(!(proxy && *proxy)
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		&& g_pcnet->IsAutomaticProxyOn()
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
		)
	{
		proxy = urlManager->GetProxy(req->origin_host, type);
		if(proxy && !urlManager->UseProxyOnServer(req->origin_host, req->origin_port))
			proxy = NULL;
	}

	if(proxy && *proxy)
	{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		if (g_pcnet->IsAutomaticProxyOn())
		{
			GetAutoProxyL(proxy, req.get());
			if(type != URL_HTTPS && req->proxy != NO_PROXY)
				request = url->GetAttribute(URL::KName_Escaped).CStr();
		}
		else
#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
		{
			OP_STATUS op_err = OpStatus::OK;
			req->connect_host = urlManager->GetServerName(op_err, proxy, req->connect_port, TRUE);
			LEAVE_IF_ERROR(op_err);

			if(req->connect_host == NULL)
			{
				LEAVE(OpStatus::ERR_NO_MEMORY);
			}
#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
			if (req->connect_host->SocketAddress())
			{
				TRAPD(res, req->connect_host->SocketAddress()->SetNetworkLinkNameL(0)); // FIXME: OOM
			}
#endif // MULTIPLE_NETWORK_LINKS_SUPPORT
			if(req->connect_port == 0)
				req->connect_port = 80;
			if(type != URL_HTTPS)
				request = url->GetAttribute(URL::KName_Escaped).CStr();
			req->proxy = PROXY_HTTP;
		}
	}
	else if(type != URL_HTTP && type != URL_HTTPS)
			return NULL;

#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
	req->is_tunnel = !!url->GetAttribute(URL::KUsedAsTunnel);
#endif // URL_SIGNAL_PROTOCOL_TUNNEL

#ifdef WEB_TURBO_MODE
	req->use_turbo = !!url->GetAttribute(URL::KUsesTurbo);

	OpString8 orig_req;
	ANCHOR(OpString8, orig_req);

	if(request == NULL)
		url->GetAttributeL(URL::KPathAndQuery_L,orig_req);
	else
		orig_req.SetL(request);

	if( hptr->sendinfo.use_proxy_passthrough && type == URL_HTTP
#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
		&& !req->is_tunnel
#endif // URL_SIGNAL_PROTOCOL_TUNNEL
		)
	{
		int pos = orig_req.Find("://");
		char* actual_req = orig_req.CStr();
		if( pos != KNotFound )
			actual_req += (pos+3);
		req->request.AppendFormat("http://%s/%s",g_obml_config->GetTurboProxyName8(),actual_req);
		req->use_proxy_passthrough = TRUE;
	}
	else
		req->request.TakeOver(orig_req);
#else
	if(request == NULL)
		url->GetAttributeL(URL::KPathAndQuery_L,req->request);
	else
		req->request.SetL(request);
#endif // WEB_TURBO_MODE

	if(req->request.IsEmpty())
		req->request.SetL("/");

	if(req->proxy != NO_PROXY && type != URL_HTTPS)
	{
		req->path = op_strstr(req->request.CStr(),"://");
		if(req->path)
			req->path = op_strchr(req->path+3,'/');
	}
	else
		req->path = req->request.CStr();

	return req.release();
}

CommState URL_HTTP_LoadHandler::Load()
{
	OP_MEMORY_VAR CommState ret = COMM_REQUEST_FAILED;
	TRAPD(op_err, ret = LoadL());

	if(OpStatus::IsError(op_err))
	{
		if(OpStatus::IsMemoryError(op_err))
			g_memory_manager->RaiseCondition(op_err);
		ret = COMM_REQUEST_FAILED;
	}

	if(ret == COMM_REQUEST_FAILED && req)
	{
		DeleteComm();
	}

	return ret;
}

CommState URL_HTTP_LoadHandler::LoadL()
{
	OP_STATUS op_err = OpStatus::OK;
/* stat: setup HTTP_request_st */
	HTTP_request_st *request = NULL;
	OpStackAutoPtr<HTTP_request_st> request1(0);

	info.waiting = FALSE;

	request1.reset(request = GetServerAndRequestL()); // request and request1 point to the same thing

	if(request == NULL)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		return (info.waiting ? COMM_REQUEST_WAITING : COMM_REQUEST_FAILED);
	}

	if(request->origin_host == NULL || request->origin_host->Name() == NULL ||
		*(request->origin_host->Name()) == '\0')
	{
		url->GetDataStorage()->BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND), MH_LIST_ALL);
		return COMM_REQUEST_FAILED;
	}
/* end: setup HTTP_request_st*/

/* start: get variables */
	URLType type = (URLType) url->GetAttribute(URL::KType);
	URL_DataStorage *url_ds = url->GetDataStorage();
	if(!url_ds)
		return COMM_REQUEST_FAILED;

	URL_HTTP_ProtocolData *hptr = url_ds->GetHTTPProtocolDataL();
/* end: get variables */
/* start: determine method*/
	BOOL http_req = (type == URL_HTTP || type == URL_HTTPS);
	HTTP_Method method = http_req ? (HTTP_Method) url_ds->GetAttribute(URL::KHTTP_Method) : HTTP_METHOD_GET; // if url type is URL_HTTP or URL_HTTPS then get method from the request else set it to HTTP_METHOD:GET

	if(method == HTTP_METHOD_Invalid)
	{
		url->GetDataStorage()->BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_HTTP_METHOD_NOT_ALLOWED), MH_LIST_ALL);
		return COMM_REQUEST_FAILED;
	}
/* end: determine method*/

#ifdef _EMBEDDED_BYTEMOBILE
	if (GetPredicted())
	{
		request->predicted = TRUE;
		request->predicted_depth = GetPredictedDepth();
	}
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEBSERVER_SUPPORT
	if (GetUniteConnection())
	{
		request->unite_connection = TRUE;
		if (GetUniteAdminConnection())
			request->unite_admin_connection = TRUE;
	}
#endif // WEBSERVER_SUPPORT

	hptr->flags.redirect_blocked = TRUE;
	hptr->flags.connect_auth = FALSE;
	hptr->sendinfo.redirect_count=0;

	UINT timeout = url->GetAttribute(URL::KTimeoutMaxTCPConnectionEstablished);

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	BOOL open_extra_idle_connections = (url->GetDataStorage()->GetWasInline() || url->GetDataStorage()->GetWasUserInitiated()) && (request->proxy != PROXY_HTTP || type != URL_HTTPS);
	req = OP_NEW_L(HTTP_Request, (mh, method, request, timeout, open_extra_idle_connections, (type == URL_HTTPS)));
#else
	BOOL open_extra_idle_connections = url->GetDataStorage()->GetWasInline() || url->GetDataStorage()->GetWasUserInitiated();
	req = OP_NEW_L(HTTP_Request, (mh, method, request, timeout, open_extra_idle_connections));
#endif // _SSL_SUPPORT_ || _NATIVE_SSL_SUPPORT_ || defined(_CERTICOM_SSL_SUPPORT_)

	request1.release();

#if (defined EXTERNAL_ACCEPTLANGUAGE || defined EXTERNAL_HTTP_HEADERS)
	req->SetContextId(url->GetContextId());
#endif // EXTERNAL_ACCEPTLANGUAGE

	req->ConstructL(
#ifdef IMODE_EXTENSIONS
		!!GetURL()->GetAttribute(URL::KSetUseUtn)
#endif
	);

	req->SetPrivacyMode(urlManager->GetContextIsRAM_Only(url->GetContextId()));
	if(method == HTTP_METHOD_String)
	{
		OpString8 method_string;

		url->GetAttributeL(URL::KHTTPSpecialMethodStr, method_string);
		LEAVE_IF_ERROR(req->SetMethod(method, method_string));
	}

	req->SetMsgID((MH_PARAM_1) url->GetID());
	req->SetPriority(url->GetAttribute(URL::KHTTP_Priority));
#ifdef HTTP_CONTENT_USAGE_INDICATION
	req->SetUsageIndication((HTTP_ContentUsageIndication)url->GetAttribute(URL::KHTTP_ContentUsageIndication));
#endif // HTTP_CONTENT_USAGE_INDICATION

#ifdef URL_DISABLE_INTERACTION
	if(url->GetAttribute(URL::KBlockUserInteraction))
		req->SetUserInteractionBlocked(TRUE);
#endif

	//Only use the nettype checks if not using a proxy.
//	if(req->info.proxy_request && request->origin_host->GetNetType() == NETTYPE_UNDETERMINED)
//			request->origin_host->SetNetType(NETTYPE_PUBLIC);
	if (!req->info.proxy_request)
		req->Set_NetType((OpSocketAddressNetType) url_ds->GetAttribute(URL::KLimitNetType));
	if((req->Get_NetType() != NETTYPE_UNDETERMINED &&
		!req->info.proxy_request && req->Get_NetType() > request->origin_host->GetNetType() &&
		!(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, request->origin_host))
		) /*||
		(req->info.proxy_request && request->origin_host->GetNetType() <= NETTYPE_PRIVATE)*/)
	{
		if (mh)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(),
							(request->origin_host->GetNetType() == NETTYPE_LOCALHOST &&
							   !request->origin_host->GetIsLocalHost() &&
							   request->origin_host->GetName().Compare("localhost") != 0 ?
							ERR_SSL_ERROR_HANDLED : (int) GetCrossNetworkError(req->Get_NetType(), request->origin_host->GetNetType())));
		url->OnLoadFinished(URL_LOAD_FAILED);
		return COMM_LOADING; // error message will arrive
	}

	if(hptr->sendinfo.external_headers)
	{
		req->SetExternalHeaders(hptr->sendinfo.external_headers); // ownership over headers taken
		hptr->sendinfo.external_headers=NULL;
	}
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	if (GetLoadDirect())
		req->SetLoadDirect();
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

#ifdef URL_ALLOW_DISABLE_COMPRESS
	if (url->GetAttribute(URL::KDisableCompress))
		req->SetDisableCompress(TRUE);
#endif

	URL this_url(url, (const char *) NULL);
	ANCHOR(URL, this_url);

#if defined URL_ENABLE_DYNATTR_SEND_MULTIHEADER || defined URL_ENABLE_DYNATTR_SEND_HEADER
	URL_DynamicStringAttributeDescriptor *attr = NULL;

	urlManager->GetFirstAttributeDescriptor(&attr);

	while(attr)
	{
		if(attr->GetSendHTTP() && attr->GetHTTPHeaderName().HasContent())
		{
#ifdef URL_ENABLE_DYNATTR_SEND_MULTIHEADER
			if(attr->GetIsPrefixHeader())
			{
				do{
					const char * const*headers= NULL;
					int i,count=0;

					attr->GetSendHTTPHeaders(this_url, headers, count);
					if(count == 0 || headers == NULL)
						break;

					OpString8 original_value;
					ANCHOR(OpString8, original_value);
					BOOL send = FALSE;

					url_ds->GetAttributeL(attr->GetAttributeID(), original_value);

					for(i = 0; i< count; i++)
					{
						OpString8 value;
						ANCHOR(OpString8, value);

						value.SetL(original_value);
						LEAVE_IF_ERROR(attr->OnSendPrefixHTTPHeader(this_url,headers[i], value,send));

						if(send && value.HasContent())
						{
							req->Headers.SetSeparatorL(headers[i], SEPARATOR_COMMA);
							req->Headers.AddParameterL(headers[i], value);
						}
					}
				}while(0);
			}
#ifdef URL_ENABLE_DYNATTR_SEND_HEADER
			else
#endif
#endif
#ifdef URL_ENABLE_DYNATTR_SEND_HEADER
			{
				OpString8 value;
				ANCHOR(OpString8, value);
				BOOL send = FALSE;

				url_ds->GetAttributeL(attr->GetAttributeID(), value);

				LEAVE_IF_ERROR(attr->OnSendHTTPHeader(this_url,value,send));

				if(send && value.HasContent())
				{
					req->Headers.SetSeparatorL(attr->GetHTTPHeaderName(), SEPARATOR_COMMA);
					req->Headers.AddParameterL(attr->GetHTTPHeaderName(), value);
				}
			}
#endif
		}
		attr = attr->Suc();
	}
#endif

	hptr->flags.header_loaded_sent = FALSE;

	if (type != URL_HTTPS )
		SetProgressInformation(SET_SECURITYLEVEL,SECURITY_STATE_NONE);

	BOOL do_not_load = FALSE;

	if (http_req)
	{
		ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);

#ifdef MULTIPLE_NETWORK_LINKS_SUPPORT
		OpSocketAddress *saddr = server ? server->SocketAddress() : 0;
		if (saddr)
		{
			TRAPD(res, saddr->SetNetworkLinkNameL(0));
			if (OpStatus::IsError(res))
			{
				DeleteComm();
				g_memory_manager->RaiseCondition(res);
				return COMM_REQUEST_FAILED;
			}
		}
#endif // MULTIPLE_NETWORK_LINKS_SUPPORT

		if(server && server->GetOverRideUserAgent() != UA_NotOverridden)
		{
			hptr->flags.used_ua_id = server->GetOverRideUserAgent();
		}
		else
		{
			hptr->flags.used_ua_id = g_uaManager->GetUABaseId();
		}
/* start: referer */
		URL referrer =  url_ds->GetAttribute(URL::KReferrerURL);
		if (referrer.IsValid() && !referrer.GetAttribute(URL::KDontUseAsReferrer))
		{
			URLType reftyp = (URLType) referrer.GetAttribute(URL::KType);
			if(reftyp == URL_HTTP || (reftyp == URL_HTTPS && type == URL_HTTPS))
			{
				OpString8 ref;
				referrer.GetAttribute(URL::KName_Escaped, ref);

				req->SetRefererL(ref);
			}
		}
/* end: referer */
		{
			CommState cookie_stat = UpdateCookieL();

			if(cookie_stat == COMM_REQUEST_WAITING)
				do_not_load = TRUE;
			else if(cookie_stat != COMM_LOADING)
				return COMM_REQUEST_FAILED;
		}
/* start: post */
		if (GetMethodHasRequestBody(method) && (hptr->sendinfo.http_data.HasContent() || hptr->sendinfo.upload_data))
		{
			if (hptr->flags.http_data_new || url_ds->GetAttribute(URL::KReloading))
			{
#ifdef HAS_SET_HTTP_DATA
				if(hptr->sendinfo.upload_data)
				{
					hptr->sendinfo.upload_data->ResetL();
					req->SetData(hptr->sendinfo.upload_data);
				}
				else
#endif // HAS_SET_HTTP_DATA
				{
					if (hptr->sendinfo.http_data.HasContent())
					{
						req->SetDataL(hptr->sendinfo.http_data, hptr->flags.http_data_with_header);
					}
					if (url_ds->GetAttribute(URL::KHTTP_ContentType).HasContent())
						req->SetRequestContentTypeL(url_ds->GetAttribute(URL::KHTTP_ContentType));
				}

				if (url_ds->GetAttribute(URL::KHTTP_ContentType).HasContent() &&
					req->GetContentType().IsEmpty() &&
					!(hptr->sendinfo.upload_data && hptr->sendinfo.upload_data->GetContentType().Length()))
					req->SetRequestContentTypeL(url_ds->GetAttribute(URL::KHTTP_ContentType));

				hptr->flags.http_data_new = FALSE;
				hptr->flags.only_if_modified = FALSE;
			}
			else
			{
				url_ds->SetAttributeL(URL::KLoadStatus, URL_LOADED);
				url_ds->SetAttributeL(URL::KContentType, URL_HTML_CONTENT);
				url_ds->SetAttributeL(URL::KHeaderLoaded,TRUE);
				url_ds->BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);
				hptr->flags.header_loaded_sent = TRUE;
				url_ds->BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
				url_ds->CleanMessageHandlers();
				return COMM_LOADING;
			}
		}
	}

/* end: post */
/* start: range header */

	{
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
		OpFileLength range_start=0;
		OpFileLength range_end=0;
		url->GetAttribute(URL::KHTTPRangeStart, &range_start);
		url->GetAttribute(URL::KHTTPRangeEnd, &range_end);
#endif

		if(url_ds->GetAttribute(URL::KIsResuming)
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
			|| range_start!=FILE_LENGTH_NONE || range_end!=FILE_LENGTH_NONE
#endif
			)
		{
#ifdef URL_ENABLE_HTTP_RANGE_SPEC
			if(range_start!=FILE_LENGTH_NONE || range_end!=FILE_LENGTH_NONE)
			{
				req->SetRangeStart(range_start);
				req->SetRangeEnd(range_end);
			}
			else
#endif
			{
				OpFileLength registered_len=0;
				url->GetAttribute(URL::KContentLoaded, &registered_len);

				req->SetRangeStart(registered_len);
			}
			if(http_req)
			{
				OpStringC8 temp_entity(url_ds->GetAttribute(URL::KHTTP_EntityTag));

				if (temp_entity.HasContent() && temp_entity.Compare("W/",2) != 0)
					req->SetIfRangeL(temp_entity);
				else
					req->SetIfRangeL(url_ds->GetAttribute(URL::KHTTP_LastModified));
			}
			hptr->flags.only_if_modified = FALSE;
		}
/* end: range header */
/* start: if-modified-since / if-none-match / prama / Cache-Control*/
		if (hptr->flags.only_if_modified )
		{
			int response_code = url_ds->GetAttribute(URL::KHTTP_Response_Code);
			time_t expire =0;
			url_ds->GetAttribute(URL::KVHTTP_ExpirationDate, &expire);

			if(response_code < 400 &&
				!(response_code == HTTP_FOUND && expire == 0) &&
				!((response_code == HTTP_FOUND) && expire))
			{
				OpStringC8 temp_lastmod(url_ds->GetAttribute(URL::KHTTP_LastModified));

				if (temp_lastmod.HasContent())
 					req->SetIfModifiedSinceL(temp_lastmod);
				//else
				//	req->SetIfModifiedSinceL(url_ds->GetAttribute(URL::KHTTP_Date));

				OpStringC8 temp_etag(url_ds->GetAttribute(URL::KHTTP_EntityTag));
				req->SetIfNoneMatchL(temp_etag);

				if (temp_lastmod.HasContent() || temp_etag.HasContent())
					hptr->flags.added_conditional_headers = TRUE;
			}
			else
			{
				req->SetProxyNoCache(TRUE);
			}
		}

		if (hptr->flags.proxy_no_cache /* && request->proxy */)
		{
			req->SetProxyNoCache(TRUE);
		}
/* end if-modified-since / if-none-match / prama / Cache-Control*/

		req->SetIsFormsRequest(!!url->GetAttribute(URL::KHTTPIsFormsRequest));

		hptr->flags.auth_status = HTTP_AUTH_NOT;
		hptr->flags.proxy_auth_status = HTTP_AUTH_NOT;

		const char *req_user_id = request->userid;
		const char *req_passwd = request->password;

		if( req_user_id!= NULL ||
			req_passwd != NULL)
		{
			int user_passwd_len =  (req_user_id ? op_strlen(req_user_id) : 0) +
				(req_passwd ? op_strlen(req_passwd) : 0) +1 ;
			char *auth_temp = (char *) g_memory_manager->GetTempBuf2k();

			if( user_passwd_len +1 <= (int)g_memory_manager->GetTempBuf2kLen() )
			{
				auth_temp[0] = 0;
				StrMultiCat(auth_temp, req_user_id, ":", req_passwd);
				user_passwd_len = UriUnescape::ReplaceChars(auth_temp, UriUnescape::NonCtrlAndEsc);

				int auth_str_len = ((user_passwd_len+3)*4)/3 + 1;
				// Won't need ANCHOR for this
				OpString8 auth_str;
				ANCHOR(OpString8, auth_str);

				auth_str.ReserveL(auth_str_len+STRINGLENGTH("Basic "));

				auth_str.Append("Basic ");
    			UU_encode((unsigned char*)auth_temp, user_passwd_len, auth_str.DataPtr()+STRINGLENGTH("Basic "));
    			req->SetAuthorization(auth_str);
				url->SetAttributeL(URL::KHaveAuthentication, TRUE);
				hptr->flags.auth_status = HTTP_AUTH_HAS;
				OPERA_cleanse_heap(auth_temp, user_passwd_len);
			}
		}

#if defined(_ENABLE_AUTHENTICATE) && defined(_USE_PREAUTHENTICATION_)
		if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowPreSendAuthentication))
		{
			if(request->connect_host != request->origin_host || request->connect_port != request->origin_port)
			{
				AuthElm *auth = request->connect_host->Get_Auth(NULL /*request->use_proxy_realm*/,
					request->connect_port, NULL, URL_HTTP, AUTH_SCHEME_HTTP_PROXY, 0);

				if(auth)
				{
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
					AuthScheme scheme = (auth->GetScheme());
					if(scheme == AUTH_SCHEME_HTTP_PROXY_NTLM || scheme == AUTH_SCHEME_HTTP_PROXY_NEGOTIATE || scheme == AUTH_SCHEME_HTTP_PROXY_NTLM_NEGOTIATE)
						req->proxy_ntlm_auth_element = auth;
					else
#endif
					{
						OpStringS8 auth_str;
						ANCHOR(OpStringS8, auth_str);

						op_err = auth->GetAuthString(auth_str, url, req);
						if (OpStatus::IsError(op_err))
						{
							RAISE_IF_MEMORY_ERROR(op_err);
							DeleteComm();
							return COMM_REQUEST_FAILED;
						}
						req->SetProxyAuthorization(auth_str);
						req->SetProxyAuthorizationId(auth->GetId());
#ifdef HTTP_DIGEST_AUTH
						if(auth->GetScheme() == AUTH_SCHEME_HTTP_PROXY_DIGEST)
							info.check_proxy_auth_info = TRUE;
#endif // HTTP_DIGEST_AUTH
					}
					hptr->flags.proxy_auth_status = HTTP_AUTH_HAS;

					if (auth && auth->GetAuthenticateOnce())
					{
						URL_DataStorage *url_ds;
						URL_HTTP_ProtocolData *hptr;

						if ((url_ds = url->GetDataStorage()) != NULL && (hptr = url_ds->GetHTTPProtocolData()) != NULL && hptr->flags.auth_has_credetentials)
						{
							hptr->flags.auth_credetentials_used = FALSE;
						}
						auth->Out();
						OP_DELETE(auth);
						auth = NULL;
					}
				}

			}
			AuthElm *auth = request->origin_host->Get_Auth(NULL /*request->use_realm*/,
				request->origin_port, request->path, type, AUTH_SCHEME_HTTP_UNKNOWN
				, url->GetContextId());

			if(auth)
			{
				if(url->GetAttribute(URL::KUserName).CStr() || url->GetAttribute(URL::KPassWord).CStr())
				{
					if(url->GetAttribute(URL::KUserName).Compare(auth->GetUser()) != 0)
					{
						// redirect to non-username URL
						OpString8 temp_url;
						url->GetAttribute(URL::KName_With_Fragment_Escaped,temp_url);

						URL redirect_url = urlManager->GetURL(temp_url);
						hptr->flags.redirect_blocked = FALSE;
						TRAPD(op_err, url_ds->SetAttributeL(URL::KMovedToURL, redirect_url));
						if(OpStatus::IsSuccess(op_err))
							url_ds->ExecuteRedirect_Stage2();
						mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
						url->OnLoadFinished(URL_LOAD_FINISHED);
						return COMM_LOADING;
					}
				}
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
				AuthScheme scheme = (auth->GetScheme());
				if(scheme == AUTH_SCHEME_HTTP_NTLM || scheme == AUTH_SCHEME_HTTP_NEGOTIATE || scheme == AUTH_SCHEME_HTTP_NTLM_NEGOTIATE)
					req->server_negotiate_auth_element = auth;
				else
#endif
				{
					OpString8 auth_str;
					ANCHOR(OpStringS8, auth_str);

					LEAVE_IF_ERROR(auth->GetAuthString(auth_str, url, req));
					req->SetAuthorization(auth_str);
					req->SetAuthorizationId(auth->GetId());
#ifdef HTTP_DIGEST_AUTH
					if(auth->GetScheme() == AUTH_SCHEME_HTTP_DIGEST)
						info.check_auth_info = TRUE;
#endif // HTTP_DIGEST_AUTH
				}
				url->SetAttributeL(URL::KHaveAuthentication, TRUE);
				hptr->flags.auth_status = HTTP_AUTH_HAS;
				}
		}
#endif // _ENABLE_AUTHENTICATE) && _USE_PREAUTHENTICATION_
	}

	req->SetSendAcceptEncodingL(!!url_ds->GetAttribute(URL::KSendAcceptEncoding));
	if(url_ds->GetAttribute(URL::KReloadSameTarget))
		req->info.disable_automatic_refetch_on_error = TRUE;

	req->info.managed_request = (url_ds->GetAttribute(URL::KHTTP_Managed_Connection) ? 1 : 0);
	url_ds->SetAttribute(URL::KHTTP_Managed_Connection, FALSE);

	req->ChangeParent(this);
	LEAVE_IF_ERROR(req->SetCallbacks(this,this));

	if (!do_not_load)
	{
		CommState state = req->Load();
		if (state == COMM_REQUEST_FAILED)
		{
			DeleteComm();
		}
		return state;
	}
	else
		return COMM_LOADING;
}

void URL_HTTP_LoadHandler::ProcessReceivedData()
{
	URL_LoadHandler::ProcessReceivedData();
}

CommState URL_HTTP_LoadHandler::UpdateCookieL()
{
	// Assumes url->storage != NULL (Invariant)
	if(url->GetAttribute(URL::KDisableProcessCookies))
		return COMM_LOADING; // No need to check any further if we will not send any cookies anyway.

#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
	int cookies_ask_mode;
	if((cookies_ask_mode = urlManager->HandlingCookieForURL(url, url->GetContextId())) != 0)
	{
		if(!g_main_message_handler->HasCallBack(this,MSG_HTTP_COOKIE_LOADED,0) && OpStatus::IsError(g_main_message_handler->SetCallBack(this,MSG_HTTP_COOKIE_LOADED,0)))
			return COMM_REQUEST_FAILED;
		SetProgressInformation((cookies_ask_mode == 1 ? WAITING_FOR_COOKIES : WAITING_FOR_COOKIES_DNS) ,0,0);
		return COMM_REQUEST_WAITING;
	}
	else
#endif // _ASK_COOKIE
	{
		BOOL have_password = FALSE;
		BOOL have_authentication = FALSE;
		{
			int version=0, max_version=0;
			const char *cookies = NULL;

			cookies = urlManager->GetCookiesL(url, version, max_version,
				!!url->GetAttribute(URL::KHavePassword),
				!!url->GetAttribute(URL::KHaveAuthentication),
				have_password,
				have_authentication,
				url->GetContextId(),
				TRUE
				);
			req->SetCookieL(cookies, version, max_version);
		}
		if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TagUrlsUsingPasswordRelatedCookies))
		{
			if(have_password)
				url->SetAttributeL(URL::KHavePassword, TRUE);
			if(have_authentication)
				url->SetAttributeL(URL::KHaveAuthentication, TRUE);
		}
	}
	return COMM_LOADING;
}


unsigned URL_HTTP_LoadHandler::ReadData(char *buffer, unsigned buffer_len)
{
	if(!req)
		return 0;
	URL_DataStorage *url_ds = url->GetDataStorage();
	if(!url_ds->GetAttribute(URL::KHeaderLoaded))
	{
		mh->PostDelayedMessage(MSG_COMM_DATA_READY, Id(),0,100);
		return 0;
	}
	unsigned rlen;

#ifdef STRICT_CACHE_LIMIT
	/* Abort download if this piece of data will use more than available memory
	 * This has a minor bug:  The previously viewed document can not be discarded
	 * from the cache until the current document is loaded.
	 */
//	printf("Examining size of %s in ReadData (req) (ct=%i)\n", url->Name(), url_ds->GetCacheType());
	if (url_ds->GetAttribute(URL::KCacheType) != URL_CACHE_USERFILE)
	{
		if (url_ds->GetCacheStorage()!=0)
		{
			urlManager->SubFromRamCacheSizeExtra(ramcacheextra);
			ramcacheextra=url_ds->GetCacheStorage()->ResourcesUsed(MEMORY_RESOURCE);
			urlManager->AddToRamCacheSizeExtra(ramcacheextra);
		};

		if (urlManager->GetUsedUnderMemoryLimit() + buffer_len > urlManager->GetMemoryLimit())
		{
			urlManager->MakeRoomUnderMemoryLimit(buffer_len);

			if (urlManager->GetUsedUnderMemoryLimit() + buffer_len > urlManager->GetMemoryLimit())
			{
				Stop();
				if (tryagain)
				{
					// FIXME: Ugly, but probably safe in this case
					// FIXME: How about ReadData calls before this message is handled?
					mh->PostDelayedMessage(MSG_URL_RELOAD, (int)url, 0, 100 );
					tryagain = FALSE;
				}
				else
				{
					mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(D, ERR_LOADING_ABORTED_OOM));
					url->LoadFinished(URL_LOAD_FAILED);
				};
			};
		};
	};
#endif // STRICT_CACHE_LIMIT

	rlen = req->ReadData(buffer, buffer_len);

	req->SetProgressInformation(LOAD_PROGRESS, rlen, req->HostName()->UniName());

	return rlen;
}

void URL_HTTP_LoadHandler::EndLoading(BOOL force)
{
	NormalCallCount blocker(this);
	if(req)
	{
		if(force)
			req->EndLoading();
		else
			req->Stop();
	}
}

void URL_HTTP_LoadHandler::DeleteComm()
{
	if(req)
	{
		g_main_message_handler->RemoveCallBacks(this, req->Id());
		g_main_message_handler->UnsetCallBacks(req);

		req->EndLoading();
		SafeDestruction(req);
		req = NULL;
	}
}

void URL_HTTP_LoadHandler::DisableAutomaticRefetching()
{
	if(req)
		req->info.disable_automatic_refetch_on_error = TRUE;
}

void URL_HTTP_LoadHandler::SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1,
											 const void *progress_info2)
{
	NormalCallCount blocker(this);
	switch(progress_level)
	{
	case HEADER_LOADED:
		{
			TRAPD(op_err, HandleHeaderLoadedL());
			if(OpStatus::IsError(op_err))
			{
				g_memory_manager->RaiseCondition(op_err);
				HandleLoadingFailed(Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			}
		}
		break;
	case CONNECT_FINISHED:
#ifdef HTTP_DIGEST_AUTH
		{
			HeaderInfo *hinfo = req->GetHeader();
			if(hinfo && info.check_proxy_auth_info)
			{
				HeaderList *headerlist = (hinfo->info.has_trailer ? &hinfo->trailingheaders : &hinfo->headers);
				BOOL failed = FALSE;
				info.check_proxy_auth_info = !CheckAuthentication(headerlist, HTTP_Header_Proxy_Authentication_Info, TRUE, TRUE, URL_ERRSTR(SI, ERR_HTTP_PROXY_AUTH_FAILED), TRUE, failed);
			}
		}
#endif // HTTP_DIGEST_AUTH
		break;
	case HANDLE_SECONDARY_DATA:
		{
			if(req)
				req->SetReadSecondaryData();
			ProcessReceivedData();
			break;
		}
	case RESTART_LOADING:
		{
			URL_DataStorage *url_ds = url->GetDataStorage();
			if(url_ds)
			{
				URL_HTTP_ProtocolData *hptr = url_ds->GetHTTPProtocolData();
				BOOL header_loaded_sent = hptr ? hptr->flags.header_loaded_sent : !!url_ds->GetAttribute(URL::KHeaderLoaded);

				url_ds->MoveCacheToOld(TRUE);
				url_ds->SetAttribute(URL::KHeaderLoaded,FALSE);
				if(hptr)
					hptr->flags.header_loaded_sent = FALSE;
				if(header_loaded_sent)
				{
					url_ds->BroadcastMessage(MSG_MULTIPART_RELOAD, url->GetID(), 0, MH_LIST_ALL);
					url_ds->BroadcastMessage(MSG_INLINE_REPLACE, url->GetID(), 0, MH_LIST_ALL);
				}
			}
		}
		break;
	case GET_APPLICATIONINFO:
		{
			URL *target = (URL *) progress_info2;

			if(url)
			{
				URL temp(url, (char *)NULL);
				*target = temp;
			}
		}
		break;
	case REPORT_LOAD_STATUS:
#ifdef SCOPE_RESOURCE_MANAGER
		ReportLoadStatus(progress_info1, progress_info2);
#endif // SCOPE_RESOURCE_MANAGER
		break;
#ifdef ABSTRACT_CERTIFICATES
	case SET_SECURITYLEVEL:
		{
			if( progress_info1 != SECURITY_STATE_NONE
				&& url->GetAttribute(URL::KType) == URL_HTTPS
				&& url->GetAttribute(URL::KCertificateRequested)
				&& url->GetAttribute(URL::KSecurityStatus) == SECURITY_STATE_UNKNOWN )
			{
				TRAPD(err,url->SetAttributeL(URL::KRequestedCertificate, req->ExtractCertificate()));
				OpStatus::Ignore(err);
			}
		}
		// Do not break!
#endif // ABSTRACT_CERTIFICATES
	case REQUEST_FINISHED:
	default:
		URL_LoadHandler::SetProgressInformation(progress_level,progress_info1, progress_info2);
	}
}

#ifdef SCOPE_RESOURCE_MANAGER
void URL_HTTP_LoadHandler::ReportLoadStatus(unsigned long progress_info1, const void *progress_info2)
{
	if (!OpScopeResourceListener::IsEnabled() || !progress_info2)
		return;

	union
	{
		const HTTP_Request::LoadStatusData_Compose* compose;
		const HTTP_Request::LoadStatusData_Raw* raw;
		HTTP_Request* req;
	} u;

	switch (progress_info1)
	{
	case OpScopeResourceListener::LS_COMPOSE_REQUEST:
		u.compose = static_cast<const HTTP_Request::LoadStatusData_Compose*>(progress_info2);

		// If this request already has a request_id, it means we are retrying this request.
		if (u.compose->orig_id != 0 && u.compose->req->GetRequestNumber() != u.compose->orig_id)
			OpScopeResourceListener::OnRequestRetry(url, u.compose->orig_id, u.compose->req->GetRequestNumber());

		OpScopeResourceListener::OnComposeRequest(url, u.compose->req->GetRequestNumber(), u.compose->req, mh->GetDocumentManager(), mh->GetWindow());
		break;

	case OpScopeResourceListener::LS_REQUEST:
		u.raw = static_cast<const HTTP_Request::LoadStatusData_Raw*>(progress_info2);
		OpScopeResourceListener::OnRequest(url, u.raw->req->GetRequestNumber(), u.raw->req, u.raw->buf, u.raw->len);
		break;

	case OpScopeResourceListener::LS_REQUEST_FINISHED:
		u.req = (HTTP_Request*)progress_info2;
		OpScopeResourceListener::OnRequestFinished(url, u.req->GetRequestNumber(), u.req);
		break;

	case OpScopeResourceListener::LS_RESPONSE:
		u.req = (HTTP_Request*)progress_info2;
		{
			HeaderInfo *hinfo = u.req->GetHeader();
			OpScopeResourceListener::OnResponse(url, u.req->GetRequestNumber(), hinfo->response);
		}
		break;

	case OpScopeResourceListener::LS_RESPONSE_HEADER:
		u.raw = static_cast<const HTTP_Request::LoadStatusData_Raw*>(progress_info2);
		{
			HeaderInfo *hinfo = u.raw->req->GetHeader();
			HeaderList *headerlist = hinfo ? (hinfo->info.has_trailer ? &hinfo->trailingheaders : &hinfo->headers) : NULL;
			OpScopeResourceListener::OnResponseHeader(url, u.raw->req->GetRequestNumber(), headerlist, u.raw->buf, u.raw->len);
		}
		break;

	case OpScopeResourceListener::LS_RESPONSE_FINISHED:
		u.req = (HTTP_Request*)progress_info2;
		OpScopeResourceListener::OnResponseFinished(url, u.req->GetRequestNumber());
		break;

	case OpScopeResourceListener::LS_RESPONSE_FAILED:
		u.req = (HTTP_Request*)progress_info2;
		OpScopeResourceListener::OnResponseFailed(url, u.req ? u.req->GetRequestNumber() : 0);
		break;

	case OpScopeResourceListener::LS_DNS_LOOKUP_START:
		OpScopeResourceListener::OnDNSResolvingStarted(url);
		break;

	case OpScopeResourceListener::LS_DNS_LOOKUP_END:
		{
			OpHostResolver::Error *err = (OpHostResolver::Error*)progress_info2;
			OpScopeResourceListener::OnDNSResolvingEnded(url, *err);
		}
		break;

	default:
		OP_ASSERT(!"Unknown load status");
	}
}
#endif // SCOPE_RESOURCE_MANAGER

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
void URL_HTTP_LoadHandler::PauseLoading()
{
	if (req)
		req->PauseLoading();
}

OP_STATUS URL_HTTP_LoadHandler::ContinueLoading()
{
	if (req)
		return req->ContinueLoading();
	return OpStatus::ERR;
}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
void URL_HTTP_LoadHandler::SetIsFileDownload(BOOL value)
{
	if( req )
		req->SetIsFileDownload(value);
}
#endif

OP_STATUS URL_HTTP_LoadHandler::CopyExternalHeadersToUrl(URL &url)
{
	return req->CopyExternalHeadersToUrl(url);
}

BOOL URL_HTTP_LoadHandler::SetupAuthenticationDataL(HeaderList *headerlist, int header_name_id, URL_HTTP_ProtocolData *hptr, HeaderInfo *hinfo, HTTPAuth &auth_status, int &resp)
{
	OP_ASSERT(headerlist);
	OP_ASSERT(hptr);
	OP_ASSERT(hinfo);

	HeaderEntry *auth;

	auth = headerlist->GetHeaderByID(header_name_id);
	if(!auth)
	{
		/*
		resp = hinfo->response = HTTP_FORBIDDEN;
		url->SetAttributeL(URL::KHTTP_Response_Code, resp);
			*/
		return FALSE;
	}

	if(auth && auth_status == HTTP_AUTH_NOT)
		auth_status = HTTP_AUTH_NEED;

	if(!hptr->CheckAuthData())//FIXME:OOM (unable to report)
	{
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		resp = 0;
		return FALSE;
	}

	hptr->authinfo->auth_headers->Clear();
	hptr->authinfo->connect_host = req->request->connect_host;
	hptr->authinfo->connect_port = req->request->connect_port;
	hptr->authinfo->origin_host = req->request->origin_host;
	hptr->authinfo->origin_port = req->request->origin_port;

	headerlist->DuplicateHeadersL(hptr->authinfo->auth_headers, header_name_id);

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	if(header_name_id == HTTP_Header_WWW_Authenticate)
	{
		HeaderEntry *proxy_support = headerlist->GetHeaderByID(HTTP_Header_Proxy_Support);
		while(proxy_support)
		{
			ParameterList *params = proxy_support->GetParametersL(PARAM_SEP_SEMICOLON|PARAM_ONLY_SEP);

			if(params && params->GetParameter("Session-Based-Authentication") != NULL)
			{
				hptr->authinfo->session_based_support = TRUE;
				break;
			}

			proxy_support = headerlist->GetHeaderByID( 0, proxy_support);
		}

	}
#endif

	return TRUE;
}


#ifdef HTTP_DIGEST_AUTH

// Return TRUE if operation is completed, failed is TRUE if the operation failed
BOOL URL_HTTP_LoadHandler::CheckAuthentication(HeaderList *headerlist, int header_name_id, BOOL proxy, BOOL finished, UINT error_code, BOOL terminate_on_fail, BOOL &failed)
{
	failed = FALSE;

	HeaderEntry *header = headerlist->GetHeaderByID(header_name_id);
	if(header)
	{
		HTTP_Authinfo_Status status = AuthInfo_Failed;
		OP_STATUS op_err = OpStatus::ERR;
		ParameterList *param = header->GetParameters(PARAM_SEP_COMMA | PARAM_STRIP_ARG_QUOTES, KeywordIndex_Authentication);
		if(param != NULL)
			RAISE_IF_ERROR(op_err = req->Check_Digest_Authinfo(status, param, proxy, finished));
		if(OpStatus::IsError(op_err) || status == AuthInfo_Failed)
		{
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), error_code);
			if(terminate_on_fail)
			{
				req->Stop();
				DeleteComm();
			}
			url->OnLoadFinished(URL_LOAD_FAILED);
			failed = TRUE;
			return TRUE;
		}
		else if(status == Authinfo_Not_Finished)
			return FALSE;
	}

	return TRUE;
}
#endif // HTTP_DIGEST_AUTH
#ifdef LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
void URL_HTTP_LoadHandler::HandleStrictTransportSecurityHeader(HeaderList *header_list)
{
	// Only allow strict transport header for HTTPS urls.
	if ((URLType) url->GetAttribute(URL::KType) != URL_HTTPS)
		return;

	HeaderEntry *header = header_list->GetHeaderByID(HTTP_Header_Strict_Transport_security);
	if (!header)
		return;

	ServerName *current_server = (ServerName*) url->GetAttribute(URL::KServerName, NULL);

	// Security must be full. If user has accepted lower security, do not note this host as strict transport security host.
	SecurityLevel security_level = static_cast<SecurityLevel>(url->GetAttribute(URL::KSecurityStatus));
	if(security_level < SECURITY_STATE_FULL || security_level == SECURITY_STATE_UNKNOWN)
	{
		// SSL layer do not allow user interaction for HSTS connections.
		// However, on the first connection to a new server we get the HSTS header after SSL connection is negotiated,
		// and user interaction during SSL negotiation is possible since SSL layer doesn'ẗ know HSTS header.
		// Thus, we must only get here if this server isn't already known to support HSTS.
		OP_ASSERT(current_server->SupportsStrictTransportSecurity() == FALSE);
		return;
	}

	if (current_server)
	{
		time_t max_age = 0;

		ParameterList *params = header->GetParametersL((PARAM_SEP_SEMICOLON), KeywordIndex_HTTP_General_Parameters);
		if (params)
		{
			Parameters *max_age_param = params->GetParameterByID(HTTP_General_Tag_Max_Age);
			if (max_age_param && max_age_param->Value())
			{
				max_age = (time_t) max_age_param->GetSignedValue();
				if (max_age > 0)
				{
					current_server->SetStrictTransportSecurity(TRUE, params->ParameterExists(HTTP_General_Include_SubDomains),
							g_timecache->CurrentTime() + max_age);
				}
			}
		}
	}
}
#endif // LIBSSL_ENABLE_STRICT_TRANSPORT_SECURITY
BOOL URL_HTTP_LoadHandler::GenerateRedirectHTML(BOOL terminate_if_failed)
{
	OP_STATUS op_err;

	RAISE_IF_ERROR(op_err = url->SetAttribute(URL::KMIME_Type, "text/html"));
	if(OpStatus::IsMemoryError(op_err))
	{
		//FIXME:OOM - Check this OOM handling.
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		if(terminate_if_failed)
		{
			if (req != NULL)
				req->Stop();
			DeleteComm();
		}
		url->OnLoadFinished(URL_LOAD_FAILED);
		return FALSE;
	}

	URL location = g_url_api->GetURL(url->GetAttribute(URL::KHTTP_Location));
	OpString8 moved_to_name;
	url->GetAttribute(URL::KHTTP_MovedToURL_Name, moved_to_name);
	URL moved_to = g_url_api->GetURL(moved_to_name);
	URL me(url, static_cast<char *>(NULL));
	OpRedirectPage document(me, location.IsEmpty() ? &moved_to : &location);
	document.GenerateData();

	url->OnLoadFinished(URL_LOAD_FINISHED);
	return FALSE;
}
