/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 2003-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"

#include "modules/probetools/probepoints.h"
#include "modules/dom/src/domasynccallback.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domcore/text.h"
#include "modules/dom/src/domcore/cdatasection.h"
#include "modules/dom/src/domcore/comment.h"
#include "modules/dom/src/domcore/procinst.h"
#include "modules/dom/src/domcore/domxmldocument.h"
#include "modules/dom/src/domcss/cssrule.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventdata.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domprogressevent.h"
#include "modules/dom/src/domfile/dombloburl.h"
#include "modules/dom/src/domfile/domfilereader.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/domhtml/htmlimplem.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domload/lsloader.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/doc/frm_doc.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/ecmascript/ecmascript.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/forms/webforms2support.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/logdoc/htm_ldoc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/style/css_dom.h"
#include "modules/url/url_sn.h"
#include "modules/windowcommander/src/WindowCommander.h"

#ifdef JS_PLUGIN_SUPPORT
# include "modules/jsplugins/src/js_plugin_manager.h"
# include "modules/jsplugins/src/js_plugin_context.h"
#endif // JS_PLUGIN_SUPPORT

#ifdef DOM_SUPPORT_BLOB_URLS
# include "modules/util/opguid.h"
#endif // DOM_SUPPORT_BLOB_URLS

#ifdef WEBSERVER_SUPPORT
# include "modules/dom/src/domwebserver/domwebserver.h"
#endif // WEBSERVER_SUPPORT

#ifdef APPLICATION_CACHE_SUPPORT
# include "modules/applicationcache/application_cache_manager.h"
# include "modules/dom/src/domapplicationcache/domapplicationcache.h"
#endif // APPLICATION_CACHE_SUPPORT

#ifdef SVG_DOM
# include "modules/dom/src/domsvg/domsvgelement.h"
# include "modules/dom/src/domsvg/domsvgdoc.h"
# include "modules/dom/src/domsvg/domsvgelementinstance.h"
# include "modules/dom/src/domsvg/domsvgtimeevent.h"
#endif // SVG_DOM

#ifdef SVG_SUPPORT
# include "modules/svg/SVGManager.h"
# include "modules/svg/svg_image.h"
#endif // SVG_SUPPORT

#ifdef GADGET_SUPPORT
# include "modules/dom/src/opera/domwidget.h"
#endif // GADGET_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "modules/dom/src/opera/domgadgetfile.h"
#endif // DOM_GADGET_FILE_API_SUPPORT

#ifdef DOM_XSLT_SUPPORT
# include "modules/dom/src/opera/domxslt.h"
#endif // DOM_XSLT_SUPPORT

#ifdef JS_SCOPE_CLIENT
# include "modules/dom/src/opera/domscopeclient.h"
#endif // JS_SCOPE_CLIENT

#ifdef CLIENTSIDE_STORAGE_SUPPORT
# include "modules/dom/src/storage/storage.h"
#endif

#ifdef DOM_JIL_API_SUPPORT
# include "modules/dom/src/domjil/domjilwidget.h"
#endif // DOM_JIL_API_SUPPORT

#ifdef WEBSOCKETS_SUPPORT
#include "modules/dom/src/websockets/domwebsocket.h"
#endif // WEBSOCKETS_SUPPORT

#ifdef TOUCH_EVENTS_SUPPORT
# include "modules/dom/src/domevents/domtouchevent.h"
#endif // TOUCH_EVENTS_SUPPORT

#ifdef MEDIA_HTML_SUPPORT
# include "modules/media/mediatrack.h"
# include "modules/dom/src/media/domtexttrack.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef WEBSERVER_SUPPORT
# include "modules/webserver/webserver-api.h"
#endif // WEBSERVER_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
# include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
# include "modules/dom/src/domwebworkers/domcrossmessage.h"
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
# include "modules/database/opdatabasemanager.h"
#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef DOM_GEOLOCATION_SUPPORT
# include "modules/dom/src/js/geoloc.h"
#endif // DOM_GEOLOCATION_SUPPORT

#ifdef EXTENSION_SUPPORT
# include "modules/dom/src/extensions/domextensionmanager.h"
#endif // EXTENSION_SUPPORT

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/pi/OpDragObject.h"
#endif // DRAG_SUPPORT

extern DOM_FunctionImpl DOM_dummyMethod;

/*=== Creation and destruction ===*/

/* static */
OP_STATUS
DOM_Environment::Create(DOM_Environment *&env, FramesDocument *doc)
{
	// If this is a sub-frame then we could either create a runtime
	// with a heap of its own (good for GC performance) or join it
	// with its parent immediately (good for fragmentation). We
	// decide which to do based on some heuristics to make
	// sure we are fast when we need to be fast.
	ES_Runtime *parent_runtime = NULL;
	if (doc->GetParentDoc())
	{
		parent_runtime = doc->GetParentDoc()->GetESRuntime();
		if (parent_runtime)
		{
			// Would be nice to also do a security check here but security checks
			// sadly have side effects regarding document.domain inheritance.
			if (parent_runtime->HasHighMemoryFragmentation())
				parent_runtime->SuitableForReuse(TRUE);
			else
				parent_runtime = NULL;
		}
	}

#ifdef DOM_WEBWORKERS_SUPPORT
	OP_STATUS status = DOM_EnvironmentImpl::Make(env, doc, NULL);
#else
	OP_STATUS status = DOM_EnvironmentImpl::Make(env, doc);
#endif // DOM_WEBWORKERS_SUPPORT

	if (parent_runtime)
		parent_runtime->SuitableForReuse(FALSE);

	return status;
}

/* static */ OP_STATUS
#ifdef DOM_WEBWORKERS_SUPPORT
DOM_EnvironmentImpl::Make(DOM_Environment *&environment, FramesDocument *doc, URL *base_worker_url, DOM_Runtime::Type type)
#else
DOM_EnvironmentImpl::Make(DOM_Environment *&environment, FramesDocument *doc)
#endif // DOM_WEBWORKERS_SUPPORT
{
#ifdef DOM_WEBWORKERS_SUPPORT
	BOOL for_worker = type == DOM_Runtime::TYPE_DEDICATED_WEBWORKER || type == DOM_Runtime::TYPE_SHARED_WEBWORKER;
	OP_ASSERT(!(for_worker && !base_worker_url));
#else
	DOM_Runtime::Type type = DOM_Runtime::TYPE_DOCUMENT;
	BOOL for_worker = FALSE;
#endif // DOM_WEBWORKERS_SUPPORT

	/* A non-worker _must_ supply a valid document; a worker musn't. */
	OP_ASSERT(for_worker == (doc == NULL));

	BOOL pref_js_enabled = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, DOM_EnvironmentImpl::GetHostName(doc));
	BOOL ecmascript_enabled = for_worker || DOM_Environment::IsEnabled(doc, pref_js_enabled);

#ifdef DEBUG_ENABLE_OPASSERT
	if (!ecmascript_enabled && doc->GetURL().Type() == URL_OPERA && !IsAboutBlankURL(doc->GetURL()))
		OP_ASSERT(!"Scripts trying to run on an opera: page with blocked scripting. Check who tries to do that or whether scripts should be allowed on this page.");
#endif // DEBUG_ENABLE_OPASSERT

#ifdef USER_JAVASCRIPT
	BOOL user_js_enabled = !for_worker && g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSEnabled, DOM_EnvironmentImpl::GetHostName(doc));
#endif // USER_JAVASCRIPT

#ifdef JS_PLUGIN_SUPPORT
	JS_Plugin_Context *js_plugin_context;
#endif // JS_PLUGIN_SUPPORT

	if (!ecmascript_enabled)
		return OpStatus::ERR;

	environment = NULL;

	DOM_EnvironmentImpl *impl = OP_NEW(DOM_EnvironmentImpl, ());
	if (!impl)
		return OpStatus::ERR_NO_MEMORY;

	impl->is_loading = !for_worker && !doc->IsLoaded();

#ifdef DOM_WEBWORKERS_SUPPORT
	impl->web_workers = OP_NEW(DOM_WebWorkerController, (impl));
	if (!impl->web_workers)
		goto oom_error;
#endif // DOM_WEBWORKERS_SUPPORT

	impl->window = NULL;
	impl->runtime = OP_NEW(DOM_Runtime, ());
	if (!impl->runtime)
		goto oom_error;

	// deprecated to use FramesDocument on a runtime only to get
	// at its associated scheduler and asyncInterface
	impl->runtime->SetFramesDocument(doc);

	impl->scheduler = ES_ThreadScheduler::Make(impl->runtime);
	if (!impl->scheduler)
		goto oom_error_delete_runtime;

	impl->asyncif = OP_NEW(ES_AsyncInterface, (impl->runtime, impl->scheduler));
	if (!impl->asyncif)
		goto oom_error_delete_runtime;

	impl->runtime->SetESScheduler(impl->scheduler);
	impl->runtime->SetESAsyncInterface(impl->asyncif);
#ifdef DOM_WEBWORKERS_SUPPORT
	if (for_worker)
	{
		if (OpStatus::IsMemoryError(impl->runtime->Construct(type, impl, *base_worker_url)))
			goto oom_error_delete_runtime;
	}
	else
#endif // DOM_WEBWORKERS_SUPPORT
	if (OpStatus::IsMemoryError(impl->runtime->Construct(type, impl, "Window", doc->GetMutableOrigin())))
		goto oom_error_delete_runtime;

	impl->frames_document = doc;

#ifdef JS_PLUGIN_SUPPORT
	if (!for_worker)
	{
		if (!(js_plugin_context = g_jsPluginManager->CreateContext(impl->runtime, impl->window)))
			goto oom_error;
		impl->window->SetJSPluginContext(js_plugin_context);
	}
#endif // JS_PLUGIN_SUPPORT

#ifdef TOUCH_EVENTS_SUPPORT
# ifdef PI_UIINFO_TOUCH_EVENTS
	if (g_op_ui_info->IsTouchEventSupportWanted())
# endif // PI_UIINFO_TOUCH_EVENTS
		if (OpStatus::IsMemoryError(DOM_TouchList::Make(impl->active_touches, impl->runtime)))
			goto oom_error;
#endif // TOUCH_EVENTS_SUPPORT

#ifdef USER_JAVASCRIPT
	impl->user_js_manager = NULL;

	if (!for_worker && doc->GetWindow()->GetType() != WIN_TYPE_NORMAL)
		user_js_enabled = FALSE;

#ifdef GADGET_SUPPORT
	if (!for_worker && doc->GetWindow()->GetGadget())
	{
# ifdef ENABLE_LAX_SECURITY_AND_GADGET_USER_JS //  This is defined for internal builds ONLY!
		user_js_enabled = !doc->GetWindow()->GetGadget()->IsExtension();
# else
		user_js_enabled = FALSE;
# endif // ENABLE_LAX_SECURITY_AND_GADGET_USER_JS
	}
#endif // GADGET_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	if (OpStatus::IsMemoryError(impl->web_workers->InitWorkerPrefs(impl)))
		goto oom_error;
#endif // DOM_WEBWORKERS_SUPPORT

	if (!for_worker && doc->GetURL().Type() == URL_OPERA)
		user_js_enabled = FALSE;

	if (!for_worker
#ifndef DOM_BROWSERJS_SUPPORT
	    && user_js_enabled
#endif // DOM_BROWSERJS_SUPPORT
		)
	{
		int security = doc->GetURL().GetAttribute(URL::KSecurityStatus);

		impl->user_js_manager = OP_NEW(DOM_UserJSManager, (impl, user_js_enabled, security != SECURITY_STATE_NONE && security != SECURITY_STATE_UNKNOWN));

		OP_STATUS status;

		if (!impl->user_js_manager || OpStatus::IsMemoryError(status = impl->user_js_manager->Construct()))
			goto oom_error;

		if (OpStatus::IsError(status))
		{
			OP_DELETE(impl->user_js_manager);
			impl->user_js_manager = NULL;
		}
	}
#endif // USER_JAVASCRIPT

	if (!for_worker)
		if (LogicalDocument *logdoc = doc->GetLogicalDocument())
		{
			unsigned count;

			if (logdoc->GetEventHandlerCount(ONMOUSEMOVE, count))
				*impl->GetEventHandlersCounter(ONMOUSEMOVE) = MIN(count, USHRT_MAX);
			if (logdoc->GetEventHandlerCount(ONMOUSEOVER, count))
				*impl->GetEventHandlersCounter(ONMOUSEOVER) = MIN(count, USHRT_MAX);
			if (logdoc->GetEventHandlerCount(ONMOUSEENTER, count))
				*impl->GetEventHandlersCounter(ONMOUSEENTER) = MIN(count, USHRT_MAX);
			if (logdoc->GetEventHandlerCount(ONMOUSEOUT, count))
				*impl->GetEventHandlersCounter(ONMOUSEOUT) = MIN(count, USHRT_MAX);
			if (logdoc->GetEventHandlerCount(ONMOUSELEAVE, count))
				*impl->GetEventHandlersCounter(ONMOUSELEAVE) = MIN(count, USHRT_MAX);
#ifdef TOUCH_EVENTS_SUPPORT
			if (logdoc->GetEventHandlerCount(TOUCHSTART, count))
				*impl->GetEventHandlersCounter(TOUCHSTART) = MIN(count, USHRT_MAX);
			if (logdoc->GetEventHandlerCount(TOUCHMOVE, count))
				*impl->GetEventHandlersCounter(TOUCHMOVE) = MIN(count, USHRT_MAX);
			if (logdoc->GetEventHandlerCount(TOUCHEND, count))
				*impl->GetEventHandlersCounter(TOUCHEND) = MIN(count, USHRT_MAX);
			if (logdoc->GetEventHandlerCount(TOUCHCANCEL, count))
				*impl->GetEventHandlersCounter(TOUCHCANCEL) = MIN(count, USHRT_MAX);
#endif // TOUCH_EVENTS_SUPPORT
		}

	impl->prefs_listener.owner = impl;
	impl->prefs_listener.is_host_overridden = FALSE;
#ifdef PREFS_HOSTOVERRIDE
	if (ServerName *server_name = DOM_EnvironmentImpl::GetHostName(doc))
	{
		const uni_char *host_name = server_name->UniName();
		impl->prefs_listener.is_host_overridden = host_name && g_pcjs->IsHostOverridden(host_name, FALSE) > 0;
	}
#endif // PREFS_HOSTOVERRIDE

	if (OpStatus::IsError(g_pcjs->RegisterListener(&impl->prefs_listener)))
		goto oom_error;

	impl->runtime->InitializationFinished();

	impl->state = RUNNING;
	impl->is_es_enabled = ecmascript_enabled;

	environment = impl;
	return OpStatus::OK;

oom_error_delete_runtime:
	if (impl->runtime && impl->runtime->GetEnvironment())
	{
		/* Runtime object created, but OOMed on initialization, so detach the
		   runtime only. */
		impl->runtime->Detach();
	}
	else
	{
		OP_DELETE(impl->runtime);
		impl->runtime = NULL;
	}

oom_error:
#ifdef EXTENSION_SUPPORT
	/* Release any extension contexts already started. */
	if (impl->user_js_manager)
		impl->user_js_manager->BeforeDestroy();

	if (impl->extension_background)
	{
		g_extension_manager->RemoveExtensionContext(impl->extension_background->GetExtensionSupport());
		impl->extension_background = NULL;
	}
#endif // EXTENSION_SUPPORT

	/* Avoid assertion in destructor. */
	impl->frames_document = NULL;

	if (impl->scheduler)
        impl->scheduler->RemoveThreads(TRUE, TRUE);

	OP_DELETE(impl);
	return OpStatus::ERR_NO_MEMORY;
}

/* static */
void
DOM_Environment::Destroy(DOM_Environment *env)
{
	if (env)
	{
		DOM_EnvironmentImpl *impl = (DOM_EnvironmentImpl *) env;

		if (impl->state < DOM_EnvironmentImpl::DESTROYED)
			impl->BeforeDestroy();

		impl->scheduler->RemoveThreads(TRUE, TRUE);

		g_pcjs->UnregisterListener(&impl->prefs_listener);

		impl->node_collection_tracker.RemoveAll();
		impl->element_collection_tracker.RemoveAll();

#ifdef DOM_WEBWORKERS_SUPPORT
		if (impl->web_workers)
			impl->web_workers->DetachWebWorkers();
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef USER_JAVASCRIPT
		OP_DELETE(impl->user_js_manager);
		impl->user_js_manager = NULL;
#endif // USER_JAVASCRIPT

		if (impl->frames_document)
		{
			/* We need to clear the pointer to the js window object or
			   we will keep the dom environment alive through the protected
			   window reference and all memory won't be freed. */
			DocumentManager *docman = impl->frames_document->GetDocManager();
			if (DOM_ProxyEnvironmentImpl *proxy_env = static_cast<DOM_ProxyEnvironmentImpl *>(docman->GetDOMEnvironment()))
				proxy_env->ResetRealObject(impl->window, impl->document, impl->window->GetLocation());
		}


		impl->runtime->SetFramesDocument(NULL);
		impl->runtime->SetESScheduler(NULL);
		impl->runtime->SetESAsyncInterface(NULL);
		impl->frames_document = NULL;

		/* This needs to be done last, since this might trigger a GC run
		   that frees both impl->runtime and impl. */
		impl->runtime->Detach();
	}
}

class DOM_PropertyNameTranslator
	: public EcmaScript_Manager::PropertyNameTranslator
{
public:
	virtual int TranslatePropertyName(const uni_char *name)
	{
		if (name)
			return DOM_StringToAtom(name);
		else
			return OP_ATOM_UNASSIGNED;
	}
};

