/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef OP_TAB_API_LISTENER_H
#define OP_TAB_API_LISTENER_H

#ifdef WIC_TAB_API_SUPPORT

class OpWindowCommander;

/** The base interface for core to query Tab and Window structure of the UI.
 *
 * Its functionality includes querying of toplevel window information,
 * including the tabbed documents contained in such a toplevel window
 * as well as tab groups.
 *
 * The platform wishing to support extensions tab API must implement
 * the OpTabAPIListener interface and call the notification interfaces
 * to propagate events from the UI to core.
 *
 * @note some popular terms and conventions used in this interface regarding API:
 *     - "Browser window" is used for a top-level browser window. The Opera application
 *     may open multiple browser windows. A browser window may open one or more tabs.
 *     - "Tab window" or "tab" is used for single tab window which is inside
 *     a browser window. A tab may or may not contain one or more core OpWindowComander.
 *     This OpWindowCommander is constant throughout the whole lifetime of a tab.
 *     Multiple tabs may be combined to one tab group if a platform supports it.
 *     - "Tab group" groups multiple tabs to one group. In a tab toolbar which displays
 *     all open tabs of a browser window, the tab-group may be displayed as a collapsed
 *     or expanded element.
 *     - xxx_id == 0 - currently active browser window/tab.
 *     - position - The positions used in this interface are relative
 *     to the item's (tab or tab group) parent (browser window or tab group).
 *     For example a if we have a window with tabs as follows:
 *
 *     | tab_1 | tab_2 | tab_group_1[ tab_3 | tab_4 ] | tab_5 |
 *
 *     then the position of the items are as follow:
 *
 *     |   0   |   1   |      2     [   0   |   1   ] |  3    |
 *
 *     The position is 0 based. It is up to the platform to interpret the
 *     order left-to-right, right-to-left, top-down, ...
 */
class OpTabAPIListener
{
public:
	/** Platform provided unique ID for a platform tab, tab-group or top level browser window.
	 *  Tabs, tab-groups, and browser windows share one pool of unique ids.
	 *  Do NOT confuse this with id received WindowCommander::GetWindowId()!
	 */
	typedef UINT32 TabAPIItemId;

	/** Collection of tab/tab-group/browser window ids.
	 */
	typedef OpINT32Vector TabAPIItemIdCollection;

	/** Collection of window ids(as in WindowCommander::GetWindowId()).
	 *  Different typedef used to differentiate betwen browser window id and core window id.
	 */
	typedef OpINT32Vector TabWindowIdCollection;

	/** Core-supplied arguments to request creating a new top-level browser
	 * window.
	 *
	 * @see WindowTabInsertTarget
	 *
	 * @note A dimension can be set to UNSPECIFIED_WINDOW_DIMENSION to indicate
	 *  that the platform shall choose a sensible default value. */
	struct WindowCreateData
	{
		/** Requested screen position (in pixels) of the top of the new window.
		 * UNSPECIFIED_WINDOW_DIMENSION if no position requested/specified. */
		int top;

		/** Requested screen position (in pixels) of the left-hand edge of the
		 * window. UNSPECIFIED_WINDOW_DIMENSION if no position
		 * requested/specified. */
		int left;

		/** Requested width (in pixels) of the new window.
		 * UNSPECIFIED_WINDOW_DIMENSION if no value requested/specified. */
		int width;

		/** Requested height (in pixels) of the new window.
		 * UNSPECIFIED_WINDOW_DIMENSION if no value requested/specified. */
		int height;

		/** If TRUE then the window shall be maximized.
		 *
		 * @note If a window is maximized on a multi-screen environment and the
		 *  window-rectangle covers more than one screen, then this rectangle
		 *  should be used to choose the screen on which to maximize the
		 *  window. A possible algorithm is to choose the screen which is at the
		 *  center of the window-rectangle. So even if the specified position
		 *  and size does not determine the position and size of the maximized
		 *  window, it should be used to determine on which screen to maximize
		 *  it. */
		bool maximized;

		/** If set to TRUE, the window will be in private browsing mode. All new
		 * tabs created in it will default to private browsing mode. */
		bool is_private;
	};

	/** Core-supplied arguments to request creating a new tab-group.
	 *
	 * @see WindowTabInsertTarget
	 */
	struct TabGroupCreateData
	{
		/** If TRUE then the tab-group shall be visually collapsed.
		 */
		bool collapsed;
	};

