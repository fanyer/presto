/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file GadgetSignatureVerifier.cpp
 *
 * Gadget signature verifier implementation.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#ifdef SIGNED_GADGET_SUPPORT

#include "modules/libcrypto/include/GadgetSignatureVerifier.h"
#include "modules/libcrypto/include/GadgetSignatureStorage.h"


// Limitations as mentioned in the internal spec.

// Maximum 100 distributor signatures.
// Rationale: hinder DOS by eating all processing power
// and all time of user's life.
#define MAX_DISTRIBUTOR_SIGNATURE_COUNT 100

// Maximum number of digits in distributor signature number is 9.
// I.e. the maximum number is 999 999 999 and the last possible
// distributor signature filename is "signature999999999.xml".
// Enough and fits into 32-bit int.
// Rationale: prevent parsing buffer overflows, integer overflows.
#define MAX_SIGNATURE_DIGIT_COUNT 9

// Maximum size of signature file is 640K. Should be enough for everyone.
// Rationale: hinder DOS by eating all memory.
#define MAX_SIGNATURE_FILE_SIZE (640 * 1024 * 1024)


GadgetSignatureVerifier::GadgetSignatureVerifier()
	: m_ca_storage(NULL)
	, m_gadget_signature_storage(NULL)
	, m_author_signature_filename(NULL)
	, m_current_signature_index(0)
{}


GadgetSignatureVerifier::~GadgetSignatureVerifier()
{
	// m_author_signature_filename  doesn't own the memory.
	// m_gadget_signature_storage   doesn't own the memory.
	// m_ca_storage                 doesn't own the memory.
}


void GadgetSignatureVerifier::SetGadgetFilenameL(const OpString& zipped_gadget_filename)
{
	m_gadget_filename.SetL(zipped_gadget_filename);
}


void GadgetSignatureVerifier::SetCAStorage(const CryptoCertificateStorage* ca_storage)
{
	m_ca_storage = ca_storage;
}


void GadgetSignatureVerifier::SetGadgetSignatureStorageContainer(GadgetSignatureStorage* signature_storage)
{
	m_gadget_signature_storage = signature_storage;
}


void GadgetSignatureVerifier::ProcessL()
{
	if (m_gadget_filename.IsEmpty() || !m_ca_storage || !m_gadget_signature_storage)
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);

	m_gadget_signature_storage->Clear();

	VerifyOffline();

#ifdef CRYPTO_OCSP_SUPPORT
	CryptoXmlSignature::VerifyError common_verify_error =
		m_gadget_signature_storage->GetCommonVerifyError();

	if (common_verify_error != CryptoXmlSignature::OK_CHECKED_LOCALLY)
	{
		// Offline verification already failed. Finish now.
		NotifyAboutFinishedVerification();
	}
	else
	{
		// Offline verification succeeded. Check OCSP.
		VerifyOnline();
	}
#else
	NotifyAboutFinishedVerification();
#endif
}


MH_PARAM_1 GadgetSignatureVerifier::Id() const
{
	return reinterpret_cast <MH_PARAM_1> (this);
}


#ifdef CRYPTO_OCSP_SUPPORT
void GadgetSignatureVerifier::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
		case MSG_GADGET_SIGNATURE_PROCESS_OCSP:
			LaunchOCSPProcessing();
			break;

		case MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_FINISHED:
			ProcessOCSPVerificationResult();
			break;
	}
}
#endif


void GadgetSignatureVerifier::VerifyOffline()
{
	TRAPD(status,
		CheckWidgetFileExistsL();
		CheckWidgetIsZipFileL();
		LookForSignaturesL();
		ProcessAllSignaturesOfflineL();
	);

	if (m_gadget_file.IsOpen())
		m_gadget_file.Close();

}


void GadgetSignatureVerifier::CheckWidgetFileExistsL()
{
	ANCHORD(OpFile, file);
	LEAVE_IF_ERROR(file.Construct(m_gadget_filename, OPFILE_ABSOLUTE_FOLDER));

	BOOL found = FALSE;
	LEAVE_IF_ERROR(file.Exists(found));
	if (found == FALSE)
	{
		OP_ASSERT(m_gadget_signature_storage);
		m_gadget_signature_storage->SetCommonVerifyError(CryptoXmlSignature::WIDGET_ERROR_ZIP_FILE_NOT_FOUND);
		LEAVE(OpStatus::ERR);
	}
}