/* static */
OP_STATUS
DOM_Environment::CreateStatic()
{
	OP_ASSERT(!g_DOM_globalData);

	g_DOM_globalData = OP_NEW(DOM_GlobalData, ());
	if (!g_DOM_globalData)
		return OpStatus::ERR_NO_MEMORY;

	DOM_PropertyNameTranslator *translator = OP_NEW(DOM_PropertyNameTranslator, ());
	if (!translator)
		return OpStatus::ERR_NO_MEMORY;
	g_ecmaManager->SetPropertyNameTranslator(translator);

	return OpStatus::OK;
}

/* static */
void
DOM_Environment::DestroyStatic()
{
	if (g_DOM_globalData)
	{
		DOM_Object::GetEmptyTempBuf()->FreeStorage();

		if (g_DOM_globalCallbacks)
		{
			g_DOM_globalCallbacks->Clear();
			OP_DELETE(g_DOM_globalCallbacks);
			g_DOM_globalCallbacks = NULL;
		}

		if (g_DOM_operaCallbacks)
		{
			g_DOM_operaCallbacks->Clear();
			OP_DELETE(g_DOM_operaCallbacks);
			g_DOM_operaCallbacks = NULL;
		}

#ifdef USER_JAVASCRIPT
		DOM_UserJSManager::RemoveScripts();
#endif // USER_JAVASCRIPT

		OP_DELETE(g_DOM_globalData);
		g_DOM_globalData = NULL;
	}
}

/* virtual */ void
DOM_EnvironmentImpl::BeforeDestroy()
{
	state = DESTROYED;
	BeforeUnload();
#ifdef DOM3_LOAD
	while (Link *lsloader = lsloaders.First())
		/* This doesn't wake the calling thread up, if there is one.  That's
		   okay, because the calling thread will be running in our scheduler,
		   and will thus be cancelled real soon since the environment is about
		   to be destroyed. */
		((DOM_LSLoader *) lsloader)->Abort();
#endif // DOM3_LOAD

#ifdef APPLICATION_CACHE_SUPPORT
	if (g_application_cache_manager)
		g_application_cache_manager->CacheHostDestructed(this);
#endif // APPLICATION_CACHE_SUPPORT
#ifdef DOM_XSLT_SUPPORT
	for (Link *processor = xsltprocessors.First(); processor; processor = processor->Suc())
		((DOM_XSLTProcessor *) processor)->Cleanup();
#endif // DOM_XSLT_SUPPORT
#ifdef MEDIA_HTML_SUPPORT
	media_elements.RemoveAll();
#endif // MEDIA_HTML_SUPPORT
#ifdef DOM_JIL_API_SUPPORT
	if (jil_widget)
	{
		jil_widget->OnBeforeEnvironmentDestroy();
		SetJILWidget(NULL);
	}
#endif // DOM_JIL_API_SUPPORT

#ifdef SECMAN_USERCONSENT
	g_secman_instance->OnBeforeRuntimeDestroy(GetDOMRuntime());
#endif // SECMAN_USERCONSENT

	for (DOM_AsyncCallback *callback = async_callbacks.First(); callback;)
	{
		DOM_AsyncCallback* destroy_callback = callback;
		callback = callback->Suc();
		destroy_callback->OnBeforeDestroy();	// Deletes the callback
	}

	query_selector_cache.Reset();

#ifdef USER_JAVASCRIPT
	if (user_js_manager)
		user_js_manager->BeforeDestroy();
#endif // USER_JAVASCRIPT
#ifdef EXTENSION_SUPPORT
	if (g_extension_manager && GetFramesDocument())
		g_extension_manager->RemoveExtensionContext(this);
#endif // EXTENSION_SUPPORT
}

/* virtual */ void
DOM_EnvironmentImpl::BeforeUnload()
{
#ifdef DOM_GADGET_FILE_API_SUPPORT
	for (Link *file = file_objects.First(); file; file = file->Suc())
		((DOM_FileBase *) file)->Cleanup();
#endif // DOM_GADGET_FILE_API_SUPPORT
	while (DOM_FileReader *reader = file_readers.First())
		reader->Abort();
#ifdef JS_SCOPE_CLIENT
	if (scope_client)
		scope_client->Disconnect(FALSE);
	OP_DELETE(scope_client); scope_client = 0;
#endif // JS_SCOPE_CLIENT
#ifdef JS_PLUGIN_SUPPORT
	JS_Plugin_Context *ctx = window ? window->GetJSPluginContext() : NULL;
	if (ctx)
		ctx->BeforeUnload();
#endif // JS_PLUGIN_SUPPORT
#ifdef DOM_WEBWORKERS_SUPPORT
	if (web_workers)
		web_workers->DetachWebWorkers();
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef DATABASE_STORAGE_SUPPORT
	DOM_Database::BeforeUnload(this);
#endif //DATABASE_STORAGE_SUPPORT
#ifdef CLIENTSIDE_STORAGE_SUPPORT
	DOM_Storage::FreeStorageResources(&webstorage_objects);
#endif //CLIENTSIDE_STORAGE_SUPPORT
#ifdef DOM_GEOLOCATION_SUPPORT
	DOM_Geolocation::BeforeUnload(this);
#endif // DOM_GEOLOCATION_SUPPORT
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
# ifdef DOM_WEBWORKERS_SUPPORT
	if (window)
# endif // DOM_WEBWORKERS_SUPPORT
	{
		window->RemoveOrientationListener();
		window->RemoveMotionListener();
	}
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#ifdef WEB_HANDLERS_SUPPORT
	JS_Navigator::BeforeUnload(this);
#endif // WEB_HANDLERS_SUPPORT
}

/*=== Simple getters ===*/

/* static */
BOOL
DOM_Environment::IsEnabled(FramesDocument *frames_document)
{
	if (frames_document)
	{
		DOM_EnvironmentImpl *impl = static_cast<DOM_EnvironmentImpl *>(frames_document->GetDOMEnvironment());

		return IsEnabled(frames_document, impl ? impl->IsEnabled() : g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, frames_document->GetURL()));
	}
	else
		return IsEnabled(frames_document, g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, static_cast<uni_char *>(NULL)));
}

