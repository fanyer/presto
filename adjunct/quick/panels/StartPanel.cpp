// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2005 Opera Software ASA.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#include "core/pch.h"

#include "StartPanel.h"

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

OP_STATUS StartPanel::Init()
{
	SetSkinned(TRUE);
	GetBorderSkin()->SetImage("Start Skin");
	GetForegroundSkin()->SetImage("Start Logo");

	SetToolbarName("Start Panel Toolbar", "Start Full Toolbar");

	return OpStatus::OK;
}

/***********************************************************************************
**
**	StartPanel
**
***********************************************************************************/

void StartPanel::GetPanelText(OpString& text, Hotlist::PanelTextType text_type)
{
	g_languageManager->GetString(Str::DI_IDM_START_PREF_BOX, text);
}

/***********************************************************************************
**
**	OnFullModeChanged
**
***********************************************************************************/

void StartPanel::OnFullModeChanged(BOOL full_mode)
{
}

/***********************************************************************************
**
**	OnPaint
**
***********************************************************************************/

void StartPanel::OnPaint(OpWidgetPainter* widget_painter, const OpRect &paint_rect)
{
	if (!IsFullMode())
		return;

	OpRect rect = GetBounds().InsetBy(10, 10);

	INT32 width, height;
	GetForegroundSkin()->GetSize(&width, &height);

	if (width > rect.width)
	{
		height = height * rect.width / width;
		width = rect.width;
	}

	rect.x += rect.width - width;
	rect.y += rect.height - height;
	rect.width = width;
	rect.height = height;

	GetForegroundSkin()->Draw(vis_dev, rect);
}

/***********************************************************************************
**
**	OnLayoutToolbar
**
***********************************************************************************/

void StartPanel::OnLayoutToolbar(OpToolbar* toolbar, OpRect& rect)
{
	rect = rect.InsetBy(10, 10);
	rect = toolbar->LayoutToAvailableRect(rect);
}

/***********************************************************************************
**
**	OnFocus
**
***********************************************************************************/

void StartPanel::OnFocus(BOOL focus,FOCUS_REASON reason)
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

BOOL StartPanel::OnInputAction(OpInputAction* action)
{
	return FALSE;
}

void StartPanel::OnClick(OpWidget *widget, UINT32 id)
{
}
