/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
** Yngve Pettersen
*/
#ifndef LIBSSL_SIG_H
#define LIBSSL_SIG_H

#if defined _NATIVE_SSL_SUPPORT_

class SSL_PublicKeyCipher;

#include "modules/libssl/methods/sslphash.h"
#include "modules/libssl/handshake/tlssighash.h"

class SSL_Signature : public SSL_Handshake_Message_Base
{
private:
    TLS_SigAndHash sign_alg;
	uint16 sig_len;
    union{
		struct {} anonymous;
		struct {
			uint8 md5_sha_hash[SSL_MD5_LENGTH+SSL_SHA_LENGTH];
		} md5_sha_rsa_sign;
		struct{
			uint8 sha_hash[SSL_SHA_LENGTH];
		}dsa_sign;
		struct{
			uint8 sha_hash[SSL_SHA_LENGTH];
		}sha_sign;
		struct{
			uint8 sha_hash[SSL_SHA_224_LENGTH];
		}sha_224_sign;
		struct{
			uint8 sha_hash[SSL_SHA_256_LENGTH];
		}sha_256_sign;
		struct{
			uint8 sha_hash[SSL_SHA_384_LENGTH];
		}sha_384_sign;
		struct{
			uint8 sha_hash[SSL_SHA_512_LENGTH];
		}sha_512_sign;
		struct{
			uint8 dummy_hash[SSL_MAX_HASH_LENGTH];
		}dummy;
    };
	BOOL verify_ASN1;
    SSL_varvector16 signature;
    
    SSL_PublicKeyCipher *sigcipher;
    SSL_Hash_Pointer sig_hasher;
    void Init();
    void PerformSigning();
	
public:
    SSL_Signature(); 
	SSL_Signature(SSL_PublicKeyCipher *cipher);
    SSL_Signature(const SSL_Signature &);
    SSL_Signature &operator =(const SSL_Signature &);
    virtual ~SSL_Signature();
    
    uint32 Size() const;
	virtual OP_STATUS PerformStreamActionL(DataStream::DatastreamStreamAction action, DataStream *src_target);
    virtual BOOL Valid(SSL_Alert *msg=NULL) const;
	
    void SetSignatureAlgorithm(SSL_SignatureAlgorithm item){sign_alg = item;sig_hasher=SignAlgToHash(sign_alg);};
    SSL_SignatureAlgorithm GetSignatureAlgorithm()const {return sign_alg;};
    void SetCipher(SSL_PublicKeyCipher *cipher){
		sigcipher = cipher;
    };
    BOOL Verify(SSL_Signature &, SSL_Alert *msg = NULL);
    void InitSigning();
    void Sign(const SSL_varvector16 &source);   
    void FinishSigning();
    void SignHash(SSL_varvector32 &hash_data);   

	void SetTLS12_Mode(){sign_alg.SetEnableRecord(TRUE); verify_ASN1 = TRUE;}

#ifdef _DATASTREAM_DEBUG_
protected:
	virtual const char *Debug_ClassName(){return "SSL_Signature";}
#endif
};
#endif
#endif // LIBSSL_SIG_H
