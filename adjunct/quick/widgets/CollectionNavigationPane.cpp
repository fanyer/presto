/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2012 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*
*
*/

#include "core/pch.h"

#include "adjunct/quick/widgets/CollectionNavigationPane.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/NavigationModel.h"
#include "adjunct/quick/widgets/CollectionViewPane.h"
#include "adjunct/quick/widgets/OpCollectionView.h"
#include "adjunct/quick/windows/DocumentDesktopWindow.h"
#include "adjunct/quick_toolkit/contexts/WidgetContext.h"
#include "adjunct/quick_toolkit/widgets/QuickTreeView.h"
#include "adjunct/quick_toolkit/widgets/QuickWidgetDecls.h" //Required for Quickfind
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"

#include "modules/dragdrop/dragdrop_manager.h"

CollectionNavigationPane::CollectionNavigationPane(OpCollectionView* host)
	: m_container(host)
	, m_navigation_pane_controller(NULL)
	, m_tree_view(NULL)
	, m_navigation_folder_model(NULL)
{

}

CollectionNavigationPane::~CollectionNavigationPane()
{
	OP_DELETE(m_navigation_folder_model);
	OP_DELETE(m_navigation_pane_controller);
}

void CollectionNavigationPane::InitL()
{
	TypedObjectCollection widgets_collection;
	LEAVE_IF_ERROR(WidgetContext::CreateQuickWidget("Collection Manager NavigationPane", m_navigation_pane_controller, widgets_collection));
	m_navigation_pane_controller->SetContainer(this);
	m_navigation_pane_controller->SetParentOpWidget(this);

	SetupTreeviewWidgetL(widgets_collection);
	SetupFindWidgetL(widgets_collection);
}

const char* CollectionNavigationPane::GetMenuName() const
{
	const char* menu_name = NULL;

	NavigationItem* item = static_cast<NavigationItem*>(m_tree_view->GetOpWidget()->GetSelectedItem());
	if (!item || !item->GetHotlistItem() && item->IsRecentFolder())
		return NULL;

	if (!item || !item->GetHotlistItem() && item->IsAllBookmarks())
		menu_name = "Navigationpane All Bookmarks Folder Options";
	else if (item && item->IsDeletedFolder())
		menu_name = "Navigationpane TrashBin Options";
	else if (item->GetHotlistItem()->GetIsInsideTrashFolder())
		menu_name = "Navigationpane Folders in TrashBin Options";
	else
		menu_name = "Navigationpane Bookmark Folder Options";

	return menu_name;
}

BOOL CollectionNavigationPane::GetActionGetState(OpInputAction* action)
{
	if (CanHandleAction(action))
	{
		action->SetEnabled(!DisablesAction(action));
		return TRUE;
	}

	return FALSE;
}

BOOL CollectionNavigationPane::CanHandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_NEW_FOLDER:
		case OpInputAction::ACTION_EMPTY_TRASH:
			 return TRUE;
	}

	return FALSE;
}

BOOL CollectionNavigationPane::DisablesAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_EMPTY_TRASH:
		{
			NavigationItem* item = static_cast<NavigationItem*>(m_tree_view->GetOpWidget()->GetSelectedItem());
			if (item && item->IsDeletedFolder() && item->GetHotlistItem() && item->GetHotlistItem()->GetChildCount() > 0)
				return FALSE;
			return TRUE;
		}

		default:
			return FALSE;
	}
}

OP_STATUS CollectionNavigationPane::HandleAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_DELETE:
			 ProcessDeleteAction();
			 return OpStatus::OK;

		case OpInputAction::ACTION_EMPTY_TRASH:
			 g_hotlist_manager->EmptyTrash(g_hotlist_manager->GetBookmarksModel());
			 return OpStatus::OK;

		default:
			 return OpStatus::ERR;
	}
}

void CollectionNavigationPane::OnLayout()
{
	if (m_navigation_pane_controller)
		m_navigation_pane_controller->Layout(GetBounds());

	OpWidget::OnLayout();
}

