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
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/libssl/external/openssl/genciph.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/methods/sslcipher.h"
#include "modules/libssl/methods/sslnull.h"
#include "modules/util/cleanse.h"

#include "modules/url/tools/arrays.h"

#include "modules/libssl/debug/tstdump2.h"

#ifdef _DEBUG
#ifdef YNP_WORK
#define TST_DUMP
#endif
#endif

#define SSLEAY_MAX_KEY_LENGTH 24
#define SSLEAY_MAX_IV_LENGTH 8

const EVP_MD *GetMD_From_Algorithm(SSL_HashAlgorithmType digest);


typedef const EVP_CIPHER* (*EVP_CIPHER_CB)();

struct SSL_Cipher_and_NID
{
	SSL_BulkCipherType cipher_alg;
	int nid;
	EVP_CIPHER_CB func;
};

#define CIPHER_ENTRY(typ, ciphername) CONST_TRIPLE_ENTRY(cipher_alg, typ, nid, NID_##ciphername , func, EVP_##ciphername )
#define CIPHER_ENTRY_NID(typ, ciphername, cfb_nid) CONST_TRIPLE_ENTRY(cipher_alg, typ, nid, cfb_nid , func, EVP_##ciphername )

// param2 "name" will be expanded to NID_name and EVP_name
PREFIX_CONST_ARRAY(static, SSL_Cipher_and_NID, SSL_Cipher_map, libssl)
	CIPHER_ENTRY(SSL_RC4, rc4)
#ifndef OPENSSL_NO_AES
	CIPHER_ENTRY(SSL_AES_128_CBC, aes_128_cbc)
	CIPHER_ENTRY(SSL_AES_256_CBC, aes_256_cbc)
#endif
	CIPHER_ENTRY(SSL_3DES, des_ede3_cbc)
#ifdef OPENSSL_USE_CFB_CIPHERS
	CIPHER_ENTRY_NID(SSL_3DES_CFB, des_ede3_cfb, NID_des_ede3_cfb64)
#ifndef OPENSSL_NO_AES
	CIPHER_ENTRY_NID(SSL_AES_128_CFB, aes_128_cfb, NID_aes_128_cfb128)
	CIPHER_ENTRY_NID(SSL_AES_192_CFB, aes_192_cfb, NID_aes_192_cfb128)
	CIPHER_ENTRY_NID(SSL_AES_256_CFB, aes_256_cfb, NID_aes_256_cfb128)
#endif
#ifndef OPENSSL_NO_CAST
	CIPHER_ENTRY_NID(SSL_CAST5_CFB, cast5_cfb, NID_cast5_cfb64)
#endif
#endif
#ifdef SSL_BLOWFISH_SUPPORT
	CIPHER_ENTRY(SSL_BLOWFISH_CBC, bf_cbc)
#endif
#ifdef LIBSSL_USE_RC4_256
	CIPHER_ENTRY_NID(SSL_RC4_256, rc4_256, NID_rc4)
#endif
CONST_END(SSL_Cipher_map)

