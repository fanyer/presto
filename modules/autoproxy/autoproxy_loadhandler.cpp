/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2001-2011
 */

/** @mainpage Automatic proxy configuration implementation
 *
 * The URL_DataStorage code creates a URL_AutoProxyConfig_LoadHandler whose
 * "url" member is the URL for which we are trying to find a proxy.
 *
 * URL_AutoProxyConfig_LoadHandler::Load() is called next, from the URL code.
 * If this is the first time through, and the autoproxy script has not been
 * loaded for a different URL already, Load() determines the script URL
 * and starts loading it, before returning COMM_LOADING; otherwise it goes
 * straight into the worker function ExecuteScript().  If the URL must be loaded,
 * a later callback on URL_AutoProxyConfig_LoadHandler::HandleCallback() will
 * complete the operation, also by calling ExecuteScript().
 *
 * ExecuteScript creates an instance of JSProxyConfig to handle that particular
 * proxy config script.  This instance is cached (because it is expensive to
 * create it).  It then calls FindProxyForURL() on that instance, passing it
 * the load handler's "url" member.
 *
 * FindProxyforURL() may suspend waiting for network data or because the
 * script used up its timeslice, in which ExecuteScript() sets up callbacks
 * to resume the operation later.
 *
 * When FindProxyForURL() finally returns with a result, the result is posted
 * back to the URL code and we're done.
 *
 * The JSProxyConfig constructor (!) reads the script data from the script URL,
 * compiles it, and installs it in an ECMAScript runtime structure.  This
 * runtime structure is shared by all lookups that use that script.  (Whether
 * this is correct is an open question.)
 *
 * When FindProxyForURL() is called, a small program is constructed that calls
 * the user's proxy config function; this program is then compiled and run
 * in a standard manner.  If it suspends waiting for the network (dns lookup)
 * or because the timeslice expired, then the suspension is passed out to the
 * caller (see above).
 *
 * The user's proxy-script has a small library of functions available.  Some
 * of these perform dns lookups.  When a dns lookup function is called and the
 * result is not immediately available, the lookup function returns a suspension
 * code which will cause suspension structures to be allocated and returned
 * up through the call tree to FindProxyForURL(), which must handle it and
 * allow itself to be restarted later.
 *
 * Buggy scripts are handled by cutting them off after a fairly large number
 * of computation steps (currently 10 million steps).  A script that fails with
 * an error will cause the URL not to be loaded at all.
 */

/** @file autoproxy_loadhandler.cpp
 *
 * Main entry points to the autoproxy implementation
 *
 * @author Yngve Pettersen / Lars Thomas Hansen
 */

#include "core/pch.h"
#include "modules/probetools/probepoints.h"

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/autoproxy/autoproxy.h"
#include "modules/url/url_rep.h"
#include "modules/url/url_ds.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/loadhandler/url_lh.h"
#include "modules/url/url_pd.h"
#include "modules/url/url_tools.h"
#include "modules/url/url_man.h"
#ifdef WEBSOCKETS_SUPPORT
#include "modules/url/protocols/websocket.h"
#endif //WEBSOCKETS_SUPPORT

#if defined OPERA_CONSOLE
# include "modules/console/opconsoleengine.h"
#endif // OPERA_CONSOLE
#include "modules/locale/oplanguagemanager.h"
#include "modules/locale/locale-enum.h"

#define RETRY_DELAY 100

#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
#define AP_ERRSTR(p,x) Str::##p##_##x
#else
#define AP_ERRSTR(p,x) x
#endif

/* Public API */

URL_LoadHandler* CreateAutoProxyLoadHandler(URL_Rep *url_rep, MessageHandler *mh)
{
	if (g_proxycfg == NULL)
	{
		g_proxycfg = OP_NEW(AutoProxyGlobals, ());
	}
	if (g_proxycfg == NULL)
		return NULL;
	return OP_NEW(URL_AutoProxyConfig_LoadHandler, (url_rep, mh));
}

