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

#ifdef _SSL_USE_OPENSSL_
#include "modules/libssl/sslbase.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libopeay/openssl/dsa.h"
#include "modules/libopeay/openssl/dh.h"
#include "modules/libssl/external/openssl/certhand1.h"
#include "modules/libssl/external/openssl/pubkey.h"
#include "modules/libopeay/addon/elgamal.h"
#include "modules/libssl/methods/sslhash.h"

#define PUBKEY_RSA_FIELD_COUNT 7
#define PUBKEY_RSA_FIELD_INITCOUNT 3
static BIGNUM *RSA::* const rsa_fields[] = {&RSA::n, &RSA::e, &RSA::d,
&RSA::p, &RSA::q, &RSA::dmp1, &RSA::iqmp};
static BIGNUM *RSA::* const rsa_pub_field[] = {&RSA::n, &RSA::e};
static BIGNUM *RSA::* const rsa_priv_field[] = {&RSA::d, &RSA::p, &RSA::q, &RSA::iqmp};


#ifdef SSL_DSA_SUPPORT
#define PUBKEY_DSA_FIELD_COUNT 5
#define PUBKEY_DSA_FIELD_INITCOUNT 5
static BIGNUM *DSA::* const dsa_fields[] = {&DSA::p, &DSA::q, &DSA::g, &DSA::pub_key, &DSA::priv_key};
static BIGNUM *DSA::* const dsa_pub_field[] = {&DSA::p, &DSA::q, &DSA::g, &DSA::pub_key};
static BIGNUM *DSA::* const dsa_priv_field[] = {&DSA::priv_key};
#endif // SSL_DSA_SUPPORT

#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
#define PUBKEY_DH_FIELD_COUNT 5
#define PUBKEY_DH_FIELD_INITCOUNT 5
static BIGNUM *DH::* const dh_fields[] = {&DH::p, &DH::g, &DH::pub_key,NULL, &DH::priv_key};
static BIGNUM *DH::* const dh_pub_field[] = {&DH::p, &DH::g, &DH::pub_key, NULL};
static BIGNUM *DH::* const dh_priv_field[] = {&DH::priv_key};
#endif

