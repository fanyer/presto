/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** karie@opera.com
**
*/

#include "core/pch.h"
#include "GadgetsPanel.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/Application.h"
#include "adjunct/widgetruntime/hotlist/GadgetsHotlistView.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/locale/oplanguagemanager.h"


/***********************************************************************************
 **
 **	GadgetsPanel
 **
 ***********************************************************************************/

GadgetsPanel::GadgetsPanel()
	: m_hotlist_view(0)
{
}


OP_STATUS GadgetsPanel::Init()
{
#ifdef WIDGET_RUNTIME_SUPPORT
	m_hotlist_view = OP_NEW(GadgetsHotlistView, ());
	RETURN_OOM_IF_NULL(m_hotlist_view);
	RETURN_IF_ERROR(m_hotlist_view->Init());
#else // WIDGET_RUNTIME_SUPPORT
	RETURN_IF_ERROR(OpHotlistView::Construct(&m_hotlist_view, WIDGET_TYPE_GADGETS_VIEW));
#endif // WIDGET_RUNTIME_SUPPORT

	AddChild(m_hotlist_view);
	SetToolbarName("Widgets Panel Toolbar", "Widgets Full Toolbar");
	SetName("Widgets");

	OnChange(m_hotlist_view, FALSE);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **	GetPanelText
 **
 ***********************************************************************************/

void GadgetsPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::D_WIDGETS, text);
}

/***********************************************************************************
 **
 **	OnFullModeChanged
 **
 ***********************************************************************************/

void GadgetsPanel::OnFullModeChanged(BOOL full_mode)
{
	m_hotlist_view->SetDetailed(full_mode);
}

/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void GadgetsPanel::OnLayoutPanel(OpRect& rect)
{
	m_hotlist_view->SetRect(rect);
}

/***********************************************************************************
 **
 **	OnFocus
 **
 ***********************************************************************************/

void GadgetsPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_hotlist_view->SetFocus(reason);
	}
}


BOOL GadgetsPanel::OnInputAction(OpInputAction* action)
{
	return m_hotlist_view->OnInputAction(action);
}