SSL_GeneralCipher *SSL_API::CreateSymmetricCipher(SSL_BulkCipherType cipher_alg, OP_STATUS &op_err)
{
	op_err = OpStatus::OK;
	
	if(cipher_alg== SSL_NoCipher)
	{
		SSL_GeneralCipher *ret = OP_NEW(SSL_Null_Cipher, ());
		if(ret == NULL)
			op_err = OpStatus::ERR_NO_MEMORY;
		return ret;
	}

	const EVP_CIPHER *cipher = NULL;
	size_t i;
	for(i = 0; i<CONST_ARRAY_SIZE(libssl, SSL_Cipher_map); i++)
	{
		if(g_SSL_Cipher_map[i].cipher_alg == cipher_alg)
		{
			cipher = g_SSL_Cipher_map[i].func();
			break;
		}
	}

	if(cipher == NULL)
	{
		op_err = OpStatus::ERR_OUT_OF_RANGE;
		return NULL;
	}

	OpAutoPtr<SSL_GeneralCipher> key(OP_NEW(SSLEAY_GeneralCipher, (cipher_alg, cipher)));

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


SSLEAY_GeneralCipher::SSLEAY_GeneralCipher(SSL_BulkCipherType cipher_id, const EVP_CIPHER *cipherspec)
{
	encrypt_status = NULL;
	cipher = cipherspec;
	if(cipher == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
	}
	else
	{
		cipher_alg = cipher_id;
		cipher_type =((cipher->flags & EVP_CIPH_MODE)!= EVP_CIPH_CFB_MODE && cipher->block_size >1 ? 
					SSL_BlockCipher : SSL_StreamCipher);
		out_block_size = in_block_size = ((cipher->flags & EVP_CIPH_MODE)!= EVP_CIPH_CFB_MODE ? cipher->block_size : 1);
		key_size = cipher->key_len;
		iv_size = cipher->iv_len;
	}
	InitCipher(NULL);
}

void SSLEAY_GeneralCipher::InitCipher(const EVP_CIPHER_CTX *old)
{
	encrypt_status = OP_NEW(EVP_CIPHER_CTX, ());
	if(encrypt_status == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return;
	}
#ifdef USE_SSL_PGP_CFB_MODE
	PGP_CFB_mode = SSL_Cipher::PGP_CFB_Mode_Disabled;
	PGP_CFB_started = FALSE;
	PGP_CFB_IVread = 0;
#endif

	if(old != NULL)
		*encrypt_status = *old;
	else
	{
		encrypt_status->encrypt = 1;
		if (!EVP_CipherInit(encrypt_status,cipher,NULL,NULL,encrypt_status->encrypt))
			RaiseAlert(SSL_Internal, SSL_InternalError);
	}
}

// Unref YNP
#if 0
SSLEAY_GeneralCipher::SSLEAY_GeneralCipher(const SSLEAY_GeneralCipher *old)
{
	if(old == NULL || old->cipher == NULL)
	{
		RaiseAlert(SSL_Internal, SSL_InternalError);
		return;
	}
	
	cipher = (EVP_CIPHER *)++ old->cipher;
	encrypt_status = NULL;
	cipher_alg = old->CipherID();
	cipher_type =((cipher->flags & EVP_CIPH_MODE)!= EVP_CIPH_CFB_MODE && cipher->block_size >1 ? 
					SSL_BlockCipher : SSL_StreamCipher);	
	out_block_size = in_block_size = ((cipher->flags & EVP_CIPH_MODE)!= EVP_CIPH_CFB_MODE ? cipher->block_size : 1)
	key_size = cipher->key_len;
	iv_size = cipher->iv_len;

	if(old->encrypt_status != NULL)
	{
		InitCipher(old->encrypt_status);
	}
}
#endif

SSLEAY_GeneralCipher::~SSLEAY_GeneralCipher()
{
	cipher = NULL;
	if(encrypt_status != NULL)
	{
		EVP_CIPHER_CTX_cleanup(encrypt_status);
		OPERA_cleanse_heap(encrypt_status,sizeof(EVP_CIPHER_CTX));
		OP_DELETE(encrypt_status);
		encrypt_status = NULL;
	}
#ifdef USE_SSL_PGP_CFB_MODE
	op_memset(PGP_CFB_IVbuffer,0, sizeof(PGP_CFB_IVbuffer));
#endif
}

void SSLEAY_GeneralCipher::SetCipherDirection(SSL_CipherDirection dir)
{
	encrypt_status->encrypt = (dir == SSL_Encrypt ? 1 : 0);	
}

#ifdef USE_SSL_PGP_CFB_MODE
void SSLEAY_GeneralCipher::SetPGP_CFB_Mode(SSL_Cipher::PGP_CFB_Mode_Type mode)
{
	PGP_CFB_mode = mode;
}
#endif

void SSLEAY_GeneralCipher::StartCipher()
{
	EVP_CIPHER_CTX_set_padding(encrypt_status, appendpad != SSL_NO_PADDING);

#ifdef USE_SSL_PGP_CFB_MODE
	PGP_CFB_started = FALSE;
	PGP_CFB_IVread = 0;
	op_memset(PGP_CFB_Resync, 0, sizeof(PGP_CFB_Resync));
#endif
}

byte *SSLEAY_GeneralCipher::CipherUpdate(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	int len2 = 0;

	len1 = 0;

	ERR_clear_error();

#ifdef USE_SSL_PGP_CFB_MODE
	if(PGP_CFB_mode != SSL_Cipher::PGP_CFB_Mode_Disabled && !PGP_CFB_started)
	{
		byte *trg = target;
		const byte *src =source;
		uint32 len_in=0;

		if(encrypt_status->encrypt)
		{
			OP_ASSERT(len + (cipher->iv_len+2)  <= bufferlen);

			SSL_RND(PGP_CFB_IVbuffer, cipher->iv_len);

			PGP_CFB_IVbuffer[cipher->iv_len] = PGP_CFB_IVbuffer[cipher->iv_len-2];
			PGP_CFB_IVbuffer[cipher->iv_len+1] = PGP_CFB_IVbuffer[cipher->iv_len-1];

			src = (byte *) PGP_CFB_IVbuffer;
			len_in = cipher->iv_len+2;
		}
		else
		{
			if(source == NULL || len == 0)
				return target;

			trg = PGP_CFB_IVbuffer + PGP_CFB_IVread;
			len_in = (cipher->iv_len+2) - PGP_CFB_IVread;
			if(len_in > len)
				len_in = len;
		}

		// ALways in CFB mode
		EVP_CipherUpdate(encrypt_status,trg,&len2, src,len_in);

		if(encrypt_status->encrypt)
		{
			op_memcpy(PGP_CFB_Resync, target+2, cipher->iv_len);
			target += len2;
			len1 += len2;
		}
		else
		{
			op_memcpy(PGP_CFB_Resync + PGP_CFB_IVread, src, len_in);

			PGP_CFB_IVread += len_in;

			if(PGP_CFB_IVread < cipher->iv_len+2)
				return target;

			if(PGP_CFB_IVbuffer[cipher->iv_len-2] != PGP_CFB_IVbuffer[cipher->iv_len] ||
				PGP_CFB_IVbuffer[cipher->iv_len-1] != PGP_CFB_IVbuffer[cipher->iv_len+1])
			{
				RaiseAlert(SSL_Fatal, SSL_Decrypt_Error);
				return target;
			}
			op_memmove(PGP_CFB_Resync, PGP_CFB_Resync+2, cipher->iv_len);
			source += len_in;
			len -= len_in;
		}
		PGP_CFB_IVread = 0;
		PGP_CFB_started = TRUE;
		if(PGP_CFB_mode != SSL_Cipher::PGP_CFB_Mode_Enabled_No_Resync)
			CFB_Resync();
	}
#endif

	if(source == NULL || len == 0 || target == NULL || bufferlen == 0)
		return target;
	
	EVP_CipherUpdate(encrypt_status,target,&len2, source,(int) len);

#ifdef USE_SSL_PGP_CFB_MODE
	if(EVP_CIPHER_CTX_mode(encrypt_status) == EVP_CIPH_CFB_MODE)
	{
		const byte *resync_src = target;
		int resync_len = len2;

		if(encrypt_status->encrypt)
		{
			resync_src = target;
			resync_len = len2;
		}
		else
		{
			resync_src = source;
			resync_len = len;
		}


		if(resync_len >= cipher->iv_len)
		{
			op_memcpy(PGP_CFB_Resync, resync_src +(resync_len -cipher->iv_len), cipher->iv_len);
			PGP_CFB_IVread = 0;
		}
		else
		{
			for(int i=0; i< resync_len; i++)
			{
				PGP_CFB_Resync[PGP_CFB_IVread++] = resync_src[i];
				if(PGP_CFB_IVread >= cipher->iv_len)
					PGP_CFB_IVread = 0;
			}
		}
	}
#endif
	len1 += (uint32) len2;
	CheckError();
	
	return target + len1;
}


byte *SSLEAY_GeneralCipher::FinishCipher(byte *target, uint32 &len1,uint32 bufferlen)
{
	int len2=0;
	
	ERR_clear_error();
#ifdef TST_DUMP
	DumpTofile(debug_key, debug_key.GetLength(),"SSLEAY_GeneralCipher Finsish En/Decrypt key ","sslcrypt.txt"); 
#endif
#ifdef USE_SSL_PGP_CFB_MODE
	if(!encrypt_status->encrypt && PGP_CFB_mode != SSL_Cipher::PGP_CFB_Mode_Disabled  && !PGP_CFB_started)
	{
		RaiseAlert(SSL_Fatal, SSL_Decrypt_Error);
		return target;
	}
#endif
	EVP_CipherFinal_ex(encrypt_status,target,&len2);
	len1 = (uint32) len2;
	CheckError();
	
	return target + len1;
}


void SSLEAY_GeneralCipher::InitEncrypt()
{
	StartCipher();
}

byte *SSLEAY_GeneralCipher::Encrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen)
{
	return CipherUpdate(source, len, target, len1, bufferlen);
}

