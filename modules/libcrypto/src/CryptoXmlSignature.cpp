/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/
#include "core/pch.h"

#ifdef CRYPTO_XML_DIGITAL_SIGNING_SUPPORT

#include "modules/libcrypto/include/CryptoXmlSignature.h"

#include "modules/formats/base64_decode.h"
#include "modules/libcrypto/include/CryptoCertificate.h"
#include "modules/libcrypto/include/CryptoCertificateChain.h"
#include "modules/libcrypto/include/CryptoCertificateStorage.h"
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/locale/locale-enum.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/OpHashTable.h"
#include "modules/util/zipload.h"
#include "modules/xmlutils/xmlnames.h"


#if defined(_DEBUG) || defined(SELFTEST)

#ifdef CASE_ELEMENT
#error "#define collision: CASE_ELEMENT is already defined."
#endif
#define CASE_ELEMENT(code) case code: return #code

/* static */
const char* CryptoXmlSignature::VerifyErrorName(VerifyError error)
{
	switch (error)
	{
		CASE_ELEMENT(OK_CHECKED_LOCALLY);
		CASE_ELEMENT(OK_CHECKED_WITH_OCSP);
		CASE_ELEMENT(CERTIFICATE_EXPIRED);
		CASE_ELEMENT(CERTIFICATE_REVOKED);
		CASE_ELEMENT(CERTIFICATE_INVALID_CERTIFICATE_CHAIN);
		CASE_ELEMENT(CERTIFICATE_INVALID_CERTIFICATE_ISSUER);
		CASE_ELEMENT(CERTIFICATE_PARSE_ERROR);
		CASE_ELEMENT(CERTIFICATE_ERROR_BAD_CERTIFICATE);
		CASE_ELEMENT(WIDGET_ERROR_NOT_A_ZIPFILE);
		CASE_ELEMENT(WIDGET_ERROR_ZIP_FILE_NOT_FOUND);
		CASE_ELEMENT(WIDGET_SIGNATURE_VERIFYING_FAILED_ALL_FILES_NOT_SIGNED);
		CASE_ELEMENT(SIGNATURE_FILE_MISSING);
		CASE_ELEMENT(SIGNATURE_FILE_XML_GENERIC_ERROR);
		CASE_ELEMENT(SIGNATURE_FILE_XML_PARSE_ERROR);
		CASE_ELEMENT(SIGNATURE_FILE_XML_WRONG_ALGORITHM);
		CASE_ELEMENT(SIGNATURE_FILE_XML_MISSING_ARGS);
		CASE_ELEMENT(SIGNATURE_VERIFYING_FAILED);
		CASE_ELEMENT(SIGNATURE_VERIFYING_WRONG_PROPERTIES);
		CASE_ELEMENT(SIGNATURE_VERIFYING_REFERENCE_FILE_NOT_FOUND);
		CASE_ELEMENT(SIGNATURE_VERIFYING_FAILED_WRONG_DIGEST);
		CASE_ELEMENT(SIGNATURE_VERIFYING_GENERIC_ERROR);
	}

	return "(Unknown CryptoXmlSignature::VerifyError)";
}

#undef CASE_ELEMENT
#endif

/* static */
OP_STATUS CryptoXmlSignature::SignedReference::Make(
	SignedReference*& reference,
	const uni_char* file,
	const OpStringC& transform_algorithm,
	CryptoHashAlgorithm digest_method,
	const ByteBuffer& digest_value)
{
	OP_STATUS status;
	unsigned digest_value_length;
	char *temp = digest_value.GetChunk(0, &digest_value_length);

	// Unescape "%20" and similar characters.
	URL normalized_url = g_url_api->GetURL(file, 0, TRUE);
	OpString file_path;
	RETURN_IF_ERROR(normalized_url.GetAttribute(URL::KUniPath, file_path));

	// file_path must not be empty.
	if (file_path.IsEmpty())
		return OpStatus::ERR;

	// file_path must not start with '/' or whitespace.
	uni_char first_char = file_path[0];
	if (first_char == (uni_char)'/' || uni_isspace(first_char))
		return OpStatus::ERR;

	// URL shouldn't leave '\' in file_path, let's check it.
	if (file_path.FindFirstOf((uni_char)'\\') != KNotFound)
		return OpStatus::ERR;

	// We'll need this length in 2 places, so let's calculate it here.
	const int file_path_len = file_path.Length();

	// URL shouldn't leave "/../" elements in the path, let's check it.
	{
		const int index = file_path.Find(UNI_L(".."));
		if (index != KNotFound)
		{
			OP_ASSERT(index >= 0);
			const int index_after = index + 2;
			OP_ASSERT(index_after <= file_path_len);

			// Check if ".." is both prefixed and suffixed by "/".
			if (
				(index == 0                   || file_path[index - 1]   == (uni_char)'/') &&
				(index_after == file_path_len || file_path[index_after] == (uni_char)'/')
			   )
			{
				// Potential security issue.
				//OP_ASSERT(!"Path has a reference to a parent directory!");
				return OpStatus::ERR;
			}
		}
	}

#if PATHSEPCHAR == '\\'
	// Convert '/' to '\\' in file paths if PATHSEPCHAR
	// is '\\'. Paths must only have '/' separators in XML,
	// it's mandated by the standard, thus the opposite
	// conversion (from '\\' to '/') is not supported.
	{
		for (int i = 0; i < file_path_len; i++)
			if (file_path[i] == (uni_char)'/')
				file_path[i] = (uni_char)'\\';
	}
#endif

	reference = OP_NEW(CryptoXmlSignature::SignedReference, ());
	RETURN_OOM_IF_NULL(reference);

	reference->m_digest_method = digest_method;

	if (
			OpStatus::IsError(status = reference->m_file_path.Set(file_path.CStr())) ||
			OpStatus::IsError(status = reference->m_transform_algorithm.Set(transform_algorithm)) ||
			OpStatus::IsError(status = reference->m_digest_value.AppendBytes(temp, digest_value_length))
		)
	{
		OP_DELETE(reference);
		reference = NULL;
		return status;
	}

	return OpStatus::OK;
	
}

