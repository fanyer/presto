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

#include "modules/dochand/win.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/util/datefun.h"
#include "modules/util/timecache.h"
#include "modules/formats/uri_escape.h"

#include "modules/url/url_man.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/hdsplit.h"
#include "modules/cookies/url_cm.h"
#include "modules/cookies/cookie_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int uint_cmp(const void *arg1,const void *arg2)
{
	unsigned short entry1 = *((unsigned short *) arg1);
	unsigned short entry2 = *((unsigned short *) arg2);
	return (entry1 < entry2 ? -1 : (entry1 > entry2 ? 1 : 0));
}

#ifdef __cplusplus
}
#endif

BOOL CheckIsInDomainList(const char *domain, const char *domain_list);

BOOL CheckIsDotComDomain(const OpStringC8 &domain)
{
	if(domain.Length() == 3) // Current list only has 3 characters
	{
		return CheckIsInDomainList(domain.CStr(), ":com:net:org:edu:gov:mil:int:");
	}

	return FALSE;
}

void Cookie_Manager::HandleSingleCookieL(Head &currently_parsed_cookies, URL_Rep *url, const char *cookiearg,
							   const char *request,
							   int version,
							   BOOL set_from_http,
							   Window* win
							   )
{
	if (version && !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableCookiesV2))
		return;

	if(url == NULL || ((URLType) url->GetAttribute(URL::KType) != URL_HTTP && (URLType) url->GetAttribute(URL::KType) != URL_HTTPS
#ifdef __FILE_COOKIES_SUPPORTED
		 && (URLType) url->GetAttribute(URL::KType) != URL_FILE
#endif
#ifdef WEBSOCKETS_SUPPORT
		 && (URLType) url->GetAttribute(URL::KType) != URL_WEBSOCKET && (URLType) url->GetAttribute(URL::KType) != URL_WEBSOCKET_SECURE
#endif //WEBSOCKETS_SUPPORT
		) || cookiearg == NULL || *cookiearg == '\0' ||
		op_strlen(cookiearg) > GetMaxCookieLen())
			return;

	//PrintfTofile("cookies.txt", "URL: %s\r\nSet-Cookie: %s\r\nFull request: %s\r\n\r\n", url->Name(), cookiearg, request);

	OpStringC8 hostname(url->GetAttribute(URL::KHostName));
	ServerName *servername = (ServerName *) url->GetAttribute(URL::KServerName, NULL);

	OP_ASSERT(hostname.HasContent() && servername);
	if(hostname.IsEmpty() || !servername
#ifdef _ASK_COOKIE
		|| servername->GetAcceptCookies(TRUE) == COOKIE_NONE
#endif
#ifdef COOKIES_CONTROL_PER_CONTEXT
		|| (context_id && !cookies_enabled_for_context )
#endif
		)
		return;

	ParameterList cookie(KeywordIndex_Cookies);
	ANCHOR(ParameterList, cookie);

	cookie.SetValueL((char *) cookiearg, PARAM_SEP_SEMICOLON|PARAM_ONLY_SEP|NVS_COOKIE_SEPARATION
#ifdef PARAM_CHECK_FOR_INVALID_QUOTES
		| PARAM_CHECK_FOR_INVALID_QUOTES
#endif
		);

	if(cookie.Cardinal() == 0)
		return;

	OpStackAutoPtr<Cookie_Item_Handler> item(OP_NEW_L(Cookie_Item_Handler, ()));


	item->context_id = url->GetContextId();

	URL_DataStorage *url_ds = url->GetDataStorage();
	MsgHndlList		*mh_list = (url_ds ? url_ds->GetMessageHandlerList() : NULL);

	if (win)
		item->first_window = win;
	else if(mh_list)
	{
		MsgHndlList::Iterator itr = mh_list->Begin();
		for(; itr != mh_list->End(); ++itr)
		{
			MessageHandler *mh = (*itr)->GetMessageHandler();
			if (mh && (item->first_window = mh->GetWindow()) != NULL)
			{
				break;
			}
		}
	}

	{
		URL temp_url(url, (char *) NULL);
		item->url = temp_url;
	}
	//item->url.IncUsed();
	item->request.SetL(request);
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TagUrlsUsingPasswordRelatedCookies))
	{
		item->flags.have_password = !!url->GetAttribute(URL::KHavePassword);
		item->flags.have_authentication = !!url->GetAttribute(URL::KHaveAuthentication);
	}
