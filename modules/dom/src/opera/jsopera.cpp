/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/opera/jsopera.h"
#include "modules/dom/src/opera/jshwa.h"
#include "modules/dom/src/js/window.h"
#include "modules/dom/src/userjs/userjsmanager.h"

#include "modules/doc/frm_doc.h"
#include "modules/dochand/win.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/security_manager/include/security_manager.h"

#ifdef _DEBUG
# include "modules/hardcore/mh/messages.h"
# include "modules/hardcore/mh/messobj.h"
# include "modules/hardcore/mh/mh.h"
# include "modules/ecmascript_utils/essched.h"
# include "modules/ecmascript_utils/esthread.h"
# include "modules/ecmascript_utils/esasyncif.h"
#endif // _DEBUG

#ifdef WEBFEEDS_BACKEND_SUPPORT
# include "modules/dom/src/feeds/domwebfeeds.h"
# include "modules/dom/src/feeds/domfeed.h"
# include "modules/dom/src/feeds/domfeedentry.h"
# include "modules/dom/src/feeds/domfeedreader.h"
#endif // WEBFEEDS_BACKEND_SUPPORT

#include "modules/layout/layout_workplace.h"

#ifdef GADGET_SUPPORT
# include "modules/gadgets/OpGadget.h"
# include "modules/dom/src/opera/domwidgetmanager.h"
# include "modules/dom/src/opera/domoperaaccountmanager.h"
# include "modules/dom/src/opera/domunitedevicemanager.h"
#endif // GADGET_SUPPORT

#ifdef OPERABOOKMARKS_URL
#include "modules/bookmarks/bookmark_item.h"
#include "modules/bookmarks/bookmark_manager.h"
#include "modules/bookmarks/operabookmarks.h"
#endif // OPERABOOKMARKS_URL

#ifdef OPERASPEEDDIAL_URL
# include "modules/bookmarks/speeddial_manager.h"
# include "modules/bookmarks/operaspeeddial.h"
#endif

#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "modules/dom/src/opera/domgadgetfile.h"
#endif // DOM_GADGET_FILE_API_SUPPORT

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
# include "modules/dom/src/domwebserver/domupnp.h"
#endif

#ifdef WEBSERVER_SUPPORT
# include "modules/webserver/webserver-api.h"
# include "modules/webserver/webserver_resources.h"
# include "modules/dom/src/domwebserver/domwebserver.h"
#endif //WEBSERVER_SUPPORT

#ifdef ABOUT_OPERA_DEBUG
#include "modules/prefs/prefsmanager/collections/pc_tools.h"
#include "modules/about/operadebug.h"
#endif // ABOUT_OPERA_DEBUG

#ifdef JS_SCOPE_CLIENT
#include "modules/dom/src/opera/domscopeclient.h"
#endif // JS_SCOPE_CLIENT

#ifdef SCOPE_CONNECTION_CONTROL
#include "modules/scope/scope_connection_manager.h"
#include "modules/scope/scope_internal_proxy.h"
#endif // SCOPE_CONNECTION_CONTROL

#ifdef SCOPE_SUPPORT
#include "modules/scope/scope_connection_manager.h"
#endif // SCOPE_SUPPORT

#if defined(DOM_GADGET_FILE_API_SUPPORT) || defined(WEBSERVER_SUPPORT)
# include "modules/dom/src/opera/domio.h"
#endif // DOM_GADGET_FILE_API_SUPPORT || WEBSERVER_SUPPORT

#if defined(PREFS_WRITE) && defined(PREFS_HOSTOVERRIDE)
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#endif // PREFS_WRITE && PREFS_HOSTOVERRIDE

#ifdef DOM_LOAD_TV_APP
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#endif // DOM_LOAD_TV_APP

#ifdef DOM_LOAD_TV_APP
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#endif // DOM_LOAD_TV_APP

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
# include "modules/database/opdatabasemanager.h"
#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef DATABASE_STORAGE_SUPPORT
#include "modules/database/opdatabase.h"
#endif // DATABASE_STORAGE_SUPPORT

#ifdef CLIENTSIDE_STORAGE_SUPPORT
#include "modules/database/opstorage.h"
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#include "modules/dom/src/storage/storage.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_LOCALE_SUPPORT
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"
#endif // DOM_LOCALE_SUPPORT

#ifdef CPUUSAGETRACKING
#include "modules/hardcore/cpuusagetracker/cpuusagetracker.h"
#include "modules/hardcore/cpuusagetracker/cpuusagetrackers.h"
#include "modules/dochand/winman.h"
#endif // CPUUSAGETRACKING

#include "modules/about/operaversion.h"

#if defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT
#include "modules/device_api/OrientationManager.h"
#endif // defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT

#if defined(SELFTEST) && defined(USE_DUMMY_OP_CAMERA_IMPL)
#include "platforms/dummy_pi_impl/pi_impl/DummyOpCamera.h"
#endif // SELFTEST && USE_DUMMY_OP_CAMERA_IMPL

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKENDS_USE_BLOCKLIST)
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#endif // VEGA_3DDEVICE && VEGA_BACKENDS_USE_BLOCKLIST

#ifdef _DEBUG
// This code is used to test the suspendable Get and Put in the ECMAScript engine
class JS_OperaTestRestarter : public MessageObject, public ES_ThreadListener
{
private:
	MessageHandler* mh;
	ES_Thread*      thread;

public:
	JS_OperaTestRestarter( MessageHandler* mh, ES_Thread* thread ) : mh(mh), thread(thread)
	{
		thread->Block();
		thread->AddListener(this);
	}

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
	{
		mh->UnsetCallBack(this, MSG_ES_ASYNC_MESSAGE);
		if (thread)
		{
			OpStatus::Ignore(thread->Unblock());
			thread = NULL;
		}
		OP_DELETE(this);
	}
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal)
	{
		ES_ThreadListener::Remove();
		this->thread = NULL;
		mh->UnsetCallBack(this, MSG_ES_ASYNC_MESSAGE);
		return OpStatus::OK;
	}
};

static BOOL SetupTestDelay( ES_Runtime* runtime, ES_Runtime *origining_runtime, ES_Value *value, double v )
{
	FramesDocument* frames_doc = runtime->GetFramesDocument();
	if (frames_doc == NULL || origining_runtime->GetFramesDocument() == NULL)
		return FALSE;
	MessageHandler  *mh   = frames_doc->GetMessageHandler();
	ES_Thread       *thread = origining_runtime->GetESScheduler()->GetCurrentThread();

	JS_OperaTestRestarter *test = OP_NEW(JS_OperaTestRestarter, (mh,thread));
	if (test == NULL)
		return FALSE;

	ES_Object* restart_obj = origining_runtime->CreateHostObjectWrapper(NULL, NULL, "Restart", FALSE);

	if (restart_obj == NULL)
	{
		OP_DELETE(test);
		return FALSE;
	}

	value->type = VALUE_NUMBER;
	value->value.number = v;

	if (OpStatus::IsError(runtime->PutName(restart_obj, UNI_L("numcalls"), *value)))
	{
		OP_DELETE(test);
		return FALSE;
	}

	value->type = VALUE_OBJECT;
	value->value.object = restart_obj;

	mh->SetCallBack(test, MSG_ES_ASYNC_MESSAGE, 0);
	mh->PostMessage(MSG_ES_ASYNC_MESSAGE, 0, 0);

	return TRUE;
}

static BOOL RedoTestDelay( ES_Runtime* runtime, ES_Runtime* origining_runtime, ES_Value* value, ES_Object* restart_object )
{
	ES_Value v;
	if (
        origining_runtime->GetName(restart_object,UNI_L("numcalls"),&v) == OpBoolean::IS_TRUE && v.type == VALUE_NUMBER && v.value.number < 2.0)
	{
		value->type = VALUE_OBJECT;
		value->value.object = restart_object;
		v.value.number += 1.0;
		if (OpStatus::IsSuccess(runtime->PutName(restart_object,UNI_L("numcalls"),v)) &&
			SetupTestDelay( runtime, origining_runtime, value, v.value.number+1.0 ))
			return TRUE;
	}
	return FALSE;
}
#endif // _DEBUG

#if defined(ABOUT_OPERA_DEBUG) || defined(JS_SCOPE_CLIENT)

/**
 * Expose Scope API functions on the JS_Opera object, if security
 * restrictions allow it.
 *
 * This must be separated from the regular initialization code, because
 * the Scope API functions can also be exposed by calling opera.scopeExpose().
 *
 * @param opera The object to expose functions on.
 * @param runtime The runtime that owns the object.
 */
static void
AddScopeFunctionsL(JS_Opera* opera, DOM_Runtime* runtime)
{
#ifdef JS_SCOPE_CLIENT
	BOOL scope_allowed = FALSE;

	LEAVE_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_SCOPE,
		OpSecurityContext(static_cast<DOM_Runtime*>(runtime)),
		OpSecurityContext(static_cast<DOM_Runtime*>(NULL)),
		scope_allowed));

	if (scope_allowed)
	{
		opera->AddFunctionL(JS_Opera::scopeEnableService, "scopeEnableService", "s-");
		opera->AddFunctionL(JS_Opera::scopeAddClient, "scopeAddClient", "---n-");
		opera->AddFunctionL(JS_Opera::scopeTransmit, "scopeTransmit", "s-nn-");
#ifdef SCOPE_MESSAGE_TRANSCODING
		// This is only used for testing and only available when selftests are enabled.
		opera->AddFunctionL(JS_Opera::scopeSetTranscodingFormat, "scopeSetTranscodingFormat", "s-");
#endif // SCOPE_MESSAGE_TRANSCODING
	}
#endif // JS_SCOPE_CLIENT

#ifdef ABOUT_OPERA_DEBUG
	BOOL opera_connect_allowed = FALSE;

	LEAVE_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_OPERA_CONNECT,
		OpSecurityContext(static_cast<DOM_Runtime*>(runtime)),
		OpSecurityContext(static_cast<DOM_Runtime*>(NULL)),
		opera_connect_allowed));

	if (opera_connect_allowed)
	{
		opera->AddFunctionL(JS_Opera::connect, "connect", "sn-");
		opera->AddFunctionL(JS_Opera::disconnect, "disconnect", "-");
		opera->AddFunctionL(JS_Opera::isConnected, "isConnected", "");
		opera->AddFunctionL(JS_Opera::setConnectStatusCallback, "setConnectStatusCallback", "");
#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
		opera->AddFunctionL(JS_Opera::setDevicelistChangedCallback, "setDevicelistChangedCallback", "");
#endif //UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
	}
#endif // ABOUT_OPERA_DEBUG
} // AddScopeFunctionsL

#ifdef SELFTEST
/**
 * Remove Scope API functions.
 *
 * @param opera The object to remove the functions from.
 */
static void
RemoveScopeFunctions(JS_Opera* opera)
{
	OpStatus::Ignore(opera->Delete(UNI_L("scopeEnableService")));
	OpStatus::Ignore(opera->Delete(UNI_L("scopeAddClient")));
	OpStatus::Ignore(opera->Delete(UNI_L("scopeTransmit")));
#ifdef SCOPE_MESSAGE_TRANSCODING
	OpStatus::Ignore(opera->Delete(UNI_L("scopeSetTranscodingFormat")));
#endif // SCOPE_MESSAGE_TRANSCODING
	OpStatus::Ignore(opera->Delete(UNI_L("connect")));
	OpStatus::Ignore(opera->Delete(UNI_L("disconnect")));
	OpStatus::Ignore(opera->Delete(UNI_L("isConnected")));
	OpStatus::Ignore(opera->Delete(UNI_L("setConnectStatusCallback")));
#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
	OpStatus::Ignore(opera->Delete(UNI_L("setDevicelistChangedCallback")));
#endif //UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
}

/**
 * Check whether the Scope API is currently exposed or not.
 *
 * @param opera Check if Scope is exposed on this object.
 * @return OpBoolean::IS_TRUE if exposed, IS_FALSE otherwise.
 */
static OP_BOOLEAN
IsScopeExposed(JS_Opera* opera)
{
	return opera->Get(UNI_L("scopeEnableService"), NULL);
}

/**
 * Set the type of the window associated with the specified runtime.
 *
 * @param runtime The runtime which is associated the window to change.
 * @param type The new type of the window.
 * @return TRUE if type was changed, FALSE if runtime is not associated with
 *         a window.
 */
static BOOL
SetWindowType(DOM_Runtime* runtime, Window_Type type)
{
	FramesDocument* frm_doc = runtime->GetFramesDocument();

	if (!frm_doc)
		return FALSE;

	frm_doc->GetWindow()->SetType(type);

	return TRUE;
}
#endif // SELFTEST

#endif // ABOUT_OPERA_DEBUG || JS_SCOPE_CLIENT

