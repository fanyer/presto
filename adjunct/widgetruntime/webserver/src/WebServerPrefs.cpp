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

#include "WebServerPrefs.h"

#include "adjunct/quick/webserver/controller/WebServerSettingsContext.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"


OP_STATUS WebServerPrefs::WriteIsStarted(BOOL started)
{
	TRAPD(status, g_pcwebserver->WriteIntegerL(
			PrefsCollectionWebserver::WebserverEnable, started));
	return status;
}


BOOL WebServerPrefs::ReadIsConfigured()
{
	return g_pcwebserver->GetIntegerPref(
			PrefsCollectionWebserver::WebserverUsed);
}


OP_STATUS WebServerPrefs::WriteIsConfigured(BOOL configured)
{
	TRAPD(status, g_pcwebserver->WriteIntegerL(
			PrefsCollectionWebserver::WebserverUsed, configured));
	return status;
}


OP_STATUS WebServerPrefs::WriteUsername(const OpStringC& username)
{
	TRAPD(status, g_pcwebserver->WriteStringL(
			PrefsCollectionWebserver::WebserverUser, username));
	return status;
}


OpStringC WebServerPrefs::ReadDeviceName()
{
	return g_pcwebserver->GetStringPref(
			PrefsCollectionWebserver::WebserverDevice);
}


OP_STATUS WebServerPrefs::WriteDeviceName(const OpStringC& device_name)
{
	TRAPD(status, g_pcwebserver->WriteStringL(
			PrefsCollectionWebserver::WebserverDevice, device_name));
	return status;
}


OP_STATUS WebServerPrefs::ReadSharedSecret(OpString8& shared_secret)
{
	const OpStringC& hashed_password = g_pcwebserver->GetStringPref(
			PrefsCollectionWebserver::WebserverHashedPassword);
	return shared_secret.Set(hashed_password);
}


OP_STATUS WebServerPrefs::WriteSharedSecret(const OpStringC& shared_secret)
{
	TRAPD(status, g_pcwebserver->WriteStringL(
			PrefsCollectionWebserver::WebserverHashedPassword, shared_secret));
	return status;
}


WebserverListeningMode WebServerPrefs::ReadListeningMode()
{
	WebserverListeningMode mode = WEBSERVER_LISTEN_NONE;

	if (0 == g_pcwebserver->GetIntegerPref(
			PrefsCollectionWebserver::UseOperaAccount))
	{
		mode = WEBSERVER_LISTEN_LOCAL;
	}
	else
	{
#ifdef UPNP_SUPPORT
		BOOL upnp_enabled = TRUE;
# ifdef PREFS_CAP_UPNP_ENABLED
		upnp_enabled = ReadIsUPnPEnabled();
# endif // PREFS_CAP_UPNP_ENABLED
		if (upnp_enabled)
		{
			mode = WEBSERVER_LISTEN_OPEN_PORT | WEBSERVER_LISTEN_PROXY_LOCAL;
		}
		else
#endif // UPNP_SUPPORT
		{
			mode = WEBSERVER_LISTEN_PROXY_LOCAL;
		}
	}

	return mode;
}


#ifdef PREFS_CAP_UPNP_ENABLED
BOOL WebServerPrefs::ReadIsUPnPEnabled()
{
	return g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::UPnPEnabled);
}


OP_STATUS WebServerPrefs::WriteIsUPnPEnabled(BOOL enabled)
{
	TRAPD(status, g_pcwebserver->WriteIntegerL(
			PrefsCollectionWebserver::UPnPEnabled, enabled));
	return status;
}
#endif // PREFS_CAP_UPNP_ENABLED


int WebServerPrefs::ReadMaxUploadRate()
{
	return g_pcwebserver->GetIntegerPref(
			PrefsCollectionWebserver::WebserverUploadRate);
}


OP_STATUS WebServerPrefs::WriteMaxUploadRate(int rate)
{
	TRAPD(status, g_pcwebserver->WriteIntegerL(
			PrefsCollectionWebserver::WebserverUploadRate, rate));
	return status;
}


OP_STATUS WebServerPrefs::ReadAll(WebServerSettingsContext& settings)
{
	settings.SetDeviceName(ReadDeviceName());
#ifdef PREFS_CAP_UPNP_ENABLED
	settings.SetUPnPEnabled(ReadIsUPnPEnabled());
#endif // PREFS_CAP_UPNP_ENABLED
	settings.SetUploadSpeed(ReadMaxUploadRate());

	return OpStatus::OK;
}


OP_STATUS WebServerPrefs::WriteAll(const WebServerSettingsContext& settings)
{
	RETURN_IF_ERROR(WriteDeviceName(settings.GetDeviceName()));
#ifdef PREFS_CAP_UPNP_ENABLED
	RETURN_IF_ERROR(WriteIsUPnPEnabled(settings.IsUPnPEnabled()));
#endif // PREFS_CAP_UPNP_ENABLED
	RETURN_IF_ERROR(WriteMaxUploadRate(settings.GetUploadSpeed()));

	return OpStatus::OK;
}


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
