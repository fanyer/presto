/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
*
* Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
*
* This file is part of the Opera web browser.	It may not be distributed
* under any circumstances.
*/

#include "core/pch.h"

#include "adjunct/quick/controller/BookmarkPropertiesController.h"

#include "adjunct/quick_toolkit/widgets/QuickEdit.h"
#include "adjunct/quick_toolkit/widgets/QuickDropDown.h"
#include "adjunct/quick/controller/SimpleDialogController.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/BookmarkModel.h"
#include "adjunct/quick/widgets/OpBookmarkView.h"
#include "adjunct/quick/quick-widget-names.h"
#include "adjunct/desktop_util/string/stringutils.h"


BookmarkPropertiesController::BookmarkPropertiesController(HotlistModelItem* item)
	: m_bookmark(item)
	, m_bookmark_view(NULL)
	, m_is_valid_nickname(true)
{
	HotlistModelItem* parent = m_bookmark->GetParentFolder();
	m_parent_id.Set(parent ? parent->GetID() : HotlistModel::BookmarkRoot);
}

void BookmarkPropertiesController::InitL()
{
	LEAVE_IF_ERROR(SetDialog("Bookmark Properties Dialog"));
	
	LEAVE_IF_ERROR(GetBinder()->Connect("Panel_Checkbox", m_in_panel));
	LEAVE_IF_ERROR(GetBinder()->Connect("Bookmarkbar_Checkbox", m_on_bookmarkbar));
	LEAVE_IF_ERROR(GetBinder()->Connect("BmTitle_Edit", m_name));
	LEAVE_IF_ERROR(GetBinder()->Connect("BmAddress_Edit", m_url));
	LEAVE_IF_ERROR(GetBinder()->Connect("BmNick_Edit", m_nick));
	LEAVE_IF_ERROR(GetBinder()->Connect("BmDescription_Edit", m_desc));
	LEAVE_IF_ERROR(GetBinder()->Connect("BmFolder_Dropdown", m_parent_id));

	OpEdit* BmAddress_edit = m_dialog->GetWidgetCollection()->GetL<QuickEdit>("BmAddress_Edit")->GetOpWidget();
	BmAddress_edit->SetForceTextLTR(TRUE);
	m_url.Set(m_bookmark->GetDisplayUrl());
	m_name.Set(m_bookmark->GetName());
	m_nick.Set(m_bookmark->GetShortName());
	m_desc.Set(m_bookmark->GetDescription());

	Image img = m_bookmark->GetIcon();
	OpEdit* edit = m_dialog->GetWidgetCollection()->GetL<QuickEdit>("BmTitle_Edit")->GetOpWidget();
	edit->GetForegroundSkin()->SetImage( m_bookmark->GetImage());
	edit->GetForegroundSkin()->SetBitmapImage(img, FALSE);
	edit->GetForegroundSkin()->SetRestrictImageSize(TRUE);

	m_in_panel.Set(m_bookmark->GetInPanel() == TRUE);
	m_on_bookmarkbar.Set(m_bookmark->GetOnPersonalbar() == TRUE);

	LEAVE_IF_ERROR(OpBookmarkView::Construct(&m_bookmark_view));
	LEAVE_IF_ERROR(InitDropDown());

	LEAVE_IF_ERROR(m_nick.Subscribe(MAKE_DELEGATE(*this, &BookmarkPropertiesController::OnTextChanged)));
}

OP_STATUS BookmarkPropertiesController::InitDropDown()
{
	QuickDropDown* dropdown = m_dialog->GetWidgetCollection()->GetL<QuickDropDown>("BmFolder_dropdown");
	return PopulateDropDown(dropdown->GetOpWidget());
}

bool BookmarkPropertiesController::IsNonTrashFolder(HotlistModelItem* folder)
{
	return folder && folder->IsFolder() && !folder->GetIsTrashFolder() && !folder->GetIsInsideTrashFolder();
}