void GadgetSignatureVerifier::CheckWidgetIsZipFileL()
{
	// Doing it using OpZip::Open() instead of OpZip::IsZipFile()
	// because the first variant contains the second plus additional checks.
	OP_STATUS status = m_gadget_file.Open(m_gadget_filename, /* write_perm = */ FALSE);
	if (OpStatus::IsError(status))
	{
		OP_ASSERT(m_gadget_signature_storage);
		m_gadget_signature_storage->SetCommonVerifyError(CryptoXmlSignature::WIDGET_ERROR_NOT_A_ZIPFILE);
		LEAVE(status);
	}

}


void GadgetSignatureVerifier::LookForSignaturesL()
{
	OP_ASSERT(m_gadget_file.IsOpen());

	m_number2filename.RemoveAll();
	m_distributor_signature_list.Clear();
	m_author_signature_filename = NULL;
	m_file_list.DeleteAll();

	LEAVE_IF_ERROR(m_gadget_file.GetFileNameList(m_file_list));

	const unsigned int file_count = m_file_list.GetCount();
	for (unsigned int file_idx = 0; file_idx < file_count; file_idx++)
	{
		// Get pathname of a zip file entry (file or directory).
		const OpString* entry = m_file_list.Get(file_idx);
		OP_ASSERT(entry);

		// Skip directories.
		const OpStringC& pathname = *entry;
		const int len = pathname.Length();
		OP_ASSERT(len > 0);
		// Condition to check if it's not a directory taken from OpZipFolderLister::IsFolder().
		if (pathname[len - 1] == (uni_char)PATHSEPCHAR)
			continue;

		// Check if it is an author signature.
		if (IsAuthorSignature(pathname))
		{
			m_author_signature_filename = entry;
			continue;
		}

		// Check if it is a distributor signature.
		unsigned int distributor_signature_number = 0;
		if (IsDistributorSignature(pathname, &distributor_signature_number))
		{
			OP_ASSERT(distributor_signature_number != 0);
			AddDistributorSignatureFilenameL(pathname, distributor_signature_number);
			continue;
		}
	}
}


BOOL GadgetSignatureVerifier::IsAuthorSignature(const OpStringC& pathname)
{
	return (pathname == UNI_L("author-signature.xml"));
}


BOOL GadgetSignatureVerifier::IsDistributorSignature(const OpStringC& pathname, unsigned int* distributor_signature_number)
{
	// Check if it is a distributor signature.

	// Prefix.
	const uni_char* prefix = UNI_L("signature");
	const unsigned int prefix_len = uni_strlen(prefix);

	// Suffix.
	const uni_char* suffix = UNI_L(".xml");
	const unsigned int suffix_len = uni_strlen(suffix);

	// Possible string lengths.
	const unsigned int min_len = prefix_len + 1 + suffix_len;
	const unsigned int max_len = prefix_len + MAX_SIGNATURE_DIGIT_COUNT + suffix_len;
	const unsigned int len = pathname.Length();

	// Check string length.
	if (len < min_len || len > max_len)
		// Wrong filename length. Not a signature.
		return FALSE;

	// Check prefix.
	if (pathname.Compare(prefix, prefix_len))
		return FALSE;

	// Check suffix.
	const unsigned int real_suffix_pos = len - suffix_len;
	const uni_char* real_suffix = pathname.CStr() + real_suffix_pos;
	if (uni_strcmp(real_suffix, suffix))
		return FALSE;

	// Check digits.
	const unsigned int expected_digit_count = real_suffix_pos - prefix_len;
	OP_ASSERT(expected_digit_count >= 1 && expected_digit_count <= MAX_SIGNATURE_DIGIT_COUNT);
	const uni_char* expected_digits = pathname.CStr() + prefix_len;
	const unsigned int real_digit_count = uni_strspn(expected_digits, UNI_L("0123456789"));
	if (real_digit_count != expected_digit_count || expected_digits[0] == (uni_char)'0')
		return FALSE;

	// Calculate distributor_signature_number.
	if (distributor_signature_number)
	{
		// Copy digits because we need a null-terminated string.
		uni_char digits[MAX_SIGNATURE_DIGIT_COUNT + 1]; /* ARRAY OK 2010-10-26 alexeik */
		uni_strncpy(digits, expected_digits, expected_digit_count);
		digits[expected_digit_count] = (uni_char)'\0';
		OP_ASSERT(uni_strlen(digits) == expected_digit_count);

		// Using atoi(), because we are sure that expected_digits
		// consists entirely of digits, thus the conversion should not fail.
		*distributor_signature_number = uni_atoi(digits);
	}

	return TRUE;
}


