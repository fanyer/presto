/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpHistoryView.h"

#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/visited_search_threaded/VisitedSearchThread.h"

#include "adjunct/quick/managers/DesktopBookmarkManager.h"

#include "adjunct/quick/menus/DesktopMenuHandler.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"

/***********************************************************************************
 **	OpHistoryView
 ***********************************************************************************/

DEFINE_CONSTRUCT(OpHistoryView)

OpHistoryView::OpHistoryView()
{
	OP_STATUS status = OpTreeView::Construct(&m_history_view);
	CHECK_STATUS(status);
	AddChild(m_history_view, TRUE);

	m_history_view->SetListener(this);
	m_history_view->SetShowColumnHeaders(FALSE);
	m_history_view->SetMultiselectable(TRUE);
	m_history_view->SetShowThreadImage(TRUE);
	m_history_view->SetBoldFolders(TRUE);
	m_history_view->SetRestrictImageSize(TRUE);
	m_history_view->SetForceEmptyMatchQuery(TRUE);
	m_history_view->SetAllowMultiLineItems(TRUE);
	m_history_view->SetAsyncMatch(TRUE);
	m_history_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_BOOKMARK)
	{
		m_history_view->SetDragEnabled(TRUE);
	}

	//Default view is Time Site
	m_history_model = DesktopHistoryModel::GetInstance();
	if (!m_history_model)
	{
		init_status = OpStatus::ERR_NO_MEMORY;
		return;
	}

	m_history_model->ChangeMode(m_history_model->GetMode());
	m_history_view->SetTreeModel(m_history_model);

	m_detailed = FALSE;
	SetDetailed(FALSE, TRUE);

    //Listen to model to highlight added element
	status = m_history_model->AddListener(this);
	CHECK_STATUS(status);

	//Listen to view to react to quick finds and use the indexed history
	status = m_history_view->AddTreeViewListener(this);
	CHECK_STATUS(status);

	g_main_message_handler->SetCallBack(this, MSG_VPS_SEARCH_RESULT, 0);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/
OpHistoryView::~OpHistoryView()
{
	g_main_message_handler->UnsetCallBacks(this);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/

//Highlight added element if this is not an initialisation of the model
void OpHistoryView::OnItemAdded(OpTreeModel* tree_model, INT32 item)
{
	//Don't move focus when inititializing
	if(!m_history_model->Initialized()) //Still initializing
		return;

	HistoryModelItem * h_item   = (HistoryModelItem *) tree_model->GetItemByPosition(item);

	if(!h_item) //shouldn't happen
		return;

	//Don't move focus with folders
	if(h_item->GetType() == OpTypedObject::FOLDER_TYPE)
		return;

	//Only move when items is actually moving
	if(!h_item->IsMoving())
		return;

	//and are moving within the same folder:
	HistoryModelItem * h_parent = (HistoryModelItem *) h_item->GetParentItem();

	if(!h_parent)
		return;

	if(h_item->GetOldParentId() != h_parent->GetID())
		return;

	if(!m_history_view->IsItemOpen(h_parent->GetIndex()))
		return;

	//Focus on item
	m_history_view->SetSelectedItem(item);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/

OP_STATUS OpHistoryView::SearchContent()
{
#ifdef VPS_WRAPPER

	RETURN_IF_ERROR(g_visited_search->Search(m_history_view->GetMatchText(),
											 VisitedSearch::RankSort,
											 KAll,
											 500,
											 UNI_L("<b>"),
											 UNI_L("</b>"),
											 16,
											 this));

#endif // VPS_WRAPPER

	return OpStatus::OK;
}

/***********************************************************************************
**
** HandleCallback
**
***********************************************************************************/
void OpHistoryView::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg != MSG_VPS_SEARCH_RESULT || ((MessageObject *) par1 != this))
	{
		OpWidget::HandleCallback(msg, par1, par2);
		return;
	}

#ifdef VPS_WRAPPER

	OpAutoVector<VisitedSearch::SearchResult> * result = (OpAutoVector<VisitedSearch::SearchResult> *) par2;

	if(result)
	{
		// A result was returned, mark the items as

		for(unsigned int i = 0; i < result->GetCount(); i++)
		{
			VisitedSearch::SearchResult * search_result = result->Get(i);
			OpString url;
			url.SetFromUTF8(search_result->url.CStr());
			
			HistoryModelPage* item = m_history_model->GetItemByUrl(url);
			
			if(item)
				item->Match(search_result->excerpt);
		}
		
		OP_DELETE(result);
	}

	m_history_view->UpdateAfterMatch(FALSE);

#endif // VPS_WRAPPER
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/

void OpHistoryView::OnDeleted()
{
	m_history_model->RemoveListener(this);

	OpWidget::OnDeleted();
}

/***********************************************************************************
 **	SetDetailed
 ** @param detailed
 ** @param force
 ***********************************************************************************/

void OpHistoryView::SetDetailed(BOOL detailed, BOOL force)
{

	if (!force && detailed == m_detailed)
		return;

	m_detailed = detailed;

	if(m_detailed)
	{
		m_history_view->GetBorderSkin()->SetImage("Detailed Panel Treeview Skin", "Treeview Skin");
	}
	else
	{
		m_history_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");
	}
	m_history_view->SetShowColumnHeaders(m_detailed);
	m_history_view->SetColumnVisibility(1, m_detailed);
	m_history_view->SetColumnVisibility(2, m_detailed);
	m_history_view->SetColumnVisibility(3, m_detailed);
	m_history_view->SetLinkColumn(IsSingleClick() ? 0 : -1);
	m_history_view->SetName(m_detailed ? "History View" : "History View Small");
}

/***********************************************************************************
 **	IsSingleClick
 ***********************************************************************************/

BOOL OpHistoryView::IsSingleClick()
{
	return !m_detailed && g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick);
}

