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

#include "WebServerDeviceHandler.h"
#include "WebServerPrefs.h"

#include "adjunct/quick/webserver/controller/WebServerSettingsContext.h"


namespace
{
	const uni_char* DEFAULT_DEVICE_NAMES[] = {
		UNI_L("home"),
		UNI_L("work"),
		UNI_L("office"),
		UNI_L("notebook"),
		UNI_L("school"),
	};
}


WebServerDeviceHandler::WebServerDeviceHandler()
	: m_taking_over_device(FALSE)
{
}

WebServerDeviceHandler::~WebServerDeviceHandler()
{
	OpStatus::Ignore(ReleaseDevice());
}


OP_STATUS WebServerDeviceHandler::Init()
{
	return OpStatus::OK;
}


OP_STATUS WebServerDeviceHandler::RegisterDevice()
{
	OP_STATUS result = OpStatus::ERR;

	const OpStringC& device_name = WebServerPrefs::ReadDeviceName();
	if (device_name.HasContent())
	{
		OpString install_id;
		g_desktop_account_manager->GetInstallID(install_id);

		OpString username;
		RETURN_IF_ERROR(username.Set(g_desktop_account_manager->GetUsername()));

		OpString password;
		RETURN_IF_ERROR(password.Set(g_desktop_account_manager->GetPassword()));

		result = g_desktop_account_manager->Authenticate(
				username, password, device_name, install_id);
	}

	return result;
}


OP_STATUS WebServerDeviceHandler::ReleaseDevice()
{
	OP_STATUS result = OpStatus::ERR;

	const OpStringC& device_name = WebServerPrefs::ReadDeviceName();
	if (device_name.HasContent())
	{
		OpString username;
		RETURN_IF_ERROR(username.Set(g_desktop_account_manager->GetUsername()));

		OpString password;
		RETURN_IF_ERROR(password.Set(g_desktop_account_manager->GetPassword()));

		result = g_desktop_account_manager->ReleaseDevice(
				username, password, device_name);
	}

	return result;
}


OP_STATUS WebServerDeviceHandler::TakeOverDevice()
{
	RETURN_IF_ERROR(g_desktop_account_manager->AddListener(this));
	RETURN_IF_ERROR(ReleaseDevice());
	m_taking_over_device = TRUE;

	return OpStatus::OK;
	// ...and continue in OnOperaAccountReleaseDevice()...
}


void WebServerDeviceHandler::OnOperaAccountReleaseDevice(OperaAuthError error)
{
	// Error comments copied from
	// WebServerManager::OnOperaAccountReleaseDevice()
	switch (error)
	{
		// code 400: missing argument/malformed URL.  This REALLY shouldn't
		// happen.
		case AUTH_ERROR_PARSER:
		// code 403/404: seems to happen when another computer took over the
		// computer name and this computer is trying to disable
		case AUTH_ACCOUNT_AUTH_FAILURE:
		// code 411: invalid device name
		case AUTH_ACCOUNT_AUTH_INVALID_KEY:
			// There shouldn't really be an error here.
			OP_ASSERT(!"Unexpected auth error");
			
			// Fall through.

		case AUTH_OK:
			if (m_taking_over_device)
			{
				// Finalize device take-over now.
				m_taking_over_device = FALSE;
				g_desktop_account_manager->RemoveListener(this);
				RegisterDevice();
			}
			break;

		default:
			// Ignore
			;
	}
}


OP_STATUS WebServerDeviceHandler::AddDefaultDeviceNames(
		WebServerSettingsContext& settings)
{
	OpAutoPtr<OpString> name;

	for (size_t i = 0; i < ARRAY_SIZE(DEFAULT_DEVICE_NAMES); ++i)
	{
		name.reset(OP_NEW(OpString, ()));
		if (NULL != name.get())
		{
			RETURN_IF_ERROR(name->Set(DEFAULT_DEVICE_NAMES[i]));
			RETURN_IF_ERROR(settings.AddDeviceNameSuggestion(name.get()));
			name.release();
		}
	}

	return OpStatus::OK;
}


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
