/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-1999 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _JS11_WINDOW_
#define _JS11_WINDOW_

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/util/tempbuf.h"

#ifdef CLIENTSIDE_STORAGE_SUPPORT
# include "modules/database/opstorage.h"
#endif // CLIENTSIDE_STORAGE_SUPPORT

#include "modules/device_api/OrientationManager.h"

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
# include "modules/windowcommander/OpWindowCommander.h"
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT

class DOM_Collection;
class DOM_JSWCCallback;
class JS_Location;
class JS_FakeWindow;
class JS_Window;
class Window;
class DOM_JSWindowEventTarget;

#ifdef JS_PLUGIN_SUPPORT
class JS_Plugin_Context;
#endif // JS_PLUGIN_SUPPORT

#ifdef USER_JAVASCRIPT
class DOM_UserJSMagicFunction;
class DOM_UserJSMagicVariable;
#endif // USER_JAVASCRIPT

#ifdef WEBSERVER_SUPPORT
class DOM_WebServer;
#endif // WEBSERVER_SUPPORT

/** The prototype of the global Window object. */
class JS_WindowPrototype
	: public DOM_Prototype
{
public:
    JS_WindowPrototype();

	static void InitializeL(DOM_Prototype *prototype, JS_Window *window);

	enum {
		FUNCTIONS_BASIC = 8,
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
		FUNCTIONS_onSearch,
		FUNCTIONS_getSearchSuggest,
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT
		FUNCTIONS_home,
#ifdef DOM_SELECTION_SUPPORT
		FUNCTIONS_getSelection,
#endif //DOM_SELECTION_SUPPORT
#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
		FUNCTIONS_postMessage,
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#ifdef DATABASE_STORAGE_SUPPORT
		FUNCTIONS_openDatabase,
#endif // DATABASE_STORAGE_SUPPORT
#ifdef URL_UPLOAD_BASE64_SUPPORT
		FUNCTIONS_base64_atob,
		FUNCTIONS_base64_btoa,
#endif // URL_UPLOAD_BASE64_SUPPORT
#ifdef DOM_SUPPORT_BLOB_URLS
		FUNCTIONS_createObjectURL,
		FUNCTIONS_revokeObjectURL,
#endif // DOM_SUPPORT_BLOB_URLS
		FUNCTIONS_matchMedia,
		FUNCTIONS_supportsCSS,

		FUNCTIONS_ARRAY_SIZE
	};

	enum { FUNCTIONS_WITH_DATA_BASIC = 23,
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	       FUNCTIONS_WITH_DATA_showModalDialog,
#endif // DOM_SHOWMODALDIALOG_SUPPORT
	       FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};
};

/** The Window object is annotated with [NamePropertiesObject]. This is
    to allow id + names on the document to be brought into scope on
	the window object -- older practice that we still need to support.

	The resolution of these properties happens after the Window object
	properties (and that of its prototype) have been checked. To support
	that order, we follow spec and put a special 'named properties' object
	on Window's prototype chain. Currently as window.prototype.prototype. */
class JS_WindowNamedProperties
	: public DOM_Object
{
public:
	static OP_STATUS Make(JS_WindowNamedProperties *&named_properties, DOM_Runtime *runtime);

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);

	void SetDocRootElement();

private:
	JS_WindowNamedProperties();

	DOM_Collection *named_properties;

	OP_STATUS Initialize();
};

