/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslphash.h"
#include "modules/libssl/handshake/sslsig.h"

#ifdef _DEBUG 
//#define TST_DUMP

#include "modules/libssl/debug/tstdump2.h"
#endif // _DEBUG

void SSL_Signature::Init()
{
	AddItem(sign_alg);
	AddItem(signature);
	sign_alg.SetEnableRecord(FALSE);
	sig_hasher.ForwardTo(this);
	sign_alg = SSL_Anonymous_sign;
	sig_len = 0;
	sigcipher = NULL;
	verify_ASN1 = FALSE;
}

SSL_Signature::SSL_Signature()
{
	Init();
} 

SSL_Signature::SSL_Signature(SSL_PublicKeyCipher *cipher)
{
	Init();
	sigcipher = cipher;
}

SSL_Signature::~SSL_Signature()
{ 
// Todo: leaking memory
}

uint32 SSL_Signature::Size() const
{
	uint32 size = 0, basesize = 0;

	switch(sign_alg)
	{
    case SSL_RSA_MD5_SHA_sign : 
		basesize = sizeof(md5_sha_rsa_sign.md5_sha_hash);
		break;
    case SSL_DSA_sign : 
		basesize =  sizeof(dsa_sign.sha_hash);
		break;
	case SSL_RSA_SHA: 
		basesize =  sizeof(sha_sign.sha_hash);
		break;
	case SSL_RSA_SHA_224: 
		basesize =  sizeof(sha_224_sign.sha_hash);
		break;
	case SSL_RSA_SHA_256: 
		basesize =  sizeof(sha_256_sign.sha_hash);
		break;
	case SSL_RSA_SHA_384: 
		basesize =  sizeof(sha_384_sign.sha_hash);
		break;
	case SSL_RSA_SHA_512: 
		basesize =  sizeof(sha_512_sign.sha_hash);
		break;
    default: break; // nothing to do for illegal or anon
	}
	if(basesize)
	{
		if (sigcipher != NULL)
			basesize = sigcipher->Calc_BufferSize(basesize);

		size = 2 + basesize;
	}

	return size;  
}

OP_STATUS SSL_Signature::PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target)
{
	if(action == DataStream::KWriteRecord && !sigcipher)
	{
		//SSL_SignatureAlgorithm sig_bas = SignAlgToBasicSignAlg(sign_alg);
		if(sign_alg.GetEnabledRecord())
			sign_alg.WriteRecordL(src_target);
		if(sig_len)
		{
			WriteIntegerL(sig_len, SSL_SIZE_UINT_16, TRUE, FALSE, src_target);
			src_target->WriteDataL(dummy.dummy_hash,sig_len);
		}
		return OpRecStatus::FINISHED;
	}

	return SSL_Handshake_Message_Base::PerformStreamActionL(action, src_target);
}


BOOL SSL_Signature::Valid(SSL_Alert *msg) const
{
	return (SSL_Handshake_Message_Base::Valid(msg) && /*signature.Valid(msg) && */
			sig_hasher.Valid(msg));
}

BOOL SSL_Signature::Verify(SSL_Signature &reference, SSL_Alert *msg)
{
	if (!Valid(msg) || !reference.Valid(msg) || sign_alg != reference.sign_alg)
		return FALSE;

	if(sign_alg != SSL_Anonymous_sign)
	{
		if((sigcipher != NULL && (
#ifdef _SUPPORT_TLS_1_2
			verify_ASN1 ? !sigcipher->VerifyASN1(reference.sig_hasher.Ptr(),signature.GetDirect(), signature.GetLength()) :
#endif
				!sigcipher->VerifyVector(reference.signature,signature)) )  ||
			(sigcipher == NULL && reference.signature != signature ))
		{
			RaiseAlert(SSL_Fatal, SSL_Illegal_Parameter, msg);
			return FALSE;
		}
	}
	return TRUE;
}

void SSL_Signature::InitSigning()
{
	sig_hasher->InitHash();
}

void SSL_Signature::Sign(const SSL_varvector16 &source)   
{
#ifdef TST_DUMP
    DumpTofile(source,source.GetLength(),"signing data vector","sslsign.txt");
#endif
	sig_hasher->CalculateHash(source);
}