void
JS_Opera::InitializeL()
{
	DOM_Runtime *runtime = GetRuntime();
	FramesDocument *frames_doc = runtime->GetFramesDocument();
	(void)frames_doc; // silence "unused" warning

#ifdef _DEBUG
	AddFunctionL(OP_NEW_L(JS_Opera_hardToCall, ()), "hardToCall");
	AddFunctionL(OP_NEW_L(JS_Opera_delay, ()), "delay", "n-");
	AddFunctionL(OP_NEW_L(JS_Opera_asyncEval, ()), "asyncEval", "s-");
	AddFunctionL(OP_NEW_L(JS_Opera_asyncCallMethod, ()), "asyncCallMethod", "-s");
#ifdef ESUTILS_ASYNC_SLOT_SUPPORT
	AddFunctionL(OP_NEW_L(JS_Opera_asyncGetSlot, ()), "asyncGetSlot");
	AddFunctionL(OP_NEW_L(JS_Opera_asyncSetSlot, ()), "asyncSetSlot");
#endif // ESUTILS_ASYNC_SLOT_SUPPORT
	AddFunctionL(OP_NEW_L(JS_Opera_cancelThisThread, ()), "cancelThisThread");

	ES_Value value;
	DOMSetBoolean(&value, TRUE);

	DOM_Object *macros;
	LEAVE_IF_ERROR(DOMSetObjectRuntime(macros = OP_NEW_L(DOM_Object, ()), (DOM_Runtime *) runtime, runtime->GetObjectPrototype(), "Object"));
	EcmaScript_Object::PutL(UNI_L("macros"), *macros);

#ifdef DOM2_MUTATION_EVENTS
	macros->PutL("DOM2_MUTATION_EVENTS", value);
#endif // DOM2_MUTATION_EVENTS
#ifdef DOM2_TRAVERSAL
	macros->PutL("DOM2_TRAVERSAL", value);
#endif // DOM2_TRAVERSAL
#ifdef DOM2_RANGE
	macros->PutL("DOM2_RANGE", value);
#endif // DOM2_RANGE
	macros->PutL("DOM3_CORE", value);
#ifdef DOM3_EVENTS
	macros->PutL("DOM3_EVENTS", value);
#endif // DOM3_EVENTS
#ifdef DOM3_XPATH
	macros->PutL("DOM3_XPATH", value);
#endif // DOM3_XPATH
#ifdef DOM3_LOAD
	macros->PutL("DOM3_LOAD", value);
#endif // DOM3_LOAD
#ifdef DOM3_SAVE
	macros->PutL("DOM3_SAVE", value);
#endif // DOM3_SAVE
#ifdef DOM_HTTP_SUPPORT
	macros->PutL("DOM_HTTP_SUPPORT", value);
#endif // DOM_HTTP_SUPPORT
#ifdef DOM_SUPPORT_ENTITY
	macros->PutL("DOM_SUPPORT_ENTITY", value);
#endif // DOM_SUPPORT_ENTITY
#ifdef DOM_SUPPORT_NOTATION
	macros->PutL("DOM_SUPPORT_NOTATION", value);
#endif // DOM_SUPPORT_NOTATION
#ifdef DOM_SELECTION_SUPPORT
	macros->PutL("DOM_SELECTION_SUPPORT", value);
#endif // DOM_SELECTION_SUPPORT
#ifdef USER_JAVASCRIPT
	macros->PutL("USER_JAVASCRIPT", value);
#endif // USER_JAVASCRIPT

	DOM_Object *features;
	LEAVE_IF_ERROR(DOMSetObjectRuntime(features = OP_NEW_L(DOM_Object, ()), (DOM_Runtime *) runtime, runtime->GetObjectPrototype(), "Object"));
    EcmaScript_Object::PutL(UNI_L("features"), *features);

#ifdef DOM2_MUTATION_EVENTS
	features->PutL("DOM2_MUTATION_EVENTS", value);
#endif // DOM2_MUTATION_EVENTS
#if defined DOM2_TRAVERSAL && defined DOM2_RANGE
	features->PutL("FEATURE_DOM2_TRAV_AND_RANGE", value);
#endif // DOM2_TRAVERSAL && DOM2_RANGE
#ifdef DOM3_XPATH
	features->PutL("FEATURE_DOM3_XPATH", value);
#endif // DOM3_XPATH
#if defined DOM3_LOAD && defined DOM3_SAVE
	features->PutL("FEATURE_DOM3_LOAD_SAVE", value);
#endif // DOM3_LOAD && DOM3_SAVE
#ifdef DOM_HTTP_SUPPORT
	features->PutL("FEATURE_DOM_HTTP", value);
#endif // DOM_HTTP_SUPPORT
#ifdef USER_JAVASCRIPT
	features->PutL("FEATURE_USER_JAVASCRIPT", value);
#endif // USER_JAVASCRIPT
#endif // _DEBUG

	AddFunctionL(buildNumber, "buildNumber", "s");
	AddFunctionL(version, "version", "");
	AddFunctionL(collect, "collect", "");
#ifdef OPERA_CONSOLE
	AddFunctionL(postError, "postError", "s");
#else // OPERA_CONSOLE
	extern int DOM_dummyMethod(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);
	AddFunctionL(DOM_dummyMethod, "postError", "s");
#endif // OPERA_CONSOLE

#if defined OPERAWIDGETS_URL || defined OPERAUNITE_URL || defined DOM_LOCALE_SUPPORT || defined CPUUSAGETRACKING
	if (frames_doc)
	{
#if defined OPERAWIDGETS_URL || defined OPERAUNITE_URL || defined OPERAEXTENSIONS_URL || defined CPUUSAGETRACKING
		ANCHORD(OpString8, urlName);
		frames_doc->GetURL().GetAttribute(URL::KName, urlName);
#endif
#ifdef CPUUSAGETRACKING
# define IS_OPERA_CPU() (urlName.CompareI("opera:cpu") == 0)
#else
# define IS_OPERA_CPU() FALSE
#endif // CPUUSAGETRACKING
#ifdef OPERAWIDGETS_URL
# define IS_OPERA_WIDGETS() (urlName.CompareI("opera:widgets") == 0)
#else // OPERAWIDGETS_URL
# define IS_OPERA_WIDGETS() FALSE
#endif // OPERAWIDGETS_URL
#ifdef OPERAUNITE_URL
# define IS_OPERA_UNITE() (urlName.CompareI("opera:unite") == 0)
#else // OPERAUNITE_URL
# define IS_OPERA_UNITE() FALSE
#endif // OPERAUNITE_URL
#ifdef OPERAEXTENSIONS_URL
# define IS_OPERA_EXTENSIONS() (urlName.CompareI("opera:extensions") == 0)
#else // OPERAEXTENSIONS_URL
# define IS_OPERA_EXTENSIONS() FALSE
#endif // OPERAEXTENSIONS_URL
#ifdef SELFTEST
# define IS_OPERA_BLANKER() (urlName.CompareI("opera:blanker") == 0)
#else // SELFTEST
# define IS_OPERA_BLANKER() FALSE
#endif

#ifdef CPUUSAGETRACKING
	if (frames_doc->GetOrigin()->security_context.Type() == URL_OPERA || frames_doc->ContainsOnlyOperaInternalScripts())
	{
		// Nice utility function.
		AddFunctionL(getCPUUsage, "getCPUUsage", "[n]-");
		AddFunctionL(getCPUUsageSamples, "getCPUUsageSamples", "n-");
		// More dangerous functions.
		if (IS_OPERA_CPU())
			AddFunctionL(activateCPUUser, "activateCPUUser", "n-");
	}
#endif // CPUUSAGETRACKING

#ifdef DOM_WIDGETMANAGER_SUPPORT
		if (IS_OPERA_WIDGETS() || IS_OPERA_BLANKER())
		{
			DOM_WidgetManager *wm;
			LEAVE_IF_ERROR(DOM_WidgetManager::Make(wm, GetRuntime(), FALSE));

			ES_Value value;
			DOMSetObject(&value, wm);

			PutL("widgetManager", value);
		}
#endif // DOM_WIDGETMANAGER_SUPPORT

#if defined DOM_UNITEAPPMANAGER_SUPPORT || defined DOM_ACCOUNTMANAGER_SUPPORT || defined DOM_UNITEDEVMANAGER_SUPPORT
		if (g_webserver &&
			((IS_OPERA_UNITE() || IS_OPERA_BLANKER() ||
			(frames_doc->GetWindow()->GetGadget() && frames_doc->GetWindow()->GetGadget()->GetGadgetId() &&
			 uni_str_eq("http://unite.opera.com/bundled/home/", frames_doc->GetWindow()->GetGadget()->GetGadgetId()))
			)))
		{
# ifdef DOM_UNITEAPPMANAGER_SUPPORT
			DOM_WidgetManager *wm;
			LEAVE_IF_ERROR(DOM_WidgetManager::Make(wm, GetRuntime(), TRUE));
			ES_Value value;
			DOMSetObject(&value, wm);
			PutL("uniteApplicationManager", value);
# endif // DOM_UNITEAPPMANAGER_SUPPORT

# ifdef DOM_ACCOUNTMANAGER_SUPPORT
			DOM_OperaAccountManager *oam;
			DOM_OperaAccountManager::MakeL(oam, GetRuntime());
			ES_Value val2;
			DOMSetObject(&val2, oam);
			PutL("operaAccountManager", val2);
# endif // DOM_ACCOUNTMANAGER_SUPPORT

# ifdef DOM_UNITEDEVMANAGER_SUPPORT
			DOM_UniteDeviceManager *udm;
			DOM_UniteDeviceManager::MakeL(udm, GetRuntime());
			ES_Value val3;
			DOMSetObject(&val3, udm);
			PutL("uniteDeviceManager", val3);
# endif // DOM_UNITEDEVMANAGER_SUPPORT
		}
#endif // DOM_UNITEAPPMANAGER_SUPPORT || DOM_ACCOUNTMANAGER_SUPPORT || DOM_UNITEDEVMANAGER_SUPPORT

#ifdef DOM_EXTENSIONMANAGER_SUPPORT
		if (IS_OPERA_EXTENSIONS() || IS_OPERA_BLANKER())
		{
			DOM_WidgetManager *wm;
			DOM_WidgetManager::Make(wm, GetRuntime(), FALSE, TRUE);

			ES_Value value;
			DOMSetObject(&value, wm);

			PutL("extensionManager", value);
		}
#endif // DOM_EXTENSIONMANAGER_SUPPORT

#ifdef DOM_LOCALE_SUPPORT
		if (frames_doc->GetOrigin()->security_context.Type() == URL_OPERA || frames_doc->ContainsOnlyOperaInternalScripts())
			AddFunctionL(getLocaleString, "getLocaleString", "s-");
#endif // DOM_LOCALE_SUPPORT
	} // end if (frames_doc)
#endif // OPERAWIDGETS_URL || OPERAUNITE_URL || DOM_LOCALE_SUPPORT

#ifdef DOM_TO_PLATFORM_MESSAGES
	AddFunctionL(sendPlatformMessage, "sendPlatformMessage", "s-");
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef USER_JAVASCRIPT
	AddFunctionL(DOM_Node::accessEventListener, 0, "addEventListener", "s-b-");
	AddFunctionL(DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-");

#ifdef DOM3_EVENTS
	AddFunctionL(DOM_Node::accessEventListener, 2, "addEventListenerNS", "ss-b-");
	AddFunctionL(DOM_Node::accessEventListener, 3, "removeEventListenerNS", "ss-b-");
#endif // DOM3_EVENTS

	AddFunctionL(defineMagicFunction, "defineMagicFunction", "s-");
	AddFunctionL(defineMagicVariable, "defineMagicVariable", "s-");

	AddFunctionL(accessOverrideHistoryNavigationMode, 0, "getOverrideHistoryNavigationMode", NULL);
	AddFunctionL(accessOverrideHistoryNavigationMode, 1, "setOverrideHistoryNavigationMode", "s-");
#endif // USER_JAVASCRIPT

#ifdef DOM_PREFERENCES_ACCESS
# ifdef PREFS_HOSTOVERRIDE
	AddFunctionL(accessPreference, 0, "getPreference", "ss|s-");
# else
	AddFunctionL(accessPreference, 0, "getPreference", "ss-");
# endif // PREFS_HOSTOVERRIDE
	AddFunctionL(accessPreference, 1, "getPreferenceDefault", "ss-");

#ifdef PREFS_WRITE
# ifdef PREFS_HOSTOVERRIDE
	AddFunctionL(accessPreference, 2, "setPreference", "sss|s-");
# else
	AddFunctionL(accessPreference, 2, "setPreference", "sss-");
# endif // PREFS_HOSTOVERRIDE
	AddFunctionL(commitPreferences, "commitPreferences", "");
#endif // PREFS_WRITE
#endif // DOM_PREFERENCES_ACCESS

#if defined _DEBUG
#ifdef XMLUTILS_XMLSERIALIZER_SUPPORT
	AddFunctionL(serializeToXML, "serializeToXML", NULL);
#endif // XMLUTILS_XMLSERIALIZER_SUPPORT
#endif // _DEBUG

#ifdef DOM_XSLT_SUPPORT
#ifndef XSLT_MORPH_2
	AddFunctionL(pushXSLTransform, "pushXSLTransform", NULL);
	AddFunctionL(popXSLTransform, "popXSLTransform", NULL);
#endif // XSLT_MORPH_2
#endif // DOM_XSLT_SUPPORT

#ifdef DOM_INVOKE_ACTION_SUPPORT
	AddFunctionL(invokeAction, "invokeAction", "s-");
#endif // DOM_INVOKE_ACTION_SUPPORT

#ifdef DOM_BENCHMARKXML_SUPPORT
	AddFunctionL(benchmarkXML, "benchmarkXML", "sn-");
#endif // DOM_BENCHMARKXML_SUPPORT

#ifdef OPERABOOKMARKS_URL
	AddFunctionL(bookmarksSaveFormValues, "bookmarksSaveFormValues", "sssss");
	AddFunctionL(bookmarksGetFormUrlValue, "bookmarksGetFormUrlValue", "");
	AddFunctionL(bookmarksGetFormTitleValue, "bookmarksGetFormTitleValue", "");
	AddFunctionL(bookmarksGetFormDescriptionValue, "bookmarksGetFormDescriptionValue", "");
	AddFunctionL(bookmarksGetFormShortnameValue, "bookmarksGetFormShortnameValue", "");
	AddFunctionL(bookmarksGetFormParentTitleValue, "bookmarksGetFormParentTitleValue", "");
	AddFunctionL(setBookmarkListener, "setBookmarkListener", "-");
	AddFunctionL(addBookmark, "addBookmark", "sss");
	AddFunctionL(addBookmarkFolder, "addBookmarkFolder", "ss");
	AddFunctionL(deleteBookmark, "deleteBookmark", "s");
	AddFunctionL(moveBookmark, "moveBookmark", "ssssss");
	AddFunctionL(loadBookmarks, "loadBookmarks", "");
	AddFunctionL(saveBookmarks, "saveBookmarks", "");
#endif // OPERABOOKMARKS_URL

#ifdef OPERASPEEDDIAL_URL
	AddFunctionL(setSpeedDial, "setSpeedDial", "ns");
	AddFunctionL(connectSpeedDial, "connectSpeedDial", "o");
	AddFunctionL(reloadSpeedDial, "reloadSpeedDial", "n");
	AddFunctionL(setSpeedDialReloadInterval, "setSpeedDialReloadInterval", "nn");
	AddFunctionL(swapSpeedDials, "swapSpeedDials", "nn");
#endif // OPERASPEEDDIAL_URL

#if defined(JS_SCOPE_CLIENT) || defined(ABOUT_OPERA_DEBUG)
	AddScopeFunctionsL(this, runtime);
#if defined(SELFTEST) && defined(JS_SCOPE_CLIENT)
	AddFunctionL(scopeExpose, "scopeExpose", "");
	AddFunctionL(scopeUnexpose, "scopeUnexpose", "");
#endif // SELFTEST && JS_SCOPE_CLIENT
#endif // JS_SCOPE_CLIENT || ABOUT_OPERA_DEBUG

#ifdef WEBFEEDS_BACKEND_SUPPORT
	DOM_FeedEntry::ConstructFeedEntryObjectL(*PutConstructorL("FeedEntry", DOM_Runtime::FEEDENTRY_PROTOTYPE), runtime);
	DOM_Feed::ConstructFeedObjectL(*PutConstructorL("Feed", DOM_Runtime::FEED_PROTOTYPE), runtime);
#endif // WEBFEEDS_BACKEND_SUPPORT

#ifdef ESUTILS_PROFILER_SUPPORT
	AddFunctionL(createProfiler, "createProfiler");
#endif // ESUTILS_PROFILER_SUPPORT

#ifdef DATABASE_STORAGE_SUPPORT
	AddFunctionL(deleteDatabase, "deleteDatabase", "s|s");
#endif // DATABASE_STORAGE_SUPPORT

#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	URL origin = static_cast<DOM_Runtime *>(runtime)->GetOriginURL();
	if (origin.Type() == URL_OPERA)
	{
		OpString page_url;
		LEAVE_IF_ERROR(origin.GetAttribute(URL::KName, page_url));
		if (page_url.CompareI("opera:" DATABASE_ABOUT_WEBSTORAGE_URL) == 0)
		{
			AddFunctionL(clearWebStorage, 0, "-clearLocalStorage", "s");
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
			AddFunctionL(clearWebStorage, 1, "-clearWidgetPreferences", "s");
# endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
			AddFunctionL(clearWebStorage, 2, "-clearScriptStorage", "s");
# endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		}
	}
#endif // DATABASE_ABOUT_WEBSTORAGE_URL

#ifdef _PLUGIN_SUPPORT_
	if (frames_doc && frames_doc->GetURL().Type() == URL_OPERA
		&& frames_doc->GetURL().GetAttribute(URL::KName).CompareI("opera:plugins") == 0)
		AddFunctionL(togglePlugin, "togglePlugin", "sb");
#endif // _PLUGIN_SUPPORT_

#ifdef DOM_LOAD_TV_APP
	AddFunctionL(loadTVApp, "loadTVApp", "{name:s,icon_url:s,application:s,whitelist:?[s]}-");
#endif //DOM_LOAD_TV_APP

#if defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT
	AddFunctionL(requestCompassCalibration, "requestCompassCalibration", "");
#endif // defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT

#ifdef SPECULATIVE_PARSER
	AddFunctionL(getSpeculativeParserUrls, "getSpeculativeParserUrls", "");
#endif // SPECULATIVE_PARSER

	((DOM_Runtime *) runtime)->GetEnvironment()->RegisterCallbacksL(DOM_Environment::OPERA_CALLBACK, this);

#if defined(SELFTEST) && defined(USE_DUMMY_OP_CAMERA_IMPL)
	AddFunctionL(attachDetachDummyCamera, 1, "attachDummyCamera");
	AddFunctionL(attachDetachDummyCamera, 0, "detachDummyCamera");
#endif // SELFTEST && USE_DUMMY_OP_CAMERA_IMPL
}

/* virtual */ ES_GetState
JS_Opera::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	FramesDocument* frames_doc = GetFramesDocument();
	if (!frames_doc)
		return GET_FAILED;

	switch (property_name)
	{
#ifdef DOM_INTEGRATED_DEVTOOLS_SUPPORT
	case OP_ATOM_attached:
		{
			Window* window = frames_doc->GetWindow();

			if (window->GetType() != WIN_TYPE_DEVTOOLS)
				return GET_FAILED;

			BOOL attached = FALSE;
# ifdef WIC_USE_WINDOW_ATTACHMENT
			WindowCommander * wc = window->GetWindowCommander();
			wc->GetDocumentListener()->OnGetWindowAttached(wc, &attached);
# endif // WIC_USE_WINDOW_ATTACHMENT

			DOMSetBoolean(value, attached);
			return GET_SUCCESS;
		}
#endif // DOM_INTEGRATED_DEVTOOLS_SUPPORT
#ifdef SCOPE_SUPPORT
	case OP_ATOM_stpVersion:
		{
			if (value)
			{
				Window* window = frames_doc->GetWindow();

				if (window->GetType() != WIN_TYPE_DEVTOOLS)
					return GET_FAILED;

#ifdef JS_SCOPE_CLIENT
				JS_ScopeClient *client_conn = static_cast<DOM_Runtime *>(origining_runtime)->GetEnvironment()->GetScopeClient();
				if (client_conn == NULL)
				{
					DOMSetString(value, UNI_L("STP/0"));
					return GET_SUCCESS;
				}

				DOMSetBoolean(value, FALSE);

				TempBuffer *buf = GetEmptyTempBuf();
				RETURN_VALUE_IF_ERROR(buf->Append(UNI_L("STP/")), GET_FAILED);
				int stp_version = client_conn->GetHostVersion();
				if (stp_version < 0)
					stp_version = 0;
				RETURN_VALUE_IF_ERROR(buf->AppendUnsignedLong(stp_version), GET_FAILED);
				DOMSetString(value, buf);
#endif // JS_SCOPE_CLIENT

			}
			return GET_SUCCESS;

		}
	case OP_ATOM_scopeListenerAddress:
		{
			if (frames_doc->GetWindow()->GetType() != WIN_TYPE_DEVTOOLS)
				return GET_FAILED;
			if (value)
			{
				OpString address;
				GET_FAILED_IF_ERROR(OpScopeConnectionManager::GetListenerAddress(address));
				TempBuffer *buffer = GetEmptyTempBuf();
				GET_FAILED_IF_ERROR(buffer->Append(address.CStr()));
				DOMSetString(value, buffer);
			}
			return GET_SUCCESS;
		}
#endif // SCOPE_SUPPORT

	case OP_ATOM_renderingMode:
		{
			// If we can't get the rendering mode for some reason, just return an
			// empty string.
			DOMSetString(value, UNI_L(""));
			WindowCommander *commander = 0;
			if(origining_runtime
				&& origining_runtime->GetFramesDocument()
				&& origining_runtime->GetFramesDocument()->GetWindow())
				commander = origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander();
			if (value && commander)
			{
				const uni_char *mode;
				switch (commander->GetLayoutMode())
				{
				case OpWindowCommander::NORMAL:
					mode = UNI_L("Normal");
					break;

				case OpWindowCommander::SSR:
					mode = UNI_L("SSR");
					break;

				case OpWindowCommander::CSSR:
					mode = UNI_L("CSSR");
					break;

				case OpWindowCommander::AMSR:
					mode = UNI_L("AMSR");
					break;

				case OpWindowCommander::MSR:
					mode = UNI_L("MSR");
					break;

				case OpWindowCommander::ERA:
					mode = UNI_L("ERA");
					break;

#ifdef TV_RENDERING
				case OpWindowCommander::TVR:
					mode = UNI_L("TVR");
					break;
#endif // TV_RENDERING

				default:
					mode = NULL;
				}

				DOMSetString(value, mode);
			}
			return GET_SUCCESS;
		}

#ifdef DOM_DSE_DEBUGGING
	case OP_ATOM_dseEnabled:
		if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
			DOMSetBoolean(value, logdoc->DSEGetEnabled());
		return GET_SUCCESS;

	case OP_ATOM_dseRecovered:
		if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
			DOMSetBoolean(value, logdoc->DSEGetRecovered());
		return GET_SUCCESS;
#endif // DOM_DSE_DEBUGGING

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */
JS_Opera::~JS_Opera()
{
#ifdef ABOUT_OPERA_DEBUG
	OP_DELETE(debugConnectionCallback);
#endif // ABOUT_OPERA_DEBUG
#ifdef OPERASPEEDDIAL_URL
	// speeddial_callback will unregister itself when destroyed
	OP_DELETE(speeddial_listener);
#endif // OPERASPEEDDIAL_URL
#ifdef OPERABOOKMARKS_URL
	if (bookmark_listener && g_bookmark_manager)
	{
		g_bookmark_manager->UnregisterBookmarkManagerListener(bookmark_listener);
		bookmark_listener->SetCallback(NULL);
		bookmark_listener->SetAsyncInterface(NULL);
		bookmark_listener->SetSavedFormValues(NULL, NULL, NULL, NULL, NULL);
	}
#endif // OPERABOOKMARKS_URL
}

/* virtual */ void
JS_Opera::GCTrace()
{
#ifdef OPERASPEEDDIAL_URL
	if (speeddial_listener)
		speeddial_listener->GCTrace(GetRuntime());
#endif // OPERASPEEDDIAL_URL
#ifdef OPERABOOKMARKS_URL
	if (bookmark_listener)
	{
		ES_Object *callback = bookmark_listener->GetCallback();
		if (callback)
			GCMark(callback);
	}
#endif // OPERABOOKMARKS_URL
#ifdef ABOUT_OPERA_DEBUG
	if (debugConnectionCallback)
		GCMark(debugConnectionCallback->GetCallback());
#endif // ABOUT_OPERA_DEBUG
}

/* virtual */ ES_GetState
JS_Opera::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef _DEBUG
	// This code is used to test the suspendable Get and Put in the ECMAScript engine
	if (uni_str_eq(property_name, "debugMode"))
	{
		DOMSetBoolean(value,TRUE);
		return GET_SUCCESS;
	}
	else if (uni_str_eq(property_name, "hardToGet"))
	{
		if (value)
		{
			if (!SetupTestDelay( GetRuntime(), origining_runtime, value, 0.0 ))
				return GET_NO_MEMORY;
			else
				return GET_SUSPEND;
		}
		else
			return GET_SUCCESS;
	}
	else if (uni_str_eq(property_name, "putWillConvert"))
	{
		DOMSetNumber( value, saved_value_name );
		return GET_SUCCESS;
	}

#endif // _DEBUG

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	if (uni_str_eq(property_name, "scriptStorage"))
	{
		/**
		 * The following code is based on DOM_UserJSManager::GetIsActive
		 *
		 * The scriptStorage object shall only be returned during the User JS
		 * thread and only if the callee is from within a user script, and nothing else
		 *
		 * This clode block always returns GET_SUCCESS because we don't
		 * want an ecmascript property to shadow the host property
		 */
		if (value)
		{
			DOM_UserJSManager *user_js_manager = GetEnvironment()->GetUserJSManager();
			if (!user_js_manager || !user_js_manager->GetIsActive(origining_runtime))
				return GET_SUCCESS;

			ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);

			if (thread)
			{
				if (ES_Context *context = thread->GetContext())
					if (!ES_Runtime::HasPrivilegeLevelAtLeast(context, ES_Runtime::PRIV_LVL_USERJS))
						return GET_SUCCESS;

				if (!DOM_Storage::CanRuntimeAccessObject(WEB_STORAGE_USERJS, origining_runtime, GetRuntime()))
					return GET_SUCCESS;

				if (thread->Type() == ES_THREAD_COMMON &&
					uni_str_eq(thread->GetInfoString(), "User Javascript thread"))
				{
					DOM_UserJSThread* ujs_thread = static_cast<DOM_UserJSThread*>(thread);

					DOM_Storage *storage = ujs_thread->GetScriptStorage();
					if (storage == NULL)
					{
						GET_FAILED_IF_ERROR(DOM_Storage::MakeScriptStorage(storage, ujs_thread->GetScriptFileName(), GetRuntime()));
						OP_ASSERT(storage != NULL);
						ujs_thread->SetScriptStorage(storage);
					}
					DOMSetObject(value, storage);
				}
			}
		}
		return GET_SUCCESS;
	}
#endif //defined WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

	// Export reflow instrumentation
	if (uni_str_eq(property_name, "reflowCount"))
	{
		DOMSetNumber(value, -1);
		if (FramesDocument *frames_doc = GetFramesDocument())
			if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
				if (LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace())
					DOMSetNumber( value, workplace->GetReflowCount() );
		return GET_SUCCESS;
	}
	else if (uni_str_eq(property_name, "reflowTime"))
	{
		DOMSetNumber(value, -1);
		if (FramesDocument *frames_doc = GetFramesDocument())
			if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
				if (LayoutWorkplace* workplace = logdoc->GetLayoutWorkplace())
					DOMSetNumber(value, workplace->GetReflowTime());
		return GET_SUCCESS;
	}

#ifdef WEBFEEDS_BACKEND_SUPPORT
# ifdef DOM_FEEDS_ACCESS
	if (uni_str_eq(property_name, "feeds"))
	{
		if (FramesDocument* frm_doc = GetFramesDocument())
		{
			if (frm_doc->GetOrigin()->security_context.Type() == URL_OPERA || frm_doc->ContainsOnlyOperaInternalScripts())
			{
				if (value)
				{
					ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_feeds);
					if (result != GET_FAILED)
						return result;

					DOM_WebFeeds *feeds;
					GET_FAILED_IF_ERROR(DOM_WebFeeds::Make(feeds, GetRuntime()));
					GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_feeds, *feeds));

					DOMSetObject(value, feeds);
				}
				return GET_SUCCESS;
			}
		}
	}
