/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OCSPRequest_impl.h
 *
 * OpenSSL-based OCSP request container interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OCSP_REQUEST_IMPL_H
#define OCSP_REQUEST_IMPL_H

#if defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)

#include "modules/libcrypto/include/OpOCSPRequest.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/ocsp.h"


class OCSPRequest_impl : public OpOCSPRequest
{
public:
	OCSPRequest_impl();
	virtual ~OCSPRequest_impl();

public:
	// OpOCSPRequest methods.
	virtual unsigned int CalculateDERLengthL() const;
	virtual void ExportAsDERL(unsigned char* der) const;

public:
	// Implementation-specific methods.
	void TakeOverOCSP_REQUEST(OCSP_REQUEST* ocsp_request);
	OCSP_REQUEST* GetOCSP_REQUEST() const;
	void Clear();

private:
	// These objects are created and owned by this object.
	OCSP_REQUEST* m_ocsp_request;
};

#endif // defined(CRYPTO_OCSP_SUPPORT) && defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION)
#endif // OCSP_REQUEST_IMPL_H
