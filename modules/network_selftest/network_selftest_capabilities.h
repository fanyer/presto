/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef NETWORK_SELFTEST_CAPABILITIES_H
#define NETWORK_SELFTEST_CAPABILITIES_H

// The Network Selftst module is included
#define NETWORK_SELFTEST_CAP_MODULE

// Inherit NullSSLListener instead of OpSSLListener, to avoid cumbersome dependencies
#define NETWORK_SELFTEST_CAP_USE_NULLSSLLISTENER

// Includes SSL_WaitForAutoUpdate and Selftest_ProxyOveride
#define NETWORK_SELFTEST_CAP_PROXY_REPLACE_WAITUPDATE

// Can override autoproxy preference
#define NETWORK_SELFTEST_CAP_AUTOPROXY_REPLACE

// This module can load just a URL and leave it at that
#define NETWORK_SELFTEST_CAP_JUSTLOAD 

// This module includes the char/unichar url/filename simple test construct API
#define NETWORK_SELFTEST_CAP_NEW_SIMPLE_CONSTRUCT_1

// This module has test frameworks for the URL_DocumentLoader class
#define NETWORK_SELFTEST_CAP_DOCLOAD_TEST

#endif // NETWORK_SELFTEST_CAPABILITIES_H
