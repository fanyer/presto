/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
* @author: Deepak Arora (deepaka), Karianne Ekern (karie)
*
*/

#ifndef COLLECTION_NAVIGATION_PANE_H
#define COLLECTION_NAVIGATION_PANE_H

#include "modules/widgets/OpWidget.h"
#include "adjunct/quick_toolkit/widgets/QuickWidget.h"
#include "adjunct/quick/models/NavigationModel.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetContainer.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"

class TypedObjectCollection;
class QuickTreeView;
class OpCollectionView;
class NavigationFolderItem;
class NavigationItem;
class HotlistModelItem;

/**
 * @brief Collection's navigation pane shows various models which are
 * shown as a folder.
 *
 * @author Deepak Arora & Karianne Ekern
 *
 * Navigation-pane shows following kinds of items which are:
 * a. Bookmark. It has 3 special folders which are 1. Recently Added
 *    2. All Bookmarks  3. Deleted in addition to one folder for each
 *    bookmarks folder itself
 * b. Opened Windows (not implemented)
 * c. History (not implemented)
 * d. Downloads (not implemented)
 *
 * This view is shown on left hand side of splitter, which
 * separates navigation-pane from view-pane.
 *
 * Note: At present bookmark related stuffs are handled here
 */
class CollectionNavigationPane
	: public OpWidget
	, public QuickWidgetContainer
	, public NavigationModel::NavigationModelListener
{
public:
	CollectionNavigationPane(OpCollectionView* host);
	~CollectionNavigationPane();

	void InitL();

	/**
	 * @return bookmark root folder
	 */
	NavigationItem* GetBookmarkRootNavigationFolder() const { return m_navigation_folder_model->GetBookmarkRootFolder(); }

	/**
	 * Selects item in treeview. Here items logically represent folder.
	 * @param item_id is the id of item which has to be selected
	 */
	void SelectItem(int item_id) { m_tree_view->GetOpWidget()->SetSelectedItemByID(item_id); }

	// NavigationModelListener
	void					ItemAdded(CollectionModelItem* item);
	void					ItemRemoved(CollectionModelItem* item); // Removed from a model or folder
	void					ItemModified(CollectionModelItem* item, UINT32 changed_flag);
	void					ItemMoved(CollectionModelItem* item);
	void					FolderMoved(NavigationItem* item);

	// OpWidget
	virtual void			OnLayout();
	virtual BOOL			OnInputAction(OpInputAction* action);

	// OpWidgetListener
	virtual void			OnChange(OpWidget *widget, BOOL changed_by_mouse = FALSE);
	virtual void			OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks);
	virtual BOOL			OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked);
	virtual void			OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y);
	virtual void			OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y);
	virtual void			OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y);

	// QuickWidgetContainer
	virtual void			OnContentsChanged() { Relayout(); }

private:
	void					SetupTreeviewWidgetL(const TypedObjectCollection& widgets_collection);
	void 					SetupFindWidgetL(const TypedObjectCollection& widgets_collection);
	void					ProcessDeleteAction();
	const char*				GetMenuName() const;
	bool					ShowContextMenu(const OpPoint &point, BOOL center, OpTreeView* view, BOOL use_keyboard_context);
	bool					ExternalDragDrop(NavigationFolderItem* item, DesktopDragObject* drag_object, DropType drop_type, DesktopDragObject::InsertType insert_type, BOOL drop);
	int						DropItem(NavigationItem* to, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type, INT32 *first_id);
	int						DropItem(NavigationItem* to, DesktopDragObject* drag_object, INT32 i, DesktopDragObject::InsertType insert_type, INT32 *got_id);
	OP_STATUS				DropURLItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	OP_STATUS				DropHistoryItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	OP_STATUS				DropWindowItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	int						DropBookmarkItem(NavigationItem* to, HotlistModelItem* from, DesktopDragObject::InsertType insert_type, INT32 *got_id);
	void					UpdateDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	void					UpdateHistoryDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type);
	BOOL					GetActionGetState(OpInputAction* action);
	BOOL					CanHandleAction(OpInputAction* action);
	BOOL					DisablesAction(OpInputAction* action);
	OP_STATUS				HandleAction(OpInputAction* action);

	OpCollectionView*		m_container;
	QuickWidget*			m_navigation_pane_controller;
	QuickTreeView*			m_tree_view;
	NavigationModel*		m_navigation_folder_model;
};

#endif //COLLECTION_NAVIGATION_PANE_H
