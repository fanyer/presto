/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"       
#include "HistoryPanel.h"    
#include "adjunct/quick/widgets/OpHistoryView.h" 
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
 **	HistoryPanel
 ***********************************************************************************/

HistoryPanel::HistoryPanel(){}

/***********************************************************************************
 **	Init
 ***********************************************************************************/
OP_STATUS HistoryPanel::Init()
{
	RETURN_IF_ERROR(OpHistoryView::Construct(&m_history_view));
	m_history_view->SetSystemFont(OP_SYSTEM_FONT_UI_PANEL);

	AddChild(m_history_view, TRUE);

	SetToolbarName("History Panel Toolbar", "History Full Toolbar");
	SetName("History");

	return OpStatus::OK;
}

/***********************************************************************************
 **	GetPanelText
 ** @param text
 ** @param text_type
 ***********************************************************************************/

void HistoryPanel::GetPanelText(OpString& text, 
								Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::DI_IDSTR_M2_HOTLISTTAB_HISTORY, text);
}

/***********************************************************************************
 **	OnFullModeChanged
 ** @param full_mode
 ***********************************************************************************/

void HistoryPanel::OnFullModeChanged(BOOL full_mode)
{
	m_history_view->SetDetailed(full_mode);
}

/***********************************************************************************
 **	OnLayout
 ** @param rect
 ***********************************************************************************/

void HistoryPanel::OnLayoutPanel(OpRect& rect)
{
	m_history_view->SetRect(rect);
}

/***********************************************************************************
 **	OnFocus
 ** @param focus
 ** @param reason
 ***********************************************************************************/

void HistoryPanel::OnFocus(BOOL focus,
						   FOCUS_REASON reason)
{
	if (focus)
	{
		m_history_view->SetFocus(reason);
	}
}

/***********************************************************************************
 **	OnInputAction
 ** @param action
 ** @return
 ***********************************************************************************/

BOOL HistoryPanel::OnInputAction(OpInputAction* action)
{
	return m_history_view->OnInputAction(action);
}

/***********************************************************************************
 **	SetSearchText
 ** @param search_text
 ***********************************************************************************/
void HistoryPanel::SetSearchText(const OpStringC& search_text)
{
	m_history_view->SetSearchText(search_text);
}
