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


#if defined _NATIVE_SSL_SUPPORT_ && defined EXTERNAL_DIGEST_API && defined _SSL_USE_OPENSSL_ && defined SELFTEST
#include "modules/libssl/sslbase.h"

#include "modules/libssl/external/openssl/eayhead.h"

#include "modules/libssl/external/ext_digest/register_digest_openssl.h"

const char *External_testdomains[] = {
	"certo.opera.com",
	".intern.opera.com",
	"certo"
};

int External_test_GetCredentials(char**credential_name, char**credential_password, const void *params, OpSetStringCB set_string_cb)
{
	if(!set_string_cb)
		return 0;

	if(set_string_cb(credential_name, "tester")<0)
		return -1;
	if(set_string_cb(credential_password, "secret"))
		return -1;

	return 1;
}


int External_test_InitParameter(EVP_MD_CTX *ctx, int operation, const void *params)
{
	OP_ASSERT(ctx);
	OP_ASSERT(params);
	return 0;
}

void InitExternalTest()
{
	OpDigestPutError error_func=NULL;
	int ret = RegisterDigestAuthenticationOpenSSLMethod("md5", EVP_md5(), 
				External_test_GetCredentials, External_test_InitParameter, OP_PARAMS_TYPE_1,
				External_testdomains, ARRAY_SIZE(External_testdomains),
				&error_func);
	OP_ASSERT(ret == 0); 
	OP_ASSERT(error_func == ERR_put_error);
}
#endif
