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

#include "modules/hardcore/mh/messages.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/prefstypes.h"

#include "modules/url/url2.h"
#include "modules/util/datefun.h"
#include "modules/dochand/win.h"
#include "modules/dochand/docman.h"

#include "modules/hardcore/mh/messages.h"

#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/cache/url_cs.h"
#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_pd.h"
#ifdef _MIME_SUPPORT_
#include "modules/mime/mimedec2.h"
#endif // _MIME_SUPPORT_
#include "modules/formats/argsplit.h"
#include "modules/formats/base64_decode.h"
#include "modules/formats/uri_escape.h"
#include "modules/url/url_man.h"
#include "modules/locale/locale-enum.h"
#include "modules/network/src/op_request_impl.h"

#ifdef URL_FILTER
#include "modules/content_filter/content_filter.h"
#endif // URL_FILTER

#ifdef APPLICATION_CACHE_SUPPORT
#include "modules/applicationcache/application_cache_manager.h"
#include "modules/applicationcache/application_cache.h"
#include "modules/applicationcache/manifest.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef SCOPE_RESOURCE_MANAGER
#include "modules/scope/scope_network_listener.h"
#include "modules/doc/frm_doc.h"
#endif // SCOPE_RESOURCE_MANAGER

#ifdef URL_FILTER
#include "modules/logdoc/htm_elm.h"
#endif // URL_FILTER

// Url_lh.cpp

// URL Load Handlers

URL_LoadHandler::URL_LoadHandler(URL_Rep *url_rep, MessageHandler *msgh)
:ProtocolComm(msgh, NULL,NULL)
{
	url = url_rep;
	//mh = msgh;
	stored_progress_level = NO_STATE;
	stored_progress_info1 = 0;
}

URL_LoadHandler::~URL_LoadHandler()
{
	url = NULL;
	//mh = NULL;
}

void URL_LoadHandler::ProcessReceivedData()
{
	NormalCallCount blocker(this);
	url->GetDataStorage()->ReceiveData();
}

void URL_LoadHandler::RefreshProgressInformation()
{
	if(stored_progress_level != NO_STATE)
	{
		ProgressState send_progress = stored_progress_level;/*(stored_progress_level != REQUEST_QUEUED ?
				stored_progress_level : START_REQUEST);*/
		url->GetDataStorage()->GetMessageHandlerList()->SetProgressInformation(
			send_progress ,stored_progress_info1, (const uni_char *) stored_progress_info2.CStr());	
	}
}

void URL_LoadHandler::DisableAutomaticRefetching()
{
}

/*
  OOM: Failed Opstring::Set in SetProgressInformation will only 
  influence display output. Can safely be ignored.
*/
void URL_LoadHandler::SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1,
											const void *progress_info2)
{
	URL_DataStorage *url_ds = url->GetDataStorage(); // This can be NULL even with valid usage! (e.g. REQUEST_FINISHED for a response without data)

	if(url_ds && (url_ds->GetAttribute(URL::KTimeoutMaximum) || url_ds->GetAttribute(URL::KTimeoutPollIdle)))
	{
		switch(progress_level)
		{
		case START_REQUEST:
			if(url_ds->GetAttribute(URL::KRequestSendTimeoutValue)>0)
			{
				url_ds->StartTimeout(FALSE);
			}
			if(url_ds->GetAttribute(URL::KTimeoutStartsAtRequestSend))
			{
				url_ds->StartTimeout();
			}
			break;
		case ACTIVITY_CONNECTION:
			if(url_ds->GetAttribute(URL::KTimeoutStartsAtConnectionStart))
			{
				url_ds->StartTimeout();
			}
			break;
		case LOAD_PROGRESS:
			if(url_ds->GetAttribute(URL::KResponseReceiveIdleTimeoutValue)>0)
			{
				url_ds->StartTimeout(TRUE);
			}
			//fall through
		case START_CONNECT:
		case UPLOADING_PROGRESS:
		case ACTIVITY_REPORT:
			url_ds->SetAttribute(URL::KTimeoutSeenDataSinceLastPoll_INTERNAL, TRUE);
			break;
		}
	}

	switch (progress_level)
	{
	case GET_ORIGINATING_CORE_WINDOW:
		{
			MessageHandler* msg_handler = url_ds && url_ds->mh_list ?url_ds->mh_list->FirstMsgHandler() : NULL;
			Window *window = msg_handler ? msg_handler->GetWindow() : NULL;
			*((Window **) progress_info2) = window;
			return;
		}

	case START_NAME_LOOKUP:
#ifdef SCOPE_RESOURCE_MANAGER
		{
			ServerName *server = (ServerName *) url->GetAttribute(URL::KServerName, NULL);
			SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_DNS_LOOKUP_START, (const void*)(server));
		}
#endif //SCOPE_RESOURCE_MANAGER
	case REQUEST_QUEUED:
	case WAITING_FOR_COOKIES:
	case WAITING_FOR_COOKIES_DNS:

	case START_CONNECT_PROXY:
	case WAITING_FOR_CONNECTION:
	case START_CONNECT:
	case START_REQUEST:
	case STARTED_40SECOND_TIMEOUT:
	case START_NAME_COMPLETION  : // 27/11/98 YNP
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	case START_SECURE_CONNECTION: // 28/11/98 YNP
#endif
	case EXECUTING_ECMASCRIPT:
	case UPLOADING_FINISHED:
		stored_progress_level = progress_level;
		stored_progress_info1 = progress_info1;
		OpStatus::Ignore(stored_progress_info2.Set((const uni_char *)progress_info2)); 
		break;
	case UPLOADING_PROGRESS:
	case LOAD_PROGRESS:
		stored_progress_level = (progress_level == UPLOADING_PROGRESS? UPLOADING_FILES : FETCHING_DOCUMENT);
		stored_progress_info1 = progress_info1;
		OpStatus::Ignore(stored_progress_info2.Set((const uni_char *)progress_info2));	
		break;
	case REQUEST_FINISHED:
		stored_progress_level = NO_STATE;
		stored_progress_info1 = 0;
		OpStatus::Ignore(stored_progress_info2.Set((uni_char*)NULL));
		break;
	case UPLOAD_COUNT:
		{
			OpRequestImpl *request = NULL;
			if (url_ds)
				request = url_ds->GetOpRequestImpl();
			if (request)
				request->RequestDataSent();
			break;
		}
	default:
		break;
	}
	switch (progress_level)
	{
	case SET_INTERNAL_ERROR_MESSAGE:
		if (url_ds)
			url_ds->SetAttribute(URL::KInternalErrorString, (const uni_char *)progress_info2);
		break;
#ifdef _SECURE_INFO_SUPPORT
	case SET_SESSION_INFORMATION:
		{
			URL_HTTP_ProtocolData *hptr = url_ds ? url_ds->GetHTTPProtocolData() : NULL;
			URL *session_data = (URL *) progress_info2;

			if(hptr && session_data)
			{
				hptr->recvinfo.secure_session_info = *session_data;
			}
		}
		break;
#endif
	case SET_SECURITYLEVEL:
        OP_ASSERT((progress_info1 & SECURITY_STATE_MASK) != SECURITY_STATE_UNKNOWN);
        if (url_ds)
        	OpStatus::Ignore(url_ds->SetSecurityStatus((uint32) progress_info1, (const uni_char *) progress_info2));
		progress_info1 &= SECURITY_STATE_MASK;
		// fall-through
	case REQUEST_QUEUED:
	case START_NAME_LOOKUP:
	case START_CONNECT_PROXY:
	case WAITING_FOR_COOKIES:
	case WAITING_FOR_COOKIES_DNS:
	case WAITING_FOR_CONNECTION:
	case START_CONNECT:
	case START_REQUEST:
	case UPLOADING_FINISHED:
	case REQUEST_FINISHED:
	case STARTED_40SECOND_TIMEOUT:
	case GET_ORIGINATING_WINDOW:
	case START_NAME_COMPLETION  : // 27/11/98 YNP
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	case START_SECURE_CONNECTION: // 28/11/98 YNP
#endif
	case UPLOADING_PROGRESS:
	case UPLOAD_COUNT:
#ifdef EMBROWSER_SUPPORT
	case UPLOAD_SIZE:
#endif
	case LOAD_PROGRESS:
	case EXECUTING_ECMASCRIPT:
		// Only process the above messages here;
		{
			URL this_url(url, (char *) NULL);
			if (url_ds)
				url_ds->GetMessageHandlerList()->SetProgressInformation(progress_level,progress_info1, (const uni_char *) progress_info2, &this_url);
		}
		break;
	default:
		ProtocolComm::SetProgressInformation(progress_level,progress_info1, progress_info2);
		;
	}
}


