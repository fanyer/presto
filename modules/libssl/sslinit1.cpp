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
#include "modules/libssl/options/sslopt.h"

#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslnull.h"
#include "modules/libssl/keyex/sslkersa.h"
#include "modules/libssl/keyex/sslkedh.h"

#include "modules/libssl/cipher_list.h"

SSL_CipherDescriptions *Cipher_CipherSetUp(unsigned int item)
{
	SSL_CipherDescriptions * temp;
	const Cipher_spec *spec;
	uint8 id1,id2;
	unsigned int i, j;
	
	temp = NULL;
	
	item++;
	for(j=i=0;i<CONST_ARRAY_SIZE(libssl, Cipher_ciphers);i++)
	{
		spec = &CONST_ARRAY_GLOBAL_NAME(libssl, Cipher_ciphers)[i];
		j++;
		if(j==item)
			break;
	}
	
	if(j != item)
		return NULL;
	
	if(i < CONST_ARRAY_SIZE(libssl, Cipher_ciphers))
	{
		spec = &CONST_ARRAY_GLOBAL_NAME(libssl, Cipher_ciphers)[i];
		id1 = spec->id[0];
		id2 = spec->id[1];
		
		{
			SSL_CipherMethod methods(spec->method, spec->hash, 
				spec->kea_alg, spec->sigalg,
#ifdef _SUPPORT_TLS_1_2
				spec->hash_tls12, spec->prf_fun,
#endif
				spec->KeySize
				);
			SSL_CipherNames names(
				spec->fullname,	spec->KEAname, spec->EncryptName, spec->HashName);
			
			{
				temp = new SSL_CipherDescriptions(
					SSL_CipherID(id1, id2),methods, names,
					spec->security_rating, spec->authentication, spec->low_reason);
			}
		}
	}
	
	return temp;
}

void SSL_Options::ConfigureSystemCiphers()
{
	AddSystemCiphers(Cipher_CipherSetUp,CONST_ARRAY_SIZE(libssl, Cipher_ciphers));
}  
#endif