BOOL CollectionNavigationPane::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_LOWLEVEL_PREFILTER_ACTION:
		{
			OpInputAction* child_action = action->GetChildAction();
			if (child_action->GetAction() != OpInputAction::ACTION_LOWLEVEL_KEY_PRESSED)
				return FALSE;

			switch(child_action->GetActionData())
			{
				case OP_KEY_ENTER:
				{
					OpTreeView* tv = m_tree_view->GetOpWidget();
					BOOL state = g_op_system_info->GetShiftKeyState();
					if (state == 0 && child_action->GetFirstInputContext() == tv && tv->GetEditPos() == -1)
					{
						int pos = tv->GetSelectedItemPos();
						BOOL is_folder_open = tv->IsItemOpen(pos);
						tv->OpenItem(pos, !is_folder_open);
						if (!is_folder_open)
							m_container->OpenSelectedFolder((NavigationItem*)tv->GetSelectedItem());
						return TRUE;
					}
				}
			}
		}
		return FALSE;

		case OpInputAction::ACTION_GET_ACTION_STATE:
			 return GetActionGetState(action->GetChildAction());
	}

	if (m_tree_view->GetOpWidget()->OnInputAction(action))
		return TRUE;

	BOOL handled = FALSE;
	if (CanHandleAction(action) && !DisablesAction(action))
		handled =  OpStatus::IsSuccess(HandleAction(action));

	return handled;
}

void CollectionNavigationPane::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget->GetType() != WIDGET_TYPE_EDIT)
		return;

	OpString filter;
	RETURN_VOID_IF_ERROR(widget->GetText(filter));
	static_cast<OpCollectionView*>(GetParent())->SetMatchText(filter);
}

void CollectionNavigationPane::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (down || pos == -1 || widget->GetType() != WIDGET_TYPE_TREEVIEW)
		return;

	NavigationItem* item = static_cast<NavigationItem*>(static_cast<OpTreeView*>(widget)->GetItemByPosition(pos));
	if (nclicks == 1 && button == MOUSE_BUTTON_1 && item)
		m_container->OpenSelectedFolder(item);
}

BOOL CollectionNavigationPane::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint &menu_point, const OpRect *avoid_rect, BOOL keyboard_invoked)
{
	if (!widget || widget->GetType() != WIDGET_TYPE_TREEVIEW)
		return FALSE;

	// Needed for split view layout
	int x = widget->GetRect(FALSE).x + menu_point.x;
	int y = widget->GetRect(FALSE).y + menu_point.y;
	return ShowContextMenu(OpPoint(x,y), FALSE, static_cast<OpTreeView*>(widget),FALSE);
}

void CollectionNavigationPane::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget != m_tree_view->GetOpWidget())
		return;

	// No drag operation is allowed if item is deleted folder
	NavigationItem* item = static_cast<NavigationItem*>(static_cast<OpTreeView*>(m_tree_view->GetOpWidget())->GetItemByPosition(pos));
	HotlistModelItem* bookmark = item->GetHotlistItem();
	if (!bookmark || bookmark->IsDeletedFolder())
		return;

	// Close the folder when dragging
	if (bookmark->IsFolder())
		static_cast<OpTreeView*>(m_tree_view->GetOpWidget())->OpenItem(pos, FALSE);

	DesktopDragObject* drag_object = widget->GetDragObject(DRAG_TYPE_BOOKMARK, x, y);
	if (!drag_object)
		return;

	BookmarkItemData item_data;
	if (g_desktop_bookmark_manager->GetItemValue(bookmark->GetID(), item_data))
	{
		OpStatus::Ignore(drag_object->SetURL(item_data.url.CStr()));
		RETURN_VOID_IF_ERROR(drag_object->SetTitle(item_data.name.CStr()));
		drag_object->SetID(bookmark->GetID());
	}

	g_drag_manager->StartDrag(drag_object, NULL, FALSE);
}

