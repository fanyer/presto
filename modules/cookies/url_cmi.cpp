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

// URL Cookie Management

#include "core/pch.h"

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/handy.h"

#include "modules/url/url_man.h"
#include "modules/url/url_ds.h"
#include "modules/cookies/url_cm.h"

#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"
#endif

#include "modules/olddebug/tstdump.h"
#include "modules/url/protocols/common.h"
#include "modules/url/protocols/http1.h"

Cookie_Item_Handler::Cookie_Item_Handler()
#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
	: ProtocolComm(g_main_message_handler,NULL, NULL)
#endif
{
	InternalInit();
}

void Cookie_Item_Handler::InternalInit()
{
#if defined(MEMTEST) && defined(USE_DEBUGGING_LEAVE)
	oom_delete_unanchored_object(&url); // Avoid getting a report for unanchored url, the importance is that
	                                // Cookie_Item_Handler is anchored
#endif // MEMTEST
	context_id = 0;
	first_window = NULL;

	expire = 0;
	last_used = 0;
	last_sync = 0;

	flags.secure = FALSE;
	flags.httponly = FALSE;
	flags.set_fromhttp = FALSE;
	flags.full_path_only = FALSE;
	version = 0;
	portlist = NULL;
	port_count = 0;
	flags.recv_dom = flags.recv_path = FALSE;
	flags.delete_cookie = FALSE;
	flags.discard_at_exit = FALSE;
	flags.only_server = FALSE;
	flags.protected_cookie = FALSE;
	flags.accept_updates = FALSE;
	flags.accepted_as_third_party = FALSE;
	flags.illegal_path = FALSE;
	flags.have_password = FALSE;
	flags.have_authentication = FALSE;
#ifdef _SSL_USE_SMARTCARD_
	flags.smart_card_authenticated = FALSE;
#endif
#if defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
	flags.do_dns_lookup =FALSE;
	flags.waiting_for_dns = FALSE;
	flags.do_not_set = FALSE;
#endif
#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
	flags.http_done_same_port = FALSE;
	flags.force_direct_lookup  = FALSE;
	lookup = NULL;
#endif
	domain_sn = NULL;
}

Cookie_Item_Handler::~Cookie_Item_Handler()
{
	OP_ASSERT(!InList());
	Clear();
}

void Cookie_Item_Handler::Clear()
{
	url.UnsetURL();

	first_window = NULL;

	expire = 0;
	last_used = 0;
	last_sync = 0;
	OP_DELETEA(portlist);
	portlist = NULL;


	flags.secure = FALSE;
	version = 0;
	flags.recv_dom = flags.recv_path = FALSE;
	flags.discard_at_exit = FALSE;
	flags.delete_cookie = FALSE;
	flags.have_password = FALSE;
	flags.have_authentication = FALSE;
#ifdef _SSL_USE_SMARTCARD_
	flags.smart_card_authenticated = FALSE;
#endif

#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
	g_main_message_handler->UnsetCallBacks(this);
	if(!lookup_url->IsEmpty())
	{
		lookup_url->StopLoading(NULL);
	}
	lookup_url.UnsetURL();
	SafeDestruction(lookup);
	lookup = NULL;
#endif
	domain_sn = NULL;
}

