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

#ifndef SSLCERTHANDLER_H
#define SSLCERTHANDLER_H

#ifdef _NATIVE_SSL_SUPPORT_

#include "modules/util/opstrlst.h"
#include "modules/libssl/options/certitem.h"
#include "modules/locale/locale-enum.h"

class CRL_List;
class SSL_PublicKeyCipher;
class SSL_ServerName_List;
struct SSL_CertificateVerification_Info;
class SSL_CertificateItem;

#if defined _SSL_USE_OPENSSL_
struct x509_st;
typedef struct x509_st X509;
#endif

class SSL_CertificateHandler : public SSL_Error_Status{
public: 
	
    //virtual ~SSL_CertificateHandler();
    
    virtual void LoadCertificate(SSL_ASN1Cert_list &)=0;
    virtual void LoadCertificate(SSL_ASN1Cert &)=0;
#ifdef _SUPPORT_SMIME_
    virtual void LoadCertificate(X509 *)=0;
    virtual void LoadCertificates(STACK_OF(X509) *crts)=0;
	virtual BOOL SelectCertificate(PKCS7_ISSUER_AND_SERIAL *issuer)=0;
	//virtual void AddCertificate(X509 *)=0;
	//virtual void AddLookup(SSL_CertificateHandler *) = 0;
	//virtual void SetLookUpInContacts(BOOL) = 0;
#endif
    virtual void LoadExtraCertificate(SSL_ASN1Cert &)=0;
    virtual BOOL VerifySignatures(SSL_CertificatePurpose purpose,SSL_Alert *msg=NULL,
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
						CRL_List *crl_list=NULL,
#endif // LIBSSL_ENABLE_CRL_SUPPORT
						SSL_Options *opt = NULL, // Non-null means retrieve certs from here, pre-initialized
						BOOL standalone = FALSE // TRUE means that selfsigned need not be from reposiroty to be OK
		) = 0;
    //virtual BOOL CheckIssuedBy(uint24 i, SSL_ASN1Cert &,SSL_Alert *msg=NULL) = 0;
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	virtual	BOOL CollectCRLs(CRL_List *precollected_crls, CRL_List *collect_crls, BOOL &CRL_distribution)=0;
#endif // LIBSSL_ENABLE_CRL_SUPPORT
	
    virtual uint24 CertificateCount() const =0;
	virtual void GetValidatedCertificateChain(SSL_ASN1Cert_list &certs)=0;
    virtual void GetSubjectName(uint24 item,SSL_DistinguishedName &) const = 0;
    virtual OP_STATUS GetSubjectName(uint24 item, OpString_list &)const = 0;
    virtual void GetIssuerName(uint24 item,SSL_DistinguishedName &) const = 0;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
    virtual void GetIssuerID(uint24 item,SSL_varvector32 &) const = 0;
	/** Generate an ID based on the subject name and the public key */
	virtual void GetSubjectID(uint24 item,SSL_varvector32 &target, BOOL always_use_pubkey=FALSE) const = 0;
#endif
    virtual OP_STATUS GetIssuerName(uint24 item, OpString_list &)const = 0;
	//virtual void GetSerialNumber(uint24 item, SSL_varvector32 &) const = 0;
    virtual void GetServerName(uint24 item, SSL_ServerName_List &) const = 0;
    // virtual char *GetParsedCertificateInfo(uint24 item, uint32 &bytecount) const = 0;
	//  virtual void GetParsedCertificateInfo(uint24 item, SSL_varvector32 &target) const = 0;

	virtual OP_STATUS GetCertificateInfo(uint24 item, URL &target, Str::LocaleString description=Str::NOT_A_STRING, SSL_Certinfo_mode info_type= SSL_Cert_XML)const = 0;


	virtual void GetPublicKeyHash(uint24 item, SSL_varvector16 &) const = 0;

