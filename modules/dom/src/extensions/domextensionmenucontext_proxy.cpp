/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT

#include "modules/dom/src/extensions/domextensionmenucontext_proxy.h"
#include "modules/dom/src/extensions/domextensionmenucontext.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dom/src/extensions/domextensioncontexts.h"
#include "modules/dom/src/extensions/domextensionmenuitem.h"
#include "modules/dom/src/extensions/domextensionmenuevent.h"
#include "modules/dom/src/extensions/domextensionmenuitem_proxy.h"

/* static */ OP_STATUS
DOM_ExtensionMenuContextProxy::Make(DOM_ExtensionMenuContextProxy *&new_obj, DOM_ExtensionSupport *extension_support, DOM_Runtime *origining_runtime)
{
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_ExtensionMenuContextProxy, (extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::EXTENSION_MENUCONTEXT_PROXY_PROTOTYPE), "MenuContextProxy");
}

DOM_ExtensionMenuContextProxy::DOM_ExtensionMenuContextProxy(DOM_ExtensionSupport *extension_support)
	: m_extension_support(extension_support)
{
}

DOM_ExtensionMenuContextProxy::~DOM_ExtensionMenuContextProxy()
{
	class Iterator : public OpHashTableForEachListener
	{
		virtual void HandleKeyData(const void* key, void* data)
		{
			OP_ASSERT(data);
			reinterpret_cast<DOM_ExtensionMenuItemProxy*>(data)->OnTopLevelContextDestroyed();
		}
	} iterator;
	m_item_cache.ForEach(&iterator);
}

/* virtual */ ES_GetState
DOM_ExtensionMenuContextProxy::GetName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_GetState res = GetEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != GET_FAILED)
			return res;
	}

	return DOM_Object::GetName(property_name, property_atom, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_ExtensionMenuContextProxy::PutName(const uni_char* property_name, int property_atom, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		ES_PutState res = PutEventProperty(property_name, value, static_cast<DOM_Runtime*>(origining_runtime));
		if (res != PUT_FAILED)
			return res;
	}
	return DOM_Object::PutName(property_name, property_atom, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_ExtensionMenuContextProxy::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime* origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("onclick");
	return GET_SUCCESS;
}

void DOM_ExtensionMenuContextProxy::GCTraceItemCacheIterator::HandleKeyData(const void* key, void* data)
{
	OP_ASSERT(data);
	DOM_ExtensionMenuItemProxy* item = reinterpret_cast<DOM_ExtensionMenuItemProxy*>(data);
	if (item->GetIsSignificant())
		runtime->GCMark(item);
}

/* virtual */ void
DOM_ExtensionMenuContextProxy::GCTrace()
{
	GCTraceItemCacheIterator iterator(GetRuntime());
	m_item_cache.ForEach(&iterator);
	GCMark(FetchEventTarget());
}

DOM_ExtensionMenuItemProxy*
DOM_ExtensionMenuContextProxy::FindOrCreateProxyItem(UINT32 item_id, const uni_char* js_id)
{
	DOM_ExtensionMenuItemProxy* proxy_item = NULL;
	OP_STATUS status = m_item_cache.GetData(item_id, &proxy_item);
	if (OpStatus::IsError(status))
	{
		OP_ASSERT(!proxy_item);

		RETURN_VALUE_IF_ERROR(DOM_ExtensionMenuItemProxy::Make(proxy_item, m_extension_support, this, item_id, js_id, GetRuntime()), NULL);
		OP_ASSERT(proxy_item);
		status = m_item_cache.Add(item_id, proxy_item);
		if (OpStatus::IsError(status))
		{
			// Break the connection between proxy item a container...
			proxy_item->OnTopLevelContextDestroyed();
			// ... and return NULL - we dont want to have items which might be inaccessible later.
			return NULL;
		}
	}
	return proxy_item;
}

void DOM_ExtensionMenuContextProxy::ProxyMenuItemDestroyed(UINT32 item_id)
{
	DOM_ExtensionMenuItemProxy* unused;

	OP_STATUS status = m_item_cache.Remove(item_id, &unused);
	OpStatus::Ignore(status);
	OP_ASSERT(OpStatus::IsSuccess(status));
}

void
DOM_ExtensionMenuContextProxy::OnMenuItemClicked(UINT32 item_id, const uni_char* item_js_id, OpDocumentContext* document_context, FramesDocument* document, HTML_Element* element)
{
	DOM_EventTarget* event_target = FetchEventTarget();
	OP_BOOLEAN status = OpStatus::OK;
	if (event_target && event_target->HasListeners(ONCLICK, NULL, ES_PHASE_ANY)) // no point in creating proxy menu item if we know no one listens for events.
	{
		DOM_ExtensionMenuItemProxy* item_proxy = FindOrCreateProxyItem(item_id, item_js_id);
		if (!item_proxy)
			status = OpStatus::ERR_NO_MEMORY;

		DOM_Event* menu_event;
		OP_BOOLEAN status = DOM_ExtensionMenuEvent_UserJS::Make(menu_event, document_context, element, GetRuntime());
		if (OpStatus::IsSuccess(status))
		{
			menu_event->InitEvent(ONCLICK, item_proxy, NULL, this);
			menu_event->SetCurrentTarget(item_proxy);
			menu_event->SetEventPhase(ES_PHASE_ANY);

			status = GetEnvironment()->SendEvent(menu_event);
		}
	}
	if (OpStatus::IsMemoryError(status))
		g_memory_manager->RaiseCondition(status);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_WITH_DATA_START(DOM_ExtensionMenuContextProxy)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionMenuContextProxy, DOM_Node::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_ExtensionMenuContextProxy, DOM_Node::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_ExtensionMenuContextProxy)

#endif // DOM_EXTENSIONS_CONTEXT_MENU_API_SUPPORT
