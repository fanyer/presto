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

#ifdef _NATIVE_SSL_SUPPORT_
#ifdef _SSL_USE_PKCS11_

#include "modules/libssl/sslbase.h"
#include "modules/libssl/external/p11/p11_pubk.h"

PKCS11_PublicKeyCipher::PKCS11_PublicKeyCipher(PKCS_11_Manager *mstr, PKCS_11_Token *tokn, CK_FUNCTION_LIST_PTR func, CK_SLOT_ID slot, SSL_BulkCipherType ctyp)
 : master(mstr), token(tokn), functions(func), slot_id(slot), 
	session(CK_INVALID_HANDLE),	ciphertype(ctyp)
{
	templen = temppos =0;

	if(master == NULL || token == NULL || functions == NULL)
		RaiseAlert(SSL_Fatal, SSL_InternalError);
}

PKCS11_PublicKeyCipher::PKCS11_PublicKeyCipher(const PKCS11_PublicKeyCipher *old)
 : functions(NULL), slot_id(0), session(CK_INVALID_HANDLE),	ciphertype(SSL_RSA)
{
	use_privatekey = FALSE;
	templen = temppos =0;

	if(old == NULL)
	{
		RaiseAlert(SSL_Internal,SSL_InternalError);
		return;
	}

	master = old->master;
	token = old->token;
	functions = old->functions;
	
	slot_id = old->slot_id;
	ciphertype = old->ciphertype;

	if(master == NULL || functions == NULL)
		RaiseAlert(SSL_Fatal, SSL_InternalError);
}

PKCS11_PublicKeyCipher::~PKCS11_PublicKeyCipher()
{
	CloseSession();
}

void PKCS11_PublicKeyCipher::SetUsePrivate(BOOL stat)
{
	use_privatekey = stat;
}

BOOL PKCS11_PublicKeyCipher::UsingPrivate() const
{
	return use_privatekey;
}

SSL_CipherType PKCS11_PublicKeyCipher::Type() const
{
	return SSL_BlockCipher;
}

SSL_BulkCipherType PKCS11_PublicKeyCipher::CipherID() const
{
	return ciphertype;
}


SSL_BulkCipherType PKCS11_PublicKeyCipher::GetCipherType() const
{
	return ciphertype;
}

uint16 PKCS11_PublicKeyCipher::InputBlockSize() const
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

uint16 PKCS11_PublicKeyCipher::OutputBlockSize() const
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

void PKCS11_PublicKeyCipher::InitEncrypt()
{
	templen = InputBlockSize();
	temppos = 0;
	if(templen == 0 || OutputBlockSize() == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		return;
	}

	tempbuffer.Resize(templen);
	if(Error())
		templen = 0;
}

BOOL PKCS11_PublicKeyCipher::CheckSession()
{
	CK_RV status;
	
	status = token->CreateSession(&session);
	if(CheckError(status))
		return FALSE;

	return TRUE;
}

void PKCS11_PublicKeyCipher::Login(SSL_secure_varvector32 &password)
{
	CK_RV status;

	status = token->Login(password);

	CheckError(status);
}

BOOL PKCS11_PublicKeyCipher::Need_Login()
{
	return token->NeedLogIn();
}

BOOL PKCS11_PublicKeyCipher::Need_PIN()
{
	return token->NeedPin();
}

void PKCS11_PublicKeyCipher::CloseSession()
{
	token->CloseSession(&session);
}

BOOL PKCS11_PublicKeyCipher::SetupKey(CK_OBJECT_HANDLE &key)
{
	token->UsedNow();
	token->Activate();

	if(!CheckSession())
		return FALSE;

	CK_RV status;
	CK_ULONG key_class, key_alg;
	CK_ATTRIBUTE key_att[] = {
		{CKA_CLASS,		&key_class,	sizeof(key_class)},
		{CKA_KEY_TYPE,	&key_alg,	sizeof(key_alg)}
	};	

	CK_ULONG	key_count=1;

	key_class = use_privatekey ? CKO_PRIVATE_KEY : CKO_PUBLIC_KEY;
	key_alg = CKK_RSA;

	status = functions->C_FindObjectsInit(session, key_att, ARRAY_SIZE(key_att));
	if(CheckError(status))
		return FALSE;

	status = functions->C_FindObjects(session, &key, 1, &key_count);
	if(CheckError(status))
		return FALSE;

	status = functions->C_FindObjectsFinal(session);
	if(CheckError(status))
		return FALSE;

	return TRUE;
}