SSL_PublicKeyCipher *SSL_API::CreatePublicKeyCipher(SSL_BulkCipherType cipher, OP_STATUS &op_err)
{
	op_err = OpStatus::OK;

	switch(cipher)
	{
	case SSL_RSA:
#ifdef SSL_DSA_SUPPORT
	case SSL_DSA:
#endif
#ifdef SSL_ELGAMAL_SUPPORT
	case SSL_ElGamal:
#endif
#ifdef SSL_DH_SUPPORT
	case SSL_DH:
#endif
		break;
	default:
		op_err = OpStatus::ERR_OUT_OF_RANGE;
		return NULL;
	}

	OpAutoPtr<SSL_PublicKeyCipher> key(OP_NEW(SSLEAY_PublicKeyCipher, (cipher)));

	if(key.get() == NULL)
	{
		op_err = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	if(key->Error())
	{
		op_err = key->GetOPStatus();
		return NULL;
	}

	return key.release();
}


BOOL SSL_InitBN(BIGNUM *&target)
{
	target = BN_new();
	return (target != NULL);
}

BOOL SSL_CopyBN(BIGNUM *&target, BIGNUM *source)
{
	if (source == NULL)
	{
		if(target != NULL)
		{
			BN_free(target);
			target = NULL;
		}
		return TRUE;
	}
	
	if(target == NULL)
	{
		if(!SSL_InitBN(target))
			return FALSE;
		
	}

	return (BN_copy(target,source) != NULL);
}

uint32 BN_bn2Vector(BIGNUM *source, SSL_varvector32 &target)
{
	if(source)
	{
		target.Resize(BN_num_bytes(source));
		if(!target.ErrorRaisedFlag)
			return BN_bn2bin(source,(byte *) target.GetDirect());
	}

	return 0;
}

BIGNUM *BN_Vector2bn(const SSL_varvector32 &source, BIGNUM **target)
{
	BIGNUM *temp = NULL;
	if(target != NULL)
		*target = temp = BN_bin2bn((byte *) source.GetDirect(),source.GetLength(),*target);

	return temp;
}


SSL_PublicKeyCipher *GetPublicKeyCipher(X509 *x)
{
	EVP_PKEY *key = X509_extract_key(x);

	SSL_PublicKeyCipher *ret = OP_NEW(SSLEAY_PublicKeyCipher, (key));

	EVP_PKEY_free(key);
	ERR_clear_error();
	return ret;
}

SSLEAY_PublicKeyCipher::SSLEAY_PublicKeyCipher(EVP_PKEY *item)
{
	InternalInit(item);
}

void SSLEAY_PublicKeyCipher::InternalInit(EVP_PKEY *item)
{
	ERR_clear_error();
	Init();

	keyspec = item;
	if(item)
		CRYPTO_add(&item->references,1,CRYPTO_LOCK_EVP_PKEY);

	if(keyspec == NULL)
		RaiseAlert(SSL_Internal,SSL_InternalError);
	else
	{
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
		if(keyspec->type ==EVP_PKEY_DH)
		{
			DH_recv_public = BN_new();
			if(DH_recv_public == NULL)
				RaiseAlert(SSL_Internal,SSL_InternalError);
		}
#endif
		cipher_alg = GetCipherType();

		SetupInformation();
	}
	CheckError();
}

void SSLEAY_PublicKeyCipher::SetupInformation()
{
	OP_ASSERT(keyspec);

	switch(keyspec->type)
	{
	case EVP_PKEY_RSA  :
	case EVP_PKEY_RSA2 :
		SetPaddingStrategy(SSL_PKCS1_PADDING);
	};

	switch (cipher_alg)
	{
	case SSL_RSA : 
		out_block_size = RSA_size(keyspec->pkey.rsa);
		in_block_size = out_block_size -11;
		break;
	case SSL_DSA : 
		out_block_size = DSA_size(keyspec->pkey.dsa);
		in_block_size = 20;
		break;
	case SSL_DH  : 
		out_block_size = in_block_size = 0;
		break;
#ifdef SSL_ELGAMAL_SUPPORT
	case SSL_ElGamal:
		out_block_size = DH_size(keyspec->pkey.dh);
		in_block_size = out_block_size -11;
		break;
#endif
	}
}

void SSLEAY_PublicKeyCipher::Init()
{
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	DH_recv_public = NULL;
#endif
	use_privatekey = FALSE;
}


SSLEAY_PublicKeyCipher::SSLEAY_PublicKeyCipher(SSL_BulkCipherType type)
{
	InternalInit(type);
}

void SSLEAY_PublicKeyCipher::InternalInit(SSL_BulkCipherType type)
{
	ERR_clear_error();
	Init();
	
	keyspec = EVP_PKEY_new();
	cipher_alg =  type;
	if(keyspec == NULL)
		RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
	else
	{
		switch (cipher_alg )
		{
		case SSL_RSA :
			{
				RSA* temp_rsa = RSA_new();
				
				BOOL ret =
					// Check RSA object allocation.
					temp_rsa                   &&
					// Create empty BIGNUMs in the RSA object.
					SSL_InitBN(temp_rsa->n)    &&
					SSL_InitBN(temp_rsa->e)    &&
					SSL_InitBN(temp_rsa->d)    &&
					SSL_InitBN(temp_rsa->p)    &&
					SSL_InitBN(temp_rsa->q)    &&
					SSL_InitBN(temp_rsa->dmp1) &&
					SSL_InitBN(temp_rsa->iqmp) &&
					// Assign the RSA object to EVP_PKEY container.
					(EVP_PKEY_assign_RSA(keyspec, temp_rsa) == 1);

				if (!ret)
				{
					// It's OK to call RSA_free() even if temp_rsa is NULL.
					// If temp_rsa is not NULL, RSA_free() will deallocate
					// eventual allocated BIGNUMs and the RSA object itself.
					RSA_free(temp_rsa);
					RaiseAlert(SSL_Internal,SSL_InternalError);
					break;
				}
				// EVP_PKEY_assign_RSA() succeeded, now keyspec owns temp_rsa.

				SetPaddingStrategy(SSL_PKCS1_PADDING);
			}
			break;
#ifdef SSL_DSA_SUPPORT
		case SSL_DSA :
			{
				DSA* temp_dsa = DSA_new();

				BOOL ret =
					// Check DSA object allocation.
					temp_dsa                       &&
					// Create empty BIGNUMs in the DSA object.
					SSL_InitBN(temp_dsa->p)        &&
					SSL_InitBN(temp_dsa->q)        &&
					SSL_InitBN(temp_dsa->g)        &&
					SSL_InitBN(temp_dsa->pub_key)  &&
					SSL_InitBN(temp_dsa->priv_key) &&
					// Assign the DSA object to EVP_PKEY container.
					(EVP_PKEY_assign_DSA(keyspec, temp_dsa) == 1);

				if (!ret)
				{
					// It's OK to call DSA_free() even if temp_dsa is NULL.
					// If temp_dsa is not NULL, DSA_free() will deallocate
					// eventual allocated BIGNUMs and the DSA object itself.
					DSA_free(temp_dsa);
					RaiseAlert(SSL_Internal,SSL_InternalError);
					break;
				}
				// EVP_PKEY_assign_DSA() succeeded, now keyspec owns temp_dsa.
			}
			break;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
		case SSL_ElGamal :  
		case SSL_DH  :  
			{
				DH* temp_dh = DH_new();

				BOOL ret =
					// Check DH object allocation.
					temp_dh                       &&
					// Create empty BIGNUMs in the DH object.
					SSL_InitBN(temp_dh->p)        &&
					SSL_InitBN(temp_dh->g)        &&
					SSL_InitBN(temp_dh->pub_key)  &&
					SSL_InitBN(temp_dh->priv_key) &&
					// Assign the DSA object to EVP_PKEY container.
					(EVP_PKEY_assign_DH(keyspec, temp_dh) == 1);

				if (!ret)
				{
					// It's OK to call DH_free() even if temp_dh is NULL.
					// If temp_dh is not NULL, DH_free() will deallocate
					// eventual allocated BIGNUMs and the DH object itself.
					DH_free(temp_dh);
					RaiseAlert(SSL_Internal,SSL_InternalError);
					break;
				}
				// EVP_PKEY_assign_DH() succeeded, now keyspec owns temp_dh.

				DH_recv_public = BN_new();
				if(DH_recv_public == NULL)
					RaiseAlert(SSL_Internal,SSL_InternalError);
			}
			break;
#endif
		default : 
			RaiseAlert(SSL_Internal,SSL_InternalError);
		}
	}
	CheckError();
}

SSLEAY_PublicKeyCipher::SSLEAY_PublicKeyCipher(const SSLEAY_PublicKeyCipher *old)
{
	InternalInit(old);
}

void SSLEAY_PublicKeyCipher::InternalInit(const SSLEAY_PublicKeyCipher *old)
{
	ERR_clear_error();
	Init();

	if (!old)
	{
		RaiseAlert(SSL_Internal,SSL_InternalError);
		return;
	}

	InternalInit(old->keyspec);

#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	if((keyspec->type == EVP_PKEY_DH) && DH_recv_public)
	{
		if(BN_copy(DH_recv_public,old->DH_recv_public) == NULL)
		{
			RaiseAlert(SSL_Internal,SSL_InternalError);
			return;
		}
	}
#endif

	use_privatekey = old->use_privatekey;

	CheckError();
}

SSLEAY_PublicKeyCipher::~SSLEAY_PublicKeyCipher()
{
	EVP_PKEY_free(keyspec);
	keyspec= NULL;
#ifdef SSL_DH_SUPPORT
	if(DH_recv_public != NULL)
		BN_free(DH_recv_public);
	DH_recv_public = NULL;
	DH_calc_common.Blank();
#endif
}

SSL_BulkCipherType SSLEAY_PublicKeyCipher::GetCipherType() const
{
	if(keyspec != NULL)
	{
		switch (keyspec->type)
		{
		case EVP_PKEY_NONE : return SSL_NoCipher;
		case EVP_PKEY_RSA  :
		case EVP_PKEY_RSA2 : return SSL_RSA;
		case EVP_PKEY_DSA  :
		case EVP_PKEY_DSA2 :
		case EVP_PKEY_DSA3 : return SSL_DSA;
		case EVP_PKEY_DH   : return SSL_DH;
		}
	}
	return SSL_NoCipher;
}

void SSLEAY_PublicKeyCipher::InitEncrypt()
{
	if(InputBlockSize() == 0 || OutputBlockSize() == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
	}
}

/* Input is ALWAYS assumed to be blocks of less than or equal to InputBlockSize(), to be encrypted in one step. */
byte *SSLEAY_PublicKeyCipher::Encrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 =0;
	int len2 = 0;

	ERR_clear_error();
	if(OutputBlockSize() > bufferlen)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		return target;
	}

	switch (cipher_alg)
	{
    case SSL_RSA : 
		if(use_privatekey)
		{
			len2 = RSA_private_encrypt((int) len,(byte *) source,target,keyspec->pkey.rsa ,RSA_PKCS1_PADDING );
		}
		else
		{
			len2 = RSA_public_encrypt((int) len,(byte *) source,target,
				keyspec->pkey.rsa, 
				(int) GetPaddingStrategy()
				);
		}
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
    	{
			unsigned int len4;
			if(DSA_sign(0,(byte *) source,(int) len,target,&len4,keyspec->pkey.dsa)<0)
				RaiseAlert(SSL_Internal,SSL_InternalError);
			len2 =len4;
		}
#endif
#if defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
		{
			len1 = ElGamal_encrypt(len, source, target, &DH_recv_public, keyspec->pkey.dh);
		}
#endif
    default:
		break;
	}
	CheckError();
	if(len2>=0)
		len1 = (uint32) len2;

	return target;
}

