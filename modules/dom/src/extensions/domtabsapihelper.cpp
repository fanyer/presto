/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA. All rights reserved.
**
** This file is part of the Opera web browser. It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef DOM_EXTENSIONS_TAB_API_SUPPORT

#include "modules/dom/src/extensions/domtabsapihelper.h"
#include "modules/dom/src/extensions/dombrowsertab.h"
#include "modules/dom/src/extensions/dombrowsertabgroup.h"
#include "modules/dom/src/extensions/dombrowserwindow.h"
#include "modules/dochand/winman.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/util/str.h"
#include "modules/doc/frm_doc.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/ecmascript_utils/essched.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"

static int convert_value_impl(DOM_Object* this_object, ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(argc > 0);
	*return_value = argv[0];
	return ES_VALUE;
}

static OP_STATUS DOM_ConvertValue(ES_Value* value, const char* convert_to, ES_AsyncCallback* callback, DOM_Runtime* origining_runtime)
{
	DOM_Function* func = OP_NEW(DOM_Function, (convert_value_impl));
	RETURN_OOM_IF_NULL(func);
	RETURN_IF_ERROR(func->SetFunctionRuntime(origining_runtime, UNI_L("c"), "Function", convert_to));

	ES_AsyncInterface* async_if = origining_runtime->GetEnvironment()->GetAsyncInterface();
	OP_ASSERT(async_if);

	ES_ThreadScheduler *scheduler = origining_runtime->GetESScheduler();
	ES_Thread* thread = scheduler->GetCurrentThread();

	return async_if->CallFunction(func->GetNativeObject(), func->GetNativeObject(), 1, value, callback, thread);
};

DOM_TabsApiHelperBase::DOM_TabsApiHelperBase()
	: m_status(OpStatus::OK)
	, m_is_finished(FALSE)
{

}

void
DOM_TabsApiHelperBase::Finish(OP_STATUS status)
{
	m_is_finished = TRUE;
	m_status = status;
	DOM_RestartObject::Resume();
}

/* static */ int
DOM_TabsApiHelperBase::CallTabAPIException(DOM_Object* this_object, ES_Value* return_value, OP_STATUS error)
{
	if (OpStatus::IsMemoryError(error))
		return ES_NO_MEMORY;
	if (error == DOM_TabsApiHelper::Error::ERR_NOT_ENOUGH_INITIAL_CONTENT)
		return DOM_CALL_DOMEXCEPTION(VALIDATION_ERR);
	if (error == OpStatus::ERR_NOT_SUPPORTED)
		return DOM_CALL_DOMEXCEPTION(NOT_SUPPORTED_ERR);
	if (error == DOM_TabsApiHelper::Error::ERR_WRONG_MOVE_ARGUMENT)
		return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);
	return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);
}

/* static */ void
DOM_TabsApiHelperBase::ExtractTabProps(TabProps& props, ES_Object* es_props, DOM_Runtime* runtime, BOOL clone_strings)
{
	props.title = NULL;
	props.url = NULL;
	props.is_locked = MAYBE;
	props.is_private = MAYBE;
	props.is_selected = MAYBE;

	if (!es_props)
		return;

	ES_Value value;
	OP_BOOLEAN result;

	result = runtime->GetName(es_props, UNI_L("url"), &value);
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
		if (clone_strings)
			props.url = UniSetNewStr(value.value.string); // Ignore OOM here - treat it as we had empty strings.
		else
			props.url = value.value.string;

	result = runtime->GetName(es_props, UNI_L("title"), &value);
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_STRING)
		if (clone_strings)
			props.title = UniSetNewStr(value.value.string); // Ignore OOM here - treat it as we had empty strings.
		else
			props.title = value.value.string;

	result = runtime->GetName(es_props, UNI_L("focused"), &value);
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_BOOLEAN)
		props.is_selected = value.value.boolean ? YES : NO;

	result = runtime->GetName(es_props, UNI_L("private"), &value);
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_BOOLEAN)
	props.is_private = value.value.boolean ? YES : NO;

	result = runtime->GetName(es_props, UNI_L("locked"), &value);
	if (result == OpBoolean::IS_TRUE && value.type == VALUE_BOOLEAN)
		props.is_locked = value.value.boolean ? YES : NO;
}

