/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/dombrowserwindows.h"
#include "modules/dom/src/extensions/dombrowsertabs.h"
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowsertabgroups.h"
#include "modules/dom/src/extensions/domtabsapihelper.h"
#include "modules/dom/src/extensions/domtabapicache.h"
#include "modules/dom/src/extensions/domextension_background.h"

/* static */ OP_STATUS
DOM_BrowserWindow::Make(DOM_BrowserWindow*& new_obj, DOM_ExtensionSupport* extension_support, OpTabAPIListener::TabAPIItemId window_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(window_id);
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserWindow, (window_id, extension_support)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_WINDOW_PROTOTYPE), "BrowserWindow"));
	return OpStatus::OK;
}

DOM_BrowserTabManager* DOM_BrowserWindow::GetTabs()
{
	if (m_tabs)
		return m_tabs;

	OP_STATUS err = DOM_BrowserTabManager::MakeWindowTabs(m_tabs, m_extension_support, static_cast<DOM_Runtime*>(GetRuntime()), GetWindowId());
	OP_ASSERT(OpStatus::IsSuccess(err) || OpStatus::IsMemoryError(err));
	RETURN_VALUE_IF_ERROR(err, NULL);
	return m_tabs;
}

DOM_BrowserTabGroupManager* DOM_BrowserWindow::GetTabGroups()
{
	if (m_tab_groups)
		return m_tab_groups;

	OP_STATUS err = DOM_BrowserTabGroupManager::Make(m_tab_groups, m_extension_support, GetWindowId(), static_cast<DOM_Runtime*>(GetRuntime()));
	OP_ASSERT(OpStatus::IsSuccess(err) || OpStatus::IsMemoryError(err));
	RETURN_VALUE_IF_ERROR(err, NULL);
	return m_tab_groups;
}

/* static */ int
DOM_BrowserWindow::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_BROWSER_WINDOW, DOM_BrowserWindow);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestWindowClose(window->GetWindowId());
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
DOM_BrowserWindow::update(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_BROWSER_WINDOW, DOM_BrowserWindow);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o");

		OpRect rect;
		window->GetRectProperties(origining_runtime, argv[0].value.object, rect);

		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestWindowMove(window->GetWindowId(), rect);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		return ES_FAILED;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