/* static */
BOOL
DOM_Environment::IsEnabled(FramesDocument *frames_document, BOOL is_es_enabled)
{
	if (frames_document->GetURL().GetAttribute(URL::KIsDirectoryListing, TRUE))
		return FALSE;

	Window* win = frames_document->GetWindow();

	// Dev tools need to be enabled all the time
	if (win->GetType() == WIN_TYPE_DEVTOOLS)
		return TRUE;

#ifdef CORE_GOGI
	if (win->GetType() == WIN_TYPE_DIALOG)
		return TRUE;
#endif // CORE_GOGI

	if (frames_document->ContainsOnlyOperaInternalScripts() || frames_document->UsesWrapperDocument())
		return TRUE;

	// Search suggestions (which require scripts) for domain errors are not set up in an "opera:" page
	// KIsGeneratedByOpera is an extra security test not required by search suggestions.
	URL url = frames_document->GetURL();
	if (url.GetAttribute(URL::KIsGeneratedByOpera) && url.GetAttribute(URL::KForceAllowScripts))
		return TRUE;

	if (frames_document->GetURL().Type() == URL_OPERA)
	{
		// Bug CORE-8691, we have decided to keep a whitelist of
		// the opera urls that are allowed to run scripts since
		// they get heightened privilegies and if someone manage
		// to inject scripts it will make it possible we
		// are rescued by it.

		const uni_char* opera_page = frames_document->GetURL().GetAttribute(URL::KUniPath).CStr();
		if (opera_page)
		{
			if (
#ifdef OPERACONFIG_URL
				uni_stri_eq(opera_page, "config") ||
#endif // OPERACONFIG_URL
#ifdef SELFTEST
				uni_stri_eq(opera_page, "blanker") ||
#endif // SELFTEST
#ifdef CPUUSAGETRACKING
				uni_stri_eq(opera_page, "cpu") ||
#endif // CPUUSAGETRACKING
#ifdef WEBFEEDS_DISPLAY_SUPPORT
				uni_stri_eq(opera_page, "feeds") ||
#endif // WEBFEEDS_DISPLAY_SUPPORT
#ifdef ABOUT_OPERA_DEBUG
				uni_stri_eq(opera_page, "debug") ||
#endif // ABOUT_OPERA_DEBUG
#ifdef OPERAWIDGETS_URL
				uni_stri_eq(opera_page, "widgets") ||
#endif // OPERAWIDGETS_URL
#ifdef OPERAUNITE_URL
				uni_stri_eq(opera_page, "unite") ||
#endif // OPERAUNITE_URL
#ifdef OPERAEXTENSIONS_URL
				uni_stri_eq(opera_page, "extensions") ||
#endif // OPERAEXTENSIONS_URL
#ifdef OPERA_PERFORMANCE
				uni_stri_eq(opera_page, "performance") ||
#endif // OPERA_PERFORMANCE
#ifdef OPERABOOKMARKS_URL
				uni_stri_eq(opera_page, "bookmarks") ||
#endif // OPERABOOKMARKS_URL
#ifdef OPERASPEEDDIAL_URL
				uni_stri_eq(opera_page, "speeddial") ||
#endif // OPERASPEEDDIAL_URL
#ifdef SELFTEST
				uni_stri_eq(opera_page, "selftest") ||
#endif // SELFTEST
#ifdef ABOUT_OPERA_CACHE
				uni_stri_eq(opera_page, "cache") ||
				uni_strnicmp(opera_page, "cache?", 6) == 0 ||
#endif // ABOUT_OPERA_CACHE
#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
				uni_stri_eq(opera_page, DATABASE_ABOUT_WEBSTORAGE_URL) ||
#endif // DATABASE_ABOUT_WEBSTORAGE_URL
#ifdef DATABASE_ABOUT_WEBDATABASES_URL
				uni_stri_eq(opera_page, DATABASE_ABOUT_WEBDATABASES_URL) ||
#endif // DATABASE_ABOUT_WEBDATABASES_URL
#ifdef ABOUT_PRIVATE_BROWSING
				uni_stri_eq(opera_page, "private") ||
#endif // ABOUT_PRIVATE_BROWSING
#ifdef _PLUGIN_SUPPORT_
				uni_stri_eq(opera_page, "plugins") ||
#endif // _PLUGIN_SUPPORT_
				FALSE)
			{
				// These contain scripts that we've developed and need to be
				// able to run it.
				return TRUE;
			}
		}

		// Any other opera page has no need to run scripts as far as we know.
		if (!opera_page || !uni_str_eq(opera_page, "blank"))
			return FALSE;
	}

#ifdef WEBSERVER_SUPPORT
	// Allow the server side unite scripts to run
	if (g_webserver && g_webserver->WindowHasWebserverAssociation(win->Id()))
		return TRUE;
#endif // WEBSERVER_SUPPORT

	// There is a flag per window that also might disable ECMAScript
	if (!win->IsEcmaScriptEnabled())
		return FALSE;

#ifdef SVG_SUPPORT
	if(!g_svg_manager->IsEcmaScriptEnabled(frames_document))
		return FALSE;
#endif // SVG_SUPPORT

	return is_es_enabled;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::IsEnabled()
{
	BOOL runtime_enabled = runtime->Enabled();
	BOOL frame_enabled = frames_document && is_es_enabled;
#ifdef DOM_WEBWORKERS_SUPPORT
	frame_enabled = frame_enabled || !frames_document && GetWorkerController()->GetWorkerObject();
#endif // DOM_WEBWORKERS_SUPPORT

	return (runtime_enabled && frame_enabled);
}

/* virtual */
ES_Runtime *
DOM_EnvironmentImpl::GetRuntime()
{
	return runtime;
}

DOM_Runtime *
DOM_EnvironmentImpl::GetDOMRuntime()
{
	return runtime;
}

/* virtual */
ES_ThreadScheduler *
DOM_EnvironmentImpl::GetScheduler()
{
	return scheduler;
}

/* virtual */
ES_AsyncInterface *
DOM_EnvironmentImpl::GetAsyncInterface()
{
	return asyncif;
}

/* virtual */
DOM_Object *
DOM_EnvironmentImpl::GetWindow()
{
#ifdef DOM_WEBWORKERS_SUPPORT
	return window ? static_cast<DOM_Object *>(window) : static_cast<DOM_Object *>(GetWorkerController()->GetWorkerObject());
#else
	return (static_cast<DOM_Object *>(window));
#endif // DOM_WEBWORKERS_SUPPORT
}

/* virtual */
DOM_Object *
DOM_EnvironmentImpl::GetDocument()
{
	return document;
}

/* virtual */
FramesDocument *
DOM_EnvironmentImpl::GetFramesDocument()
{
	return frames_document;
}

OP_STATUS
DOM_EnvironmentImpl::GetProxyWindow(DOM_Object *&object, ES_Runtime *origining_runtime)
{
	if (!frames_document)
	{
		object = window;
		return OpStatus::OK;
	}

	DocumentManager *docman = frames_document->GetDocManager();

	RETURN_IF_ERROR(docman->ConstructDOMProxyEnvironment());

	return static_cast<DOM_ProxyEnvironmentImpl *>(docman->GetDOMEnvironment())->GetProxyWindow(object, origining_runtime);
}

OP_STATUS
DOM_EnvironmentImpl::GetProxyDocument(DOM_Object *&object, ES_Runtime *origining_runtime)
{
	if (!frames_document)
	{
		object = document;
		return OpStatus::OK;
	}

	DocumentManager *docman = frames_document->GetDocManager();

	RETURN_IF_ERROR(docman->ConstructDOMProxyEnvironment());

	return static_cast<DOM_ProxyEnvironmentImpl *>(docman->GetDOMEnvironment())->GetProxyDocument(object, origining_runtime);
}

OP_STATUS
DOM_EnvironmentImpl::GetProxyLocation(DOM_Object *&object, ES_Runtime *origining_runtime)
{
	if (!frames_document)
	{
		object = window->GetLocation();
		return OpStatus::OK;
	}

	DocumentManager *docman = frames_document->GetDocManager();

	RETURN_IF_ERROR(docman->ConstructDOMProxyEnvironment());

	return static_cast<DOM_ProxyEnvironmentImpl *>(docman->GetDOMEnvironment())->GetProxyLocation(object, origining_runtime);
}

/*=== Security and current executing script ===*/

/* virtual */
BOOL
DOM_EnvironmentImpl::AllowAccessFrom(const URL &url)
{
	if (frames_document)
		return window->OriginCheck(frames_document->GetSecurityContext(), url);
	else
		return FALSE;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::AllowAccessFrom(ES_Thread* thread)
{
	OP_ASSERT(thread || !"Use the other AllowAccessFrom");
	return window->OriginCheck(thread->GetScheduler()->GetRuntime());
}

/* virtual */
ES_Thread *
DOM_EnvironmentImpl::GetCurrentScriptThread()
{
	if (current_origining_runtime && current_origining_runtime->GetESScheduler())
		return DOM_Object::GetCurrentThread(current_origining_runtime);
	else
		return NULL;
}

/* virtual */
DocumentReferrer
DOM_EnvironmentImpl::GetCurrentScriptURL()
{
	if (current_origining_runtime)
		return DocumentReferrer(current_origining_runtime->GetMutableOrigin());
	else
		return DocumentReferrer();
}

/* virtual */
BOOL
DOM_EnvironmentImpl::SkipScriptElements()
{
	return skip_script_elements;
}

/* virtual */
TempBuffer *
DOM_EnvironmentImpl::GetTempBuffer()
{
	return current_buffer;
}


/*=== Creating nodes ===*/

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::ConstructNode(DOM_Object *&node, HTML_Element *element)
{
	DOM_Node *node2;
	RETURN_IF_ERROR(ConstructNode(node2, element, owner_document));
	node = node2;
	return OpStatus::OK;
}

OP_STATUS
DOM_EnvironmentImpl::ConstructNode(DOM_Node *&node, HTML_Element *element, DOM_Document *owner_document)
{
#ifdef DOM_EXPENSIVE_DEBUG_CODE
	extern void DOM_VerifyAllNodes();
	DOM_VerifyAllNodes();
#endif // DOM_EXPENSIVE_DEBUG_CODE

	if (!owner_document)
	{
		RETURN_IF_ERROR(ConstructDocumentNode());
		owner_document = document;
	}

	node = (DOM_Node *) element->GetESElement();

	if (!node)
	{
		if (frames_document && owner_document == document && !owner_document->GetRootElement())
			if (LogicalDocument *logdoc = frames_document->GetLogicalDocument())
			{
				HTML_Element *root_element = logdoc->GetDocRoot();

				if (root_element && root_element != element)
					RETURN_IF_ERROR(NewRootElement(root_element));
			}

		HTML_ElementType element_type = element->Type();

		ES_Object *prototype = NULL;
		const char *classname = NULL;

		if (element_type == HE_TEXT || element_type == HE_TEXTGROUP)
		{
			prototype = runtime->GetPrototype(DOM_Runtime::TEXT_PROTOTYPE);

			if (element->GetIsCDATA())
				DOM_ALLOCATE(node, DOM_CDATASection, (), prototype, "CDATASection", runtime);
			else
				DOM_ALLOCATE(node, DOM_Text, (), prototype, "Text", runtime);
		}
		else if (element_type == HE_COMMENT)
			DOM_ALLOCATE(node, DOM_Comment, (), runtime->GetPrototype(DOM_Runtime::COMMENT_PROTOTYPE), "Comment", runtime);
		else if (element_type == HE_PROCINST)
			DOM_ALLOCATE(node, DOM_ProcessingInstruction, (), runtime->GetPrototype(DOM_Runtime::PROCESSINGINSTRUCTION_PROTOTYPE), "ProcessingInstruction", runtime);
		else if (element_type == HE_DOCTYPE)
		{
			DOM_DocumentType *node0;
			RETURN_IF_ERROR(DOM_DocumentType::Make(node0, element, owner_document));
			node = node0;
		}
		else if (element_type == HE_DOC_ROOT)
		{
			OP_ASSERT(FALSE);
			node = owner_document;
			return OpStatus::OK;
		}
		else if (element->GetNsType() == NS_HTML)
		{
			DOM_HTMLElement *node0;
			RETURN_IF_ERROR(DOM_HTMLElement::Make(node0, element, this));
			node = node0;
		}
#if defined(SVG_DOM)
		else if (element->GetNsType() == NS_SVG)
		{
			if(element_type == Markup::SVGE_SHADOW ||
				element_type == Markup::SVGE_ANIMATED_SHADOWROOT ||
				element_type == Markup::SVGE_BASE_SHADOWROOT)
			{
				DOM_SVGElementInstance *node0;
				RETURN_IF_ERROR(DOM_SVGElementInstance::Make(node0, element, this));
				node = node0;
			}
			else
			{
				DOM_SVGElement *node0;
				RETURN_IF_ERROR(DOM_SVGElement::Make(node0, element, this));
				node = node0;
			}
		}
#endif // SVG_DOM
		else
			DOM_ALLOCATE(node, DOM_Element, (), runtime->GetPrototype(DOM_Runtime::ELEMENT_PROTOTYPE), "Element", runtime);

		if (classname)
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(node, runtime, prototype, classname));

		element->SetESElement(node);

		if (node->IsA(DOM_TYPE_ELEMENT))
			((DOM_Element *) node)->SetThisElement(element);
		else if (node->IsA(DOM_TYPE_CHARACTERDATA) || node->IsA(DOM_TYPE_PROCESSINGINSTRUCTION))
			((DOM_CharacterData *) node)->SetThisElement(element);
#ifdef SVG_DOM
		else if (node->IsA(DOM_TYPE_SVG_ELEMENT_INSTANCE))
			((DOM_SVGElementInstance *) node)->SetThisElement(element);
#endif // SVG_DOM
#ifdef CANVAS_SUPPORT
		if (node->IsA(DOM_TYPE_HTML_CANVASELEMENT))
		{
			/* Marked as significant in order to guarantee
			   that this node is marked during GCing (and
			   its external allocation cost can be accounted
			   for.) */
			node->SetIsSignificant();
			static_cast<DOM_HTMLCanvasElement *>(node)->RecordAllocationCost();
		}
#endif // CANVAS_SUPPORT

		node->SetOwnerDocument(owner_document);

		if (!element->ParentActual())
			node->SetIsSignificant();

		if (OpStatus::IsMemoryError(element->DOMSetEventHandlers(this)))
			/* FIXME: there is now a node created, but its won't have the
			   correct event handlers registered.  Perhaps no big deal. */
			return OpStatus::ERR_NO_MEMORY;

#ifdef DOM_EXPENSIVE_DEBUG_CODE
		DOM_VerifyAllNodes();
#endif // DOM_EXPENSIVE_DEBUG_CODE
	}
	else
		OP_ASSERT(node->GetOwnerDocument() == owner_document);

	return OpStatus::OK;
}

OP_STATUS
DOM_EnvironmentImpl::ConstructDocumentNode()
{
	if (!document)
	{
		if (!frames_document)
			return OpStatus::ERR;
		else if (LogicalDocument *logdoc = frames_document->GetLogicalDocument())
		{
			HTML_Element *root_element = logdoc->GetDocRoot();
			HLDocProfile *hld_profile = logdoc->GetHLDocProfile();

			/* This function is probably called before we know if the document
			   will be HTML or XML/XHTML. */
			if (hld_profile && hld_profile->IsXml())
			{
				is_xml = TRUE;

				if (root_element && root_element->GetNsType() == NS_HTML)
					is_xhtml = TRUE;
			}

#ifdef SVG_DOM
			if (root_element && root_element->GetNsType() == NS_SVG)
				is_svg = TRUE;
#endif // SVG_DOM

			OpString8 mimetype;
			RETURN_IF_ERROR(frames_document->GetURL().GetAttribute(URL::KMIME_Type, mimetype, TRUE));

			if (is_xml && mimetype.Find("xhtml") == KNotFound)
#ifdef SVG_DOM
				if (is_svg)
				{
					DOM_DOMImplementation *implementation;
					if (OpStatus::IsMemoryError(DOM_DOMImplementation::Make(implementation, this)))
						return OpStatus::ERR_NO_MEMORY;

					DOM_SVGDocument *svg_document;
					if (OpStatus::IsMemoryError(DOM_SVGDocument::Make(svg_document, implementation)))
						return OpStatus::ERR_NO_MEMORY;

					document = svg_document;
				}
				else
#endif // SVG_DOM
				{
					DOM_DOMImplementation *implementation;
					if (OpStatus::IsMemoryError(DOM_DOMImplementation::Make(implementation, this)))
						return OpStatus::ERR_NO_MEMORY;

					if (OpStatus::IsMemoryError(DOM_XMLDocument::Make(document, implementation, FALSE)))
						return OpStatus::ERR_NO_MEMORY;
				}
			else
			{
				DOM_HTMLDOMImplementation *implementation;
				if (OpStatus::IsMemoryError(DOM_HTMLDOMImplementation::Make(implementation, this)))
					return OpStatus::ERR_NO_MEMORY;

				DOM_HTMLDocument *html_document;
				if (OpStatus::IsMemoryError(DOM_HTMLDocument::Make(html_document, implementation, FALSE, is_xml)))
					return OpStatus::ERR_NO_MEMORY;

				document = html_document;
			}

			if (is_xml)
				if (XMLDocumentInformation *xml_document_info = logdoc->GetXMLDocumentInfo())
					if (OpStatus::IsMemoryError(document->SetXMLDocumentInfo(xml_document_info)))
						return OpStatus::ERR_NO_MEMORY;

			if (OpStatus::IsMemoryError(runtime->KeepAliveWithRuntime(document)))
				return OpStatus::ERR_NO_MEMORY;

			if (root_element && OpStatus::IsMemoryError(NewRootElement(root_element)))
				return OpStatus::ERR_NO_MEMORY;
			else if (HTML_Element *placeholder = logdoc->GetRoot())
				OpStatus::Ignore(document->ResetPlaceholderElement(placeholder));
		}
		else
			return OpStatus::ERR;
	}

	return OpStatus::OK;
}

/*=== Creating DOM_CSSRules ===*/

/* virtual */ OP_STATUS
DOM_EnvironmentImpl::ConstructCSSRule(DOM_Object *&rule, CSS_DOMRule *css_rule)
{
	DOM_CSSRule* ret_rule;
	RETURN_IF_ERROR(DOM_CSSRule::GetRule(ret_rule, css_rule, runtime));
	rule = ret_rule;
	return OpStatus::OK;
}


#ifdef APPLICATION_CACHE_SUPPORT

/*=== Send events to the ApplicationCache singleton ===*/

/* virtual */ OP_STATUS
DOM_EnvironmentImpl::SendApplicationCacheEvent(DOM_EventType event_type, BOOL lengthComputable, OpFileLength loaded, OpFileLength total)
{
	ES_Value application_cache;
	OP_BOOLEAN got_it;

	RETURN_IF_MEMORY_ERROR(got_it = window->GetPrivate(DOM_PRIVATE_applicationCache, &application_cache));

	DOM_ApplicationCache *app_cache;
	if (got_it == OpBoolean::IS_TRUE)
	{
		app_cache = DOM_VALUE2OBJECT(application_cache, DOM_ApplicationCache);
		return app_cache->OnEvent(event_type, lengthComputable, loaded, total);
	}
	return OpStatus::ERR;
}

#endif // APPLICATION_CACHE_SUPPORT

#ifdef WEBSERVER_SUPPORT

/* virtual */ BOOL
DOM_EnvironmentImpl::WebserverEventHasListeners(const uni_char *type)
{
	DOM_WebServer *webserver = window->GetWebServer();

	if (webserver && webserver->GetEventTarget() != NULL && webserver->GetEventTarget()->HasListeners(DOM_EVENT_CUSTOM, type, ES_PHASE_AT_TARGET) == TRUE)
	{
		return TRUE;
	}
	return FALSE;
}

/* virtual */ OP_STATUS
DOM_EnvironmentImpl::SendWebserverEvent(const uni_char *type, WebResource_Custom *web_resource_script)
{
	DOM_WebServer *webserver = window->GetWebServer();

	if (!webserver)
		return OpStatus::ERR;

	return DOM_WebServerRequestEvent::CreateAndSendEvent(type, webserver, web_resource_script);
}

#endif // WEBSERVER_SUPPORT


/*=== Handling events ===*/

/* static */
DOM_EventType
DOM_Environment::GetEventType(const uni_char *type)
{
	return DOM_Event::GetEventType(type, FALSE);
}

/* static */
const char *
DOM_Environment::GetEventTypeString(DOM_EventType type)
{
	if (type >= 0 && type < DOM_EVENTS_COUNT)
		return DOM_EVENT_NAME(type);
	else
		return NULL;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::HasEventHandlers(DOM_EventType type)
{
	unsigned short *counter = GetEventHandlersCounter(type);
	if (counter)
		return *counter > 0;
	return TRUE;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::HasEventHandler(HTML_Element *target, DOM_EventType type, HTML_Element **nearest_handler)
{
	if (!HasEventHandlers(type))
		return FALSE;

	if (target == frames_document->GetWindowEventTarget(type) || (DOM_Event::IsDocumentEvent(type) && target->Type() == HE_DOC_ROOT))
	{
		if (frames_document->HasWindowHandlerAsBodyAttr(type) || HasWindowEventHandler(type))
		{
			if (nearest_handler)
				*nearest_handler = NULL;
			return TRUE;
		}
	}

	HTML_Element *currentTarget = target;
	DOM_Object* node = NULL;
	BOOL in_document = FALSE;

	if (currentTarget)
		in_document = IsInDocument(currentTarget);

	ES_EventPhase phase = ES_PHASE_AT_TARGET;

	DOM_EventTarget *event_target;
	while (1)
	{
		if (currentTarget)
		{
			event_target = DOM_Node::GetEventTargetFromElement(currentTarget);

			if (!event_target && phase != ES_PHASE_CAPTURING && DOM_Node::GetEventTargetElement(currentTarget)->DOMHasEventHandlerAttribute(this, type))
				break;
		}
		else if (node) // !currentTarget -> document
			event_target = node->GetEventTarget();
		else
			return FALSE;

		// There can be listeners of different events triggered by this event (onload -> onreadystatechange, onfocus->domfocusin, ...)
		if (event_target)
		{
			BOOL found_listener = FALSE;
			DOM_EventType related_type = type;
			while (related_type != DOM_EVENT_NONE && !found_listener)
			{
				found_listener = event_target->HasListeners(related_type, NULL, phase);
				switch (related_type)
				{
				case ONFOCUS:
					related_type = DOMFOCUSIN;
					break;

				case ONBLUR:
					related_type = DOMFOCUSOUT;
					break;

				case DOMFOCUSIN:
					related_type = ONFOCUSIN;
					break;

				case DOMFOCUSOUT:
					related_type = ONFOCUSOUT;
					break;

				case ONLOAD:
					related_type = ONREADYSTATECHANGE;
					break;

				default:
					related_type = DOM_EVENT_NONE;
				}
			}

			if (found_listener)
				break;
		}

		// FIXME: What will happen with ProgressEvents abort/error
		//        which shouldn't bubble but share the same event data
		//        as the window events by the same name?

		/* If the event does not bubble, only consider capturing listener
		   at this element's ancestors. */
		phase = DOM_EVENT_BUBBLES(type) ? ES_PHASE_ANY : ES_PHASE_CAPTURING;

		if (currentTarget)
		{
			currentTarget = DOM_Node::GetEventPathParent(currentTarget, target);

			if (!currentTarget && in_document)
				node = window;
		}
		else
			return FALSE;
	}

	if (nearest_handler)
		*nearest_handler = currentTarget;

	return TRUE;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::HasWindowEventHandler(DOM_EventType type)
{
	if (!HasEventHandlers(type))
		return FALSE;

	if (HasEventHandler(window, type, NULL))
		return TRUE;

	// Might also be event handlers not yet registered on the window because they're
	// specified in an attribute that hasn't been analyzed

	if (FramesDocument* doc = GetFramesDocument())
	{
		HTML_Element* target = doc->GetWindowEventTarget(type);
		BOOL success = target && !target->GetESElement() && target->DOMHasEventHandlerAttribute(this, type);

		/* If a frameset lacks an unload handler, determine if its frame(set)s have any. */
		if (!success && type == ONUNLOAD && target && target->IsMatchingType(Markup::HTE_FRAMESET, NS_HTML))
			if (FramesDocElm *target_fde = FramesDocElm::GetFrmDocElmByHTML(target))
				return target_fde->CheckForUnloadTarget();
		return success;
	}

	return FALSE;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::HasEventHandler(DOM_Object *target, DOM_EventType event, DOM_Object **nearest_handler)
{
	if (HasEventHandlers(event))
	{
		if (target->IsA(DOM_TYPE_NODE))
			if (HTML_Element *target_element = ((DOM_Node *) target)->GetThisElement())
			{
				HTML_Element *nearest_handler_element;
				if (HasEventHandler(target_element, event, &nearest_handler_element))
				{
					if (nearest_handler && nearest_handler_element)
						*nearest_handler = nearest_handler_element->GetESElement();
					return TRUE;
				}
				else
					return FALSE;
			}

		if (DOM_EventTarget *event_target = target->GetEventTarget())
			if (event_target->HasListeners(event, NULL, ES_PHASE_AT_TARGET))
			{
				if (nearest_handler)
					*nearest_handler = target;
				return TRUE;
			}
	}

	return FALSE;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::HasLocalEventHandler(HTML_Element *target, DOM_EventType event)
{
	if (HasEventHandlers(event))
	{
		if (DOM_EventTarget *event_target = DOM_Node::GetEventTargetFromElement(target))
		{
			if (event_target->HasListeners(event, NULL, ES_PHASE_AT_TARGET))
				return TRUE;
		}
		else if (DOM_Node::GetEventTargetElement(target)->DOMHasEventHandlerAttribute(this, event))
			return TRUE;
	}

	return FALSE;
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::SetEventHandler(DOM_Object *node_object, DOM_EventType event, const uni_char *handler, int handler_length)
{
	if (!node_object->IsA(DOM_TYPE_ELEMENT))
		return OpStatus::OK;
	else
	{
		DOM_Element *node = static_cast<DOM_Element *>(node_object);
		HTML_Element *element = node->GetThisElement();

		// Ignore event handlers on unknown elements (such as "LAYER") in HTML documents.
		if (element->GetNsIdx() == NS_IDX_HTML && element->Type() == HE_UNKNOWN)
			return OpStatus::OK;

		if (DOM_Event::IsWindowEventAsBodyAttr(event) && GetFramesDocument() && GetFramesDocument()->GetWindowEventTarget(event) == element)
			node_object = window;

		RETURN_IF_ERROR(node_object->CreateEventTarget());

		DOM_EventTarget *target = node_object->GetEventTarget();
		DOM_EventListener *listener = OP_NEW(DOM_EventListener, ());

		if (!listener || OpStatus::IsMemoryError(listener->SetNativeText(this, event, handler, handler_length, NULL)))
		{
			OP_DELETE(listener);
			return OpStatus::ERR_NO_MEMORY;
		}

#ifdef ECMASCRIPT_DEBUGGER
		// If debugging is enabled we store the caller which registered the event listener.
		ES_Thread *thread = GetCurrentScriptThread();
		if (thread)
		{
			ES_Context *context = thread->GetContext();
			if (context && ES_Runtime::IsContextDebugged(context))
			{
				ES_Runtime::CallerInformation call_info;
				OP_STATUS status = ES_Runtime::GetCallerInformation(context, call_info);
				if (OpStatus::IsSuccess(status))
					listener->SetCallerInformation(call_info.script_guid, call_info.line_no);
			}
		}
#endif // ECMASCRIPT_DEBUGGER

#ifdef SVG_SUPPORT
		// For eventhandler in svg attributes, the return value doesn't control propagation, see bug 190273
		if (element->GetNsType() == NS_SVG)
			listener->SetPropagationControlledByReturnValue(FALSE);
#endif // SVG_SUPPORT

		target->AddListener(listener);
		return OpStatus::OK;
	}
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::ClearEventHandler(DOM_Object *node_object, DOM_EventType event)
{
	if (node_object->IsA(DOM_TYPE_ELEMENT))
	{
		DOM_Element *node = static_cast<DOM_Element *>(node_object);
		HTML_Element *element = node->GetThisElement();

		if (DOM_Event::IsWindowEventAsBodyAttr(event) && GetFramesDocument() && GetFramesDocument()->GetWindowEventTarget(event) == element)
			node_object = window;

		DOM_EventTarget *target = node_object->GetEventTarget();

		if (target)
		{
			DOM_EventListener listener;

			if (OpStatus::IsMemoryError(listener.SetNative(this, event, NULL, FALSE, TRUE, NULL, NULL)))
				return OpStatus::ERR_NO_MEMORY;

			target->RemoveListener(&listener);
		}
	}

	return OpStatus::OK;
}

/* virtual */
void
DOM_EnvironmentImpl::AddEventHandler(DOM_EventType type)
{
	unsigned short *counter = GetEventHandlersCounter(type);
	if (counter)
		if (++*counter == 0)
			--*counter;

#ifdef RESERVED_REGIONS
	if (frames_document && FramesDocument::IsReservedRegionEvent(type))
		frames_document->SignalReservedRegionChange();
#endif // RESERVED_REGIONS

	/* The underlying frames structure needs to be notified
	   if an unload handler is added or removed; signal it here
	   directly. */
	if (type == ONUNLOAD && frames_document)
		if (FramesDocElm* fde = frames_document->GetDocManager()->GetFrame())
			fde->UpdateUnloadTarget(TRUE);
}

/* virtual */
void
DOM_EnvironmentImpl::RemoveEventHandler(DOM_EventType type)
{
	unsigned short *counter = GetEventHandlersCounter(type);
	if (counter)
		if (*counter > 0)
			--*counter;

#ifdef RESERVED_REGIONS
	if (frames_document && FramesDocument::IsReservedRegionEvent(type))
		frames_document->SignalReservedRegionChange();
#endif // RESERVED_REGIONS

	if (type == ONUNLOAD && frames_document)
		if (FramesDocElm* fde = frames_document->GetDocManager()->GetFrame())
			fde->UpdateUnloadTarget(FALSE);
}

DOM_EnvironmentImpl::EventData::EventData()
	: type(DOM_EVENT_NONE),
	  type_custom(NULL),
	  target(NULL),
	  modifiers(0),
	  detail(0),
	  screenX(0),
	  screenY(0),
	  offsetX(0),
	  offsetY(0),
	  clientX(0),
	  clientY(0),
	  button(0),
	  might_be_click(FALSE),
	  has_keyboard_origin(FALSE),
	  relatedTarget(NULL),
	  key_code(OP_KEY_INVALID),
	  key_value(NULL),
	  key_repeat(FALSE),
	  key_location(OpKey::LOCATION_STANDARD),
	  key_data(0),
	  key_event_data(NULL),
#ifdef PROGRESS_EVENTS_SUPPORT
	  progressEvent(0),
#endif // PROGRESS_EVENTS_SUPPORT
	  old_fragment(NULL),
	  new_fragment(NULL),
#ifdef PAGED_MEDIA_SUPPORT
	  current_page(0),
	  page_count(1),
#endif // PAGED_MEDIA_SUPPORT
	  id(0),
	  windowEvent(FALSE)
{
}

/* virtual */
OP_BOOLEAN
DOM_EnvironmentImpl::HandleEvent(const EventData &data, ES_Thread *interrupt_thread, ES_Thread **event_thread)
{
	HTML_Element *targetElement = data.target;

#ifdef TOUCH_EVENTS_SUPPORT
	/*
	 * A sequence of touch events is delivered to the element that received the
	 * touchstart event, regardless of the current location of the touch.
	 */
	bool is_touch_event = (data.type == TOUCHMOVE || data.type == TOUCHEND || data.type == TOUCHCANCEL);
	if ((data.type == TOUCHSTART || is_touch_event) && !active_touches)
		return OpBoolean::IS_FALSE;
	else if (is_touch_event && !(targetElement = GetTouchTarget(data.button)))
		return OpBoolean::IS_FALSE;
#endif // !TOUCH_EVENTS_SUPPORT

	OP_ASSERT(targetElement);

	BOOL handle_event = FALSE, force_handle_event = force_next_event;
	force_next_event = FALSE;

	DOM_EventType realtype = data.type;
	if (realtype == ONMOUSEWHEELH || realtype == ONMOUSEWHEELV)
		realtype = ONMOUSEWHEEL;

	if (realtype == DOM_EVENT_CUSTOM || (data.windowEvent ? HasWindowEventHandler(realtype) : HasEventHandler(targetElement, realtype)))
		handle_event = TRUE;
	else if (realtype == ONMOUSEOVER && HasEventHandler(data.relatedTarget, ONMOUSEOUT))
		handle_event = TRUE;
	else if (realtype == ONMOUSEENTER && HasEventHandler(data.relatedTarget, ONMOUSELEAVE))
		handle_event = TRUE;
	else if (realtype == ONMOUSEUP && HasEventHandler(data.target, ONMOUSEDOWN))
		handle_event = TRUE;
	else if (realtype == ONLOAD && data.windowEvent)
	{
		if (HLDocProfile *hld_profile = frames_document->GetHLDocProfile())
			if (!hld_profile->GetESLoadManager()->IsFinished())
				force_handle_event = TRUE;
	}
	else if (realtype == ONKEYUP && HasEventHandler(data.target, ONKEYDOWN))
		handle_event = TRUE;
	else if (realtype == ONKEYPRESS && (HasEventHandler(data.target, ONKEYDOWN) || HasEventHandler(data.target, ONKEYUP)))
		handle_event = TRUE;
	else if (realtype == TEXTINPUT && (HasEventHandler(data.target, ONKEYDOWN) || HasEventHandler(data.target, ONKEYPRESS)))
		handle_event = TRUE;
#ifdef TOUCH_EVENTS_SUPPORT
	else if (realtype == TOUCHSTART && (HasEventHandler(targetElement, TOUCHMOVE) || HasEventHandler(targetElement, TOUCHEND)))
		handle_event = TRUE;
	else if (realtype == TOUCHMOVE || realtype == TOUCHEND)
		handle_event = TRUE;
#endif // TOUCH_EVENTS_SUPPORT

#ifdef USER_JAVASCRIPT
	if (user_js_manager && user_js_manager->GetIsEnabled())
	{
#ifdef USER_JAVASCRIPT_ADVANCED
		if (realtype == ONLOAD && data.windowEvent)
			RETURN_IF_ERROR(user_js_manager->RunScripts(TRUE));
#endif // USER_JAVASCRIPT_ADVANCED

		if (user_js_manager->GetIsActive())
			force_handle_event = TRUE;
		else
		{
			OP_BOOLEAN has_userjs_listener = user_js_manager->HasBeforeOrAfterEvent(realtype, data.type_custom);
			RETURN_IF_ERROR(has_userjs_listener);

			if (has_userjs_listener == OpBoolean::IS_TRUE)
				force_handle_event = TRUE;
		}
	}
#endif // USER_JAVASCRIPT

	DOM_Event *event = NULL;
	DOM_Object *target = NULL;
	DOM_Node *realTarget = NULL;
	DOM_Node *relatedTarget = NULL;
	DOM_Object *dispatch_target = NULL;

	OP_BOOLEAN result = OpBoolean::IS_FALSE;

	HTML_Element *relatedTargetElement = data.relatedTarget;
	HTML_Element *realTargetElement = NULL;

	if (targetElement->GetInserted() == HE_INSERTED_BY_LAYOUT || targetElement->IsText())
	{
		realTargetElement = targetElement;
		targetElement = DOM_Node::GetActualEventTarget(targetElement);
	}

	if (relatedTargetElement && (relatedTargetElement->GetInserted() == HE_INSERTED_BY_LAYOUT || relatedTargetElement->IsText()))
		relatedTargetElement = DOM_Node::GetActualEventTarget(relatedTargetElement);

	if (data.windowEvent || DOM_Event::IsWindowEventAsBodyAttr(realtype) && targetElement->Type() == HE_DOC_ROOT)
	{
		DOM_Node *target_node;

		/* Need to construct the node for the event target, since it will then register any
		   attribute event listeners it has on the window. See CORE-35066 for why the event
		   target may not exist */
		if (HTML_Element *eventTargetElement = frames_document->GetWindowEventTarget(realtype))
			RETURN_IF_ERROR(ConstructNode(target_node, eventTargetElement, NULL));

		if (data.windowEvent)
		{
			dispatch_target = window;
			target = document;

			OP_ASSERT((handle_event && target) || !handle_event);
		}
	}

	if (handle_event || force_handle_event)
	{
		if (!data.windowEvent)
		{
			DOM_Node *target_node;

			RETURN_IF_ERROR(ConstructNode(target_node, targetElement, NULL));

			target = target_node;

			if (relatedTargetElement)
				RETURN_IF_ERROR(ConstructNode(relatedTarget, relatedTargetElement, NULL));

			if (realTargetElement)
				RETURN_IF_ERROR(ConstructNode(realTarget, realTargetElement, NULL));
		}

		switch (realtype)
		{
		case ONCLICK:
		case ONMOUSEDOWN:
		case ONMOUSEUP:
		case ONMOUSEOVER:
		case ONMOUSEENTER:
		case ONMOUSEMOVE:
		case ONMOUSEOUT:
		case ONMOUSELEAVE:
		case ONMOUSEWHEEL:
		case ONSUBMIT:
		case ONDBLCLICK:
		case ONCONTEXTMENU:
		{
			int button;
			if (data.button == MOUSE_BUTTON_1)
				button = 0;
			else if (data.button == MOUSE_BUTTON_2)
				button = 2;
			else
				button = 1;

			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_MouseEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::MOUSEEVENT_PROTOTYPE), "MouseEvent"));
			((DOM_UIEvent *) event)->InitUIEvent(window, data.detail);
			((DOM_MouseEvent *) event)->InitMouseEvent(data.screenX, data.screenY,
			                                           data.clientX, data.clientY,
			                                           data.calculate_offset_lazily,
			                                           data.offsetX, data.offsetY,
			                                           data.modifiers,
			                                           button, data.might_be_click,
			                                           data.has_keyboard_origin, relatedTarget);
			break;
		}

		case ONKEYPRESS:
		case ONKEYDOWN:
		case ONKEYUP:
		{
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_KeyboardEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::KEYBOARDEVENT_PROTOTYPE), "KeyboardEvent"));
			DOM_KeyboardEvent *key_event = static_cast<DOM_KeyboardEvent *>(event);
			RETURN_IF_ERROR(key_event->InitKeyboardEvent(data.key_code, data.key_value, data.modifiers, data.key_repeat, data.key_location, data.key_data, data.key_event_data));
			break;
		}
		case TEXTINPUT:
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_TextEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::TEXTEVENT_PROTOTYPE), "TextEvent"));
			RETURN_IF_ERROR(static_cast<DOM_TextEvent *>(event)->InitTextEvent(data.key_value));
			break;

		case ONFOCUSIN:
		case ONFOCUSOUT:
		case DOMFOCUSIN:
		case DOMFOCUSOUT:
		case DOMACTIVATE:
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_UIEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::UIEVENT_PROTOTYPE), "UIEvent"));
			static_cast<DOM_UIEvent *>(event)->InitUIEvent(window, data.detail);
			break;