void SSL_Signature::SignHash(SSL_varvector32 &hash_data)   
{
#ifdef TST_DUMP
    DumpTofile(hash_data,hash_data.GetLength(),"signing hash md5","sslsign.txt");
#endif
	sig_len = 0;
	switch(sign_alg)
	{
    case SSL_Anonymous_sign : break;
    case SSL_RSA_MD5_SHA_sign : 
		sig_len = sizeof(md5_sha_rsa_sign.md5_sha_hash);
		break;
    case SSL_DSA_sign : 
		sig_len =  sizeof(dsa_sign.sha_hash);
		break;
	case SSL_RSA_SHA: 
		sig_len =  sizeof(sha_sign.sha_hash);
		break;
	case SSL_RSA_SHA_224: 
		sig_len =  sizeof(sha_224_sign.sha_hash);
		break;
	case SSL_RSA_SHA_256: 
		sig_len =  sizeof(sha_256_sign.sha_hash);
		break;
	case SSL_RSA_SHA_384: 
		sig_len =  sizeof(sha_384_sign.sha_hash);
		break;
	case SSL_RSA_SHA_512: 
		sig_len =  sizeof(sha_512_sign.sha_hash);
		break;

    default: break; // nothing to do for anon or illegal
	}
	if(sig_len)
	{
		if(sig_len != hash_data.GetLength())
		{
			RaiseAlert(SSL_Internal,SSL_InternalError);
			return;
		}
		op_memcpy(dummy.dummy_hash, hash_data.GetDirect(), sig_len);
	}
	PerformSigning();
}

void SSL_Signature::PerformSigning()   
{
	SSL_varvector16 temp,*temp1;
	
	temp.ForwardTo(this);  
	temp1 = (sigcipher != NULL ? &temp : &signature);
	switch(sign_alg)
	{
    case SSL_Anonymous_sign : break;
    case SSL_RSA_MD5_SHA_sign : 
    case SSL_DSA_sign : 
	case SSL_RSA_SHA: 
	case SSL_RSA_SHA_224: 
	case SSL_RSA_SHA_256: 
	case SSL_RSA_SHA_384: 
	case SSL_RSA_SHA_512: 
		temp1->Set(dummy.dummy_hash, sig_len);
		break;
   default: break; // nothing to do for anon or illegal
	}
	if(sigcipher != NULL)
	{
		sigcipher->SetUsePrivate(TRUE);
#ifdef _SUPPORT_TLS_1_2
		if(verify_ASN1)
		{
			uint32 len,size, sig_len;

			len = temp.GetLength();
			size = sigcipher->Calc_BufferSize(len);
			signature.Resize(size);
			if(size == 0)
			{
				RaiseAlert(SSL_Internal,SSL_InternalError);
				signature.RaiseAlert(this);
			}
			
			if(signature.ErrorRaisedFlag)
			{
				signature.Resize(0);
				return;
			}
			
			sigcipher->SignASN1(SignAlgToHash(sign_alg), temp, signature.GetDirect(), sig_len, size);
			OP_ASSERT(sig_len == size);
		}
		else
#endif
			sigcipher->SignVector(temp, signature);
	}
}

void SSL_Signature::FinishSigning()
{
	switch(sign_alg)
	{
    case SSL_Anonymous_sign :
		break;
    case SSL_RSA_MD5_SHA_sign : 
	case SSL_RSA_SHA: 
	case SSL_RSA_SHA_224: 
	case SSL_RSA_SHA_256: 
	case SSL_RSA_SHA_384: 
	case SSL_RSA_SHA_512: 
    case SSL_DSA_sign :
		{
			sig_hasher->ExtractHash(dummy.dummy_hash);
			sig_len = sig_hasher->Size();
#ifdef TST_DUMP
			DumpTofile(dummy.dummy_hash, sig_len ,"signing extracted digest result","sslsign.txt");
#endif
			break;
		}
		
    default: break; // nothing to do for anon or illegal
	}
	PerformSigning(); 
}
#endif // _NATIVE_SSL_SUPPORT_