byte *SSLEAY_PublicKeyCipher::FinishEncrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	return target;
}

void SSLEAY_PublicKeyCipher::InitDecrypt()
{
	if(OutputBlockSize() == 0 || InputBlockSize() == 0)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
	}
}

/* Input is ALWAYS assumed to be blocks of less than or equal to OutputBlockSize(), to be encrypted in one step. */
byte *SSLEAY_PublicKeyCipher::Decrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	int len2 =0;

	ERR_clear_error();
	if(InputBlockSize() > bufferlen)
	{
		RaiseAlert(SSL_Fatal, SSL_InternalError);
		return target;
	}

	switch (cipher_alg)
	{
    case SSL_RSA : 
#ifndef SSL_PRIVATE_KEY_DECRYPT
		OP_ASSERT(!use_privatekey);
#else
		if(use_privatekey)
			len2 = (uint32) RSA_private_decrypt((int) len, (byte *) source, target, keyspec->pkey.rsa, RSA_PKCS1_PADDING);
		else
#endif
			len2 = (uint32) RSA_public_decrypt((int) len, (byte *) source, target, keyspec->pkey.rsa, RSA_PKCS1_PADDING);
		break;
#if defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
		len2 = ElGamal_decrypt((int) len, (byte *) source, target, DH_recv_public, keyspec->pkey.dh);
#endif
    default      : break;
	}
	CheckError();
		
	if(len2>=0)
		len1 = (uint32) len2;

	return target;
}

