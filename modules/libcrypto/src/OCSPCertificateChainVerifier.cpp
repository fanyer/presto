/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OCSPCertificateChainVerifier.cpp
 *
 * OCSP certificate chain verifier implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#ifdef CRYPTO_OCSP_SUPPORT

#include "modules/libcrypto/include/OCSPCertificateChainVerifier.h"

#include "modules/cache/url_dd.h"
#include "modules/libcrypto/include/CryptoCertificate.h"
#include "modules/libcrypto/include/OpOCSPRequest.h"
#include "modules/libcrypto/include/OpOCSPRequestProducer.h"
#include "modules/libcrypto/include/OpOCSPResponseVerifier.h"
#include "modules/upload/upload.h"
#include "modules/url/url_sn.h"


OCSPCertificateChainVerifier::OCSPCertificateChainVerifier()
	: m_certificate_chain(0)
	, m_ca_storage(0)
	, m_verify_status(CryptoCertificateChain::VERIFY_STATUS_UNKNOWN)
	, m_retry_count(0)
	// Default timeout is one minute.
	, m_timeout(60)
	, m_explicit_timeout_set(FALSE)
{}


OCSPCertificateChainVerifier::~OCSPCertificateChainVerifier()
{
	StopLoading();
	// m_verify_status doesn't need deallocation.
	// m_ca_storage         is not owned by the object.
	// m_certificate_chain  is not owned by the object.
}


void OCSPCertificateChainVerifier::SetCertificateChain(const CryptoCertificateChain* certificate_chain)
{
	m_certificate_chain = certificate_chain;
}


void OCSPCertificateChainVerifier::SetCAStorage(const CryptoCertificateStorage* ca_storage)
{
	m_ca_storage = ca_storage;
}


void OCSPCertificateChainVerifier::ProcessL()
{
	if (!m_certificate_chain || !m_ca_storage)
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	unsigned int cert_count = m_certificate_chain->GetNumberOfCertificates();
	if (cert_count == 0)
		// Empty chain. Nothing to check.
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	else if (cert_count == 1)
	{
		// The chain consists of only 1 certificate. It is either self signed,
		// or the chain is incomplete. In both cases it doesn't make sense
		// to proceed with OCSP check. If a self signed cert is compromised -
		// the OCSP response can be faked anyway. Also, a scenario when
		// a widget is signed with a self signed (root CA) cert, this cert
		// contains OCSP URL and OCSP responder actually works, doesn't
		// practically happen IRL. Thus replying "status unknown" right away.

		m_verify_status = CryptoCertificateChain::VERIFY_STATUS_UNKNOWN;
		NotifyAboutFinishedVerification();
		return;
	}

	// Default value. Will be checked in the end of the function.
	OP_STATUS status = OpStatus::OK;

	// Main part.
	TRAP(status,
		// Delayed initialization: creating OCSP request producer and response verifier.
		InitL();

		// URL.
		OpString responder_url_name;
		FetchOCSPResponderURLL(responder_url_name);

		// Request.
		ProduceOCSPRequestL();

		// Prepare URL for the request.
		PrepareURLL(responder_url_name);

		// Launch.
		SubmitOCSPRequest();
	);

	if (OpStatus::IsError(status))
	{
		m_verify_status = CryptoCertificateChain::VERIFY_STATUS_UNKNOWN;
		NotifyAboutFinishedVerification();
	}
}


MH_PARAM_1 OCSPCertificateChainVerifier::Id() const
{
	return reinterpret_cast <MH_PARAM_1> (this);
}


CryptoCertificateChain::VerifyStatus
OCSPCertificateChainVerifier::GetVerifyStatus() const
{
	return m_verify_status;
}


void OCSPCertificateChainVerifier::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_HEADER_LOADED:
			ProcessURLHeader();
			break;

		case MSG_URL_DATA_LOADED:
			ProcessURLDataLoaded();
			break;

		case MSG_URL_LOADING_FAILED:
			ProcessURLLoadingFailed();
			break;

		case MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_RETRY:
			OP_ASSERT(par1 == Id());
			SubmitOCSPRequest();
			break;
	}
}


void OCSPCertificateChainVerifier::InitL()
{
	if (!m_request.get())
		m_request = OpOCSPRequest::Create();
	LEAVE_IF_NULL(m_request.get());

	if (!m_request_producer.get())
		m_request_producer = OpOCSPRequestProducer::Create();
	LEAVE_IF_NULL(m_request_producer.get());

	if (!m_response_verifier.get())
		m_response_verifier = OpOCSPResponseVerifier::Create();
	LEAVE_IF_NULL(m_response_verifier.get());
}


void OCSPCertificateChainVerifier::FetchOCSPResponderURLL(OpString& responder_url_name)
{
	const CryptoCertificate* cert = m_certificate_chain->GetCertificate(0);
	OP_ASSERT(cert);

	OP_STATUS status = cert->GetOCSPResponderURL(responder_url_name);
	LEAVE_IF_ERROR(status);
}