BOOL ReleaseAutoProxyResources()
{
	if (g_proxycfg == NULL || g_proxycfg->pending_requests > 0)
		return FALSE;

#ifdef SELFTEST
	DebugAPC::LogNumberOfLoads();
#endif

	g_proxycfg->configuration_script_url = URL();
	JSProxyConfig::Destroy(g_proxycfg->proxycfg);
	g_proxycfg->proxycfg = NULL;
	g_proxycfg->settings_have_changed = FALSE;

	return TRUE;
}

/* Private */

URL_AutoProxyConfig_LoadHandler::URL_AutoProxyConfig_LoadHandler(URL_Rep *url_rep, MessageHandler *mh)
	: URL_LoadHandler(url_rep, mh)
	, pending_request(0)
{
}

#define BeCallBack(sig, id) mh->SetCallBack(this, sig, id)

CommState URL_AutoProxyConfig_LoadHandler::Load()
{
	URL resolved_url;
	OP_STATUS resolve_err;
	OP_NEW_DBG("Load", "apc");

	// Look up the current setting
	resolve_err = GetScriptURL(resolved_url);
	if(OpStatus::IsError(resolve_err))
	{
		if (OpStatus::IsMemoryError(resolve_err))
		{
			g_memory_manager->RaiseCondition(resolve_err);
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), AP_ERRSTR(SI,ERR_COMM_INTERNAL_ERROR));
		}
		else
			DisableProxyAndFail(AP_ERRSTR(SI,ERR_COMM_INTERNAL_ERROR));
		return COMM_LOADING;
	}

	// If changed since last time, set up variables to signal the change
	if (g_proxycfg->configuration_script_url.Id(TRUE) != resolved_url.Id(TRUE))
	{
		if (resolved_url.IsEmpty())
		{
			JSProxyConfig::Destroy(g_proxycfg->proxycfg);
			g_proxycfg->proxycfg = NULL;
		}

		g_proxycfg->configuration_script_url = resolved_url;
		g_proxycfg->settings_have_changed = TRUE;
	}

	// If empty URL, then ignore it
	if(g_proxycfg->configuration_script_url.IsEmpty())
	{
		mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
		return COMM_LOADING;
	}

	// Check if script is current, and load it if not
	switch (MaybeLoadProxyScript())
	{
	case NO:
		return COMM_LOADING;
	case MAYBE:
		if(!mh->HasCallBack(this, MSG_IMG_CONTINUE_DECODING, 0))
			BeCallBack(MSG_URL_DATA_LOADED, 0);
		mh->PostDelayedMessage(MSG_IMG_CONTINUE_DECODING, 0, 0, RETRY_DELAY);
		return COMM_WAITING;
	default:
		// Script was already loaded and current: start the proxy resolution
		OP_DBG(("Calling ExecuteScript"));
		return ExecuteScript();
	}
}

BOOL3
URL_AutoProxyConfig_LoadHandler::MaybeLoadProxyScript()
{
	URLStatus status = g_proxycfg->configuration_script_url.Status(TRUE);
	BOOL expired = !!g_proxycfg->configuration_script_url.GetAttribute(URL::KFermented);
	OP_NEW_DBG("MaybeLoadProxyScript", "apc");

	// Waiting for script to load?
	if (status == URL_LOADING && g_proxycfg->settings_have_changed)
		return MAYBE;

	// Script not loaded or not current?
	if (status != URL_LOADED || expired)
	{
 		g_proxycfg->settings_have_changed = TRUE;
		g_proxycfg->number_of_loads_from_url++;

		URL load_url = g_proxycfg->configuration_script_url;
		if(status == URL_LOADING)
        {
			URL final_url = g_proxycfg->configuration_script_url.GetAttribute(URL::KMovedToURL,TRUE);
			if(!final_url.IsEmpty())
				load_url = final_url;
        }

		CommState stat;
		URL referrer;

		load_url.SetAttribute(URL::KIsPACFile,TRUE);

#ifdef _DEBUG
		OpString url_name;
		load_url.GetAttribute(URL::KUniName_Username_Password_Hidden, url_name);
		OP_DBG((UNI_L("Starting loading of script %s because %s"),
				url_name.CStr(),
				status != URL_LOADED ? UNI_L("not loaded") : UNI_L("expired")));
#endif

		if(expired)
			stat = load_url.Reload(mh, referrer, TRUE, FALSE);
		else
			stat = load_url.Load(mh, referrer);

		if(stat != COMM_LOADING)
		{
			DisableProxyAndFail(AP_ERRSTR(SI,ERR_AUTO_PROXY_CONFIG_FAILED));
			return NO;
		}

		if(!mh->HasCallBack(this, MSG_URL_DATA_LOADED,load_url.Id()))	  BeCallBack(MSG_URL_DATA_LOADED,load_url.Id());
		if(!mh->HasCallBack(this, MSG_URL_LOADING_FAILED,load_url.Id()))  BeCallBack(MSG_URL_LOADING_FAILED,load_url.Id());
		if(!mh->HasCallBack(this, MSG_NOT_MODIFIED,load_url.Id()))		  BeCallBack(MSG_NOT_MODIFIED,load_url.Id());
		if(!mh->HasCallBack(this, MSG_URL_MOVED,load_url.Id()))			  BeCallBack(MSG_URL_MOVED,load_url.Id());

		return NO;
	}

	// Script present and current.  Go on and use it.
	return YES;
}