void GadgetSignatureVerifier::AddDistributorSignatureFilenameL(const OpStringC& pathname, unsigned int number)
{
	// OpINT32Vector stores signed integers. OpUINT32Vector does not exist. :(
	// Let's convert our argument now, check the value and avoid compiler warnings.
	INT32 signed_number = static_cast <INT32> (number);
	OP_ASSERT(sizeof(INT32) == sizeof(unsigned int));
	OP_ASSERT(signed_number >= 0);

	// Check if we reached MAX_DISTRIBUTOR_SIGNATURE_COUNT limit.
	const unsigned int distributor_signature_count = m_distributor_signature_list.GetCount();
	if (distributor_signature_count >= MAX_DISTRIBUTOR_SIGNATURE_COUNT)
	{
		// Ignore extra signatures.
		return;
	}

	// W3C spec, 9.1.3: sort the list of signatures by the file name
	// in ascending numerical order.
	// For example, signature1.xml followed by signature2.xml
	// followed by signature3.xml and so on.
	// As another example, signature9.xml followed by signature44.xml
	// followed by signature122134.xml and so on.
	unsigned int insert_idx = m_distributor_signature_list.Search(signed_number);
	OP_ASSERT(insert_idx <= distributor_signature_count);

	// Insert new signature number only if it's not a duplicate.
	// Duplicates might happen if we have a bogus zip file with
	// duplicate filenames.
	if (insert_idx == distributor_signature_count ||
			m_distributor_signature_list.Get(insert_idx) != signed_number)
	{
		OP_STATUS status = m_distributor_signature_list.Insert(insert_idx, signed_number);
		LEAVE_IF_ERROR(status);

		status = m_number2filename.Add(signed_number, &pathname);
		if (OpStatus::IsError(status))
		{
			m_distributor_signature_list.Remove(insert_idx);
			LEAVE(status);
		}
	}

	OP_ASSERT(m_distributor_signature_list.GetCount() == (UINT32)m_number2filename.GetCount());
	OP_ASSERT(m_number2filename.GetCount() > 0);
}


void GadgetSignatureVerifier::ProcessAllSignaturesOfflineL()
{
	const unsigned int distributor_signature_count = m_distributor_signature_list.GetCount();
	if (!m_author_signature_filename && distributor_signature_count == 0)
	{
		OP_ASSERT(m_gadget_signature_storage);
		m_gadget_signature_storage->SetCommonVerifyError(CryptoXmlSignature::SIGNATURE_FILE_MISSING);
		LEAVE(OpStatus::ERR);
	}

	// W3C spec, 9.1.6: Validate the signature files in the signatures list in
	// descending numerical order, with distributor signatures first (if any).
	// For example, validate signature3.xml, then signature2.xml,
	// then signature1.xml and lastly author-signature.xml.
	// As another example, validate signature122134.xml, then signature44.xml,
	// and then signature9.xml, and lastly author-signature.xml.

	for (m_current_signature_index = 0; m_current_signature_index < distributor_signature_count; m_current_signature_index++)
	{
		unsigned int distributor_signature_idx =
			distributor_signature_count - 1 - m_current_signature_index;

		// Resolve signature filename.
		unsigned int signature_number = m_distributor_signature_list.Get(distributor_signature_idx);
		const OpStringC* signature_filename = NULL;
		LEAVE_IF_ERROR( m_number2filename.GetData(signature_number, &signature_filename) );
		OP_ASSERT(signature_filename);

		ProcessSignatureOfflineL(*signature_filename);
	}

	if (m_author_signature_filename)
		ProcessSignatureOfflineL(*m_author_signature_filename);
}


