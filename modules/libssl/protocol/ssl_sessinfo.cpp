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

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/sslbase.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/options/cipher_description.h"
#include "modules/libssl/certs/certhandler.h"

#include "modules/url/url2.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_man.h"
#include "modules/url/protocols/http1.h"

#include "modules/hardcore/mem/mem_man.h"

#ifdef _SECURE_INFO_SUPPORT
#include "modules/util/htmlify.h"
#endif

#ifdef _SECURE_INFO_SUPPORT

void SSL_SessionStateRecord::SetUpSessionInformation(SSL_Port_Sessions *server_info)
{
	URL url = urlManager->GetNewOperaURL();
	if (url.IsEmpty())
		return;
#if defined LIBSSL_COMPRESS_CERT_INFO
	url.SetAttribute(URL::KCompressCacheContent, TRUE);
#endif
	CertificateInfoDocument doc(url, this, server_info);
	OP_STATUS op_err = doc.GenerateData();
	if(OpStatus::IsFatal(op_err))
		g_memory_manager->RaiseCondition(op_err);
}

OP_STATUS PrintCertToURL(URL &target, SSL_Certificate_st &certificate, Str::LocaleString description);

OP_STATUS  SSL_SessionStateRecord::CertificateInfoDocument::WriteLocaleString(const OpStringC &prefix, Str::LocaleString id, const OpStringC &postfix)
{
	OpString tr;
	m_url.WriteDocumentData(URL::KNormal, prefix);
	RETURN_IF_ERROR(SetLangString(id, tr));
	OP_ASSERT(tr.HasContent()); // Language string missing - check LNG file
	m_url.WriteDocumentData(URL::KNormal, tr.CStr());
	m_url.WriteDocumentData(URL::KNormal, postfix);
	return OpStatus::OK;
}