void CollectionNavigationPane::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (pos == -1 || widget != m_tree_view->GetOpWidget())
		return;

	// If no drop type suggested, use move
	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);
	drag_object->SetDesktopDropType(DROP_NOT_AVAILABLE);

	DesktopDragObject::InsertType insert_type = drag_object->GetSuggestedInsertType();
	NavigationItem* target_folder = static_cast<NavigationItem*>(static_cast<OpTreeView*>(widget)->GetItemByPosition(pos));
	if (!target_folder || !target_folder->IsUserEditable() || insert_type != DesktopDragObject::INSERT_INTO)
		return; // If target_folder is not valid, like separator.

	NavigationFolderItem* target_folder_item = static_cast<NavigationFolderItem*>(target_folder);
	BookmarkModel* bookmarks = g_hotlist_manager->GetBookmarksModel();
	HotlistModelItem* bookmark_target = target_folder_item->GetHotlistItem();

	if (drag_object->GetType() == DRAG_TYPE_HISTORY
		|| drag_object->GetType() == DRAG_TYPE_WINDOW
		|| drag_object->GetURL() && *drag_object->GetURL())
	{
		if (target_folder_item->IsDeletedFolder() && drag_object->GetType() != DRAG_TYPE_BOOKMARK)
			return;

		drag_object->SetDesktopDropType(DROP_COPY);
		return;
	}

	HotlistModelItem* dragged_item = drag_object->GetType() == DRAG_TYPE_BOOKMARK ? bookmarks->GetItemByID(drag_object->GetID()) : NULL;
	if (!dragged_item)
		return;
	if (dragged_item && dragged_item->IsDeletedFolder())
		return; // If dragged object is deleted folder
	else if (!bookmark_target && dragged_item && !dragged_item->GetParent())
		return; // If dragged object is being moved/dropped into All Bookmarks From All Bookmarks
	else if (dragged_item && bookmark_target == dragged_item)
		return; // If source and destination are the same, then return
	else if (bookmark_target && dragged_item && dragged_item->GetParent() == bookmark_target)
		return; // If dragged item is folder and its being moved/dropped into its immediate parent

	drag_object->SetDesktopDropType(DROP_MOVE);
}

void CollectionNavigationPane::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	if (pos == -1 || widget != m_tree_view->GetOpWidget())
		return;

	DesktopDragObject* drag_object = static_cast<DesktopDragObject*>(op_drag_object);
	DropType drop_type = drag_object->GetSuggestedDropType();
	DesktopDragObject::InsertType insert_type = drag_object->GetSuggestedInsertType();
	NavigationItem* target_folder = static_cast<NavigationItem*>(static_cast<OpTreeView*>(widget)->GetItemByPosition(pos));
	if (!target_folder->IsUserEditable() || insert_type != DesktopDragObject::INSERT_INTO)
		return; // If target_folder is not valid, like separator.

	NavigationFolderItem* target_folder_item = static_cast<NavigationFolderItem*>(target_folder);
	BookmarkModel* bookmarks = g_hotlist_manager->GetBookmarksModel();
	HotlistModelItem* bookmark_target = target_folder_item->GetHotlistItem();
	HotlistModelItem* dragged_item = drag_object->GetType() == DRAG_TYPE_BOOKMARK ? bookmarks->GetItemByID(drag_object->GetID()) : NULL;
	if (!dragged_item || bookmarks != dragged_item->GetModel())
	{
		// Handle dragged object whose type doesn't matches to DRAG_TYPE_BOOKMARK
		ExternalDragDrop(target_folder_item, drag_object, drop_type, insert_type, TRUE);
		return;
	}

	INT32 dragged_objects_count = drag_object->GetIDCount();
	if (dragged_objects_count <= 0 && drag_object->GetID() > 0)
		dragged_objects_count = 1;

	GenericTreeModel::ModelLock lock(m_navigation_folder_model);
	for (INT32 i = 0; i < dragged_objects_count; i++)
	{
		if (drop_type == DROP_MOVE && bookmark_target && dragged_item)
		{
			// TODO: Handle trash issues
			// If the source is the trash then MOVE will be MOVE
			if (!dragged_item->GetIsInsideTrashFolder())
			{
				HotlistModelItem *target_parent_folder = dragged_item->GetIsInMoveIsCopy();

				// No moving the target folder itself into the trash, without a message,
				// And then do a direct delete
				if (dragged_item->GetTarget().HasContent() && target_parent_folder->GetIsTrashFolder())
				{
					// If they don't want to delete just stop.
					if (!g_hotlist_manager->ShowTargetDeleteDialog(dragged_item))
						break;
				}
			}
		}

		int dropped_id = -1;
		int ret = 1;
		if (dragged_objects_count == 1)
			ret = DropItem(target_folder, drag_object, insert_type, &dropped_id);
		else
			ret = DropItem(target_folder, drag_object, i, insert_type, &dropped_id);

		if (ret == -1)
			break;
	}
}

