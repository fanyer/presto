/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "modules/dom/src/js/window.h"

#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/domerror.h"
#include "modules/dom/src/domcore/domexception.h"
#include "modules/dom/src/domcss/cssrule.h"
#include "modules/dom/src/domcss/cssstyledeclaration.h"
#include "modules/dom/src/domdebugger/domdebugger.h"
#include "modules/dom/src/domevents/domevent.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/dom/src/domevents/domeventsource.h"
#include "modules/dom/src/domevents/dommutationevent.h"
#include "modules/dom/src/domevents/domeventthread.h"
#include "modules/dom/src/domfile/domfile.h"
#include "modules/dom/src/domfile/domfilereader.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/domhtml/htmldoc.h"
#include "modules/dom/src/domhtml/htmlelem.h"
#include "modules/dom/src/domhtml/htmlcoll.h"
#include "modules/dom/src/domload/domparser.h"
#include "modules/dom/src/domload/lsparser.h"
#include "modules/dom/src/domload/lsexception.h"
#include "modules/dom/src/domrange/range.h"
#include "modules/dom/src/domsave/xmlserializer.h"
#include "modules/dom/src/domstylesheets/mediaquerylist.h"
#include "modules/dom/src/domtraversal/nodefilter.h"
#include "modules/dom/src/domxpath/xpathevaluator.h"
#include "modules/dom/src/domxpath/xpathnamespace.h"
#include "modules/dom/src/domxpath/xpathresult.h"
#include "modules/dom/src/domxpath/xpathexception.h"
#include "modules/dom/src/js/dombase64.h"
#include "modules/dom/src/js/history.h"
#include "modules/dom/src/js/location.h"
#include "modules/dom/src/js/navigat.h"
#include "modules/dom/src/js/screen.h"
#include "modules/dom/src/js/js_console.h"
#include "modules/dom/src/js/searchsuggestionsevent.h"
#include "modules/dom/src/opatom.h"
#include "modules/dom/src/opera/domhttp.h"
#include "modules/dom/src/opera/domio.h"
#include "modules/dom/src/opera/domselection.h"
#include "modules/dom/src/opera/jsopera.h"
#include "modules/dom/src/userjs/userjsmagic.h"
#include "modules/dom/src/userjs/userjsmanager.h"
#include "modules/dom/src/webforms2/webforms2dom.h"
#include "modules/dom/domutils.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/dom/src/domwebserver/domwebserver.h"
#endif //WEBSERVER_SUPPORT

#if defined WEBSERVER_SUPPORT || defined DOM_GADGET_FILE_API_SUPPORT
#include "modules/dom/src/opera/dombytearray.h"
#endif

#ifdef MSWIN
#include "platforms/windows/windows_ui/printwin.h"
#endif // MSWIN

#ifdef _MACINTOSH_
# include "platforms/mac/pi/MacOpPrinterController.h"
#endif // _MACINTOSH_

#include "modules/doc/frm_doc.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essyncif.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/ecmascript_utils/esterm.h"
#include "modules/ecmascript_utils/estimermanager.h"
#include "modules/ecmascript_utils/estimerevent.h"
#include "modules/hardcore/mh/messages.h"

#include "modules/display/vis_dev.h"
#include "modules/display/coreview/coreview.h"

#include "modules/dochand/docman.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/logdoc/htm_elm.h"
#include "modules/url/url_man.h"
#include "modules/util/htmlify.h"
#include "modules/formats/argsplit.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_js.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/pi/OpSystemInfo.h"

#ifdef JS_PLUGIN_SUPPORT
# include "modules/jsplugins/src/js_plugin_manager.h"
# include "modules/jsplugins/src/js_plugin_context.h"
# include "modules/jsplugins/src/js_plugin_object.h"
#endif // JS_PLUGIN_SUPPORT

#include "modules/windowcommander/src/WindowCommander.h"

#if defined(QUICK) && defined(_KIOSK_MANAGER_)
# include "adjunct/quick/Application.h"
# include "adjunct/quick/managers/KioskManager.h"
#endif

#ifdef MEDIA_HTML_SUPPORT
# include "modules/dom/src/media/dommediaerror.h"
# include "modules/dom/src/media/domtexttrack.h"
#endif // MEDIA_HTML_SUPPORT

#ifdef CANVAS_SUPPORT
# include "modules/dom/src/canvas/domcontext2d.h"
#endif // CANVAS_SUPPORT

#ifdef DOM_XSLT_SUPPORT
# include "modules/dom/src/opera/domxslt.h"
#endif // DOM_XSLT_SUPPORT

#ifdef GADGET_SUPPORT
# include "modules/dom/src/opera/domwidget.h"
# include "modules/gadgets/OpGadget.h"
#endif // GADGET_SUPPORT

#ifdef DOM_JIL_API_SUPPORT
# include "modules/dom/src/domcallsecurefunctionwrapper.h" // Used for JIL security rules on XmlHttpRequest
# include "modules/dom/src/domjil/domjilwidget.h"
# include "modules/dom/src/domjil/utils/jilutils.h"
#endif // DOM_JIL_API_SUPPORT

#ifdef SVG_DOM
# include "modules/dom/src/domsvg/domsvgelement.h"
# include "modules/dom/src/domsvg/domsvgexception.h"
#endif // SVG_DOM

#ifdef ABOUT_HTML_DIALOGS
# include "modules/about/ophtmldialogs.h"
#endif // ABOUT_HTML_DIALOGS

#ifdef CLIENTSIDE_STORAGE_SUPPORT
# include "modules/database/opstorage.h"
# include "modules/dom/src/storage/storage.h"
# include "modules/dom/src/storage/storageutils.h"
# include "modules/dom/src/storage/storageevent.h"
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
# include "modules/dom/src/domwebworkers/domwebworkers.h"
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
# include "modules/dom/src/domwebworkers/domcrossmessage.h"
# include "modules/dom/src/domwebworkers/domcrossutils.h"
/* Enable for spec compliance wrt checking 'targetOrigin' parameter to window.postMessage() */
/* #define DOM_CROSSDOCUMENT_MESSAGING_STRICT_HOSTSPECIFIC */
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EXTENSION_SUPPORT
# include "modules/dom/src/userjs/userjsmanager.h"
#endif // EXTENSION_SUPPORT

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
#include "modules/dom/src/orientation/deviceorientationevent.h"
#include "modules/dom/src/orientation/compassneedscalibrationevent.h"
#include "modules/dom/src/orientation/devicemotionevent.h"
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#ifdef DRAG_SUPPORT
# include "modules/dragdrop/dragdrop_manager.h"
# include "modules/dom/src/domdatatransfer/domdatatransfer.h"
# include "modules/pi/OpDragObject.h"
#endif // DRAG_SUPPORT

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
# include "modules/scope/scope_window_listener.h"
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

#include "modules/layout/layout_workplace.h"
#include "modules/style/css_condition.h"

extern DOM_FunctionImpl DOM_dummyMethod;


static OP_STATUS
DOM_GetWindowObject(ES_Value *value, DocumentManager *docman, ES_Runtime *origining_runtime)
{
	DOM_Object::DOMSetNull(value);

	if (docman)
	{
		if (FramesDocElm *frame = docman->GetFrame())
			if (HTML_Element *element = frame->GetHtmlElement())
			{
				HTML_ElementType type = element->Type();
				if (type != HE_FRAME && type != HE_IFRAME)
					return OpStatus::OK;
			}

		RETURN_IF_ERROR(docman->ConstructDOMProxyEnvironment());

		DOM_Object *window;
		RETURN_IF_ERROR(static_cast<DOM_ProxyEnvironmentImpl *>(docman->GetDOMEnvironment())->GetProxyWindow(window, origining_runtime));

		DOM_Object::DOMSetObject(value, window);
	}

	return OpStatus::OK;
}

static FramesDocElm *
DOM_GetFramesRoot(FramesDocument *frames_doc)
{
	// frames_doc might be NULL.
	if (!frames_doc)
		return NULL;
	else if (FramesDocElm *root = frames_doc->GetFrmDocRoot())
		return root;
	else
		return frames_doc->GetIFrmRoot();
}


static ES_GetState
DOM_GetWindowFrame(ES_Value *value, FramesDocument *frames_doc, const uni_char *name, int index, DOM_Runtime *origining_runtime)
{
	// frames_doc might be NULL.
	if (FramesDocElm *root = DOM_GetFramesRoot(frames_doc))
	{
		FramesDocElm *frame = (FramesDocElm *) root->FirstLeaf();

		while (frame)
		{
			if (frame != root)
				if (name)
				{
					const uni_char *frame_name = frame->GetName();
					if (frame_name && uni_str_eq(frame_name, name))
							break;
					const uni_char *frame_id = frame->GetFrameId();
					if (frame_id && uni_str_eq(frame_id, name))
							break;
				}
				else if (index-- == 0)
					break;

			frame = (FramesDocElm *) frame->NextLeaf();
		}

		if (frame)
		{
			origining_runtime->GetEnvironment()->AccessedOtherEnvironment(frame->GetCurrentDoc());

			if (value)
				GET_FAILED_IF_ERROR(DOM_GetWindowObject(value, frame->GetDocManager(), origining_runtime));

			return GET_SUCCESS;
		}
	}

	return GET_FAILED;
}

static int
DOM_CountFrames(FramesDocument *frames_doc)
{
	// frames_doc might be NULL.
	int count = 0;

	if (FramesDocElm *root = DOM_GetFramesRoot(frames_doc))
	{
		FramesDocElm *iter = (FramesDocElm *) root->FirstLeaf();

		while (iter)
		{
			BOOL include = iter != root;

#ifdef DELAYED_SCRIPT_EXECUTION
			if (include)
				if (HTML_Element *frame_element = iter->GetHtmlElement())
					if (frame_element->GetInserted() == HE_INSERTED_BY_PARSE_AHEAD)
						include = FALSE;
#endif // DELAYED_SCRIPT_EXECUTION

			if (include)
				++count;

			iter = (FramesDocElm *) iter->NextLeaf();
		}
	}

	return count;
}

#ifdef CLIENTSIDE_STORAGE_SUPPORT
/* virtual */ BOOL
JS_Window::StorageEventListener::HasListeners()
{
	return (m_window->GetFramesDocument() &&
			m_window->GetFramesDocument()->IsCurrentDoc() &&
			m_window->GetEnvironment()->HasWindowEventHandler(STORAGE));
}

/* virtual */ OP_STATUS
JS_Window::StorageEventListener::HandleEvent(OpStorageValueChangedEvent* event_obj)
{
	OP_ASSERT(HasListeners());
	OP_ASSERT(event_obj);

	if (event_obj->m_context.IsSameDOMEnvironment(m_window->GetEnvironment()))
	{
		/** This check ensures that the storage event is not
		 *  called for the dom environment that created the storage
		 *  change in the first place
		 */
		event_obj->Release();
		return OpStatus::OK;
	}

	DOM_StorageEvent *event;
	RETURN_IF_ERROR(DOM_StorageEvent::Make(event, event_obj, m_window->GetRuntime()));
	RETURN_IF_ERROR(m_window->GetEnvironment()->SendEvent(event));

	return OpStatus::OK;
}
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
OP_STATUS
JS_Window::SetSearchSuggestions(URL_ID id, OpVector<OpString>& list)
{
	FramesDocument* frames_doc = GetFramesDocument();
	if (!frames_doc)
		return OpStatus::ERR;
	// Test that window is still showing same url as it was when requesting suggestions
	URL url = frames_doc->GetWindow()->DocManager()->GetCurrentURL();
	if (id != url.Id(TRUE))
		return OpStatus::ERR;

	// We need {url},{name},{url},{name},....
	UINT32 count = list.GetCount();
	if (count < 2 || count%2)
		return OpStatus::ERR;

	OpString html;
	RETURN_IF_ERROR(html.Append("<ul id=\"searchSuggestions\">\n"));
	for (UINT32 i=0; i<count; i+=2)
	{
		uni_char* url_string = HTMLify_string(list.Get(i)->CStr(), list.Get(i)->Length(), TRUE);
		uni_char* name_string = HTMLify_string(list.Get(i+1)->CStr(), list.Get(i+1)->Length(), TRUE);
		if (!url_string || !name_string)
		{
			OP_DELETE(url_string);
			OP_DELETE(name_string);
			return OpStatus::ERR_NO_MEMORY;
		}
		OpString s1, s2;
		s1.TakeOver(url_string);
		s2.TakeOver(name_string);
		RETURN_IF_ERROR(html.Append("<li><a href=\""));
		RETURN_IF_ERROR(html.Append(s1));
		RETURN_IF_ERROR(html.Append("\" onclick=\"return window.onSearch(true)\">"));
		RETURN_IF_ERROR(html.Append(s2));
		RETURN_IF_ERROR(html.Append("</a>\n"));
	}
	RETURN_IF_ERROR(html.Append("</ul>\n"));

	DOM_SearchSuggestionsEvent* event;
	RETURN_IF_ERROR(DOM_SearchSuggestionsEvent::Make(event, html, GetRuntime()));
	RETURN_IF_ERROR(GetEnvironment()->SendEvent(event));

	return OpStatus::OK;
}
#endif

void
JS_Window::SetDocRootElement(HTML_Element* root)
{
	window_named_properties->SetDocRootElement();
}

/* virtual */ DOM_Object *
JS_Window::GetOwnerObject()
{
	return this;
}

/* virtual */ OP_STATUS
JS_Window::HandleError(ES_Runtime *runtime, const ES_Runtime::ErrorHandler::ErrorInformation *information, const uni_char *msg, const uni_char *url, const ES_SourcePosition &position, const ES_ErrorData *data)
{
	if (information->type == ES_Runtime::ErrorHandler::COMPILATION_ERROR)
	{
		if (is_compiling_error_handler)
		{
			/* This is a compilation error in the error handler itself; just
			   report it to the console to avoid recursion. */
			runtime->ReportErrorToConsole(data);
			return OpStatus::OK;
		}

#ifdef USER_JAVASCRIPT
		switch (static_cast<const CompilationErrorInformation *>(information)->script_type)
		{
		case SCRIPT_TYPE_USER_JAVASCRIPT:
		case SCRIPT_TYPE_USER_JAVASCRIPT_GREASEMONKEY:
		case SCRIPT_TYPE_BROWSER_JAVASCRIPT:
		case SCRIPT_TYPE_EXTENSION_JAVASCRIPT:
			/* Compilation errors from User JS (or browser.js or extensions)
			   shouldn't be reported to the page's error handler, report
			   directly to the console instead. */
			runtime->ReportErrorToConsole(data);
			return OpStatus::OK;
		}
#endif // USER_JAVASCRIPT
	}

	ES_Thread *interrupt_thread;
	if (runtime->GetESScheduler())
		interrupt_thread = runtime->GetESScheduler()->GetErrorHandlerInterruptThread(information->type);
	else
	{
		/* The runtime has been detached; simply report the error to the console. */
		runtime->ReportErrorToConsole(data);
		return OpStatus::OK;
	}

	if (information->type == ES_Runtime::ErrorHandler::RUNTIME_ERROR || information->type == ES_Runtime::ErrorHandler::LOAD_ERROR)
	{
#ifdef USER_JAVASCRIPT
		if (DOM_UserJSManager::IsActiveInRuntime(runtime))
		{
			/* Runtime errors from User JS (or browser.js or extensions)
			   shouldn't be reported to the page's error handler, report
			   directly to the console instead. */
			runtime->ReportErrorToConsole(data);
			return OpStatus::OK;
		}
#endif // USER_JAVASCRIPT

		for (ErrorHandlerCall *call = current_error_handler_calls.First(); call; call = call->Suc())
			if (interrupt_thread->IsOrHasInterrupted(call->thread))
			{
				/* Runtime errors from the error handler itself should just be
				   reported to the user, they shouldn't trigger another call to
				   the error handler. */
				runtime->ReportErrorToConsole(data);
				return OpStatus::OK;
			}
	}

	if (!error_handler)
	{
		if (event_target)
		{
			/* Create body node to force <body onerror=""> listener to be
			   registered before we look it up. */
			RETURN_IF_ERROR(CreateBodyNodeIfNeeded());

			is_compiling_error_handler = TRUE;

			OP_BOOLEAN found = event_target->FindOldStyleHandler(ONERROR, &error_handler);

			is_compiling_error_handler = FALSE;

			if (OpStatus::IsMemoryError(found))
				return found;
		}

		if (!error_handler)
		{
			runtime->ReportErrorToConsole(data);
			return OpStatus::OK;
		}
	}

	unsigned line = position.GetAbsoluteLine();

	if (url && !information->cross_origin_allowed)
	{
		BOOL allowed = uni_strncmp(url, "data:", 5) != 0 && uni_strncmp(url, "javascript:", 11) != 0;

		if (allowed)
		{
			URL parsed_url = g_url_api->GetURL(url);

			allowed = OriginCheck(parsed_url, GetRuntime()->GetOriginURL());
		}

		if (!allowed)
		{
			msg = UNI_L("Script error.");
			url = NULL;
			line = 0;
		}
	}

	/* The listener is registered when there's an error handler set, and
	   unregistered again if unset. */
	OP_ASSERT(error_handler);

	ErrorHandlerCall *call = OP_NEW(ErrorHandlerCall, ());

	if (!call)
		return OpStatus::ERR_NO_MEMORY;

	ES_Value argv[3]; /* ARRAY OK 2011-09-15 jl */
	DOMSetString(&argv[0], msg);
	DOMSetString(&argv[1], url);
	DOMSetNumber(&argv[2], line);

	ES_AsyncInterface *asyncif = runtime->GetESAsyncInterface();
	OP_STATUS status = asyncif->CallFunction(error_handler, NULL, 3, argv, call, interrupt_thread);

	if (OpStatus::IsSuccess(status))
	{
		call->window = this;
		call->thread = asyncif->GetLastStartedThread();
		call->data = data;
		call->Into(&current_error_handler_calls);
		return OpStatus::OK;
	}
	else
	{
		OP_DELETE(call);

		if (OpStatus::IsMemoryError(status))
			return status;
		else
		{
			runtime->ReportErrorToConsole(data);
			return OpStatus::OK;
		}
	}
}

/* virtual */ OP_STATUS
JS_Window::ErrorHandlerCall::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	ES_Runtime *runtime = window->GetRuntime();

	if (status == ES_ASYNC_SUCCESS && result.type == VALUE_BOOLEAN && !result.value.boolean)
		runtime->IgnoreError(data);
	else if (data)
		runtime->ReportErrorToConsole(data);

	Out();
	OP_DELETE(this);

	return OpStatus::OK;
}

JS_WindowNamedProperties::JS_WindowNamedProperties()
	: named_properties(NULL)
{
}

/* static */ OP_STATUS
JS_WindowNamedProperties::Make(JS_WindowNamedProperties *&named_properties, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(named_properties = OP_NEW(JS_WindowNamedProperties, ()), runtime, runtime->GetObjectPrototype(), "Object"));

	DOM_SimpleCollectionFilter filter(NAME_IN_WINDOW);
	DOM_Collection *collection;

	DOM_EnvironmentImpl *environment = named_properties->GetEnvironment();
	RETURN_IF_ERROR(DOM_Collection::Make(collection, environment, "HTMLCollection", NULL, FALSE, FALSE, filter));
	RETURN_IF_ERROR(named_properties->PutPrivate(DOM_PRIVATE_nameInWindow, *collection));

	collection->SetCreateSubcollections();
 	collection->SetPreferWindowObjects();
	collection->SetOwner(named_properties);
	named_properties->named_properties = collection;

	return OpStatus::OK;
}

/* virtual */ ES_GetState
JS_WindowNamedProperties::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	/* The "origin load check" treats accesses to document-less runtimes as unsafe.
	   Preferable not to deny such accesses outright, hence the FramesDocument
	   check. (The named properties object will also check security, if needs be.) */
	if (GetFramesDocument() && OriginLoadCheck(GetRuntime(), origining_runtime) == NO)
		return GET_SECURITY_VIOLATION;

	GET_FAILED_IF_ERROR(Initialize());
	if (*property_name)
		return named_properties->GetName(property_name, OP_ATOM_UNASSIGNED, value, origining_runtime);
	else
		return GET_FAILED;
}

OP_STATUS
JS_WindowNamedProperties::Initialize()
{
	if (!named_properties->GetRoot())
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();
		RETURN_IF_ERROR(environment->ConstructDocumentNode());
		if (DOM_Document *document = static_cast<DOM_Document*>(environment->GetDocument()))
			named_properties->SetRoot(document, FALSE);
	}
	return OpStatus::OK;
}