#ifdef GADGET_SUPPORT
/* static */ OP_STATUS
CryptoXmlSignature::VerifyWidgetSignature(
	OpAutoPtr <CryptoCertificate>& signer_certificate,
	VerifyError& reason,
	const uni_char* zipped_widget_filename,
	const CryptoCertificateStorage* ca_storage)
{
	OpAutoPtr <CryptoCertificateChain> signer_certificate_chain;
	OP_STATUS result = VerifyWidgetSignature(
		signer_certificate_chain,
		reason,
		zipped_widget_filename,
		ca_storage);

	if (signer_certificate_chain.get() != NULL)
		signer_certificate = signer_certificate_chain->RemoveCertificate(0);

	return result;
}


/* static */ OP_STATUS
CryptoXmlSignature::VerifyWidgetSignature(
	OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain,
	VerifyError& reason,
	const uni_char* zipped_widget_filename,
	const CryptoCertificateStorage* ca_storage)
{
	if (!zipped_widget_filename || !ca_storage)
		return OpStatus::ERR_OUT_OF_RANGE;

	signer_certificate_chain = NULL;
	reason = SIGNATURE_VERIFYING_GENERIC_ERROR;
	OpString base_path;
	RETURN_IF_ERROR(base_path.Set(zipped_widget_filename));
	RETURN_IF_ERROR(base_path.Append(PATHSEP));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(zipped_widget_filename, OPFILE_ABSOLUTE_FOLDER));

	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (found == FALSE)
	{
		reason = WIDGET_ERROR_ZIP_FILE_NOT_FOUND;
		return OpStatus::ERR;
	}
	file.Close();

	// Check if it's a zip file.
#ifdef UTIL_ZIP_CACHE
	if(!g_zipcache->IsZipFile(zipped_widget_filename))
#else // !UTIL_ZIP_CACHE
	OpZip zip_file;
	// Doing it using zip_file.Open() instead of zip_file.IsZipFile()
	// because the first variant contains the second plus additional checks.
	if (OpStatus::IsError(zip_file.Open(zipped_widget_filename)))
#endif // !UTIL_ZIP_CACHE
	{
		reason = WIDGET_ERROR_NOT_A_ZIPFILE;
		return OpStatus::ERR;
	}
#ifndef UTIL_ZIP_CACHE
	zip_file.Close();
#endif
	// Checked, it's a zip file.

	OpAutoVector<TempBuffer> X509_certificates;
	ByteBuffer signed_info_element_signature;
	ByteBuffer canonicalized_signed_info;
	OpString canonicalization_method;
	OpAutoStringHashTable<SignedReference> reference_objects(/* case_sensitive = */ TRUE);
	OpAutoPtr<CryptoHash> canonicalized_signed_info_hash;

	OpString xml_signature_file;
	OpString xml_signature_basename;
	RETURN_IF_ERROR(xml_signature_file.Set(base_path));
#ifdef DOM_JIL_API_SUPPORT
	RETURN_IF_ERROR(xml_signature_basename.Set("signature1.xml"));
#else
	RETURN_IF_ERROR(xml_signature_basename.Set("signature.xml"));