void PKCS11_PublicKeyCipher::EncryptStep(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	CK_RV status;
	CK_MECHANISM crypto_method[] = {
		{CKM_RSA_PKCS, NULL_PTR, 0}
	};
	CK_OBJECT_HANDLE key;
	CK_ULONG len2=bufferlen;

	len1 = 0;
	if(!SetupKey(key))
		return;

	if(use_privatekey)
	{
		status = functions->C_SignInit(session, crypto_method, key);
		if(CheckError(status))
			return;
		
		status = functions->C_Sign(session, (CK_BYTE_PTR) source, len, (CK_BYTE_PTR) target, &len2);
		if(CheckError(status))
			return;
	}
	else
	{
		status = functions->C_EncryptInit(session, crypto_method, key);
		if(CheckError(status))
			return;
		
		status = functions->C_Encrypt(session, (CK_BYTE_PTR) source, len, (CK_BYTE_PTR) target, &len2);
		if(CheckError(status))
			return;
	}

	len1 = len2;

	CloseSession();
}


byte *PKCS11_PublicKeyCipher::Encrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	uint32 len2,len3;
	const byte *source1;
	
	len1 = 0;
	if(ciphertype != SSL_DH)
		while(len >0)
		{
			if(temppos || len<templen)
			{
				len3 = (len + temppos > templen ? templen-temppos : len);
				source = tempbuffer.Input(temppos,source,len3);
				len -= len3;
				temppos += len3;
				if(temppos <templen)
					continue;
				source1 = tempbuffer.GetDirect();
				len2 = temppos;
			}
			else
			{
				source1 = source;
				source+=templen;
				len -= templen;
				len2 = templen;
			}
			if(len1 + OutputBlockSize() > bufferlen)
			{
				RaiseAlert(SSL_Fatal, SSL_InternalError);
				break;
			}
			EncryptStep(source1,len2,target,len3, bufferlen);
			len1+=len3;
			target+=len3;
		}
		return target;
}

byte *PKCS11_PublicKeyCipher::FinishEncrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	if(ciphertype != SSL_DH && temppos)
	{
		EncryptStep(tempbuffer.GetDirect(),temppos,target,len1, bufferlen);
		target+=len1;
		tempbuffer.Resize(0);
		templen = 0;
	}
	
	return target;
}

void PKCS11_PublicKeyCipher::InitDecrypt()
{
	templen = OutputBlockSize();
	temppos = 0;
	if(templen == 0 || InputBlockSize() == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		return;
	}

	tempbuffer.Resize(templen);
	if(Error())
		templen = 0;
}

void PKCS11_PublicKeyCipher::DecryptStep(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	CK_RV status;
	CK_MECHANISM crypto_method[] = {
		{CKM_RSA_PKCS, NULL_PTR, 0}
	};
	CK_OBJECT_HANDLE key;
	CK_ULONG len2=bufferlen;

	len1 = 0;

	if(!SetupKey(key))
		return;

	if(use_privatekey)
	{
		status = functions->C_DecryptInit(session, crypto_method, key);
		if(CheckError(status))
			return;
		
		status = functions->C_Decrypt(session, (CK_BYTE_PTR) source, len, (CK_BYTE_PTR) target, &len2);
		if(CheckError(status))
			return;
	}
	else
	{
		status = functions->C_VerifyRecoverInit(session, crypto_method, key);
		if(CheckError(status))
			return;
		
		status = functions->C_VerifyRecover(session, (CK_BYTE_PTR) source, len, (CK_BYTE_PTR) target, &len2);
		if(CheckError(status))
			return;
	}

	len1 = len2;

	CloseSession();
}

byte *PKCS11_PublicKeyCipher::Decrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	uint32 len2,len3;
	const byte *source1;
	
	len1 = 0;
	if(ciphertype == SSL_RSA)
		while(len >0)
		{
			len2 = 0;
			if(temppos || len<templen)
			{
				len3 = (len + temppos > templen ? templen-temppos : len);
				source = tempbuffer.Input(temppos,source,len3);
				temppos += len3;
				len -= len3;
				if(temppos <templen)
					continue;
				source1 = tempbuffer.GetDirect();
				len2 = temppos;
			}
			else
			{
				source1 = source;
				source+=templen;
				len -= templen;
				len2 = templen;
			}
			if(len1 + InputBlockSize() > bufferlen)
			{
				RaiseAlert(SSL_Fatal, SSL_InternalError);
				break;
			}
			DecryptStep(source1,len2,target,len3, bufferlen);
			len1 += len3;
			target+= len3;
		}
		return target;
}

byte *PKCS11_PublicKeyCipher::FinishDecrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	if(ciphertype == SSL_RSA && temppos) 
	{
		DecryptStep(tempbuffer.GetDirect(),temppos,target,len1, bufferlen);
		target+= len1;
		tempbuffer.Resize(0);
		templen = 0;
	}
	return target;
}