# endif // DOM_FEEDS_ACCESS

	if (uni_str_eq(property_name, "feed"))
	{
		if (FramesDocument* frm_doc = GetFramesDocument())
		{
			if (frm_doc->ContainsOnlyOperaInternalScripts())
			{
				if (value)
				{
					ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_feed);
					if (result != GET_FAILED)
						return result;

					DOM_Feed *feed;
					URL doc_url = frm_doc->GetURL();

					GET_FAILED_IF_ERROR(DOM_Feed::Make(feed, doc_url, GetRuntime()));
					GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_feed, *feed));

					DOMSetObject(value, feed);
				}
				return GET_SUCCESS;
			}
		}
	}

# ifdef WEBFEEDS_EXTERNAL_READERS
	if (uni_str_eq(property_name, "feedreaders"))
	{
		if (FramesDocument* frm_doc = GetFramesDocument())
		{
			URL doc_url = frm_doc->GetOrigin()->security_context;
			BOOL allow_access_to_feedreaders = doc_url.Type() == URL_OPERA || frm_doc->ContainsOnlyOperaInternalScripts();

			if (allow_access_to_feedreaders)
			{
				if (value)
				{
					ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_feedreaders);
					if (result != GET_FAILED)
						return result;

					DOM_FeedReaderList *feedreaders;
					GET_FAILED_IF_ERROR(DOM_FeedReaderList::Make(feedreaders, GetRuntime()));
					GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_feedreaders, *feedreaders));

					DOMSetObject(value, feedreaders);
				}
				return GET_SUCCESS;
			}
		}
	}
# endif // WEBFEEDS_EXTERNAL_READERS
#endif // WEBFEEDS_BACKEND_SUPPORT

#if defined(DOM_GADGET_FILE_API_SUPPORT) || defined(WEBSERVER_SUPPORT)
	if (uni_str_eq(property_name, "io") && DOM_IO::AllowIOAPI(GetFramesDocument()))
	{
		if (value)
		{
			ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_io);
			if (result != GET_FAILED)
				return result;

			DOM_IO *io;
			GET_FAILED_IF_ERROR(DOM_IO::Make(io, GetRuntime()));
			GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_io, *io));

			DOMSetObject(value, io);
		}

		return GET_SUCCESS;
	}
#endif // DOM_GADGET_FILE_API_SUPPORT || WEBSERVER_SUPPORT

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	if (uni_str_eq(property_name, "nearbyDevices"))
	{
		// Enabled only on widgets
		BOOL feature_enabled = GetFramesDocument() && GetFramesDocument()->GetWindow()->GetType() == WIN_TYPE_GADGET;

#ifdef SELFTEST
		if (g_selftest.running)
			feature_enabled = TRUE;
#endif // SELFTEST

		if (feature_enabled)
		{
			if (value)
			{
				ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_upnp);
				if (result != GET_FAILED)
					return result;

				DOM_UPnPCollection *coll;

				GET_FAILED_IF_ERROR(DOM_UPnPCollection::MakeItemCollectionDevices(coll, GetRuntime(), UPNP_DISCOVERY_OPERA_SEARCH_TYPE));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_upnp, *coll));

				DOMSetObject(value, coll);
			}

			return GET_SUCCESS;
		}
		return GET_FAILED;
	}

	if (uni_str_eq(property_name, "nearbyDragonflies"))
	{
		// Only enabled on 'opera:debug'
		BOOL is_opera_debug = FALSE;

		LEAVE_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_OPERA_CONNECT,
			OpSecurityContext(static_cast<DOM_Runtime*>(GetRuntime())),
			OpSecurityContext(static_cast<DOM_Runtime*>(NULL)),
			is_opera_debug));

