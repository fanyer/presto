/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef REVOKE_LIST_H
#define REVOKE_LIST_H

class SSL_Cert_Revoked;
class SSL_CertificateHandler;
class SSL_varvector16;
class SSL_varvector24;
typedef SSL_varvector24 SSL_ASN1Cert;
class SSL_ASN1Cert_list;

class SSL_Revoke_List: public AutoDeleteList<SSL_Cert_Revoked>
{
public:
	/** Insert cert into revoked list */
	OP_STATUS AddRevoked(SSL_varvector16 &fingerprint, SSL_ASN1Cert &cert);

	/** TRUE if revoked cert is in list */
	BOOL CheckForRevokedCert(unsigned int i, SSL_CertificateHandler *hndlr, SSL_ASN1Cert_list &certs);

private:
	/** Return revoked item for the provided cert, if it exists */
	SSL_Cert_Revoked *LocateRevokedCert(SSL_varvector16 &fingerprint, SSL_ASN1Cert &cert);
};

#endif // REVOKE_LIST_H
