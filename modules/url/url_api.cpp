/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2004-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#include "modules/url/url2.h"
#include "modules/url/url_man.h"
#include "modules/url/url_api.h"
#include "modules/url/protocols/commcleaner.h"
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#endif
#include "modules/locale/locale-enum.h"
#include "modules/url/url_rep.h"
#include "modules/about/operrorpage.h"
#include "modules/about/opblockedurlpage.h"

#include "modules/url/url_lop_api.h"

#ifdef PI_NETWORK_INTERFACE_MANAGER
#include "modules/pi/network/OpNetworkInterface.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/internet_connection_status_checker.h"
#endif

#ifdef PI_INTERNET_CONNECTION
#include "modules/pi/network/OpInternetConnection.h"
#endif

#ifdef USE_SPDY
#include "modules/url/protocols/spdy/spdy_internal/spdy_settingsmanager.h"
#endif // USE_SPDY

#ifndef URL_MODULE_REQUIRED
URL_API *g_url_api= NULL;

CommCleaner *g_comm_cleaner = NULL;
#endif // URL_MODULE_REQUIRED

/** Constructor */
URL_API::URL_API()
{
}

/** Destructor */
URL_API::~URL_API()
{
	if(urlManager)
		urlManager->WriteFiles();

	OP_DELETE(urlManager);
	urlManager = NULL;

	OP_DELETE(g_comm_cleaner);
	g_comm_cleaner = NULL;
}

/** Set up the object */
void URL_API::ConstructL()
{
	if (!urlManager)
	{
		OpStackAutoPtr<URL_Manager> temp(NULL);

		temp.reset(OP_NEW_L(URL_Manager, ()));

		urlManager = temp.release();

		urlManager->InitL();

#ifndef RAMCACHE_ONLY
#ifndef SEARCH_ENGINE_CACHE
		urlManager->ReadDCacheFileL();
		urlManager->ReadVisitedFileL();
#endif
#endif // !RAMCACHE_ONLY
	}

	if (!g_comm_cleaner)
	{
		OpStackAutoPtr<CommCleaner> temp_cleaner(OP_NEW_L(CommCleaner, ()));

		temp_cleaner->ConstructL();

		g_comm_cleaner = temp_cleaner.release();
	}
}

#ifdef HISTORY_SUPPORT
BOOL URL_API::GlobalHistoryPermitted(URL &url)
{
	return urlManager->GlobalHistoryPermitted(url);
}
#endif

#ifdef URL_SESSION_FILE_PERMIT
BOOL URL_API::SessionFilePermitted(URL &url)
{
	return urlManager->SessionFilePermitted(url);
}
#endif

BOOL URL_API::LoadAndDisplayPermitted(URL &url)
{
	return urlManager->LoadAndDisplayPermitted(url);
}

#ifdef _MIME_SUPPORT_
BOOL URL_API::EmailUrlSuppressed(URLType url_type)
{
	return urlManager->EmailUrlSuppressed(url_type);
}
#endif

#if defined ERROR_PAGE_SUPPORT || defined URL_OFFLINE_LOADABLE_CHECK
BOOL URL_API::OfflineLoadable(URLType url_type)
{
	return urlManager->OfflineLoadable(url_type);
}
#endif

#ifdef MHTML_ARCHIVE_REDIRECT_SUPPORT
BOOL URL_API::IsOffline(URL &url)
{
	return urlManager->IsOffline(url);
}
#endif


ServerName* URL_API::GetServerName(const OpStringC8 &name, BOOL create)
{
	return urlManager->GetServerName(name.CStr(),create);
}

ServerName* URL_API::GetServerName(const OpStringC &name, BOOL create)
{
	unsigned short dummy_port=0;
	OP_STATUS op_err = OpStatus::OK;
	return urlManager->GetServerName(op_err, name.CStr(),dummy_port, create);
}

ServerName* URL_API::GetFirstServerName()
{
	return urlManager->GetFirstServerName();
}

