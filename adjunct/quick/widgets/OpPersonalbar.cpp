/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"
#include "OpPersonalbar.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/dialogs/ContactPropertiesDialog.h"
#include "adjunct/quick/dialogs/PreferencesDialog.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/models/DesktopBookmark.h"
#include "adjunct/quick/widgets/OpSearchEdit.h"

#include "modules/dragdrop/dragdrop_data_utils.h"
#include "modules/dragdrop/dragdrop_manager.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/pi/OpDragManager.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/WidgetContainer.h"


#ifdef DESKTOP_UTIL_SEARCH_ENGINES
#include "adjunct/desktop_util/search/search_net.h"
#include "adjunct/desktop_util/search/searchenginemanager.h"
#endif // DESKTOP_UTIL_SEARCH_ENGINES

/***********************************************************************************
**
**	OpPersonalbar
**
***********************************************************************************/

OP_STATUS OpPersonalbar::Construct(OpPersonalbar** obj)
{ 
	*obj = OP_NEW(OpPersonalbar, (PrefsCollectionUI::PersonalbarAlignment));
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OP_STATUS OpPersonalbarInline::Construct(OpPersonalbarInline** obj)
{ 
	*obj = OP_NEW(OpPersonalbarInline, (PrefsCollectionUI::PersonalbarInlineAlignment));
	if (*obj == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError((*obj)->init_status))
	{
		OP_DELETE(*obj);
		return OpStatus::ERR_NO_MEMORY;
	}
	return OpStatus::OK;
}

OpPersonalbarInline::OpPersonalbarInline(PrefsCollectionUI::integerpref prefs_setting) : OpPersonalbar(prefs_setting)
{
	m_is_inline = TRUE;
}

void OpPersonalbarInline::EnableTransparentSkin(BOOL enable)
{
	if (enable)
		GetBorderSkin()->SetImage("Personalbar Transparent Skin", "Personalbar Inline Skin");
	else
		GetBorderSkin()->SetImage("Personalbar Inline Skin");
}


OpPersonalbar::OpPersonalbar(PrefsCollectionUI::integerpref prefs_setting) 
: OpToolbar(prefs_setting), m_is_inline(FALSE), m_listeners_set(FALSE), m_disable_indicator(FALSE)
{
	SetListener(this);
	SetWrapping(OpBar::WRAPPING_OFF);
	SetShrinkToFit(TRUE);
	// Not use FIXED_HEIGHT_BUTTON_AND_EDIT since edit it too high
	SetFixedHeight(FIXED_HEIGHT_BUTTON);
	SetButtonType(OpButton::TYPE_LINK);
	SetStandardToolbar(FALSE);
	SetTabStop(TRUE);
}

void OpPersonalbar::OnDeleted()
{
	if(m_listeners_set)
	{
		g_desktop_bookmark_manager->RemoveBookmarkListener(this);
		g_hotlist_manager->GetBookmarksModel()->RemoveListener(this);
		g_hotlist_manager->GetContactsModel()->RemoveListener(this);

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		if (g_searchEngineManager)
			g_searchEngineManager->RemoveListener(this);
#endif // DESKTOP_UTIL_SEARCH_ENGINES

	}
	OpToolbar::OnDeleted();
}

/***********************************************************************************
**
**	AddItem()
**
***********************************************************************************/

BOOL OpPersonalbar::AddItem(OpTreeModel* model, INT32 model_pos, BOOL init)
{
	OpTreeModelItem* model_item = model->GetItemByPosition(model_pos);

 	BookmarkItemData item_data;
	INT32 personalbar_pos;

	if (model_item->GetType() == SEARCHTEMPLATE_TYPE)
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		SearchTemplate* search = (SearchTemplate*) model_item;
		personalbar_pos = search->GetPersonalbarPos();

		if (personalbar_pos == -1)
		{
			return FALSE;
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	}
	else // Assumption: then its hotlist
	{
		// BOOKMARKS!

		if (!g_hotlist_manager->IsOnPersonalBar(model_item))
		{
			return FALSE;
		}
		if (!g_desktop_bookmark_manager->GetItemValue(model_item, item_data))
		{
			return FALSE;
		}
		personalbar_pos = item_data.personalbar_position;
	}

	if (init)
	{
		for (INT32 i = 0; i < GetWidgetCount(); i++)
		{
			INT32 compare_pos = -1;

			OpTreeModel* model = (OpTreeModel*) GetUserData(i);
			INT32 id = GetWidget(i)->GetID();

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			if (model != g_searchEngineManager)
			{
#endif // DESKTOP_UTIL_SEARCH_ENGINES

				compare_pos = g_hotlist_manager->GetPersonalBarPosition(id);

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			}
			else
			{
				OpTreeModelItem* search;
				g_searchEngineManager->GetItemByID(id, search);
				if(search)
				{
					compare_pos = ((SearchTemplate*)search)->GetPersonalbarPos();
				}
			}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

			if (compare_pos >= personalbar_pos)
			{
				if( compare_pos == -1 && personalbar_pos == -1 )
				{
					personalbar_pos = GetWidgetCount();
				}
				else
				{
					personalbar_pos = i;
				}
				break;
			}
		}
		if( GetWidgetCount() == 0  && personalbar_pos == -1 )
		{
			personalbar_pos = 0;
		}
	}

	if (model_item->GetType() == SEARCHTEMPLATE_TYPE)
	{

		OpSearchEdit* edit = OP_NEW(OpSearchEdit, (model_pos)); //model pos equals search number.. clean up later
		if (edit)
		{
			edit->SetID(model_item->GetID());
			edit->SetUserData(model);
			AddWidget(edit, personalbar_pos);
		}
	}
	else
	{
		OpInputAction* action = NULL;

		if (model_item->GetType() == OpTypedObject::BOOKMARK_TYPE)
		{
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPEN_LINK));
		}
#ifdef M2_SUPPORT
		else if (model_item->GetType() == OpTypedObject::CONTACT_TYPE)
		{
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_VIEW_MESSAGES_FROM_CONTACT));
		}