void CollectionNavigationPane::SetupTreeviewWidgetL(const TypedObjectCollection& widgets_collection)
{
	m_tree_view = widgets_collection.GetL<QuickTreeView>("tree_view");
	OpTreeView* treeview = m_tree_view->GetOpWidget();

	OpStackAutoPtr<NavigationModel>collection_folder_model(OP_NEW_L(NavigationModel, ()));

	LEAVE_IF_ERROR(collection_folder_model->Init());
	m_navigation_folder_model = collection_folder_model.release();
	m_navigation_folder_model->SetListener(this);
	treeview->SetListener(this);
	treeview->SetTreeModel(m_navigation_folder_model, 0, TRUE);
	treeview->GetBorderSkin()->SetImage("Detailed Panel Treeview Skin", "Treeview Skin");
	treeview->SetShowColumnHeaders(FALSE);
	treeview->SetName("Content Folders View");
	treeview->SetRestrictImageSize(TRUE);
	treeview->SetShowThreadImage(TRUE);
	treeview->SetDragEnabled(TRUE);
	treeview->SetMatchType(MATCH_ALL);
	treeview->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL); // So that a panel specific font will be used
	treeview->SetVisibility(TRUE);
}

void CollectionNavigationPane::SetupFindWidgetL(const TypedObjectCollection& widgets_collection)
{
	OpQuickFind* find = widgets_collection.GetL<QuickQuickFind>("quick_find")->GetOpWidget();
	find->ShowButton(FALSE);
	find->SetTarget(NULL, false);
	find->SetListener(this);
}

void CollectionNavigationPane::ProcessDeleteAction()
{
	OpTreeView* tree_view = m_tree_view->GetOpWidget();

	OpINT32Vector id_list;
	tree_view->GetSelectedItems(id_list);
	if(!id_list.GetCount())
		return;

	OpINT32Vector hotlist_list;
	for (UINT32 i = 0; i < id_list.GetCount(); i++)
	{
		NavigationItem* item = m_navigation_folder_model->GetItemByID(id_list.Get(i));
		if (item->GetHotlistItem())
			RETURN_VOID_IF_ERROR(hotlist_list.Add(item->GetHotlistItem()->GetID()));
	}

	g_desktop_bookmark_manager->Delete(hotlist_list);
}

bool CollectionNavigationPane::ShowContextMenu(const OpPoint &point, BOOL center, OpTreeView* view, BOOL use_keyboard_context)
{
	const char* menu_name = GetMenuName();
	if (!menu_name)
		return true;

	NavigationItem* item = static_cast<NavigationItem*>(m_tree_view->GetOpWidget()->GetSelectedItem());

	OpPoint p(point.x+GetScreenRect().x, point.y+GetScreenRect().y);
	g_application->GetMenuHandler()->ShowPopupMenu(menu_name, PopupPlacement::AnchorAt(p, center), item->GetID(), use_keyboard_context);
	return true;
}