OP_STATUS SSL_SessionStateRecord::CertificateInfoDocument::GenerateData()
{
	RETURN_IF_ERROR(OpenDocument(Str::S_MSG_SSL_TEXT_SECINFO, PrefsCollectionFiles::StyleCertificateInfoPanelFile, FALSE));
	RETURN_IF_ERROR(OpenBody());
	m_url.SetAttribute(URL::KLoadStatus, URL_LOADING);

	RETURN_IF_ERROR(WriteLocaleString( UNI_L("<securityinformation>\n<h1>"), Str::S_MSG_SSL_TEXT_SECINFO, UNI_L(": <secureserver><url>")));
	
	OpStringC server_name(m_server_info->GetServerName()->UniName());
	OpStringC8 server_name8(m_server_info->GetServerName()->Name());

	m_url.WriteDocumentData(URL::KHTMLify, server_name);
	m_url.WriteDocumentDataUniSprintf(UNI_L("</url>:<port>%d</port></secureserver></h1>\r\n"),m_server_info->Port());
	
	RETURN_IF_ERROR(WriteLocaleString(UNI_L("<table>\r\n<tr><th>"), Str::S_MSG_SSL_TEXT_SECPROTO, UNI_L("</th><td><protocol>")));
	m_url.WriteDocumentData(URL::KHTMLify, m_session->security_cipher_string);
	m_url.WriteDocumentData(URL::KNormal, UNI_L("</protocol></td></tr>\r\n"));
	
	if(m_session->Matched_name.HasContent())
	{
		RETURN_IF_ERROR(WriteLocaleString(UNI_L("<tr><th>"), Str::S_MSG_SSL_TEXT_VALIDATED_SERVERNAME, UNI_L("</th><td><certificate_servername>")));
		m_url.WriteDocumentData(URL::KHTMLify, m_session->Matched_name);
		m_url.WriteDocumentData(URL::KNormal, UNI_L("</certificate_servername>"));

		if(m_session->Matched_name.CompareI(server_name) != 0)
		{
			m_url.WriteDocumentData(URL::KNormal, UNI_L(" (<servername>"));
			m_url.WriteDocumentData(URL::KHTMLify, server_name);
			m_url.WriteDocumentData(URL::KNormal, UNI_L("</servername>"));

			if(server_name.CompareI(server_name8) != 0)
			{
				m_url.WriteDocumentData(URL::KNormal, UNI_L(", "));
				m_url.WriteDocumentData(URL::KNormal, UNI_L("<uniservername>"));
				OpString name16;
				LEAVE_IF_ERROR(name16.Set(server_name8));
				m_url.WriteDocumentData(URL::KHTMLify, name16);
				m_url.WriteDocumentData(URL::KNormal, UNI_L("</uniservername>"));
			}
			m_url.WriteDocumentData(URL::KNormal, UNI_L(")"));
		}
		m_url.WriteDocumentData(URL::KNormal, UNI_L("</td></tr>\r\n"));
	}

	unsigned long name_count =m_session->CertificateNames.Count();
	unsigned long name_i;
	if(name_count > 0)
	{
		RETURN_IF_ERROR(WriteLocaleString(UNI_L("<tr><th>"), Str::S_MSG_SSL_TEXT_ISSUED_TO, UNI_L("</th><td><issued_to>")));
		m_url.WriteDocumentData(URL::KHTMLify, m_session->CertificateNames[0]);
		m_url.WriteDocumentData(URL::KNormal, UNI_L("</issued_to></td></tr>\r\n"));
	}
	for(name_i = 1; name_i < name_count; name_i++)
	{
		RETURN_IF_ERROR(WriteLocaleString(UNI_L("<tr><th>"), Str::S_MSG_SSL_TEXT_ISSUED_BY, UNI_L("</th><td><issued_by>")));
		m_url.WriteDocumentData(URL::KHTMLify, m_session->CertificateNames[name_i]);
		m_url.WriteDocumentData(URL::KNormal, UNI_L("</issued_by></td></tr>\r\n"));
	}

	m_url.WriteDocumentData(URL::KNormal, UNI_L("</table>\r\n"));

	if(m_session->UserConfirmed!=NO_INTERACTION)
	{

		if(m_session->UserConfirmed==PERMANENTLY_CONFIRMED)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<permanentlyconfirmed>"));
		else
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<userconfirmedcertificate>"));

		if ((m_session->certificate_status & SSL_CERT_ANONYMOUS_CONNECTION) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<Anonymous_Connection/>"));

		if((m_session->certificate_status & SSL_CERT_NO_ISSUER) != 0 || (
			m_session->certificate_status & SSL_CERT_UNKNOWN_ROOT) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<unknownca/>"));
		
		if((m_session->certificate_status & SSL_CERT_INCORRECT_ISSUER) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<Bad_CA_Flags/>"));

		if ((m_session->certificate_status & SSL_CERT_NOT_YET_VALID) != 0)
		{
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<CertNotYetValid/>"));
		}
		else if((m_session->certificate_status & SSL_CERT_EXPIRED) != 0) 
		{
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<Certexpired/>"));
		}

		if((m_session->certificate_status & SSL_CERT_WARN_MASK) == SSL_CERT_WARN)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<ConfiguredWarning/>"));

		if ((m_session->certificate_status & SSL_CERT_AUTHENTICATION_ONLY) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<AuthenticationOnly_NoEncryption/>"));

		if ((m_session->certificate_status & SSL_CERT_LOW_ENCRYPTION) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<LowEncryptionlevel/>\r\n"));
		
		if((m_session->certificate_status & SSL_CERT_NAME_MISMATCH) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<ServernameMismatch/>\r\n"));

		if(m_session->UserConfirmed==PERMANENTLY_CONFIRMED)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("</permanentlyconfirmed>"));
		else
			m_url.WriteDocumentData(URL::KNormal, UNI_L("</userconfirmedcertificate>\r\n"));

#if !defined LIBOPEAY_DISABLE_MD5_PARTIAL
		if((m_session->certificate_status & SSL_USED_MD5) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<WarningMd5/>\r\n"));
#endif

#if !defined LIBOPEAY_DISABLE_SHA1_PARTIAL
		if((m_session->certificate_status & SSL_USED_SHA1) != 0)
			m_url.WriteDocumentData(URL::KNormal, UNI_L("<WarningSha1/>\r\n"));
#endif

	}

	if(m_session->low_security_reason & SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE)
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<OcspRequestFailed/>\r\n"));

	if(m_session->low_security_reason & SECURITY_LOW_REASON_OCSP_FAILED)
	{
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<OcspResponseFailed reason=\""));
		m_url.WriteDocumentData(URL::KHTMLify, m_session->ocsp_fail_reason);
		m_url.WriteDocumentData(URL::KNormal, UNI_L("\"/>\r\n"));
	}

	if(m_session->low_security_reason & (SECURITY_LOW_REASON_UNABLE_TO_CRL_VALIDATE | SECURITY_LOW_REASON_CRL_FAILED))
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<CrlFailed/>\r\n"));

#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	if(m_session->extended_validation)
	{
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<ExtendedValidationCert/>\r\n"));
	}
#endif
	if(!m_session->renegotiation_extension_supported)
	{
		m_url.WriteDocumentData(URL::KNormal, UNI_L("<RenegExtensionUnsupported/>\r\n"));
	}

	m_url.WriteDocumentData(URL::KNormal, UNI_L("<servercertchain>\r\n"));
	
	if(m_session->Site_Certificate.Count())
		RETURN_IF_ERROR(PrintCertToURL(m_url, m_session->Site_Certificate, Str::S_MSG_SSL_TEXT_SERVERCERT));

	m_url.WriteDocumentData(URL::KNormal, UNI_L("</servercertchain>\r\n<validatedcertchain>\r\n"));
	if(m_session->Validated_Site_Certificate.Count())
		RETURN_IF_ERROR(PrintCertToURL(m_url, m_session->Validated_Site_Certificate,Str::S_MSG_SSL_TEXT_VALIDATEDCERT));

	m_url.WriteDocumentData(URL::KNormal, UNI_L("</validatedcertchain>\r\n<clientcertchain>\r\n"));

	if(m_session->Client_Certificate.Count())
		RETURN_IF_ERROR(PrintCertToURL(m_url,m_session->Client_Certificate,Str::S_MSG_SSL_TEXT_CLIENTCERT));

	m_url.WriteDocumentData(URL::KNormal, UNI_L("</clientcertchain>\n</securityinformation>\n")); // My version
	RETURN_IF_ERROR(CloseDocument());
	m_url.SetAttribute(URL::KLoadStatus, URL_LOADED);

	m_session->session_information = OP_NEW(URL, (m_url));
	if(m_session->session_information == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	m_session->session_information_lock.SetURL(*m_session->session_information);

	return OpStatus::OK;
}

OP_STATUS PrintCertToURL(URL &target, SSL_Certificate_st &certificate, Str::LocaleString description)
{
	OpAutoPtr<SSL_CertificateHandler> server_cert(g_ssl_api->CreateCertificateHandler());
	if(server_cert.get() == NULL)
		return OpStatus::ERR_NO_MEMORY;
	
	server_cert->LoadCertificate(certificate);
	if(server_cert->Error())
		return server_cert->GetOPStatus();
	
	uint24 i;
	for(i = 0; i< server_cert->CertificateCount(); i++)
	{
		RETURN_IF_ERROR(server_cert->GetCertificateInfo(i, target, description));
	}		

	return OpStatus::OK;
}
#endif


#endif