BOOL URL_Rep::Expired(BOOL inline_load, BOOL user_setting_only)
{
	if((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADED && !GetAttribute(URL::KUnique) &&
#ifdef URL_REDUCED_CHECK_FOR_CACHE_DATA_PRESENT
		!storage
#else
	!GetAttribute(URL::KDataPresent)
#endif
	)
	{
		switch ((URLType) GetAttribute(URL::KType))
		{
#ifdef _MIME_SUPPORT_
		case URL_NEWSATTACHMENT:
		case URL_SNEWSATTACHMENT:
		case URL_ATTACHMENT:
			break;
#endif
		default:
			Unload();
			break;
		}
	}

	return !storage || storage->Expired(inline_load, user_setting_only);
}

#ifdef NEED_URL_EXPIRED_WITH_STALE
BOOL URL_Rep::Expired(BOOL inline_load, BOOL user_setting_only, INT32 maxstale, INT32 maxage)
{
	if((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADED && !GetAttribute(URL::KUnique) && !GetAttribute(URL::KDataPresent))
		Unload();

	return (storage ? storage->Expired(inline_load, user_setting_only, maxstale, maxage) : TRUE);
}

BOOL URL_DataStorage::Expired(BOOL inline_load, BOOL user_setting_only, INT32 maxstale, INT32 maxage)
{
#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	if(GetIsOutOfCoverageFile())
		return TRUE;
#endif

	if (maxstale < 0)
	{
		maxstale = 0;
	}
	if (local_time_loaded)
	{
		if((URLStatus) url->GetAttribute(URL::KLoadStatus, TRUE) == URL_LOADING)
			return FALSE;

		time_t current_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

#ifdef _EMBEDDED_BYTEMOBILE
		if (GetPredicted() && current_time < local_time_loaded+60)
		{
			return FALSE;
		}
		else if (GetPredicted())
			SetPredicted(FALSE, predicted_depth);
#endif //_EMBEDDED_BYTEMOBILE

		uint32 response = GetAttribute(URL::KHTTP_Response_Code);

		if ((!user_setting_only && url->GetAttribute(URL::KUnique)) ||
			(!storage && GetAttribute(URL::KMovedToURL).IsEmpty()) || !http_data ||
			(response != HTTP_OK && 
			response != HTTP_NOT_MODIFIED &&
			!IsRedirectResponse(response)))
			return TRUE;

		if (!storage || storage->GetCacheType() == URL_CACHE_USERFILE)
			return TRUE;
		
		time_t expires = 0;
		GetAttribute(URL::KVHTTP_ExpirationDate, &expires);
		if (expires && current_time > expires + (maxstale / 1000))
				return TRUE;

		unsigned long current_age = current_time - local_time_loaded;
		
		current_age += GetAttribute(URL::KHTTP_Age); // We add the age of the proxy on the server, too.

		if (maxage >= 0)
		{
			if (current_age > (unsigned long) maxage / 1000)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}
#endif // NEED_URL_EXPIRED_WITH_STALE

#include "modules/olddebug/tstdump.h"

BOOL URL_DataStorage::Expired(BOOL inline_load, BOOL user_setting_only)
{

#ifdef __OEM_EXTENDED_CACHE_MANAGEMENT
	if(GetIsOutOfCoverageFile())
		return TRUE;
#endif


	URLType url_type = (URLType) url->GetAttribute(URL::KType);
	if ((url_type == URL_HTTP || url_type == URL_HTTPS) && local_time_loaded)
	{
		URLStatus status = (URLStatus) url->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect);
		if(status == URL_LOADING)
			return FALSE;

		time_t current_time = (time_t) (g_op_time_info->GetTimeUTC()/1000.0);

		if((URLStatus) url->GetAttribute(URL::KLoadStatus) != URL_UNLOADED && 
			url->GetAttribute(URL::KSpecialRedirectRestriction) && current_time < local_time_loaded + 24*3600)
			return FALSE; // Dont verify/reload special URLs, presently used for favicons, unless it is more than 24 hours since they were loaded

		URL tmpUrl(url, (char*)0);
		BOOL offline = urlManager->IsOffline(tmpUrl);
		if(!user_setting_only && ((GetAttribute(URL::KCachePolicy_AlwaysVerify) && !offline) || GetAttribute(URL::KCachePolicy_MustRevalidate)))
			return TRUE;

		HTTP_Method method = (HTTP_Method) GetAttribute(URL::KHTTP_Method);
		if (method == HTTP_METHOD_POST || method == HTTP_METHOD_PUT ) // post is only reloaded when the user ask for it !
			return FALSE;

        
        if(status != URL_LOADED)
			return TRUE;

		if (offline)
			return FALSE;
		
		uint32 response = GetAttribute(URL::KHTTP_Response_Code);
		if ((!storage && GetAttribute(URL::KMovedToURL).IsEmpty()) || !http_data ||
			(response != HTTP_OK && response != HTTP_PARTIAL_CONTENT &&
			 response != HTTP_NOT_MODIFIED && !IsRedirectResponse(response)))
			return TRUE;

		// Ignore inline_load argument, check resulting content-type instead.
		inline_load = !!url->GetAttribute(URL::KIsImage, URL::KFollowRedirect);

		// 23/09/97
		//if (prefsManager->AlwaysCheckRedirectChanged() && GetMovedToUrl(FALSE))
		if (!user_setting_only &&
			(	(inline_load && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysCheckRedirectChangedImages)) || 
				(!inline_load && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysCheckRedirectChanged))) && 
				GetAttribute(URL::KMovedToURL).IsValid())
			return TRUE;

		if(!storage || storage->GetCacheType() == URL_CACHE_USERFILE)
			return TRUE;

		
		if (!user_setting_only)
		{
			time_t expires = 0;
			
			GetAttribute(URL::KVHTTP_ExpirationDate, &expires);
			if (expires)
			{
				/*
				PrintfTofile("headers.txt", "%s: checking expiration %lu (%lu : %lu : %lu : %d : lu)\n",url->url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI).CStr(), expires, (time_t) GetAttribute(URL::KHTTP_ExpirationDate), 
					(time_t) g_op_time_info->GetTimeUTC()/1000.0, g_timecache->CurrentTime() , g_op_time_info->GetTimezone() , g_timecache->CurrentTime() + g_op_time_info->GetTimezone() ); 
					*/
				if(current_time > expires)
					return TRUE;
				else
					return FALSE;
			}
		}

		time_t current_age = current_time - local_time_loaded;
		
		if (!user_setting_only)
			current_age += GetAttribute(URL::KHTTP_Age);

		/* Removed
		if(http_data->sendinfo.http_method == HTTP_METHOD_GET && 
			url->GetPath() && op_strchr(url->GetPath(), '?') != NULL)
			return TRUE;
		*/

		PrefsCollectionNetwork::integerpref ck_mod_pref;
		PrefsCollectionNetwork::integerpref expiry_pref;

		switch ((URLContentType) GetAttribute(URL::KContentType))
		{
		case URL_GIF_CONTENT:
		case URL_JPG_CONTENT:
		case URL_BMP_CONTENT:
		case URL_WEBP_CONTENT:
		case URL_XBM_CONTENT:
		case URL_CPR_CONTENT:
		case URL_CSS_CONTENT: // Not really images, but it is used inline
		case URL_X_JAVASCRIPT: // Not really images, but it is used inline
#ifdef _PNG_SUPPORT_
		case URL_PNG_CONTENT:
#endif
#ifdef _WML_SUPPORT_
		case URL_WBMP_CONTENT:
#endif
			ck_mod_pref = PrefsCollectionNetwork::CheckFigModification;
			expiry_pref = PrefsCollectionNetwork::FigsExpiry;
			break;
		case URL_TEXT_CONTENT:
		case URL_HTML_CONTENT:
		case URL_XML_CONTENT:
			ck_mod_pref = PrefsCollectionNetwork::CheckDocModification;
			expiry_pref = PrefsCollectionNetwork::DocExpiry;
			break;
		default:
			ck_mod_pref = PrefsCollectionNetwork::CheckOtherModification;
			expiry_pref = PrefsCollectionNetwork::OtherExpiry;
			break;
		}

		CHMOD ck_mod = (CHMOD) g_pcnet->GetIntegerPref(ck_mod_pref);
		if (ck_mod == TIME)
		{
			// HTTP 301 ==> Moved permanently: no point in checking the expiration date, as it is permanent
			// Also, MyOpera does not set KHTTP_LastModified, which seems ok, and if we return TRUE the result is that we don't properly cache the images...
			if(response == HTTP_MOVED)
				return FALSE;

			/* Bug CORE-21377: Expire immediately if both "Expires" and "Last-Modified" header is absent */
			if (GetAttribute(URL::KHTTP_LastModified).IsEmpty())
				return TRUE;

			return (current_age > (time_t) g_pcnet->GetIntegerPref(expiry_pref));
		}
		else
			return (ck_mod == ALWAYS);
	}
#if !defined(NO_URL_OPERA) || defined(HAS_OPERABLANK)
	else if(url_type == URL_OPERA)
	{
		OpStringC8 path = url->GetAttribute(URL::KPath);

		if(path.Compare("cache") == 0 || path.Compare("history") == 0 
		   || path.Compare("plugins") == 0 || path.Compare("about") == 0 
		   || path.Compare("feed-id", 7) == 0 || path.Compare("feeds") == 0)
			return TRUE;
	}
#endif // !NO_URL_OPERA

	if(!storage || url_type == URL_DATA)
		return TRUE;

	if (storage->GetCacheType() == URL_CACHE_USERFILE)
	{
		OpFileFolder folder;
		const OpStringC fileName = storage->FileName(folder, TRUE);
		OpFile file;
		if (OpStatus::IsError(file.Construct(fileName)))
			return TRUE;
		
		OpFileInfo info;
		info.flags = OpFileInfo::LAST_MODIFIED;
		OP_STATUS res = file.GetFileInfo(&info);
		if (OpStatus::IsError(res) || info.last_modified>local_time_loaded)
			return TRUE;
	}

	return FALSE;
}

#ifdef STRICT_CACHE_LIMIT
CommState URL_Rep::Reload(MessageHandler* mh)
{
    URL u(GetAttribute(URL::KReferrerURL));
	// FIXME: Is there any way to improve the (arbitrarily determined) values here?
    return Reload(mh, u, FALSE, FALSE, FALSE, FALSE, NotEnteredByUser);
};
#endif

#ifdef SCOPE_RESOURCE_MANAGER
void URL_Rep::GetLoadOwner(MessageHandler *mh, DocumentManager *&docman, Window *&window)
{
	if (!mh)
	{
		URL_DataStorage *url_ds = GetDataStorage();
		mh = url_ds && url_ds->GetMessageHandlerList() ? url_ds->GetMessageHandlerList()->FirstMsgHandler() : NULL;
	}
	docman = mh ? mh->GetDocumentManager() : NULL;
	window = mh ? mh->GetWindow() : NULL;
	if (docman && !window)
		window = docman->GetWindow();

	if (!window && !docman)
	{
		OpString url_string;
		url_string.Set(GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped));

		ApplicationCacheGroup* appcachegroup = g_application_cache_manager->GetApplicationCacheGroupFromManifest(url_string.CStr());

		if (appcachegroup)
		{
			ApplicationCache* cache = appcachegroup->GetMostRecentCache(FALSE);
			if (cache && cache->GetCacheHosts().GetCount())
				window = cache->GetCacheHosts().Get(0)->GetFramesDocument()->GetWindow();
			else if (appcachegroup->GetCurrentUpdateHost())
			{
				window = appcachegroup->GetCurrentUpdateHost()->GetFramesDocument()->GetWindow();
			}
		}
	}

}