class JS_Window
	: public DOM_Object
	, public DOM_EventTargetOwner
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	, public OpOrientationListener
	, public OpMotionListener
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	, public ES_Runtime::ErrorHandler
{
private:
	friend class JS_WindowPrototype;

	JS_WindowNamedProperties *window_named_properties;
	/**< The named properties of this Window, @see JS_WindowNamedProperties. */

#ifdef JS_PLUGIN_SUPPORT
	JS_Plugin_Context *jsplugin_context;
#endif

	BOOL body_node_created;

#ifdef USER_JAVASCRIPT
	DOM_PropertyStorage *userjs_magic_storage;
	DOM_UserJSMagicFunction *first_magic_function;
	DOM_UserJSMagicVariable *first_magic_variable;
	BOOL is_fixing_magic_names;
#endif // USER_JAVASCRIPT

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	BOOL m_orientation_listener_attached;
	BOOL m_motion_listener_attached;
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	int keep_alive_counter;

	BOOL has_top_declared;
	/**< TRUE if 'top' has been declared as variable in global scope
	     by script code using this Window as global object. */

	BOOL has_frames_declared;
	/**< TRUE if 'frames' has been declared as variable in global scope
	     by script code using this Window as global object. */

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	class SearchSuggestionsCallback : public OpSearchSuggestionsCallback
	{
	public:
		/**
		 * Creates a search suggestion callback object. @ref OnDataReceived
		 * will be send data to the specified ecmascript window
		 *
		 * @param window ecmascript window
		 */
		SearchSuggestionsCallback(JS_Window* window)
			: OpSearchSuggestionsCallback(), m_id(0), m_window(window) {}

		/**
		 * Sets the url id of the page that created the callback. It is later
		 * sent back to the window when @ref OnDataReceived gets called
		 */
		void SetID(URL_ID id) { m_id = id; }

		/**
		 * Sends formatted data to the ecmascript window
		 *
		 * @param list Search suggestions. The list count must be an even number. 2 or more.
		 *
		 * @return OpStatus::OK on success, otherwise OpStatus::ERR if data could not be sent
		 *         or OpStatus::ERR_NO_MEMORY on OOM
		 */
		virtual OP_STATUS OnDataReceived(OpVector<OpString>& list) { return m_window->SetSearchSuggestions(m_id, list); }

	private:
		URL_ID m_id;
		JS_Window* m_window;
	};
#endif // DOC_SEARCH_SUGGESTIONS_SUPPORT

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	class StorageEventListener : public OpStorageEventListener
	{
	public:
		StorageEventListener(JS_Window *window) : m_window(window) {}
		virtual OP_STATUS HandleEvent(OpStorageValueChangedEvent* e);
		virtual BOOL HasListeners();
	private:
		JS_Window *m_window;
	};

	StorageEventListener m_local_storage_event_listener;
	StorageEventListener m_session_storage_event_listener;
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	StorageEventListener m_widget_preferences_event_listener;
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	SearchSuggestionsCallback* m_search_suggestions_callback;
#endif

	/* From DOM_EventTargetOwner: */
	virtual DOM_Object *GetOwnerObject();

	/* From ES_Runtime::ErrorHandler: */
	virtual OP_STATUS HandleError(ES_Runtime *runtime, const ES_Runtime::ErrorHandler::ErrorInformation *information, const uni_char *msg, const uni_char *url, const ES_SourcePosition &position, const ES_ErrorData *data);

	friend class DOM_JSWindowEventTarget;
	ES_Object *error_handler;
	BOOL is_compiling_error_handler;

	class ErrorHandlerCall
		: public ES_AsyncCallback,
		  public ListElement<ErrorHandlerCall>
	{
	public:
		/* From ES_AsyncCallback: */
		virtual OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

		JS_Window *window;
		ES_Thread *thread;
		const ES_ErrorData *data;
	};

	List<ErrorHandlerCall> current_error_handler_calls;

	/**
	 * Some events are targetted at the window, some at the document.  This
	 * figures out which.
	 *
	 * @param[in] eventname The event to check.
	 *
	 * @param[out] result_target The document or the window object.
	 *
	 * @return OpStatus::OK or an error code.
	 */
	OP_STATUS GetTargetForEvent(const uni_char* eventname, DOM_Object*& result_target);

	/**
	 * Event listeners specified as attributes on the BODY element are supposed
	 * to show up as properties of the window object.  Since attribute event
	 * listeners are only registered when the element's node is created, we need
	 * to create the BODY element's node before either reading or writing event
	 * properties on the window object.
	 */
	OP_STATUS CreateBodyNodeIfNeeded();

public:
	JS_Window();

	static OP_STATUS Make(JS_Window *&window, DOM_Runtime *runtime);
	/** Construct a new Window instance as the global object for 'runtime'.
	    This entails registering the window object instance with the runtime
	    as its (host) global object and setting up the methods and types
	    that the global object exposes.

	    Upon success, the runtime will have the window object installed as
	    its global object.

	    @param [out]window The resulting Window object.
	    @param runtime The freshly allocated runtime to attach the
	           window object to.
	    @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	virtual ~JS_Window();

	void InitializeL();

	virtual BOOL SecurityCheck(ES_Runtime *origining_runtime);
	virtual BOOL SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL TypeofYieldsObject() { return TRUE; }

	void SetDocRootElement(HTML_Element* root);
	/** <Set the HE_DOC_ROOT for a document that didn't have one before. */

	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
#ifdef USER_JAVASCRIPT
	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
#endif // USER_JAVASCRIPT
	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
#ifdef USER_JAVASCRIPT
	virtual ES_PutState PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);
