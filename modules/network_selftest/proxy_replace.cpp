/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#include "core/pch.h"

#if defined(SELFTEST)

#include "modules/network_selftest/proxy_replace.h"

Selftest_ProxyOveride::	Selftest_ProxyOveride()
: http(PrefsCollectionNetwork::HTTPProxy, PrefsCollectionNetwork::UseHTTPProxy),
	https(PrefsCollectionNetwork::HTTPSProxy, PrefsCollectionNetwork::UseHTTPSProxy),
#ifdef FTP_SUPPORT
	ftp(PrefsCollectionNetwork::FTPProxy, PrefsCollectionNetwork::UseFTPProxy),
#endif
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	auto_proxy(PrefsCollectionNetwork::AutomaticProxyConfigURL, PrefsCollectionNetwork::AutomaticProxyConfig),
#endif
	no_proxy(PrefsCollectionNetwork::NoProxyServers, PrefsCollectionNetwork::HasNoProxyServers)
{
}

OP_STATUS Selftest_ProxyOveride::Construct(const OpStringC &proxy_name, BOOL enable_proxy)
{
	RETURN_IF_ERROR(http.Construct(proxy_name, enable_proxy));
	RETURN_IF_ERROR(https.Construct(proxy_name, enable_proxy));
#ifdef FTP_SUPPORT
	RETURN_IF_ERROR(ftp.Construct(proxy_name, enable_proxy));
#endif
	RETURN_IF_ERROR(no_proxy.Construct(UNI_L("certs.opera.com;xml.opera.com;ocsp.verisign.com;crl.verisign.com;EVSecure-crl.verisign.com;EVSecure-aia.verisign.com"), TRUE));
	RETURN_IF_ERROR(http.Activate());
	RETURN_IF_ERROR(https.Activate());
#ifdef FTP_SUPPORT
	RETURN_IF_ERROR(ftp.Activate());
#endif
	RETURN_IF_ERROR(no_proxy.Activate());
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	// Disable autoproxy
	RETURN_IF_ERROR(auto_proxy.Construct(UNI_L(""), FALSE));
	RETURN_IF_ERROR(auto_proxy.Activate());
#endif
	return OpStatus::OK;
}

#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
OP_STATUS Selftest_ProxyOveride::SetAutoProxy(const OpStringC8 &proxy_url, BOOL enable_proxy)
{
	OpString url_temp;
	RETURN_IF_ERROR(url_temp.Set(proxy_url));
	RETURN_IF_ERROR(auto_proxy.Construct(url_temp, enable_proxy));
	RETURN_IF_ERROR(auto_proxy.Activate());
	return OpStatus::OK;
}
#endif

void Selftest_ProxyOveride::ShutDown()
{
	http.ShutDown();
	https.ShutDown();
#ifdef FTP_SUPPORT
	ftp.ShutDown();
#endif
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	auto_proxy.ShutDown();
#endif
	no_proxy.ShutDown();
}

BOOL Selftest_ProxyOveride::Active()
{
	return (http.Active() && https.Active() &&
#ifdef FTP_SUPPORT
		ftp.Active() &&
#endif
		no_proxy.Active() 
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		&& auto_proxy.Active()
#endif
		)
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
		|| (!http.Active() && !https.Active() &&
#ifdef FTP_SUPPORT
		!ftp.Active() &&
#endif
		!no_proxy.Active() &&
		auto_proxy.Active()
		)
#endif

		;
}

Selftest_ProxyOveride::Individual_ProxyOverride::Individual_ProxyOverride(
			PrefsCollectionNetwork::stringpref a_name, 
			PrefsCollectionNetwork::integerpref a_flag
		)
		: name_pref(a_name), enabled_pref(a_flag),
		proxy_enabled(FALSE), new_enable_proxy(FALSE),
		configured(FALSE)
{
}

OP_STATUS Selftest_ProxyOveride::Individual_ProxyOverride::Construct(const OpStringC &a_proxy, BOOL a_enabled)
{
	RETURN_IF_ERROR(new_proxy_name.Set(a_proxy));
	new_enable_proxy = a_enabled;
	RETURN_IF_ERROR(proxy_name.Set(g_pcnet->GetStringPref(name_pref)));
	proxy_enabled = (BOOL) g_pcnet->GetIntegerPref(enabled_pref);

	return OpStatus::OK;
}

OP_STATUS Selftest_ProxyOveride::Individual_ProxyOverride::SetPrefs(const OpStringC &a_proxy, BOOL a_enabled)
{
#ifdef PREFS_WRITE
	RETURN_IF_LEAVE(g_pcnet->WriteStringL(name_pref,a_proxy));
	RETURN_IF_LEAVE(g_pcnet->WriteIntegerL(enabled_pref, a_enabled));

	return OpStatus::OK;
#else
	return OpStatus::ERR_NOT_SUPPORTED;
#endif
}

void Selftest_ProxyOveride::Individual_ProxyOverride::ShutDown()
{
	if(!configured)
		return;

	OpStatus::Ignore(SetPrefs(proxy_name, proxy_enabled));
	configured = TRUE;
}

OP_STATUS Selftest_ProxyOveride::Individual_ProxyOverride::Activate()
{
	RETURN_IF_ERROR(SetPrefs(new_proxy_name, new_enable_proxy));
	configured = TRUE;

	return OpStatus::OK;
}

#endif // SELFTEST
