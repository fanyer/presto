/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OCSPRequest_impl.cpp
 *
 * OpenSSL-based OCSP request container implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#if defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)

#include "modules/libcrypto/src/openssl_impl/OCSPRequest_impl.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"


OpOCSPRequest* OpOCSPRequest::Create()
{
	return OP_NEW(OCSPRequest_impl, ());
}


OCSPRequest_impl::OCSPRequest_impl()
	: m_ocsp_request(0)
{}


OCSPRequest_impl::~OCSPRequest_impl()
{
	Clear();
}


unsigned int OCSPRequest_impl::CalculateDERLengthL() const
{
	if (!m_ocsp_request)
		LEAVE(OpStatus::ERR);

	int len = i2d_OCSP_REQUEST(m_ocsp_request, 0);
	OPENSSL_LEAVE_IF(len <= 0);

	return len;
}


void OCSPRequest_impl::ExportAsDERL(unsigned char* der) const
{
	if (!m_ocsp_request)
		LEAVE(OpStatus::ERR);

	// Temporary variable as recommended by "man i2d_X509".
	unsigned char* der_tmp = der;

	int len = i2d_OCSP_REQUEST(m_ocsp_request, &der_tmp);
	OPENSSL_LEAVE_IF(len <= 0);
}


void OCSPRequest_impl::TakeOverOCSP_REQUEST(OCSP_REQUEST* ocsp_request)
{
	Clear();
	m_ocsp_request = ocsp_request;
}


OCSP_REQUEST* OCSPRequest_impl::GetOCSP_REQUEST() const
{
	return m_ocsp_request;
}


void OCSPRequest_impl::Clear()
{
	if (m_ocsp_request)
	{
		OCSP_REQUEST_free(m_ocsp_request);
		m_ocsp_request = 0;
	}
}

#endif // defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)