	/** Specifies position in window/tab structure. Used as a target for
	 *  tab/tab-group creation and move operations. */
	struct WindowTabInsertTarget
	{
		/** Specifies type of position specification. */
		enum Type
		{
			/** Create a new top-level browser window for a tab/tab-group.
			 *  dst_group_id, dst_window_id and insert_before_item_id are ignored.
			 */
			NEW_BROWSER_WINDOW,
			/** The tab/tab-group will be placed in existing top-level browser window
			 * specified by dst_window_id and its position by insert_before_item_id.
			 * dst_group_id is ignored.
			 */
			EXISTING_BROWSER_WINDOW,
			/** Create a new tab-group for a tab. The window in which
			 *  the tab-group will be created is specified by dst_window_id
			 *  and its position by insert_before_item_id.
			 *  dst_group_id is ignored.
			 */
			NEW_TAB_GROUP,
			/** The tab will be placed in existing tab-group
			 * specified by dst_group_id and its position by insert_before_item_id.
			 * dst_window_id is ignored.
			 */
			EXISTING_TAB_GROUP,
			/** The tab will be placed in its current container if it's a move
			 *  or in the currently active window if it's create. The position
			 *  is specified by insert_before_item_id.
			 *  dst_group_id and dst_window_id are ignored.
			 */
			CURRENT_CONTAINER
		};

		Type type;

		/** Specifies position at which tab/tab-group will be placed in its container or
		 *  position at wich the newly constructed tab-group will be placed in its
		 *  top-level browser window. Negative position means append at the end of container. */

		/** Id of a tab-group in which the tab will be placed in.
		 *  0 means move/create within current. */
		TabAPIItemId dst_group_id;

		/** Id of a browser window in which the tab/tab-group will be placed in.
		 *  0 means move/create within current. */
		TabAPIItemId dst_window_id;

		/** Id of a tab or a tab-group before which the tab/tab-group will be placed in.
		 *  0 means place at the end of container.  A valid insert_before_item_id  MUST be
		 *  a direct child of the container specified by other attributes (dst_group/window_id)
		 *  or if container is not specified at least with the type - ie. for 
		 *  type == EXISTING_BROWSER_WINDOW before_item_id cannot specify grouped tab.
		 */
		TabAPIItemId before_item_id;

		union
		{
			/** Additional create data. Only valid for NEW_BROWSER_WINDOW type. */
			WindowCreateData window_create;
			/** Additional create data. Only valid for NEW_TAB_GROUP type. */
			TabGroupCreateData tab_group_create;
		};
	};

	/** Special value of dimension (top, left, width or height) which indicates
	 *  That called code should ignore this value or use default.
	 */
	enum { UNSPECIFIED_WINDOW_DIMENSION = INT_MIN };

	/** Platform provided information about a single top-level browser window.	 *
	 * @see OnQueryBrowserWindow() */
	struct ExtensionBrowserWindowInfo
	{
		/** ID of the top-level browser window. 0 if not available. */
		TabAPIItemId id;

		/** ID of the window's parent or 0 the window has no parent. */
		TabAPIItemId parent_id;

		/** If TRUE, toplevel window is known to be currently active/focused.
		 * FALSE if not active/focused or unknown. */
		bool has_focus;

		/** The screen position (in pixels) for the top of the window. */
		int top;

		/** The screen position (in pixels) for the leftmost edge of the window.
		 */
		int left;

		/** The width (in pixels) of the window. */
		int width;

		/** The height (in pixels) of the window. */
		int height;

		/** Set to TRUE if the window is in private browsing mode. */
		bool is_private;

		/** Currently selected tab id in the window. */
		TabAPIItemId selected_tab_id;

		/** Currently selected tab window id in the window.
		    May be 0 if there is no core window assigned to this tab.*/
		TabAPIItemId selected_tab_window_id;
	};

	/** Platform provided information about a single tab group.
	 *
	 * @see OnQueryTabGroup() */
	struct ExtensionTabGroupInfo
	{
	public:
		/** ID of the tab-group. */
		TabAPIItemId id;

		/** ID of the top-level browser window which contains the tab group.
		 * If the ID is set to 0, then the information is unavailable or
		 * unknown. */
		TabAPIItemId browser_window_id;

		/** If TRUE then one tab in this group is currently selected/focused.
		 * FALSE if no tab in this groups is selected/focused or unknown. */
		bool is_selected;

		/** TRUE if the group is visually collapsed, FALSE otherwise. */
		bool is_collapsed;