BOOL RequireEventForURLType(URLType type)
{
	// Only allow the types which we have installed callbacks for
	return type == URL_FILE || type == URL_DATA || type == URL_HTTP || type == URL_HTTPS;
}
#endif // SCOPE_RESOURCE_MANAGER

#ifdef SCOPE_RESOURCE_MANAGER
BOOL URL_Rep::IsEventRequired() const
{
	return RequireEventForURLType((URLType)GetAttribute(URL::KType));
}
#endif // SCOPE_RESOURCE_MANAGER

void URL_Rep::OnLoadRedirect(URL_Rep *redirect, MessageHandler* mh)
{
	(void)redirect;
	(void)mh;
#ifdef SCOPE_RESOURCE_MANAGER
	if (IsEventRequired() && OpScopeResourceListener::IsEnabled())
		OpStatus::Ignore(OpScopeResourceListener::OnUrlRedirect(this, redirect));
#endif // SCOPE_RESOURCE_MANAGER
}

void URL_Rep::OnLoadStarted(MessageHandler *mh)
{
	(void)mh; // In case no ifdefs are used
#ifdef SCOPE_RESOURCE_MANAGER
	if (IsEventRequired() && OpScopeResourceListener::IsEnabled())
	{
		DocumentManager *docman;
		Window *window;
		GetLoadOwner(mh, docman, window);
		OpStatus::Ignore(OpScopeResourceListener::OnUrlLoad(this, docman, window));
	}
#endif // SCOPE_RESOURCE_MANAGER
}