#ifdef _SSL_USE_SMARTCARD_
	item->flags.smart_card_authenticated = url->GetAttribute(URL::KHaveSmartCardAuthentication);
#endif

	Parameters *cookie_val= NULL;
	Parameters *domainarg= NULL;
	Parameters *patharg= NULL;

	Parameters *portarg= NULL;
	item->version = version;

	int hostname_len = hostname.Length();
	ServerName *hostdomain_sn = servername->GetParentDomain();

	OpStringC8 url_path = url->GetAttribute(URL::KPath);
	COOKIE_MODES cmode = (COOKIE_MODES) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiesEnabled);

	if(url->GetAttribute(URL::KIsThirdParty) &&
		cmode == COOKIE_WARN_THIRD_PARTY)
	{
		COOKIE_MODES tcmode = servername->GetAcceptThirdPartyCookies(TRUE);
		if(tcmode != COOKIE_DEFAULT &&
			tcmode != COOKIE_ACCEPT_THIRD_PARTY)
			{
				return;
			}
		item->flags.accepted_as_third_party = TRUE;
	}

	cookie_val= cookie.First();
	cookie_val->SetNameID(HTTP_Cookie_Unknown);
	if(!cookie_val->Name() || *cookie_val->Name() == '\0' || (version >= 1 && *cookie_val->Name() == '$'))
		LEAVE(OpStatus::ERR);

	domainarg= cookie.GetParameterByID(HTTP_Cookie_Domain);
	patharg= cookie.GetParameterByID(HTTP_Cookie_Path);
	item->flags.secure= (cookie.GetParameterByID(HTTP_Cookie_Secure) != NULL);
	item->flags.set_fromhttp = set_from_http;
	item->flags.assigned=cookie_val->AssignedValue();
	item->flags.httponly= item->flags.set_fromhttp && (cookie.GetParameter("httponly") != NULL);

	if(patharg && (!patharg->AssignedValue() || patharg->GetValue().IsEmpty()))
		patharg = NULL;
	if(domainarg && (!domainarg->AssignedValue() || domainarg->GetValue().IsEmpty()))
		domainarg = NULL;

	Parameters *max_age;
	max_age= cookie.GetParameterByID(HTTP_Cookie_Max_Age);
	if(max_age)
		max_age->StripQuotes();
	if(max_age && (!max_age->AssignedValue() || max_age->GetValue().IsEmpty()))
	{
		LEAVE(OpStatus::ERR);
	}

	if(max_age)
	{
		do{
			OpStringC8 tempname(max_age->GetValue());
			int pos_plus = tempname.FindLastOf('+');
			int pos_minus = tempname.FindLastOf('-');
			if(tempname.SpanOf("0123456789+-") != tempname.Length() ||
				(pos_plus != KNotFound && pos_plus != 0) ||
				(pos_minus != KNotFound && pos_minus != 0))
				LEAVE(OpStatus::ERR); // anything but digits or a "+" at the start is invalid
		}while(0);
		int age = max_age->GetSignedValue();
		if(age <= 0)
			item->flags.delete_cookie = TRUE;
#if defined(HAVE_TIME_T64)
		else if(age == INT_MAX)
			item->expire = 0; // invalid input -> session cookie
#else
		else if(age == INT_MAX ||
			(g_op_time_info->GetTimeUTC()/1000.0) > (double) INT_MAX ||
			age >= INT_MAX - (time_t) (g_op_time_info->GetTimeUTC()/1000.0))
			item->expire = 0; // invalid input or would result in integer overflow -> session cookie
#endif
		else
		{
			item->expire =
				(time_t) (g_op_time_info->GetTimeUTC()/1000.0)
				+ age;
			if(item->expire <
				(time_t) (g_op_time_info->GetTimeUTC()/1000.0)
				)
				item->flags.delete_cookie = TRUE;
		}
	}

	if(!version)
	{
		Parameters *expires;

		expires= cookie.GetParameterByID(HTTP_Cookie_Expires);

		if(cookie_val->Name() == NULL || *(cookie_val->Name()) == '\0' /*||
			(expires && (!expires->AssignedValue() || expires->Value() == NULL || *(expires->Value()) == '\0'))*/)
			{
				return;
			}

		if(!max_age && expires && expires->AssignedValue() && expires->GetValue().HasContent())
		{
			expires->StripQuotes();
			item->expire = GetDate(expires->Value());
		}
		if(item->expire && item->expire <
			(time_t) (g_op_time_info->GetTimeUTC()/1000.0))
			item->flags.delete_cookie = TRUE;
	}
	else
	{
		Parameters *comment= NULL;
		Parameters *comment_url= NULL;
		Parameters *discard_arg;
		Parameters *version_arg;

		comment= cookie.GetParameterByID(HTTP_Cookie_Comment);
		comment_url= cookie.GetParameterByID(HTTP_Cookie_CommentURL);
		discard_arg = cookie.GetParameterByID(HTTP_Cookie_Discard);
		portarg = cookie.GetParameterByID(HTTP_Cookie_Port);
		version_arg = cookie.GetParameterByID(HTTP_Cookie_Version);

		if(cookie_val->GetName().IsEmpty())
		{
			LEAVE(OpStatus::ERR);
		}


		if(discard_arg)
			item->flags.discard_at_exit = TRUE;

		if(comment)
		{
			comment->StripQuotes();
			item->comment.SetL(comment->Value());
		}

		if(comment_url)
		{
			comment_url->StripQuotes();
			item->comment_URL.SetL(comment_url->Value());
		}

		if(version_arg)
		{
			int ver = version_arg->GetSignedValue();
			item->version = (ver < 1 ? 0 : ver);
		}
		else
			item->version = 0;

		if(item->version < 1)
		{
			LEAVE(OpStatus::ERR);
		}
	}
