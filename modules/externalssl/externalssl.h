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

#ifndef EXTERNALSSL_H_
#define EXTERNALSSL_H_

#if defined(_EXTERNAL_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/pi/network/OpSocket.h"

struct External_Cipher_spec;

class External_SSL_Options
{
private:
	BOOL TLS_enabled;
	
	cipherentry_st *Ciphers;
	UINT32	cipher_count;

private:

	static const External_Cipher_spec *LocateCipherSpec(UINT8 *cipher_id);
	const External_Cipher_spec *InternalLocateCipherSpec(UINT8 *cipher_id);

public:

	External_SSL_Options();
	~External_SSL_Options();

	const uni_char *GetCipher(int item, UINT8 *cipher_id, BOOL &presently_enabled);
	static const uni_char *GetCipher(UINT8 *cipher_id, int &security_level);
	
	void SetEnableTLS(BOOL enabled);
	BOOL GetEnableTLS() const{return TLS_enabled;};

	const cipherentry_st *GetCipherArray(UINT32 &count){count = cipher_count; return Ciphers;};
};

#ifdef ERROR_PAGE_SUPPORT
Str::LocaleString SSLErrorToMessage(int aError);
#endif 

#endif // _EXTERNAL_SSL_SUPPORT_ || _CERTICOM_SSL_SUPPORT_
#endif // EXTERNALSSL_H_
