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

#include "WebServerStarter.h"
#include "WebServerPrefs.h"

#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_module.h"


OP_STATUS WebServerStarter::Init()
{
	return OpStatus::OK;
}


BOOL WebServerStarter::IsStarted()
{
	return g_webserver ? g_webserver->IsRunning() : FALSE;
}


OP_STATUS WebServerStarter::Start()
{
	const WebserverListeningMode listening_mode =
			WebServerPrefs::ReadListeningMode();

	if (!g_webserver) return OpStatus::OK;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OpString8 shared_secret;
	RETURN_IF_ERROR(WebServerPrefs::ReadSharedSecret(shared_secret));

	RETURN_IF_ERROR(g_webserver->Start(listening_mode, shared_secret));
#else
	RETURN_IF_ERROR(g_webserver->Start(listening_mode));
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

	OpStatus::Ignore(WebServerPrefs::WriteIsConfigured(TRUE));
	OpStatus::Ignore(WebServerPrefs::WriteIsStarted(TRUE));

	return OpStatus::OK;
}


OP_STATUS WebServerStarter::Stop()
{
	return g_webserver ? g_webserver->Stop() : OpStatus::OK;
}


OP_STATUS WebServerStarter::OnWebServerSetUpCompleted(
		const OpStringC& shared_secret)
{
	if (shared_secret.HasContent())
	{
		RETURN_IF_ERROR(WebServerPrefs::WriteSharedSecret(shared_secret));
		RETURN_IF_ERROR(Start());
	}

	return OpStatus::OK;
}


void WebServerStarter::OnWebServerError(WebserverStatus status)
{
	OP_ASSERT(WEBSERVER_OK != status);

	if (IsStarted())
	{
		Stop();
	}
}


# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT
