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

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/keyex/sslkersa.h"
#include "modules/libssl/keyex/sslkedh.h"

SSL_KeyExchange *CreateKeyExchangeL(SSL_KeyExchangeAlgorithm kea,
									SSL_SignatureAlgorithm signertype)
{
	switch(kea)
	{
	case SSL_RSA_KEA:
		return OP_NEW_L(SSL_RSA_KeyExchange, ());
#ifdef __SSL_WITH_DH_DSA__
	case SSL_Diffie_Hellman_KEA: 
		return OP_NEW_L(SSL_DH_KeyExchange, (FALSE, FALSE, signertype));
	case SSL_Ephemeral_Diffie_Hellman_KEA:
		return OP_NEW_L(SSL_DH_KeyExchange, (TRUE, FALSE, signertype));
	case SSL_Anonymous_Diffie_Hellman_KEA:
		return OP_NEW_L(SSL_DH_KeyExchange, (FALSE, TRUE, signertype));
#endif
	}
	LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	return NULL;
}

#endif
