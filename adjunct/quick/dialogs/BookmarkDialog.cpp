/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
 */
#include "core/pch.h"

#include "adjunct/quick/dialogs/BookmarkDialog.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/widgets/OpEdit.h"

#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick/managers/DesktopBookmarkManager.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/models/DesktopBookmark.h"
#include "adjunct/quick/panels/BookmarksPanel.h"
#include "adjunct/quick/widgets/OpBookmarkView.h"
#include "adjunct/quick/dialogs/NewBookmarkFolderDialog.h"
#include "adjunct/quick/controller/SimpleDialogController.h"

INT32 BookmarkDialog::s_last_saved_folder = HotlistModel::BookmarkRoot;

BookmarkDialog::BookmarkDialog()
{
	m_model = NULL;
	m_bookmark_view = NULL;
	m_parent_id = -1;
}

BookmarkDialog::~BookmarkDialog()
{
	if (m_bookmark_view)
	{
		m_bookmark_view->OnDeleted();
		OP_DELETE(m_bookmark_view);
	}
}

/*************************************************************************
 *
 * UpdateFolderDropDown
 *
 *
 *************************************************************************/

void BookmarkDialog::UpdateFolderDropDown()
{
	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Parent_dropdown");

	if (dropdown && m_bookmark_view)
	{
		OpTreeView* tree_view = m_bookmark_view->GetFolderView();
		INT32 got_index;

		OpString bookmarks_translated;
		g_languageManager->GetString(Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME, bookmarks_translated);

		dropdown->Clear();
		dropdown->AddItem(bookmarks_translated.CStr(), -1, &got_index, HotlistModel::BookmarkRoot);
		dropdown->ih.SetImage(got_index, "Folder", dropdown);

		INT32 count = tree_view->GetItemCount();

		for (INT32 i = 0; i < count; i++)
		{
			HotlistModelItem*  item = static_cast<HotlistModelItem*>(tree_view->GetItemByPosition(i));

			if (item && item->IsFolder() && !item->GetIsTrashFolder() && !item->GetIsInsideTrashFolder())
			{
				OpStringC name = item->GetName();
				{
					dropdown->AddItem(name.CStr(), -1, &got_index, item->GetID());
					dropdown->ih.SetImage(got_index, "Folder", dropdown);

					if (item->GetID() == m_parent_id)
					{
						dropdown->SelectItem(got_index, TRUE);

					}

					INT32 indent = 0;

					while (item)
					{
						indent++;
						item = static_cast<HotlistModelItem*>(item->GetParentItem());
					}

					dropdown->ih.SetIndent(got_index, indent, dropdown);
				}
			}
		}
	}
}


/*************************************************************************
 * 
 * OnInputAction
 *
 *
 *************************************************************************/

BOOL BookmarkDialog::OnInputAction(OpInputAction* action)
{
	if( action->GetAction() == OpInputAction::ACTION_OK )
	{
		if( !SaveSettings() )
		{
			return FALSE;
		}
	}

	if( action->GetAction() == OpInputAction::ACTION_NEW_FOLDER )
	{
		OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Parent_dropdown");

		HotlistModelItem* parent_item = m_model->GetItemByID(m_parent_id);
		if (parent_item && parent_item->IsFolder() && ((FolderModelItem*)parent_item)->GetMaximumItemReached())
		{
			g_hotlist_manager->ShowMaximumReachedDialog(parent_item->GetIsInMaxItemsFolder());
			return TRUE;
		}

		if (dropdown)
		{
			NewBookmarkFolderDialog* dialog = OP_NEW(NewBookmarkFolderDialog, ());
			dialog->SetDialogListener(this);
			dialog->Init(this, m_parent_id);
		}

		return TRUE;
	}

	return Dialog::OnInputAction(action);
}


