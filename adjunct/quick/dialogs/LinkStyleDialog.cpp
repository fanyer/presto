/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#include "LinkStyleDialog.h"

# include "modules/display/styl_man.h"

#include "modules/style/css.h"

#include "modules/display/color.h"
#include "modules/dochand/winman.h"
#include "modules/prefs/prefsmanager/collections/pc_display.h"
#include "modules/prefs/prefsmanager/collections/pc_fontcolor.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

void LinkStyleDialog::Init(DesktopWindow* parent_window)
{
	Dialog::Init(parent_window);
}


void LinkStyleDialog::OnInit()
{
	SetWidgetValue("Underline_link_checkbox",       g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasUnderline));
	SetWidgetValue("Color_link_checkbox",           g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasColor));
	SetWidgetValue("Strikethrough_link_checkbox",   g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasStrikeThrough));

	SetWidgetValue("Border_links_checkbox",         g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::LinkHasFrame));

	SetWidgetValue("Underline_visited_checkbox",    g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasUnderline));
	SetWidgetValue("Color_visited_checkbox",        g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasColor));
	SetWidgetValue("Strikethrough_visited_checkbox",g_pcdisplay->GetIntegerPref(PrefsCollectionDisplay::VisitedLinkHasStrikeThrough));

	short days, hours;
	char tmpexp[5];
	days = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::FollowedLinkExpireDays);
	hours= g_pcnet->GetIntegerPref(PrefsCollectionNetwork::FollowedLinkExpireHours);
	sprintf(tmpexp, "%d", days);
	SetWidgetText("Mark_visited_days_edit", tmpexp);
	sprintf(tmpexp, "%d", hours);
	SetWidgetText("Mark_visited_hours_edit", tmpexp);
}


void LinkStyleDialog::OnColorSelected(COLORREF color)
{
	OpWidget* widget = NULL;

	if (m_choose_visited)
	{
		widget = GetWidgetByName("Color_visited_button");
	}
	else
	{
		widget = GetWidgetByName("Color_link_button");
	}

	if (widget)
	{
		widget->SetBackgroundColor(color);
	}
}


UINT32 LinkStyleDialog::OnOk()
{
	OpWidget* widget = NULL;

	widget = GetWidgetByName("Color_visited_button");
	if (widget)
	{
		COLORREF color = 0;
		color = widget->GetBackgroundColor(color);
		g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_VISITED_LINK, color);
	}

	widget = GetWidgetByName("Color_link_button");
	if (widget)
	{
		COLORREF color = 0;
		color = widget->GetBackgroundColor(color);
		g_pcfontscolors->WriteColorL(OP_SYSTEM_COLOR_LINK, color);
	}

	BOOL underline=FALSE;
	BOOL italic=FALSE;
	BOOL color=FALSE;
	BOOL bold=FALSE;
	BOOL strikethrough=FALSE;
	BOOL button=FALSE;

	underline = GetWidgetValue("Underline_visited_checkbox");
	color = GetWidgetValue("Color_visited_checkbox");
	strikethrough = GetWidgetValue("Strikethrough_visited_checkbox");

	TRAPD(err, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::VisitedLinkHasColor, color));
	TRAP(err, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::VisitedLinkHasUnderline, underline));
	TRAP(err, g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::VisitedLinkHasStrikeThrough, strikethrough));

	underline = GetWidgetValue("Underline_link_checkbox");
	color = GetWidgetValue("Color_link_checkbox");
	strikethrough = GetWidgetValue("Strikethrough_link_checkbox");

	button = GetWidgetValue("Border_links_checkbox");

	g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::LinkHasColor, color);
	g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::LinkHasUnderline, underline);
	g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::LinkHasStrikeThrough, strikethrough);
	g_pcdisplay->WriteIntegerL(PrefsCollectionDisplay::LinkHasFrame, button);

#ifdef DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
	LayoutStyle *styl;
#else // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
	Style *styl;
#endif // DISPLAY_CAP_STYLE_IS_LAYOUTSTYLE
	styl = styleManager->GetStyle(HE_A);
	PresentationAttr pres = styl->GetPresentationAttr();
	if (color)
	{
		pres.Color = g_pcfontscolors->GetColor(OP_SYSTEM_COLOR_LINK);
	}
	else
		pres.Color = USE_DEFAULT_COLOR;
	pres.Bold = bold;
	pres.Italic = italic;
	pres.Underline = underline;
	pres.StrikeOut = strikethrough;
	//pres.Frame = button;
	styl->SetPresentationAttr(pres);

	OP_MEMORY_VAR short days = -1, hours = -1;

	OpString num;

	GetWidgetText("Mark_visited_days_edit", num);
	if (num.HasContent())
		days = uni_atoi(num.CStr());

	GetWidgetText("Mark_visited_hours_edit", num);
	if (num.HasContent())
		hours = uni_atoi(num.CStr());

	TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::FollowedLinkExpireDays, days));
	TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::FollowedLinkExpireHours, hours));

	windowManager->UpdateWindows(TRUE);

	return 0;
}


BOOL LinkStyleDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
		case OpInputAction::ACTION_CHOOSE_LINK_COLOR:
		{
			COLORREF color = 0;

			OpColorChooser* chooser = NULL;

			m_choose_visited = action->GetActionData() == 1;

			if (OpStatus::IsSuccess(OpColorChooser::Create(&chooser)))
			{
				chooser->Show(color, this, this);
				OP_DELETE(chooser);
			}
			break;
		}
		default:
			break;
	}
	return Dialog::OnInputAction(action);
}
