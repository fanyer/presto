/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file PEMX509Loader.cpp
 *
 * PEM X509 loader implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#if defined(EXTERNAL_SSL_OPENSSL_IMPLEMENTATION) && defined(DIRECTORY_SEARCH_SUPPORT)

#include "modules/libcrypto/src/openssl_impl/PEMX509Loader.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/pem.h"


inline void PEMX509Loader::ProcessPEMChunk(const char* pem_chunk, size_t pem_len)
{
	// Errors are ignored in this function, because it's mass-loading of certificates.
	// Some certificates may contain errors, still we shouldn't abandon the whole process.

	OP_ASSERT(pem_chunk);
	OP_ASSERT(pem_len > 0);
	OP_ASSERT(m_x509_store);

	BIO* bio = BIO_new_mem_buf((void*) pem_chunk, pem_len);
	if (!bio)
	{
		ERR_clear_error();
		return;
	}

	// Load certificate into X509 structure.
	X509* x509 = PEM_read_bio_X509(bio, 0, 0, 0);
	BIO_free(bio);
	if (!x509)
	{
		ERR_clear_error();
		return;
	}
	// We own x509.

	// Add cert to the store.
	int res = X509_STORE_add_cert(m_x509_store, x509);
	X509_free(x509);
	if (res != 1)
		ERR_clear_error();
}

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION && DIRECTORY_SEARCH_SUPPORT