/***********************************************************************************
 **	OnInputAction
 ** @param action
 ***********************************************************************************/

BOOL OpHistoryView::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
    {
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();
			BOOL allowed_folder_action = TRUE;

			switch(child_action->GetAction())
			{
				case OpInputAction::ACTION_OPEN_LINK:
				case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
				case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
				case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
				case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
					if( child_action->GetActionData() )
						return FALSE;

					// fall through

				case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
					if (!g_desktop_bookmark_manager->GetBookmarkModel()->Loaded())
					{
						child_action->SetEnabled(FALSE);
						return TRUE;
					}
					// fall through

				case OpInputAction::ACTION_COPY:
				case OpInputAction::ACTION_CUT:
					allowed_folder_action = FALSE;

				case OpInputAction::ACTION_DELETE:
				{
					//Actions not allowed on folders - must check that there are no folders selected
					UINT32 num_folders = 0;
					UINT32 num_items   = 0;
					BOOL is_selected = FALSE;
					OpINT32Vector list;

					if(m_history_view)
					{
						m_history_view->GetSelectedItems(list);
						is_selected = list.GetCount() > 0;

						UINT32 i;
						for (i = 0; i<list.GetCount(); i++ )
						{
							HistoryModelItem* item = (HistoryModelItem *)m_history_view->GetItemByPosition(m_history_view->GetItemByID(list.Get(i)));

							if( item )
							{
								if(item->GetType() == OpTypedObject::FOLDER_TYPE){ //Not allowed
									if(!allowed_folder_action || item->GetChildCount() == 0)
									{
										is_selected = FALSE; //Do not supply this menu option
										break;
									}
									else
									{
										num_folders++;
									}
								}
								else
								{
									num_items++;
								}
							}
						}

						if(allowed_folder_action && list.GetCount()) //Action does not allow a mix of folders and items
						{
							is_selected = (num_folders == list.GetCount()) || (num_items == list.GetCount());
						}
					}

					child_action->SetEnabled( is_selected );
					return TRUE;
				}
				case OpInputAction::ACTION_VIEW_STYLE:
				{
					child_action->SetSelected(child_action->GetActionData() == m_history_model->GetMode());
					return TRUE;
				}
			}
			break;
		}

		case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
		{
			DoAllSelected(action->GetAction());
			return TRUE;
		}

		case OpInputAction::ACTION_NEXT_ITEM:
		case OpInputAction::ACTION_PREVIOUS_ITEM:
			return m_history_view->OnInputAction(action);

		case OpInputAction::ACTION_CUT:
		case OpInputAction::ACTION_COPY:
		case OpInputAction::ACTION_DELETE:
			DoAllSelected(action->GetAction());
			return TRUE;

		case OpInputAction::ACTION_OPEN_LINK:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
			if( action->GetActionData() == 0 )
			{
				DoAllSelected(action->GetAction());
				return TRUE;
			}
			else
			{
				return FALSE;
			}

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
			ShowContextMenu(GetBounds().Center(),TRUE,TRUE);
			return TRUE;

		case OpInputAction::ACTION_VIEW_STYLE:
		{
			SetViewStyle((DesktopHistoryModel::History_Mode) action->GetActionData(),TRUE);
			return TRUE;
		}
    }

    return m_history_view->OnInputAction(action);
}