OP_STATUS BookmarkDialog::Init(DesktopWindow* parent_window, HotlistModelItem*)
{
	m_model = g_hotlist_manager->GetBookmarksModel();
	OpBookmarkView::Construct(&m_bookmark_view);
	OP_STATUS err = m_model->AddListener(this);
	if (OpStatus::IsError(err))
	{
		Close(TRUE);
		return err;
	}
	return Dialog::Init(parent_window);
}

void BookmarkDialog::OnClose(BOOL user_initiated)
{
	m_model->RemoveListener(this);
	Dialog::OnClose(user_initiated); 
}

void BookmarkDialog::OnItemAdded(OpTreeModel* tree_model, INT32 index) 
{
	HotlistModelItem* item = static_cast<HotlistModelItem*>(tree_model->GetItemByPosition(index));
	if(item->IsFolder())
	{
		m_parent_id = item->GetID();
		UpdateFolderDropDown();
	}
}

BOOL BookmarkDialog::ValidateNickName(OpString& nick,HotlistModelItem* hmi)
{
	if( uni_strpbrk(nick.CStr(), UNI_L(".:/?\\")) )
	{
		nick.Empty();
		SimpleDialogController* controller = OP_NEW(SimpleDialogController,(SimpleDialogController::TYPE_OK, SimpleDialogController::IMAGE_INFO,
			WINDOW_NAME_NICKNAME_TAKEN,Str::SI_IDSTR_HL_ERRMSG_NICKNAME,Str::SI_IDSTR_HL_ERRMSG_NICKNAMETITLE));
		
		ShowDialog(controller, g_global_ui_context, this);	
		return FALSE;
	}

	if( g_hotlist_manager->HasNickname(nick, hmi))
	{
		OpString message, title;
		RETURN_VALUE_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_HL_ERRMSG_NICKNAMETITLE, title),FALSE);
		RETURN_VALUE_IF_ERROR(StringUtils::GetFormattedLanguageString(message, Str::D_BOOKMARK_NICKNAME_TAKEN,nick.CStr()),FALSE);

		SimpleDialogController* controller = OP_NEW(SimpleDialogController,(SimpleDialogController::TYPE_OK, SimpleDialogController::IMAGE_INFO,
			WINDOW_NAME_NICKNAME_TAKEN,message,title));

		ShowDialog(controller, g_global_ui_context, this);
		return FALSE;
	}

	return TRUE;
}

/*******************************************************************
 *
 *
 *
 *
 *******************************************************************/

EditBookmarkDialog::EditBookmarkDialog(BOOL attempted_add) : 
	m_add(attempted_add)
{}

EditBookmarkDialog::~EditBookmarkDialog() 
{ 
}

/*******************************************************************
 *
 * Init
 *
 *
 *******************************************************************/
OP_STATUS EditBookmarkDialog::Init(DesktopWindow* parent_window, 
								   HotlistModelItem* item)
{
	HotlistModelItem* parent = item->GetParentFolder();
	m_bookmark_id = item->GetID();
	m_is_folder	  = item->IsFolder();
	m_parent_id   = parent ? parent->GetID() : HotlistModel::BookmarkRoot;

	return BookmarkDialog::Init(parent_window,parent);
}

/*******************************************************************
 *
 * OnInit
 *
 *
 *******************************************************************/