#ifdef SELFTEST
		if (g_selftest.running)
			is_opera_debug = TRUE;
#endif // SELFTEST

		if (is_opera_debug)
		{
			if (value)
			{
				ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_nearbydebuggers);
				if (result != GET_FAILED)
					return result;

				DOM_UPnPCollection *coll;

				GET_FAILED_IF_ERROR(DOM_UPnPCollection::MakeItemCollectionDevices(coll, GetRuntime(), UPNP_DISCOVERY_DRAGONFLY_SEARCH_TYPE));
				GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_nearbydebuggers, *coll));

				DOMSetObject(value, coll);
			}

			return GET_SUCCESS;
		}
		return GET_FAILED;
	}

#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

#if defined(VEGA_3DDEVICE) && defined(VEGA_BACKENDS_USE_BLOCKLIST)
	if (uni_str_eq(property_name, "hwa"))
	{
		if (FramesDocument* frm_doc = GetFramesDocument())
		{
			URL url = frm_doc->GetOrigin()->security_context;
			if (url.Type() == URL_HTTPS)
			{
				OpStringC bugwizard = g_pcjs->GetStringPref(PrefsCollectionJS::JS_HWAAccess);
				ServerName *sn = url.GetServerName();
				if (sn != NULL && sn->UniName() != NULL && uni_stri_eq(sn->UniName(), bugwizard.CStr()))
				{
					if (value)
					{
						ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_hwa);
						if (result != GET_FAILED)
							return result;

						JS_HWA *hwa;

						GET_FAILED_IF_ERROR(JS_HWA::Make(hwa, this));
						GET_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_hwa, *hwa));

						DOMSetObject(value, hwa);
					}

					return GET_SUCCESS;
				}
			}
		}
		return GET_FAILED;
	}
#endif // VEGA_3DDEVICE && VEGA_BACKENDS_USE_BLOCKLIST

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
JS_Opera::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	FramesDocument* frames_doc = GetFramesDocument();
	if (!frames_doc)
		return PUT_FAILED;

	switch (property_name)
	{
#ifdef DOM_INTEGRATED_DEVTOOLS_SUPPORT
	case OP_ATOM_attached:
		{
			Window* window = frames_doc->GetWindow();
			if (window->GetType() != WIN_TYPE_DEVTOOLS)
				return PUT_FAILED;

			if (value->type != VALUE_BOOLEAN)
				return PUT_NEEDS_BOOLEAN;

# ifdef WIC_USE_WINDOW_ATTACHMENT
			WindowCommander * wc = window->GetWindowCommander();
			wc->GetDocumentListener()->OnWindowAttachRequest(wc, value->value.boolean);
# endif // WIC_USE_WINDOW_ATTACHMENT

			return PUT_SUCCESS;
		}
#endif // DOM_INTEGRATED_DEVTOOLS_SUPPORT

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

#ifdef _DEBUG
// This code is used to test the suspendable Get and Put in the ECMAScript engine

/* virtual */ ES_GetState
JS_Opera::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (uni_str_eq(property_name, "hardToGet"))
	{
		if (RedoTestDelay(GetRuntime(), origining_runtime, value, restart_object))
			return GET_SUSPEND;

		if (saved_value_name != -1.0)
			DOMSetNumber( value, saved_value_name );
		else
			DOMSetString( value, UNI_L("GetName was restarted!") );
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_GetState
JS_Opera::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	if (property_index == 0)
	{
		if (value)
		{
			if (!SetupTestDelay( GetRuntime(), origining_runtime, value, 0.0 ))
				return GET_FAILED;
			else
				return GET_SUSPEND;
		}
		else
			return GET_SUCCESS;
	}
	return GET_FAILED;
}

/* virtual */ ES_GetState
JS_Opera::GetIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object)
{
	if (property_index == 0)
	{
		if (RedoTestDelay(GetRuntime(), origining_runtime, value, restart_object))
			return GET_SUSPEND;

		if (saved_value_index != -1.0)
			DOMSetNumber( value, saved_value_index );
		else
			DOMSetString( value, UNI_L("GetIndex was restarted!") );
		return GET_SUCCESS;
	}
	else
		return GET_FAILED;
}

/* virtual */ ES_PutState
JS_Opera::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (uni_str_eq(property_name, "hardToGet"))
	{
		if (!SetupTestDelay(GetRuntime(), GetRuntime(), value, 0.0))
			return PUT_FAILED;
		else
			return PUT_SUSPEND;
	}
	else
	if (uni_str_eq(property_name, "putWillConvert"))		// Tests for a regression
	{
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		saved_value_name = value->value.number;
		return PUT_SUCCESS;
	}
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	else
	if (uni_str_eq(property_name, "scriptStorage"))
		return PUT_SUCCESS;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
JS_Opera::PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object )
{
	if (uni_str_eq(property_name, "hardToGet"))
	{
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (RedoTestDelay(GetRuntime(), GetRuntime(), value, restart_object))
			return PUT_SUSPEND;
		else
		{
			saved_value_name = value->value.number;
			return PUT_SUCCESS;
		}
	}
	return PUT_FAILED;
}

/* virtual */ ES_PutState
JS_Opera::PutIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	if (property_index == 0)
	{
		if (!SetupTestDelay(GetRuntime(), origining_runtime, value, 0.0))
			return PUT_FAILED;
		else
			return PUT_SUSPEND;
	}
	return PUT_FAILED;
}

/* virtual */ ES_PutState
JS_Opera::PutIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object)
{
	if (property_index == 0)
	{
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (RedoTestDelay(GetRuntime(), origining_runtime, value, restart_object))
			return PUT_SUSPEND;
		else
		{
			saved_value_index = value->value.number;
			return PUT_SUCCESS;
		}
	}
	return PUT_FAILED;
}

/* virtual */ int
JS_Opera_hardToCall::Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime)
{
	if (argc == 0)
	{
		SetupTestDelay( GetRuntime(), origining_runtime, return_value, 0.0 );
		return ES_SUSPEND|ES_RESTART;
	}
	else if (argc > 0)
	{
		return ES_FAILED;
	}
	else
	{
		OP_ASSERT( argc == -1 );
		if (RedoTestDelay(GetRuntime(), origining_runtime, return_value, return_value->value.object))
			return ES_SUSPEND|ES_RESTART;
		else
		{
			ES_Value value;

			ES_Object* obj = origining_runtime->CreateHostObjectWrapper(NULL, NULL, "Restart", FALSE);

			if (obj == NULL)
				return ES_NO_MEMORY;

			value.type = VALUE_STRING;
			value.value.string = UNI_L("Construct was suspended!");

			if (OpStatus::IsError(origining_runtime->PutName(obj, UNI_L("myproperty"), value)))
				return ES_NO_MEMORY;
			DOMSetObject( return_value, obj );

			return ES_VALUE;
		}
	}
}

/* virtual */ int
JS_Opera_hardToCall::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	if (argc == 0)
	{
		SetupTestDelay( GetRuntime(), origining_runtime, return_value, 0.0 );
		return ES_SUSPEND|ES_RESTART;
	}
	else if (argc > 0)
	{
		*return_value = argv[0];
		return ES_VALUE;
	}
	else
	{
		OP_ASSERT( argc == -1 );
		if (RedoTestDelay(GetRuntime(), origining_runtime, return_value, return_value->value.object))
			return ES_SUSPEND|ES_RESTART;
		else
		{
			DOMSetString( return_value, UNI_L("Call was suspended!") );
			return ES_VALUE;
		}
	}
}

/* virtual */ int
JS_Opera_delay::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	if (argc == 1 && argv[0].type == VALUE_NUMBER)
	{
		FramesDocument  *frames_doc = origining_runtime->GetFramesDocument();
		if (frames_doc == NULL)
			return ES_FAILED;

		MessageHandler  *mh         = frames_doc->GetMessageHandler();
		ES_Thread       *thread     = origining_runtime->GetESScheduler()->GetCurrentThread();

		JS_OperaTestRestarter *test = OP_NEW(JS_OperaTestRestarter, (mh, thread));
		if (test == NULL)
			return ES_NO_MEMORY;

		return_value->type = VALUE_NULL;

		mh->SetCallBack(test, MSG_ES_ASYNC_MESSAGE, 0);
		mh->PostDelayedMessage(MSG_ES_ASYNC_MESSAGE, 0, 0, (int) argv[0].value.number);

		return ES_RESTART | ES_SUSPEND;
	}
	else
		return ES_FAILED;
}

OP_STATUS
JS_Opera_AsyncCallback::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	OP_ASSERT(!"Removed");
	return OpStatus::OK;
}

/* virtual */ int
JS_Opera_asyncEval::Call(ES_Object* native_this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	if (argc >= 1 && argv[0].type == VALUE_STRING)
	{
		DOM_Object *this_object = DOM_HOSTOBJECT(native_this_object, DOM_Object);
		DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

		int count;

		for (count = 0; count + 1 < argc && argv[count + 1].type == VALUE_OBJECT; ++count)
		{
			// Find the first non-object among the arguments. We'll not include those.
		}

		if (count > 0)
		{
			int index;
			ES_Object **scope_chain = OP_NEWA(ES_Object*, count);

			for (index = 0; index < count; ++index)
				scope_chain[index] = argv[index + 1].value.object;

			GetEnvironment()->GetAsyncInterface()->Eval(argv[0].value.string, scope_chain, count, &opera->asyncCallback);
			OP_DELETEA(scope_chain);
		}
		else
			GetEnvironment()->GetAsyncInterface()->Eval(argv[0].value.string, &opera->asyncCallback);
	}

	return ES_FAILED;
}

/* virtual */ int
JS_Opera_asyncCallMethod::Call(ES_Object* native_this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	if (argc >= 2 && argv[0].type == VALUE_OBJECT && argv[1].type == VALUE_STRING)
	{
		DOM_Object *this_object = DOM_HOSTOBJECT(native_this_object, DOM_Object);
		DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

		GetEnvironment()->GetAsyncInterface()->CallMethod(argv[0].value.object, argv[1].value.string, argc - 2, argv + 2, &opera->asyncCallback);
	}

	return ES_FAILED;
}

#ifdef ESUTILS_ASYNC_SLOT_SUPPORT

/* virtual */ int
JS_Opera_asyncGetSlot::Call(ES_Object* native_this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	if (argc == 2 && argv[0].type == VALUE_OBJECT && argv[1].type == VALUE_STRING || argv[1].type == VALUE_NUMBER)
	{
		DOM_Object *this_object = DOM_HOSTOBJECT(native_this_object, DOM_Object);
		DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

		if (argv[1].type == VALUE_STRING)
			GetEnvironment()->GetAsyncInterface()->GetSlot(argv[0].value.object, argv[1].value.string, &opera->asyncCallback);
		else
			GetEnvironment()->GetAsyncInterface()->GetSlot(argv[0].value.object, (int) argv[1].value.number, &opera->asyncCallback);
	}

	return ES_FAILED;
}

/* virtual */ int
JS_Opera_asyncSetSlot::Call(ES_Object* native_this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	if (argc == 3 && argv[0].type == VALUE_OBJECT && argv[1].type == VALUE_STRING || argv[1].type == VALUE_NUMBER)
	{
		DOM_Object *this_object = DOM_HOSTOBJECT(native_this_object, DOM_Object);
		DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

		if (argv[1].type == VALUE_STRING)
			GetEnvironment()->GetAsyncInterface()->SetSlot(argv[0].value.object, argv[1].value.string, argv[2], &opera->asyncCallback);
		else
			GetEnvironment()->GetAsyncInterface()->SetSlot(argv[0].value.object, (int) argv[1].value.number, argv[2], &opera->asyncCallback);
	}

	return ES_FAILED;
}

#endif // ESUTILS_ASYNC_SLOT_SUPPORT

/* virtual */ int
JS_Opera_cancelThisThread::Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime)
{
	ES_Thread *thread = GetCurrentThread(origining_runtime);
	CALL_FAILED_IF_ERROR(thread->GetScheduler()->CancelThread(thread));
	return ES_FAILED;
}

#endif // _DEBUG

/* static */ int
JS_Opera::buildNumber(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	return_value->type = VALUE_STRING;
	return_value->value.string = VER_BUILD_IDENTIFIER_UNI;
	return ES_VALUE;
}

/* static */ int
JS_Opera::version(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	return_value->type = VALUE_STRING;
	return_value->value.string = VER_NUM_STR_UNI;
	return ES_VALUE;
}

/* static */ int
JS_Opera::collect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	g_ecmaManager->ScheduleGarbageCollect();
	return_value->type = VALUE_UNDEFINED;
	return ES_VALUE;
}

#ifdef OPERA_CONSOLE

static OP_STATUS
DOM_PostMessageToConsole(OpConsoleEngine::Message *message)
{
	if (!g_console->IsLogging())
		return OpStatus::OK;

	TRAPD(status, g_console->PostMessageL(message));
	return status;
}

/* static */ int
JS_Opera::postError(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	// Since this is a potential slow loop we'll limit the number
	// of arguments to prevent accidental (or malicious) hangs.
	const int MAX_ARGS_TO_POST_ERROR = 500;
	if (argc > MAX_ARGS_TO_POST_ERROR)
		argc = MAX_ARGS_TO_POST_ERROR;

	OpConsoleEngine::Message message(OpConsoleEngine::EcmaScript, OpConsoleEngine::Information);
	CALL_FAILED_IF_ERROR(message.context.Set(GetCurrentThread(origining_runtime)->GetInfoString()));
	if (FramesDocument *frames_doc = origining_runtime->GetFramesDocument())
		message.window = frames_doc->GetWindow()->Id();
	CALL_FAILED_IF_ERROR(origining_runtime->GetDisplayURL(message.url));

	for (int index = 0; index < argc; index++)
	{
		// All arguments are strings because of the format string used in AddFunctionL.
		OP_ASSERT(argv[index].type == VALUE_STRING);
		CALL_FAILED_IF_ERROR(message.message.Set(argv[index].value.string));
		CALL_FAILED_IF_ERROR(DOM_PostMessageToConsole(&message));
	}

	return ES_FAILED;
}

#endif // OPERA_CONSOLE

#ifdef USER_JAVASCRIPT

/* static */ int
JS_Opera::defineMagicFunction(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("so");

	DOM_EnvironmentImpl *environment = opera->GetEnvironment();

	if (DOM_UserJSManager *userjsmanager = environment->GetUserJSManager())
		if (userjsmanager->GetIsActive(origining_runtime))
		{
			OP_ASSERT(environment->GetWindow()->IsA(DOM_TYPE_WINDOW));
			CALL_FAILED_IF_ERROR(static_cast<JS_Window *>(environment->GetWindow())->AddMagicFunction(argv[0].value.string, argv[1].value.object));
		}

	return ES_FAILED;
}

