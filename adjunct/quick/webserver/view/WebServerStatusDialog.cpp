/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/view/WebServerStatusDialog.h"

#include "adjunct/quick/managers/WebServerManager.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/quick_toolkit/widgets/OpLabel.h"
#include "adjunct/quick_toolkit/widgets/OpIcon.h"
#include "modules/widgets/OpButton.h"
#include "modules/widgets/OpMultiEdit.h"
#include "modules/locale/oplanguagemanager.h"

#define STATUS_UPDATE_RATE 2000

/***********************************************************************************
**  WebServerStatusDialog::WebServerStatusDialog
************************************************************************************/
WebServerStatusDialog::WebServerStatusDialog(WebServerController & controller)
	: m_controller(controller)
	, m_timer(NULL)
{
}

/***********************************************************************************
**  WebServerStatusDialog::~WebServerStatusDialog
************************************************************************************/
/*virtual*/
WebServerStatusDialog::~WebServerStatusDialog()
{
	OP_DELETE(m_timer);
}

/***********************************************************************************
**  WebServerStatusDialog::OnInputAction
************************************************************************************/
/*virtual*/ BOOL
WebServerStatusDialog::OnInputAction(OpInputAction* action)
{
	switch (action->GetAction())
	{
    case OpInputAction::ACTION_GET_ACTION_STATE:
		{
			OpInputAction* child_action = action->GetChildAction();

			switch (child_action->GetAction())
			{
			case OpInputAction::ACTION_FEATURE_CHANGE_ACCOUNT:
			case OpInputAction::ACTION_OPERA_UNITE_RESTART:
				{
					child_action->SetEnabled(m_controller.IsFeatureEnabled());
					return TRUE;
				}
			}
			break;
		}
	case OpInputAction::ACTION_FEATURE_CHANGE_ACCOUNT:
		{
			if(m_controller.IsFeatureEnabled())
			{
				OpStatus::Ignore(m_controller.ChangeAccount());
				CloseDialog(FALSE);
				return TRUE;
			}
			else
				return FALSE;
			break;
		}
	case OpInputAction::ACTION_OPERA_UNITE_RESTART:
		{
			if(m_controller.IsFeatureEnabled())
			{
				m_controller.Restart();
				CloseDialog(FALSE);
				return TRUE;
			}
			else
				return FALSE;
		}
	}
	return Dialog::OnInputAction(action);
}

/***********************************************************************************
**  WebServerStatusDialog::OnInit
************************************************************************************/
/*virtual*/ void
WebServerStatusDialog::OnInit()
{
	OpMultilineEdit* status_edit = static_cast<OpMultilineEdit*>(GetWidgetByName("status_edit"));
	if (status_edit)
	{
		status_edit->SetWrapping(TRUE);
		status_edit->SetFlatMode();
		status_edit->SetSystemFontWeight(QuickOpWidgetBase::WEIGHT_BOLD);
	}

	if (m_controller.IsFeatureEnabled())
	{
		if( !m_timer )
		{
			m_timer = OP_NEW(OpTimer, ());
		}
		m_timer->SetTimerListener( this );
		m_timer->Start(STATUS_UPDATE_RATE);

#ifndef WEBSERVER_CAP_STATS_TRANSFER
		ShowWidget("upload_label", FALSE);
#endif // !WEBSERVER_CAP_STATS_TRANSFER

#ifndef WEBSERVER_CAP_STATS_TRANSFER
		ShowWidget("download_label", FALSE);
#endif // !WEBSERVER_CAP_STATS_TRANSFER

#ifndef WEBSERVER_CAP_STATS_NUM_USERS
		ShowWidget("connections_label", FALSE);
#endif // !WEBSERVER_CAP_STATS_NUM_USERS
	}

	UpdateStatus();
}

/*virtual*/ void
WebServerStatusDialog::OnTimeOut(OpTimer* timer)
{
	if (timer == m_timer)
	{
		UpdateStatus();
		timer->Start(STATUS_UPDATE_RATE);
	}
	else
	{
		Dialog::OnTimeOut(timer);
	}
}

