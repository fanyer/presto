/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef DESKTOP_WINDOW_COLLECTION_H
#define DESKTOP_WINDOW_COLLECTION_H

#include "adjunct/desktop_util/treemodel/optreemodel.h"
#include "adjunct/quick/models/DesktopWindowCollectionItem.h"
#include "adjunct/quick/windows/DocumentWindowListener.h"
#include "modules/util/adt/oplisteners.h"
#include "adjunct/desktop_pi/DesktopDragObject.h"

class DesktopWindow;
class OpWorkspace;
class OpWindow;
class DesktopGroupModelItem;

/**
 * @brief Represents the hierarchy of all windows managed by Opera
 *
 * This represents the hierarchy of windows (which window is a child of which other window,
 * how are windows grouped) in Opera, modeled as a tree.
 *
 * Here is an example of a tree that you could find in the wild. It represents a browser
 * with two windows open. Each window contains two tabs. In the second window, the tabs are stacked
 * together.
 *
 *                                        (Root)
 *                                          |
 *                                          |
 *                        +-----------------+-----------------+
 *                        |                                   |
 *                        |                                   |
 *              DesktopWindowModelItem              DesktopWindowModelItem
 *              (BrowserDesktopWindow)              (BrowserDesktopWindow)
 *                        |                                   |
 *                        |                                   |
 *           +------------+-----------+                       |
 *           |                        |             DesktopGroupModelItem
 *           |                        |                (Group of tabs)
 * DesktopWindowModelItem   DesktopWindowModelItem            |
 * (DocumentDesktopWindow)    (MailDesktopWindow)             |
 *                                                            |
 *                                               +------------+-----------+
 *                                               |                        |
 *                                               |                        |
 *                                     DesktopWindowModelItem   DesktopWindowModelItem
 *                                     (DocumentDesktopWindow)  (DocumentDesktopWindow)
 */
