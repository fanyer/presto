/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef WIC_TAB_API_SUPPORT

#include "modules/windowcommander/TabsApiNotificationHelper.h"

template<typename T>
T* CloneArray(T* src, unsigned int len)
{
	T* retval = OP_NEWA(T, len);
	if (retval)
		op_memcpy(retval, src, len * sizeof(T));
	return retval;
}

TabsApiNotificationHelper::QueryResult::QueryResult()
	: type(RESULT_UNINITIALIZED)
{
	op_memset(&value, 0, sizeof(value));
}

TabsApiNotificationHelper::QueryResult::~QueryResult()
{
	Clean();
}

void
TabsApiNotificationHelper::QueryResult::Clean()
{
	switch (type)
	{
	case RESULT_QUERY_TAB:
		OP_DELETEA(value.query_tab.title);
		break;
	}
	op_memset(&value, 0, sizeof(value));
	type = RESULT_UNINITIALIZED;
}

TabsApiNotificationHelper::TabsApiNotificationHelper()
{
}

void
TabsApiNotificationHelper::SetNotificationResult(OP_STATUS status, QueryResult::ResultType type)
{
	m_query_result.type = type;
	OnNotification(status, type);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyQueryAllBrowserWindows(OP_STATUS status, const OpTabAPIListener::TabAPIItemIdCollection* windows)
{
	if (OpStatus::IsSuccess(status) && windows)
		status = m_query_result.browser_windows.DuplicateOf(*windows);

	SetNotificationResult(status, QueryResult::RESULT_QUERY_ALL_WINDOWS);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyQueryAllTabs(OP_STATUS status
                                            , const OpTabAPIListener::TabAPIItemIdCollection* tabs
											, const OpTabAPIListener::TabWindowIdCollection* tab_windows)
{
	if (OpStatus::IsSuccess(status))
	{
		if (tabs)
		{
			OP_ASSERT(tab_windows);
			OP_ASSERT(tab_windows->GetCount() == tabs->GetCount());
			status = m_query_result.tabs.DuplicateOf(*tabs);
			if (OpStatus::IsSuccess(status))
				status = m_query_result.tab_windows.DuplicateOf(*tab_windows);
		}
		if (OpStatus::IsError(status))
		{
			m_query_result.tabs.Clear();
			m_query_result.tab_windows.Clear();
		}
	}
	SetNotificationResult(status, QueryResult::RESULT_QUERY_ALL_TABS);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyQueryTabGroup(OP_STATUS status
                                             , const OpTabAPIListener::ExtensionTabGroupInfo *info
                                             , const OpTabAPIListener::TabAPIItemIdCollection* tabs
                                             , const OpTabAPIListener::TabWindowIdCollection* tab_windows)
{
	
	if (OpStatus::IsSuccess(status))
	{
		op_memcpy(&m_query_result.value.query_tab_group, info, sizeof(*info));
		if (tabs)
		{
			OP_ASSERT(tab_windows);
			OP_ASSERT(tab_windows->GetCount() == tabs->GetCount());
			status = m_query_result.tabs.DuplicateOf(*tabs);
			if (OpStatus::IsSuccess(status))
				status = m_query_result.tab_windows.DuplicateOf(*tab_windows);
		}
		if (OpStatus::IsError(status))
		{
			m_query_result.tabs.Clear();
			m_query_result.tab_windows.Clear();
		}
	}
	SetNotificationResult(status, QueryResult::RESULT_QUERY_TAB_GROUP);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyQueryBrowserWindow(OP_STATUS status
	                                             , const OpTabAPIListener::ExtensionBrowserWindowInfo* info
	                                             , const OpTabAPIListener::TabAPIItemIdCollection* tab_groups
	                                             , const OpTabAPIListener::TabAPIItemIdCollection* tabs
	                                             , const OpTabAPIListener::TabWindowIdCollection* tab_windows)
{
	if (OpStatus::IsSuccess(status))
	{
		op_memcpy(&m_query_result.value.query_window, info, sizeof(*info));
		if (tabs)
		{
			OP_ASSERT(tab_windows);
			OP_ASSERT(tab_windows->GetCount() == tabs->GetCount());
			status = m_query_result.tabs.DuplicateOf(*tabs);
			if (OpStatus::IsSuccess(status))
				status = m_query_result.tab_windows.DuplicateOf(*tab_windows);
		}
		if (tab_groups && OpStatus::IsSuccess(status))
			status = m_query_result.tab_groups.DuplicateOf(*tab_groups);
		if (OpStatus::IsError(status))
		{
			m_query_result.tabs.Clear();
			m_query_result.tab_windows.Clear();
			m_query_result.tab_groups.Clear();
		}
	}
	SetNotificationResult(status, QueryResult::RESULT_QUERY_WINDOW);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyQueryTab(OP_STATUS status, const OpTabAPIListener::ExtensionTabInfo *info)
{
	if (OpStatus::IsSuccess(status))
	{
		OP_ASSERT(!m_query_result.value.query_tab.title);
		op_memcpy(&m_query_result.value.query_tab, info, sizeof(*info));
		m_query_result.value.query_tab.title = UniSetNewStr(info->title);
		if (!m_query_result.value.query_tab.title)
			status = OpStatus::ERR_NO_MEMORY;
	}
	SetNotificationResult(status, QueryResult::RESULT_QUERY_TAB);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyQueryAllTabGroups(OP_STATUS status
                                                 , const OpTabAPIListener::TabAPIItemIdCollection* tab_groups)
{
	if (OpStatus::IsSuccess(status) && tab_groups)
		status = m_query_result.tab_groups.DuplicateOf(*tab_groups);

	SetNotificationResult(status, QueryResult::RESULT_QUERY_ALL_TAB_GROUPS);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyBrowserWindowCreate(OP_STATUS status, OpTabAPIListener::TabAPIItemId window_id)
{
	if (OpStatus::IsSuccess(status))
	{
		m_query_result.value.created_window_id = window_id;
	}
	SetNotificationResult(status, QueryResult::RESULT_REQUEST_WINDOW_CREATE);
}

/* virtual */ void
TabsApiNotificationHelper::NotifyTabMove(OP_STATUS status, OpTabAPIListener::TabAPIItemId browser_window_id, OpTabAPIListener::TabAPIItemId tab_group_id)
{
	m_query_result.value.moved_to_info.browser_window_id = browser_window_id;
	m_query_result.value.moved_to_info.tab_group_id = tab_group_id;
	SetNotificationResult(status, QueryResult::RESULT_REQUEST_TAB_MOVE); 
}

/* virtual */ void
TabsApiNotificationHelper::NotifyTabGroupMove(OP_STATUS status, OpTabAPIListener::TabAPIItemId browser_window_id)
{
	m_query_result.value.moved_to_info.browser_window_id = browser_window_id;
	m_query_result.value.moved_to_info.tab_group_id = 0;
	SetNotificationResult(status, QueryResult::RESULT_REQUEST_TAB_GROUP_MOVE); 
}

#endif // WIC_TAB_API_SUPPORT
