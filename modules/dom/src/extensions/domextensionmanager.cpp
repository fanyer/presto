/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef EXTENSION_SUPPORT

#include "modules/dom/src/extensions/domextensionmanager.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/extensions/domextensionpagecontext.h"
#include "modules/content_filter/content_filter.h"
#include "modules/doc/frm_doc.h"
#include "modules/windowcommander/src/generated/g_message_windowcommander_messages.h"

/* static */ OP_STATUS
DOM_ExtensionManager::Init()
{
	OP_ASSERT(g_extension_manager == NULL);
	return Make(g_extension_manager);
}

/* static */ void
DOM_ExtensionManager::Shutdown(DOM_ExtensionManager *manager)
{
	OP_NEW_DBG("DOM_ExtensionManager::Shutdown()", "extensions.dom");
	OP_DBG(("this: %p", manager));

	OP_ASSERT(g_extension_manager && g_extension_manager == manager);
	OP_DELETE(manager);
}

/* static */ OP_STATUS
DOM_ExtensionManager::Make(DOM_ExtensionManager *&new_obj)
{
	new_obj = OP_NEW(DOM_ExtensionManager, ());

	return new_obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}

DOM_ExtensionSupport *
DOM_ExtensionManager::GetExtensionSupport(OpGadget *gadget)
{
	ExtensionBackgroundInfoElement *e;

	if (OpStatus::IsError(m_extensions.GetData(gadget, &e)))
	{
		DOM_ExtensionSupport *support;
		RETURN_VALUE_IF_ERROR(DOM_ExtensionSupport::Make(support, gadget), NULL);
		OpAutoPtr<DOM_ExtensionSupport> support_anchor(support);

		e = OP_NEW(ExtensionBackgroundInfoElement, (support));
		if (!e)
			return NULL;
		support_anchor.release();

		if (OpStatus::IsError(m_extensions.Add(gadget, e)))
		{
			OP_DELETE(e);
			return NULL;
		}

	}

	return e->m_extension_support;
}

OP_STATUS
DOM_ExtensionManager::AddExtensionContext(Window *window, DOM_Runtime *runtime, DOM_ExtensionBackground **extension_background)
{
	if (extension_background)
		*extension_background = NULL;

	OpGadget *gadget = window->GetGadget();
	if (!gadget || !gadget->IsExtension())
		return OpStatus::OK;

	BOOL allowed = FALSE;
	RETURN_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::GADGET_DOM, runtime->GetFramesDocument()->GetURL(), gadget, allowed));

	if (!allowed)
		return OpStatus::OK;

	OP_NEW_DBG("DOM_ExtensionManager::AddExtensionContext()", "extensions.dom");
	OP_DBG(("this: %p gadget: %p", this, gadget));

	if (gadget->IsExtensionBackgroundWindow(window->GetOpWindow()))
	{
		// We're in the background process.

		BOOL new_support = !m_extensions.Contains(gadget);
		DOM_ExtensionSupport *support = GetExtensionSupport(gadget);

		RETURN_OOM_IF_NULL(support);

		DOM_ExtensionBackground *background;
		OP_STATUS status = DOM_ExtensionBackground::Make(background, support, runtime);

		if (OpStatus::IsError(status) && new_support)
		{
			ExtensionBackgroundInfoElement *e = NULL;
			m_extensions.Remove(gadget, &e);
			OP_DELETE(e);
		}
		RETURN_IF_ERROR(status);

		support->SetExtensionBackground(background);

		if (extension_background)
			*extension_background = background;

		OP_DBG(("this: %p background process context: %p", this, support));
	}
	else
	{
		// We're in a regular window that's loading extension stuff (e.g. the popup or options page)

		ExtensionBackgroundInfoElement *e;
		OP_STATUS status = m_extensions.GetData(gadget, &e);

		/* The background process should've been started by now, but it has
		 * happened before so i'll leave both the assert and the RETURN_IF_ERROR
		 * so we can at least get some indication that something is not working
		 * properly, but without crashing. */
		OP_ASSERT(OpStatus::IsSuccess(status));
		RETURN_IF_ERROR(status);

		RETURN_IF_ERROR(e->m_connections.Add(runtime->GetEnvironment()));
		DOM_ExtensionPageContext *context;
		if (OpStatus::IsError(DOM_ExtensionPageContext::Make(context, e->m_extension_support, runtime)))
		{
			e->m_connections.Remove(e->m_connections.GetCount() - 1);
			return status;
		}

		OP_DBG(("this: %p page_context: %p", this, context));
	}

	return OpStatus::OK;
}

