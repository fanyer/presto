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

#ifndef _SSL_KEDH_H_
#define _SSL_KEDH_H_

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/keyex/sslkeyex.h"

class SSL_DH_KeyExchange : public SSL_KeyExchange
{
protected:
    struct dh_kea_flags{
		BOOL ephermal,anonymous;
		SSL_SignatureAlgorithm signertype;
    } flags; 
    
    SSL_secure_varvector16 Premaster;
    SSL_PublicKeyCipher *DH_premasterkey;
	
private:
	virtual int SetupServerCertificateCiphers();
	virtual int SetupServerKeys(SSL_Server_Key_Exchange_st *serverkeys);
	void InitFlags();
	
public:
	SSL_DH_KeyExchange(BOOL ephermal,BOOL anonymous, 
		SSL_SignatureAlgorithm signertype);
	SSL_DH_KeyExchange(const SSL_DH_KeyExchange *);
	virtual ~SSL_DH_KeyExchange();
	
	virtual void PreparePremaster();
	virtual SSL_KeyExchangeAlgorithm GetKeyExhangeAlgorithm();

	virtual BOOL GetIsAnonymous();
};

#endif
#endif
