/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Yngve Pettersen   <yngve@opera.com>
** @author Haavard Molland   <haavardm@opera.com>
** @author Alexei Khlebnikov <alexeik@opera.com>
**
*/

#include "core/pch.h"

#if defined(CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT)

#include "modules/libcrypto/include/CryptoVerifySignedTextFile.h"
#include "modules/libcrypto/include/CryptoSignature.h"
#include "modules/url/url2.h"
#include "modules/formats/base64_decode.h"


BOOL CryptoVerifySignedTextFile(URL &signed_file, const OpStringC8 &prefix, const unsigned char *key, unsigned long key_len, CryptoHashAlgorithm alg)
{
	if(signed_file.IsEmpty() || (URLStatus) signed_file.GetAttribute(URL::KLoadStatus) != URL_LOADED || key == NULL || key_len == 0)
		return FALSE;

	// Get The raw data
	OpAutoPtr<URL_DataDescriptor> desc(signed_file.GetDescriptor(NULL, URL::KFollowRedirect, TRUE, TRUE));
	if(!desc.get())
		return FALSE;

	BOOL more = FALSE;
	unsigned long pos, buf_len;
	const char *buffer;

	if(desc->RetrieveData(more) == 0 || desc->GetBuffer() == NULL)
		return FALSE;

	if(prefix.HasContent())
	{
		unsigned long i_len = prefix.Length();
		buf_len = desc->GetBufSize();
		if(buf_len <= i_len)
			return FALSE;

		buffer = desc->GetBuffer();
		if(op_memcmp(buffer, prefix.CStr(), i_len) != 0)
			return FALSE;

		buffer+= i_len;

		while(i_len < buf_len && *buffer != '\n' && *buffer != '\r' && op_isspace((unsigned char) *buffer))
		{
			buffer++;
			i_len++;
		}

		desc->ConsumeData(i_len);
	}
	
	if(desc->GetBufSize() == 0)
		return FALSE;
	
	pos= 0;
	do{
		buf_len = desc->GetBufSize();
		buffer = desc->GetBuffer();

		for(pos = 0; pos < buf_len; pos++)
		{
			if(buffer[pos] == '\n')
				break;
		}

		if(pos < buf_len)
			break;

		if(!more || desc->Grow() == 0)
			return FALSE;

		pos = 0;
	}while(desc->RetrieveData(more) > buf_len);

	if(pos == 0)
		return FALSE;

	pos ++;

	if (pos > 1 << 30)
		return FALSE;

	// pos is now the length of b64-encoded signature.
	UINT8 *signature_in = OP_NEWA(UINT8, pos);
	if (!signature_in)
		return FALSE;
	ANCHOR_ARRAY(UINT8, signature_in);

	unsigned long read_len=0; 
	BOOL warning= FALSE;
	const unsigned int signature_in_len = GeneralDecodeBase64((unsigned char *)desc->GetBuffer(), pos, read_len, signature_in, warning);

	if(warning || read_len != pos || signature_in_len == 0)
		return FALSE;

	desc->ConsumeData(pos);

	// We have now signature of length signature_in_len in signature_in array.
	OP_ASSERT(signature_in_len < pos);

	OpAutoPtr<CryptoHash> digester;

	switch (alg) {
#ifdef CRYPTO_HASH_SHA1_SUPPORT // import API_CRYPTO_HASH_SHA1			
		case CRYPTO_HASH_TYPE_SHA1:
			digester.reset(CryptoHash::CreateSHA1());
			break;
#endif
#ifdef CRYPTO_HASH_SHA256_SUPPORT // import API_CRYPTO_HASH_SHA256
		case CRYPTO_HASH_TYPE_SHA256:
			digester.reset(CryptoHash::CreateSHA256());
			break;
#endif
			
		default:
			OP_ASSERT(!"Hash algorithm not supported");
			return FALSE;
	}

	if (!digester.get() || OpStatus::IsError(digester->InitHash()))
		return FALSE;

	do{
		more = FALSE;
		buf_len = desc->RetrieveData(more);
	
		digester->CalculateHash((unsigned char *)desc->GetBuffer(), buf_len);

		desc->ConsumeData(buf_len);
	}while(more);

	UINT8 *signature_out = OP_NEWA(UINT8, digester->Size());
	if (!signature_out)
		return FALSE;
	ANCHOR_ARRAY(UINT8, signature_out);
	
	digester->ExtractHash(signature_out);

	CryptoSignature *sign;
	if (OpStatus::IsError(CryptoSignature::Create(sign, CRYPTO_CIPHER_TYPE_RSA, alg)))
		return FALSE;
	
	OpAutoPtr<CryptoSignature> signature_checker(sign);
	signature_checker->SetPublicKey(key, key_len);

	OP_STATUS status;
	// For some historical reasons, non-ASN1-based signatures are used with SHA1,
	// but ASN1-based ones are used with other hashes.
	if(alg == CRYPTO_HASH_TYPE_SHA1)
		status = signature_checker->Verify(signature_out, digester->Size(), signature_in, signature_in_len);
	else
		status = signature_checker->VerifyASN1(signature_out, digester->Size(), signature_in, signature_in_len);

	if(status == OpStatus::OK)
		return TRUE;

	return FALSE;
}

#endif // CRYPTO_VERIFY_SIGNED_TEXTFILE_SUPPORT
