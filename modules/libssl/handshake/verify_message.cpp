/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/pi/OpSystemInfo.h"

#include "modules/libssl/sslbase.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/base/sslciphspec.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/handshake/verify_message.h"

#include "modules/util/cleanse.h"

void SSL_CertificateVerify_st::SetUpMessageL(SSL_ConnectionState *pending)
{
	SSL_varvector32 hash_data;
	SSL_CipherSpec *cipher;
	
	if(pending == NULL || pending->version_specific == NULL || (cipher = pending->write.cipher) == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return;
	};

#ifdef _SUPPORT_TLS_1_2
	if(pending->version.Compare(3,3) >= 0)
		SetTLS12_Mode();
#endif

	pending->version_specific->GetHandshakeHash(pending->sigalg, hash_data);
	
	SetSignatureAlgorithm(pending->sigalg);  
	SetCipher(cipher->SignCipherMethod);
	
	SignHash(hash_data);
}
#endif // relevant support
