/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/extensions/domextensionmenuevent.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domwebworkers/domcrossmessage.h"
#include "modules/dom/src/extensions/domextensionsupport.h"
#include "modules/dom/src/extensions/domextensionscope.h"
#include "modules/dom/src/extensions/domextension_userjs.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dochand/winman.h"
#include "modules/doc/frm_doc.h"


DOM_ExtensionMenuEvent_Base::DOM_ExtensionMenuEvent_Base(OpDocumentContext* document_context)
	: m_document_context(document_context)
{
}

DOM_ExtensionMenuEvent_Base::~DOM_ExtensionMenuEvent_Base()
{
	OP_DELETE(m_document_context);
}

/* virtual */ ES_GetState
DOM_ExtensionMenuEvent_Base::GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_mediaType:
		if (m_document_context->HasImage())
			DOMSetString(value, UNI_L("image"));
#ifdef MEDIA_SUPPORT
		else if (m_document_context->HasMedia())
		{
			if (m_document_context->IsVideo())
				DOMSetString(value, UNI_L("video"));
			else
				DOMSetString(value, UNI_L("audio"));
		}
#endif // MEDIA_SUPPORT
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_srcURL:
		DOMSetNull(value);
		if (m_document_context->HasImage())
		{
			OpString tmp;
			TempBuffer* buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(m_document_context->GetImageAddress(&tmp));
			GET_FAILED_IF_ERROR(buffer->Append(tmp.CStr()));
			if (buffer->Length() > 0)
				DOMSetString(value, buffer);
		}
#ifdef MEDIA_SUPPORT
		else if (m_document_context->HasMedia())
		{
			OpString tmp;
			TempBuffer* buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(m_document_context->GetMediaAddress(&tmp));
			GET_FAILED_IF_ERROR(buffer->Append(tmp.CStr()));
			if (buffer->Length() > 0)
				DOMSetString(value, buffer);
		}
#endif // MEDIA_SUPPORT
		return GET_SUCCESS;

	case OP_ATOM_linkURL:
		DOMSetNull(value);
		if (m_document_context->HasLink())
		{
			OpString tmp;
			TempBuffer* buffer = GetEmptyTempBuf();
			GET_FAILED_IF_ERROR(m_document_context->GetLinkData(OpDocumentContext::AddressForUI, &tmp));
			GET_FAILED_IF_ERROR(buffer->Append(tmp.CStr()));
			if (buffer->Length() > 0)
				DOMSetString(value, buffer);
		}
		return GET_SUCCESS;

	case OP_ATOM_isEditable:
		DOMSetBoolean(value, m_document_context->HasEditableText());
		return GET_SUCCESS;
	}
	return DOM_Event::GetName(property_atom, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_ExtensionMenuEvent_Base::PutName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_mediaType:
	case OP_ATOM_srcURL:
	case OP_ATOM_linkURL:
	case OP_ATOM_documentURL:
	case OP_ATOM_pageURL:
	case OP_ATOM_selectionText:
	case OP_ATOM_isEditable:
	case OP_ATOM_srcElement:
		return PUT_READ_ONLY;
	}
	return DOM_Event::PutName(property_atom, value, origining_runtime);
}


DOM_ExtensionMenuEvent_UserJS::DOM_ExtensionMenuEvent_UserJS(OpDocumentContext* document_context, DOM_Node* node)
	: DOM_ExtensionMenuEvent_Base(document_context)
	, m_node(node)
{
}

/* static */ OP_STATUS
DOM_ExtensionMenuEvent_UserJS::Make(DOM_Event*& new_object, OpDocumentContext* context, HTML_Element* element, DOM_Runtime* runtime)
{
	OP_ASSERT(context);
	OP_ASSERT(runtime);

	OpAutoPtr<OpDocumentContext> ctx_cpy(context->CreateCopy());
	RETURN_OOM_IF_NULL(ctx_cpy.get());
	DOM_Node* node = NULL;
	if (element)
		GET_FAILED_IF_ERROR(runtime->GetEnvironment()->ConstructNode(node, element, NULL));
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_object = OP_NEW(DOM_ExtensionMenuEvent_UserJS, (ctx_cpy.get(), node)), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "MenuEvent"));
	ctx_cpy.release();
	return OpStatus::OK;
}

/* virtual */ void
DOM_ExtensionMenuEvent_UserJS::GCTrace()
{
	GCMark(m_node);
	DOM_Event::GCTrace();
}

/* virtual */ ES_GetState
DOM_ExtensionMenuEvent_UserJS::GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_pageURL:
		{
			FramesDocument* top_level_doc = GetEnvironment()->GetFramesDocument()->GetTopDocument();
			DOMSetString(value, top_level_doc->GetURL().GetAttribute(URL::KUniName_With_Fragment, TRUE));
		}
		return GET_SUCCESS;

	case OP_ATOM_documentURL:
		{
			FramesDocument* frm_doc = GetEnvironment()->GetFramesDocument();
			DOMSetString(value, frm_doc->GetURL().GetAttribute(URL::KUniName_With_Fragment, TRUE));
		}
		return GET_SUCCESS;

	case OP_ATOM_selectionText:
		if (m_document_context->HasTextSelection())
		{
			TempBuffer* tmp_buf = GetEmptyTempBuf();
			FramesDocument* frm_doc = GetEnvironment()->GetFramesDocument();

			GET_FAILED_IF_ERROR(tmp_buf->Expand(frm_doc->GetSelectedTextLen() + 1));
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL result =
#endif // DEBUG_ENABLE_OPASSERT
				frm_doc->GetSelectedText(tmp_buf->GetStorage(), tmp_buf->GetCapacity());
			OP_ASSERT(result);

			DOMSetString(value, tmp_buf);
		}
		else
			DOMSetNull(value);
		return GET_SUCCESS;

	case OP_ATOM_srcElement:
		DOMSetObject(value, m_node);
		return GET_SUCCESS;
	}
	return DOM_ExtensionMenuEvent_Base::GetName(property_atom, value, origining_runtime);
}


