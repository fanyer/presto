/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Manuela Hutter
*/

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/view/WebServerAdvancedSettingsDialog.h"

#include "adjunct/quick/webserver/controller/WebServerSettingsContext.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "modules/widgets/OpDropDown.h"
#include "modules/widgets/OpEdit.h"
#include "modules/widgets/OpNumberEdit.h"
#include "modules/util/str.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"

#define UPLOADSPEED_MAX_LENGTH 4

WebServerAdvancedSettingsDialog::WebServerAdvancedSettingsDialog(const WebServerSettingsContext* preset_settings, WebServerSettingsContext* out_settings) :
	m_preset_settings(preset_settings),
	m_out_settings(out_settings)
{
	g_languageManager->GetString(Str::S_UNLIMITED, m_unlimited_text);
}


/*virtual*/	void
WebServerAdvancedSettingsDialog::OnInit()
{
	OpLabel * global_vis_label = static_cast<OpLabel *>(GetWidgetByName("webserver_global_vis_label"));
	if (global_vis_label)
	{
		global_vis_label->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	}

	// set upnp on/off
#ifdef PREFS_CAP_UPNP_ENABLED
	SetWidgetValue("webserver_upnp_checkbox", m_preset_settings->IsUPnPEnabled());
#else
	ShowWidget("webserver_upnp_checkbox", FALSE);
#endif // PREFS_CAP_UPNP_ENABLED

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	SetWidgetValue("webserver_asd_vis_checkbox", m_preset_settings->IsASDEnabled());
#else
	ShowWidget("webserver_asd_vis_checkbox", FALSE);
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
	SetWidgetValue("webserver_upnp_vis_checkbox", m_preset_settings->IsUPnPServiceDiscoveryEnabled());
#else
	ShowWidget("webserver_upnp_vis_checkbox", FALSE);
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	SetWidgetValue("home_robots_vis_checkbox", m_preset_settings->IsVisibleRobotsOnHome());
#else
	ShowWidget("home_robots_vis_checkbox", FALSE);
#endif // WEBSERVER_CAP_SET_VISIBILITY

	// download rate
	INT32 speed = m_preset_settings->GetUploadSpeed();

	OpDropDown* uploadspeed_dropdown = static_cast<OpDropDown *>(GetWidgetByName("webserver_uploadspeed_dropdown"));
	if (uploadspeed_dropdown)
	{
		uploadspeed_dropdown->SetEditableText(TRUE);
		uploadspeed_dropdown->edit->SetAllowedChars("0123456789");
		uploadspeed_dropdown->edit->SetMaxLength(UPLOADSPEED_MAX_LENGTH);
		uploadspeed_dropdown->edit->SetOnChangeOnEnter(FALSE); // make sure changes are reported back on all input

		// dropdown entries
		uploadspeed_dropdown->AddItem(m_unlimited_text.CStr());
		uploadspeed_dropdown->AddItem(UNI_L("5"));
		uploadspeed_dropdown->AddItem(UNI_L("10"));
		uploadspeed_dropdown->AddItem(UNI_L("20"));
		uploadspeed_dropdown->AddItem(UNI_L("30"));
		uploadspeed_dropdown->AddItem(UNI_L("40"));
		uploadspeed_dropdown->AddItem(UNI_L("50"));
		uploadspeed_dropdown->AddItem(UNI_L("75"));
		uploadspeed_dropdown->AddItem(UNI_L("100"));
		uploadspeed_dropdown->AddItem(UNI_L("150"));
		uploadspeed_dropdown->AddItem(UNI_L("200"));
		uploadspeed_dropdown->AddItem(UNI_L("250"));
		uploadspeed_dropdown->AddItem(UNI_L("500"));
		uploadspeed_dropdown->AddItem(UNI_L("750"));

		if (speed != 0)
		{
			OpString text;
			text.AppendFormat(UNI_L("%d"), speed); // integer.toString() is what I really want here..
			uploadspeed_dropdown->edit->SetText(text.CStr());
		}
		else
		{
			uploadspeed_dropdown->SetText(m_unlimited_text.CStr());
		}
	}
	OpNumberEdit * webserver_port_numberedit = static_cast<OpNumberEdit *>(GetWidgetByName("webserver_port_numberedit"));
	if (webserver_port_numberedit)
	{
#ifdef WIDGETS_CAP_NUMBEREDIT_ALLOWED_CHARS
		webserver_port_numberedit->SetAllowedChars("01234567890");
#endif // WIDGETS_CAP_NUMBEREDIT_ALLOWED_CHARS
		webserver_port_numberedit->SetStepBase(0.0);
		webserver_port_numberedit->SetStep(1.0);
		webserver_port_numberedit->SetMaxValue(65535);

		uni_char buf[6];
		webserver_port_numberedit->SetText(uni_itoa(m_preset_settings->GetPort(), buf, 10));
	}
}


/*virtual*/ UINT32
WebServerAdvancedSettingsDialog::OnOk()
{
#ifdef PREFS_CAP_UPNP_ENABLED
	m_out_settings->SetUPnPEnabled(GetWidgetValue("webserver_upnp_checkbox"));
#endif // PREFS_CAP_UPNP_ENABLED

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	m_out_settings->SetASDEnabled(GetWidgetValue("webserver_asd_vis_checkbox"));
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
	m_out_settings->SetUPnPServiceDiscoveryEnabled(GetWidgetValue("webserver_upnp_vis_checkbox"));
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	m_out_settings->SetVisibleRobotsOnHome(GetWidgetValue("home_robots_vis_checkbox"));
#endif // WEBSERVER_CAP_SET_VISIBILITY

	// upload speed
	OpString str;
	GetWidgetText("webserver_uploadspeed_dropdown", str);
	if (str.HasContent())
	{
		// uni_atoi will return 0 for invalid input, which is our default anyway
		m_out_settings->SetUploadSpeed(uni_atoi(str.CStr()));
	}
	else
	{
		m_out_settings->SetUploadSpeed(0);
	}

	// port
	OpNumberEdit * webserver_port_numberedit = static_cast<OpNumberEdit *>(GetWidgetByName("webserver_port_numberedit"));
	if (webserver_port_numberedit)
	{
		INT32 port;
		if (webserver_port_numberedit->GetIntValue(port))
		{
			m_out_settings->SetPort(port);
		}
	}
	return 0;
}

#endif // WEBSERVER_SUPPORT