void
JS_WindowNamedProperties::SetDocRootElement()
{
	// Update the root in the NAME_IN_WINDOW collection because
	// it might have (or rather will have) been
	// created with just an empty document
	// as root and therefore have no tree_root at all.
	if (named_properties)
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();
		if (DOM_Document *document = static_cast<DOM_Document*>(environment->GetDocument()))
			named_properties->SetRoot(document, FALSE);
	}
}

JS_WindowPrototype::JS_WindowPrototype()
#ifdef DOM_NO_COMPLEX_GLOBALS
	: DOM_Prototype(g_DOM_globalData->JS_WindowPrototype_functions, g_DOM_globalData->JS_WindowPrototype_functions_with_data)
#else // DOM_NO_COMPLEX_GLOBALS
	: DOM_Prototype(JS_WindowPrototype_functions, JS_WindowPrototype_functions_with_data)
#endif // DOM_NO_COMPLEX_GLOBALS
{
}

JS_Window::JS_Window()
	: window_named_properties(NULL),
#ifdef JS_PLUGIN_SUPPORT
	  jsplugin_context(NULL),
#endif // JS_PLUGIN_SUPPORT
	  body_node_created(FALSE),
#ifdef USER_JAVASCRIPT
	  userjs_magic_storage(NULL),
	  first_magic_function(NULL),
	  first_magic_variable(NULL),
	  is_fixing_magic_names(FALSE),
#endif // USER_JAVASCRIPT
#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	  m_orientation_listener_attached(FALSE),
	  m_motion_listener_attached(FALSE),
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	  keep_alive_counter(1024),
	  has_top_declared(FALSE),
	  has_frames_declared(FALSE),
#ifdef CLIENTSIDE_STORAGE_SUPPORT
	  m_local_storage_event_listener(this),
	  m_session_storage_event_listener(this),
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	  m_widget_preferences_event_listener(this),
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
#endif // CLIENTSIDE_STORAGE_SUPPORT
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	  m_search_suggestions_callback(0),
#endif
	  error_handler(NULL),
	  is_compiling_error_handler(FALSE)
{
}

/* static */ void
JS_WindowPrototype::InitializeL(DOM_Prototype *window_prototype, JS_Window *window)
{
	DOM_Runtime *runtime = window_prototype->GetRuntime();

	JS_WindowNamedProperties *named_properties;
	LEAVE_IF_ERROR(JS_WindowNamedProperties::Make(named_properties, runtime));
	LEAVE_IF_ERROR(static_cast<ES_Runtime *>(runtime)->SetPrototype(window_prototype, *named_properties));
	window->window_named_properties = named_properties;

	window_prototype->InitializeL();

	DOM_BuiltInConstructor *constructor = OP_NEW(DOM_BuiltInConstructor, (window_prototype));
	LEAVE_IF_ERROR(DOM_Object::DOMSetFunctionRuntime(constructor, runtime, "Window"));

	ES_Value value;
	DOMSetObject(&value, constructor);
	window_prototype->PutL("Window", value, PROP_DONT_ENUM);
	window_prototype->PutL("constructor", value, PROP_DONT_ENUM);

	/* To allow cross-domain properties to be accessed via 'window',
	   mark 'window.prototype' as having such host properties. The engine
	   will then leave 'window.prototype' responsible for handling the
	   security checks on access even if 'window' itself isn't accessible. */
	DOM_Runtime::SetIsCrossDomainHostAccessible(window_prototype);

	window->InitializeL();
	LEAVE_IF_ERROR(DOM_Node::HideMSIEEventAPI(*window_prototype, runtime));
}

/* static */ OP_STATUS
JS_Window::Make(JS_Window *&window, DOM_Runtime *runtime)
{
	window = OP_NEW(JS_Window, ());
	if (!window)
		return OpStatus::ERR_NO_MEMORY;

	JS_WindowPrototype *window_prototype = OP_NEW(JS_WindowPrototype, ());
	if (!window_prototype)
	{
		OP_DELETE(window);
		window = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(runtime->SetHostGlobalObject(window, window_prototype));

	TRAPD(status, JS_WindowPrototype::InitializeL(window_prototype, window));
	return status;
}

void
JS_Window::InitializeL()
{
	DOM_Runtime *runtime = GetRuntime();
	DOM_Object *object;

	DOM_CSSRule::ConstructCSSRuleObjectL(*PutConstructorL("CSSRule", DOM_Runtime::CSSRULE_PROTOTYPE, TRUE), runtime);

#ifdef DOM2_TRAVERSAL
	DOM_NodeFilter::ConstructNodeFilterObjectL(*PutConstructorL("NodeFilter", DOM_Runtime::NODEFILTER_PROTOTYPE, TRUE), runtime);
#endif // DOM2_TRAVERSAL

	DOM_DOMError::ConstructDOMErrorObjectL(PutSingletonObjectL("DOMError", PROP_DONT_ENUM), runtime);

#if defined DOM3_LOAD || defined DOM3_SAVE
	DOM_LSException::ConstructLSExceptionObjectL(*PutConstructorL("LSException", DOM_Runtime::LSEXCEPTION_PROTOTYPE, TRUE), runtime);
#endif // DOM3_LOAD || DOM3_SAVE

#ifdef DOM3_LOAD
	PutFunctionL("DOMParser", OP_NEW(DOM_DOMParser_Constructor, ()), "DOMParser", NULL);

	DOM_LSParser::ConstructLSParserFilterObjectL(PutSingletonObjectL("LSParserFilter", PROP_DONT_ENUM), runtime);
	DOM_LSParser::ConstructDOMImplementationLSObjectL(PutSingletonObjectL("DOMImplementationLS", PROP_DONT_ENUM), runtime);

#ifdef DOM_HTTP_SUPPORT
	DOM_XMLHttpRequest_Constructor* xhr_constructor = OP_NEW(DOM_XMLHttpRequest_Constructor, ());
	PutConstructorL(xhr_constructor);
	DOM_XMLHttpRequest::ConstructXMLHttpRequestObjectL(*xhr_constructor, runtime);
	DOM_AnonXMLHttpRequest_Constructor* anon_xhr_constructor = OP_NEW(DOM_AnonXMLHttpRequest_Constructor, ());
	PutConstructorL(anon_xhr_constructor);
# ifdef PROGRESS_EVENTS_SUPPORT
	PutFunctionL("XMLHttpRequestUpload", OP_NEW(DOM_Object, ()), "XMLHttpRequestUpload");
# endif // PROGRESS_EVENTS_SUPPORT

	DOM_FormData_Constructor* formdata_constructor = OP_NEW(DOM_FormData_Constructor, ());
	PutConstructorL(formdata_constructor);
#endif // DOM_HTTP_SUPPORT
#endif // DOM3_LOAD

#ifdef DOM3_SAVE
	PutConstructorL(OP_NEW(DOM_XMLSerializer_Constructor, ()));
#endif // DOM3_SAVE

#ifdef DOM3_XPATH
	PutConstructorL(OP_NEW(DOM_XPathEvaluator_Constructor, ()));
	DOM_XPathException::ConstructXPathExceptionObjectL(*PutConstructorL("XPathException", DOM_Runtime::XPATHEXCEPTION_PROTOTYPE), runtime);
#endif // DOM3_XPATH

#ifdef DOM_XSLT_SUPPORT
	PutConstructorL(OP_NEW(DOM_XSLTProcessor_Constructor, ()));
#endif // DOM_XSLT_SUPPORT

	// Console object. Exists always in safari and after opening a debugger in MSIE8 and FF3. For now we'll let it exist always
	DOMSetObjectRuntimeL(object = OP_NEW(JS_Console, ()), runtime, runtime->GetPrototype(DOM_Runtime::CONSOLE_PROTOTYPE), "Console");
	ES_Value value;
	DOMSetObject(&value, object);
	PutL("console", value);

	DOM_Object *opera;
	PutObjectL("opera", opera = OP_NEW(JS_Opera, ()), "Opera");
	static_cast<JS_Opera *>(opera)->InitializeL();

	PutFunctionL("Image", OP_NEW(DOM_HTMLImageElement_Constructor, ()), "Image", "nn-");
	PutFunctionL("Option", OP_NEW(DOM_HTMLOptionElement_Constructor, ()), "Option", "ssbb-");

	DOM_Event_Constructor::AddConstructorL(this);
	DOM_CustomEvent_Constructor::AddConstructorL(this);
	DOM_KeyboardEvent_Constructor::AddConstructorL(this);
	DOM_TextEvent_Constructor::AddConstructorL(this);

#ifdef PAGED_MEDIA_SUPPORT
	DOM_PageEvent_Constructor::AddConstructorL(this);
#endif // PAGED_MEDIA_SUPPORT

#ifdef DRAG_SUPPORT
	DOM_DragEvent_Constructor::AddConstructorL(this);
#endif // DRAG_SUPORT

#ifdef USE_OP_CLIPBOARD
	DOM_ClipboardEvent_Constructor::AddConstructorL(this);
#endif // USE_OP_CLIPBOARD

#ifdef PROGRESS_EVENTS_SUPPORT
	DOM_ProgressEvent_Constructor::AddConstructorL(this);
#endif // PROGRESS_EVENTS_SUPPORT

	DOMSetObjectRuntimeL(object = OP_NEW(JS_History, ()), runtime, runtime->GetPrototype(DOM_Runtime::HISTORY_PROTOTYPE), "History");
	PutPrivateL(DOM_PRIVATE_history, object);

	DOMSetObjectRuntimeL(object = OP_NEW(JS_Location, ()), runtime, runtime->GetPrototype(DOM_Runtime::LOCATION_PROTOTYPE), "Location");
	PutPrivateL(DOM_PRIVATE_location, object);

	JS_Navigator::MakeL(object, runtime);
	PutPrivateL(DOM_PRIVATE_navigator, object);

	DOMSetObjectRuntimeL(object = OP_NEW(JS_Screen, ()), runtime, runtime->GetObjectPrototype(), "Screen");
	PutPrivateL(DOM_PRIVATE_screen, object);

#ifdef MEDIA_HTML_SUPPORT
	PutFunctionL("Audio", OP_NEW(DOM_HTMLAudioElement_Constructor, ()), "Audio", "s-");
	PutConstructorL(OP_NEW(DOM_TextTrackCue_Constructor, ()), "nnz-");
	DOM_TrackEvent_Constructor::AddConstructorL(this);
#endif // MEDIA_HTML_SUPPORT

#ifdef SVG_DOM
	PutObjectL("SVGException", object = OP_NEW(DOM_Object, ()), "SVGException", PROP_DONT_ENUM);
	DOM_SVGException::ConstructSVGExceptionObjectL(*object, runtime);

#ifdef SVG_FULL_11
	PutFunctionL("SVGUnitTypes", object = OP_NEW(DOM_Object, ()), "SVGUnitTypes");
	DOM_SVGElement::ConstructDOMImplementationSVGElementObjectL(*object, DOM_SVGElementInterface::SVG_INTERFACE_UNIT_TYPES, runtime);

	PutFunctionL("SVGZoomAndPan", object = OP_NEW(DOM_Object, ()), "SVGZoomAndPan");
	DOM_SVGElement::ConstructDOMImplementationSVGElementObjectL(*object, DOM_SVGElementInterface::SVG_INTERFACE_ZOOM_AND_PAN, runtime);
#endif // SVG_FULL_11
#endif // SVG_DOM

#ifdef WEBSOCKETS_SUPPORT
	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::EnableWebSockets, runtime->GetOriginURL()))
	{
		DOM_WebSocket_Constructor::AddConstructorL(this);
		DOM_CloseEvent_Constructor::AddConstructorL(this);
	}
#endif // WEBSOCKETS_SUPPORT

#ifdef DOM_DEBUGGER_SUPPORT
	DOM_Debugger *debugger;
	LEAVE_IF_ERROR(DOM_Debugger::Make(debugger, GetEnvironment()));

	DOMSetObject(&value, debugger);
	PutL("debugger", value);
#endif // DOM_DEBUGGER_SUPPORT

#ifdef GADGET_SUPPORT
	if (runtime->GetFramesDocument()->GetWindow()->GetGadget())
	{
		BOOL allowed = FALSE;
		LEAVE_IF_ERROR(OpSecurityManager::CheckSecurity(
			OpSecurityManager::GADGET_DOM,
			OpSecurityContext(GetFramesDocument()->GetURL()),
			OpSecurityContext(GetFramesDocument()->GetWindow()->GetGadget()),
			allowed));

		if (allowed)
		{
			DOM_Widget* widget;
			OpGadget* gadget = runtime->GetFramesDocument()->GetWindow()->GetGadget();
			OP_ASSERT(gadget);

			LEAVE_IF_ERROR(DOM_Widget::Make(widget, runtime, gadget));
			PutPrivateL(DOM_PRIVATE_widget, widget);
			ES_Value widget_value;
			DOMSetObject(&widget_value, widget);
			PutL("widget", widget_value, PROP_READ_ONLY);

# ifdef DOM_JIL_API_SUPPORT
			if (g_DOM_jilUtils->IsJILApiRequested(JILFeatures::JIL_API_WIDGET, runtime))
			{
				DOM_JILWidget* jil_widget;
				DOM_JILWidget::MakeL(jil_widget, runtime);

				PutPrivateL(DOM_PRIVATE_jilWidget, jil_widget);

				DOMSetObject(&widget_value, jil_widget);
				PutL("Widget", widget_value);

#  if defined DOM_HTTP_SUPPORT && defined DOM3_LOAD
				// Wrap XMLHttpRequest constructor with a special one for JIL widgets
				DOM_FunctionWithSecurityRuleWrapper* wrapper = OP_NEW(DOM_FunctionWithSecurityRuleWrapper, (xhr_constructor, "XMLHttpRequest"));
				PutFunctionL("XMLHttpRequest", wrapper, "XMLHttpRequest", NULL);
#  endif // DOM_HTTP_SUPPORT && DOM3_LOAD
			}
# endif // DOM_JIL_API_SUPPORT
		}
	}
#endif // GADGET_SUPPORT

	DOM_FileReader_Constructor* filereader_constructor = OP_NEW_L(DOM_FileReader_Constructor, ());
	PutConstructorL(filereader_constructor);
	DOM_FileReader::ConstructFileReaderObjectL(*filereader_constructor, runtime);

	PutConstructorL(OP_NEW_L(DOM_Blob_Constructor, ()));

#ifdef APPLICATION_CACHE_SUPPORT
	// Init window.applicationCache object
	DOM_ApplicationCache::ConstructApplicationCacheObjectL(*PutConstructorL("ApplicationCache", DOM_Runtime::APPLICATIONCACHE_PROTOTYPE), runtime);

	DOMSetObjectRuntimeL(object = OP_NEW(DOM_ApplicationCache, ()), runtime, runtime->GetPrototype(DOM_Runtime::APPLICATIONCACHE_PROTOTYPE), "ApplicationCache");
	DOM_ApplicationCache::ConstructApplicationCacheObjectL(*object, runtime);
	PutPrivateL(DOM_PRIVATE_applicationCache, object);
#endif // APPLICATION_CACHE_SUPPORT

#ifdef DOM_WEBWORKERS_SUPPORT
	PutConstructorL(OP_NEW(DOM_DedicatedWorkerObject_Constructor,()), "s");
	PutConstructorL(OP_NEW(DOM_SharedWorkerObject_Constructor,()), "ss-");
#endif // DOM_WEBWORKERS_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	PutConstructorL(OP_NEW(DOM_MessageChannel_Constructor,()));
	PutConstructorL(OP_NEW(DOM_MessageEvent_Constructor,()), "-O");
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef EVENT_SOURCE_SUPPORT
	DOM_EventSource_Constructor::AddConstructorL(this);
#endif // EVENT_SOURCE_SUPPORT

#ifdef DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT
	DOMSetObjectRuntimeL(object = OP_NEW(DOM_Performance, ()), runtime, runtime->GetPrototype(DOM_Runtime::PERFORMANCE_PROTOTYPE), "Performance");
	DOMSetObject(&value, object);
	PutL("performance", value);
#endif // DOM_WINDOW_PERFORMANCE_OBJECT_SUPPORT

#if defined WEBSERVER_SUPPORT || defined DOM_GADGET_FILE_API_SUPPORT
	if (runtime->GetFramesDocument()->GetWindow()->GetGadget())
		PutConstructorL(OP_NEW(DOM_ByteArray_Constructor, ()), "n-");
#endif //WEBSERVER_SUPPORT || DOM_GADGET_FILE_API_SUPPORT

#ifdef CSS_ANIMATIONS
	PutConstructorL("CSSKeyframesRule", DOM_Runtime::CSSKEYFRAMESRULE_PROTOTYPE, TRUE);
#endif // CSS_ANIMATIONS

	PutPrivateL(DOM_PRIVATE_opera, opera);

	((DOM_Runtime *) runtime)->GetEnvironment()->RegisterCallbacksL(DOM_Environment::GLOBAL_CALLBACK, this);

	runtime->SetErrorHandler(this);
}

#ifdef CLIENTSIDE_STORAGE_SUPPORT
OP_STATUS
JS_Window::AddStorageListeners()
{
	FramesDocument *frames_doc = GetFramesDocument();

	if (!frames_doc)
		return OpStatus::ERR;

	OP_ASSERT(GetEventTarget() && GetEventTarget()->HasListeners(STORAGE, NULL, ES_PHASE_AT_TARGET));

	OpStorageManager *storage_manager = frames_doc->GetWindow()->DocManager()->GetStorageManager(TRUE);
	RETURN_OOM_IF_NULL(storage_manager);

	DOM_PSUtils::PS_OriginInfo oi;
	RETURN_IF_ERROR(DOM_PSUtils::GetPersistentStorageOriginInfo(GetRuntime(), oi));

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	RETURN_IF_ERROR(storage_manager->AddStorageEventListener(
			WEB_STORAGE_WGT_PREFS, oi.m_context_id, oi.m_origin, oi.m_is_persistent,
			&m_widget_preferences_event_listener));
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT

	RETURN_IF_ERROR(storage_manager->AddStorageEventListener(
			WEB_STORAGE_LOCAL, oi.m_context_id, oi.m_origin, oi.m_is_persistent,
			&m_local_storage_event_listener));

	return storage_manager->AddStorageEventListener(
			WEB_STORAGE_SESSION, oi.m_context_id, oi.m_origin, oi.m_is_persistent,
			&m_session_storage_event_listener);
}

void
JS_Window::RemoveStorageListeners()
{
	FramesDocument *frames_doc = GetFramesDocument();

	if (!frames_doc)
		return;

	OP_ASSERT(GetEventTarget() && !GetEventTarget()->HasListeners(STORAGE, NULL, ES_PHASE_AT_TARGET));

	OpStorageManager *storage_manager = frames_doc->GetWindow()->DocManager()->GetStorageManager(FALSE);
	if (!storage_manager)
		return;

	DOM_PSUtils::PS_OriginInfo oi;
	RETURN_VOID_IF_ERROR(DOM_PSUtils::GetPersistentStorageOriginInfo(GetRuntime(), oi));

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	storage_manager->RemoveStorageEventListener(
			WEB_STORAGE_WGT_PREFS, oi.m_context_id, oi.m_origin, oi.m_is_persistent,
			&m_widget_preferences_event_listener);
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
	storage_manager->RemoveStorageEventListener(
			WEB_STORAGE_LOCAL, oi.m_context_id, oi.m_origin, oi.m_is_persistent,
			&m_local_storage_event_listener);
	storage_manager->RemoveStorageEventListener(
			WEB_STORAGE_SESSION, oi.m_context_id, oi.m_origin, oi.m_is_persistent,
			&m_session_storage_event_listener);
}
#endif // CLIENTSIDE_STORAGE_SUPPORT

/* virtual */ OP_STATUS
JS_Window::CreateEventTarget()
{
	if (!event_target)
		RETURN_OOM_IF_NULL(event_target = OP_NEW(DOM_JSWindowEventTarget, (this)));

	return OpStatus::OK;
}

JS_Window::~JS_Window()
{
	OP_ASSERT(current_error_handler_calls.Empty());

#ifdef JS_PLUGIN_SUPPORT
	if (jsplugin_context)
		g_jsPluginManager->DestroyContext(jsplugin_context);
#endif // JS_PLUGIN_SUPPORT

#ifdef USER_JAVASCRIPT
	OP_DELETE(userjs_magic_storage);
#endif // USER_JAVASCRIPT

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	RemoveOrientationListener();
	RemoveMotionListener();
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	OP_DELETE(m_search_suggestions_callback);
#endif
}

