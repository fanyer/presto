/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"
#include "modules/libssl/handshake/asn1certlist.h"
#include "modules/libssl/certs/certhandler.h"

OP_STATUS SSL_Revoke_List::AddRevoked(SSL_varvector16 &fingerprint, SSL_ASN1Cert &cert)
{
	if(LocateRevokedCert(fingerprint, cert))
		return OpStatus::OK;

	SSL_Cert_Revoked *revoked_item = OP_NEW(SSL_Cert_Revoked, ());
	RETURN_OOM_IF_NULL(revoked_item);

	revoked_item->cert = cert;
	revoked_item->fingerprint = fingerprint;
	if(revoked_item->cert.Error() || revoked_item->fingerprint.Error())
	{
		OP_DELETE(revoked_item);
		return OpStatus::ERR_NO_MEMORY;
	}

	revoked_item->Into(this);
	return OpStatus::OK;
}

SSL_Cert_Revoked *SSL_Revoke_List::LocateRevokedCert(SSL_varvector16 &fingerprint, SSL_ASN1Cert &cert)
{
	SSL_Cert_Revoked *revoke_item = g_revoked_certificates.First();

	while(revoke_item)
	{
		if(revoke_item->fingerprint == fingerprint && revoke_item->cert == cert)
		{
			return revoke_item;
		}
		revoke_item = revoke_item->Suc();
	}
	return NULL;
}

BOOL SSL_Revoke_List::CheckForRevokedCert(unsigned int i, SSL_CertificateHandler *hndlr, SSL_ASN1Cert_list &certs)
{
	OP_ASSERT(hndlr != NULL);
	OP_ASSERT(hndlr->CertificateCount() > i);
	OP_ASSERT(certs.Count() > i);

	SSL_varvector16 fingerprint;
	hndlr->GetFingerPrint(i, fingerprint);

	return (LocateRevokedCert(fingerprint, certs[i]) != NULL ? TRUE : FALSE);
}

#endif // _NATIVE_SSL_SUPPORT_
