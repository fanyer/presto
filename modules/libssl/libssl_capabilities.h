/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef LIBSSL_CAPABILITIES_H
#define LIBSSL_CAPABILITIES_H

#define SSL_CAP_DIFFIE_3_FINAL_3

// This modules does not have the global variable factories for encryption, certificates abd digest methods, separate create functions are used istead
#define SSL_CAP_NO_GLOBAL_FACTORIES

// This version has all previously global objects located in the g_opera object
#define SSL_CAP_GLOBAL_OBJECTS_IN_GOPERA

// This module uses the platform's UINT64 type when available
#define SSL_CAP_PLATFORM_UINT64

// This module can provide URL information about the certificate
#define LIBSSL_CAP_DISPCTX_URL

// This module contains the g_ssl_api object, and also indicates that all global libssl variables now have a g_ prefix
#define LIBSSL_CAP_SSL_API

// This module sends the MSG_SSL_RESTART_CONNECTION message, which instructs the parent protocol to restart after tearing the previous connection
#define LIBSSL_CAP_HAS_RESTART_MESSAGE

// This module knows about the network selftest utility module
#define LIBSSL_CAP_KNOW_NETWORK_SELFTEST

// This module have the Standalone certificate verification functionality
#define LIBSSL_CAP_STANDALONE_CERT_VERIFICATION

// This module have the Servername::FreeUnusedSSLresources function
#define LIBSSL_CAP_FREE_UNUSED_SESSION

// This module can remember certificates accepted for Java applets
#define LIBSSL_CAP_APPLET_CERT_ACCEPT_STORE

// SSL_Certificate_DisplayContext implements OpSSLListener::SSLCertificateContext::GetServerName(OpString*)
#define LIBSSL_CAP_WIC_GETSERVERNAME

// This module have the SSL_CertificateVerifier fully capable of handling multiple purposes of verification
#define LIBSSL_CAP_CERTVERIFY_MANAGER

#endif // LIBSSL_CAPABILITIES_H
