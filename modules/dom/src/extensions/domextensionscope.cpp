/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT
#include "modules/dom/src/extensions/domextensionscope.h"
#include "modules/dom/src/extensions/domextensionmanager.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/extensions/domextensionmenucontext_proxy.h"

DOM_ExtensionScopePrototype::DOM_ExtensionScopePrototype()
#ifdef DOM_NO_COMPLEX_GLOBALS
	: DOM_Prototype(NULL, g_DOM_globalData->DOM_ExtensionScopePrototype_functions_with_data)
#else // DOM_NO_COMPLEX_GLOBALS
	: DOM_Prototype(NULL, DOM_ExtensionScopePrototype_functions_with_data)
#endif
{
}

/* static */ void
DOM_ExtensionScopePrototype::InitializeL(DOM_Prototype *prototype, DOM_ExtensionScope *scope)
{
	prototype->InitializeL();
	DOM_Runtime *runtime = prototype->GetRuntime();
	DOM_BuiltInConstructor *constructor = OP_NEW(DOM_BuiltInConstructor, (prototype));
	LEAVE_IF_ERROR(DOM_Object::DOMSetFunctionRuntime(constructor, runtime, "Window"));

	ES_Value value;
	DOMSetObject(&value, constructor);
	prototype->PutL("Window", value, PROP_DONT_ENUM);
	prototype->PutL("constructor", value, PROP_DONT_ENUM);
}

DOM_ExtensionScope::DOM_ExtensionScope()
	: m_owner(NULL)
#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	, m_menu_context(NULL)
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPOR
{
}

DOM_ExtensionScope::~DOM_ExtensionScope()
{
	OP_NEW_DBG("DOM_ExtensionScope::~DOM_ExtensionScope()", "extensions.dom");
	OP_DBG(("this: %p", this));
}

/* static */ OP_STATUS
DOM_ExtensionScope::Make(DOM_Runtime *runtime)
{
	DOM_ExtensionScope *scope = OP_NEW(DOM_ExtensionScope, ());
	if (!scope)
		return OpStatus::ERR_NO_MEMORY;

	DOM_ExtensionScopePrototype *prototype = OP_NEW(DOM_ExtensionScopePrototype, ());
	if (!prototype)
	{
		OP_DELETE(scope);
		scope = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}
	RETURN_IF_ERROR(runtime->SetHostGlobalObject(scope, prototype));

	TRAPD(status, DOM_ExtensionScopePrototype::InitializeL(prototype, scope));
	return status;
}

/* virtual */ void
DOM_ExtensionScope::OnGadgetRunning()
{
	if (!m_extension || m_extension->IsConnected())
		return;

	OpStatus::Ignore(Start());
}

/* virtual */ void
DOM_ExtensionScope::OnGadgetTerminated()
{
	OP_NEW_DBG("DOM_ExtensionScope::OnGadgetTerminated()", "extensions.dom");
	OP_DBG(("this: %p", this));

	m_owner = NULL; // Any further use is unsafe

	if (m_extension)
		m_extension->HandleDisconnect();
}

/* virtual */ BOOL
DOM_ExtensionScope::HasGadgetConnected()
{
	return m_extension->IsConnected();
}

/* virtual */ void
DOM_ExtensionScope::GCTrace()
{
	GetEnvironment()->GCTrace();
	GCMark(FetchEventTarget());
	if (m_extension)
		GCMark(m_extension);
}