#if defined COOKIE_USE_DNS_LOOKUP && !defined PUBSUFFIX_ENABLED
void Cookie_Item_Handler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// Check HTTP if it succeded
	if(!lookup_url->IsEmpty() && domain_sn != NULL)
	{
		BOOL retry = FALSE;
		switch(msg)
		{
		case MSG_HEADER_LOADED:
			{
				uint32 response = lookup_url->GetAttribute(URL::KHTTP_Response_Code);

				if((response && response>= 200 && response< 400) || response == 401)
				{
					domain_sn->SetHTTP_Lookup_Succceded(TRUE);
				}
				msg = MSG_COMM_LOADING_FINISHED;
			}
			break;
		case MSG_URL_LOADING_FAILED:
			if(par2 == MSG_NOT_USING_PROXY)
				flags.force_direct_lookup = TRUE;
			else
				msg = MSG_COMM_LOADING_FAILED;
			break;
		}

		if(retry)
		{
			g_main_message_handler->RemoveDelayedMessage(MSG_URL_LOADING_FAILED, lookup_url->Id(),0);
			lookup_url->StopLoading(NULL); // If we have timed out
			lookup_url.UnsetURL();
			g_main_message_handler->UnsetCallBacks(this);

			if(!flags.http_done_same_port)
			{
				flags.http_done_same_port = TRUE;
				if(Start_Lookup())
					return;
			}
			msg = MSG_COMM_LOADING_FAILED;
		}
	}


	switch(msg)
	{
	case MSG_COMM_NAMERESOLVED:
	case MSG_COMM_LOADING_FINISHED:
		if(domain_sn == NULL ||
		   (!domain_sn->SocketAddress()->IsValid() && !domain_sn->GetHTTP_Lookup_Succceded())
		   || domain_sn->GetIsLocalHost())
		{
			// Continue to next case
		}
		else
		{
			domain_sn->SetCookieDNS_Lookup_Done(CookieDNS_Succeded);
			domain_sn->SetCookieDNSExpires(g_timecache->CurrentTime() + 24*60*60);
			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
			break;
		}
	case MSG_COMM_LOADING_FAILED:
		{
			BOOL ok = FALSE;
			domain_sn->SetCookieDNS_Lookup_Done(CookieDNS_Failed);
			domain_sn->SetCookieDNSExpires(g_timecache->CurrentTime() + 24*60*60);

#ifdef _ASK_COOKIE
			if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::DontShowCookieNoIPAddressErr) == COOKIE_NOIPADDR_SERVER_ONLY)
#endif
			{
				flags.only_server = TRUE;
				TRAPD(op_err, url->GetAttributeL(URL::KHostName, domain));
				OpStatus::Ignore(op_err);
				ok = domain.HasContent();
			}
			if(ok)
			{
				mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
			}
			else
			{
				flags.do_not_set = TRUE;
				mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), 0);
#if defined(_ASK_COOKIE)
				WarnCookieDomainError(mh->GetWindow(), url.GetURL().GetRep(), request.CStr(), TRUE);
#endif
			}
		}
		break;
	}
	flags.do_dns_lookup = FALSE;
	if(!lookup_url->IsEmpty())
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_LOADING_FAILED, lookup_url->Id(),0);
		lookup_url.UnsetURL();
	}
	if(lookup)
		g_main_message_handler->RemoveDelayedMessage(MSG_COMM_LOADING_FAILED, lookup->Id(),0);

	SafeDestruction(lookup);
	lookup = NULL;

	g_main_message_handler->PostMessage(MSG_URL_PROCESS_COOKIES,0,1);
	g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS Cookie_Item_Handler::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	static const OpMessage messages[] =
    {
        MSG_COMM_LOADING_FINISHED,
        MSG_COMM_LOADING_FAILED
    };

	RETURN_IF_ERROR(mh->SetCallBackList((parent ? parent : master), Id(), messages, ARRAY_SIZE(messages)));

	return OpStatus::OK;
}