void URL_Rep::OnLoadFinished(URLLoadResult result, MessageHandler* mh)
{
	(void)result; // In case no ifdefs are used
#ifdef SCOPE_RESOURCE_MANAGER
	if (IsEventRequired() && OpScopeResourceListener::IsEnabled())
		OpStatus::Ignore(OpScopeResourceListener::OnUrlFinished(this, result));
#endif // SCOPE_RESOURCE_MANAGER
}

CommState URL_Rep::Reload(
		MessageHandler* msg_handler, const URL& referer_url,
		BOOL only_if_modified, BOOL proxy_no_cache,
		BOOL user_initiated, BOOL thirdparty_determined, EnteredByUser entered_by_user, BOOL inline_loading, BOOL* allow_if_modified)
{
	if (!msg_handler || !CheckStorage())
		return COMM_REQUEST_FAILED;

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	CheckBypassFilter();
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
	if(!GetAttribute(URL::KReloadSameTarget))
		info.content_type = FROM_RANGED_ENUM(URLContentType, URL_UNDETERMINED_CONTENT);
	return storage->Reload(msg_handler, referer_url, only_if_modified, proxy_no_cache,
						   user_initiated, thirdparty_determined, entered_by_user, inline_loading, allow_if_modified);
}

CommState URL_Rep::ResumeLoad(
		MessageHandler* msg_handler, const URL& referer_url)
{
	if (!msg_handler)
		return COMM_REQUEST_FAILED;

	if(storage == NULL)
		return Load(msg_handler, referer_url, TRUE, FALSE, TRUE);

	return storage->ResumeLoad(msg_handler,referer_url);
}

