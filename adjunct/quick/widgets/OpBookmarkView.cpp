/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpBookmarkView.h"

#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/windows/BrowserDesktopWindow.h"
#include "modules/inputmanager/inputaction.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"



/***********************************************************************************
 *
 *	Construct
 *
 ***********************************************************************************/
// static
OP_STATUS OpBookmarkView::Construct(OpBookmarkView** obj,
								   PrefsCollectionUI::integerpref splitter_prefs_setting,
								   PrefsCollectionUI::integerpref viewstyle_prefs_setting,
								   PrefsCollectionUI::integerpref detailed_splitter_prefs_setting,
								   PrefsCollectionUI::integerpref detailed_viewstyle_prefs_setting)
{
	*obj = OP_NEW(OpBookmarkView, ());

	if (!*obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	(*obj)->SetPrefsmanFlags(splitter_prefs_setting,viewstyle_prefs_setting,detailed_splitter_prefs_setting,detailed_viewstyle_prefs_setting);
	(*obj)->Init();

	return OpStatus::OK;
}

/***********************************************************************************
 *
 *	OpBookmarkView
 *
 ***********************************************************************************/

OpBookmarkView::OpBookmarkView()
{
	// When creating a new BookmarkView -> Get the sorting from prefs
	m_sort_column    = g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSortBy);
	m_sort_ascending = g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSortAsc);
	
	// Some corrections so that we can keep using settings from 6.x
	// Keep in sync with code in HotlistManager.cpp
	if( m_sort_column > 6 )
	{
		if( m_sort_column == 16 ) // Unsorted
		{
			m_sort_column = -1;
			m_sort_ascending = 1;
		}
		else if( m_sort_column == 1024 ) // Name
			m_sort_column = 0;
		else if( m_sort_column == 2048 ) // Created
			m_sort_column = 4;
		else if( m_sort_column == 4096 ) // Visited
			m_sort_column = 5;
	}

	m_hotlist_view->SetBoldFolders(TRUE);
	m_hotlist_view->SetAllowMultiLineItems(TRUE);

	// --- SetTreeModel of TreeView --------------
	if(g_hotlist_manager->GetBookmarksModel()->Loaded())
	{
		m_model = g_hotlist_manager->GetBookmarksModel();
		// --- SetTreeModel of TreeView --------------
		m_hotlist_view->SetTreeModel(m_model,m_sort_column, m_sort_ascending);
		m_folders_view->SetTreeModel(m_model,m_sort_column, m_sort_ascending);
	}
	else
	{
		// Ignore Drag N Drop before the m_model is set.
		SetIgnoresMouse(TRUE);
		g_desktop_bookmark_manager->AddBookmarkListener(this);
	}

}

/***********************************************************************************
 *
 *	OnSetDetailed
 *
 ***********************************************************************************/

void OpBookmarkView::OnSetDetailed(BOOL detailed)
{
	SetHorizontal(detailed != FALSE);

	m_hotlist_view->SetName(m_detailed ? "Bookmarks View" : "Bookmarks View Small");
	m_hotlist_view->SetColumnWeight(1, 50);
	m_hotlist_view->SetColumnWeight(2, 150);
	m_hotlist_view->SetColumnVisibility(1, m_detailed);
	m_hotlist_view->SetColumnVisibility(2, m_detailed);
	m_hotlist_view->SetColumnVisibility(3, FALSE);
	m_hotlist_view->SetColumnVisibility(4, FALSE);
	m_hotlist_view->SetColumnVisibility(5, FALSE);

	m_folders_view->SetName("Bookmarks Folders View");
	
	m_hotlist_view->SetLinkColumn(IsSingleClick() ? 0 : -1);
}

/***********************************************************************************
 *
 *	OnSortingChanged
 *
 ***********************************************************************************/

