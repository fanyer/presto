/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef WINDOWCOMMANDER_TABSAPINOTIFICATIONHELPER_H
#define WINDOWCOMMANDER_TABSAPINOTIFICATIONHELPER_H

#ifdef WIC_TAB_API_SUPPORT

#include "modules/windowcommander/OpTabAPIListener.h"

/**
 * This class implements OpTabAPIListener::ExtensionWindowTabNotifications by
 * copying the reported result.
 *
 * The data in the information structures passed to the
 * OpTabAPIListener::ExtensionWindowTabNotifications callbacks is only valid
 * during the callback. If the result of the notification callback is used
 * asynchronously, a deep copy of the information structure needs to be made.
 *
 * This class implements the OpTabAPIListener::ExtensionWindowTabNotifications
 * interface by creating a deep copy of the provided data and calling the
 * virtual abstract method OnNotification(). The user of this helper class must
 * implement the OnNotification() method. The implementation of the
 * OnNotification() method may decide to postpone the handling of the
 * notification data. On handling the notification, the data can be accessed by
 * calling the method GetResult().
 *
 * @note The success (or error) of a query or operation is not stored in this
 *  instance. It is only passed to the OnNotification() method and must be
 *  handled immediately by the implementation of that method.
 *
 */
class TabsApiNotificationHelper
	: public OpTabAPIListener::ExtensionWindowTabNotifications
{
public:
	TabsApiNotificationHelper();

	// Implementation of OpExtensionUIListener::ExtensionWindowTabNotifications
	virtual void NotifyQueryAllBrowserWindows(OP_STATUS status
	                                        , const OpTabAPIListener::TabAPIItemIdCollection* windows);
	virtual void NotifyQueryAllTabGroups(OP_STATUS status
	                                   , const OpTabAPIListener::TabAPIItemIdCollection* tab_groups);
	virtual void NotifyQueryAllTabs(OP_STATUS status
	                              , const OpTabAPIListener::TabAPIItemIdCollection* tabs
	                              , const OpTabAPIListener::TabWindowIdCollection* tab_windows);

	virtual void NotifyQueryBrowserWindow(OP_STATUS status
	                                    , const OpTabAPIListener::ExtensionBrowserWindowInfo* info
	                                    , const OpTabAPIListener::TabAPIItemIdCollection* tab_groups
	                                    , const OpTabAPIListener::TabAPIItemIdCollection* tabs
	                                    , const OpTabAPIListener::TabWindowIdCollection* tab_windows);

	virtual void NotifyQueryTabGroup(OP_STATUS status
	                               , const OpTabAPIListener::ExtensionTabGroupInfo* info
	                               , const OpTabAPIListener::TabAPIItemIdCollection* tabs
	                               , const OpTabAPIListener::TabWindowIdCollection* tab_windows);

	virtual void NotifyQueryTab(OP_STATUS status
	                          , const OpTabAPIListener::ExtensionTabInfo* info);

	virtual void NotifyBrowserWindowMoveResize(OP_STATUS status) { SetNotificationResult(status, QueryResult::RESULT_REQUEST_WINDOW_MOVE_RESIZE); }
	virtual void NotifyTabClose(OP_STATUS status) { SetNotificationResult(status, QueryResult::RESULT_REQUEST_TAB_CLOSE); }
	virtual void NotifyTabGroupClose(OP_STATUS status) { SetNotificationResult(status, QueryResult::RESULT_REQUEST_TAB_GROUP_CLOSE); }
	virtual void NotifyBrowserWindowClose(OP_STATUS status) { SetNotificationResult(status,  QueryResult::RESULT_REQUEST_WINDOW_CLOSE); }
	virtual void NotifyBrowserWindowCreate(OP_STATUS status, OpTabAPIListener::TabAPIItemId window_id);
	virtual void NotifyTabMove(OP_STATUS status, OpTabAPIListener::TabAPIItemId browser_window_id, OpTabAPIListener::TabAPIItemId tab_group_id);
	virtual void NotifyTabGroupMove(OP_STATUS status, OpTabAPIListener::TabAPIItemId browser_window_id);
	virtual void NotifyTabGroupUpdate(OP_STATUS status) { SetNotificationResult(status, QueryResult::RESULT_REQUEST_TAB_GROUP_UPDATE); }
	virtual void NotifyTabUpdate(OP_STATUS status) { SetNotificationResult(status, QueryResult::RESULT_REQUEST_TAB_UPDATE); }

	/**
	 * Result of performed query
	 */ 
	struct QueryResult
	{
		void Clean();
		enum ResultType
		{
			RESULT_UNINITIALIZED,

			RESULT_QUERY_ALL_WINDOWS,       ///< value in value.query_all_windows
			RESULT_QUERY_ALL_TAB_GROUPS,    ///< value in value.query_all_tab_groups
			RESULT_QUERY_ALL_TABS,          ///< value in value.query_all_tabs
			
			RESULT_QUERY_WINDOW,            ///< value in value.query_window
			RESULT_QUERY_TAB_GROUP,         ///< value in value.query_tab_group
			RESULT_QUERY_TAB,               ///< value in value.query_tab
			RESULT_REQUEST_WINDOW_CREATE,    ///< value in value.created_window_id
			RESULT_REQUEST_TAB_GROUP_CREATE, ///< value in value.created_tab_group_id
			RESULT_REQUEST_TAB_CREATE,       ///< value in value.created_tab_group_id

			RESULT_REQUEST_WINDOW_MOVE_RESIZE, ///< only status
			RESULT_REQUEST_WINDOW_CLOSE,       ///< only status
			RESULT_REQUEST_TAB_CLOSE,          ///< only status
			RESULT_REQUEST_TAB_GROUP_CLOSE,    ///< only status
			RESULT_REQUEST_TAB_MOVE,           ///< only status
			RESULT_REQUEST_TAB_UPDATE,         ///< only status
			RESULT_REQUEST_TAB_GROUP_MOVE,     ///< only status
			RESULT_REQUEST_TAB_GROUP_UPDATE,  ///< only status
		};

		ResultType type;
		union
		{	
			OpTabAPIListener::ExtensionBrowserWindowInfo query_window;
			OpTabAPIListener::ExtensionTabGroupInfo query_tab_group;
			OpTabAPIListener::ExtensionTabInfo query_tab;
			
			OpTabAPIListener::TabAPIItemId created_window_id;
			OpTabAPIListener::TabAPIItemId      created_tab_group_id;
			struct
			{
				OpTabAPIListener::TabAPIItemId  tab_id;
				unsigned int                           window_id;
			} created_tab_info;

			struct
			{
				OpTabAPIListener::TabAPIItemId browser_window_id;
				OpTabAPIListener::TabAPIItemId      tab_group_id;
			} moved_to_info;
		} value;
		OpTabAPIListener::TabAPIItemIdCollection browser_windows;
		OpTabAPIListener::TabAPIItemIdCollection tab_groups;
		OpTabAPIListener::TabAPIItemIdCollection tabs;
		OpTabAPIListener::TabWindowIdCollection tab_windows;

		QueryResult();
		~QueryResult();
	};
	const QueryResult& GetResult() { return m_query_result; }

protected:
	virtual void OnNotification(OP_STATUS status, QueryResult::ResultType type) = 0;

	/**
	 * Result for operations which return any info besided success/error.
	 */ 
	QueryResult m_query_result;
private:

	/** Sets the notification type to the specified type and calls OnNotification().
	 *
	 * @param status is the success/error reported to the notification callbacks.
	 * @param type is the type of notification.
	 */
	void SetNotificationResult(OP_STATUS status, QueryResult::ResultType type);
};

#endif // WIC_TAB_API_SUPPORT

#endif // WINDOWCOMMANDER_TABSAPINOTIFICATIONHELPER_H