CommState URL_Rep::Load(MessageHandler* mh, const URL& referer_url, BOOL user_initiated, BOOL thirdparty_determined, BOOL inline_item)
{
	/* Javascript URLs should be loaded through FramesDocument::ESOpenJavascriptURL(). */
	OP_ASSERT((URLType) name.GetAttribute(URL::KType) != URL_JAVASCRIPT);

	if (!mh)
		return COMM_REQUEST_FAILED;

	if(!CheckStorage())
		return COMM_REQUEST_FAILED;

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	CheckBypassFilter();
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

	SetAttribute(URL::KIsUserInitiated, user_initiated);

	return storage->Load(mh,referer_url,  user_initiated, thirdparty_determined, inline_item);
}

BOOL URL_Rep::QuickLoad(BOOL guess_content_type)
{
	if((URLType) GetAttribute(URL::KType) != URL_FILE
#ifdef SUPPORT_DATA_URL
		&& (URLType) GetAttribute(URL::KType) != URL_DATA
#endif
		)
		return FALSE;

	if(!CheckStorage())
		return FALSE;

	return storage->QuickLoad(guess_content_type);
}


CommState	URL::LoadDocument(MessageHandler* mh, const URL& referer_url, const URL_LoadPolicy &loadpolicy, HTMLLoadContext *ctx)
{
	if(mh == NULL)
		return COMM_REQUEST_FAILED;

	OP_ASSERT(rep);

#ifdef CACHE_URL_RANGE_INTEGRATION
	CommState custom_state;

	// Used to articulate the download in a different way, in particular the Multimedia cache can break the download in more pieces based on the
	// content available in the cache
	if( rep &&
		rep->GetDataStorage() &&
		rep->GetDataStorage()->GetCacheStorage() &&
		rep->GetDataStorage()->GetCacheStorage()->CustomizeLoadDocument(mh, referer_url, loadpolicy, custom_state))
		return custom_state;
#endif// CACHE_URL_RANGE_INTEGRATION

	// If there is a Document Manager and it has a Window then mh->GetWindow **MUST** match
//	OP_ASSERT(mh->GetDocumentManager() == NULL || mh->GetDocumentManager()->GetWindow() == mh->GetWindow()) ;

	OP_MEMORY_VAR URL_Reload_Policy policy = loadpolicy.GetReloadPolicy();
	URLStatus status = (URLStatus) GetAttribute(URL::KLoadStatus);
	URLType type = (URLType) GetAttribute(URL::KType);
	
	SetAttribute(URL::KBypassProxy, loadpolicy.IsProxyBypass());

#ifdef APPLICATION_CACHE_SUPPORT
	ApplicationCacheManager::ApplicationCacheNetworkStatus appcache_status = g_application_cache_manager->CheckApplicationCacheNetworkModel(*this);
	
	switch (appcache_status) {
		case ApplicationCacheManager::LOAD_FROM_APPLICATION_CACHE:
			OP_ASSERT(status == URL_LOADED);
			OP_ASSERT(GetAttribute(URL::KDataPresent));
			policy = URL_Reload_None;			
			break;
			
		case ApplicationCacheManager::RELOAD_FROM_NETWORK:
			policy = URL_Reload_Full;
			break;
			
		default:
			break;
	}

#endif // APPLICATION_CACHE_SUPPORT

#ifdef URL_FILTER
	if (!GetAttribute(URL::KSkipContentBlocker))
	{
		BOOL allowed = FALSE;
		OpString url_str;
		OP_STATUS status = rep->GetAttribute(URL::KUniName_Escaped, 0, url_str);
		if (OpStatus::IsSuccess(status))
		{
			if (ctx)
				status = g_urlfilter->CheckURL(url_str.CStr(), allowed, mh->GetWindow(), ctx);
			else
			{
				HTMLLoadContext load_ctx(RESOURCE_UNKNOWN, mh->GetWindow() ? mh->GetWindow()->GetCurrentShownURL().GetServerName() : NULL, NULL, mh->GetWindow() && mh->GetWindow()->GetType() == WIN_TYPE_GADGET);
				status = g_urlfilter->CheckURL(url_str.CStr(), allowed, mh->GetWindow(), &load_ctx);
			}
		}

		if (OpStatus::IsSuccess(status) && !allowed)
		{
			SetAttribute(URL::KLoadStatus, URL_LOADING_FAILURE);

			mh->PostMessage(MSG_URL_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_BLOCKED_URL));

			return COMM_REQUEST_FAILED;
		}
		else if (OpStatus::IsMemoryError(status))
			return COMM_REQUEST_FAILED;
	}
#endif // URL_FILTER
	
#if defined(MHTML_ARCHIVE_REDIRECT_SUPPORT)
	if (urlManager->GetContextIsOffline(GetContextId()))
	{
		if (loadpolicy.IsUserInitiated())
		{
			// Any user-initiated loading should proceed as normal, and the
			// context should become "online", enabling further loads
			urlManager->SetContextIsOffline(GetContextId(), FALSE);
			URL emptyURL;
			TRAPD(err, SetAttributeL(g_mime_module.GetOriginalURLAttribute(), emptyURL)); // Since we will reload the url from the internet, reset the original url
			OpStatus::Ignore(err);
		}
		else if (status != URL_LOADED && type != URL_DATA)
		{
			// No new content should be loaded if the context is "offline"
			SetAttribute(URL::KLoadStatus, URL_LOADING_FAILURE);
			mh->PostMessage(MSG_URL_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_HTTP_NOT_FOUND));
			return COMM_REQUEST_FAILED;
		}
	}
