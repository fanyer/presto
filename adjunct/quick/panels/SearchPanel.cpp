// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#include "SearchPanel.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick/hotlist/HotlistManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick/widgets/OpAddressDropDown.h"
#include "modules/widgets/WidgetContainer.h"
#include "modules/widgets/OpEdit.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/url/url_man.h"
#include "modules/locale/oplanguagemanager.h"

/***********************************************************************************
 **
 **	Init
 **
 ***********************************************************************************/

OP_STATUS SearchPanel::Init()
{
	GetBorderSkin()->SetImage("Search Skin");
	SetToolbarName("Search Panel Toolbar", "Search Full Toolbar");
	SetName("Search");
	SetSkinned(TRUE);

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **	SearchPanel
 **
 ***********************************************************************************/

void SearchPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::SI_IDPREFS_SEARCH, text);
}

/***********************************************************************************
 **
 **	OnFullModeChanged
 **
 ***********************************************************************************/

void SearchPanel::OnFullModeChanged(BOOL full_mode)
{
}

/***********************************************************************************
 **
 **	OnLayoutToolbar
 **
 ***********************************************************************************/

void SearchPanel::OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect)
{
	rect = rect.InsetBy(10, 10);
	toolbar->SetRect(rect);
}

/***********************************************************************************
 **
 **	OnFocus
 **
 ***********************************************************************************/

void SearchPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
{
	if (focus)
	{
	}
}

/***********************************************************************************
 **
 **	OnInputAction
 **
 ***********************************************************************************/

BOOL SearchPanel::OnInputAction(OpInputAction* action)
{
	return FALSE;
}
