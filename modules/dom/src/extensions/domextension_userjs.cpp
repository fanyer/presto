/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextension_userjs.h"
#include "modules/dom/src/extensions/domextensionscope.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domwebworkers/domcrossutils.h"
#include "modules/dom/src/domwebworkers/domwwutils.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/opera/domextscriptloader.h"

/* static */ OP_STATUS
DOM_Extension::Make(DOM_Extension *&new_obj, DOM_ExtensionScope *toplevel, DOM_Runtime *origining_runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_Extension, (toplevel)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_PROTOTYPE), "Extension"));
	RETURN_IF_LEAVE(new_obj->InjectOptionalAPIL());
	OP_ASSERT(toplevel->GetRuntime() == origining_runtime);
	return OpStatus::OK;
}

void
DOM_Extension::InjectOptionalAPIL()
{
	if (GetExtensionScope()->GetExtensionSupport()->GetGadget()->GetAttribute(WIDGET_EXTENSION_SCREENSHOT_SUPPORT))
		AddFunctionL(getScreenshot, "getScreenshot", "-");
}

void
DOM_Extension::HandleDisconnect()
{
	SetConnectStatus(FALSE);

	if (m_ondisconnect_handler || GetEventTarget() && GetEventTarget()->HasListeners(ONDISCONNECT, NULL, ES_PHASE_AT_TARGET))
	{
		DOM_MessageEvent *disconnect_event;
		RETURN_VOID_IF_ERROR(DOM_MessageEvent::MakeConnect(disconnect_event, FALSE, GetEnvironment()->GetDOMRuntime()->GetOriginURL(), this, m_extension_port, FALSE/* no ports alias*/));
		RETURN_VOID_IF_ERROR(GetEnvironment()->SendEvent(disconnect_event));
	}
}

DOM_Extension::~DOM_Extension()
{
	OP_NEW_DBG("DOM_Extension::~DOM_Extension()", "extensions.dom");
	OP_DBG(("this: %p", this));
}

/* static */ int
DOM_Extension::postMessage(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION, DOM_Extension);
	DOM_CHECK_ARGUMENTS("-|O");

	OpGadget* owner = extension->GetExtensionScope()->GetExtensionGadget();
	if (!owner || !owner->GetWindow() || !owner->GetWindow()->GetCurrentDoc())
		return ES_FAILED;

	FramesDocument *target_doc = owner->GetWindow()->GetCurrentDoc();

	DOM_ExtensionBackground *extension_background = extension->GetExtensionScope()->GetExtensionSupport()->GetBackground();
	DOM_EnvironmentImpl *target_environment = static_cast<DOM_EnvironmentImpl*>(target_doc->GetDOMEnvironment());

	if (!target_environment || !extension_background)
		return ES_FAILED;

	DOM_MessagePort *target_port = extension_background->GetPort();
	DOM_MessagePort *target_return_port = extension->GetPort() ? extension->GetPort()->GetTarget() : NULL;
	URL extension_url = extension->GetEnvironment()->GetDOMRuntime()->GetOriginURL();
	DOM_Runtime *target_runtime = target_environment->GetDOMRuntime();

	return DOM_ExtensionSupport::PostMessageToPort(extension, target_port, target_return_port, extension_url, argv, argc, return_value, target_runtime);

}

#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
/*virtual*/ OP_STATUS
DOM_Extension::HandleException(const ES_Value &exception, URL *url, const uni_char *message, int line_number, const uni_char *full_message/*ignored*/)
{
	OpString url_str;
	if (url)
		RETURN_IF_ERROR(url->GetAttribute(URL::KUniName_With_Fragment, url_str));
	else
		RETURN_IF_ERROR(GetRuntime()->GetOriginURL().GetAttribute(URL::KUniName_With_Fragment, url_str));

	DOM_ErrorEvent *event = NULL;
	RETURN_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(this, event, url_str.CStr(), message, line_number, TRUE/*propagate, if needs be*/));
	event->SetWindowEvent();
	return GetEnvironment()->SendEvent(event);
}

