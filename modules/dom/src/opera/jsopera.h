/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _JS_OPERA_
#define _JS_OPERA_

#include "modules/ecmascript/ecmascript.h"
#include "modules/dom/src/domobj.h"

#include "modules/util/simset.h"
#include "modules/ecmascript_utils/esprofiler.h"

#ifdef _DEBUG
# include "modules/ecmascript_utils/esasyncif.h"
#endif // _DEBUG

#ifdef _DEBUG
class JS_Opera_hardToCall : public DOM_Object
{
public:
	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class JS_Opera_delay : public DOM_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class JS_Opera_asyncEval : public DOM_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class JS_Opera_asyncCallMethod : public DOM_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

#ifdef ESUTILS_ASYNC_SLOT_SUPPORT

class JS_Opera_asyncGetSlot: public DOM_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class JS_Opera_asyncSetSlot : public DOM_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

#endif // ESUTILS_ASYNC_SLOT_SUPPORT

class JS_Opera_cancelThisThread : public DOM_Object
{
public:
	virtual int	Call(ES_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime* origining_runtime);
};

class JS_Opera;

class JS_Opera_AsyncCallback
	: public ES_AsyncCallback
{
public:
	void SetOperaObject(JS_Opera *opera_object) { opera = opera_object; }

	OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

private:
	JS_Opera *opera;
};

#endif

#ifdef OPERASPEEDDIAL_URL
class OperaSpeedDialCallback;
#endif // OPERASPEEDDIAL_URL

#ifdef OPERABOOKMARKS_URL
class OperaBookmarksListener;
#endif // OPERABOOKMARKS_URL

class JS_Opera
	: public DOM_Object
{
public:

	/** Add custom function to javascript environment.

        This method makes it possible to extend the javascript environment with
		custom functions.  These custom functions may for instance perform tasks
		specific to a platform or a manufacturer's device.

		(Note, a "callback" is what JS calls a "host function".  lth / 2001-05-28)

          js_callback callback

            The callback that will be called when the corresponding javascript
			function is called.

          const uni_char* name

            Name of the custom function.  After AddCallback has been called, the
		    function will be available in opera.{name}.

          const char* cast_arguments

            The arguments that this function will accept.  The length of the
		    string indicates the number of arguments, and the characters indicate
		    the type of the arguments will be cast to, following the EcmaScript
		    standard.

              n                 = number
		  	  s                 = string
			  b                 = boolean
			  {everything else} = do not typecast the argument

            ie. "s-n" means that the function takes three arguments, of the types
			string, no typecasting, number.

	    There are no provisions for removing callbacks.

		The presence of custom functions can easily be tested with the following
		construct

          if (opera && opera.{function name})
			// function is present

	    When this function call is exported, the js_callback prototype needs
	    to be exported, along with the lang/ecmascript/es_value.h header file. */

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_OPERA || DOM_Object::IsA(type); }

    void InitializeL();

	JS_Opera()
		: DOM_Object()
#ifdef _DEBUG
		, saved_value_name(-1.0)
		, saved_value_index(-1.0)
#endif // _DEBUG
#ifdef OPERABOOKMARKS_URL
		, bookmark_listener(NULL)
#endif // OPERABOOKMARKS_URL
#ifdef OPERASPEEDDIAL_URL
		, speeddial_listener(NULL)
#endif // OPERASPEEDDIAL_URL
#ifdef ABOUT_OPERA_DEBUG
		, debugConnectionCallback(NULL)
#endif //ABOUT_OPERA_DEBUG
	{ }

#ifdef _DEBUG
	// For testing

	JS_Opera_AsyncCallback asyncCallback;

	double	saved_value_name;
	double	saved_value_index;

	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutIndexRestart(int property_index, ES_Value* value, ES_Runtime *origining_runtime, ES_Object* restart_object);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object );
#endif

	virtual ~JS_Opera();
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(buildNumber);
	DOM_DECLARE_FUNCTION(version);
	DOM_DECLARE_FUNCTION(collect);
#ifdef OPERA_CONSOLE
	DOM_DECLARE_FUNCTION(postError);
#endif // OPERA_CONSOLE

#ifdef USER_JAVASCRIPT
	DOM_DECLARE_FUNCTION(defineMagicFunction);
	DOM_DECLARE_FUNCTION(defineMagicVariable);
#endif // USER_JAVASCRIPT

#ifdef DOM_PREFERENCES_ACCESS
	DOM_DECLARE_FUNCTION_WITH_DATA(accessPreference);
	DOM_DECLARE_FUNCTION(commitPreferences);
#endif // DOM_PREFERENCES_ACCESS

#ifdef DOM_LOCALE_SUPPORT
	DOM_DECLARE_FUNCTION(getLocaleString);
#endif // DOM_LOCALE_SUPPORT

#ifdef CPUUSAGETRACKING
	DOM_DECLARE_FUNCTION(getCPUUsage);
	DOM_DECLARE_FUNCTION(getCPUUsageSamples);
	DOM_DECLARE_FUNCTION(activateCPUUser);
#endif // CPUUSAGETRACKING

#ifdef DOM_TO_PLATFORM_MESSAGES
	DOM_DECLARE_FUNCTION(sendPlatformMessage);
#endif // DOM_TO_PLATFORM_MESSAGES

#if defined(PREFS_WRITE) && defined(PREFS_HOSTOVERRIDE)
	DOM_DECLARE_FUNCTION(setOverridePreference);
#endif // defined(PREFS_WRITE) && defined(PREFS_HOSTOVERRIDE)

