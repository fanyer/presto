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
#include "modules/libssl/methods/sslmd5sha.h"

SSL_MD5_SHA_Hash::SSL_MD5_SHA_Hash()
: md5(SSL_MD5),sha(SSL_SHA)
{
	hash_alg = SSL_MD5_SHA;
	hash_size =md5->Size()+sha->Size();
}

SSL_MD5_SHA_Hash::SSL_MD5_SHA_Hash(const SSL_MD5_SHA_Hash *old)
{
	ForwardToThis(md5, sha);

	if(old)
	{
		md5=old->md5;
		sha=old->sha;
	}
	else
	{
		md5=SSL_MD5;
		sha=SSL_SHA;
	}

	hash_alg = SSL_MD5_SHA;
	hash_size =md5->Size()+sha->Size();
}

SSL_MD5_SHA_Hash::SSL_MD5_SHA_Hash(const SSL_MD5_SHA_Hash &old)
: SSL_Hash()
{
	ForwardToThis(md5, sha);

	md5=old.md5;
	sha=old.sha;

	hash_alg = SSL_MD5_SHA;
	hash_size =md5->Size()+sha->Size();
}

SSL_MD5_SHA_Hash::~SSL_MD5_SHA_Hash()
{
}

void SSL_MD5_SHA_Hash::InitHash()
{
	md5->InitHash();
	sha->InitHash();
}

const byte *SSL_MD5_SHA_Hash::CalculateHash(const byte *source,uint32 len)
{
	md5->CalculateHash(source,len);
	return sha->CalculateHash(source,len);
}

byte *SSL_MD5_SHA_Hash::ExtractHash(byte *target)
{
	if(!target)
		return NULL;

	target=md5->ExtractHash(target);
	return sha->ExtractHash(target);
}

BOOL SSL_MD5_SHA_Hash::Valid(SSL_Alert *msg) const
{
	if(Error(msg))
		return FALSE;

	if(md5->HashID()!= SSL_MD5 ||sha->HashID() != SSL_SHA)
	{
		if(msg)
			msg->RaiseAlert(SSL_Internal, SSL_InternalError);
		return FALSE;
	}

	return TRUE;
}

SSL_Hash *SSL_MD5_SHA_Hash::Fork() const
{
	return OP_NEW(SSL_MD5_SHA_Hash, (this));
}

#endif
