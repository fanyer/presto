/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file GadgetSignatureStorage.h
 *
 * Gadget signature storage interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef GADGET_SIGNATURE_STORAGE_H
#define GADGET_SIGNATURE_STORAGE_H

#ifdef SIGNED_GADGET_SUPPORT

#include "modules/libcrypto/include/GadgetSignature.h"
class GadgetSignature;


/**
 * @class GadgetSignatureStorage
 *
 * This class stores information about all signatures of a gadget
 * plus final gadget signature verification status.
 * This class is a container.
 *
 * Usage:
 *
 *		@code
 *		GadgetSignatureStorage m_storage;
 *		...
 *		m_storage.AddDistributorSignatureL(signature100);
 *		m_storage.AddDistributorSignatureL(signature55);
 *		m_storage.SetCommonVerifyError(CryptoXmlSignature::OK_CHECKED_WITH_OCSP);
 *		m_storage.SetBestSignatureIndex(1);
 *		m_storage.AddDistributorSignatureL(signature10);
 *		m_storage.AddAuthorSignatureL(author_signature);
 *		...
 *		CryptoXmlSignature::VerifyError verify_error = m_storage.GetCommonVerifyError();
 *		switch (verify_error)
 *		{
 *			case CryptoXmlSignature::OK_CHECKED_LOCALLY:
 *				// Handle success.
 *
 *			case CryptoXmlSignature::OK_CHECKED_WITH_OCSP:
 *				// Handle "extended success".
 *
 *			case ...
 *				// Handle error.
 *		}
 *		...
 *		// If the verification is successful.
 *		const GadgetSignature* signature = m_storage.GetBestSignature();
 *		// Best signature exists if verification is successful.
 *		OP_ASSERT(best_signature);
 *		// Best signature's verify error matches common verify error.
 *		OP_ASSERT(best_signature->GetVerifyError() == verify_error);
 *		...
 *		@endcode
 */
class GadgetSignatureStorage
{
public:
	/** Constructor. */
	GadgetSignatureStorage();

public:
	/** @name Getters.
	 *  They don't give object ownership.
	 *  Used by gadgets module. */
	/** @{ */
	unsigned int GetAuthorSignatureCount() const;
	unsigned int GetDistributorSignatureCount() const;
	const GadgetSignature* GetAuthorSignature() const;
	const GadgetSignature* GetDistributorSignature(unsigned int index) const;
	const GadgetSignature* GetBestSignature() const;
	CryptoXmlSignature::VerifyError GetCommonVerifyError() const;
	/** @} */

public:
	/** @name Adders/setters.
	 *  They take over object ownership.
	 *  Used by GadgetSignatureVerifier class. */
	/** @{ */
	void AddAuthorSignatureL(GadgetSignature* signature);
	void AddDistributorSignatureL(GadgetSignature* signature);
	void SetCommonVerifyError(CryptoXmlSignature::VerifyError verify_error);
	void SetBestSignatureIndex(unsigned int index);
	/** @} */

public:
	/** Get "flat" index of a signature.
	 *
	 *  The index starts from 0 and denotes the place of a particular
	 *  signature in the signature processing order, according to
	 *  the W3C spec, i.e. distributor signatures sorted in descending
	 *  order of their signature numbers, then author signature.
	 *  I.e. if the index is less than GetDistributorSignatureCount(),
	 *  then it is denotes a distributor signature with the same index.
	 *  If it is equal to GetDistributorSignatureCount(),
	 *  then it denotes author signature. It must not be greater then
	 *  GetDistributorSignatureCount().
	 *
	 *  This function is an accessor.
	 *  It is used by GadgetSignatureVerifier class.
	 */
	GadgetSignature* GetSignatureByFlatIndex(unsigned int index);

public:
	/** Clear resources and return the object to its initial state.
	 *  This function is a clearer.
	 *  It is used by GadgetSignatureVerifier class.
	 */
	void Clear();

private:
	// Helper.
	const GadgetSignature* GetSignatureByFlatIndex(unsigned int index) const;

private:
	OpAutoPtr    <GadgetSignature>  m_author_signature;
	OpAutoVector <GadgetSignature>  m_distributor_signatures;
	// "Flat" index of the "best" signature, i.e. the primary signature
	// that will be displayed to user for example. The index starts
	// from 0 and denotes the place of a particular signature in
	// the signature processing order, according to the W3C spec,
	// i.e. distributor signatures sorted in descending order
	// of their signature numbers, then author signature.
	// I.e. if the index is less than GetDistributorSignatureCount(),
	// then it is denotes a distributor signature with the same index.
	// If it is equal to GetDistributorSignatureCount(),
	// then it denotes author signature. It must not be greater then
	// GetDistributorSignatureCount().
	unsigned int                    m_best_signature_index;
	CryptoXmlSignature::VerifyError m_verify_error;
};


// Inlines implementation.


// Constructor.

inline GadgetSignatureStorage::GadgetSignatureStorage()
	: m_best_signature_index(0)
	// Default result is negative and generic.
	, m_verify_error(CryptoXmlSignature::SIGNATURE_VERIFYING_GENERIC_ERROR)
{}


// Getters.

inline unsigned int GadgetSignatureStorage::GetAuthorSignatureCount() const
{ return (m_author_signature.get() ? 1 : 0); }

inline unsigned int GadgetSignatureStorage::GetDistributorSignatureCount() const
{ return m_distributor_signatures.GetCount(); }

inline const GadgetSignature* GadgetSignatureStorage::GetAuthorSignature() const
{ return m_author_signature.get(); }

inline const GadgetSignature* GadgetSignatureStorage::GetDistributorSignature(unsigned int index) const
{ return m_distributor_signatures.Get(index); }

inline const GadgetSignature* GadgetSignatureStorage::GetBestSignature() const
{ return GetSignatureByFlatIndex(m_best_signature_index); }

inline CryptoXmlSignature::VerifyError GadgetSignatureStorage::GetCommonVerifyError() const
{ return m_verify_error; }


// Adders/setters.

inline void GadgetSignatureStorage::AddAuthorSignatureL(GadgetSignature* signature)
{ m_author_signature.reset(signature); }

inline void GadgetSignatureStorage::AddDistributorSignatureL(GadgetSignature* signature)
{ LEAVE_IF_ERROR( m_distributor_signatures.Add(signature) ); }

inline void GadgetSignatureStorage::SetCommonVerifyError(CryptoXmlSignature::VerifyError verify_error)
{ m_verify_error = verify_error; }

inline void GadgetSignatureStorage::SetBestSignatureIndex(unsigned int index)
{ m_best_signature_index = index; }


// Clearer.

inline void GadgetSignatureStorage::Clear()
{
	m_verify_error = CryptoXmlSignature::SIGNATURE_VERIFYING_GENERIC_ERROR;
	m_best_signature_index = 0;
	m_distributor_signatures.DeleteAll();
	m_author_signature.reset(NULL);
}

#endif // SIGNED_GADGET_SUPPORT
#endif // GADGET_SIGNATURE_STORAGE_H