#if TEMPORARY_COOKIES_EXPIRE > 0
	if (item->expire == 0)
		item->expire = (time_t) (g_op_time_info->GetTimeUTC()/1000.0 + TEMPORARY_COOKIES_EXPIRE);
#endif

	item->name.SetL(cookie_val->Name());
	item->value.SetL(cookie_val->Value());
	//if(item->value.HasContent() && item->value[0] != '\"' && item->value.FindFirstOf(',') != KNotFound)
	//	LEAVE(OpStatus::ERR); // Do not permit unquoted commas, may upset Cookie v1+ servside parsers that understand the comma syntax.

	switch (version)
	{
	case 0:
		item->cookie_type = NETSCAPE_COOKIE;
		break;
	case 1:
		item->cookie_type = COOKIE2_COOKIE;
		break;
	default:
		// from what i can tell, this function should always be passed
		// version corresponding to eg Set-Cookie/Set-Cookie2, though
		// actual version passed from server is what you get from the
		// cookie later on. thus, this assert should only trigger if
		// the function is called improperly or when a new cookie type
		// is added.
		OP_ASSERT(!"passed invalid cookie version");
		item->cookie_type = UNKNOWN_COOKIE;
	}

#ifdef _ASK_COOKIE
	if(!item->flags.discard_at_exit){
		COOKIE_DELETE_ON_EXIT_MODE disc_mode =  servername->GetDeleteCookieOnExit(TRUE);
		if(disc_mode == COOKIE_EXIT_DELETE_DEFAULT)
			disc_mode  = (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AcceptCookiesSessionOnly) ? COOKIE_DELETE_ON_EXIT : COOKIE_NO_DELETE_ON_EXIT);

		item->flags.discard_at_exit = (disc_mode == COOKIE_DELETE_ON_EXIT ? TRUE : FALSE);
	}