ServerName* URL_API::GetNextServerName()
{
	return urlManager->GetNextServerName();
}

URL URL_API::GetURL(const uni_char* url, URL_CONTEXT_ID context_id)
{
	OpStringC temp_url(url);
	return GetURL(temp_url, context_id);
}

URL URL_API::GetURL(const URL& prnt_url, const char* url, const char* rel, BOOL unique)
{
	OpStringC8 temp_url(url);
	OpStringC8 temp_rel(rel);
	return GetURL(prnt_url, temp_url, temp_rel, unique);
}

URL URL_API::GetURL(const char* url, const char* rel, BOOL unique, URL_CONTEXT_ID context_id)
{
	OpStringC8 temp_url(url);
	OpStringC8 temp_rel(rel);
	return GetURL(temp_url, temp_rel, unique, context_id);
}

URL URL_API::GetURL(const uni_char* url, const uni_char* rel, BOOL unique, URL_CONTEXT_ID context_id)
{
	OpStringC temp_url(url);
	OpStringC temp_rel(rel);
	return GetURL(temp_url, temp_rel, unique, context_id);
}
URL URL_API::GetURL(const URL& prnt_url, const uni_char* url, BOOL unique, URL_CONTEXT_ID context_id)
{
	OpStringC temp_url(url);
	return GetURL(prnt_url, temp_url, unique, context_id);
}

URL URL_API::GetURL(const URL& prnt_url, const uni_char* url, const uni_char* rel, BOOL unique)
{
	OpStringC temp_url(url);
	OpStringC temp_rel(rel);
	return GetURL(prnt_url, temp_url, temp_rel, unique);
}

URL URL_API::GetURL(const char* url, URL_CONTEXT_ID context_id)
{
	OpStringC8 temp_url(url);
	return GetURL(temp_url, context_id);
}

URL URL_API::GetURL(const URL& prnt_url, const char* url, BOOL unique, URL_CONTEXT_ID context_id)
{
	OpStringC8 temp_url(url);
	return GetURL(prnt_url, temp_url, unique, context_id);
}

URL URL_API::GetURL(const OpStringC &url, URL_CONTEXT_ID context_id)
{
	return urlManager->GetURL(url, context_id);
}

URL URL_API::GetURL(const OpStringC8 &url, const OpStringC8 &rel, BOOL unique, URL_CONTEXT_ID context_id)
{
	return urlManager->GetURL(url, rel, unique, context_id);
}

URL URL_API::GetURL(const OpStringC &url, const OpStringC &rel, BOOL unique, URL_CONTEXT_ID context_id)
{
	return urlManager->GetURL(url, rel, unique, context_id);
}

URL URL_API::GetURL(const URL& prnt_url, const OpStringC &url, BOOL unique, URL_CONTEXT_ID context_id)
{
	return urlManager->GetURL(prnt_url, url, unique, context_id);
}

URL URL_API::GetURL(const URL& prnt_url, const OpStringC &url, const OpStringC &rel, BOOL unique)
{
	return urlManager->GetURL(prnt_url, url, rel, unique);
}

URL URL_API::GetURL(const URL& prnt_url, const OpStringC8 &url, BOOL unique, URL_CONTEXT_ID context_id)
{
	return urlManager->GetURL(prnt_url, url, unique, context_id);
}

URL URL_API::GetURL(const OpStringC8 &url, URL_CONTEXT_ID context_id)
{
	return urlManager->GetURL(url, context_id);
}

void URL_API::MakeUnique(URL &url)
{
	urlManager->MakeUnique(url.GetRep());
}

URL URL_API::MakeUniqueCopy(const URL &url)
{
	OpString8 url_string;

	if (OpStatus::IsSuccess(url.GetAttribute(URL::KName_With_Fragment_Username_Password_NOT_FOR_UI, url_string)))
	{
		if (url_string.HasContent())
		{
			URL ref;
			return GetURL(ref, url_string.CStr(), TRUE, url.GetContextId());
		}
	}

	return URL();
}