#ifdef TOUCH_EVENTS_SUPPORT
		case TOUCHSTART:
		case TOUCHMOVE:
		case TOUCHEND:
		case TOUCHCANCEL:
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_TouchEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::TOUCHEVENT_PROTOTYPE), "TouchEvent"));
			static_cast<DOM_UIEvent *>(event)->InitUIEvent(window, data.detail);
			RETURN_IF_ERROR(static_cast<DOM_TouchEvent*>(event)->InitTouchEvent(data.clientX, data.clientY, data.offsetX, data.offsetY, data.screenX, data.screenY, data.modifiers, data.button, target, realtype, data.radius, active_touches, runtime, data.user_data));
			break;
#endif // TOUCH_EVENTS_SUPPORT

#ifdef SVG_DOM
		case ENDEVENT:
		case BEGINEVENT:
		case REPEATEVENT:
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_TimeEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::TIMEEVENT_PROTOTYPE), "TimeEvent"));
			((DOM_TimeEvent *) event)->InitTimeEvent(window, data.detail);
			break;
#endif // SVG_DOM

#ifdef CSS_TRANSITIONS
		case WEBKITTRANSITIONEND:
		case OTRANSITIONEND:
		case TRANSITIONEND:
		{
			const char* typestr = "TransitionEvent";
			DOM_Runtime::Prototype prototype = DOM_Runtime::TRANSITIONEVENT_PROTOTYPE;

			if (realtype != TRANSITIONEND)
				if (realtype == OTRANSITIONEND)
				{
					typestr = "OTransitionEvent";
					prototype = DOM_Runtime::OTRANSITIONEVENT_PROTOTYPE;
				}
				else
				{
					typestr = "WebKitTransitionEvent";
					prototype = DOM_Runtime::WEBKITTRANSITIONEVENT_PROTOTYPE;
				}

			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_TransitionEvent, (data.elapsed_time, data.css_property)), runtime, runtime->GetPrototype(prototype), typestr));
			break;
		}
#endif // CSS_TRANSITIONS
#ifdef CSS_ANIMATIONS
		case ANIMATIONSTART:
		case ANIMATIONEND:
		case ANIMATIONITERATION:
			RETURN_IF_ERROR(DOM_AnimationEvent::Make(event, data.elapsed_time, data.animation_name, runtime));
			break;
#endif // CSS_ANIMATIONS

		case ONHASHCHANGE:
			RETURN_IF_ERROR(DOM_HashChangeEvent::Make(event, data.old_fragment, data.new_fragment, runtime));
			break;

		case ONPOPSTATE:
			RETURN_IF_ERROR(DOM_PopStateEvent::Make(event, runtime));
			break;

#ifdef PAGED_MEDIA_SUPPORT
		case ONPAGECHANGE:
			RETURN_IF_ERROR(DOM_PageEvent::Make(event, runtime, data.current_page, data.page_count));
			break;
#endif // PAGED_MEDIA_SUPPORT

#ifdef DRAG_SUPPORT
		case ONDRAGSTART:
		case ONDRAGENTER:
		case ONDRAGLEAVE:
		case ONDRAGOVER:
		case ONDROP:
		case ONDRAG:
		case ONDRAGEND:
			int button;
			if (data.button == MOUSE_BUTTON_1)
				button = 0;
			else if (data.button == MOUSE_BUTTON_2)
				button = 2;
			else
				button = 1;
			RETURN_IF_ERROR(DOM_DragEvent::Make(event, runtime, data.type));
			static_cast<DOM_UIEvent *>(event)->InitUIEvent(window, data.detail);
			static_cast<DOM_MouseEvent *>(event)->InitMouseEvent(data.screenX, data.screenY,
			                                           data.clientX, data.clientY,
			                                           data.calculate_offset_lazily,
			                                           data.offsetX, data.offsetY,
			                                           data.modifiers,
			                                           button, data.might_be_click,
			                                           data.has_keyboard_origin, relatedTarget);
			break;
#endif // DRAG_SUPPORT


#ifdef USE_OP_CLIPBOARD
		case ONCUT:
		case ONCOPY:
		case ONPASTE:
			RETURN_IF_ERROR(DOM_ClipboardEvent::Make(event, runtime, data.id));
			break;
#endif // USE_OP_CLIPBOARD

		default:
#ifdef PROGRESS_EVENTS_SUPPORT
			if (data.progressEvent)
			{
				RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_ProgressEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::PROGRESSEVENT_PROTOTYPE), "ProgressEvent"));
				((DOM_ProgressEvent *) event)->InitProgressEvent(data.lengthComputable, data.loaded, data.total);
				break;
			}
#endif // PROGRESS_EVENTS_SUPPORT

#if defined JS_PLUGIN_SUPPORT && defined DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
			if (realtype == DOM_EVENT_CUSTOM && data.type_custom)
			{
				// Allow a jsplugin to use a custom prototype for the event object so that
				// it can add properties.
				ES_Value val;
				BOOL cachable;
				const char* class_name = NULL;
				if (GetJSPluginContext() && GetJSPluginContext()->GetEvent(data.type_custom, &val, &class_name, &cachable) == GET_SUCCESS && val.type == VALUE_OBJECT)
				{
					RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), runtime, val.value.object, class_name ? class_name : "CustomEvent"));
					break;	//do not create default DOM_Event object
				}
			}
#endif // JS_PLUGIN_SUPPORT && DOM_JSPLUGIN_CUSTOM_EVENT_HOOK

			// A normal generic Event with no extra data or information
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));
		}

		if (!event)
			return OpStatus::ERR_NO_MEMORY;

		event->InitEvent(data.type, target, realTarget, dispatch_target);

		if (interrupt_thread)
			event->SetSynthetic();

		if (data.windowEvent)
			event->SetWindowEvent();

		if (realtype == DOM_EVENT_CUSTOM)
			if (OpStatus::IsMemoryError(event->SetType(data.type_custom)))
				return OpStatus::ERR_NO_MEMORY;

		if (realtype == ONLOAD && data.windowEvent)
		{
			event->SetSignalDocumentFinished();
			++document_finished_signals;
		}

		RETURN_IF_ERROR(result = SendEvent(event, interrupt_thread, event_thread));

		if (realtype == ONUNLOAD)
			frames_document->SetCompatibleHistoryNavigationNeeded();
	}

	if (OpStatus::IsSuccess(result))
	{
		DOM_EventType nexttypes[2] = {DOM_EVENT_NONE, DOM_EVENT_NONE}; /* ARRAY OK joaoe 2010-07-13 */

		switch (realtype)
		{
		case ONFOCUS:
			nexttypes[0] = DOMFOCUSIN;
			nexttypes[1] = ONFOCUSIN;
			break;

		case ONBLUR:
			nexttypes[0] = DOMFOCUSOUT;
			nexttypes[1] = ONFOCUSOUT;
			break;

		case ONLOAD:
			if (data.windowEvent)
				nexttypes[0] = ONREADYSTATECHANGE;
			break;

#ifdef CSS_TRANSITIONS
		case TRANSITIONEND:
			/* If there was no event handler registered for 'transitionEnd', fire
			   'otransitionend' instead. We support the relevant properties with a -o-
			   prefix, so we need to support the prefixed event as well. */
			if (!handle_event)
				nexttypes[0] = OTRANSITIONEND;
			break;

		case OTRANSITIONEND:
			/* If there was no event handler registered for 'otransitionend' either, fire
			   'webkitTransitionEnd'. Since we support the relevant CSS properties with a
			   -webkit- prefix, some sites may be tricked into thinking that we are
			   WebKit, and listen for 'webkitTransitionEnd'. */
			if (!handle_event)
				nexttypes[0] = WEBKITTRANSITIONEND;
			break;
#endif // CSS_TRANSITIONS
		}

		for (unsigned index = 0; index < ARRAY_SIZE(nexttypes) && nexttypes[index] != DOM_EVENT_NONE; index++)
		{
			EventData data0(data);

			data0.type = nexttypes[index];

			if (nexttypes[index] == ONREADYSTATECHANGE)
			{
				// onreadystatechange fires only on document.
				data0.target = frames_document->GetLogicalDocument()->GetRoot();

				if (!data0.target)
					continue;

				data0.windowEvent = FALSE;
			}

			OP_BOOLEAN result0 = HandleEvent(data0, interrupt_thread);

			if (OpBoolean::IsError(result0) || result0 == OpBoolean::IS_TRUE)
				result = result0;
		}
	}

	return result;
}