/* virtual */ BOOL
JS_Window::SecurityCheck(ES_Runtime *origining_runtime)
{
	return DOM_Object::SecurityCheck(origining_runtime);
}

/* virtual */ BOOL
JS_Window::SecurityCheck(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (value && (property_name == OP_ATOM_name || property_name == OP_ATOM_defaultStatus || property_name == OP_ATOM_status))
		if (!OriginCheck(origining_runtime))
			if (FramesDocument* frames_doc = GetRuntime()->GetFramesDocument())
				if (frames_doc->GetURL().Type() != URL_NULL_TYPE && !IsAboutBlankURL(frames_doc->GetURL()))
					return FALSE;

	if (property_name == OP_ATOM_frameElement
		|| property_name == OP_ATOM_event
		|| property_name == OP_ATOM_devicePixelRatio
#ifdef APPLICATION_CACHE_SUPPORT
		|| property_name == OP_ATOM_applicationCache
#endif // APPLICATION_CACHE_SUPPORT
		|| property_name == OP_ATOM_document
		)
		return OriginCheck(origining_runtime);

	/* Most things are accessible, since most things are about the window
	   rather than about the document in it. */
	return TRUE;
}

static OP_STATUS
GetTopFrame(DOM_Object*& top_frame, FramesDocument* frames_doc, DOM_Runtime *origining_runtime)
{
	OP_ASSERT(frames_doc);
	FramesDocument* pdoc = frames_doc;

	for (;;)
	{
		DocumentManager* doc_man = pdoc->GetDocManager();
		FramesDocument* ppdoc = doc_man->GetParentDoc();

		if (!ppdoc)
			break;

#ifdef _MIME_SUPPORT_
		// Check if it's just our dummy wrapper object. We don't
		// want it here, but as long as it is we better try to hide
		// it. See bug CORE-5631.
		URLCacheType cache_type = static_cast<URLCacheType>(ppdoc->GetURL().GetAttribute(URL::KCacheType));
		if (cache_type == URL_CACHE_MHTML)
			break;
#endif // _MIME_SUPPORT_

		pdoc = ppdoc;
	}

	if (pdoc != origining_runtime->GetFramesDocument())
		origining_runtime->GetEnvironment()->AccessedOtherEnvironment(pdoc);

	RETURN_IF_ERROR(pdoc->GetDocManager()->ConstructDOMProxyEnvironment());

	return static_cast<DOM_ProxyEnvironmentImpl *>(pdoc->GetDocManager()->GetDOMEnvironment())->GetProxyWindow(top_frame, origining_runtime);
}

OP_STATUS
JS_Window::GetTargetForEvent(const uni_char* eventname, DOM_Object*& result_target)
{
#ifdef DOM3_EVENTS
	const uni_char* namespaceURI = NULL;
	DOM_EventType known_type = DOM_Event::GetEventType(namespaceURI, eventname, FALSE);
#else // DOM3_EVENTS
	DOM_EventType known_type = DOM_Event::GetEventType(eventname, FALSE);
#endif // DOM3_EVENTS

	result_target = known_type == DOM_EVENT_NONE ? NULL : this;

	return OpStatus::OK;
}

OP_STATUS
JS_Window::CreateBodyNodeIfNeeded()
{
	if (!body_node_created)
		if (LogicalDocument *logdoc = GetLogicalDocument())
		{
			/* Need to create a node for the BODY element (or FRAMESET element,
			   if that be the case,) to trigger registration of event handlers
			   from attributes. */

			if (HTML_Element *root = logdoc->GetDocRoot())
				if (root->IsMatchingType(HE_HTML, NS_HTML))
				{
					HTML_Element *child = root->FirstChildActual();
					while (child && !(child->IsMatchingType(HE_BODY, NS_HTML) || child->IsMatchingType(HE_FRAMESET, NS_HTML)))
						child = child->SucActual();

					if (child)
					{
						DOM_Object *node;
						RETURN_IF_ERROR(GetEnvironment()->ConstructNode(node, child));
						OP_ASSERT(node->IsA(DOM_TYPE_NODE));
						static_cast<DOM_Node*>(node)->SetIsSignificant(); // Must not be recreated or it can overwrite event handlers set on window
						body_node_created = TRUE;
					}
				}
		}

	return OpStatus::OK;
}

#ifdef DOM_SHOWMODALDIALOG_SUPPORT
static ES_Value* GetDialogArguments(HTML_Dialog* dialog);
#endif // DOM_SHOWMODALDIALOG_SUPPORT

/* virtual */ ES_GetState
JS_Window::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	DOM_EnvironmentImpl *environment = GetEnvironment();
	FramesDocument *frames_doc = environment->GetFramesDocument();

	if (frames_doc)
		switch (property_name)
		{
		case OP_ATOM_innerHeight:
		case OP_ATOM_innerWidth:
		case OP_ATOM_outerHeight:
		case OP_ATOM_outerWidth:
		case OP_ATOM_pageXOffset:
		case OP_ATOM_pageYOffset:
		case OP_ATOM_screenLeft:
		case OP_ATOM_screenTop:
		case OP_ATOM_screenX:
		case OP_ATOM_screenY:
		case OP_ATOM_scrollX:
		case OP_ATOM_scrollY:
			GET_FAILED_IF_ERROR(frames_doc->Reflow(FALSE, TRUE));
		}

	switch (property_name)
	{
	case OP_ATOM_closed:
		DOMSetBoolean(value, GetFramesDocument() ? FALSE : TRUE);
		return GET_SUCCESS;

	case OP_ATOM_defaultStatus:
		DOMSetString(value, frames_doc ? frames_doc->GetWindow()->GetDefaultMessage() : NULL);
		return GET_SUCCESS;

#ifdef DOM_SHOWMODALDIALOG_SUPPORT
		// These only exist if it's a dialog opened with showModalDialog
	case OP_ATOM_dialogArguments:
	case OP_ATOM_returnValue:
		if (HTML_Dialog *dialog = g_html_dialog_manager->FindDialog(frames_doc->GetWindow()))
		{
			if (value)
			{
				if (property_name == OP_ATOM_dialogArguments)
					*value = *GetDialogArguments(dialog);
				else
					DOMSetString(value, dialog->GetResult());
			}
			return GET_SUCCESS;
		}
		break;
#endif // DOM_SHOWMODALDIALOG_SUPPORT

	case OP_ATOM_document:
		if (value)
		{
			DOM_Object *document;
			GET_FAILED_IF_ERROR(GetEnvironment()->GetProxyDocument(document, origining_runtime));
			DOMSetObject(value, document);
		}
		return GET_SUCCESS;

	case OP_ATOM_event:
		if (value)
		{
			/* If outside an event thread, DOM_PRIVATE_event holds the value (if any.)

			   If within and the event handler has updated window.event, then resolve
			   this [[Get]] to it. If not, it refers to the event of the event thread. */

			ES_Thread *thread = GetCurrentThread(origining_runtime);
			while (thread)
			{
				if (thread->Type() == ES_THREAD_EVENT && thread->GetScheduler()->GetRuntime() == GetRuntime())
				{
					DOM_EventThread *event_thread = static_cast<DOM_EventThread *>(thread);
					GET_FAILED_IF_ERROR(event_thread->GetWindowEvent(value));
					return GET_SUCCESS;
				}
				thread = thread->GetInterruptedThread();
			}
			GET_FAILED_IF_ERROR(GetPrivate(DOM_PRIVATE_event, value));
		}
		return GET_SUCCESS;

	case OP_ATOM_innerHeight:
	case OP_ATOM_innerWidth:
		{
			unsigned int width = 0, height = 0;

			if (frames_doc)
				frames_doc->DOMGetInnerWidthAndHeight(width, height);

			DOMSetNumber(value, property_name == OP_ATOM_innerHeight ? height : width);
			return GET_SUCCESS;
		}

	case OP_ATOM_location:
		if (value)
		{
			DOM_Object *location;
			GET_FAILED_IF_ERROR(GetEnvironment()->GetProxyLocation(location, origining_runtime));
			DOMSetObject(value, location);
		}
		return GET_SUCCESS;

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	case OP_ATOM_localStorage:
	case OP_ATOM_sessionStorage:
		{
			if (!OriginCheck(origining_runtime))
				return GET_SECURITY_VIOLATION;

			WebStorageType type = property_name == OP_ATOM_sessionStorage ? WEB_STORAGE_SESSION : WEB_STORAGE_LOCAL;

			int private_prop = property_name == OP_ATOM_localStorage
				? DOM_PRIVATE_localStorage : DOM_PRIVATE_sessionStorage;
			ES_GetState state = DOMSetPrivate(value, private_prop);

			if (state == GET_FAILED)
			{
				DOM_Storage *storage = NULL;

				GET_FAILED_IF_ERROR(DOM_Storage::Make(storage, type, GetRuntime()));
				OP_ASSERT(storage != NULL);
				GET_FAILED_IF_ERROR(PutPrivate(private_prop, *storage));

				DOMSetObject(value, storage);
				return GET_SUCCESS;
			}
			else
				return state;
		}
#endif // CLIENT_SIDE_STORAGE_SUPPORT

	case OP_ATOM_history:
		return DOMSetPrivate(value, DOM_PRIVATE_history);

	case OP_ATOM_navigator:
		return DOMSetPrivate(value, DOM_PRIVATE_navigator);

	case OP_ATOM_screen:
		return DOMSetPrivate(value, DOM_PRIVATE_screen);

	case OP_ATOM_name:
		if (value)
		{
			const uni_char* name;

			if (frames_doc)
			{
				FramesDocElm* fde = frames_doc->GetDocManager()->GetFrame();
				if (fde)
				{
					name = fde->GetName();
					if (!name)
						name = fde->GetFrameId();
				}
				else
					name = frames_doc->GetWindow()->Name();
			}
			else
				name = NULL;

			DOMSetString(value, name);
		}
		return GET_SUCCESS;

	case OP_ATOM_opener:
		if (value)
		{
			DOMSetNull(value);

			if (frames_doc)
			{
				Window* window = frames_doc->GetWindow();
				FramesDocument* opener = window->GetOpener();

				if (!frames_doc->GetParentDoc() && opener)
				{
					DOM_Object *js_window;

					GET_FAILED_IF_ERROR(opener->GetDocManager()->ConstructDOMProxyEnvironment());
					GET_FAILED_IF_ERROR(static_cast<DOM_ProxyEnvironmentImpl *>(opener->GetDocManager()->GetDOMEnvironment())->GetProxyWindow(js_window, origining_runtime));

					DOMSetObject(value, js_window);

					((DOM_Runtime *) origining_runtime)->GetEnvironment()->AccessedOtherEnvironment(opener);
				}
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_outerHeight:
	case OP_ATOM_outerWidth:
		if (value)
		{
			if (frames_doc)
			{
				int w, h;
				frames_doc->GetWindow()->GetWindowSize(w, h);
				DOMSetNumber(value, property_name == OP_ATOM_outerHeight ? h : w);
			}
			else
				DOMSetNumber(value, 0);
		}
		return GET_SUCCESS;

	case OP_ATOM_pageXOffset:
	case OP_ATOM_scrollX:
		if (OriginCheck(origining_runtime))
			DOMSetNumber(value, frames_doc ? frames_doc->DOMGetScrollOffset().x : 0);
		else
			DOMSetUndefined(value);
		return GET_SUCCESS;

	case OP_ATOM_pageYOffset:
	case OP_ATOM_scrollY:
		if (OriginCheck(origining_runtime))
			DOMSetNumber(value, frames_doc ? frames_doc->DOMGetScrollOffset().y : 0);
		else
			DOMSetUndefined(value);
		return GET_SUCCESS;

	case OP_ATOM_screenLeft:
	case OP_ATOM_screenTop:
		if (value)
		{
			if (frames_doc)
			{
				OpPoint screen_pt;
				CoreView* view = frames_doc->GetVisualDevice()->GetView();
				if (view) // or is this a removed iframe or something in case we leave it at (0,0)
					screen_pt = view->ConvertToScreen(screen_pt);
				DOMSetNumber(value, property_name == OP_ATOM_screenLeft ? screen_pt.x : screen_pt.y);
			}
			else
				DOMSetNumber(value, 0);
		}
		return GET_SUCCESS;

	case OP_ATOM_screenX:
	case OP_ATOM_screenY:
		if (value)
		{
			if (frames_doc)
			{
				int x, y;
				frames_doc->GetWindow()->GetWindowPos(x,y);
				DOMSetNumber(value, property_name == OP_ATOM_screenX ? x : y);
			}
			else
				DOMSetNumber(value, 0);
		}
		return GET_SUCCESS;

	case OP_ATOM_self:
	case OP_ATOM_window:
		if (value)
		{
			DOM_Object *window;
			GET_FAILED_IF_ERROR(GetEnvironment()->GetProxyWindow(window, origining_runtime));
			DOMSetObject(value, window);
		}
		return GET_SUCCESS;

	case OP_ATOM_parent:
		if (value)
		{
			// Parent document's window or this window in case we have no parent
			if (frames_doc)
			{
				DocumentManager *doc_man = frames_doc->GetDocManager();
				DOM_Object *parent;

				if (FramesDocElm *fde = doc_man->GetFrame())
				{
					FramesDocument *parent_doc = fde->GetParentFramesDoc();

#ifdef _MIME_SUPPORT_
					// Check if it's just our dummy wrapper object. We don't
					// want it here, but as long as it is we better try to hide
					// it. See bug CORE-5631.
					URLCacheType cache_type = static_cast<URLCacheType>(parent_doc->GetURL().GetAttribute(URL::KCacheType));
					if (cache_type == URL_CACHE_MHTML)
						parent_doc = frames_doc;
#endif // _MIME_SUPPORT_

					GET_FAILED_IF_ERROR(parent_doc->GetDocManager()->ConstructDOMProxyEnvironment());
					GET_FAILED_IF_ERROR(static_cast<DOM_ProxyEnvironmentImpl *>(parent_doc->GetDocManager()->GetDOMEnvironment())->GetProxyWindow(parent, origining_runtime));

					((DOM_Runtime *) origining_runtime)->GetEnvironment()->AccessedOtherEnvironment(parent_doc);
				}
				else
					GET_FAILED_IF_ERROR(GetEnvironment()->GetProxyWindow(parent, origining_runtime));

				DOMSetObject(value, parent);
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_status:
		DOMSetString(value, frames_doc ? frames_doc->GetWindow()->GetMessage() : 0);
		return GET_SUCCESS;

	case OP_ATOM_top:
		if (value)
		{
			/* See OP_ATOM_parent comment. */
			if (!GetCurrentThread(origining_runtime)->IsPluginThread())
			{
				ES_GetState result = DOMSetPrivate(value, DOM_PRIVATE_top);
				if (result != GET_FAILED)
					return result;
			}
			if (frames_doc)
			{
				DOM_Object* window;
				GET_FAILED_IF_ERROR(GetTopFrame(window, frames_doc, (DOM_Runtime *) origining_runtime));
				DOMSetObject(value, *window);
			}
			else
				DOMSetNull(value);
		}
		return GET_SUCCESS;

	case OP_ATOM_frameElement:
		if (value)
		{
			if (frames_doc)
			{
				DocumentManager *docman = frames_doc->GetDocManager();
				if (FramesDocument *pdoc = docman->GetParentDoc())
					if (FramesDocElm *frame = docman->GetFrame())
						if (HTML_Element *element = frame->GetHtmlElement())
						{
							GET_FAILED_IF_ERROR(pdoc->ConstructDOMEnvironment());
							DOM_EnvironmentImpl *pdoc_environment = static_cast<DOM_EnvironmentImpl *>(pdoc->GetDOMEnvironment());
							if (!pdoc_environment->GetWindow()->OriginCheck(origining_runtime))
								return GET_SECURITY_VIOLATION;
							GET_FAILED_IF_ERROR(pdoc_environment->ConstructDocumentNode());
							return static_cast<DOM_Document *>(pdoc_environment->GetDocument())->DOMSetElement(value, element);
						}
			}
			DOMSetNull(value);
		}
		return GET_SUCCESS;

#ifdef APPLICATION_CACHE_SUPPORT
	case OP_ATOM_applicationCache:
		return DOMSetPrivate(value, DOM_PRIVATE_applicationCache);
#endif // APPLICATION_CACHE_SUPPORT

	case OP_ATOM_devicePixelRatio:
		double dpr = 1.0;

		if (frames_doc)
		{
			Window* win = frames_doc->GetWindow();
			dpr *= win->GetTrueZoomBaseScale() / 100.0;
#ifdef PIXEL_SCALE_RENDERING_SUPPORT
			dpr *= win->GetPixelScale() / 100.0;
#endif // PIXEL_SCALE_RENDERING_SUPPORT
		}

		DOMSetNumber(value, dpr);
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

ES_GetState
JS_Window::GetEventOrInternalProperty(const uni_char *property_name, OpAtom property_atom, ES_Value *value, ES_Runtime *origining_runtime)
{
#ifdef USER_JAVASCRIPT
	ES_Value internal;
	if (DOM_PropertyStorage::Get(userjs_magic_storage, property_name, &internal))
	{
		ES_GetState result = GET_SUCCESS;

		if (OriginLoadCheck(GetRuntime(), origining_runtime) == NO)
			return GET_SECURITY_VIOLATION;

		if (internal.type == VALUE_OBJECT)
		{
			DOM_HOSTOBJECT_SAFE(function, internal.value.object, DOM_TYPE_USERJSMAGICFUNCTION, DOM_UserJSMagicFunction);
			if (function)
				if (is_fixing_magic_names)
					return GET_FAILED;
				else if (!function->GetReal())
					internal.type = VALUE_UNDEFINED;

			DOM_HOSTOBJECT_SAFE(variable, internal.value.object, DOM_TYPE_USERJSMAGICVARIABLE, DOM_UserJSMagicVariable);
			if (variable)
				if (is_fixing_magic_names)
					return GET_FAILED;
				else if (value)
					result = variable->GetValue(&internal, origining_runtime);
		}

		if (value && (result == GET_SUCCESS || result == GET_SUSPEND))
			*value = internal;

		return result;
	}
#endif // USER_JAVASCRIPT

	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		/* Fetch "old style" event handler function from event_target. */

		// Opera has historically allowed people to set
		// different types of listeners on the window and get
		// them on the document. That may be wrong (or not),
		// but we want to keep that behaviour for now.
		DOM_Object *target;
		GET_FAILED_IF_ERROR(GetTargetForEvent(property_name + 2, target));
		if (target)
		{
			OP_ASSERT(target == this || target->IsA(DOM_TYPE_DOCUMENT));

			if (target == this)
				GET_FAILED_IF_ERROR(CreateBodyNodeIfNeeded());

			ES_GetState result = target->GetEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
			BOOL secure = OriginLoadCheck(GetRuntime(), origining_runtime) != NO;
			if (result != GET_FAILED)
				return secure ? result : GET_SECURITY_VIOLATION;
		}
	}

	return GET_FAILED;
}

static BOOL
LookupConstructorProperty(const uni_char *property_name, DOM_ConstructorInformation *&data)
{
	if (*property_name >= 'A' && *property_name <= 'Z')
	{
		unsigned length = 0, longest = g_opera->dom_module.longest_constructor_name;
		uni_char ch;
		char name[64]; // ARRAY OK 2010-07-01 sof

		while ((name[length] = static_cast<char>(ch = property_name[length])) != 0)
			if (++length > longest || !(ch >= 'A' && ch <= 'Z' || ch >= 'a' && ch <= 'z' || ch == '2'))
				return FALSE;

		OpString8HashTable<DOM_ConstructorInformation> *constructors = static_cast<OpString8HashTable<DOM_ConstructorInformation> *>(g_opera->dom_module.constructors);

		return OpStatus::IsSuccess(constructors->GetData(name, &data));
	}
	else
		return FALSE;
}

/* virtual */ ES_GetState
JS_Window::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	// Order as we think so far. See for instance CORE-5476 about the home property
	//  1a. normal functions (outside this method)
	//  1b. constructor properties (created on first access, then found in 1a)
	//  2. the global window property (to avoid frames named "window" overwrite that property. See CORE-28697)
	//  3. the fixed properties frames and length
	//  4. frames (a frame named "home" should be found before the home() function)
	//  5. special functions (close(), home())
	//  6. event properties
	//  7. everything else

	DOM_ConstructorInformation *data;
	if (*property_name >= 'A' && *property_name <= 'Z' && LookupConstructorProperty(property_name, data))
	{
		if (!OriginCheck(origining_runtime))
			return GET_SECURITY_VIOLATION;

		OP_STATUS status = GetRuntime()->CreateConstructor(value, this, data->name, data->ns, data->id);

		if (status == OpStatus::OK)
			return GET_SUCCESS;
		else if (status == OpStatus::ERR_NO_MEMORY)
			return GET_NO_MEMORY;
	}

	// Frame information (frames, length [index]) is not security protected.
	if (property_code == OP_ATOM_window || property_code == OP_ATOM_frames)
	{
		if (property_code == OP_ATOM_frames && OriginCheck(origining_runtime))
		{
			ES_GetState state = DOMSetPrivate(value, DOM_PRIVATE_frames);
			if (state != GET_FAILED)
				return state;
		}
		DOM_Object *proxy_window;
		GET_FAILED_IF_ERROR(GetEnvironment()->GetProxyWindow(proxy_window, origining_runtime));
		DOMSetObject(value, proxy_window);
		return GET_SUCCESS;
	}

	FramesDocument* frames_doc = GetFramesDocument();

	if (property_code == OP_ATOM_length)
	{
		DOMSetNumber(value, DOM_CountFrames(frames_doc));
		return GET_SUCCESS;
	}

	// Look for frames; like for indexed frame access, not security protected.
	ES_GetState result = DOM_GetWindowFrame(value, frames_doc, property_name, 0, static_cast<DOM_Runtime *>(origining_runtime));
	if (result != GET_FAILED)
		return result;

	result = GetEventOrInternalProperty(property_name, static_cast<OpAtom>(property_code), value, origining_runtime);
	if (result != GET_FAILED)
		return result;

	result = DOM_Object::GetName(property_name, property_code, value, origining_runtime);
	if (result != GET_FAILED)
		return result;

#ifdef JS_PLUGIN_SUPPORT
	if (jsplugin_context)
	{
		BOOL cacheable = FALSE;
		ES_GetState result;

		if (jsplugin_context->GetName(property_name, NULL, &cacheable) == GET_SUCCESS && !OriginCheck(origining_runtime))
			return GET_SECURITY_VIOLATION;

		cacheable = FALSE;
		switch (result = jsplugin_context->GetName(property_name, value, &cacheable))
		{
		case GET_FAILED:
			break;

		case GET_SUCCESS:
			if (cacheable && value)
				GET_FAILED_IF_ERROR(Put(property_name, *value));
			/* fall through */

		default:
			return result;
		}
	}
#endif // JS_PLUGIN_SUPPORT

	return GET_FAILED;
}

#ifdef USER_JAVASCRIPT

/* virtual */ ES_GetState
JS_Window::GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	ES_Value magic;

	if (DOM_PropertyStorage::Get(userjs_magic_storage, property_name, &magic) && magic.type == VALUE_OBJECT)
	{
		DOM_HOSTOBJECT_SAFE(variable, magic.value.object, DOM_TYPE_USERJSMAGICVARIABLE, DOM_UserJSMagicVariable);
		if (variable)
			return variable->GetValue(value, origining_runtime, restart_object);
	}

	DOMSetUndefined(value);
	return GET_SUCCESS;
}

#endif // USER_JAVASCRIPT

/* virtual */ ES_GetState
JS_Window::GetIndex(int property_index, ES_Value* value, ES_Runtime *origining_runtime)
{
	// Frame information (frames, length [index]) is not security protected.
	return DOM_GetWindowFrame(value, GetFramesDocument(), NULL, property_index, static_cast<DOM_Runtime *>(origining_runtime));
}

/* virtual */ ES_PutState
JS_Window::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_name:
		{
			ES_Thread *thread = GetCurrentThread(origining_runtime);
			if (origining_runtime->GetIsInIdentifierExpression(thread->GetContext()))
			{
				/* Overwrite it.  Don't change the frame's name, the others don't, and
				   frame lookup by name breaks if the frame's name is changed. */
				return PUT_FAILED;
			}
			else
			{
				if (value->type != VALUE_STRING)
					return PUT_NEEDS_STRING;
				else
				{
					const uni_char* name = value->value.string;
					if (FramesDocument *frames_doc = GetFramesDocument())
					{
						if (FramesDocElm* fde = frames_doc->GetDocManager()->GetFrame())
							PUT_FAILED_IF_ERROR(fde->SetName(name));
						else
							frames_doc->GetWindow()->SetName(name);
					}
				}
				return PUT_SUCCESS;
			}
		}

	case OP_ATOM_location:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else
		{
			ES_Value location;
			PUT_FAILED_IF_ERROR(GetPrivate(DOM_PRIVATE_location, &location));
			return DOM_VALUE2OBJECT(location, JS_Location)->PutName(OP_ATOM_href, value, origining_runtime);
		}

	case OP_ATOM_defaultStatus:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (FramesDocument *frames_doc = GetFramesDocument())
			PUT_FAILED_IF_ERROR(frames_doc->GetWindow()->SetDefaultMessage(value->value.string));
		return PUT_SUCCESS;

	case OP_ATOM_status:
		if (value->type != VALUE_STRING)
			return PUT_NEEDS_STRING;
		else if (FramesDocument *frames_doc = GetFramesDocument())
			PUT_FAILED_IF_ERROR(frames_doc->GetWindow()->SetMessage(value->value.string));
		return PUT_SUCCESS;

	case OP_ATOM_event:
		{
			/* If "window.event" is updated while inside an event handler, store
			   the value on the thread so that the binding will go out of scope
			   on thread completion.

			   If not within an event handler, simply store it on a private. */

			ES_Thread *thread = GetCurrentThread(origining_runtime);
			while (thread)
			{
				if (thread->Type() == ES_THREAD_EVENT && thread->GetScheduler()->GetRuntime() == GetRuntime())
				{
					DOM_EventThread *event_thread = static_cast<DOM_EventThread *>(thread);
					PUT_FAILED_IF_ERROR(event_thread->PutWindowEvent(value));
					return PUT_SUCCESS;
				}
				thread = thread->GetInterruptedThread();
			}
			PUT_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_event, *value));
			return PUT_SUCCESS;
		}

	case OP_ATOM_outerHeight:
	case OP_ATOM_outerWidth:
	case OP_ATOM_screenX:
	case OP_ATOM_screenY:
		if (value->type != VALUE_NUMBER)
			return PUT_NEEDS_NUMBER;
		else if (FramesDocument *frames_doc = GetFramesDocument())
		{
			Window* window = frames_doc->GetWindow();
			int x, y, w, h, new_value = (int) value->value.number;

			window->GetWindowPos(x, y);
			window->GetWindowSize(w, h);

			if (property_name == OP_ATOM_outerHeight)
				h = new_value;
			else if (property_name == OP_ATOM_outerWidth)
				w = new_value;
			else if (property_name == OP_ATOM_screenX)
				x = new_value;
			else
				y = new_value;

			window->SetWindowPos(x, y);
			window->SetWindowSize(w, h);
		}
		return PUT_SUCCESS;