BOOL URL_API::ResolveUrlNameL(const OpStringC& name_in, OpString& resolved_out, BOOL entered_by_user)
{
	return ::ResolveUrlNameL(name_in, resolved_out, entered_by_user);
}

URL	URL_API::GetURL(const URL& prnt_url, const OpStringC8 &url, const OpStringC8 &rel, BOOL unique )
{
	return urlManager->GetURL(prnt_url, url, rel, unique);
}

const char *URL_API::GetCookiesL(URL &url
								,int& version
								,int &max_version
#ifdef URL_DETECT_PASSWORD_COOKIES
								,BOOL already_have_password
								,BOOL already_have_authentication
								,BOOL &have_password
								,BOOL &has_password
#endif
								)
{
	return urlManager->GetCookiesL(url.GetRep(), version, max_version
#ifdef URL_DETECT_PASSWORD_COOKIES
								   ,already_have_password, already_have_authentication,have_password, has_password
#endif
								   , url.GetRep()->GetContextId());
}

void URL_API::HandleSingleCookieL(URL &url, const char *cookiearg, const char *request, int version, Window *win /*= NULL*/)
{
	urlManager->HandleSingleCookieL(url.GetRep(), cookiearg, request, version, url.GetContextId(), win);
}

void URL_API::HasStartedLoading()
{
	urlManager->HasStartedLoading();
}
#ifdef QUICK_COOKIE_EDITOR_SUPPORT
void URL_API::BuildCookieEditorListL()
{
	urlManager->BuildCookieEditorListL();
}

#endif

void URL_API::CheckCookiesReadL()
{
	urlManager->CheckCookiesReadL();
}

#ifdef URL_API_CLEAR_COOKIE_API
void URL_API::ClearCookiesL(BOOL delete_filters)
{
	urlManager->ClearCookiesCommitL(delete_filters);
}
#endif

#ifdef NEED_URL_COOKIE_ARRAY
OP_STATUS URL_API::BuildCookieList(
									Cookie **cookieArray,
									int *size_returned,
									URL &url)
{
	return urlManager->BuildCookieList(cookieArray,size_returned,url.GetRep());
}


OP_STATUS URL_API::BuildCookieList(
									Cookie **cookieArray,
									int * size_returned,
									uni_char * domain,
									uni_char * path,
									BOOL match_subpaths /*= FALSE*/)
{
	return urlManager->BuildCookieList(cookieArray,size_returned,domain,path, FALSE, match_subpaths);
}

OP_STATUS URL_API::RemoveCookieList(uni_char *domainstr, uni_char *pathstr, uni_char *namestr)
{
	return urlManager->RemoveCookieList(domainstr, pathstr, namestr);
}
#endif // NEED_URL_COOKIE_ARRAY


#if defined(_ENABLE_AUTHENTICATE) && defined(UNUSED_CODE)
void URL_API::AuthenticationDialogFinished()
{
	urlManager->AuthenticationDialogFinished();
}
#endif


#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
void URL_API::ForgetCertificates()
{
	urlManager->ForgetCertificates();
}
#endif


void URL_API::PurgeData(BOOL visited_links, BOOL disk_cache, BOOL sensitive, BOOL session_cookies, BOOL cookies, BOOL certificates, BOOL memory_cache)
{
	if (disk_cache)
	{
		urlManager->EmptyDCache();
	}
	if (visited_links)
	{
		urlManager->DeleteVisitedLinks();
	}
#ifndef RAMCACHE_ONLY
	if (disk_cache || visited_links)
	{
		TRAPD(op_err, urlManager->WriteCacheIndexesL(FALSE, FALSE));
		OpStatus::Ignore(op_err);
	}
#endif // !RAMCACHE_ONLY
	if (session_cookies)
		urlManager->CleanNonPersistenCookies();
	if (sensitive)
	{
		urlManager->RemoveSensitiveData();
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
		if (g_securityManager)
			g_securityManager->RemoveSensitiveData();
#endif
	}
	if (cookies)
	{
		TRAPD(op_err, urlManager->ClearCookiesCommitL());
		OpStatus::Ignore(op_err);
#ifdef USE_SPDY
		TRAP(op_err, g_spdy_settings_manager->ClearAllL());
		OpStatus::Ignore(op_err);
#endif // USE_SPDY
	}

#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_) && !defined(_CERTICOM_SSL_SUPPORT_)
	if (certificates)
		urlManager->ForgetCertificates();
#endif
	if (memory_cache)
	{
		urlManager->CheckRamCacheSize(0);
		urlManager->FreeUnusedResources();
	}
}

