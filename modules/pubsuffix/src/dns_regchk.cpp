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

#if defined PUBSUFFIX_ENABLED

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/handy.h"

#include "modules/url/url_man.h"
#include "modules/pubsuffix/src/dns_regchk.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/comm.h"

#include "modules/olddebug/tstdump.h"

DNS_RegistryCheck_Handler::DNS_RegistryCheck_Handler(ServerName *sn)
	: ProtocolComm(g_main_message_handler,NULL, NULL),
	domain_sn(sn)
{
	InternalInit();
}

void DNS_RegistryCheck_Handler::InternalInit()
{
	force_direct_lookup  = FALSE;
	lookup = NULL;
}

DNS_RegistryCheck_Handler::~DNS_RegistryCheck_Handler()
{
	Clear();
}

void DNS_RegistryCheck_Handler::Clear()
{
	g_main_message_handler->UnsetCallBacks(this);
	if(!lookup_url->IsEmpty())
	{
		lookup_url->StopLoading(NULL);
	}
	lookup_url.UnsetURL();
	SafeDestruction(lookup);
	lookup = NULL;
	domain_sn = NULL;
}

void DNS_RegistryCheck_Handler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// Check HTTP if it succeded
	if(lookup_url->IsValid() && domain_sn != NULL)
	{
		BOOL retry = FALSE;
		switch(msg)
		{
		case MSG_HEADER_LOADED:
			{
#ifdef PUBSUF_DEBUG
				PrintfTofile("pubsuff.txt", "DNS hdr %s\n", domain_sn->Name());
#endif
				uint32 response = lookup_url->GetAttribute(URL::KHTTP_Response_Code);

				if((response && response>= 200 && response< 400) || response == 401)
				{
					domain_sn->SetHTTP_Lookup_Succceded(TRUE);
				}
				msg = MSG_COMM_LOADING_FINISHED;
			}
			break;
		case MSG_URL_LOADING_FAILED:
#ifdef PUBSUF_DEBUG
			PrintfTofile("pubsuff.txt", "DNS fail %s\n", domain_sn->Name());
#endif
			if(par2 == MSG_NOT_USING_PROXY)
				force_direct_lookup = retry = TRUE;
			else
				msg = MSG_COMM_LOADING_FAILED;
			break;
#ifdef PUBSUF_DEBUG
		default:
		PrintfTofile("pubsuff.txt", "DNS hdr %s other msg %d\n", domain_sn->Name(), (int) msg);
#endif
		}

		if(retry)
		{
#ifdef PUBSUF_DEBUG
			PrintfTofile("pubsuff.txt", "DNS retry %s\n", domain_sn->Name());
#endif
			g_main_message_handler->RemoveDelayedMessage(MSG_URL_LOADING_FAILED, lookup_url->Id(),0);
			lookup_url->StopLoading(NULL); // If we have timed out
			lookup_url.UnsetURL();
			g_main_message_handler->UnsetCallBacks(this);

			if(Start_Lookup())
			{
#ifdef PUBSUF_DEBUG
				PrintfTofile("pubsuff.txt", "DNS retry started%s\n", domain_sn->Name());
#endif

				return;
			}
			//else failed
#ifdef PUBSUF_DEBUG
			PrintfTofile("pubsuff.txt", "DNS retry failed %s\n", domain_sn->Name());
#endif
			msg = MSG_COMM_LOADING_FAILED;
		}
	}


	switch(msg)
	{
	case MSG_COMM_NAMERESOLVED:
	case MSG_COMM_LOADING_FINISHED:
#ifdef PUBSUF_DEBUG
		PrintfTofile("pubsuff.txt", "DNS resolved %s\n", domain_sn->Name());
#endif
		if(domain_sn == NULL ||
		   (!domain_sn->SocketAddress()->IsValid() && !domain_sn->GetHTTP_Lookup_Succceded())
		   || domain_sn->GetIsLocalHost())
		{
			// Continue to next case
		}
		else
		{
#ifdef PUBSUF_DEBUG
			PrintfTofile("pubsuff.txt", "DNS resolved 2 %s\n", domain_sn->Name());
#endif
			domain_sn->SetDomainType(ServerName::DOMAIN_NORMAL);
			domain_sn->SetCookieDNS_Lookup_Done(CookieDNS_Succeded);
			domain_sn->SetCookieDNSExpires(g_timecache->CurrentTime() + 24*60*60);
			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
			break;
		}
	case MSG_COMM_LOADING_FAILED:
		{
#ifdef PUBSUF_DEBUG
			PrintfTofile("pubsuff.txt", "DNS failed %s\n", domain_sn->Name());
#endif
			domain_sn->SetDomainType(ServerName::DOMAIN_NORMAL);
			domain_sn->SetCookieDNS_Lookup_Done(CookieDNS_Failed);
			domain_sn->SetCookieDNSExpires(g_timecache->CurrentTime() + 24*60*60);
			g_main_message_handler->PostMessage(MSG_COMM_LOADING_FAILED,Id(),1);
		}
		break;
	}
#ifdef PUBSUF_DEBUG
	PrintfTofile("pubsuff.txt", "DNS clean %s\n", domain_sn->Name());