class DesktopWindowCollection
  : public DocumentWindowListener
  , public TreeModel<DesktopWindowCollectionItem>
{
public:
	typedef TreeModel<DesktopWindowCollectionItem> Base;

	class Listener
	{
	public :
		virtual ~Listener(){}

		/**
		 * A window was added to the list
		 * @param window added
		 */
		virtual void OnDesktopWindowAdded(DesktopWindow* window) = 0;

		/**
		 * A window was removed to the list
		 * @param window removed
		 */
		virtual void OnDesktopWindowRemoved(DesktopWindow* window) = 0;

		/**
		 * A document window in the list had its url altered
		 * @param document_window altered
		 */
		virtual void OnDocumentWindowUrlAltered(DocumentDesktopWindow* document_window,
												const OpStringC& url) = 0;

		/**
		 * An item position in model tree has changed
		 * @param item item that was reordered.
		 * @param old_parent parent of item before it was reordered. NULL for top level items.
		 * @param old_previous previous item of item before it was reordered. NULL for first child items.
		 */
		virtual void OnCollectionItemMoved(DesktopWindowCollectionItem* item,
		                                   DesktopWindowCollectionItem* old_parent,
		                                   DesktopWindowCollectionItem* old_previous) = 0;
	};

	/*
	 * @param listener
	 *
	 * @return OpStatus::OK if successful in setting listener
	 **/
	OP_STATUS AddListener(DesktopWindowCollection::Listener *listener) { return m_listeners.Add(listener);}
	OP_STATUS AddListener(OpTreeModel::Listener* listener) { return Base::AddListener(listener); }

	/*
	 * @param listener
	 *
	 * @return OpStatus::OK if successful in removing listener
	 **/
	OP_STATUS RemoveListener(DesktopWindowCollection::Listener* listener) { return m_listeners.Remove(listener); }
	OP_STATUS RemoveListener(OpTreeModel::Listener* listener) { return Base::RemoveListener(listener); }

	/**
	 * @return Window that currently has input focus
	 */
	DesktopWindow* GetCurrentInputWindow();

	/**
	 *
	 * @param id
	 * @return
	 */
	DesktopWindow* GetDesktopWindowByID(INT32 id);

	/**
	 *
	 * @param search_url
	 * @return
	 */
	DesktopWindow* GetDesktopWindowByUrl(const uni_char* search_url);

	/**
	 *
	 * @param type
	 * @return
	 */
	DesktopWindow* GetDesktopWindowByType(OpTypedObject::Type type);


	/** Close all desktop windows of a specific type, eg WINDOW_TYPE_MAIL_VIEW
	 *
	 * @param type
	 */
	void CloseDesktopWindowsByType(OpTypedObject::Type type);

	/**
	 *
	 * @return
	 */
	OpWorkspace* GetWorkspace();

	/** Get desktop windows of a certain type
	  * @param type Type of desktop windows to retrieve
	  * @param windows Found desktop windows will be added to the end of this list
	  */
	OP_STATUS GetDesktopWindows(OpTypedObject::Type type, OpVector<DesktopWindow>& windows);

	/** Get all desktop windows in this collection
	  */
	OP_STATUS GetDesktopWindows(OpVector<DesktopWindow>& windows) { return GetDesktopWindows(OpTypedObject::WINDOW_TYPE_UNKNOWN, windows); }

	/** Get the first toplevel item (if available)
	  */
	DesktopWindowCollectionItem* GetFirstToplevel() { return GetChildItem(-1); }

	/** Get total number of toplevel items
	  */
	UINT32 GetToplevelCount() { return GetChildCount(-1); }

	/**
	 *
	 * @param window
	 * @return
	 */
	DesktopWindow* GetDesktopWindowFromOpWindow(OpWindow* window);

	DocumentDesktopWindow* GetDocumentWindowFromCommander(OpWindowCommander* commander);

	/**
	 *
	 */
	void CloseAll();

	/**
	 *
	 */
	void GentleClose();

	/**
	 *
	 */
	void MinimizeAll();

	/** Show or hide all Opera windows
	  * @param visible Whether to make windows visible or invisible
	  */
	void SetWindowsVisible(bool visible);

	/**
	 *
	 * @return
	 */
	UINT32 GetCount() { return Base::GetCount(); }

	/**
	 *
	 * @return The current number of windows of a particular type
	 */
	UINT32 GetCount(OpTypedObject::Type type);

	/**
	 *
	 * @param desktop_window
	 */
	void RemoveDesktopWindow(DesktopWindow* desktop_window);

	/**
	 *
	 * @param desktop_window
	 * @return
	 */
	OP_STATUS AddDesktopWindow(DesktopWindow* desktop_window, DesktopWindow* parent_window);

	/** Reorder a desktop window relative to other windows or groups
	  * @param item Item to change
	  * @param parent Should have this parent, or NULL for no parent
	  * @param after Should be placed after this item (NULL for placing at the start)
	  */
	void ReorderByItem(DesktopWindowCollectionItem& item, DesktopWindowCollectionItem* parent, DesktopWindowCollectionItem* after);

	/** Reorder a desktop window relative to other windows or groups, using a position argument
	  * @param item Item to change
	  * @param parent Should have this parent, or NULL for no parent
	  * @param pos Position to change to (relative to the parent). If larger than total amount of children for this parent will add to end
	  */
	void ReorderByPos(DesktopWindowCollectionItem& item, DesktopWindowCollectionItem* parent, UINT32 pos);

	/** Create a group out of two desktop items
	  * @param main Main item of the group (group will take this position)
	  * @param second Second item of the group
	  * @param collapsed Whether the group should initially be collapsed
	  * @return The group created, or NULL on failure (the created group is owned by the DesktopWindowCollection)
	  */
	DesktopGroupModelItem* CreateGroup(DesktopWindowCollectionItem& main, DesktopWindowCollectionItem& second, bool collapsed = true)
	{
		return CreateGroup(NULL, &main, &second, collapsed);
	}

	/** Create a group out of _one_ desktop item. Required by WinTab API NEW_TAB_GROUP create request with first tab already created
	  * @param wintab Main item of the group (group will take this position)
	  * @param collapsed Whether the group should initially be collapsed
	  * @return The group created, or NULL on failure (the created group is owned by the DesktopWindowCollection)
	  */
	DesktopGroupModelItem* CreateGroup(DesktopWindowCollectionItem& wintab, bool collapsed = true)
	{
		return CreateGroup(NULL, &wintab, NULL, collapsed);
	}

	/** Create a group out of _none_ desktop items. Required by WinTab API NEW_TAB_GROUP create request with first tab not yet created
	  * @param parent Parent of the new group, used if after is NULL.
	  * @param after Item to use for the positioning of a new group. If NULL, parent is used and the group is placed at the start of its children
	  * @param collapsed Whether the group should initially be collapsed
	  * @return The group created, or NULL on failure (the created group is owned by the DesktopWindowCollection)
	  */
	DesktopGroupModelItem* CreateGroupAfter(DesktopWindowCollectionItem& parent, DesktopWindowCollectionItem* after, bool collapsed = true)
	{
		return CreateGroup(&parent, after, NULL, collapsed);
	}

	/** Ungroup a group, making group members revert to the parent of the group
	  * @param group Group to disband
	  * @note group will be deleted in this operation
	  */
	void UnGroup(DesktopWindowCollectionItem* group) { ModelLock lock(this); return Delete(group->GetIndex(), FALSE); }

	/** Destroy a group and close its contents
	  * @param group Group to destroy
	  */
	void DestroyGroup(DesktopWindowCollectionItem* group);

	/** Merge two items - will create a group if necessary
	  * @param item Item to merge
	  * @param into Item to merge in to
	  */
	OP_STATUS MergeInto(DesktopWindowCollectionItem& item, DesktopWindowCollectionItem& into, DesktopWindow* new_active);

	/** Handles dragging of windows onto another window
	 * @param drag_object Windows to be dragged (must contain window IDs)
	 * @param target Where to drop windows
	 */
	void OnDragWindows(DesktopDragObject* drag_object, DesktopWindowCollectionItem* target);

	/** Handles dropping of windows onto another window
	  * @param drag_object Windows to be dropped (must contain window IDs)
	  * @param target Where to drop windows
	  */
	void OnDropWindows(DesktopDragObject* drag_object, DesktopWindowCollectionItem* target);

	// DocumentDesktopWindow::DocumentWindowListener
	virtual void OnUrlChanged(DocumentDesktopWindow* document_window, const uni_char* url);
	virtual void OnDocumentChanging(DocumentDesktopWindow* document_window);

	// From TreeModel
	virtual INT32 GetColumnCount() {return 2;}
	virtual OP_STATUS GetColumnData(ColumnData* column_data);

private:

	/** Create a group out of zero to two desktop items.
	  * @param parent If set, changes the meaning of main and the algorithm from "create from" to "create after"
	  * @param main If parent is not set, this is the main item of the group (group will take this position);
	  *             if parent is set, this is collection item, after which the new group should be placed. In the
	  *             second case, main can be NULL, in which case the group will be created at the start of the parent collection.
	  * @param second Second item of the group, used if parent is not set
	  * @param collapsed Whether the group should initially be collapsed
	  * @return The group created, or NULL on failure (the created group is owned by the DesktopWindowCollection)
	  */
	DesktopGroupModelItem* CreateGroup(DesktopWindowCollectionItem* parent, DesktopWindowCollectionItem* main, DesktopWindowCollectionItem* second, bool collapsed);

	void PrepareToplevelMove(DesktopDragObject* drag_object, DesktopWindowCollectionItem* toplevel);
	void RemovingFromGroup(DesktopGroupModelItem& group, DesktopWindow* desktop_window);
	void BroadcastDesktopWindowAdded(DesktopWindow* window);
	void BroadcastDesktopWindowRemoved(DesktopWindow* window);
	void BroadcastDocumentWindowUrlAltered(DocumentDesktopWindow* document_window,
										   const uni_char* url);

	void BroadcastCollectionItemMoved(DesktopWindowCollectionItem* collection_item,
	                                  DesktopWindowCollectionItem* old_parent,
	                                  DesktopWindowCollectionItem* old_previous);

	void StartListening(DesktopWindow* window);
	void StopListening(DesktopWindow* window);

	OpListeners<Listener> m_listeners;
};

#endif // DESKTOP_WINDOW_COLLECTION_H