BOOL Cookie_Item_Handler::Start_Lookup()
{
	OP_STATUS op_err = OpStatus::OK;

	domain_sn->SetCookieDNS_Lookup_Done(CookieDNS_NotDone);
	domain_sn->SetCookieDNSExpires(0);

	if (!flags.force_direct_lookup)
	{
		BOOL autoproxy = FALSE;
		BOOL use_proxy = FALSE;
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		if (g_pcnet->IsAutomaticProxyOn())
		{
			URL_DataStorage *url_datastorage = url->GetRep()->GetDataStorage();

			URL_HTTP_ProtocolData *hptr = NULL;
			if (url_datastorage)
				hptr = url_datastorage->GetHTTPProtocolData();

			if (hptr)
			{
				// Get output from auto proxy script.
				const uni_char *auto_proxy_string = hptr->sendinfo.proxyname.CStr();
				if (auto_proxy_string)
				{
					HTTP_request_st req;

					// Parses the output from auto proxy script, to determine if a proxy is used for this request.
					GetAutoProxyL(auto_proxy_string, &req);

					if (req.proxy != NO_PROXY)
					{
						use_proxy = TRUE;
						autoproxy = TRUE;
					}

				}
			}
		}
#endif
		BOOL secure = FALSE;
		if (!autoproxy)
		{
			const uni_char *proxy = NULL;
			if(!use_proxy)
				proxy = urlManager->GetProxy(NULL, URL_HTTP);

			if(proxy && *proxy)
			{
				if((URLType) url->GetAttribute(URL::KType) == URL_HTTPS && !flags.http_done_same_port)
				{
					secure = TRUE;
					if((proxy = g_pcnet->GetHTTPSProxyServer(NULL)) != NULL)
					{
						if(*proxy)
							use_proxy = TRUE;
					}
				}
				else
					use_proxy = TRUE;
			}
		}

		if(use_proxy)
		{
			OpString8 lookup_url_text;
			URL_InUse temp;
			URL ref;

			unsigned short port = (unsigned short) url->GetAttribute(URL::KServerPort);

			if(port)
				flags.http_done_same_port = TRUE;

			OpString8 port_temp;
			port_temp.AppendFormat((port ? "%s://%s:%u/" : "%s://%s/"), (secure ? "https" : "http"), domain_sn->Name(), port);
			if(OpStatus::IsError(lookup_url_text.Set(port_temp)))
				goto http_error;

			{
				URL temp_url = urlManager->GetURL(lookup_url_text, NULL, TRUE);
				temp.SetURL(temp_url);
			}
			if(temp->IsEmpty())
				goto http_error;

			temp->SetAttribute(URL::KHTTP_Method, HTTP_METHOD_HEAD);
			temp->SetAttribute(URL::KDisableProcessCookies, TRUE);
			temp->SetAttribute(URL::KSendOnlyIfProxy, TRUE);

			if(temp->Load(g_main_message_handler,ref, FALSE, FALSE, FALSE, TRUE) != COMM_LOADING)
				goto http_error;

			if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_HEADER_LOADED, temp->Id())))
				goto http_error;
			if(OpStatus::IsError(g_main_message_handler->SetCallBack(this, MSG_URL_LOADING_FAILED, temp->Id())))
				goto http_error;

			g_main_message_handler->PostDelayedMessage(MSG_URL_LOADING_FAILED, temp->Id(),0, 20000); // Fail if Request does not resolve within 10 seconds
			lookup_url = temp;
			temp.UnsetURL();

http_error:
			if(!temp->IsEmpty())
				temp->StopLoading(NULL);
		}
	}

	if(lookup_url->IsEmpty())
	{

		Comm *comm = Comm::Create(g_main_message_handler, domain_sn, 80);

		if(comm)
		{
			lookup = comm;
			OpStatus::Ignore(comm->SetCallbacks(this, this));
			OpStatus::Ignore(g_main_message_handler->SetCallBack(this, MSG_COMM_NAMERESOLVED, comm->Id()));
			g_main_message_handler->PostDelayedMessage(MSG_COMM_LOADING_FAILED, comm->Id(),0, 5000); // Fail if name lookup does not resolve within 5 seconds

			CommState stat = comm->LookUpName(domain_sn);

			if(stat != COMM_LOADING && stat != COMM_WAITING)
			{
				comm = NULL;
			}
		}

		if(comm == NULL)
		{
			flags.do_dns_lookup = FALSE;
			flags.do_not_set = TRUE;
			return FALSE;
		}
	}

	flags.waiting_for_dns = TRUE;
	return (OpStatus::IsMemoryError(op_err)) ? FALSE : TRUE;
}

#ifdef UNUSED_CODE
BOOL Cookie_Item_Handler::Is_LookingUp()
{
	return (lookup != NULL || !lookup_url->IsEmpty());
}
#endif // UNUSED_CODE


#endif

#ifdef _OPERA_DEBUG_DOC_
void Cookie_Item_Handler::GetMemUsed(DebugUrlMemory &debug)
{
	debug.number_waiting_cookies++;
	debug.memory_cookies += sizeof(*this);
	debug.memory_cookies += request.Length()+1;
	debug.memory_cookies += name.Length()+1;
	debug.memory_cookies += value.Length()+1;
	debug.memory_cookies += comment.Length()+1;
	debug.memory_cookies += comment_URL.Length()+1;
	debug.memory_cookies += domain.Length()+1;
	debug.memory_cookies += path.Length()+1;
	debug.memory_cookies += port.Length()+1;
	if(portlist && port_count)
		debug.memory_cookies += port_count * sizeof(unsigned short);
}

#endif

#if defined _ASK_COOKIE