void EditBookmarkDialog::OnInit()
{
	HotlistModelItem* bm = GetBookmark();

	if(bm)
	{
		BookmarkItemData item_data;
		g_desktop_bookmark_manager->GetItemValue( bm, item_data );

		SetWidgetText("Name_edit", item_data.name.IsEmpty() ? item_data.url.CStr() : item_data.name.CStr());
		SetWidgetText("Nick_edit", item_data.shortname.CStr());
		OpEdit* url_edit = GetWidgetByName<OpEdit>("URL_edit", WIDGET_TYPE_EDIT);
		if (url_edit)
		{
			url_edit->SetText(bm->GetDisplayUrl().CStr());
			url_edit->SetForceTextLTR(TRUE);
		}
		SetWidgetText("Description_edit", item_data.description.CStr());
		SetWidgetText("Created_text", item_data.created.CStr());
		SetWidgetText("Visited_text", item_data.visited.CStr());
		SetWidgetValue("ShowOnPersonalbar_check", item_data.personalbar_position >= 0 );
		SetWidgetValue("ViewInPanel_check", item_data.panel_position >= 0 );

		OpEdit* edit = GetWidgetByName<OpEdit>("Name_edit", WIDGET_TYPE_EDIT);
		if( edit )
		{
			edit->GetForegroundSkin()->SetImage( item_data.direct_image_pointer );
			Image img = bm->GetIcon();
			edit->GetForegroundSkin()->SetBitmapImage( img, FALSE );
			edit->GetForegroundSkin()->SetRestrictImageSize(TRUE);
		}

		if (item_data.url.HasContent() && item_data.panel_position == -1)
		{
			ShowWidget("Advanced_group", FALSE);
		}
	}
	
	ShowWidget("already_exist", m_add);

	UpdateFolderDropDown();
}


const char* EditBookmarkDialog::GetWindowName()
{
	if (GetBookmark() && GetBookmark()->IsBookmark()) 
		return "Bookmark Properties Dialog"; 
	else 
		return "Folder Properties Dialog";
}

HotlistModelItem* EditBookmarkDialog::GetBookmark()
{
	return g_hotlist_manager->GetBookmarksModel()->GetItemByID(m_bookmark_id); // Use id instead
}

/*******************************************************************
 *
 * OnChange
 *
 *
 *******************************************************************/

void EditBookmarkDialog::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	Dialog::OnChange(widget, changed_by_mouse);

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Parent_dropdown");

	if (dropdown == widget)
	{
		// 64-bit support would work much better if ids were INTPTRs
		m_parent_id = (INTPTR) dropdown->GetItemUserData(dropdown->GetSelectedItem());
		//TODO: UpdateIsDuplicate();?
	}

}



/*******************************************************************
 *
 * SaveSettings
 *
 *
 *******************************************************************/
BOOL EditBookmarkDialog::SaveSettings()
{
	HotlistModelItem* hmi = GetBookmark();

	if(hmi)
	{
		BookmarkItemData item_data;     // new values
		BookmarkItemData item_data_old; // check old value of each field

		g_desktop_bookmark_manager->GetItemValue( hmi, item_data_old );

		GetWidgetText("Name_edit", item_data.name, FALSE );
		GetWidgetText("Nick_edit", item_data.shortname, FALSE);
		GetWidgetText("URL_edit", item_data.url, FALSE);
		GetWidgetText("Description_edit", item_data.description, FALSE);

		BOOL on_personalbar = GetWidgetValue("ShowOnPersonalbar_check", FALSE);
		BOOL in_panel       = GetWidgetValue("ViewInPanel_check", FALSE);
		
		INT32 flag_changed = 0;
		if ( item_data_old.name.Compare(item_data.name) ) // If not equal
		{
			flag_changed |= HotlistModel::FLAG_NAME;
		}
		if ( item_data_old.shortname.Compare(item_data.shortname ))
		{
			flag_changed |= HotlistModel::FLAG_NICK;
		}
		if ( item_data.url.Compare(item_data_old.display_url.HasContent() ? item_data_old.display_url : item_data_old.url))
		{
			flag_changed |= HotlistModel::FLAG_URL;
		}
		if ( item_data_old.description.Compare(item_data.description))
		{
			flag_changed |= HotlistModel::FLAG_DESCRIPTION;
		}
		if ( (item_data_old.personalbar_position >= 0) != on_personalbar)
		{
			item_data.personalbar_position = on_personalbar ? 0 : -1;
			flag_changed |= HotlistModel::FLAG_PERSONALBAR;
		}
		if ( (item_data_old.panel_position >= 0) != in_panel )
		{
			item_data.panel_position = in_panel ? 0 : -1;
			flag_changed |= HotlistModel::FLAG_PANEL;
		}

		s_last_saved_folder = m_parent_id;
		
		if( item_data.shortname.HasContent() && (flag_changed & HotlistModel::FLAG_NICK))
		{
			if(!ValidateNickName(item_data.shortname,hmi))
				return FALSE;

			flag_changed |= HotlistModel::FLAG_NICK;
		}

		if ( m_add && hmi->GetUrl().CompareI(item_data.url.CStr()) != 0 )
		{
			g_desktop_bookmark_manager->NewBookmark(item_data,m_parent_id); // previous?
		}
		else
		{
			g_desktop_bookmark_manager->SetItemValue( hmi, item_data, TRUE, flag_changed );
			if (!hmi->GetParentFolder() && m_parent_id != HotlistModel::BookmarkRoot ||
				 hmi->GetParentFolder() && hmi->GetParentFolder()->GetID() != m_parent_id)
			{
				HotlistModelItem* parent_item = m_model->GetItemByID(m_parent_id);
				CoreBookmarkPos pos(parent_item, DesktopDragObject::INSERT_INTO);
				g_bookmark_manager->MoveBookmark(BookmarkWrapper(hmi)->GetCoreItem(), pos.previous, pos.parent);
			}
		}
	}

	return TRUE;

}