#endif // USER_JAVASCRIPT
	virtual ES_DeleteStatus DeleteName(const uni_char* property_name, ES_Runtime* origining_runtime);

	virtual BOOL AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_WINDOW || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);
	virtual ES_GetState GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime);

	ES_GetState GetEventOrInternalProperty(const uni_char *property, OpAtom property_atom, ES_Value *value, ES_Runtime *origining_runtime);

#ifdef JS_PLUGIN_SUPPORT
	void SetJSPluginContext(JS_Plugin_Context *jsp_context) { jsplugin_context = jsp_context; }
	JS_Plugin_Context *GetJSPluginContext() { return jsplugin_context; }
#endif // JS_PLUGIN_SUPPORT

	OP_STATUS AddKeepAlive(DOM_Object *object, int *id);
	void RemoveKeepAlive(int id);

#ifdef USER_JAVASCRIPT
	OP_STATUS AddMagicFunction(const uni_char *name, ES_Object *impl);
	OP_STATUS AddMagicVariable(const uni_char *name, ES_Object *getter, ES_Object *setter);
#endif // USER_JAVASCRIPT

	JS_Location *GetLocation();

#ifdef WEBSERVER_SUPPORT
	DOM_WebServer *GetWebServer();
#endif // WEBSERVER_SUPPORT

	static BOOL IsUnrequestedThread(ES_Thread *thread);
	static BOOL IsUnrequestedPopup(ES_Thread *thread);
	static BOOL RefusePopup(FramesDocument *document, ES_Thread *thread, BOOL unrequested);
	static BOOL OpenInBackground(FramesDocument *document, ES_Thread *thread);

	static OP_STATUS OpenPopupWindow(Window *&window, DOM_Object *opener, const URL &url, DocumentReferrer refurl, const uni_char *name, const OpRect* rect, BOOL3 scrollbars, BOOL3 location, BOOL3 open_in_background, BOOL set_opener, BOOL user_initiated, BOOL from_window_open);
	static OP_STATUS WriteDocumentData(Window *window, const uni_char *data);

	virtual OP_STATUS CreateEventTarget();

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	OP_STATUS AddStorageListeners();
	void RemoveStorageListeners();
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	OP_STATUS AddOrientationListener();
	OP_STATUS AddMotionListener();
	void RemoveOrientationListener();
	void RemoveMotionListener();
	virtual void OnOrientationChange(const OpOrientationListener::Data& data);
	virtual void OnCompassNeedsCalibration();
	virtual void OnAccelerationChange(const OpMotionListener::Data& data);
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	OP_STATUS SetSearchSuggestions(URL_ID id, OpVector<OpString>& list);
#endif

#ifdef DOM_SELECTION_SUPPORT
	/**
	 * Helper method for getting the current selection object.
	 * @param[out] return_value Will contain the selection object if successful.
	 * @return ES_VALUE if successful, ES_NO_MEMORY or ES_FAILED otherwise.
	 */
	int GetSelection(ES_Value* return_value);
