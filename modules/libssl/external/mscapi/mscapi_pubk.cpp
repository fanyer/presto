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

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_MSCAPI_

#if !defined(MSWIN)
#error "FEATURE_SMARTCARD_MSCAPI can only be used in MSWIN builds."
#endif

#include "modules/libssl/sslbase.h"

#include "modules/hardcore/mem/mem_man.h"

#include "mscapi_pubk.h"

MSCAPI_PublicKeyCipher::MSCAPI_PublicKeyCipher(HCRYPTPROV prov)
{
	provider = prov;
	key_bits = 1024;	
	ciphertype = SSL_RSA;

	if(provider == NULL)
		RaiseAlert(SSL_Fatal, SSL_InternalError);
	else
	{
		CryptContextAddRef(provider, NULL, 0);

		HCRYPTKEY key = NULL;
		ALG_ID alg;
		DWORD key_len;
		DWORD len;

		if(!CryptGetUserKey(provider, AT_KEYEXCHANGE, &key))
		{
			CheckError(GetLastError());
			return;
		}

		len = sizeof(alg);
		if(CryptGetKeyParam(key, KP_ALGID, (byte *) &alg, &len, 0))
		{
			switch(alg)
			{
			case CALG_RSA_SIGN:
			case CALG_RSA_KEYX:
				ciphertype = SSL_RSA;
				break;
			default:
				ciphertype = SSL_NoCipher;
			}
		}

		len = sizeof(key_len);
		if(CryptGetKeyParam(key, KP_BLOCKLEN, (byte *) &key_len, &len, 0))
		{
			key_bits = (uint16) key_len;
		}
		CryptDestroyKey(key);
	}
}

MSCAPI_PublicKeyCipher::MSCAPI_PublicKeyCipher(const MSCAPI_PublicKeyCipher *old)
{
	use_privatekey = FALSE;

	if(old == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_InternalError);
		return;
	}
	provider = old->provider;
	ciphertype = old->ciphertype;
	key_bits = old->key_bits;	

	if(provider == NULL)
		RaiseAlert(SSL_Fatal, SSL_InternalError);
	else
		CryptContextAddRef(provider, NULL, 0);
}

MSCAPI_PublicKeyCipher::~MSCAPI_PublicKeyCipher()
{
	if(provider)
		CryptReleaseContext(provider, 0);
	provider = NULL;
}

void MSCAPI_PublicKeyCipher::SetUsePrivate(BOOL stat)
{
	use_privatekey = stat;
}

BOOL MSCAPI_PublicKeyCipher::UsingPrivate() const
{
	return use_privatekey;
}

SSL_CipherType MSCAPI_PublicKeyCipher::Type() const
{
	return SSL_BlockCipher;
}

SSL_BulkCipherType MSCAPI_PublicKeyCipher::CipherID() const
{
	return ciphertype;
}


SSL_BulkCipherType MSCAPI_PublicKeyCipher::GetCipherType() const
{
	return ciphertype;
}

uint16 MSCAPI_PublicKeyCipher::InputBlockSize() const
{
	switch (ciphertype)
	{
    case SSL_RSA : return ((KeyBitsLength()+7)/8)-11;
    case SSL_DSA : return 20;
    case SSL_DH  : return 0;
    default      : break;
    }
	return 0;
}

uint16 MSCAPI_PublicKeyCipher::OutputBlockSize() const
{
	switch (ciphertype)
	{
    case SSL_RSA : return (KeyBitsLength()+7)/8;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return (KeyBitsLength()+7)/8;
#endif
    case SSL_DH  : return 0;
    default      : break;
	}
	return 0;
}

void MSCAPI_PublicKeyCipher::Login(SSL_secure_varvector32 &password)
{
}

BOOL MSCAPI_PublicKeyCipher::Need_Login()
{
	return FALSE;
}

BOOL MSCAPI_PublicKeyCipher::Need_PIN()
{
	return FALSE;
}

void MSCAPI_PublicKeyCipher::InitEncrypt()
{
	if(OutputBlockSize() == 0 || InputBlockSize() == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
	}
}