OP_STATUS BookmarkPropertiesController::PopulateDropDown(OpDropDown* dropdown)
{
	OpTreeView* folder_view = m_bookmark_view->GetFolderView();
	INT32 got_index = 0;

	OpString bookmarks_translated;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_IDSTR_HL_BOOKMARKS_ROOT_FOLDER_NAME, bookmarks_translated));
	RETURN_IF_ERROR(dropdown->AddItem(bookmarks_translated.CStr(), -1, &got_index, HotlistModel::BookmarkRoot));
	dropdown->ih.SetImage(got_index, "Folder", dropdown);

	INT32 count = folder_view->GetItemCount();
	for (INT32 i = 0; i < count; i++)
	{
		HotlistModelItem* folder = static_cast<HotlistModelItem*>(folder_view->GetItemByPosition(i));
		if (!IsNonTrashFolder(folder))
			continue;

		RETURN_IF_ERROR(dropdown->AddItem(folder->GetName().CStr(), -1, &got_index, folder->GetID()));
		dropdown->ih.SetImage(got_index, "Folder", dropdown);

		if (folder->GetID() == m_parent_id.Get())
			dropdown->SelectItem(got_index, TRUE);

		INT32 indent = 0;
		while (folder)
		{
			indent++;
			folder = static_cast<HotlistModelItem*>(folder->GetParentItem());
		}

		dropdown->ih.SetIndent(got_index, indent, dropdown);
	}

	return OpStatus::OK;
}

BOOL BookmarkPropertiesController::ParentChanged() const
{
	return !m_bookmark->GetParentFolder() && m_parent_id.Get() != HotlistModel::BookmarkRoot 
		|| m_bookmark->GetParentFolder() && m_bookmark->GetParentFolder()->GetID() != m_parent_id.Get();

}

void BookmarkPropertiesController::OnTextChanged(const OpStringC& text)
{
	bool is_valid_nickname;
	if (text.HasContent() && (text.FindFirstOf(UNI_L(".:/?\\")) != -1 || g_hotlist_manager->HasNickname(text, m_bookmark)))
		is_valid_nickname = false;
	else
		is_valid_nickname = true;

	if (m_is_valid_nickname != is_valid_nickname)
	{
		m_is_valid_nickname = is_valid_nickname;
		g_input_manager->UpdateAllInputStates();
	}
}

BOOL BookmarkPropertiesController::DisablesAction(OpInputAction* action)
{
	if (action->GetAction() == OpInputAction::ACTION_OK)
		return !m_is_valid_nickname;

	return OkCancelDialogContext::DisablesAction(action);
}

void BookmarkPropertiesController::OnOk()
{
	if (m_bookmark->GetName().Compare(m_name.Get()))
		m_bookmark->SetName(m_name.Get().CStr());
	// We display the display url in the dialog, but if the user didn't change it
	// we still want the (possibly redir) url stored
	if (m_bookmark->GetDisplayUrl().Compare(m_url.Get()))
		m_bookmark->SetUrl(m_url.Get());
	if (m_bookmark->GetDescription().Compare(m_desc.Get()))
		m_bookmark->SetDescription(m_desc.Get());
	if (m_bookmark->GetShortName().Compare(m_nick.Get()))
		m_bookmark->SetShortName(m_nick.Get());
	m_bookmark->SetPanelPos(m_in_panel.Get() ? 0 : -1);
	m_bookmark->SetPersonalbarPos(m_on_bookmarkbar.Get() ? 0 : -1);

	if (ParentChanged())
	{
		HotlistModelItem* parent_item = m_bookmark->GetModel()->GetItemByID(m_parent_id.Get());
		CoreBookmarkPos pos(parent_item, DesktopDragObject::INSERT_INTO);
		g_bookmark_manager->MoveBookmark(BookmarkWrapper(m_bookmark)->GetCoreItem(), pos.previous, pos.parent);
	}
}