void OCSPCertificateChainVerifier::ProduceOCSPRequestL()
{
	OP_ASSERT(m_request.get());
	OP_ASSERT(m_request_producer.get());

	const CryptoCertificate* cert = m_certificate_chain->GetCertificate(0);
	OP_ASSERT(cert);

	const CryptoCertificate* issuer_cert = m_certificate_chain->GetCertificate(1);
	OP_ASSERT(issuer_cert);

	m_request_producer->SetCertificate(cert);
	m_request_producer->SetIssuerCertificate(issuer_cert);
	m_request_producer->SetOutput(m_request.get());
	m_request_producer->ProcessL();
}


void OCSPCertificateChainVerifier::PrepareURLL(const OpStringC& responder_url_name)
{
	// The first implementation only supports POST.
	// Support for GET can be implemented in future.
	// The obstacle for implementing GET so far: not all OCSP servers support GET.
	// It can be solved by trying first GET, then POST, and/or by looking up
	// this support in a predefined table.

	OP_ASSERT(m_request.get());

	unsigned int   request_length = m_request->CalculateDERLengthL();
	unsigned char* request_der    = OP_NEWA_L(unsigned char, request_length);
	OP_ASSERT(request_der);
	ANCHOR_ARRAY(unsigned char, request_der);
	m_request->ExportAsDERL(request_der);

	// Build POST version.

	// Set up buffer.
	Upload_BinaryBuffer* buffer = OP_NEW_L(Upload_BinaryBuffer, ());
	OP_ASSERT(buffer);
	ANCHOR_PTR(Upload_BinaryBuffer, buffer);

	buffer->InitL(request_der, request_length, UPLOAD_TAKE_OVER_BUFFER,	"application/ocsp-request");
	// Now buffer owns request_der.
	ANCHOR_ARRAY_RELEASE(request_der);

	buffer->PrepareUploadL(UPLOAD_BINARY_NO_CONVERSION);

	// Set up URL.
	m_url = g_url_api->GetURL(responder_url_name
#ifdef _NATIVE_SSL_SUPPORT_
		, g_revocation_context
#endif
	);

	URLType url_type = (URLType) m_url.GetAttribute(URL::KType);
	if (url_type != URL_HTTP && url_type != URL_HTTPS)
	{
		// Clear URL.
		m_url = URL();
		LEAVE(OpStatus::ERR);
	}

	m_url_inuse.SetURL(m_url);
	g_url_api->MakeUnique(m_url);
	m_url.SetAttributeL(URL::KHTTP_Managed_Connection, TRUE);

	m_url.SetAttributeL(URL::KHTTP_Method, HTTP_METHOD_POST);
	m_url.SetHTTP_Data(buffer, TRUE);
	// Now m_url owns buffer.
	ANCHOR_PTR_RELEASE(buffer);

	m_url.SetAttributeL(URL::KSpecialRedirectRestriction, TRUE);
	m_url.SetAttributeL(URL::KDisableProcessCookies, TRUE);
	m_url.SetAttributeL(URL::KBlockUserInteraction, TRUE);
	m_url.SetAttributeL(URL::KTimeoutMaximum, m_timeout);
	if (!m_explicit_timeout_set)
	{
		m_url.SetAttributeL(URL::KTimeoutPollIdle, 5);
		m_url.SetAttributeL(URL::KTimeoutMinimumBeforeIdleCheck, 15);
	}
	ServerName* server_name = m_url.GetServerName();
	if (server_name)
		server_name->SetConnectionNumRestricted(TRUE);

	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_HEADER_LOADED, m_url.Id()));
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_DATA_LOADED, m_url.Id()));
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_URL_LOADING_FAILED, m_url.Id()));
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_RETRY, Id()));
}


void OCSPCertificateChainVerifier::SubmitOCSPRequest()
{
	URL ref;
	URL_LoadPolicy load_policy(FALSE, URL_Load_Normal);

	// Launch OCSP request.
	CommState comm_state = m_url.LoadDocument(g_main_message_handler, ref, load_policy);
	switch (comm_state)
	{
		case COMM_LOADING:
			break;

		case COMM_REQUEST_FINISHED:
			ProcessURLLoadingFinished();
			break;

		default:
			OP_ASSERT(!"Unexpected CommState.");
			// fall-through

		case COMM_REQUEST_FAILED:
			ProcessURLLoadingFailed();
	}
}


void OCSPCertificateChainVerifier::ProcessURLHeader()
{
	UINT32 response_code =
		m_url.GetAttribute(URL::KHTTP_Response_Code, URL::KFollowRedirect);

	OpStringC8 mime_type =
		m_url.GetAttribute(URL::KMIME_Type, URL::KFollowRedirect);

	if (response_code != HTTP_OK || mime_type != "application/ocsp-response")
	{
		m_verify_status = CryptoCertificateChain::VERIFY_STATUS_UNKNOWN;
		NotifyAboutFinishedVerification();
	}
}