		/** 0-based position of the tab-group in the top-level browser window.
		 *
		 * @note The interpretation of the order (left-to-right / top-to-bottom)
		 *  is up to the platform. */
		unsigned int position;

		/** Currently selected tab id in the tab group. */
		TabAPIItemId selected_tab_id;

		/** Currently selected tab window id in the tab group.
		    May be 0 if there is no core window assigned to this tab.*/
		unsigned int selected_tab_window_id;
	};

	/** Platform provided information about a single tab.
	 *
	 * @see OnQueryTab() */
	struct ExtensionTabInfo
	{
		/** Id of the tab. */
		TabAPIItemId id;

		/** Window id of the tab.
		    May be 0 if there is no core window assigned to this tab.*/
		TabAPIItemId window_id;

		/** ID of the top-level browser window which contains the tab.
		 * If the ID is set to 0, then the information is unavailable or
		 * unknown. */
		TabAPIItemId browser_window_id;

		/** ID of the tab group which contains the tab. If the ID is set to 0,
		 * then the tab is not grouped. */
		TabAPIItemId tab_group_id;

		/** The title of the tab. May be NULL if not provided. */
		const uni_char* title;

		/** 0-based position of the tab in its container. If tab_group_id is 0,
		 * then this is the position of the tab in the top-level browser window.
		 * If tab_group_id is not 0, then this is the position of the tab in the
		 * tab group.
		 *
		 * @note The interpretation of the order (left-to-right / top-to-bottom)
		 *  is up to the platform. */
		unsigned int position;

		/** If TRUE, then this tab is locked.
		 *  @see OpUiWindowListener::CreateUIWindowArgs::locked.
		 */
		bool is_locked;

		/** If TRUE, then this tab is in private browsing mode. */
		bool is_private;

		/** If TRUE, then this tab is currently selected in its container. */
		bool is_selected;
	};

	/** Core-supplied arguments to request moving or resizing a top-level
	 * browser window.
	 *
	 * @see OnRequestBrowserWindowMoveResize()
	 *
	 * @note A dimension can be set to UNSPECIFIED_WINDOW_DIMENSION to indicate
	 *  that the value shall not be changed. */
	struct ExtensionWindowMoveResize
	{
		/** Requested new screen position (in pixels) for the top of the window.
		 * UNSPECIFIED_WINDOW_DIMENSION if no change is requested. */
		int window_top;

		/** Requested new screen position (in pixels) for the left-hand edge of
		 * the window. UNSPECIFIED_WINDOW_DIMENSION if no change is requested.
		 */
		int window_left;

		/** Requested new width (in pixels) of the window.
		 * UNSPECIFIED_WINDOW_DIMENSION if no change is requested. */
		int window_width;

		/** Requested new height (in pixels) of the window.
		 * UNSPECIFIED_WINDOW_DIMENSION if no change is requested. */
		int window_height;

		/** If TRUE then the window shall be maximized.
		 *
		 * @note If a window is maximized on a multi-screen environment and the
		 *  window-rectangle covers more than one screen, then this rectangle
		 *  should be used to choose the screen on which to maximize the
		 *  window. A possible algorithm is to choose the screen which is at the
		 *  center of the window-rectangle. So even if the specified position
		 *  and size does not determine the position and size of the maximized
		 *  window, it should be used to determine on which screen to maximize
		 *  it. */
		BOOL maximize;
	};

	/** The notification interface for Windows + Tabs requests.
	 *
	 * Core passes an instance of this class to each \c OnRequest* and
	 * \c OnQuery* method of the OpTabAPIListener interface. The platform
	 * implementation of the OpTabAPIListener interface shall report the result
	 * of the request/query to core via a suitable method of this callback
	 * class. */
	class ExtensionWindowTabNotifications
	{
	public:
		virtual ~ExtensionWindowTabNotifications() {}

		/** The platform reports the result of the query
		 * OpTabAPIListener::OnQueryAllBrowserWindows() through this method.
		 *
		 * If the query was successful, \c info holds an array of all top-level
		 * browser window IDs. The ownership of \c info remains with the
		 * platform, and should NOT be assumed to live beyond the end of the
		 * notification.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnQueryAllBrowserWindows() query.
		 *
		 * @param status The success of the query.
		 * @param all_windows On success the pointer to the requested data or NULL if
		 *  \c status != OpStatus::OK.
		 *  @note The ordering in the collection of all top-level browser
		 *  windows is arbitrary and no information or relationships between the
		 *  windows should be inferred from it. */
		virtual void NotifyQueryAllBrowserWindows(OP_STATUS status,
		                                          const TabAPIItemIdCollection* all_windows) = 0;