void
DOM_ExtensionScope::InitializeL()
{
	DOM_Runtime *runtime = GetRuntime();
	DOM_Object *object;

	DOMSetObjectRuntimeL(object = OP_NEW(JS_Console, ()), runtime, runtime->GetPrototype(DOM_Runtime::CONSOLE_PROTOTYPE), "Console");
	ES_Value value;
	DOMSetObject(&value, object);
	PutL("console", value);

	LEAVE_IF_ERROR(DOM_Extension::Make(m_extension, this, runtime));

	DOM_Object *exts_container;
	DOMSetObjectRuntimeL(exts_container = OP_NEW(DOM_Object, ()), runtime, runtime->GetObjectPrototype(), "Object");

	DOMSetObject(&value, *exts_container);
	PutL("opera", value, PROP_READ_ONLY);
	DOMSetObject(&value, *m_extension);
	PutL(*exts_container, "extension", value, runtime, PROP_READ_ONLY);
	AddFunctionL(*exts_container, runtime, JS_Opera::postError, "postError", "s");

#ifdef URL_FILTER
	DOM_ExtensionURLFilter *urlfilter;

	// Defines opera.extensions.urlfilter in the injected script (rules disabled, events allowed)
	LEAVE_IF_ERROR(DOM_ExtensionURLFilter::Make(urlfilter, m_extension_support->GetGadget(), runtime, FALSE, TRUE));
	DOMSetObject(&value, urlfilter);
	m_extension->PutL("urlfilter", value, PROP_READ_ONLY);
#endif // URL_FILTER

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
	if (m_extension_support->GetGadget()->GetAttribute(WIDGET_EXTENSION_ALLOW_CONTEXTMENUS))
	{
		ES_Object* es_contexts;
		LEAVE_IF_ERROR(runtime->CreateNativeObjectObject(&es_contexts));
		DOMSetObject(&value, es_contexts);
		PutL(exts_container->GetNativeObject(), "contexts", value, runtime, PROP_READ_ONLY);

		LEAVE_IF_ERROR(DOM_ExtensionMenuContextProxy::Make(m_menu_context, m_extension_support, runtime));
		DOMSetObject(&value, m_menu_context);
		PutL(es_contexts, "menu", value, runtime, PROP_READ_ONLY);
	}
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

	DOM_Widget* widget;
	LEAVE_IF_ERROR(DOM_Widget::Make(widget, runtime, m_owner));
	DOMSetObject(&value, widget);
	PutL("widget", value, PROP_READ_ONLY);

	PutFunctionL("Image", OP_NEW(DOM_HTMLImageElement_Constructor, ()), "Image", "nn-");

	PutFunctionL("MessageChannel", OP_NEW(DOM_MessageChannel_Constructor,()), "MessageChannel", NULL);
	PutFunctionL("MessageEvent", OP_NEW(DOM_MessageEvent_Constructor,()), "MessageEvent", NULL);
	PutConstructorL("MessagePort", DOM_Runtime::CROSSDOCUMENT_MESSAGEPORT_PROTOTYPE, TRUE);

	DOM_FileReader_Constructor* filereader_constructor = OP_NEW_L(DOM_FileReader_Constructor, ());
	PutConstructorL(filereader_constructor);
	DOM_FileReader::ConstructFileReaderObjectL(*filereader_constructor, runtime);
}

OP_STATUS
DOM_ExtensionScope::RegisterWithExtension()
{
	if (GetExtensionGadget() && GetExtensionGadget()->GetWindow())
	{
		FramesDocument *frames_doc = GetExtensionGadget()->GetWindow()->GetCurrentDoc();
		if (frames_doc && frames_doc->GetDOMEnvironment())
		{
			DOM_EnvironmentImpl *target_environment = static_cast<DOM_EnvironmentImpl *>(frames_doc->GetDOMEnvironment());
			m_extension_support = target_environment->GetExtensionBackground()->GetExtensionSupport();
			RETURN_IF_ERROR(m_extension_support->AddExtensionJSEnvironment(GetEnvironment(), this));
		}
	}
	return OpStatus::OK;
}

static OP_STATUS
InitializeExtensionScope(DOM_ExtensionScope *scope)
{
	TRAPD(status, scope->InitializeL());
	return status;
}

