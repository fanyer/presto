/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"
#include "modules/libssl/keyex/certverify.h"
#include "modules/libssl/protocol/sslstat.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/cache/url_stream.h"
#include "modules/upload/upload.h"
#include "modules/formats/uri_escape.h"
#include "modules/formats/encoder.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/pi/OpSystemInfo.h"

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::CheckOCSP()
{
	OpString ocsp_url_name;
	SSL_varvector32 ocsp_challenge;
	//SSL_ConnectionState *commstate = AccessConnectionState();

	if(
		!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseOCSPValidation) ||
		!cert_handler
		)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	cert_handler->ResetError();

	OpString_list ocsp_urls;
	SSL_varvector32 ocsp_request;

	if(OpStatus::IsError(cert_handler->GetOCSP_Request(ocsp_urls, ocsp_request
#ifndef TLS_NO_CERTSTATUS_EXTENSION
				, sent_ocsp_extensions
#endif
				)))
		return SSL_CertificateVerifier::Verify_TaskCompleted;

#ifndef TLS_NO_CERTSTATUS_EXTENSION
	if(sent_ocsp_extensions.GetLength() || ocsp_extensions_sent)
	{
		if(OpStatus::IsError(cert_handler->VerifyOCSP_Response(received_ocsp_response)) || Error())
		{
			RaiseAlert(cert_handler);
			CancelPendingLoad();
			return SSL_CertificateVerifier::Verify_Failed;
		}
		return SSL_CertificateVerifier::Verify_TaskCompleted;
	}
#endif

	URL ocsp_url_candidate;
	unsigned long i,n;

	n = ocsp_urls.Count();
	if(n == 0 || ocsp_request.GetLength() == 0)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	// Build POST version
	OpAutoPtr<Upload_BinaryBuffer> buffer(OP_NEW(Upload_BinaryBuffer, ()));

	if(buffer.get() == NULL)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	OP_STATUS op_err = OpStatus::OK;
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, buffer->InitL(ocsp_request.GetDirect(), ocsp_request.GetLength(),
					UPLOAD_COPY_BUFFER, "application/ocsp-request"), SSL_CertificateVerifier::Verify_TaskCompleted);

	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, buffer->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION), SSL_CertificateVerifier::Verify_TaskCompleted);

	// Build GET version
	OpString escaped_query;
	OpString encoded_query;
	char *b64_ocsp = NULL;
	int b64_ocsp_len = 0;

	MIME_Encode_SetStr(b64_ocsp, b64_ocsp_len, (const char *) ocsp_request.GetDirect(), ocsp_request.GetLength(), NULL, GEN_BASE64_ONELINE);

	if(b64_ocsp == NULL || b64_ocsp_len == 0)
	{
		OP_DELETEA(b64_ocsp);
		return SSL_CertificateVerifier::Verify_TaskCompleted;
	}

	encoded_query.Set(b64_ocsp, b64_ocsp_len);

	uni_char *target = escaped_query.Reserve(b64_ocsp_len*3+10);
	if(!target)
	{
		OP_DELETEA(b64_ocsp);
		return SSL_CertificateVerifier::Verify_TaskCompleted;
	}

	target += UriEscape::Escape(target, b64_ocsp, b64_ocsp_len, UriEscape::AllUnsafe);
	*(target++) = '\0';
	OP_DELETEA(b64_ocsp);

	// Find URL
	for(i=0;i<n;i++)
	{
		OpString temp_url;
		int len = ocsp_urls[i].Length();

		BOOL use_post = FALSE;
		{
			OpStringC temp_ocsp(ocsp_urls[i]);

			for(unsigned long j = 0; j< g_securityManager->ocsp_override_list.Count(); j++)
			{
				if(temp_ocsp.Compare(g_securityManager->ocsp_override_list[j]) == 0)
				{
					use_post = TRUE;
					break;
				}
			}
		}

		if(OpStatus::IsError(temp_url.SetConcat(ocsp_urls[i], (use_post || ocsp_urls[i][len-1] == '/' ? (const uni_char *) NULL : UNI_L("/")), (use_post ? (const uni_char *) NULL : escaped_query.CStr()))))
			continue;

		ocsp_url_candidate = g_url_api->GetURL(temp_url, g_revocation_context);

		URLType ocsp_type = (URLType) ocsp_url_candidate.GetAttribute(URL::KType);

		if(ocsp_url_candidate.IsEmpty() || (ocsp_type != URL_HTTP && ocsp_type != URL_HTTPS) ||
			ocsp_url_candidate.GetAttribute(URL::KHostName).FindFirstOf('.') == KNotFound)
		{
			ocsp_url_candidate = URL();
			continue;
		}

		unsigned short port_num = ocsp_url_candidate.GetAttribute(URL::KResolvedPort);
		ServerName *ocsp_sn = (ServerName *) ocsp_url_candidate.GetAttribute(URL::KServerName, (const void*) 0);

		if(server_info != NULL && ocsp_sn == server_info->GetServerName() &&
			port_num == server_info->Port())
		{
			// same host, try another one
			ocsp_url_candidate = URL();
			continue;
		}

		if(use_post)
		{
			g_url_api->MakeUnique(ocsp_url_candidate);
			ocsp_url_candidate.SetAttribute(URL::KHTTP_Method, HTTP_METHOD_POST);
			ocsp_url_candidate.SetHTTP_Data(buffer.release(), TRUE);
		}

		ocsp_url_candidate.SetAttribute(URL::KHTTP_Managed_Connection, TRUE);

		break;
	}

	if(ocsp_url_candidate.IsEmpty())
	{

		low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE;

		if(security_rating >SECURITY_STATE_HALF)
		{
			security_rating=SECURITY_STATE_HALF;
		}
		return SSL_CertificateVerifier::Verify_TaskCompleted;
	}

	pending_url = OP_NEW(URL, (ocsp_url_candidate));
	if(pending_url == NULL)
		return SSL_CertificateVerifier::Verify_TaskCompleted;

	url_loading_mode = Loading_CRL_Or_OCSP;
	pending_url_ref.SetURL(*pending_url);