/* static */ int
JS_Opera::defineMagicVariable(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("sOO");

	DOM_EnvironmentImpl *environment = opera->GetEnvironment();

	if (DOM_UserJSManager *userjsmanager = environment->GetUserJSManager())
		if (userjsmanager->GetIsActive(origining_runtime))
		{
			OP_ASSERT(environment->GetWindow()->IsA(DOM_TYPE_WINDOW));
			JS_Window *window = static_cast<JS_Window *>(environment->GetWindow());
			const uni_char *name = argv[0].value.string;
			CALL_FAILED_IF_ERROR(window->GetRuntime()->DeleteName(*window, name));
			OP_ASSERT(window->GetRuntime()->GetName(*window, name, NULL) == OpBoolean::IS_FALSE || !"Magic variable is unable to override native property");
			CALL_FAILED_IF_ERROR(window->AddMagicVariable(name, argv[1].type == VALUE_OBJECT ? argv[1].value.object : NULL, argv[2].type == VALUE_OBJECT ? argv[2].value.object : NULL));
		}

	return ES_FAILED;
}

/* static */ int
JS_Opera::accessOverrideHistoryNavigationMode(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

	if (data == 1)
		DOM_CHECK_ARGUMENTS("s");

	DOM_EnvironmentImpl *environment = opera->GetEnvironment();
	FramesDocument *document = environment->GetFramesDocument();

	if (!document)
		return DOM_CALL_INTERNALEXCEPTION(RESOURCE_UNAVAILABLE_ERR);

#ifdef USER_JAVASCRIPT
	if (DOM_UserJSManager *userjsmanager = environment->GetUserJSManager())
		if (userjsmanager->GetIsActive(origining_runtime))
		{
			if (data == 0)
			{
				const uni_char *mode;

				switch (document->GetOverrideHistoryNavigationMode())
				{
				case HISTORY_NAV_MODE_AUTOMATIC:
					mode = UNI_L("automatic");
					break;

				case HISTORY_NAV_MODE_COMPATIBLE:
					mode = UNI_L("compatible");
					break;

				default:
					mode = UNI_L("fast");
				}

				DOMSetString(return_value, mode);
			}
			else
			{
				const uni_char *argument = argv[0].value.string;
				HistoryNavigationMode mode;

				if (uni_stri_eq(argument, UNI_L("automatic")))
					mode = HISTORY_NAV_MODE_AUTOMATIC;
				else if (uni_stri_eq(argument, UNI_L("compatible")))
					mode = HISTORY_NAV_MODE_COMPATIBLE;
				else if (uni_stri_eq(argument, UNI_L("fast")))
					mode = HISTORY_NAV_MODE_FAST;
				else
					return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

				if (document->GetOverrideHistoryNavigationMode() != mode)
				{
					document->SetOverrideHistoryNavigationMode(mode);
					DOMSetBoolean(return_value, TRUE);
				}
				else
					DOMSetBoolean(return_value, FALSE);
			}

			return ES_VALUE;
		}
#endif // USER_JAVASCRIPT

	return ES_FAILED;
}

#endif // USER_JAVASCRIPT

#ifdef DOM_PREFERENCES_ACCESS

OP_BOOLEAN
DOM_AccessPreference(const uni_char *section, const uni_char *key, OpString &value, int data, const uni_char *host = NULL)
{
	OpString8 section8s, key8s;

	RETURN_IF_ERROR(section8s.Set(section));
	RETURN_IF_ERROR(key8s.Set(key));

	const char *section8 = section8s.CStr(), *key8 = key8s.CStr();
	OP_MEMORY_VAR BOOL success = FALSE;

#ifdef PREFS_WRITE
	OP_ASSERT(data == 1 && !host || 1); // can't get a default value for a host

	if (data != 2)
		RETURN_IF_LEAVE(success = g_prefsManager->GetPreferenceL(section8, key8, value, data == 1, host));
	else
	{
# ifdef PREFS_HOSTOVERRIDE
		RETURN_IF_LEAVE(success = host
						? g_prefsManager->OverridePreferenceL(host, section8, key8, value, TRUE)
						: g_prefsManager->WritePreferenceL(section8, key8, value));
# else
		RETURN_IF_LEAVE(success = g_prefsManager->WritePreferenceL(section8, key8, value));
# endif // PREFS_HOSTOVERRIDE
	}
#else // PREFS_WRITE
	RETURN_IF_LEAVE(success = g_prefsManager->GetPreferenceL(section8, key8, value));
#endif // PREFS_WRITE

	return success ? OpBoolean::IS_TRUE : OpBoolean::IS_FALSE;
}

#endif // DOM_PREFERENCES_ACCESS

#ifdef DOM_TO_PLATFORM_MESSAGES
class DOM_PlatformMessageContext :
	public OpPlatformMessageListener::PlatformMessageContext,
	public DOM_Object,
	public ES_ThreadListener
{
private:
	ES_Thread *thread;

public:
	BOOL called;
	const uni_char *return_string;

	DOM_PlatformMessageContext(ES_Thread *t)
		: thread(t),
		  called(FALSE),
		  return_string(NULL)
	{
	}

	~DOM_PlatformMessageContext()
	{
		if (return_string)
			op_free((void*)return_string);
	}

	/* From OpPlatformMessageListener::PlatformMessageContext: */
	virtual void Return(const uni_char *answer);

	/* From ES_ThreadListener: */
	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal);

	void Block();
};

void
DOM_PlatformMessageContext::Return(const uni_char *answer)
{
	called = TRUE;

	if (thread)
	{
		return_string = uni_strdup(answer); // can't do much if OOM occurs, ignore and carry on

		OpStatus::Ignore(thread->Unblock(ES_BLOCK_USER_INTERACTION));
	}

	GetRuntime()->Unprotect(*this);
}

OP_STATUS
DOM_PlatformMessageContext::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_FAILED || signal == ES_SIGNAL_CANCELLED || signal == ES_SIGNAL_FINISHED)
	{
		ES_ThreadListener::Remove();
		thread = NULL;
	}

	return OpStatus::OK;
}

void
DOM_PlatformMessageContext::Block()
{
	thread->Block(ES_BLOCK_USER_INTERACTION);
	thread->AddListener(this);
}

/* static */ int
JS_Opera::sendPlatformMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("s");
		DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

		WindowCommander *wc = origining_runtime->GetFramesDocument()->GetWindow()->GetWindowCommander();
		ES_Thread *thread = GetCurrentThread(origining_runtime);

		DOM_PlatformMessageContext *context;

		CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(context = OP_NEW(DOM_PlatformMessageContext, (thread)), origining_runtime));

		if (!origining_runtime->Protect(*context))
			return ES_NO_MEMORY;

		TempBuffer buf;
		CALL_FAILED_IF_ERROR(origining_runtime->GetSerializedOrigin(buf));

		wc->GetPlatformMessageListener()->OnMessage(wc, context, argv->value.string, buf.GetStorage());

		// If callback was called, don't freeze thread
		if (context->called)
		{
			DOMSetString(return_value, context->return_string);
			return ES_VALUE;
		}
		else
		{
			context->Block();
			DOMSetObject(return_value, context);
			return ES_SUSPEND | ES_RESTART;
		}
	}
	else
	{
		DOM_PlatformMessageContext *context = DOM_VALUE2OBJECT(*return_value, DOM_PlatformMessageContext);
		DOMSetString(return_value, context->return_string);
		return ES_VALUE;
	}
}
#endif // DOM_TO_PLATFORM_MESSAGES

#ifdef DOM_LOCALE_SUPPORT
/* static */ int
JS_Opera::getLocaleString(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("s");

	OpString localized_string;
	Str::LocaleString string_identifier(argv[0].value.string);
	CALL_FAILED_IF_ERROR(g_languageManager->GetString(string_identifier, localized_string));

	TempBuffer *buffer = GetEmptyTempBuf();
	CALL_FAILED_IF_ERROR(buffer->Append(localized_string.CStr()));
	DOMSetString(return_value, buffer);
	return ES_VALUE;
}
#endif // DOM_LOCALE_SUPPORT

#ifdef CPUUSAGETRACKING
/* static */ int
JS_Opera::getCPUUsage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	// The arguments is an array of numbers. Each number represents one
	// call to CPUUsageTracker::GetLastXSecondAverage() with that number as
	// second arguments (so you can fetch the average over 5, 10, 30 or any
	// other number of seconds).
	DOM_CHECK_ARGUMENTS("o");
	ES_Object* second_array = argv[0].value.object;
	ES_Value get_value;
	CALL_FAILED_IF_ERROR(origining_runtime->GetName(second_array, UNI_L("length"), &get_value));
	unsigned second_array_length = TruncateDoubleToUInt(get_value.value.number);

	OpString string_fallback_process_name;
	OpStatus::Ignore(g_languageManager->GetString(Str::S_OPERACPU_FALLBACK_PROCESS_NAME, string_fallback_process_name));

	ES_Value value;
	ES_Object* return_obj;
	CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&return_obj));

	// Add an array with the total load for each measurement.
	ES_Object* total_data;
	CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&total_data, second_array_length));
	DOMSetObject(&value, total_data);
	CALL_FAILED_IF_ERROR(origining_runtime->PutName(return_obj, UNI_L("total"), value));

	unsigned tracker_count = 0;
	CPUUsageTrackers::Iterator iterator;
	g_cputrackers->StartIterating(iterator);
	while (CPUUsageTracker* tracker = iterator.GetNext())
	{
		ES_Object* tracker_data;
		CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&tracker_data, second_array_length));
		DOMSetObject(&value, tracker_data);
		CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(return_obj, tracker_count, value));

		if (tracker->IsFallbackTracker())
			DOMSetString(&value, string_fallback_process_name.CStr());
		else
			DOMSetString(&value, tracker->GetDisplayName());

		CALL_FAILED_IF_ERROR(origining_runtime->PutName(tracker_data, UNI_L("displayName"), value));

		DOMSetNumber(&value, tracker->GetId());
		CALL_FAILED_IF_ERROR(origining_runtime->PutName(tracker_data, UNI_L("id"), value));

		DOMSetBoolean(&value, tracker->IsActivatable());
		CALL_FAILED_IF_ERROR(origining_runtime->PutName(tracker_data, UNI_L("activatable"), value));

		tracker_count++;
	}

	double now = g_op_time_info->GetRuntimeMS();

	for (unsigned i = 0; i < second_array_length; i++)
	{
		unsigned tracker_index = 0;
		double total = 0.0; // Total time for all trackers.
		CALL_FAILED_IF_ERROR(origining_runtime->GetIndex(second_array, i, &get_value));
		int seconds = TruncateDoubleToInt(get_value.value.number);
		CPUUsageTrackers::Iterator iterator;
		g_cputrackers->StartIterating(iterator);
		while (CPUUsageTracker* tracker = iterator.GetNext())
		{
			CALL_FAILED_IF_ERROR(origining_runtime->GetIndex(return_obj, tracker_index, &get_value));
			ES_Object* tracker_data = get_value.value.object;

			double average = tracker->GetLastXSecondAverage(now, seconds);
			total += average;
			DOMSetNumber(&value, average);
			CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(tracker_data, i, value));

			tracker_index++;
		}

		DOMSetNumber(&value, total);
		CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(total_data, i, value));
	}

	DOMSetObject(return_value, return_obj);
	return ES_VALUE;
}

/* static */ int
JS_Opera::getCPUUsageSamples(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("n");
	unsigned id = TruncateDoubleToUInt(argv[0].value.number);
	CPUUsageTrackers::Iterator iterator;
	g_cputrackers->StartIterating(iterator);
	while (CPUUsageTracker* tracker = iterator.GetNext())
	{
		if (tracker->GetId() == id)
		{
			ES_Object* tracker_data;
			CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&tracker_data, CPUUsageTracker::SAMPLE_LENGTH));
			DOMSetObject(return_value, tracker_data);
			double samples[CPUUsageTracker::SAMPLE_LENGTH]; // ARRAY OK - bratell 2012-06-30
			tracker->GetLastXSeconds(g_op_time_info->GetRuntimeMS(), samples);
			for (unsigned int i = 0; i < CPUUsageTracker::SAMPLE_LENGTH; i++)
			{
				ES_Value value;
				DOMSetNumber(&value, samples[i]);
				CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(tracker_data, i, value));
			}
			return ES_VALUE;
		}
	}
	return ES_FAILED;
}

/* static */ int
JS_Opera::activateCPUUser(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("n");
	unsigned id = TruncateDoubleToUInt(argv[0].value.number);
	for (Window* window = g_windowManager->FirstWindow(); window; window = window->Suc())
	{
		if (window->GetCPUUsageTracker()->GetId() == id)
		{
			window->Raise();
			return ES_FAILED;
		}
	}

	return ES_FAILED;
}
#endif // CPUUSAGETRACKING

#ifdef DOM_PREFERENCES_ACCESS

static OP_STATUS AllowDomPreferencesAccessForUrl(URL origin, BOOL *allowed)
{
	*allowed = FALSE;

	if (origin.Type() == URL_OPERA)
	{
		OpString value;
		RETURN_IF_ERROR(origin.GetAttribute(URL::KUniName, value));
		if (
#ifdef OPERACONFIG_URL
			value.CompareI("opera:config") == 0 ||
#endif // OPERACONFIG_URL
#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
			value.CompareI("opera:" DATABASE_ABOUT_WEBSTORAGE_URL) == 0 ||
#endif // DATABASE_ABOUT_WEBSTORAGE_URL
#ifdef DATABASE_ABOUT_WEBDATABASES_URL
			value.CompareI("opera:" DATABASE_ABOUT_WEBDATABASES_URL) == 0 ||
#endif // DATABASE_ABOUT_WEBDATABASES_URL
#ifdef ABOUT_PRIVATE_BROWSING
			value.CompareI("opera:private") == 0 ||
#endif // ABOUT_PRIVATE_BROWSING
#ifdef _PLUGIN_SUPPORT_
			value.CompareI("opera:plugins") == 0 ||
#endif // _PLUGIN_SUPPORT_
			FALSE)
			*allowed = TRUE;
	}

	return OpStatus::OK;
}

/* static */ int
JS_Opera::accessPreference(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
#ifdef PREFS_HOSTOVERRIDE
	DOM_CHECK_ARGUMENTS((data == 2) ? "sss|s" : ((data == 1) ? "ss" : "ss|s"));
#else
	DOM_CHECK_ARGUMENTS((data == 2) ? "sss" : "ss");
#endif // PREFS_HOSTOVERRIDE

	BOOL allowed;
	CALL_FAILED_IF_ERROR(AllowDomPreferencesAccessForUrl(origining_runtime->GetOriginURL(), &allowed));

#ifdef USER_JAVASCRIPT
#if 0 // Not safe yet.
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_EnvironmentImpl *environment = opera->GetEnvironment();
	if (DOM_UserJSManager *userjsmanager = environment->GetUserJSManager())
		if (userjsmanager->GetIsActive(origining_runtime))
			allowed = TRUE;
#endif // 0
#endif // USER_JAVASCRIPT

	if (!allowed)
		return ES_FAILED;

	OP_BOOLEAN result;
	OpString value;

#ifdef PREFS_WRITE
	if (data == 2)
		CALL_FAILED_IF_ERROR(value.Set(argv[2].value.string));
	else
#endif // PREFS_WRITE
		CALL_FAILED_IF_ERROR(value.Set(""));

	const uni_char *host = NULL;
#ifdef PREFS_HOSTOVERRIDE
	if (data == 0 && argc > 2)
		host = argv[2].value.string;
	else if (data == 2 && argc > 3)
		host = argv[3].value.string;
#endif // PREFS_HOSTOVERRIDE

	CALL_FAILED_IF_ERROR(result = DOM_AccessPreference(argv[0].value.string, argv[1].value.string, value, data, host));

	if (data != 2)
		if (result == OpBoolean::IS_TRUE)
		{
			TempBuffer *buffer = GetEmptyTempBuf();

			CALL_FAILED_IF_ERROR(buffer->Append(value.CStr()));
			DOMSetString(return_value, buffer);
		}
		else
			DOMSetNull(return_value);
#ifdef PREFS_WRITE
	else
		DOMSetBoolean(return_value, result == OpBoolean::IS_TRUE);
#endif // PREFS_WRITE

	return ES_VALUE;
}

