/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowsertabs.h"
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowserwindows.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/domextensionscope.h"
#include "modules/dom/src/extensions/domextension_userjs.h"
#include "modules/dochand/winman.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/extensions/domtabsapihelper.h"
#include "modules/dom/src/extensions/domtabapicache.h"
#include "modules/gadgets/OpGadget.h"
#include "modules/dom/src/extensions/dombrowsertabevent.h"

/* static */ OP_STATUS
DOM_BrowserTabManager::MakeGlobalTabs(DOM_BrowserTabManager*& new_obj, DOM_ExtensionSupport* extension_support, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(extension_support);
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserTabManager, (extension_support, 0, 0)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_MANAGER_PROTOTYPE), "BrowserTabManager");
}

/* static */ OP_STATUS
DOM_BrowserTabManager::MakeWindowTabs(DOM_BrowserTabManager*& new_obj, DOM_ExtensionSupport* extension_support, DOM_Runtime* origining_runtime, OpTabAPIListener::TabAPIItemId window_id)
{
	OP_ASSERT(extension_support);
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserTabManager, (extension_support, window_id, 0)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_MANAGER_PROTOTYPE), "BrowserTabManager");
}

/* static */ OP_STATUS
DOM_BrowserTabManager::MakeTabGroupTabs(DOM_BrowserTabManager*& new_obj, DOM_ExtensionSupport* extension_support, DOM_Runtime* origining_runtime, OpTabAPIListener::TabAPIItemId tab_group_id)
{
	OP_ASSERT(extension_support);
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserTabManager, (extension_support, 0, tab_group_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_MANAGER_PROTOTYPE), "BrowserTabManager");
}

/* private */ OP_STATUS
DOM_BrowserTabManager::RegisterTabsListener()
{
	if (!IsGlobalTabCollection())
		return OpStatus::OK; // only global tabs receive events.

	if (!m_has_registered_listener)
	{
		RETURN_IF_ERROR(g_windowCommanderManager->GetTabAPIListener()->OnRegisterWindowTabActionListener(&m_notifications));
		m_has_registered_listener = TRUE;
	}
	return OpStatus::OK;
}

/* virtual */ void
DOM_BrowserTabManager::ExtensionTabNotifications::OnTabAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_id, unsigned int tab_window_id, ExtensionWindowTabActionListener::ActionData* data)
{
	const uni_char* event_name;
	DOM_EventType event_type;

	switch (action)
	{
	case ACTION_CREATE:
		event_name = UNI_L("create");
		event_type = ONCREATE;
		break;
	case ACTION_CLOSE:
		event_name = UNI_L("close");
		event_type = ONCLOSE;
		break;
	case ACTION_MOVE:
	case ACTION_MOVE_FROM_GROUP:
		event_name = UNI_L("move");
		event_type = ONMOVE;
		break;
	case ACTION_FOCUS:
		event_name = UNI_L("focus");
		event_type = ONFOCUS;
		break;
	case ACTION_BLUR:
		event_name = UNI_L("blur");
		event_type = ONBLUR;
		break;
	default:
		OP_ASSERT(FALSE);
	case ACTION_UPDATE:
		return; // Tab update is ignored for now.
	}


	DOM_Runtime* runtime = m_owner->GetRuntime();

	DOM_BrowserTabEvent* evt;

	DOM_BrowserTabEvent::Make(evt, m_owner->m_extension_support, tab_id, tab_window_id, runtime);


	if (action == ACTION_MOVE)
	{
		OP_ASSERT(data);
		evt->SetPreviousWindow(data->move_data.previous_parent_id, data->move_data.position);
	}
	else if (action == ACTION_MOVE_FROM_GROUP)
	{
		OP_ASSERT(data);
		evt->SetPreviousTabGroup(data->move_data.previous_parent_id, data->move_data.position);
	}
	ES_Value argv[3]; /* ARRAY OK 2011-10-06 wmaslowski */
	ES_Value value;

	DOMSetString(&argv[0], event_name);
	DOMSetBoolean(&argv[1], FALSE);
	DOMSetBoolean(&argv[2], FALSE);

	int status = DOM_Event::initEvent(evt, argv, ARRAY_SIZE(argv), &value, runtime);
	if (status != ES_FAILED)
		return;

	evt->InitEvent(event_type, m_owner);
	evt->SetCurrentTarget(m_owner);
	evt->SetSynthetic();
	evt->SetEventPhase(ES_PHASE_ANY);

	m_owner->GetEnvironment()->SendEvent(evt);
}

/* static */ int
DOM_BrowserTabManager::getSelected(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tabs, DOM_TYPE_BROWSER_TAB_MANAGER, DOM_BrowserTabManager);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		if (tabs->IsTabGroupTabCollection())
			call_helper->QueryTabGroup(tabs->m_tab_group_id, TRUE);
		else
			call_helper->QueryWindow(tabs->m_window_id, TRUE);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());

		OpTabAPIListener::TabAPIItemId selected_tab_id;
		unsigned int selected_tab_window_id;
		if (tabs->IsTabGroupTabCollection())
		{
			selected_tab_id = call_helper->GetResult().value.query_tab_group.selected_tab_id;
			selected_tab_window_id = call_helper->GetResult().value.query_tab_group.selected_tab_window_id;
		}
		else
		{
			selected_tab_id = call_helper->GetResult().value.query_window.selected_tab_id;
			selected_tab_window_id = call_helper->GetResult().value.query_window.selected_tab_window_id;
		}

		DOM_BrowserTab* tab;
		CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateTab(tab, tabs->m_extension_support, selected_tab_id, selected_tab_window_id, origining_runtime));
		DOMSetObject(return_value, tab);
		return ES_VALUE;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

