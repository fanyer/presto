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

#ifndef SSLKERSA_H
#define SSLKERSA_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/keyex/sslkeyex.h"

class SSL_RSA_KeyExchange : public SSL_KeyExchange{
protected:
    SSL_PublicKeyCipher *RSA_premasterkey;
	
public:
	virtual int SetupServerCertificateCiphers();
	void InitFlags();
	
public:
	SSL_RSA_KeyExchange();
	virtual ~SSL_RSA_KeyExchange();
	

	virtual void PreparePremaster();
	virtual void CreatePremasterSecretToSend(SSL_secure_varvector16 &buffer);

	virtual SSL_KeyExchangeAlgorithm GetKeyExhangeAlgorithm() { return SSL_RSA_KEA; }
};

#endif
#endif /* SSLKERSA_H */
