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

#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslnull.h"

SSL_Hash::~SSL_Hash()
{
}

#ifdef LIBSSL_VECTOR_EXTRACT_DIGESTS

void SSL_Hash::ExtractHash(SSL_varvector32 &target)
{
	target.Resize(Size());
	if(!target.ErrorRaisedFlag)
		ExtractHash(target.GetDirect());
	else
		target.Resize(0);
}

void SSL_Hash::CompleteHash(const byte *source,uint32 len, SSL_varvector32 &target)
{
	InitHash();
	CalculateHash(source,len);
	ExtractHash(target);
}

void SSL_Hash::CompleteHash(const SSL_varvector32 &source, SSL_varvector32 &target)
{
	CompleteHash(source.GetDirect(), source.GetLength(), target);
}

void SSL_Hash::CalculateHash(const SSL_varvector32 &source)
{
	CalculateHash(source.GetDirect(),source.GetLength());
} 
#endif

const byte *SSL_Hash::LoadSecret(const byte *source, uint32 len)
{
	return source;
}

void SSL_Hash::CalculateHash(byte pad, uint32 i)
{
	byte padarray[64];  /* ARRAY OK 2009-07-08 yngve */
	uint32 len;                  
	
	op_memset(padarray, pad, sizeof(padarray));
	while(i >0)
	{
		len = (i>64 ? 64 : i);
		CalculateHash(padarray,len);
		i -= len;
	}  
}

void SSL_Hash::CalculateHash(const char *source)
{
	if(source)
		CalculateHash((byte *) source, (uint32) op_strlen(source));
} 

#ifdef EXTERNAL_DIGEST_API
OP_STATUS SSL_Hash::PerformInitOperation(int operation, void *params)
{
	return OpStatus::OK;
}
#endif


#endif