void
DOM_TabsApiHelperBase::ExtractWindowProps(WindowProps& props, ES_Object* es_props, DOM_Runtime* runtime)
{
	props.is_private = MAYBE;
	props.is_focused = MAYBE;

	if (!es_props)
		return;

	ES_Value value;
	OP_BOOLEAN result;

	result = runtime->GetName(es_props, UNI_L("focused"), &value);
	if (result == OpBoolean::IS_TRUE)
		props.is_focused = value.value.boolean ? YES : NO;

	result = runtime->GetName(es_props, UNI_L("private"), &value);
	if (result == OpBoolean::IS_TRUE)
		props.is_private = value.value.boolean  ? YES : NO;

	props.top = TruncateDoubleToInt(DOMGetDictionaryNumber(es_props, UNI_L("top"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
	props.left = TruncateDoubleToInt(DOMGetDictionaryNumber(es_props, UNI_L("left"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
	props.height = TruncateDoubleToInt(DOMGetDictionaryNumber(es_props, UNI_L("height"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
	props.width = TruncateDoubleToInt(DOMGetDictionaryNumber(es_props, UNI_L("width"), OpTabAPIListener::UNSPECIFIED_WINDOW_DIMENSION));
}

/*static */ void
DOM_TabsApiHelperBase::ExtractTabGroupProps(TabGroupProps& props, ES_Object* es_props, DOM_Runtime* runtime)
{
	props.is_collapsed = MAYBE;

	if (!es_props)
		return;

	ES_Value value;
	OP_BOOLEAN result;
	result = runtime->GetName(es_props, UNI_L("collapsed"), &value);
	if (result == OpBoolean::IS_TRUE)
		props.is_collapsed = value.value.boolean  ? YES : NO;
}

OP_STATUS
DOM_TabsApiHelperBase::OpenNewTab(OpTabAPIListener::WindowTabInsertTarget& insert_target, ES_Object* es_props, BOOL3 focus_override, OpWindowCommander** out_wc)
{
	FramesDocument* frm_doc = GetRuntime()->GetFramesDocument();
	if (!frm_doc)
		return OpStatus::ERR; // No doc == no go.

	OpWindowCommander* source = frm_doc->GetWindow()->GetWindowCommander();
	OpGadget* gadget = source->GetGadget();

	TabProps props;
	ExtractTabProps(props, es_props, GetRuntime(), FALSE);

	OpUiWindowListener::CreateUiWindowArgs args;

	args.locked = props.is_locked == YES;
	args.opener = source;
	args.type = OpUiWindowListener::WINDOWTYPE_REGULAR;
	if (focus_override == MAYBE)
		args.regular.focus_document = props.is_selected == YES;
	else
		args.regular.focus_document = focus_override == YES;
	args.regular.open_background = !args.regular.focus_document;
	args.regular.height = 0;
	args.regular.width = 0;
	args.regular.opened_by_script = TRUE;
	args.regular.scrollbars = !!g_pcdoc->GetIntegerPref(PrefsCollectionDoc::ShowScrollbars);
	args.regular.toolbars = TRUE;
	args.regular.transparent = FALSE;
	args.regular.user_initiated = FALSE;

	BOOL is_gadget_url = FALSE;
	OpString url;
	if (props.url)
	{
		OP_ASSERT(gadget);
		URL resolved = gadget->ResolveExtensionURL(props.url);
		RETURN_IF_ERROR(url.Set(resolved.GetAttribute(URL::KUniName_With_Fragment).CStr()));
		is_gadget_url = resolved.Type() == URL_WIDGET;
	}

	OpWindowCommander* wc;
	RETURN_IF_ERROR(g_windowCommanderManager->GetWindowCommander(&wc));

	if (is_gadget_url)
		static_cast<WindowCommander*>(wc)->SetGadget(gadget);

	args.insert_target = &insert_target;
	OP_STATUS status = g_windowCommanderManager->GetUiWindowListener()->CreateUiWindow(wc, args);
	if (OpStatus::IsError(status))
	{
		g_windowCommanderManager->ReleaseWindowCommander(wc);
		return status;
	}

	if (props.is_private == YES)
		wc->SetPrivacyMode(TRUE);
	else if (props.is_private == NO)
		wc->SetPrivacyMode(FALSE);

	wc->OpenURL(url.CStr(), FALSE);
	if (out_wc)
		*out_wc = wc;
	return OpStatus::OK;
}

////////////////////////////////////////////////////////////////////


DOM_TabsApiHelper::DOM_TabsApiHelper()
{
}

/* static */ OP_STATUS
DOM_TabsApiHelper::Make(DOM_TabsApiHelper*& new_obj, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_TabsApiHelper, ()), runtime));
	return new_obj->KeepAlive();
}

void
DOM_TabsApiHelper::QueryAllWindows()
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryAllBrowserWindows(this);
}

void
DOM_TabsApiHelper::QueryAllTabs()
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryAllTabs(this);
}

void
DOM_TabsApiHelper::QueryTabGroup(OpTabAPIListener::TabAPIItemId tab_group_id, BOOL include_content)
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryTabGroup(tab_group_id, include_content, this);
}

void
DOM_TabsApiHelper::QueryWindow(OpTabAPIListener::TabAPIItemId window_id, BOOL include_content)
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryBrowserWindow(window_id, include_content, this);
}

void
DOM_TabsApiHelper::QueryTab(OpWindowCommander* target_widow)
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryTab(target_widow, this);
}

void
DOM_TabsApiHelper::QueryTab(OpTabAPIListener::TabAPIItemId target_tab)
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryTab(target_tab, this);
}