void
DOM_BrowserWindow::GetRectProperties(DOM_Runtime* origining_runtime, ES_Object* properties, OpRect& out_rect)
{
	out_rect.height = OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION;
	out_rect.width = OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION;
	out_rect.x = OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION;
	out_rect.y = OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION;
	if (properties)
	{
		out_rect.x = TruncateDoubleToInt(DOMGetDictionaryNumber(properties, UNI_L("left"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
		out_rect.y = TruncateDoubleToInt(DOMGetDictionaryNumber(properties, UNI_L("top"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
		out_rect.height = TruncateDoubleToInt(DOMGetDictionaryNumber(properties, UNI_L("height"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
		out_rect.width = TruncateDoubleToInt(DOMGetDictionaryNumber(properties, UNI_L("width"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
	}
}

/* static */ int
DOM_BrowserWindow::focus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(window, DOM_TYPE_BROWSER_WINDOW, DOM_BrowserWindow);

	DOM_TabsApiFocusHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiFocusHelper::Make(call_helper, origining_runtime));
		call_helper->RequestWindowFocus(window->GetWindowId());
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiFocusHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		return ES_FAILED;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

ES_GetState
DOM_BrowserWindow::GetWindowInfo(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (!value)
		return GET_SUCCESS;

	if (GetWindowId() == 0)
		return GET_SUCCESS;


	DOM_TabsApiHelper* call_helper;
	if (!restart_object)
	{
		GET_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, static_cast<DOM_Runtime*>(origining_runtime)));
		call_helper->QueryWindow(GetWindowId(), FALSE);
	}
	else
		call_helper = DOM_HOSTOBJECT(restart_object, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		if (property_name == OP_ATOM_closed)
		{
			DOMSetBoolean(value, OpStatus::IsError(call_helper->GetStatus()));
			return GET_SUCCESS;
		}
		else
			GET_FAILED_IF_ERROR(call_helper->GetStatus());

		switch (property_name)
		{
		case OP_ATOM_focused:
			DOMSetBoolean(value, call_helper->GetResult().value.query_window.has_focus);
			break;

		case OP_ATOM_height:
			DOMSetNumber(value, call_helper->GetResult().value.query_window.height);
			break;

		case OP_ATOM_width:
			DOMSetNumber(value, call_helper->GetResult().value.query_window.width);
			break;

		case OP_ATOM_top:
			DOMSetNumber(value, call_helper->GetResult().value.query_window.top);
			break;

		case OP_ATOM_left:
			DOMSetNumber(value, call_helper->GetResult().value.query_window.left);
			break;

		case OP_ATOM_private:
			DOMSetBoolean(value, call_helper->GetResult().value.query_window.is_private);
			break;
		default:
			OP_ASSERT(!"Unexpected property");
		}
		return GET_SUCCESS;
	}
	else
		return call_helper->BlockGet(value, origining_runtime);
}

/* virtual */ ES_GetState
DOM_BrowserWindow::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	switch (property_name)
	{
	case OP_ATOM_top:
	case OP_ATOM_left:
	case OP_ATOM_width:
	case OP_ATOM_height:
	case OP_ATOM_focused:
	case OP_ATOM_private:
	case OP_ATOM_closed:
		return GetWindowInfo(property_name, value, origining_runtime, restart_object);

	default:
		return DOM_Object::GetNameRestart(property_name, value, origining_runtime, restart_object);
	}
}

/* virtual */ ES_GetState
DOM_BrowserWindow::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_top:
	case OP_ATOM_left:
	case OP_ATOM_width:
	case OP_ATOM_height:
	case OP_ATOM_focused:
	case OP_ATOM_private:
	case OP_ATOM_closed:
		return GetWindowInfo(property_name, value, origining_runtime, NULL);

	case OP_ATOM_tabs:
		if (value) // GetTabs is not a simple getter so we avoid if for HasName.
			DOMSetObject(value, GetTabs());
		return GET_SUCCESS;

	case OP_ATOM_tabGroups:
		if (value)
			DOMSetObject(value, GetTabGroups());
		return GET_SUCCESS;

	case OP_ATOM_id:
		if (GetWindowId() > 0)
			DOMSetNumber(value, GetWindowId());
		return GET_SUCCESS;
	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_PutState
DOM_BrowserWindow::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_focused:
	case OP_ATOM_id:
	case OP_ATOM_left:
	case OP_ATOM_top:
	case OP_ATOM_width:
	case OP_ATOM_height:
	case OP_ATOM_tabs:
	case OP_ATOM_tabGroups:
	case OP_ATOM_closed:
		return PUT_READ_ONLY;
	default:
		return DOM_TabApiCachedObject::PutName(property_name, value, origining_runtime);
	}
}

/* virtual */ void
DOM_BrowserWindow::GCTrace()
{
	GCMark(m_tabs);
	GCMark(m_tab_groups);
}

DOM_BrowserWindow::~DOM_BrowserWindow()
{
	OP_NEW_DBG("DOM_BrowserWindow::~DOM_BrowserWindow()", "extensions.dom");
	OP_DBG(("this: %p window_id: %d", this, GetWindowId()));
}

/* static */ int
DOM_BrowserWindow::insert(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_TabsApiInsertHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o|O");
		ES_Object* insert_what = argv[0].value.object;
		ES_Object* before_what = (argc > 1 && argv[1].type == VALUE_OBJECT) ? argv[1].value.object : NULL;

		CALL_FAILED_IF_ERROR(DOM_TabsApiInsertHelper::Make(call_helper, origining_runtime));
		call_helper->RequestInsert(insert_what, this_object->GetNativeObject(), before_what);
	}
	else
		call_helper = DOM_VALUE2OBJECT(*return_value, DOM_TabsApiInsertHelper);

	if (call_helper->IsFinished())
	{
		TAB_API_CALL_FAILED_IF_ERROR(call_helper->GetStatus());
		return ES_FAILED;
	}
	else
		return call_helper->BlockCall(return_value, origining_runtime);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_BrowserWindow)
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindow, DOM_BrowserWindow::close, "close", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindow, DOM_BrowserWindow::update, "update", "{focused:b,private:b,width:n,height:n,top:n,left:n}-")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindow, DOM_BrowserWindow::focus, "focus", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserWindow, DOM_BrowserWindow::insert, "insert", "--")
DOM_FUNCTIONS_END(DOM_BrowserWindow)

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
