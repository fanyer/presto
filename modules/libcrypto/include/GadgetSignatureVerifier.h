/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file GadgetSignatureVerifier.h
 *
 * Gadget signature verifier interface.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef GADGET_SIGNATURE_VERIFIER_H
#define GADGET_SIGNATURE_VERIFIER_H

#ifdef SIGNED_GADGET_SUPPORT

#include "modules/libcrypto/include/CryptoXmlSignature.h"
#include "modules/libcrypto/include/OCSPCertificateChainVerifier.h"
class GadgetSignature;
class GadgetSignatureStorage;


/**
 * @class GadgetSignatureVerifier
 *
 * This class takes a gadget and verifies it, first locally, then using OCSP.
 *
 * The class user is supposed to start the verification, then wait
 * for MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED message,
 * unless an exception has been thrown during the starting process.
 *
 * This class is a processor.
 * It is asynchronous.
 *
 * Usage:
 *
 * 1) Starting:
 *
 *		@code
 *		// Verifier and signature storage can be allocated on heap
 *		// or as class members.
 *		GadgetSignatureVerifier m_verifier;
 *		GadgetSignatureStorage  m_signature_storage;
 *		// ...
 *		g_main_message_handler->SetCallBack(this,
 *			MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED, m_verifier.Id());
 *		m_verifier.SetGadgetFilenameL(zipped_gadget_filename);
 *		m_verifier.SetCAStorage(ca_storage);
 *		m_verifier.SetGadgetSignatureStorageContainer(&m_signature_storage);
 *		m_verifier.ProcessL();
 *		@endcode
 *
 * 2) Verification result handling:
 *
 *		@code
 *		// After the class user's class has received
 *		// MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED.
 *		CryptoXmlSignature::VerifyError verify_error = m_signature_storage.GetCommonVerifyError();
 *
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
 *
 *		// Verification successful.
 *		// Find out which signatures are present.
 *		unsigned int author_signature_count = m_signature_storage.GetAuthorSignatureCount();
 *		unsigned int distributor_signature_count = m_signature_storage.GetAuthorSignatureCount();
 *		...
 *		// Get author signature.
 *		const GadgetSignature* author_signature = m_signature_storage.GetAuthorSignature();
 *		...
 *		// Get distributor signature with index "idx".
 *		const GadgetSignature* distributor_signature = m_signature_storage.GetDistributorSignature(idx);
 *		...
 *		// Get verification result of the distributor signature.
 *		const CryptoCertificateChain* chain = distributor_signature->GetVerifyError();
 *		...
 *		// Checking all signatures is too boring. Get the best signature at once.
 *		const GadgetSignature* best_signature = m_signature_storage.GetBestSignature();
 *		// Best signature exists if verification is successful.
 *		OP_ASSERT(best_signature);
 *		// Best signature's verify error matches common verify error.
 *		OP_ASSERT(best_signature->GetVerifyError() == verify_error);
 *		...
 *		// Get certificate chain of the best signature.
 *		const CryptoCertificateChain* chain = best_signature->GetCertificateChain();
 *		// Certificate chain exists if verification is successful.
 *		OP_ASSERT(chain);
 *		// Certificate chain is not empty if verification is successful.
 *		OP_ASSERT(chain->GetNumberOfCertificates() > 0);
 *		...
 *		// Get signing certificate.
 *		const CryptoCertificate* cert = chain->GetCertificate(0);
 *		// Certificate exists if verification is successful.
 *		OP_ASSERT(cert);
 *		...
 *		// Get root CA certificate.
 *		int chain_len = chain->GetNumberOfCertificates();
 *		const CryptoCertificate* root_cert = chain->GetCertificate(chain_len - 1);
 *		// Certificate exists if verification is successful.
 *		OP_ASSERT(root_cert);
 *		...
 *		@endcode
 *
 */
class GadgetSignatureVerifier
#ifdef CRYPTO_OCSP_SUPPORT
	: public MessageObject
