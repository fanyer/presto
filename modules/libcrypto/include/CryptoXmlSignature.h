#ifndef CRYPTO_XMLSIGNATURE_H_
#define CRYPTO_XMLSIGNATURE_H_

#if defined(CRYPTO_API_SUPPORT) && defined(CRYPTO_XML_DIGITAL_SIGNING_SUPPORT) && defined(XMLUTILS_CANONICAL_XML_SUPPORT)

#include "modules/libcrypto/include/CryptoCertificate.h"
#include "modules/libcrypto/include/CryptoCertificateChain.h"
#include "modules/libcrypto/include/CryptoCertificateStorage.h"
#include "modules/libcrypto/include/CryptoHash.h"
#include "modules/libcrypto/include/CryptoSignature.h"
#include "modules/util/adt/bytebuffer.h"
#include "modules/util/zipload.h"
#include "modules/util/opautoptr.h"
#include "modules/xmlutils/xmlfragment.h"


/*	Api for verifying xml signatures. Read the tutorial at modules/libssl/documentation/xmlsignature.html 
	 
	To use this API, put the following into your module's module.import file:

	API_XML_DIGITAL_SIGNING name

	Activates support for verifying xml signatures.
*/

// Turn on this api with API_CRYPTO_XML_DIGITAL_SIGNING_SUPPORT
class CryptoXmlSignature
{
public:	
	enum VerifyError 
	{
		/** Verification succeeded locally.
		 *
		 *  Verification of the signature succeeded. The signing
		 *  certificate was verified successfully using the local
		 *  CA storage.
		 *
		 *  OCSP check, however, could not be performed.
		 *  Some possible reasons are:
		 *  - certificate didn't provide a URL to the OCSP server
		 *  - the OCSP server couldn't be contacted
		 *  - the OCSP server didn't give a valid response
		 *  - more than one problem of such kind.
		 */
		OK_CHECKED_LOCALLY,

		/** Verification succeeded with OCSP.
		 *
		 *  Verification of the signature succeeded. The signing
		 *  certificate was verified successfully using both
		 *  the local CA storage and the issuer's OCSP server.
		 */
		OK_CHECKED_WITH_OCSP,

		CERTIFICATE_EXPIRED,
		CERTIFICATE_REVOKED,
		CERTIFICATE_INVALID_CERTIFICATE_CHAIN,
		CERTIFICATE_INVALID_CERTIFICATE_ISSUER,
		CERTIFICATE_PARSE_ERROR,
		// Certificate is bad for another reason.
		CERTIFICATE_ERROR_BAD_CERTIFICATE,

		WIDGET_ERROR_NOT_A_ZIPFILE,
		WIDGET_ERROR_ZIP_FILE_NOT_FOUND,
		WIDGET_SIGNATURE_VERIFYING_FAILED_ALL_FILES_NOT_SIGNED,		
		
		SIGNATURE_FILE_MISSING,
		SIGNATURE_FILE_XML_GENERIC_ERROR,
		SIGNATURE_FILE_XML_PARSE_ERROR,
		SIGNATURE_FILE_XML_WRONG_ALGORITHM,
		SIGNATURE_FILE_XML_MISSING_ARGS,
		
		SIGNATURE_VERIFYING_FAILED,
		SIGNATURE_VERIFYING_WRONG_PROPERTIES,
		SIGNATURE_VERIFYING_REFERENCE_FILE_NOT_FOUND,
		SIGNATURE_VERIFYING_FAILED_WRONG_DIGEST,
		SIGNATURE_VERIFYING_GENERIC_ERROR
	};

#if defined(_DEBUG) || defined(SELFTEST)
	static const char* VerifyErrorName(VerifyError error);
#endif

#ifdef GADGET_SUPPORT
	/** Checks the signature of a widget.

		The widget must be in zipped form, and contain the signature file "signature.xml".

		To sign a widget, read the tutorial at
		modules/libssl/documentation/xmlsignature.html.

		@param signer_certificate (out) The certificate used to signed the widget.
		                                Check for NULL, if not success. Is owned and
		                                must be deleted by caller.
		@param reason (out) If the function returns OpBoolean::IS_FALSE or
		                    OpBoolean::ERR, the reason for failing is given here.
		@param zipped_widget_file File path to a zipped widget.
		@param ca_storage A storage of CA certificates for which the widget certificate
		                  is checked against.

		@return OpStatus::OK if success,
		        OpStatus::ERR if verify error, missing files or malformed signature xml file, or
		        OpStatus::ERR_NO_MEMORY on OOM.
	*/
	static OP_STATUS VerifyWidgetSignature(
		OpAutoPtr<CryptoCertificate>& signer_certificate,
		VerifyError& reason,
		const uni_char* zipped_widget_file,
		const CryptoCertificateStorage* ca_storage);