byte *SSLEAY_PublicKeyCipher::FinishDecrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	len1 = 0;
	return target;
}

byte *SSLEAY_PublicKeyCipher::Sign(const byte *source,uint32 len,byte *target, uint32 &len1, uint32 bufferlen)
{
	//uint32 len2;
	
	use_privatekey = TRUE;
	//InitEncrypt(); // can be ignored Encrypt is single step
	target = Encrypt(source,len,target,len1,bufferlen);
	//target = FinishEncrypt(target,len2,bufferlen-len1);  // can be ignored Encrypt is single step
	//len1 += len2;  // can be ignored Encrypt is single step
	return target;
}

#ifdef USE_SSL_ASN1_SIGNING
byte *SSLEAY_PublicKeyCipher::SignASN1(SSL_Hash *reference, byte *signature, uint32 &len, uint32 bufferlen)
{
	SSL_varvector32 temp;
	
	reference->ExtractHash(temp);

	//DumpTofile(temp, temp.GetLength(), "SignASN1", "pgpstream.txt");
	return SignASN1(reference->HashID(), temp, signature, len, bufferlen);
}

byte *SSLEAY_PublicKeyCipher::SignASN1(SSL_HashAlgorithmType alg, SSL_varvector32 &reference, byte *signature, uint32 &len, uint32 bufferlen)
{
	int nid = 0;

	switch(alg)
	{
	case SSL_MD5:
		nid = NID_md5;
		break;
	case SSL_SHA:
		nid = NID_sha1;
		break;
	case SSL_SHA_224:
		nid = NID_sha224;
		break;
	case SSL_SHA_256:
		nid = NID_sha256;
		break;
	case SSL_SHA_384:
		nid = NID_sha384;
		break;
	case SSL_SHA_512:
		nid = NID_sha512;
		break;
    default      : break;
	}

	len = 0;
	switch (cipher_alg)
	{
    case SSL_RSA : 
		{
			unsigned int len1;
			RSA_sign(nid, reference.GetDirect(), reference.GetLength(), (unsigned char *) signature, &len1, keyspec->pkey.rsa);
			len = len1;
		}
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		{
			unsigned int len1;
			DSA_sign(nid, reference.GetDirect(), reference.GetLength(), signature, &len1, keyspec->pkey.dsa);
			len = len1;
			break;
		}
#endif
#if defined(SSL_DH_SUPPORT) && defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
		{
			unsigned int len1;
			ElGamal_sign(nid, reference.GetDirect(), reference.GetLength(), signature, &len1, &DH_recv_public, keyspec->pkey.dh);
			len = len1;
			break;
		}

#endif
    default:
		break;
    }
	CheckError();
	return signature + len;
}