#endif // MHTML_ARCHIVE_REDIRECT_SUPPORT

	if(status != URL_LOADED ||
		(type == URL_FILE && GetAttribute(URL::KBaseAliasURL).IsValid()) || // Check if it can be a MHTML archive
		(policy == URL_Reload_None && !GetAttribute(URL::KDataPresent)) )
		policy = URL_Load_Normal;
	else if(policy == URL_Reload_None && GetAttribute(URL::KCachePolicy_AlwaysVerify) && !urlManager->IsOffline(*this))
		policy = URL_Reload_Conditional;
	else if(referer_url.GetAttribute(URL::KType) == URL_OPERA && 
		loadpolicy.IsUserInitiated() &&
		referer_url.GetAttribute(URL::KPath).Compare("opera:cache") == 0)
	{ 
		policy = URL_Reload_None; // Don't reload document when loading them from opera:cache view and it is user initiated
	}
#if defined(GADGET_SUPPORT)
	else if(type == URL_WIDGET)
	{
		policy = URL_Reload_If_Expired;
	}
#endif // GADGET_SUPPORT
	else if(policy == URL_Reload_If_Expired)
	{
		do{
			CheckExpiryType expirytype;
			
			if(loadpolicy.IsHistoryWalk())
			{
				if(type == URL_HTTPS &&
					(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AlwaysReloadHTTPSInHistory) ||
					GetAttribute(URL::KCachePolicy_MustRevalidate)))
				{
					policy = URL_Reload_Conditional;
					break;
				};


				expirytype = (CheckExpiryType) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckExpiryHistory);
				if(expirytype == CHECK_EXPIRY_NEVER)
				{
					policy = URL_Reload_None;
					break;
				}
			}
			else
			{
				expirytype = (CheckExpiryType) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::CheckExpiryLoad);
			}
			
			if((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADED)
			{
				if(GetAttribute(URL::KCachePolicy_MustRevalidate) && Status(TRUE) != URL_LOADING || Expired(FALSE, (expirytype== CHECK_EXPIRY_USER)))
					policy = URL_Reload_Conditional;
				else
					policy = URL_Reload_None;
				break;
			}
			// Use normal policy
		}
		while(0);
	}

	/**
	 * This request will not be validated by the underlying network code,
	 * so we need to do an explicit cross network check here. But don't do
	 * it if it's initiated by the user. For instance, prevents links from
	 * BTS to t from triggering the cross network warning.
	 */
	if(policy == URL_Reload_None && !loadpolicy.IsUserInitiated())
	{
		OpSocketAddressNetType current_nettype = (OpSocketAddressNetType)rep->GetAttribute(URL::KLoadedFromNetType);
		OpSocketAddressNetType ref_nettype = (OpSocketAddressNetType)referer_url.GetAttribute(URL::KLoadedFromNetType);

		if (current_nettype == NETTYPE_UNDETERMINED)
		{
			ServerName *this_servername = (ServerName *) rep->GetAttribute(URL::KServerName, (const void*) 0);
			current_nettype = this_servername ? this_servername->GetNetType() : NETTYPE_UNDETERMINED;
		}
		if (ref_nettype == NETTYPE_UNDETERMINED)
		{
			ServerName *ref_servername = (ServerName *) referer_url.GetAttribute(URL::KServerName,(const void*) 0);
			ref_nettype = ref_servername ? ref_servername->GetNetType() : NETTYPE_UNDETERMINED;
		}

		if (current_nettype != NETTYPE_UNDETERMINED && 
			ref_nettype != NETTYPE_UNDETERMINED &&
			ref_nettype > current_nettype && 
			!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::AllowCrossNetworkNavigation, *this))
		{
			mh->PostMessage(MSG_URL_LOADING_FAILED, Id(), (int) GetCrossNetworkError(ref_nettype, current_nettype));
			return COMM_LOADING;
		}
	}

	g_url_api->HasStartedLoading();

#ifdef SCOPE_RESOURCE_MANAGER
	if (OpScopeResourceListener::IsEnabled())
		if (OpScopeResourceListener::ForceFullReload(GetRep(), mh->GetDocumentManager(), mh->GetWindow()))
			policy = URL_Reload_Full;
#endif // SCOPE_RESOURCE_MANAGER

	switch(policy)
	{
	case URL_Load_Normal:
		if(type == URL_HTTP || type == URL_HTTPS
#if defined(GADGET_SUPPORT)
			 || type == URL_WIDGET
#endif
			 )
			SetAttribute(URL::KCachePolicy_AlwaysCheckNeverExpiredQueryURLs, !loadpolicy.IsInlineElement());
		return Load(mh, referer_url, loadpolicy.IsInlineElement(), FALSE, loadpolicy.IsUserInitiated(), loadpolicy.HasDeterminedThirdparty());
	case URL_Reload_Conditional:
	case URL_Reload_Exclusive:
	case URL_Reload_Full:
		if(type == URL_HTTP || type == URL_HTTPS)
			SetAttribute(URL::KCachePolicy_AlwaysCheckNeverExpiredQueryURLs, !loadpolicy.IsInlineElement());
		return Reload(mh, referer_url, (policy == URL_Reload_Conditional), (policy == URL_Reload_Full || policy == URL_Reload_Exclusive), loadpolicy.IsInlineElement(), FALSE, FALSE, loadpolicy.IsUserInitiated(), loadpolicy.HasDeterminedThirdparty(), loadpolicy.WasEnteredByUser());
	default:
		{
			URL redirect = GetAttribute(KMovedToURL);

			if(mh->GetWindow())
				mh->GetWindow()->SetSecurityState((BYTE) GetAttribute(URL::KSecurityStatus), FALSE, GetAttribute(URL::KSecurityText).CStr());

			if(redirect.IsValid() && redirect.Id() != Id())
			{
				URL_LoadPolicy loadpolicy2(loadpolicy);
				loadpolicy2.SetRedirectedHistoryWalk(TRUE);

				mh->PostMessage(MSG_URL_MOVED, Id(), redirect.Id());
				rep->OnLoadStarted(mh);
				rep->OnLoadRedirect(redirect.GetRep(), mh);
				return redirect.LoadDocument(mh, referer_url, loadpolicy2);
			}

			rep->OnLoadStarted(mh);
			if(loadpolicy.IsHistoryWalk() && !loadpolicy.IsRedirectedHistoryWalk() && !GetAttribute(URL::KIsGeneratedByOpera))
			{
				rep->OnLoadFinished(URL_LOAD_FINISHED);
				return COMM_REQUEST_FINISHED;
			}

			SetAttribute(URL::KHeaderLoaded, TRUE);
			mh->PostMessage(MSG_HEADER_LOADED, Id(), GetAttribute(URL::KIsFollowed) ? 0 : 1);
			mh->PostMessage(MSG_URL_DATA_LOADED, Id(), FALSE);

			if (status == URL_LOADED)
				rep->OnLoadFinished(URL_LOAD_FINISHED, mh);
		}
	}

	return COMM_LOADING;
}