void GadgetSignatureVerifier::ProcessSignatureOfflineL(const OpStringC& signature_filename)
{
	OP_ASSERT(m_gadget_signature_storage);

	CryptoXmlSignature::VerifyError verify_error = CryptoXmlSignature::SIGNATURE_FILE_XML_GENERIC_ERROR;

	ANCHORD(OpAutoPtr <CryptoCertificateChain>, signer_certificate_chain);

	TRAPD(status,
		VerifySignatureOfflineL(signature_filename, verify_error, signer_certificate_chain)
	);

	AddSignatureToStorageL(signature_filename, verify_error, signer_certificate_chain);

	// Applying policy "any valid signature is required."

	// If it's the first signature - set its error code as common error code.
	if (m_current_signature_index == 0)
	{
		m_gadget_signature_storage->SetCommonVerifyError(verify_error);
		// The best signature index is already set to 0, because we have
		// cleared m_gadget_signature_storage in ProcessL().
		OP_ASSERT(m_gadget_signature_storage->GetBestSignature() ==
		          m_gadget_signature_storage->GetSignatureByFlatIndex(m_current_signature_index));
		// No need to update best signature index.
	}
	else
	{
		CryptoXmlSignature::VerifyError current_common_verify_error =
			m_gadget_signature_storage->GetCommonVerifyError();

		// If it's the first successfully verified signature - set error code to success.
		if (current_common_verify_error != CryptoXmlSignature::OK_CHECKED_LOCALLY &&
		    verify_error == CryptoXmlSignature::OK_CHECKED_LOCALLY)
		{
			m_gadget_signature_storage->SetCommonVerifyError(verify_error);
			m_gadget_signature_storage->SetBestSignatureIndex(m_current_signature_index);
		}

		// Otherwise maintain the current error code.
	}

	// If we reached this point - best signature exists.
	OP_ASSERT(m_gadget_signature_storage->GetBestSignature());

	// Common error matches the best signature error.
	OP_ASSERT(m_gadget_signature_storage->GetCommonVerifyError() ==
	          m_gadget_signature_storage->GetBestSignature()->GetVerifyError());
}


void GadgetSignatureVerifier::VerifySignatureOfflineL(
	const OpStringC& signature_filename,
	CryptoXmlSignature::VerifyError& verify_error,
	OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain)
{
	OP_ASSERT(m_gadget_file.IsOpen());

	// Check signature file size.
	{
		ANCHORD(OpString, signature_filename_tmp);
		signature_filename_tmp.SetL(signature_filename);

		const int zip_entry_idx = m_gadget_file.GetFileIndex(
			&signature_filename_tmp, /* unused argument */ NULL);
		if (zip_entry_idx < 0)
			LEAVE(OpStatus::ERR_FILE_NOT_FOUND);

		OpZip::file_attributes zip_entry_attr;
		m_gadget_file.GetFileAttributes(zip_entry_idx, &zip_entry_attr);

		if (zip_entry_attr.length > MAX_SIGNATURE_FILE_SIZE)
			LEAVE(OpStatus::ERR);
	}

	ANCHORD(OpString, signature_full_path);
	signature_full_path.SetL(m_gadget_filename);
	signature_full_path.AppendL(UNI_L(PATHSEP));
	signature_full_path.AppendL(signature_filename);

	// Containers for parsing results.
	OpAutoStringHashTable <CryptoXmlSignature::SignedReference> reference_objects(/* case_sensitive = */ TRUE);
	ANCHOR(OpAutoStringHashTable <CryptoXmlSignature::SignedReference>, reference_objects);
	ANCHORD(OpAutoVector <TempBuffer>, X509_certificates);
	ANCHORD(ByteBuffer, signed_info_element_signature);
	CryptoCipherAlgorithm signature_cipher_type;
	ANCHORD(CryptoHashAlgorithm, signature_hash_type);
	ANCHORD(ByteBuffer, canonicalized_signed_info);
	ANCHORD(OpAutoPtr <CryptoHash>, canonicalized_signed_info_hash);
	ANCHORD(OpString, canonicalization_method);
	ANCHORD(OpString, signature_profile);
	ANCHORD(OpString, signature_role);
	ANCHORD(OpString, signature_identifier);

	// Parse XML.
	OP_STATUS status = CryptoXmlSignature::ParseSignaturXml(
		signature_full_path.CStr(),
		FALSE,
		verify_error,
		reference_objects,
		X509_certificates,
		signed_info_element_signature,
		signature_cipher_type,
		signature_hash_type,
		canonicalized_signed_info,
		canonicalized_signed_info_hash,
		canonicalization_method,
		signature_profile,
		signature_role,
		signature_identifier);
	LEAVE_IF_ERROR(status);

	// Check signature properties.
	CheckSignaturePropertiesL(
		signature_filename,
		verify_error,
		signature_profile,
		signature_role,
		signature_identifier);

	// Check the signing of <SignedInfo> element, create cert chain.
	CryptoCertificateChain* certificate_chain = NULL;
	status = CryptoXmlSignature::CreateX509CertificateFromBinary(
		certificate_chain,
		m_ca_storage,
		verify_error,
		X509_certificates);
	LEAVE_IF_ERROR(status);

	// Write certificate_chain into output parameter signer_certificate_chain.
	signer_certificate_chain = certificate_chain;
	// certificate_chain memory is now owned by signer_certificate_chain.

	// Check that all files are signed.
	CheckAllFilesSignedL(signature_filename, verify_error, reference_objects);

	// Check the signature.
	status = CryptoXmlSignature::CheckHashSignature(
		signed_info_element_signature,
		signature_hash_type,
		certificate_chain,
		canonicalized_signed_info_hash.get(),
		verify_error);
	LEAVE_IF_ERROR(status);

	// Check that the file-digests are correct.
	status = CryptoXmlSignature::CheckFileDigests(
		m_gadget_filename.CStr(),
		verify_error,
		reference_objects);
	LEAVE_IF_ERROR(status);

	verify_error = CryptoXmlSignature::OK_CHECKED_LOCALLY;
}