OP_STATUS
DOM_ExtensionScope::Initialize(ES_Environment *es_environment, OpGadget *owner)
{
	DOM_ExtensionScope *toplevel = GetGlobalScope(es_environment);
	DOM_ExtensionSupport *support = g_extension_manager->GetExtensionSupport(owner);

	if (!support)
		return OpStatus::ERR_NO_MEMORY;

	OP_ASSERT(toplevel->m_extension_support == NULL);
	toplevel->m_extension_support = support;
	owner->AddExtensionListener(toplevel);
	toplevel->m_owner = owner;

	RETURN_IF_ERROR(InitializeExtensionScope(toplevel));
	return toplevel->Start();
}

OP_STATUS
DOM_ExtensionScope::Start()
{
	DOM_ExtensionScope *toplevel = this;

	DOM_MessagePort *userjs_port = toplevel->GetExtension()->GetPort();

	if (!userjs_port)
	{
		RETURN_IF_ERROR(DOM_MessagePort::Make(userjs_port, toplevel->GetExtension()->GetRuntime()));
		/* Make the port available. */
		OP_ASSERT(userjs_port->GetRuntime() == toplevel->GetExtension()->GetRuntime());
		toplevel->GetExtension()->SetPort(userjs_port);

		RETURN_IF_ERROR(toplevel->GetExtensionSupport()->AddPortConnection(userjs_port));
	}

	RETURN_IF_ERROR(toplevel->RegisterWithExtension());

	/* Issue a CONNECT event at the extension background to notify of new. */
	if (toplevel->GetExtensionGadget() && toplevel->GetExtensionGadget()->GetWindow())
	{
		FramesDocument *frames_doc = toplevel->GetExtensionGadget()->GetWindow()->GetCurrentDoc();
		if (frames_doc && frames_doc->GetDOMEnvironment() && toplevel->GetExtensionSupport()->GetBackground())
		{
			DOM_EnvironmentImpl *target_environment = static_cast<DOM_EnvironmentImpl *>(frames_doc->GetDOMEnvironment());
			DOM_Object *target_object = toplevel->GetExtensionSupport()->GetBackground();
			BOOL has_listeners = FALSE;
			/* Do we have a plausible recipient? If not, leave as disconnected. */
			RETURN_IF_ERROR(target_object->CreateEventTarget());

			if (DOM_EventTarget *event_target = target_object->GetEventTarget())
				has_listeners = event_target->HasListeners(ONCONNECT, NULL, ES_PHASE_AT_TARGET);

			if (has_listeners)
			{
				DOM_MessageEvent *connect_event;
				URL origin_url = toplevel->GetEnvironment()->GetDOMRuntime()->GetOriginURL();
				RETURN_IF_ERROR(DOM_MessageEvent::MakeConnect(connect_event, TRUE, origin_url, target_object, userjs_port, FALSE/* no ports alias*/));
				toplevel->GetExtension()->SetConnectStatus(TRUE);

				return target_environment->SendEvent(connect_event);
			}
			else
				toplevel->GetExtension()->SetConnectStatus(FALSE);
		}
		else
			/* Marked as disconnected. The extension background 'sees' this upon registration of a connect handler on its window
			 * and causes Start() to be called again. */
			toplevel->GetExtension()->SetConnectStatus(FALSE);
	}

	return OpStatus::OK;
}

/* static */ DOM_ExtensionScope *
DOM_ExtensionScope::GetGlobalScope(ES_Environment *es_environment)
{
	if (EcmaScript_Object *global_object = ES_Runtime::GetHostObject(es_environment->GetGlobalObject()))
		return static_cast<DOM_ExtensionScope *>(global_object);
	else
		return NULL;
}

void DOM_ExtensionScope::BeforeDestroy()
{
	OP_NEW_DBG("DOM_ExtensionScope::BeforeDestroy()", "extensions.dom");
	OP_DBG(("this: %p gadget: %p", this, m_owner));

	if (m_owner)
		m_extension_support->RemoveExtensionJSEnvironment(GetEnvironment(), this);

	Out(); // Unregister from gadget listener.

	Shutdown(this);
}