OP_STATUS
URL_AutoProxyConfig_LoadHandler::GetScriptURL(URL& resolved_url)
{
	OP_STATUS ret = 0;

	OpStringC apcURL =
		g_pcnet->IsAutomaticProxyOn()
			? g_pcnet->GetStringPref(PrefsCollectionNetwork::AutomaticProxyConfigURL)
			: OpStringC((uni_char *) NULL);

	OpString ResolvedApcURL;
	TRAP(ret, g_url_api->ResolveUrlNameL(apcURL, ResolvedApcURL));

	if (OpStatus::IsSuccess(ret))
	{
		resolved_url = g_url_api->GetURL(ResolvedApcURL.CStr());

		if (url == resolved_url.GetRep())
		{
			urlManager->MakeUnique(url);
			resolved_url = g_url_api->GetURL(ResolvedApcURL.CStr());
		}
	}
	return ret;
}

void URL_AutoProxyConfig_LoadHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_NEW_DBG("HandleCallback", "apc");
	switch(msg)
	{
	case MSG_URL_DATA_LOADED:
	{
		OP_DBG(("DATA_LOADED"));
		if(g_proxycfg->configuration_script_url.Id(TRUE) != URL_ID(par1))
			return;

		URLStatus status = g_proxycfg->configuration_script_url.Status(TRUE);

		if(status == URL_LOADING)
			return;

		if(status != URL_LOADED)
		{
			DisableProxyAndFail(ERR_SSL_ERROR_HANDLED);
			return;
		}

		mh->UnsetCallBack(this,MSG_URL_DATA_LOADED);
		mh->UnsetCallBack(this,MSG_URL_LOADING_FAILED);
		mh->UnsetCallBack(this,MSG_NOT_MODIFIED);
        mh->UnsetCallBack(this,MSG_URL_MOVED);
	}

	execute_it:
		OP_DBG(("Calling ExecuteScript"));
		if(ExecuteScript() == COMM_REQUEST_FAILED)
			DisableProxyAndFail(AP_ERRSTR(SI,ERR_AUTO_PROXY_CONFIG_FAILED));
		break;

	case MSG_COMM_NAMERESOLVED:			// Callback on comm object, not on url object, hence no Id check
		OP_DBG(("COMM_NAMERESOLVED"));
		goto execute_it;

	case MSG_COMM_LOADING_FAILED:
		OP_DBG(("COMM_LOADING_FAILED"));
		goto execute_it;

	case MSG_URL_LOADING_FAILED:
		OP_DBG(("LOADING_FAILED"));

		if(g_proxycfg->configuration_script_url.Id(TRUE) != URL_ID(par1))
			return;

		mh->UnsetCallBack(this,MSG_URL_DATA_LOADED);
		mh->UnsetCallBack(this,MSG_URL_LOADING_FAILED);
		mh->UnsetCallBack(this,MSG_NOT_MODIFIED);
        mh->UnsetCallBack(this,MSG_URL_MOVED);

		DisableProxyAndFail(AP_ERRSTR(SI,ERR_AUTO_PROXY_CONFIG_FAILED));
		break;

	case MSG_URL_MOVED:
		OP_DBG(("URL_MOVED"));
		{
			URL temp = g_proxycfg->configuration_script_url.GetAttribute(URL::KMovedToURL);
			if(g_proxycfg->configuration_script_url.Id() != URL_ID(par1) || temp.IsEmpty())
				return;
             
			g_proxycfg->configuration_script_url = temp;

			mh->UnsetCallBack(this,MSG_URL_DATA_LOADED);
			mh->UnsetCallBack(this,MSG_URL_LOADING_FAILED);
			mh->UnsetCallBack(this,MSG_NOT_MODIFIED);
			mh->UnsetCallBack(this,MSG_URL_MOVED);
			BeCallBack(MSG_URL_DATA_LOADED,par2);
			BeCallBack(MSG_URL_LOADING_FAILED,par2);
			BeCallBack(MSG_NOT_MODIFIED,par2);
			BeCallBack(MSG_URL_MOVED,par2);
		}
		break;

	case MSG_IMG_CONTINUE_DECODING:
	{
		switch (g_proxycfg->configuration_script_url.Status(TRUE))
		{
		case URL_LOADING:
			if(!mh->HasCallBack(this, MSG_IMG_CONTINUE_DECODING, 0))
				BeCallBack(MSG_URL_DATA_LOADED, 0);
			mh->PostDelayedMessage(MSG_IMG_CONTINUE_DECODING, 0, 0, RETRY_DELAY);
			break;

		case URL_LOADED:
			// Script was already loaded and current: start the proxy resolution
			mh->UnsetCallBack(this,MSG_IMG_CONTINUE_DECODING);
			goto execute_it;
			break;

		default:
			// Oh well, it probably wasn't important anyway
			mh->UnsetCallBack(this,MSG_IMG_CONTINUE_DECODING);
			break;
		}
		return;
	}
	}
}

