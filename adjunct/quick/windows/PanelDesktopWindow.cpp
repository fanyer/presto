/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @file
 * @author Owner:    Karianne Ekern (karie)
 * @author Co-owner: Espen Sand (espen)
 *
 */

#include "core/pch.h"
#include "adjunct/quick/windows/PanelDesktopWindow.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/panels/BookmarksPanel.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/DesktopHistoryModel.h"
#include "adjunct/quick/panels/ContactsPanel.h"
#include "adjunct/quick/panels/HistoryPanel.h"
#include "adjunct/quick/panels/WindowsPanel.h"
#include "adjunct/quick/panels/WebPanel.h"
#include "adjunct/quick/panels/TransfersPanel.h"
#include "adjunct/quick/panels/LinksPanel.h"
#include "adjunct/m2_ui/panels/AccordionMailPanel.h"
#include "adjunct/m2_ui/panels/ChatPanel.h"
#include "adjunct/quick/panels/InfoPanel.h"
#include "adjunct/quick/panels/GadgetsPanel.h"
#include "adjunct/quick/panels/NotesPanel.h"
#include "adjunct/quick/panels/StartPanel.h"
#include "adjunct/quick/panels/SearchPanel.h"
#include "adjunct/quick_toolkit/widgets/OpWorkspace.h"
#include "adjunct/quick/managers/opsetupmanager.h"

#include "modules/widgets/WidgetContainer.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/prefsfile/prefsentry.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/util/OpLineParser.h"
#include "adjunct/desktop_util/sessions/opsession.h"



/***********************************************************************************
 **
 **	PanelDesktopWindow
 **
 ***********************************************************************************/

PanelDesktopWindow::PanelDesktopWindow(OpWorkspace* parent_workspace) :
	m_panel(NULL)
{
	OP_STATUS status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, parent_workspace);
	CHECK_STATUS(status);
}



/***********************************************************************************
 **
 **	UpdateTitle
 **
 ***********************************************************************************/

void PanelDesktopWindow::UpdateTitle()
{
	OpString title;

	if (m_panel)
	{
		m_panel->GetPanelText(title, Hotlist::PANEL_TEXT_TITLE);
		SetAttention(m_panel->GetPanelAttention());
	}
	else
	{
		SetIcon(NULL);
	}

	SetTitle(title.CStr());
}



/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void PanelDesktopWindow::OnLayout()
{
	if (!m_panel)
		return;

	UINT32 width, height;

	GetInnerSize(width, height);

	m_panel->SetRect(OpRect(0, 0, width, height));
}



/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL PanelDesktopWindow::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_FOCUS_ADDRESS_FIELD:
		case OpInputAction::ACTION_FOCUS_SEARCH_FIELD:
		{
			SetFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}
		case OpInputAction::ACTION_FOCUS_PAGE:
		{
			RestoreFocus(FOCUS_REASON_ACTION);
			return TRUE;
		}
	}

	return DesktopWindow::OnInputAction(action);
}



/***********************************************************************************
 **
 **	OnChange
 **
 ***********************************************************************************/

void PanelDesktopWindow::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_panel)
		UpdateTitle();
}



/***********************************************************************************
 **
 **	SetPanelByType
 **
 ***********************************************************************************/

void PanelDesktopWindow::SetPanelByType(Type panel_type)
{
	if (m_panel)
	{
		m_panel->Delete();
		m_panel = NULL;
		m_window_name.Empty();
	}

	m_panel = Hotlist::CreatePanelByType(panel_type);

	if (m_panel)
	{
		m_panel->SetListener(this);
		GetWidgetContainer()->GetRoot()->AddChild(m_panel);

		if (panel_type == PANEL_TYPE_GADGETS
			/* || panel_type == PANEL_TYPE_UNITE_SERVICES */)
		{
			m_panel->SetSplitView();
		}

		if (OpStatus::IsError(m_panel->Init()))
		{
			m_panel->Delete();
			m_panel = NULL;
			return;
		}

		m_panel->SetFullMode(TRUE);

		m_window_name.Set(GetPanelName());
		m_window_name.Append(" Panel Window");
		SetIcon(m_panel->GetPanelImage());
	}

	UpdateTitle();
}

/***********************************************************************************
 **
 **	GetWindowName
 **
 ***********************************************************************************/

const char*	PanelDesktopWindow::GetWindowName()
{
	return m_window_name.CStr();
}



/***********************************************************************************
 **
 **	OnActivate
 **
 ***********************************************************************************/

void PanelDesktopWindow::OnActivate(BOOL activate, BOOL first_time)
{
	if (activate && GetPanelType() == PANEL_TYPE_START && IsFocused(TRUE) && g_input_manager->GetKeyboardInputContext()->GetType() == WIDGET_TYPE_EDIT)
	{
		g_input_manager->InvokeAction(OpInputAction::ACTION_SELECT_ALL, 0, NULL, g_input_manager->GetKeyboardInputContext());
	}
}



/***********************************************************************************
 **
 **	OnSessionReadL
 **
 ***********************************************************************************/

void PanelDesktopWindow::OnSessionReadL(const OpSessionWindow* session_window)
{
	SetPanelByName(session_window->GetStringL("panel type").CStr());
}



/***********************************************************************************
 **
 **	OnSessionWriteL
 **
 ***********************************************************************************/

void PanelDesktopWindow::OnSessionWriteL(OpSession* session, OpSessionWindow* session_window, BOOL shutdown)
{
	session_window->SetTypeL(OpSessionWindow::PANEL_WINDOW);
	OpString panel_name;
	panel_name.Set(GetPanelName());
	session_window->SetStringL("panel type", panel_name);
}

/***********************************************************************************
 **
 **	AddToSession
 **
 ***********************************************************************************/

OP_STATUS PanelDesktopWindow::AddToSession(OpSession* session, INT32 parent_id, BOOL shutdown, BOOL extra_info)
{
	if (GetPanelType() == OpTypedObject::PANEL_TYPE_START)
		return OpStatus::ERR;

	return DesktopWindow::AddToSession(session, parent_id, shutdown, extra_info);
}

/***********************************************************************************
 **
 **	SetSearch
 **
 ***********************************************************************************/
void PanelDesktopWindow::SetSearch(const OpStringC& search)
{
	if (m_panel)
		m_panel->SetSearchText(search);
}
