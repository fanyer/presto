/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensionpagecontext.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/doc/frm_doc.h"

/* static */ OP_STATUS
DOM_ExtensionPageContext::Make(DOM_ExtensionPageContext *&context, DOM_ExtensionSupport *extension_support, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(context = OP_NEW(DOM_ExtensionPageContext, (extension_support)), runtime, runtime->GetPrototype(DOM_Runtime::EXTENSION_PAGE_CONTEXT_PROTOTYPE), "ExtensionPage"));
	RETURN_IF_LEAVE(context->InjectOptionalAPIL());

	/* Extend the opera. namespace with extra properties. */
	ES_Value value;
	DOM_Object *global = runtime->GetEnvironment()->GetWindow();
	OP_BOOLEAN result = runtime->GetName(*global, UNI_L("opera"), &value);
	if (OpStatus::IsSuccess(result) && result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
	{
		ES_Object *opera = value.value.object;
		DOMSetObject(&value, context);
		RETURN_IF_ERROR(runtime->PutName(opera, UNI_L("extension"), value));
	}
	else
	{
		OP_ASSERT(!"Unable to locate 'opera' object");
		return OpStatus::IsSuccess(result) ? OpStatus::ERR : result;
	}

	RETURN_IF_ERROR(DOM_MessagePort::Make(context->m_extension_port, context->GetRuntime()));
	return context->Start();
}

void
DOM_ExtensionPageContext::InjectOptionalAPIL()
{
	if (m_extension_support->GetGadget()->GetAttribute(WIDGET_EXTENSION_SCREENSHOT_SUPPORT))
		AddFunctionL(getScreenshot, "getScreenshot", "-");
}

OP_STATUS
DOM_ExtensionPageContext::Start()
{
	RETURN_IF_ERROR(m_extension_support->AddPortConnection(m_extension_port));

	/* Issue a CONNECT event at the extension's target object to notify of new. Do this for every
	   document / DOM environment that's created. */
	if (DOM_ExtensionBackground *background = m_extension_support->GetBackground())
	{
		FramesDocument *frames_doc = background->GetFramesDocument();
		if (frames_doc && frames_doc->GetDOMEnvironment())
		{
			CALL_FAILED_IF_ERROR(background->DOM_Object::CreateEventTarget());
			DOM_EventTarget *event_target = background->GetEventTarget();

			BOOL has_listeners = FALSE;
			if (event_target)
				has_listeners = event_target->HasListeners(ONCONNECT, NULL, ES_PHASE_AT_TARGET);


			if (has_listeners)
			{
				DOM_MessageEvent *connect_event;
				URL origin_url = GetRuntime()->GetOriginURL();
				RETURN_IF_ERROR(DOM_MessageEvent::MakeConnect(connect_event, TRUE, origin_url, background, m_extension_port, FALSE/* no ports alias */));
				return background->GetEnvironment()->SendEvent(connect_event);
			}
		}
	}

	return OpStatus::OK;
}

/* static */ int
DOM_ExtensionPageContext::postMessage(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(context, DOM_TYPE_EXTENSION_PAGE_CONTEXT, DOM_ExtensionPageContext);
	DOM_CHECK_ARGUMENTS("-|O");
	DOM_ExtensionBackground *background = context->m_extension_support->GetBackground();
	if (!background || !background->GetFramesDocument())
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	DOM_MessagePort *target_port = background->GetPort();
	DOM_MessagePort *target_return_port = context->GetPort() ? context->GetPort()->GetTarget() : NULL;
	URL extension_url = context->GetEnvironment()->GetDOMRuntime()->GetOriginURL();
	DOM_Runtime *target_runtime = background->GetRuntime();

	return DOM_ExtensionSupport::PostMessageToPort(context, target_port, target_return_port, extension_url, argv, argc, return_value, target_runtime);
}

