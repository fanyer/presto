/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 * 
 *  Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 * 
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/quick/widgets/OpLinksView.h"

#include "adjunct/desktop_util/file_chooser/file_chooser_fun.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/menus/DesktopMenuHandler.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/managers/PrivacyManager.h"
#include "adjunct/quick/models/LinksModel.h"
#include "adjunct/quick_toolkit/widgets/OpTreeView/OpTreeView.h"

#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/logdoc/logdoc_util.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dragdrop/dragdrop_manager.h"

#include "adjunct/quick/managers/DesktopBookmarkManager.h"


/***********************************************************************************
**
**	OpLinksView
**
***********************************************************************************/

DEFINE_CONSTRUCT(OpLinksView)

OpLinksView::OpLinksView()
	:DesktopWindowSpy(TRUE)
	,m_chooser(0)
	,m_links_model(0)
	,m_links_view(0)
	,m_locked(FALSE)
	,m_needs_update(FALSE)
	,m_detailed(FALSE)
	,m_private_mode(FALSE)
{
	SetSpyInputContext(this, FALSE);

	OP_STATUS status = OpTreeView::Construct(&m_links_view);
	CHECK_STATUS(status);
	AddChild(m_links_view, TRUE);

	m_links_view->SetListener(this);
	m_links_view->SetMultiselectable(TRUE);

	if(g_pcui->GetIntegerPref(PrefsCollectionUI::EnableDrag)&DRAG_BOOKMARK)
	{
		m_links_view->SetDragEnabled(TRUE);
	}

	SetDetailed(FALSE, TRUE);
}

void OpLinksView::OnDeleted()
{
	OP_DELETE(m_chooser);
	SetSpyInputContext(NULL, FALSE);
	m_links_view->SetTreeModel(NULL);
	OpWidget::OnDeleted();
	OP_DELETE(m_links_model);
}

/***********************************************************************************
**
**	SetDetailed
**
***********************************************************************************/

void OpLinksView::SetDetailed(BOOL detailed, BOOL force)
{
	if (!force && detailed == m_detailed)
		return;

	m_detailed = detailed;

	if(m_detailed)
	{
		m_links_view->GetBorderSkin()->SetImage("Detailed Panel Treeview Skin", "Treeview Skin");
	}
	else
	{
		m_links_view->GetBorderSkin()->SetImage("Panel Treeview Skin", "Treeview Skin");
	}

	m_links_view->SetShowColumnHeaders(m_detailed);
	m_links_view->SetColumnVisibility(1, m_detailed);
	m_links_view->SetLinkColumn(IsSingleClick() ? 0 : -1);
	m_links_view->SetName(m_detailed ? "Links View" : "Links View Small");
}

/***********************************************************************************
**
**	OnShow
**
***********************************************************************************/

void OpLinksView::OnShow(BOOL show)
{
	if (show && m_needs_update)
	{
		UpdateLinks(TRUE);
	}
}

/***********************************************************************************
**
**	IsSingleClick
**
***********************************************************************************/

BOOL OpLinksView::IsSingleClick()
{
	return !m_detailed && g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistSingleClick);
}


/***********************************************************************************
**
**	SetPanelLocked
**
***********************************************************************************/

