/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen 
** (split from external_digest_openssl.cpp by Christian Söderström)
**
*/

#include "core/pch.h"

#if defined _NATIVE_SSL_SUPPORT_ && defined EXTERNAL_DIGEST_API && defined _SSL_USE_OPENSSL_

#include "modules/libssl/external/openssl/eayhead.h"

#include "modules/libssl/external/ext_digest/external_digest_man.h"
#include "modules/libssl/external/ext_digest/register_digest_openssl.h"
#include "modules/libssl/external/ext_digest/external_digest_openssl.h"

int RegisterDigestAuthenticationOpenSSLMethod(
               const char *method_name,
               const EVP_MD *method_spec,
			   OpDigestGetCredentialsCB	get_credentials,
               OpDigestParamInitCB parameter_init,
               int   parameter_uid,
               const char ** domains,
               int domain_count,
			   OpDigestPutError *put_error
     )
{
	if(!method_name || !*method_name)
		return -2;

	OpStackAutoPtr<External_OpenSSL_Spec> spec( OP_NEW(External_OpenSSL_Spec, ()));
	if(spec.get() == NULL)
		return -10;

	OP_STATUS op_err = spec->Construct(method_name, method_spec, 
					domains, domain_count, 
					get_credentials, parameter_init, parameter_uid);

	if(OpExternalDigestStatus::IsSuccess(op_err))
		op_err = AddExternalDigestMethod(spec.release());

	if(OpStatus::IsError(op_err))
	{
		switch(op_err)
		{
		case OpExternalDigestStatus::ERR_INVALID_NAME:
			return -2;
		case OpExternalDigestStatus::ERR_INVALID_DOMAIN:
			return -4;
		case OpExternalDigestStatus::ERR_INVALID_SPEC:
			return -3;
		case OpExternalDigestStatus::ERR_ALGORITHM_EXIST:
			return -1;
		default:
			return -11;
		}
	}

	if(put_error)
		*put_error = ERR_put_error;

	return 0;
}

void UnRegisterDigestAuthenticationOpenSSLMethod(
               const char *method_name
	)
{
	RemoveExternalDigestMethod(method_name);
}


#endif // EXTERNAL_DIGEST_API && defined _SSL_USE_SSLEAY