void GadgetSignatureVerifier::CheckSignaturePropertiesL(
		const OpStringC& signature_filename,
		CryptoXmlSignature::VerifyError& verify_error,
		const OpStringC& signature_profile,
		const OpStringC& signature_role,
		const OpStringC& signature_identifier)
{
	do
	{
		if (signature_profile != UNI_L("http://www.w3.org/ns/widgets-digsig#profile"))
			break;

		if (IsAuthorSignature(signature_filename) &&
		    signature_role != UNI_L("http://www.w3.org/ns/widgets-digsig#role-author"))
				break;

		if (IsDistributorSignature(signature_filename) &&
		    signature_role != UNI_L("http://www.w3.org/ns/widgets-digsig#role-distributor"))
				break;

		if (signature_identifier.IsEmpty())
			break;

		return;

	} while (0);

	verify_error = CryptoXmlSignature::SIGNATURE_VERIFYING_WRONG_PROPERTIES;
	LEAVE(OpStatus::ERR);
}


void GadgetSignatureVerifier::CheckAllFilesSignedL(
	const OpStringC& signature_filename,
	CryptoXmlSignature::VerifyError& verify_error,
	const OpStringHashTable <CryptoXmlSignature::SignedReference>& reference_objects)
{
	const unsigned int file_count = m_file_list.GetCount();
	for (unsigned int file_idx = 0; file_idx < file_count; file_idx++)
	{
		// Get pathname of a zip file entry (file or directory).
		const OpString* entry = m_file_list.Get(file_idx);
		OP_ASSERT(entry);

		// Skip directories.
		const OpStringC& pathname = *entry;
		const int len = pathname.Length();
		OP_ASSERT(len > 0);
		// Condition to check if it's not a directory taken from OpZipFolderLister::IsFolder().
		if (pathname[len - 1] == (uni_char)PATHSEPCHAR)
			continue;

		// Distributor signatures are not signed. Skip them.
		if (IsDistributorSignature(pathname))
			continue;

		// Author signature is signed by distributor signatures,
		// but not by the author signature itself.
		if (IsAuthorSignature(pathname) && IsAuthorSignature(signature_filename))
			continue;

		// Check if pathname is in the signed objects list.
		if ( !reference_objects.Contains(pathname.CStr()) )
		{
			OP_ASSERT(m_gadget_signature_storage);
			verify_error = CryptoXmlSignature::WIDGET_SIGNATURE_VERIFYING_FAILED_ALL_FILES_NOT_SIGNED;
			LEAVE(OpStatus::ERR);
		}
	}
}


void GadgetSignatureVerifier::AddSignatureToStorageL(
	const OpStringC& signature_filename,
	CryptoXmlSignature::VerifyError& verify_error,
	OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain)
{
	// Create GadgetSignature.
	GadgetSignature* gadget_signature = OP_NEW_L(GadgetSignature, ());
	OP_ASSERT(gadget_signature);
	ANCHOR_PTR(GadgetSignature, gadget_signature);

	// Fill GadgetSignature.
	gadget_signature->SetSignatureFilenameL(signature_filename);
	gadget_signature->SetVerifyError(verify_error);
	gadget_signature->SetCertificateChain(signer_certificate_chain.get());

	// Certificate chain memory is now owned by gadget_signature.
	signer_certificate_chain.release();

	// Add GadgetSignature to GadgetSignatureStorage.
	// Decide on role.
	OP_ASSERT(IsAuthorSignature(signature_filename) ^ IsDistributorSignature(signature_filename));
	if (IsAuthorSignature(signature_filename))
		m_gadget_signature_storage->AddAuthorSignatureL(gadget_signature);
	else
		m_gadget_signature_storage->AddDistributorSignatureL(gadget_signature);

	// gadget_signature is now owned by m_gadget_signature_storage.
	ANCHOR_PTR_RELEASE(gadget_signature);
}


