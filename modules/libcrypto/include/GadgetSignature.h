/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file GadgetSignature.h
 *
 * Gadget signature interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef GADGET_SIGNATURE_H
#define GADGET_SIGNATURE_H

#ifdef SIGNED_GADGET_SUPPORT

#include "modules/libcrypto/include/CryptoXmlSignature.h"


/**
 * @class GadgetSignature
 *
 * This class stores information about one signature of a gadget.
 * This class is a container.
 *
 * Usage:
 *
 *		@code
 *		GadgetSignature signature;
 *		...
 *		// Before verification.
 *		signature.SetSignatureFilenameL(UNI_L("signature20.xml"));
 *		...
 *		// After local verification.
 *		signature.SetVerifyError(CryptoXmlSignature::OK_CHECKED_LOCALLY);
 *		signature.SetCertificateChain(certificate_chain);
 *		...
 *		// After OCSP verification.
 *		signature.SetVerifyError(CryptoXmlSignature::OK_CHECKED_WITH_OCSP);
 *		...
 *		// Getting information.
 *		CryptoXmlSignature::VerifyError verify_error = signature.GetVerifyError();
 *		if (verify_error == CryptoXmlSignature::OK_CHECKED_LOCALLY ||
 *		    verify_error == CryptoXmlSignature::OK_CHECKED_WITH_OCSP)
 *		{
 *			// Chain exists if verification is successful.
 *			const CryptoCertificateChain* chain = signature.GetCertificateChain();
 *			OP_ASSERT(chain);
 *			...
 *		}
 *		const OpStringC& filename = signature.GetSignatureFilename();
 *		...
 *		@endcode
 */
class GadgetSignature
{
public:
	/** Constructor. */
	GadgetSignature();

public:
	/** @name Getters.
	 *  They don't give object ownership.
	 *  Used by gadgets module. */
	/** @{ */

	/** Get the signature verification result. */
	CryptoXmlSignature::VerifyError GetVerifyError() const;

	/** Get the certificate chain, which signs the gadget.
	 *
	 * The gadget is signed by a certificate. During this certificate
	 * verification, a certificate chain is built. This function
	 * returns a constant pointer to this chain.
	 *
	 * The returned CryptoCertificateChain object is owned
	 * by the class, the caller shouldn't free it.
	 */
	const CryptoCertificateChain* GetCertificateChain() const;

	/** Get the signature filename, for example "signature1.xml". */
	const OpStringC& GetSignatureFilename() const;

	/** @} */

public:
	/** @name Setters.
	 *  They take over object ownership.
	 *  Used by GadgetSignatureVerifier class. */
	/*  @{ */
	void SetVerifyError(CryptoXmlSignature::VerifyError verify_error);
	void SetCertificateChain(CryptoCertificateChain* certificate_chain);
	void SetSignatureFilenameL(const OpStringC& filename);
	/** @} */

private:
	// Data.
	CryptoXmlSignature::VerifyError m_verify_error;
	OpAutoPtr <CryptoCertificateChain> m_certificate_chain;
	OpString m_signature_filename;
};


// Inlines implementation.


// Constructor.

inline GadgetSignature::GadgetSignature()
	// Default result is negative and generic.
	: m_verify_error(CryptoXmlSignature::SIGNATURE_VERIFYING_GENERIC_ERROR)
{}


// Getters.

inline CryptoXmlSignature::VerifyError GadgetSignature::GetVerifyError() const
{ return m_verify_error; }

inline const CryptoCertificateChain* GadgetSignature::GetCertificateChain() const
{ return m_certificate_chain.get(); }

inline const OpStringC& GadgetSignature::GetSignatureFilename() const
{ return m_signature_filename; }


// Setters.

inline void GadgetSignature::SetVerifyError(CryptoXmlSignature::VerifyError verify_error)
{ m_verify_error = verify_error; }

inline void GadgetSignature::SetCertificateChain(CryptoCertificateChain* certificate_chain)
{ m_certificate_chain.reset(certificate_chain); }

inline void GadgetSignature::SetSignatureFilenameL(const OpStringC& filename)
{ m_signature_filename.SetL(filename); }

#endif // SIGNED_GADGET_SUPPORT
#endif // GADGET_SIGNATURE_H