/* virtual */ OP_STATUS
DOM_Extension::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	if (m_loader)
		OpStatus::Ignore(m_loader->HandleCallback(status, result));

	switch(status)
	{
	case ES_ASYNC_SUCCESS:
		return OpStatus::OK;
	case ES_ASYNC_EXCEPTION:
		return HandleError(result/*this is the exception object*/, GetRuntime());

	/* 'result' is of an undefined nature for these; propagate an internal error event for now. */
	case ES_ASYNC_FAILURE:
	case ES_ASYNC_NO_MEMORY:
	case ES_ASYNC_CANCELLED:
	default:
		DOM_ErrorEvent	*event = NULL;
		const uni_char *msg = UNI_L("Internal error");
		unsigned int line_number = 0;
		OpString url_str;
		RETURN_IF_ERROR(GetRuntime()->GetOriginURL().GetAttribute(URL::KUniName_With_Fragment, url_str));
		RETURN_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(this, event, url_str.CStr(), msg, line_number, TRUE/*propagate, if needs be*/));
		event->SetWindowEvent();
		return GetEnvironment()->SendEvent(event);
	}
}

/*static*/ OP_STATUS
DOM_Extension::HandleError(const ES_Value &exception, ES_Runtime *origining_runtime)
{
	/* This is the exception case of ES_AsyncCallback::HandleCallback() */
	ES_Value message_value;

	unsigned line_number = 1;
	const uni_char *msg = UNI_L("");

	/* Extrapolate the arguments required by HandleException() */
	if (exception.type == VALUE_STRING)
		msg = exception.value.string;
	else if (exception.type == VALUE_OBJECT)
	{
		ES_Object *error_object = exception.value.object;
		DOM_Object *host_object = DOM_HOSTOBJECT(error_object, DOM_Object);
		if (host_object && host_object->IsA(DOM_TYPE_ERROREVENT))
		{
			DOM_ErrorEvent *error_event = static_cast<DOM_ErrorEvent*>(host_object);
			msg = error_event->GetMessage();
			line_number = error_event->GetResourceLineNumber();
		}
		else if (host_object)
		{
			if (host_object->GetName(OP_ATOM_message, &message_value, origining_runtime) == GET_SUCCESS && message_value.type == VALUE_STRING)
					msg = message_value.value.string;

			if (host_object->GetName(OP_ATOM_lineno, &message_value, origining_runtime) == GET_SUCCESS && message_value.type == VALUE_NUMBER)
					line_number = static_cast<unsigned>(message_value.value.number);
			else if (host_object->GetName(OP_ATOM_code, &message_value, origining_runtime) == GET_SUCCESS && message_value.type == VALUE_NUMBER)
					line_number = static_cast<unsigned>(message_value.value.number);
		}
		else
		{
			if (OpStatus::IsSuccess(origining_runtime->GetName(error_object, UNI_L("message"), &message_value)) && message_value.type == VALUE_STRING)
				msg = message_value.value.string;
			if (OpStatus::IsSuccess(origining_runtime->GetName(error_object, UNI_L("lineno"), &message_value)) && message_value.type == VALUE_NUMBER)
				line_number = static_cast<unsigned>(message_value.value.number);
		}
	}

	DOM_ErrorEvent *event = NULL;
	OpString url_str;
	RETURN_IF_ERROR(GetRuntime()->GetOriginURL().GetAttribute(URL::KUniName_With_Fragment, url_str));
	RETURN_IF_ERROR(DOM_ErrorException_Utils::BuildErrorEvent(this, event, url_str.CStr(), msg, line_number, TRUE/*propagate, if needs be*/));
	event->SetWindowEvent();
	return GetEnvironment()->SendEvent(event);
}

OP_STATUS
DOM_Extension::SetLoaderReturnValue(const ES_Value &value)
{
	DOM_Object::DOMFreeValue(m_loader_return_value);
	return DOM_Object::DOMCopyValue(m_loader_return_value, value);
}