#endif // DOM_JIL_API_SUPPORT
	RETURN_IF_ERROR(xml_signature_file.Append(xml_signature_basename));
	
	CryptoHashAlgorithm signature_hash_type;
	CryptoCipherAlgorithm signature_cipher_type;
	OpString signature_profile;
	OpString signature_role;
	OpString signature_identifier;

	OP_STATUS status = ParseSignaturXml(
		xml_signature_file.CStr(),
		FALSE,
		reason,
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

#ifdef DOM_JIL_API_SUPPORT
	if (OpStatus::IsError(status) && reason == SIGNATURE_FILE_MISSING)
	{
		xml_signature_file.Empty();
		RETURN_IF_ERROR(xml_signature_basename.Set("signature.xml"));
		RETURN_IF_ERROR(xml_signature_file.Set(base_path));
		RETURN_IF_ERROR(xml_signature_file.Append(xml_signature_basename));
		status = ParseSignaturXml(
			xml_signature_file.CStr(),
			FALSE,
			reason,
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
	}
#endif // DOM_JIL_API_SUPPORT

	RETURN_IF_ERROR(status);

	CryptoCertificateChain* certificate_chain = NULL;

	// Check the signing of <SignedInfo> element, create cert chain.
	status = CreateX509CertificateFromBinary(
		certificate_chain,
		ca_storage,
		reason,
		X509_certificates);
	RETURN_IF_ERROR(status);

	// signer_certificate_chain will be returned.
	signer_certificate_chain = certificate_chain;
#ifdef UTIL_ZIP_CACHE
	OpZip* zip_p = NULL;
	// the file should be already in the cache (constructed by call to IsZipFile 
	if(OpStatus::IsError(g_zipcache->GetData(zipped_widget_filename, &zip_p)) || !zip_p)
	{
		reason = WIDGET_ERROR_NOT_A_ZIPFILE;
		return OpStatus::ERR;
	}
	OP_ASSERT(zip_p);
	RETURN_IF_ERROR(CheckAllFilesSigned(*zip_p, xml_signature_basename.CStr(), reason, reference_objects));
#else // !UTIL_ZIP_CACHE
	/* Check that all files are signed. */
	RETURN_IF_ERROR(zip_file.Open(zipped_widget_filename));
	RETURN_IF_ERROR(CheckAllFilesSigned(zip_file, xml_signature_basename.CStr(), reason, reference_objects));
#endif // !UTIL_ZIP_CACHE

	// Check the signature.
	status = CheckHashSignature(
		signed_info_element_signature,
		signature_hash_type,
		certificate_chain,
		canonicalized_signed_info_hash.get(),
		reason);
	RETURN_IF_ERROR(status);

	/* Check that the file-digests are correct */ 
	status = CheckFileDigests(base_path.CStr(), reason, reference_objects);
	RETURN_IF_ERROR(status);

	reason = OK_CHECKED_LOCALLY;
	return OpStatus::OK;
}
#endif // GADGET_SUPPORT

/* static */ OP_STATUS
CryptoXmlSignature::VerifyXmlSignature(
	OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain,
	VerifyError& reason,
	const uni_char* signed_files_base_path,
	const uni_char* signature_xml_file,
	CryptoCertificateStorage* ca_storage,
	BOOL ignore_certificate,
	const unsigned char* public_key,
	unsigned long key_len)
{
	if (!signature_xml_file || !signed_files_base_path || !ca_storage)
		return OpStatus::ERR_OUT_OF_RANGE;

	OP_ASSERT(
			  (ignore_certificate == FALSE && public_key == NULL && key_len == 0)  || 
			  (ignore_certificate == TRUE && public_key != NULL && key_len != 0)
	          );
	reason = SIGNATURE_VERIFYING_GENERIC_ERROR;
	
	OpAutoVector<TempBuffer> X509_certificates;
	ByteBuffer signed_info_element_signature;
	ByteBuffer canonicalized_signed_info;
	OpString canonicalization_method;
	OpAutoStringHashTable<SignedReference> reference_objects(/* case_sensitive = */ TRUE);
	OpAutoPtr<CryptoHash> canonicalized_signed_info_hash;
	CryptoHashAlgorithm signature_hash_type;
	CryptoCipherAlgorithm signature_cipher_type;

	OpString signature_profile;
	OpString signature_role;
	OpString signature_identifier;

	OP_STATUS status = ParseSignaturXml(
		signature_xml_file,
		ignore_certificate,
		reason,
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
	RETURN_IF_ERROR(status);

	if (ignore_certificate == TRUE)
	{
		UINT8 *signed_info_element_signature_bytes = reinterpret_cast<byte*>(signed_info_element_signature.Copy(FALSE));
		ANCHOR_ARRAY(byte, signed_info_element_signature_bytes);

		UINT8 *canonicalized_signed_info_hash_bytes = OP_NEWA(byte, canonicalized_signed_info_hash->Size());
		ANCHOR_ARRAY(byte, canonicalized_signed_info_hash_bytes);

		/* Opera provides a built-in public key */ 
		CryptoSignature *signature;
		RETURN_IF_ERROR(CryptoSignature::Create(signature, CRYPTO_CIPHER_TYPE_RSA, CRYPTO_HASH_TYPE_SHA1));
		OpAutoPtr<CryptoSignature> signature_chain_deleter(signature);
		
		RETURN_IF_ERROR(signature->SetPublicKey(public_key, key_len));

		status = signature->VerifyASN1(
			canonicalized_signed_info_hash_bytes,
			canonicalized_signed_info_hash->Size(),
			signed_info_element_signature_bytes,
			signed_info_element_signature.Length());

		if (OpStatus::IsError(status))
		{
			RETURN_IF_MEMORY_ERROR(status);
			reason = SIGNATURE_VERIFYING_FAILED;
			return status;
		}
	}
	else
	{
		CryptoCertificateChain* certificate_chain = NULL;

		// Check the signing of <SignedInfo> element, create cert chain.
		status = CreateX509CertificateFromBinary(
			certificate_chain,
			ca_storage,
			reason,
			X509_certificates);
		RETURN_IF_ERROR(status);

		// signer_certificate_chain will be returned.
		signer_certificate_chain = certificate_chain;

		/* Check the signature. */
		status = CheckHashSignature(
			signed_info_element_signature,
			signature_hash_type,
			certificate_chain,
			canonicalized_signed_info_hash.get(),
			reason);
		RETURN_IF_ERROR(status);
	}
	
	// Check that the file-digests are correct.
	status = CheckFileDigests(signed_files_base_path, reason, reference_objects);
	RETURN_IF_ERROR(status);

	reason = OK_CHECKED_LOCALLY;
	return OpStatus::OK;
}

/* static */ OP_STATUS
CryptoXmlSignature::CreateX509CertificateFromBinary(
	CryptoCertificateChain*& signer_certificate_chain,
	const CryptoCertificateStorage* ca_storage,
	VerifyError& reason,
	OpAutoVector <TempBuffer>& X509Certificates
	)
{
	OP_ASSERT(ca_storage);

	reason = CERTIFICATE_PARSE_ERROR;
	CryptoCertificateChain *certchain;
	RETURN_IF_ERROR(CryptoCertificateChain::Create(certchain));
	OpAutoPtr<CryptoCertificateChain> chain_anchor(certchain);

	UINT32 count = X509Certificates.GetCount();
	for (UINT32 index = 0; index < count; index++)
	{
		TempBuffer *cert = X509Certificates.Get(index);

		OpString8 cert8;
		RETURN_IF_ERROR(cert8.SetUTF8FromUTF16(cert->GetStorage(), cert->Length()));
		// Create the chain
		RETURN_IF_ERROR(certchain->AddToChainBase64(cert8.CStr()));
	}

	// Check certificate signatures
	CryptoCertificateChain::VerifyStatus verify_status;
	RETURN_IF_ERROR(certchain->VerifyChainSignatures(verify_status, ca_storage));
	
	if (verify_status != CryptoCertificateChain::OK_CHECKED_LOCALLY)
	{
		switch (verify_status)
		{
		case CryptoCertificateChain::CERTIFICATE_GENERIC:
			reason = CERTIFICATE_PARSE_ERROR;
			break;

		case CryptoCertificateChain::CERTIFICATE_BAD_CERTIFICATE:
			reason = CERTIFICATE_ERROR_BAD_CERTIFICATE;
			break;

		case CryptoCertificateChain::CERTIFICATE_EXPIRED:
			reason = CERTIFICATE_EXPIRED;
			break;

		case CryptoCertificateChain::CERTIFICATE_INVALID_CERTIFICATE_CHAIN:
			reason = CERTIFICATE_INVALID_CERTIFICATE_CHAIN;
			break;

		case CryptoCertificateChain::CERTIFICATE_INVALID_SIGNATURE:
			reason = CERTIFICATE_ERROR_BAD_CERTIFICATE;
			break;

		default:
			reason = CERTIFICATE_PARSE_ERROR;
			break;
		}

		signer_certificate_chain = 0;
		return OpStatus::ERR;
	}

	signer_certificate_chain = chain_anchor.release();
	return OpStatus::OK;
}


#ifdef GADGET_SUPPORT
/* static */ OP_STATUS
CryptoXmlSignature::CheckAllFilesSigned(
	OpZip& zip_file,
	const uni_char* signature_filename,
	VerifyError& reason,
	OpStringHashTable<SignedReference>& reference_objects)
{
	reason = SIGNATURE_VERIFYING_GENERIC_ERROR;
	OpAutoVector<OpString> file_list;
	RETURN_IF_ERROR(zip_file.GetFileNameList(file_list));

	// Check that every file in the zip file exists and is signed in signature.xml.
	const UINT32 count = file_list.GetCount();
	for (UINT32 idx = 0; idx < count; idx++)
	{
		// Get pathname of a zip file entry (file or directory).
		OpString* pstrName = file_list.Get(idx);
		OP_ASSERT(pstrName);

		// Directories itself are not signed, so skip them.
		OpString& pathname = *pstrName;
		int len = pathname.Length();
		OP_ASSERT(len > 0);
		// Condition to check if it's not a directory taken from OpZipFolderLister::IsFolder().
		if (pathname[len - 1] == (uni_char)PATHSEPCHAR)
			continue;

		// Signature file itself is not signed, so skip it.
		if (pathname == signature_filename)
			continue;

		// Check if pathname is in the signed objects list.
		if ( !reference_objects.Contains(pathname.CStr()) )
		{
			reason = WIDGET_SIGNATURE_VERIFYING_FAILED_ALL_FILES_NOT_SIGNED;
			return OpStatus::ERR;
		}
	}

	return OpStatus::OK;
}
#endif // GADGET_SUPPORT

/* static */ OP_STATUS
CryptoXmlSignature::CheckFileDigests(
	const uni_char* base_path,
	VerifyError& reason,
	OpStringHashTable <SignedReference>& refrerence_objects)
{
	OP_ASSERT(base_path);

	reason = SIGNATURE_VERIFYING_GENERIC_ERROR;
	UINT8 *temp_buffer =  OP_NEWA(byte, 16384);
	RETURN_OOM_IF_NULL(temp_buffer);
	ANCHOR_ARRAY(byte, temp_buffer);

	OpString enforce_slash;
	RETURN_IF_ERROR(enforce_slash.Set(base_path));
	if (*base_path == '\0')
		return OpStatus::ERR;
		
	if (base_path[uni_strlen(base_path) - 1] != PATHSEPCHAR)
	{
		RETURN_IF_ERROR(enforce_slash.Append(PATHSEP));
	}

	OpString filename;
	
	OpHashIterator*	iterator = refrerence_objects.GetIterator();
	OpAutoPtr<OpHashIterator> iterator_deleter(iterator);
	
	OP_STATUS ret = iterator->First();
	while (ret == OpStatus::OK)
	{
		SignedReference *reference = static_cast<SignedReference*>(iterator->GetData());
		OP_ASSERT(reference);
		
		CryptoHash* file_hasher = CryptoHash::Create(reference->m_digest_method);
		RETURN_OOM_IF_NULL(file_hasher);
		OpAutoPtr <CryptoHash> file_hasher_deleter(file_hasher);

		RETURN_IF_ERROR(filename.Set(enforce_slash));
		RETURN_IF_ERROR(filename.Append(reference->m_file_path));

		OpFile file;
		RETURN_IF_ERROR(file.Construct(filename.CStr(), OPFILE_ABSOLUTE_FOLDER));
		
		BOOL found;
		RETURN_IF_ERROR(file.Exists(found));
		if (found == FALSE)
		{
			reason = SIGNATURE_VERIFYING_REFERENCE_FILE_NOT_FOUND;
			return OpStatus::ERR;
		}

		RETURN_IF_ERROR(file.Open(OPFILE_READ));	/* calculate and verify digests */
		OpFileLength bytes_read;

		file_hasher->InitHash();
		
		while (!file.Eof())
		{
			RETURN_IF_ERROR(file.Read(temp_buffer, 16384, &bytes_read));
			file_hasher->CalculateHash(temp_buffer, (UINT32) bytes_read);	
		}
		
		file_hasher->ExtractHash(temp_buffer);		
		
		unsigned digest_value_bytes_length;
		UINT8 *digest_value_bytes =  reinterpret_cast<byte*>(reference->m_digest_value.GetChunk(0, &digest_value_bytes_length));
		
		if (digest_value_bytes_length != static_cast<unsigned int>(file_hasher->Size()) || op_memcmp(digest_value_bytes, temp_buffer, digest_value_bytes_length) != 0)
		{
			reason = SIGNATURE_VERIFYING_FAILED_WRONG_DIGEST;
			return OpStatus::ERR;
		}

		ret = iterator->Next();
	}

	return OpStatus::OK;
}

/* static */ OP_STATUS
CryptoXmlSignature::CheckCurrentElementDigest(
	XMLFragment& signature_fragment,
	const uni_char* id,
	VerifyError& reason,
	OpStringHashTable <SignedReference>& local_refs)
{
	OP_ASSERT(id);
	SignedReference* ref = NULL;
	OP_STATUS status = local_refs.GetData(id, &ref);
	RETURN_IF_ERROR(status);
	OP_ASSERT(ref);
	OP_ASSERT(uni_str_eq(id, ref->m_file_path.CStr() + 1));

	XMLFragment::GetXMLOptions options(FALSE);
	options.canonicalize = GetCanonicalizeByCanonicalizationMethod(ref->m_transform_algorithm);
	options.encoding = "UTF-8";
	options.scope = XMLFragment::GetXMLOptions::SCOPE_CURRENT_ELEMENT_INCLUSIVE;

	ByteBuffer canonicalized_object;
	status = signature_fragment.GetEncodedXML(canonicalized_object, options);
	RETURN_IF_ERROR(status);

	CryptoHash* hasher = NULL;
	status = CalculateByteBufferHash(
		canonicalized_object, ref->m_digest_method, hasher);
	RETURN_IF_ERROR(status);
	OP_ASSERT(hasher);

	const unsigned int hash_size = hasher->Size();
	OP_ASSERT(hash_size < CRYPTO_MAX_HASH_SIZE);

	UINT8 hash_bytes[CRYPTO_MAX_HASH_SIZE]; /* ARRAY OK 2010-12-06 alexeik */
	hasher->ExtractHash(hash_bytes);

	OP_DELETE(hasher);
	hasher = NULL;

	unsigned int announced_hash_size = 0;
	const char*  announced_hash = ref->m_digest_value.GetChunk(0, &announced_hash_size);
	const UINT8* announced_hash_bytes = reinterpret_cast <const UINT8*> (announced_hash);

	// Check hash.
	if (hash_size != announced_hash_size ||
	    op_memcmp(hash_bytes, announced_hash_bytes, hash_size))
	{
		reason = SIGNATURE_VERIFYING_FAILED_WRONG_DIGEST;
		return OpStatus::ERR;
	}

	// The local reference digest is verified successfully - remove it from the list.
	status = local_refs.Remove(id, &ref);
	RETURN_IF_ERROR(status);
	OP_DELETE(ref);

	return OpStatus::OK;
}

/* static */ OP_STATUS
CryptoXmlSignature::CheckHashSignature(
	const ByteBuffer&       signed_info_element_signature,
	CryptoHashAlgorithm     signature_hash_type,
	CryptoCertificateChain* certificate_chain,
	CryptoHash*             canonicalized_signed_info_hash,
	VerifyError&            reason)
{
	UINT8 *signed_info_element_signature_bytes = reinterpret_cast<byte*>(signed_info_element_signature.Copy(FALSE));
	ANCHOR_ARRAY(byte, signed_info_element_signature_bytes);

	CryptoSignature *signature = NULL;
	if (signature_hash_type == CRYPTO_HASH_TYPE_SHA1 ||
	    signature_hash_type == CRYPTO_HASH_TYPE_SHA256)
	{
		RETURN_IF_ERROR(CryptoSignature::Create(signature, CRYPTO_CIPHER_TYPE_RSA, signature_hash_type));
	}
	if (signature == NULL)
		return OpStatus::ERR;

	OpAutoPtr<CryptoSignature> signature_chain_deleter(signature);

	int key_len;
	const UINT8 *key = certificate_chain->GetCertificate(0)->GetPublicKey(key_len);
	RETURN_IF_ERROR(signature->SetPublicKey(key, key_len));

	UINT8 *canonicalized_signed_info_hash_bytes = OP_NEWA(byte, canonicalized_signed_info_hash->Size());
	ANCHOR_ARRAY(byte, canonicalized_signed_info_hash_bytes);

	canonicalized_signed_info_hash->ExtractHash(canonicalized_signed_info_hash_bytes);

	OP_STATUS status = signature->VerifyASN1(
		canonicalized_signed_info_hash_bytes,
		canonicalized_signed_info_hash->Size(),
		signed_info_element_signature_bytes,
		signed_info_element_signature.Length());

	if (OpStatus::IsError(status))
	{
		RETURN_IF_MEMORY_ERROR(status);
		reason = SIGNATURE_VERIFYING_FAILED;
	}

	return status;
}

/* static */ OP_STATUS 
CryptoXmlSignature::ParseSignaturXml(
	const uni_char *xml_signature_file,
	BOOL ignore_certificate,
	VerifyError &reason,
	OpStringHashTable<SignedReference> &reference_objects,
	OpAutoVector<TempBuffer> &X509Certificates,
	ByteBuffer &signature,
	CryptoCipherAlgorithm &signature_cipher_type,
	CryptoHashAlgorithm &signature_hash_type,
	ByteBuffer &canonicalized_signed_info,
	OpAutoPtr<CryptoHash> &canonicalized_signed_info_hash,
	OpString &canonicalization_method,
	OpString &signature_profile,
	OpString &signature_role,
	OpString &signature_identifier
)
{
	OP_ASSERT(xml_signature_file);

	OpString signature_method;
	OP_STATUS status;	

	// Setting the current potential failure reason.
	reason = SIGNATURE_FILE_MISSING;

	OpFile file;
	RETURN_IF_ERROR(file.Construct(xml_signature_file, OPFILE_ABSOLUTE_FOLDER));
	
	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (found == FALSE)
		return OpStatus::ERR;

	// We now know that the signature file is not missing,
	// thus we are changing the default failure reason.
	reason = SIGNATURE_FILE_XML_GENERIC_ERROR;

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	// Clear output containers.
	X509Certificates.DeleteAll();
	signature.FreeStorage();
	signature_hash_type = CRYPTO_HASH_TYPE_UNKNOWN;
	canonicalized_signed_info.FreeStorage();
	reference_objects.DeleteAll();
	canonicalized_signed_info_hash.reset(NULL);
	canonicalization_method.Empty();
	signature_profile.Empty();
	signature_role.Empty();
	signature_identifier.Empty();

	OpAutoStringHashTable <SignedReference> local_refs;
	OP_BOOLEAN found_element = OpBoolean::IS_FALSE;

	XMLFragment signature_fragment;
	signature_fragment.SetDefaultWhitespaceHandling(XMLWHITESPACEHANDLING_PRESERVE);
	
	if (OpStatus::IsSuccess(status = signature_fragment.Parse(static_cast<OpFileDescriptor*>(&file))))
	{
		const uni_char *xmlns = NULL;
		if (signature_fragment.EnterElement(XMLExpandedName(UNI_L("http://www.w3.org/2000/09/xmldsig#"), UNI_L("Signature"))))
			xmlns = UNI_L("http://www.w3.org/2000/09/xmldsig#");
		else if (signature_fragment.EnterElement(XMLExpandedName(UNI_L("bytemobile"), UNI_L("Signature")))) /* FIXME: put in proper namespace for bytemobile */
			xmlns = UNI_L("bytemobile");	
		else
		{
			reason = SIGNATURE_FILE_XML_PARSE_ERROR;
			return OpStatus::ERR;
		}
		
		while (signature_fragment.HasMoreElements())
		{
			/* traverse the Signature element */
			if (!canonicalized_signed_info.Length() && signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("SignedInfo"))))
			{
				while (signature_fragment.HasMoreElements())
				{
					/* traverse the SignedInfo element */
					if (!signature_method.CStr() && signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("SignatureMethod"))))
					{
						signature_method.Set(signature_fragment.GetAttribute(UNI_L("Algorithm")));
						if (!uni_stri_eq(signature_method.CStr(), "http://www.w3.org/2000/09/xmldsig#rsa-sha1")
                            && !uni_stri_eq(signature_method.CStr(), "http://www.w3.org/2001/04/xmldsig-more#rsa-sha256")
                           )
						{
							reason = SIGNATURE_FILE_XML_WRONG_ALGORITHM;
							return OpStatus::ERR_NOT_SUPPORTED;
						}
						// End of <SignatureMethod> processing.
					}

					else if (!canonicalization_method.CStr() && signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("CanonicalizationMethod"))))
					{
						canonicalization_method.Set(signature_fragment.GetAttribute(UNI_L("Algorithm")));
						// End of <CanonicalizationMethod> processing.
					}
					else if (signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("Reference"))))
					{
						OpString file_path;
						RETURN_IF_ERROR(file_path.Set(signature_fragment.GetAttribute(UNI_L("URI"))));

						OpString transform_algorithm;
						CryptoHashAlgorithm digest_method = CRYPTO_HASH_TYPE_UNKNOWN;
						ByteBuffer digest_value;

						while (signature_fragment.HasMoreElements())
						{
							/* Parse a signature. build a vector of the files */
							if (!digest_value.Length() && (found_element = signature_fragment.GetBinaryData(XMLExpandedName(xmlns, UNI_L("DigestValue")), digest_value)) != OpBoolean::IS_FALSE)
							{
								RETURN_IF_ERROR(found_element);
								// End of <DigestValue> processing.
							}
							else if (signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("Transforms"))))
							{
								if (transform_algorithm.IsEmpty() &&
								    signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("Transform"))))
								{
									RETURN_IF_ERROR(transform_algorithm.Set(signature_fragment.GetAttribute(UNI_L("Algorithm"))));
									signature_fragment.LeaveElement();
								}
								signature_fragment.LeaveElement();
								// End of <Transforms> processing.
							}
							else if (digest_method == CRYPTO_HASH_TYPE_UNKNOWN &&
							         signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("DigestMethod"))))
							{
								const uni_char* algorithm = signature_fragment.GetAttribute(UNI_L("Algorithm"));
								if (!algorithm)
								{
									reason = SIGNATURE_FILE_XML_MISSING_ARGS;
									return OpStatus::ERR;
								}

								digest_method = GetHashTypeByDigestMethod(algorithm);
								if (digest_method == CRYPTO_HASH_TYPE_UNKNOWN)
								{
									reason = SIGNATURE_FILE_XML_WRONG_ALGORITHM;
									return OpStatus::ERR_NOT_SUPPORTED;
								}

								signature_fragment.LeaveElement();
								// End of <DigestMethod> processing.
							}							
							else
							{
								signature_fragment.EnterAnyElement();
								signature_fragment.LeaveElement();
							}
						}


						if (file_path.HasContent() &&
						    digest_method != CRYPTO_HASH_TYPE_UNKNOWN &&
						    digest_value.Length())
                        {
							if (file_path[0] != (uni_char)'#' && transform_algorithm.HasContent())
							{
								// From widget-digsig spec:
								// A ds:Reference that is not to same-document
								// XML content MUST NOT have any ds:Transform
								// elements.
								reason = SIGNATURE_FILE_XML_GENERIC_ERROR;
								return OpStatus::ERR;
							}

							SignedReference* reference_object = NULL;
							status = SignedReference::Make(
								reference_object,
								file_path.CStr(),
								transform_algorithm,
								digest_method,
								digest_value);
							RETURN_IF_ERROR(status);
							OP_ASSERT(reference_object);

							if (file_path[0] == (uni_char)'#')
							{
								// file_path is not a pathname, it's a reference to an XML element.
								// Let's add to local references.
								status = local_refs.Add(
										reference_object->m_file_path.CStr() + 1,
										reference_object);
							}
							else
							{
								// file_path is a pathname.
								// Let's add to reference objects.
								status = reference_objects.Add(
										reference_object->m_file_path.CStr(),
										reference_object);
							}

							if (OpStatus::IsError(status))
							{
								OP_DELETE(reference_object);
								return status;
							}
						}
						else
						{
							reason = SIGNATURE_FILE_XML_MISSING_ARGS;
							return OpStatus::ERR;
						}

						// End of <Reference> processing.
					}
					else
						signature_fragment.EnterAnyElement();

					signature_fragment.LeaveElement();
				}

				// Canonicalize <SignedInfo>.
				XMLFragment::GetXMLOptions options(FALSE);
				options.canonicalize = GetCanonicalizeByCanonicalizationMethod(canonicalization_method.CStr());
				options.encoding = "UTF-8";
				options.scope = XMLFragment::GetXMLOptions::SCOPE_CURRENT_ELEMENT_INCLUSIVE;
				RETURN_IF_ERROR(signature_fragment.GetEncodedXML(canonicalized_signed_info, options));

				signature_fragment.LeaveElement();
				// End of <SignedInfo> processing.
			}
			else if (signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("KeyInfo"))))
			{
				while (signature_fragment.HasMoreElements())
				{
					if (!ignore_certificate && signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("X509Data"))))
					{
						while (signature_fragment.HasMoreElements())
						{
							if (signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("X509Certificate"))))
							{
								OpAutoPtr<TempBuffer> X509Certificate(OP_NEW(TempBuffer, ()));
							
								if (!X509Certificate.get())
									return OpStatus::ERR_NO_MEMORY;
								
								RETURN_IF_ERROR(signature_fragment.GetAllText(*(X509Certificate.get())));
								RETURN_IF_ERROR(X509Certificates.Add(X509Certificate.get()));
								X509Certificate.release(); // If we come here, the certificate is safely stored in X509Certificates
								
								signature_fragment.LeaveElement();

								// End of <X509Certificate> processing.
							}
							else
							{
								signature_fragment.EnterAnyElement();
								signature_fragment.LeaveElement();
							}
						}
						signature_fragment.LeaveElement();

						// End of <X509Data> processing.
					}
					else
					{
						signature_fragment.EnterAnyElement();
						signature_fragment.LeaveElement();
					}
				}				
				signature_fragment.LeaveElement();

				// End of <KeyInfo> processing.
			}
			else if (!signature.Length() && (found_element = signature_fragment.GetBinaryData(XMLExpandedName(xmlns, UNI_L("SignatureValue")), signature)) != OpBoolean::IS_FALSE)
			{
				RETURN_IF_ERROR(found_element);
				// End of <SignatureValue> processing.
			}
			else if (signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("Object"))))
			{
				const uni_char* id = signature_fragment.GetAttribute(UNI_L("Id"));
				if (id && local_refs.Contains(id))
				{
					status = CheckCurrentElementDigest(signature_fragment, id, reason, local_refs);
					RETURN_IF_ERROR(status);

					BOOL seen_signature_properties = FALSE;

					while (signature_fragment.HasMoreElements())
					{
						if (signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("SignatureProperties"))))
						{
							if (seen_signature_properties)
							{
								// From widget-digsig spec:
								// The implementation MUST ensure that this
								// ds:Object element contains a single
								// ds:SignatureProperties element that contains
								// a different ds:SignatureProperty element
								// for each property required by this specification.
								reason = SIGNATURE_VERIFYING_WRONG_PROPERTIES;
								return OpStatus::ERR;
							}
							else
								seen_signature_properties = TRUE;

							while (signature_fragment.HasMoreElements())
							{
								if (signature_fragment.EnterElement(XMLExpandedName(xmlns, UNI_L("SignatureProperty"))))
								{
									while (signature_fragment.HasMoreElements())
									{
										const uni_char* xmlns_dsp = UNI_L("http://www.w3.org/2009/xmldsig-properties");

										if (signature_profile.IsEmpty() &&
										    signature_fragment.EnterElement(XMLExpandedName(xmlns_dsp, UNI_L("Profile"))))
										{
											RETURN_IF_ERROR(signature_profile.Set(signature_fragment.GetAttribute(UNI_L("URI"))));
											signature_fragment.LeaveElement();
										}
										else if (signature_role.IsEmpty() &&
										         signature_fragment.EnterElement(XMLExpandedName(xmlns_dsp, UNI_L("Role"))))
										{
											RETURN_IF_ERROR(signature_role.Set(signature_fragment.GetAttribute(UNI_L("URI"))));
											signature_fragment.LeaveElement();
										}
										else if (signature_fragment.EnterElement(XMLExpandedName(xmlns_dsp, UNI_L("Identifier"))))
										{
											if (signature_identifier.HasContent())
											{
												// From xmldsig-properties spec:
												// If multiple instances of this [Identifier] property are found on a single signature,
												// then applications must not deem any of these properties valid.
												reason = SIGNATURE_VERIFYING_WRONG_PROPERTIES;
												return OpStatus::ERR;
											}

											RETURN_IF_ERROR(signature_identifier.Set(signature_fragment.GetText()));
											signature_fragment.LeaveElement();
										}
										else
										{
											signature_fragment.EnterAnyElement();
											signature_fragment.LeaveElement();
										}
									}
									signature_fragment.LeaveElement();

									// End of <SignatureProperty> processing.
								}
								else
								{
									signature_fragment.EnterAnyElement();
									signature_fragment.LeaveElement();
								}
							}

							// All 3 signature properties must be contained in 1 SignatureProperties element.
							if (signature_profile.IsEmpty() ||
							    signature_role.IsEmpty()    ||
							    signature_identifier.IsEmpty())
							{
								reason = SIGNATURE_FILE_XML_MISSING_ARGS;
								return OpStatus::ERR;
							}

							signature_fragment.LeaveElement();

							// End of <SignatureProperties> processing.
						}
						else
						{
							signature_fragment.EnterAnyElement();
							signature_fragment.LeaveElement();
						}
					}

					// End of if (id && local_refs.Contains(id)).
				}
				signature_fragment.LeaveElement();

				// End of <Object> processing.
			}
			else
			{
				signature_fragment.EnterAnyElement();
				signature_fragment.LeaveElement();
			}
		}

		// End of parsing.
	}
	else
	{
		reason = SIGNATURE_FILE_XML_PARSE_ERROR;
		return OpStatus::ERR;
	}
	
	if (!canonicalized_signed_info.Length() ||
		!signature_method.CStr() ||
		!signature.Length() ||
		(!ignore_certificate && !X509Certificates.GetCount()) ||
		!reference_objects.GetCount() ||
		// All local references must be found and checked successfully.
		local_refs.GetCount() ||
		// 3 signature properties must be present.
		signature_profile.IsEmpty() ||
		signature_role.IsEmpty() ||
		signature_identifier.IsEmpty()
	)
	{
		reason = SIGNATURE_FILE_XML_MISSING_ARGS;
		return OpStatus::ERR;
	}

	// FIXME: get cipher type from xml file.
	signature_cipher_type = CRYPTO_CIPHER_TYPE_RSA;

	// Hash SignedInfo element.
	CryptoHash* hasher = NULL;
	signature_hash_type = GetHashTypeBySignatureMethod(signature_method.CStr());
	status = CalculateByteBufferHash(canonicalized_signed_info, signature_hash_type, hasher);
	RETURN_IF_ERROR(status);

	OP_ASSERT(hasher);
	canonicalized_signed_info_hash.reset(hasher);

	return OpStatus::OK;
}