#endif // M2_SUPPORT
		else if (model_item->GetType() == OpTypedObject::FOLDER_TYPE )
		{
			action = OP_NEW(OpInputAction, (OpInputAction::ACTION_SHOW_POPUP_MENU));
			if (!action)
				return TRUE;
			if( model == g_hotlist_manager->GetBookmarksModel() )
			{
				action->SetActionDataString(UNI_L("Bookmark Folder Menu"));
			}
			else if( model == g_hotlist_manager->GetContactsModel() )
			{
				action->SetActionDataString(UNI_L("Contact Folder Menu"));
			}
		}

		if( action )
		{
			action->SetActionData(model_item->GetID());

			((HotlistModelItem*)model_item)->GetInfoText(action->GetActionInfo());

			OpButton *button = AddButton(item_data.name.CStr(), item_data.direct_image_pointer, action, model, personalbar_pos);
			if( button )
			{
				Image icon = ((HotlistModelItem*)model_item)->GetIcon();
				button->GetForegroundSkin()->SetBitmapImage( icon, FALSE );
				button->GetForegroundSkin()->SetRestrictImageSize(TRUE);

				button->SetID(model_item->GetID());
				button->SetEllipsis(g_pcui->GetIntegerPref(PrefsCollectionUI::EllipsisInCenter) == 1 ? ELLIPSIS_CENTER : ELLIPSIS_END);
				button->SetTabStop(TRUE);
			}
		}
	}
	return TRUE;
}

/***********************************************************************************
**
**	RemoveItem()
**
***********************************************************************************/

BOOL OpPersonalbar::RemoveItem(OpTreeModel* model, INT32 model_pos)
{
	OpTreeModelItem* model_item = model->GetItemByPosition(model_pos);

	INT32 pos = FindItem(model_item->GetID(), model);

	if (pos != -1)
	{
		RemoveWidget(pos);
		return TRUE;
	}

	return FALSE;
}

/***********************************************************************************
**
**	ChangeItem()
**
***********************************************************************************/