/* virtual */
OP_BOOLEAN DOM_EnvironmentImpl::HandleError(ES_Thread *thread_with_error, const uni_char *message, const uni_char *resource_url, int resource_line)
{
	// Don't trigger onerror if the error occurred in an onerror handler
	ES_Thread* involved_thread = thread_with_error;
	while (involved_thread)
	{
		if (involved_thread->Type() == ES_THREAD_EVENT)
		{
			DOM_EventType event_type;
			event_type = static_cast<DOM_EventThread *>(involved_thread)->GetEventType();
			if (event_type == ONERROR) // Should make a difference between window.onerror and for instance image.onerror
				return OpBoolean::IS_FALSE;
		}

		involved_thread = involved_thread->GetInterruptedThread();
	}

	if (HasEventHandler(window, ONERROR))
	{
		DOM_Event *event = NULL;
		// TODO: Input the resource url and resource line in the event object
		RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_ErrorEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "ErrorEvent"));
		RETURN_IF_ERROR(((DOM_ErrorEvent *) event)->InitErrorEvent(message, resource_url, resource_line));
		event->InitEvent(ONERROR, window, window);
		event->SetWindowEvent();
		return SendEvent(event, NULL);
	}
	else
		return OpBoolean::IS_FALSE;
}

#ifdef MEDIA_HTML_SUPPORT
/* virtual */ OP_BOOLEAN
DOM_EnvironmentImpl::HandleTextTrackEvent(DOM_Object* target_object, DOM_EventType event_type, MediaTrack* affected_track /* = NULL */)
{
	OP_ASSERT(target_object);
	OP_ASSERT(event_type == MEDIACUECHANGE ||
			  event_type == MEDIACUEENTER ||
			  event_type == MEDIACUEEXIT ||
			  event_type == MEDIAADDTRACK ||
			  event_type == MEDIAREMOVETRACK);

	if (!HasEventHandler(target_object, event_type))
		return OpBoolean::IS_FALSE;

	DOM_Event *event = NULL;

	if (event_type == MEDIAADDTRACK || event_type == MEDIAREMOVETRACK)
	{
		RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_TrackEvent, ()), runtime, runtime->GetPrototype(DOM_Runtime::TRACKEVENT_PROTOTYPE), "TrackEvent"));

		ES_Object* native_affected_track = NULL;
		if (affected_track)
		{
			DOM_Object* domtrack = affected_track->GetDOMObject();
			if (!domtrack)
			{
				DOM_TextTrack* domtexttrack;
				RETURN_IF_ERROR(DOM_TextTrack::Make(domtexttrack, this, affected_track));

				affected_track->SetDOMObject(domtexttrack);
				domtrack = domtexttrack;
			}

			native_affected_track = domtrack->GetNativeObject();
		}

		static_cast<DOM_TrackEvent*>(event)->SetTrack(native_affected_track);
	}
	else
		RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(event = OP_NEW(DOM_Event, ()), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "Event"));

	event->InitEvent(event_type, target_object);

	return SendEvent(event);
}
#endif // MEDIA_HTML_SUPPORT

#if defined(SELFTEST) && defined(APPLICATION_CACHE_SUPPORT)
/* virtual */ ES_Object*
DOM_EnvironmentImpl::GetApplicationCacheObject()
{
	ES_Value application_cache_object;
	if (window->GetPrivate(DOM_PRIVATE_applicationCache, &application_cache_object) == OpBoolean::IS_TRUE)
		return application_cache_object.value.object;
	return NULL;
}
#endif // SELFTEST && APPLICATION_CACHE_SUPPORT

#ifdef DRAG_SUPPORT
class DOM_DragEventThread : public DOM_EventThread
{
protected:
	virtual OP_STATUS Signal(ES_ThreadSignal signal);
};

/* virtual */
OP_STATUS
DOM_DragEventThread::Signal(ES_ThreadSignal signal)
{
	OP_STATUS status = DOM_EventThread::Signal(signal);

	if (!g_drag_manager->IsDragging())
		return status;

	FramesDocument* frm_doc = event->GetFramesDocument();
	BOOL performed_default = has_performed_default || (signal == ES_SIGNAL_CANCELLED && frm_doc && frm_doc->GetHLDocProfile());

	/* When the event's default action couldn't be performed (it happans only in rare situation when e.g. the document gets deleted
	in a meanwhile) do the clean up (not the full default action but just a clean up).
	*/
	if (signal == ES_SIGNAL_CANCELLED && !performed_default)
	{
		DOM_EventType event_type = event->GetKnownType();
		BOOL canceled = event->GetPreventDefault();
		if ((event_type == ONDRAG && !canceled) || event_type == ONDRAGENTER || event_type == ONDRAGOVER)
			g_drag_manager->MoveToNextSequence();
		else if (event_type == ONDROP || event_type == ONDRAGEND || (event_type == ONDRAG && canceled))
		{
			// The drag should ended so notify about its end and clean up.
			if (event_type == ONDROP)
			{
				if (!canceled)
					g_drag_manager->GetDragObject()->SetDropType(DROP_NONE);
			}

			g_drag_manager->StopDrag(event_type == ONDRAG);
		}
	}

	return status;
}
#endif // DRAG_SUPPORT

