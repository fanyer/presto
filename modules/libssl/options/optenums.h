/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef SSLOPTENUMS_H
#define SSLOPTENUMS_H

#ifdef _NATIVE_SSL_SUPPORT_

enum Cert_Update_enum {Cert_Not_Updated, Cert_Updated, Cert_Inserted, Cert_Deleted};

enum CertAcceptMode {
	CertAccept_Site,
	CertAccept_Applet
};

class InstallerStatus : public OpStatus
{
public:
	enum installer_errs
	{
		ERR_PASSWORD_NEEDED = -2048,
		ERR_NO_PRIVATE_KEY = -2049,
		ERR_WRONG_PRIVATE_KEY = -2050,
		ERR_INSTALL_FAILED = -2051,
		ERR_UNSUPPORTED_KEY_ENCRYPTION = -2052,
		ERR_CERTIFICATE_ALREADY_PRESENT = -2053,

		INSTALL_FINISHED = 2048,
		KEYGEN_WORKING = 2049,
		KEYGEN_FINISHED = 2050,
		EXPORT_FINISHED = 2051,
		VERIFYING_CERT
	};
};

enum SSL_SessionCipherHandle {SSL_NoAddSessionCipher, SSL_AppendSessionCipher, SSL_SessionCipherInFront};
enum SSL_SessionCompressHandle {SSL_NoAddSessionCompress, SSL_AppendSessionCompress, SSL_SessionCompressInFront};

#define SSL_ASK_PASSWD_ONCE 0
#define SSL_ASK_PASSWD_EVERY_TIME 1
#define SSL_ASK_PASSWD_AFTER_TIME 2

enum SSL_CertificateStore {
	SSL_CA_Store,
	SSL_ClientStore,
	SSL_ClientOrCA_Store,

	SSL_IntermediateCAStore,
	SSL_TrustedSites,
	SSL_UntrustedSites,
	SSL_Unknown_Store
};

#define SSL_LOAD_CLIENT_STORE		0x01
#define SSL_LOAD_INTERMEDIATE_CA_STORE			0x02
#define SSL_LOAD_CA_STORE			0x04
#define SSL_LOAD_TRUSTED_STORE		0x08
#define SSL_LOAD_UNTRUSTED_STORE	0x10

#define SSL_LOAD_ALL_STORES		(SSL_LOAD_CA_STORE | SSL_LOAD_CLIENT_STORE | SSL_LOAD_TRUSTED_STORE | SSL_LOAD_UNTRUSTED_STORE | SSL_LOAD_INTERMEDIATE_CA_STORE)

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLOPTENUMS_H */