#ifdef ABOUT_HTML_DIALOGS
	case OP_ATOM_dialogArguments:
	case OP_ATOM_returnValue:
		// These properties only exist if this window is a dialog
		// opened with showModalDialog
		if (FramesDocument *frames_doc = GetFramesDocument())
		{
			if (HTML_Dialog *dialog = g_html_dialog_manager->FindDialog(frames_doc->GetWindow()))
			{
				// dialogArguments is read-only
				if (property_name == OP_ATOM_dialogArguments)
					return PUT_READ_ONLY;

				if (value->type != VALUE_STRING)
					return PUT_NEEDS_STRING;
				else
					PUT_FAILED_IF_ERROR(dialog->SetResult(value->value.string));
				return PUT_SUCCESS;
			}
		}
		break;
#endif // ABOUT_HTML_DIALOGS

	case OP_ATOM_top:
		if (!GetCurrentThread(origining_runtime)->IsPluginThread() && has_top_declared)
			PUT_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_top, *value));

		return PUT_SUCCESS;

	case OP_ATOM_frames:
		if (OriginCheck(origining_runtime))
			PUT_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_frames, *value));
		return PUT_SUCCESS;

	case OP_ATOM_window:
	case OP_ATOM_document:
	case OP_ATOM_frameElement:
	case OP_ATOM_devicePixelRatio:
		return PUT_SUCCESS;
#ifdef CLIENTSIDE_STORAGE_SUPPORT
	case OP_ATOM_localStorage:
	case OP_ATOM_sessionStorage:
		if (!OriginCheck(origining_runtime))
			return PUT_SECURITY_VIOLATION;
		return PUT_SUCCESS;
#endif // CLIENT_SIDE_STORAGE_SUPPORT
	}

	return PUT_FAILED;
}

/* virtual */ ES_PutState
JS_Window::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
#ifdef USER_JAVASCRIPT
	ES_Value magic;
	if (DOM_PropertyStorage::Get(userjs_magic_storage, property_name, &magic) && magic.type == VALUE_OBJECT)
	{
		DOM_HOSTOBJECT_SAFE(function, magic.value.object, DOM_TYPE_USERJSMAGICFUNCTION, DOM_UserJSMagicFunction);
		if (function)
		{
			if (value->type == VALUE_OBJECT && op_strcmp(ES_Runtime::GetClass(value->value.object), "Function") == 0)
			{
				function->SetReal(value->value.object);
				return PUT_SUCCESS;
			}
		}

		DOM_HOSTOBJECT_SAFE(variable, magic.value.object, DOM_TYPE_USERJSMAGICVARIABLE, DOM_UserJSMagicVariable);
		if (variable)
			return variable->SetValue(value, (DOM_Runtime *) origining_runtime);
	}
#endif // USER_JAVASCRIPT

	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		// Opera has historically allowed people to set
		// different types of listeners on the window and get
		// them on the document. That may be wrong (or not),
		// but we want to keep that behaviour for now.
		DOM_Object* target;
		PUT_FAILED_IF_ERROR(GetTargetForEvent(property_name + 2, target));
		if (target)
		{
			OP_ASSERT(target == this || target->IsA(DOM_TYPE_DOCUMENT));

			if (target == this)
				PUT_FAILED_IF_ERROR(CreateBodyNodeIfNeeded());

			ES_PutState state = target->PutEventProperty(property_name, value, static_cast<DOM_Runtime *>(origining_runtime));
			if (state != PUT_FAILED)
				return state;
		}
	}

	ES_PutState state = DOM_Object::PutName(property_name, property_code, value, origining_runtime);
	if (state != PUT_FAILED)
		return state;

#ifdef JS_PLUGIN_SUPPORT
	if (jsplugin_context)
	{
		if (jsplugin_context->PutName(property_name, NULL) == PUT_SUCCESS && !OriginCheck(origining_runtime))
			return PUT_SECURITY_VIOLATION;

		state = jsplugin_context->PutName(property_name, value);
		if (state != PUT_FAILED)
			return state;
	}
#endif // JS_PLUGIN_SUPPORT

	return PUT_FAILED;
}

#ifdef USER_JAVASCRIPT

/* virtual */ ES_PutState
JS_Window::PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	ES_Value magic;

	if (DOM_PropertyStorage::Get(userjs_magic_storage, property_name, &magic) && magic.type == VALUE_OBJECT)
	{
		DOM_HOSTOBJECT_SAFE(variable, magic.value.object, DOM_TYPE_USERJSMAGICVARIABLE, DOM_UserJSMagicVariable);
		if (variable)
			return variable->SetValue(value, origining_runtime, restart_object);
	}

	return PUT_FAILED;
}

#endif // USER_JAVASCRIPT

/* virtual */ ES_DeleteStatus
JS_Window::DeleteName(const uni_char *property_name, ES_Runtime *origining_runtime)
{
	if (!has_frames_declared && uni_str_eq(property_name, "frames"))
		if (OriginCheck(origining_runtime))
			DELETE_FAILED_IF_MEMORY_ERROR(DeletePrivate(DOM_PRIVATE_frames));

	return DELETE_OK;
}

/* virtual */ BOOL
JS_Window::AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op)
{
	if (op == ALLOW_NATIVE_OVERRIDE || op == ALLOW_NATIVE_DECLARED_OVERRIDE)
	{
		DOM_ConstructorInformation *dummy_data;
		if (LookupConstructorProperty(property_name, dummy_data))
			return FALSE;

		/* not a constructor; fall through to consider other properties */
	}

	if (uni_str_eq(property_name, "top"))
	{
		if (op == ALLOW_NATIVE_DECLARED_OVERRIDE)
		{
			/* Record that the 'top' property was declared, but disallow it
			   as a native property. Use this bit of information to only
			   permit [[Put]] over declared 'top' properties. */
			has_top_declared = TRUE;

			ES_Value undefined;
			OpStatus::Ignore(PutPrivate(DOM_PRIVATE_top, undefined));
		}
		return FALSE;
	}
	if (uni_str_eq(property_name, "frames"))
	{
		if (op == ALLOW_NATIVE_DECLARED_OVERRIDE)
		{
			/* Similarly for 'frames', but declaration is only considered
			   by DeleteName(). */
			has_frames_declared = TRUE;

			ES_Value undefined;
			OpStatus::Ignore(PutPrivate(DOM_PRIVATE_frames, undefined));
		}
		return FALSE;
	}

	if (uni_str_eq(property_name, "window") ||
	    uni_str_eq(property_name, "document") ||
	    uni_str_eq(property_name, "location") ||
	    uni_str_eq(property_name, "frameElement"))
		return FALSE;

#ifdef USER_JAVASCRIPT
	DOM_UserJSMagicFunction *function = first_magic_function;
	while (function)
	{
		if (uni_str_eq(property_name, function->GetFunctionName()))
			return FALSE;

		function = function->GetNext();
	}

	DOM_UserJSMagicVariable *variable = first_magic_variable;
	while (variable)
	{
		if (uni_str_eq(property_name, variable->GetVariableName()))
			return FALSE;

		variable = variable->GetNext();
	}
#endif // USER_JAVASCRIPT

	return TRUE;
}

/* virtual */ void
JS_Window::GCTrace()
{
	GetEnvironment()->GCTrace();
	GCMark(window_named_properties);
	GCMark(event_target);
#ifdef USER_JAVASCRIPT
	DOM_PropertyStorage::GCTrace(GetRuntime(), userjs_magic_storage);
#endif // USER_JAVASCRIPT
	GCMark(error_handler);
}

/* virtual */ ES_GetState
JS_Window::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	// 'window' property taking precedence. Webkit and Gecko seem to put it in front as well.
	enumerator->AddPropertyL(UNI_L("window"), OP_ATOM_window);

	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	if (FramesDocument *frames_doc = GetFramesDocument())
		for (int index = 0, length = frames_doc->CountJSFrames(); index < length; ++index)
			if (FramesDocument *doc = frames_doc->GetJSFrame(index))
				if (FramesDocElm *frame = doc->GetDocManager()->GetFrame())
				{
					const uni_char *name = frame->GetName();
					if (name)
						enumerator->AddPropertyL(name);
					const uni_char *id = frame->GetFrameId();
					if (id && (!name || !uni_str_eq(id, name)))
						enumerator->AddPropertyL(id);

				}

	return GET_SUCCESS;
}

/* virtual */ ES_GetState
JS_Window::GetIndexedPropertiesLength(unsigned &count, ES_Runtime *origining_runtime)
{
	if (FramesDocument *frames_doc = GetFramesDocument())
		count = DOM_CountFrames(frames_doc);
	else
		count = 0;

	return GET_SUCCESS;
}

OP_STATUS
JS_Window::AddKeepAlive(DOM_Object *object, int *id)
{
	RETURN_IF_ERROR(PutPrivate(keep_alive_counter, *object));

	if (id)
		*id = keep_alive_counter;

	++keep_alive_counter;
	return OpStatus::OK;
}

void
JS_Window::RemoveKeepAlive(int id)
{
	OpStatus::Ignore(DeletePrivate(id));
}

#ifdef USER_JAVASCRIPT

OP_STATUS
JS_Window::AddMagicFunction(const uni_char *name, ES_Object *impl)
{
	DOM_UserJSMagicFunction *function = first_magic_function, *previous = NULL;

	while (function)
		if (uni_strcmp(function->GetFunctionName(), name) == 0)
		{
			if (previous)
				previous->SetNext(function->GetNext());
			else
				first_magic_function = function->GetNext();

			OP_DELETE(function);
			break;
		}
		else
		{
			previous = function;
			function = function->GetNext();
		}

	RETURN_IF_ERROR(DOM_UserJSMagicFunction::Make(function, GetEnvironment(), name, impl));

	ES_Value value;
	DOMSetObject(&value, function);

	RETURN_IF_ERROR(DOM_PropertyStorage::Put(userjs_magic_storage, name, TRUE/*enumerable*/, value));

	function->SetNext(first_magic_function);
	first_magic_function = function;
	return OpStatus::OK;
}

OP_STATUS
JS_Window::AddMagicVariable(const uni_char *name, ES_Object *getter, ES_Object *setter)
{
	DOM_UserJSMagicVariable *variable = first_magic_variable, *previous = NULL;

	while (variable)
		if (uni_strcmp(variable->GetVariableName(), name) == 0)
		{
			if (previous)
				previous->SetNext(variable->GetNext());
			else
				first_magic_variable = variable->GetNext();

			OP_DELETE(variable);
			break;
		}
		else
		{
			previous = variable;
			variable = variable->GetNext();
		}

	RETURN_IF_ERROR(DOM_UserJSMagicVariable::Make(variable, GetEnvironment(), name, getter, setter));

	ES_Value value;
	DOMSetObject(&value, variable);

	RETURN_IF_ERROR(DOM_PropertyStorage::Put(userjs_magic_storage, name, TRUE/*enumerable*/, value));

	variable->SetNext(first_magic_variable);
	first_magic_variable = variable;
	return OpStatus::OK;
}

#endif // USER_JAVASCRIPT

JS_Location *
JS_Window::GetLocation()
{
	ES_Value value;

	if (GetPrivate(DOM_PRIVATE_location, &value) == OpBoolean::IS_TRUE)
		return DOM_VALUE2OBJECT(value, JS_Location);
	else
		return NULL;
}

#ifdef WEBSERVER_SUPPORT
DOM_WebServer*
JS_Window::GetWebServer()
{
	ES_Value value;

	if (GetPrivate(DOM_PRIVATE_webserver, &value) == OpBoolean::IS_TRUE)
		return DOM_VALUE2OBJECT(value, DOM_WebServer);
	else
		return NULL;
}
#endif // WEBSERVER

/* static */ BOOL
JS_Window::IsUnrequestedThread(ES_Thread *thread)
{
	if (thread)
	{
		/* Get information about the "origin" thread of this thread
		   (which might be the thread itself). */

		ES_ThreadInfo info = thread->GetOriginInfo();
		return !info.is_user_requested;
	}

	return FALSE;
}

static double GetTotalRequestedDelay(ES_Thread *thread)
{
	while (thread)
	{
		if (thread->Type() == ES_THREAD_TIMEOUT)
			return static_cast<ES_TimeoutThread*>(thread)->GetTimerEvent()->GetTotalRequestedDelay();

		thread = thread->GetInterruptedThread();
	}

	return 0;
}

/* static */ BOOL
JS_Window::IsUnrequestedPopup(ES_Thread *thread)
{
	if (thread)
	{
		/* Get information about the "origin" thread of this thread
		   (which might be the thread itself). */

		ES_ThreadInfo info = thread->GetOriginInfo();
		if (!info.open_in_new_window && (!info.is_user_requested || info.has_opened_new_window))
			return TRUE;

		// A popup opening after more than 1000ms isn't really "requested".
		if (GetTotalRequestedDelay(thread) > 1000)
			return TRUE;
	}

	return FALSE;
}

/* static */ BOOL
JS_Window::RefusePopup(FramesDocument *frames_doc, ES_Thread *thread, BOOL unrequested)
{
	ServerName *hostname = DOM_EnvironmentImpl::GetHostName(frames_doc);

	if (thread)
	{
		ES_ThreadInfo info = thread->GetOriginInfo();
		if (info.open_in_new_window)
			return FALSE;
	}

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::IgnoreTarget, hostname))
		return TRUE;
	else if (g_pcjs->GetIntegerPref(PrefsCollectionJS::IgnoreUnrequestedPopups, hostname) && unrequested)
		return TRUE;
	else
		return FALSE;
}