void
WebServerStatusDialog::UpdateStatus()
{
	WebServerStatusContext context;
	m_controller.GetWebServerStatusContext(context);

	OpIcon* status_icon = static_cast<OpIcon*>(GetWidgetByName("status_icon"));
	if (status_icon)
	{
		status_icon->SetImage(context.m_status_icon_name.CStr());
	}

	OpString status_text;
	GetWidgetText("status_edit", status_text);
	if (status_text.Compare(context.m_status_text) != 0)
	{
		// only update the status string if it has changed.
		// otherwise the text selection will be gone on update
		SetWidgetText("status_edit", context.m_status_text.CStr());
	}

	if (m_controller.IsFeatureEnabled())
	{
		OpLabel* running_services_label = static_cast<OpLabel*>(GetWidgetByName("running_services_label"));
		if (running_services_label)
		{
			OpString services_running_text;
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_SERVICES_RUNNING, services_running_text);

			OpString services_running_full_text;
			services_running_full_text.AppendFormat(services_running_text.CStr(), context.m_services_running);

			running_services_label->SetText(services_running_full_text.CStr());
		}

#ifdef WEBSERVER_CAP_STATS_TRANSFER
		OpLabel* upload_label = static_cast<OpLabel*>(GetWidgetByName("upload_label"));
		if (upload_label)
		{
			OpString upload_text;
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_DATA_UPLOADED, upload_text);

			OpString size_text;
			size_text.Reserve(256);
			StrFormatByteSize(size_text, context.m_size_uploaded, SFBS_DEFAULT);
			
			OpString upload_full_text;
			upload_full_text.AppendFormat(upload_text.CStr(), size_text.CStr());

			upload_label->SetText(upload_full_text.CStr());
		}
#endif // WEBSERVER_CAP_STATS_TRANSFER

#ifdef WEBSERVER_CAP_STATS_TRANSFER
		OpLabel* download_label = static_cast<OpLabel*>(GetWidgetByName("download_label"));
		if (download_label)
		{
			OpString download_text;
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_DATA_DOWNLOADED, download_text);

			OpString size_text;
			size_text.Reserve(256);
			StrFormatByteSize(size_text, context.m_size_downloaded, SFBS_DEFAULT);
			
			OpString download_full_text;
			download_full_text.AppendFormat(download_text.CStr(), size_text.CStr());
			download_label->SetText(download_full_text.CStr());
		}
#endif // WEBSERVER_CAP_STATS_TRANSFER

#ifdef WEBSERVER_CAP_STATS_NUM_USERS
		OpLabel* connections_label = static_cast<OpLabel*>(GetWidgetByName("connections_label"));
		if (connections_label)
		{
			OpString connections_text;
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_PEOPLE_CONNECTED, connections_text);

			OpString connections_full_text;
			connections_full_text.AppendFormat(connections_text.CStr(), context.m_people_connected, S_NUM_MIN);
			connections_label->SetText(connections_full_text.CStr());
		}
#endif // WEBSERVER_CAP_STATS_NUM_USERS

		OpString upnp_str;
		if (context.m_direct_access_address.HasContent())
		{
			OpString lang_str;
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_UPNP, lang_str);
			upnp_str.AppendFormat(lang_str.CStr(), context.m_direct_access_address.CStr());
		}
		else
		{
			g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_UPNP_NO_PORT, upnp_str);
		}
		SetWidgetText("upnp_port_label", upnp_str.CStr());
	}
	else
	{
		ShowWidget("stats_group", FALSE);
	}

	if (context.m_suggested_action == WebServerErrorDescr::ActionNone)
	{
		ShowWidget("suggestion_group", FALSE);
	}
	else
	{
		ShowWidget("suggestion_group", TRUE);

		OpString suggestion_text, button_text;
		OpInputAction * suggested_action = NULL;

		switch(context.m_suggested_action)
		{
		case WebServerErrorDescr::ActionRestart:
			{
				g_languageManager->GetString(Str::D_WEBSERVER_STATUS_DIALOG_SUGGESTION, suggestion_text);
				g_languageManager->GetString(Str::S_WEBSERVER_RESTART, button_text);
				suggested_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_OPERA_UNITE_RESTART));
			}
			break;
		case WebServerErrorDescr::ActionChangeUser:
			{
				// todo: change strings to webserver strings!!
				g_languageManager->GetString(Str::D_SYNC_STATUS_DIALOG_SUGGESTION_CHANGE_USER, suggestion_text);
				g_languageManager->GetString(Str::D_SYNC_STATUS_DIALOG_CHANGE_USER, button_text);
				suggested_action = OP_NEW(OpInputAction, (OpInputAction::ACTION_FEATURE_CHANGE_ACCOUNT));
			}
			break;
		default:
			OP_ASSERT(!"unhandled action");
		}

		SetWidgetText("suggestion_label", suggestion_text.CStr());
		OpButton * suggestion_button = static_cast<OpButton *>(GetWidgetByName("suggestion_button"));
		if (suggestion_button)
		{
			suggestion_button->SetText(button_text.CStr());
			if (suggested_action)
			{
				suggestion_button->SetAction(suggested_action);
			}
		}
	}

	// make sure only the space for visible groups is used
	CompressGroups();
}

#endif // WEBSERVER_SUPPORT
