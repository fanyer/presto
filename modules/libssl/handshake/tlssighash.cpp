/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
*/
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#if !defined(ADS12) || defined(ADS12_TEMPLATING) // see end of streamtype.cpp

#include "modules/libssl/sslbase.h"
#include "modules/libssl/handshake/tlssighash.h"

TLS_SigAndHash::TLS_SigAndHash()
{
	InternalInit();
}

TLS_SigAndHash::TLS_SigAndHash(SSL_SignatureAlgorithm p_alg)
{
	InternalInit();
	SetValue(p_alg);
}

TLS_SigAndHash::TLS_SigAndHash(const TLS_SigAndHash &old)
: LoadAndWritableList()
{
	InternalInit();
	SetValue(old);
}

void TLS_SigAndHash::InternalInit()
{
	AddItem(hash_alg);
	AddItem(sign_alg);
	alg = SSL_Anonymous_sign;
	hash_alg = SSL_NoHash;
	sign_alg = SSL_Anonymous_sign;
}

void TLS_SigAndHash::SetValue(SSL_SignatureAlgorithm p_alg)
{
	alg = p_alg;
	hash_alg = SignAlgToHash(alg);
	sign_alg = SignAlgToBasicSignAlg(alg);
}

static const struct BaseSigAlgAndHashToSigAlg {
	SSL_SignatureAlgorithm base_alg;
	SSL_HashAlgorithmType hash_alg;
	SSL_SignatureAlgorithm actual_alg;
} basesig_hash_to_sig[] ={
	{SSL_RSA_sign, SSL_SHA_512, SSL_RSA_SHA_512},
	{SSL_RSA_sign, SSL_SHA_384, SSL_RSA_SHA_384},
	{SSL_RSA_sign, SSL_SHA_256, SSL_RSA_SHA_256},
	{SSL_RSA_sign, SSL_SHA_224, SSL_RSA_SHA_224},
	{SSL_RSA_sign, SSL_SHA,		SSL_RSA_SHA},
	{SSL_DSA_sign, SSL_SHA,		SSL_DSA_sign},
	{SSL_RSA_sign, SSL_MD5,		SSL_RSA_MD5}
};

void TLS_SigAndHash::PerformActionL(DataStream::DatastreamAction action, uint32 arg1, uint32 arg2)
{
	LoadAndWritableList::PerformActionL(action, arg1, arg2);
	if(action == DataStream::KReadAction && (int) arg2 == DataStream_SequenceBase::STRUCTURE_FINISHED)
	{
		SSL_SignatureAlgorithm in_base_alg = sign_alg;
		SSL_HashAlgorithmType in_hash_alg = hash_alg;
		int n = ARRAY_SIZE(basesig_hash_to_sig);
		const BaseSigAlgAndHashToSigAlg *mapper = basesig_hash_to_sig;

		alg = SSL_Illegal_sign;
		while(n>0)
		{
			if(mapper->base_alg == in_base_alg &&
				mapper->hash_alg == in_hash_alg)
			{
				alg = mapper->actual_alg;
				break;
			}
			n--;
			mapper++;
		}
	}
}

#ifdef _SUPPORT_TLS_1_2
void TLS_SignatureAlgList::Set(const SSL_SignatureAlgorithm *source, uint16 count)
{
	Resize(count);
	uint16 i;
	for(i=0; i<count;i++)
	{
		operator [](i) = source[i];
	}
}

void TLS_SignatureAlgList::GetCommon(TLS_SignatureAlgListBase &master, const SSL_SignatureAlgorithm *preferred, uint16 count)
{
	Resize(count);
	uint16 i,j,n,m=0;
	n = master.Count();
	for(i=0; i<count;i++)
	{
		SSL_SignatureAlgorithm alg= preferred[i];
		for(j=0; j<n; j++)
		{
			if(master[j] == alg)
				operator [](m++) = alg;
		}
	}
	Resize(m);
}
#endif

#endif // !ADS12 or ADS12_TEMPLATING

#endif // _NATIVE_SSL_SUPPORT_