#ifndef TLS_ADD_OCSP_NONCE
	if(ocsp_url_candidate.Status(FALSE) == URL_LOADED)
	{
		time_t expires = 0;
			
		ocsp_url_candidate.GetAttribute(URL::KVHTTP_ExpirationDate, &expires);
		if(expires && expires > (time_t) (g_op_time_info->GetTimeUTC()/1000.0))
			return ValidateOCSP(TRUE);
	}
#endif

	return SSL_CertificateVerifier::Verify_Started;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::ValidateOCSP(BOOL full_check_if_fail)
{
	if(!cert_handler || url_loading_mode != Loading_CRL_Or_OCSP || pending_url == NULL || pending_url->IsEmpty())
	{
		CancelPendingLoad();
		return SSL_CertificateVerifier::Verify_TaskCompleted;
	}

	URL_InUse ocsp_url(pending_url_ref);
	pending_url_ref.UnsetURL();
	OP_DELETE(pending_url);
	pending_url= NULL;

	OpFileLength len = 0;
	ocsp_url->GetAttribute(URL::KContentLoaded, &len,URL::KFollowRedirect);

	if((URLStatus) ocsp_url->GetAttribute(URL::KLoadStatus, URL::KFollowRedirect) != URL_LOADED ||
		ocsp_url->GetAttribute(URL::KHTTP_Response_Code,URL::KFollowRedirect) != HTTP_OK ||
		len == 0 ||
		ocsp_url->GetAttribute(URL::KMIME_Type,URL::KFollowRedirect).CompareI("application/ocsp-response") != 0
		)
	{

		low_security_reason |= SECURITY_LOW_REASON_UNABLE_TO_OCSP_VALIDATE;

		if(security_rating >SECURITY_STATE_HALF)
		{
			security_rating=SECURITY_STATE_HALF;
		}

		CancelPendingLoad();
		goto finished_ocsp;
	}

	{
		URL_DataStream ocsp_reader(*ocsp_url);
		SSL_varvector32 ocsp_response;

		TRAPD(op_err, ocsp_response.AddContentL(&ocsp_reader));

		if(OpStatus::IsError(op_err) || ocsp_response.Error() || ocsp_response.GetLength() == 0)
		{
			if(full_check_if_fail)
				return SSL_CertificateVerifier::Verify_Started; // Caller is ready to send this request
#ifdef SECURITY_LOW_REASON_OCSP_FAILED
			low_security_reason |= SECURITY_LOW_REASON_OCSP_FAILED;
#endif
			if(security_rating >SECURITY_STATE_HALF)
			{
				security_rating = SECURITY_STATE_HALF;
			}
			CancelPendingLoad();
			goto finished_ocsp;
		}

		if(OpStatus::IsError(cert_handler->VerifyOCSP_Response(ocsp_response)) || Error())
		{
			SSL_Alert msg;
			cert_handler->Error(&msg);

			if(msg.GetDescription() == SSL_Certificate_OCSP_error || msg.GetDescription() == SSL_Certificate_OCSP_Verify_failed)
			{
				if(full_check_if_fail)
					return SSL_CertificateVerifier::Verify_Started;
				ocsp_fail_reason.Set(msg.GetReason());
				cert_handler->ResetError();

				low_security_reason |= SECURITY_LOW_REASON_OCSP_FAILED;

				if(security_rating >SECURITY_STATE_HALF)
				{
					security_rating= SECURITY_STATE_HALF;
				}
				CancelPendingLoad();
				goto finished_ocsp;
			}
			else
			{
				if(msg.GetDescription() == SSL_Certificate_Revoked)
				{
					SSL_varvector16 fingerprint;
					cert_handler->GetFingerPrint(0, fingerprint);
					OpStatus::Ignore(g_revoked_certificates.AddRevoked(fingerprint, Certificate[0]));
				}
				RaiseAlert(cert_handler);
				if(!Error())
					RaiseAlert(SSL_Internal, SSL_InternalError);
				CancelPendingLoad();
				return SSL_CertificateVerifier::Verify_Failed;
			}
		}
	}

finished_ocsp:;

	if (pending_url)
	{
		OP_DELETE(pending_url);
		pending_url = NULL;
	}
	
	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

#endif