void OpBookmarkView::OnSortingChanged(OpWidget *widget)
{
	int column     = 0;
	BOOL ascending = false;

	if( widget == m_folders_view )
	{
		column    = m_folders_view->GetSortByColumn();
		ascending = m_folders_view->GetSortAscending();
		if( m_hotlist_view )
		{
			m_hotlist_view->Sort(column,ascending);
		}
	}
	else if( widget == m_hotlist_view )
	{
		column    = m_hotlist_view->GetSortByColumn();
		ascending = m_hotlist_view->GetSortAscending();
		if( m_folders_view )
		{
			m_folders_view->Sort(column,ascending);
		}
	}
	else
	{
		return;
	}

	m_sort_column    = column;
	m_sort_ascending = ascending;

	g_pcui->WriteIntegerL(PrefsCollectionUI::HotlistSortBy, column);
	g_pcui->WriteIntegerL(PrefsCollectionUI::HotlistSortAsc, ascending);
	g_prefsManager->CommitL();
}

/***********************************************************************************
 *
 *	OnMouseEvent
 *
 ***********************************************************************************/

void OpBookmarkView::HandleMouseEvent(OpTreeView* widget, HotlistModelItem* item, INT32 pos, MouseButton button, BOOL down, UINT8 nclicks)
{
	BOOL shift = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;
	// single or double click mode
	BOOL click_state_ok = (IsSingleClick() && !down && nclicks == 1 && !shift) || nclicks == 2;

	// UNIX behavior. I guess everyone wants it.
	if( down && nclicks == 1 && button == MOUSE_BUTTON_3 )
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE);
		return;
	}

	if (click_state_ok)
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_OPEN_LINK);
	}

}


/***********************************************************************************
 *
 *	ShowContextMenu
 *
 ***********************************************************************************/

BOOL OpBookmarkView::ShowContextMenu(const OpPoint &point,
									BOOL center,
									OpTreeView* view,
									BOOL use_keyboard_context)
{
	if (!view)
		return FALSE;
	
	HotlistModelItem* item = static_cast<HotlistModelItem*>(view->GetSelectedItem());

	const char *menu_name = 0;

	if( item && item->GetIsTrashFolder() )
		menu_name = "Bookmark Trash Popup Menu";
	else
		menu_name = "Bookmark Item Popup Menu";

	if (menu_name)
	{
		OpPoint p = point + GetScreenRect().TopLeft();
		g_application->GetMenuHandler()->ShowPopupMenu(menu_name, PopupPlacement::AnchorAt(p, center), 0,use_keyboard_context);
	}

	return TRUE;
}

/***********************************************************************************
 *
 *	OnDragStart
 *
 ***********************************************************************************/

void OpBookmarkView::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget == m_folders_view || widget == m_hotlist_view)
	{
		HotlistModelItem* item = GetItemByPosition(pos);

		if( item )
		{
			// Close the folder when dragging
			if (item->IsFolder())
			{
				m_hotlist_view->OpenItem(pos, FALSE);
			}

			DesktopDragObject* drag_object = 0;

			drag_object = widget->GetDragObject(DRAG_TYPE_BOOKMARK, x, y);

			if( drag_object )
			{
				BookmarkItemData item_data;
				
				if( g_desktop_bookmark_manager->GetItemValue(item->GetID(), item_data ) )
				{
					drag_object->SetID(item->GetID());
					drag_object->SetURL(item_data.url.CStr());
					drag_object->SetTitle(item_data.name.CStr());
					DragDrop_Data_Utils::SetText(drag_object, item_data.url.CStr());
				}
			}

			g_drag_manager->StartDrag(drag_object, NULL, FALSE);
		}
	}
}

/***********************************************************************************
 *
 *	DropURLItem
 *
 ***********************************************************************************/
void OpBookmarkView::DropURLItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	INT32 new_selection_id = -1;
	INT32 target_id = target ? target->GetID() : -1;

	if( drag_object->GetURLs().GetCount() > 0 )
	{
		for( UINT32 i=0; i < drag_object->GetURLs().GetCount(); i++ )
		{
			BookmarkItemData item_data;
			if( i == 0 )
				item_data.name.Set( drag_object->GetTitle() );

			item_data.url.Set( *drag_object->GetURLs().Get(i) );

			if(i == 0)
			{
				item_data.description.Set( DragDrop_Data_Utils::GetText(drag_object) );
			}

			g_desktop_bookmark_manager->DropItem( item_data, target_id, insert_type, TRUE, drag_object->GetType(), &new_selection_id );
		}
	}
	else
	{
		BookmarkItemData item_data;
		item_data.name.Set( drag_object->GetTitle() );

		item_data.url.Set( drag_object->GetURL() );
		item_data.description.Set( DragDrop_Data_Utils::GetText(drag_object) );

		g_desktop_bookmark_manager->DropItem( item_data, target_id, insert_type, TRUE, drag_object->GetType(), &new_selection_id );
	}
}