/*************************************************************************
 * 
 * OnOk
 *
 *
 *************************************************************************/
 
void EditBookmarkDialog::OnOk(Dialog* dialog, UINT32 result)
{
	if (result)
	{
		m_parent_id = result;
		UpdateFolderDropDown();
	}
}
  

/***********************************************************************************
 **
 **	AddBookmarkDialog
 **
 **
 ***********************************************************************************/




/*************************************************************************
 * 
 * Init
 *
 *
 *************************************************************************/

OP_STATUS AddBookmarkDialog::Init(DesktopWindow* parent_window, 
								  BookmarkItemData* item_data,
								  HotlistModelItem* parent)
{
	m_item_data = OP_NEW(BookmarkItemData,(*item_data));
	if(!m_item_data)
		return OpStatus::ERR_NO_MEMORY;

	m_parent_id   = parent ? parent->GetID() : s_last_saved_folder;
	// s_last_saved_folder might be deleted now, so check
	if ( !g_hotlist_manager->GetBookmarksModel()->GetItemByID(m_parent_id))
	{
		m_parent_id = HotlistModel::BookmarkRoot;
	}

	return BookmarkDialog::Init(parent_window,parent);
}


/*************************************************************************
 * 
 * Destructor
 *
 *
 *************************************************************************/

AddBookmarkDialog::~AddBookmarkDialog()
{
	OP_DELETE(m_item_data);
}


/*************************************************************************
 * 
 * OnInit
 *
 *
 *************************************************************************/

void AddBookmarkDialog::OnInit()
{
	// The item_data stuff will be set if doing ctrl-d on a web page for example
	
	SetWidgetText("Name_edit", m_item_data->name.IsEmpty() ? m_item_data->url.CStr() : m_item_data->name.CStr());
	SetWidgetText("Nick_edit", m_item_data->shortname.CStr());
	OpEdit* url_edit = GetWidgetByName<OpEdit>("URL_edit", WIDGET_TYPE_EDIT);
	if (url_edit)
	{
		url_edit->SetText(m_item_data->url.CStr());
		url_edit->SetForceTextLTR(TRUE);
	}
	SetWidgetText("Description_edit", m_item_data->description.CStr());
	SetWidgetText("Created_text", m_item_data->created.CStr());
	SetWidgetText("Visited_text", m_item_data->visited.CStr());
	SetWidgetValue("ShowOnPersonalbar_check", m_item_data->personalbar_position >= 0 );
	SetWidgetValue("ViewInPanel_check", m_item_data->panel_position >= 0 );

	if (m_item_data->url.HasContent() && m_item_data->panel_position == -1)
	{
		ShowWidget("Advanced_group", FALSE);
	}
	
	OpEdit* edit = GetWidgetByName<OpEdit>("Name_edit", WIDGET_TYPE_EDIT);
	if( edit )
	{
		//edit->GetForegroundSkin()->SetImage( m_item_data->direct_image_pointer );
		Image img = g_favicon_manager->Get(m_item_data->url.CStr());
		edit->GetForegroundSkin()->SetBitmapImage( img, FALSE );
		edit->GetForegroundSkin()->SetRestrictImageSize(TRUE);
	}


	UpdateFolderDropDown();
}