#ifdef CRYPTO_OCSP_SUPPORT
void GadgetSignatureVerifier::VerifyOnline()
{
	// Initialize common error with the worst error code, then "upgrade"
	// the error code when more information is available.
	OP_ASSERT(m_gadget_signature_storage);
	m_gadget_signature_storage->SetCommonVerifyError(
		CryptoXmlSignature::CERTIFICATE_REVOKED);

	m_current_signature_index = 0;
	g_main_message_handler->SetCallBack(this, MSG_GADGET_SIGNATURE_PROCESS_OCSP, Id());
	ScheduleOCSPProcessing();
}


void GadgetSignatureVerifier::ScheduleOCSPProcessing()
{
	OP_ASSERT(g_main_message_handler);
	g_main_message_handler->PostMessage(
		MSG_GADGET_SIGNATURE_PROCESS_OCSP, Id(), 0);
}


void GadgetSignatureVerifier::LaunchOCSPProcessing()
{
	OP_ASSERT(m_gadget_signature_storage);

	GadgetSignature* signature = GetCurrentSignature();

	if (signature)
	{
		// Process OCSP for this signature.
		TRAPD(status, ProcessOCSPL(*signature));

		if (OpStatus::IsError(status))
		{
			CryptoXmlSignature::VerifyError signature_verify_error =
				signature->GetVerifyError();
			CryptoXmlSignature::VerifyError common_verify_error =
				m_gadget_signature_storage->GetCommonVerifyError();
			BOOL upgrade_common =
				(signature_verify_error == CryptoXmlSignature::OK_CHECKED_LOCALLY) &&
				(common_verify_error    == CryptoXmlSignature::CERTIFICATE_REVOKED);

			FinishOCSPVerification(signature_verify_error, upgrade_common);
		}
	}
	else
	{
#ifdef _DEBUG
		const unsigned int total_signature_count =
			m_gadget_signature_storage->GetDistributorSignatureCount() +
			m_gadget_signature_storage->GetAuthorSignatureCount();
		OP_ASSERT(m_current_signature_index == total_signature_count);
#endif

		NotifyAboutFinishedVerification();
	}
}


GadgetSignature* GadgetSignatureVerifier::GetCurrentSignature() const
{
	OP_ASSERT(m_gadget_signature_storage);

	GadgetSignature* signature =
		m_gadget_signature_storage->GetSignatureByFlatIndex(m_current_signature_index);

#ifdef _DEBUG
	const unsigned int distributor_signature_count =
		m_gadget_signature_storage->GetDistributorSignatureCount();

	if (m_current_signature_index < distributor_signature_count)
	{
		// Distributor signature.
		OP_ASSERT(signature);
		OP_ASSERT(IsDistributorSignature(signature->GetSignatureFilename()));
	}
	else if (m_current_signature_index == distributor_signature_count &&
			 m_gadget_signature_storage->GetAuthorSignatureCount())
	{
		// Author signature.
		OP_ASSERT(signature);
		OP_ASSERT(IsAuthorSignature(signature->GetSignatureFilename()));
	}
	else
		OP_ASSERT(!signature);
#endif

	return signature;
}