/***********************************************************************************
 *
 *	UpdateDragObject
 *
 ***********************************************************************************/

void OpBookmarkView::UpdateDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	BookmarkItemData item_data;
	INT32 target_id = target ? target->GetID() : -1;
	if( g_desktop_bookmark_manager->DropItem( item_data, target_id, insert_type, FALSE, drag_object->GetType() ) )
	{
		drag_object->SetInsertType(insert_type);
		drag_object->SetDesktopDropType(DROP_COPY);
	}
}

/***********************************************************************************
 *
 *	DropHistoryItem
 *
 ***********************************************************************************/

void OpBookmarkView::DropHistoryItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
	{
		DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
		HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(i));
		
		if (history_item)
		{
			BookmarkItemData item_data;

			history_item->GetAddress(item_data.url);
			history_item->GetTitle(item_data.name);
			
			INT32 id = target ? target->GetID() : -1;
			
			// TODO/Note: The other paths here call DropItem. This does NewBookmark directly.
			// Means if duplicate, nothing will happen.
			// Note: Calling NewBookmark instead of Drop here, means the position you drop to wont be used, item is dropped to Root
			//g_hotlist_manager->NewBookmark( item_data, id, GetParentDesktopWindow(), FALSE );
			g_desktop_bookmark_manager->DropItem(item_data, id, insert_type, TRUE, drag_object->GetType());
		}
	}
}


/***********************************************************************************
 *
 *	UpdateHistoryDragObject
 *
 ***********************************************************************************/

void OpBookmarkView::UpdateHistoryDragObject(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();
	HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(0));
	
	if (history_item)
		return UpdateDragObject(target, drag_object, insert_type);
}

/***********************************************************************************
 *
 *	DropWindowItem
 *
 ***********************************************************************************/

void OpBookmarkView::DropWindowItem(HotlistModelItem* target, DesktopDragObject* drag_object, DesktopDragObject::InsertType insert_type)
{
	DesktopWindowCollection& model = g_application->GetDesktopWindowCollection();
	for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
	{
		DesktopWindowCollectionItem* item = model.GetItemByID(drag_object->GetID(i));
		if (!item || item->GetType() != WINDOW_TYPE_DOCUMENT || !item->GetDesktopWindow())
			continue;

		OpWindowCommander* document = static_cast<DocumentDesktopWindow*>(item->GetDesktopWindow())->GetWindowCommander();

		BookmarkItemData item_data;
		RETURN_VOID_IF_ERROR(item_data.name.Set(document->GetCurrentTitle()));
		RETURN_VOID_IF_ERROR(item_data.url.Set(document->GetCurrentURL(FALSE)));

		INT32 id = target ? target->GetID() : -1;
		g_desktop_bookmark_manager->DropItem(item_data, id, insert_type, TRUE, drag_object->GetType());
	}
}

BOOL OpBookmarkView::OnExternalDragDrop(OpTreeView* tree_view, OpTreeModelItem* item, DesktopDragObject* drag_object, DropType drop_type, DesktopDragObject::InsertType insert_type, BOOL drop, INT32& new_selection_id)
{
	// Set type correctly
	INT32 target_id = item ? item->GetID() : HotlistModel::BookmarkRoot;
	HotlistModelItem * target = m_model->GetItemByID(target_id);

	if (drag_object->GetType() == DRAG_TYPE_HISTORY)
	{
		if (drop)
		{
			DropHistoryItem(target, drag_object, insert_type);
		}
		else if( drag_object->GetIDCount() > 0 )
		{
			UpdateHistoryDragObject(target, drag_object, insert_type);
		}
	}
	else if (drag_object->GetType() == DRAG_TYPE_WINDOW)
	{
		if (drop)
		{
			DropWindowItem(target, drag_object, insert_type);
		}
		else if (drag_object->GetIDCount() > 0)
		{
			UpdateDragObject(target, drag_object, insert_type);
		}
	}
	else if (drag_object->GetURL() && *drag_object->GetURL() )
	{
		if (drop)
		{
			DropURLItem(target, drag_object, insert_type);
		}
		else
		{
			UpdateDragObject(target, drag_object, insert_type);
		}
	}
	return TRUE;
}