BOOL OpLinksView::SetPanelLocked(BOOL locked)
{
	if (m_locked == locked)
		return FALSE;

	m_locked = locked;

	if (!m_locked)
		UpdateLinks(TRUE);

	return TRUE;
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL OpLinksView::OnInputAction(OpInputAction* action)
{
	LinksModelItem* item = (LinksModelItem *) m_links_view->GetSelectedItem();

	switch (action->GetAction())
	{
		case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
				case OpInputAction::ACTION_LOCK_PANEL:
				case OpInputAction::ACTION_UNLOCK_PANEL:
				{
					child_action->SetSelectedByToggleAction(OpInputAction::ACTION_LOCK_PANEL, m_locked);
					return TRUE;
				}
 			    case OpInputAction::ACTION_ADD_LINK_TO_BOOKMARKS:
				{
					if (item && g_desktop_bookmark_manager->GetBookmarkModel()->Loaded())
						child_action->SetEnabled(TRUE);
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}
			    case OpInputAction::ACTION_COPY:
			    case OpInputAction::ACTION_OPEN_LINK:
			    case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
			    case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
			    case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
			    case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
			    case OpInputAction::ACTION_DOWNLOAD_URL_AS:
			    case OpInputAction::ACTION_DOWNLOAD_URL:
				{
					if (item)
						child_action->SetEnabled(TRUE);
					else
						child_action->SetEnabled(FALSE);
					return TRUE;
				}

			}
			break;
		}

		case OpInputAction::ACTION_LOCK_PANEL:
			return SetPanelLocked(TRUE);

		case OpInputAction::ACTION_UNLOCK_PANEL:
			return SetPanelLocked(FALSE);

		case OpInputAction::ACTION_NEXT_ITEM:
		case OpInputAction::ACTION_PREVIOUS_ITEM:
			return m_links_view->OnInputAction(action);

		case OpInputAction::ACTION_DOWNLOAD_URL_AS:
		case OpInputAction::ACTION_DOWNLOAD_URL:
			if (action->GetActionDataString())
				return FALSE;
			// fall through

		case OpInputAction::ACTION_ADD_LINK_TO_BOOKMARKS:
		case OpInputAction::ACTION_COPY:
		case OpInputAction::ACTION_OPEN_LINK:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE:
		case OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW:
			DoAllSelected(action->GetAction());
			return TRUE;

		case OpInputAction::ACTION_SHOW_CONTEXT_MENU:
			ShowContextMenu(GetBounds().Center(), TRUE, TRUE);
			return TRUE;
	}

	return m_links_view->OnInputAction(action);
}

/***********************************************************************************
**
**	OnMouseEvent
**
***********************************************************************************/

