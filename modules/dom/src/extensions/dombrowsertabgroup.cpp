/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowsertabs.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dom/src/extensions/domtabapicache.h"
#include "modules/dom/src/extensions/domtabsapihelper.h"
#include "modules/dom/src/extensions/domextension_background.h"

/* static */ OP_STATUS
DOM_BrowserTabGroup::Make(DOM_BrowserTabGroup*& new_obj, DOM_ExtensionSupport* extension_support, unsigned int tab_group_id, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(extension_support);
	OP_ASSERT(tab_group_id);
	return DOMSetObjectRuntime(new_obj = OP_NEW(DOM_BrowserTabGroup, (extension_support, tab_group_id)), origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::BROWSER_TAB_GROUP_PROTOTYPE), "BrowserTabGroup");
}

/* virtual */
DOM_BrowserTabGroup::~DOM_BrowserTabGroup()
{
}

/* static */ int
DOM_BrowserTabGroup::close(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab_group, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestTabGroupClose(tab_group->GetTabGroupId());
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
DOM_BrowserTabGroup::focus(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab_group, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);
	DOM_TabsApiFocusHelper* call_helper;
	if (argc >= 0)
	{
		CALL_FAILED_IF_ERROR(DOM_TabsApiFocusHelper::Make(call_helper, origining_runtime));
		call_helper->RequestTabGroupFocus(tab_group->GetTabGroupId());
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

/* static */ int
DOM_BrowserTabGroup::update(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(tab_group, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);

	DOM_TabsApiHelper* call_helper;
	if (argc >= 0)
	{
		DOM_CHECK_ARGUMENTS("o");
		ES_Object* props = argv[0].value.object;

		CALL_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, origining_runtime));
		call_helper->RequestTabGroupUpdate(tab_group->GetTabGroupId(), props, origining_runtime);
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

/* virtual */ ES_GetState
DOM_BrowserTabGroup::GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_tabs:
		if (value) // GetTabs is not a simple getter so we avoid if for HasName.
			DOMSetObject(value, GetTabs());
		return GET_SUCCESS;

	case OP_ATOM_id:
		DOMSetNumber(value, GetTabGroupId());
		return GET_SUCCESS;

	case OP_ATOM_closed:
	case OP_ATOM_collapsed:
	case OP_ATOM_position:
	case OP_ATOM_browserWindow:
		return GetTabGroupInfo(property_name, value, origining_runtime, NULL);

	default:
		return DOM_Object::GetName(property_name, value, origining_runtime);
	}
}

/* virtual */ ES_GetState
DOM_BrowserTabGroup::GetNameRestart(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	switch (property_name)
	{
	case OP_ATOM_closed:
	case OP_ATOM_collapsed:
	case OP_ATOM_position:
	case OP_ATOM_browserWindow:
		return GetTabGroupInfo(property_name, value, origining_runtime, restart_object);
	default:
		return DOM_Object::GetNameRestart(property_name, value, origining_runtime, restart_object);
	}
}

ES_GetState
DOM_BrowserTabGroup::GetTabGroupInfo(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object)
{
	if (!value)
		return GET_SUCCESS;

	OP_ASSERT(GetTabGroupId());

	DOM_TabsApiHelper* call_helper;
	if (!restart_object)
	{
		GET_FAILED_IF_ERROR(DOM_TabsApiHelper::Make(call_helper, static_cast<DOM_Runtime*>(origining_runtime)));
		call_helper->QueryTabGroup(GetTabGroupId(), FALSE);
	}
	else
		call_helper = DOM_HOSTOBJECT(restart_object, DOM_TabsApiHelper);

	if (call_helper->IsFinished())
	{
		if (property_name != OP_ATOM_closed)
			GET_FAILED_IF_ERROR(call_helper->GetStatus());

		switch (property_name)
		{
		case OP_ATOM_collapsed:
			DOMSetBoolean(value, call_helper->GetResult().value.query_tab_group.is_collapsed);
			break;
		case OP_ATOM_position:
			DOMSetNumber(value, call_helper->GetResult().value.query_tab_group.position);
			break;
		case OP_ATOM_closed:
			if (OpStatus::IsMemoryError(call_helper->GetStatus()))
				return GET_NO_MEMORY;
			// For now let's assume error other than OOM means that there is no such group -> closed == true.
			DOMSetBoolean(value, OpStatus::IsError(call_helper->GetStatus()));
			break;
		case OP_ATOM_browserWindow:
			{
				DOM_BrowserWindow* new_win;
				GET_FAILED_IF_ERROR(DOM_TabApiCache::GetOrCreateWindow(new_win, m_extension_support, call_helper->GetResult().value.query_tab_group.browser_window_id, GetRuntime()));
				DOMSetObject(value, new_win);
			}
			break;
		default:
			OP_ASSERT(!"Unexpected property");
		}
		return GET_SUCCESS;
	}
	else
		return call_helper->BlockGet(value, origining_runtime);
}

/* virtual */ ES_PutState
DOM_BrowserTabGroup::PutName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_tabs:
	case OP_ATOM_id:
	case OP_ATOM_closed:
	case OP_ATOM_collapsed:
	case OP_ATOM_position:
	case OP_ATOM_browserWindow:
		return PUT_SUCCESS; // read only

	default:
		return DOM_TabApiCachedObject::PutName(property_name, value, origining_runtime);
	}
}

DOM_BrowserTabManager*
DOM_BrowserTabGroup::GetTabs()
{
	if (m_tabs)
		return m_tabs;
	DOM_BrowserTabManager* tabs;
	OP_STATUS err = DOM_BrowserTabManager::MakeTabGroupTabs(tabs, m_extension_support, static_cast<DOM_Runtime*>(GetRuntime()), GetTabGroupId());
	OP_ASSERT(OpStatus::IsSuccess(err) || OpStatus::IsMemoryError(err));
	RETURN_VALUE_IF_ERROR(err, NULL);
	m_tabs = tabs;
	return m_tabs;
}

/* virtual */ void
DOM_BrowserTabGroup::GCTrace()
{
	GCMark(m_tabs);
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_BrowserTabGroup)
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabGroup, DOM_BrowserTabGroup::close, "close", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabGroup, DOM_BrowserTabGroup::focus, "focus", "")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabGroup, DOM_BrowserTabGroup::update, "update", "{collapsed:b}-")
	DOM_FUNCTIONS_FUNCTION(DOM_BrowserTabGroup, DOM_BrowserWindow::insert, "insert", "--")       // using the same as window
DOM_FUNCTIONS_END(DOM_BrowserTabGroup)

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
