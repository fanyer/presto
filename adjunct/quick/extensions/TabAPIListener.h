/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef EXTENSION_TAB_API_LISTENER_H
#define EXTENSION_TAB_API_LISTENER_H

#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "modules/util/OpTypedObject.h"
#include "modules/windowcommander/OpTabAPIListener.h"

class TabAPIListener:
	public OpTabAPIListener,
	public Application::BrowserDesktopWindowListener,
	public OpWorkspace::WorkspaceListener,
	public DesktopWindowCollection::Listener,
	public MessageObject
{
public:

	TabAPIListener(): m_supress_this_moving(NULL), m_dualton_id(OpTypedObject::GetUniqueID()) {}
	~TabAPIListener();

	OP_STATUS Init();

	/* Helper function implementing the OpUiWindowListener::CreateUiWindow
	 * with WINDOWTYPE_REGULAR.
	 *
	 * @param wc   The window commander passed to the CreateUiWindow
	 * @param args The description of the request. If the insert_target is
	 *             null, the tab will be opened in the current window.
	 **/
	TabAPIItemId CreateNewTab(OpWindowCommander* wc,
		const OpUiWindowListener::CreateUiWindowArgs& args);

	// OpTabAPIListener
	virtual void OnQueryTab(
		TabAPIItemId target_tab_id,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnQueryTab(
		OpWindowCommander* target_window,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnQueryAllBrowserWindows(
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnQueryAllTabGroups(
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnQueryAllTabs(
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnQueryBrowserWindow(
		TabAPIItemId window_id,
		BOOL include_contents,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnQueryTabGroup(
		TabAPIItemId tab_group_id,
		BOOL include_contents,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnRequestBrowserWindowMoveResize(
		TabAPIItemId window_id,
		const ExtensionWindowMoveResize &window_info,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnRequestTabClose(
		TabAPIItemId tab_id,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnRequestBrowserWindowClose(
		TabAPIItemId window_id,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnRequestTabMove(
		TabAPIItemId tab_id,
		const WindowTabInsertTarget& insert_target,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnRequestTabGroupMove(
		TabAPIItemId tab_group_id,
		const WindowTabInsertTarget& insert_target,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnRequestTabGroupClose(
		TabAPIItemId tab_group_id,
		ExtensionWindowTabNotifications* callbacks);

	virtual void OnRequestTabGroupUpdate(
		TabAPIItemId tab_group_id,
		int collapsed,
		ExtensionWindowTabNotifications* callbacks);

	virtual OP_STATUS OnRegisterWindowTabActionListener(
		ExtensionWindowTabActionListener *callbacks);

	virtual OP_STATUS OnUnRegisterWindowTabActionListener(
		ExtensionWindowTabActionListener *callbacks);

	virtual void OnRequestTabUpdate(
		TabAPIItemId tab_id,
		BOOL set_focus,
		int set_pinned,
		const uni_char* set_url,
		const uni_char* set_title,
		ExtensionWindowTabNotifications* callbacks);

	//
	// Methods that will be called by windows and tabs to inform about
	// their creation or destruction
	//
	// BrowserDesktopWindowListener methods
	//
	void OnBrowserDesktopWindowCreated(BrowserDesktopWindow *window) { GenerateWindowAction(ExtensionWindowTabActionListener::ACTION_CREATE, window->GetID()); }
	void OnBrowserDesktopWindowActivated(BrowserDesktopWindow *window) { GenerateWindowAction(ExtensionWindowTabActionListener::ACTION_FOCUS, window->GetID()); }
	void OnBrowserDesktopWindowDeactivated(BrowserDesktopWindow *window) { GenerateWindowAction(ExtensionWindowTabActionListener::ACTION_BLUR, window->GetID()); }
	void OnBrowserDesktopWindowDeleting(BrowserDesktopWindow *window);

	//
	// WorkspaceListener methods
	//
	virtual void OnWorkspaceAdded(OpWorkspace* workspace) {}
	virtual void OnWorkspaceDeleted(OpWorkspace* workspace) {}
	virtual void OnWorkspaceEmpty(OpWorkspace* workspace, BOOL has_minimized_window) {}
	virtual void OnDesktopWindowRemoved(OpWorkspace* workspace, DesktopWindow* desktop_window) {}
	virtual void OnDesktopWindowOrderChanged(OpWorkspace* workspace) {}
	virtual void OnDesktopWindowAttention(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL attention) {}
	virtual void OnDesktopWindowChanged(OpWorkspace* workspace, DesktopWindow* desktop_window) {}
	virtual void OnDesktopWindowAdded(OpWorkspace* workspace, DesktopWindow* desktop_window) {}
	virtual void OnDesktopWindowActivated(OpWorkspace* workspace,DesktopWindow* desktop_window, BOOL activate);
	virtual void OnDesktopWindowClosing(OpWorkspace* workspace, DesktopWindow* desktop_window, BOOL user_initiated) {}
	virtual void OnDesktopGroupCreated(OpWorkspace* workspace, DesktopGroupModelItem& group);
	virtual void OnDesktopGroupAdded(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void OnDesktopGroupRemoved(OpWorkspace* workspace, DesktopGroupModelItem& group) {}
	virtual void OnDesktopGroupDestroyed(OpWorkspace* workspace, DesktopGroupModelItem& group);
	virtual void OnDesktopGroupChanged(OpWorkspace* workspace, DesktopGroupModelItem& group);

	// from DesktopWindowCollection::Listener
	virtual void OnDesktopWindowAdded(DesktopWindow* window);
	virtual void OnDesktopWindowRemoved(DesktopWindow* window);
	virtual void OnDocumentWindowUrlAltered(DocumentDesktopWindow* document_window,	const OpStringC& url) {}
	virtual void OnCollectionItemMoved(DesktopWindowCollectionItem* item,
		                                   DesktopWindowCollectionItem* old_parent,
		                                   DesktopWindowCollectionItem* old_previous);

	// MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
private:
	enum Filter {
		ALL,
		ONLY_TOP_LEVEL,
		ONLY_TAB_GROUPS,
		ONLY_TABS
	};

	struct TabActionPostData {
		ExtensionWindowTabActionListener::ActionType action;
		TabAPIItemId tab_id;
		OpWindowCommander* tab_window_commander;
		ExtensionWindowTabActionListener::ActionData data;

		TabActionPostData(ExtensionWindowTabActionListener::ActionType action,
			TabAPIItemId tab_id,
			OpWindowCommander* tab_window_commander,
			const ExtensionWindowTabActionListener::ActionData& data)
			: action(action), tab_id(tab_id)
			, tab_window_commander(tab_window_commander)
		{
			op_memcpy(&this->data, &data, sizeof data);
		}
	};

	// With asychronous selftests, listener stopped being singleton and became a series of... dualtones:
	// the one registered for the actual work and a series of ones in each of the tests. This is
	// problematic for the MSG_TABAPI_TAB_ACTION, where data attached to the event is deleted on the first
	// entry into _any_ listener, so it must be serialized somehow to be only handled in a listener it originated from.
	INT32 m_dualton_id;

	static BOOL CheckFilter(DesktopWindowCollectionItem* item, Filter filter);
	static DesktopWindowCollectionItem* FindNextItem(DesktopWindowCollectionItem* cur, DesktopWindowCollectionItem* root, Filter filter);

	/** Locates an insert point for the new/moved item. The parent might represent a window or a group, inside which the item is to be searched.
	  * This is an interface-type of code, one of it's responsibilites is to translate the idea of "insert before" used in DOM to the idea
	  * of "insert after" used by Quick.
	  *
	  * Param parent is an out parameter only the caller calls this method with insert_before_id equal to zero and opening_new_tab equal to true.
	  * Otherwise, the parameter is never changed.
	  *
	  * @param [in, out] parent The root of the search.
	  * @param [in] insert_before_id The identifier of the tab _following_ the looked up tab. When set to 0, represents the
	  *             imaginary tab next to the end of the collection. The invalid id (id not in the tree model) is not permitted.
	  * @param [out] insert_after On success, a pointer to the insert item; on failure - undefined.
	  * @param [in] opening_new_tab hint for the lookup code to take opera:config#UserPrefs|OpenPageNextToCurrent into account
	  * @return true - the tab with a requested id was found in the collection, or the id was 0.
	  * @return false - the tab with a requested id was not found in the tree model of the collection.
	  */
	static bool FindInsertPosition(DesktopWindowCollectionItem*& parent, int insert_before_id, DesktopWindowCollectionItem*& insert_after, bool opening_new_tab);

	static int GetItemPosition(DesktopWindowCollectionItem* item);
	static DesktopWindow* GetToplevelWindowByID(INT32 id);
	static DesktopWindow* FindTabWindowByID(INT32 tab_id);
	static void MoveWindowToTheNearestScreen(DesktopWindow* browser);
	OP_STATUS GetTabInfo(OpTabAPIListener::ExtensionTabInfo& info, DesktopWindow* tab);

	void GenerateWindowAction(ExtensionWindowTabActionListener::ActionType action, TabAPIItemId window_id, ExtensionWindowTabActionListener::ActionData* data = NULL);
	void GenerateTabAction(ExtensionWindowTabActionListener::ActionType action, TabAPIItemId tab_id, OpWindowCommander* tab_window_commander, ExtensionWindowTabActionListener::ActionData* data = NULL);
	void GenerateTabActionDelayed(ExtensionWindowTabActionListener::ActionType action, TabAPIItemId tab_id, OpWindowCommander* tab_window_commander, const ExtensionWindowTabActionListener::ActionData& data);
	void GenerateTabGroupAction(ExtensionWindowTabActionListener::ActionType action, TabAPIItemId tab_group_id, ExtensionWindowTabActionListener::ActionData* data = NULL);

	static DesktopWindowCollectionItem* GetOrCreateInsertTarget(const WindowTabInsertTarget& data, DesktopWindowCollectionItem* item, TabAPIItemId& dst_group_id, TabAPIItemId& dst_window_id, BOOL open_background);

	/** Finds a top level item corresponding to window_id, if window_id is not 0. If window_id is 0, the current active browser window is used,
	 * unless helper_id is non-0. If window_id is 0 and helper_id is not 0, then a direct parent of the object with id equal to helper_id is used.
	 * This means, that in certain cases, a group, not a browser, is returned. This is to support calls such as o.e.windows.create(..., tab), where
	 * the tab is a groupped tab.
	 *
	 * @param window_id The id of the toplevel window in question. If set to 0, then helper_id is used to determine the window
	 * @param helper_id Used only, when window_id is 0. If set to 0, then the toplevel window is the currently active one;
	 *                  otherwise, it's the parent of the object with this id. The id might represent either tab or a group.
	 * @return The desktop window collection item described byt the two IDs.
	 */
	static DesktopWindowCollectionItem* GetContainerItem(TabAPIItemId window_id, TabAPIItemId helper_id);

	static OP_STATUS MoveItem(DesktopWindowCollectionItem* item, const WindowTabInsertTarget& insert_target, TabAPIItemId& dst_group_id, TabAPIItemId& dst_window_id);

	/** Opens the new tab inside the parent item. This function opens a tab on behalf of CreateNewTab, after the proper parent item is selected.
	  * @param wc Window commander to use for the new tab
	  * @param parent A parent selected based on args - either a browser window or tab group
	  * @param insert_pos Position to use for the new tab.
	  * @param args The description of the request.
	  */
	DocumentDesktopWindow* OpenNewTab(OpWindowCommander* wc, DesktopWindowCollectionItem* parent, int insert_before_id, const OpUiWindowListener::CreateUiWindowArgs &args);

	OpListeners<ExtensionWindowTabActionListener> m_listeners;
	DesktopWindowCollectionItem* m_supress_this_moving;

	static int MapDimension(int x) { return x == UNSPECIFIED_WINDOW_DIMENSION ? DEFAULT_SIZEPOS : x; }
	static void WinTabReport(const char* type, ExtensionWindowTabActionListener::ActionType action, TabAPIItemId window_id, TabAPIItemId group_id, TabAPIItemId tab_id);
};

#endif // EXTENSION_TAB_API_LISTENER_H
