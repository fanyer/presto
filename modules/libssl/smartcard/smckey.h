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

#ifndef SSLSMCKEYCERT_H
#define SSLSMCKEYCERT_H

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_EXTERNAL_KEYMANAGERS_
#include "modules/libssl/handshake/asn1certlist.h"
#include "modules/libssl/methods/sslpubkey.h"

class SSL_PublicKeyCipher;


class SSL_Certificate_And_Key_List : public SSL_Error_Status
{
private:
	OpString name;
	OpString label;
	OpString serialnumber;
	SSL_ASN1Cert_list der_certificate;
	SSL_CertificateHandler *certificate;
	SSL_PublicKeyCipher *key;

public:
	/** Takes ownership of pkey object */
	SSL_Certificate_And_Key_List(const OpStringC &lbl, const OpStringC &sn, SSL_ASN1Cert_list &cert, SSL_PublicKeyCipher *pkey);

	virtual ~SSL_Certificate_And_Key_List();

	/** Return reference to the Common Name in the card's certificate */
	OpStringC &Name(){return name;}

	/** Return reference to label of card */
	OpStringC &Label(){return label;}

	/** Return reference to serialnumber of card */
	OpStringC &SerialNumber(){return serialnumber;}

	/** returns reference to certificate list */
	SSL_ASN1Cert_list &DERCertificate(){return der_certificate;}

	/** returns pointer to certificate list */
	SSL_CertificateHandler *Certificate();

	/** Forks a new key with the same properties as the old */
	SSL_PublicKeyCipher *GetKey(){return (key ? key->Fork() : NULL);}

	/** read only access to the key */
	const SSL_PublicKeyCipher *AccessKey(){return key;}
};
#endif

#endif /* SSLSMCKEYCERT_H */