/*************************************************************************
 * 
 * OnChange
 *
 *
 *************************************************************************/

void AddBookmarkDialog::OnChange(OpWidget* widget, BOOL changed_by_mouse)
{
	Dialog::OnChange(widget, changed_by_mouse);

	OpDropDown* dropdown = (OpDropDown*) GetWidgetByName("Parent_dropdown");

	if (dropdown == widget)
	{
		// 64-bit support would work much better if ids were INTPTRs

		m_parent_id = (INTPTR) dropdown->GetItemUserData(dropdown->GetSelectedItem());

	}
}


/*************************************************************************
 * 
 * OnOk
 *
 *
 *************************************************************************/

void AddBookmarkDialog::OnOk(Dialog* dialog, UINT32 result)
{
	if (result)
	{
		m_parent_id = result;
		UpdateFolderDropDown();
	}
}

 /*************************************************************************
  * 
  * SaveSettings
  *
  *
  *************************************************************************/

BOOL AddBookmarkDialog::SaveSettings()
{
	GetWidgetText("Name_edit", m_item_data->name, FALSE );
	GetWidgetText("Nick_edit", m_item_data->shortname, FALSE);
	GetWidgetText("URL_edit", m_item_data->url, FALSE);
	GetWidgetText("Description_edit", m_item_data->description, FALSE);

	if (m_item_data->url.IsEmpty())
		return TRUE;

	m_item_data->personalbar_position = GetWidgetValue("ShowOnPersonalbar_check",FALSE) ? 0 : -1;
	m_item_data->panel_position       = GetWidgetValue("ViewInPanel_check",FALSE) ? 0 : -1;

	s_last_saved_folder = m_parent_id;

	// Max num items check 
	HotlistModelItem* parent_item = m_model->GetItemByID(m_parent_id);
	if (parent_item && parent_item->IsFolder() && ((FolderModelItem*)parent_item)->GetMaximumItemReached())
	{
		g_hotlist_manager->ShowMaximumReachedDialog(parent_item->GetIsInMaxItemsFolder());
		return TRUE;
	}

	if(!ValidateNickName(m_item_data->shortname,NULL))
		return FALSE;

	if (m_model)
	{
		// Check for duplicate
		if (m_item_data->url.HasContent())
		{
			BookmarkAttribute attribute;
			attribute.SetAttributeType(BOOKMARK_URL);
			if (OpStatus::IsSuccess(attribute.SetTextValue(m_item_data->url.CStr())))
			{
				BookmarkItem* item = g_bookmark_manager->GetFirstByAttribute(BOOKMARK_URL, &attribute);
				if (item && item->GetParentFolder() && item->GetParentFolder()->GetFolderType() == FOLDER_TRASH_FOLDER)
				{
					// Duplicate found in the trash, delete that
					CoreBookmarkWrapper* desktop_item = static_cast<CoreBookmarkWrapper*>(item->GetListener());
					g_hotlist_manager->GetBookmarksModel()->DeleteItem(desktop_item->CastToHotlistModelItem(), TRUE);
				}
			}
		}
		g_desktop_bookmark_manager->NewBookmark(*m_item_data,m_parent_id); // previous?
	}
	return TRUE;
}