void Cookie_Item_Handler::OnAskCookieDone(OpCookieListener::CookieAction response)
{
	BOOL action_set_cookie = FALSE;
	BOOL action_set_flag = FALSE;
	BOOL action_set_domain = FALSE;
	COOKIE_MODES set_policy = COOKIE_NONE;
	BOOL action_set_discard = FALSE;

	switch (response)
	{	
	case OpCookieListener::COOKIE_ACTION_REFUSE:
		/* Do nothing */
		break;		
	case OpCookieListener::COOKIE_ACTION_ACCEPT_AND_UPDATES:
		flags.accept_updates = TRUE;
		action_set_cookie = TRUE;
		break;	
	case OpCookieListener::COOKIE_ACTION_ACCEPT_SHOW_UPDATES:
		flags.accept_updates = FALSE;
		// fallthrough
	case OpCookieListener::COOKIE_ACTION_ACCEPT_AND_SAVE: // default: flags.accept_updates == FALSE
		flags.discard_at_exit = FALSE;
		action_set_cookie = TRUE;
		break;	
	case OpCookieListener::COOKIE_ACTION_ACCEPT_BUT_DISCARD:
		flags.discard_at_exit = TRUE;
		action_set_cookie = TRUE;
		break;

	case OpCookieListener::COOKIE_ACTION_ACCEPT_ALL_FROM_DOMAIN_BUT_DISCARD:
		action_set_discard = TRUE;
		// Do not put anything here. Fall through to COOKIE_ACTION_ACCEPT_ALL_FROM_DOMAIN
	case OpCookieListener::COOKIE_ACTION_ACCEPT_ALL_FROM_DOMAIN:
		action_set_cookie = TRUE;
		set_policy = COOKIE_ALL;
		action_set_domain = TRUE;
		action_set_flag = TRUE;
		break;
	case OpCookieListener::COOKIE_ACTION_REFUSE_ALL_FROM_DOMAIN:
		set_policy = COOKIE_NONE;
		action_set_domain = TRUE;
		action_set_flag = TRUE;
		break;
	case OpCookieListener::COOKIE_ACTION_ACCEPT_ALL_3P_FROM_DOMAIN:
		action_set_cookie = TRUE;
		set_policy = COOKIE_ACCEPT_THIRD_PARTY;
		action_set_domain = TRUE;
		action_set_flag = TRUE;
		break;		
	case OpCookieListener::COOKIE_ACTION_REFUSE_ALL_3P_FROM_DOMAIN:
		set_policy = COOKIE_NO_THIRD_PARTY;
		action_set_domain = TRUE;
		action_set_flag = TRUE;
		break;	
	}

	if(InList())
		Out();
	g_url_api->RemoveSameCookiesFromQueue(this);

	if(action_set_flag)
	{
		ServerName *target_sn = action_set_domain && domain_sn != NULL ? ((ServerName *) domain_sn) /*(action_set_domain && domain.HasContent()) ? 
				g_url_api->GetServerName(domain, TRUE)*/ :
				url->GetServerName();
		if (target_sn)
		{
			if(set_policy == COOKIE_ACCEPT_THIRD_PARTY || 
				set_policy == COOKIE_NO_THIRD_PARTY)
				target_sn->SetAcceptThirdPartyCookies(COOKIE_NO_THIRD_PARTY);
			else
				target_sn->SetAcceptCookies(set_policy);
			if (action_set_discard)
			{
				target_sn->SetDeleteCookieOnExit(COOKIE_DELETE_ON_EXIT);
			}
			g_url_api->CreateCookieDomain(target_sn);
		}
	}

	if (action_set_cookie)
		g_url_api->SetCookie(this);

	g_main_message_handler->PostMessage(MSG_HTTP_COOKIE_LOADED, 0, 0);
	g_main_message_handler->PostMessage(MSG_URL_PROCESS_COOKIES, 0, 0);

	OP_DELETE(this);
}

void Cookie_Item_Handler::OnAskCookieCancel()
{
	if(InList())
		Out();
	g_url_api->RemoveSameCookiesFromQueue(this);

	g_main_message_handler->PostMessage(MSG_HTTP_COOKIE_LOADED, 0, 0);
	g_main_message_handler->PostMessage(MSG_URL_PROCESS_COOKIES, 0, 0);	

	OP_DELETE(this);
}

#endif // defined _ASK_COOKIE


