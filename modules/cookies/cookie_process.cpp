/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */

// Url_cm.cpp

// URL Cookie Management

#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/uri_escape.h"
#include "modules/cookies/url_cm.h"
#include "modules/cookies/cookie_common.h"

#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
#include "modules/url/protocols/comm.h"
#endif

#if defined (_ASK_COOKIE)
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/network/src/op_request_impl.h"
#endif // defined _ASK_COOKIE

void Cookie_Manager::HandleCookiesL(URL_Rep *url, HeaderList &cookies, URL_CONTEXT_ID context_id)
{
	if(context_id && !GetContextIsTemporary(context_id))
	{
		CookieContextItem *ctx = FindContext(context_id);

		if(!ctx || !ctx->context)
			return;

		if(!ctx->share_with_main_context)
		{
			ctx->context->HandleCookiesL(url,cookies, 0);
			return;
		}

		// Contexts sharing with main will continue here
	}

	if(!url || ((URLType) url->GetAttribute(URL::KType) != URL_HTTP && (URLType) url->GetAttribute(URL::KType) != URL_HTTPS
#ifdef WEBSOCKETS_SUPPORT
		&& (URLType) url->GetAttribute(URL::KType) != URL_WEBSOCKET && (URLType) url->GetAttribute(URL::KType) != URL_WEBSOCKET_SECURE
#endif //WEBSOCKETS_SUPPORT
		))
		LEAVE(OpStatus::ERR);

#ifdef DISK_COOKIES_SUPPORT
	if (!cookie_file_read)
	{
		ReadCookiesL();
	}
#endif // DISK_COOKIES_SUPPORT
	Head currently_parsed_cookies;

	HeaderEntry * OP_MEMORY_VAR cookieheader;
	while((cookieheader= cookies.First()) != NULL)
	{
		if(!cookieheader->Value() || *(cookieheader->Value()) == '\0' ||
			op_strlen(cookieheader->Value()) > GetMaxCookieLen())
			goto next_cookie;

		switch(cookieheader->GetNameID())
		{
		case HTTP_Header_Set_Cookie:
			{
				TRAPD(op_err, HandleSingleCookieL(currently_parsed_cookies, url, cookieheader->Value(), cookieheader->Value(), 0, TRUE));
				OpStatus::Ignore(op_err);
			}
			break;
		case HTTP_Header_Set_Cookie2:
			{
				ParameterList *args = cookieheader->GetParametersL(PARAM_SEP_COMMA| PARAM_NO_ASSIGN | PARAM_ONLY_SEP);

				Parameters * OP_MEMORY_VAR current = args ? args->First() : NULL;
				while(current)
				{
					TRAPD(op_err, HandleSingleCookieL(currently_parsed_cookies, url, current->Name(), cookieheader->Value(),1, TRUE));
					if (OpStatus::IsMemoryError(op_err))
						break;
					current = current->Suc();
				}
			}
		}

next_cookie:;
		cookieheader->Out();
		OP_DELETE(cookieheader);
		cookieheader = NULL;
	}

	unprocessed_cookies.Append(&currently_parsed_cookies);

#ifdef _ASK_COOKIE
	if(unprocessed_cookies.Cardinal() && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisplayReceivedCookies))
    {
        g_main_message_handler->PostMessage(MSG_URL_PROCESS_COOKIES, 0,1);
    }
	else
#endif
	StartProcessingCookies();
}

#if defined(_ASK_COOKIE)
void WarnCookieDomainError(Window* win, URL_Rep *url, const char *cookie, BOOL no_ip_address)
{
	if (url)
	{
		if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::ShowCookieDomainErr))
		{
			WindowCommander* wic = win ? win->GetWindowCommander() : NULL;
			if (wic)
				wic->GetCookieListener()->OnCookieSecurityWarning(wic, url->GetAttribute(URL::KName_Username_Escaped).CStr(), cookie, no_ip_address);
			else
				g_cookie_API->GetCookieListener()->OnCookieSecurityWarning(NULL, url->GetAttribute(URL::KName_Username_Escaped).CStr(), cookie, no_ip_address);
		}
	}
}
#endif