BOOL OpPersonalbar::ChangeItem(OpTreeModel* model, INT32 model_pos)
{
	if( !model )
	{
		return FALSE;
 	}

	OpTreeModelItem* model_item = model->GetItemByPosition(model_pos);
	if( !model_item )
	{
		return FALSE;
	}

	INT32 pos = FindItem(model_item->GetID(), model);
	if (pos == -1)
		return AddItem(model, model_pos);

	if (model_item->GetType() == SEARCHTEMPLATE_TYPE)
	{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		SearchTemplate* search = (SearchTemplate*) model_item;
		INT32 personalbar_pos = search->GetPersonalbarPos();

		if (personalbar_pos != -1)
		{
			MoveWidget(pos, personalbar_pos);
			return TRUE;
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

		return RemoveItem(model, model_pos);
	}

	HotlistManager::ItemData item_data;
	if( !g_hotlist_manager->IsOnPersonalBar(model_item) )
	{
		return RemoveItem(model, model_pos);
	}
	if( !g_hotlist_manager->GetItemValue(model_item, item_data ) )
	{
		return FALSE;
	}

	OpWidget* widget = GetWidget(pos);

	widget->GetForegroundSkin()->SetImage(item_data.direct_image_pointer);

	if( ((HotlistModelItem*)model_item)->IsBookmark())
	{
		Image icon = ((Bookmark*)model_item)->GetIcon();
		widget->GetForegroundSkin()->SetBitmapImage( icon );
		widget->GetForegroundSkin()->SetRestrictImageSize(TRUE);
	}

	widget->SetText(item_data.name.CStr());
	if( item_data.personalbar_position != -1 )
	{
		MoveWidget(pos,item_data.personalbar_position);
	}

	return TRUE;
}

/***********************************************************************************
**
**	FindItem()
**
***********************************************************************************/

INT32 OpPersonalbar::FindItem(INT32 id)
{
	for (INT32 i = 0; i < GetWidgetCount(); i++)
	{
		if (id == GetWidget(i)->GetID())
		{
			return i;
		}
	}

	return -1;
}


// We can have the same identifier in search and bookmarks/contacts model. That will
// lead to confusion so we test for model as well.
INT32 OpPersonalbar::FindItem(INT32 id, OpTreeModel* model_that_id_belongs_to)
{
	for (INT32 i = 0; i < GetWidgetCount(); i++)
	{
		OpWidget* widget = GetWidget(i);

		if (id == widget->GetID())
		{
			if( model_that_id_belongs_to )
			{
				BOOL is_search_template = widget->GetType() == WIDGET_TYPE_SEARCH_EDIT;
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				BOOL is_search_model    = g_searchEngineManager == model_that_id_belongs_to;
				if( is_search_template != is_search_model )
				{
					continue;
				}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			}

			return i;
		}
	}

	return -1;
}

HotlistModelItem* OpPersonalbar::GetModelItemFromPos(int pos)
{
	OpTreeModelItem* item = NULL;
	OpWidget* widget = GetWidget(pos);
	if (widget && widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
	{
		OpTreeModel* model = (OpTreeModel*)widget->GetUserData();
		if (model)
		{
			model->GetItemByID(widget->GetID(), item);
		}
	}
	return (HotlistModelItem*)item;
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void OpPersonalbar::OnLayout(BOOL compute_size_only, INT32 available_width, INT32 available_height, INT32& used_width, INT32& used_height)
{
	OpToolbar::OnLayout(compute_size_only, available_width, available_height, used_width, used_height);
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void OpPersonalbar::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	OpWidget* widget = GetWidgetByType(WIDGET_TYPE_SEARCH_EDIT);

	if (widget)
	{
		widget->SetFocus(reason);
	}
}

/***********************************************************************************
**
**	GetDragSourcePos
**
***********************************************************************************/

INT32 OpPersonalbar::GetDragSourcePos(DesktopDragObject* drag_object)
{
	return -1;
}

/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpPersonalbar::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{

	DesktopDragObject* drag_object = NULL;

	OpTreeModel* model = (OpTreeModel*) widget->GetUserData();

	INT32 id = widget->GetID();

	if (model == g_hotlist_manager->GetBookmarksModel())
	{
		drag_object = widget->GetDragObject(OpTypedObject::DRAG_TYPE_BOOKMARK, x, y);

		HotlistManager::ItemData item_data;
		if( !g_hotlist_manager->GetItemValue(id, item_data ) )
		{
			return;
		}
		else
		{
			drag_object->AddID(id);
			drag_object->SetURL(item_data.url.CStr());
			drag_object->SetTitle(item_data.name.CStr());
			
		}
	}
	else if (model == g_hotlist_manager->GetContactsModel())
	{
		drag_object = widget->GetDragObject(OpTypedObject::DRAG_TYPE_CONTACT, x, y);
		drag_object->AddID(id);
	}

	if (drag_object)
	{
		g_drag_manager->StartDrag(drag_object, NULL, FALSE);
	}
}

/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/
void OpPersonalbar::DragMoveDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y, BOOL drop)
{
	DesktopDragObject* drag_object = static_cast<DesktopDragObject *>(op_drag_object);

	if (widget == this)
	{
		// Make it possible to drop into folder
		int target_pos = GetWidgetPosition(x,y);
		HotlistModelItem* target_folder = NULL;
		OpWidget* target_widget = NULL;
		BOOL drag_into_folder = FALSE;

		if (target_pos >= 0 && target_pos < GetWidgetCount())
		{
			target_widget = GetWidget(target_pos);
			HotlistModelItem* item = GetModelItemFromPos(target_pos);

			if (target_widget && target_widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON
				&& item && item->IsFolder())
			{
				OpRect rect = target_widget->GetRect();
				if (x > rect.x + rect.width/4 && x < rect.x + rect.width*3/4 && rect.Contains(OpPoint(x,y)))
				{
					target_folder = item;
				}
			}
		}

		if (drag_object->GetType() == DRAG_TYPE_SEARCH_EDIT)
		{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
			SearchTemplate* search = g_searchEngineManager->GetSearchEngine(drag_object->GetID());

			BOOL copy = search ? (search->GetPersonalbarPos() == -1) : FALSE;

			if (drop && search)
			{
				search->SetPersonalbarPos(pos);
				g_searchEngineManager->ChangeItem(search, drag_object->GetID(), SearchEngineManager::FLAG_PERSONALBAR);
			}
			if(!drop && search)
			{
				drag_object->SetDesktopDropType(copy ? DROP_COPY : DROP_MOVE);
			}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
		}
		else if (drag_object->GetType() == DRAG_TYPE_BOOKMARK || drag_object->GetType() == DRAG_TYPE_CONTACT)
		{
			BOOL copy = FALSE;
			HotlistManager::ItemData item_data;
			if( g_hotlist_manager->GetItemValue(drag_object->GetID(0), item_data) )
			{
				copy = item_data.personalbar_position == -1;
			}

			if (drop)
			{
				for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
				{
					g_hotlist_manager->SetPersonalBarPosition( drag_object->GetID(i), pos, TRUE);
				}
			}
			else
				drag_object->SetDesktopDropType(copy ? DROP_COPY : DROP_MOVE);
		}
		else if (drag_object->GetType() == DRAG_TYPE_HISTORY)
		{
			DesktopHistoryModel* history_model = DesktopHistoryModel::GetInstance();

			if (drop)
			{
				for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
				{
					HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(i));

					if (history_item)
					{
						BookmarkItemData item_data;

						OpString title;
						OpString address;

						history_item->GetTitle(title);
						history_item->GetAddress(address);

						item_data.name.Set( title.CStr() );
						item_data.url.Set( address.CStr() );
						item_data.personalbar_position = target_folder ? -1 : pos;

						g_desktop_bookmark_manager->DropItem( item_data, target_folder ? target_folder->GetID() : HotlistModel::BookmarkRoot,
							DesktopDragObject::INSERT_INTO, TRUE, DRAG_TYPE_HISTORY );
					}
				}
			}
			else if( drag_object->GetIDCount() > 0 )
			{
				HistoryModelItem* history_item = history_model->FindItem(drag_object->GetID(0));			

				if (history_item)
				{
					BookmarkItemData item_data;
					item_data.personalbar_position = target_folder ? -1 : pos;

					if( g_desktop_bookmark_manager->DropItem( item_data, target_folder ? target_folder->GetID() : HotlistModel::BookmarkRoot,
						DesktopDragObject::INSERT_INTO, FALSE, DRAG_TYPE_HISTORY ) )
					{
						drag_object->SetDesktopDropType(DROP_COPY);
						drag_into_folder = target_folder != NULL;
					}
				}
			}
		}
		else if (drag_object->GetURL() && *drag_object->GetURL() )
		{
			if (drop)
			{
				if( drag_object->GetURLs().GetCount() > 0 )
				{				   
					for( UINT32 i=0; i<drag_object->GetURLs().GetCount(); i++ )
					{
						BookmarkItemData item_data;
						if( i == 0 )
							item_data.name.Set( drag_object->GetTitle() );

						item_data.url.Set( *drag_object->GetURLs().Get(i) );
						item_data.personalbar_position = target_folder ? -1 : pos+i;
						if (i == 0)
							item_data.description.Set(DragDrop_Data_Utils::GetText(drag_object));

						g_desktop_bookmark_manager->DropItem( item_data, target_folder ? target_folder->GetID() : HotlistModel::BookmarkRoot,
							DesktopDragObject::INSERT_INTO, TRUE, drag_object->GetType() );
					}
				}
				else
				{
					BookmarkItemData item_data;
					item_data.name.Set( drag_object->GetTitle() );
					item_data.url.Set( drag_object->GetURL() );
					item_data.personalbar_position = target_folder ? -1 : pos;

					item_data.description.Set(DragDrop_Data_Utils::GetText(drag_object));

					g_desktop_bookmark_manager->DropItem( item_data, target_folder ? target_folder->GetID() : HotlistModel::BookmarkRoot,
						DesktopDragObject::INSERT_INTO, TRUE, drag_object->GetType() );
				}
			}
			else
			{
				BookmarkItemData item_data;
				item_data.personalbar_position = pos;

				if( g_desktop_bookmark_manager->DropItem( item_data, target_folder ? target_folder->GetID() : HotlistModel::BookmarkRoot,
					DesktopDragObject::INSERT_INTO, FALSE, drag_object->GetType()) )
				{
					drag_object->SetDesktopDropType(DROP_COPY);
					drag_into_folder = target_folder != NULL;
				}
			}
		}

		if (drop)
		{
			Write();
		}

		// Highlight folder button
		if (target_widget)
			target_widget->SetValue(drag_into_folder);

		m_disable_indicator = drag_into_folder;

		return;
	}
	if(drop)
	{
		OpToolbar::OnDragDrop(widget, drag_object, pos, x, y);
	}
	else
	{
		OpToolbar::OnDragMove(widget, drag_object, pos, x, y);
	}
}

/***********************************************************************************
**
**	OnDragMove
**
***********************************************************************************/

void OpPersonalbar::OnDragMove(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DragMoveDrop(widget, op_drag_object, pos, x, y, FALSE);
}
/***********************************************************************************
**
**	OnDragDrop
**
***********************************************************************************/

void OpPersonalbar::OnDragDrop(OpWidget* widget, OpDragObject* op_drag_object, INT32 pos, INT32 x, INT32 y)
{
	DragMoveDrop(widget, op_drag_object, pos, x, y, TRUE);
}

void OpPersonalbar::OnDragLeave(OpWidget* widget, OpDragObject* op_drag_object)
{
	DesktopDragObject* drag_object = (DesktopDragObject*)op_drag_object;

	// Remove the highlight when leaving folder
	if (widget && widget->GetType() == OpTypedObject::WIDGET_TYPE_BUTTON)
	{
		widget->SetValue(0);
	}
	OpToolbar::OnDragLeave(widget, drag_object);
}

/***********************************************************************************
**
**	OnContextMenu
**
***********************************************************************************/

BOOL OpPersonalbar::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	const PopupPlacement at_cursor = PopupPlacement::AnchorAtCursor();
	if (widget == this)
	{
		if(m_is_inline)
		{
			g_application->GetMenuHandler()->ShowPopupMenu(child_index == -1 ? "Personalbar Inline Popup Menu" : "Personalbar Inline Item Popup Menu",
			                                               at_cursor, 0, keyboard_invoked);
		}
		else
		{
			g_application->GetMenuHandler()->ShowPopupMenu(child_index == -1 ? "Personalbar Popup Menu" : "Personalbar Item Popup Menu",
			                                               at_cursor, 0, keyboard_invoked);

		}
		return TRUE;
	}
	else if( widget && widget->GetType() == WIDGET_TYPE_SEARCH_EDIT )
	{
		g_application->GetMenuHandler()->ShowPopupMenu("Personalbar Edit Item Popup Menu", at_cursor, 0, keyboard_invoked);
		return TRUE;
	}

	return OpToolbar::OnContextMenu(widget, child_index, menu_point, avoid_rect, keyboard_invoked);
}

