/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file PEMX509Loader.h
 *
 * PEM X509 loader interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef PEM_X509_LOADER_H
#define PEM_X509_LOADER_H

#if defined(EXTERNAL_SSL_OPENSSL_IMPLEMENTATION) && defined(DIRECTORY_SEARCH_SUPPORT)

#include "modules/libcrypto/include/PEMCertificateAbstractLoader.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/ossl_typ.h"


/**
 * @class PEMX509Loader
 *
 * This class loads all X.509 certificates in PEM format from a particular directory
 * into X509_STORE. It looks for all *.pem files in a directory
 * and loads all certificates found in them.
 *
 * This class is a processor.
 * It is synchronous.
 *
 * Usage:
 *
 *		@code
 *		PEMX509Loader loader;
 *		loader.SetInputDirectoryL(folder_path);
 *		loader.Set_X509_STORE_Container(store);
 *		loader.ProcessL();
 *		@endcode
 */
class PEMX509Loader : public PEMCertificateAbstractLoader
{
public:
	PEMX509Loader();

public:
	/** Set output container.
	 *  The class doesn't take over ownership of store.
	 */
	void Set_X509_STORE_Container(X509_STORE* store);

	/** Process certificate loading.
	 *
	 *  The function can leave with the following codes:
	 *  - OpStatus::ERR_OUT_OF_RANGE  on bad input parameters,
	 *                                i.e. empty input directory
	 *                                set via SetInputDirectoryL()
	 *                                or NULL X509_STORE pointer
	 *                                set via Set_X509_STORE_Container().
	 *  - OpStatus::ERR_NO_MEMORY     on OOM.
	 *  - OpStatus::ERR               on other conditions.
	 */
	void ProcessL();

protected:
	/** @name PEMLoader methods. */
	/** @{ */
	/** Decode a certificate and add it into X509_STORE. */
	virtual void ProcessPEMChunk(const char* pem_chunk, size_t pem_len);
	/** @} */

private:
	// These objects are set from outside. Pointed objects are not owned.
	X509_STORE* m_x509_store;
};


// Inlines implementation.

inline PEMX509Loader::PEMX509Loader() : m_x509_store(NULL) {}

inline void PEMX509Loader::Set_X509_STORE_Container(X509_STORE* store)
{ m_x509_store = store; }

inline void PEMX509Loader::ProcessL()
{
	if (!m_x509_store)
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	PEMLoader::ProcessL();
}

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION && DIRECTORY_SEARCH_SUPPORT
#endif // PEM_X509_LOADER_H