#endif

#ifdef __FILE_COOKIES_SUPPORTED
	if((URLType) url->GetAttribute(URL::KType) == URL_FILE)
	{
		domainarg = NULL; // No domain needed, single server only
	}
#endif
	if(domainarg && domainarg->Value())
	{
		item->recvd_domain.SetL(domainarg->Value());
		domainarg->StripQuotes();
		const char *domain1 = domainarg->Value();
		while(*domain1 == '.')
			domain1++;

		OpStringC8 domain(domain1);

		//Empty domain (or only periods)
		if(domain.IsEmpty())
		{
#if defined(_ASK_COOKIE)
			WarnCookieDomainError(item->first_window, url, request);
#endif
			LEAVE(OpStatus::ERR);
		}

		ServerName *domain_sn = g_url_api->GetServerName(domain, TRUE);
		if(domain_sn == NULL)
			return; // Invalid servername, will not match
		item->domain_sn = domain_sn;

		if(!hostdomain_sn && hostname.SpanOf("0123456789") < hostname_len)
		{
			BOOL set_host_and_local_as_domain = FALSE;
			if(version)
			{
				if(!(domain.CompareI("local") == 0 ||
					(domain_sn->GetNameComponentCount() == 2 &&
					 domain_sn->GetNameComponent(0).Compare("local") == 0 &&
					 domain_sn->GetNameComponent(1).Compare(hostname) == 0)) )
				{
#if defined(_ASK_COOKIE)
					WarnCookieDomainError(item->first_window, url, request);
#endif
					LEAVE(OpStatus::ERR);
				}
			}
			else
			{
				if(servername != domain_sn)
				{
#if defined(_ASK_COOKIE)
					WarnCookieDomainError(item->first_window, url, request);
#endif
					LEAVE(OpStatus::ERR);
				}
				set_host_and_local_as_domain = TRUE;
			}

			if(set_host_and_local_as_domain)
				LEAVE_IF_ERROR(item->domain.SetConcat(servername->Name(),".local"));
			item->flags.only_server = TRUE;
		}
		else
		{
			if(domain_sn->GetNameComponentCount() < 2)
			{
#if defined(_ASK_COOKIE)
				WarnCookieDomainError(item->first_window, url, request);
#endif
				LEAVE(OpStatus::ERR);
			}

			if(domain_sn != servername)
			{
				ServerName *common_domain = servername->GetCommonDomain(domain_sn);

				// If domain is not common, and there is not at least two levels in the common name refuse to accept it
				if(common_domain == NULL ||
					common_domain->GetNameComponentCount() < 2||
					common_domain != domain_sn // domain_sn must be a parent domain

					)
				{
#if defined(_ASK_COOKIE)
					WarnCookieDomainError(item->first_window, url, request);
#endif
					LEAVE(OpStatus::ERR);
				}

#if defined PUBSUFFIX_ENABLED
				ServerName::DomainType domain_type = domain_sn->GetDomainTypeASync();

				if(domain_type == ServerName::DOMAIN_WAIT_FOR_UPDATE)
				{
					if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableCookiesDNSCheck))
						item->flags.do_dns_lookup = TRUE;
				}
				else if(domain_type != ServerName::DOMAIN_NORMAL)
				{
					if(common_domain != hostdomain_sn)
					{
						if(version  != 0)
						{
							LEAVE(OpStatus::ERR);
						}
					}
					item->flags.only_server = TRUE;
					OpStatus::Ignore(item->domain.Set(item->url->GetAttribute(URL::KHostName)));//OOM7 Handled by code below
					OpStatus::Ignore(item->recvd_domain.SetConcat("\"", item->url->GetAttribute(URL::KHostName), "\""));//OOM7 Handled by code below
				}
				else
				{
					if(common_domain != hostdomain_sn && !CheckIsDotComDomain(domain_sn->GetNameComponent(0)) )
					{
						if(version  != 0)
						{
#if defined(_ASK_COOKIE)
							WarnCookieDomainError(item->first_window, url, request);
#endif
							LEAVE(OpStatus::ERR);
						}
					}
				}
#else // !PUBSUFFIX_ENABLED
				if(common_domain != hostdomain_sn)
				{
					if(CheckIsDotComDomain(domain_sn->GetNameComponent(0)))
					{
						// only hosts in domains in the above list are allowed
						// to set cookies for more than its own domain level
						// without a challenge
					}
#if defined COOKIE_USE_DNS_LOOKUP
					else if(version == 0)
					{
						// Do a DNS lookup to check if the domain has a valid IP address (only oldstyle cookies)
						if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableCookiesDNSCheck))
							item->flags.do_dns_lookup = TRUE;
					}