byte *MSCAPI_PublicKeyCipher::Encrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	DWORD result_len;

	if(!use_privatekey)
	{
		HCRYPTKEY key = NULL;

		if(!CryptGetUserKey(provider, AT_KEYEXCHANGE, &key))
		{
			CheckError(GetLastError());
			return target;
		}

		op_memcpy(target, source, len);
		
		result_len = len;

		if(!CryptEncrypt(key, NULL, TRUE, 0, target, &result_len, bufferlen))
		{
			CheckError(GetLastError());
			CryptDestroyKey(key);
			return target;
		}
		CryptDestroyKey(key);
	}
	else
	{
		HCRYPTHASH hash;

		if(use_privatekey && len != (SSL_MD5_LENGTH + SSL_SHA_LENGTH))
		{
			RaiseAlert(SSL_Internal, SSL_Illegal_Parameter);
			return target;
		}

		if(!CryptCreateHash(provider, CALG_SSL3_SHAMD5, NULL, 0, &hash))
		{
			CheckError(GetLastError());
			return target;
		}

		if(!CryptSetHashParam(hash, HP_HASHVAL, (unsigned char *) source, 0))
		{
			CheckError(GetLastError());
			CryptDestroyHash(hash);
			return target;
		}

		result_len = bufferlen;
		if(!CryptSignHash(hash, AT_KEYEXCHANGE, NULL, 0, target, &result_len))
		{
			CheckError(GetLastError());
			CryptDestroyHash(hash);
			return target;
		}

#if 0
		do{
			HCRYPTKEY key = NULL;

			if(!CryptGetUserKey(provider, AT_KEYEXCHANGE, &key))
			{
				int err = GetLastError();
				break;
			}

			DWORD res_2 = result_len;
			if(!CryptVerifySignature(hash, target,res_2, key,NULL,0))
			{
				int err = GetLastError();
			}

			CryptDestroyKey(key);
		}while(0);
#endif

		CryptDestroyHash(hash);
	}

	len1 = result_len;

	DWORD i=0;
	unsigned char temp;

	result_len--;
	while(i<result_len)
	{
		temp = target[i];
		target[i] = target[result_len];
		target[result_len] = temp;
		result_len--;
		i++;
	}

	return target+len1;
}

byte *MSCAPI_PublicKeyCipher::FinishEncrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	return target;
}

void MSCAPI_PublicKeyCipher::InitDecrypt()
{
	if(OutputBlockSize() == 0 || InputBlockSize() == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
	}
}

byte *MSCAPI_PublicKeyCipher::Decrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;

	OP_ASSERT(bufferlen>= len);
	if(bufferlen< len)
		return target;

	HCRYPTKEY key = NULL;

	if(!CryptGetUserKey(provider, AT_KEYEXCHANGE, &key))
	{
		CheckError(GetLastError());
		return target;
	}

	op_memcpy(target, source, len);

	DWORD result_len = len;
	DWORD i=0;
	unsigned char temp;

	result_len--;
	while(i<result_len)
	{
		temp = target[i];
		target[i] = target[result_len];
		target[result_len] = temp;
		result_len--;
		i++;
	}
	
	result_len = len;

	if(!CryptDecrypt(key, NULL, TRUE, 0, target, &result_len))
	{
		CheckError(GetLastError());
		CryptDestroyKey(key);
		return target;
	}
	CryptDestroyKey(key);

	len1 = result_len;

	return target;
}

byte *MSCAPI_PublicKeyCipher::FinishDecrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	return target;
}

byte *MSCAPI_PublicKeyCipher::Sign(const byte *source,uint32 len,byte *target, uint32 &len1, uint32 bufferlen)
{
	uint32 len2;
	
	use_privatekey = TRUE;
	InitEncrypt();
	target = Encrypt(source,len,target,len1,bufferlen);
	target = FinishEncrypt(target,len2,bufferlen-len1);
	len1 += len2;
	return target;
}

#if 0 && (defined(USE_SSL_ASN1_SIGNING))
#error "Not implemented yet"
byte *MSCAPI_PublicKeyCipher::SignASN1(SSL_Hash *reference, byte *signature, uint32 &len, uint32 bufferlen)
{
	len = 0;
	OP_ASSERT(0);

	return signature;
}

BOOL MSCAPI_PublicKeyCipher::VerifyASN1(SSL_Hash *reference, const byte *signature, uint32 len)
{
	OP_ASSERT(0);
	return FALSE;
}
#endif