void GadgetSignatureVerifier::ProcessOCSPL(GadgetSignature& gadget_signature)
{
	CryptoXmlSignature::VerifyError local_error_code = gadget_signature.GetVerifyError();
	if (local_error_code != CryptoXmlSignature::OK_CHECKED_LOCALLY)
	{
		// The signature was not verified successfully locally.
		// As local verification has failed already,
		// it doesn't make sense to do OCSP check.
		// It may even be impossible, because the certificate chain
		// may not have been built.

		// Let's LEAVE and LaunchOCSPProcessing() will
		// ScheduleOCSPProcessing() of the next signature.
		LEAVE(OpStatus::ERR_OUT_OF_RANGE);
	}

	// The local verification succeeded, the certificate chain must be present.
	const CryptoCertificateChain* certificate_chain = gadget_signature.GetCertificateChain();
	OP_ASSERT(certificate_chain);

	m_ocsp_verifier.SetCertificateChain(certificate_chain);
	m_ocsp_verifier.SetCAStorage(m_ca_storage);
	m_ocsp_verifier.SetRetryCount(CRYPTO_GADGET_SIGNATURE_VERIFICATION_OCSP_RETRY_COUNT);
	m_ocsp_verifier.SetTimeout(CRYPTO_GADGET_SIGNATURE_VERIFICATION_OCSP_TIMEOUT);
	g_main_message_handler->SetCallBack(
		this, MSG_OCSP_CERTIFICATE_CHAIN_VERIFICATION_FINISHED, m_ocsp_verifier.Id());
	m_ocsp_verifier.ProcessL();
}


void GadgetSignatureVerifier::ProcessOCSPVerificationResult()
{
	CryptoXmlSignature::VerifyError common_verify_error =
		m_gadget_signature_storage->GetCommonVerifyError();
	CryptoXmlSignature::VerifyError signature_verify_error;

	BOOL upgrade_common = FALSE;

	// Determine signature_verify_error and "upgrade" common_verify_error if necessary.
	CryptoCertificateChain::VerifyStatus verify_status = m_ocsp_verifier.GetVerifyStatus();
	switch (verify_status)
	{
		case CryptoCertificateChain::OK_CHECKED_WITH_OCSP:
			signature_verify_error = CryptoXmlSignature::OK_CHECKED_WITH_OCSP;
			// Only upgrade status.
			if (common_verify_error != CryptoXmlSignature::OK_CHECKED_WITH_OCSP)
				upgrade_common = TRUE;
			break;

		case CryptoCertificateChain::CERTIFICATE_REVOKED:
			signature_verify_error = CryptoXmlSignature::CERTIFICATE_REVOKED;
			// Only upgrade status.
			if (common_verify_error != CryptoXmlSignature::OK_CHECKED_WITH_OCSP &&
			    common_verify_error != CryptoXmlSignature::OK_CHECKED_LOCALLY &&
			    common_verify_error != CryptoXmlSignature::CERTIFICATE_REVOKED
			)
				upgrade_common = TRUE;
			break;

		default:
			OP_ASSERT(verify_status == CryptoCertificateChain::VERIFY_STATUS_UNKNOWN);
			signature_verify_error = CryptoXmlSignature::OK_CHECKED_LOCALLY;
			// Only upgrade status.
			if (common_verify_error != CryptoXmlSignature::OK_CHECKED_WITH_OCSP &&
			    common_verify_error != CryptoXmlSignature::OK_CHECKED_LOCALLY
			)
				upgrade_common = TRUE;
	}

	GadgetSignature* signature = GetCurrentSignature();
	if (signature)
		signature->SetVerifyError(signature_verify_error);
	else
		OP_ASSERT(!"Should be impossible: NULL signature pointer!");

	FinishOCSPVerification(signature_verify_error, upgrade_common);

	// If we reached this point - best signature exists.
	OP_ASSERT(m_gadget_signature_storage->GetBestSignature());

	// Common error matches the best signature error.
	OP_ASSERT(m_gadget_signature_storage->GetCommonVerifyError() ==
	          m_gadget_signature_storage->GetBestSignature()->GetVerifyError());
}


void GadgetSignatureVerifier::FinishOCSPVerification(CryptoXmlSignature::VerifyError signature_verify_error, BOOL upgrade_common)
{
	if (upgrade_common)
	{
		m_gadget_signature_storage->SetCommonVerifyError(signature_verify_error);
		m_gadget_signature_storage->SetBestSignatureIndex(m_current_signature_index);
	}

	m_current_signature_index++;
	ScheduleOCSPProcessing();
}
#endif // CRYPTO_OCSP_SUPPORT


void GadgetSignatureVerifier::NotifyAboutFinishedVerification()
{
	OP_ASSERT(g_main_message_handler);

#ifdef CRYPTO_OCSP_SUPPORT
	g_main_message_handler->UnsetCallBacks(this);
#endif

	g_main_message_handler->PostMessage(
		MSG_GADGET_SIGNATURE_VERIFICATION_FINISHED, Id(), 0);
}

#endif // SIGNED_GADGET_SUPPORT