int DOM_BrowserTabManager::MakeTabArray(ES_Value* return_value, const OpTabAPIListener::TabAPIItemIdCollection* tabs, const OpTabAPIListener::TabWindowIdCollection* tab_windows, DOM_Runtime* origining_runtime)
{
	unsigned int tab_count = 0;
	if (tabs)
	{
		OP_ASSERT(tab_windows);
		OP_ASSERT(tabs->GetCount() == tab_windows->GetCount());
		tab_count = tabs->GetCount();
	}
	ES_Object* retval;
	CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&retval, tab_count));
	for (unsigned int i = 0; i < tab_count; ++i)
	{
		DOM_BrowserTab* tab;
		CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateTab(tab, m_extension_support, tabs->Get(i), tab_windows->Get(i), origining_runtime));
		ES_Value tab_val;
		DOMSetObject(&tab_val, tab);
		CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(retval, i, tab_val));
	}
	DOMSetObject(return_value, retval);
	return ES_VALUE;
}

int
DOM_BrowserTabManager::getAll(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tabs, DOM_TYPE_BROWSER_TAB_MANAGER, DOM_BrowserTabManager);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		if (tabs->IsTabGroupTabCollection())
			call_helper->QueryTabGroup(tabs->m_tab_group_id, TRUE);
		else if (tabs->IsWindowTabCollection())
			call_helper->QueryWindow(tabs->m_window_id, TRUE);
		else
		{
			OP_ASSERT(tabs->IsGlobalTabCollection());
			call_helper->QueryAllTabs();
		}
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		if (OpStatus::IsError(call_helper->GetStatus()))
			return tabs->MakeTabArray(return_value,NULL, NULL, origining_runtime);
		else
			return tabs->MakeTabArray(return_value, &call_helper->GetResult().tabs, &call_helper->GetResult().tab_windows, origining_runtime);
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserTabManager::create(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tabs, DOM_TYPE_BROWSER_TAB_MANAGER, DOM_BrowserTabManager);

	DOM_TabsApiTabCreateHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("|OO");
		ES_Object* properties = (argc > 0 && argv[0].type == VALUE_OBJECT) ? argv[0].value.object : NULL;
		ES_Object* before_what = (argc > 1 && argv[1].type == VALUE_OBJECT) ? argv[1].value.object : NULL;

		CALL_FAILED_IF_ERROR(DOM_TabsApiTabCreateHelper::Make(call_helper, origining_runtime));
		call_helper->RequestCreateTab(properties, tabs->m_window_id, tabs->m_tab_group_id, before_what);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiTabCreateHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		DOM_BrowserTab* new_tab;
		CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateTab(new_tab, tabs->m_extension_support, call_helper->GetResult().value.created_tab_info.tab_id, call_helper->GetResult().value.created_tab_info.window_id, origining_runtime));
		DOMSetObject(return_value, new_tab);
		return ES_VALUE;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserTabManager::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_BROWSER_TAB_MANAGER);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o");
		DOM_ARGUMENT_OBJECT(tab, 0, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestTabClose(tab->GetTabId());
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		return ES_FAILED;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserTabManager::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(tabs, DOM_TYPE_BROWSER_TAB_MANAGER, DOM_BrowserTabManager);

	CALL_FAILED_IF_ERROR(tabs->RegisterTabsListener());
	return DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
}

/* virtual */ ES_GetState
DOM_BrowserTabManager::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime* >(origining_runtime));
	if (result != GET_FAILED)
		return result;

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}


/* virtual */ ES_PutState
DOM_BrowserTabManager::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		PUT_FAILED_IF_ERROR(RegisterTabsListener());
		ES_PutState state = PutEventProperty(property_name, value, static_cast<DOM_Runtime* >(origining_runtime));
		if (state != PUT_FAILED)
			return state;
	}
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_BrowserTabManager::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime* origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("oncreate");
	enumerator->AddPropertyL("onclose");
	enumerator->AddPropertyL("onblur");
	enumerator->AddPropertyL("onfocus");
	enumerator->AddPropertyL("onupdate");
	enumerator->AddPropertyL("onmove");

	return GET_SUCCESS;
}

/* virtual */ void
DOM_BrowserTabManager::GCTrace()
{
	GCMark(FetchEventTarget());
}

/* virtual */
DOM_BrowserTabManager::~DOM_BrowserTabManager()
{
	OP_NEW_DBG("DOM_BrowserTabManager::~DOM_BrowserTabManager()", "extensions.dom");
	OP_DBG(("this: %p", this));

	OP_ASSERT(!m_has_registered_listener);
}

void
DOM_BrowserTabManager::BeforeDestroy()
{
	if (m_has_registered_listener)
	{
		OpStatus::Ignore(g_windowCommanderManager->GetTabAPIListener()->OnUnRegisterWindowTabActionListener(&m_notifications));
		m_has_registered_listener = FALSE;
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_BrowserTabManager)
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabManager, DOM_BrowserTabManager::create, "create", "?{url:s,title:s,focused:b,locked:b,private:b}-")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabManager, DOM_BrowserTabManager::getAll, "getAll", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabManager, DOM_BrowserTabManager::getSelected, "getSelected", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabManager, DOM_BrowserTabManager::getSelected, "getFocused", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabManager, DOM_BrowserTabManager::close, "close", "")
DOM_FUNCTIONS_END(DOM_BrowserTabManager)

DOM_FUNCTIONS_WITH_DATA_START(DOM_BrowserTabManager)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_BrowserTabManager, DOM_BrowserTabManager::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_BrowserTabManager, DOM_BrowserTabManager::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_BrowserTabManager)

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
