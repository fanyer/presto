/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/dom/src/extensions/domextensionmanager.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/domevents/domeventlistener.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/domfile/domfile.h"
#include "modules/gadgets/OpGadgetManager.h"
#include "modules/dochand/docman.h"
#include "modules/dom/src/canvas/domcontext2d.h"

/* static */ OP_STATUS
DOM_ExtensionSupport::Make(DOM_ExtensionSupport *&new_obj, OpGadget *extension_gadget)
{
	new_obj = OP_NEW(DOM_ExtensionSupport, (extension_gadget));
	RETURN_OOM_IF_NULL(new_obj);
	extension_gadget->AddExtensionListener(new_obj);
	return OpStatus::OK;
}

DOM_ExtensionSupport::~DOM_ExtensionSupport()
{
	OP_NEW_DBG("DOM_ExtensionSupport::~DOM_ExtensionSupport()", "extensions.dom");
	OP_DBG(("this: %p", this));

	Shutdown();

	if (InList())
		Out(); // Unregister from gadget listener.

	m_background = NULL;
	m_ports.Clear();
}


void DOM_ExtensionSupport::HandleKeyData(const void *key, void *data)
{
	OP_DELETE(static_cast<ActiveExtensionElement *>(data));
}

void
DOM_ExtensionSupport::Shutdown()
{
	OP_NEW_DBG("DOM_ExtensionSupport::Shutdown()", "extensions.dom");
	OP_DBG(("this: %p gadget: %p", this, m_gadget));

	if (!m_gadget)
		return; // We already shut things down when the gadget was killed.

	if (g_extension_manager)
		g_extension_manager->RemoveExtensionContext(this);

	m_active_environments.ForEach(this);
	m_gadget = NULL;
}

/* static */ OP_STATUS
DOM_ExtensionSupport::GetPortTarget(DOM_MessagePort *port, DOM_MessagePort *&target_port, DOM_Runtime *target_runtime)
{
	if (!port->GetTarget())
	{
		DOM_MessagePort *proxy_port_at_target = NULL;
		RETURN_IF_ERROR(DOM_MessagePort::Make(proxy_port_at_target, target_runtime));
		RETURN_IF_ERROR(port->Entangle(proxy_port_at_target));
	}

	target_port = port->GetTarget();
	return OpStatus::OK;
}

void
DOM_ExtensionSupport::SetExtensionBackground(DOM_ExtensionBackground *background)
{
	OP_NEW_DBG("DOM_ExtensionSupport::SetExtensionBackground()", "extensions.dom");
	OP_DBG(("this: %p background %p", this, background));

	OP_ASSERT(background || !"Use UnsetExtensionBackground() instead of SetExtensionBackground(NULL).");

	/* If the new document is not using the same window that was created when
	 * the gadget first went up problems are afoot, i.e. the background page
	 * should only be allowed to be reloaded, not replaced by something else. */
	OP_ASSERT(m_gadget->GetWindow() == background->GetEnvironment()->GetFramesDocument()->GetWindow());

	m_background = background;

	for (UINT32 i=0; i < m_ports.GetCount(); i++)
	{
		DOM_MessagePort *p = m_ports.Get(i);
		p->Disentangle();

		if (background)
		{
			DOM_MessagePort *target_port;
			if (OpStatus::IsSuccess(GetPortTarget(p, target_port, background->GetRuntime())))
			{
				target_port->SetForwardingPort(background->GetPort());
				p->Entangle(target_port);
			}
			// Otherwise, tough luck, better to have some messages not getting where
			// they're going than be left with no background process.
		}
	}
}

void
DOM_ExtensionSupport::UnsetExtensionBackground(DOM_ExtensionBackground *background)
{
	OP_NEW_DBG("DOM_ExtensionSupport::UnsetExtensionBackground()", "extensions.dom");
	OP_DBG(("this: %p background %p", this, background));

	// If they're different, a new background page went up before this one died, ignore.
	if (m_background == background)
	{
		m_background = NULL;

		for (UINT32 i=0; i < m_ports.GetCount(); i++)
			m_ports.Get(i)->Disentangle();
	}
}


OP_STATUS
DOM_ExtensionSupport::AddPortConnection(DOM_MessagePort *p)
{
	OP_NEW_DBG("DOM_ExtensionSupport::AddPortConnection()", "extensions.dom");
	OP_DBG(("this: %p port: %p", this, p));

	if (m_background)
	{
		DOM_MessagePort *target_port = NULL;
		RETURN_IF_ERROR(GetPortTarget(p, target_port, m_background->GetRuntime()));

		/* Many ports may communicate with the extension background;
		   the m_ports list take care of broadcasting. Setting
		   the extension's port as the forwarding port for each
		   port's (proxy) handle the other direction, causing messages
		   posted to the port proxy to be forward and delivered to
		   the extension port. */
		p->GetTarget()->SetForwardingPort(m_background->GetPort());
	}

	return m_ports.Add(p);
}

void
DOM_ExtensionSupport::RemovePortConnection(DOM_MessagePort *p)
{
	OP_NEW_DBG("DOM_ExtensionSupport::RemovePortConnection()", "extensions.dom");
	OP_DBG(("this: %p port: %p", this, p));

	if (OpStatus::IsSuccess(m_ports.RemoveByItem(p)))
		p->Disentangle();
}

class ActiveElement
	: public Link
{
public:
	ActiveElement(DOM_ExtensionScope *s)
		: m_extension(s)
	{
	}

	DOM_ExtensionScope *m_extension;
};

DOM_ExtensionSupport::ActiveExtensionElement::~ActiveExtensionElement()
{
	m_extensions.Clear();
}

BOOL
DOM_ExtensionSupport::IsExtensionActiveInWindow(Window *window)
{
	return window &&
		   window->GetCurrentDoc() &&
		   window->GetCurrentDoc()->GetDOMEnvironment() &&
		   m_active_environments.Contains(static_cast<DOM_EnvironmentImpl *>(window->GetCurrentDoc()->GetDOMEnvironment()));
}

DOM_ExtensionScope *
DOM_ExtensionSupport::FindExtensionUserJS(FramesDocument *frames_doc)
{
	OP_ASSERT(frames_doc);

	DOM_EnvironmentImpl *env =  static_cast<DOM_EnvironmentImpl*>(frames_doc->GetDOMEnvironment());
	ActiveExtensionElement *elem;
	if (env && OpStatus::IsSuccess(m_active_environments.GetData(env, &elem)))
	{
		DOM_ExtensionScope *scope = static_cast<ActiveElement*>(elem->m_extensions.First())->m_extension;
		OP_ASSERT(scope);
		return scope;
	}
	return NULL;
}

OP_STATUS
DOM_ExtensionSupport::AddExtensionJSEnvironment(DOM_EnvironmentImpl *environment, DOM_ExtensionScope *toplevel)
{
	/* Each extension JS is registered along with its environment; needed
	   so that when a Tab object attaches, it can get at a port to communicate with.

	   An (uncomfortable) question is what's the right extension JS instance within that (tabbed)
	   window to communicate with? Pick the most-recent one for now. */
	ActiveExtensionElement *active_elem = NULL;
	if (OpStatus::IsError(m_active_environments.GetData(environment, &active_elem)))
	{
		active_elem = OP_NEW(ActiveExtensionElement, ());
		if (!active_elem)
			return OpStatus::ERR_NO_MEMORY;

		ActiveElement *elem = OP_NEW(ActiveElement, (toplevel));
		if (!elem)
		{
			OP_DELETE(active_elem);
			return OpStatus::ERR_NO_MEMORY;
		}
		elem->Into(&active_elem->m_extensions);

		return m_active_environments.Add(environment, active_elem);
	}
	else
	{
		ActiveElement *elem = OP_NEW(ActiveElement, (toplevel));
		if (!elem)
			return OpStatus::ERR_NO_MEMORY;

		elem->Into(&active_elem->m_extensions);
		return OpStatus::OK;
	}
}

void
DOM_ExtensionSupport::RemoveExtensionJSEnvironment(DOM_EnvironmentImpl *environment, DOM_ExtensionScope *toplevel)
{
	ActiveExtensionElement *active_elem = NULL;

	/* Removing to-be-invalid references; if unbound, that's acceptable. */
	if (OpStatus::IsSuccess(m_active_environments.GetData(environment, &active_elem)))
	{
		for (ActiveElement *elem = static_cast<ActiveElement *>(active_elem->m_extensions.First()); elem; elem = static_cast<ActiveElement *>(elem->Suc()))
		{
			if (elem->m_extension == toplevel)
			{
				elem->Out();
				OP_DELETE(elem);
				if (active_elem->m_extensions.Empty())
				{
					OpStatus::Ignore(m_active_environments.Remove(environment, &active_elem));
					OP_DELETE(active_elem);
				}
				break;
			}
		}
	}
}

DOM_ExtensionScope*
DOM_ExtensionSupport::GetExtensionGlobalScope(Window *window)
{
	if (window && window->GetCurrentDoc() && window->GetCurrentDoc()->GetDOMEnvironment())
	{
		ActiveExtensionElement *active_elem = NULL;
		if (OpStatus::IsSuccess(m_active_environments.GetData(static_cast<DOM_EnvironmentImpl *>(window->GetCurrentDoc()->GetDOMEnvironment()), &active_elem)) && active_elem)
		{
			ActiveElement *elem = static_cast<ActiveElement *>(active_elem->m_extensions.Last());
			return (elem ? elem->m_extension : NULL);
		}
	}

	return NULL;
}


/* Local helper functions */


/* static */ int
DOM_ExtensionSupport::PostMessageToPort(DOM_Object *this_object, DOM_MessagePort *target_port, DOM_MessagePort *target_return_port, URL &extension_url, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *target_runtime)
{
	DOM_Object::DOMSetNull(return_value);

	ES_Value message_argv[2];
	message_argv[0] = argv[0];
	if (argc == 1)
		DOM_Object::DOMSetNull(&message_argv[1]);
	else
		message_argv[1] = argv[1];

	ES_Value message_event_value;
	DOM_MessageEvent *message_event;
	int result;

	if ((result = DOM_MessageEvent::Create(message_event, this_object, target_runtime, NULL, target_return_port, extension_url, message_argv, ARRAY_SIZE(message_argv), &message_event_value, TRUE)) != ES_FAILED)
	{
		*return_value = message_event_value;
		return result;
	}

	message_event->SetSynthetic();
	message_event->SetTarget(target_port);
	CALL_FAILED_IF_ERROR(DOM_MessagePort::SendMessage(target_port, message_event));
	return ES_FAILED;
}

/* static */ OP_STATUS
DOM_ExtensionSupport::AddEventHandler(DOM_Object *target, DOM_EventListener *&handler, ES_Object *properties, ES_Value *given_value, ES_Runtime *origining_runtime, const uni_char *event_handler_name, DOM_EventType event_type, BOOL *has_changed /*= NULL*/)
{
	ES_Value value;
	OP_BOOLEAN result = OpBoolean::IS_TRUE;

	if (given_value)
		value = *given_value;
	else
		RETURN_IF_ERROR(result = target->GetRuntime()->GetName(properties, event_handler_name, &value));

	if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT && op_strcmp(ES_Runtime::GetClass(value.value.object), "Function") == 0)
	{
		RETURN_IF_ERROR(target->CreateEventTarget());

		DOM_EventListener *listener = OP_NEW(DOM_EventListener, ());
		if (!listener || OpStatus::IsMemoryError(listener->SetNative(target->GetEnvironment(), event_type, NULL, FALSE, TRUE, NULL, value.value.object)))
		{
			if (listener)
				DOM_EventListener::DecRefCount(listener);
			return OpStatus::ERR_NO_MEMORY;
		}

#ifdef ECMASCRIPT_DEBUGGER
		// If debugging is enabled we store the caller which registered the event listener.
		ES_Context *context = DOM_Object::GetCurrentThread(origining_runtime)->GetContext();
		if (origining_runtime->IsContextDebugged(context))
		{
			ES_Runtime::CallerInformation call_info;
			OP_STATUS status = ES_Runtime::GetCallerInformation(context, call_info);
			if (OpStatus::IsSuccess(status))
				listener->SetCallerInformation(call_info.script_guid, call_info.line_no);
		}
#endif // ECMASCRIPT_DEBUGGER

		if (handler)
			target->GetEventTarget()->RemoveListener(handler);
		target->GetEventTarget()->AddListener(listener);

		handler = listener;
		if (has_changed)
			*has_changed = TRUE;
	}
	return OpStatus::OK;
}

int
DOM_ExtensionSupport::GetExtensionFile(const uni_char* path, URL parent_url, ES_Value* return_value, DOM_Object* this_object)
{
	DOM_Runtime* runtime = this_object->GetRuntime();
	Window* wnd = GetGadget()->GetWindow();
	if (!wnd)
		return this_object->CallDOMException(DOM_Object::INVALID_STATE_ERR, return_value);
	URL url = g_url_api->GetURL(parent_url, path, FALSE, GetGadget()->UrlContextId());

	OpString full_path;
	OP_STATUS status = g_gadget_manager->FindGadgetResource(GetGadget()->GetWindow()->GetMessageHandler(), &url, full_path);
	if (OpStatus::IsError(status))
		DOM_Object::DOMSetNull(return_value);
	else
	{
		DOM_File* file;
		CALL_FAILED_IF_ERROR(DOM_File::Make(file, full_path.CStr(), FALSE, FALSE, runtime));
		DOM_Object::DOMSetObject(return_value, file);
	}
	return ES_VALUE;
}