/* static */ void
DOM_ExtensionScope::Shutdown(DOM_ExtensionScope *toplevel)
{
	OP_NEW_DBG("DOM_ExtensionScope::Shutdown()", "extensions.dom");
	OP_DBG(("toplevel: %p gadget: %p", toplevel, toplevel->GetExtensionGadget()));

	if (!toplevel->GetExtension())
		return;

	if (toplevel->GetExtension()->IsConnected())
	{
		toplevel->GetExtension()->SetConnectStatus(FALSE);
		if (toplevel->GetExtensionGadget() && toplevel->GetExtensionSupport()->GetBackground())
		{
			FramesDocument *frames_doc = toplevel->GetExtensionSupport()->GetBackground()->GetFramesDocument();
			if (frames_doc && frames_doc->GetDOMEnvironment() && toplevel->GetExtension()->GetPort())
			{
				DOM_MessageEvent *disconnect_event;

				DOM_Object *target_object = toplevel->GetExtensionSupport()->GetBackground();
				RETURN_VOID_IF_ERROR(DOM_MessageEvent::MakeConnect(disconnect_event, FALSE, frames_doc->GetURL(), target_object, toplevel->GetExtension()->GetPort(), FALSE /* no ports alias*/));

				DOM_EnvironmentImpl *target_environment = static_cast<DOM_EnvironmentImpl*>(frames_doc->GetDOMEnvironment());
				OpStatus::Ignore(target_environment->SendEvent(disconnect_event));
			}
		}
	}

	if (toplevel->GetExtension()->GetPort())
		toplevel->GetExtensionSupport()->RemovePortConnection(toplevel->GetExtension()->GetPort());
}

/*static*/int
DOM_ExtensionScope::dialog(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(extension, DOM_TYPE_EXTENSION_SCOPE, DOM_ExtensionScope);

	/* alert() et al are run in the context of the containing page's window. */
	DOM_Object *window = extension->GetEnvironment()->GetWindow();
	return JS_Window::dialog(window, argv, argc, return_value, origining_runtime, data);
}

/* virtual */ ES_GetState
DOM_ExtensionScope::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_document:
		if (value)
		{
			DOM_Object *document;
			GET_FAILED_IF_ERROR(GetEnvironment()->GetProxyDocument(document, origining_runtime));
			DOMSetObject(value, document);
		}
		return GET_SUCCESS;

	case OP_ATOM_self:
		DOMSetObject(value, this);
		return GET_SUCCESS;

	case OP_ATOM_window:
		if (value)
		{
			DOM_Object *window;
			GET_FAILED_IF_ERROR(GetEnvironment()->GetProxyWindow(window, origining_runtime));
			DOMSetObject(value, window);
		}
		return GET_SUCCESS;

	default:
		return GET_FAILED;
	}
}

/* virtual */ ES_PutState
DOM_ExtensionScope::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch(property_name)
	{
	case OP_ATOM_document:
	case OP_ATOM_self:
	case OP_ATOM_window:
		return PUT_SUCCESS;
	default:
		return DOM_Object::PutName(property_name, value, origining_runtime);
	}
}

DOM_FUNCTIONS_WITH_DATA_START(DOM_ExtensionScopePrototype)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, DOM_ExtensionScope::dialog, 0, "alert", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, DOM_ExtensionScope::dialog, 1, "confirm", "s-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, DOM_ExtensionScope::dialog, 2, "prompt", "ss-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, JS_Window::setIntervalOrTimeout, 0, "setTimeout", "-n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, JS_Window::setIntervalOrTimeout, 1, "setInterval", "-n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, JS_Window::clearIntervalOrTimeout, 0, "clearInterval", "n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, JS_Window::clearIntervalOrTimeout, 1, "clearTimeout", "n-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionScopePrototype, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ExtensionScopePrototype)

#endif // EXTENSION_SUPPORT