#endif
	if(!lookup_url->IsEmpty())
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_URL_LOADING_FAILED, lookup_url->Id(),0);
		lookup_url.UnsetURL();
	}
	if(lookup)
		g_main_message_handler->RemoveDelayedMessage(MSG_COMM_LOADING_FAILED, lookup->Id(),0);

	SafeDestruction(lookup);
	lookup = NULL;

	g_main_message_handler->UnsetCallBacks(this);
}

OP_STATUS DNS_RegistryCheck_Handler::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	static const OpMessage messages[] =
    {
        MSG_COMM_LOADING_FINISHED,
        MSG_COMM_LOADING_FAILED
    };

	RETURN_IF_ERROR(mh->SetCallBackList((parent ? parent : master), Id(), messages, ARRAY_SIZE(messages)));

	return OpStatus::OK;
}

BOOL DNS_RegistryCheck_Handler::Start_Lookup()
{
#ifdef PUBSUF_DEBUG
	PrintfTofile("pubsuff.txt", "DNS start lookup %s\n", domain_sn->Name());
#endif
	OP_STATUS op_err = OpStatus::OK;

	domain_sn->SetCookieDNS_Lookup_Done(CookieDNS_NotDone);
	domain_sn->SetCookieDNSExpires(0);

	BOOL use_proxy = FALSE;

	const uni_char *proxy = NULL;
	BOOL autoproxy = FALSE;

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	if (g_pcnet->IsAutomaticProxyOn())
	{
		proxy = g_pcnet->GetStringPref(PrefsCollectionNetwork::AutomaticProxyConfigURL).CStr();
		if(proxy && *proxy)
			autoproxy = TRUE;
	}
#endif

	if(!autoproxy)
	{
		proxy = urlManager->GetProxy(NULL, URL_HTTP);
	}

	if(!force_direct_lookup && proxy)
	{
		if(*proxy)
		{
			use_proxy = TRUE;
		}
	}

	if(use_proxy)
	{
		URL_InUse temp;

		do{
#ifdef PUBSUF_DEBUG
		PrintfTofile("pubsuff.txt", "DNS start lookup proxy %s\n", domain_sn->Name());
#endif
			OpString8 lookup_url_text;
			URL ref;

			if(OpStatus::IsError(op_err = lookup_url_text.SetConcat("http://", domain_sn->Name(), "/")))
				break;

			URL temp_url = urlManager->GetURL(lookup_url_text.CStr(), NULL, TRUE);
			if(temp_url.IsEmpty())
				break;

			temp.SetURL(temp_url);

			temp_url.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_HEAD);
			temp_url.SetAttribute(URL::KDisableProcessCookies, TRUE);
			temp_url.SetAttribute(URL::KSendOnlyIfProxy, TRUE);
			temp_url.SetAttribute(URL::KHTTP_Managed_Connection, TRUE);

			if(temp_url.Load(g_main_message_handler,ref, FALSE, FALSE, FALSE, TRUE) != COMM_LOADING)
				break;

			if(OpStatus::IsError(op_err = g_main_message_handler->SetCallBack(this, MSG_HEADER_LOADED, temp_url.Id())))
				break;
			if(OpStatus::IsError(op_err = g_main_message_handler->SetCallBack(this, MSG_URL_LOADING_FAILED, temp_url.Id())))
				break;

			g_main_message_handler->PostDelayedMessage(MSG_URL_LOADING_FAILED, temp_url.Id(),0, 20000); // Fail if Request does not resolve within 20 seconds
			lookup_url = temp;
			temp.UnsetURL();
		}while(0);

		if(temp->IsValid())
			temp->StopLoading(NULL);
	}

	if(lookup_url->IsEmpty())
	{

#ifdef PUBSUF_DEBUG
		PrintfTofile("pubsuff.txt", "DNS start lookup dns %s\n", domain_sn->Name());
#endif
		Comm *comm = Comm::Create(g_main_message_handler, domain_sn, 80);

		if(comm)
		{
			lookup = comm;
			OpStatus::Ignore(op_err = comm->SetCallbacks(this, this));
			OpStatus::Ignore(op_err = g_main_message_handler->SetCallBack(this, MSG_COMM_NAMERESOLVED, comm->Id()));
			g_main_message_handler->PostDelayedMessage(MSG_COMM_LOADING_FAILED, comm->Id(),0, 20000); // Fail if name lookup does not resolve within 20 seconds
			comm->SetManagedConnection();

			CommState stat = comm->LookUpName(domain_sn);

			if(stat != COMM_LOADING && stat != COMM_WAITING)
			{
#ifdef PUBSUF_DEBUG
				PrintfTofile("pubsuff.txt", "DNS start lookup fail %s\n", domain_sn->Name());
#endif
				comm = NULL;
			}
		}

		if(comm == NULL)
		{
#ifdef PUBSUF_DEBUG
			PrintfTofile("pubsuff.txt", "DNS start lookup fail2 %s\n", domain_sn->Name());
#endif
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			return FALSE;
		}
	}

	RAISE_IF_MEMORY_ERROR(op_err);

	return (OpStatus::IsMemoryError(op_err)) ? FALSE : TRUE;
}

#endif // PUBSUFFIX_ENABLED