void Cookie_Manager::HandleSingleCookieL(URL_Rep *url, const char *cookiearg,
							   const char *request,
							   int version,
							   URL_CONTEXT_ID context_id,
							   Window* win
							   )
{
	if(context_id && !GetContextIsTemporary(context_id))
	{
		CookieContextItem *ctx = FindContext(context_id);

		if(!ctx || !ctx->context)
			return;

		if(!ctx->share_with_main_context)
		{
			ctx->context->HandleSingleCookieL(url,cookiearg, request, version ,0 ,win);
			return;
		}
		// Contexts sharing with main will continue here
	}

	Head currently_parsed_cookies; ANCHOR(Head, currently_parsed_cookies);

#ifdef DISK_COOKIES_SUPPORT
	if (!cookie_file_read)
		ReadCookiesL();
#endif // DISK_COOKIES_SUPPORT

	HandleSingleCookieL(currently_parsed_cookies, url, cookiearg, request, version, FALSE, win);

	unprocessed_cookies.Append(&currently_parsed_cookies);

#ifdef _ASK_COOKIE
	if(unprocessed_cookies.Cardinal())
    {
        g_main_message_handler->PostMessage(MSG_URL_PROCESS_COOKIES, 0,1);
    }
#else
	StartProcessingCookies();
#endif
}


#if defined(_ASK_COOKIE)
int AskCookie(Cookie_Item_Handler *item);
#endif
void Cookie_Manager::StartProcessingCookies(BOOL reset_process)
{
	StartProcessingCookiesAction(reset_process);

	CookieContextItem *item = (CookieContextItem *) ContextManagers.First();

	while(item)
	{
		item->context->StartProcessingCookies();

		item = (CookieContextItem *) item->Suc();
	}
}

void Cookie_Manager::StartProcessingCookiesAction(BOOL reset_process)
{
	Cookie_Item_Handler* cookie_item      = NULL;
	Cookie_Item_Handler* next_cookie_item = NULL;

#if defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
	{
		next_cookie_item= (Cookie_Item_Handler *) unprocessed_cookies.First();
		while(next_cookie_item != NULL)
		{
			cookie_item = next_cookie_item;
			next_cookie_item = (Cookie_Item_Handler *) next_cookie_item->Suc();

			if(cookie_item->flags.do_not_set)
			{
				cookie_item->Out();
				OP_DELETE(cookie_item);
			}
#if defined PUBSUFFIX_ENABLED
			else if(cookie_item->flags.do_dns_lookup)
			{
				if(cookie_item->domain_sn == NULL ||
					cookie_item->domain_sn->GetIsLocalHost())
				{
					cookie_item->flags.do_dns_lookup = FALSE;
					cookie_item->flags.do_not_set = TRUE;
					next_cookie_item = cookie_item;
					continue;
				}

				ServerName::DomainType domain_type = cookie_item->domain_sn->GetDomainTypeASync();

				if(domain_type == ServerName::DOMAIN_WAIT_FOR_UPDATE)
					continue;

				if(domain_type != ServerName::DOMAIN_NORMAL)
				{
					cookie_item->flags.only_server = TRUE;
					OpStatus::Ignore(cookie_item->recvd_domain.Set(cookie_item->url->GetAttribute(URL::KHostName)));//OOM7 Handled by code below
					OpStatus::Ignore(cookie_item->domain.SetConcat("\"", cookie_item->url->GetAttribute(URL::KHostName), "\""));//OOM7 Handled by code below
				}
				cookie_item->flags.do_dns_lookup = FALSE;
			}
#else
			else if(cookie_item->flags.do_dns_lookup && !cookie_item->flags.waiting_for_dns)
			{
				if(cookie_item->lookup == NULL && cookie_item->lookup_url->IsEmpty())
				{
					//cookie_item->domain_sn = g_url_api->GetServerName(cookie_item->domain,TRUE);
					if(cookie_item->domain_sn == NULL ||
						cookie_item->domain_sn->GetIsLocalHost())
					{
						cookie_item->flags.do_dns_lookup = FALSE;
						cookie_item->flags.do_not_set = TRUE;
						next_cookie_item = cookie_item;
						continue;
					}

					if(cookie_item->domain_sn->IsHostResolved() || cookie_item->domain_sn->GetHTTP_Lookup_Succceded() ||
						(cookie_item->domain_sn->GetCookieDNS_Lookup_Done() != CookieDNS_NotDone &&
						cookie_item->domain_sn->GetCookieDNSExpires() &&
							cookie_item->domain_sn->GetCookieDNSExpires() >= g_timecache->CurrentTime()
							) )
					{
						cookie_item->flags.do_dns_lookup = FALSE;
						if(!cookie_item->domain_sn->IsHostResolved() &&
							cookie_item->domain_sn->GetCookieDNS_Lookup_Done() == CookieDNS_Failed)
						{
#ifdef _ASK_COOKIE
							if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DontShowCookieNoIPAddressErr) == COOKIE_NOIPADDR_SERVER_ONLY)
							{
								cookie_item->flags.only_server = TRUE;
								OpStatus::Ignore(cookie_item->domain.Set(cookie_item->url->GetAttribute(URL::KHostName)));//OOM7 Handled by code below
							}
							else
#endif
								cookie_item->flags.do_not_set = TRUE;
						}
					}
					else
					{
						Cookie_Item_Handler *cookie_item2 = (Cookie_Item_Handler *) unprocessed_cookies.First();
						while(cookie_item2)
						{
							if(cookie_item2->flags.do_dns_lookup &&
								cookie_item2->domain_sn == cookie_item->domain_sn &&
								(cookie_item2->lookup != NULL || !cookie_item2->lookup_url->IsEmpty()) )
								break;
							cookie_item2 = (Cookie_Item_Handler *) cookie_item2->Suc();
						}

						if(cookie_item2)
						{
							cookie_item->flags.waiting_for_dns = TRUE;
							SComm *looker = (cookie_item2->lookup ? (SComm *) cookie_item2->lookup : (SComm *) cookie_item2);
							if(
								OpStatus::IsError(looker->SetCallbacks(cookie_item, cookie_item)) ||
								OpStatus::IsError(g_main_message_handler->SetCallBack(cookie_item, MSG_COMM_NAMERESOLVED, looker->Id())) )
								// OOM: deleting item if we cannot set up callbacks ??
							{
								cookie_item2->Out();
								OP_DELETE(cookie_item2);
							}
							continue;
						}

						if(!cookie_item->Start_Lookup())
							next_cookie_item = cookie_item;
					}
				}
			}
#endif
			else if(cookie_item->flags.delete_cookie) // This must be done after the lookup to make sure there are no loopholes
			{
				cookie_item->Out();
				SetCookie(cookie_item);
				OP_DELETE(cookie_item);
			}
		}
	}