void
DOM_TabsApiHelper::OnNotification(OP_STATUS status, QueryResult::ResultType type)
{
	Finish(status);
}

void
DOM_TabsApiHelper::RequestTabClose(OpTabAPIListener::TabAPIItemId tab_id)
{
	g_windowCommanderManager->GetTabAPIListener()->OnRequestTabClose(tab_id, this);
}

void
DOM_TabsApiHelper::RequestWindowClose(OpTabAPIListener::TabAPIItemId window_id)
{
	g_windowCommanderManager->GetTabAPIListener()->OnRequestBrowserWindowClose(window_id, this);
}

void
DOM_TabsApiHelper::RequestWindowMove(OpTabAPIListener::TabAPIItemId window_id, const OpRect& rect)
{
	OpTabAPIListener::ExtensionWindowMoveResize info;
	info.window_height = rect.height;
	info.window_width = rect.width;
	info.window_top = rect.Top();
	info.window_left = rect.Left();
	info.maximize = !(info.window_height >= 0 || info.window_width > 0 || info.window_top  > 0 || info.window_left > 0);
	g_windowCommanderManager->GetTabAPIListener()->OnRequestBrowserWindowMoveResize(window_id, info, this);
}

void
DOM_TabsApiHelper::QueryAllTabGroups()
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryAllTabGroups(this);
}

void
DOM_TabsApiHelper::RequestTabGroupUpdate(OpTabAPIListener::TabAPIItemId target_tab_group_id, ES_Object* es_props, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(origining_runtime);

	TabGroupProps tab_group_props;
	ExtractTabGroupProps(tab_group_props, es_props, origining_runtime);

	g_windowCommanderManager->GetTabAPIListener()->OnRequestTabGroupUpdate(target_tab_group_id, tab_group_props.is_collapsed, this);
}

void
DOM_TabsApiHelper::RequestTabUpdate(OpTabAPIListener::TabAPIItemId target_tab_id, ES_Object* es_props, DOM_Runtime* origining_runtime)
{
	OP_ASSERT(origining_runtime);

	TabProps tab_props;
	ExtractTabProps(tab_props, es_props, origining_runtime, FALSE);

	g_windowCommanderManager->GetTabAPIListener()->OnRequestTabUpdate(target_tab_id, tab_props.is_selected == YES, tab_props.is_locked, tab_props.url, tab_props.title, this);
}