void URL_Rep::StopLoading(MessageHandler* mh)
{
	if (storage) 
		storage->StopLoading(mh);
}

#ifdef DYNAMIC_PROXY_UPDATE
void URL_Rep::StopAndRestartLoading()
{
	if (storage) 
		storage->StopAndRestartLoading();
}
#endif // DYNAMIC_PROXY_UPDATE

unsigned long URL_Rep::SaveAsFile(const OpStringC &file_name)	// temporary routine, will be deleted when FileClass is fully implemented
{
	return (storage ? storage->SaveAsFile(file_name) : 0);
}

OP_STATUS URL_Rep::ExportAsFile(const OpStringC &file_name)
{
	return (storage && storage->GetCacheStorage()) ? storage->GetCacheStorage()->ExportAsFile(file_name) : OpStatus::ERR_NULL_POINTER;
}

BOOL URL_Rep::IsExportAllowed()
{
	return storage && storage->GetCacheStorage() && storage->GetCacheStorage()->IsExportAllowed();
}

#ifdef PI_ASYNC_FILE_OP
	OP_STATUS URL_Rep::ExportAsFileAsync(const OpStringC &file_name, AsyncExporterListener *listener, UINT32 param, BOOL delete_if_error, BOOL safe_fall_back)
	{
		return (storage && storage->GetCacheStorage()) ?
				storage->GetCacheStorage()->ExportAsFileAsync(file_name, listener, param, delete_if_error, safe_fall_back) :
				OpStatus::ERR_NULL_POINTER;
	}
#endif // PI_ASYNC_FILE_OP

OP_STATUS URL_Rep::LoadToFile(const OpStringC &file_name)
{
	return (CheckStorage() ? storage->LoadToFile(file_name) : OpStatus::ERR);
}

unsigned long  URL_DataStorage::SaveAsFile(const OpStringC &file_name)
{
	if(!storage )
		return ((URLStatus) GetAttribute(URL::KLoadStatus) == URL_LOADING ? (LoadToFile(file_name) == OpStatus::OK ? 0 : URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR)) : 0);

	unsigned long err = PrepareForViewing(TRUE, TRUE);
	if(err)
		return err;
	return storage->SaveToFile(file_name);
}

OP_STATUS URL_DataStorage::LoadToFile(const OpStringC &file_name)
{
	Cache_Storage *new_cs = NULL;
	BOOL finished = FALSE;

#ifdef URL_NOTIFY_FILE_DOWNLOAD_SOCKET
	SetAttribute(URL::KUsedForFileDownload, TRUE);
#endif

	if(storage)
	{
		if((URLType) url->GetAttribute(URL::KType) != URL_FILE && (URLStatus) url->GetAttribute(URL::KLoadStatus) == URL_LOADING && storage->GetCacheType() == URL_CACHE_USERFILE)
			return OpStatus::OK; // Already loading;

		OpString src_name;
		RETURN_IF_ERROR(storage->FilePathName(src_name));
		if(file_name.Compare(src_name) == 0)
			return OpStatus::OK; // Same name;
	}

	if(!storage)
	{
		new_cs = Download_Storage::Create(url, file_name, FALSE);
		if (!new_cs)
			return OpStatus::ERR_NO_MEMORY;
		//if(!loading)
 		SetAttribute(URL::KReloadSameTarget,TRUE);

	}
	else
	{
		finished = storage->GetFinished();
		new_cs = Download_Storage::Create(storage, file_name);
		if (!new_cs)
			return OpStatus::ERR_NO_MEMORY;
	}

	SetAttribute(URL::KCachePolicy_NoStore, FALSE);
	if(!url->GetAttribute(URL::KUnique))
	{
		urlManager->MakeUnique(url); // Makes sure the URL cannot be used again
		if(loading)
			loading->DisableAutomaticRefetching(); // Make sure HTTP downloads are not automatically restarted
		if(url->IsFollowed())
		{
			// New, empty URL for bookkeeping
			OpString8 temp_name;
			url->GetAttribute(URL::KName_Username_Password_Escaped_NOT_FOR_UI,temp_name);
			URL temp_url = urlManager->GetURL(temp_name); // Password is needed
			temp_url.Access(TRUE);
		}
	}

	storage = new_cs;
	
	if(finished)
		storage->SetFinished();

	return OpStatus::OK;
}

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
void URL_Rep::CheckBypassFilter()
{
#ifdef URL_FILTER
	BOOL bypass = FALSE;
	if (g_urlfilter)
		OpStatus::Ignore(g_urlfilter->CheckBypassURL((uni_char *)GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI).CStr(), bypass));

#ifdef WEB_TURBO_MODE
	if( bypass )
	{
		ServerName* sname = (ServerName*)GetAttribute(URL::KServerName, (void*)NULL, URL::KNoRedirect);
		int setting = sname ? g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo, sname->GetUniName().CStr()) : 0;
		if( setting > 1 && (time_t)setting < g_timecache->CurrentTime() ) // setting > 1 means it represents expiry time for override
		{
			// TODO: remove override setting
			int def = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo);
			OpString wildcardUrl;
			TRAPD(err,
				g_pcnet->OverridePrefL(sname->GetUniName().CStr(), PrefsCollectionNetwork::UseWebTurbo, def, FALSE);
				URLFilter::GetWildcardURLL(sname->GetUniName(), wildcardUrl));
			if( OpStatus::IsSuccess(err) )
					g_urlfilter->RemoveFilter(wildcardUrl.CStr());
		
			bypass = FALSE; // Correct even if filter isn't removed
		}
	}