		/** The platform reports the result of the query
		 * OpTabAPIListener::OnQueryAllTabGroups() through this method.
		 *
		 * If the query was successful, \c info holds an array of all
		 * tab-group IDs. The ownership of \c info remains with the platform,
		 * and should NOT be assumed to live beyond the end of the
		 * notification.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnQueryAllTabGroups() query.
		 *
		 * @param status The success of the query.
		 * @param all_tab_groups On success the pointer to the requested data
		 * or NULL if \c status != OpStatus::OK.
		 *  @note The ordering in the collection of all tab group IDs is
		 *  arbitrary and no information or relationships between the tab groups
		 *  should be inferred from it. */
		virtual void NotifyQueryAllTabGroups(OP_STATUS status,
		                                     const TabAPIItemIdCollection* all_tab_groups) = 0;

		/** The platform reports the result of the query
		 * OpTabAPIListener::OnQueryAllTabs() through this method.
		 *
		 * If the query was successful, \c info holds an array of TabEntry. The
		 * ownership of \c info remains with the platform, and should NOT be
		 * assumed to live beyond the end of the notification.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnQueryAllTabs() query.
		 *
		 * @param status The success of the query.
		 * @param all_tabs Vector of id's of all the tabs or NULL if
		 *  \c status != OpStatus::OK.
		 * @param all_tabs_window_ids Vector of id's of all the core windows
		 * or NULL if \c status != OpStatus::OK.
		 * The order is the same as all_tabs ie. all_tabs_window_ids[n] is
		 * an id of a window of a tab of id all_tabs[n].
		 *  \c status != OpStatus::OK.
		 *  @note The ordering in the collection of all tab IDs is arbitrary and
		 *  no information or relationships between the tabs should be inferred
		 *  from it. */
		virtual void NotifyQueryAllTabs(OP_STATUS status,
		                                const TabAPIItemIdCollection* all_tabs,
		                                const TabWindowIdCollection* all_tabs_window_ids) = 0;

		/** The platform reports the result of the query
		 * OpTabAPIListener::OnQueryTab() through this method.
		 *
		 * If the query was successful, \c info holds information about the
		 * requested tab. The ownership of \c info remains with the platform,
		 * and should NOT be assumed to live beyond the end of the notification.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnQueryTab() query.
		 *
		 * @param status The success of the query.
		 * @param info On success the pointer to the requested data or NULL if
		 *  \c status != OpStatus::OK. */
		virtual void NotifyQueryTab(OP_STATUS status, const ExtensionTabInfo *info) = 0;

		/** The platform reports the result of the query
		 * OpTabAPIListener::OnQueryBrowserWindow() through this method.
		 *
		 * If the query was successful, \c info holds the information about the
		 * requested top-level browser window. The ownership of \c info remains
		 * with the platform, and should NOT be assumed to live beyond the end
		 * of the notification.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnQueryBrowserWindow() query.
		 *
		 * @param status The success of the query.
		 * @param info On success the pointer to the requested data or NULL if
		 *  \c status != OpStatus::OK.
		 * @param groups Vector of id's of tab groups in this window. Might be
		 * NULL if content of a window was not requested.
		 * @param tabs Vector of id's of tabs in this window. Might be NULL if
		 * content of a window was not requested.
		 * @param tab_windows Vector of id's of core windows tabs in this window.
		 * Might be NULL if content of a window was not requested.
		 * The order is the same as tabs ie. tabs_windows[n] is
		 * an id of a window of a tab of id tabs[n].
		 */
		virtual void NotifyQueryBrowserWindow(OP_STATUS status,
		                                      const ExtensionBrowserWindowInfo* info,
		                                      const TabAPIItemIdCollection* groups,
		                                      const TabAPIItemIdCollection* tabs,
		                                      const TabWindowIdCollection* tabs_windows) = 0;