bool CollectionNavigationPane::ExternalDragDrop(NavigationFolderItem* item, DesktopDragObject* drag_object, DropType drop_type, DesktopDragObject::InsertType insert_type, BOOL drop)
{
	if (drag_object->GetURL() == NULL || *drag_object->GetURL() == UNI_L('\0'))
		return false;

	INT32 target_id = item->GetHotlistItem() ? item->GetHotlistItem()->GetID() : HotlistModel::BookmarkRoot;
	HotlistModelItem* target = static_cast<HotlistModel*>(item->GetItemModel())->GetItemByID(target_id);

	OP_ASSERT(target);

	if (drag_object->GetType() == DRAG_TYPE_HISTORY)
	{
		if (drop)
			DropHistoryItem(target, drag_object, insert_type);
		else if( drag_object->GetIDCount() > 0 )
			UpdateHistoryDragObject(target, drag_object, insert_type);
	}
	else if (drag_object->GetType() == DRAG_TYPE_WINDOW)
	{
		if (drop)
			DropWindowItem(target, drag_object, insert_type);
		else if (drag_object->GetIDCount() > 0)
			UpdateDragObject(target, drag_object, insert_type);
	}
	else if (drag_object->GetURL() && *drag_object->GetURL())
	{
		if (drop)
			DropURLItem(target, drag_object, insert_type);
		else
			UpdateDragObject(target, drag_object, insert_type);
	}

	return true;
}

int CollectionNavigationPane::DropItem(NavigationItem* to, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type, INT32 *first_id)
{
	if (!to)
		return -1;

	HotlistModelItem* from = g_hotlist_manager->GetBookmarksModel()->GetItemByID(drag_object->GetID());
	return DropBookmarkItem(to, from, insert_type, first_id);
}

int CollectionNavigationPane::DropItem(NavigationItem* to, DesktopDragObject* drag_object, INT32 i, DesktopDragObject::InsertType insert_type, INT32 *got_id)
{
	if (!to)
		return -1;

	HotlistModelItem* from = g_hotlist_manager->GetBookmarksModel()->GetItemByID(drag_object->GetID(i));
	return DropBookmarkItem(to, from, insert_type, got_id);
}

OP_STATUS CollectionNavigationPane::DropURLItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	INT32 new_selection_id = -1;
	INT32 target_id = target ? target->GetID() : -1;

	if (drag_object->GetURLs().GetCount() > 0)
	{
		BookmarkItemData item_data;
		RETURN_IF_ERROR(item_data.name.Set(drag_object->GetTitle()));
		RETURN_IF_ERROR(item_data.description.Set(DragDrop_Data_Utils::GetText(drag_object)));

		for (UINT32 i=0; i < drag_object->GetURLs().GetCount(); i++)
		{
			RETURN_IF_ERROR(item_data.url.Set(*drag_object->GetURLs().Get(i)));
			g_desktop_bookmark_manager->DropItem(item_data, target_id, insert_type, TRUE, drag_object->GetType(), &new_selection_id);
		}
	}
	else
	{
		BookmarkItemData item_data;
		RETURN_IF_ERROR(item_data.name.Set(drag_object->GetTitle()));
		RETURN_IF_ERROR(item_data.url.Set(drag_object->GetURL()));
		RETURN_IF_ERROR(item_data.description.Set(DragDrop_Data_Utils::GetText(drag_object)));
		g_desktop_bookmark_manager->DropItem(item_data, target_id, insert_type, TRUE, drag_object->GetType(), &new_selection_id);
	}

	return OpStatus::OK;
}

OP_STATUS CollectionNavigationPane::DropHistoryItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

	for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
	{
		HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(i));
		if (!history_item)
			continue;

		BookmarkItemData item_data;
		OpStatus::Ignore(history_item->GetAddress(item_data.url));
		OpStatus::Ignore(history_item->GetTitle(item_data.name));
		g_desktop_bookmark_manager->DropItem(item_data, target ? target->GetID() : -1, insert_type, TRUE, drag_object->GetType());
	}

	return OpStatus::OK;
}

OP_STATUS CollectionNavigationPane::DropWindowItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	DesktopWindowCollection& model = g_application->GetDesktopWindowCollection();
	for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
	{
		DesktopWindowCollectionItem* item = model.GetItemByID(drag_object->GetID(i));
		if (!item || item->GetType() != WINDOW_TYPE_DOCUMENT || !item->GetDesktopWindow())
			continue;

		OpWindowCommander* document = static_cast<DocumentDesktopWindow*>(item->GetDesktopWindow())->GetWindowCommander();
		BookmarkItemData item_data;
		RETURN_IF_ERROR(item_data.name.Set(document->GetCurrentTitle()));
		RETURN_IF_ERROR(item_data.url.Set(document->GetCurrentURL(FALSE)));
		g_desktop_bookmark_manager->DropItem(item_data, target ? target->GetID() : -1, insert_type, TRUE, drag_object->GetType());
	}

	return OpStatus::OK;
}

