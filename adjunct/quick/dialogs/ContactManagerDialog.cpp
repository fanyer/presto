/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** 
*/
#include "core/pch.h"

#include "ContactManagerDialog.h"
#include "modules/widgets/WidgetContainer.h"
#include "adjunct/quick/widgets/OpHotlistView.h"
#include "adjunct/quick_toolkit/widgets/OpQuickFind.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

/***********************************************************************************
**
**	OnInit
**
***********************************************************************************/

void ContactManagerDialog::OnInit()
{
	m_hotlist_view = (OpHotlistView*) GetWidgetByName("Contacts_view");
	OpQuickFind* quickfind = (OpQuickFind*) GetWidgetByName("Quickfind_edit");

	m_hotlist_view->SetPrefsmanFlags(PrefsCollectionUI::HotlistContactsSplitter, PrefsCollectionUI::HotlistContactsStyle, PrefsCollectionUI::HotlistContactsManagerSplitter, PrefsCollectionUI::HotlistContactsManagerStyle);
	m_hotlist_view->SetDetailed(TRUE);

	if (quickfind && m_hotlist_view)
	{
		quickfind->SetTarget(m_hotlist_view->GetItemView());
	}
}

ContactManagerDialog::~ContactManagerDialog()
{
}

/***********************************************************************************
**
**	OnInputAction
**
***********************************************************************************/

BOOL ContactManagerDialog::OnInputAction(OpInputAction* action)
{
	if (m_hotlist_view && m_hotlist_view->OnInputAction(action))
	{
		return TRUE;
	}

	return Dialog::OnInputAction(action);
}