/* static */ BOOL
JS_Window::OpenInBackground(FramesDocument *frames_doc, ES_Thread *thread)
{
	if (thread)
	{
		ES_ThreadInfo info = thread->GetOriginInfo();
		if (info.open_in_background)
			return TRUE;
		else if (info.open_in_new_window)
			return FALSE;
	}

	ServerName *hostname = DOM_EnvironmentImpl::GetHostName(frames_doc);

	switch (g_pcjs->GetIntegerPref(PrefsCollectionJS::TargetDestination, hostname))
	{
	case POPUP_WIN_IGNORE:
	case POPUP_WIN_BACKGROUND:
		return TRUE;
	}

	return FALSE;
}

static BOOL
IsAllowedToNavigate(JS_Window *this_object, ES_Runtime* origining_runtime)
{
	BOOL allowed;
	return (OpStatus::IsSuccess(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_ALLOWED_TO_NAVIGATE, static_cast<DOM_Runtime *>(origining_runtime), this_object->GetRuntime(), allowed)) && allowed);
}

/* Refuse certain dialogs that interfere with the necessary UI features
   such as context menus.  This should perhaps be configurable, but
   until it is, we have to block it. */
static BOOL
RefuseDialog(ES_Runtime *origining_runtime)
{
	/* Get information about the "origin" thread of this thread
	   (which might be the thread itself). */
	ES_Thread* thread = origining_runtime->GetESScheduler()->GetCurrentThread();
	ES_ThreadInfo origin_info = thread->GetOriginInfo();

	if (origin_info.type == ES_THREAD_EVENT)
		switch (origin_info.data.event.type)
		{
		case ONMOUSEDOWN:
		case ONMOUSEUP:
		case ONCLICK:
			/* Ignore dialogs opened from right mouse button events. */
			if (origin_info.data.event.data.button == MOUSE_BUTTON_2)
				return TRUE;
			break;
#ifdef OP_KEY_ALT_ENABLED
		case ONKEYDOWN:
		case ONKEYUP:
		case ONKEYPRESS:
			/* Ignore dialogs from pressing ALT since that may prevent usage of ALT-commands. */
			if (origin_info.data.event.data.keycode == OP_KEY_ALT)
				return TRUE;
#endif // OP_KEY_ENABLED
		}

	return FALSE;
}

/* static */ OP_STATUS
JS_Window::OpenPopupWindow(Window *&window, DOM_Object *opener, const URL &url, DocumentReferrer refurl, const uni_char *name, const OpRect* rect, BOOL3 scrollbars, BOOL3 location, BOOL3 open_in_background, BOOL set_opener, BOOL user_initiated, BOOL from_window_open)
{
	Window *opener_window = NULL;
	int opener_subwinid = -1;

	if (g_pcdoc->GetIntegerPref(PrefsCollectionDoc::SingleWindowBrowsing))
	{
		URL tmpurl = url;

		FramesDocument *frames_doc = opener->GetFramesDocument()->GetTopDocument();
		frames_doc->GetDocManager()->OpenURL(tmpurl, refurl, TRUE, FALSE, user_initiated, TRUE);
		window = frames_doc->GetWindow();

		return OpStatus::OK;
	}

	if (opener)
		if (FramesDocument *frames_doc = opener->GetFramesDocument())
		{
			opener_window = frames_doc->GetWindow();
			opener_subwinid = frames_doc->GetSubWinId();
		}

	window = g_windowManager->SignalNewWindow(opener_window, rect ? rect->width : 0, rect ? rect->height : 0, scrollbars, location, YES, open_in_background);

#ifdef WEBSERVER_SUPPORT
	// Never set opener for windows opened from unite admin page
	if (refurl.url.GetAttribute(URL::KIsUniteServiceAdminURL))
		set_opener = FALSE;
#endif // WEBSERVER_SUPPORT

#ifdef EXTENSION_SUPPORT
	if (refurl.url.Type() == URL_WIDGET)
	{
		if (url.Type() == URL_WIDGET && opener_window && opener_window->GetGadget() && opener_window->GetGadget()->IsExtension())
			window->SetGadget(opener_window->GetGadget());
		else
			set_opener = FALSE;
	}
#endif //EXTENSION_SUPPORT

	if (window)
	{
		if (name)
			window->SetName(name);

		if (opener)
			window->SetOpener(opener_window, opener_subwinid, set_opener, from_window_open);

		if (rect)
		{
			window->SetWindowPos(rect->x, rect->y);

			// If we got a too small window from the platform,
			// we will enable scrollbars regardless
			if (scrollbars == NO)
			{
				VisualDevice* vd = window->VisualDev();
				if (!rect->IsEmpty() &&	(vd->GetRenderingViewWidth() < rect->width || vd->GetRenderingViewHeight() < rect->height))
					// We received a too small window. Possibly because of a small device screen
					// Enable scrollbars after all so that the user may use the window.
					scrollbars = YES;
			}
		}

		BOOL show_scrollbars = scrollbars != NO;
		if (scrollbars == MAYBE)
			show_scrollbars = g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars);
		window->SetShowScrollbars(show_scrollbars);

#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
		OpScopeWindowListener::OnWindowTitleChanged(window);
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
	}
	else
		return OpStatus::ERR;

	if (!IsAboutBlankURL(url))
	{
		// We can get an about:blank through DocumentManager::CreateInitialEmptyDocument() if
		// it's needed. Might save us a few milliseconds in case it's not needed.
		DocumentManager *docman = window->DocManager();
		URL tmpurl = url;

		docman->OpenURL(tmpurl, refurl, TRUE, FALSE, user_initiated);
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
JS_Window::WriteDocumentData(Window *window, const uni_char *data)
{
	DocumentManager *docman = window->DocManager();
	if (FramesDocument *doc = docman->GetCurrentDoc())
	{
		RETURN_IF_ERROR(doc->ConstructDOMEnvironment());

		if (DOM_EnvironmentImpl *environment = (DOM_EnvironmentImpl *) doc->GetDOMEnvironment())
		{
			ES_Value value;
			DOMSetString(&value, data);

			DOM_Object *object;
			if (OpStatus::IsSuccess(DOMSetObjectRuntime(object = OP_NEW(DOM_Object, ()), environment->GetDOMRuntime())) &&
			    OpStatus::IsSuccess(object->Put(UNI_L("x"), value)))
			{
				ES_Object *esobject = *object;
				RETURN_IF_ERROR(environment->GetAsyncInterface()->Eval(UNI_L("with(document){write(x);close();}"), &esobject, 1, NULL, NULL));
			}
		}
	}

	return OpStatus::OK;
}

class DOM_JSWCCallback
	: public DOM_Object,
	  public ES_ThreadListener,
	  public OpDocumentListener::JSPopupCallback,
	  public OpDocumentListener::JSPopupOpener,
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	  public AsyncHTMLDialogCallback,
#endif // DOM_SHOWMODALDIALOG_SUPPORT
	  public OpDocumentListener::JSDialogCallback
{
public:
	enum Type { ALERT, CONFIRM, PROMPT, POPUP
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	          , MODALHTML
#endif // DOM_SHOWMODALDIALOG_SUPPORT
	          };

	DOM_JSWCCallback(ES_Thread *thread, Type type);
	~DOM_JSWCCallback();

	/* From DOM_Object. */
	virtual void GCTrace();

	/* From ES_ThreadListener. */
	virtual OP_STATUS Signal(ES_Thread *, ES_ThreadSignal signal);

	/* From OpDocumentListener::JSDialogCallback. */
	virtual void OnAlertDismissed();
	virtual void OnConfirmDismissed(BOOL ok);
	virtual void OnPromptDismissed(BOOL ok, const uni_char *value);
	virtual void AbortScript();

#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	// From AsyncHTMLDialogCallback
	virtual OP_STATUS OnClose(const uni_char *result);

	virtual HTMLDialogCallbackType	GetCallbackType() { return AsyncHTMLDialogCallback::HTML_DIALOG_TYPE_JS; }
#endif // DOM_SHOWMODALDIALOG_SUPPORT

	/* From OpDocumentListener::JSPopupCallback. */
	virtual void Continue(Action action, OpDocumentListener::JSPopupOpener **opener);

	/* From OpDocumentListener::JSPopupOpener. */
	virtual void Open();
	virtual void Release();

	Type type;
	BOOL called;
	ES_Value return_value;
	URL url;
	DocumentReferrer refurl;
	TempBuffer documentdata;
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	ES_Value dialog_arguments;
#endif // DOM_SHOWMODALDIALOG_SUPPORT
#ifdef DRAG_SUPPORT
	DropType drop_type;
	DropType visual_drop_type;
#endif // DRAG_SUPPORT

	union
	{
		struct
		{
			uni_char *message;
			uni_char *default_value;
			uni_char *return_value_string;
		} dialog;
		struct
		{
			DOM_Object *opener, *fakewindowproxy;
			uni_char *name;
			int left, top, width, height;
			BOOL3 scrollbars, location, open_in_background;
			bool refuse, replace;
			bool is_delayed;
			bool open, has_data;
		} popup;
	} data;

private:
	void Restart(BOOL abort);

	ES_Thread *thread;
};

DOM_JSWCCallback::DOM_JSWCCallback(ES_Thread *thread, Type type)
	: type(type)
	, called(FALSE)
#ifdef DRAG_SUPPORT
	, drop_type(DROP_NONE)
	, visual_drop_type(DROP_NONE)
#endif // DRAG_SUPPORT
	, thread(thread)
{
	if (type != POPUP)
	{
		data.dialog.message = NULL;
		data.dialog.default_value = NULL;
		data.dialog.return_value_string = NULL;
	}
	else
		data.popup.name = NULL;

	return_value.type = VALUE_UNDEFINED;
	thread->AddListener(this);
}

DOM_JSWCCallback::~DOM_JSWCCallback()
{
	if (type != POPUP)
	{
		OP_DELETEA(data.dialog.message);
		OP_DELETEA(data.dialog.default_value);
		OP_DELETEA(data.dialog.return_value_string);
	}
	else
		OP_DELETEA(data.popup.name);
}

void
DOM_JSWCCallback::GCTrace()
{
	if (type == POPUP)
	{
		GCMark(data.popup.opener);
		GCMark(data.popup.fakewindowproxy);
	}
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	GCMark(GetRuntime(), dialog_arguments);
#endif // DOM_SHOWMODALDIALOG_SUPPORT
}

OP_STATUS
DOM_JSWCCallback::Signal(ES_Thread *, ES_ThreadSignal signal)
{
	switch (signal)
	{
	case ES_SIGNAL_FINISHED:
	case ES_SIGNAL_FAILED:
	case ES_SIGNAL_CANCELLED:
		ES_ThreadListener::Remove();
		thread = NULL;
	}

	return OpStatus::OK;
}

void
DOM_JSWCCallback::OnAlertDismissed()
{
	Restart(FALSE);
}

void
DOM_JSWCCallback::OnConfirmDismissed(BOOL ok)
{
	return_value.type = VALUE_BOOLEAN;
	return_value.value.boolean = !!ok;

	Restart(FALSE);
}

void
DOM_JSWCCallback::OnPromptDismissed(BOOL ok, const uni_char *value)
{
	if (ok && OpStatus::IsSuccess(UniSetStr(data.dialog.return_value_string, value)))
	{
		return_value.type = VALUE_STRING;
		return_value.value.string = data.dialog.return_value_string;
	}
	else
		return_value.type = VALUE_NULL;

	Restart(FALSE);
}

void
DOM_JSWCCallback::AbortScript()
{
	Restart(TRUE);
}

#ifdef DOM_SHOWMODALDIALOG_SUPPORT

/* virtual */ OP_STATUS
DOM_JSWCCallback::OnClose(const uni_char *result)
{
	OP_STATUS status = UniSetStr(data.dialog.return_value_string, result ? result : UNI_L(""));
	if (OpStatus::IsSuccess(status))
	{
		DOMSetString(&return_value, data.dialog.return_value_string);
	}
	Restart(FALSE);

	return status;
}

#endif // DOM_SHOWMODALDIALOG_SUPPORT

void
DOM_JSWCCallback::Continue(Action action, OpDocumentListener::JSPopupOpener **opener)
{
	if (opener)
		*opener = NULL;

	if (action == POPUP_ACTION_ACCEPT)
		data.popup.refuse = FALSE;
	else if (action == POPUP_ACTION_REFUSE)
		data.popup.refuse = TRUE;

	if (opener && data.popup.refuse)
	{
		*opener = this;
		data.popup.opener = GetEnvironment()->GetWindow();
		data.popup.is_delayed = TRUE;
	}

	Restart(FALSE);
}

/* virtual */ void
DOM_JSWCCallback::Open()
{
	URL tmpurl = url;
	if (data.popup.has_data)
		tmpurl = URL();

	// This call might (through OpenURL) cause a GC so we better still be locked in memory
	Window *window;
	OpRect rect(data.popup.left, data.popup.top, data.popup.width, data.popup.height);
	OP_STATUS status = JS_Window::OpenPopupWindow(window, data.popup.opener, tmpurl, refurl, data.popup.name, &rect, data.popup.scrollbars, data.popup.location, data.popup.open_in_background, TRUE, FALSE, TRUE);

	if (data.popup.has_data && OpStatus::IsSuccess(status))
	{
		ES_Value value;

		DocumentManager *docman = window->DocManager();
		if (OpStatus::IsSuccess(DOM_GetWindowObject(&value, docman, GetRuntime())))
		{
			if (data.popup.fakewindowproxy && value.type == VALUE_OBJECT)
			{
				DOM_Object *hostobject = (DOM_Object *) ES_Runtime::GetHostObject(value.value.object);

				static_cast<DOM_ProxyObject *>(data.popup.fakewindowproxy)->SetObject(hostobject);
			}

			OpStatus::Ignore(JS_Window::WriteDocumentData(window, documentdata.GetStorage()));
		}
	}

	OpStatus::Ignore(status);

	GetRuntime()->Unprotect(*this);
}

/* virtual */ void
DOM_JSWCCallback::Release()
{
	GetRuntime()->Unprotect(*this);
}

void
DOM_JSWCCallback::Restart(BOOL abort)
{
	called = TRUE;

	if (type != POPUP || !data.popup.is_delayed)
		GetRuntime()->Unprotect(*this);

#ifdef DRAG_SUPPORT
	// A dialog during d'n'd event was dismissed. If we're still dragging unblock d'n'd.
	if (g_drag_manager->IsDragging() && g_drag_manager->IsBlocked() && DOM_DataTransfer::IsDataStoreValid(thread))
	{
		OpDragObject* object = g_drag_manager->GetDragObject();
		object->SetDropType(drop_type);
		object->SetVisualDropType(visual_drop_type);
		g_drag_manager->Unblock();
	}
#endif // DRAG_SUPPORT

	if (thread)
		if (abort)
		{
			if (thread->Type() == ES_THREAD_TIMEOUT)
				static_cast<ES_TimeoutThread *>(thread)->StopRepeating();

			OpStatus::Ignore(thread->GetScheduler()->CancelThread(thread));
		}
		else if (thread->GetBlockType() == ES_BLOCK_USER_INTERACTION)
			OpStatus::Ignore(thread->Unblock(ES_BLOCK_USER_INTERACTION));
}

static int
StartDialog(JS_Window *this_window, DOM_JSWCCallback::Type type, ES_Value *argv, int argc, ES_Value *return_value, ES_Runtime *origining_runtime)
{
	OP_ASSERT(this_window);

	FramesDocument *document = origining_runtime->GetFramesDocument();
	FramesDocument *this_document = this_window->GetFramesDocument();
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	AsyncHTMLDialogData data;

	if (type == DOM_JSWCCallback::MODALHTML)
	{
		DOM_CHECK_ARGUMENTS("s");

		data.html_template = g_url_api->GetURL(document->GetURL(), argv[0].value.string, FALSE);

		if (!this_window->OriginCheck(data.html_template, origining_runtime))
			return ES_EXCEPT_SECURITY;
	}
#endif // DOM_SHOWMODALDIALOG_SUPPORT

	/* This is the default, ends up being used for any script for
	   which we can't figure out a domain.  The idea is that it looks
	   suspicious enough to alert the user. */
	const uni_char *hostname = UNI_L("about:blank");
	OpString hostname0;

# ifdef EXTENSION_SUPPORT
	if (!document && this_document && static_cast<DOM_Runtime *>(origining_runtime)->HasSharedEnvironment())
		document = this_document;
# endif // EXTENSION_SUPPORT

# if defined(_DEBUG) && defined(DOM_WEBWORKERS_SUPPORT)
	if (!document && this_document)
		document = this_document;
	else if (!document && this_window->GetEnvironment()->GetWorkerController() && this_window->GetEnvironment()->GetWorkerController()->GetWorkerDocManager())
	{
		document = this_window->GetEnvironment()->GetWorkerController()->GetWorkerDocManager()->GetCurrentDoc();
		this_document = document;
	}
# endif // _DEBUG && DOM_WEBWORKERS_SUPPORT

	URL url = document->GetURL();
	if (ServerName *sn = url.GetServerName())
	{
		const uni_char *snname = sn->UniName();
		if (snname && *snname)
			CALL_FAILED_IF_ERROR(hostname0.Set(snname));
		hostname = hostname0.CStr();
	}
	else if (url.Type() == URL_OPERA && url.GetAttribute(URL::KName).CompareI("opera:config") == 0)
		hostname = UNI_L("opera:config");

	const uni_char *message_tmp = UNI_L("");
	const uni_char *default_value_tmp = UNI_L("");

	if (RefuseDialog(origining_runtime))
	{
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
		if (type == DOM_JSWCCallback::MODALHTML)
		{
			// From the spec:
			// f the user agent is configured such that this invocation
			// of showModalDialog() is somehow disabled, then the method
			// returns the empty string
			DOM_Object::DOMSetString(return_value);
			return ES_VALUE;
		}
#endif // DOM_SHOWMODALDIALOG_SUPPORT
		// else (prompt, alert, confirm)
		return ES_FAILED;
	}

	if (argc >= 1 && argv[0].type == VALUE_STRING)
		message_tmp = argv[0].value.string;

	if (type == DOM_JSWCCallback::PROMPT && argc >= 2 && argv[1].type == VALUE_STRING)
		default_value_tmp = argv[1].value.string;

	/* The thread calling alert() */
	ES_Thread *thread = DOM_Object::GetCurrentThread(origining_runtime);

	DOM_JSWCCallback *callback;

	CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(callback = OP_NEW(DOM_JSWCCallback, (thread, type)), (DOM_Runtime *) origining_runtime));

	if (!origining_runtime->Protect(*callback))
		return ES_NO_MEMORY;

#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	if (type == DOM_JSWCCallback::MODALHTML)
	{
		callback->dialog_arguments = argv[1];

		data.modal = TRUE;
		data.width = 360;
		data.height = 320;
		data.callback = callback;

		if (OpStatus::IsMemoryError(g_html_dialog_manager->OpenAsyncDialog(this_document->GetWindow(), &data)))
		{
			callback->GetRuntime()->Unprotect(*callback);
			return ES_NO_MEMORY;
		}
	}
	else
#endif // DOM_SHOWMODALDIALOG_SUPPORT
	{
		if (OpStatus::IsError(UniSetStr(callback->data.dialog.message, message_tmp)) ||
			OpStatus::IsError(UniSetStr(callback->data.dialog.default_value, default_value_tmp)))
		{
			callback->GetRuntime()->Unprotect(*callback);
			return ES_NO_MEMORY;
		}

		// Show the alert in the window object's window unless it has
		// been replaced in which case we show it in the origining
		// thread's window instead. All to be certain it's displayed
		// in a window where the caller has a document that is shown.
		Window* target_win = this_document && this_document->IsCurrentDoc() ? this_document->GetWindow() : document->GetWindow();
		WindowCommander *commander = target_win->GetWindowCommander();
		OpDocumentListener *listener = commander->GetDocumentListener();

		if (this_document)
			this_document->GetVisualDevice()->JSDialogShown();

		if (type == DOM_JSWCCallback::ALERT)
			listener->OnJSAlert(commander, hostname, callback->data.dialog.message, callback);
		else if (type == DOM_JSWCCallback::CONFIRM)
			listener->OnJSConfirm(commander, hostname, callback->data.dialog.message, callback);
		else
			listener->OnJSPrompt(commander, hostname, callback->data.dialog.message, callback->data.dialog.default_value, callback);

#ifdef DRAG_SUPPORT
		// If d'n'd event handler shows any js dialog (blocks the thread), block d'n'd.
		if (g_drag_manager->IsDragging() && DOM_DataTransfer::IsDataStoreValid(origining_runtime))
		{
			OpDragObject* object = g_drag_manager->GetDragObject();
			callback->drop_type = object->GetDropType();
			callback->visual_drop_type = object->GetVisualDropType();
			object->SetDropType(DROP_NONE);
			object->SetVisualDropType(DROP_NONE);
			g_drag_manager->Block();
		}
#endif // DRAG_SUPPORT
	}

	if (!callback->called)
	{
		thread->Block(ES_BLOCK_USER_INTERACTION);

		return_value->type = VALUE_OBJECT;
		return_value->value.object = *callback;
		return ES_SUSPEND | ES_RESTART;
	}
	else
	{
		*return_value = callback->return_value;
		return ES_VALUE;
	}
}