CommState URL_AutoProxyConfig_LoadHandler::ExecuteScript()
{
	OP_PROBE7(URL_AUTOPROXYCONFIG_LOADHANDLER_EXECUTESCRIPT);
	uni_char *ret;
	OP_STATUS stat = OpStatus::OK;
	OP_NEW_DBG("ExecuteScript", "apc");

	ret = NULL;

	if (!g_proxycfg->proxycfg || g_proxycfg->settings_have_changed)
	{
		if (g_proxycfg->pending_requests == 0)		// Only delete the proxyconf object and allow changes to take effect if no requests are active
		{
			JSProxyConfig::Destroy(g_proxycfg->proxycfg);
			g_proxycfg->proxycfg = NULL;
			g_proxycfg->settings_have_changed = FALSE;	// Absorb the changed settings
			g_proxycfg->proxycfg = JSProxyConfig::Create( g_proxycfg->configuration_script_url, stat );
			if (!g_proxycfg->proxycfg)
			{
				if (!OpStatus::IsMemoryError(stat))
					DisableProxyAndFail(0);				// Bugs in script.  JSProxyConfig::Create takes care of printing the error msg
				goto oom_abort;
			}
		}
	}

	if (!pending_request)
	{
		uni_char *url_str = SetNewStr(url->GetAttribute(URL::KUniName_Username_Password_Hidden).CStr());
		if (url_str == NULL)
			stat = OpStatus::ERR_NO_MEMORY;
		else
		{
			const uni_char *host = url->GetAttribute(URL::KUniHostName).CStr();
			if(!host)
				host = UNI_L("");
			OP_DBG((UNI_L("Starting (%p) FindProxyForURL(\"%s\", \"%s\")"), (void*)(g_proxycfg->proxycfg), url_str, host));
			stat = g_proxycfg->proxycfg->FindProxyForURL( url_str, host, &ret, &pending_request );
			OP_DELETEA(url_str);
		}
	}
	else
	{
		--g_proxycfg->pending_requests;
		// NOTE: callback for dns lookup must set a flag for whether the
		// lookup was successful or not in the pending_request structure.
		OP_DBG((UNI_L("Continuing (%p) FindProxyForURL()"), (void*)(g_proxycfg->proxycfg)));
		stat = g_proxycfg->proxycfg->FindProxyForURL( pending_request, &ret );
	}

	if(stat != OpStatus::OK)
		goto oom_abort;

	if(ret)
	{
		OP_DBG((UNI_L("Completed (%p) FindProxyForURL()"), (void*)(g_proxycfg->proxycfg)));
#ifdef WEBSOCKETS_SUPPORT
		if (url->GetWebSocket())
			url->GetWebSocket()->SetAutoProxy(ret);
#endif //WEBSOCKETS_SUPPORT
		if (url->GetDataStorage())
		{
			URL_HTTP_ProtocolData *hptr = url->GetDataStorage()->GetHTTPProtocolData();
			if (!hptr)
				goto oom_abort;
			else
			{
				if (OpStatus::IsMemoryError(hptr->sendinfo.proxyname.Set( ret )))
				{
					stat = OpStatus::ERR_NO_MEMORY;
					goto oom_abort;
				}
			}
		}

		op_free(ret);
		OP_DELETE(pending_request);
		pending_request = 0;
		mh->PostMessage(MSG_COMM_PROXY_DETERMINED, Id(), 1);
		return COMM_REQUEST_FINISHED;
	}

	if (pending_request)
	{
		++g_proxycfg->pending_requests;
		if (pending_request->comm)
		{
			/* Suspended waiting for DNS on url: just block until ready */
			BeCallBack(MSG_COMM_NAMERESOLVED, pending_request->comm->Id());
			BeCallBack(MSG_COMM_LOADING_FAILED, pending_request->comm->Id());

		}
		else
		{
			/* Suspended because timeslice expired: run it again in 50ms */
			BeCallBack(MSG_URL_DATA_LOADED, g_proxycfg->configuration_script_url.Id());
			mh->PostDelayedMessage(MSG_URL_DATA_LOADED, g_proxycfg->configuration_script_url.Id(), 0, 50);
		}
		return COMM_REQUEST_WAITING;
	}

oom_abort:
	op_free(ret);
	OP_DELETE(pending_request);
	pending_request = 0;
	if (OpStatus::IsMemoryError(stat))
		g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	return COMM_REQUEST_FAILED;
}

