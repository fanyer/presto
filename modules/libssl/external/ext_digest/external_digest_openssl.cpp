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

#include "core/pch.h"

#if defined _NATIVE_SSL_SUPPORT_ && defined EXTERNAL_DIGEST_API && defined _SSL_USE_OPENSSL_

#include "modules/libssl/external/openssl/eayhead.h"

#include "modules/libssl/external/ext_digest/external_digest_man.h"
#include "modules/libssl/external/ext_digest/register_digest_openssl.h"
#include "modules/libssl/external/ext_digest/external_digest_openssl.h"

External_OpenSSL_Spec::External_OpenSSL_Spec()
{
	md_spec = NULL;
	parameter_init = NULL;
	get_credentials = NULL;
}

External_OpenSSL_Spec::~External_OpenSSL_Spec()
{
	md_spec = NULL;
	parameter_init = NULL;
	get_credentials = NULL;
}

OP_STATUS External_OpenSSL_Spec::Construct(
					const char *meth_name, 
					const EVP_MD	*p_md_spec,
					const char **domains_in, 
					int count, 
					OpDigestGetCredentialsCB   p_get_credentials,
					OpDigestParamInitCB p_parameter_init,
					int param_uid)
{
	if(p_md_spec == NULL ||
		p_md_spec->md_size == 0 || 
		p_md_spec->init == 0 || 
		p_md_spec->update == 0 || 
		p_md_spec->final == 0)
		return OpExternalDigestStatus::ERR_INVALID_SPEC;

	RETURN_IF_ERROR(External_Digest_Handler::Construct(meth_name, domains_in, count, param_uid));

	md_spec = p_md_spec;
	parameter_init = p_parameter_init;
	get_credentials = p_get_credentials;

	return OpExternalDigestStatus::OK;
}

OP_STATUS	External_OpenSSL_Spec::GetDigest(SSL_Hash *&digest)
{
	if(digest != NULL)
		OP_DELETE(digest);

	digest = OP_NEW(External_OpenSSL_Digest, (this));

	if(digest == NULL)
		return OpExternalDigestStatus::ERR_NO_MEMORY;

	return OpExternalDigestStatus::OK;
}

int External_OpenSSL_CopyString(char **target, const char *source)
{
	if(target == NULL)
		return -1;

	if(OpExternalDigestStatus::IsError(SetStr(*target, source)))
		return -2;

	return 0;
}

OP_STATUS	External_OpenSSL_Spec::GetCredentials(BOOL &retrieved_credentials, OpString &username, OpString &password, const void *parameter)
{
	char *t_username = NULL;
	char *t_password = NULL;

	retrieved_credentials = FALSE;
	username.Empty();
	password.Empty();

	if(!get_credentials)
		return OpExternalDigestStatus::OK;

	int ret = get_credentials(&t_username, &t_password, parameter, External_OpenSSL_CopyString);

	if(ret == 1)
	{
		RETURN_IF_ERROR(username.SetFromUTF8(t_username));
		RETURN_IF_ERROR(password.SetFromUTF8(t_password));
		retrieved_credentials = TRUE;
	}

	if(t_username)
		OPERA_cleanse_heap(t_username, op_strlen(t_username));
	if(t_password)
		OPERA_cleanse_heap(t_password, op_strlen(t_password));

	SetStr(t_username, NULL);
	SetStr(t_password, NULL);

	if(ret < 0)
	{
		return (ret == -1 ? OpExternalDigestStatus::ERR_NO_MEMORY : OpExternalDigestStatus::ERR);
	}

	return OpExternalDigestStatus::OK;
}

OP_STATUS	External_OpenSSL_Spec::PerformInitOperation(EVP_MD_CTX *digest, int operation, void *params)
{
	int ret = 0;
	
	if(parameter_init)
		parameter_init(digest, operation, params);

	return (ret == 0 ? OpStatus::OK : OpStatus::ERR);
}

External_OpenSSL_Digest::External_OpenSSL_Digest(External_OpenSSL_Spec *_spec)
: SSLEAY_Hash(_spec ? _spec->GetMD() : NULL)
{
	spec = _spec;
}

External_OpenSSL_Digest::External_OpenSSL_Digest(External_OpenSSL_Digest *old)
: SSLEAY_Hash(old)
{
	spec = (old ? (External_OpenSSL_Spec*)old->spec : NULL);
}

External_OpenSSL_Digest::~External_OpenSSL_Digest()
{
	spec = NULL;
}

OP_STATUS External_OpenSSL_Digest::PerformInitOperation(int operation, void *params)
{
	return spec->PerformInitOperation(&md_status, operation, params);
}


SSL_Hash *External_OpenSSL_Digest::Fork()
{
	return OP_NEW(External_OpenSSL_Digest, (this));
}


#endif // EXTERNAL_DIGEST_API 