void
DOM_TabsApiHelper::RequestTabGroupClose(OpTabAPIListener::TabAPIItemId target_tab_group_id)
{
	g_windowCommanderManager->GetTabAPIListener()->OnRequestTabGroupClose(target_tab_group_id, this);
}

void
DOM_TabsApiHelper::RequestTabFocus(OpTabAPIListener::TabAPIItemId target_tab_id)
{
	g_windowCommanderManager->GetTabAPIListener()->OnRequestTabUpdate(target_tab_id, TRUE, -1, NULL, NULL, this);
}

///////////////////////////////////////////////////////////////////////////

/* static */ OP_STATUS
DOM_TabsApiFocusHelper::Make(DOM_TabsApiFocusHelper*& new_obj, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_TabsApiFocusHelper, ()), runtime));
	return new_obj->KeepAlive();
}

void
DOM_TabsApiFocusHelper::RequestTabGroupFocus(OpTabAPIListener::TabAPIItemId target_tab_group_id)
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryTabGroup(target_tab_group_id, FALSE, this);
}

void
DOM_TabsApiFocusHelper::RequestWindowFocus(OpTabAPIListener::TabAPIItemId target_window_id)
{
	g_windowCommanderManager->GetTabAPIListener()->OnQueryBrowserWindow(target_window_id, FALSE, this);
}

/* virtual */ void
DOM_TabsApiFocusHelper::OnNotification(OP_STATUS status, QueryResult::ResultType type)
{
	if (OpStatus::IsSuccess(status))
	{
		switch (type)
		{
		case QueryResult::RESULT_QUERY_WINDOW:
			g_windowCommanderManager->GetTabAPIListener()->OnRequestTabUpdate(GetResult().value.query_window.selected_tab_id, TRUE, -1, NULL, NULL, this);
			return;
		case QueryResult::RESULT_QUERY_TAB_GROUP:
			g_windowCommanderManager->GetTabAPIListener()->OnRequestTabUpdate(GetResult().value.query_tab_group.selected_tab_id, TRUE, -1, NULL, NULL, this);
			return;
		default:
			OP_ASSERT(!"Unexpected notification!"); // Intentional fall through
		case QueryResult::RESULT_REQUEST_TAB_UPDATE:
			break; // go to Finish(status).
		}
	}

	Finish(status);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

DOM_TabsApiInsertHelper::DOM_TabsApiInsertHelper()
	: m_src_group_id(0)
	, m_src_tab_id(0)
{
	op_memset(&m_insert_target, 0 ,sizeof(m_insert_target));
}

/* static */ OP_STATUS
DOM_TabsApiInsertHelper::Make(DOM_TabsApiInsertHelper*& new_obj, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_TabsApiInsertHelper, ()), runtime));
	return new_obj->KeepAlive();
}

void DOM_TabsApiInsertHelper::RequestInsert(ES_Object* insert_what, ES_Object* into_what, ES_Object* before_what /*optional*/)
{
	OP_STATUS error;
	if (OpStatus::IsError(error = FindInsertDestination(into_what))
	 || OpStatus::IsError(error = CheckSrc(insert_what))
	 || OpStatus::IsError(error = CheckBeforeWhat(before_what)))
		Finish(error);
	else
		PerformMove();
}

