/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
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
#include "adjunct/quick/windows/HotlistDesktopWindow.h"

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
 **	HotlistDesktopWindow
 **
 ***********************************************************************************/

HotlistDesktopWindow::HotlistDesktopWindow(OpWorkspace* parent_workspace, Hotlist* hotlist) : DesktopWindow()
{
	OP_STATUS status = DesktopWindow::Init(OpWindow::STYLE_DESKTOP, parent_workspace);
	CHECK_STATUS(status);

	OpWidget* root_widget = GetWidgetContainer()->GetRoot();

	if (hotlist)
	{
		m_hotlist = hotlist;
	}
	else
	{
		if (!(m_hotlist = OP_NEW(Hotlist, ())))
		{
			init_status = OpStatus::ERR_NO_MEMORY;
			return;
		}
	}


	root_widget->AddChild(m_hotlist);
	status = m_hotlist->init_status;
	CHECK_STATUS(status);

	UpdateTitle();
}


/***********************************************************************************
 **
 **	UpdateTitle
 **
 ***********************************************************************************/

void HotlistDesktopWindow::UpdateTitle()
{
	OpString title;

	INT32 selected = m_hotlist->GetSelectedPanel();

	if (selected != -1)
	{
		m_hotlist->GetPanelText(selected, title, Hotlist::PANEL_TEXT_TITLE);
		SetIcon(m_hotlist->GetPanelImage(selected)->GetImage());
	}
	else
	{
		g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLIST_DLG_TITLE, title);
		SetIcon("Window Hotlist Icon");
	}

	SetTitle(title.CStr());
}



/***********************************************************************************
 **
 **	OnChange
 **
 ***********************************************************************************/

void HotlistDesktopWindow::OnChange(OpWidget *widget, BOOL changed_by_mouse)
{
	if (widget == m_hotlist)
		UpdateTitle();
}


/***********************************************************************************
 **
 **	OnRelayout
 **
 ***********************************************************************************/

void HotlistDesktopWindow::OnRelayout(OpWidget* widget)
{
	OpWorkspace* workspace;

	if (widget == m_hotlist && (workspace = GetParentWorkspace()) != NULL)
	{
		workspace->GetParentDesktopWindow()->Relayout();
	}
	else
	{
		DesktopWindow::OnRelayout(widget);
	}
}

/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void HotlistDesktopWindow::OnLayout()
{
	UINT32 width, height;

	GetInnerSize(width, height);

	m_hotlist->SetRect(OpRect(0, 0, width, height));
}
