/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/network/network.h"
#include "modules/url/url_api.h"
#include "modules/url/url_man.h"
#include "modules/network/src/op_server_name_impl.h"

BOOL Network::SetVisited(OpURL url, URL_CONTEXT_ID context_id)
{
	OpString8 urlpath;
	URL referrer;
	OP_STATUS op_err = url.GetNameWithFragmentUsername(urlpath, TRUE, OpURL::PasswordVisible_NOT_FOR_UI);
	if (OpStatus::IsError(op_err))
		return FALSE;

	URL temp = g_url_api->GetURL(referrer, urlpath, FALSE, context_id);
	return (BOOL) temp.SetAttribute(URL::KIsFollowed, TRUE);

}

BOOL Network::IsVisited(OpURL url, URL_CONTEXT_ID context_id)
{
	OpString8 urlpath;
	URL referrer;
	OP_STATUS op_err = url.GetNameWithFragmentUsername(urlpath, TRUE, OpURL::PasswordVisible_NOT_FOR_UI);
	if (OpStatus::IsError(op_err))
		return FALSE;

	URL temp = g_url_api->GetURL(referrer, urlpath, FALSE, context_id);
	return (BOOL) temp.GetAttribute(URL::KIsFollowed);
}

void Network::GetServerName(const OpStringC8 &name, ServerNameCallback *callback, BOOL create)
{
	OP_ASSERT(callback);
	OP_STATUS status = OpStatus::OK;
	OpServerNameImpl *result = OP_NEW(OpServerNameImpl,(g_url_api->GetServerName(name, create)));

	if (!result)
		status = OpStatus::ERR_NO_MEMORY;

	callback->ServerNameResult(result, status );
}

void Network::GetServerName(const OpStringC &name, ServerNameCallback *callback, BOOL create)
{
	OP_ASSERT(callback);
	OP_STATUS status = OpStatus::OK;
	OpServerNameImpl *result = OP_NEW(OpServerNameImpl,(g_url_api->GetServerName(name, create)));

	if (!result)
		status = OpStatus::ERR_NO_MEMORY;

	callback->ServerNameResult(result, status );
}

BOOL Network::ResolveUrlNameL(const OpStringC& name_in, OpString& resolved_out, BOOL entered_by_user)
{
	return ::ResolveUrlNameL(name_in, resolved_out, entered_by_user);
}

OP_STATUS Network::GetCookiesL(OpURL url, OpString8 &cookies
#ifdef URL_DETECT_PASSWORD_COOKIES
								,BOOL already_have_password
								,BOOL already_have_authentication
								,BOOL &have_password
								,BOOL &has_password
#endif
								)
{
	int version = 0, max_version;
	return cookies.Set(urlManager->GetCookiesL(url.GetURL().GetRep(), version, max_version
#ifdef URL_DETECT_PASSWORD_COOKIES
								   ,already_have_password, already_have_authentication,have_password, has_password
#endif
								   , url.GetURL().GetRep()->GetContextId()));
}

void Network::HandleSingleCookieL(OpURL url, const char *cookiearg, const char *request)
{
	int version = 0;
	urlManager->HandleSingleCookieL(url.GetURL().GetRep(), cookiearg, request, version, url.GetURL().GetContextId(), NULL);
}

void Network::PrefetchDNS(OpURL url)
{
	URL temp = url.GetURL();
	temp.PrefetchDNS();
}

#ifdef QUICK_COOKIE_EDITOR_SUPPORT
void Network::BuildCookieEditorListL()
{
	urlManager->BuildCookieEditorListL();
}

#endif

void Network::PurgeData(BOOL visited_links, BOOL disk_cache, BOOL sensitive, BOOL session_cookies, BOOL cookies, BOOL certificates, BOOL memory_cache)
{
	g_url_api->PurgeData(visited_links, disk_cache, sensitive, session_cookies, cookies, certificates, memory_cache);
}

#ifdef URL_ENABLE_SAVE_DATA
void Network::SaveDataL(BOOL visited_links, BOOL disk_cache,  BOOL cookies)
{
	g_url_api->SaveDataL(visited_links, disk_cache, cookies);
}
#endif

void Network::CheckTimeOuts()
{
	urlManager->CheckTimeOuts();
}

void Network::CleanUp(BOOL ignore_downloads)
{
	urlManager->CleanUp(ignore_downloads);
}

#ifdef NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
void Network::RestartAllConnections()
{
	urlManager->RestartAllConnections();
}

void Network::StopAllLoadingURLs()
{
	urlManager->StopAllLoadingURLs();
}

#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS

void Network::CloseAllConnections()
{
	urlManager->CloseAllConnections();
}

#ifdef PI_NETWORK_INTERFACE_MANAGER
BOOL Network::IsNetworkAvailable()
{
	return g_url_api->IsNetworkAvailable();
}
#endif // PI_NETWORK_INTERFACE_MANAGER

#ifdef _ASK_COOKIE
#ifdef ENABLE_COOKIE_CREATE_DOMAIN
void Network::CreateCookieDomain(const char *domain_name)
{
	urlManager->CreateCookieDomain(domain_name);
}
#endif

#ifdef URL_ENABLE_EXT_COOKIE_ITEM
void Network::SetCookie(Cookie_Item_Handler *cookie_item)
{
	urlManager->SetCookie(cookie_item);
}
#endif
#endif // _ASK_COOKIE

void Network::SetPauseStartLoading(BOOL val)
{
	urlManager->SetPauseStartLoading(val);
}

#ifdef URL_ENABLE_REGISTER_SCHEME
URLType Network::RegisterUrlScheme(const OpStringC &scheme_name, BOOL have_servername, URLType real_type, BOOL *actual_servername_flag)
{
	return g_url_api->RegisterUrlScheme(scheme_name, have_servername, real_type, actual_servername_flag);
}
#endif
