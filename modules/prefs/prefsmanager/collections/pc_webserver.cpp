/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"

#include "modules/prefs/prefsmanager/collections/pc_webserver_c.inl"

PrefsCollectionWebserver *PrefsCollectionWebserver::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcwebserver)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcwebserver = OP_NEW_L(PrefsCollectionWebserver, (reader));
	return g_opera->prefs_module.m_pcwebserver;
}

PrefsCollectionWebserver::~PrefsCollectionWebserver()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCWEBSERVER_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCWEBSERVER_NUMBEROFINTEGERPREFS);
#endif

	g_opera->prefs_module.m_pcwebserver = NULL;
}

void PrefsCollectionWebserver::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCWEBSERVER_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCWEBSERVER_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_VALIDATE
void PrefsCollectionWebserver::CheckConditionsL(int which, int *value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
	case WebserverEnable:
	case WebserverListenToAllNetworks:
	case WebserverPort:
	case WebserverBacklog:
	case WebserverUploadRate:
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	case WebserverProxyPort:
#endif
	case WebserverUsed:
	case WebserverAlwaysOn:
	case UseOperaAccount:
#ifdef UPNP_SUPPORT
	case UPnPEnabled:
	case UPnPServiceDiscoveryEnabled:
#endif
	case RobotsTxtEnabled:
	case ServiceDiscoveryEnabled:
		break;

	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionWebserver::CheckConditionsL(int which, const OpStringC &invalue,
                                                OpString **outvalue, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<stringpref>(which))
	{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	case WebserverProxyHost:
	case WebserverDevice:
	case WebserverUser:		
	case WebserverHashedPassword:
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
#ifdef WEB_UPLOAD_SERVICE_LIST
	case ServiceDiscoveryServer:
#endif
        break;
	default:
		// Unhandled preference! For clarity, all preferenes not needing to
		// be checked should be put in an empty case something: break; clause
		// above.
		OP_ASSERT(!"Unhandled preference");
	}

	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE

#endif // WEBSERVER_SUPPORT