void
DOM_ExtensionManager::RemoveExtensionContext(DOM_ExtensionSupport *support)
{
	OP_NEW_DBG("DOM_ExtensionManager::RemoveExtensionContext()", "extensions.dom");
	OP_DBG(("this: %p gadget: %p", this, support->GetGadget()));

	ExtensionBackgroundInfoElement *e;
	RETURN_VOID_IF_ERROR(m_extensions.Remove(support->GetGadget(), &e));

	OpAutoPtr<ExtensionBackgroundInfoElement> e_anchor(e); // Delete the element no matter what.

	if (support->GetBackground())
		support->GetBackground()->BeforeDestroy();

#ifdef URL_FILTER
	if (g_urlfilter)
		g_urlfilter->RemoveExtension(support->GetGadget());
#endif // URL_FILTER

	for (UINT32 i=0, n=e->m_connections.GetCount(); i < n; i++)
	{
		DOM_EnvironmentImpl *environment = e->m_connections.Get(i);

		if (DOM_Object *global = environment->GetWindow())
		{
			ES_Value value;
			ES_Runtime *runtime = environment->GetRuntime();
			OP_BOOLEAN result;

			RETURN_VOID_IF_ERROR(result = runtime->GetName(*global, UNI_L("opera"), &value));
			if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
			{
				ES_Object *opera = value.value.object;
				RETURN_VOID_IF_ERROR(result = runtime->GetName(opera, UNI_L("extension"), &value));
				if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
				{
					DOM_Object *object = DOM_GetHostObject(value.value.object);
					if (object && object->IsA(DOM_TYPE_EXTENSION_PAGE_CONTEXT))
						static_cast<DOM_ExtensionPageContext*>(object)->HandleDisconnect();
				}
			}
		}
	}
}

void
DOM_ExtensionManager::RemoveExtensionContext(DOM_EnvironmentImpl *environment)
{
	OP_NEW_DBG("DOM_ExtensionManager::RemoveExtensionContext()", "extensions.dom");

	/* If the environment has set up a connection to the extension background, drop it. */
	if (environment->GetWindow() && environment->GetFramesDocument())
	{
		ES_Value value;
		ES_Runtime *runtime = environment->GetRuntime();
		DOM_Object *global = environment->GetWindow();
		OP_BOOLEAN result;

		RETURN_VOID_IF_ERROR(result = runtime->GetName(*global, UNI_L("opera"), &value));
		if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		{
			ES_Object *opera = value.value.object;
			RETURN_VOID_IF_ERROR(result = runtime->GetName(opera, UNI_L("extension"), &value));
			if (result == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
			{
				DOM_Object *object = DOM_GetHostObject(value.value.object);
				if (object)
				{
					if (object->IsA(DOM_TYPE_EXTENSION_PAGE_CONTEXT))
					{
						OP_DBG(("this: %p page_context: %p", this, object));

						DOM_ExtensionPageContext *context = static_cast<DOM_ExtensionPageContext*>(object);

						/* Notify background with a 'disconnect' event. */
						context->BeforeDestroy();

						ExtensionBackgroundInfoElement *e;
						if (OpStatus::IsSuccess(m_extensions.GetData(context->GetExtensionSupport()->GetGadget(), &e)))
						{
							// Deregister this context from the extension background.
							OP_STATUS status = e->m_connections.RemoveByItem(environment);
							OP_ASSERT(OpStatus::IsSuccess(status)); // If the environment has a page context it should've been associated with an extension.
							OpStatus::Ignore(status);
						}
					}
					else if (object->IsA(DOM_TYPE_EXTENSION_BACKGROUND))
					{
						OP_DBG(("this: %p background_context: %p", this, object));

						static_cast<DOM_ExtensionBackground *>(object)->BeforeDestroy();
					}
				}
			}
		}
	}
}

/* virtual */ void
DOM_ExtensionManager::HandleKeyData(const void *key, void *data)
{
	OP_DELETE(static_cast<ExtensionBackgroundInfoElement *>(data));
}

DOM_ExtensionManager::~DOM_ExtensionManager()
{
	OP_NEW_DBG("DOM_ExtensionManager::~DOM_ExtensionManager()", "extensions.dom");
	OP_DBG(("this: %p", this));

	m_extensions.ForEach(this); // Delete all the ExtensionBackgroundInfoElements.
}

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
OP_STATUS
DOM_ExtensionManager::AppendExtensionMenu(OpWindowcommanderMessages_MessageSet::PopupMenuRequest::MenuItemList& menu_items, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	OpHashIterator* it = m_extensions.GetIterator();
	RETURN_OOM_IF_NULL(it);
	OP_STATUS status = it->First();
	while (OpStatus::IsSuccess(status))
	{
		ExtensionBackgroundInfoElement* el = static_cast<ExtensionBackgroundInfoElement*>(it->GetData());
		BOOL allowed;
		RETURN_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_EXTENSION_ALLOW
		                                               , OpSecurityContext(document)
		                                               , OpSecurityContext(document->GetURL(), el->m_extension_support->GetGadget())
		                                               , allowed));

		if (allowed)
			if (DOM_ExtensionBackground* background = el->m_extension_support->GetBackground())
				if (DOM_ExtensionContexts* contexts = background->GetContexts())
					if (DOM_ExtensionMenuContext* menu_context = contexts->GetMenuContext())
						if (OpStatus::IsError(status = menu_context->AppendContextMenu(menu_items, document_context, document, element)))
							break;

		status = it->Next();
	}
	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);
	OP_DELETE(it);
	return OpStatus::OK;
}
#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#endif // EXTENSION_SUPPORT
