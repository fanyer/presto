/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "SimpleWebServerManagerImpl.h"
#include "WebServerDeviceHandler.h"
#include "WebServerPrefs.h"

#include "adjunct/quick/managers/OperaAccountManager.h"
#include "adjunct/quick/webserver/view/WebServerSetupWizard.h"


WebServerSettingsContext SimpleWebServerManagerImpl::s_web_server_settings;


SimpleWebServerManagerImpl::SimpleWebServerManagerImpl()
	: m_configuration_pending(FALSE)
{
}

SimpleWebServerManagerImpl::~SimpleWebServerManagerImpl()
{

	if (g_webserver) g_webserver->RemoveWebserverListener(&m_controller);
	g_opera_account_manager->RemoveListener(&m_controller);
	m_web_server_starter.Stop();
}


OP_STATUS SimpleWebServerManagerImpl::Init()
{
	RETURN_IF_ERROR(m_web_server_starter.Init());
	RETURN_IF_ERROR(m_device_handler.Init());
	RETURN_IF_ERROR(m_controller.Init(m_device_handler));
	RETURN_IF_ERROR(m_web_server_state_view.Init(m_controller));


	RETURN_IF_ERROR(g_desktop_account_manager->AddListener(&m_controller));
	RETURN_IF_ERROR(g_webserver ? g_webserver->AddWebserverListener(&m_controller) : OpStatus::OK);

	RETURN_IF_ERROR(AddWebServerStateListener(m_web_server_starter));
	RETURN_IF_ERROR(AddWebServerStateListener(m_web_server_state_view));
	RETURN_IF_ERROR(AddWebServerStateListener(*this));

	return OpStatus::OK;
}


OP_STATUS SimpleWebServerManagerImpl::AddWebServerStateListener(
		WebServerStateListener& listener)
{
	// Add the listener to all the state change generators that we know.
	RETURN_IF_ERROR(m_web_server_starter.AddWebServerStateListener(listener));
	RETURN_IF_ERROR(m_controller.AddWebServerStateListener(listener));

	return OpStatus::OK;
}


OP_STATUS SimpleWebServerManagerImpl::StartWebServer()
{
	BOOL logged_in = FALSE;

	if (g_desktop_account_manager->IsAutoLoginRequest())
	{
		// Postpone the web server configuration dialog (if required) until the
		// automatic log-in completes.  Will continue in OnLoggedIn().
		// This makes life easier for the Unite wizard.
		m_configuration_pending = !WebServerPrefs::ReadIsConfigured();
		logged_in = TRUE;
	}
	else
	{
		BOOL configured = WebServerPrefs::ReadIsConfigured();
		if (!configured)
		{
			configured = ConfigureWithDialog();
			logged_in = configured;
		}

		if (configured && !logged_in)
		{
			logged_in = g_desktop_account_manager->GetLoggedIn();
			if (!logged_in)
			{
				logged_in = LogInWithDialog();
			}
		}

		// The controller might have accumulated some actions to be performed
		// once user interaction is finished.
		RETURN_IF_ERROR(m_controller.Flush());
	}

	return logged_in ? OpStatus::OK : OpStatus::ERR;
}


OP_STATUS SimpleWebServerManagerImpl::OnLoggedIn()
{
	OP_STATUS result = OpStatus::OK;

	if (m_configuration_pending)
	{
		m_configuration_pending = FALSE;
		result = ConfigureWithDialog() ? OpStatus::OK : OpStatus::ERR;

		// The controller might have accumulated some actions to be performed
		// once user interaction is finished.
		RETURN_IF_ERROR(m_controller.Flush());
	}

	return result;
}


BOOL SimpleWebServerManagerImpl::ConfigureWithDialog()
{
	BOOL configured = FALSE;

	WebServerDialog* dialog = OP_NEW(WebServerDialog, (
			&m_controller, &GetWebServerSettings(), g_desktop_account_manager,
			g_desktop_account_manager->GetAccountContext(), TRUE));

	if (NULL != dialog)
	{
		dialog->SetBlockingForReturnValue(&configured);
		if (OpStatus::IsError(dialog->Init(NULL)))
		{
			configured = FALSE;
		}
	}

	return configured;
}


BOOL SimpleWebServerManagerImpl::LogInWithDialog()
{
	BOOL logged_in = FALSE;

	WebServerDialog* dialog = OP_NEW(WebServerDialog, (
			&m_controller, &GetWebServerSettings(), g_desktop_account_manager,
			g_desktop_account_manager->GetAccountContext(), FALSE));

	if (NULL != dialog)
	{
		if (OpStatus::IsError(dialog->InitLoginDialog(&logged_in)))
		{
			logged_in = FALSE;
		}
	}

	return logged_in;
}


OP_STATUS SimpleWebServerManagerImpl::ShowWebServerStatus()
{
	return m_web_server_state_view.ShowStatusDialog();
}


const WebServerSettingsContext&
		SimpleWebServerManagerImpl::GetWebServerSettings()
{
	OpStatus::Ignore(WebServerPrefs::ReadAll(s_web_server_settings));

	s_web_server_settings.SetIsFeatureEnabled(m_web_server_starter.IsStarted());

	if (0 == s_web_server_settings.GetDeviceNameSuggestions()->GetCount())
	{
		OpStatus::Ignore(WebServerDeviceHandler::AddDefaultDeviceNames(
				s_web_server_settings));
	}

	return s_web_server_settings;
}


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