#endif

#ifdef COOKIES_CONTROL_PER_CONTEXT
	if( context_id && !cookies_enabled_for_context )
	{
		// Just to be onthe safe side; parser should exclude the cookies immediately
		if(!unprocessed_cookies.Empty())
		{
			// Unnecessary to send message if this is empty
			unprocessed_cookies.Clear();
			g_main_message_handler->PostMessage(MSG_HTTP_COOKIE_LOADED,0,0);
		}
		return;
	}
#endif // COOKIES_CONTROL_PER_CONTEXT

#ifdef _ASK_COOKIE
	if(reset_process)
		processing_cookies = FALSE;
#endif // _ASK_COOKIE

	COOKIE_MODES cmode = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);
	if(cmode == COOKIE_NONE)
	{
		unprocessed_cookies.Clear();
		g_main_message_handler->PostMessage(MSG_HTTP_COOKIE_LOADED,0,0);
		return;
	}

#ifdef _ASK_COOKIE
	if(!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DisplayReceivedCookies))
#endif // _ASK_COOKIE
	{
		next_cookie_item= (Cookie_Item_Handler *) unprocessed_cookies.First();
		while(next_cookie_item != NULL)
		{
			cookie_item = next_cookie_item;
			next_cookie_item = (Cookie_Item_Handler *) next_cookie_item->Suc();

#if defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
			if(cookie_item->flags.do_dns_lookup)
				continue;
#endif

#ifdef _ASK_COOKIE
			ServerName *servername = (ServerName *) cookie_item->url->GetAttribute(URL::KServerName, (const void*) 0);
			OP_ASSERT(servername);

			BOOL set_it = TRUE;
			if(cookie_item->flags.illegal_path)
			{
				ServerName *domain_sn = (cookie_item->domain.HasContent() ?
					g_url_api->GetServerName(cookie_item->domain,TRUE):
					servername);

				if(!domain_sn || domain_sn->GetAcceptIllegalPaths() == COOKIE_ILLPATH_DEFAULT)
                {
                    int mode = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiePathErrMode);

                    if(mode == COOKIE_ILLEGAL_PATH_REFUSE_ALL)
                        set_it= FALSE;
                    else if(mode == COOKIE_ILLEGAL_PATH_SHOW_WARNING)
                        continue;

                }
				else if(domain_sn->GetAcceptIllegalPaths() == COOKIE_ILLPATH_REFUSE)
					set_it = FALSE;
			}
			if(set_it && cookie_item->flags.accepted_as_third_party && cmode == COOKIE_WARN_THIRD_PARTY)
			{
				COOKIE_MODES tmod = servername->GetAcceptThirdPartyCookies(TRUE);
				if(tmod == COOKIE_DEFAULT)
					continue; // Ask user
				if(tmod != COOKIE_ACCEPT_THIRD_PARTY)
					set_it = FALSE;
			}

			if(set_it)
#endif // _ASK_COOKIE
				SetCookie(cookie_item);

			cookie_item->Out();
			OP_DELETE(cookie_item);
		}
	}

	g_main_message_handler->PostMessage(MSG_HTTP_COOKIE_LOADED,0,0);
	if(unprocessed_cookies.Empty()
#ifdef _ASK_COOKIE
		|| processing_cookies
#endif
		)
		return;