byte *PKCS11_PublicKeyCipher::Sign(const byte *source,uint32 len,byte *target, uint32 &len1, uint32 bufferlen)
{
	uint32 len2;
	
	use_privatekey = TRUE;
	InitEncrypt();
	target = Encrypt(source,len,target,len1,bufferlen);
	target = FinishEncrypt(target,len2,bufferlen-len1);
	len1 += len2;
	return target;
}

byte *PKCS11_PublicKeyCipher::SignASN1(SSL_Hash *reference, byte *signature, uint32 &len, uint32 bufferlen)
{
	CK_RV status;
	CK_MECHANISM crypto_method[] = {
		{CKM_MD5_RSA_PKCS, NULL_PTR, 0}
	};
	CK_OBJECT_HANDLE key;
	SSL_varvector32 temp;
	CK_ULONG len2 = 0;
	
	switch(reference->HashID())
	{
	case SSL_MD5:
		crypto_method[0].mechanism = CKM_MD5_RSA_PKCS;
		break;
	case SSL_SHA:
		crypto_method[0].mechanism = CKM_SHA1_RSA_PKCS;
		break;
    default      : break;
	}
	reference->ExtractHash(temp);

	len = 0;
	use_privatekey = TRUE;

	if(!SetupKey(key))
		return signature;

	status = functions->C_SignInit(session, crypto_method, key);
	if(CheckError(status))
		return signature;

	status = functions->C_Sign(session, (CK_BYTE_PTR) temp.GetDirect(), temp.GetLength(), (CK_BYTE_PTR) signature, &len2);
	if(CheckError(status))
		return signature;

	len = len2;

	CloseSession();

	return signature + len;
}

BOOL PKCS11_PublicKeyCipher::VerifyASN1(SSL_Hash *reference, const byte *signature, uint32 len)
{
	CK_RV status;
	CK_MECHANISM crypto_method[] = {
		{CKM_MD5_RSA_PKCS, NULL_PTR, 0}
	};
	CK_OBJECT_HANDLE key;
	SSL_varvector32 temp;
	BOOL ret = FALSE;
	
	switch(reference->HashID())
	{
	case SSL_MD5:
		crypto_method[0].mechanism = CKM_MD5_RSA_PKCS;
		break;
	case SSL_SHA:
		crypto_method[0].mechanism = CKM_SHA1_RSA_PKCS;
		break;
    default      : break;
	}
	reference->ExtractHash(temp);

	len = 0;
	use_privatekey = FALSE;

	if(!SetupKey(key))
		return ret;

	status = functions->C_VerifyInit(session, crypto_method, key);
	if(CheckError(status))
		return ret;

	status = functions->C_Verify(session, (CK_BYTE_PTR) temp.GetDirect(), temp.GetLength(), (CK_BYTE_PTR) signature, len);
	switch(status)
	{
	case CKR_OK: 
		ret = TRUE;
		break;
	case CKR_SIGNATURE_INVALID:
	case CKR_SIGNATURE_LEN_RANGE:
		ret= FALSE;
		break;
	default:
		if(CheckError(status))
			return ret;
	}

	CloseSession();

	return ret;
}

BOOL PKCS11_PublicKeyCipher::Verify(const byte *reference,uint32 len,const byte *signature,uint32 len1)
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
#if 0 // def SSL_DSA_SUPPORT
    case SSL_DSA : 
		{
			int ret;
			
			ret = DSA_verify(0,(byte *) reference,(int) len,(byte *) signature,(int) len1,keyspec->pkey.dsa);
			return (ret<=0 ? FALSE : TRUE);
		}
#endif
    default      : break;
	}
	return FALSE;
}

SSL_PublicKeyCipher *PKCS11_PublicKeyCipher::Produce() const
{
	return Fork();
}

SSL_PublicKeyCipher *PKCS11_PublicKeyCipher::Fork() const
{
	return OP_NEW(PKCS11_PublicKeyCipher, (this));
}

uint16 PKCS11_PublicKeyCipher::PublicCount() const
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

uint16 PKCS11_PublicKeyCipher::GeneratedPublicCount() const
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

uint16 PKCS11_PublicKeyCipher::PublicSize(uint16 item) const
{
	return (KeyBitsLength() +7)/8;
}

uint16 PKCS11_PublicKeyCipher::PrivateCount() const
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

uint16 PKCS11_PublicKeyCipher::GeneratedPrivateCount() const
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

uint16 PKCS11_PublicKeyCipher::PrivateSize(uint16 item) const
{
	return 0;
}

void PKCS11_PublicKeyCipher::LoadPrivateKey(uint16 item, const SSL_varvector16 &source)
{
	RaiseAlert(SSL_Internal, SSL_InternalError);
}