void OpLinksView::OnMouseEvent(OpWidget *widget, INT32 pos, INT32 x, INT32 y, MouseButton button, BOOL down, UINT8 nclicks)
{
	if (widget != m_links_view)
		return;

	LinksModelItem* item = (LinksModelItem *) m_links_view->GetItemByPosition(pos);

	if (!down && nclicks == 1 && button == MOUSE_BUTTON_2)
	{
		ShowContextMenu( OpPoint(x,y), FALSE, FALSE);
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

	if (click_state_ok && button == MOUSE_BUTTON_1 && item && !item->m_frames_doc)
	{
		DoAllSelected(OpInputAction::ACTION_OPEN_LINK);
	}
}


BOOL OpLinksView::ShowContextMenu(const OpPoint &point, BOOL center, BOOL use_keyboard_context)
{
	OpPoint p = point + GetScreenRect().TopLeft();
	g_application->GetMenuHandler()->ShowPopupMenu("Links Panel Item Menu", PopupPlacement::AnchorAt(p, center), 0, use_keyboard_context);
	return TRUE;
}


/***********************************************************************************
**
**	DoAllSelected
**
***********************************************************************************/

void OpLinksView::DoAllSelected(OpInputAction::Action action)
{
	OpINT32Vector index_list;
	m_links_view->GetSelectedItems(index_list,FALSE);
	if (index_list.GetCount() == 0)
		return;

	BOOL all_items_have_info = TRUE;

	INT32 i;
	OpAutoVector<LinkData> list;

	for (i = 0; i < (INT32) index_list.GetCount(); i++ )
	{
		LinksModelItem* item = (LinksModelItem *) m_links_view->GetItemByPosition(index_list.Get(i));
		if (!item)
			return; // Fatal. 'index_list' and 'list' must be in sync below

		LinkData* ld = OP_NEW(LinkData,());
		if (!ld || 
			OpStatus::IsError(ld->address.Set(item->m_url.CStr())) || 
			OpStatus::IsError(ld->title.Set(item->m_title.CStr())) ||
			OpStatus::IsError(list.Add(ld)))
		{
			// Fatal.'index_list' and 'list' must be in sync below
			OP_DELETE(ld);
			return;
		}
		else if (!item->m_url_info)
			all_items_have_info = FALSE;
	}

	if (action == OpInputAction::ACTION_OPEN_LINK ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE ||
		action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW )
	{
		BOOL3 new_window, new_page, in_background;
		if( action == OpInputAction::ACTION_OPEN_LINK )
		{
			new_window = g_application->IsOpeningInNewWindowPreferred() ? YES : NO;
			new_page = g_application->IsOpeningInNewPagePreferred() ? YES : NO;
			in_background = g_application->IsOpeningInBackgroundPreferred() ? YES : NO;
		}
		else
		{
			new_window    = action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_WINDOW || action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW ? YES : NO;
			new_page      = action == OpInputAction::ACTION_OPEN_LINK_IN_NEW_PAGE || action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE ? YES : NO;
			in_background = action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_PAGE || action == OpInputAction::ACTION_OPEN_LINK_IN_BACKGROUND_WINDOW ? YES : NO;
		}

		for (i=0; i<(INT32) list.GetCount(); i++)
		{
			g_application->OpenURL( list.Get(i)->address, new_window, new_page, in_background, 0, FALSE, FALSE, m_private_mode );
			if (new_window == NO && new_page == NO)
				new_page = YES;
			in_background = YES;
		}
		return;
	}
	else if (action == OpInputAction::ACTION_DOWNLOAD_URL_AS && list.GetCount() > 1 && all_items_have_info)
	{
		// Save all files to a specified directory (DSK-311439). We can only do this if each item 
		// has a 'm_url_info'.

		if (!m_chooser)
			RETURN_VOID_IF_ERROR(DesktopFileChooser::Create(&m_chooser));

		ResetDesktopFileChooserRequest(m_request);
		m_request.action = DesktopFileChooserRequest::ACTION_DIRECTORY;
		OpString tmp_storage;
		m_request.initial_path.Set(g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_SAVE_FOLDER, tmp_storage));
		g_languageManager->GetString(Str::SI_IDSTR_DOWNLOAD_DLG_WHERE_DIR, m_request.caption);
		OpStatus::Ignore(m_chooser->Execute(GetParentOpWindow(), this, m_request));
		return;
	}

	for (i = 0; i < (INT32) list.GetCount(); i++)
	{
		switch(action)
		{
			case OpInputAction::ACTION_DOWNLOAD_URL_AS:
				{
					LinksModelItem* item = (LinksModelItem *) m_links_view->GetItemByPosition(index_list.Get(i));
					if (item->m_url_info)
						item->m_url_info->DownloadTo(item, NULL);
					else
						g_input_manager->InvokeAction(action, 0, list.Get(i)->address.CStr());
				}
				break;
			case OpInputAction::ACTION_DOWNLOAD_URL:
				{
					LinksModelItem* item = (LinksModelItem *) m_links_view->GetItemByPosition(index_list.Get(i));
					if (item->m_url_info)
						item->m_url_info->DownloadDefault(item, NULL);
					else
						g_input_manager->InvokeAction(action, 0, list.Get(i)->address.CStr());
				}
				break;
			case OpInputAction::ACTION_ADD_LINK_TO_BOOKMARKS:
			case OpInputAction::ACTION_COPY:
			{
				BookmarkItemData item_data;
			    item_data.name.Set( list.Get(i)->title );
				item_data.url.Set( list.Get(i)->address );
				INT32 flag_changed = 0;
				flag_changed |= HotlistModel::FLAG_NAME;
				flag_changed |= HotlistModel::FLAG_URL;

				if (action == OpInputAction::ACTION_ADD_LINK_TO_BOOKMARKS)
				{
					if( item_data.name.IsEmpty() )
					{
						item_data.name.Set( item_data.url.CStr() );
					}
					else
					{
						// Bug #177155 (remove quotes)
						ReplaceEscapes( item_data.name.CStr(), item_data.name.Length(), TRUE );
					}
					g_desktop_bookmark_manager->NewBookmark(item_data, -1);

				}
				else
					g_desktop_bookmark_manager->AddToClipboard(item_data, i>0);

				break;
			}
		}
	}
}


/***********************************************************************************
**
**	OnFileChoosingDone
**
***********************************************************************************/
void OpLinksView::OnFileChoosingDone(DesktopFileChooser* chooser, const DesktopFileChooserResult& result)
{
	if (chooser == m_chooser)
	{
		if (result.files.GetCount() > 0)
		{
			OpINT32Vector index_list;
			m_links_view->GetSelectedItems(index_list,FALSE);
			for (INT32 i = 0; i < (INT32) index_list.GetCount(); i++ )
			{
				LinksModelItem* item = (LinksModelItem *) m_links_view->GetItemByPosition(index_list.Get(i));
				if (item && item->m_url_info)
					item->m_url_info->DownloadDefault(item, result.files.Get(0)->CStr());
			}
			FileChooserUtil::SavePreferencePath(DesktopFileChooserRequest::ACTION_FILE_SAVE, result);
		}
	}
}