/* static */ XMLFragment::GetXMLOptions::Canonicalize
CryptoXmlSignature::GetCanonicalizeByCanonicalizationMethod(const uni_char* method)
{
	// Fallback for unsupported or unknown method.
	const Canonicalize fallback = XMLFragment::GetXMLOptions::CANONICALIZE_WITHOUT_COMMENTS;

	if (!method)
		// From widget-digsig spec:
		// An implementation SHOULD be able to process a ds:Reference
		// to same-document XML content when that ds:Reference
		// does not have a ds:Transform child element, for backward
		// compatibility. In this case the default canonicalization
		// algorithm Canonical XML 1.0 will be used, as specified
		// in XML Signature 1.1.
		return fallback;
	// Canonical 1.0 (non-exclusive).
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/TR/2001/REC-xml-c14n-20010315")))
		return XMLFragment::GetXMLOptions::CANONICALIZE_WITHOUT_COMMENTS;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/TR/2001/REC-xml-c14n-20010315#WithComments")))
		return XMLFragment::GetXMLOptions::CANONICALIZE_WITH_COMMENTS;
	// Exclusive 1.0.
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2001/10/xml-exc-c14n#")))
		return XMLFragment::GetXMLOptions::CANONICALIZE_WITHOUT_COMMENTS_EXCLUSIVE;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2001/10/xml-exc-c14n#WithComments")))
		return XMLFragment::GetXMLOptions::CANONICALIZE_WITH_COMMENTS_EXCLUSIVE;
	// Canonical 1.1 is not supported so far, will be handled by fallback for now.
	//else if (uni_str_eq(method, UNI_L("http://www.w3.org/2006/12/xml-c14n11")))
	//	return XMLFragment::GetXMLOptions::CANONICALIZE_1_1_WITHOUT_COMMENTS;
	//else if (uni_str_eq(method, UNI_L("http://www.w3.org/2006/12/xml-c14n11#WithComments")))
	//	return XMLFragment::GetXMLOptions::CANONICALIZE_1_1_WITH_COMMENTS;
	else
		return fallback;
}