BOOL SSLEAY_PublicKeyCipher::VerifyASN1(SSL_Hash *reference, const byte *signature, uint32 len)
{
	SSL_varvector32 temp;
	int nid = 0;
	int ret = 0;
	
	switch(reference->HashID())
	{
	case SSL_MD5:
		nid = NID_md5;
		break;
	case SSL_SHA:
		nid = NID_sha1;
		break;
	case SSL_SHA_224:
		nid = NID_sha224;
		break;
	case SSL_SHA_256:
		nid = NID_sha256;
		break;
	case SSL_SHA_384:
		nid = NID_sha384;
		break;
	case SSL_SHA_512:
		nid = NID_sha512;
		break;
	case SSL_RIPEMD160:
		nid = NID_ripemd160;
		break;
    default      : 
		break;
	}
	reference->ExtractHash(temp);

	//DumpTofile(temp, temp.GetLength(), "VerifyASN1", "pgpstream.txt");

	switch (cipher_alg)
	{
    case SSL_RSA : 
		{
			ret = RSA_verify(nid, temp.GetDirect(), temp.GetLength(), (unsigned char *) signature, len, keyspec->pkey.rsa);
		}
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		{
			ret = DSA_verify(nid, temp.GetDirect(), temp.GetLength(), (unsigned char *) signature, len, keyspec->pkey.dsa);
			break;
		}
#endif
#if defined(SSL_DH_SUPPORT) && defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
		{
			ret = ElGamal_verify(nid, temp.GetDirect(), temp.GetLength(), (unsigned char *) signature, len, DH_recv_public, keyspec->pkey.dh);
			break;
		}
		break;
#endif
    default:
		break;
	}
	CheckError();
	return (ret<=0 ? FALSE : TRUE);
}
#endif


BOOL SSLEAY_PublicKeyCipher::Verify(const byte *reference,uint32 len,const byte *signature,uint32 len1)
{
	switch (cipher_alg)
	{
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
#endif
    case SSL_RSA : 
		{
			SSL_varvector32 temp;
			uint32 decryplen1=0;//,decryplen2;
			byte *target;
			
			temp.Resize(len1);
			if(temp.ErrorRaisedFlag)
				return FALSE;
			target = temp.GetDirect();
			use_privatekey = FALSE;
			//InitDecrypt(); // can be ignored Encrypt is single step
			/*target = */
			Decrypt(signature,len1,target,decryplen1, len1);
			//target = FinishDecrypt(target,decryplen2, temp.GetLength()- decryplen1); // can be ignored Encrypt is single step
			temp.Resize(decryplen1 /*+decryplen2*/);
			
			CheckError();
			if(temp.GetLength() == len)
				return (op_memcmp(temp.GetDirect(),reference,(size_t) len) == 0 ? TRUE : FALSE);
		}
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		{
			int ret;
			
			ret = DSA_verify(0,(byte *) reference,(int) len,(byte *) signature,(int) len1,keyspec->pkey.dsa);
			CheckError();
			return (ret<=0 ? FALSE : TRUE);
		}
#endif
    default:
		break;
	}

	return FALSE;
}

SSL_PublicKeyCipher *SSLEAY_PublicKeyCipher::Fork() const
{
	return OP_NEW(SSLEAY_PublicKeyCipher, (this));
}