static int
ResumeDialog(ES_Value* return_value)
{
	DOM_JSWCCallback *callback = DOM_VALUE2OBJECT(*return_value, DOM_JSWCCallback);

	*return_value = callback->return_value;

	return ES_VALUE;
}

#ifdef DOM_SHOWMODALDIALOG_SUPPORT
static ES_Value* GetDialogArguments(HTML_Dialog* dialog)
{
	HTML_AsyncDialog* async_dialog = static_cast<HTML_AsyncDialog*>(dialog);
	AsyncHTMLDialogCallback* async_callback = async_dialog->GetCallback();
	if (async_callback->GetCallbackType() == AsyncHTMLDialogCallback::HTML_DIALOG_TYPE_JS)
	{
		DOM_JSWCCallback* callback = static_cast<DOM_JSWCCallback*>(async_callback);
		return &callback->dialog_arguments;
	}
	else
		return NULL;
}
#endif // DOM_SHOWMODALDIALOG_SUPPORT


static BOOL3 IsSecureToLoadInto(FramesDocument* frames_doc, JS_Window* this_window, DOM_Runtime* origining_runtime)
{
	BOOL3 secure;
	if (frames_doc->GetESRuntime())
		secure = this_window->OriginLoadCheck(frames_doc->GetESRuntime(), origining_runtime);
	else
	{
		URL url = frames_doc->GetURL();
		int security = url.GetAttribute(URL::KSecurityStatus);

		if (origining_runtime->GetFramesDocument() && this_window->OriginCheck(url, origining_runtime->GetFramesDocument()->GetURL()))
			secure = YES;
		else
			secure = (security == SECURITY_STATE_NONE || security == SECURITY_STATE_UNKNOWN) ? MAYBE : NO;
	}

	return secure;
}

static BOOL
DOM_CheckFrameSpoofing(JS_Window *object, DocumentManager *docman, DOM_Runtime *origining_runtime, const uni_char *url)
{
#ifdef GADGET_SUPPORT
	if (docman->GetWindow()->GetGadget() || origining_runtime->GetFramesDocument() && origining_runtime->GetFramesDocument()->GetWindow()->GetGadget())
		return FALSE;
#endif // GADGET_SUPPORT

	// Allow reuse of windows opened by a script in the same window as the current script unless
	// the target window has loaded a secure page.
	if (origining_runtime->GetFramesDocument() && docman->GetWindow()->GetOpenerSecurityContext() == origining_runtime->GetFramesDocument()->GetSecurityContext())
	{
		if (FramesDocument* existing_doc = docman->GetCurrentDoc())
		{
			BOOL3 secure = IsSecureToLoadInto(existing_doc, object, origining_runtime);
			if (secure == NO)
				return FALSE;
		}
		return TRUE;
	}

	if (FramesDocument *doc = docman->GetCurrentDoc())
	{
		const uni_char *target_domain, *script_domain;
		URLType target_type, script_type;
		int target_port, script_port;

		if (DOM_EnvironmentImpl *target_environment = (DOM_EnvironmentImpl *) doc->GetDOMEnvironment())
			target_environment->GetDOMRuntime()->GetDomain(&target_domain, &target_type, &target_port);
		else
		{
			ServerName *target_sn = doc->GetURL().GetServerName();
			target_domain = target_sn ? target_sn->UniName() : UNI_L("");
			target_type = doc->GetURL().Type();
			target_port = doc->GetURL().GetServerPort();
		}

		origining_runtime->GetDomain(&script_domain, &script_type, &script_port);

		if (!object->OriginCheck(target_type, target_port, target_domain, script_type, script_port, script_domain))
		{
			URL newurl;

			if (url && *url)
				newurl = GetEncodedURL(origining_runtime->GetFramesDocument(), NULL, url);

			ServerName *newurl_sn = newurl.GetServerName();
			script_domain = newurl_sn ? newurl_sn->UniName() : UNI_L("");
			script_type = newurl.Type();
			script_port = newurl.GetServerPort();

			return object->OriginCheck(target_type, target_port, target_domain, script_type, script_port, script_domain);
		}
		else
			return TRUE;
	}

	return FALSE;
}

/* static */ int
JS_Window::open(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(this_window, DOM_TYPE_WINDOW, JS_Window);

	DOM_JSWCCallback *callback;

	const uni_char *url = NULL, *windowName = NULL;

	/* Height, left and top, width; same order as in the 'keys' array below. */
	int dimensions[4] = { 0, -1, -1, 0 }, sub_win_id = -1;

	/* Location and scrollbars; same order as in the 'keys' array below. */
	BOOL3 features[2] = { NO, NO };
	BOOL unrequested = FALSE, refuse = FALSE, replace = FALSE, open_in_background = FALSE;

	FramesDocument *frames_doc = NULL;
	DocumentManager *new_doc_man = NULL;

	URL ref_url, win_url;

	if (argc >= 0)
	{
		/* Use preferences specific to the script's URL rather than
		   the Window's.  Probably doesn't matter much, since a script
		   from a different domain wouldn't be allowed to call this
		   function. */
		ServerName *hostname = DOM_EnvironmentImpl::GetHostName(origining_runtime->GetFramesDocument());
		BOOL allow_hidden_location = g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::AllowScriptToHideURL, hostname);
		ES_Thread *thread = GetCurrentThread(origining_runtime);
		FramesDocument *document = origining_runtime->GetFramesDocument();

		unrequested = IsUnrequestedPopup(thread);
		refuse = RefusePopup(document, thread, unrequested);
		open_in_background = OpenInBackground(document, thread);

		frames_doc = this_window->GetFramesDocument();

		if (!frames_doc)
			return ES_FAILED;

		url = argc >= 1 && argv[0].type == VALUE_STRING ? argv[0].value.string : NULL;
		windowName = argc >= 2 && argv[1].type == VALUE_STRING ? argv[1].value.string : NULL;
		replace = argc >= 4 && argv[3].type == VALUE_BOOLEAN && argv[3].value.boolean;

		if (!url || !*url)
			url = UNI_L("about:blank");

		if (windowName && *windowName)
		{
			const uni_char *windowName_tmp = windowName;

			frames_doc->FindTarget(windowName_tmp, sub_win_id);

			if (!windowName_tmp && sub_win_id != -2)
			{
				new_doc_man = frames_doc->GetDocManager()->GetWindow()->GetDocManagerById(sub_win_id);

				FramesDocument *fd = NULL;
				for (fd = new_doc_man->GetCurrentDoc(); fd; fd = fd->GetDocManager()->GetParentDoc())
					if (fd->GetDocManager() == document->GetDocManager())
						break;

				if (!fd)
					// Not ancestor so check origin.
					if (!DOM_CheckFrameSpoofing(this_window, new_doc_man, origining_runtime, url))
						new_doc_man = NULL;
			}
			else
			{
				Window* window = NULL;

				if (sub_win_id > -2)
					for (window = g_windowManager->FirstWindow(); window; window = window->Suc())
						if (window->Name() && uni_strcmp(window->Name(), windowName) == 0)
							if (DOM_CheckFrameSpoofing(this_window, window->DocManager(), origining_runtime, url))
								break;

				if (window)
					new_doc_man = window->DocManager();
			}
		}

		if (frames_doc->GetOrigin()->in_sandbox)
		{
			// In a sandbox. Refuse everything that ventures into the unknown.
			if (!new_doc_man || !new_doc_man->GetCurrentDoc() ||
				!(frames_doc->GetOrigin()->security_context == new_doc_man->GetCurrentDoc()->GetOrigin()->security_context))
			{
				return_value->type = VALUE_NULL;
				return ES_VALUE;
			}
		}


		if (!new_doc_man)
		{
			// An empty third argument is the same as no third argument at all
			if (argc > 2 && argv[2].type == VALUE_STRING && *argv[2].value.string)
			{
				KeywordIndex keys[6];
				keys[0].keyword = "height";
				keys[1].keyword = "left";
				keys[2].keyword = "location"; // if not mentioned defaults to NO
				keys[3].keyword = "scrollbars"; // if not mentioned defaults to NO
				keys[4].keyword = "top";
				keys[5].keyword = "width";

				ParameterList parameterlist(keys, sizeof keys / sizeof keys[0]);
				int index;

				char *options = uni_down_strdup(argv[2].value.string);
				if (!options)
					return ES_NO_MEMORY;

				OP_STATUS status = parameterlist.SetValue(options, PARAM_SEP_COMMA | PARAM_SEP_WHITESPACE | PARAM_ONLY_SEP);
				op_free(options);
				CALL_FAILED_IF_ERROR(status);

				for (index = 0; index < 4; ++index)
					if (Parameters* parameters = parameterlist.GetParameter(keys[index + (index > 1 ? 2 : 0)].keyword, PARAMETER_ANY))
					{
						const char *value = parameters->Value();
						if (value)
							dimensions[index] = op_atoi(value);
					}

				for (index = 0; index < 2; ++index)
					if (Parameters* parameters = parameterlist.GetParameter(keys[index + 2].keyword, PARAMETER_ANY))
					{
						const char *value = parameters->Value();
						features[index] = !value || !*value || op_stricmp(value, "yes") == 0 || op_strcmp(value, "1") == 0 ? YES : NO;
					}
			}
			else
			{
				features[0] = MAYBE;
				features[1] = MAYBE;
			}

			if (!allow_hidden_location && features[0] == NO)
				features[0] = MAYBE;

			ES_Thread *thread = GetCurrentThread(origining_runtime);
			WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
			OpDocumentListener *listener = wc->GetDocumentListener();

			callback = OP_NEW(DOM_JSWCCallback, (thread, DOM_JSWCCallback::POPUP));

			if (!callback ||
			    OpStatus::IsMemoryError(DOMSetObjectRuntime(callback, this_window->GetRuntime())) ||
			    !this_window->GetRuntime()->Protect(*callback))
			{
				OP_DELETE(callback);
				return ES_NO_MEMORY;
			}

			if (url && *url)
				win_url = GetEncodedURL(origining_runtime->GetFramesDocument(), frames_doc, url);

			callback->url = win_url;
			callback->refurl = DocumentReferrer(frames_doc);

			CALL_FAILED_IF_ERROR(UniSetStr(callback->data.popup.name, windowName));

			callback->data.popup.opener = this_window;
			callback->data.popup.fakewindowproxy = NULL;
			callback->data.popup.left = dimensions[1];
			callback->data.popup.top = dimensions[2];
			callback->data.popup.width = dimensions[3];
			callback->data.popup.height = dimensions[0];
			callback->data.popup.scrollbars = features[1];
			callback->data.popup.location = features[0];
			callback->data.popup.open_in_background = open_in_background ? YES : NO;
			callback->data.popup.refuse = !!refuse;
			callback->data.popup.replace = !!replace;
			callback->data.popup.is_delayed = FALSE;
			callback->data.popup.open = FALSE;
			callback->data.popup.has_data = FALSE;

			thread->SetHasOpenedNewWindow();

			listener->OnJSPopup(wc, callback->url.GetAttribute(URL::KUniName_Username_Password_Hidden).CStr(), callback->data.popup.name, dimensions[1], dimensions[2], dimensions[3], dimensions[0], features[1], features[0], refuse, unrequested, callback);

			if (!callback->called)
			{
				thread->Block(ES_BLOCK_USER_INTERACTION);

				return_value->type = VALUE_OBJECT;
				return_value->value.object = *callback;
				return ES_SUSPEND | ES_RESTART;
			}
			else if (callback->data.popup.is_delayed)
				goto return_fake_window;
			else
			{
				refuse = callback->data.popup.refuse;
				goto create_window;
			}
		}
	}
	else
	{
		callback = DOM_VALUE2OBJECT(*return_value, DOM_JSWCCallback);

		if (callback->data.popup.is_delayed)
		{
		return_fake_window:
			/* Create fake window object. */
			JS_FakeWindow *fakewindow;

			CALL_FAILED_IF_ERROR(JS_FakeWindow::Make(fakewindow, callback));

			DOM_ProxyObject *fakewindowproxy;

			CALL_FAILED_IF_ERROR(DOM_WindowProxyObject::Make(fakewindowproxy, this_window->GetRuntime(), NULL));

			fakewindowproxy->SetObject(fakewindow);
			callback->data.popup.fakewindowproxy = fakewindowproxy;

			DOMSetObject(return_value, fakewindowproxy);
			return ES_VALUE;
		}

		frames_doc = this_window->GetFramesDocument();

		if (!frames_doc)
			return ES_FAILED;

		win_url = callback->url;
		windowName = callback->data.popup.name;
		dimensions[1] = callback->data.popup.left;
		dimensions[2] = callback->data.popup.top;
		dimensions[3] = callback->data.popup.width;
		dimensions[0] = callback->data.popup.height;
		features[1] = callback->data.popup.scrollbars;
		features[0] = callback->data.popup.location;
		open_in_background = callback->data.popup.open_in_background != NO;
		refuse = callback->data.popup.refuse;
		replace = callback->data.popup.replace;

	create_window:

		if (refuse)
		{
			/* This popup window should be refused. */

			return_value->type = VALUE_NULL;
			return ES_VALUE;
		}

		if (url && *url)
			win_url = GetEncodedURL(origining_runtime->GetFramesDocument(), frames_doc, url);

#ifdef GADGET_SUPPORT
		if (origining_runtime->GetFramesDocument() && origining_runtime->GetFramesDocument()->GetWindow()->GetGadget())
		{
			OpGadget *gadget = origining_runtime->GetFramesDocument()->GetWindow()->GetGadget();
			if (gadget->IsExtension() && win_url.Type() != URL_WIDGET)
			{
				/* Hackish, but windows that pop out of extensions and are not internal should share
				 * the main browsing context so as not to mix "identical" looking tabs using
				 * different contexts and confuse the user. Hardcoding 0 as the context since it can't
				 * be a private/turbo/etc context since it was previously in the gadget's context. */
				if (win_url.GetContextId() == origining_runtime->GetFramesDocument()->GetWindow()->GetGadget()->UrlContextId())
					win_url = g_url_api->GetURL(win_url.GetAttribute(URL::KUniName_With_Fragment_Username_Password_NOT_FOR_UI).CStr(), static_cast<URL_CONTEXT_ID>(0));
			}
			else if (win_url.Type() != URL_HTTP && win_url.Type() != URL_HTTPS)
				return ES_FAILED;
		}
#endif // GADGET_SUPPORT

		Window *new_window;
		OpRect rect(dimensions[1], dimensions[2], dimensions[3], dimensions[0]);
		CALL_FAILED_IF_ERROR(OpenPopupWindow(new_window, this_window, win_url, frames_doc, windowName, &rect, features[1], features[0], open_in_background ? YES : NO, TRUE, FALSE, TRUE));

		DOM_Object *window;

		CALL_FAILED_IF_ERROR(new_window->DocManager()->ConstructDOMProxyEnvironment());
		CALL_FAILED_IF_ERROR(static_cast<DOM_ProxyEnvironmentImpl *>(new_window->DocManager()->GetDOMEnvironment())->GetProxyWindow(window, origining_runtime));

		DOMSetObject(return_value, window);
		return ES_VALUE;
	}

	if (url && *url && win_url.IsEmpty())
		win_url = GetEncodedURL(origining_runtime->GetFramesDocument(), frames_doc, url);

	if (FramesDocument *new_frames_doc = new_doc_man->GetCurrentDoc())
	{
		CALL_FAILED_IF_ERROR(new_frames_doc->ConstructDOMEnvironment());

		BOOL3 secure = IsSecureToLoadInto(new_frames_doc, this_window, origining_runtime);
		if (secure == NO || (secure == MAYBE && win_url.Type() == URL_JAVASCRIPT))
			return ES_EXCEPT_SECURITY;

		ES_Thread *thread = GetCurrentThread(origining_runtime);
		if (!uni_str_eq(url, UNI_L("about:blank")))
		{
			DocumentManager::OpenURLOptions options;
			options.user_initiated = FALSE;
			options.entered_by_user = NotEnteredByUser;
			options.is_walking_in_history = FALSE;
			options.origin_thread = thread;
			if (OpStatus::IsMemoryError(new_frames_doc->ESOpenURL(win_url, frames_doc, TRUE, FALSE, replace, options)))
				return ES_NO_MEMORY;
		}
		else if (OpStatus::IsMemoryError(FramesDocument::CheckOnLoad(new_frames_doc, NULL)))
			return ES_NO_MEMORY;
	}
	else
	{
		DocumentManager::OpenURLOptions options;

		new_doc_man->SetReplace(replace);
		new_doc_man->OpenURL(win_url, frames_doc, TRUE, FALSE, options);
	}

	DOM_Object *window;

	CALL_FAILED_IF_ERROR(new_doc_man->ConstructDOMProxyEnvironment());
	CALL_FAILED_IF_ERROR(static_cast<DOM_ProxyEnvironmentImpl *>(new_doc_man->GetDOMEnvironment())->GetProxyWindow(window, origining_runtime));

	DOM_Object::DOMSetObject(return_value, window);
	return ES_VALUE;
}

/* static */ int
JS_Window::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	JS_Window *window;

	if (argc >= 0)
	{
		DOM_THIS_OBJECT_EXISTING(window, DOM_TYPE_WINDOW, JS_Window);

		if (!IsAllowedToNavigate(window, origining_runtime))
			return ES_EXCEPT_SECURITY;

		BOOL other_window = FALSE;

		if (FramesDocument *frames_doc = window->GetFramesDocument())
		{
			DocumentManager *doc_man = frames_doc->GetDocManager();
			Window *win = doc_man->GetWindow();

			/* A window is close()able if opened by a script. Additionally, align
			   with others in allowing a window opened by a user action but
			   without a history to be closed. This latter part is not currently
			   covered by the specification text for window.close(). */
			if (!doc_man->GetParentDoc() && win->CanClose() &&
			    (win->CanBeClosedByScript() ||
			     doc_man->GetWindow()->GetOpener() != NULL && doc_man->GetHistoryLen() <= 1))
			{
				other_window = origining_runtime->GetFramesDocument() && doc_man->GetWindow() != origining_runtime->GetFramesDocument()->GetWindow();

				/* If we're currently loading in this document manager, our
				   terminating action might get aborted if that load receives
				   its MSG_HEADER_LOADED before the terminating action is
				   performed.  To avoid that, just stop the loading instead.

				   This is a bit of a hack, but neither prematurely aborted
				   loading in a window we're about to close or accidentally
				   aborted terminating action are very serious problems in
				   practice. */
				if (doc_man->GetLoadStatus() != NOT_LOADING && doc_man->GetLoadStatus() != DOC_CREATED)
					doc_man->StopLoading(FALSE);

				ES_Thread *interrupt_thread;

				if (other_window)
					interrupt_thread = GetCurrentThread(origining_runtime);
				else
					interrupt_thread = NULL;

				ES_WindowCloseAction *action = OP_NEW(ES_WindowCloseAction, (doc_man->GetWindow()));

				/* AddTerminatingAction might return OpStatus::ERR, if a terminating action that
				   cannot be overridden (like this one) has already been added.  I don't believe
				   there is any point to signal that in any way. */
				if (!action || OpStatus::IsMemoryError(window->GetEnvironment()->GetScheduler()->AddTerminatingAction(action, interrupt_thread)))
					return ES_NO_MEMORY;

#ifdef SELFTEST
				/* Signal to the selftest system that the corresponding
				   Window is about to close and shouldn't be reused. */
				doc_man->GetWindow()->SetIsAboutToClose(TRUE);
#endif // SELFTEST
			}
		}

		if (!other_window)
			return ES_FAILED;
	}
	else
		window = DOM_VALUE2OBJECT(*return_value, JS_Window);

	if (window->GetFramesDocument())
	{
		DOMSetObject(return_value, window);
		return ES_SUSPEND | ES_RESTART;
	}
	else
		return ES_FAILED;
}