OP_BOOLEAN
DOM_EnvironmentImpl::SendEvent(DOM_Event *event, ES_Thread *interrupt_thread, ES_Thread **event_thread_ptr)
{
	/* Event delivery is performed by a thread run by the event's scheduler. It
	   being the scheduler of the event's runtime -- the environment's
	   scheduler may belong to another runtime (e.g., an environment containing
	   subordinate User JS runtimes.) However, if the event's runtime is in
	   a detached state, the scheduler will not be present and the event
	   cannot be delivered. */
	ES_ThreadScheduler *thread_scheduler = event->GetRuntime()->GetESScheduler();
	if (!thread_scheduler)
		return OpBoolean::IS_FALSE;

#ifdef DRAG_SUPPORT
	/* In case the event to be sent is a drag event make a create the specialized thread type for it.
	 The thread will take care of cleaning up when the event's default action couldn't be performed
	 e.g. due to the deletion of the document it should be fired in.
	*/
	DOM_EventType type = event->GetKnownType();
	BOOL dnd_event = type == ONDRAGSTART || type == ONDRAG || type == ONDRAGENTER ||
	                 type == ONDRAGOVER || type == ONDRAGLEAVE || type == ONDROP ||
	                 type == ONDRAGEND;
#endif // DRAG_SUPPORT

	DOM_EventThread *event_thread =
#ifdef DRAG_SUPPORT
	dnd_event ? OP_NEW(DOM_DragEventThread, ()) :
#endif // DRAG_SUPPORT
	OP_NEW(DOM_EventThread, ());

	if (!event_thread || OpStatus::IsMemoryError(event_thread->InitEventThread(event, this)))
	{
		OP_DELETE(event_thread);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_BOOLEAN result = thread_scheduler->AddRunnable(event_thread, interrupt_thread);
#ifdef USER_JAVASCRIPT
	if (result == OpBoolean::IS_TRUE && user_js_manager && user_js_manager->GetIsEnabled())
		RETURN_IF_MEMORY_ERROR(user_js_manager->BeforeEvent(event, event_thread));
#endif // USER_JAVASCRIPT

	if (result == OpBoolean::IS_TRUE && event_thread_ptr)
		*event_thread_ptr = event_thread;

	return result;
}

OP_STATUS
DOM_EnvironmentImpl::SignalDocumentFinished()
{
	if (document_finished_signals-- == 1 && frames_document)
	{
		RETURN_IF_ERROR(frames_document->DOMSignalDocumentFinished());
		is_loading = FALSE;
	}

	return OpStatus::OK;
}

unsigned short *
DOM_EnvironmentImpl::GetEventHandlersCounter(DOM_EventType type)
{
#ifdef DOM2_MUTATION_EVENTS
	/* This information, on the other hand, is still reliable, because
	   "delayed event handler registration" only affect event handler
	   attributes, and there are no such attributes for registering
	   mutation event handlers. */
	if (type >= DOMSUBTREEMODIFIED && type <= DOMCHARACTERDATAMODIFIED)
		return &dom2_mutation_event_handlers_counter[type - DOMSUBTREEMODIFIED];
#endif // DOM2_MUTATION_EVENTS

	/* With delayed event handler registration, this information is
	   not reliable.  Should probably fix that some day. */
	if (type >= ONMOUSEOVER && type <= ONMOUSELEAVE)
		return &mouse_movement_event_handlers_counter[type - ONMOUSEOVER];

#ifdef TOUCH_EVENTS_SUPPORT
	if (type >= TOUCHSTART && type <= TOUCHCANCEL)
		return &touch_event_handlers_counter[type - TOUCHSTART];
#endif

	return NULL;
}


/*=== Notifications ===*/

/* virtual */
void
DOM_EnvironmentImpl::NewDocRootElement(HTML_Element *element)
{
	OP_ASSERT(element->Type() == HE_DOC_ROOT);
	if (document)
		document->SetDocRootElement(element);

	if (window)
		window->SetDocRootElement(element);
}


/* virtual */
OP_STATUS
DOM_EnvironmentImpl::NewRootElement(HTML_Element *element)
{
	if (IsEnabled() && document)
	{
		DOM_Node *node;

		if (element)
			RETURN_IF_ERROR(ConstructNode(node, element, document));
		else
			node = NULL;

		RETURN_IF_ERROR(document->SetRootElement((DOM_Element *) node));
	}

	return OpStatus::OK;
}

OP_STATUS
DOM_Environment::ElementInsertedIntoDocument(HTML_Element *element, BOOL recurse, BOOL hidden)
{
	return ElementInserted(element);
}

void
DOM_EnvironmentImpl::SetTrackCollectionChanges(BOOL f)
{
	/* If back on again, invalidate the collections. */
	if (!signal_collection_changes && f)
	{
		node_collection_tracker.InvalidateCollections();
		element_collection_tracker.InvalidateCollections();
	}

	signal_collection_changes = f;
}


void
DOM_EnvironmentImpl::MoveCollection(DOM_NodeCollection* collection, HTML_Element* old_tree_root, HTML_Element* new_tree_root)
{
	collection->Out();
	if (collection->IsElementCollection())
		AddElementCollection(collection, new_tree_root);
	else
		AddNodeCollection(collection, new_tree_root);
}

/*static*/ OP_STATUS
DOM_EnvironmentImpl::QuerySelectorCache::QueryResult::Make(DOM_EnvironmentImpl::QuerySelectorCache::QueryResult *&query_result, DOM_StaticNodeList *matched_list, const uni_char *query, unsigned data, DOM_Node *root)
{
	const uni_char *query_s = UniSetNewStr(query);
	if (!query_s)
		return OpStatus::ERR_NO_MEMORY;

	query_result = OP_NEW(DOM_EnvironmentImpl::QuerySelectorCache::QueryResult, (matched_list, query_s, data, root));
	if (!query_result)
	{
		OP_DELETEA(query_s);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}


OP_STATUS
DOM_EnvironmentImpl::QuerySelectorCache::Add(DOM_EnvironmentImpl *environment, DOM_StaticNodeList *matched_list, const uni_char *query, unsigned data, DOM_Node *root)
{
	/* Delayed invalidation of cache. */
	if (!is_valid || environment->GetFramesDocument() && environment->GetFramesDocument()->DOMHadRecentLayoutChanges())
		Invalidate(environment);

	QueryResult *cached_query;

	if (entries >= QuerySelectorCache::max_query_results)
	{
		/* Let go of the last result re-used. */
		QueryResult *last = query_results.Last();
		DOM_StaticNodeList *last_result = query_matches.Last();

		last->Out();

		/* if last element in the match list isn't the same, then this entry is invalid. */
		if (last->list == last_result)
			last_result->Out();

		entries--;

		cached_query = last;

		cached_query->root = root;
		cached_query->list = matched_list;
		cached_query->data = data;
		if (cached_query->query)
			OP_DELETEA(cached_query->query);

		cached_query->query = UniSetNewStr(query);
		if (!cached_query->query)
			return OpStatus::ERR_NO_MEMORY;

	}
	else
	    RETURN_IF_ERROR(DOM_EnvironmentImpl::QuerySelectorCache::QueryResult::Make(cached_query, matched_list, query, data, root));

	cached_query->IntoStart(&query_results);
	cached_query->list->IntoStart(&query_matches);
	entries++;

	return OpStatus::OK;
}

BOOL
DOM_EnvironmentImpl::QuerySelectorCache::Find(DOM_EnvironmentImpl *environment, DOM_StaticNodeList *&node_list, const uni_char *query, unsigned data, DOM_Node *root)
{
	if (!is_valid || environment->GetFramesDocument() && environment->GetFramesDocument()->DOMHadRecentLayoutChanges())
	{
		Invalidate(environment);
		return FALSE;
	}

	/* Validate the entries; some of the matched lists may have been unregistered/deleted. */
	DOM_StaticNodeList *matched_result = query_matches.First();
	QueryResult *res = query_results.First();
	QueryResult *match = NULL;

	entries = 0;

	while (res)
	{
		if (res->list != matched_result)
		{
			/* result gone; delete cache record. */
			QueryResult *next = res->Suc();
			res->Out();
			OP_DELETE(res);
			res = next;
		}
		else
		{
			if (!match && res->data == data && res->root == root && uni_str_eq(res->query, query))
				match = res;

			res = res->Suc();
			matched_result = matched_result->Suc();

			entries++;
		}
	}

	if (!match)
		return FALSE;

	node_list = match->list;

	/* Put this most-recently shared collection at the front */
	match->Out();
	match->list->Out();

	match->IntoStart(&query_results);
	match->list->IntoStart(&query_matches);

	return TRUE;
}

/* private */ void
DOM_EnvironmentImpl::QuerySelectorCache::Invalidate(DOM_EnvironmentImpl *environment)
{
	query_results.Clear();
	query_matches.RemoveAll();
	entries = 0;
	if (environment->GetFramesDocument())
		environment->GetFramesDocument()->DOMResetRecentLayoutChanges();
	is_valid = TRUE;
}

void
DOM_EnvironmentImpl::QuerySelectorCache::Reset()
{
	is_valid = FALSE;
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::ElementInserted(HTML_Element *element)
{
	OP_PROBE3(OP_PROBE_DOM_ENVIRONMENTIMPL_ELEMENTINSERTEDINTODOCUMENT);

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (call_mutation_listeners && element->IsIncludedActual())
		if (OpStatus::IsMemoryError(SignalOnAfterInsert(element, current_origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;

#ifdef DOM_EXPENSIVE_DEBUG_CODE
	extern void DOM_VerifyAllNodes();
	DOM_VerifyAllNodes();
#endif // DOM_EXPENSIVE_DEBUG_CODE

	if (signal_collection_changes)
	{
		if (Markup::IsRealElement(element->Type()))
			element_collection_tracker.SignalChange(element, TRUE, FALSE, 0);

		node_collection_tracker.SignalChange(element, TRUE, FALSE, 0);
	}

	query_selector_cache.Reset();

	return status;
}

OP_STATUS
DOM_Environment::ElementRemovedFromDocument(HTML_Element *element, BOOL recurse)
{
	return ElementRemoved(element);
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::ElementRemoved(HTML_Element *element)
{
#ifdef DOM_EXPENSIVE_DEBUG_CODE
	extern void DOM_VerifyAllNodes();
	DOM_VerifyAllNodes();
#endif // DOM_EXPENSIVE_DEBUG_CODE

	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (call_mutation_listeners && element->IsIncludedActual())
		if (OpStatus::IsMemoryError(SignalOnBeforeRemove(element, current_origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;

	if (signal_collection_changes && element->IsIncludedActual())
	{
		if (Markup::IsRealElement(element->Type()))
			element_collection_tracker.SignalChange(element, FALSE, TRUE, 0);

		node_collection_tracker.SignalChange(element, FALSE, TRUE, 0);
	}

	query_selector_cache.Reset();

	HTML_Element *iter = element, *stop = iter->NextSibling();
	while (iter != stop)
	{
		if (DOM_Node *node = static_cast<DOM_Node *>(iter->GetESElement()))
		{
			node->SetIsSignificant();
			break;
		}
		else
			iter = iter->Next();
	}

	if (iter != stop)
	{
		// Need to make sure someone still owns the remaining tree as well.
		if (HTML_Element* rest_elm = element->Parent())
		{
			while (!rest_elm->GetESElement() && rest_elm->Parent())
				rest_elm = rest_elm->Parent();

			if (!rest_elm->GetESElement())
			{
				HTML_Element* best_candidate = NULL;
				if (rest_elm->Type() != HE_DOC_ROOT && rest_elm->IsIncludedActual())
					best_candidate = rest_elm;
				else
				{
					// Search for another subtree owner but make sure to
					// skip the part that is about to be removed.
					while (rest_elm && (rest_elm == element || !rest_elm->GetESElement()))
					{
						if (rest_elm == element)
							rest_elm = rest_elm->NextSiblingActual();
						else
							rest_elm = rest_elm->NextActual();

						if (!best_candidate && rest_elm != element)
							best_candidate = rest_elm;
					}
				}

				if (!(rest_elm && rest_elm->GetESElement()) && best_candidate)
				{
					rest_elm = best_candidate;
					DOM_Object *node;
					if (OpStatus::IsMemoryError(ConstructNode(node, rest_elm)))
						status = OpStatus::ERR_NO_MEMORY;
				}
				// If we didn't find or created a node here there is a risk we will leak
				// some HTML_Elements, depending on external factors.
			}
			if (rest_elm && rest_elm->GetESElement())
				static_cast<DOM_Node *>(rest_elm->GetESElement())->SetIsSignificant();
		}
	}

	return status;
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::ElementCharacterDataChanged(HTML_Element *element,
												 int modification_type,
												 unsigned offset /* = 0*/,
												 unsigned length1 /* = 0 */, unsigned length2 /* = 0 */)
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (call_mutation_listeners && !mutation_listeners.Empty() && element->IsIncludedActual())
	{
		TempBuffer buffer;
		const uni_char *value;

		if (!(value = element->DOMGetContentsString(this, &buffer)))
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsMemoryError(SignalOnAfterValueModified(element, value, (DOM_MutationListener::ValueModificationType) modification_type, offset, length1, length2, current_origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;
	}

	return status;
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::ElementAttributeChanged(HTML_Element *element, const uni_char *name, int ns_idx)
{
	OP_STATUS status = OpStatus::OK;
	OpStatus::Ignore(status);

	if (call_mutation_listeners && !mutation_listeners.Empty() && element->IsIncludedActual())
		if (OpStatus::IsMemoryError(SignalOnAttrModified(element, name, ns_idx, current_origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;

	return status;
}

/* virtual */
void
DOM_EnvironmentImpl::ElementCollectionStatusChanged(HTML_Element *element, unsigned collections)
{
	if (signal_collection_changes)
	{
		if (Markup::IsRealElement(element->Type()))
			element_collection_tracker.SignalChange(element, FALSE, FALSE, collections);

		node_collection_tracker.SignalChange(element, FALSE, FALSE, collections);
	}

	query_selector_cache.Reset();
}

/* virtual */
void
DOM_EnvironmentImpl::OnSelectionUpdated()
{
#ifdef DOM_SELECTION_SUPPORT
	ES_Value return_value;
	int result = JS_Window::getSelection(GetWindow(), NULL, 0, &return_value, NULL);
	if (result == ES_VALUE)
	{
		DOM_WindowSelection *dom_selection = DOM_VALUE2OBJECT(return_value, DOM_WindowSelection);
		dom_selection->OnSelectionUpdated();
	}
#endif // DOM_SELECTION_SUPPORT
}

#ifdef USER_JAVASCRIPT

/*=== User Javascript ===*/

/* virtual */
OP_BOOLEAN
DOM_EnvironmentImpl::HandleScriptElement(HTML_Element *element, ES_Thread *interrupt_thread)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
	{
		DOM_Node *node;
		RETURN_IF_ERROR(ConstructNode(node, element, NULL));
		OP_ASSERT(node->IsA(DOM_TYPE_ELEMENT));

		return user_js_manager->BeforeScriptElement((DOM_Element *) node, interrupt_thread);
	}
	else
		return OpBoolean::IS_FALSE;
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::HandleScriptElementFinished(HTML_Element *element, ES_Thread *script_thread)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
	{
		DOM_Node *node;
		RETURN_IF_ERROR(ConstructNode(node, element, NULL));
		OP_ASSERT(node->IsA(DOM_TYPE_ELEMENT));

		RETURN_IF_ERROR(user_js_manager->AfterScriptElement((DOM_Element *) node, script_thread));
	}
	return OpStatus::OK;
}

/* virtual */
OP_BOOLEAN
DOM_EnvironmentImpl::HandleExternalScriptElement(HTML_Element *element, ES_Thread *interrupt_thread)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
	{
		DOM_Node *node;
		RETURN_IF_ERROR(ConstructNode(node, element, NULL));
		OP_ASSERT(node->IsA(DOM_TYPE_ELEMENT));

		return user_js_manager->BeforeExternalScriptElement((DOM_Element *) node, interrupt_thread);
	}
	else
		return OpBoolean::IS_FALSE;
}

/* virtual */
BOOL
DOM_EnvironmentImpl::IsHandlingScriptElement(HTML_Element *element)
{
	if (user_js_manager && current_origining_runtime)
		return user_js_manager->GetIsHandlingScriptElement(current_origining_runtime, element);
	else
		return FALSE;
}

/* virtual */
OP_BOOLEAN
DOM_EnvironmentImpl::HandleCSS(HTML_Element *element, ES_Thread *interrupt_thread)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
	{
		DOM_Node *node = NULL;
		if (element)
			RETURN_IF_ERROR(ConstructNode(node, element, NULL));
		return user_js_manager->BeforeCSS(node, interrupt_thread);
	}
	else
		return OpBoolean::IS_FALSE;
}

/* virtual */
OP_BOOLEAN
DOM_EnvironmentImpl::HandleCSSFinished(HTML_Element *element, ES_Thread *interrupt_thread)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
	{
		DOM_Node *node = NULL;
		if (element)
			RETURN_IF_ERROR(ConstructNode(node, element, NULL));
		return user_js_manager->AfterCSS(node, interrupt_thread);
	}
	else
		return OpBoolean::IS_FALSE;
}

#ifdef _PLUGIN_SUPPORT_
/* virtual */
OP_BOOLEAN
DOM_EnvironmentImpl::PluginInitializedElement(HTML_Element *element)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
	{
		DOM_Node *node;
		RETURN_IF_ERROR(ConstructNode(node, element, NULL));
		OP_ASSERT(node->IsA(DOM_TYPE_ELEMENT));

		return user_js_manager->PluginInitializedElement((DOM_Element *) node);
	}
	else
		return OpBoolean::IS_FALSE;
}
#endif // _PLUGIN_SUPPORT_

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::HandleJavascriptURL(ES_JavascriptURLThread *thread)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
		return user_js_manager->BeforeJavascriptURL(thread);
	else
		return OpStatus::OK;
}

/* virtual */
OP_STATUS
DOM_EnvironmentImpl::HandleJavascriptURLFinished(ES_JavascriptURLThread *thread)
{
	if (user_js_manager && user_js_manager->GetIsEnabled())
		return user_js_manager->AfterJavascriptURL(thread);
	else
		return OpStatus::OK;
}

/* static */
BOOL
DOM_Environment::GetUserJSEnabled()
{
	return g_pcjs->GetIntegerPref(PrefsCollectionJS::UserJSEnabled);
}

/* static */
unsigned
DOM_Environment::GetUserJSFilesCount()
{
	return DOM_UserJSManager::GetFilesCount();
}

/* static */
const uni_char *
DOM_Environment::GetUserJSFilename(unsigned index)
{
	return DOM_UserJSManager::GetFilename(index);
}

/* static */
OP_STATUS
DOM_Environment::UpdateUserJSFiles()
{
#ifndef USER_JAVASCRIPT_ADVANCED
	OP_STATUS status = DOM_UserJSManager::ReloadScripts();

	if (status == OpStatus::ERR_NO_MEMORY)
		return status;
	else
		return OpStatus::OK;
#else // USER_JAVASCRIPT_ADVANCED
	return OpStatus::OK;
#endif // USER_JAVASCRIPT_ADVANCED
}

#ifdef DOM_BROWSERJS_SUPPORT

/* static */
OP_BOOLEAN
DOM_Environment::CheckBrowserJSSignature(const OpFile &file)
{
	/* Defined in dom/src/userjs/userjsmanager.cpp. */
	extern OP_BOOLEAN DOM_CheckBrowserJSSignature(const OpFile &file);

	return DOM_CheckBrowserJSSignature(file);
}

#endif // DOM_BROWSERJS_SUPPORT

DOM_UserJSManager *
DOM_EnvironmentImpl::GetUserJSManager()
{
	return user_js_manager;
}

#endif // USER_JAVASCRIPT

#ifdef ECMASCRIPT_DEBUGGER

/* virtual */ OP_STATUS
DOM_EnvironmentImpl::GetESRuntimes(OpVector<ES_Runtime> &es_runtimes)
{
	// Add regular ES_Runtime.
	RETURN_IF_ERROR(es_runtimes.Add(GetRuntime()));

#ifdef EXTENSION_SUPPORT
	// Add extension ES_Runtimes.
	if (GetUserJSManager())
		RETURN_IF_ERROR(GetUserJSManager()->GetExtensionESRuntimes(es_runtimes));
#endif // EXTENSION_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	if (DOM_WebWorkerController *wc = GetWorkerController())
		RETURN_IF_ERROR(wc->GetDomainRuntimes(es_runtimes));
#endif // DOM_WEBWORKERS_SUPPORT

	return OpStatus::OK;
}

#endif // ECMASCRIPT_DEBUGGER

/* virtual */
void
DOM_EnvironmentImpl::PrefsListener::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id == OpPrefsCollection::JS && pref == PrefsCollectionJS::EcmaScriptEnabled && owner)
	{
		if (FramesDocument *frames_document = owner->GetFramesDocument())
		{
			/* If a site-preference is in effect, possibly adjust preference value. */
			if (is_host_overridden)
				newvalue = g_pcjs->GetIntegerPref(PrefsCollectionJS::EcmaScriptEnabled, frames_document->GetURL());

			owner->is_es_enabled = DOM_Environment::IsEnabled(frames_document, newvalue > 0);
		}
		else
			owner->is_es_enabled = newvalue > 0;
	}
}

#ifdef PREFS_HOSTOVERRIDE
/* virtual */
void
DOM_EnvironmentImpl::PrefsListener::HostOverrideChanged(OpPrefsCollection::Collections id, const uni_char *hostname)
{
	if (id == OpPrefsCollection::JS && owner && owner->GetFramesDocument())

		/* could match on hostname to filter out lookups, but requires mirroring matching algorithm. */
		if (ServerName *this_host = DOM_EnvironmentImpl::GetHostName(owner->GetFramesDocument()))
		{
			const uni_char *host_name = this_host->UniName();
			is_host_overridden = host_name && g_pcjs->IsHostOverridden(host_name, FALSE) > 0;
		}
}

#endif // PREFS_HOSTOVERRIDE

#ifdef JS_PLUGIN_SUPPORT

/*=== JS plugins ===*/

/* virtual */
JS_Plugin_Context *
DOM_EnvironmentImpl::GetJSPluginContext()
{
	return (window ? window->GetJSPluginContext() : NULL);
}

/* virtual */ void
DOM_EnvironmentImpl::SetJSPluginContext(JS_Plugin_Context *context)
{
	window->SetJSPluginContext(context);
}

#endif // JS_PLUGIN_SUPPORT


#ifdef GADGET_SUPPORT

/*=== Gadget support ===*/

DOM_EnvironmentImpl::GadgetEventData::GadgetEventData()
	:	mode(NULL),
		screen_width(0),
		screen_height(0)
{
}

OP_STATUS
DOM_EnvironmentImpl::HandleGadgetEvent(GadgetEvent event, GadgetEventData *data)
{
	ES_Value value;

	if (window->GetPrivate(DOM_PRIVATE_widget, &value) != OpBoolean::IS_TRUE)
	{
		OP_ASSERT(FALSE); //  Why was the widget object not found?? -- chrispi
		return OpStatus::ERR;
	}

	DOM_Widget* widget = DOM_VALUE2OBJECT(value, DOM_Widget);

#if defined JS_PLUGIN_SUPPORT && defined DOM_JSPLUGIN_CUSTOM_EVENT_HOOK
	if (event == GADGET_EVENT_CUSTOM && data && data->type_custom)
	{
		ES_Value val;
		BOOL cachable;
		const char* class_name = NULL;
		JS_Plugin_Context* jsplugin_context = GetJSPluginContext();
		if (jsplugin_context && jsplugin_context->GetEvent(data->type_custom, &val, &class_name, &cachable) == GET_SUCCESS && val.type == VALUE_OBJECT)
		{
			DOM_Event* evt = OP_NEW(DOM_Event, ());
			if (!evt)
				return OpStatus::ERR_NO_MEMORY;

			//init event with class info here
			RETURN_IF_ERROR(DOM_Object::DOMSetObjectRuntime(evt, GetDOMRuntime(), val.value.object, class_name));
			evt->InitEvent(DOM_EVENT_CUSTOM, widget);
			RETURN_IF_ERROR(evt->SetType(data->type_custom));
			if (!SendEvent(evt))
				return OpStatus::ERR;

			return OpStatus::OK;
		}
	}
#endif // JS_PLUGIN_SUPPORT && DOM_JSPLUGIN_CUSTOM_EVENT_HOOK

	RETURN_IF_ERROR(widget->HandleEvent(event, data));

#ifdef DOM_JIL_API_SUPPORT
	if (window->GetPrivate(DOM_PRIVATE_jilWidget, &value) == OpBoolean::IS_TRUE)
	{
		DOM_HOSTOBJECT_SAFE(jil_widget, value.value.object, DOM_TYPE_JIL_WIDGET, DOM_JILWidget);
		if (jil_widget)
			RETURN_IF_ERROR(jil_widget->HandleEvent(event, data));
	}
#endif // DOM_JIL_API_SUPORT

	return OpStatus::OK;
}

#endif // GADGET_SUPPORT


/*=== External callbacks ===*/

class DOM_Callback
	: public Link
{
public:
	DOM_Callback()
		: impl_v1(NULL),
		  impl_v2(NULL),
		  name(NULL),
		  arguments(NULL)
	{
	}

	~DOM_Callback()
	{
		OP_DELETEA(name);
		OP_DELETEA(arguments);
	}

	DOM_Environment::CallbackImpl impl_v1;
	DOM_Environment::ExtendedCallbackImpl impl_v2;
	char *name, *arguments;
};

class DOM_CallbackFunction
	: public DOM_Object
{
protected:
	DOM_Environment::CallbackImpl impl_v1;
	DOM_Environment::ExtendedCallbackImpl impl_v2;

public:
	DOM_CallbackFunction(DOM_Environment::CallbackImpl impl_v1, DOM_Environment::ExtendedCallbackImpl impl_v2)
		: impl_v1(impl_v1),
		  impl_v2(impl_v2)
	{
	}

	virtual int Call(ES_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
	{
		DOM_Environment::CallbackSecurityInfo security_info;
		((DOM_Runtime *) origining_runtime)->GetDomain(&security_info.domain, &security_info.type, &security_info.port);

		if (impl_v1)
			return impl_v1(argv, argc, return_value, &security_info);
		else
		{
			FramesDocument *doc = origining_runtime->GetFramesDocument();
			OpWindowCommander *win_commander = doc->GetWindow()->GetWindowCommander();
			FramesDocElm *frame = doc->GetDocManager()->GetFrame();
			if (frame)
				doc->GetTopDocument()->SetActiveFrame(frame);
			else
				doc->GetTopDocument()->SetNoActiveFrame();

			return impl_v2(argv, argc, return_value, win_commander, &security_info);
		}
	}
};

/* static */
OP_STATUS
DOM_Environment::AddCallback(CallbackImpl impl, CallbackLocation location, const char *name, const char *arguments)
{
	Head **list;

	if (location == GLOBAL_CALLBACK)
		list = &g_DOM_globalCallbacks;
	else if (location == OPERA_CALLBACK)
		list = &g_DOM_operaCallbacks;
	else
		return OpStatus::OK;

	if (!*list && !(*list = OP_NEW(Head, ())))
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DOM_Callback> callback(OP_NEW(DOM_Callback, ()));
	if (!callback.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(SetStr(callback->name, name));
	RETURN_IF_ERROR(SetStr(callback->arguments, arguments));

	callback->impl_v1 = impl;
	callback->Into(*list);
	callback.release();

	return OpStatus::OK;
}

/* static */
OP_STATUS
DOM_Environment::AddCallback(ExtendedCallbackImpl impl, CallbackLocation location, const char *name, const char *arguments)
{
	Head **list;

	if (location == GLOBAL_CALLBACK)
		list = &g_DOM_globalCallbacks;
	else if (location == OPERA_CALLBACK)
		list = &g_DOM_operaCallbacks;
	else
		return OpStatus::OK;

	if (!*list && !(*list = OP_NEW(Head, ())))
		return OpStatus::ERR_NO_MEMORY;

	OpAutoPtr<DOM_Callback> callback(OP_NEW(DOM_Callback, ()));
	if (!callback.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(SetStr(callback->name, name));
	RETURN_IF_ERROR(SetStr(callback->arguments, arguments));

	callback->impl_v2 = impl;
	callback->Into(*list);
	callback.release();

	return OpStatus::OK;
}

class DOM_JSPopupCallback;

class DOM_JSPopupOpener
	: public OpDocumentListener::JSPopupOpener
{
public:
	virtual void Open();
	virtual void Release();

	DOM_JSPopupCallback *callback;
};

class DOM_JSPopupCallback
	: public OpDocumentListener::JSPopupCallback
{
public:
	DOM_JSPopupCallback()
	{
		openercallback.callback = this;
	}

	virtual ~DOM_JSPopupCallback()
	{
		if (opener)
			opener->GetRuntime()->Unprotect(*opener);
	}

	virtual void Continue(Action action, OpDocumentListener::JSPopupOpener **opener);

	DOM_Object *opener;
	URL url;
	DocumentReferrer refurl;
	OpString target, data;
	BOOL unrequested, refuse, openinbackground, userinitiated;
	DOM_JSPopupOpener openercallback;

	void Open();
};

/* virtual */ void
DOM_JSPopupOpener::Open()
{
	callback->Open();
}

/* virtual */ void
DOM_JSPopupOpener::Release()
{
	OP_DELETE(callback);
}

/* virtual */ void
DOM_JSPopupCallback::Continue(Action action, OpDocumentListener::JSPopupOpener **openerptr)
{
	if (action == POPUP_ACTION_ACCEPT)
		refuse = FALSE;
	else if (action == POPUP_ACTION_REFUSE)
		refuse = TRUE;

	if (!refuse)
	{
		if (openerptr)
			*openerptr = NULL;

		Open();
	}
	else if (openerptr)
		*openerptr = &openercallback;
	else
		OP_DELETE(this);
}

void
DOM_JSPopupCallback::Open()
{
	Window *window;

	BOOL allow_scripts_to_access_opener = TRUE; // Potentially look at the background flag, see bug 212064
	OpStatus::Ignore(JS_Window::OpenPopupWindow(window, opener, url, refurl, target.CStr(), NULL, MAYBE, MAYBE, openinbackground ? YES : NO, allow_scripts_to_access_opener, userinitiated, FALSE));

	if (!data.IsEmpty() && window)
		OpStatus::Ignore(JS_Window::WriteDocumentData(window, data.CStr()));

	OP_DELETE(this);
}

/* static */ OP_STATUS
DOM_Environment::OpenURLWithTarget(URL url, DocumentReferrer& refurl, const uni_char *target, FramesDocument *document, ES_Thread *thread, BOOL user_initiated, BOOL plugin_unrequested_popup)
{
	OpString urlname;

	if (refurl.origin && refurl.origin->in_sandbox)
		return OpStatus::OK;  // Don't let Sandboxed documents open new windows.

	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Hidden, urlname));
	RETURN_IF_ERROR(document->ConstructDOMEnvironment());

	DOM_Object *opener = document->GetDOMEnvironment()->GetWindow();

	DOM_JSPopupCallback *callback = OP_NEW(DOM_JSPopupCallback, ());
	if (!callback || OpStatus::IsMemoryError(callback->target.Set(target)) || !opener->GetRuntime()->Protect(*opener))
	{
		OP_DELETE(callback);
		return OpStatus::ERR_NO_MEMORY;
	}

	callback->opener = opener;
	callback->url = url;
	callback->refurl = refurl;
	callback->unrequested = plugin_unrequested_popup || JS_Window::IsUnrequestedPopup(thread);
	callback->refuse = JS_Window::RefusePopup(document, thread, callback->unrequested);
	callback->openinbackground = JS_Window::OpenInBackground(document, thread);
	callback->userinitiated = user_initiated;

	if (thread)
		thread->SetHasOpenedNewWindow();

	WindowCommander *wc = document->GetWindow()->GetWindowCommander();

	wc->GetDocumentListener()->OnJSPopup(wc, urlname.CStr(), callback->target.CStr(), -1, -1, 0, 0, MAYBE, MAYBE, callback->refuse, callback->unrequested, callback);

	return OpStatus::OK;
}

/* static */ OP_STATUS
DOM_Environment::OpenWindowWithData(const uni_char *data, FramesDocument *document, ES_JavascriptURLThread *thread, BOOL user_initiated)
{
	OpString urlname;

	RETURN_IF_ERROR(document->GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden, urlname));
	RETURN_IF_ERROR(document->ConstructDOMEnvironment());

	DOM_Object *opener = document->GetDOMEnvironment()->GetWindow();

	DOM_JSPopupCallback *callback = OP_NEW(DOM_JSPopupCallback, ());
	if (!callback || OpStatus::IsMemoryError(callback->data.Set(data)) || !opener->GetRuntime()->Protect(*opener))
	{
		OP_DELETE(callback);
		return OpStatus::ERR_NO_MEMORY;
	}

	callback->opener = opener;
	callback->unrequested = JS_Window::IsUnrequestedPopup(thread);
	callback->refuse = JS_Window::RefusePopup(document, thread, callback->unrequested);
	callback->openinbackground = JS_Window::OpenInBackground(document, thread);
	callback->userinitiated = user_initiated;

	if (thread)
		thread->SetHasOpenedNewWindow();

	WindowCommander *wc = document->GetWindow()->GetWindowCommander();

	wc->GetDocumentListener()->OnJSPopup(wc, urlname.CStr(), callback->target.CStr(), -1, -1, 0, 0, MAYBE, MAYBE, callback->refuse, callback->unrequested, callback);

	return OpStatus::OK;
}

/* static */ BOOL
DOM_Environment::IsCalledFromUnrequestedScript(ES_Thread *thread)
{
	if (thread)
		return JS_Window::IsUnrequestedPopup(thread);
	else
		return FALSE;
}

void
DOM_EnvironmentImpl::RegisterCallbacksL(CallbackLocation location, DOM_Object *object)
{
	Head **list;

	if (location == GLOBAL_CALLBACK)
		list = &g_DOM_globalCallbacks;
	else if (location == OPERA_CALLBACK)
		list = &g_DOM_operaCallbacks;
	else
		return;

	if (*list)
		for (DOM_Callback *callback = (DOM_Callback *) (*list)->First();
		     callback;
		     callback = (DOM_Callback *) callback->Suc())
			object->PutFunctionL(callback->name, OP_NEW(DOM_CallbackFunction, (callback->impl_v1, callback->impl_v2)), "Function", callback->arguments);
}


/*=== Document modification tracking ===*/

int
DOM_EnvironmentImpl::GetSerialNr()
{
	return frames_document ? frames_document->GetSerialNr() : 0;
}


/*=== Mutation listeners ===*/

void
DOM_EnvironmentImpl::AddMutationListener(DOM_MutationListener *listener)
{
	listener->Into(&mutation_listeners);
}

void
DOM_EnvironmentImpl::RemoveMutationListener(DOM_MutationListener *listener)
{
	listener->Out();
}

OP_STATUS
DOM_EnvironmentImpl::SignalOnAfterInsert(HTML_Element *child, DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	Head temporary;
	temporary.Append(&mutation_listeners);

	while (DOM_MutationListener *listener = static_cast<DOM_MutationListener *>(temporary.First()))
	{
		listener->Out();
		listener->Into(&mutation_listeners);

		if (OpStatus::IsMemoryError(listener->OnAfterInsert(child, origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;
	}

	return status;
}

OP_STATUS
DOM_EnvironmentImpl::SignalOnBeforeRemove(HTML_Element *child, DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	Head temporary;
	temporary.Append(&mutation_listeners);

	while (DOM_MutationListener *listener = static_cast<DOM_MutationListener *>(temporary.First()))
	{
		listener->Out();
		listener->Into(&mutation_listeners);

		if (OpStatus::IsMemoryError(listener->OnBeforeRemove(child, origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;
	}

	return status;
}

OP_STATUS
DOM_EnvironmentImpl::SignalOnAfterValueModified(HTML_Element *element, const uni_char *new_value, DOM_MutationListener::ValueModificationType type, unsigned offset, unsigned length1, unsigned length2, DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	Head temporary;
	temporary.Append(&mutation_listeners);

	while (DOM_MutationListener *listener = static_cast<DOM_MutationListener *>(temporary.First()))
	{
		listener->Out();
		listener->Into(&mutation_listeners);

		if (OpStatus::IsMemoryError(listener->OnAfterValueModified(element, new_value, type, offset, length1, length2, origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;
	}

	return status;
}

OP_STATUS
DOM_EnvironmentImpl::SignalOnAttrModified(HTML_Element *element, const uni_char *name, int ns_idx, DOM_Runtime *origining_runtime)
{
	OP_STATUS status = OpStatus::OK;

	Head temporary;
	temporary.Append(&mutation_listeners);

	while (DOM_MutationListener *listener = static_cast<DOM_MutationListener *>(temporary.First()))
	{
		listener->Out();
		listener->Into(&mutation_listeners);

		if (OpStatus::IsMemoryError(listener->OnAttrModified(element, name, ns_idx, origining_runtime)))
			status = OpStatus::ERR_NO_MEMORY;
	}

	return status;
}


#ifdef MEDIA_HTML_SUPPORT

/*=== Media support ===*/

void
DOM_EnvironmentImpl::AddMediaElement(DOM_HTMLMediaElement *media)
{
	media->Into(&media_elements);
}

#endif // MEDIA_HTML_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT

/*=== Web Workers support ===*/

DOM_WebWorkerController*
DOM_EnvironmentImpl::GetWorkerController()
{
	return web_workers;
}

#endif // DOM_WEBWORKERS_SUPPORT

#if defined(DOM_CROSSDOCUMENT_MESSAGING_SUPPORT)

/*=== Cross-Document Messaging support ===*/

void
DOM_EnvironmentImpl::AddMessagePort(DOM_MessagePort *port)
{
	port->Out();
	port->Into(&message_ports);
}

#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT

/*=== EventSource support ===*/

void
DOM_EnvironmentImpl::AddEventSource(DOM_EventSource *event_source)
{
	OP_ASSERT(!event_source->InList());
	event_source->Into(&event_sources);
}

#endif // EVENT_SOURCE_SUPPORT

#ifdef DOM_GADGET_FILE_API_SUPPORT

/*=== DOM File API support ===*/

void
DOM_EnvironmentImpl::AddFileObject(DOM_FileBase *file)
{
	file->Into(&file_objects);
}

#endif // DOM_GADGET_FILE_API_SUPPORT


#ifdef JS_SCOPE_CLIENT

/* virtual */ void
DOM_EnvironmentImpl::SetScopeClient(JS_ScopeClient *client)
{
	scope_client = client;
}

/* virtual */ JS_ScopeClient *
DOM_EnvironmentImpl::GetScopeClient()
{
	return scope_client;
}

#endif // JS_SCOPE_CLIENT

#ifdef CLIENTSIDE_STORAGE_SUPPORT

void
DOM_EnvironmentImpl::AddDOMStoragebject(DOM_Storage *s)
{
	s->Into(&webstorage_objects);
}
#endif

#ifdef DOM_JIL_API_SUPPORT

void
DOM_EnvironmentImpl::SetJILWidget(DOM_JILWidget* widget)
{
	OP_ASSERT(jil_widget == NULL || widget == NULL);
	jil_widget = widget;
}

#endif // DOM_JIL_API_SUPPORT

void
DOM_EnvironmentImpl::AddAsyncCallback(DOM_AsyncCallback* async_callback)
{
	async_callback->Into(&async_callbacks);
}

#ifdef EXTENSION_SUPPORT
void
DOM_EnvironmentImpl::SetExtensionBackground(DOM_ExtensionBackground *background)
{
	OP_ASSERT(!extension_background || !background);
	extension_background = background;
}
#endif // EXTENSION_SUPPORT

/*=== Other ===*/

void
DOM_EnvironmentImpl::GCTrace()
{
	MarkNodesWithWaitingEventHandlers();

#ifdef MEDIA_HTML_SUPPORT
	DOM_HTMLMediaElement *media = (DOM_HTMLMediaElement *)media_elements.First();
	while (media)
	{
		// Trace the media element, but only if it's currently playing.
		media->GCMarkPlaying();
		media = (DOM_HTMLMediaElement *)media->Suc();
	}
#endif // MEDIA_HTML_SUPPORT

	for (DOM_FileReader* reader = file_readers.First(); reader; reader = reader->Suc())
		GetRuntime()->GCMark(reader);

#ifdef DOM_SUPPORT_BLOB_URLS
	DOM_BlobURL::GCTrace(&blob_urls);
#endif // DOM_SUPPORT_BLOB_URLS

#ifdef WEBSOCKETS_SUPPORT
	TraceAllWebSockets();
#endif //WEBSOCKETS_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	if (web_workers)
		web_workers->GCTrace(GetRuntime());
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	for (DOM_MessagePort *port = message_ports.First(); port; port = port->Suc())
		GetRuntime()->GCMark(port);
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
	DOM_EventSource *source = event_sources.First();
	while (source)
	{
		GetRuntime()->GCMark(source);
		source = source->Suc();
	}
#endif // EVENT_SOURCE_SUPPORT

#ifdef TOUCH_EVENTS_SUPPORT
	DOM_Object::GCMark(active_touches);
#endif // TOUCH_EVENTS_SUPPORT

#ifdef EXTENSION_SUPPORT
	DOM_Object::GCMark(extension_background);
#endif // EXTENSION_SUPPORT

	// Free the shared string buffer in case some script grew it unnecessarily big.
	TempBuffer* b = DOM_Object::GetEmptyTempBuf();
	OP_ASSERT(b || !"!b may be caused by a document which was not closed correctly!");
	if (b)
		b->FreeStorage();
}

void
DOM_EnvironmentImpl::MarkNodesWithWaitingEventHandlers()
{
	// onload/onerror handlers have to keep Image nodes alive
	if (frames_document)
	{
		LoadingImageElementIterator it(frames_document);
		while (HTML_Element* elm = it.GetNext())
		{
			if (DOM_Object* node = elm->GetESElement())
				if (HasEventHandler(node, ONLOAD) || HasEventHandler(node, ONERROR))
					DOM_Object::GCMark(node);
		}
	}

#if defined(DOM3_LOAD) && defined (DOM_HTTP_SUPPORT)
	for (DOM_LSLoader *loader = lsloaders.First(); loader; loader = loader->Suc())
		if (DOM_LSParser *parser = loader->GetLSParser())
			if (DOM_XMLHttpRequest *xhr = parser->GetXMLHttpRequest())
				if (xhr->HasEventHandlerAndUnsentEvents())
					DOM_Object::GCMark(parser);
#endif // DOM3_LOAD && DOM_HTTP_SUPPORT
}

BOOL
DOM_EnvironmentImpl::IsInDocument(HTML_Element *element)
{
	if (document)
		if (HTML_Element *ancestor = document->GetPlaceholderElement())
			return ancestor->IsAncestorOf(element);
	return FALSE;
}

DOM_Element *
DOM_EnvironmentImpl::GetRootElement()
{
	return document ? document->GetRootElement() : NULL;
}

OP_STATUS
DOM_EnvironmentImpl::SetDocument(DOM_Document *new_document)
{
	RETURN_IF_ERROR(runtime->KeepAliveWithRuntime(new_document));
	RETURN_IF_ERROR(new_document->ResetPlaceholderElement(frames_document->GetDocRoot()));

	document = new_document;
	return OpStatus::OK;
}

/* static */
ServerName *
DOM_EnvironmentImpl::GetHostName(FramesDocument *frames_doc)
{
	if (frames_doc)
	{
		URL url = frames_doc->GetOrigin()->security_context;
		return url.GetServerName();
	}
	return NULL;
}

void
DOM_EnvironmentImpl::AccessedOtherEnvironment(FramesDocument *other)
{
	if (is_loading)
	{
		frames_document->SetCompatibleHistoryNavigationNeeded();

		if (other)
		{
			FramesDocument *parent_doc = frames_document->GetParentDoc();

			while (parent_doc)
			{
				if (parent_doc == other)
				{
					other->SetCompatibleHistoryNavigationNeeded();
					break;
				}
				parent_doc = parent_doc->GetParentDoc();
			}
		}
	}
}

#ifdef OPERA_CONSOLE

OP_STATUS
DOM_EnvironmentImpl::PostError(DOM_EnvironmentImpl *environment, const ES_ErrorInfo &error, const uni_char *context, const uni_char *url)
{
	if (!g_console->IsLogging())
		return OpStatus::OK;

	OpConsoleEngine::Message message(OpConsoleEngine::EcmaScript, OpConsoleEngine::Error);
	FramesDocument *document = environment ? environment->GetFramesDocument() : NULL;

	if (document)
	{
		message.window = document->GetWindow()->Id();
		if (!url)
			RETURN_IF_ERROR(document->GetURL().GetAttribute(URL::KUniName_Username_Password_Hidden, message.url));
	}
	else
		message.window = 0;

	if (url)
		RETURN_IF_ERROR(message.url.Set(url));

	if (context)
		RETURN_IF_ERROR(message.context.Set(context));

	RETURN_IF_ERROR(message.message.Set(error.error_text));

	TRAPD(status, g_console->PostMessageL(&message));
	return status;
}

#endif // OPERA_CONSOLE

#ifdef DOM3_LOAD

void
DOM_EnvironmentImpl::AddLSLoader(DOM_LSLoader *lsloader)
{
	lsloader->Into(&lsloaders);
}

void
DOM_EnvironmentImpl::RemoveLSLoader(DOM_LSLoader *lsloader)
{
	lsloader->Out();
}

#endif // DOM3_LOAD

void
DOM_EnvironmentImpl::AddFileReader(DOM_FileReader *reader)
{
	reader->Into(&file_readers);
}

void
DOM_EnvironmentImpl::RemoveFileReader(DOM_FileReader *reader)
{
	reader->Out();
}

#ifdef DOM_SUPPORT_BLOB_URLS
OP_STATUS
DOM_EnvironmentImpl::CreateObjectURL(DOM_Blob* blob, TempBuffer* buf)
{
	// Generate a UUID
	OpGuid guid;

	RETURN_IF_ERROR(g_opguidManager->GenerateGuid(guid));
	RETURN_IF_ERROR(buf->AppendFormat(UNI_L("blob:%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x"),
		guid[0], guid[1], guid[2], guid[3],
		guid[4], guid[5],
		guid[6], guid[7],
		guid[8], guid[9],
		guid[10], guid[11], guid[12], guid[13], guid[14], guid[15]));

	DOM_BlobURL* blob_url;
	RETURN_IF_ERROR(DOM_BlobURL::Make(blob_url, buf->GetStorage(), blob));
	if (OpStatus::IsSuccess(blob_urls.Add(blob_url->GetURL(), blob_url)))
		return OpStatus::OK;
	OP_DELETE(blob_url);
	return OpStatus::ERR_NO_MEMORY;
}

void
DOM_EnvironmentImpl::RevokeObjectURL(const uni_char* blob_url)
{
	/* Eventually we might have to signal out to users that have already started using it. */
	DOM_BlobURL* data;
	blob_urls.Remove(blob_url, &data);
}
#endif // DOM_SUPPORT_BLOB_URLS

#ifdef DOM_XSLT_SUPPORT

void
DOM_EnvironmentImpl::AddXSLTProcessor(DOM_XSLTProcessor *processor)
{
	processor->Into(&xsltprocessors);
}

void
DOM_EnvironmentImpl::RemoveXSLTProcessor(DOM_XSLTProcessor *processor)
{
	processor->Out();
}

#endif // DOM_XSLT_SUPPORT

void
DOM_EnvironmentImpl::TreeDestroyed(HTML_Element *tree_root)
{
	node_collection_tracker.TreeDestroyed(tree_root);
	element_collection_tracker.TreeDestroyed(tree_root);

	DOM_MutationListener *listener = (DOM_MutationListener *) mutation_listeners.First();
	while (listener)
	{
		DOM_MutationListener *next_listener = listener->Next();
		listener->OnTreeDestroyed(tree_root);
		listener = next_listener;
	}
}


DOM_EnvironmentImpl::CurrentState::CurrentState(DOM_EnvironmentImpl *environment, DOM_Runtime *origining_runtime)
	: environment(environment),
	  current_origining_runtime(environment->current_origining_runtime),
	  current_buffer(environment->current_buffer),
	  owner_document(environment->owner_document),
	  skip_script_elements(environment->skip_script_elements),
	  is_setting_attribute(environment->is_setting_attribute)
{
	environment->current_origining_runtime = origining_runtime;
}

DOM_EnvironmentImpl::CurrentState::~CurrentState()
{
	environment->current_origining_runtime = current_origining_runtime;
	environment->current_buffer = current_buffer;
	environment->owner_document = owner_document;
	environment->skip_script_elements = skip_script_elements;
	environment->is_setting_attribute = is_setting_attribute;
}

void
DOM_EnvironmentImpl::CurrentState::SetSkipScriptElements()
{
	environment->skip_script_elements = TRUE;
}

void
DOM_EnvironmentImpl::CurrentState::SetTempBuffer(TempBuffer *buffer)
{
	environment->current_buffer = buffer ? buffer : DOM_Object::GetEmptyTempBuf();
}

void
DOM_EnvironmentImpl::CurrentState::SetOwnerDocument(DOM_Document *owner_document)
{
	environment->owner_document = owner_document;
}

void
DOM_EnvironmentImpl::CurrentState::SetIsSettingAttribute()
{
	environment->is_setting_attribute = TRUE;
}


DOM_EnvironmentImpl::DOM_EnvironmentImpl()
	: state(CREATED),
	  runtime(NULL),
	  scheduler(NULL),
	  asyncif(NULL),
	  window(NULL),
	  document(NULL),
	  owner_document(NULL),
	  frames_document(NULL),
	  is_xml(FALSE),
	  is_xhtml(FALSE),
#ifdef SVG_DOM
	  is_svg(FALSE),
#endif // SVG_DOM
	  skip_script_elements(FALSE),
	  is_setting_attribute(FALSE),
	  is_loading(FALSE),
	  signal_collection_changes(TRUE),
	  document_finished_signals(0),
#ifdef USER_JAVASCRIPT
	  user_js_manager(NULL),
#endif // USER_JAVASCRIPT
	  current_origining_runtime(NULL),
	  current_buffer(NULL),
	  force_next_event(FALSE)
	, call_mutation_listeners(TRUE)
#ifdef DOM_SUPPORT_BLOB_URLS
	, blob_urls(TRUE) // Case sensitive.
#endif // DOM_SUPPORT_BLOB_URLS
#ifdef JS_SCOPE_CLIENT
	, scope_client(0)
#endif // JS_SCOPE_CLIENT
#ifdef DOM_JIL_API_SUPPORT
	, jil_widget(NULL)
#endif // DOM_JIL_API_SUPPORT
#ifdef TOUCH_EVENTS_SUPPORT
	, active_touches(NULL)
#endif // TOUCH_EVENTS_SUPPORT
#ifdef EXTENSION_SUPPORT
	, extension_background(NULL)
#endif // EXTENSION_SUPPORT
{
	op_memset(mouse_movement_event_handlers_counter, 0, sizeof mouse_movement_event_handlers_counter);
#ifdef DOM2_MUTATION_EVENTS
	op_memset(dom2_mutation_event_handlers_counter, 0, sizeof dom2_mutation_event_handlers_counter);
#endif // DOM2_MUTATION_EVENTS
#ifdef TOUCH_EVENTS_SUPPORT
	op_memset(touch_event_handlers_counter, 0, sizeof touch_event_handlers_counter);
#endif // TOUCH_EVENTS_SUPPORT
}

DOM_EnvironmentImpl::~DOM_EnvironmentImpl()
{
	OP_ASSERT(!frames_document);

	if (state == CREATED)
	{
		OP_DELETE(document);
		OP_DELETE(window);

		if (runtime)
			runtime->Reset();
	}

#ifdef APPLICATION_CACHE_SUPPORT
	if (g_application_cache_manager)
		g_application_cache_manager->CacheHostDestructed(this);
#endif // APPLICATION_CACHE_SUPPORT

#ifdef USER_JAVASCRIPT
	OP_DELETE(user_js_manager);
#endif // USER_JAVASCRIPT

	OP_DELETE(asyncif);
	OP_DELETE(scheduler);

	OP_ASSERT(mutation_listeners.Empty());

#ifdef WEBSOCKETS_SUPPORT
	CloseAllWebSockets();
#endif //WEBSOCKETS_SUPPORT

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	OP_ASSERT(webstorage_objects.Empty());
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	if (web_workers)
		OP_DELETE(web_workers);
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	while (DOM_MessagePort *p = message_ports.First())
	{
		p->Out();
		p->Disentangle();
	}
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

}

#ifdef WEBSOCKETS_SUPPORT
OP_STATUS
DOM_EnvironmentImpl::AddWebSocket(DOM_WebSocket *ws)
{
	if (websockets.Find(ws) < 0)
		return websockets.Add(ws);
	return OpStatus::OK;
}

void
DOM_EnvironmentImpl::RemoveWebSocket(DOM_WebSocket *ws)
{
	INT32 idx = websockets.Find(ws);
	if (idx >= 0)
		websockets.Remove(idx);
}

BOOL
DOM_EnvironmentImpl::CloseAllWebSockets()
{
	BOOL did_close = FALSE;
	UINT32 count = websockets.GetCount();
	for (UINT32 idx = 0; idx < count; idx++)
	{
		DOM_WebSocket *ws = websockets.Get(idx);
		if (ws->CloseHard())
			did_close = TRUE;
	}

	websockets.Remove(0, count);
	return did_close;
}

void
DOM_EnvironmentImpl::TraceAllWebSockets()
{
	UINT32 count = websockets.GetCount();
	for (UINT32 idx = 0; idx < count; idx++)
	{
		DOM_WebSocket *ws = websockets.Get(idx);
		ws->GCTraceConditional();
	}
}
#endif //WEBSOCKETS_SUPPORT

#ifdef TOUCH_EVENTS_SUPPORT
HTML_Element*
DOM_EnvironmentImpl::GetTouchTarget(int id)
{
	for (UINT32 i = 0; i < active_touches->GetCount(); i++)
		if (active_touches->Get(i)->GetIdentifier() == id)
			return active_touches->Get(i)->GetTargetElement();

	return NULL;
}
#endif // TOUCH_EVENTS_SUPPORT


/*=== Creation and destruction ===*/

/* static */
OP_STATUS
DOM_ProxyEnvironment::Create(DOM_ProxyEnvironment *&env)
{
	env = NULL;

	DOM_ProxyEnvironmentImpl *impl = OP_NEW(DOM_ProxyEnvironmentImpl, ());
	if (!impl)
		return OpStatus::ERR_NO_MEMORY;

	env = impl;
	return OpStatus::OK;
}

/* static */
void
DOM_ProxyEnvironment::Destroy(DOM_ProxyEnvironment *env)
{
	if (env)
	{
		DOM_ProxyEnvironmentImpl *impl = (DOM_ProxyEnvironmentImpl *) env;

		OP_DELETE(impl);
	}
}


/*=== Proxy/real object management ===*/

/* virtual */
void
DOM_ProxyEnvironmentImpl::SetRealWindowProvider(RealWindowProvider *new_provider)
{
	provider = new_provider;
}

/* virtual */
void
DOM_ProxyEnvironmentImpl::Update()
{
	for (ProxyObjectGroup *group = static_cast<ProxyObjectGroup *>(proxy_object_groups.First());
	     group;
	     group = static_cast<ProxyObjectGroup *>(group->Suc()))
	{
		if (group->window)
			group->window->ResetObject();
		if (group->document)
			group->document->ResetObject();
		if (group->location)
			group->location->ResetObject();
	}
}

/* virtual */
OP_STATUS
DOM_ProxyEnvironmentImpl::GetProxyWindow(DOM_Object *&window, ES_Runtime *origining_runtime)
{
	ProxyObjectGroup *group;

	RETURN_IF_ERROR(GetOrCreateProxyObjectGroup(group, origining_runtime));

	if (!group->window)
	{
		if (OpStatus::IsError(DOM_WindowProxyObject::Make(group->window, static_cast<DOM_Runtime *>(origining_runtime), &windowprovider)))
		{
			group->window = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

		TRAPD(status, group->window->AddFunctionL(DOM_dummyMethod, "close"));

		if (OpStatus::IsError(status))
		{
			group->window = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	window = group->window;
	return OpStatus::OK;
}

OP_STATUS
DOM_ProxyEnvironmentImpl::GetProxyDocument(DOM_Object *&document, ES_Runtime *origining_runtime)
{
	ProxyObjectGroup *group;

	RETURN_IF_ERROR(GetOrCreateProxyObjectGroup(group, origining_runtime));

	if (!group->document)
		if (OpStatus::IsError(DOM_ProxyObject::Make(group->document, static_cast<DOM_Runtime *>(origining_runtime), &documentprovider)))
		{
			group->document = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

	document = group->document;
	return OpStatus::OK;
}

OP_STATUS
DOM_ProxyEnvironmentImpl::GetProxyLocation(DOM_Object *&location, ES_Runtime *origining_runtime)
{
	ProxyObjectGroup *group;

	RETURN_IF_ERROR(GetOrCreateProxyObjectGroup(group, origining_runtime));

	if (!group->location)
		if (OpStatus::IsError(DOM_ProxyObject::Make(group->location, static_cast<DOM_Runtime *>(origining_runtime), &locationprovider)))
		{
			group->location = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}

	location = group->location;

	return OpStatus::OK;
}

/* virtual */
void
DOM_ProxyEnvironmentImpl::WindowProvider::ProxyObjectDestroyed(DOM_Object *proxy_window)
{
	environment->ResetProxyObject(proxy_window, NULL, NULL);
}

/* virtual */
OP_STATUS
DOM_ProxyEnvironmentImpl::WindowProvider::GetObject(DOM_Object *&object)
{
	return environment->GetRealWindowProvider()->GetRealWindow(object);
}

/* virtual */
void
DOM_ProxyEnvironmentImpl::DocumentProvider::ProxyObjectDestroyed(DOM_Object *proxy_document)
{
	environment->ResetProxyObject(NULL, proxy_document, NULL);
}

/* virtual */
OP_STATUS
DOM_ProxyEnvironmentImpl::DocumentProvider::GetObject(DOM_Object *&object)
{
	object = NULL;

	DOM_Object *real_window = NULL;

	RETURN_IF_ERROR(environment->GetRealWindowProvider()->GetRealWindow(real_window));

	if (real_window)
	{
		DOM_EnvironmentImpl *environment = real_window->GetEnvironment();

		if (OpStatus::IsMemoryError(environment->ConstructDocumentNode()))
			return OpStatus::ERR_NO_MEMORY;

		object = environment->GetDocument();
	}

	return OpStatus::OK;
}

/* virtual */
void
DOM_ProxyEnvironmentImpl::LocationProvider::ProxyObjectDestroyed(DOM_Object *proxy_location)
{
	environment->ResetProxyObject(NULL, NULL, proxy_location);
}

/* virtual */
OP_STATUS
DOM_ProxyEnvironmentImpl::LocationProvider::GetObject(DOM_Object *&object)
{
	object = NULL;

	DOM_Object *real_window = NULL;

	RETURN_IF_ERROR(environment->GetRealWindowProvider()->GetRealWindow(real_window));

	if (real_window)
		object = ((JS_Window *) real_window)->GetLocation();

	return OpStatus::OK;
}

OP_STATUS
DOM_ProxyEnvironmentImpl::GetOrCreateProxyObjectGroup(ProxyObjectGroup *&group, ES_Runtime *runtime0)
{
	DOM_Runtime *runtime = static_cast<DOM_Runtime *>(runtime0);

	for (group = static_cast<ProxyObjectGroup *>(proxy_object_groups.First());
	     group;
	     group = static_cast<ProxyObjectGroup *>(group->Suc()))
		if (group->runtime == runtime)
			return OpStatus::OK;

	group = OP_NEW(ProxyObjectGroup, ());

	if (!group)
		return OpStatus::ERR_NO_MEMORY;

	group->runtime = runtime;
	group->runtime_link = runtime->AddAccessedProxyEnvironment(this, group);

	if (!group->runtime_link)
	{
		OP_DELETE(group);
		return OpStatus::ERR_NO_MEMORY;
	}

	group->IntoStart(&proxy_object_groups);
	return OpStatus::OK;
}

void
DOM_ProxyEnvironmentImpl::ResetRealObject(DOM_Object *window, DOM_Object *document, DOM_Object *location)
{
	BOOL found = FALSE;

	for (ProxyObjectGroup *group = static_cast<ProxyObjectGroup *>(proxy_object_groups.First());
	     group && !found;
	     group = static_cast<ProxyObjectGroup *>(group->Suc()))
	{
		if (group->window && group->window->GetObject() == window)
		{
			group->window->ResetObject();
			found = TRUE;
		}

		if (group->document && group->document->GetObject() == document)
		{
			group->document->ResetObject();
			found = TRUE;
		}

		if (group->location && group->location->GetObject() == location)
		{
			group->location->ResetObject();
			found = TRUE;
		}
	}
}

void
DOM_ProxyEnvironmentImpl::ResetProxyObject(DOM_Object *window, DOM_Object *document, DOM_Object *location)
{
	ProxyObjectGroup *group;
	BOOL found = FALSE;

	for (group = static_cast<ProxyObjectGroup *>(proxy_object_groups.First());
	     group;
	     group = static_cast<ProxyObjectGroup *>(group->Suc()))
	{
		if (window && group->window == window)
		{
			group->window = NULL;
			found = TRUE;
		}

		if (document && group->document == document)
		{
			group->document = NULL;
			found = TRUE;
		}

		if (location && group->location == location)
		{
			group->location = NULL;
			found = TRUE;
		}

		if (found)
			break;
	}

	if (found && !group->window && !group->document && !group->location)
	{
		group->Out();
		OP_DELETE(group);
	}
}

void
DOM_ProxyEnvironmentImpl::Close()
{
	for (ProxyObjectGroup *group = static_cast<ProxyObjectGroup *>(proxy_object_groups.First());
	     group;
	     group = static_cast<ProxyObjectGroup *>(group->Suc()))
	{
		if (group->window)
			group->window->SetProvider(NULL);
		if (group->document)
			group->document->SetProvider(NULL);
		if (group->location)
			group->location->SetProvider(NULL);
	}
}

void
DOM_ProxyEnvironmentImpl::RuntimeDetached(ES_Runtime *runtime, Link *group0)
{
	ProxyObjectGroup *group = static_cast<ProxyObjectGroup *>(group0);

	OP_ASSERT(proxy_object_groups.HasLink(group));

	if (group->window)
		group->window->DisableCaches();
	if (group->document)
		group->document->DisableCaches();
	if (group->location)
		group->location->DisableCaches();
}

void
DOM_ProxyEnvironmentImpl::RuntimeDestroyed(ES_Runtime *runtime, Link *group0)
{
	ProxyObjectGroup *group = static_cast<ProxyObjectGroup *>(group0);

	OP_ASSERT(proxy_object_groups.HasLink(group));

	group->Out();
	group->runtime_link = NULL;

	OP_DELETE(group);
}

/* virtual */
DOM_ProxyEnvironmentImpl::~DOM_ProxyEnvironmentImpl()
{
	for (ProxyObjectGroup *group = static_cast<ProxyObjectGroup *>(proxy_object_groups.First());
	     group;
	     group = static_cast<ProxyObjectGroup *>(group->Suc()))
	{
		if (group->window)
			group->window->SetProvider(NULL);
		if (group->document)
			group->document->SetProvider(NULL);
		if (group->location)
			group->location->SetProvider(NULL);
	}
}

DOM_MutationListener *
DOM_MutationListener::Next()
{
	return (DOM_MutationListener *) Suc();
}

/* virtual */ void
DOM_MutationListener::OnTreeDestroyed(HTML_Element *tree_root)
{
}

/* virtual */ OP_STATUS
DOM_MutationListener::OnAfterInsert(HTML_Element *, DOM_Runtime *)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_MutationListener::OnBeforeRemove(HTML_Element *, DOM_Runtime *)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_MutationListener::OnAfterValueModified(HTML_Element *, const uni_char *, ValueModificationType, unsigned, unsigned, unsigned, DOM_Runtime *)
{
	return OpStatus::OK;
}

/* virtual */ OP_STATUS
DOM_MutationListener::OnAttrModified(HTML_Element *, const uni_char *, int, DOM_Runtime *)
{
	return OpStatus::OK;
}

/* virtual */ void
DOM_EnvironmentImpl::OnWindowActivated()
{
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	if (DOM_EventTarget* window_event_target = window->FetchEventTarget())
	{
		if (window_event_target->HasListeners(ONDEVICEMOTION, NULL, ES_PHASE_AT_TARGET))
			OpStatus::Ignore(window->AddMotionListener());

		if (window_event_target->HasListeners(ONDEVICEORIENTATION, NULL, ES_PHASE_AT_TARGET))
			OpStatus::Ignore(window->AddOrientationListener());
	}
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
}

/* virtual */ void
DOM_EnvironmentImpl::OnWindowDeactivated()
{
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	window->RemoveMotionListener();
	window->RemoveOrientationListener();
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
}
