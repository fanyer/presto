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
#include "modules/libssl/sslbase.h"

BOOL Valid(SSL_AlertLevel item,SSL_Alert *msg)
{
	switch(item)
	{
    case SSL_NoError:
    case SSL_Warning :
    case SSL_Fatal : break;
    case SSL_Internal:
    default: 
		if(msg != NULL)
			msg->Set(SSL_Internal, SSL_InternalError);
		return FALSE;
	}
	return TRUE;
} 

const SSL_AlertDescription Enum_Alert_Description_List[] = {
	SSL_Close_Notify,
	SSL_Unexpected_Message,
	SSL_Bad_Record_MAC, 
	SSL_Decryption_Failed,
	SSL_Record_Overflow,
	SSL_Decompression_Failure,
	SSL_Handshake_Failure,
	SSL_No_Certificate,
	SSL_Bad_Certificate,
	SSL_Unsupported_Certificate,
	SSL_Certificate_Revoked,
	SSL_Certificate_Expired,
	SSL_Certificate_Unknown,
	SSL_Illegal_Parameter,
	SSL_Unknown_CA,
	SSL_Access_Denied,
	SSL_Decode_Error,
	SSL_Decrypt_Error,
	SSL_Export_Restriction,
	SSL_Protocol_Version_Alert,
	SSL_Insufficient_Security,
	SSL_InternalError,
	SSL_User_Canceled,
	SSL_No_Renegotiation,
	SSL_Restart_Handshake,
	SSL_Renego_PatchUnPatch,
	SSL_Certificate_Verify_Error,
	SSL_Certificate_Purpose_Invalid,
	SSL_Not_TLS_Server,
	SSL_Allocation_Failure,
	SSL_Certificate_Not_Yet_Valid,
	SSL_No_Cipher_Selected,
	SSL_Components_Lacking,
	SSL_Security_Disabled,
	SSL_Bad_PIN,
	SSL_Smartcard_Open,
	SSL_Smartcard_Failed,
	SSL_Authentication_Only_Warning,
	SSL_Certificate_OCSP_error,
	SSL_Certificate_OCSP_Verify_failed,
	SSL_Certificate_CRL_Verify_failed,
	SSL_NoRenegExtSupport,
	SSL_Insufficient_Security1,
	//SSL_Insufficient_Security2,
	SSL_Unknown_CA_WithAIA_URL,
	SSL_Unknown_CA_RequestingUpdate,
	SSL_Untrusted_Cert_RequestingUpdate,
	SSL_Unloaded_CRLs_Or_OCSP,
	SSL_WaitForRepository,
	SSL_Unknown_CA_RequestingExternalUpdate,
	SSL_Bad_Certificate_Status_Response,
	SSL_Unrecognized_Name,
	SSL_Fraudulent_Certificate,
	//SSL_UnSupported_Extension,	// For future use
	//SSL_Certificate_Unobtainable,	// For future use
	SSL_No_Description
};

BOOL Valid(SSL_AlertDescription item,SSL_Alert *msg)
{
	const SSL_AlertDescription *checkitem;
    SSL_AlertDescription testitem;

#ifdef _DEBUG
	checkitem = Enum_Alert_Description_List;
	while((testitem = *(checkitem++)) != SSL_No_Description)
	{
		const SSL_AlertDescription *checkitem2 = checkitem;
		SSL_AlertDescription testitem2;

		while((testitem2 = *(checkitem2++)) != SSL_No_Description)
		{
			OP_ASSERT(testitem2 != testitem);
		}
	}	
#endif
	
	if(item == SSL_No_Description)
		return TRUE;
	
	checkitem = Enum_Alert_Description_List;
	while((testitem = *(checkitem++)) != SSL_No_Description)
	{
		if(item == testitem)
			return TRUE;
	}	
	if(msg != NULL)
		msg->Set(SSL_Internal, SSL_InternalError);
	return FALSE;
} 

const SSL_ContentType Enum_Contenttype_List[]= {
	SSL_ChangeCipherSpec,
	SSL_AlertMessage,
	SSL_Handshake,
	SSL_Application,
	SSL_ContentError
};

BOOL Valid(SSL_ContentType item, SSL_Alert *msg)
{ 
	const SSL_ContentType *checkitem;
    SSL_ContentType testitem;
	
	checkitem = Enum_Contenttype_List;
	while((testitem = *(checkitem++)) != SSL_ContentError)
	{
		if(item == testitem)
			return TRUE;
	}
	
	if(msg != NULL)
		msg->Set(SSL_Internal, SSL_InternalError);
	return FALSE;
	
}

SSL_BulkCipherType SignAlgToCipher(SSL_SignatureAlgorithm sig_alg)
{
	switch(sig_alg)
	{
	case SSL_Anonymous_sign:
		return SSL_NoCipher;
	case SSL_RSA_MD5:
	case SSL_RSA_SHA:
	case SSL_RSA_SHA_256:
	case SSL_RSA_SHA_384:
	case SSL_RSA_SHA_512:
	case SSL_RSA_MD5_SHA_sign:
		return SSL_RSA;
	case SSL_DSA_sign:
		return SSL_DSA;
	}

	return SSL_NoCipher;
}

SSL_HashAlgorithmType SignAlgToHash(SSL_SignatureAlgorithm sig_alg)
{
	switch(sig_alg)
	{
	case SSL_Anonymous_sign:
		return SSL_NoHash;
	case SSL_RSA_MD5:
		return SSL_MD5;
	case SSL_RSA_SHA:
		return SSL_SHA;
	case SSL_RSA_MD5_SHA_sign:
		return SSL_MD5_SHA;
	case SSL_DSA_sign:
		return SSL_SHA;
	case SSL_RSA_SHA_224:
		return SSL_SHA_224;
	case SSL_RSA_SHA_256:
		return SSL_SHA_256;
	case SSL_RSA_SHA_384:
		return SSL_SHA_384;
	case SSL_RSA_SHA_512:
		return SSL_SHA_512;
	}

	return SSL_NoHash;
}

SSL_SignatureAlgorithm SignAlgToBasicSignAlg(SSL_SignatureAlgorithm sig_alg)
{
	switch(sig_alg)
	{
	case SSL_Anonymous_sign:
		return SSL_Anonymous_sign;
	case SSL_RSA_MD5:
	case SSL_RSA_SHA:
	case SSL_RSA_SHA_256:
	case SSL_RSA_SHA_384:
	case SSL_RSA_SHA_512:
	case SSL_RSA_MD5_SHA_sign:
	case SSL_RSA_sign:
		return SSL_RSA_sign;
	case SSL_DSA_sign:
		return SSL_DSA_sign;
	}

	return SSL_Illegal_sign;
}
#endif