/***********************************************************************************
 **
 **	OnItemAdded - item was added to the tree_model (if on pbar, should be added)
 **  
 **
 **
 ***********************************************************************************/

void OpPersonalbar::OnItemAdded(OpTreeModel* tree_model, INT32 pos)
{
	AddItem(tree_model, pos, TRUE);
}

/***********************************************************************************
**
**	OnItemRemoving
**
***********************************************************************************/

void OpPersonalbar::OnItemRemoving(OpTreeModel* tree_model, INT32 pos)
{
	RemoveItem(tree_model, pos);
}

/***********************************************************************************
**
**	OnItemChanged
**
***********************************************************************************/

void OpPersonalbar::OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort)
{
	ChangeItem(tree_model, pos);
}

/***********************************************************************************
**
**	OnTreeChanged
**
***********************************************************************************/

void OpPersonalbar::OnTreeChanged(OpTreeModel* tree_model)
{
	if( tree_model == g_hotlist_manager->GetBookmarksModel() )
	{
		for( INT32 i=GetWidgetCount()-1; i>=0; i-- )
		{
			OpWidget* widget = GetWidget(i);
			if( widget && widget->GetType() != WIDGET_TYPE_SEARCH_EDIT )
			{
				RemoveWidget(i);
			}
		}
	}
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	else if( tree_model == g_searchEngineManager )
	{
		for( INT32 i=GetWidgetCount()-1; i>=0; i-- )
		{
			OpWidget* widget = GetWidget(i);
			if( widget && widget->GetType() == WIDGET_TYPE_SEARCH_EDIT )
			{
				RemoveWidget(i);
			}
		}
	}
#endif // DESKTOP_UTIL_SEARCH_ENGINES

	for (INT32 i = 0; i < tree_model->GetItemCount(); i++)
	{
		AddItem(tree_model, i, TRUE);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpPersonalbar::OnInputAction(OpInputAction* action)
{
	OpTreeModel* model = (OpTreeModel*) GetUserData(GetFocused());

	if (model)
	{
		INT32 id = GetWidget(GetFocused())->GetID();

		switch (action->GetAction())
		{
			case OpInputAction::ACTION_EDIT_PROPERTIES:
			{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				if (model != g_searchEngineManager)
				{
#endif // DESKTOP_UTIL_SEARCH_ENGINES
					g_desktop_bookmark_manager->EditItem( id, GetWidgetContainer()->GetParentDesktopWindow());
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				}
				else
				{
					g_input_manager->InvokeAction(OpInputAction::ACTION_SHOW_PREFERENCES, NewPreferencesDialog::SearchPage);
				}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
				return TRUE;
			}

			case OpInputAction::ACTION_HIDE_FROM_PERSONAL_BAR:
			{
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				if (model != g_searchEngineManager)
				{
#endif // DESKTOP_UTIL_SEARCH_ENGINES
					g_hotlist_manager->ShowOnPersonalBar(id, FALSE );
					Write();
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
				}
				else
				{
					OpTreeModelItem* search;
					g_searchEngineManager->GetItemByID(id, search);
					if(search)
					{
						((SearchTemplate*)search)->SetPersonalbarPos(-1);
						g_searchEngineManager->ChangeItem((SearchTemplate*)search, 
														  g_searchEngineManager->FindItem((SearchTemplate*)search), 
														  SearchEngineManager::FLAG_PERSONALBAR);
					}
					Write();
				}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
				return TRUE;
			}
		}
	}

	BOOL handled = OpToolbar::OnInputAction(action);

	if(!g_application->IsCustomizingToolbars())
	{
		// this action comes from a menu so only refresh it then
		switch (action->GetAction())
		{
			case OpInputAction::ACTION_SET_ALIGNMENT:
			case OpInputAction::ACTION_SET_BUTTON_STYLE:
			{
				// force a relayout of the window
				g_application->SettingsChanged(SETTINGS_TOOLBAR_SETUP);
				break;
			}
		}

	}
	return handled;
}

/***********************************************************************************
**
**	Write
**
***********************************************************************************/

void OpPersonalbar::Write()
{
	for (INT32 i = 0; i < GetWidgetCount(); i++)
	{
		OpTreeModel* model = (OpTreeModel*) GetUserData(i);
		INT32 id = GetWidget(i)->GetID();

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		if (model != g_searchEngineManager)
		{
#endif // DESKTOP_UTIL_SEARCH_ENGINES
			g_hotlist_manager->SetPersonalBarPosition(id, i, FALSE);
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		}
		else
		{
			OpTreeModelItem* search;
			g_searchEngineManager->GetItemByID(id, search);
			if(search)
			{
				((SearchTemplate*)search)->SetPersonalbarPos(i);
			}
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	}
#ifdef DESKTOP_UTIL_SEARCH_ENGINES
	g_searchEngineManager->Write();
#endif // DESKTOP_UTIL_SEARCH_ENGINES
}

void OpPersonalbar::EnableTransparentSkin(BOOL enable)
{
	if (enable)
		GetBorderSkin()->SetImage("Personalbar Transparent Skin", "Personalbar Skin");
	else
		GetBorderSkin()->SetImage("Personalbar Skin");
}

void OpPersonalbar::OnBookmarkModelLoaded()
{
	OnTreeChanged(g_hotlist_manager->GetBookmarksModel());
	OP_STATUS status = g_hotlist_manager->GetBookmarksModel()->AddListener(this);
	CHECK_STATUS(status);
}


void OpPersonalbar::OnShow(BOOL show)
{
	// don't load anything until we are visible, saves time and resources
	if(show && !m_listeners_set)
	{
		OP_STATUS status;

		m_listeners_set = TRUE;

		// don't load bookmarks if it's not ready yet.
		if (g_hotlist_manager->GetBookmarksModel()->Loaded())
		{
			OnTreeChanged(g_hotlist_manager->GetBookmarksModel());
			status = g_hotlist_manager->GetBookmarksModel()->AddListener(this);
			RETURN_VOID_IF_ERROR(status);
		}
		else
		{
			g_desktop_bookmark_manager->AddBookmarkListener(this);
		}

		OnTreeChanged(g_hotlist_manager->GetContactsModel());
		status = g_hotlist_manager->GetContactsModel()->AddListener(this);
		RETURN_VOID_IF_ERROR(status);

#ifdef DESKTOP_UTIL_SEARCH_ENGINES
		if (g_searchEngineManager)
		{
			OnTreeChanged(g_searchEngineManager);
			status = g_searchEngineManager->AddListener(this);
			RETURN_VOID_IF_ERROR(status);
		}
#endif // DESKTOP_UTIL_SEARCH_ENGINES
	}
}


INT32 OpPersonalbar::GetPosition(OpWidget* wdg)
{
	INT32 count = GetWidgetCount();

	for (INT32 i = 0; i < count; ++i)
	{
		OpWidget *widget = GetWidget(i);
		if (widget == wdg)
			return i;
	}
	return -1;
}