/***********************************************************************************
 **
 **
 **
 ***********************************************************************************/

void OpHistoryView::SetViewStyle(DesktopHistoryModel::History_Mode view_style, BOOL force)
{
 	if (m_history_model->GetMode() == view_style && !force)
 		return;

	m_history_model->ChangeMode(view_style);
}


/***********************************************************************************
 **	OnMouseEvent
 ** @param widget
 ** @param pos
 ** @param x
 ** @param y
 ** @param button
 ** @param down
 ** @param nclicks
 ***********************************************************************************/

void OpHistoryView::OnMouseEvent(OpWidget *widget,
								 INT32 pos,
								 INT32 x,
								 INT32 y,
								 MouseButton button,
								 BOOL down,
								 UINT8 nclicks)
{
	if (!down && nclicks == 1 && button == MOUSE_BUTTON_2)
    {
		if( widget == m_history_view )
		{
			ShowContextMenu(OpPoint(x+widget->GetRect().x,y+widget->GetRect().y),FALSE,FALSE);
		}
		return;
    }

    // UNIX behavior. I guess everyone wants it.
    if( down && nclicks == 1 && button == MOUSE_BUTTON_3 )
    {
		DoAllSelected(OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE);
		return;
    }

    BOOL shift = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_SHIFT;
    BOOL ctrl = GetWidgetContainer()->GetView()->GetShiftKeys() & SHIFTKEY_CTRL;
    if (ctrl && !shift)
    {
		// This combination is now allowed here
		return;
    }

    BOOL click_state_ok = (IsSingleClick() && !down && nclicks == 1 && !shift) || nclicks == 2;

    if (click_state_ok && button == MOUSE_BUTTON_1)
    {
		DoAllSelected(OpInputAction::ACTION_OPEN_LINK);
    }
}

/***********************************************************************************
 **	ShowContextMenu
 ** @param point
 ** @param center
 ** @param use_keyboard_context
 ***********************************************************************************/
BOOL OpHistoryView::ShowContextMenu(const OpPoint &point,
									BOOL center,
									BOOL use_keyboard_context)
{
	OpPoint p = point + GetScreenRect().TopLeft();
	g_application->GetMenuHandler()->ShowPopupMenu("History Item Popup Menu",
			PopupPlacement::AnchorAt(p, center), 0, use_keyboard_context);
	return TRUE;
}


/***********************************************************************************
 **	DoAllSelected
 ** @param action
 ***********************************************************************************/