int
DOM_Extension::EvalImportedScript(ES_Value *return_value)
{
	if (!return_value)
		return ES_FAILED;

	/* Ready to go -- evaluate */
	ES_ProgramText program_array[1];
	int program_array_length = ARRAY_SIZE(program_array);
	program_array[0].program_text = return_value->value.string;
	program_array[0].program_text_length = uni_strlen(return_value->value.string);

	/* NOTE: if the UserJS execution environment's runtime has no FramesDocument,
	   this will get at the right async interface -- ie., the imported script must
	   execute within its context.

	   If that runtime has a FramesDocument, equal to that of its shared DOM environment,
	   GetESAsyncInterface() returns its async interface instead.

	   This is currently the case, but if a FramesDocument turns out to be required, this
	   will have to change. Or, ES_Runtime::GetESAsyncInterface(). */
	ES_AsyncInterface *asyncif = GetRuntime()->GetESAsyncInterface();

	/* The extension has gone, just shut down loader. */
	if (!asyncif)
		return ES_FAILED;

	/* If exceptions are thrown during evaluation, then we'd want to have those reported directly. */
	asyncif->SetWantExceptions();

	ES_Thread *interrupt_thread = GetRuntime()->GetESScheduler()->GetCurrentThread();
	OP_STATUS status = asyncif->Eval(program_array, program_array_length, NULL, 0, this/*callback*/, interrupt_thread, GetRuntime()->GetGlobalObject());

	DOM_Object::DOMFreeValue(*return_value);
	DOM_Object::DOMSetUndefined(return_value);

	if (OpStatus::IsMemoryError(status))
		return ES_NO_MEMORY;
	else if (OpStatus::IsError(status))
		return CallDOMException(DOM_Object::SYNTAX_ERR, return_value);
	else
	{
		DOMSetUndefined(return_value);
		m_is_evaluating = TRUE; /* Distinguish between waiting-for-load and waiting-for-eval */
		return (ES_RESTART | ES_SUSPEND);
	}
}

/* static */ int
DOM_Extension::importScript(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION, DOM_Extension);

	if (argc < 0)
	{
		if (extension->m_is_evaluating)
		{
			/* Done. */
			extension->m_is_evaluating = FALSE;
			return ES_FAILED;
		}
		else if (extension->m_loader_return_value.type != VALUE_STRING)
			return DOM_CALL_DOMEXCEPTION(SYNTAX_ERR);
		else
		{
			int result = extension->EvalImportedScript(&extension->m_loader_return_value);
			if (extension->m_loader)
			{
				extension->m_loader->Shutdown();
				extension->m_loader = NULL;
			}
			return result;
		}
	}

	DOM_CHECK_ARGUMENTS("s");

	URL origin_url = extension->GetRuntime()->GetOriginURL();

	FramesDocument *frames_doc = extension->GetRuntime()->GetFramesDocument();

	if (extension->GetExtensionScope()->GetExtensionGadget())
	{
		if (extension->GetExtensionScope()->GetExtensionGadget()->GetWindow())
			frames_doc = extension->GetExtensionScope()->GetExtensionGadget()->GetWindow()->GetCurrentDoc();
		if (frames_doc)
			origin_url = frames_doc->GetURL();
	}

	URL resolved_url = g_url_api->GetURL(origin_url, argv[0].value.string);

	if (!DOM_WebWorker_Utils::CheckImportScriptURL(resolved_url))
		return DOM_CALL_DOMEXCEPTION(SECURITY_ERR);

	DOM_Runtime *runtime = extension->GetRuntime();

	if (extension->m_loader)
		extension->m_loader->Shutdown();

	CALL_FAILED_IF_ERROR(DOM_ExtensionScriptLoader::Make(extension->m_loader, resolved_url, frames_doc, extension, runtime));

	ES_Thread *interrupt_thread = origining_runtime->GetESScheduler()->GetCurrentThread();
	OP_BOOLEAN result;
	if (OpStatus::IsError(result = extension->m_loader->LoadScript(this_object, return_value, interrupt_thread)))
	{
		if (OpStatus::IsMemoryError(result))
			return ES_NO_MEMORY;
		else
			return DOM_CALL_DOMEXCEPTION(NOT_FOUND_ERR);
	}
	else if (result == OpBoolean::IS_TRUE && return_value->type == VALUE_STRING)
		return extension->EvalImportedScript(return_value);
	else
		return (ES_RESTART | ES_SUSPEND);
}
#endif // EXTENSION_FEATURE_IMPORTSCRIPT