#ifdef SSL_PUBKEY_DETAILS
uint16 SSLEAY_PublicKeyCipher::PublicCount() const
{
	switch (cipher_alg)
	{
    case SSL_RSA : return 2;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 4;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
    case SSL_DH  : return 4;
#endif
    default      : break;
	}
	return 0;
}

uint16 SSLEAY_PublicKeyCipher::GeneratedPublicCount() const
{
	switch (cipher_alg)
	{
    case SSL_RSA : return 0;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 1;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
    case SSL_DH  : return 1;
#endif
    default      : break;
	}
	return 0;
}
#endif

uint16 SSLEAY_PublicKeyCipher::PublicSize(uint16 item) const
{
	BIGNUM *source= NULL;
	
	switch (cipher_alg)
	{
    case SSL_RSA : 
#ifndef HAS_COMPLEX_GLOBALS
        switch (item) {
        case 0: source = keyspec->pkey.rsa->n; break;
        case 1: source = keyspec->pkey.rsa->e; break;
        }
#else
		if(item < 2)
			source = keyspec->pkey.rsa->*rsa_pub_field[item];
#endif
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		if(item < 4)
			source = keyspec->pkey.dsa->*dsa_pub_field[item];
		break;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
    case SSL_DH  : 
		if(item < 4)
		{
			if(item !=3)
				source = keyspec->pkey.dh->*dh_pub_field[item];
			else
				source = DH_recv_public;
		}
#endif
    default      : break;
	}
	
	return (source != NULL ?  BN_num_bytes(source) : 0);
	
}

#ifdef SSL_PUBKEY_DETAILS
uint16 SSLEAY_PublicKeyCipher::PrivateCount() const
{
	switch (cipher_alg)
	{
    case SSL_RSA : return 4;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 1;
#endif
#if defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal: return 1;
#endif
#if defined(SSL_DH_SUPPORT)
    case SSL_DH  : return 2;
#endif
    default      : break;
	}
	return 0;
}

uint16 SSLEAY_PublicKeyCipher::GeneratedPrivateCount() const
{
	switch (cipher_alg)
	{
    case SSL_RSA : return 0;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : return 0;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
    case SSL_DH  : return 1;
#endif
    default      : break;
	}
	return 0;
}

uint16 SSLEAY_PublicKeyCipher::PrivateSize(uint16 item) const
{
	BIGNUM *source= NULL;
	
	
	switch (cipher_alg)
	{
    case SSL_RSA : 
		if(item < 2)
			source = keyspec->pkey.rsa->*rsa_pub_field[item];
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		if(item < 4)
			source = keyspec->pkey.dsa->*dsa_pub_field[item];
		break;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
		if(item > 0)
			break;
    case SSL_DH  : 
		if(item < 2)
		{
			if(item ==0)
				source = keyspec->pkey.dh->*dh_priv_field[item];
			else
				return DH_calc_common.GetLength();
		}
#endif
    default      : break;
	}
	
	return (source != NULL ?  BN_num_bytes(source) : 0);
}
#endif

void SSLEAY_PublicKeyCipher::LoadPrivateKey(uint16 item, const SSL_varvector16 &source)
{
	BIGNUM *temp = NULL;
	BIGNUM **target= NULL;
	
	ERR_clear_error();
	switch (cipher_alg)
	{
    case SSL_RSA : if(item < 4)
					   target = &(keyspec->pkey.rsa->*rsa_priv_field[item]);
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		if(item == 0)
			target = &(keyspec->pkey.dsa->*dsa_priv_field[item]);
		break;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
    case SSL_DH  :	if(item ==0)
						target = &(keyspec->pkey.dh->*dh_priv_field[0]);
		break;
#endif
    default:
		break;
	}
	
	temp = BN_Vector2bn(source, target);
	if(temp == NULL)
		RaiseAlert(SSL_Internal, SSL_InternalError);
	SetupInformation();
	CheckError();
}

