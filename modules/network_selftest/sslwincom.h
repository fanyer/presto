/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2004-2008 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve N. Pettersen
 */
#ifndef _BASIC_SSL_LISTENER_H
#define _BASIC_SSL_LISTENER_H

#ifdef SELFTEST

#include "modules/network_selftest/urldoctestman.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/windowcommander/src/WindowCommanderManager.h"

class BasicSSLWindowListener
	: public NullSSLListener
{
private:
	URL_DocSelfTest_Manager *test_manager;
	OpSSLListener *fallback;

public:
	BasicSSLWindowListener (URL_DocSelfTest_Manager *manager, OpSSLListener *fallback)
		: test_manager(manager), fallback(fallback) {}

	virtual ~BasicSSLWindowListener();
	OpSSLListener *GetFallback(){return fallback;}

	virtual void OnCertificateBrowsingNeeded(OpWindowCommander* wic, SSLCertificateContext* context, SSLCertificateReason reason, SSLCertificateOption options);
	virtual void OnSecurityPasswordNeeded(OpWindowCommander* wic, SSLSecurityPasswordCallback* callback);


	void ReportFailure(URL &url, const char *format, ...);
};

#endif // SELFTEST
#endif // _BASIC_SSL_LISTENER_H
