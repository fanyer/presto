/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file PEMCertificateAbstractLoader.h
 *
 * PEM certificate abstract loader interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef PEM_CERTIFICATE_ABSTRACT_LOADER_H
#define PEM_CERTIFICATE_ABSTRACT_LOADER_H

#if defined(CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT) && defined(DIRECTORY_SEARCH_SUPPORT)

#include "modules/libcrypto/include/PEMLoader.h"


/**
 * @class PEMCertificateAbstractLoader
 *
 * This is intermediate abstract class for deriving actual PEM certificate
 * loaders from it. It provides PEM begin and end markers for a certificate block.
 *
 * @see PEMLoader documentation for more info.
 *
 */
class PEMCertificateAbstractLoader : public PEMLoader
{
protected:
	/** @name PEMLoader methods. */
	/** @{ */
	/** Provide begin marker for X.509 certificate. */
	virtual const char* GetBeginMarker() const;
	/** Provide end   marker for X.509 certificate. */
	virtual const char* GetEndMarker() const;
	/** @} */
};


// Inlines implementation.

inline const char* PEMCertificateAbstractLoader::GetBeginMarker() const
{ return "-----BEGIN CERTIFICATE-----"; }

inline const char* PEMCertificateAbstractLoader::GetEndMarker() const
{ return "-----END CERTIFICATE-----"; }

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT && DIRECTORY_SEARCH_SUPPORT
#endif // PEM_CERTIFICATE_ABSTRACT_LOADER_H
