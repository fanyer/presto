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
#include "adjunct/quick/panels/HotlistPanel.h"

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
 **	HotlistPanel
 **
 ***********************************************************************************/

HotlistPanel::HotlistPanel() :
	m_normal_toolbar(NULL),
	m_full_toolbar(NULL),
	m_full_mode(FALSE)
{
	m_delayed_trigger.SetDelayedTriggerListener(this);
	m_delayed_trigger.SetTriggerDelay(0, 300);
}


/***********************************************************************************
 **
 **	SetToolbarName
 **
 ***********************************************************************************/

void HotlistPanel::SetToolbarName(const char* normal_name, const char* full_name)
{
	if (normal_name)
	{
		if (!m_normal_toolbar)
		{
			RETURN_VOID_IF_ERROR(OpToolbar::Construct(&m_normal_toolbar));
			AddChild(m_normal_toolbar, FALSE, TRUE);
		}

		if (m_normal_toolbar->GetName().Compare(normal_name) != 0)
		{
			m_normal_toolbar->SetName(normal_name);
			m_normal_toolbar->GetBorderSkin()->SetImage("Panel Normal Toolbar Skin");
		}
	}

	if (full_name)
	{
		if (!m_full_toolbar)
		{
			RETURN_VOID_IF_ERROR(OpToolbar::Construct(&m_full_toolbar));
			AddChild(m_full_toolbar, FALSE, TRUE);
		}

		if (m_full_toolbar->GetName().Compare(full_name) != 0)
		{
			m_full_toolbar->SetName(full_name);
			m_full_toolbar->GetBorderSkin()->SetImage("Panel Full Toolbar Skin");
		}
	}
}

/***********************************************************************************
 **
 **	SetFullMode
 **
 ***********************************************************************************/

void HotlistPanel::SetFullMode(BOOL full_mode, BOOL force)
{
	if (!force && full_mode == m_full_mode)
		return;

	m_full_mode = full_mode;

	OnFullModeChanged(m_full_mode);
}



/***********************************************************************************
 **
 **	GetHotlist
 **
 ***********************************************************************************/

Hotlist* HotlistPanel::GetHotlist()
{
	OpWidget* parent = GetParent();

	if (parent && parent->GetType() == WIDGET_TYPE_HOTLIST)
		return (Hotlist*) parent;

	return NULL;
}


/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void HotlistPanel::OnLayout()
{
	OpRect rect = GetBounds();

	if (IsFullMode() && m_full_toolbar)
	{
		if (m_normal_toolbar)
			m_normal_toolbar->SetVisibility(FALSE);

		if (m_full_toolbar)
			OnLayoutToolbar(m_full_toolbar, rect);
	}
	else
	{
		if (m_normal_toolbar)
			OnLayoutToolbar(m_normal_toolbar, rect);

		if (m_full_toolbar)
			m_full_toolbar->SetVisibility(FALSE);
	}

	OnLayoutPanel(rect);
}


/***********************************************************************************
 **
 **	OnLayoutToolbar
 **
 ***********************************************************************************/

void HotlistPanel::OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect)
{
	rect = toolbar->LayoutToAvailableRect(rect);
}