#ifdef URL_ENABLE_SAVE_DATA
void URL_API::SaveDataL(BOOL visited_links, BOOL disk_cache,  BOOL cookies)
{
	if (visited_links || disk_cache)
		urlManager->AutoSaveCacheL();
#ifdef DISK_COOKIES_SUPPORT
	if (cookies)
		urlManager->WriteCookiesL();
#endif
}
#endif

void URL_API::CheckTimeOuts()
{
	urlManager->CheckTimeOuts();
}

void URL_API::CleanUp(BOOL ignore_downloads)
{
	urlManager->CleanUp(ignore_downloads);
}

#ifdef NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS
void URL_API::RestartAllConnections()
{
	urlManager->RestartAllConnections();
}

void URL_API::StopAllLoadingURLs()
{
	urlManager->StopAllLoadingURLs();
}

#endif // NEED_URL_PAUSE_STOP_RESTART_CONNECTIONS

void URL_API::CloseAllConnections()
{
	urlManager->CloseAllConnections();
}

#ifdef PI_NETWORK_INTERFACE_MANAGER
BOOL URL_API::IsNetworkAvailable()
{
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode))
		return FALSE;

#ifdef PI_INTERNET_CONNECTION
	// Will check GetNetworkStatus()
	return g_internet_connection.IsConnected();
#else
	if (g_network_connection_status_checker->GetNetworkStatus() == InternetConnectionStatusChecker::NETWORK_STATUS_IS_OFFLINE)
		return FALSE;

	return TRUE;
#endif // PI_INTERNET_CONNECTION
}

#endif // PI_NETWORK_INTERFACE_MANAGER

#ifdef NEED_CACHE_EMPTY_INFO
BOOL URL_API::IsCacheEmpty()
{
	return urlManager->IsCacheEmpty();
}
#endif // NEED_CACHE_EMPTY_INFO

#ifdef _ASK_COOKIE
#ifdef ENABLE_COOKIE_CREATE_DOMAIN
void URL_API::CreateCookieDomain(const char *domain_name)
{
	urlManager->CreateCookieDomain(domain_name);
}

void URL_API::CreateCookieDomain(ServerName *domain_sn)
{
	urlManager->CreateCookieDomain(domain_sn);
}
#endif

#ifdef URL_ENABLE_EXT_COOKIE_ITEM
void URL_API::SetCookie(Cookie_Item_Handler *cookie_item)
{
	urlManager->SetCookie(cookie_item);
}
void URL_API::RemoveSameCookiesFromQueue(Cookie_Item_Handler *set_item)
{
	urlManager->RemoveSameCookiesFromQueue(set_item);
}
#endif
#endif // _ASK_COOKIE

#if defined _ASK_COOKIE || defined COOKIE_USE_DNS_LOOKUP || defined PUBSUFFIX_ENABLED
int	URL_API::HandlingCookieForURL(URL &url)
{
	return urlManager->HandlingCookieForURL(url.GetRep(), url.GetRep()->GetContextId());
}
#endif // _ASK_COOKIE

URL	URL_API::GetNewOperaURL()
{
	return urlManager->GetNewOperaURL();
}

void URL_API::SetPauseStartLoading(BOOL val)
{
	urlManager->SetPauseStartLoading(val);
}

