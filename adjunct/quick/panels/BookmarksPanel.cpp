/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "BookmarksPanel.h"

#include "adjunct/quick/widgets/CollectionViewPane.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"

#include "modules/prefs/prefsmanager/collections/pc_ui.h"


/***********************************************************************************
 **
 **	BookmarksPanel
 **
 ***********************************************************************************/

BookmarksPanel::BookmarksPanel()
	: m_bookmark_view(NULL)
{
}

OP_STATUS BookmarksPanel::Init()
{
	RETURN_IF_ERROR(OpBookmarkView::Construct(&m_bookmark_view, 
											 PrefsCollectionUI::HotlistBookmarksSplitter, 
											 PrefsCollectionUI::HotlistBookmarksStyle, 
											 PrefsCollectionUI::HotlistBookmarksManagerSplitter, 
											 PrefsCollectionUI::HotlistBookmarksManagerStyle));
	AddChild(m_bookmark_view);
	SetToolbarName("Bookmarks Panel Toolbar", "Bookmarks Full Toolbar");
	SetName("Bookmarks");

	return OpStatus::OK;
}

BookmarksPanel::~BookmarksPanel()
{
}

void BookmarksPanel::OnAdded()
{
}

void BookmarksPanel::OnRemoving()
{
}

/***********************************************************************************
 **
 **	GetPanelText
 **
 ***********************************************************************************/

void BookmarksPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_BOOKMARKS, text);
}

/***********************************************************************************
 **
 **	OnFullModeChanged
 **
 ***********************************************************************************/

void BookmarksPanel::OnFullModeChanged(BOOL full_mode)
{
	m_bookmark_view->SetDetailed(full_mode);
}

void BookmarksPanel::OnShow(BOOL show)
{
	if(show && !IsFullMode())
	{
	}
}

/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void BookmarksPanel::OnLayoutPanel(OpRect& rect)
{
	m_bookmark_view->SetRect(rect);
}

void BookmarksPanel::OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect)
{
	rect = toolbar->LayoutToAvailableRect(rect);
}

/***********************************************************************************
 **
 **	OnFocus
 **
 ***********************************************************************************/

void BookmarksPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_bookmark_view->SetFocus(reason);
	}
}

/***********************************************************************************
 **	SetSearchText
 ** @param search_text
 ***********************************************************************************/
void BookmarksPanel::SetSearchText(const OpStringC& search_text)
{
	m_bookmark_view->GetItemView()->SetMatchText(search_text.CStr());
}

/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL BookmarksPanel::OnInputAction(OpInputAction* action)
{
	return m_bookmark_view->OnInputAction(action);
}

INT32 BookmarksPanel::GetSelectedFolderID()
{
	return m_bookmark_view->GetSelectedFolderID();
}


BOOL BookmarksPanel::OpenUrls( const OpINT32Vector& index_list, 
							   BOOL3 new_window, 
							   BOOL3 new_page, 
							   BOOL3 in_background, 
							   DesktopWindow* target_window,
							   INT32 position, 
							   BOOL ignore_modifier_keys)
{
	return m_bookmark_view->OpenUrls(index_list, new_window, new_page, in_background, target_window, position, ignore_modifier_keys);
}