		/** The platform reports the result of the query
		 * OpTabAPIListener::OnQueryTabGroup() through this method.
		 *
		 * If the query was successful, \c info holds the information about the
		 * requested tab group. The ownership of \c info remains with the
		 * platform, and should NOT be assumed to live beyond the end of the
		 * notification.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnQueryTabGroup() query.
		 *
		 * @param status The success of the query.
		 * @param info On success the pointer to the requested data or NULL if
		 *  \c status != OpStatus::OK.
		 * @param tabs Vector of id's of tabs in this tab group. Might be NULL if
		 * content of a tab group was not requested.
		 * @param tab_windows Vector of id's of core windows tabs in this tab group.
		 * Might be NULL if content of a tab group was not requested.
		 * The order is the same as tabs ie. tabs_windows[n] is
		 * an id of a window of a tab of id tabs[n].
		*/
		virtual void NotifyQueryTabGroup(OP_STATUS status,
		                                 const ExtensionTabGroupInfo *info,
		                                 const TabAPIItemIdCollection* tabs,
		                                 const TabWindowIdCollection* tab_windows) = 0;

		/** The platform reports the result of the request
		 * OpTabAPIListener::OnRequestBrowserWindowClose() through this method.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestBrowserWindowClose()
		 *  request.
		 *
		 * @param status The success of closing the window. */
		virtual void NotifyBrowserWindowClose(OP_STATUS status) = 0;

		/** The platform reports the result of the request
		 * OpTabAPIListener::OnRequestBrowserWindowMoveResize() through this
		 * method.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestBrowserWindowMoveResize()
		 *  request.
		 *
		 * @param status The success of moving/resizing the window. */
		virtual void NotifyBrowserWindowMoveResize(OP_STATUS status) = 0;

		/** The platform reports the result of the request
		 * OpTabAPIListener::OnRequestTabGroupClose() through this methods.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestTabGroupClose() request.
		 *
		 * @param status The success of closing the tab group. */
		virtual void NotifyTabGroupClose(OP_STATUS status) = 0;

		/** The platform reports the result of the request
		 * OpTabAPIListener::OnRequestTabGroupMove() through this method.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestTabGroupMove() request.
		 *
		 *  @param browser_window_id Id of a browser window to which the tab-group was moved.
		 * @param status The success of moving the tab group. */
		virtual void NotifyTabGroupMove(OP_STATUS status, TabAPIItemId browser_window_id) = 0;

		/** The platform reports the result of the request
		 * OpTabAPIListener::OnRequestTabGroupUpdate() through this method.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestTabGroupUpdate() request.
		 *
		 * @param status The success of updating the tab group. */
		virtual void NotifyTabGroupUpdate(OP_STATUS status) = 0;

		/** The platform reports the result of the request
		 * OpTabAPIListener::OnRequestTabClose() through this method.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestTabClose() request.
		 *
		 * @param status The success of closing the tab. */
		virtual void NotifyTabClose(OP_STATUS status) = 0;

		/** The platform reports the result of the request
		 *  OpTabAPIListener::OnRequestTabMove() through this method.
		 *
		 *  @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestTabMove() request.
		 *
		 *  @param browser_window_id Id of a browser window to which the tab was moved.
		 *  @param tab_group_id Id of a tab group to which the tab was moved.
		 *                      0 if the tab hasn't been moved to tab group.
		 *  @param status The success of moving the tab. */
		virtual void NotifyTabMove(OP_STATUS status, TabAPIItemId browser_window_id, TabAPIItemId tab_group_id) = 0;

		/** The platform reports the result of the request
		 * OpTabAPIListener::OnRequestTabUpdate() through this method.
		 *
		 * @note The OpTabAPIListener implementation must call this method for
		 *  each received OpTabAPIListener::OnRequestTabUpdate() request.
		 *
		 * @param status The success of updating the tab. */
		virtual void NotifyTabUpdate(OP_STATUS status) = 0;
	};

	/** @name Tab&Windows Queries
	 *  @{ */