#ifdef ERROR_PAGE_SUPPORT
OP_STATUS URL_API::GenerateErrorPage(URL& url, long errorcode, URL *ref_url, BOOL show_search_field, const OpStringC &user_text)
{
	//don't overwrite a page from the server that is loading ref.bug 148842
	if ((URLStatus) url.GetAttribute(URL::KLoadStatus) == URL_LOADING)
		return OpStatus::OK;

	OpString InternalError;
	OpStatus::Ignore(url.GetAttribute(URL::KInternalErrorString, InternalError));

	url.Unload();	// if the url had been loaded, WriteDocumentData would fail

	Str::LocaleString string_code =
#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
		(Str::LocaleString) errorcode;
#else
		ConvertUrlStatusToLocaleString(errorcode);
#endif

#ifdef URL_FILTER
	if (string_code == Str::SI_ERR_COMM_BLOCKED_URL)
	{
		OpBlockedUrlPage document(url, ref_url);
		return document.GenerateData();
	}
	else
#endif // URL_FILTER
	{
		OpErrorPage *document = NULL;
		RETURN_IF_ERROR(OpErrorPage::Create(&document, url, string_code, ref_url, InternalError, show_search_field, user_text));
		OP_STATUS status = document->GenerateData();
		OP_DELETE(document);
		return status;
	}
}
#endif

void	URL_API::RegisterOperaURL(OperaURL_Generator *generator)
{
	if (generator)
		generator->Into(&g_OperaUrlGenerators);
}

#ifdef URL_ENABLE_REGISTER_SCHEME
URLType URL_API::RegisterUrlScheme(const OpStringC &scheme_name, BOOL have_servername, URLType real_type, BOOL *actual_servername_flag)
{
	if (actual_servername_flag)
		*actual_servername_flag = FALSE;

	if (scheme_name.IsEmpty())
		return URL_NULL_TYPE;

	OpString temp_scheme;

	if (OpStatus::IsError(temp_scheme.Set(scheme_name)))
		return URL_NULL_TYPE;

	temp_scheme.MakeLower();

	if (temp_scheme[0] <'a' || temp_scheme[0] >'z' || // Schemes start with alpha
		temp_scheme.SpanOf(UNI_L("abcdefghijklmnopqrstuvwxyz0123456789.-+")) < temp_scheme.Length()) // And only alpha,digit, dot, dash, plus for the rest
		return URL_NULL_TYPE;

	const protocol_selentry *scheme = urlManager->GetUrlScheme(temp_scheme, FALSE);

	if (scheme)
	{
		if (actual_servername_flag)
			*actual_servername_flag = scheme->have_servername;
		return scheme->used_urltype;
	}

	scheme = urlManager->GetUrlScheme(temp_scheme, TRUE,
#ifdef EXTERNAL_PROTOCOL_SCHEME_SUPPORT
									  FALSE,
#endif
									  have_servername, real_type);

	if (scheme && actual_servername_flag)
		*actual_servername_flag = scheme->have_servername;

	return scheme ? scheme->used_urltype : URL_NULL_TYPE;

}
#endif

#ifdef _DEBUG
Debug& operator<<(Debug& d, enum URLStatus s)
{
	switch (s) {
	case URL_EMPTY:              return d << UNI_L("URL_EMPTY");
	case URL_UNLOADED:           return d << UNI_L("URL_UNLOADED");
	case URL_LOADED:             return d << UNI_L("URL_LOADED");
	case URL_LOADING:            return d << UNI_L("URL_LOADING");
	case URL_LOADING_ABORTED:    return d << UNI_L("URL_LOADING_ABORTED");
	case URL_LOADING_FAILURE:    return d << UNI_L("URL_LOADING_FAILURE");
	case URL_LOADING_WAITING:    return d << UNI_L("URL_LOADING_WAITING");
	case URL_LOADING_FROM_CACHE: return d << UNI_L("URL_LOADING_FROM_CACHE");
	default:
		return d.AddDbg(UNI_L("URLStatus(unknown:%d)"), static_cast<int>(s));
	}
}
#endif