void PKCS11_PublicKeyCipher::UnLoadPrivateKey(uint16 item, SSL_varvector16 &target)
{
	RaiseAlert(SSL_Internal, SSL_InternalError);
}

void PKCS11_PublicKeyCipher::UnLoadPublicKey(uint16 item, SSL_varvector16 &target)
{
	CK_RV status;
	CK_ULONG key_class, key_alg;
	CK_ATTRIBUTE key_att[] = {
		{CKA_CLASS,		&key_class,	sizeof(key_class)},
		{CKA_KEY_TYPE,	&key_alg,	sizeof(key_alg)}
	};	
	CK_ATTRIBUTE key_item[] = {
		{CKA_MODULUS,		NULL_PTR,	0},
	};	
	CK_OBJECT_HANDLE key;
	CK_ULONG  key_count=1;

	target.Resize(0);

	switch(item)
	{
	case 0: 
		key_item[0].type = CKA_MODULUS;
		break;
	case 1: 
		key_item[0].type = CKA_PUBLIC_EXPONENT;
		break;
	default:
		return;
	}

	if(!CheckSession())
		return;

	key_class = CKO_PUBLIC_KEY;
	key_alg = CKK_RSA;

	status = functions->C_FindObjectsInit(session, key_att, ARRAY_SIZE(key_att));
	if(CheckError(status))
		return;

	status = functions->C_FindObjects(session, &key, 1, &key_count);
	if(CheckError(status))
		return;

	status = functions->C_FindObjectsFinal(session);
	if(CheckError(status))
		return;

			
	status = functions->C_GetAttributeValue(session, key, key_item, 1);
	if(CheckError(status))
		return;
	if(((CK_LONG) key_item[0].ulValueLen) == -1 || key_item[0].ulValueLen ==0)
		return;
		
	target.Resize(key_item[0].ulValueLen);
	if(target.Error())
		return;
	
	key_item[0].pValue = target.GetDirect();
	key_item[0].ulValueLen = target.GetLength();
	
	status = functions->C_GetAttributeValue(session, key, key_item, 1);
	if(CheckError(status))
		return;
}


void PKCS11_PublicKeyCipher::LoadPublicKey(uint16 item, const SSL_varvector16 &source)
{
	RaiseAlert(SSL_Internal, SSL_InternalError);
}

#ifdef SSL_DH_SUPPORT
void PKCS11_PublicKeyCipher::ProduceGeneratedPublicKeys()
{
}
#endif

#ifdef SSL_DH_SUPPORT
void PKCS11_PublicKeyCipher::ProduceGeneratedPrivateKeys()
{
}
#endif

uint32 PKCS11_PublicKeyCipher::KeyBitsLength() const
{
	CK_RV status;
	CK_ULONG key_bits;
	CK_ULONG key_class, key_alg;
	CK_ATTRIBUTE key_att[] = {
		{CKA_CLASS,		&key_class,	sizeof(key_class)},
		{CKA_KEY_TYPE,	&key_alg,	sizeof(key_alg)}
	};	
	CK_ATTRIBUTE key_size[] = {
		{CKA_MODULUS_BITS,		&key_bits,	sizeof(key_bits)},
	};	
	CK_OBJECT_HANDLE key;
	CK_ULONG  key_count=1;
	CK_SESSION_HANDLE session2 = CK_INVALID_HANDLE;

	status = token->CreateSession(&session2);
	if(master->TranslateToSSL_Alert(status,NULL))
		return 0;

	key_class = CKO_PUBLIC_KEY;
	key_alg = CKK_RSA;

	status = functions->C_FindObjectsInit(session2, key_att, ARRAY_SIZE(key_att));
	if(master->TranslateToSSL_Alert(status,NULL))
		return 0;

	status = functions->C_FindObjects(session2, &key, 1, &key_count);
	if(master->TranslateToSSL_Alert(status,NULL))
		return 0;

	status = functions->C_FindObjectsFinal(session2);
	if(master->TranslateToSSL_Alert(status,NULL))
		return 0;

	status = functions->C_GetAttributeValue(session2, key, key_size, 1);
	if(master->TranslateToSSL_Alert(status,NULL))
		return 0;

	token->CloseSession(&session2);

	return key_bits;
}

void PKCS11_PublicKeyCipher::LoadAllKeys(SSL_varvector32 &key)
{
}


BOOL PKCS11_PublicKeyCipher::CheckError(CK_RV status)
{
	if(master->TranslateToSSL_Alert(status, this))
	{
		CloseSession();
		return TRUE;
	}

	return FALSE;
	
}


BOOL PKCS11_PublicKeyCipher::Error(SSL_Alert *msg) const
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
#endif