void SSLEAY_PublicKeyCipher::UnLoadPrivateKey(uint16 item, SSL_varvector16 &target)
{
	uint32 temp=0;
	BIGNUM *source= NULL;
	
	ERR_clear_error();
	switch (cipher_alg)
	{
    case SSL_RSA : 
		if(item < 4)
			source = keyspec->pkey.rsa->*rsa_priv_field[item];
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		if(item == 0)
			source = keyspec->pkey.dsa->*dsa_priv_field[item];
		break;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
		if(item != 0)
			break;
    case SSL_DH  : 
		if(item < 2)
		{
			if(item ==0)
				source = keyspec->pkey.dh->*dh_priv_field[0];
			else
			{
				target = DH_calc_common;
				temp = target.GetLength();
				return;
			}
		}
		break;
#endif
    default      : break;
	}
	
	temp = BN_bn2Vector(source, target);
	if(temp == 0)
		target.Resize(0);
	CheckError();
}

void SSLEAY_PublicKeyCipher::UnLoadPublicKey(uint16 item, SSL_varvector16 &target)
{
	int  temp=0;
	BIGNUM *source= NULL;
	
	ERR_clear_error();
	switch (cipher_alg)
	{
    case SSL_RSA : 
		if(item < 2)
		   source = keyspec->pkey.rsa->*rsa_pub_field[item];
		break;
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		if(item < 4)
			source = keyspec->pkey.dsa->*dsa_pub_field[item];
		break;
#endif
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
    case SSL_DH  :  
		if(item < 4)
		{
			if(item !=3)
				source = keyspec->pkey.dh->*dh_pub_field[item];
			else
				source = DH_recv_public;
		}
		break;
#endif
    default      : break;
	}
	
	
	temp = BN_bn2Vector(source, target);
	if(temp == 0)
		target.Resize(0);
	
	if(temp <= 0)
		target.Resize(0);
	CheckError();
}


void SSLEAY_PublicKeyCipher::LoadPublicKey(uint16 item, const SSL_varvector16 &source)
{
	BIGNUM *temp = NULL;
	BIGNUM **target= NULL;
	
	ERR_clear_error();
	switch (cipher_alg)
	{
    case SSL_RSA : 
		if(item < 2)
			target = &(keyspec->pkey.rsa->*rsa_pub_field[item]);
		break;
		
#ifdef SSL_DSA_SUPPORT
    case SSL_DSA : 
		if(item < 4)
			target = &(keyspec->pkey.dsa->*dsa_pub_field[item]);
		break;
#endif // SSL_DSA_SUPPORT
		
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
    case SSL_DH  : 
		if(item < 4)
		{
			if(item !=3)
				target = &(keyspec->pkey.dh->*dh_pub_field[item]);
			else
				target = &DH_recv_public;
		}
		break;
#endif
    default:
		break;
	}
	temp = BN_Vector2bn(source, target);
	if(temp == NULL)
		RaiseAlert(SSL_Internal, SSL_InternalError);
	SetupInformation();
	CheckError();
}

#if defined(SSL_DH_SUPPORT)
void SSLEAY_PublicKeyCipher::ProduceGeneratedPublicKeys()
{
	ERR_clear_error();
	if(CipherID()== SSL_DH)
	{
		BN_free(keyspec->pkey.dh->priv_key);
		keyspec->pkey.dh->priv_key = NULL;
		if(DH_generate_key(keyspec->pkey.dh)<0)
			RaiseAlert(SSL_Internal, SSL_InternalError);
	}

	CheckError();
}
#endif

#if defined(SSL_DH_SUPPORT)
void SSLEAY_PublicKeyCipher::ProduceGeneratedPrivateKeys()
{
	ERR_clear_error();
	if(CipherID()== SSL_DH)
	{
		DH_calc_common.Resize(PublicSize(0));
		if(DH_calc_common.Error())
			return;

		int len2 = 0;
		if((len2 = DH_compute_key(DH_calc_common.GetDirect(), DH_recv_public, keyspec->pkey.dh))<0)
			RaiseAlert(SSL_Internal, SSL_InternalError);

		DH_calc_common.Resize(len2);
		OP_ASSERT(*DH_calc_common.GetDirect() != 0);
	}
	CheckError();
}
#endif

uint32 SSLEAY_PublicKeyCipher::KeyBitsLength() const
{
	switch (cipher_alg)
	{
    case SSL_DSA :
    case SSL_RSA : return EVP_PKEY_bits(keyspec);
#if defined(SSL_DH_SUPPORT) || defined(SSL_ELGAMAL_SUPPORT)
	case SSL_ElGamal:
    case SSL_DH : return 8* DH_size(keyspec->pkey.dh);
#endif
    default      : break;
	}
	return 0;
}