/***********************************************************************************
**
**	OnDragStart
**
***********************************************************************************/

void OpLinksView::OnDragStart(OpWidget* widget, INT32 pos, INT32 x, INT32 y)
{
	if (widget == m_links_view)
	{
		OpINT32Vector index_list;
		m_links_view->GetSelectedItems(index_list,FALSE);

		DesktopDragObject* drag_object = index_list.GetCount() > 0 ? m_links_view->GetDragObject(DRAG_TYPE_LINK, x, y) : NULL;
		if (drag_object)
		{
			bool first_item = true;
			for (INT32 i = 0; i < (INT32) index_list.GetCount(); i++ )
			{
				LinksModelItem* item = static_cast<LinksModelItem*>(m_links_view->GetItemByPosition(index_list.Get(i)));
				OP_ASSERT(item);
				if (!item)
					continue;

				if (first_item)
				{
					first_item = false; // Drag object only supports one title
					drag_object->SetTitle(item->m_title.CStr());
				}
				drag_object->SetURL(item->m_url.CStr());

			}
			if (DragDrop_Data_Utils::HasURL(drag_object))
				g_drag_manager->StartDrag(drag_object, NULL, FALSE);
			else
				OP_DELETE(drag_object);
		}
	}
}

/***********************************************************************************
**
**	OnSettingsChanged
**
***********************************************************************************/

void OpLinksView::OnSettingsChanged(DesktopSettings* settings)
{
	if (settings->IsChanged(SETTINGS_DELETE_PRIVATE_DATA))
	{
		if (PrivacyManager::GetInstance()->GetDeleteState(PrivacyManager::ALL_WINDOWS))
		{
			m_links_view->SetTreeModel(NULL);
			OP_DELETE(m_links_model);
			m_links_model = NULL;
		}
	}
}

/***********************************************************************************
**
**	UpdateLinks
**
***********************************************************************************/

void OpLinksView::UpdateLinks(BOOL restart)
{
	if (m_locked ||
	    (GetTargetDesktopWindow() && (GetTargetDesktopWindow()->GetType() == WINDOW_TYPE_PANEL ||
	                                  GetTargetDesktopWindow()->GetType() == WINDOW_TYPE_HOTLIST)))
		return;

	BOOL has_document =  (GetTargetBrowserView() && (GetTargetBrowserView()->GetWindowCommander()->IsLoading() || (GetTargetBrowserView()->GetWindowCommander()->GetCurrentURL(FALSE) && *GetTargetBrowserView()->GetWindowCommander()->GetCurrentURL(FALSE))));

	if (!has_document || !IsAllVisible())
	{
		if (m_links_model)
		{
			m_links_view->SetTreeModel(NULL);
			OP_DELETE(m_links_model);
			m_links_model = NULL;
		}

		if (has_document)
		{
			m_needs_update = TRUE;
		}

		StopTimer();
		return;
	}

	m_private_mode = GetTargetBrowserView() && GetTargetBrowserView()->GetWindowCommander()->GetPrivacyMode();

	if (restart && GetTargetBrowserView()->GetWindowCommander()->IsLoading())
	{
		StartTimer(500);
		m_needs_update = TRUE;
		return;
	}

	if (!GetTargetBrowserView()->GetWindowCommander()->IsLoading())
	{
		StopTimer();
	}

	if (m_needs_update || restart)
	{
		m_links_view->SetTreeModel(NULL);
		OP_DELETE(m_links_model);
		m_links_model = NULL;
		m_needs_update = FALSE;
	}

	if (!m_links_model)
	{
		m_links_model = OP_NEW(LinksModel, ());
		if (m_links_model)
		{
			m_links_model->UpdateLinks(GetTargetBrowserView()->GetWindowCommander());
			m_links_view->SetTreeModel(m_links_model, 0);
			m_links_view->SetColumnVisibility(1, m_detailed);
		}
	}
	else
	{
		m_links_model->UpdateLinks(GetTargetBrowserView()->GetWindowCommander(), !GetTargetBrowserView()->GetWindowCommander()->IsLoading());
	}

	m_links_view->SetSelectedItem(0);
}