BOOL OpBookmarkView::OnInputAction(OpInputAction* action)
{
	BOOL special_folder;

	OpTreeView* tree_view = GetTreeViewFromInputContext(action->GetFirstInputContext());

	HotlistModelItem* item;

	item = (HotlistModelItem*)tree_view->GetSelectedItem();
	if (!item && m_view_style != STYLE_TREE && tree_view != m_folders_view)
		item = (HotlistModelItem*)m_folders_view->GetSelectedItem();

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			    case OpInputAction::ACTION_SAVE_SELECTED_BOOKMARKS_AS:
				case OpInputAction::ACTION_EXPORT_SELECTED_BOOKMARKS_TO_HTML:
				{
					// Disable if no items are selected or if the selected item is a separator
					child_action->SetEnabled((item != NULL) && !item->IsSeparator());
					return TRUE;
				}
			}
			break;
		} // ENC case ACTION_GET_ACTION_STATE

		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			if (!item && m_folders_view)
				item = (HotlistModelItem*)m_folders_view->GetSelectedItem();

			if( tree_view->GetSelectedItemCount() == 1 && item)
			{
				g_desktop_bookmark_manager->EditItem(item->GetID(), GetParentDesktopWindow() );
			}
			return TRUE;
		}
	}

	if (item)
	{
		// Selected item is special folder...
		special_folder = item->GetIsSpecialFolder();

		if (!special_folder)
		{
			// ...or inside special folder
			special_folder = item->GetParentFolder() && item->GetParentFolder()->GetIsSpecialFolder();
		}
	}
	else // !item
	{
		special_folder = FALSE;
	}

	// these actions don't need a selected item
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_EDIT_PROPERTIES:
				{
					BOOL enabled = (tree_view->GetSelectedItemCount() == 1);

					if (item && item->IsSeparator())
						enabled = FALSE;

					child_action->SetEnabled(enabled);
					return TRUE;
				}
				case OpInputAction::ACTION_OPEN_LINK:
				{
					if(child_action->GetActionData() || child_action->GetActionDataString())
						return FALSE;

					BOOL enabled = tree_view->GetSelectedItemCount();

					if (item && item->IsSeparator())
						enabled = FALSE;

					child_action->SetEnabled(enabled);
					return TRUE;
				}
				case OpInputAction::ACTION_PASTE:
				{
					child_action->SetEnabled(g_desktop_bookmark_manager->GetBookmarkModel()->Loaded() &&
											 g_desktop_bookmark_manager->GetClipboard()->GetCount());
					return TRUE;
				}
				case OpInputAction::ACTION_NEW_SEPERATOR:
				case OpInputAction::ACTION_NEW_FOLDER:
				case OpInputAction::ACTION_NEW_BOOKMARK:
					child_action->SetEnabled(g_desktop_bookmark_manager->GetBookmarkModel()->Loaded());
					return TRUE;
			}
			break;
		}

		case OpInputAction::ACTION_NEW_SEPERATOR:
		{
			int item_id = item ? item->GetID() : GetCurrentFolderID();
			if (item)
			{
				if (item->GetIsTrashFolder() || item ->GetIsInsideTrashFolder())
				{
					item_id = item->GetModel()->GetModelType();
				}
				else
					item_id = item->GetID();
			}

			g_desktop_bookmark_manager->NewSeparator(item_id);
			return TRUE;
		}

		case OpInputAction::ACTION_NEW_FOLDER:
		{
			INT32 item_id;
			if (item)
			{
				item_id = item->GetID();
			}
			else
			{
				item_id = GetCurrentFolderID();
			}

			g_desktop_bookmark_manager->NewBookmarkFolder(NULL, item_id, tree_view);
			return TRUE;
		}
		
		case OpInputAction::ACTION_NEW_BOOKMARK:
		{
			g_desktop_bookmark_manager->NewBookmark(item ? item->GetID() : GetCurrentFolderID(), GetParentDesktopWindow() );
			return TRUE;
		}

		case OpInputAction::ACTION_DELETE:
		case OpInputAction::ACTION_CUT:
		{
			OpINT32Vector id_list;
			tree_view->GetSelectedItems( id_list );

			if( id_list.GetCount() == 0 )
			{
				return FALSE;
			}
			if( action->GetAction() == OpInputAction::ACTION_DELETE )
			{
				// remember the next item to active after deleting.
				INT32 last_item = tree_view->GetItemByID(id_list.Get(id_list.GetCount()-1));
				INT32 last_line = tree_view->GetLineByItem(last_item);
				INT32 select_line = last_line - id_list.GetCount() + 1;
				g_desktop_bookmark_manager->Delete(id_list);
				INT32 line = (select_line >= tree_view->GetLineCount()) ? tree_view->GetLineCount() - 1 : select_line;
				tree_view->SetSelectedItem(tree_view->GetItemByLine(line));
			}
			else if( action->GetAction() == OpInputAction::ACTION_CUT )
			{
				g_desktop_bookmark_manager->CutItems ( id_list );
			}
			return TRUE;
		}
		case OpInputAction::ACTION_COPY:
		{
			OpINT32Vector id_list;
			tree_view->GetSelectedItems( id_list );
			if( id_list.GetCount() == 0 )
			{
				return FALSE;
			}
			else if( action->GetAction() == OpInputAction::ACTION_COPY )
			{
				g_desktop_bookmark_manager->CopyItems( id_list );
			}
			return TRUE;
		}
		break;

		case OpInputAction::ACTION_PASTE:
		{
			g_desktop_bookmark_manager->PasteItems(item );
			return TRUE;
		}

		case OpInputAction::ACTION_EXPORT_SELECTED_BOOKMARKS_TO_HTML:
		{
			if( tree_view->GetSelectedItemCount() > 0 && item )
			{
				OpINT32Vector item_list;
				tree_view->GetSelectedItems(item_list);
				g_hotlist_manager->SaveSelectedAs( item->GetID(), item_list, HotlistManager::SaveAs_Html );
			}
			return TRUE;
		}

		case OpInputAction::ACTION_SAVE_SELECTED_BOOKMARKS_AS:
		{
			OpINT32Vector id_list;
			tree_view->GetSelectedItems( id_list, TRUE );
			if( id_list.GetCount() > 0 && item )
			{
				g_hotlist_manager->SaveSelectedAs( item->GetID(), id_list, HotlistManager::SaveAs_Export );
			}
			return TRUE;
		}

		// Set where we should add the bookmarks to, the action is performed in BrowserDesktopWindow
		case OpInputAction::ACTION_ADD_ALL_TO_BOOKMARKS:
		{
			action->SetActionData(GetSelectedFolderID());
		}
		break;
	}

	return OpHotlistTreeView::OnInputAction(action);
}