/* static */ int
JS_Window::print(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
#ifdef _PRINT_SUPPORT_
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);

	if (FramesDocument *frames_doc = window->GetFramesDocument())
		if (frames_doc->IsLoaded(TRUE))
		{
#if defined QUICK && defined _KIOSK_MANAGER_
			if (KioskManager::GetInstance()->GetNoPrint())
				return ES_FAILED;
#endif // QUICK && _KIOSK_MANAGER_

#ifdef MSWIN
			cdPrint(frames_doc->GetWindow());		// No reasonable way to detect OOM in this interface
#elif defined GENERIC_PRINTING
			WindowCommander *wc = frames_doc->GetWindow()->GetWindowCommander();
			if (wc->GetPrintingListener())
				wc->GetPrintingListener()->OnStartPrinting(wc);
#elif defined(KYOCERA)
			// no print support
#else
			OP_ASSERT(!"window.print support missing");
#endif
		}
#endif // _PRINT_SUPPORT_

	return ES_FAILED;
}

/* static */ int
JS_Window::stop(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);

	if (HLDocProfile *hld_profile = window->GetHLDocProfile())
	{
		ES_Thread *thread = GetCurrentThread(origining_runtime);
		hld_profile->GetESLoadManager()->StopLoading(thread);
	}

	return ES_FAILED;
}

/* static */ int
JS_Window::getComputedStyle(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);
	DOM_CHECK_ARGUMENTS("o");
	DOM_ARGUMENT_OBJECT(element, 0, DOM_TYPE_ELEMENT, DOM_Element);

	if (!window->OriginCheck(origining_runtime) || !element->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOM_EnvironmentImpl *environment = element->GetEnvironment();
	if (environment->GetDocument() != element->GetOwnerDocument())
		return ES_FAILED;

	const uni_char *pseudo_class;
	if (argc >= 2 && argv[1].type == VALUE_STRING)
		pseudo_class = argv[1].value.string;
	else
		pseudo_class = NULL;

	DOM_CSSStyleDeclaration *style;

	CALL_FAILED_IF_ERROR(DOM_CSSStyleDeclaration::Make(style, element, DOM_CSSStyleDeclaration::DOM_ST_COMPUTED, pseudo_class));

	DOM_Object::DOMSetObject(return_value, style);
	return ES_VALUE;
}

#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
/* static */ int
JS_Window::onSearch(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("b");

	FramesDocument* frames_doc = origining_runtime->GetFramesDocument();
	if (frames_doc)
	{
		URL url = frames_doc->GetWindow()->DocManager()->GetCurrentURL();
		// Only internal opera pages can call this function.
		if (url.GetAttribute(URL::KIsGeneratedByOpera))
		{
			WindowCommander* wc = frames_doc->GetWindow()->GetWindowCommander();
			wc->GetDocumentListener()->OnSearchSuggestionsUsed(wc, argv[0].value.boolean);
		}
	}
	return ES_FAILED;
}

/* static */ int
JS_Window::getSearchSuggest(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);

	DOM_CHECK_ARGUMENTS("s");

	FramesDocument* frames_doc = origining_runtime->GetFramesDocument();
	if (frames_doc)
	{
		URL url = frames_doc->GetWindow()->DocManager()->GetCurrentURL();
		// Only internal opera pages can call this function. Prevent random pages on the web starting search suggestion requests
		if (url.GetAttribute(URL::KIsGeneratedByOpera))
		{
			if (!window->m_search_suggestions_callback)
				window->m_search_suggestions_callback = OP_NEW(SearchSuggestionsCallback,(window));
			if (window->m_search_suggestions_callback)
			{
				const uni_char* url_string = argv[0].value.string;
				WindowCommander* wc = frames_doc->GetWindow()->GetWindowCommander();

				window->m_search_suggestions_callback->SetID(url.Id(TRUE));
				wc->GetLoadingListener()->OnSearchSuggestionsRequested(wc, url_string, window->m_search_suggestions_callback);
			}
		}
	}
	return ES_FAILED;
}
#endif

/* static */ int
JS_Window::clearIntervalOrTimeout(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_Object *window = NULL;
	DOM_THIS_OBJECT_EXISTING_OR(window, DOM_TYPE_WINDOW, JS_Window);
#ifdef DOM_WEBWORKERS_SUPPORT
	DOM_THIS_OBJECT_EXISTING_OR(window, DOM_TYPE_WEBWORKERS_WORKER, DOM_WebWorker);
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef EXTENSION_SUPPORT
	DOM_THIS_OBJECT_EXISTING_OR(window, DOM_TYPE_EXTENSION_SCOPE, DOM_ExtensionScope);
#endif // EXTENSION_SUPPORT
	DOM_THIS_OBJECT_CHECK(window);

	DOM_CHECK_ARGUMENTS_SILENT("N");

	if (argv[0].value.number >= 0)
	{
		ES_ThreadScheduler *scheduler = this_object->GetRuntime()->GetESScheduler();
		/* If the environment has been destroyed, the runtime's scheduler (and frames doc) is
		   cleared. Return silently. */
		if (!scheduler)
			return ES_FAILED;
		CALL_FAILED_IF_ERROR(scheduler->CancelTimeout(TruncateDoubleToUInt(argv[0].value.number)));
	}

	return ES_FAILED;
}

/* static */ int
JS_Window::home(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(js_window, DOM_TYPE_WINDOW, JS_Window);

	if (!js_window->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	if (FramesDocument *frames_doc = js_window->GetFramesDocument())
	{
		Window *window = frames_doc->GetWindow();
		OpStringC url = g_pccore->GetStringPref(PrefsCollectionCore::HomeURL);

		if (!url.IsEmpty())
			CALL_FAILED_IF_ERROR(window->OpenURL(url.CStr()));
	}

	return ES_FAILED;
}

#ifdef DATABASE_STORAGE_SUPPORT
/* static */ int
JS_Window::openDatabase(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);
	DOM_CHECK_ARGUMENTS("sssn");

	DOM_DbManager *db_manager = DOM_DbManager::LookupManagerForWindow(window);

	if (db_manager == NULL)
	{
		GET_FAILED_IF_ERROR(DOM_DbManager::Make(db_manager, window->GetRuntime()));
		OP_ASSERT(db_manager != NULL);
		GET_FAILED_IF_ERROR(window->PutPrivate(DOM_PRIVATE_database, *db_manager));
	}

	DOM_Database *database;
	OP_STATUS status = db_manager->FindOrCreateDb(database, argv[0].value.string,
			argv[1].value.string, argv[2].value.string, static_cast<OpFileLength>(argv[3].value.number));

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else if (PS_Status::ERR_MAX_DBS_PER_ORIGIN == status)
		return DOM_CALL_DOMEXCEPTION(QUOTA_EXCEEDED_ERR);
	else if (OpStatus::IsError(status))
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	DOMSetObject(return_value, database);
	return ES_VALUE;
}
#endif // DATABASE_STORAGE_SUPPORT

#ifdef DOM_SELECTION_SUPPORT
int
JS_Window::GetSelection(ES_Value* return_value)
{
	ES_GetState state = DOMSetPrivate(return_value, DOM_PRIVATE_selection);
	if (state == GET_FAILED)
	{
		DOM_EnvironmentImpl *environment = GetEnvironment();

		CALL_FAILED_IF_ERROR(environment->ConstructDocumentNode());

		DOM_WindowSelection *selection;
		CALL_FAILED_IF_ERROR(DOM_WindowSelection::Make(selection, static_cast<DOM_Document *>(environment->GetDocument())));
		CALL_FAILED_IF_ERROR(PutPrivate(DOM_PRIVATE_selection, *selection));

		DOMSetObject(return_value, selection);
	}

	return ES_VALUE;
}

/* static */ int
JS_Window::getSelection(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);

	return window->GetSelection(return_value);
}

#endif // DOM_SELECTION_SUPPORT

#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
/* static */ int
JS_Window::postMessage(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);
	DOM_CHECK_ARGUMENTS("-s");

	/* postMessage(data, targetOrigin [, ports]):
	 *   - data: Any cloneable value.
	 *   - targetOrigin: The stated origin of the target. Either
	 *     passed as '*' to match the window's origin or
	 *     an origin value that matches that of the target window.
	 *   - ports: Optional array of message ports.
	 */
	const uni_char *target_origin = argv[1].value.string;

	ES_Object *ports_array = NULL;
	if (argc > 2 && argv[2].type == VALUE_OBJECT)
		ports_array = argv[2].value.object;

	DOM_EnvironmentImpl *environment = origining_runtime->GetEnvironment();
	DOM_Object *origining_window = environment->GetWindow();

	BOOL fail_same_origin_test = FALSE;

	if (!uni_str_eq(target_origin, "*"))
	{
		URL current_url = window->GetRuntime()->GetOriginURL();
		URL source_origin_url = current_url;
		URL target_origin_url = current_url;
		if (!uni_str_eq(target_origin, "/"))
			target_origin_url = g_url_api->GetURL(target_origin);
		else
			source_origin_url = origining_runtime->GetOriginURL();

		if (target_origin_url.Type() == URL_NULL_TYPE || target_origin_url.Type() == URL_UNKNOWN || DOM_Utils::IsOperaIllegalURL(target_origin_url))
			return origining_window->CallDOMException(DOM_Object::SYNTAX_ERR, return_value);
		else
		{
			/* Section 3.3, step 1 (http://dev.w3.org/html5/postmsg/)
			     If the value of the targetOrigin argument is neither a single U+002A ASTERISK character (*), a
			     single U+002F SOLIDUS character (/), nor an absolute URL with a <host-specific> component that
			     is either empty or a single U+002F SOLIDUS character (/), then throw a SYNTAX_ERR exception
			     and abort the overall set of steps.

			   window.postMessage(msg, targetOrigin) usage is already entrenched and either depend on passing
			   a host-specific component in the targetOrigin URL or do so by accident/redundantly. This
			   causes compat problems, both backwards and with respect to other implementations (cf. DSK-302997.)
			   Hence, opt to disable this check; for now. If spec compliance by others will eventually happen,
			   re-enable the check here. */
#ifdef DOM_CROSSDOCUMENT_MESSAGING_STRICT_HOSTSPECIFIC
			if (!uni_str_eq(target_origin, "/"))
			{
				OpString  host_specific;
				OpString8 str;

				TRAP_AND_RETURN(stat, target_origin_url.GetAttributeL(URL::KPathAndQuery_L, str));
				CALL_FAILED_IF_ERROR(host_specific.Append(str));
				TRAP_AND_RETURN(stat, target_origin_url.GetAttributeL(URL::KFragment_Name, str));
				CALL_FAILED_IF_ERROR(host_specific.Append(str));

				const uni_char *host_specific_cstr = host_specific.CStr();

				/* only "" or "/" permitted */
				if (host_specific_cstr && host_specific_cstr[0] && (host_specific_cstr[0] != '/' || host_specific_cstr[1]))
					return origining_window->CallDOMException(DOM_Object::SYNTAX_ERR, return_value);
			}
#endif // DOM_CROSSDOCUMENT_MESSAGING_STRICT_HOSTSPECIFIC

			/* The test for same origin happens after we've cloned the message
			   and ports per spec (Step 4); here we simply compute the outcome. */
			fail_same_origin_test = !OriginCheck(source_origin_url, target_origin_url);
		}
	}

	DOM_Runtime *runtime = window->GetRuntime();

	DOM_MessageEvent *event = OP_NEW(DOM_MessageEvent, ());
	if (OpStatus::IsError(DOM_Object::DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::CROSSDOCUMENT_MESSAGEEVENT_PROTOTYPE), "MessageEvent")))
		return ES_NO_MEMORY;

	ES_Value init_argv[9];
	ES_Value init_return_value;

	DOMSetString(&init_argv[0],  UNI_L("message"));
	DOMSetBoolean(&init_argv[1], FALSE);  // cannot bubble
	DOMSetBoolean(&init_argv[2], FALSE);  // isn't cancelable.
	init_argv[3] = argv[0];

	TempBuffer buffer;
	const uni_char *origin = NULL;
	DOM_Object *source_object = NULL;

	/* The .source and .origin of the MessageEvent resolve to the current
	   script's global object (and its origin), and not that of the
	   current thread. See the specification for window.postMessage(). */
	if (ES_Object *code_global_object = ES_Runtime::GetGlobalObjectInScope(GetCurrentThread(origining_runtime)->GetContext()))
	{
		DOM_HOSTOBJECT_SAFE(source_window_object, code_global_object, DOM_TYPE_WINDOW, DOM_Object);
		if (source_window_object)
		{
			DOM_Runtime *source_runtime = source_window_object->GetRuntime();
			RETURN_IF_ERROR(source_runtime->GetSerializedOrigin(buffer));
			origin = buffer.GetStorage() ? buffer.GetStorage() : source_runtime->GetDomain();
			RETURN_IF_ERROR(source_window_object->GetEnvironment()->GetProxyWindow(source_object, origining_runtime));
		}
	}
	if (!source_object)
	{
		RETURN_IF_ERROR(origining_runtime->GetSerializedOrigin(buffer));
		origin = buffer.GetStorage() ? buffer.GetStorage() : origining_runtime->GetDomain();
		source_object = origining_window;
	}

	DOMSetString(&init_argv[4], origin);
	DOMSetString(&init_argv[5], UNI_L("")); // lastEventId
	DOMSetObject(&init_argv[6], source_object);
	DOMSetObject(&init_argv[7], ports_array);
	DOMSetBoolean(&init_argv[8], FALSE); // FALSE => map NULL ports_array to empty array.

	int result = DOM_MessageEvent::initMessageEvent(event, init_argv, ARRAY_SIZE(init_argv), &init_return_value, window->GetRuntime());
	if (result != ES_FAILED && result != ES_VALUE)
	{
		*return_value = init_return_value;
		return result;
	}
	/* Step 4 of Section 8.2.3, if not same-origin for non-"*" targetOrigins; silently fail. */
	if (fail_same_origin_test)
		return ES_FAILED;

	CALL_FAILED_IF_ERROR(window->CreateBodyNodeIfNeeded());

	event->SetTarget(window);
	event->SetSynthetic();
	CALL_FAILED_IF_ERROR(window->GetEnvironment()->SendEvent(event));
	return ES_FAILED;
}
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT

#ifdef URL_UPLOAD_BASE64_SUPPORT
/* static */ int
JS_Window::base64_atob(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WINDOW);
	return DOM_Base64::atob(this_object, argv, argc, return_value, origining_runtime);
}

/* static */ int
JS_Window::base64_btoa(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WINDOW);
	return DOM_Base64::btoa(this_object, argv, argc, return_value, origining_runtime);
}

#endif // URL_UPLOAD_BASE64_SUPPORT

#ifdef DOM_SUPPORT_BLOB_URLS
/*static */ int
JS_Window::createObjectURL(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WINDOW);

	if (argc > 0 && argv[0].type == VALUE_OBJECT)
	{
		DOM_Object* arg = DOM_GetHostObject(argv[0].value.object);
		if (arg && arg->IsA(DOM_TYPE_BLOB))
		{
			DOM_Blob *blob = static_cast<DOM_Blob *>(arg);
			TempBuffer *buf = GetEmptyTempBuf();
			CALL_FAILED_IF_ERROR(this_object->GetEnvironment()->CreateObjectURL(blob, buf));
			DOMSetString(return_value, buf);
			return ES_VALUE;
		}
	}

	DOMSetNull(return_value);
	return ES_VALUE;
}

/*static */ int
JS_Window::revokeObjectURL(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_WINDOW);
	DOM_CHECK_ARGUMENTS_SILENT("s");
	this_object->GetEnvironment()->RevokeObjectURL(argv[0].value.string);
	return ES_FAILED;
}
#endif // DOM_SUPPORT_BLOB_URLS

/* static */ int
JS_Window::dialog(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);

#ifdef EXTENSION_SUPPORT
	if (FramesDocument *frames_doc = window->GetEnvironment()->GetFramesDocument())
		if (frames_doc->GetWindow()->GetType() == WIN_TYPE_GADGET && frames_doc->GetWindow()->GetGadget()->IsExtension())
			return ES_FAILED;
#endif // EXTENSION_SUPPORT

	if (argc >= 0)
		return StartDialog(window, (DOM_JSWCCallback::Type) data, argv, argc, return_value, origining_runtime);
	else
		return ResumeDialog(return_value);
}

/* static */ int
JS_Window::focusOrBlur(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);

	if (!IsAllowedToNavigate(window, origining_runtime))
		return ES_EXCEPT_SECURITY;

	if (FramesDocument *frames_doc = window->GetRuntime()->GetFramesDocument())
	{
		// Check if element is made unfocusable by setting style visibility:hidden or display:none.
		if (FramesDocElm *fdelm = frames_doc->GetDocManager()->GetFrame())
			if (HTML_Element *he = fdelm->GetHtmlElement())
				if (LayoutWorkplace *workplace = fdelm->GetParentFramesDoc()->GetLayoutWorkplace())
					if (!workplace->IsRendered(he))
						return ES_FAILED;

		if (data == 0)
		{
			frames_doc->GetWindow()->Raise();
			frames_doc->GetVisualDevice()->SetFocus(FOCUS_REASON_OTHER);
		}
		else
			frames_doc->GetWindow()->Lower();
	}

	return ES_FAILED;
}

class DOM_ContextAnchor
{
	ES_Context *context;

public:
	DOM_ContextAnchor(ES_Context *context) : context(context) {}
	~DOM_ContextAnchor() { if (context) ES_Runtime::DeleteContext(context); }
	void Release() { context = NULL; }
};

/* static */ int
JS_Window::setIntervalOrTimeout(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_Object *window = NULL;
	DOM_THIS_OBJECT_EXISTING_OR(window, DOM_TYPE_WINDOW, JS_Window);
#ifdef DOM_WEBWORKERS_SUPPORT
	DOM_THIS_OBJECT_EXISTING_OR(window, DOM_TYPE_WEBWORKERS_WORKER, DOM_WebWorker);
#endif // DOM_WEBWORKERS_SUPPORT
#ifdef EXTENSION_SUPPORT
	DOM_THIS_OBJECT_EXISTING_OR(window, DOM_TYPE_EXTENSION_SCOPE, DOM_ExtensionScope);
#endif // EXTENSION_SUPPORT

	DOM_THIS_OBJECT_CHECK(window);

	OP_ASSERT(data == 0 || data == 1);
	BOOL is_interval = data == 1;

	if (!this_object->OriginCheck(origining_runtime))
		return ES_EXCEPT_SECURITY;

	DOM_EnvironmentImpl *environment = this_object->GetEnvironment();
	DOM_Runtime *runtime = this_object->GetRuntime();
	ES_ThreadScheduler *scheduler = runtime->GetESScheduler();

	/* Code may run and the runtime of the object hasn't been detached. */
	if (environment->IsEnabled() && scheduler)
	{
		ES_Thread *origin_thread = GetCurrentThread(origining_runtime);

		if (argc >= 1)
		{
			const double length_limit = (is_interval ? DOM_INTERVAL_LENGTH_LIMIT : DOM_TIMEOUT_LENGTH_LIMIT);

			double timeout = length_limit; // Default in case we have a strange value, like negative, NaN or Infinity

			if (argc >= 2)
			{
				double timeout_requested = (argv[1].value.number > 0.0 &&
									 op_isfinite(argv[1].value.number))
					? argv[1].value.number : 0;

				if (timeout_requested > length_limit)
					timeout = timeout_requested;
			}

#ifdef SHORTEN_SHORT_DELAYS
			if (timeout > 2000.0)
				return ES_FAILED;
			else
				timeout = 1.0;

			if (is_interval)
				return ES_FAILED;
#endif // SHORTEN_SHORT_DELAYS

			ES_TimeoutTimerEvent *timer_event = OP_NEW(ES_TimeoutTimerEvent, (runtime, timeout, is_interval, origin_thread));
			if (!timer_event)
				return ES_NO_MEMORY;
			OpAutoPtr<ES_TimerEvent> timer_event_anchor(timer_event);

			if (argv[0].type == VALUE_STRING)
			{
				ES_ProgramText program_text;
				ES_Program *program;

				program_text.program_text = argv[0].value.string;
				program_text.program_text_length = uni_strlen(argv[0].value.string);

				ES_Runtime::CallerInformation call_info;

				CALL_FAILED_IF_ERROR(origining_runtime->GetCallerInformation(origin_thread->GetContext(), call_info));

				if (call_info.privilege_level > ES_Runtime::PRIV_LVL_UNTRUSTED)
					/* Don't reveal URL of User JS or Browser JS if called from
					   there. */
					call_info.url = NULL;

				ES_Runtime::CompileProgramOptions options;
				options.program_is_function = TRUE;
				options.script_url_string = call_info.url;
				options.script_type = SCRIPT_TYPE_TIMEOUT;
				options.when = is_interval ? UNI_L("in call to setInterval") : UNI_L("in call to setTimeout");
#ifdef ECMASCRIPT_DEBUGGER
				options.reformat_source = g_ecmaManager->GetWantReformatScript(runtime);
#endif // ECMASCRIPT_DEBUGGER

				runtime->GetESScheduler()->SetErrorHandlerInterruptThread(origin_thread);

				OP_STATUS status = runtime->CompileProgram(&program_text, 1, &program, options);

				runtime->GetESScheduler()->ResetErrorHandlerInterruptThread();

				CALL_FAILED_IF_ERROR(status);

				timer_event->SetProgram(program);
			}
			else if (argv[0].type == VALUE_OBJECT)
			{
				if (OpStatus::IsMemoryError(timer_event->SetCallable(argv[0].value.object, &argv[2], MAX(argc - 2, 0))))
					return ES_NO_MEMORY;
			}
			else
				return ES_FAILED;

			timer_event_anchor.release();
			OP_STATUS result = runtime->GetESScheduler()->GetTimerManager()->AddEvent(timer_event);
			CALL_FAILED_IF_ERROR(result);

			unsigned id = timer_event->GetId();

			DOM_Object::DOMSetNumber(return_value, id);
			return ES_VALUE;
		}
	}

	return ES_FAILED;
}

