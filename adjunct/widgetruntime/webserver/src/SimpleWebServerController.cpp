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

#include "SimpleWebServerController.h"
#include "WebServerDeviceHandler.h"
#include "WebServerPrefs.h"

#include "adjunct/quick/dialogs/SimpleDialog.h"

#include "modules/locale/oplanguagemanager.h"


SimpleWebServerController::SimpleWebServerController()
	: m_device_handler(NULL),
	  m_web_server_running(FALSE),
	  m_configuring(FALSE),
	  m_device_registration_pending(FALSE)
{
}


OP_STATUS SimpleWebServerController::Init(
		WebServerDeviceHandler& device_handler)
{
	m_device_handler = &device_handler;

	return OpStatus::OK;
}


OP_STATUS SimpleWebServerController::Flush()
{
	OP_ASSERT(NULL != m_device_handler);

	if (m_device_registration_pending)
	{
		RETURN_IF_ERROR(m_device_handler->RegisterDevice());
		m_device_registration_pending = FALSE;
	}

	return OpStatus::OK;
}


void SimpleWebServerController::EnableFeature(
		const FeatureSettingsContext* settings,	BOOL force_device_release)
{
	OP_ASSERT(NULL != m_device_handler);

	if (force_device_release)
	{
		m_device_handler->TakeOverDevice();
	}
	else
	{
		if (NULL != settings)
		{
			WebServerPrefs::WriteAll(
					static_cast<const WebServerSettingsContext&>(*settings));
		}
		WebServerPrefs::WriteUsername(
				OpStringC(g_desktop_account_manager->GetUsername()));

		m_device_handler->RegisterDevice();
	}
}


void SimpleWebServerController::EnableFeature(
		const FeatureSettingsContext* settings)
{
	EnableFeature(settings, FALSE);
}


OP_STATUS SimpleWebServerController::Restart()
{
	OP_ASSERT(!m_web_server_running);
	return m_device_handler->TakeOverDevice();
}


void SimpleWebServerController::OnOperaAccountAuth(OperaAuthError error,
		const OpStringC& shared_secret)
{
	OP_ASSERT(NULL != m_device_handler);

	if (AUTH_OK == error)
	{
		if (WebServerPrefs::ReadDeviceName().HasContent())
		{
			if (g_desktop_account_manager->GetLoggedIn())
			{
				m_device_handler->RegisterDevice();
			}
			else
			{
				// This happens if FeatureDialog is handling the logging in and
				// it hasn't handled that same notification yet.  Which means
				// OperaAccountManager is not storing the credentials yet.

				// The device can be registered later via Flush().
				m_device_registration_pending = TRUE;
			}
		}

		NotifyLoggedIn();
	}
	// FIXME: Error handling?
}


void SimpleWebServerController::OnOperaAccountDeviceCreate(OperaAuthError error,
		const OpStringC& shared_secret, const OpStringC& server_message)
{
	OP_ASSERT(NULL != m_device_handler);

	switch (error)
	{
		case AUTH_OK:
			// The Unite device is ready.  The web server can actually be
			// started now.

			NotifyWebServerSetUpCompleted(shared_secret);

			// NOTE: While other parties may be interested in the event we're
			// notifying about here, the Unite setup wizard dialog in particular
			// _must_ receive it.  It won't close if it doesn't.
			BroadcastOnFeatureEnablingSucceeded();

			break;

		case AUTH_ACCOUNT_CREATE_DEVICE_EXISTS:
			// We're only interested in this here if we're not in the middle of
			// web server set-up with the wizard dialog.  The wizard dialog
			// handles device take-over by itself.
			if (!IsConfiguring())
			{
				if (IsUserRequestingDeviceRelease(
						g_application->GetActiveDesktopWindow()))
				{
					m_device_handler->TakeOverDevice();
				}
				else
				{
					NotifyWebServerStartUpError();
				}
			}
			break;

		default:
			// Ignore.
			;
	}
	// FIXME: Error handling?
}


BOOL SimpleWebServerController::IsUserRequestingDeviceRelease(
		DesktopWindow* parent_window)
{
	OpString dialog_text;
	g_languageManager->GetString(
			Str::D_WEBSERVER_RELEASE_DEVICE_NAME_IN_USE, dialog_text);

	OpString dialog_title;
	g_languageManager->GetString(Str::D_WEBSERVER_NAME, dialog_title);

	INT32 answer = Dialog::DIALOG_RESULT_CANCEL;
	SimpleDialog* dialog = OP_NEW(SimpleDialog, ());
	if (NULL != dialog)
	{
		dialog->Init(dialog_title, dialog_text, parent_window,
				Dialog::TYPE_NO_YES, Dialog::IMAGE_WARNING, TRUE, &answer);
	}

	return Dialog::DIALOG_RESULT_YES == answer;
}


void SimpleWebServerController::OnWebserverServiceStarted(
		const uni_char* service_name, const uni_char* service_path,
		BOOL is_root_service)
{
	if (!is_root_service)
	{
		m_web_server_running = TRUE;
		NotifyWebServerStarted();
	}
}


void SimpleWebServerController::OnWebserverStopped(WebserverStatus status)
{
	m_web_server_running = FALSE;
	NotifyWebServerStopped();
}


void SimpleWebServerController::OnWebserverListenLocalFailed(
		WebserverStatus status)
{
	HandleWebServerError(status);
}


#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
void SimpleWebServerController::OnProxyConnectionFailed(
		WebserverStatus status, BOOL retry)
{
	HandleWebServerError(status);
}


void SimpleWebServerController::OnProxyConnectionProblem(
		WebserverStatus status,  BOOL retry)
{
	HandleWebServerError(status);
}


void SimpleWebServerController::OnProxyDisconnected(
		WebserverStatus status, BOOL retry)
{
	if (PROXY_CONNECTION_LOGGED_OUT == status)
	{
		HandleWebServerError(status);
	}
}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT


void SimpleWebServerController::HandleWebServerError(WebserverStatus status)
{
	if (m_web_server_running)
	{
		m_web_server_running = FALSE;
		NotifyWebServerError(status);
	}
}



# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