INT32 OpBookmarkView::OnDropItem(HotlistModelItem* to,DesktopDragObject* drag_object, INT32 i, DropType drop_type, DesktopDragObject::InsertType insert_type, INT32 *first_id, BOOL force_delete)
{
	HotlistModelItem* from = m_model->GetItemByID(drag_object->GetID(i));
	
	CoreBookmarkPos pos(to, insert_type);
	g_bookmark_manager->MoveBookmark(BookmarkWrapper(from)->GetCoreItem(), pos.previous, pos.parent);

	*first_id = from->GetID();

	return 0;
}

void OpBookmarkView::SetModel(BookmarkModel* model)
{
	OP_ASSERT(!m_model);
	if(!m_model)
	{
		m_model = model;
		// --- SetTreeModel of TreeView --------------
		m_hotlist_view->SetTreeModel(m_model,m_sort_column, m_sort_ascending);
		m_folders_view->SetTreeModel(m_model,m_sort_column, m_sort_ascending);
	
		SetDetailed(m_detailed, TRUE); // was called once in Init() but didn't take effect as the model wasn't set there.
		InitModel(); 
	}
}

void OpBookmarkView::OnBookmarkModelLoaded()
{
	SetIgnoresMouse(FALSE);
	SetModel(g_hotlist_manager->GetBookmarksModel());
}

void OpBookmarkView::OnDeleted()
{	
	if(g_desktop_bookmark_manager)
		g_desktop_bookmark_manager->RemoveBookmarkListener(this);

	OpHotlistTreeView::OnDeleted();
}