/* static */ int
DOM_Extension::getFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION, DOM_Extension);
	DOM_CHECK_ARGUMENTS("s");

	const uni_char* resource_path = argv[0].value.string;
	OpGadget* gadget = extension->GetExtensionScope()->GetExtensionGadget();
	Window* wnd;
	if (!(wnd = gadget->GetWindow()))
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR); //
	URL includes_url = g_url_api->GetURL(wnd->GetCurrentURL(), UNI_L("/includes/"), FALSE, gadget->UrlContextId());
	return extension->GetExtensionScope()->GetExtensionSupport()->GetExtensionFile(resource_path, includes_url, return_value, this_object);
}

/* virtual */ ES_GetState
DOM_Extension::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_onmessage:
		if (m_extension_port)
			return m_extension_port->GetName(property_name, value, origining_runtime);
		else
		{
			DOMSetNull(value);
			return GET_SUCCESS;
		}

	case OP_ATOM_ondisconnect:
		DOMSetObject(value, m_ondisconnect_handler);
		return GET_SUCCESS;

#ifdef EXTENSION_FEATURE_EXTENSION_PORT
	case OP_ATOM_port:
		DOMSetObject(value, m_extension_port);
		return GET_SUCCESS;
#endif // EXTENSION_FEATURE_EXTENSION_PORT

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_Extension::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_onmessage:
		if (m_extension_port)
			return m_extension_port->PutName(property_name, value, origining_runtime);
		else
			return PUT_SUCCESS;

	case OP_ATOM_ondisconnect:
		PUT_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(this, m_ondisconnect_handler, NULL, value, origining_runtime, UNI_L("disconnect"), ONDISCONNECT));
		return PUT_SUCCESS;

#ifdef EXTENSION_FEATURE_EXTENSION_PORT
	case OP_ATOM_port:
		return PUT_READ_ONLY;
#endif // EXTENSION_FEATURE_EXTENSION_PORT

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_Extension::GCTrace()
{
	GCMark(FetchEventTarget());
	GCMark(m_extension_port);

#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
	if (m_loader)
		GetRuntime()->GCMark(m_loader);
#endif // EXTENSION_FEATURE_IMPORTSCRIPT
}

/* static */ int
DOM_Extension::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION, DOM_Extension);

	if (argc > 0 && argv[0].type == VALUE_STRING && uni_str_eq(argv[0].value.string, "message"))
		if (extension->m_extension_port)
			return DOM_MessagePort::accessEventListenerStart(extension->m_extension_port, argv, argc, return_value, origining_runtime, data);
		else
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
	else
		return DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
}

/* static */ int
DOM_Extension::getScreenshot(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION, DOM_Extension);
	return extension->GetExtensionScope()->GetExtensionSupport()->GetScreenshot(extension->GetEnvironment()->GetFramesDocument(), argv, argc, return_value, origining_runtime);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Extension)
	DOM_FUNCTIONS_FUNCTION(DOM_Extension, DOM_Extension::postMessage, "postMessage", "")
#ifdef EXTENSION_FEATURE_IMPORTSCRIPT
	DOM_FUNCTIONS_FUNCTION(DOM_Extension, DOM_Extension::importScript, "importScript", "s-")
#endif // EXTENSION_FEATURE_IMPORTSCRIPT
	DOM_FUNCTIONS_FUNCTION(DOM_Extension, DOM_Extension::getFile, "getFile", "s-")
DOM_FUNCTIONS_END(DOM_Extension)

DOM_FUNCTIONS_WITH_DATA_START(DOM_Extension)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Extension, DOM_Extension::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Extension, DOM_Extension::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_Extension)

#endif // EXTENSION_SUPPORT