/* static */ int
JS_Window::moveOrResizeOrScroll(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(js_window, DOM_TYPE_WINDOW, JS_Window);

	if (!IsAllowedToNavigate(js_window, origining_runtime))
		return ES_EXCEPT_SECURITY;

	// Check for NaN, using "nn" for 2 args and "n" for 1 argument
	DOM_CHECK_ARGUMENTS_SILENT((argc >= 2 ? "nn" : (argc ? "n" : "")));

	if (FramesDocument *frames_doc = js_window->GetFramesDocument())
	{
		Window *window = frames_doc->GetWindow();
		int x, y;

		if (data < 2)
			window->GetWindowPos(x, y);
		else if (data < 4)
			window->GetWindowSize(x, y);
		else
		{
			OpPoint scroll_offset = frames_doc->DOMGetScrollOffset();

			x = scroll_offset.x;
			y = scroll_offset.y;
		}

		if (argc >= 2 && argv[1].type == VALUE_NUMBER)
		{
			int dy = int(argv[1].value.number);

			if ((data & 1) == 0)
				y = dy;
			else
				y += dy;
		}

		if (argc >= 1 && argv[0].type == VALUE_NUMBER)
		{
			int dx = int(argv[0].value.number);

			if ((data & 1) == 0)
				x = dx;
			else
				x += dx;
		}

		if (data < 2)
			window->SetWindowPos(x, y);
		else if (data < 4)
			window->SetWindowSize(x, y);
		else
		{
			CALL_FAILED_IF_ERROR(frames_doc->Reflow(FALSE));
			frames_doc->DOMSetScrollOffset(&x, &y);
		}
	}

	return ES_SUSPEND;
}

void
DOM_JSWindowEventTarget::AddListener(DOM_EventListener *listener)
{
	DOM_EventTarget::AddListener(listener);

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	if (listener->HandlesEvent(STORAGE, NULL, ES_PHASE_AT_TARGET))
		OpStatus::Ignore(static_cast<JS_Window*>(owner)->AddStorageListeners());
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef EXTENSION_SUPPORT
	/* When registering a .connect handler for an extension, explicitly notify the underlying gadget. */

# ifdef DOM3_EVENTS
	if (listener->HandlesEvent(ONCONNECT, NULL, NULL, ES_PHASE_AT_TARGET))
# else
	if (listener->HandlesEvent(ONCONNECT, NULL, ES_PHASE_AT_TARGET))
# endif // DOM3_EVENTS
		if (FramesDocument *frames_doc = GetOwner()->GetEnvironment()->GetFramesDocument())
			if (frames_doc->GetWindow()->GetType() == WIN_TYPE_GADGET)
			{
				OpGadget* gadget = frames_doc->GetWindow()->GetGadget();
				OP_ASSERT(gadget);
				if (gadget->IsExtension())
					gadget->ConnectGadgetListeners();
			}
#endif // EXTENSION_SUPPORT

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	JS_Window *win = static_cast<JS_Window*>(owner);
	FramesDocument *doc = win->GetFramesDocument();
	VisualDevice* visdev = doc ? doc->GetWindow()->VisualDev() : NULL;
	if (visdev && (visdev->IsFocused(FALSE) || visdev->IsFocused(TRUE)))
	{
		if (listener->HandlesEvent(ONDEVICEMOTION, NULL, ES_PHASE_AT_TARGET))
			OpStatus::Ignore(win->AddMotionListener());

		if (listener->HandlesEvent(ONDEVICEORIENTATION, NULL, ES_PHASE_AT_TARGET))
			OpStatus::Ignore(win->AddOrientationListener());
	}
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
}

void
DOM_JSWindowEventTarget::RemoveListener(DOM_EventListener *listener)
{
	JS_Window *window = static_cast<JS_Window *>(owner);

	if (listener->IsFromAttribute() && listener->GetKnownType() == ONERROR)
		window->error_handler = NULL;

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	BOOL maybe_remove_device_motion_listener = window->m_motion_listener_attached && listener->HandlesEvent(ONDEVICEMOTION, NULL, ES_PHASE_AT_TARGET);
	BOOL maybe_remove_device_orientation_listener = window->m_orientation_listener_attached &&  listener->HandlesEvent(ONDEVICEORIENTATION, NULL, ES_PHASE_AT_TARGET);
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

	DOM_EventTarget::RemoveListener(listener);

#ifdef CLIENTSIDE_STORAGE_SUPPORT
	if (!HasListeners(STORAGE, NULL, ES_PHASE_AT_TARGET))
		window->RemoveStorageListeners();
#endif // CLIENTSIDE_STORAGE_SUPPORT

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
	if (maybe_remove_device_motion_listener && !this->HasListeners(ONDEVICEMOTION, NULL, ES_PHASE_AT_TARGET))
		window->RemoveMotionListener();

	if (maybe_remove_device_orientation_listener && !this->HasListeners(ONDEVICEORIENTATION, NULL, ES_PHASE_AT_TARGET))
		window->RemoveOrientationListener();
#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
}

JS_FakeDocument::JS_FakeDocument(JS_FakeWindow *fakewindow)
	: fakewindow(fakewindow)
{
}

void
JS_FakeDocument::InitializeL()
{
	AddFunctionL(open, "open");
	AddFunctionL(close, "close");
	AddFunctionL(write, 0, "write", "s");
	AddFunctionL(write, 1, "writeln", "s");
}

/* virtual */ ES_GetState
JS_FakeDocument::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_cookie:
	case OP_ATOM_title:
	case OP_ATOM_referrer:
		DOMSetString(value);
		return GET_SUCCESS;

	case OP_ATOM_location:
		return fakewindow->GetName(property_name, value, origining_runtime);
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
JS_FakeDocument::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_location:
		ES_Value dummy;
		if (GetName(OP_ATOM_location, &dummy, origining_runtime) == GET_NO_MEMORY)
			return PUT_NO_MEMORY;
		JS_Location *location = DOM_VALUE2OBJECT(dummy, JS_Location);
		return location->PutName(OP_ATOM_href, value, origining_runtime);
	}

	return PUT_FAILED;
}

/* virtual */ void
JS_FakeDocument::GCTrace()
{
	GCMark(fakewindow);
}

/* virtual */ int
JS_FakeDocument::open(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_FAKE_DOCUMENT, JS_FakeDocument);
	document->fakewindow->Open(origining_runtime);
	return ES_FAILED;
}

/* virtual */ int
JS_FakeDocument::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_FAKE_DOCUMENT, JS_FakeDocument);
	document->fakewindow->Close();
	return ES_FAILED;
}

/* virtual */ int
JS_FakeDocument::write(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(document, DOM_TYPE_FAKE_DOCUMENT, JS_FakeDocument);
	document->fakewindow->Open(origining_runtime);

	// data = 0: write  data = 1: writeln
	// writeln() -> writeln("") -> a single newline
	ES_Value empty_string;
	if (argc == 0 && data == 1)
	{
		empty_string.type = VALUE_STRING;
		empty_string.value.string = UNI_L("");
		argc = 1;
		argv = &empty_string;
	}

	for (int index = 0; index < argc; ++index)
		if (argv[index].type == VALUE_STRING)
			CALL_FAILED_IF_ERROR(document->fakewindow->AddDocumentData(argv[index].value.string, data == 1));
	return ES_FAILED;
}

JS_FakeWindow::JS_FakeWindow(DOM_JSWCCallback *callback)
	: callback(callback),
	  location(NULL),
	  document(NULL)
{
}

void
JS_FakeWindow::InitializeL()
{
	AddFunctionL(DOM_dummyMethod, "focus");
	AddFunctionL(DOM_dummyMethod, "blur");
	AddFunctionL(DOM_dummyMethod, "moveTo");
	AddFunctionL(DOM_dummyMethod, "moveBy");
	AddFunctionL(DOM_dummyMethod, "resizeTo");
	AddFunctionL(DOM_dummyMethod, "resizeBy");
	AddFunctionL(DOM_dummyMethod, "scroll");
	AddFunctionL(DOM_dummyMethod, "scrollTo");
	AddFunctionL(DOM_dummyMethod, "scrollBy");
}

/* static */ OP_STATUS
JS_FakeWindow::Make(JS_FakeWindow *&fakewindow, DOM_JSWCCallback *callback)
{
	DOM_Runtime *runtime = callback->GetRuntime();
	RETURN_IF_ERROR(DOMSetObjectRuntime(fakewindow = OP_NEW(JS_FakeWindow, (callback)), runtime, runtime->GetObjectPrototype(), "Window"));

	TRAPD(status, fakewindow->InitializeL());
	return status;
}

/* virtual */ ES_GetState
JS_FakeWindow::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_closed:
		DOMSetBoolean(value, FALSE);
		return GET_SUCCESS;

	case OP_ATOM_location:
		if (value && !location)
		{
			DOM_Runtime *runtime = GetRuntime();
			GET_FAILED_IF_ERROR(DOMSetObjectRuntime(location = OP_NEW(JS_Location, (this)), runtime, runtime->GetPrototype(DOM_Runtime::LOCATION_PROTOTYPE), "Location"));
		}
		DOMSetObject(value, location);
		return GET_SUCCESS;

	case OP_ATOM_document:
		if (value && !document)
		{
			DOM_Runtime *runtime = GetRuntime();
			GET_FAILED_IF_ERROR(DOMSetObjectRuntime(document = OP_NEW(JS_FakeDocument, (this)), runtime, runtime->GetObjectPrototype(), "HTMLDocument"));

			TRAPD(status, document->InitializeL());
			GET_FAILED_IF_ERROR(status);
		}
		DOMSetObject(value, document);
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
JS_FakeWindow::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	ES_Value dummy;

	switch (property_name)
	{
	case OP_ATOM_location:
		if (GetName(OP_ATOM_location, &dummy, origining_runtime) == GET_NO_MEMORY)
			return PUT_NO_MEMORY;
		return location->PutName(OP_ATOM_href, value, origining_runtime);

	case OP_ATOM_document:
		return PUT_SUCCESS;

	default:
		return PUT_FAILED;
	}
}

/* virtual */ void
JS_FakeWindow::GCTrace()
{
	GCMark(callback);
	GCMark(location);
	GCMark(document);
}

const URL &
JS_FakeWindow::GetURL()
{
	return callback->url;
}

void
JS_FakeWindow::SetURL(const URL &url, DocumentReferrer &refurl)
{
	callback->url = url;
	callback->refurl = refurl;
	callback->data.popup.has_data = FALSE;
}

OP_STATUS
JS_FakeWindow::AddDocumentData(const uni_char *data, BOOL newline)
{
	RETURN_IF_ERROR(callback->documentdata.Append(data));
	if (newline)
		RETURN_IF_ERROR(callback->documentdata.Append("\n"));
	return OpStatus::OK;
}

void
JS_FakeWindow::Open(DOM_Runtime *origining_runtime)
{
	if (!callback->data.popup.open)
	{
		callback->documentdata.Clear();
		callback->data.popup.open = callback->data.popup.has_data = TRUE;
	}
}

void
JS_FakeWindow::Close()
{
	callback->data.popup.open = FALSE;
}

#ifdef DOM_DEVICE_ORIENTATION_EVENT_SUPPORT
OP_STATUS
JS_Window::AddOrientationListener()
{
	if (!m_orientation_listener_attached)
		RETURN_IF_ERROR(g_DAPI_orientation_manager->AttachOrientationListener(this));
	m_orientation_listener_attached = TRUE;
	return OpStatus::OK;
}

OP_STATUS
JS_Window::AddMotionListener()
{
	if (!m_motion_listener_attached)
		RETURN_IF_ERROR(g_DAPI_orientation_manager->AttachMotionListener(this));
	m_motion_listener_attached = TRUE;
	return OpStatus::OK;
}

void
JS_Window::RemoveOrientationListener()
{
	if (m_orientation_listener_attached)
	{
		g_DAPI_orientation_manager->DetachOrientationListener(this);
		m_orientation_listener_attached = FALSE;
	}
}

void
JS_Window::RemoveMotionListener()
{
	if (m_motion_listener_attached)
	{
		g_DAPI_orientation_manager->DetachMotionListener(this);
		m_motion_listener_attached = FALSE;
	}

}

/* virtual */ void
JS_Window::OnOrientationChange(const OpOrientationListener::Data& data)
{
	DOM_DeviceOrientationEvent *evt;
	RETURN_VOID_IF_ERROR(DOM_DeviceOrientationEvent::Make(evt, GetRuntime()));
	RETURN_VOID_IF_ERROR(evt->InitValues(data.alpha, data.beta, data.gamma, data.absolute));
	GetRuntime()->GetEnvironment()->SendEvent(evt);
}

/* virtual */ void
JS_Window::OnCompassNeedsCalibration()
{
	DOM_CompassNeedsCalibrationEvent *evt;
	if (OpStatus::IsError(DOM_CompassNeedsCalibrationEvent::Make(evt, GetRuntime())) ||
		GetRuntime()->GetEnvironment()->SendEvent(evt) != OpBoolean::IS_TRUE)
		g_DAPI_orientation_manager->CompassCalibrationReply(this, FALSE); // If anything fails at least report that we don't handle this event.
}

/* virtual */ void
JS_Window::OnAccelerationChange(const OpMotionListener::Data& data)
{
	DOM_DeviceMotionEvent *evt;
	RETURN_VOID_IF_ERROR(DOM_DeviceMotionEvent::Make(evt, GetRuntime()
	                                               , data.x, data.y, data.z
	                                               , data.x_with_gravity, data.y_with_gravity, data.z_with_gravity
	                                               , data.rotation_alpha, data.rotation_beta, data.rotation_gamma
	                                               , data.interval));
	GetRuntime()->GetEnvironment()->SendEvent(evt);
}

#endif // DOM_DEVICE_ORIENTATION_EVENT_SUPPORT

/* static */ int
JS_Window::matchMedia(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_WINDOW, JS_Window);
	DOM_CHECK_ARGUMENTS("s");

	if (LogicalDocument* logdoc = window->GetLogicalDocument())
	{
		DOM_MediaQueryList* media_query_list;
		CALL_FAILED_IF_ERROR(DOM_MediaQueryList::Make(media_query_list, window->GetRuntime(), logdoc->GetHLDocProfile()->GetCSSCollection(), argv[0].value.string));
		DOMSetObject(return_value, media_query_list);
	}
	else
		DOMSetNull(return_value);

	return ES_VALUE;
}

/* static */ int
JS_Window::supportsCSS(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_CHECK_ARGUMENTS("zz");
	DOM_THIS_OBJECT_CHECK(this_object);

	ES_ValueString *prop = argv[0].value.string_with_length;
	ES_ValueString *value = argv[1].value.string_with_length;

	if (prop->length != uni_strlen(prop->string) ||
	    value->length != uni_strlen(value->string))
	{
		DOMSetBoolean(return_value, FALSE);
		return ES_VALUE;
	}

	CSS_SimpleCondition c;
	CALL_FAILED_IF_ERROR(c.SetDecl(prop->string, prop->length, value->string, value->length));

	DOMSetBoolean(return_value, c.Evaluate());

	return ES_VALUE;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(JS_WindowPrototype)
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::close, "!close", 0)
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::open, "open", "sssb-")
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::print, "print", 0)
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::stop, "stop", 0)
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::getComputedStyle, "getComputedStyle", "-s-")
#ifdef DOC_SEARCH_SUGGESTIONS_SUPPORT
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::onSearch, "onSearch", "b")
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::getSearchSuggest, "searchSuggest", "s")
#endif
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::home, "!home", 0)
#ifdef DOM_SELECTION_SUPPORT
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::getSelection, "getSelection", 0)
#endif // DOM_SELECTION_SUPPORT
#if 0
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::setDocument, "setDocument", 0)
#endif // 0
#ifdef DATABASE_STORAGE_SUPPORT
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::openDatabase, "openDatabase", "sssn-")
#endif // DATABASE_STORAGE_SUPPORT
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, DOM_dummyMethod, "releaseEvents", "n-")
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, DOM_dummyMethod, "captureEvents", "n-")
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, DOM_Node::dispatchEvent, "dispatchEvent", "-")
#ifdef URL_UPLOAD_BASE64_SUPPORT
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::base64_atob, "atob", "z-")
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::base64_btoa, "btoa", "z-")
#endif // URL_UPLOAD_BASE64_SUPPORT
#ifdef DOM_SUPPORT_BLOB_URLS
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::createObjectURL, "createObjectURL", 0)
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::revokeObjectURL, "revokeObjectURL", "s-")
#endif // DOM_SUPPORT_BLOB_URLS
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::matchMedia, "matchMedia", "s")
#ifdef DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::postMessage, "!postMessage", "-s-")
#endif // DOM_CROSSDOCUMENT_MESSAGING_SUPPORT
	DOM_FUNCTIONS_FUNCTION(JS_WindowPrototype, JS_Window::supportsCSS, "supportsCSS", "zz-")
DOM_FUNCTIONS_END(JS_WindowPrototype)

DOM_FUNCTIONS_WITH_DATA_START(JS_WindowPrototype)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::dialog, DOM_JSWCCallback::ALERT, "alert", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::dialog, DOM_JSWCCallback::CONFIRM, "confirm", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::dialog, DOM_JSWCCallback::PROMPT, "prompt", "ss-")
#ifdef DOM_SHOWMODALDIALOG_SUPPORT
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::dialog, DOM_JSWCCallback::MODALHTML, "showModalDialog", "s-")
#endif // DOM_SHOWMODALDIALOG_SUPPORT
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::focusOrBlur, 0, "!focus", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::focusOrBlur, 1, "!blur", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::setIntervalOrTimeout, 0, "setTimeout", "-n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::setIntervalOrTimeout, 1, "setInterval", "-n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::moveOrResizeOrScroll, 0, "!moveTo", "nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::moveOrResizeOrScroll, 1, "!moveBy", "nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::moveOrResizeOrScroll, 2, "!resizeTo", "nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::moveOrResizeOrScroll, 3, "!resizeBy", "nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::moveOrResizeOrScroll, 4, "!scroll", "nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::moveOrResizeOrScroll, 4, "!scrollTo", "nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::moveOrResizeOrScroll, 5, "!scrollBy", "nn-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::clearIntervalOrTimeout, 0, "clearInterval", "n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Window::clearIntervalOrTimeout, 1, "clearTimeout", "n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_History::walk, -1, "back", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_History::walk, 1, "forward", 0)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, DOM_Node::attachOrDetachEvent, 0, "attachEvent", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, DOM_Node::attachOrDetachEvent, 1, "detachEvent", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(JS_WindowPrototype, JS_Location::replaceOrAssign, 2, "navigate", "s-")
DOM_FUNCTIONS_WITH_DATA_END(JS_WindowPrototype)
