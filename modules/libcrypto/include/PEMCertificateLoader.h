/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file PEMCertificateLoader.h
 *
 * PEM certificate loader interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef PEM_CERTIFICATE_LOADER_H
#define PEM_CERTIFICATE_LOADER_H

#if defined(CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT) && defined(DIRECTORY_SEARCH_SUPPORT)

#include "modules/libcrypto/include/CryptoCertificateStorage.h"
#include "modules/libcrypto/include/PEMCertificateAbstractLoader.h"


/**
 * @class PEMCertificateLoader
 *
 * This class loads all X.509 certificates in PEM format from a particular directory
 * into CryptoCertificateStorage. It looks for all *.pem files in a directory
 * and loads all certificates found in them.
 *
 * This class is a processor.
 * It is synchronous.
 *
 * Usage:
 *
 *		@code
 *		PEMCertificateLoader loader;
 *		loader.SetInputDirectoryL(folder_path);
 *		loader.SetCertificateStorageContainer(ca_storage);
 *		loader.ProcessL();
 *		@endcode
 *
 */
class PEMCertificateLoader : public PEMCertificateAbstractLoader
{
public:
	PEMCertificateLoader();

public:
	/** Set output container.
	 *  The class doesn't take over ownership of cert_storage.
	 */
	void SetCertificateStorageContainer(CryptoCertificateStorage* cert_storage);

	/** Process certificate loading.
	 *
	 *  The function can leave with the following codes:
	 *  - OpStatus::ERR_OUT_OF_RANGE  on bad input parameters,
	 *                                i.e. empty input directory
	 *                                set via SetInputDirectoryL()
	 *                                or NULL CryptoCertificateStorage pointer
	 *                                set via SetCertificateStorageContainer().
	 *  - OpStatus::ERR_NO_MEMORY     on OOM.
	 *  - OpStatus::ERR               on other conditions.
	 */
	void ProcessL();

protected:
	/** @name PEMLoader methods. */
	/** @{ */
	/** Decode a certificate and add it into CryptoCertificateStorage. */
	virtual void ProcessPEMChunk(const char* pem_chunk, size_t pem_len);
	/** @} */

private:
	// These objects are set from outside. Pointed objects are not owned.
	CryptoCertificateStorage* m_cert_storage;
};


// Inlines implementation.

inline PEMCertificateLoader::PEMCertificateLoader() : m_cert_storage(NULL) {}

inline void PEMCertificateLoader::SetCertificateStorageContainer(CryptoCertificateStorage* cert_storage)
{ m_cert_storage = cert_storage; }

inline void PEMCertificateLoader::ProcessL()
{
	if (!m_cert_storage)
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	PEMLoader::ProcessL();
}

inline void PEMCertificateLoader::ProcessPEMChunk(const char* pem_chunk, size_t pem_len)
{
	// Errors are ignored in this function, because it's mass-loading of certificates.
	// Some certificates may contain errors, still we shouldn't abandon the whole process.

	OP_ASSERT(pem_chunk);
	OP_ASSERT(pem_len > 0);
	OP_ASSERT(m_cert_storage);

	OpStatus::Ignore(m_cert_storage->AddPEM(pem_chunk, pem_len));
}

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT && DIRECTORY_SEARCH_SUPPORT
#endif // PEM_CERTIFICATE_LOADER_H