#endif
{
public:
	GadgetSignatureVerifier();
	virtual ~GadgetSignatureVerifier();

public:
	/** @name Functions to be called to start the verification. */
	/** @{ */

	/** Set the widget to be verified. */
	void SetGadgetFilenameL(const OpString& zipped_gadget_filename);

	/** Set trusted CA storage. */
	void SetCAStorage(const CryptoCertificateStorage* ca_storage);

	/** Set output container for gadget signatures.
	 *  The container's old content will be cleared if @ref ProcessL() is called.
	 */
	void SetGadgetSignatureStorageContainer(GadgetSignatureStorage* signature_storage);

	/** Helper function for getting this object's id. */
	MH_PARAM_1 Id() const;

	/** Process asynchronous verification of the gadget.
	 *
	 * Exception means that something went wrong during launching,
	 * for example OOM, bad input parameters or internal error.
	 * In this case no verification-finished message will be posted afterwards.
	 *
	 * If the exception is not thrown - the caller (or its appointed listener)
	 * should wait for the callback with the following parameters:
	 *
	 * - msg  == MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED;
	 * - par1 == Id() - this verifier object's id;
	 * - par2 == 0.
	 */
	void ProcessL();

	/** @} */

public:
#ifdef CRYPTO_OCSP_SUPPORT
	// MessageObject methods.
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
#endif

private:
	void VerifyOffline();
	void CheckWidgetFileExistsL();
	void CheckWidgetIsZipFileL();
	void LookForSignaturesL();
	static BOOL IsAuthorSignature(const OpStringC& pathname);
	static BOOL IsDistributorSignature(const OpStringC& pathname, unsigned int* distributor_signature_number = NULL);
	void AddDistributorSignatureFilenameL(const OpStringC& pathname, unsigned int number);
	void ProcessAllSignaturesOfflineL();
	void ProcessSignatureOfflineL(const OpStringC& signature_filename);
	void VerifySignatureOfflineL(
		const OpStringC& signature_filename,
		CryptoXmlSignature::VerifyError& verify_error,
		OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain);
	void CheckSignaturePropertiesL(
			const OpStringC& signature_filename,
			CryptoXmlSignature::VerifyError& verify_error,
			const OpStringC& signature_profile,
			const OpStringC& signature_role,
			const OpStringC& signature_identifier);
	void CheckAllFilesSignedL(
		const OpStringC& signature_filename,
		CryptoXmlSignature::VerifyError& verify_error,
		const OpStringHashTable <CryptoXmlSignature::SignedReference>& reference_objects);
	void AddSignatureToStorageL(
		const OpStringC& signature_filename,
		CryptoXmlSignature::VerifyError& verify_error,
		OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain);

#ifdef CRYPTO_OCSP_SUPPORT
	void VerifyOnline();
	void ScheduleOCSPProcessing();
	void LaunchOCSPProcessing();
	GadgetSignature* GetCurrentSignature() const;
	void ProcessOCSPL(GadgetSignature& gadget_signature);
	void ProcessOCSPVerificationResult();
	void FinishOCSPVerification(CryptoXmlSignature::VerifyError signature_verify_error, BOOL upgrade_common);
#endif

	void NotifyAboutFinishedVerification();

private:
	// These objects are set from outside. Pointed objects are not owned.
	OpString m_gadget_filename;
	const CryptoCertificateStorage* m_ca_storage;
	GadgetSignatureStorage* m_gadget_signature_storage;

private:
	// These objects are created and owned by this object.

	// Gadget zip file and zip container file list.
	OpZip m_gadget_file;
	OpAutoVector <OpString> m_file_list;

	// Signature filename related data.

	// If author signature is found - it's a pointer to the string
	// in m_file_list. Otherwise NULL.
	const OpStringC* m_author_signature_filename;

	// Stores distributor signature numbers (as in signatureNUMBER.xml).
	// This list must always be sorted (ascending).
	// According to the W3C spec, signatures must be processed in descending
	// order, thus this list will be traversed backwards.
	OpINT32Vector m_distributor_signature_list;

	// Maps distributor signature numbers (as in signatureNUMBER.xml)
	// to pointers to filenames contained in m_file_list.
	// If we had a map class where keys were guaranteed to be sorted,
	// we could avoid using m_distributor_signature_list.
	OpINT32HashTable <const OpStringC> m_number2filename;

	// Flat index of the current signature in m_gadget_signature_storage.
	// It's not the distributor signature number as in signatureNUMBER.xml!
	// Please refer to the documentation of @ref class GadgetSignatureStorage
	// for more verbose description.
	unsigned int m_current_signature_index;

#ifdef CRYPTO_OCSP_SUPPORT
	OCSPCertificateChainVerifier m_ocsp_verifier;
#endif
};

#endif // SIGNED_GADGET_SUPPORT
#endif // GADGET_SIGNATURE_VERIFIER_H
