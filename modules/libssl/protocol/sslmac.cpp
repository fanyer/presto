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
#include "modules/libssl/protocol/sslmac.h"

#ifdef YNP_WORK
#include "modules/libssl/debug/tstdump2.h"
#define SSL_VERIFIER
#endif

SSL_Hash *SSL_MAC::Fork() const
{
	OP_ASSERT(0); // No longer active
	return NULL;
}

SSL_MAC::SSL_MAC()
{
	InternalInit(NULL);
}

SSL_MAC::SSL_MAC(const SSL_MAC *old)
{
	InternalInit(old);
}

void SSL_MAC::InternalInit(const SSL_MAC *old)
{
	ForwardToThis(secret, hash);
	
	if(old != NULL)
	{
		secret = old->secret;
		hash = old->hash;
		hash_alg = old->hash_alg;
		hash_size = old->hash_size;
	}
}

SSL_MAC::~SSL_MAC()
{
}

void SSL_MAC::SetHash(SSL_HashAlgorithmType nhash)
{
	hash.Set(nhash);
	hash_alg = hash->HashID();
	hash_size = hash->Size();
}

const byte *SSL_MAC::LoadSecret(const byte *secret_buffer,uint32 len)
{
	if(secret_buffer != NULL)
	{
		secret_buffer = secret.Set(secret_buffer,len);
	}
	return secret_buffer;
}

void SSL_MAC::InitHash()
{
	hash->InitHash();
}

const byte *SSL_MAC::CalculateHash(const byte *source,uint32 len)
{
	return hash->CalculateHash(source,len);
}

byte *SSL_MAC::ExtractHash(byte *target)
{
	return hash->ExtractHash(target);
}

BOOL SSL_MAC::Valid(SSL_Alert *msg) const
{
	return ((!Error(msg)) && secret.Valid(msg) &&  hash.Valid(msg)); 
}

#endif
