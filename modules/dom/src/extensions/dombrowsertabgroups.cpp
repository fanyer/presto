/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowsertabgroups.h"
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/dombrowsertabgroupevent.h"
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/domtabsapihelper.h"
#include "modules/dom/src/extensions/domtabapicache.h"
#include "modules/dom/src/domcore/node.h"

OP_STATUS
DOM_BrowserTabGroupManager::RegisterTabsListener()
{
	if (m_window_id != 0)
		return OpStatus::OK; // Only global tab groups receive events.

	if (!m_has_registered_listener)
	{
		RETURN_IF_ERROR(g_windowCommanderManager->GetTabAPIListener()->OnRegisterWindowTabActionListener(&m_notifications));
		m_has_registered_listener = TRUE;
	}
	return OpStatus::OK;
}

/* virtual */ void
DOM_BrowserTabGroupManager::ExtensionTabGroupNotifications::OnTabGroupAction(ActionType action, OpTabAPIListener::TabAPIItemId tab_group_id, ActionData* data)
{
	const uni_char* event_name;
	DOM_EventType event_type;

	switch (action)
	{
	case ACTION_CREATE:
		event_name = UNI_L("create");
		event_type = ONCREATE;
		break;
	case ACTION_MOVE:
		event_name = UNI_L("move");
		event_type = ONMOVE;
		break;
	case ACTION_CLOSE:
		event_name = UNI_L("close");
		event_type = ONCLOSE;
		break;
	default:
		OP_ASSERT(FALSE);
	case ACTION_UPDATE:
		return; // Tab-group update is ignored for now.
	}

	DOM_Runtime* runtime = m_owner->GetRuntime();

	DOM_BrowserTabGroupEvent* evt;
	DOM_BrowserTabGroupEvent::Make(evt, m_owner->m_extension_support, tab_group_id, runtime);

	if (action == ACTION_MOVE)
	{
		OP_ASSERT(data);
		evt->SetPreviousWindow(data->move_data.previous_parent_id, data->move_data.position);
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

/* static */ OP_STATUS
DOM_BrowserTabGroupManager::Make(DOM_BrowserTabGroupManager*& new_obj, DOM_ExtensionSupport* extension_support, unsigned int window_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(extension_support);
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserTabGroupManager, (extension_support, window_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_GROUP_MANAGER_PROTOTYPE), "BrowserTabGroupManager");
}

/* virtual */
DOM_BrowserTabGroupManager::~DOM_BrowserTabGroupManager()
{
	if (m_has_registered_listener)
		OpStatus::Ignore(g_windowCommanderManager->GetTabAPIListener()->OnUnRegisterWindowTabActionListener(&m_notifications));
}

/* static */ int
DOM_BrowserTabGroupManager::create(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab_groups, DOM_TYPE_BROWSER_TAB_GROUP_MANAGER, DOM_BrowserTabGroupManager);

	DOM_TabsApiContainerCreateHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o|OO");
		ES_Object* initial_tab_group_content = (argc > 0 && argv[0].type == VALUE_OBJECT) ? argv[0].value.object : NULL;
		ES_Object* properties = (argc > 1 && argv[1].type == VALUE_OBJECT) ? argv[1].value.object : NULL;
		ES_Object* before_what = (argc > 2 && argv[2].type == VALUE_OBJECT) ? argv[2].value.object : NULL;

		CALL_FAILED_IF_ERROR(DOM_TabsApiContainerCreateHelper::Make(call_helper, origining_runtime));
		call_helper->RequestCreateTabGroup(initial_tab_group_content, properties, tab_groups->GetWindowId(), before_what);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiContainerCreateHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		DOM_BrowserTabGroup* new_tab_group;
		CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateTabGroup(new_tab_group, tab_groups->m_extension_support, call_helper->GetResult().value.created_tab_group_id, origining_runtime));
		DOMSetObject(return_value, new_tab_group);
		return ES_VALUE;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserTabGroupManager::getAll(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab_groups, DOM_TYPE_BROWSER_TAB_GROUP_MANAGER, DOM_BrowserTabGroupManager);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));

		if (tab_groups->IsWindowTabGroupsCollection())
			call_helper->QueryWindow(tab_groups->m_window_id, TRUE);
		else
		{
			OP_ASSERT(tab_groups->IsGlobalTabGroupsCollection());
			call_helper->QueryAllTabGroups();
		}
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		unsigned int group_count = 0;
		if (OpStatus::IsSuccess(call_helper->GetStatus()))
			group_count = call_helper->GetResult().tab_groups.GetCount();
		ES_Object* retval;
		CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&retval, group_count));

		for (unsigned int i = 0; i < group_count; ++i)
		{
			DOM_BrowserTabGroup* group;
			CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateTabGroup(group, tab_groups->m_extension_support, call_helper->GetResult().tab_groups.Get(i), origining_runtime));
			ES_Value group_val;
			DOMSetObject(&group_val, group);
			CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(retval, i, group_val));
		}
		DOMSetObject(return_value, retval);
		return ES_VALUE;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_BrowserTabGroupManager::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime* >(origining_runtime));
	if (result != GET_FAILED)
		return result;

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_BrowserTabGroupManager::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
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
DOM_BrowserTabGroupManager::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("oncreate");
	enumerator->AddPropertyL("onclose");
	enumerator->AddPropertyL("onupdate");
	enumerator->AddPropertyL("onmove");

	return GET_SUCCESS;
}

/* virtual */ void
DOM_BrowserTabGroupManager::GCTrace()
{
	GCMark(FetchEventTarget());
}

/* static */ int
DOM_BrowserTabGroupManager::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(tab_groups, DOM_TYPE_BROWSER_TAB_GROUP_MANAGER, DOM_BrowserTabGroupManager);

	CALL_FAILED_IF_ERROR(tab_groups->RegisterTabsListener());
	return DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
}

void
DOM_BrowserTabGroupManager::BeforeDestroy()
{
	if (m_has_registered_listener)
	{
		OpStatus::Ignore(g_windowCommanderManager->GetTabAPIListener()->OnUnRegisterWindowTabActionListener(&m_notifications));
		m_has_registered_listener = FALSE;
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_BrowserTabGroupManager)
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabGroupManager, DOM_BrowserTabGroupManager::create, "create", "[(#BrowserTab|{url:s,title:s,locked:b,private:b})]?{collapsed:b}-")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabGroupManager, DOM_BrowserTabGroupManager::getAll, "getAll", "")
DOM_FUNCTIONS_END(DOM_BrowserTabGroupManager)

DOM_FUNCTIONS_WITH_DATA_START(DOM_BrowserTabGroupManager)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_BrowserTabGroupManager, DOM_BrowserTabGroupManager::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_BrowserTabGroupManager, DOM_BrowserTabGroupManager::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_BrowserTabGroupManager)

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
