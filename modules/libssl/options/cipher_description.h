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

#ifndef SSLCIPHERDESC_H
#define SSLCIPHERDESC_H

#ifdef _NATIVE_SSL_SUPPORT_
#include "modules/libssl/handshake/cipherid.h"
#include "modules/util/smartptr.h"

class SSL_CipherNames{
public:
    OpStringC8 fullname;
    OpStringC8 KEAname;
    OpStringC8 EncryptName;
    OpStringC8 HashName;
    
    SSL_CipherNames(
        const char *p_fullname,
        const char *p_KEAname,
        const char *p_EncryptName,
        const char *p_HashName
		);
    ~SSL_CipherNames();
};

class SSL_CipherMethod{
public:
    SSL_BulkCipherType method;
    SSL_HashAlgorithmType hash;
    SSL_KeyExchangeAlgorithm  kea_alg;
    SSL_SignatureAlgorithm sigalg;
#ifdef _SUPPORT_TLS_1_2
	SSL_HashAlgorithmType hash_tls12;
	TLS_PRF_func	prf_fun;
#endif
    uint16 KeySize;
    
    SSL_CipherMethod(
        SSL_BulkCipherType p_method,
        SSL_HashAlgorithmType p_hash,
        SSL_KeyExchangeAlgorithm  p_kea_alg,
        SSL_SignatureAlgorithm p_sigalg,
#ifdef _SUPPORT_TLS_1_2
		SSL_HashAlgorithmType p_hash_tls12,
		TLS_PRF_func	p_prf_fun,
#endif
        uint16 p_KeySize
		);
    ~SSL_CipherMethod();
};

class SSL_CipherDescriptions: public Link, public OpReferenceCounter
{ 
private:
    SSL_CipherDescriptions &operator =(const SSL_CipherDescriptions &);
	
public:
    SSL_CipherID id;
    SSL_BulkCipherType method;
    SSL_HashAlgorithmType hash;
    SSL_KeyExchangeAlgorithm  kea_alg;
    SSL_SignatureAlgorithm sigalg;
#ifdef _SUPPORT_TLS_1_2
	SSL_HashAlgorithmType hash_tls12;
	TLS_PRF_func	prf_fun;
#endif
    uint16 KeySize;
	
    OpStringC8 fullname;
    OpStringC8 KEAname;
    OpStringC8 EncryptName;
    OpStringC8 HashName;
    SSL_SecurityRating security_rating;    
    SSL_AuthentiRating authentication;
	BYTE		low_reason;
    
    SSL_CipherDescriptions(const SSL_CipherDescriptions &);
    SSL_CipherDescriptions(
        const SSL_CipherID &p_id,
        const SSL_CipherMethod &method,
		const SSL_CipherNames &names,
        SSL_SecurityRating p_security_rating,    
        SSL_AuthentiRating p_authentication,
		BYTE		low_reason
		);
	void Init(
        const SSL_CipherMethod &method,
        SSL_SecurityRating p_security_rating,    
        SSL_AuthentiRating p_authentication,
		BYTE		low_reason
		);
    virtual ~SSL_CipherDescriptions();    
    SSL_CipherDescriptions* Suc() const{
		return (SSL_CipherDescriptions *)Link::Suc();
    };       //pointer
    SSL_CipherDescriptions* Pred() const{
		return (SSL_CipherDescriptions *)Link::Pred();
    };     //pointer
private:
	void InternalInit(
        const SSL_CipherID &p_id,
        const SSL_CipherMethod &method,
        SSL_SecurityRating p_security_rating,    
        SSL_AuthentiRating p_authentication,
		BYTE		low_reason
		);
};

typedef OpSmartPointerWithDelete<SSL_CipherDescriptions> SSL_CipherDescriptions_Pointer;

class SSL_CipherDescriptionHead: public SSL_Head{
public:
	virtual ~SSL_CipherDescriptionHead();
    SSL_CipherDescriptions *First() const{
		return (SSL_CipherDescriptions *)SSL_Head::First();
    };       //pointer
    SSL_CipherDescriptions* Last() const{
		return (SSL_CipherDescriptions *)SSL_Head::Last();
    };     //pointer
};

#endif
#endif /* SSLCIPHERDESC_H */
