/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#ifdef _SSL_USE_OPENSSL_

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/libssl/external/openssl/sslhash.h"
#include "modules/libssl/methods/sslnull.h"
#include "modules/libssl/methods/sslmd5sha.h"

#include "modules/url/tools/arrays.h"

typedef const EVP_MD* (*EVP_MD_CB)();

struct SSL_Digest_and_NID
{
	SSL_HashAlgorithmType digest_type;
	int hmac;
	SSL_HashAlgorithmType hmac_digest_type;
	int nid;
	EVP_MD_CB func;
};

#define MD_ENTRY(typ, hmac_typ, is_hmac, digest_name) \
	CONST_ENTRY_START(digest_type, typ) \
	CONST_ENTRY_SINGLE(hmac, is_hmac)\
	CONST_ENTRY_SINGLE(hmac_digest_type, hmac_typ)\
	CONST_ENTRY_SINGLE(nid, NID_##digest_name)\
	CONST_ENTRY_SINGLE( func, EVP_##digest_name )\
	CONST_ENTRY_END

// param2 "name" will be expanded to NID_name and EVP_name
PREFIX_CONST_ARRAY(static, SSL_Digest_and_NID, SSL_Digest_map, libssl)
	MD_ENTRY(SSL_MD5,		SSL_HMAC_MD5, FALSE, md5)
	MD_ENTRY(SSL_SHA,		SSL_HMAC_SHA, FALSE, sha1)
	MD_ENTRY(SSL_SHA_256,		SSL_HMAC_SHA_256, FALSE, sha256)
	MD_ENTRY(SSL_SHA_384,		SSL_HMAC_SHA_384, FALSE, sha384)
	MD_ENTRY(SSL_SHA_512,		SSL_HMAC_SHA_512, FALSE, sha512)
	MD_ENTRY(SSL_HMAC_SHA,	SSL_HMAC_SHA, TRUE, sha1)
	MD_ENTRY(SSL_HMAC_SHA_256,	SSL_HMAC_SHA_256, TRUE, sha256)
	MD_ENTRY(SSL_HMAC_SHA_384,	SSL_HMAC_SHA_384, TRUE, sha384)
	MD_ENTRY(SSL_HMAC_SHA_512,	SSL_HMAC_SHA_512, TRUE, sha512)
	MD_ENTRY(SSL_HMAC_MD5,	SSL_HMAC_MD5, TRUE, md5)
	MD_ENTRY(SSL_HMAC_SHA_256,	SSL_HMAC_SHA_256, TRUE, sha256)
	MD_ENTRY(SSL_SHA_224,		SSL_HMAC_SHA_224, FALSE, sha224)
	MD_ENTRY(SSL_HMAC_SHA_224,	SSL_HMAC_SHA_224, TRUE, sha224)
#ifndef OPENSSL_NO_RIPEMD
	MD_ENTRY(SSL_RIPEMD160,		SSL_HMAC_RIPEMD160, FALSE, ripemd160)
	MD_ENTRY(SSL_HMAC_RIPEMD160, SSL_HMAC_RIPEMD160, TRUE, ripemd160)
#endif
CONST_END(SSL_Digest_map)

static const SSL_Digest_and_NID *GetSpecFromAlgorithm(SSL_HashAlgorithmType digest)
{
	size_t i;
	for(i = 0; i<CONST_ARRAY_SIZE(libssl, SSL_Digest_map); i++)
	{
		if(g_SSL_Digest_map[i].digest_type == digest)
		{
			return &g_SSL_Digest_map[i];
		}
	}
	return NULL;
}

const EVP_MD *GetMD_From_Algorithm(SSL_HashAlgorithmType digest)
{
	const SSL_Digest_and_NID *spec = GetSpecFromAlgorithm(digest);

	return (spec ? spec->func() : NULL);
}

SSL_Hash *SSL_API::CreateMessageDigest(SSL_HashAlgorithmType digest, OP_STATUS &op_err)
{
	op_err = OpStatus::OK;
	OpAutoPtr<SSL_Hash> hasher(NULL);

	switch(digest)
	{
	case SSL_NoHash:
		hasher.reset(OP_NEW(SSL_Null_Hash, ()));
		if(hasher.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
		}
		break;
	case SSL_MD5_SHA:
		hasher.reset(OP_NEW(SSL_MD5_SHA_Hash, ()));
		if(hasher.get() == NULL)
		{
			op_err = OpStatus::ERR_NO_MEMORY;
		}
		break;

	default:
		{
			const SSL_Digest_and_NID *alg_spec = GetSpecFromAlgorithm(digest);

			if(alg_spec == NULL)
			{
#ifdef EXTERNAL_DIGEST_API
				extern SSL_Hash *GetExternalDigestMethod(SSL_HashAlgorithmType digest, OP_STATUS &op_err);

				return GetExternalDigestMethod(digest, op_err);
#else
				op_err = OpStatus::ERR_OUT_OF_RANGE;
				return NULL;
#endif
			}

			hasher.reset(alg_spec->hmac ? (SSL_Hash *) OP_NEW(SSLEAY_HMAC_Hash, (alg_spec)) : (SSL_Hash *) OP_NEW(SSLEAY_Hash, (alg_spec)));

			if(hasher.get() == NULL)
			{
				op_err = OpStatus::ERR_NO_MEMORY;
			}
			else if(hasher->Error())
			{
				op_err = hasher->GetOPStatus();
				hasher.reset();
			}
			else
				hasher->InitHash();

		}
	}

	return hasher.release();
}



SSLEAY_Hash_Base::SSLEAY_Hash_Base(const SSL_Digest_and_NID *spec)
: alg_spec(spec), md_spec(NULL)
{
	OP_ASSERT(alg_spec);
	if(alg_spec)
	{
		md_spec = spec->func();
		hash_alg = alg_spec->digest_type;
		hash_size =  md_spec->md_size;
	}
}

SSLEAY_Hash_Base::~SSLEAY_Hash_Base()
{
	alg_spec = NULL;
	md_spec = NULL;
}

BOOL SSLEAY_Hash_Base::Valid(SSL_Alert *msg) const
{
	if(Error(msg))
		return FALSE;
	if (md_spec == NULL)
	{
		if(msg != NULL)
			msg->Set(SSL_Internal,SSL_InternalError);
		return FALSE;
	}
	
	return TRUE;
}

void SSLEAY_CheckError(SSL_Error_Status *target);

void SSLEAY_Hash_Base::CheckError()
{
	SSLEAY_CheckError(this);
}

/*
static SSLEAY_Hash md5(EVP_md5());
static SSLEAY_Hash sha(EVP_sha1());

  SSL_Hash *SSL_Master_Hash_md5 = &md5;
  SSL_Hash *SSL_Master_Hash_sha = &sha;
*/

SSLEAY_Hash::SSLEAY_Hash(const SSL_Digest_and_NID *spec)
: SSLEAY_Hash_Base(spec)
{
	EVP_MD_CTX_init(&md_status);
	InitHash();
}

/* Unref YNP
SSLEAY_Hash::SSLEAY_Hash(const SSLEAY_Hash &old)
: md_spec(old.alg_spec)
{
if(md_spec != NULL){
memset(&md_status,0,sizeof(md_status)); 
} 
// InitHash();
}
*/

SSLEAY_Hash::SSLEAY_Hash(const SSLEAY_Hash *old)
: SSLEAY_Hash_Base(old ? old->alg_spec : NULL)
{
	OP_ASSERT(old);
	EVP_MD_CTX_init(&md_status);
	if(old)
	{
		ERR_clear_error();
		if (!EVP_MD_CTX_copy(&md_status, &old->md_status))
			CheckError();
	}
	else
		InitHash();
}

SSLEAY_Hash::~SSLEAY_Hash()
{
	EVP_MD_CTX_cleanup(&md_status); 
}

void SSLEAY_Hash::InitHash()
{
	if(md_spec != NULL)
	{
		ERR_clear_error();
		EVP_MD_CTX_cleanup(&md_status); 
		EVP_DigestInit(&md_status,md_spec);
		CheckError();
	}
}

const byte *SSLEAY_Hash::CalculateHash(const byte *source,uint32 len)
{
#ifdef TST_DUMP
	PrintfTofile("sslhash.txt", "SSLEAY_Hash::CalculateHash %p", this);
	DumpTofile(source, len, "SSLEAY_Hash::CalculateHash","sslhash.txt"); 
#endif
	ERR_clear_error();
	EVP_DigestUpdate(&md_status, source,(size_t) len); 
	CheckError();
	return source+len;
}

byte *SSLEAY_Hash::ExtractHash(byte *target)
{
	EVP_MD_CTX temp;
	
	ERR_clear_error();
	if (EVP_MD_CTX_copy(&temp, &md_status))
		EVP_DigestFinal(&temp,target,NULL);
	CheckError();

#ifdef TST_DUMP
	PrintfTofile("sslhash.txt", "SSLEAY_Hash::ExtractHash %p", this);
	DumpTofile(target, md_spec->md_size, "SSLEAY_Hash::ExtractHash","sslhash.txt"); 
#endif
	return target + md_spec->md_size;
}
#endif

SSL_Hash *SSLEAY_Hash::Fork() const
{
	SSLEAY_Hash *temp;
	
	temp = OP_NEW(SSLEAY_Hash, (this));

	return temp;
}


// HMAC implementation

int HMAC_CTX_copy(HMAC_CTX *ctx, const HMAC_CTX *ctx_old)
{
	ctx->md = ctx_old->md;

	if (!EVP_MD_CTX_copy(&ctx->i_ctx, &ctx_old->i_ctx))
		return 0;
	if (!EVP_MD_CTX_copy(&ctx->o_ctx, &ctx_old->o_ctx))
		return 0;
	if (!EVP_MD_CTX_copy(&ctx->md_ctx, &ctx_old->md_ctx))
		return 0;

	ctx->key_length = ctx_old->key_length;
	op_memcpy(ctx->key, ctx_old->key, ctx_old->key_length);
	return 1;
}

SSLEAY_HMAC_Hash::SSLEAY_HMAC_Hash(const SSL_Digest_and_NID *spec)
: SSLEAY_Hash_Base(spec)
{
	HMAC_CTX_init(&md_status);
	md_status.key_length = 0;
	InitHash();
}


SSLEAY_HMAC_Hash::SSLEAY_HMAC_Hash(const SSLEAY_HMAC_Hash *old)
: SSLEAY_Hash_Base(old ? old->alg_spec : NULL)
{
	OP_ASSERT(old);

	HMAC_CTX_init(&md_status);
	md_status.key_length = 0;
	if(old)
	{
		ERR_clear_error();
		if (!HMAC_CTX_copy(&md_status, &old->md_status))
			CheckError();
	}
	else
		InitHash();
}

SSLEAY_HMAC_Hash::~SSLEAY_HMAC_Hash()
{
	HMAC_CTX_cleanup(&md_status); 
}

void SSLEAY_HMAC_Hash::InitHash()
{
	if(md_spec != NULL)
	{
		ERR_clear_error();
		//HMAC_CTX_cleanup(&md_status); 
		HMAC_Init_ex(&md_status,NULL, 0, md_spec, NULL);
		CheckError();
	}
}

const byte *SSLEAY_HMAC_Hash::LoadSecret(const byte *source, uint32 len)
{
	if(md_spec)
	{
		if(source == NULL)
		{
			source = (const byte *) g_memory_manager->GetTempBuf2k();
			len = 0;
		}
		HMAC_Init_ex(&md_status, source, len, md_spec, NULL);
		if(source)
			source += len;
	}

	return source;
}

const byte *SSLEAY_HMAC_Hash::CalculateHash(const byte *source,uint32 len)
{
	if(source && len)
	{
		ERR_clear_error();
		HMAC_Update(&md_status, source,(size_t) len); 
		CheckError();
		source += len;
	}
	return source;
}

byte *SSLEAY_HMAC_Hash::ExtractHash(byte *target)
{
	HMAC_CTX temp;
	
	ERR_clear_error();
	HMAC_CTX_init(&temp);
	if (HMAC_CTX_copy(&temp, &md_status))
		HMAC_Final(&temp,target,NULL);
	HMAC_CTX_cleanup(&temp); 
	CheckError();
	return target + md_spec->md_size;
}

SSL_Hash *SSLEAY_HMAC_Hash::Fork() const
{
	SSLEAY_HMAC_Hash *temp;
	
	temp = OP_NEW(SSLEAY_HMAC_Hash, (this));

	return temp;
}
#endif