void OpHistoryView::DoAllSelected(OpInputAction::Action action)
{

    OpINT32Vector list;
    m_history_view->GetSelectedItems(list);

    if( list.GetCount() == 0 )
    {
		return;
    }

	OpString address;
	OpString title;

    // We have to deal with the open-link actions separately because once a
    // page has been opened it will modify the history
    if( action == OpInputAction::ACTION_OPEN_LINK ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW )
    {
		OpAutoVector<OpString> slist;
		UINT32 i;

		for (i = 0; i<list.GetCount(); i++ )
		{
			HistoryModelItem* item = (HistoryModelItem *)m_history_view->GetItemByPosition(m_history_view->GetItemByID(list.Get(i)));

			if( item )
			{
				OpString* str = OP_NEW(OpString, ());
				if( str )
				{
					item->GetAddress(address);
					str->Set( address.CStr() );
					slist.Add( str );
				}
			}
		}

		BOOL3 new_window, new_page, in_background;
		if( action == OpInputAction::ACTION_OPEN_LINK )
		{
			new_window = g_application->IsOpeningInNewWindowPreferred() ? YES : NO;
			new_page = g_application->IsOpeningInNewPagePreferred() ? YES : NO;
			in_background = g_application->IsOpeningInBackgroundPreferred() ? YES : NO;
		}
		else
		{
			new_window    =
				action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW ||
				action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW ?
				YES : NO;

			new_page      =
				action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE ||
				action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE ?
				YES : NO;

			in_background =
				action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE ||
				action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW ?
				YES : NO;
		}

		for(i=0; i<slist.GetCount(); i++ )
		{
			const uni_char * search_url = slist.Get(i)->CStr();

			DesktopWindow* window = g_application->GetDesktopWindowCollection().GetDesktopWindowByUrl(search_url);

			if(window && new_window == NO && new_page == NO && in_background == NO)
			{
				window->Activate();
			}
			else
			{
				g_application->OpenURL( *slist.Get(i), new_window, new_page, in_background );
			}

			if( new_window == NO && new_page == NO )
			{
				new_page = YES;
			}
			in_background = YES;
		}

		return;
    }

    BOOL global_history_has_changed = FALSE;
	INT32 list_count = list.GetCount();
    for ( int i = 0; i < list_count; i++) 
    {
		HistoryModelItem* item = (HistoryModelItem *) m_history_view->GetItemByPosition(m_history_view->GetItemByID(list.Get(i)));

		if (!item)
			continue;

		switch (action)
		{
			case OpInputAction::ACTION_COPY:
			case OpInputAction::ACTION_CUT:
			{
				BookmarkItemData item_data;
				INT32 flag_changed = 0;

				item->GetAddress(address);
				item->GetTitle(title);

				item_data.name.Set( title.CStr() );
				item_data.url.Set( address.CStr() );

				flag_changed |= HotlistModel::FLAG_NAME;
				flag_changed |= HotlistModel::FLAG_URL;

				g_desktop_bookmark_manager->AddToClipboard(item_data, i>0);

				if( action == OpInputAction::ACTION_CUT )
				{
					m_history_model->Delete(item);
					global_history_has_changed = TRUE;
				}
			}
			break;

			case OpInputAction::ACTION_DELETE:
			{
				m_history_model->Delete(item);
				global_history_has_changed = TRUE;
			}
			break;

			case OpInputAction::ACTION_ADD_TO_BOOKMARKS:
			{
				BookmarkItemData item_data;
				item->GetAddress(address);
				item->GetTitle(title);

				item_data.name.Set( title.CStr() );
				item_data.url.Set( address.CStr() );

				if( item_data.name.IsEmpty() )
				{
					item_data.name.Set( item_data.url );
				}
				else
				{
					// Bug #177155 (remove quotes)
					//ReplaceEscapes( item_data.name.CStr(), item_data.name.Length(), TRUE );
				}

				item_data.name.Strip();
				g_desktop_bookmark_manager->NewBookmark(item_data, -1);
			}
			break;
		}
    }

    if( global_history_has_changed )
    {
		m_history_model->Save();
    }
}

/***********************************************************************************
 **	OnDragStart
 ** @param widget
 ** @param pos
 ** @param x
 ** @param y
 ***********************************************************************************/

void OpHistoryView::OnDragStart(OpWidget* widget,
								INT32 pos,
								INT32 x,
								INT32 y)
{

	if (widget == m_history_view)
	{
		HistoryModelItem* item = (HistoryModelItem *) m_history_view->GetSelectedItem();
		if (item && !item->IsFolder())
		{
			DesktopDragObject* drag_object = m_history_view->GetDragObject(DRAG_TYPE_HISTORY, x, y);
			if (drag_object)
			{
// Bug: Can only save one URL in drag object
				for (INT32 i = 0; i < drag_object->GetIDCount(); i++)
				{
					HistoryModelItem* history_item = m_history_model->FindItem(drag_object->GetID(i));

					if (history_item && !history_item->IsFolder())
					{
						OpString address;
						OpString title;

						history_item->GetAddress(address);
						history_item->GetTitle(title);

						drag_object->SetTitle( title.CStr() );
						drag_object->SetURL( address.CStr() );
						break;
					}
				}
				g_drag_manager->StartDrag(drag_object, NULL, FALSE);
			}
		}
	}
}

/***********************************************************************************
 **	SetSearchText
 ** @param search_text
 ***********************************************************************************/
void OpHistoryView::SetSearchText(const OpStringC& search_text)
{
	m_history_view->SetMatchText(search_text.CStr());
}