/* static */ CryptoHashAlgorithm
CryptoXmlSignature::GetHashTypeByDigestMethod(const uni_char* method)
{
	if (!method)
		return CRYPTO_HASH_TYPE_UNKNOWN;
	// MD5 is not supported because of low security.
	//else if (uni_str_eq(method, UNI_L("http://www.w3.org/2001/04/xmldsig-more#md5")))
	//	return CRYPTO_HASH_TYPE_MD5;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2000/09/xmldsig#sha1")))
		return CRYPTO_HASH_TYPE_SHA1;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2001/04/xmlenc#sha256")))
		return CRYPTO_HASH_TYPE_SHA256;
	else
		return CRYPTO_HASH_TYPE_UNKNOWN;
}

/* static */ CryptoHashAlgorithm
CryptoXmlSignature::GetHashTypeBySignatureMethod(const uni_char* method)
{
	if (!method)
		return CRYPTO_HASH_TYPE_UNKNOWN;
	// MD5 is not supported because of low security.
	//else if (uni_str_eq(method, UNI_L("http://www.w3.org/2001/04/xmldsig-more#rsa-md5")))
	//	return CRYPTO_HASH_TYPE_MD5;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2000/09/xmldsig#rsa-sha1")))
		return CRYPTO_HASH_TYPE_SHA1;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2001/04/xmldsig-more#rsa-sha256")))
		return CRYPTO_HASH_TYPE_SHA256;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2000/09/xmldsig#dsa-sha1")))
		return CRYPTO_HASH_TYPE_SHA1;
	else if (uni_str_eq(method, UNI_L("http://www.w3.org/2001/04/xmldsig-more#ecdsa-sha256")))
		return CRYPTO_HASH_TYPE_SHA256;
	else
		return CRYPTO_HASH_TYPE_UNKNOWN;
}

/* static */ OP_STATUS
CryptoXmlSignature::CalculateByteBufferHash(
	const ByteBuffer& buffer,
	CryptoHashAlgorithm hash_type,
	CryptoHash*& hasher)
{
	if (hash_type == CRYPTO_HASH_TYPE_UNKNOWN)
		return OpStatus::ERR_NOT_SUPPORTED;

	hasher = CryptoHash::Create(hash_type);
	RETURN_OOM_IF_NULL(hasher);

	hasher->InitHash();

	const unsigned int chunk_count = buffer.GetChunkCount();
	for (unsigned int chunk_idx = 0; chunk_idx < chunk_count; chunk_idx++)
	{
		unsigned int nbytes;
		const char*  chunk = buffer.GetChunk(chunk_idx, &nbytes);
		const UINT8* chunk_bytes = reinterpret_cast <const UINT8*> (chunk);
		hasher->CalculateHash(chunk_bytes, nbytes);
	}

	return OpStatus::OK;
}

#endif // CRYPTO_XML_DIGITAL_SIGNING_SUPPORT