	/** Get the SHA 256 fingerprint for the identified certificate */
	virtual void GetFingerPrint(uint24 item, SSL_varvector16 &) const = 0;
	
    virtual SSL_PublicKeyCipher *GetCipher(uint24) const=0;
    
    virtual uint32 KeyBitsLength(uint24) const = 0;
	//   virtual BOOL SignOnly(uint24) const = 0;
	//    virtual BOOL EncodeOnly(uint24) const = 0;
	//    virtual BOOL EncodeAndSign(uint24) const = 0;
    virtual BOOL SelfSigned(uint24) const = 0;
    virtual SSL_ClientCertificateType CertificateType(uint24 item) const = 0;
	virtual SSL_SignatureAlgorithm CertificateSignatureAlg(uint24 item) const=0;
	virtual SSL_SignatureAlgorithm CertificateSigningKeyAlg(uint24 item) const=0;
    
	/*    virtual BOOL IssuerNotInBase(uint24) const = 0;
    virtual BOOL NotInBase(uint24) const = 0;
    virtual BOOL Invalid(uint24) const = 0;
    virtual BOOL Expired(uint24) const = 0;
    virtual BOOL Newer(uint24) const = 0;
    virtual BOOL Older(uint24) const = 0;*/
	virtual SSL_ExpirationType CheckIsExpired(uint24, time_t spec_date=0) =0;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	virtual SSL_ExpirationType CheckIsExpired(uint24, OpString8 &spec_date) =0;
#endif

	virtual OP_STATUS GetIntermediateCA_Requests(uint24 index, OpString_list &ocsp_url_strings) = 0;

	virtual OP_STATUS GetOCSP_Request(OpString_list &ocsp_url_strings, SSL_varvector32 &binary_request
#ifndef TLS_NO_CERTSTATUS_EXTENSION
		, SSL_varvector32 &premade_ocsp_extensions
#endif
		)=0;
	virtual	OP_STATUS VerifyOCSP_Response(SSL_varvector32 &binary_response)=0;

	virtual SSL_CertificateVerification_Info *ExtractVerificationStatus(uint24 &number)=0; // assumed to be long enough
    
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	virtual BOOL UsagePermitted(SSL_CertificatePurpose usage_type) = 0;
#endif
    
    virtual SSL_CertificateHandler *Fork() const = 0;

	OP_STATUS GetCommonName(uint24 item, OpString &title);

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	virtual void GetPendingIssuerId(SSL_varvector32 &id)=0;
#endif
};

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
class SSL_Certificate_And_Key_List;
#endif

class SSL_CertificateHandler_List : public Link{
public:
    SSL_CertificateItem *certitem; // External
    SSL_Certificate_indirect_head ca_list; 
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	SSL_Certificate_And_Key_List *external_key_item; // owned
#endif
    
    SSL_CertificateHandler_List();
    virtual ~SSL_CertificateHandler_List();
    SSL_CertificateHandler_List* Suc() const{
		return (SSL_CertificateHandler_List *) Link::Suc();
    };     //pointer
    SSL_CertificateHandler_List* Pred() const{
		return (SSL_CertificateHandler_List *) Link::Pred();
    };     //pointer
};

class SSL_CertificateHandler_ListHead : public SSL_Head{
public:
    SSL_CertificateHandler_List* Item(int);
	// const SSL_CertificateHandler_List* Item(int) const;
    SSL_CertificateHandler_List* First() const{
		return (SSL_CertificateHandler_List *)SSL_Head::First();
    };       //pointer
    SSL_CertificateHandler_List* Last() const{
		return (SSL_CertificateHandler_List *)SSL_Head::Last();
    };     //pointer
};


class SSL_Cert_Revoked: public ListElement<SSL_Cert_Revoked>
{
public:

	SSL_varvector16 fingerprint;
	SSL_ASN1Cert cert;

public:
	~SSL_Cert_Revoked(){
		if(InList())
			Out();
	}

};

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSLCERTHANDLER_H */