BOOL MSCAPI_PublicKeyCipher::Verify(const byte *reference,uint32 len,const byte *signature,uint32 len1)
{
	switch (ciphertype)
	{
    case SSL_RSA : 
		{
			SSL_varvector32 temp;
			uint32 decryplen1,decryplen2;
			byte *target;
			
			temp.Resize(len1);
			if(temp.ErrorRaisedFlag)
				return FALSE;
			target = temp.GetDirect();
			use_privatekey = FALSE;
			InitDecrypt();
			target = Decrypt(signature,len1,target,decryplen1, temp.GetLength());
			target = FinishDecrypt(target,decryplen2, temp.GetLength()- decryplen1);
			temp.Resize(decryplen1+decryplen2);
			
			if(temp.GetLength() == len)
				return (op_memcmp(temp.GetDirect(),reference,(size_t) len) == 0 ? TRUE : FALSE);
		}
		break;
    default      : break;
	}
	return FALSE;
}

SSL_PublicKeyCipher *MSCAPI_PublicKeyCipher::Produce() const
{
	return Fork();
}

SSL_PublicKeyCipher *MSCAPI_PublicKeyCipher::Fork() const
{
	return OP_NEW(MSCAPI_PublicKeyCipher, (this));
}

uint16 MSCAPI_PublicKeyCipher::PublicCount() const
{
	switch (ciphertype)
	{
    case SSL_RSA : return 2;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 4;
#endif
#ifdef SSL_DH_SUPPORT
    case SSL_DH  : return 4;
#endif
    default      : break;
	}
	return 0;
}

uint16 MSCAPI_PublicKeyCipher::GeneratedPublicCount() const
{
	switch (ciphertype)
	{
    case SSL_RSA : return 0;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 1;
#endif
#ifdef SSL_DH_SUPPORT
    case SSL_DH  : return 1;
#endif
    default      : break;
	}
	return 0;
}

uint16 MSCAPI_PublicKeyCipher::PublicSize(uint16 item) const
{
	return (KeyBitsLength() +7)/8;
}

uint16 MSCAPI_PublicKeyCipher::PrivateCount() const
{
	switch (ciphertype)
	{
    case SSL_RSA : return 1;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 1;
#endif
#ifdef SSL_DH_SUPPORT
    case SSL_DH  : return 2;
#endif
    default      : break;
	}
	return 0;
}

uint16 MSCAPI_PublicKeyCipher::GeneratedPrivateCount() const
{
	switch (ciphertype)
	{
    case SSL_RSA : return 0;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 0;
#endif
#ifdef SSL_DH_SUPPORT
    case SSL_DH  : return 1;
#endif
    default      : break;
	}
	return 0;
}

uint16 MSCAPI_PublicKeyCipher::PrivateSize(uint16 item) const
{
	return 0;
}

void MSCAPI_PublicKeyCipher::LoadPrivateKey(uint16 item, const SSL_varvector16 &source)
{
	RaiseAlert(SSL_Internal, SSL_InternalError);
}

void MSCAPI_PublicKeyCipher::UnLoadPrivateKey(uint16 item, SSL_varvector16 &target)
{
	RaiseAlert(SSL_Internal, SSL_InternalError);
}

void MSCAPI_PublicKeyCipher::UnLoadPublicKey(uint16 item, SSL_varvector16 &target)
{
}


void MSCAPI_PublicKeyCipher::LoadPublicKey(uint16 item, const SSL_varvector16 &source)
{
	RaiseAlert(SSL_Internal, SSL_InternalError);
}

#ifdef SSL_DH_SUPPORT
void MSCAPI_PublicKeyCipher::ProduceGeneratedPublicKeys()
{
}
#endif

#ifdef SSL_DH_SUPPORT
void MSCAPI_PublicKeyCipher::ProduceGeneratedPrivateKeys()
{
}
#endif

uint32 MSCAPI_PublicKeyCipher::KeyBitsLength() const
{

	return key_bits;
}

void MSCAPI_PublicKeyCipher::LoadAllKeys(SSL_varvector32 &key)
{
}


BOOL MSCAPI_PublicKeyCipher::CheckError(int status)
{
	if(MSCAPI_Manager::TranslateToSSL_Alert(status, this))
	{
		return TRUE;
	}

	return FALSE;
	
}


BOOL MSCAPI_PublicKeyCipher::Error(SSL_Alert *msg) const
{
	BOOL crypt,fail;
	
	fail = crypt = FALSE;
	
	if(fail)
	{
		if(msg != NULL)
			if(crypt)
				msg->Set(SSL_Fatal, SSL_Decrypt_Error);
			else
				msg->Set(SSL_Internal, SSL_InternalError);
			
			return TRUE;
	}
	
	return SSL_PublicKeyCipher::Error(msg);
}

#endif