OP_STATUS DOM_TabsApiInsertHelper::CheckSrc(ES_Object* insert_what)
{
	OP_ASSERT(insert_what);
	DOM_HOSTOBJECT_SAFE(src_tab_group, insert_what, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);
	if (src_tab_group)
		m_src_group_id = src_tab_group->GetTabGroupId();
	else
	{
		DOM_HOSTOBJECT_SAFE(src_tab, insert_what, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
		if (src_tab)
			m_src_tab_id =  src_tab->GetTabId();
		else
			return Error::ERR_WRONG_MOVE_ARGUMENT;
	}
	return OpStatus::OK;
}

OP_STATUS
DOM_TabsApiInsertHelper::FindInsertDestination(ES_Object* into_what)
{
	OP_ASSERT(into_what);
	DOM_HOSTOBJECT_SAFE(dst_window, into_what, DOM_TYPE_BROWSER_WINDOW, DOM_BrowserWindow);
	if (dst_window)
	{
		m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::EXISTING_BROWSER_WINDOW;
		m_insert_target.dst_window_id = dst_window->GetWindowId();
	}
	else
	{
		DOM_HOSTOBJECT_SAFE(dst_tab_group, into_what, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);
		if (dst_tab_group)
		{
			m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::EXISTING_TAB_GROUP;
			m_insert_target.dst_group_id = dst_tab_group->GetTabGroupId();
		}
		else
			return Error::ERR_WRONG_MOVE_ARGUMENT;
	}
	return OpStatus::OK;
}

OP_STATUS
DOM_TabsApiInsertHelper::CheckBeforeWhat(ES_Object* before_what)
{
	if(before_what)
	{
		DOM_HOSTOBJECT_SAFE(tab_group, before_what, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);
		if (tab_group)
		{
			m_insert_target.before_item_id = tab_group->GetTabGroupId();
			return OpStatus::OK;
		}
		DOM_HOSTOBJECT_SAFE(tab, before_what, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
		if (tab)
		{
			m_insert_target.before_item_id = tab->GetTabId();
			return OpStatus::OK;
		}
		return Error::ERR_WRONG_MOVE_ARGUMENT;
	}
	return OpStatus::OK;
}

/* virtual */ void
DOM_TabsApiInsertHelper::OnNotification(OP_STATUS status, QueryResult::ResultType type)
{
	switch (type)
	{
	case QueryResult::RESULT_REQUEST_TAB_GROUP_MOVE:
	case QueryResult::RESULT_REQUEST_TAB_MOVE:
		Finish(status);
		return;
	default:
		OP_ASSERT(FALSE);
		Finish(OpStatus::ERR);
		return;
	}
}

void
DOM_TabsApiInsertHelper::PerformMove()
{
	OP_ASSERT(!m_src_group_id || !m_src_tab_id);
	OP_ASSERT(m_src_group_id || m_src_tab_id);
	if (m_src_group_id)
	{
		if (m_insert_target.type != OpTabAPIListener::WindowTabInsertTarget::EXISTING_BROWSER_WINDOW)
		{
			Finish(Error::ERR_WRONG_MOVE_STATE);
			return;
		}
		g_windowCommanderManager->GetTabAPIListener()->OnRequestTabGroupMove(m_src_group_id, m_insert_target, this);
	}
	else
	{
		OP_ASSERT(m_insert_target.type == OpTabAPIListener::WindowTabInsertTarget::EXISTING_BROWSER_WINDOW
			   || m_insert_target.type == OpTabAPIListener::WindowTabInsertTarget::EXISTING_TAB_GROUP);
		g_windowCommanderManager->GetTabAPIListener()->OnRequestTabMove(m_src_tab_id, m_insert_target, this);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

DOM_TabsApiTabCreateHelper::DOM_TabsApiTabCreateHelper()
	: m_properties(NULL)
{
	op_memset(&m_insert_target, 0 ,sizeof(m_insert_target));
}

/* static */ OP_STATUS
DOM_TabsApiTabCreateHelper::Make(DOM_TabsApiTabCreateHelper*& new_obj, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_TabsApiTabCreateHelper, ()), runtime));
	return new_obj->KeepAlive();
}

void
DOM_TabsApiTabCreateHelper::RequestCreateTab(ES_Object* tab_props, OpTabAPIListener::TabAPIItemId parent_win_id, OpTabAPIListener::TabAPIItemId parent_group_id, ES_Object* before_what)
{
	m_properties = tab_props;
	FindInsertDestination(parent_win_id, parent_group_id);

	OP_STATUS error;
	if (OpStatus::IsError(error = CheckBeforeWhat(before_what)))
		Finish(error);
	else
		PerformCreate();

}

/* virtual */ void
DOM_TabsApiTabCreateHelper::GCTrace()
{
	GCMark(m_properties);
	DOM_RestartObject::GCTrace();
}

/* virtual */ void
DOM_TabsApiTabCreateHelper::OnNotification(OP_STATUS status, QueryResult::ResultType type)
{

	if (OpStatus::IsSuccess(status))
	{
		switch (type)
		{
		case QueryResult::RESULT_QUERY_TAB:

			unsigned int window_id = GetResult().value.query_tab.window_id;
			OpTabAPIListener::TabAPIItemId id = GetResult().value.query_tab.id;
			m_query_result.Clean();
			m_query_result.type = QueryResult::RESULT_REQUEST_TAB_CREATE;
			m_query_result.value.created_tab_info.window_id = window_id;
			m_query_result.value.created_tab_info.tab_id = id;
			break; // GOTO Finish(status);
		}
	}

	Finish(status);
}

OP_STATUS
DOM_TabsApiTabCreateHelper::CheckBeforeWhat(ES_Object* before_what)
{
	if(before_what)
	{
		DOM_HOSTOBJECT_SAFE(tab_group, before_what, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);
		if (tab_group)
		{
			m_insert_target.before_item_id = tab_group->GetTabGroupId();
			return OpStatus::OK;
		}
		DOM_HOSTOBJECT_SAFE(tab, before_what, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
		if (tab)
		{
			m_insert_target.before_item_id = tab->GetTabId();
			return OpStatus::OK;
		}
		return Error::ERR_WRONG_MOVE_ARGUMENT;
	}
	return OpStatus::OK;
}

void
DOM_TabsApiTabCreateHelper::FindInsertDestination(OpTabAPIListener::TabAPIItemId parent_win_id, OpTabAPIListener::TabAPIItemId parent_group_id)
{
	OP_ASSERT(!parent_win_id || !parent_group_id);
	if (parent_win_id)
	{
		m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::EXISTING_BROWSER_WINDOW;
		m_insert_target.dst_window_id = parent_win_id;
	}
	else if (parent_group_id)
	{
		m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::EXISTING_TAB_GROUP;
		m_insert_target.dst_group_id = parent_group_id;
	}
	else
		m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::CURRENT_CONTAINER;
}

void
DOM_TabsApiTabCreateHelper::PerformCreate()
{
	m_query_result.Clean();
	OpWindowCommander* wc;
	OP_STATUS error = OpenNewTab(m_insert_target, m_properties, MAYBE, &wc);
	if (OpStatus::IsError(error))
	{
		Finish(error);
		return;
	}

	g_windowCommanderManager->GetTabAPIListener()->OnQueryTab(wc, this);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////

DOM_TabsApiContainerCreateHelper::DOM_TabsApiContainerCreateHelper()
	: m_initial_content_index(0)
	, m_initial_content(NULL)
	, m_querying_before(FALSE)
	, m_focus_container(NO)
{
	m_create_next_timer.SetTimerListener(this);
	op_memset(&m_insert_target, 0 ,sizeof(m_insert_target));
}

/* static */ OP_STATUS
DOM_TabsApiContainerCreateHelper::Make(DOM_TabsApiContainerCreateHelper*& new_obj, DOM_Runtime* runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(new_obj = OP_NEW(DOM_TabsApiContainerCreateHelper, ()), runtime));
	return new_obj->KeepAlive();
}

/* virtual */ void
DOM_TabsApiContainerCreateHelper::GCTrace()
{
	GCMark(m_initial_content);
	DOM_RestartObject::GCTrace();
}

void
DOM_TabsApiContainerCreateHelper::RequestCreateTabGroup(ES_Object* initial_content, ES_Object* container_props, OpTabAPIListener::TabAPIItemId parent_win_id, ES_Object* before_what)
{
	TabGroupProps props;
	ExtractTabGroupProps(props, container_props, GetRuntime());

	m_insert_target.tab_group_create.collapsed = props.is_collapsed == YES;
	m_initial_content = initial_content;
	ES_Value len_val;
	OP_BOOLEAN result = GetRuntime()->GetName(initial_content, UNI_L("length"), &len_val);
	if (result != OpBoolean::IS_TRUE || len_val.type != VALUE_NUMBER || len_val.value.number < 2.0)
	{
		Finish(Error::ERR_NOT_ENOUGH_INITIAL_CONTENT);
		return;
	}
	FindInsertDestination(parent_win_id);
	OP_STATUS error;
	if (OpStatus::IsError(error = CheckBeforeWhat(before_what)))
		Finish(error);
	else
		PerformCreateStep();
}

void
DOM_TabsApiContainerCreateHelper::RequestCreateWindow(ES_Object* initial_content, ES_Object* container_props)
{
	WindowProps props;
	ExtractWindowProps(props, container_props, GetRuntime());

	m_initial_content = initial_content;
	m_insert_target.window_create.height = props.height;
	m_insert_target.window_create.width = props.width;
	m_insert_target.window_create.left = props.left;
	m_insert_target.window_create.top = props.top;
	m_insert_target.window_create.is_private = props.is_private == YES;
	m_focus_container = props.is_focused == YES ? YES : NO;
	m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::NEW_BROWSER_WINDOW;
	PerformCreateStep();
}

void DOM_TabsApiContainerCreateHelper::SetInsertTypeToExisting()
{
	if (m_insert_target.type == OpTabAPIListener::WindowTabInsertTarget::NEW_BROWSER_WINDOW)
		m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::EXISTING_BROWSER_WINDOW;
	if (m_insert_target.type == OpTabAPIListener::WindowTabInsertTarget::NEW_TAB_GROUP)
		m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::EXISTING_TAB_GROUP;
	m_insert_target.before_item_id = 0;
}

/* virtual */void
DOM_TabsApiContainerCreateHelper::OnNotification(OP_STATUS status, QueryResult::ResultType type)
{
	if (OpStatus::IsSuccess(status))
	{
		switch (type)
		{
		case QueryResult::RESULT_REQUEST_TAB_MOVE:
		case QueryResult::RESULT_REQUEST_TAB_GROUP_MOVE:
			m_insert_target.dst_group_id = m_query_result.value.moved_to_info.tab_group_id;
			m_insert_target.dst_window_id = m_query_result.value.moved_to_info.browser_window_id;
			break;
		case QueryResult::RESULT_QUERY_TAB:
			m_insert_target.dst_group_id = m_query_result.value.query_tab.tab_group_id;
			m_insert_target.dst_window_id = m_query_result.value.query_tab.browser_window_id;
			break;
		default:
			OP_ASSERT(FALSE);
		}
		SetInsertTypeToExisting();
		m_create_next_timer.Start(0);
	}
	else
		Finish(status);
}

/* virtual */ void
DOM_TabsApiContainerCreateHelper::OnTimeOut(OpTimer* timer)
{
	PerformCreateStep();
}

OP_STATUS
DOM_TabsApiContainerCreateHelper::CheckBeforeWhat(ES_Object* before_what)
{
	if(before_what)
	{
		DOM_HOSTOBJECT_SAFE(tab_group, before_what, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);
		if (tab_group)
		{
			m_insert_target.before_item_id = tab_group->GetTabGroupId();
			return OpStatus::OK;
		}
		DOM_HOSTOBJECT_SAFE(tab, before_what, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
		if (tab)
		{
			m_insert_target.before_item_id = tab->GetTabId();
			return OpStatus::OK;
		}
		return Error::ERR_WRONG_MOVE_ARGUMENT;
	}
	return OpStatus::OK;
}

void
DOM_TabsApiContainerCreateHelper::FindInsertDestination(OpTabAPIListener::TabAPIItemId parent_win_id)
{
	m_insert_target.type = OpTabAPIListener::WindowTabInsertTarget::NEW_TAB_GROUP;
	m_insert_target.dst_window_id = parent_win_id;
}

void DOM_TabsApiContainerCreateHelper::PerformCreateStep()
{
	ES_Value initial_content_item;
	OP_BOOLEAN status;
	if (m_initial_content)
	{
		status = GetRuntime()->GetIndex(m_initial_content, m_initial_content_index, &initial_content_item);
		if (status != OpBoolean::IS_TRUE || initial_content_item.type != VALUE_OBJECT)
		{
			if (!OpStatus::IsError(status))
			{
				if (m_initial_content_index == 0) // Empty array - treat as null.
				{
					m_initial_content = NULL;
					PerformCreateStep();
					return;
				}
				status = OpStatus::OK;
			}

			CreatingContainerFinished();
			Finish(status);
			return;
		}

		++m_initial_content_index;

		DOM_HOSTOBJECT_SAFE(tab, initial_content_item.value.object, DOM_TYPE_BROWSER_TAB, DOM_BrowserTab);
		if (tab)
		{
			g_windowCommanderManager->GetTabAPIListener()->OnRequestTabMove(tab->GetTabId(), m_insert_target, this);
			return;
		}

		DOM_HOSTOBJECT_SAFE(tab_group, initial_content_item.value.object, DOM_TYPE_BROWSER_TAB_GROUP, DOM_BrowserTabGroup);
		if (tab_group)
		{
			g_windowCommanderManager->GetTabAPIListener()->OnRequestTabGroupMove(tab_group->GetTabGroupId(), m_insert_target, this);
			return;
		}

		ConversionCallback* conversion_callback = OP_NEW(ConversionCallback, (this));
		if (!conversion_callback)
		{
			Finish(OpStatus::ERR_NO_MEMORY);
			return;
		}

		status = DOM_ConvertValue(&initial_content_item, "{url:S,title:S,focused:b,locked:b,private:b}", conversion_callback, GetRuntime());
		if (OpStatus::IsError(status))
		{
			OP_DELETE(conversion_callback);
			Finish(status);
			return;
		}
	}
	else
	{
		if (m_insert_target.type != OpTabAPIListener::WindowTabInsertTarget::NEW_BROWSER_WINDOW
		 && m_insert_target.type != OpTabAPIListener::WindowTabInsertTarget::NEW_TAB_GROUP)
		{
			CreatingContainerFinished();
			Finish(OpStatus::OK);
			return;
		}
		else
		{
			OpWindowCommander* wc;
			status = OpenNewTab(m_insert_target, NULL, m_focus_container ? YES : NO, &wc);
			if (OpStatus::IsError(status))
				Finish(status);
			else
				g_windowCommanderManager->GetTabAPIListener()->OnQueryTab(wc, this);
			return;
		}
	}
}

/* virtual */ OP_STATUS
DOM_TabsApiContainerCreateHelper::ConversionCallback::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	OP_ASSERT(ES_ASYNC_CALL_FUNCTION == operation);
	if (status != ES_ASYNC_SUCCESS)
		m_helper->Finish(OpStatus::ERR);
	else
	{
		OpWindowCommander* wc;
		OP_STATUS status = m_helper->OpenNewTab(m_helper->m_insert_target, result.value.object, m_helper->m_focus_container, &wc);
		if (OpStatus::IsError(status))
			m_helper->Finish(status);
		else
		{
			m_helper->m_query_result.Clean();
			g_windowCommanderManager->GetTabAPIListener()->OnQueryTab(wc, m_helper);
		}
	}
	OP_DELETE(this);
	return OpStatus::OK;
}

void DOM_TabsApiContainerCreateHelper::CreatingContainerFinished()
{
	m_query_result.Clean();
	if (IsCreatingGroup())
	{
		m_query_result.type = QueryResult::RESULT_REQUEST_TAB_GROUP_CREATE;
		m_query_result.value.created_tab_group_id = m_insert_target.dst_group_id;
	}
	else
	{
		m_query_result.type = QueryResult::RESULT_REQUEST_WINDOW_CREATE;
		m_query_result.value.created_window_id = m_insert_target.dst_window_id;
	}
}

BOOL DOM_TabsApiContainerCreateHelper::IsCreatingGroup()
{
	return m_insert_target.type == OpTabAPIListener::WindowTabInsertTarget::EXISTING_TAB_GROUP || m_insert_target.type == OpTabAPIListener::WindowTabInsertTarget::NEW_TAB_GROUP;
}

#endif // DOM_EXTENSIONS_TAB_API_SUPPORT