#endif
					else
					{
#if defined(_ASK_COOKIE)
						WarnCookieDomainError(item->first_window, url, request);
#endif
						LEAVE(OpStatus::ERR);
					}
				}
#if defined COOKIE_USE_DNS_LOOKUP
				else if(hostdomain_sn->GetNameComponentCount() == 2)
				{
					if(!CheckIsDotComDomain(domain_sn->GetNameComponent(0)))
					{
						// for single dot domain target
						// Do a DNS lookup to check if the domain has a valid IP address (all versions)
						if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableCookiesDNSCheck))
							item->flags.do_dns_lookup = TRUE;
					}
				}
#endif
#endif
			}
		}
		if(item->domain.IsEmpty())
		{
			item->domain.SetL(domain);
		}
		item->flags.recv_dom = TRUE;
	}
	else if(domainarg)
	{
#if defined(_ASK_COOKIE)
		WarnCookieDomainError(item->first_window, url, request);
#endif
		LEAVE(OpStatus::ERR);
	}
#ifdef __FILE_COOKIES_SUPPORTED
	else if((URLType) url->GetAttribute(URL::KType) == URL_FILE)
	{
		LEAVE_IF_ERROR(item->domain.SetConcat(servername->Name(), ".$localfile$"));
		item->flags.only_server = TRUE;
	}
#endif
	else
	{
		OP_ASSERT(hostname.HasContent());
		LEAVE_IF_ERROR(item->domain.SetConcat(servername->Name(), (hostdomain_sn == NULL &&
			hostname.SpanOf("0123456789.") < hostname_len ? ".local": NULL)));
		item->flags.only_server = TRUE;
		item->domain_sn = servername;
	}

	{
		if(patharg)
			patharg->StripQuotes();

		const char *path1 = (patharg && patharg->Value() ? patharg->Value() : NULL);

		const char *temppath1 = url_path.CStr();
		int len;

		if(path1)
		{
			while(*path1 && op_isspace((unsigned char) *path1))
				path1++;

			if(*path1 == '\0')
				path1 = NULL;
		}

		OpStringC8 path(path1);

		if(path.HasContent())
		{
			if((url_path.IsEmpty()  && path.Compare("/") != 0 ) ||
				(url_path.Compare(path.CStr(), path.Length()) != 0))
			{
#ifdef _ASK_COOKIE
				if(servername->GetAcceptIllegalPaths() != COOKIE_ILLPATH_ACCEPT)
				{
					if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiePathErrMode) == COOKIE_ILLEGAL_PATH_REFUSE_ALL ||
						servername->GetAcceptIllegalPaths() != COOKIE_ILLPATH_DEFAULT)
						{
							LEAVE(OpStatus::ERR);
						}

					if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CookiePathErrMode) != COOKIE_ILLEGAL_PATH_ACCEPT_ALL)
						item->flags.illegal_path = TRUE;
				}
#else
				// Accept without warning, as that is the default policy now.