#ifdef _ASK_COOKIE
	next_cookie_item= (Cookie_Item_Handler *) unprocessed_cookies.First();
	cookie_item = NULL;
	while(next_cookie_item != NULL)
	{
		cookie_item = next_cookie_item;
		next_cookie_item = (Cookie_Item_Handler *) next_cookie_item->Suc();

#if defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
		if(cookie_item->flags.do_dns_lookup)
		{
			cookie_item = NULL;
			continue;
		}
#endif

		ServerName *servername = (ServerName *) cookie_item->url->GetAttribute(URL::KServerName, (const void*) 0);
		OP_ASSERT(servername);

		COOKIE_MODES mod = servername->GetAcceptCookies(TRUE);
		BOOL set_it = TRUE;
		if(mod == COOKIE_DEFAULT || cookie_item->flags.illegal_path)
		{
#ifdef URL_DISABLE_INTERACTION
			if(!cookie_item->flags.delete_cookie && cookie_item->url->GetAttribute(URL::KBlockUserInteraction))
				set_it = FALSE;
			else
#endif
			if(!cookie_item->flags.delete_cookie )
				break;
		}
		else if(cookie_item->flags.accepted_as_third_party && cmode == COOKIE_WARN_THIRD_PARTY)
		{
			COOKIE_MODES tmod = servername->GetAcceptThirdPartyCookies(TRUE);
			if(tmod == COOKIE_DEFAULT)
			{
#ifdef URL_DISABLE_INTERACTION
				if(!cookie_item->flags.delete_cookie && cookie_item->url->GetAttribute(URL::KBlockUserInteraction))
					set_it = FALSE;
				else
#endif
				if(!cookie_item->flags.delete_cookie)
					break; // Ask user
			}
			if(tmod != COOKIE_ACCEPT_THIRD_PARTY)
				set_it = FALSE;
		}
		else if(mod != COOKIE_ALL)
			set_it = FALSE;

		if(set_it)
			SetCookie(cookie_item);

		cookie_item->Out();
		OP_DELETE(cookie_item);
		cookie_item = NULL;
	}

	if(cookie_item)
	{
		processing_cookies = TRUE;



		OpRequestImpl *request = NULL;
		if (cookie_item->url.GetURL().GetRep()->GetDataStorage())
			request = cookie_item->url.GetURL().GetRep()->GetDataStorage()->GetOpRequestImpl();

		if (request)
		{
			request->CookieConfirmationRequired(cookie_item);
		}
		else
		{
			if (cookie_item->first_window)
			{
				Window* win;
				for(win = g_windowManager->FirstWindow(); win; win = win->Suc())
				{
					if (win == cookie_item->first_window)
					{
						break;
					}
				}
				if(!win)
					cookie_item->first_window = NULL;
			}

			if (cookie_item->first_window)
			{
				WindowCommander* wic = cookie_item->first_window->GetWindowCommander();
				if (wic)
					wic->GetCookieListener()->OnAskCookie(wic, cookie_item);
			}
			else
				g_cookie_API->GetCookieListener()->OnAskCookie(NULL, cookie_item);
		}
	}