static OP_STATUS CommitPrefs()
{
	TRAPD(rc, g_prefsManager->CommitL());
	return rc;
}

/* static */ int
JS_Opera::commitPreferences(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	BOOL allowed;
	CALL_FAILED_IF_ERROR(AllowDomPreferencesAccessForUrl(origining_runtime->GetOriginURL(), &allowed));
	if (allowed)
		OpStatus::Ignore(CommitPrefs());

	return ES_FAILED;
}

#endif // DOM_PREFERENCES_ACCESS

#if defined _DEBUG

#ifdef XMLUTILS_XMLSERIALIZER_SUPPORT
# include "modules/xmlutils/xmlserializer.h"
# include "modules/dom/src/domcore/domdoc.h"
# include "modules/dom/src/domcore/element.h"

class DOM_serializeToXML_Callback
	: public XMLSerializer::Callback
{
public:
	DOM_serializeToXML_Callback()
		: completed(FALSE), failed(FALSE), oom(FALSE)
	{
	}

	virtual void Stopped(Status status)
	{
		if (status == STATUS_COMPLETED)
			completed = TRUE;
		else if (status == STATUS_FAILED)
			failed = TRUE;
		else
			oom = TRUE;
	}

	BOOL completed, failed, oom;
};

/* static */ int
JS_Opera::serializeToXML(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(node, 0, DOM_TYPE_NODE, DOM_Node);

	if (!opera->GetFramesDocument())
		return ES_FAILED;

	DOM_Node *root_node, *target_node;

	if (node->IsA(DOM_TYPE_DOCUMENT))
		root_node = target_node = static_cast<DOM_Document *>(node)->GetRootElement();
	else if (node->IsA(DOM_TYPE_ELEMENT))
	{
		root_node = node->GetOwnerDocument()->GetRootElement();
		target_node = node;
	}
	else
	{
		DOMSetString(return_value, UNI_L("argument must be a document node or an element node"));
		return ES_EXCEPTION;
	}

	if (!root_node || !target_node)
		return ES_FAILED;

	HTML_Element *root = root_node->GetThisElement(), *target = target_node->GetThisElement();
	DOM_serializeToXML_Callback callback;
	XMLSerializer *serializer;
	TempBuffer *buffer = GetEmptyTempBuf();

	CALL_FAILED_IF_ERROR(XMLSerializer::MakeToStringSerializer(serializer, buffer));

	XMLSerializer::Configuration configuration;

	configuration.document_information = NULL;
	configuration.split_cdata_sections = TRUE;
	configuration.normalize_namespaces = TRUE;
	configuration.discard_default_content = TRUE;
	configuration.format_pretty_print = FALSE;
	configuration.encoding = "iso-8859-1";

	CALL_FAILED_IF_ERROR(serializer->SetConfiguration(configuration));

	CALL_FAILED_IF_ERROR(serializer->Serialize(opera->GetFramesDocument()->GetMessageHandler(), opera->GetFramesDocument()->GetURL(), root, target, &callback));

	OP_DELETE(serializer);

	if (callback.failed)
	{
		DOMSetString(return_value, UNI_L("serialize failed"));
		return ES_EXCEPTION;
	}
	else if (callback.oom)
		return ES_NO_MEMORY;

	OP_ASSERT(callback.completed);

	DOMSetString(return_value, buffer);
	return ES_VALUE;
}

#endif // XMLUTILS_XMLSERIALIZER_SUPPORT

#endif // _DEBUG

#ifdef DOM_XSLT_SUPPORT
#ifndef XSLT_MORPH_2
# include "modules/xmlutils/xmlserializer.h"
# include "modules/xmlutils/xmlnames.h"
# include "modules/xslt/xslt.h"
# include "modules/ecmascript_utils/esthread.h"
# include "modules/dom/src/domcore/element.h"
# include "modules/dom/src/domcore/domdoc.h"

class DOM_pushXSLTransform_State
	: public DOM_Object,
	  public XMLSerializer::Callback,
	  public ES_ThreadListener
{
public:
	DOM_pushXSLTransform_State(XSLT_StylesheetParser *parser, XMLSerializer *serializer)
		: thread(NULL),
		  parser(parser),
		  serializer(serializer),
		  called(FALSE),
		  failed(FALSE),
		  oom(FALSE)
	{
	}

	void SetThread(ES_Thread *newthread)
	{
		thread = newthread;
		thread->AddListener(this);
		thread->Block();
	}

	virtual ~DOM_pushXSLTransform_State();
	virtual void Stopped(XMLSerializer::Callback::Status status);
	virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

	ES_Thread *thread;
	XSLT_StylesheetParser *parser;
	XMLSerializer *serializer;
	BOOL called, failed, oom;
};

DOM_pushXSLTransform_State::~DOM_pushXSLTransform_State()
{
	OP_DELETE(serializer);
	XSLT_StylesheetParser::Free(parser);
}

/* virtual */ void
DOM_pushXSLTransform_State::Stopped(XMLSerializer::Callback::Status status)
{
	failed = status == XMLSerializer::Callback::STATUS_FAILED;
	oom = status == XMLSerializer::Callback::STATUS_OOM;

	if (thread)
		OpStatus::Ignore(thread->Unblock());
}

/* virtual */ OP_STATUS
DOM_pushXSLTransform_State::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	if (signal == ES_SIGNAL_CANCELLED || signal == ES_SIGNAL_FINISHED || signal == ES_SIGNAL_FAILED)
	{
		thread = NULL;
		GetRuntime()->Unprotect(*this);
	}
	return OpStatus::OK;
}

/* static */ int
JS_Opera::pushXSLTransform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

	XSLT_StylesheetParser *parser;
	XMLSerializer *serializer;

	if (argc >= 0)
	{
		if (!opera->OriginCheck(origining_runtime))
			return ES_EXCEPT_SECURITY;

		DOMSetBoolean(return_value, FALSE);

		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT(transform, 0, DOM_TYPE_NODE, DOM_Node);

		DOM_Document *ownerDocument = NULL;
		HTML_Element *element = NULL;

		if (transform->IsA(DOM_TYPE_ELEMENT))
		{
			element = ((DOM_Element *) transform)->GetThisElement();
			ownerDocument = ((DOM_Element *) transform)->GetOwnerDocument();
		}
		else if (transform->IsA(DOM_TYPE_DOCUMENT))
		{
			if (DOM_Element *root = ((DOM_Document *) transform)->GetRootElement())
				element = root->GetThisElement();
			ownerDocument = (DOM_Document *) transform;
		}

		if (!element)
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		XMLExpandedName name(element);

		if (!name.IsXSLT() || !(uni_str_eq(name.GetLocalPart(), "stylesheet") || uni_str_eq(name.GetLocalPart(), "transform")))
			return DOM_CALL_DOMEXCEPTION(TYPE_MISMATCH_ERR);

		if (FramesDocument *frames_doc = opera->GetFramesDocument())
			if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
			{
				CALL_FAILED_IF_ERROR(XSLT_StylesheetParser::Make(parser, frames_doc, logdoc));

				if (OpStatus::IsMemoryError(XMLLanguageParser::MakeSerializer(serializer, parser)))
				{
					XSLT_StylesheetParser::Free(parser);
					return ES_NO_MEMORY;
				}

				DOM_pushXSLTransform_State *state = OP_NEW(DOM_pushXSLTransform_State, (parser, serializer));

				if (!state)
				{
					XSLT_StylesheetParser::Free(parser);
					OP_DELETE(serializer);
					return ES_NO_MEMORY;
				}

				CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(state, opera->GetRuntime(), opera->GetRuntime()->GetObjectPrototype(), "State"));
				CALL_FAILED_IF_ERROR(serializer->Serialize(frames_doc->GetMessageHandler(), ownerDocument->GetURL(), element, NULL, state));

				if (!state->called)
				{
					state->SetThread(GetCurrentThread(origining_runtime));

					DOMSetObject(return_value, state);
					return ES_SUSPEND | ES_RESTART;
				}
				else if (state->failed)
					return ES_VALUE;
				else if (state->oom)
					return ES_NO_MEMORY;
			}
			else
				return ES_VALUE;
		else
			return ES_VALUE;
	}
	else
	{
		DOM_pushXSLTransform_State *state = DOM_VALUE2OBJECT(*return_value, DOM_pushXSLTransform_State);
		parser = state->parser;

		if (state->failed)
			return ES_VALUE;
		else if (state->oom)
			return ES_NO_MEMORY;

		DOMSetBoolean(return_value, FALSE);
	}

	if (FramesDocument *frames_doc = opera->GetFramesDocument())
		if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
		{
			XSLT_Stylesheet *stylesheet;

			OP_STATUS status = parser->GetStylesheet(stylesheet);

			if (OpStatus::IsSuccess(status))
				status = logdoc->PushXSLTStylesheet(stylesheet);

			if (OpStatus::IsMemoryError(status))
				return ES_NO_MEMORY;
			else if (OpStatus::IsSuccess(status))
				DOMSetBoolean(return_value, TRUE);
		}

	return ES_VALUE;
}

/* static */ int
JS_Opera::popXSLTransform(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

	if (FramesDocument *frames_doc = opera->GetFramesDocument())
		if (LogicalDocument *logdoc = frames_doc->GetLogicalDocument())
			CALL_FAILED_IF_ERROR(logdoc->PopXSLTStylesheet());

	return ES_FAILED;
}

#endif // XSLT_MORPH_2
#endif // DOM_XSLT_SUPPORT

#ifdef DOM_INVOKE_ACTION_SUPPORT

#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"

/* static */ int
JS_Opera::invokeAction(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("s");

	BOOL handled = FALSE;

	if (FramesDocument *frames_doc = opera->GetFramesDocument())
		if (frames_doc->GetOrigin()->security_context.Type() == URL_OPERA)
		{
			OpInputAction *action = NULL;

			CALL_FAILED_IF_ERROR(OpInputAction::CreateInputActionsFromString(argv[0].value.string, action));

			if (action)
			{
				handled = g_input_manager->InvokeAction(action, frames_doc->GetVisualDevice());
				OpInputAction::DeleteInputActions(action);
			}
		}

	DOMSetBoolean(return_value, handled);
	return ES_VALUE;
}

#endif // DOM_INVOKE_ACTION_SUPPORT


#ifdef DOM_BENCHMARKXML_SUPPORT

#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/pi/OpSystemInfo.h"

class DOM_DummyTokenHandler
	: public XMLTokenHandler
{
public:
	virtual XMLTokenHandler::Result HandleToken(XMLToken &token);
};

/* virtual */ XMLTokenHandler::Result
DOM_DummyTokenHandler::HandleToken(XMLToken &token)
{
	return XMLTokenHandler::RESULT_OK;
}

static OP_STATUS
DOM_RetrieveData(URL_DataDescriptor *urldd, BOOL &more)
{
	TRAPD(status, urldd->RetrieveDataL(more));
	return status;
}

/* static */ int
JS_Opera::benchmarkXML(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("sn");

	URL url = g_url_api->GetURL(origining_runtime->GetFramesDocument()->GetURL(), argv[0].value.string), ref_url;
	if (url.Type() == URL_NULL_TYPE || !url.QuickLoad(TRUE))
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	URL_DataDescriptor *urldd = url.GetDescriptor(origining_runtime->GetFramesDocument()->GetMessageHandler(), TRUE, FALSE, TRUE, NULL, URL_XML_CONTENT);
	if (!urldd)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	unsigned length = 0;
	BOOL more = TRUE;

	while (more)
	{
		CALL_FAILED_IF_ERROR(DOM_RetrieveData(urldd, more));

		length += urldd->GetBufSize() /  sizeof(uni_char);

		urldd->ConsumeData((urldd->GetBufSize() /  sizeof(uni_char)) * sizeof(uni_char));
	}
	OP_DELETE(urldd);

	uni_char *data = OP_NEWA(uni_char, length);
	if (!data)
		return ES_NO_MEMORY;
	ANCHOR_ARRAY(uni_char, data);

	urldd = url.GetDescriptor(origining_runtime->GetFramesDocument()->GetMessageHandler(), TRUE);
	if (!urldd)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	more = TRUE;
	char *ptr = (char *) data;

	while (more)
	{
		CALL_FAILED_IF_ERROR(DOM_RetrieveData(urldd, more));

		op_memcpy(ptr, urldd->GetBuffer(), urldd->GetBufSize());

		ptr += urldd->GetBufSize();

		urldd->ConsumeData(urldd->GetBufSize());
	}
	OP_DELETE(urldd);

	double before = g_op_time_info->GetRuntimeMS();

	for (unsigned index = 0, count = (unsigned) argv[1].value.number; index < count; ++index)
	{
		DOM_DummyTokenHandler *tokenhandler = OP_NEW(DOM_DummyTokenHandler, ());
		if (!tokenhandler)
			return ES_NO_MEMORY;

		XMLParser *xmlparser;
		if (OpStatus::IsMemoryError(XMLParser::Make(xmlparser, NULL, origining_runtime->GetFramesDocument(), tokenhandler, url)))
		{
			OP_DELETE(tokenhandler);
			return ES_NO_MEMORY;
		}

		xmlparser->SetOwnsTokenHandler();

		XMLParser::Configuration configuration;
		configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_NO;
		configuration.max_tokens_per_call = 0;
		xmlparser->SetConfiguration(configuration);

		OP_STATUS status = xmlparser->Parse(data, length, FALSE);
		OP_DELETE(xmlparser);

		if (OpStatus::IsError(status))
			return OpStatus::IsMemoryError(status) ? ES_NO_MEMORY : DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	}

	DOMSetNumber(return_value, g_op_time_info->GetRuntimeMS() - before);
	return ES_VALUE;
}

#endif // DOM_BENCHMARKXML_SUPPORT

#ifdef OPERABOOKMARKS_URL

static BOOL
DOM_AllowBookmarksAccess(DOM_Runtime *origining_runtime)
{
	URL origin = origining_runtime->GetOriginURL();
	return origin.Type() == URL_OPERA && origin.GetAttribute(URL::KName).CompareI("opera:bookmarks") == 0;
}