void OCSPCertificateChainVerifier::ProcessURLDataLoaded()
{
	OpFileLength content_loaded = 0;
	m_url.GetAttribute(
		URL::KContentLoaded,
		reinterpret_cast <const void*> (&content_loaded),
		URL::KFollowRedirect);

	// Check that the response hasn't grown too big.
	if (content_loaded > OCSP_RESPONSE_SIZE_LIMIT)
	{
		m_verify_status = CryptoCertificateChain::VERIFY_STATUS_UNKNOWN;
		NotifyAboutFinishedVerification();
		return;
	}

	URLStatus url_status = static_cast <URLStatus> (
		m_url.GetAttribute(URL::KLoadStatus, URL::KFollowRedirect));
	OP_ASSERT(url_status == URL_LOADING || url_status == URL_LOADED);

	if (url_status == URL_LOADED)
		ProcessURLLoadingFinished();
}


void OCSPCertificateChainVerifier::ProcessURLLoadingFailed()
{
	if (m_retry_count > 0)
	{
		m_retry_count--;
		g_main_message_handler->PostMessage(
			MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_RETRY, Id(), 0);
		return;
	}
	m_verify_status = CryptoCertificateChain::VERIFY_STATUS_UNKNOWN;
	NotifyAboutFinishedVerification();
}


void OCSPCertificateChainVerifier::ProcessURLLoadingFinished()
{
	// Default value. Will be checked in the end of the function.
	OP_STATUS status = OpStatus::OK;
	
	TRAP(status, ProcessURLLoadingFinishedL());

	if (OpStatus::IsError(status))
		m_verify_status = CryptoCertificateChain::VERIFY_STATUS_UNKNOWN;

	NotifyAboutFinishedVerification();
}


void OCSPCertificateChainVerifier::ProcessURLLoadingFinishedL()
{
	// Update visited status and LRU status of the content.
	m_url.Access(FALSE);

	// Get data descriptor.
	URL_DataDescriptor* data_descriptor = m_url.GetDescriptor(
		g_main_message_handler,
		URL::KFollowRedirect,
		TRUE, // don't use character set conversion of the data (if relevant)
		TRUE  // decode the data using the content-encoding specification (usually decompression)
	);
	if (!data_descriptor)
		LEAVE(OpStatus::ERR);
	ANCHOR_PTR(URL_DataDescriptor, data_descriptor);

	// Instead of reading the whole content into the own buffer,
	// let's grow data descriptor's buffer enough for the whole content.
	// In most real cases it will be a no-op, because usually OCSP response
	// is small enough to be fully in the buffer.

	unsigned int data_length   = 0;
	unsigned int buffer_length = 0;

	while (TRUE)
	{
		// Retrieve data.
		BOOL more;
		data_length = data_descriptor->RetrieveDataL(more);
		if (more == FALSE)
			break;

		// Special case: the response is unusually big and doesn't fit into
		// the default buffer. It may be compressed, so in general it is
		// impossible to know the response size in advance. Let's grow
		// the data descriptor buffer until the response fits
		// or size limit is hit. As this case should be very rare,
		// its handling is simple and is optimized for footprint.

		if (buffer_length >= OCSP_RESPONSE_SIZE_LIMIT)
			// Too big response.
			LEAVE(OpStatus::ERR);

		// Grow buffer.
		buffer_length = data_descriptor->Grow(0);
		if (buffer_length == 0)
			// Couldn't enlarge buffer more. Probably OOM.
			LEAVE(OpStatus::ERR_NO_MEMORY);
	}

	// Now we have the whole OCSP response in the data descriptor buffer.
	const char* buffer = data_descriptor->GetBuffer();
	OP_ASSERT(buffer);
	OP_ASSERT(data_descriptor->GetBufSize() == data_length);

	// Verify response.
	const unsigned char* response =
		reinterpret_cast <const unsigned char*> (buffer);
	m_response_verifier->SetOCSPResponseDER(response, data_length);
	m_response_verifier->SetOCSPRequest(m_request.get());
	m_response_verifier->SetCertificateChain(m_certificate_chain);
	m_response_verifier->SetCAStorage(m_ca_storage);
	m_response_verifier->ProcessL();
	m_verify_status = m_response_verifier->GetVerifyStatus();
}


void OCSPCertificateChainVerifier::StopLoading()
{
	OP_ASSERT(g_main_message_handler);
	m_url.StopLoading(g_main_message_handler);
	g_main_message_handler->UnsetCallBacks(this);
	m_url_inuse.UnsetURL();
}


void OCSPCertificateChainVerifier::NotifyAboutFinishedVerification()
{
	OP_ASSERT(g_main_message_handler);

	StopLoading();

	g_main_message_handler->PostMessage(
		MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_FINISHED, Id(), 0);
}

#endif // CRYPTO_OCSP_SUPPORT
