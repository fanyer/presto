// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2003 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#include "LinksPanel.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick/models/LinksModel.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick/widgets/OpLinksView.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/url/url_man.h"
#include "modules/widgets/OpEdit.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
**
**	Init
**
***********************************************************************************/

OP_STATUS LinksPanel::Init()
{
	RETURN_IF_ERROR(OpLinksView::Construct(&m_links_view));
	AddChild(m_links_view, TRUE);
	m_links_view->GetTree()->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);
	SetToolbarName("Links Panel Toolbar", "Links Full Toolbar");
	SetName("Links");
	return OpStatus::OK;
}

/***********************************************************************************
**
**	GetPanelText
**
***********************************************************************************/

void LinksPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::M_VIEW_HOTLIST_MENU_LINKS, text);
}

/***********************************************************************************
**
**	OnFullModeChanged
**
***********************************************************************************/

void LinksPanel::OnFullModeChanged(BOOL full_mode)
{
	m_links_view->SetDetailed(full_mode);
}

/***********************************************************************************
**
**	OnLayout
**
***********************************************************************************/

void LinksPanel::OnLayoutPanel(OpRect& rect)
{
	m_links_view->SetRect(rect);
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void LinksPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_links_view->SetFocus(reason);
	}
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL LinksPanel::OnInputAction(OpInputAction* action)
{
	return m_links_view->OnInputAction(action);
}
