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

#ifndef SSLSESS_H
#define SSLSESS_H

#if defined _NATIVE_SSL_SUPPORT_

#include "modules/libssl/base/sslprotver.h"
#include "modules/libssl/handshake/cipherid.h"
#include "modules/libssl/handshake/asn1certlist.h"
#include "modules/libssl/handshake/cert_message.h"
#include "modules/libssl/options/cipher_description.h"
#include "modules/libssl/ssl_api.h"

#include "modules/about/opgenerateddocument.h"
#include "modules/util/opstrlst.h"

class SSL_SessionStateRecord{  
public:
    //ServerName_Pointer servername;
    //uint16 port;
    
    SSL_varvector8 sessionID;
	time_t last_used;

	/** The version that is negotiated for this session */
    SSL_ProtocolVersion used_version;
    SSL_CipherID used_cipher;
    SSL_CompressionMethod used_compression;

    SSL_Certificate_st Site_Certificate;
	SSL_Certificate_st Validated_Site_Certificate;
    SSL_Certificate_st Client_Certificate;

#ifndef TLS_NO_CERTSTATUS_EXTENSION
	BOOL ocsp_extensions_sent;
	SSL_varvector32 sent_ocsp_extensions;
	SSL_varvector32 received_ocsp_response;
#endif

    SSL_secure_varvector16 mastersecret;
    SSL_varvector16 keyarg;
    
	//BOOL tls_disabled;
	BOOL used_correct_tls_no_cert;
	BOOL use_correct_tls_no_cert;
    BOOL is_resumable;   
    BOOL session_negotiated;
	SSL_ConfirmedMode_enum UserConfirmed;
    uint32 connections;

    SSL_CipherDescriptions_Pointer cipherdescription;
    int security_rating;
	int low_security_reason;
	SSL_keysizes lowest_keyex;
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	BOOL extended_validation;
#endif
	BOOL renegotiation_extension_supported;

	OpString	ocsp_fail_reason;
    OpString	security_cipher_string;
	
	OpString	Matched_name;
	OpString_list CertificateNames;
	int certificate_status;
	
	SSL_SessionStateRecord();
    virtual ~SSL_SessionStateRecord();

#ifdef _SECURE_INFO_SUPPORT
	URL	*session_information;
	URL_InUse session_information_lock;

	void SetUpSessionInformation(SSL_Port_Sessions *server_info); // located in ssl_sess.cpp
	void DestroySessionInformation();
#endif

private:
	void InternalInit();

	class CertificateInfoDocument : public OpGeneratedDocument
	{
	public:
		CertificateInfoDocument(URL &url, SSL_SessionStateRecord *session, SSL_Port_Sessions *server_info)
			: OpGeneratedDocument(url, OpGeneratedDocument::XHTML5)
			, m_session(session)
			, m_server_info(server_info)
		{};

		virtual OP_STATUS GenerateData();
	private:
		OP_STATUS WriteLocaleString(const OpStringC &prefix, Str::LocaleString id, const OpStringC &postfix);
	protected:
		SSL_SessionStateRecord *m_session;
		SSL_Port_Sessions_Pointer m_server_info;

	};
};

class SSL_SessionStateRecordList : public Link, public SSL_SessionStateRecord {
public:
	SSL_SessionStateRecordList();
	virtual ~SSL_SessionStateRecordList();
	SSL_SessionStateRecordList* Suc() const{
		return (SSL_SessionStateRecordList *)Link::Suc();
	};       //pointer
	SSL_SessionStateRecordList* Pred() const{
		return (SSL_SessionStateRecordList *)Link::Pred();
	};     //pointer
};
#endif
#endif // SSLSTAT_H