	/** Core requests the array of TabAPIItemId for the current set
	 * of toplevel browser windows. The array of ids shall be passed in the
	 * struct ExtensionBrowserWindowCollectionInfo to the specified \c callback.
	 *
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyQueryAllBrowserWindows()
	 *        to report the array of all toplevel window ids.
	 */
	virtual void OnQueryAllBrowserWindows(ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests the array of TabAPIItemId for the current set of
	 * tab groups.
	 *
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyQueryAllTabGroups() to
	 *        report the array of all tab groups.
	 */
	virtual void OnQueryAllTabGroups(ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests information about the current set of tabs.
	  *
	  * @param callbacks reference to the object implementing the notification
	  *        interface for the operation. The platform will notify caller
	  *        via its NotifyQueryAllTabs() method of outcome.
	  * @return All return values shoud be reported via calllbacks.
	  */
	virtual void OnQueryAllTabs(ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests information about a top-level browser window and
	 * optionally its content.
	 *
	 * @param browser_window_id ID of the top-level browser window for which the
	 *        ExtensionBrowserWindowInfo is requested. If the ID is 0, then
	 *        information about the currently active top-level browser window is
	 *        requested.
	 * @param include_contents Set to TRUE to request information about the tabs
	 *        and tab groups in the top-level browser window. The implementation
	 *        of this method shall fill the attributes
	 *        ExtensionBrowserWindowInfo::tabs and
	 *        ExtensionBrowserWindowInfo::tab_groups in this case. If the
	 *        argument is FALSE, then no information about the window's tabs or
	 *        tab groups is requested and the respective
	 *        ExtensionBrowserWindowInfo attributes shall be set to 0.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyQueryBrowserWindow() to
	 *        report the ExtensionBrowserWindowInfo. */
	virtual void OnQueryBrowserWindow(TabAPIItemId browser_window_id, BOOL include_contents, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests information about a tab group and optionally its content.
	 *
	 * @param tab_group_id ID of the tab group for which the
	 *        ExtensionTabGroupInfo is requested.
	 * @param include_contents Set to TRUE to request information about the tabs
	 *        in the specified \c tab_group_id. The implementation of this
	 *        method shall fill the attributes ExtensionTabGroupInfo::tabs in
	 *        this case. If the argument is FALSE, then no information about the
	 *        group's tabs is requested and the respective ExtensionTabGroupInfo
	 *        attributes shall be set to 0.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyQueryTabGroup() to
	 *        report the ExtensionTabGroupInfo. */
	virtual void OnQueryTabGroup(TabAPIItemId tab_group_id, BOOL include_contents, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests information about a tab.
	 *
	 * @param target_window OpWindowCommander associated to a tab, for which the
	 *        ExtensionTabInfo is requested. If NULL then the information is
	 *        requested for the currently active tab.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyQueryTab() to report the
	 *        ExtensionTabInfo. */
	virtual void OnQueryTab(OpWindowCommander* target_window, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests information about a tab.
	 *
	 * @param target_tab_id ID of the tab, for which the ExtensionTabInfo is
	 *        requested. If 0 then the information is requested for the
	 *        currently active tab.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyQueryTab() to report the
	 *        ExtensionTabInfo. */
	virtual void OnQueryTab(TabAPIItemId target_tab_id, ExtensionWindowTabNotifications* callbacks) = 0;

	/** @} */

	/** @name Tab&Windows Operations
	 * @{ */

	/** Core requests to close a top-level browser window.
	 *  When the browser window is closed, all its tabs and tab-groups shall be closed
	 *  as well. The platform implementation shall send ExtensionWindowTabActionListener:ACTION_CLOSE
	 *  event for each closed tab and tab-group before sending the close-event for the browser window.
	 *
	 * @note The platform and user are in control. If the request is denied, the
	 *  callback ExtensionWindowTabNotifications::NotifyBrowserWindowCreate()
	 *  shall be called with status == OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param browser_window_id ID of the top-level browser window to close.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyBrowserWindowClose() to
	 *        report the success of closing the window. */
	virtual void OnRequestBrowserWindowClose(TabAPIItemId browser_window_id, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests to move and/or resize a top-level browser window.
	 *
	 * @note The platform and user are in control. If the request is denied, the
	 *  callback ExtensionWindowTabNotifications::NotifyBrowserWindowMoveResize()
	 *  shall be called with status == OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param browser_window_id ID of the top-level browser window to move
	 *        and/or resize.
	 * @param window_move_info Requested new position and size for the window.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyBrowserWindowMoveResize()
	 *        to report the success of this operation. */
	virtual void OnRequestBrowserWindowMoveResize(TabAPIItemId browser_window_id, const ExtensionWindowMoveResize &window_move_info, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests to close a tab group and all tabs inside the group.
	 *  When the tab group is closed, all its tabs shall be closed
	 *  as well. The platform implementation shall send ExtensionWindowTabActionListener:ACTION_CLOSE
	 *  event for each closed tab before sending the close-event for the tab group.
	 *
	 * @note The platform and user are in control. If the request is denied, the
	 *  callback ExtensionWindowTabNotifications::NotifyTabGroupClose()
	 *  shall be called with status == OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param tab_group_id ID of the tab group to close.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyTabGroupClose() to report
	 *        the success of closing the tab group. */
	virtual void OnRequestTabGroupClose(TabAPIItemId tab_group_id, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests to move a tab group.
	 *
	 * @note The platform and user are in control. If the request is denied, the
	 *  callback ExtensionWindowTabNotifications::NotifyTabGroupClose()
	 *  shall be called with status == OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param tab_group_id ID of the tab group to move.
	 *
	 * @param insert_target Specifies the position in window/ tab structure
	 *        where the window should be moved to.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyTabGroupMove() to report
	 *        the success of moving the tab group. */
	virtual void OnRequestTabGroupMove(TabAPIItemId tab_group_id, const WindowTabInsertTarget& insert_target, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests to update a tab group.
	 *
	 * For now the only attribute that may be updated is the collapsed state.
	 *
	 * @note The platform and user are in control. If the request is denied, the
	 *  callback ExtensionWindowTabNotifications::NotifyTabGroupClose()
	 *  shall be called with status == OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param tab_group_id ID of the tab group to be updated.
	 * @param collapsed New collapsed state of the group. Possible values are:
	 *        - \c collapsed > 0: collapse;
	 *        - \c collapsed == 0: expand;
	 *        - \c collapsed < 0: don't change.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyTabGroupUpdate() to report
	 *        the success of updating the tab group. */
	virtual void OnRequestTabGroupUpdate(TabAPIItemId tab_group_id, int collapsed, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests to close a tab.
	 *
	 * @note The platform and user are in control. If the request is denied, the
	 *  callback ExtensionWindowTabNotifications::NotifyTabGroupClose()
	 *  shall be called with status == OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param tab_id ID of the tab to close.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyTabClose() to report the
	 *        success of updating the tab group. */
	virtual void OnRequestTabClose(TabAPIItemId tab_id, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests to move a tab.
	 *
	 * @note The platform and user are in control. If the request is denied, the
	 *  callback ExtensionWindowTabNotifications::NotifyTabGroupClose()
	 *  shall be called with status == OpStatus::ERR_NOT_SUPPORTED.
	 *
	 * @param tab_id ID of the tab to move.
	 * @param insert_target Specifies the position in window/ tab structure
	 *        where the window should be moved to.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyTabMove() to report the
	 *        success of moving the tab. */
	virtual void OnRequestTabMove(TabAPIItemId tab_id, const WindowTabInsertTarget& insert_target, ExtensionWindowTabNotifications* callbacks) = 0;

	/** Core requests to update a tab.
	 *
	 * @param tab_id ID of the tab to update.
	 * @param set_focus Specifies if the tab shall receive focus. Possible
	 *        values:
	 *        - TRUE: The tab shall receive focus.
	 *        - FALSE: The current focus shall not be changed.
	 * @param set_locked Specifies the locked state of a tab. Possible values:
	 *        - set_locked > 0: Enable locked state for the tab.
	 *        - set_locked == 0: Disable locked state for the tab.
	 *        - set_locked < 0: Don't change the locked state.
	 * @see OpUiWindowListener::CreateUIWindowArgs::locked.
	 * @param set_url Specifies a new url to open in the tab. If NULL then don't
	 *        change the url.
	 * @param set_title Specifies the new title for the tab. If NULL then don't
	 *        change the title. To clear the current title specify an empty
	 *        string.
	 * @param callbacks The implementation of this method shall call
	 *        ExtensionWindowTabNotifications::NotifyTabUpdate() to report the
	 *        success of updating the tab. */
	virtual void OnRequestTabUpdate(TabAPIItemId tab_id, BOOL set_focus, int set_locked, const uni_char* set_url, const uni_char* set_title, ExtensionWindowTabNotifications* callbacks) = 0;

	/** @} */

	/** Core uses this api to be notified by the UI about user operations on
	 * windows and tabs.
	 *
	 * When core registers an instance of this class via
	 * OpTabAPIListener::OnRegisterWindowTabActionListener(), then the UI shall
	 * notify this listener about the respective events. */
	class ExtensionWindowTabActionListener
	{
	public:
		enum ActionType
		{
			ACTION_CREATE,          ///< Tab/group/window was created.
			ACTION_CLOSE,           ///< Tab/group/window was closed.
			ACTION_FOCUS,           ///< Tab/window was focused.
			ACTION_BLUR,            ///< Tab/window was un-focused.
			ACTION_MOVE,            ///< Tab/group/window was moved.
			ACTION_MOVE_FROM_GROUP, ///< Tab was moved from tab-group.
			ACTION_UPDATE           ///< Tab/group/window was updated.
		};

		/** Additional data sent with some events
		 *  Currently used only foe MOVE/MOVE_FROM_TAB_GROUP, but
		 *  for extensibility it is union. */
		union ActionData
		{
			struct
			{
				UINT32 previous_parent_id; ///< P Window id d
				int position;
			} move_data;
		};

		virtual ~ExtensionWindowTabActionListener() {}

		/** The UI calls this method when any of the following events happened
		 *  in some top-level browser window.
		 *
		 *  @param action type of the action that happened. Accepted types:
		 *  - ACTION_CREATE
		 *  - ACTION_CLOSE
		 *  - ACTION_FOCUS
		 *  - ACTION_BLUR
		 *  - ACTION_UPDATE
		 *  - ACTION_BLUR
		 *  - ACTION_BLUR
		 *
		 *  @param window_id Is the ID of the top-level browser-window for which
		 *         the event occurred.
		 *  @param data - Additional data for the event.
		 *         As none of these events take extra data this is now always NULL for now.
		 */
		virtual void OnBrowserWindowAction(ActionType action, TabAPIItemId window_id, ActionData* data) = 0;

		/** The UI calls this method when any of the following events happened
		 * in some tab.
		 *
		 *  @param action type of the action that happened. Accepted types:
		 *  - ACTION_CREATE
		 *  - ACTION_CLOSE
		 *  - ACTION_FOCUS
		 *  - ACTION_BLUR
		 *  - ACTION_UPDATE
		 *  - ACTION_MOVE_FROM_GROUP
		 *  - ACTION_MOVE
		 *  @param tab_id Is the ID of the tab for which the event happened.
		 *  @param tab_window_id is the associated core Window (same id as 
		 *         returned OpWindowCommander::GetWindowId()).
		 *  @param data - Additional data for the event.
		 *         Only specified for ACTION_MOVE_FROM_GROUP and ACTION_MOVE, NULL otherwise.
		 */
		virtual void OnTabAction(ActionType action, TabAPIItemId tab_id, unsigned int tab_window_id, ActionData* data) = 0;

		/** The UI calls this method when any of the following events happened
		 * in some tab group.
		 *
		 *  @param action type of the action that happened. Accepted types:
		 *  - ACTION_CREATE
		 *  - ACTION_CLOSE
		 *  - ACTION_UPDATE
		 *  - ACTION_MOVE
		 *
		 * @param tab_group_id Is the ID of the tab-group for which to report
		 *        the event.
		 *  @param data - Additional data for the event.
		 *         Only specified for ACTION_MOVE, NULL otherwise.
		 */
		virtual void OnTabGroupAction(ActionType action, TabAPIItemId tab_group_id, ActionData* data) = 0;
	};

	/** Core calls this method to register a listener for events
	 * related to top-level browser window or tab operations.
	 *
	 * The UI shall report any of the listener events to all registered listener
	 * instances.
	 *
	 * @param callbacks the listener interface to use when reporting the
	 *        events.
	 * @retval OpStatus::OK on successfully registering the
	 *         listener.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if the UI does not support reporting
	 *         tab/window notifications. Core should interpret this as absolute;
	 *         that is, successive calls to register a listener will similarly
	 *         fail and should not be attempted.
	 * @retval OpStatus::ERR_NO_MEMORY if there is not enough memory to register
	 *         a listener.
	 */
	virtual OP_STATUS OnRegisterWindowTabActionListener(ExtensionWindowTabActionListener *callbacks) = 0;

	/** Core calls this method to remove a listener for events
	 * related to top-level browser window or tab operations.
	 *
	 * @param callbacks the listener interface remove.
	 * @retval OpStatus::OK on successfully removing the
	 *         listener.
	 * @retval OpStatus::ERR_NOT_SUPPORTED if the UI does not support reporting
	 *         tab/window notifications. Core should interpret this as absolute;
	 *         that is, successive calls to register a listener will similarly
	 *         fail and should not be attempted.
	 * @retval OpStatus::ERR if the specified listener wasn't registered.
	 */
	virtual OP_STATUS OnUnRegisterWindowTabActionListener(ExtensionWindowTabActionListener *callbacks) = 0;
};

#endif // WIC_TAB_API_SUPPORT

#endif // OP_TAB_API_LISTENER_H