void URL_AutoProxyConfig_LoadHandler::DisableProxyAndFail(int msg)
{
	OP_NEW_DBG("DisableProxyAndFail", "apc");
	OP_DBG(("Something went wrong, disabling APC and reporting error"));

#ifdef OPERA_CONSOLE
	if (g_console->IsLogging())
	{
		OpConsoleEngine::Message cmsg(OpConsoleEngine::Network, OpConsoleEngine::Error);
		ANCHOR(OpConsoleEngine::Message, cmsg);
		OP_STATUS op_err = url->GetAttribute(URL::KUniName_Username_Password_Hidden_Escaped, 0, cmsg.url);
		if(OpStatus::IsSuccess(op_err))
		{
			g_languageManager->GetStringL(
#ifdef HC_CAP_ERRENUM_AS_STRINGENUM
				(Str::LocaleString) msg,
#else
				ConvertUrlStatusToLocaleString(msg),
#endif
				cmsg.message);
			g_console->PostMessageL(&cmsg);
		}
	}
#endif // OPERA_CONSOLE

	ReleaseAutoProxyResources();
	g_pcnet->EnableAutomaticProxy(FALSE);
	if (msg)
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), msg);
}

void     URL_AutoProxyConfig_LoadHandler::ProcessReceivedData() { /* nothing */ }
unsigned URL_AutoProxyConfig_LoadHandler::ReadData(char *buffer, unsigned buffer_len) { return 0; }
void     URL_AutoProxyConfig_LoadHandler::EndLoading(BOOL force) { /* nothing */ }

#endif // SUPPORT_AUTO_PROXY_CONFIGURATION