EVP_PKEY *d2i_AutoPublicKey(EVP_PKEY **a, const unsigned char **pp,
	     long length)
{
	EVP_PKEY *pkey=NULL;
	
	if(a)
		*a = NULL;

	if(pp == NULL)
		return NULL;

	const unsigned char *pp1 = *pp;

	X509_PUBKEY *pubkey=NULL;

	ERR_clear_error();
	pubkey = d2i_X509_PUBKEY(&pubkey, &pp1, length);
	if(pubkey)
	{
		pkey = X509_PUBKEY_get(pubkey);
		X509_PUBKEY_free(pubkey);
	}

	if(pkey == NULL)
	{
		pp1 = *pp;
		ERR_clear_error();
		pkey = d2i_PublicKey(EVP_PKEY_RSA, a, &pp1, length);
	}
	
	if(pkey == NULL)
	{
		pp1 = *pp;
		ERR_clear_error();
		pkey = d2i_PublicKey(EVP_PKEY_DSA, a,  &pp1, length);
	}

	*pp = pp1;

	return pkey;
}

#define d2i_PublicKey_Vector(target, source) (EVP_PKEY *) d2i_Vector((void *(*)(void *, unsigned char **, long))d2i_AutoPublicKey, target, source)

void SSLEAY_PublicKeyCipher::LoadAllKeys(SSL_varvector32 &key)
{
	EVP_PKEY *old = keyspec;
	keyspec = NULL;
	
	ERR_clear_error();
	keyspec = d2i_PrivateKey_Vector(&keyspec, key);
	if(keyspec == NULL)
		keyspec = d2i_PublicKey_Vector(&keyspec, key);
	
	if(keyspec == NULL)
	{
		keyspec = old;
		RaiseAlert(SSL_Internal, SSL_InternalError);
	}
	else
	{
		EVP_PKEY_free(old);
		switch(keyspec->type)
		{
		case NID_rsa:
		case NID_rsaEncryption:
			cipher_alg = SSL_RSA;
			break;
		case NID_dsa:
			cipher_alg = SSL_DSA;
			break;
		case NID_dhKeyAgreement:
			cipher_alg = SSL_DH;
			break;
		default:
			cipher_alg = SSL_NoCipher;
		}
	}

	SetupInformation();
	CheckError();
}

#ifdef UNUSED_CODE // SSL_DSA_SUPPORT
void ConvertDSAIntegersToDER_L(SSL_varvector32 &r_target,  SSL_varvector32 &s)
{
	DSA_SIG *sig;

	sig = DSA_SIG_new();
	if(sig == NULL)
	{
		ERR_clear_error();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	BIGNUM *temp = BN_Vector2bn(r_target, &sig->r);
	if(temp == NULL)
	{
		DSA_SIG_free(sig);
		ERR_clear_error();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	temp = BN_Vector2bn(s, &sig->s);
	if(temp == NULL)
	{
		DSA_SIG_free(sig);
		ERR_clear_error();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	if(!i2d_DSA_SIG_Vector(sig, r_target))
	{
		r_target.Resize(0);
		DSA_SIG_free(sig);
		ERR_clear_error();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	DSA_SIG_free(sig);
	ERR_clear_error();
}

void ConvertDSA_DER_ToIntegers_L(SSL_varvector32 &r_source,  SSL_varvector32 &s)
{
	DSA_SIG *sig;


	sig = d2i_DSA_SIG_Vector(NULL, r_source);

	if(sig == NULL)
	{
		r_source.Resize(0);
		s.Resize(0);
		ERR_clear_error();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	uint32 len = BN_bn2Vector(sig->r, r_source);
	if(len == 0)
	{
		r_source.Resize(0);
		s.Resize(0);
		DSA_SIG_free(sig);
		ERR_clear_error();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}
	
	len = BN_bn2Vector(sig->s, s);
	if(len == 0)
	{
		r_source.Resize(0);
		s.Resize(0);
		DSA_SIG_free(sig);
		ERR_clear_error();
		LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	DSA_SIG_free(sig);
	ERR_clear_error();
}
#endif


void SSLEAY_CheckError(SSL_Error_Status *target);

void SSLEAY_PublicKeyCipher::CheckError()
{
	SSLEAY_CheckError(this);
}

#endif
#endif
