/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#ifndef _PROXY_OVERRIDE_H
#define _PROXY_OVERRIDE_H

#if defined(SELFTEST)

#include "modules/prefs/prefsmanager/collections/pc_network.h"

class Selftest_ProxyOveride
{
private:

	class Individual_ProxyOverride
	{
	private:
		PrefsCollectionNetwork::stringpref name_pref;
		PrefsCollectionNetwork::integerpref enabled_pref;

		OpString proxy_name;
		BOOL proxy_enabled;
		
		OpString new_proxy_name;
		BOOL new_enable_proxy;

		BOOL configured;

	public:
		Individual_ProxyOverride(PrefsCollectionNetwork::stringpref a_name, PrefsCollectionNetwork::integerpref a_flag);
		~Individual_ProxyOverride(){ShutDown();}

		OP_STATUS Construct(const OpStringC &a_proxy, BOOL a_enabled);

		void ShutDown();
		OP_STATUS Activate();

		BOOL Active() const{return configured;}
	protected:
		OP_STATUS SetPrefs(const OpStringC &a_proxy, BOOL a_enabled);
	};

	Individual_ProxyOverride http;
	Individual_ProxyOverride https;
#ifdef FTP_SUPPORT
	Individual_ProxyOverride ftp;
#endif
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	Individual_ProxyOverride auto_proxy;
#endif
	Individual_ProxyOverride no_proxy;

public:

	Selftest_ProxyOveride();

	/** Activates Normal proxy overrides; do not use for autoproxy overrides */
	OP_STATUS Construct(const OpStringC &proxy_name, BOOL enable_proxy);
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
	/** Avtivates override for autproxy. Use instead of Construct() */
	OP_STATUS SetAutoProxy(const OpStringC8 &proxy_url, BOOL enable_proxy);
#endif
	void ShutDown();

	BOOL Active();
};


#endif // SELFTEST 
#endif // _PROXY_OVERRIDE_H