/* static */ int
JS_Opera::addBookmark(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("sssss");

	DOMSetBoolean(return_value, FALSE);

	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_VALUE;

	BookmarkItem *bookmark = OP_NEW(BookmarkItem, ());
	if (!bookmark)
		return ES_NO_MEMORY;
	OpAutoPtr<BookmarkItem> bookmark_anchor(bookmark);

	BookmarkAttribute attribute;

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[0].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_URL, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[1].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_TITLE, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[2].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_DESCRIPTION, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[3].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SHORTNAME, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L("")));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_FAVICON_FILE, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L("")));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_THUMBNAIL_FILE, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L(""))); // Get todays date
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_CREATED, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L(""))); // Get todays date
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_VISITED, &attribute));

	bookmark->SetFolderType(FOLDER_NO_FOLDER);

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[4].value.string));

	OP_STATUS res;

	if (BookmarkItem *bookmark_folder = g_bookmark_manager->GetFirstByAttribute(BOOKMARK_TITLE, &attribute))
		res = g_bookmark_manager->AddNewBookmark(bookmark, bookmark_folder);
	else
		res = g_bookmark_manager->AddNewBookmark(bookmark, g_bookmark_manager->GetRootFolder());

	if (OpStatus::IsMemoryError(res))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(res))
		return ES_VALUE;

	bookmark_anchor.release();

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
JS_Opera::addBookmarkFolder(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("ssss");

	DOMSetBoolean(return_value, FALSE);

	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_VALUE;

	BookmarkItem *bookmark = OP_NEW(BookmarkItem, ());
	if (!bookmark)
		return ES_NO_MEMORY;
	OpAutoPtr<BookmarkItem> bookmark_anchor(bookmark);

	BookmarkAttribute attribute;

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L("")));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_URL, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[0].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_TITLE, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[1].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_DESCRIPTION, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[2].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SHORTNAME, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L("")));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_FAVICON_FILE, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L("")));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_THUMBNAIL_FILE, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L(""))); // Get todays date
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_CREATED, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(UNI_L(""))); // Get todays date
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_VISITED, &attribute));

	bookmark->SetFolderType(FOLDER_NORMAL_FOLDER);

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[3].value.string));

	OP_STATUS res;

	if (BookmarkItem *bookmark_folder = g_bookmark_manager->GetFirstByAttribute(BOOKMARK_TITLE, &attribute))
		res = g_bookmark_manager->AddNewBookmark(bookmark, bookmark_folder);
	else
		res = g_bookmark_manager->AddNewBookmark(bookmark, g_bookmark_manager->GetRootFolder());

	if (OpStatus::IsMemoryError(res))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(res))
		return ES_VALUE;

	bookmark_anchor.release();

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
JS_Opera::deleteBookmark(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("s");

	DOMSetBoolean(return_value, FALSE);

	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_VALUE;

	if (BookmarkItem *bookmark = g_bookmark_manager->FindId(argv[0].value.string))
		CALL_FAILED_IF_ERROR(g_bookmark_manager->DeleteBookmark(bookmark));
	else
		return ES_VALUE;

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
JS_Opera::moveBookmark(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("ssssss");

	DOMSetBoolean(return_value, FALSE);

	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_VALUE;

	BookmarkItem *bookmark = g_bookmark_manager->FindId(argv[0].value.string);
	if (!bookmark)
		return ES_VALUE;

	BookmarkAttribute attribute;

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[1].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_URL, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[2].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_TITLE, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[3].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_DESCRIPTION, &attribute));

	CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[4].value.string));
	CALL_FAILED_IF_ERROR(bookmark->SetAttribute(BOOKMARK_SHORTNAME, &attribute));

	BookmarkItem *parent_folder;

	if (*argv[5].value.string)
	{
		CALL_FAILED_IF_ERROR(attribute.SetTextValue(argv[5].value.string));
		parent_folder = g_bookmark_manager->GetFirstByAttribute(BOOKMARK_TITLE, &attribute);
	}
	else
		parent_folder = g_bookmark_manager->GetRootFolder();

	OP_STATUS res;

	if (parent_folder && parent_folder->GetFolderType() != FOLDER_NO_FOLDER)
		res = g_bookmark_manager->MoveBookmark(bookmark, parent_folder);
	else
		res = g_bookmark_manager->MoveBookmark(bookmark, bookmark->GetParentFolder());

	CALL_FAILED_IF_ERROR(res);

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
JS_Opera::bookmarksSaveFormValues(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("sssss");
	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_FAILED;
	if (!opera->bookmark_listener)
		return ES_FAILED;

	opera->bookmark_listener->SetSavedFormValues(argv[0].value.string, argv[1].value.string, argv[2].value.string, argv[3].value.string, argv[4].value.string);

	return ES_FAILED;
}

/* static */ int
JS_Opera::bookmarksGetFormUrlValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_FAILED;
	if (!opera->bookmark_listener)
		return ES_FAILED;

	if (!opera->bookmark_listener)
		return ES_FAILED;
	uni_char *value = opera->bookmark_listener->GetSavedFormUrlValue();
	if (value)
		DOMSetString(return_value, value);
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
JS_Opera::bookmarksGetFormTitleValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_FAILED;
	if (!opera->bookmark_listener)
		return ES_FAILED;

	if (!opera->bookmark_listener)
		return ES_FAILED;
	uni_char *value = opera->bookmark_listener->GetSavedFormTitleValue();

	if (value)
		DOMSetString(return_value, value);
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
JS_Opera::bookmarksGetFormDescriptionValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_FAILED;
	if (!opera->bookmark_listener)
		return ES_FAILED;

	if (!opera->bookmark_listener)
		return ES_FAILED;
	uni_char *value = opera->bookmark_listener->GetSavedFormDescriptionValue();
	if (value)
		DOMSetString(return_value, value);
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
JS_Opera::bookmarksGetFormShortnameValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_FAILED;
	if (!opera->bookmark_listener)
		return ES_FAILED;

	if (!opera->bookmark_listener)
		return ES_FAILED;
	uni_char *value = opera->bookmark_listener->GetSavedFormShortnameValue();
	if (value)
		DOMSetString(return_value, value);
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
JS_Opera::bookmarksGetFormParentTitleValue(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_FAILED;
	if (!opera->bookmark_listener)
		return ES_FAILED;

	if (!opera->bookmark_listener)
		return ES_FAILED;
	uni_char *value = opera->bookmark_listener->GetSavedFormParentTitleValue();
	if (value)
		DOMSetString(return_value, value);
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
JS_Opera::setBookmarkListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("o");

	if (!DOM_AllowBookmarksAccess(origining_runtime))
	{
		DOMSetBoolean(return_value, FALSE);
		return ES_VALUE;
	}

	if (!opera->bookmark_listener)
	{
		CALL_FAILED_IF_ERROR(g_bookmark_manager->GetOperaBookmarksListener(&opera->bookmark_listener));
		if (!g_bookmark_manager->BookmarkManagerListenerRegistered(opera->bookmark_listener))
			g_bookmark_manager->RegisterBookmarkManagerListener(opera->bookmark_listener);
		DOM_Environment *env = origining_runtime->GetEnvironment();
		ES_AsyncInterface *ai = env->GetAsyncInterface();

		opera->bookmark_listener->SetCallback(argv[0].value.object);
		opera->bookmark_listener->SetAsyncInterface(ai);
	}

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
JS_Opera::loadBookmarks(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

	DOMSetBoolean(return_value, FALSE);

	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_VALUE;

	CALL_FAILED_IF_ERROR(g_bookmark_manager->LoadBookmarks(BOOKMARK_NO_FORMAT, NULL));

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

/* static */ int
JS_Opera::saveBookmarks(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

	DOMSetBoolean(return_value, FALSE);

	if (!DOM_AllowBookmarksAccess(origining_runtime))
		return ES_VALUE;

	CALL_FAILED_IF_ERROR(g_bookmark_manager->SaveBookmarks(BOOKMARK_NO_FORMAT, NULL));

	DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

#endif // OPERABOOKMARKS_URL

#ifdef OPERASPEEDDIAL_URL

static BOOL
DOM_AllowSpeedDialAccess(DOM_Runtime *origining_runtime)
{
	URL origin = origining_runtime->GetOriginURL();
	return origin.Type() == URL_OPERA && origin.GetAttribute(URL::KName).CompareI("opera:speeddial") == 0;
}

/*static */ int
JS_Opera::setSpeedDial(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("ns");

	if (!DOM_AllowSpeedDialAccess(origining_runtime))
		return ES_EXCEPT_SECURITY;

	CALL_FAILED_IF_ERROR(g_speed_dial_manager->SetSpeedDial(static_cast<int>(argv[0].value.number), argv[1].value.string));

	return ES_FAILED;
}

/*static*/ int
JS_Opera::connectSpeedDial(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("o");

	if (!DOM_AllowSpeedDialAccess(origining_runtime))
		return ES_EXCEPT_SECURITY;

	if (!opera->speeddial_listener)
	{
		DOM_Environment *env = origining_runtime->GetEnvironment();
		ES_AsyncInterface *ai = env->GetAsyncInterface();

		opera->speeddial_listener = OP_NEW(OperaSpeedDialCallback, (ai, argv[0].value.object));
		if (!opera->speeddial_listener)
			return ES_NO_MEMORY;

		CALL_FAILED_IF_ERROR(g_speed_dial_manager->AddSpeedDialListener(opera->speeddial_listener));
	}

	return ES_FAILED;
}

/*static */ int
JS_Opera::reloadSpeedDial(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("n");

	if (!DOM_AllowSpeedDialAccess(origining_runtime))
		return ES_EXCEPT_SECURITY;

	CALL_FAILED_IF_ERROR(g_speed_dial_manager->ReloadSpeedDial(static_cast<int>(argv[0].value.number)));

	return ES_FAILED;
}

/*static */ int
JS_Opera::setSpeedDialReloadInterval(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("nn");

	if (!DOM_AllowSpeedDialAccess(origining_runtime))
		return ES_EXCEPT_SECURITY;

	CALL_FAILED_IF_ERROR(g_speed_dial_manager->SetSpeedDialReloadInterval(static_cast<int>(argv[0].value.number), static_cast<int>(argv[1].value.number)));

	return ES_FAILED;
}

/*static */ int
JS_Opera::swapSpeedDials(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("nn");

	if (!DOM_AllowSpeedDialAccess(origining_runtime))
		return ES_EXCEPT_SECURITY;

	CALL_FAILED_IF_ERROR(g_speed_dial_manager->SwapSpeedDials(static_cast<int>(argv[0].value.number), static_cast<int>(argv[1].value.number)));

	return ES_FAILED;
}

#endif // OPERASPEEDDIAL_URL

#ifdef JS_SCOPE_CLIENT

/* static */ int
JS_Opera::scopeEnableService(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

	if (origining_runtime->GetFramesDocument()->GetWindow()->GetType() != WIN_TYPE_DEVTOOLS)
		return ES_EXCEPT_SECURITY;
	JS_ScopeClient *client_conn = origining_runtime->GetEnvironment()->GetScopeClient();
	if (client_conn == NULL)
	{
		DOMSetBoolean(return_value, FALSE);
		return ES_VALUE;
	}

	DOMSetBoolean(return_value, FALSE);
	if (client_conn->GetHostVersion() == 0)
	{
		DOM_CHECK_ARGUMENTS("s");
	}
	else
		return ES_VALUE;

	OP_STATUS status = OpStatus::OK;
	status = client_conn->EnableService(argv[0].value.string, NULL, 0);
	DOMSetBoolean(return_value, OpStatus::IsSuccess(status));
	return ES_VALUE;
}

/* static */ int
JS_Opera::scopeAddClient(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("ooo|n");

	BOOL ok(TRUE);

	if (origining_runtime->GetFramesDocument()->GetWindow()->GetType() != WIN_TYPE_DEVTOOLS)
		return ES_EXCEPT_SECURITY;

	JS_ScopeClient *client_conn = origining_runtime->GetEnvironment()->GetScopeClient();

	if (client_conn)
	{
		origining_runtime->GetEnvironment()->SetScopeClient(NULL);
		client_conn->Disconnect(TRUE);
		OP_DELETE(client_conn);
	}

	ok = FALSE;
	client_conn = OP_NEW(JS_ScopeClient, (origining_runtime));
	if (client_conn &&
		OpStatus::IsSuccess(client_conn->Construct(
		argv[0].value.object,
		argv[1].value.object,
		argv[2].value.object
		)))
	{
		int port = argc >= 4 ? static_cast<int>(argv[3].value.number) : 0;

		if (port == 0)
			ok = OpStatus::IsSuccess(OpScopeConnectionManager::Connect(client_conn));
		else
			ok = OpStatus::IsSuccess(OpScopeConnectionManager::Listen(client_conn, port));
	}

	if (!ok)
	{
		if (client_conn)
			client_conn->Disconnect(TRUE);
		OP_DELETE(client_conn);
		client_conn = NULL;
	}
	origining_runtime->GetEnvironment()->SetScopeClient(client_conn);

	DOMSetBoolean(return_value, ok);
	return ES_VALUE;
}

/* static */ int
JS_Opera::scopeTransmit(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	JS_ScopeClient *client_conn = origining_runtime->GetEnvironment()->GetScopeClient();
	if (client_conn == NULL)
	{
		DOMSetBoolean(return_value, FALSE);
		return ES_VALUE;
	}

	if (client_conn->GetHostVersion() == 0)
		DOM_CHECK_ARGUMENTS("ss");
	else
		DOM_CHECK_ARGUMENTS("sonn");

	if (origining_runtime->GetFramesDocument()->GetWindow()->GetType() != WIN_TYPE_DEVTOOLS)
		return ES_EXCEPT_SECURITY;

	DOMSetBoolean(return_value, FALSE);

	const uni_char *service = argv[0].value.string;
	OP_STATUS status = OpStatus::OK;
	if (client_conn->GetHostVersion() == 0)
	{
		status = client_conn->Send(service, argv[1].value.string);
	}
	else
	{
		unsigned int command_id = static_cast<unsigned int>(argv[2].value.number);
		unsigned int tag = static_cast<unsigned int>(argv[3].value.number);
		ES_Object *obj = argv[1].value.object;
		status = client_conn->Send(service, command_id, tag, obj, origining_runtime);
	}
	if (OpStatus::IsSuccess(status))
		DOMSetBoolean(return_value, TRUE);

	return ES_VALUE;
}

#  ifdef SCOPE_MESSAGE_TRANSCODING
/* static */ int
JS_Opera::scopeSetTranscodingFormat(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("s");

	DOMSetBoolean(return_value, FALSE);
	const uni_char *format_name = argv[0].value.string;
	OpScopeProxy::TranscodingFormat format;
	if (uni_str_eq(format_name, "none"))
		format = OpScopeProxy::Format_None;
	else if (uni_str_eq(format_name, "protobuf"))
		format = OpScopeProxy::Format_ProtocolBuffer;
	else if (uni_str_eq(format_name, "json"))
		format = OpScopeProxy::Format_JSON;
	else if (uni_str_eq(format_name, "xml"))
		format = OpScopeProxy::Format_XML;
	else
		return ES_VALUE;

	if (OpStatus::IsSuccess(OpScopeProxy::SetTranscodingFormat(format)))
		DOMSetBoolean(return_value, TRUE);

	return ES_VALUE;
}
#  endif // SCOPE_MESSAGE_TRANSCODING

#ifdef SELFTEST
/* static */ int
JS_Opera::scopeExpose(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

	if (OpBoolean::IS_TRUE == IsScopeExposed(opera))
	{
		DOMSetString(return_value, UNI_L("Scope already exposed"));
		return ES_EXCEPTION;
	}

	if (!SetWindowType(origining_runtime, WIN_TYPE_DEVTOOLS))
	{
		DOMSetString(return_value, UNI_L("Runtime not associated with a Window"));
		return ES_EXCEPTION;
	}

	TRAPD(status, AddScopeFunctionsL(opera, origining_runtime));

	CALL_FAILED_IF_ERROR(status);

	return ES_FAILED;
}

/* static */ int
JS_Opera::scopeUnexpose(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

	if (OpBoolean::IS_FALSE == IsScopeExposed(opera))
	{
		DOMSetString(return_value, UNI_L("Scope is not exposed"));
		return ES_EXCEPTION;
	}

	if (!SetWindowType(origining_runtime, WIN_TYPE_NORMAL))
	{
		DOMSetString(return_value, UNI_L("Runtime not associated with a Window"));
		return ES_EXCEPTION;
	}

	RemoveScopeFunctions(opera);

	return ES_FAILED;
}

#endif // SELFTEST

#endif // JS_SCOPE_CLIENT

#ifdef ABOUT_OPERA_DEBUG
#ifdef PREFS_WRITE

static void
DOM_WriteToolsProxyPrefsL(const OpStringC &ip, int port)
{
	g_pctools->WriteStringL(PrefsCollectionTools::ProxyHost, ip);
	g_pctools->WriteIntegerL(PrefsCollectionTools::ProxyPort, port);
}

static OP_STATUS
DOM_WriteToolsProxyPrefs(const OpStringC &ip, int port)
{
	TRAPD(status, DOM_WriteToolsProxyPrefsL(ip, port));
	return status;
}

#endif // PREFS_WRITE

/* static */ int
JS_Opera::setConnectStatusCallback(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);
	DOM_CHECK_ARGUMENTS("o");

	OP_DELETE(opera->debugConnectionCallback);

	DOM_Environment *env = origining_runtime->GetEnvironment();
	ES_AsyncInterface *ai = env->GetAsyncInterface();

	OpAutoPtr<OperaDebugProxy> proxy(OP_NEW(OperaDebugProxy, (ai, origining_runtime, this_object, argv[0].value.object)));
	if (!proxy.get())
		return ES_NO_MEMORY;

	CALL_FAILED_IF_ERROR(g_main_message_handler->SetCallBack(proxy.get(), MSG_SCOPE_CONNECTION_STATUS, 0));
	opera->debugConnectionCallback = proxy.release();
	return ES_FAILED;
}

#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
/* static */ int
JS_Opera::setDevicelistChangedCallback(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("oo");

	DOM_Environment *env = origining_runtime->GetEnvironment();
	ES_AsyncInterface *ai = env->GetAsyncInterface();

	DOM_ARGUMENT_OBJECT(collection_object, 1, DOM_TYPE_WEBSERVER_ARRAY, DOM_UPnPCollection);

	DOM_UPnPCollectionListener *listener(OP_NEW(DOM_UPnPCollectionListener, (ai, argv[0].value.object, collection_object)));

	CALL_FAILED_IF_ERROR(DOMSetObjectRuntime(listener, origining_runtime));

	OP_STATUS stat = listener->Listen();
	if (OpStatus::IsError(stat))
		OP_DELETE(listener);

	CALL_FAILED_IF_ERROR(stat);

	return ES_FAILED;
}
#endif //UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY

/* static */ int
JS_Opera::connect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("sn");

	DOMSetBoolean(return_value, FALSE);

#ifdef SCOPE_CONNECTION_CONTROL
	OpStringC ip(argv[0].value.string);
	int port = static_cast<int>(argv[1].value.number);

	if (ip.HasContent())
	{
#ifdef PREFS_WRITE
		CALL_FAILED_IF_ERROR(DOM_WriteToolsProxyPrefs(ip, port));
#endif // PREFS_WRITE
		g_main_message_handler->PostMessage(MSG_SCOPE_CREATE_CONNECTION, 1, 0, 0);
		DOMSetBoolean(return_value, TRUE);
	}
#endif // SCOPE_CONNECTION_CONTROL

	return ES_VALUE;
}

/* static */ int
JS_Opera::disconnect(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

	DOMSetBoolean(return_value, FALSE);

#ifdef SCOPE_CONNECTION_CONTROL
	g_main_message_handler->PostMessage(MSG_SCOPE_CLOSE_CONNECTION, 1, 0, 0);
	DOMSetBoolean(return_value, TRUE);
#endif // SCOPE_CONNECTION_CONTROL

	return ES_VALUE;
}

/* static */ int
JS_Opera::isConnected(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

	DOMSetBoolean(return_value, FALSE);

#ifdef SCOPE_CONNECTION_CONTROL
	if (OpScopeConnectionManager::IsConnected())
		DOMSetBoolean(return_value, TRUE);
#endif // SCOPE_CONNECTION_CONTROL

	return ES_VALUE;
}

#endif // ABOUT_OPERA_DEBUG

#ifdef ESUTILS_PROFILER_SUPPORT

#include "modules/dom/src/opera/domprofilersupport.h"

/* static */ int
JS_Opera::createProfiler(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, DOM_Object);

	DOM_Profiler *profiler;
	CALL_FAILED_IF_ERROR(DOM_Profiler::Make(profiler, opera));

	DOMSetObject(return_value, profiler);
	return ES_VALUE;
}

