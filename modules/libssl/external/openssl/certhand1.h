/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _SSL_CERTHAND_H
#define _SSL_CERTHAND_H
#if defined _NATIVE_SSL_SUPPORT_ &&  defined(_SSL_USE_OPENSSL_)

#include "modules/libopeay/openssl/ocsp.h"
#include "modules/about/opgenerateddocument.h"
#include "modules/libssl/methods/sslphash.h"
#include "modules/libssl/handshake/asn1certlist.h"
#include "modules/libssl/certs/certhandler.h"

class CRL_item;

void UpdateX509PointerAndReferences(X509 *&dest, X509 *new_value, BOOL inc = FALSE);

class SSLEAY_CertificateHandler : public SSL_CertificateHandler
{
private:
	friend int SSL_get_issuer(X509 **issuer, X509_STORE_CTX *ctx, X509 *x);
	friend int SSL_check_revocation(X509_STORE_CTX *ctx);

	SSL_Options *options;
	BOOL is_standalone; // TRUE means that selfsigned need not be from reposiroty to be OK

	uint24 certificatecount;
    X509 *endcertificate;
    X509 *firstissuer;
    X509 *topcertificate;
    struct x509_cert_stack_st
	{
		X509 *certificate;
		int actual_index;
		BOOL visited;
		BOOL no_issuer;
		BOOL invalid,expired,not_yet_valid;
		BOOL invalid_crl, no_crl;
		BOOL have_ocsp;
		BOOL have_crl;
		BOOL incorrect_caflags;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
		BOOL extended_validation;
#endif
		BOOL warn_about_cert;
		BOOL allow_access;
		BOOL preshipped_cert;
		BOOL from_store;

#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
		BOOL used_md5;
#endif
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
		BOOL used_sha1;
#endif

		OpString issuer_name;
		OpString subject_name;
		OpString expiration_date;
		OpString valid_from_date;
		
		void ResetEntry()
		{
			actual_index = -1;
			visited = invalid = expired = not_yet_valid = no_issuer = incorrect_caflags = FALSE;
			invalid_crl = no_crl = have_ocsp = have_crl = warn_about_cert = preshipped_cert = from_store = FALSE;
#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
			used_md5 = FALSE;
#endif
#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
			used_sha1 = FALSE;
#endif
			allow_access = TRUE;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
			extended_validation =FALSE;
#endif
		}
		x509_cert_stack_st(){
			certificate = NULL;
			ResetEntry();
		};
		~x509_cert_stack_st(){
			UpdateX509PointerAndReferences(certificate, NULL);
			certificate = NULL;
		};
    } *certificate_stack;
	
	STACK_OF(X509) *chain;
	STACK_OF(X509) *extra_certs_chain;
    X509_STORE_CTX *certificates;
    SSL_varvector16_list fingerprints_sha1;
    SSL_varvector16_list fingerprints_sha256;
	SSL_Hash_Pointer fingerprintmaker_sha1;
	SSL_Hash_Pointer fingerprintmaker_sha256;
	OCSP_REQUEST *sent_ocsp_request;

#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	BOOL set_crls;
	CRL_List *current_crls;
	CRL_List *current_crls_new;

	// This callback variable is used to store libopeay's original revocation check algorithm
	// We intercept the callback to go through the certificate chain and make sure that only
	// CRLs from certificate in the chain actually used is checked.
	int (*libopeay_check_revocation)(X509_STORE_CTX *ctx); /* Check revocation status of chain */
#endif

	SSL_ASN1Cert_list validated_certificates;

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	SSL_varvector32 pending_issuer_id;
#endif

private:
	void Init();
	void Clear();

	void GetX509NameAttributeString(uint24 item, SSL_DistinguishedName &name, X509_NAME *(*)(X509 *), const X509 *cert=NULL) const;
	OP_STATUS GetName(uint24 item,OpString_list &name, X509_NAME *(*func)(X509 *)) const;
	BOOL PrepareCertificateStorage(uint24 count);
	void FinalizeCertificateStorage();

public:
	SSLEAY_CertificateHandler();
	SSLEAY_CertificateHandler(const SSLEAY_CertificateHandler &);
	SSLEAY_CertificateHandler(const SSLEAY_CertificateHandler *);
	virtual ~SSLEAY_CertificateHandler();
	
	virtual void LoadCertificate(SSL_ASN1Cert_list &);
	virtual void LoadCertificate(SSL_ASN1Cert &);
    virtual void LoadCertificates(STACK_OF(X509) *crts);
#ifdef _SUPPORT_SMIME_
    virtual void LoadCertificate(X509 *);
	virtual BOOL SelectCertificate(PKCS7_ISSUER_AND_SERIAL *issuer);

	//virtual void AddCertificate(X509 *);
	//virtual void AddLookup(SSL_CertificateHandler *);
	//virtual void SetLookUpInContacts(BOOL);
#endif
    virtual void LoadExtraCertificate(SSL_ASN1Cert &);
	virtual BOOL VerifySignatures(SSL_CertificatePurpose purpose, SSL_Alert *msg=NULL, 
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
									CRL_List *crl_list_enabled=NULL,
#endif // LIBSSL_ENABLE_CRL_SUPPORT
									SSL_Options *opt = NULL, // Non-null means retrieve certs from here, pre-initialized
									BOOL standalone = FALSE // TRUE means that selfsigned need not be from reposiroty to be OK
		);
	static int Verify_callback(int OK_status,X509_STORE_CTX *ctx);
    //virtual BOOL CheckIssuedBy(uint24 i, SSL_ASN1Cert &,SSL_Alert *msg=NULL);
	virtual void GetPublicKeyHash(uint24 item, SSL_varvector16 &) const;
	virtual void GetFingerPrint(uint24 item, SSL_varvector16 &) const;
	