#else
#if defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
    next_cookie_item= (Cookie_Item_Handler *) unprocessed_cookies.First();
    while(next_cookie_item != NULL)
    {
        cookie_item = next_cookie_item;
        next_cookie_item = (Cookie_Item_Handler *) next_cookie_item->Suc();

        if(cookie_item->flags.do_dns_lookup)
        {
            cookie_item = NULL;
            continue;
        }

        cookie_item->Out();
        OP_DELETE(cookie_item);
        cookie_item = NULL;
	}
#else
	unprocessed_cookies.Clear();
#endif
#endif
}

#ifdef _ASK_COOKIE
void Cookie_Manager::RemoveSameCookiesFromQueue(Cookie_Item_Handler *set_item)
{
	if(set_item == NULL)
		return;

	if(set_item->context_id != context_id  && !GetContextIsTemporary(set_item->context_id))
	{
		Cookie_Manager::CookieContextItem *ctx = FindContext(set_item->context_id);

		set_item->context_id = 0; // Making sure it is set for the context
		if(ctx && ctx->context)
			ctx->context->RemoveSameCookiesFromQueue(set_item);

		return;
	}

	Cookie_Item_Handler *cookie_item;
	Cookie_Item_Handler *tmp_item = (Cookie_Item_Handler *) unprocessed_cookies.First();
	while((cookie_item = tmp_item) != NULL)
	{
		tmp_item = (Cookie_Item_Handler *)cookie_item->Suc();
		if(cookie_item->version == set_item->version &&
			cookie_item->flags.secure == set_item->flags.secure &&
			cookie_item->name.Compare(set_item->name) == 0 &&
			cookie_item->domain.Compare(set_item->domain) == 0 &&
			cookie_item->path.Compare(set_item->path) == 0)
		{

			if(cookie_item->version != 0 && cookie_item->port_count)
			{
				if(cookie_item->port_count != set_item->port_count)
					continue;

				unsigned i;

				for(i=0;i< cookie_item->port_count; i++)
					if(cookie_item->portlist[i] != set_item->portlist[i])
						break;

				if(i<cookie_item->port_count)
					continue;
			}

			cookie_item->Out();
			OP_DELETE(cookie_item);
		}
	}
}
#endif

#ifdef _ASK_COOKIE
BOOL Cookie_Manager::CheckAcceptCookieUpdates(Cookie_Item_Handler *cookie_item)
{
	if(cookie_item == NULL)
		return FALSE;

	if(cookie_item->context_id != context_id && !GetContextIsTemporary(cookie_item->context_id))
	{
		Cookie_Manager::CookieContextItem *ctx = FindContext(cookie_item->context_id);

		cookie_item->context_id = 0; // Making sure it is set for the context

		if(!ctx || !ctx->context)
			return FALSE;

		if(!ctx->share_with_main_context)
			return ctx->context->CheckAcceptCookieUpdates(cookie_item);
		// Contexts sharing with main will continue here
	}

#ifdef _DEBUG_COOKIES_
	PrintfTofile("dcookie.txt", "Checking Update status cookie %s --- URL %s\n", cookie_item->request, cookie_item->url->Name());
#endif
	int len = cookie_item->domain.Length();
	int len2 = cookie_item->path.Length();
	RAISE_AND_RETURN_VALUE_IF_ERROR(CheckCookieTempBuffers(len > len2 ? len : len2), FALSE);

	char *domain = cookie_processing_buf1;
	char *path = cookie_processing_buf2;

	op_strcpy(domain, cookie_item->domain.CStr());

	if(cookie_item->path.HasContent())
		op_strcpy(path, cookie_item->path.CStr());
	else
		path[0] = '\0';

	CookieDomain* lowest_domain = 0;
	BOOL is_full_path = FALSE;

	CookiePath* OP_MEMORY_VAR cp = NULL;

	TRAPD(op_err, cp = cookie_root->GetCookiePathL(domain, path,
		NULL,
		FALSE, lowest_domain, is_full_path));
	RAISE_AND_RETURN_VALUE_IF_ERROR(op_err,FALSE);
	if (cp)
	{
		Cookie  * OP_MEMORY_VAR tempcookie = NULL;
		TRAP(op_err, tempcookie = cp->GetCookieL(cookie_item, FALSE));
		RAISE_AND_RETURN_VALUE_IF_ERROR(op_err,FALSE);
		if(tempcookie && tempcookie->AcceptUpdates())
			return TRUE;
	}
#ifdef _DEBUG_COOKIES_
			FILE* fp = fopen("c:\\klient\\dcookie.txt", "a");
			if (fp)
			{
				cookie_root->DebugWriteCookies(fp);
				fclose(fp);
			}
#endif
	return FALSE;
}
#endif

