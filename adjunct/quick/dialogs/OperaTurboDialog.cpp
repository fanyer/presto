/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 */
#include "core/pch.h"

#ifdef WEB_TURBO_MODE

#include "adjunct/quick/dialogs/OperaTurboDialog.h"
#include "adjunct/quick/managers/OperaTurboManager.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpGroup.h"

#include "modules/locale/locale-enum.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"
#include "modules/widgets/OpMultiEdit.h"

OperaTurboDialog::OperaTurboDialog(BOOL3 slow_network)
: m_slow_network(slow_network)
{
  
}

void OperaTurboDialog::OnInit()
{
	OpLabel* turbo_label = (OpLabel*) GetWidgetByName("turbo_label");
	if(turbo_label)
	{
		turbo_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		turbo_label->SetRelativeSystemFontSize(150);
	}

	if(m_slow_network == YES)
	{
		SetWidgetText("turbo_info", Str::D_OPERA_TURBO_SLOW_NET_INFO);
	}
	else if(m_slow_network == NO)
	{
		SetWidgetText("turbo_info", Str::D_OPERA_TURBO_FAST_NET_INFO);
	}
	

	OpWidget* w = GetWidgetByName("info_group");
	if(w)
	{
		w->SetSkinned(TRUE);
		w->SetHasCssBorder(FALSE);
		w->GetBorderSkin()->SetImage("Security Button Skin");
		if(m_slow_network == MAYBE)
		{
			ShowWidget("info_group", FALSE);
		}
	}


	// Setting the text for the radio buttons to bold
	OpLabel* l = (OpLabel*)GetWidgetByName("radio_turbo_text_auto");
	if(l)
		l->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	
	l = (OpLabel*)GetWidgetByName("radio_turbo_text_on");
	if(l)
		l->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
		
	l = (OpLabel*)GetWidgetByName("radio_turbo_text_off");
	if(l)
		l->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	
	
	int turbo_mode = g_pcui->GetIntegerPref(PrefsCollectionUI::OperaTurboMode);
	
	switch(turbo_mode)
	{
		case OperaTurboManager::OperaTurboAuto:
		{
			SetWidgetValue("radio_turbo_auto", TRUE);
			ShowWidget("info_group", FALSE);
			break;
		}
		case OperaTurboManager::OperaTurboOn:
		{
			if(m_slow_network == YES)
			{
				ShowWidget("info_group", FALSE);
			}
			SetWidgetValue("radio_turbo_on", TRUE);
			break;
		}
		case OperaTurboManager::OperaTurboOff:
		{
			if(m_slow_network == NO)
			{
				ShowWidget("info_group", FALSE);
			}
			SetWidgetValue("radio_turbo_off", TRUE);
			break;
		}
	}
	
	int show_notification = g_pcui->GetIntegerPref(PrefsCollectionUI::ShowNetworkSpeedNotification);
	SetWidgetValue("notify_checkbox", show_notification);

	CompressGroups();
}

UINT32 OperaTurboDialog::OnOk()
{
	int turbo_mode = -1;

	if(GetWidgetValue("radio_turbo_auto"))
	{
		turbo_mode = OperaTurboManager::OperaTurboAuto;
	}
	else if(GetWidgetValue("radio_turbo_on"))
	{
		turbo_mode = OperaTurboManager::OperaTurboOn;
	}
	else if(GetWidgetValue("radio_turbo_off"))
	{
		turbo_mode = OperaTurboManager::OperaTurboOff;
	}

	if(turbo_mode > -1)
	{
		g_opera_turbo_manager->SetOperaTurboMode(turbo_mode);
	}
	
	int show_notification = GetWidgetValue("notify_checkbox", 0);
	TRAPD(err, g_pcui->WriteIntegerL(PrefsCollectionUI::ShowNetworkSpeedNotification, show_notification));

	return 0;
}

void OperaTurboDialog::OnCancel()
{
}

#endif // WEB_TURBO_MODE