	virtual uint24 CertificateCount() const;
	virtual void GetValidatedCertificateChain(SSL_ASN1Cert_list &certs);
	//virtual void GetSerialNumber(uint24 item,SSL_varvector32 &number) const;
	virtual void GetSubjectName(uint24 item,SSL_DistinguishedName &) const;
	virtual OP_STATUS GetSubjectName(uint24 item, OpString_list &)const;
	virtual void GetIssuerName(uint24 item,SSL_DistinguishedName &) const;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
    virtual void GetIssuerID(uint24 item,SSL_varvector32 &) const;
	virtual void GetSubjectID(uint24 item,SSL_varvector32 &target, BOOL always_use_pubkey=FALSE) const;
#endif
	virtual OP_STATUS GetIssuerName(uint24 item, OpString_list &)const;
	virtual void GetServerName(uint24 item,SSL_ServerName_List &) const;

    virtual OP_STATUS GetCertificateInfo(uint24 item, URL &target, Str::LocaleString description, SSL_Certinfo_mode info_type)const;
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	virtual	BOOL	CollectCRLs(CRL_List *precollected_crls, CRL_List *collect_crls, BOOL &CRL_distribution);
			//BOOL	CollectCRLsOfChain(X509 *x, CRL_List *precollected_crls, CRL_List *collect_crls);
			BOOL	CollectCRLs(X509 *x, CRL_List *precollected_crls, CRL_List *collect_crls, BOOL &CRL_distribution);
			BOOL	LoadCRLs(CRL_List *collect_crls);
			BOOL    LoadCRL_item(CRL_item *item);
#endif

private:
	class CertInfoWriter : public OpGeneratedDocument
	{
	protected:
		X509 *m_cert;
		Str::LocaleString m_description;
		uint32 m_item;
		SSL_varvector32 m_fingerprint_sha1;
		SSL_varvector32 m_fingerprint_sha256;
		SSL_Certinfo_mode info_output_type;
	public:
		CertInfoWriter(URL &url, SSL_Certinfo_mode info_type,  X509 *cert, Str::LocaleString description, uint24 item_num, const SSL_varvector32 &sha1, const SSL_varvector32 &sha256 )
			: OpGeneratedDocument(url, OpGeneratedDocument::XHTML5),
			m_cert(cert) ,
			m_description(description),
			m_item(item_num),
			m_fingerprint_sha1(sha1),
			m_fingerprint_sha256(sha256),
			info_output_type(info_type)
		{};

		virtual OP_STATUS GenerateData();
		void GenerateDataL();

	private:
		OP_STATUS WriteLocaleString(const OpStringC &prefix, Str::LocaleString id, const OpStringC &postfix);
		void StringAppendHexLine(const OpStringC &head, const byte *source,uint32 offset, uint32 maxlen, uint16 linelen);
		void StringAppendHexOnly(const OpStringC &head, const byte *source, uint32 len, uint16 linelen,const OpStringC &tail);
		void StringAppendHexAndPrintable(const OpStringC &head, const byte *source, uint32 len, uint16 linelen,const OpStringC &tail);
		OP_STATUS Parse_name(X509_NAME *name, BOOL use_xml);
		OP_STATUS ConvertString(ASN1_STRING *string, BOOL use_xml);

	};

public:

	// char *GetParsedCertificateInfo(uint24 item, uint32 &linecount) const;
	// void GetParsedCertificateInfo(uint24 item, SSL_varvector32 &target) const;
	
	virtual SSL_PublicKeyCipher *GetCipher(uint24) const;
	
	virtual uint32 KeyBitsLength(uint24) const;
	//    BOOL SignOnly(uint24) const;
	//    BOOL EncodeOnly(uint24) const;
	//    BOOL EncodeAndSign(uint24) const;
	virtual BOOL SelfSigned(uint24) const;
	virtual SSL_ClientCertificateType CertificateType(uint24 item) const;
	virtual SSL_SignatureAlgorithm CertificateSignatureAlg(uint24 item) const;
	virtual SSL_SignatureAlgorithm CertificateSigningKeyAlg(uint24 item) const;
	
	//    BOOL NotInBase(uint24) const;
	//    BOOL IssuerNotInBase(uint24) const;
	//    BOOL Invalid(uint24) const;
	//    BOOL Expired(uint24) const;
	//    BOOL Newer(uint24) const;
	//    BOOL Older(uint24) const;
	virtual SSL_ExpirationType CheckIsExpired(uint24, time_t spec_date=0);
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	virtual SSL_ExpirationType CheckIsExpired(uint24, OpString8 &spec_date);
#endif

	virtual SSL_CertificateVerification_Info *ExtractVerificationStatus(uint24 &number);
	
#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
	virtual BOOL UsagePermitted(SSL_CertificatePurpose usage_type);
#endif
	
	virtual SSL_CertificateHandler *Fork() const;
	void CheckError();
	
	virtual OP_STATUS VerifyOCSP_Response(SSL_varvector32 &binary_response);
	virtual OP_STATUS GetOCSP_Request(OpString_list &ocsp_url_strings, SSL_varvector32 &binary_request
#ifndef TLS_NO_CERTSTATUS_EXTENSION
		, SSL_varvector32 &premade_ocsp_extensions
#endif
		);

	virtual OP_STATUS GetIntermediateCA_Requests(uint24 index, OpString_list &ocsp_url_strings);

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	virtual void GetPendingIssuerId(SSL_varvector32 &id);
#endif

private:
	OP_STATUS Get_AIA_URL_Information(uint24 index, OpString_list &aia_url_strings, int nid);

};

int SSLEAY_CertificateHandler_Verify_callback(int OK_status,X509_STORE_CTX *ctx);

#endif
#endif /* CERTHAND_H */