#endif // DOM_SELECTION_SUPPORT

	// Also: {capture,release}Events, {enable,disable}ExternalCapture, dispatchEvent
	DOM_DECLARE_FUNCTION(open);
	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION(print);
	DOM_DECLARE_FUNCTION(stop);
	DOM_DECLARE_FUNCTION(getComputedStyle);
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	DOM_DECLARE_FUNCTION(onSearch);
	DOM_DECLARE_FUNCTION(getSearchSuggest);
#endif
	DOM_DECLARE_FUNCTION(home);
#ifdef DOM_SELECTION_SUPPORT
	DOM_DECLARE_FUNCTION(getSelection);
#endif //DOM_SELECTION_SUPPORT
#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	DOM_DECLARE_FUNCTION(postMessage);
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
#ifdef DATABASE_STORAGE_SUPPORT
	DOM_DECLARE_FUNCTION(openDatabase);
#endif // DATABASE_STORAGE_SUPPORT
#ifdef URL_UPLOAD_BASE64_SUPPORT
	DOM_DECLARE_FUNCTION(base64_atob);
	DOM_DECLARE_FUNCTION(base64_btoa);
#endif // URL_UPLOAD_BASE64_SUPPORT
#ifdef DOM_SUPPORT_BLOB_URLS
	DOM_DECLARE_FUNCTION(createObjectURL);
	DOM_DECLARE_FUNCTION(revokeObjectURL);
#endif // DOM_SUPPORT_BLOB_URLS
	DOM_DECLARE_FUNCTION(matchMedia);
	DOM_DECLARE_FUNCTION(supportsCSS);

	// Also: back, forward, {attach,detach}Event, {add,remove}EventListener and navigate
	DOM_DECLARE_FUNCTION_WITH_DATA(dialog); // alert, confirm and prompt and showModalDialog
	DOM_DECLARE_FUNCTION_WITH_DATA(focusOrBlur); // focus and blur
	DOM_DECLARE_FUNCTION_WITH_DATA(setIntervalOrTimeout); // set{Interval,Timeout}
	DOM_DECLARE_FUNCTION_WITH_DATA(moveOrResizeOrScroll); // {move,resize}{To,By} and scroll{,To,By}
	DOM_DECLARE_FUNCTION_WITH_DATA(clearIntervalOrTimeout);

};

class DOM_JSWindowEventTarget
	: public DOM_EventTarget
{
public:
	DOM_JSWindowEventTarget(JS_Window *owner)
		: DOM_EventTarget(owner)
	{
	}

	virtual void AddListener(DOM_EventListener *listener);
	virtual void RemoveListener(DOM_EventListener *listener);
};

class JS_FakeDocument
	: public DOM_Object
{
public:
	JS_FakeDocument(JS_FakeWindow *fakewindow);

	void InitializeL();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_FAKE_DOCUMENT || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_DECLARE_FUNCTION(open);
	DOM_DECLARE_FUNCTION(close);
	DOM_DECLARE_FUNCTION_WITH_DATA(write);

protected:
	JS_FakeWindow *fakewindow;
};

class JS_FakeWindow
	: public DOM_Object
{
protected:
	JS_FakeWindow(DOM_JSWCCallback *callback);

	DOM_JSWCCallback *callback;
	JS_Location *location;
	JS_FakeDocument *document;

public:
	static OP_STATUS Make(JS_FakeWindow *&fakewindow, DOM_JSWCCallback *callback);

	void InitializeL();

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_FAKE_WINDOW || DOM_Object::IsA(type); }
	virtual void GCTrace();

	const URL &GetURL();
	void SetURL(const URL &url, DocumentReferrer& refurl);
	OP_STATUS AddDocumentData(const uni_char *data, BOOL newline);
	void Open(DOM_Runtime *origining_runtime);
	void Close();
};

#endif /* _JS11_WINDOW_ */