#if defined _ASK_COOKIE && defined ENABLE_COOKIE_CREATE_DOMAIN
void Cookie_Manager::CreateCookieDomain(ServerName *domain_sn)
{
	if(!domain_sn || !domain_sn->Name() || !(*domain_sn->Name()))
		return;

	char *domain_name = (char *) domain_sn->Name();

	if(!domain_name || !(*domain_name))
		return;

	if(op_strchr(domain_name,'.') == NULL)
	{
		if(op_strlen(domain_name) + 10 > g_memory_manager->GetTempBuf2kLen())
			return;

		domain_name = (char *) g_memory_manager->GetTempBuf2k();

		domain_name[0] = '\0';
		StrMultiCat(domain_name, domain_sn->Name(),".local");
	}

	CreateCookieDomain(domain_name);
}

void Cookie_Manager::CreateCookieDomain(const char *domain_name)
{
	if(!domain_name)
		return;

	CookieDomain* lowest_domain = 0;
	BOOL is_full_path = FALSE;

	TRAPD(op_err, cookie_root->GetCookiePathL(domain_name, NULL,
		NULL,
		TRUE, lowest_domain, is_full_path));
}
#endif

#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
int Cookie_Manager::HandlingCookieForURL(URL_Rep *url, URL_CONTEXT_ID context_id)
{
	// 0 : Not handling
	// 1 : Handling cookie
	// 2 : Handling a DNS request for the domain name
	if(context_id)
	{
		CookieContextItem *ctx = FindContext(context_id);

		if(!ctx || !ctx->context)
			return 0;

		if(!ctx->share_with_main_context)
			return ctx->context->HandlingCookieForURL(url, 0);
		// Contexts sharing with main will continue here
	}

	Cookie_Item_Handler *cookie_item;
	ServerName *server_name = (ServerName *) url->GetAttribute(URL::KServerName, NULL);

	if(!url || !server_name || !server_name->Name())
		return FALSE;

	const char *server = server_name->Name();
	unsigned int u_dom_len = op_strlen(server);

	if(u_dom_len > g_memory_manager->GetTempBuf2kLen()-30)
		return FALSE; //Servername too long to be legal

#ifdef __FILE_COOKIES_SUPPORTED
	if((URLType) url->GetAttribute(URL::KType) == URL_FILE)
	{
		char *temp = (char *) g_memory_manager->GetTempBuf2k();

		op_strcpy(temp, server);
		op_strcat(temp, ".$localfile$");

		server = temp;
		u_dom_len = op_strlen(server);
	}
	else
#endif
	if(op_strchr(server, '.') == NULL && op_strspn(server, "0123456789") < u_dom_len)
	{
		char *temp = (char *) g_memory_manager->GetTempBuf2k();

		op_strcpy(temp, server);
		op_strcat(temp, ".local");

		server = temp;
		u_dom_len = op_strlen(server);
	}

	BOOL doing_dns = FALSE;

	cookie_item = (Cookie_Item_Handler *) unprocessed_cookies.First();
	while(cookie_item)
	{
		unsigned int c_dom_len = cookie_item->domain.Length();
		if((c_dom_len == u_dom_len && cookie_item->domain.Compare(server) == 0) ||
			(!cookie_item->flags.only_server && c_dom_len < u_dom_len -1 &&
				server[u_dom_len - c_dom_len -1] == '.' &&
				cookie_item->domain.Compare(server+u_dom_len - c_dom_len) == 0))
		{
			// match on domainname
			// does not check on ports

			const char *url_path = url->GetAttribute(URL::KPath).CStr();

			if(url_path)
				url_path++;

			if(cookie_item->path.IsEmpty() ||
				(url_path && UriUnescape::isstrprefix(cookie_item->path.CStr(), url_path, UriUnescape::All)) )
			{
#if !defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
				return 1;
#else
				if(!cookie_item->flags.do_dns_lookup)
					return 1;
				else
					doing_dns = TRUE;
#endif
			}
		}
		cookie_item = (Cookie_Item_Handler *) cookie_item->Suc();
	}
	if(doing_dns)
		return 2;
	return 0;
}
#endif