#endif // ESUTILS_PROFILER_SUPPORT

#ifdef DATABASE_STORAGE_SUPPORT

#include "modules/dom/src/storage/database.h"

/* static */ int
JS_Opera::deleteDatabase(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(opera, DOM_TYPE_OPERA, JS_Opera);

	if (!opera->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	const uni_char *origin = NULL;
	DOMSetBoolean(return_value, FALSE); // Default return value.

	BOOL is_opera_webdatabases = FALSE;
	if (origining_runtime->GetOriginURL().Type() == URL_OPERA)
	{
		OpString page_url;
		CALL_FAILED_IF_ERROR(origining_runtime->GetOriginURL().GetAttribute(URL::KName, page_url));
		is_opera_webdatabases = page_url.CompareI("opera:" DATABASE_ABOUT_WEBDATABASES_URL) == 0;
	}

	TempBuffer buf;
	if (is_opera_webdatabases)
	{
		// opera:webdb page. deleteDatabase(name, origin) deletes db with that name for passed origin
		DOM_CHECK_ARGUMENTS("ss");
		origin = argv[1].value.string;
	}
	else
	{
		// Non opera:webdb page. deleteDatabase(name) deletes db with that name for current origin
		DOM_CHECK_ARGUMENTS("s");
		CALL_FAILED_IF_ERROR(origining_runtime->GetSerializedOrigin(buf, DOM_Utils::SERIALIZE_FILE_SCHEME));
		origin = buf.GetStorage();

		// We need to clear the expected version of the databases in this webpage
		// else they will break during the transaction steps.
		DOM_DbManager *db_mgr = DOM_DbManager::LookupManagerForWindow(origining_runtime->GetEnvironment()->GetWindow());
		if (db_mgr != NULL)
			db_mgr->ClearExpectedVersions(argv[0].value.string);
	}

	if (!origin || !*origin)
		return ES_VALUE;

	OP_STATUS status;
	FramesDocument *doc = origining_runtime->GetFramesDocument();
	if (uni_str_eq(argv[0].value.string, "*"))
		// Delete all databases from an origin
		status = WSD_Database::DeleteInstances(doc->GetURL().GetContextId(), origin, FALSE);
	else
		status = WSD_Database::DeleteInstance(origin, argv[0].value.string, !doc->GetWindow()->GetPrivacyMode(), doc->GetURL().GetContextId());

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else if(OpStatus::IsSuccess(status))
		DOMSetBoolean(return_value, TRUE);

	return ES_VALUE;
}
#endif // DATABASE_STORAGE_SUPPORT

#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
/* static */ int
JS_Opera::clearWebStorage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("s");

	if (origining_runtime->GetOriginURL().Type() != URL_OPERA)
		return ES_FAILED;

	OpString page_url;
	CALL_FAILED_IF_ERROR(origining_runtime->GetOriginURL().GetAttribute(URL::KName, page_url));
	if (page_url.CompareI("opera:" DATABASE_ABOUT_WEBSTORAGE_URL) != 0)
		return ES_FAILED;

	WebStorageType ws_t = WEB_STORAGE_LOCAL;
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	if (data == 1)
		ws_t = WEB_STORAGE_WGT_PREFS;
# endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	if (data == 2)
		ws_t = WEB_STORAGE_USERJS;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

	if (*argv[0].value.string)
		OpStatus::Ignore(g_webstorage_manager->ClearStorage(ws_t,
			origining_runtime->GetFramesDocument()->GetURL().GetContextId(),
			argv[0].value.string));

	return ES_FAILED;
}

#endif // DATABASE_ABOUT_WEBSTORAGE_URL

#ifdef _PLUGIN_SUPPORT_
/* static */ int
JS_Opera::togglePlugin(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("sb");

	// Absolute plug-in path.
	const uni_char *path = argv[0].value.string;
	// Whether plug-in should be enabled or disabled.
	BOOL enable = argv[1].value.boolean;

	OpString page_url;
	CALL_FAILED_IF_ERROR(origining_runtime->GetOriginURL().GetAttribute(URL::KName, page_url));
	if (page_url.CompareI("opera:plugins") != 0)
		return ES_FAILED;

	DOMSetNumber(return_value, -1);

	unsigned int count = g_plugin_viewers->GetPluginViewerCount();
	for (unsigned int i = 0; i < count; i++)
	{
		PluginViewer* plugin = g_plugin_viewers->GetPluginViewer(i);
		if (uni_str_eq(path, plugin->GetPath()))
		{
			// Switch state of plugin if different from current state.
			if (enable != plugin->IsEnabled())
			{
				plugin->SetEnabledPluginViewer(enable);
				g_plugin_viewers->SaveDisabledPluginsPref();
			}
			DOMSetNumber(return_value, enable);
			break;
		}
	}

	// Returns -1 on failure, 0 if plugin was turned off, 1 if turned on.
	return ES_VALUE;
}

#endif // _PLUGIN_SUPPORT_

#if defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT
/* static */ int
JS_Opera::requestCompassCalibration(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	g_DAPI_orientation_manager->TriggerCompassNeedsCalibration();
	return ES_FAILED;
}
#endif // defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT

#ifdef SPECULATIVE_PARSER
/* static */ int
JS_Opera::getSpeculativeParserUrls(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

	ES_Object *array;
	CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&array));

	OpVector<URL> urls;
	CALL_FAILED_IF_ERROR(origining_runtime->GetFramesDocument()->GetSpeculativeParserURLs(urls));

	OpString url_name;
	for (unsigned i = 0; i < urls.GetCount(); i++)
	{
		CALL_FAILED_IF_ERROR(urls.Get(i)->GetAttribute(URL::KUniName, url_name));
		ES_Value value;
		value.value.string = url_name.CStr();
		value.type = VALUE_STRING;
		CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(array, i, value));
	}

	DOMSetObject(return_value, array);
	return ES_VALUE;
}
#endif // SPECULATIVE_PARSER

#ifdef DOM_LOAD_TV_APP

/* static */ int
JS_Opera::loadTVApp(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);
	DOM_CHECK_ARGUMENTS("o");
	DOMSetBoolean(return_value, FALSE);
	if (g_pccore->GetIntegerPref(PrefsCollectionCore::EnableTVStore) == 0)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	FramesDocument* frameDoc = origining_runtime->GetFramesDocument();
	if (!frameDoc)
		return ES_FAILED;

	ES_Object *app = argv[0].value.object;
	const uni_char *name;
	const uni_char *icon_url;
	const uni_char *application;

	if ((name = this_object->DOMGetDictionaryString(app, UNI_L("name"), NULL)) == NULL)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	if ((icon_url = this_object->DOMGetDictionaryString(app, UNI_L("icon_url"), NULL)) == NULL)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	if ((application = this_object->DOMGetDictionaryString(app, UNI_L("application"), NULL)) == NULL)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);

	OpAutoVector<OpStringC> whitelist;
	BOOL whitelist_exists = FALSE;
	ES_Value value;
	OP_BOOLEAN ret = origining_runtime->GetName(app, UNI_L("whitelist"), &value);
	CALL_FAILED_IF_ERROR(ret);
	if (ret == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
	{
		ES_Object *whitelist_array = value.value.object;
		CALL_FAILED_IF_ERROR(ret = origining_runtime->GetName(whitelist_array, UNI_L("length"), &value));
		if (ret == OpBoolean::IS_TRUE && value.type == VALUE_NUMBER)
		{
			int whitelist_length = DOM_Object::TruncateDoubleToInt(value.value.number);
			if (whitelist_length >= 0)
			{
				whitelist_exists = TRUE;
				for (int i = 0; i < whitelist_length; i++)
				{
					OpStringC* whitelist_item;
					CALL_FAILED_IF_ERROR(ret = origining_runtime->GetIndex(whitelist_array, i, &value));
					if (ret == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
						whitelist_item = OP_NEW(OpStringC, (value.value.string));
					else
						whitelist_item = OP_NEW(OpStringC, (UNI_L("")));
					if (!whitelist_item)
						return ES_NO_MEMORY;
					OP_STATUS ret2 = whitelist.Add(whitelist_item);
					if (OpStatus::IsError(ret2))
					{
						OP_DELETE(whitelist_item);
						return ES_NO_MEMORY;
					}
				}
			}
		}
	}

	WindowCommander *wc = frameDoc->GetWindow()->GetWindowCommander();
	if (OpStatus::IsSuccess(wc->GetTVAppMessageListener()->OnLoadTVApp(wc, application, name, icon_url, (whitelist_exists) ? &whitelist : NULL)))
		DOMSetBoolean(return_value, TRUE);
	return ES_VALUE;
}

#endif //DOM_LOAD_TV_APP

#if defined(SELFTEST) && defined(USE_DUMMY_OP_CAMERA_IMPL)
/* static */ int
JS_Opera::attachDetachDummyCamera(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_OPERA);

	DummyOpCameraManager* dummy_op_camera_manager = static_cast<DummyOpCameraManager*>(g_op_camera_manager);
	if (data == 0)
		dummy_op_camera_manager->DetachCamera();
	else
		CALL_FAILED_IF_ERROR(dummy_op_camera_manager->AttachCamera());

	return ES_FAILED;
}

#endif // SELFTEST && USE_DUMMY_OP_CAMERA_IMPL