#ifdef _DEBUG
#ifdef XMLUTILS_XMLSERIALIZER_SUPPORT
	DOM_DECLARE_FUNCTION(serializeToXML);
#endif // XMLUTILS_XMLSERIALIZER_SUPPORT
#endif // _DEBUG

#ifdef DOM_XSLT_SUPPORT
#ifndef XSLT_MORPH_2
	DOM_DECLARE_FUNCTION(pushXSLTransform);
	DOM_DECLARE_FUNCTION(popXSLTransform);
#endif // XSLT_MORPH_2
#endif // DOM_XSLT_SUPPORT

#ifdef DOM_INVOKE_ACTION_SUPPORT
	DOM_DECLARE_FUNCTION(invokeAction);
#endif // DOM_INVOKE_ACTION_SUPPORT

#ifdef DOM_BENCHMARKXML_SUPPORT
	DOM_DECLARE_FUNCTION(benchmarkXML);
#endif // DOM_BENCHMARKXML_SUPPORT

#ifdef OPERABOOKMARKS_URL
	DOM_DECLARE_FUNCTION(setBookmarkListener);
	DOM_DECLARE_FUNCTION(addBookmark);
	DOM_DECLARE_FUNCTION(addBookmarkFolder);
	DOM_DECLARE_FUNCTION(deleteBookmark);
	DOM_DECLARE_FUNCTION(moveBookmark);
	DOM_DECLARE_FUNCTION(loadBookmarks);
	DOM_DECLARE_FUNCTION(saveBookmarks);
	DOM_DECLARE_FUNCTION(bookmarksSaveFormValues);
	DOM_DECLARE_FUNCTION(bookmarksGetFormUrlValue);
	DOM_DECLARE_FUNCTION(bookmarksGetFormTitleValue);
	DOM_DECLARE_FUNCTION(bookmarksGetFormDescriptionValue);
	DOM_DECLARE_FUNCTION(bookmarksGetFormShortnameValue);
	DOM_DECLARE_FUNCTION(bookmarksGetFormParentTitleValue);

	OperaBookmarksListener *bookmark_listener;
#endif // OPERABOOKMARKS_URL

#ifdef OPERASPEEDDIAL_URL
	DOM_DECLARE_FUNCTION(setSpeedDial);
	DOM_DECLARE_FUNCTION(connectSpeedDial);
	DOM_DECLARE_FUNCTION(reloadSpeedDial);
	DOM_DECLARE_FUNCTION(setSpeedDialReloadInterval);
	DOM_DECLARE_FUNCTION(swapSpeedDials);

	OperaSpeedDialCallback* speeddial_listener;
#endif // OPERASPEEDDIAL_URL

#ifdef JS_SCOPE_CLIENT
	DOM_DECLARE_FUNCTION(scopeEnableService);
	DOM_DECLARE_FUNCTION(scopeAddClient);
	DOM_DECLARE_FUNCTION(scopeTransmit);
#  ifdef SCOPE_MESSAGE_TRANSCODING
	DOM_DECLARE_FUNCTION(scopeSetTranscodingFormat);
#  endif // SCOPE_MESSAGE_TRANSCODING
#  ifdef SELFTEST
	DOM_DECLARE_FUNCTION(scopeExpose);
	DOM_DECLARE_FUNCTION(scopeUnexpose);
#  endif // SELFTEST
#endif // JS_SCOPE_CLIENT

	DOM_DECLARE_FUNCTION_WITH_DATA(accessOverrideHistoryNavigationMode);

#ifdef ABOUT_OPERA_DEBUG
	DOM_DECLARE_FUNCTION(connect);
	DOM_DECLARE_FUNCTION(disconnect);
	DOM_DECLARE_FUNCTION(isConnected);
	DOM_DECLARE_FUNCTION(setConnectStatusCallback);
#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
	DOM_DECLARE_FUNCTION(setDevicelistChangedCallback);
#endif //UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
	OperaDebugProxy* debugConnectionCallback;
#endif // ABOUT_OPERA_DEBUG

#ifdef ESUTILS_PROFILER_SUPPORT
	DOM_DECLARE_FUNCTION(createProfiler);
#endif // ESUTILS_PROFILER_SUPPORT

#ifdef DATABASE_STORAGE_SUPPORT
	DOM_DECLARE_FUNCTION(deleteDatabase);
#endif // DATABASE_STORAGE_SUPPORT

#ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	DOM_DECLARE_FUNCTION_WITH_DATA(clearWebStorage);
#endif // DATABASE_ABOUT_WEBSTORAGE_URL

#ifdef _PLUGIN_SUPPORT_
	DOM_DECLARE_FUNCTION(togglePlugin);
#endif // _PLUGIN_SUPPORT_

#ifdef DOM_LOAD_TV_APP
	DOM_DECLARE_FUNCTION(loadTVApp);
#endif //DOM_LOAD_TV_APP

#if defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT
	DOM_DECLARE_FUNCTION(requestCompassCalibration);
#endif // defined SELFTEST && defined DAPI_ORIENTATION_MANAGER_SUPPORT

#ifdef SPECULATIVE_PARSER
	DOM_DECLARE_FUNCTION(getSpeculativeParserUrls);
#endif // SPECULATIVE_PARSER

#if defined(SELFTEST) && defined(USE_DUMMY_OP_CAMERA_IMPL)
	DOM_DECLARE_FUNCTION_WITH_DATA(attachDetachDummyCamera);
#endif // SELFTEST && USE_DUMMY_OP_CAMERA_IMPL
};

#endif /* _JS_OPERA_ */