class ExtensionGetScreenshotOperation
	: public DOM_Object
	, public OpSecurityCheckCallback
{
private:
	friend class DOM_ExtensionSupport;

	ExtensionGetScreenshotOperation(FramesDocument* source, FramesDocument* target, ES_Object* callback)
		: m_callback(callback)
		, m_source(source)
		, m_target(target)
	{
		OP_ASSERT(m_source);
		OP_ASSERT(m_target);
		OP_ASSERT(m_callback);
	}

	virtual void GCTrace()
	{
		GCMark(m_callback);
		DOM_Object::GCTrace();
	}

	void Start()
	{
		if (OpStatus::IsError(m_this_reference.Protect(GetRuntime(), GetNativeObject())))
			return; // Not much more we can do if we OOM here

		OP_STATUS err = g_secman_instance->CheckSecurity(OpSecurityManager::DOM_EXTENSION_SCREENSHOT,
			                             OpSecurityContext(m_source->GetWindow()->GetGadget()),    // source
		                                 OpSecurityContext(m_target),    // target
		                                 this, NULL);
		if (OpStatus::IsError(err))
			Finish(NULL);

	}

	static void CallCallback(DOM_Object* image_data, ES_Object* callback, DOM_Runtime* runtime)
	{
		ES_Value callback_argv[1]; // ARRAY OK wmaslowski 2011-05-01
		DOM_Object::DOMSetObject(&callback_argv[0], image_data);
		ES_AsyncInterface* async_if = runtime->GetEnvironment()->GetAsyncInterface();
		OpStatus::Ignore(async_if->CallFunction(callback, NULL, ARRAY_SIZE(callback_argv), callback_argv));
	}

	void Finish(DOM_Object* image_data)
	{
		CallCallback(image_data, m_callback, GetRuntime());
		m_this_reference.Unprotect();
	}

	virtual void OnSecurityCheckSuccess(BOOL allowed, ChoicePersistenceType type)
	{
		DOMCanvasImageData* image_data = NULL;
		if (allowed)
		{
			OpBitmap* bitmap = NULL;
			if (OpStatus::IsSuccess(PerformScreenshot(bitmap)))
				image_data = ConvertToImageData(bitmap);
			OP_DELETE(bitmap);
		}
		// If anything failed image data should be NULL here.
		Finish(image_data);
	}

	virtual void OnSecurityCheckError(OP_STATUS error)
	{
		Finish(NULL);
	}

	OP_STATUS PerformScreenshot(OpBitmap*& out_bitmap)
	{
		return m_target->GetWindow()->PerformScreenshot(m_target, m_source, out_bitmap);
	}

	DOMCanvasImageData* ConvertToImageData(OpBitmap* bitmap)
	{
		DOMCanvasImageData* image_data = NULL;
		unsigned int width = bitmap->Width();
		unsigned int height = bitmap->Height();
		RETURN_VALUE_IF_ERROR(DOMCanvasImageData::Make(image_data, GetRuntime(), width, height), NULL);

		UINT32* data = reinterpret_cast<UINT32*>(image_data->GetPixels());
		for (UINT32 line = 0; line < height; ++line)
		{
			UINT32* line_start = data + line * width;
			UINT32* line_end = line_start + width;
			bitmap->GetLineData(line_start, line);
			for (UINT32* pixel = line_start; pixel < line_end; ++pixel)
#ifndef OPERA_BIG_ENDIAN
				// binary BBGGRRAA -> RRGGBBAA
				*pixel = ((0x0FF0000 & *pixel) >> 16) | (*pixel & 0x0FF00FF00) | ((*pixel & 0x0FF) << 16);
#else
				// binary  AARRGGBB -> RRGGBBAA
				*pixel = ((0x0FFFFFF & *pixel) << 8) | ((*pixel & 0xFF000000) >>24);
#endif
		}
		return image_data;
	}

	ES_Object*              m_callback;    ///< Stores the callback argument of getScreenshot call so it doesn't get lost when we suspend.
	FramesDocument*         m_source;
	FramesDocument*         m_target;
	ES_ObjectReference      m_this_reference;
};

int
DOM_ExtensionSupport::GetScreenshot(FramesDocument* target, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime *origining_runtime)
{
	DOM_CHECK_ARGUMENTS("f");
	if (!GetGadget())
		return ES_FAILED;
	if (!target)
		ExtensionGetScreenshotOperation::CallCallback(NULL, argv[0].value.object, origining_runtime);
	else
	{
		ExtensionGetScreenshotOperation* task;
		CALL_FAILED_IF_ERROR(DOM_Object::DOMSetObjectRuntime(task = OP_NEW(ExtensionGetScreenshotOperation, (GetBackground()->GetFramesDocument(), target, argv[0].value.object)), origining_runtime));
		task->Start();
	}
	return ES_FAILED;
}

#endif // EXTENSION_SUPPORT
