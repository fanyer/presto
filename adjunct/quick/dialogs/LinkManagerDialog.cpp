/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** 
*/
#include "core/pch.h"

#include "LinkManagerDialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/widgets/OpLinksView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
//#include "modules/prefs/prefsmanager/prefsmanager.h"


/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void LinkManagerDialog::OnInit()
{
	m_links_view = (OpLinksView*) GetWidgetByName("Links_view");
	OpQuickFind* quickfind = (OpQuickFind*) GetWidgetByName("Quickfind_edit");

	m_links_view->SetDetailed(TRUE);

	if (quickfind && m_links_view)
	{
		quickfind->SetTarget(m_links_view->GetTree());
	}
}

LinkManagerDialog::~LinkManagerDialog()
{
}

/***********************************************************************************
**
**	OnClick
**
***********************************************************************************/

void LinkManagerDialog::OnClick(OpWidget *widget, UINT32 id)
{
	if (widget->IsNamed("Lock_checkbox"))
	{
		m_links_view->SetLocked(widget->GetValue());
    }

    Dialog::OnClick(widget, id);
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL LinkManagerDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
	case OpInputAction::ACTION_DOWNLOAD_URL_AS:
	case OpInputAction::ACTION_DOWNLOAD_URL:
		if (action->GetActionData() == 0)
		{
			action->SetActionData((INTPTR)this);
		}
	}

	if (m_links_view && m_links_view->OnInputAction(action))
	{
		return TRUE;
	}

	return Dialog::OnInputAction(action);
}