#endif // WEB_TURBO_MODE

	if( bypass )
		storage->SetLoadDirect(TRUE);
#endif // URL_FILTER
}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

#ifdef SUPPORT_DATA_URL
void URL_DataStorage::StartLoadingDataURL(const URL& referer_url)
{
	SetAttribute(URL::KLoadStatus,URL_LOADING);
	SetAttribute(URL::KCachePolicy_AlwaysVerify,TRUE);
	
	SecurityLevel sec_state = (referer_url.IsValid() ? (SecurityLevel) referer_url.GetAttribute(URL::KSecurityStatus) : SECURITY_STATE_NONE);
	OpStringC sec_level(referer_url.IsValid() ? referer_url.GetAttribute(URL::KSecurityText) : (OpStringC) NULL);

	SetAttribute(URL::KSecurityStatus, sec_state);
	SetAttribute(URL::KSecurityText, sec_level);
	mh_list->SetProgressInformation(SET_SECURITYLEVEL, sec_state, sec_level.CStr());
	do{
		OpString8 url_data;
		OP_MEMORY_VAR BOOL base64 = FALSE;
		
		TRAPD(op_err, url->GetAttributeL(URL::KPathAndQuery_L,url_data));
		if(OpStatus::IsError(op_err))
			break;

		int headerlen = url_data.FindFirstOf(',');
		if(headerlen == KNotFound)
		{
            // Strictly speaking this is illegal content, but
			// treated as empty by setting it to ","
			headerlen = 0;
			url_data.Set(",");
		}
		OpString8 header;
		BOOL set_mime = FALSE;

		if(OpStatus::IsSuccess(header.Set(url_data.CStr(), headerlen)) )
		{
			int headerlen2 = UriUnescape::ReplaceChars(header.DataPtr(), UriUnescape::NonCtrlAndEsc);

			// the header start with a ";" then prepend text/plain as the type
			int leading_blanks=header.SpanOf(" \t\r\n");
			BOOL inserted_textplain = FALSE;
			if(leading_blanks >= 0 && leading_blanks <headerlen2 && header[leading_blanks] == ';')
			{
				header.Delete(0, leading_blanks);
				header.Insert(0, "text/plain");
				inserted_textplain = TRUE;
			}
			
			ParameterList type(KeywordIndex_HTTP_General_Parameters);
			if(OpStatus::IsError(type.SetValue(header.CStr(),PARAM_SEP_SEMICOLON|PARAM_STRIP_ARG_QUOTES)) )
				break;
			
			do{
				Parameters *element = type.First();
				if(!element)
					break;
				element->SetNameID(HTTP_General_Tag_Unknown);
				
				SetAttribute(URL::KMIME_ForceContentType, (element->Name() && *element->Name() ? element->Name() : "text/plain"));
				
				set_mime = TRUE;
				if(content_type_string.HasContent() && strni_eq(content_type_string.CStr(), "TEXT/", 5))
				{
					element = type.GetParameterByID(HTTP_General_Tag_Charset, PARAMETER_ASSIGNED_ONLY, element);
					
					const char* mime_charset = NULL;
					if (element)
						mime_charset = element->Value();
					else if (inserted_textplain)
						mime_charset = "US-ASCII";

					if (mime_charset)
						SetAttribute(URL::KMIME_CharSet, mime_charset);
				}
				
				element = type.GetParameterByID(HTTP_General_Tag_Base64,PARAMETER_NO_ASSIGNED);
				if(element)
					base64 = TRUE;
			}
			while(0);
		}
		if(!set_mime)
			SetAttribute(URL::KMIME_ForceContentType, "text/plain; charset=US-ASCII");


		SetAttribute(URL::KHeaderLoaded, TRUE);
		BroadcastMessage(MSG_HEADER_LOADED, url->GetID(), url->GetIsFollowed() ? 0 : 1, MH_LIST_ALL);

		if(url_data.HasContent())
		{
			unsigned char *src = (unsigned char *) url_data.DataPtr() + headerlen+1;
			unsigned char *content = NULL;
			BOOL alloc = FALSE;

			int buflen = op_strlen((char *) src);
			UriUnescape::ReplaceChars((char *) src, buflen, UriUnescape::All); // Will also unescape %00
			unsigned long len = (OpFileLength)buflen;

			if(!base64)
			{
				content = (unsigned char *) src;
			}
			else
			{
				unsigned long calc_len = ((len+3)/4)*3;

				content = OP_NEWA(unsigned char, calc_len);
				if(content == NULL)
					break;

				alloc = TRUE;

				unsigned long pos = 0;
				BOOL warn = FALSE;

				len = GeneralDecodeBase64(src, len, pos, content, warn);

				OP_ASSERT(len <= calc_len);
			}

			OpFileLength content_size = (OpFileLength)len;
			SetAttribute(URL::KContentSize, &content_size);

			WriteDocumentData(content, len);
			WriteDocumentDataFinished();

			if(alloc)
				OP_DELETEA(content);

			content = NULL;
		}

		SetAttribute(URL::KLoadStatus,URL_LOADED);
		
		BroadcastMessage(MSG_URL_DATA_LOADED, url->GetID(), 0, MH_LIST_ALL);
		UnsetListCallbacks();
		mh_list->Clean();
		url->Access(FALSE);
		OnLoadFinished(URL_LOAD_FINISHED);
		return;
	}while(0);

	// Failed to load properly
	SetAttribute(URL::KLoadStatus,URL_LOADING_FAILURE);
	BroadcastMessage(MSG_URL_LOADING_FAILED, url->GetID(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR), MH_LIST_ALL);
	OnLoadFinished(URL_LOAD_FAILED);
}
#endif