#endif
			}

			temppath1 = path.CStr();
			item->flags.recv_path = TRUE;
			item->recvd_path.SetL(path);
		}
		//else if(patharg)
		//	return;

		BOOL is_url_path = (url_path.CStr() == temppath1);

		if(temppath1 && *temppath1 != '/')
			LEAVE(OpStatus::ERR);

		if(temppath1)
			temppath1++;

		OpStringC8 temppath(temppath1);

		len = temppath.SpanNotOf(is_url_path ? "#?" : "#");

		if(is_url_path)
		{
			while(len >0 && temppath[len-1] != '/')
				len --;
		}
		if(len)
		{
			item->path.SetL(temppath.CStr(),len);
			item->flags.full_path_only = (temppath[len-1]=='/');
		}
		else
			item->path.SetL("");
	}

	{
		if(portarg)
			portarg->StripQuotes();

		OpStringC8 ports(portarg && portarg->Value() ? portarg->Value() : NULL);
		if(version && ports.HasContent())
		{
			ParameterList port_split;
			ANCHOR(ParameterList, port_split);

			item->port.SetL(ports);
			port_split.SetValueL(ports.CStr(), PARAM_SEP_COMMA | PARAM_NO_ASSIGN | PARAM_ONLY_SEP);
			item->portlist = OP_NEWA_L(unsigned short, port_split.Cardinal()+1);

			Parameters *port_item = port_split.First();
			int count = 0;
			while(port_item)
			{
				do{
					OpStringC8 tempname(port_item->GetName());
					int pos_plus = tempname.FindLastOf('+');
					if(tempname.SpanOf("0123456789+") != tempname.Length() ||
						(pos_plus != KNotFound && pos_plus != 0))
						LEAVE(OpStatus::ERR); // anything but digits or a "+" at the start is invalid
 				}while(0);

				unsigned long portnumber= port_item->GetUnsignedName();
				if(portnumber>0 && portnumber <= 65535)
					item->portlist[count++] = (unsigned short) portnumber;

				port_item = port_item->Suc();
			}

			unsigned short loading_port = (unsigned short) url->GetAttribute(URL::KResolvedPort);

			if(!count)
			{
				if(!port_split.Empty())
					LEAVE(OpStatus::ERR); // Empty list after parsing a non-empty list is no valid
				item->portlist[count++] = loading_port;
			}

			item->port_count = count;

			op_qsort(item->portlist, count, sizeof(unsigned short),uint_cmp);

			if(op_bsearch(&loading_port, item->portlist, count, sizeof(unsigned short),uint_cmp) == NULL)
			{
				LEAVE(OpStatus::ERR);
			}

		}
	}

	{
		Cookie_Item_Handler *prev_items = (Cookie_Item_Handler *) currently_parsed_cookies.First();

		while(prev_items)
		{
			if( prev_items->domain_sn == item->domain_sn &&
				prev_items->name.Compare(item->name) == 0 &&
				UriUnescape::strcmp(prev_items->path.CStr(), item->path.CStr(), UriUnescape::Safe) == 0)
			{
				if(prev_items->version == 0  ||
					(prev_items->version > 0 && item->version > 0))
				{
					Cookie_Item_Handler *cur_item = prev_items;
					prev_items = (Cookie_Item_Handler *) prev_items->Suc();

						cur_item->Out();
					OP_DELETE(cur_item);
					continue;
				}
				else
				{
					LEAVE(OpStatus::ERR);
				}
			}
			prev_items = (Cookie_Item_Handler *) prev_items->Suc();
		}
	}

#ifdef _ASK_COOKIE
	COOKIE_MODES accept_cookie_mode = servername->GetAcceptCookies(TRUE);
#endif

	if(
		!item->flags.illegal_path &&
		((item->flags.delete_cookie
#if defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
			&& !item->flags.do_dns_lookup
#endif
		)
#ifdef _ASK_COOKIE
#ifndef ASK_MODIFY_COOKIE
		|| accept_cookie_mode == COOKIE_ALL
		|| accept_cookie_mode == COOKIE_SEND_NOT_ACCEPT_3P
		|| CheckAcceptCookieUpdates(item.get())
#endif // ASK_MODIFY_COOKIE
#endif // _ASK_COOKIE
		))
		SetCookie(item.get());
	else
	{
		item->Into(&currently_parsed_cookies);
		item.release();
	}
}


