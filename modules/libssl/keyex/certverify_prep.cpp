/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/keyex/certverify.h"
#include "modules/libssl/certs/verifyinfo.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/certs/certinstaller.h"
#include "modules/libssl/certs/certhandler.h"
#include "modules/libssl/protocol/sslstat.h"
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
#include "modules/libssl/data/root_auto_retrieve.h"
#endif

SSL_CertificateVerifier::SSL_CertificateVerifier(int dialog):
	warncount(0), warnstatus(NULL), warn_mask(0),
#ifdef LIBSSL_ENABLE_CRL_SUPPORT
	loaded_crls(FALSE),
#endif
	url_loading_mode(Loading_None),
	pending_url(NULL),
	block_ica_requests(FALSE),
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	block_retrieve_requests(FALSE),
	auto_fetcher(MSG_SSL_AUTOFETCH_FINISHED, (UINTPTR) this),
	ignore_repository(FALSE),
#endif
	accept_unknown_ca(FALSE),
	verify_state(Verify_NotStarted),
	doing_iteration(FALSE),
	port(0),
	certificate_purpose(SSL_Purpose_Server_Certificate),
	accept_search_mode(SSL_CONFIRMED),
	dialog_type(dialog),
	cert_handler(NULL),
	batch_request(NULL),
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	ocsp_extensions_sent(FALSE),
	certificate_checked_for_ocsp(FALSE),
	certificate_loading_ocsp(FALSE),
#endif
	check_name(FALSE),
	certificate_status(SSL_CERT_NO_WARN),
	UserConfirmed(NO_INTERACTION),
	security_rating(SECURITY_STATE_UNKNOWN),
	low_security_reason(SECURITY_REASON_NOT_NEEDED),
	val_certificate_count(0),
#ifdef SSL_CHECK_EXT_VALIDATION_POLICY
	check_extended_validation(TRUE),
	extended_validation(FALSE),
	disable_EV(FALSE),
#endif
	show_dialog(TRUE),
	window(NULL)
{
	Certificate.ForwardTo(this);
#ifndef TLS_NO_CERTSTATUS_EXTENSION
	Validated_Certificate.ForwardTo(this);
	sent_ocsp_extensions.ForwardTo(this);
	received_ocsp_response.ForwardTo(this);
#endif

#ifdef LIBSSL_AUTO_UPDATE_ROOTS
	static const OpMessage messages[] =
	{
		MSG_DO_AUTO_ROOT_REQUEST,
		MSG_DO_AUTO_UNTRUSTED_REQUEST
	};
	OP_STATUS op_err = g_main_message_handler->SetCallBackList(&auto_fetcher, 0, messages, ARRAY_SIZE(messages));
	if(OpStatus::IsError(op_err))
		RaiseAlert(op_err);
    if(!g_ssl_api->CheckSecurityManager())
        RaiseAlert(OpStatus::ERR_NO_MEMORY);
#endif

	if(!g_ssl_api->CheckSecurityManager())
		RaiseAlert(OpStatus::ERR_NO_MEMORY);
}

SSL_CertificateVerifier::~SSL_CertificateVerifier()
{
	InternalDestruct();
}

void SSL_CertificateVerifier::InternalDestruct()
{
	if(verify_state != SSL_CertificateVerifier::Verify_NotStarted)
		verify_state = SSL_CertificateVerifier::Verify_Aborted;

	CancelPendingLoad();
	OP_DELETE(batch_request);
	batch_request = NULL;

	// Tell message pump not to call our soon-to-be-freed .HandleCallback() (see bug CORE-23189).
	g_main_message_handler->UnsetCallBacks(this);

	OP_DELETEA(warnstatus);
	warnstatus = NULL;

	OP_DELETE(pending_url);
	pending_url = NULL;

	OP_DELETE(cert_handler);
	cert_handler = NULL;
}

OP_STATUS SSL_CertificateVerifier::SetHostName(ServerName *_servername, uint32 _port)
{
	servername = _servername;
	port = _port;
	if(servername == (ServerName *) NULL)
	{
		server_info = NULL;
		return OpStatus::OK;
	}

	server_info = servername->GetSSLSessionHandler(port);
	if(server_info == (SSL_Port_Sessions *) NULL)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

void SSL_CertificateVerifier::SetHostName(SSL_Port_Sessions *ptr)
{
	server_info = ptr;
	if(server_info != (SSL_Port_Sessions *) NULL)
	{
		servername = server_info->GetServerName();
		port = server_info->Port();
	}
	else
	{
		servername = NULL;
		port = 0;
	}
}

OP_STATUS SSL_CertificateVerifier::GetCertificateNames(OpString_list &target)
{
	unsigned long i, count = CertificateNames.Count();
	RETURN_IF_ERROR(target.Resize(count));

	for(i = 0; i < count; i++)
		RETURN_IF_ERROR(target[i].Set(CertificateNames[i]));

	return OpStatus::OK;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_Init(SSL_Alert *msg)
{
	if (!batch_request)
	{
		if (OpStatus::IsError(OpBatchRequest::Make(batch_request, this)))
			return SSL_CertificateVerifier::Verify_Failed;
	}

	OP_DELETE(cert_handler);

	cert_handler = g_ssl_api->CreateCertificateHandler();
	if(cert_handler == NULL)
	{
		if(msg)
			msg->RaiseAlert(SSL_Internal, SSL_Allocation_Failure);
		return SSL_CertificateVerifier::Verify_Failed;
	}	

	cert_handler->LoadCertificate(Certificate);
	if(cert_handler->Error(msg))
	{
		OP_DELETE(cert_handler);
		cert_handler = NULL;
		return SSL_CertificateVerifier::Verify_Failed;
	}

	warncount = Certificate.Count()+1;
	OP_DELETEA(warnstatus);
	warnstatus = NULL;
	Validated_Certificate.Resize(0);
	Matched_name.Empty();

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

SSL_CertificateVerifier::VerifyStatus SSL_CertificateVerifier::VerifyCertificate_AddIntermediates(SSL_Alert *msg)
{
	SSL_DownloadedIntermediateCert *item = (SSL_DownloadedIntermediateCert *) intermediate_list.First();
	while(item)
	{
		cert_handler->LoadExtraCertificate(item->certificate);
		item = (SSL_DownloadedIntermediateCert *) item->Suc();
	}

	return SSL_CertificateVerifier::Verify_TaskCompleted;
}

#endif
