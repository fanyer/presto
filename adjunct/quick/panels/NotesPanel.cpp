/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"
#include "NotesPanel.h"

#include "adjunct/desktop_util/actions/action_utils.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
 **
 **	NotesPanel
 **
 ***********************************************************************************/

NotesPanel::NotesPanel()
{
}

OP_STATUS NotesPanel::Init()
{
	RETURN_IF_ERROR(OpSplitter::Construct(&m_splitter));
	AddChild(m_splitter);
	
	RETURN_IF_ERROR(OpHotlistView::Construct(&m_hotlist_view, WIDGET_TYPE_NOTES_VIEW));
	m_splitter->AddChild(m_hotlist_view);

	RETURN_IF_ERROR(OpMultilineEdit::Construct(&m_multiline_edit));
	m_splitter->AddChild(m_multiline_edit);

	m_splitter->SetDivision(g_pcui->GetIntegerPref(PrefsCollectionUI::HotlistNotesSplitter));

	m_multiline_edit->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	m_multiline_edit->Clear();

	OpString ghosttext;
	g_languageManager->GetString(Str::S_NEW_NOTE_HERE, ghosttext);

	m_multiline_edit->SetGhostText(ghosttext.CStr());
	m_multiline_edit->SetID(1);	// set this to something else than 0 so that it won't find itself

	m_multiline_edit->SetListener(this);
	m_multiline_edit->SetName("Notes_edit");
	m_hotlist_view->SetListener(this);

	SetToolbarName("Notes Panel Toolbar", "Notes Full Toolbar");
	SetName("Notes");

	OnChange(m_hotlist_view, FALSE);

	return OpStatus::OK;
}

void NotesPanel::OnDeleted()
{
	TRAPD(rc, g_pcui->WriteIntegerL(PrefsCollectionUI::HotlistNotesSplitter, m_splitter->GetDivision()));

	HotlistPanel::OnDeleted();
}

/***********************************************************************************
 **
 **	GetPanelText
 **
 ***********************************************************************************/

void NotesPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_NOTES, text);
}

/***********************************************************************************
 **
 **	NewNote
 **
 ***********************************************************************************/

/*static*/ void	NotesPanel::NewNote(const uni_char* text, const uni_char* url)
{
	if (text && *text)
	{
		// strip away leading spaces

		while (*text && uni_isspace(*text))
		{
			text++;
		}

		HotlistManager::ItemData item_data;

		item_data.name.Set(text);
		item_data.url.Set(url);

		if (g_hotlist_manager)
		{
			g_hotlist_manager->NewNote(item_data, HotlistModel::NoteRoot, NULL, NULL);
		}
	}
}


/***********************************************************************************
 **
 **	OnFullModeChanged
 **
 ***********************************************************************************/

void NotesPanel::OnFullModeChanged(BOOL full_mode)
{
	m_hotlist_view->SetDetailed(full_mode);
	m_splitter->SetHorizontal(full_mode != FALSE);
}

/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void NotesPanel::OnLayoutPanel(OpRect& rect)
{
	m_splitter->SetRect(rect);
}

/***********************************************************************************
 **
 **	OnChange
 **
 ***********************************************************************************/

void NotesPanel::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_hotlist_view)
	{
		HotlistModelItem* item = m_hotlist_view->GetSelectedItem();

		if (!item)
		{
			m_multiline_edit->Clear();
			m_multiline_edit->SetReadOnly(FALSE);
		}
		else
		{
			if( item->GetIsTrashFolder() || item->IsSeparator())
			{
				m_multiline_edit->SetGhostText(0);
				m_multiline_edit->Clear();
				m_multiline_edit->SetReadOnly(TRUE);
			}
			else
			{
				OpString text;
				m_multiline_edit->GetText(text);

				if (text.Compare(item->GetName()))
				{
					m_multiline_edit->SetText(item->GetName().CStr());
				}

				if( m_multiline_edit->IsReadOnly() )
				{
					m_multiline_edit->SetReadOnly( FALSE );
				}

				if( !m_multiline_edit->m_ghost_text)
				{
					OpString ghosttext;
					g_languageManager->GetString(Str::S_NEW_NOTE_HERE, ghosttext);
					m_multiline_edit->SetGhostText(ghosttext.CStr());
				}
			}
		}
	}
	else if (widget == m_multiline_edit)
	{
		HotlistModelItem* item = m_hotlist_view->GetSelectedItem();
		if (item)
		{
			OpString text;
			m_multiline_edit->GetText(text);
			g_hotlist_manager->Rename(item, text);
		}
		else
		{
			OpString text;
			m_multiline_edit->GetText(text);

			// create a new note if no note is selected and the multiedit contains texts
			if(text.HasContent())
			{
				HotlistManager::ItemData item_data;		

				m_multiline_edit->GetText(item_data.name);		

				g_hotlist_manager->NewNote(item_data, m_hotlist_view->GetCurrentFolderID(), GetParentDesktopWindow(), m_hotlist_view->GetItemView());		
			}
		}
	}
}

/***********************************************************************************
**
**	OnContextMenu
**
***********************************************************************************/

/*virtual*/ BOOL
NotesPanel::OnContextMenu(OpWidget* widget, INT32 child_index, const OpPoint& menu_point, const OpRect* avoid_rect, BOOL keyboard_invoked)
{
	return HandleWidgetContextMenu(widget, menu_point);
}

/***********************************************************************************
 **
 **	OnFocus
 **
 ***********************************************************************************/

void NotesPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_hotlist_view->SetFocus(reason);
	}
}

/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL NotesPanel::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_EDIT_PROPERTIES:
		{
			m_multiline_edit->SetFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}
	}

	return m_hotlist_view->OnInputAction(action);
}


INT32 NotesPanel::GetSelectedFolderID()
{

	return m_hotlist_view->GetSelectedFolderID();
}