byte *SSLEAY_GeneralCipher::FinishEncrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	return FinishCipher(target,len1, bufferlen);
}

void SSLEAY_GeneralCipher::InitDecrypt()
{
	StartCipher();
}

byte *SSLEAY_GeneralCipher::Decrypt(const byte *source,uint32 len,byte *target, uint32 &len1,uint32 bufferlen) 
{
	return CipherUpdate(source, len, target, len1, bufferlen);
}

byte *SSLEAY_GeneralCipher::FinishDecrypt(byte *target, uint32 &len1,uint32 bufferlen)
{
	return FinishCipher(target,len1, bufferlen);
}

#ifdef USE_SSL_PGP_CFB_MODE
void SSLEAY_GeneralCipher::CFB_Resync()
{
	if(encrypt_status && EVP_CIPHER_CTX_mode(encrypt_status) == EVP_CIPH_CFB_MODE && encrypt_status->num != 0)
	{
		size_t i,j, max;

		i = PGP_CFB_IVread;
		max = cipher->iv_len;
		j = max - i;

		op_memcpy(encrypt_status->iv, PGP_CFB_Resync+ i, j);
		op_memcpy(encrypt_status->iv + j, PGP_CFB_Resync, i);

		encrypt_status->num = 0;
	}
}

void SSLEAY_GeneralCipher::UnloadPGP_Prefix(SSL_varvector32 &buf)
{
	buf.Set(PGP_CFB_IVbuffer, cipher->iv_len+2);
}
#endif