/* static */ int
DOM_ExtensionPageContext::getScreenshot(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(context, DOM_TYPE_EXTENSION_PAGE_CONTEXT, DOM_ExtensionPageContext);
	return context->m_extension_support->GetScreenshot(context->GetFramesDocument(), argv, argc, return_value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionPageContext::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_bgProcess:
		if (value && m_extension_support->GetBackground())
		{
			DOM_Object *window;
			GET_FAILED_IF_ERROR(m_extension_support->GetBackground()->GetEnvironment()->GetProxyWindow(window, origining_runtime));
			DOMSetObject(value, window);
		}
		return GET_SUCCESS;

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

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_ExtensionPageContext::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_bgProcess:
		return PUT_SUCCESS; // Read only.

	case OP_ATOM_onmessage:
		if (m_extension_port)
			return m_extension_port->PutName(property_name, value, origining_runtime);
		else
			return PUT_SUCCESS;

	case OP_ATOM_ondisconnect:
		PUT_FAILED_IF_ERROR(DOM_ExtensionSupport::AddEventHandler(this, m_ondisconnect_handler, NULL, value, origining_runtime, UNI_L("ondisconnect"), ONDISCONNECT));
		return PUT_SUCCESS;

	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */
DOM_ExtensionPageContext::~DOM_ExtensionPageContext()
{
	OP_NEW_DBG("DOM_ExtensionPageContext::~DOM_ExtensionPageContext()", "extensions.dom");
	OP_DBG(("this: %p", this));
}

void
DOM_ExtensionPageContext::BeforeDestroy()
{
	if (m_extension_support->GetBackground() && m_extension_port && m_extension_port->GetTarget())
	{
		DOM_EnvironmentImpl *target_environment = m_extension_support->GetBackground()->GetEnvironment();
		DOM_MessageEvent *disconnect_event;

		DOM_Object *target_object = m_extension_support->GetBackground();
		URL origin_url = GetRuntime()->GetOriginURL();
		/* About to leave, so do not supply a message port for back communication. */
		RETURN_VOID_IF_ERROR(DOM_MessageEvent::MakeConnect(disconnect_event, FALSE, origin_url, target_object, NULL/* no .source port*/, FALSE /* no ports alias */));

		OpStatus::Ignore(target_environment->SendEvent(disconnect_event));
		m_extension_support->RemovePortConnection(m_extension_port);
		m_extension_port->Disentangle();
	}
	else if (m_extension_port)
		m_extension_support->RemovePortConnection(m_extension_port);

	m_extension_port = NULL;
}

void
DOM_ExtensionPageContext::HandleDisconnect()
{
	if (!m_extension_port)
		return;

	if (m_ondisconnect_handler || GetEventTarget() && GetEventTarget()->HasListeners(ONDISCONNECT, NULL, ES_PHASE_AT_TARGET))
	{
		DOM_MessageEvent *disconnect_event;
		RETURN_VOID_IF_ERROR(DOM_MessageEvent::MakeConnect(disconnect_event, FALSE, GetEnvironment()->GetDOMRuntime()->GetOriginURL(), this, NULL, FALSE /* no ports alias */));
		RETURN_VOID_IF_ERROR(GetEnvironment()->SendEvent(disconnect_event));
	}

	m_extension_support->RemovePortConnection(m_extension_port);
	m_extension_port->Disentangle();
	m_extension_port = NULL;
}


/* virtual */ void
DOM_ExtensionPageContext::GCTrace()
{
	GCMark(FetchEventTarget());
	GCMark(m_extension_port);
}

/* virtual */ DOM_Object*
DOM_ExtensionPageContext::GetOwnerObject()
{
	return (m_extension_port ? m_extension_port->GetOwnerObject() : NULL);
}

/* static */ int
DOM_ExtensionPageContext::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION_PAGE_CONTEXT, DOM_ExtensionPageContext);

	if (argc > 0 && argv[0].type == VALUE_STRING && uni_str_eq(argv[0].value.string, "message"))
		if (extension->m_extension_port)
			return DOM_MessagePort::accessEventListenerStart(extension->m_extension_port, argv, argc, return_value, origining_runtime, data);
		else
			return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
	else
		return DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
}

/* static */ int
DOM_ExtensionPageContext::getFile(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION_PAGE_CONTEXT, DOM_ExtensionPageContext);
	DOM_CHECK_ARGUMENTS("s");

	if (!extension->GetFramesDocument())
		return ES_FAILED;

	const uni_char* resource_path = argv[0].value.string;
	return extension->GetExtensionSupport()->GetExtensionFile(resource_path, extension->GetFramesDocument()->GetURL(), return_value, this_object);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_ExtensionPageContext)
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionPageContext, DOM_ExtensionPageContext::postMessage, "postMessage", "")
	DOM_FUNCTIONS_FUNCTION(DOM_ExtensionPageContext, DOM_ExtensionPageContext::getFile, "getFile", "s-")
DOM_FUNCTIONS_END(DOM_ExtensionPageContext)

DOM_FUNCTIONS_WITH_DATA_START(DOM_ExtensionPageContext)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionPageContext, DOM_ExtensionPageContext::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionPageContext, DOM_ExtensionPageContext::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ExtensionPageContext)
#endif // EXTENSION_SUPPORT
