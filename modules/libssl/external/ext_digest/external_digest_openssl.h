/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _EXTERNAL_DIGEST_H_
#define _EXTERNAL_DIGEST_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined EXTERNAL_DIGEST_API && defined _SSL_USE_OPENSSL_

#include "modules/libssl/external/openssl/sslhash.h"

class External_OpenSSL_Spec : public External_Digest_Handler
{
private:
	OpDigestParamInitCB parameter_init;
	OpDigestGetCredentialsCB   get_credentials;
	const EVP_MD	*md_spec;

public:

	External_OpenSSL_Spec();
	~External_OpenSSL_Spec();

	OP_STATUS Construct(
		const char *meth_name, 
		const EVP_MD	*md_spec,
		const char **domains_in, 
		int count, 
		OpDigestGetCredentialsCB   p_get_credentials,
		OpDigestParamInitCB p_parameter_init,
		int param_uid);

	const EVP_MD *GetMD() const{return md_spec;}

	virtual OP_STATUS	GetDigest(SSL_Hash *&digest);
	virtual OP_STATUS	GetCredentials(BOOL &retrieved_credentials, OpString &username, OpString &password, const void *parameters);
	OP_STATUS			PerformInitOperation(EVP_MD_CTX *digest, int operation, void *params);
};

class External_OpenSSL_Digest : public SSLEAY_Hash
{
	OpSmartPointerWithDelete<External_OpenSSL_Spec> spec;

public:

	/** constructor */
	External_OpenSSL_Digest(External_OpenSSL_Spec *_spec);
	
	/** copy constructor */
	External_OpenSSL_Digest(External_OpenSSL_Digest *old);

	/** Destructor */
	~External_OpenSSL_Digest();

	OP_STATUS PerformInitOperation(int operation, void *params);

	SSL_Hash *Fork();
};

#endif // EXTERNAL_DIGEST_API

#endif // _EXTERNAL_DIGEST_H_