const byte *SSLEAY_GeneralCipher::LoadKey(const byte *source)
{
#ifdef TST_DUMP
	DumpTofile(source, cipher->key_len,"SSLEAY_GeneralCipher LoadKey ","sslcrypt.txt"); 
	debug_key.Set(source, cipher->key_len); 
#endif
	ERR_clear_error();
	EVP_CipherInit(encrypt_status,NULL,(byte *) source,NULL, encrypt_status->encrypt);
	CheckError();
	return source + cipher->key_len;
}

const byte *SSLEAY_GeneralCipher::LoadIV(const byte *source)
{
#ifdef TST_DUMP
	DumpTofile(source, cipher->iv_len,"SSLEAY_GeneralCipher LoadIV ","sslcrypt.txt"); 
#endif
	ERR_clear_error();
	EVP_CipherInit(encrypt_status,NULL,NULL,(byte *) source, encrypt_status->encrypt);
	CheckError();
	return source + cipher->iv_len;
}

void SSLEAY_GeneralCipher::BytesToKey(SSL_HashAlgorithmType mdtype, 
									  const SSL_varvector32 &string,  const SSL_varvector32 &salt, int count)
{
	byte *key= (byte *) g_memory_manager->GetTempBuf2k();
	byte *iv = key+ SSLEAY_MAX_KEY_LENGTH;
	const EVP_MD *md = GetMD_From_Algorithm(mdtype);
	
	EVP_BytesToKey(cipher, md, (byte *) salt.GetDirect(), (byte *) string.GetDirect(),
		(int) string.GetLength(), count, key, iv);
	
	EVP_CipherInit(encrypt_status,NULL,key,iv,encrypt_status->encrypt);
	OPERA_cleanse_heap(key,SSLEAY_MAX_KEY_LENGTH+SSLEAY_MAX_IV_LENGTH);
}

/*
SSL_GeneralCipher *SSLEAY_GeneralCipher::Fork()  const
{
return new SSLEAY_GeneralCipher(this);
}*/

void SSLEAY_CheckError(SSL_Error_Status *target);

void SSLEAY_GeneralCipher::CheckError()
{
	SSLEAY_CheckError(this);
}

#endif
#endif