int CollectionNavigationPane::DropBookmarkItem(NavigationItem* to, HotlistModelItem* from, DesktopDragObject::InsertType insert_type, INT32 *got_id)
{
	if (!from)
		return -1;

	if (to->IsDeletedFolder())
	{
		BookmarkItem* bookmark = BookmarkWrapper(from)->GetCoreItem();
		g_bookmark_manager->DeleteBookmark(bookmark, FALSE);
	}
	else if (to->IsAllBookmarks())
	{
		BookmarkModel* bookmarks = g_hotlist_manager->GetBookmarksModel();
		if (insert_type == DesktopDragObject::INSERT_INTO && bookmarks->GetItemByIndex(0)->GetIsTrashFolder())
			insert_type= DesktopDragObject::INSERT_AFTER;

		CoreBookmarkPos pos(bookmarks->GetItemByIndex(0), insert_type);
		g_bookmark_manager->MoveBookmark(BookmarkWrapper(from)->GetCoreItem(), pos.previous, pos.parent);
	}
	else
	{
		CoreBookmarkPos pos(to->GetHotlistItem(), insert_type);
		g_bookmark_manager->MoveBookmark(BookmarkWrapper(from)->GetCoreItem(), pos.previous, pos.parent);
	}

	*got_id = from->GetID();

	return 0;
}

void CollectionNavigationPane::UpdateDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	BookmarkItemData item_data;
	INT32 target_id = target ? target->GetID() : -1;
	if (g_desktop_bookmark_manager->DropItem(item_data, target_id, insert_type, FALSE, drag_object->GetType()))
	{
		drag_object->SetInsertType(insert_type);
		drag_object->SetDesktopDropType(DROP_COPY);
	}
}

void CollectionNavigationPane::UpdateHistoryDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
	HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(0));
	if (history_item)
		UpdateDragObject(target, drag_object, insert_type);
}

void CollectionNavigationPane::ItemAdded(CollectionModelItem* item)
{
	m_container->GetCollectionViewPane().AddItem(item);
}

void CollectionNavigationPane::ItemRemoved(CollectionModelItem* item)
{
	CollectionModelIterator& it = m_container->GetCurrentIterator();
	// If this is a folder that is currently open in the view, switch to all
	if (item == it.GetFolder())
		m_container->OpenFolder(GetBookmarkRootNavigationFolder());

	m_container->GetCollectionViewPane().RemoveItem(item);
}

void CollectionNavigationPane::ItemModified(CollectionModelItem* item, UINT32 changed_flag)
{
	if (item->IsItemFolder())
		Update();
	else
	{
		if (changed_flag & HotlistModel::FLAG_URL)
			m_container->GetCollectionViewPane().UpdateItemUrl(item);
		else if (changed_flag & HotlistModel::FLAG_NAME)
			m_container->GetCollectionViewPane().UpdateItemTitle(item);
		else if (changed_flag & HotlistModel::FLAG_ICON)
			m_container->GetCollectionViewPane().UpdateItemIcon(item);
	}
}

void CollectionNavigationPane::ItemMoved(CollectionModelItem* item)
{
	m_container->GetCollectionViewPane().MoveItem(item);
}

void CollectionNavigationPane::FolderMoved(NavigationItem* item)
{
	NavigationItem* focus_item = item->GetPreviousItem();
	if (!focus_item)
		focus_item = item->GetParentItem();

	if (m_container->GetCurrentIterator().GetFolder() == item->GetHotlistItem())
	{
		// If this is a folder that is currently open in the view, switch to its previous folder
		NavigationItem* item = focus_item ? focus_item : GetBookmarkRootNavigationFolder();
		m_container->OpenFolder(item);
	}
}
