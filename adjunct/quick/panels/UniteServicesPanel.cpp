/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * @file
 * @author Owner:    Karianne Ekern (karie)
 *
*/
#include "core/pch.h"
#include "UniteServicesPanel.h"
#include "adjunct/quick_toolkit/widgets/OpToolbar.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/WindowCommanderProxy.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/locale/oplanguagemanager.h"

#ifdef WEBSERVER_SUPPORT

/***********************************************************************************
 **
 **	UniteServicesPanel
 **
 ***********************************************************************************/

UniteServicesPanel::UniteServicesPanel()
	: m_hotlist_view(0),
	  m_needs_attention(0)
{
}

UniteServicesPanel::~UniteServicesPanel()
{
}

/***********************************************************************************
 **
 **	Init
 **
 ***********************************************************************************/

OP_STATUS UniteServicesPanel::Init()
{
	RETURN_IF_ERROR(OpHotlistView::Construct(&m_hotlist_view, WIDGET_TYPE_UNITE_SERVICES_VIEW));
	AddChild(m_hotlist_view);

	SetToolbarName("Unite Services Panel Toolbar", "Unite Services Full Toolbar");
	SetName("UniteServices");

	OnChange(m_hotlist_view, FALSE);
	
	HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
	if(model)
	{
		model->AddListener(this);
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **	GetPanelText
 **
 ***********************************************************************************/

void UniteServicesPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::S_UNITE_SERVICES, text);
}

/***********************************************************************************
 **
 **	OnFullModeChanged
 **
 ***********************************************************************************/

void UniteServicesPanel::OnFullModeChanged(BOOL full_mode)
{
	m_hotlist_view->SetDetailed(full_mode);
}

/***********************************************************************************
 **
 **	OnLayout
 **
 ***********************************************************************************/

void UniteServicesPanel::OnLayoutPanel(OpRect& rect)
{
	m_hotlist_view->SetRect(rect);
}


/***********************************************************************************
 **
 **	OnFocus
 **
 ***********************************************************************************/

void UniteServicesPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
		m_hotlist_view->SetFocus(reason);
	}
}

/***********************************************************************************
 **
 **	OnRemoved
 **
 ***********************************************************************************/

void UniteServicesPanel::OnRemoved()
{
	HotlistModel* model = g_hotlist_manager->GetUniteServicesModel();
	if(model)
	{
		model->RemoveListener(this);
	}
}

/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL UniteServicesPanel::OnInputAction(OpInputAction* action)
{
	return m_hotlist_view->OnInputAction(action);
}

/***********************************************************************************
 **
 **	OnItemChanged
 **
 ***********************************************************************************/

void UniteServicesPanel::OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort)
{
	OpTreeModelItem* it = tree_model->GetItemByPosition(pos);
	if(it && it->GetType() == OpTypedObject::UNITE_SERVICE_TYPE)
	{
		UniteServiceModelItem* item = static_cast<UniteServiceModelItem*>(it);
		if(item)
		{
			if(item->GetAttentionStateChanged())
			{
				if(item->GetNeedsAttention())
				{
					m_needs_attention++;
				}
				else
				{
					m_needs_attention--;
				}
				PanelChanged();
			}
		}
	}
}

#endif // WEBSERVER_SUPPORT