	/** Checks the signature of a widget.

		Same as the other VerifyWidgetSignature, but returns
		the whole certificate chain, not only the signer certificate.
	*/
	static OP_STATUS VerifyWidgetSignature(
		OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain,
		VerifyError& reason,
		const uni_char* zipped_widget_filename,
		const CryptoCertificateStorage* ca_storage);
#endif // GADGET_SUPPORT

	/** Checks an xml signature. 

		Currently we only support a subset of the xml signature
		http://www.w3.org/TR/xmldsig-core/.

		Most important limitations:
		We only support file paths in references.
		Object tag is not supported (all signed text must be defined in 
		files outside the signature xml file). 

		To sign files, read the tutorial at 
		modules/libcrypto/documentation/xmlsignature.html.  

		@param signer_certificate_chain (out) The certificate chain used to signed the widget.
		                                Check for NULL, if not success. Is owned
		                                and must be deleted by caller.
		                                Will be NULL if ignore_certificate == TRUE.
		@param reason (out) If the function returns OpBoolean::IS_FALSE or
		                    OpBoolean::ERR, the reason for failing is given here.
		@param signed_files_base_path The common base path for all the signed files,
		                              referenced by signature_xml_file
		@param signature_xml_file File path to a signature xml file	
		@param ca_storage A storage of CA certificates for which the widget certificate
		                  is checked against. Will be ignored and can be NULL if 
		                  ignore_certificate == TRUE
		@param ignore_certificate Gives an option to check the signature against a
								public_key stored in the opera binary. The certificate
								in the xml file given by "signature_xml_file"
								will be ignored. If FALSE, the public key given in
								the certificate will be used.

		@param public_key The public key. Must be NULL if ignore_certificate == FALSE.
		@param key_len    The key length. Must be 0 if ignore_certificate == FALSE

		@return OpStatus::OK if success,
		        OpStatus::ERR if verify error, missing files or malformed xml file, or
		        OpStatus::ERR_NO_MEMORY on OOM.
	*/
	static OP_STATUS VerifyXmlSignature(
		OpAutoPtr <CryptoCertificateChain>& signer_certificate_chain,
		VerifyError& reason,
		const uni_char* signed_files_base_path,
		const uni_char* signature_xml_file,
		CryptoCertificateStorage* ca_storage,
		BOOL ignore_certificate = FALSE,
		const unsigned char* public_key = 0,
		unsigned long key_len = 0);

private:	
	class SignedReference
	{
	public:
		static OP_STATUS Make(
			SignedReference*& reference,
			const uni_char* file,
			const OpStringC& transform_algorithm,
			CryptoHashAlgorithm digest_method,
			const ByteBuffer& digest_value);

	public:
		OpString m_file_path;
		OpString m_transform_algorithm;
		CryptoHashAlgorithm m_digest_method;
		ByteBuffer m_digest_value;
	};

	static OP_STATUS CreateX509CertificateFromBinary(
		CryptoCertificateChain*& signer_certificate_chain,
		const CryptoCertificateStorage* ca_storage,
		VerifyError& reason,
		OpAutoVector <TempBuffer>& X509Certificate
		);

#ifdef GADGET_SUPPORT	
	static OP_STATUS CheckAllFilesSigned(
		OpZip& zip_file,
		const uni_char *signature_filename,
		VerifyError& reason,
		OpStringHashTable <SignedReference>& reference_objects);
#endif

	static OP_STATUS CheckFileDigests(
		const uni_char* base_path,
		VerifyError& reason,
		OpStringHashTable <SignedReference>& refrerence_objects);

	static OP_STATUS CheckCurrentElementDigest(
		XMLFragment& signature_fragment,
		const uni_char* id,
		VerifyError& reason,
		OpStringHashTable <SignedReference>& local_refs);

	static OP_STATUS CheckHashSignature(
		const ByteBuffer&       signed_info_element_signature,
		CryptoHashAlgorithm     signature_hash_type,
		CryptoCertificateChain* certificate_chain,
		CryptoHash*             canonicalized_signed_info_hash,
		VerifyError&            reason);

	static OP_STATUS ParseSignaturXml(const uni_char *xml_signature_file,
	                                  BOOL ignore_certificate,
	                                  /* out parameters */
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
	                                  OpString &signature_identifier);

	typedef XMLFragment::GetXMLOptions::Canonicalize Canonicalize;
	static Canonicalize GetCanonicalizeByCanonicalizationMethod(const uni_char* method);

	static CryptoHashAlgorithm GetHashTypeByDigestMethod(const uni_char* method);

	static CryptoHashAlgorithm GetHashTypeBySignatureMethod(const uni_char* method);

	static OP_STATUS CalculateByteBufferHash(
		const ByteBuffer& buffer,
		CryptoHashAlgorithm hash_type,
		CryptoHash*& hasher);

	CryptoXmlSignature(){};
	virtual ~CryptoXmlSignature(){};

	friend class GadgetSignatureVerifier;
};

#endif // _SSL_SUPPORT_ && CRYPTO_XML_DIGITAL_SIGNING_SUPPORT
#endif // CRYPTO_XMLSIGNATURE_H_