DOM_ExtensionMenuEvent_Background::DOM_ExtensionMenuEvent_Background(OpDocumentContext* document_context, UINT32 window_id)
	: DOM_ExtensionMenuEvent_Base(document_context)
	, m_window_id(window_id)
	, m_source(NULL)
{
}

/* virtual */ void
DOM_ExtensionMenuEvent_Background::GCTrace()
{
	GCMark(m_source);
	DOM_ExtensionMenuEvent_Base::GCTrace();
}

/* static */ OP_STATUS
DOM_ExtensionMenuEvent_Background::Make(DOM_Event*& new_object, OpDocumentContext* context, const uni_char* document_url, UINT32 window_id, DOM_ExtensionSupport* extension_support, DOM_Runtime* runtime)
{
	OP_ASSERT(context);
	OP_ASSERT(runtime);
	OP_ASSERT(extension_support);
	if (Window* window = g_windowManager->GetWindow(window_id))
	{
		OpAutoPtr<OpDocumentContext> ctx_cpy(context->CreateCopy());
		RETURN_OOM_IF_NULL(ctx_cpy.get());
		RETURN_IF_ERROR(DOMSetObjectRuntime(new_object = OP_NEW(DOM_ExtensionMenuEvent_Background, (ctx_cpy.get(), window_id)), runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "MenuEvent"));
		ctx_cpy.release();

		DOM_ExtensionMenuEvent_Background* evt = static_cast<DOM_ExtensionMenuEvent_Background*>(new_object);

		DOM_MessagePort* source = NULL;
		if (DOM_ExtensionScope* scope = extension_support->GetExtensionGlobalScope(window))
			if (OpStatus::IsSuccess(DOM_ExtensionSupport::GetPortTarget(scope->GetExtension()->GetPort(), source, runtime)))
				evt->m_source = source;

		RETURN_IF_ERROR(evt->m_page_url.Set(window->GetCurrentURL().GetAttribute(URL::KUniName_With_Fragment, TRUE)));
		return evt->m_document_url.Set(document_url);
	}
	else
		return OpStatus::ERR;
}

BOOL DOM_ExtensionMenuEvent_Background::HasAccessToWindow(Window* window)
{
	OP_ASSERT(window);
	BOOL allowed = FALSE;

	// This is not multi process friendly, but this a generic problem of security manager.
	RETURN_VALUE_IF_ERROR(OpSecurityManager::CheckSecurity(OpSecurityManager::DOM_EXTENSION_ALLOW
	                                                     , OpSecurityContext(window->GetCurrentDoc())
	                                                     , OpSecurityContext(window->GetCurrentDoc()->GetURL(), GetFramesDocument()->GetWindow()->GetGadget())
	                                                     , allowed), FALSE);

	return allowed;
}

/* virtual */ ES_GetState
DOM_ExtensionMenuEvent_Background::GetName(OpAtom property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_atom)
	{
	case OP_ATOM_documentURL:
		DOMSetString(value, m_document_url);
		return GET_SUCCESS;

	// Multi process phase 2: 2 getters below will probably have to be
	// reworked as in multi process the APIs will either dissapear or be asynchronous.
	case OP_ATOM_pageURL:
		if (value)
		{
			if (Window* window = g_windowManager->GetWindow(m_window_id))
				DOMSetString(value, window->GetCurrentURL().GetAttribute(URL::KUniName_With_Fragment, TRUE));
		}
		return GET_SUCCESS;

	case OP_ATOM_selectionText:
		DOMSetNull(value); // Default value if there is no selections.
		// The way selection is implemented means it can possibly used to
		// obtain selection even after the event. This is usually not a
		// security issue as it can be done just as well with a UserJS.
		// The main advantage is that we copy selection text only when we have to.
		if (value && m_document_context->HasTextSelection())
		{
			if (Window* window = g_windowManager->GetWindow(m_window_id))
			{
				// Do not allow access to something we would't usually have access.
				if (HasAccessToWindow(window))
				{
					WindowCommander* commander = window->GetWindowCommander();
					OpAutoArray<uni_char> selected_text(commander->GetSelectedText());

					if (selected_text.get())
					{
						TempBuffer* tmp_buf = GetEmptyTempBuf();
						tmp_buf->Append(selected_text.get());
						DOMSetString(value, tmp_buf);
					}
				}
			}
		}
		return GET_SUCCESS;

	case OP_ATOM_srcElement:
		DOMSetUndefined(value);
		return GET_SUCCESS;

	case OP_ATOM_source:
		DOMSetObject(value, m_source);
		return GET_SUCCESS;
	}
	return DOM_ExtensionMenuEvent_Base::GetName(property_atom, value, origining_runtime);
}

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT