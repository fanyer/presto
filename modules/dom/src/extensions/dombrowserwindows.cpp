/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowserwindows.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/dombrowserwindowevent.h"
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/domextensionscope.h"
#include "modules/dom/src/extensions/domextension_userjs.h"
#include "modules/dom/src/extensions/domextension_background.h"
#include "modules/dochand/winman.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/dom/src/extensions/domtabsapihelper.h"
#include "modules/dom/src/extensions/domtabapicache.h"

/* virtual */ void
DOM_BrowserWindowManager::ExtensionWindowNotifications::OnBrowserWindowAction(ActionType action, OpTabAPIListener::TabAPIItemId window_id, ActionData* data)
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
		return; // Window update is ignored for now.
	}

	DOM_Runtime* runtime = m_owner->GetRuntime();

	DOM_BrowserWindowEvent* evt;
	DOM_BrowserWindowEvent::Make(evt, m_owner->m_extension_support, window_id, runtime);

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
DOM_BrowserWindowManager::Make(DOM_BrowserWindowManager*& new_obj, DOM_ExtensionSupport* extension_support,  DOM_Runtime* origining_runtime)
{
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserWindowManager, (extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_WINDOW_MANAGER_PROTOTYPE), "BrowserWindowManager");
}

/* private */ OP_STATUS
DOM_BrowserWindowManager::RegisterWindowListener()
{
	if (!m_has_registered_listener)
	{
		RETURN_IF_ERROR(g_windowCommanderManager->GetTabAPIListener()->OnRegisterWindowTabActionListener(&m_notifications));
		m_has_registered_listener = TRUE;
	}

	return OpStatus::OK;
}

/* static */ int
DOM_BrowserWindowManager::accessEventListener(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime, int data)
{
	DOM_THIS_OBJECT(windows, DOM_TYPE_BROWSER_WINDOW_MANAGER, DOM_BrowserWindowManager);

	if (data == 0 || data == 2)
		CALL_FAILED_IF_ERROR(windows->RegisterWindowListener());
	return DOM_Node::accessEventListener(this_object, argv, argc, return_value, origining_runtime, data);
}

/* static */ int
DOM_BrowserWindowManager::getAll(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(windows, DOM_TYPE_BROWSER_WINDOW_MANAGER, DOM_BrowserWindowManager);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->QueryAllWindows();
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		unsigned int browser_window_count = 0;
		if (OpStatus::IsSuccess(call_helper->GetStatus()))
			browser_window_count = call_helper->GetResult().browser_windows.GetCount();
		ES_Object* retval;
		CALL_FAILED_IF_ERROR(origining_runtime->CreateNativeArrayObject(&retval, browser_window_count));

		for (unsigned int i = 0; i < browser_window_count; ++i)
		{
			DOM_BrowserWindow* new_window;
			CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateWindow(new_window, windows->m_extension_support, call_helper->GetResult().browser_windows.Get(i), origining_runtime));
			ES_Value new_window_val;
			DOMSetObject(&new_window_val, new_window);
			CALL_FAILED_IF_ERROR(origining_runtime->PutIndex(retval, i, new_window_val));
		}
		DOMSetObject(return_value, retval);
		return ES_VALUE;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserWindowManager::getLastFocused(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(windows, DOM_TYPE_BROWSER_WINDOW_MANAGER, DOM_BrowserWindowManager);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->QueryWindow(0, FALSE);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		CALL_FAILED_IF_ERROR(call_helper->GetStatus());

		DOM_BrowserWindow* wnd;
		CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateWindow(wnd, windows->m_extension_support, call_helper->GetResult().value.query_window.id, origining_runtime));
		DOMSetObject(return_value, wnd);
		return ES_VALUE;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* static */ int
DOM_BrowserWindowManager::create(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(windows, DOM_TYPE_BROWSER_WINDOW_MANAGER, DOM_BrowserWindowManager);

	DOM_TabsApiContainerCreateHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("|OO");
		ES_Object* initial_win_content = (argc > 0 && argv[0].type == VALUE_OBJECT) ? argv[0].value.object : NULL;
		ES_Object* properties = (argc > 1 && argv[1].type == VALUE_OBJECT) ? argv[1].value.object : NULL;

		CALL_FAILED_IF_ERROR(DOM_TabsApiContainerCreateHelper::Make(call_helper, origining_runtime));
		call_helper->RequestCreateWindow(initial_win_content, properties);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiContainerCreateHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		DOM_BrowserWindow* wnd;
		CALL_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateWindow(wnd, windows->m_extension_support, call_helper->GetResult().value.created_window_id, origining_runtime));
		DOMSetObject(return_value, wnd);
		return ES_VALUE;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_BrowserWindowManager::FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime* origining_runtime)
{
	ES_GetState result = DOM_Object::FetchPropertiesL(enumerator, origining_runtime);
	if (result != GET_SUCCESS)
		return result;

	enumerator->AddPropertyL("oncreate");
	enumerator->AddPropertyL("onclose");
	enumerator->AddPropertyL("onblur");
	enumerator->AddPropertyL("onfocus");
	enumerator->AddPropertyL("onupdate");

	return GET_SUCCESS;
}

/* virtual */ ES_GetState
DOM_BrowserWindowManager::GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	ES_GetState result = GetEventProperty(property_name, value, static_cast<DOM_Runtime* >(origining_runtime));
	if (result != GET_FAILED)
		return result;

	return DOM_Object::GetName(property_name, property_code, value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_BrowserWindowManager::PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime)
{
	if (property_name[0] == 'o' && property_name[1] == 'n')
	{
		PUT_FAILED_IF_ERROR(RegisterWindowListener());
		ES_PutState state = PutEventProperty(property_name, value, static_cast<DOM_Runtime* >(origining_runtime));
		if (state != PUT_FAILED)
			return state;
	}
	return DOM_Object::PutName(property_name, property_code, value, origining_runtime);
}

/* virtual */ void
DOM_BrowserWindowManager::GCTrace()
{
	GCMark(FetchEventTarget());
}

/* virtual */
DOM_BrowserWindowManager::~DOM_BrowserWindowManager()
{
	OP_NEW_DBG("DOM_BrowserWindowManager::~DOM_BrowserWindowManager()", "extensions.dom");
	OP_DBG(("this: %p", this));

	OP_ASSERT(!m_has_registered_listener);
}

void
DOM_BrowserWindowManager::BeforeDestroy()
{
	if (m_has_registered_listener)
	{
		OpStatus::Ignore(g_windowCommanderManager->GetTabAPIListener()->OnUnRegisterWindowTabActionListener(&m_notifications));
		m_has_registered_listener = FALSE;
	}
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_BrowserWindowManager)
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindowManager, DOM_BrowserWindowManager::getAll, "getAll", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindowManager, DOM_BrowserWindowManager::create, "create", "?[(#BrowserTabGroup|#BrowserTab|{url:s,title:s,locked:b,private:b})]?{focused:b,private:b,width:n,height:n,top:n,left:n}-")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindowManager, DOM_BrowserWindowManager::getLastFocused, "getLastFocused", NULL)
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindowManager, DOM_BrowserWindowManager::getLastFocused, "getFocused", NULL) // for backwards compatibility.
DOM_FUNCTIONS_END(DOM_BrowserWindowManager)

DOM_FUNCTIONS_WITH_DATA_START(DOM_BrowserWindowManager)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_BrowserWindowManager, DOM_BrowserWindowManager::accessEventListener, 0, "addEventListener", "s-b-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_BrowserWindowManager, DOM_BrowserWindowManager::accessEventListener, 1, "removeEventListener", "s-b-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_BrowserWindowManager)

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
