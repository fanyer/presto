/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WEBSERVERPREFS_H
#define	WEBSERVERPREFS_H

#ifdef WIDGET_RUNTIME_SUPPORT
# ifdef WIDGET_RUNTIME_UNITE_SUPPORT

#include "modules/util/opstring.h"
#include "modules/webserver/webserver-api.h"

class WebServerSettingsContext;


/**
 * The "persistence" layer of a web server manager.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WebServerPrefs
{
public:
	static OP_STATUS WriteIsStarted(BOOL started);

	static BOOL ReadIsConfigured();
	static OP_STATUS WriteIsConfigured(BOOL configured);

	static OP_STATUS WriteUsername(const OpStringC& username);

	static OpStringC ReadDeviceName();
	static OP_STATUS WriteDeviceName(const OpStringC& device_name);

	static OP_STATUS ReadSharedSecret(OpString8& shared_secret);
	static OP_STATUS WriteSharedSecret(const OpStringC& shared_secret);

	static WebserverListeningMode ReadListeningMode();

#ifdef PREFS_CAP_UPNP_ENABLED
	static BOOL ReadIsUPnPEnabled();
	static OP_STATUS WriteIsUPnPEnabled(BOOL enabled);
#endif // PREFS_CAP_UPNP_ENABLED

	static int ReadMaxUploadRate();
	static OP_STATUS WriteMaxUploadRate(int rate);

	static OP_STATUS ReadAll(WebServerSettingsContext& settings);
	static OP_STATUS WriteAll(const WebServerSettingsContext& settings);
};

# endif // WIDGET_RUNTIME_UNITE_SUPPORT
#endif // WIDGET_RUNTIME_SUPPORT

#endif // WEBSERVERPREFS_H
