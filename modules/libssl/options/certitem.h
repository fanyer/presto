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

#ifndef SSLCERTITEM_H
#define SSLCERTITEM_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/url/url_sn.h"
#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/optenums.h"

class SSL_CertificateHandler;
class SSL_PublicKeyCipher;

class SSL_CertificateItem : public SSL_Error_Status, public Link
{
public:
    OpString					cert_title;
    SSL_DistinguishedName		name;			// DER encoded (binary)
    SSL_ClientCertificateType	certificatetype;
    SSL_ASN1Cert				certificate;
    SSL_CertificateHandler *	handler;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	SSL_varvector32				cert_id; // SHA_256 of subjectname and pub key
#endif
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	SSL_varvector16_list		extra_crl_list;
#endif

    // client
    SSL_varvector16				private_key_salt;  // If private_key_salt.GetLength() > 0, it means the private key has been encrypted
    SSL_secure_varvector16		private_key;
    SSL_varvector16				public_key_hash;

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	SSL_varvector32				ev_policies;
	SSL_varvector32				decoded_ev_policies;
#endif
    
    //server 
    BOOL						WarnIfUsed;
    BOOL						DenyIfUsed;
    BOOL						PreShipped;

	// Trusted certificates
	time_t	trusted_until;
	ServerName_Pointer trusted_for_name;
	unsigned short trusted_for_port;

	CertAcceptMode	accepted_for_kind;

	// Smartcard
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	BOOL						IsExternalCert;
	SSL_PublicKeyCipher			*external_key;
#endif

	Cert_Update_enum cert_status;
private:
	void InternalInit(const SSL_CertificateItem &);
    
public:    
    virtual ~SSL_CertificateItem(); 
    SSL_CertificateItem(); 
    SSL_CertificateItem(const SSL_CertificateItem &); 
	
    SSL_CertificateItem* Suc() const{
		return (SSL_CertificateItem *)Link::Suc();
    };       //pointer
    SSL_CertificateItem* Pred() const{
		return (SSL_CertificateItem *)Link::Pred();
    };     //pointer


	OP_STATUS SetCertificate(SSL_ASN1Cert &cert, SSL_CertificateHandler *certificates, uint24 item);
	SSL_CertificateHandler *GetCertificateHandler(SSL_Alert *msg=NULL); // DO NOT DELETE! returned object is owned by this structure

};

class SSL_CertificateHead: public SSL_Head{
public:
    SSL_CertificateItem* First() const{
		return (SSL_CertificateItem *)SSL_Head::First();
    };       //pointer
    SSL_CertificateItem* Last() const{
		return (SSL_CertificateItem *)SSL_Head::Last();
    };     //pointer
};

class SSL_Certificate_indirect_list: public Link{
public:
    SSL_CertificateItem *cert_item;  // External
	
    SSL_Certificate_indirect_list* Suc() const{
		return (SSL_Certificate_indirect_list *) Link::Suc();
    };     //pointer
    SSL_Certificate_indirect_list* Pred() const{
		return (SSL_Certificate_indirect_list *) Link::Pred();
    };     //pointer
};  

class SSL_Certificate_indirect_head: public SSL_Head{
public:
    SSL_Certificate_indirect_list* First() const{
		return (SSL_Certificate_indirect_list *) Head::First();
    };     //pointer
    SSL_Certificate_indirect_list* Last() const{
		return (SSL_Certificate_indirect_list *) Head::Last();
    };     //pointer
};  

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLCERTITEM_H */
