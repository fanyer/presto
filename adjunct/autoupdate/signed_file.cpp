#include "core/pch.h"

#if defined _NEED_LIBSSL_VERIFY_SIGNED_FILE_
#include "modules/libssl/sslv3.h"
#include "modules/libssl/sslopt.h"
#include "modules/url/url2.h"

#include "adjunct/desktop_util/string/stringutils.h"

unsigned long GeneralDecodeBase64(const unsigned char *source, unsigned long len, unsigned long &read_pos, unsigned char *target, BOOL &warning, unsigned long max_write_len=0, unsigned char *decode_buf=NULL, unsigned int *decode_len=NULL);

/**	Verify signed file
 *	The function will return FALSE if signature fails or if any errors occur,
 *
 *	@param	signed_file		URL containing the file to be verified. MUST be loaded, 
 *							which can be accomplished with signed_file.QuickLoad(TRUE)
 *	@param	signature		Base64 encoded signature
 *
 *	@param	key				Pointer to buffer containing the DER encoded public key associated 
 *							with the private key used to generate the signature, MUST be an 
 *							X509_PUBKEY structure (openssl rsa -pubout ... command result)
 *	@param	key_len			Length of the public key buffer
 *
 *	@param  alg				Algorithm used to calculate signature. Default SSL_SHA
 *
 *	@return TRUE if the verification succeded, FALSE if there was any error.
 */
BOOL VerifySignedFile(URL &signed_file, const OpStringC8 &signature, const unsigned char *key, unsigned long key_len, SSL_HashAlgorithmType alg)
{
	
	if(signed_file.IsEmpty() || (URLStatus) signed_file.GetAttribute(URL::KLoadStatus) != URL_LOADED || key == NULL || key_len == 0)
		return FALSE;
	
	// Get The raw data
	OpAutoPtr<URL_DataDescriptor> desc(signed_file.GetDescriptor(NULL, TRUE, TRUE, TRUE));
	if(!desc.get())
		return FALSE;
	
	BOOL more = FALSE;
	unsigned long buf_len;
	
	if(desc->RetrieveData(more) == 0 || desc->GetBuffer() == NULL)
		return FALSE;
		
	if(desc->GetBufSize() == 0)
		return FALSE;
	
	if(signature.Length() <= 0)
		return FALSE;
	
	unsigned long signature_len = signature.Length();
	
	SSL_varvector32 signature_in;
	
	signature_in.Resize(signature_len);
	if(signature_in.Error())
		return FALSE;
	
	unsigned long read_len=0; 
	BOOL warning= FALSE;
	buf_len = GeneralDecodeBase64((unsigned char *)signature.CStr(), signature_len, read_len, signature_in.GetDirect(), warning);
	
	if(warning || read_len != signature_len || buf_len == 0) 
		return FALSE;
	
	signature_in.Resize(buf_len);
	
	SSL_Hash_Pointer digester(alg);
	if(digester.Error())
		return FALSE;
	
	digester->InitHash();
	
	do{
		more = FALSE;
		buf_len = desc->RetrieveData(more);
		
		digester->CalculateHash((unsigned char *)desc->GetBuffer(), buf_len);
		
		desc->ConsumeData(buf_len);
	}while(more);
	
	SSL_varvector32 signature_out;
	
	digester->ExtractHash(signature_out);
	
	if(digester->Error() || signature_out.Error())
		return FALSE;
	
	OpAutoPtr<SSL_PublicKeyCipher> signature_checker;
	
	OP_STATUS op_err = OpStatus::OK;
	signature_checker.reset(g_ssl_api->CreatePublicKeyCipher(SSL_RSA, op_err));
	
	if(OpStatus::IsError(op_err) || signature_checker.get() == NULL)
		return FALSE;
	
	SSL_varvector32 pubkey_bin_ex;
	
	pubkey_bin_ex.SetExternal((unsigned char *) key);
	pubkey_bin_ex.Resize(key_len);
	
	signature_checker->LoadAllKeys(pubkey_bin_ex);
	
	if(signature_checker->Error())
		return FALSE;
	
	if(alg == SSL_SHA)
	{
		if(!signature_checker->Verify(signature_out.GetDirect(), signature_out.GetLength(), signature_in.GetDirect(), signature_in.GetLength()))
			return FALSE;
	}
#ifdef USE_SSL_ASN1_SIGNING
	else
	{
		if(!signature_checker->VerifyASN1(digester, signature_in.GetDirect(), signature_in.GetLength()))
			return FALSE;
	}
#endif
	
	if(signature_checker->Error())
		return FALSE;
	
	return TRUE;
}

/**	Verify checksum
 *	The function will return FALSE if verification fails or if any errors occur,
 *
 *	@param	signed_file		URL containing the file to be verified. MUST be loaded, 
 *							which can be accomplished with signed_file.QuickLoad(TRUE)
 *	@param	checksum		Base64 encoded checksum
 *
 *	@param  alg				Algorithm used to calculate checksum. Default SSL_SHA
 *
 *	@return TRUE if the verification succeded, FALSE if there was any error. 
 */
BOOL VerifyChecksum(URL &signed_file, const OpStringC8 &checksum, SSL_HashAlgorithmType alg)
{
	
	if(signed_file.IsEmpty() || (URLStatus) signed_file.GetAttribute(URL::KLoadStatus) != URL_LOADED)
		return FALSE;
	
	// Get The raw data
	OpAutoPtr<URL_DataDescriptor> desc(signed_file.GetDescriptor(NULL, TRUE, TRUE, TRUE));
	if(!desc.get())
		return FALSE;
	
	BOOL more = FALSE;
	unsigned long buf_len;
	
	if(desc->RetrieveData(more) == 0 || desc->GetBuffer() == NULL)
		return FALSE;
	
	if(desc->GetBufSize() == 0)
		return FALSE;
		
	SSL_Hash_Pointer digester(alg);
	if(digester.Error())
		return FALSE;
	
	digester->InitHash();
	
	do{
		more = FALSE;
		buf_len = desc->RetrieveData(more);
		
		digester->CalculateHash((unsigned char *)desc->GetBuffer(), buf_len);
		
		desc->ConsumeData(buf_len);
	}while(more);

	SSL_varvector32 signature_out;

	digester->ExtractHash(signature_out);

	if(digester->Error() || signature_out.Error())
		return FALSE;

#ifdef _DEBUG
	OpString8 s8;
	OP_STATUS retval = ByteToHexStr(signature_out.GetDirect(), signature_out.GetLength(), s8);
	OP_ASSERT(retval == OpStatus::OK);
#endif
	
	byte* byte_buffer = NULL;
	unsigned int buffer_len = 0;
	OP_STATUS ret = HexStrToByte(checksum, byte_buffer, buffer_len);
	if(OpStatus::IsError(ret))
		return FALSE;

	SSL_varvector32 signature_in;
	signature_in.Set(byte_buffer, buffer_len);

	OP_DELETEA(byte_buffer);

	return signature_in == signature_out;
}

/**	Check file signature (SHA 256 algorithm)
 *	The function will return ERR if verification fails or if any errors occur,
 *
 *	@param	full_path		Full path of file to be checked
 *
 *	@param	signature		Base64 encoded checksum
 *
 *	@return OK if the verification succeded, ERR if there was any error. 
 */
OP_STATUS CheckFileSignature(const OpString &full_path, const OpString8& signature)
{
	OpString resolved;
	OpString path;
	OP_MEMORY_VAR BOOL successful;
	
	RETURN_IF_ERROR(path.Append("file:"));
	RETURN_IF_ERROR(path.Append(full_path));
	RETURN_IF_LEAVE(successful = g_url_api->ResolveUrlNameL(path, resolved));
	
	if (!successful || resolved.Find("file://") != 0)
		return OpStatus::ERR;
	
	URL url = g_url_api->GetURL(resolved.CStr());
	
	if (!url.QuickLoad(TRUE))
		return OpStatus::ERR;
	
	// Verify hash
	if (VerifyChecksum(url, signature, SSL_SHA_256))
		return OpStatus::OK;
	else
		return OpStatus::ERR;
	/*	
	 if (VerifySignedFile(url, signature, AUTOUPDATE_KEY, sizeof(AUTOUPDATE_KEY), SSL_SHA_256))
	 return OpStatus::OK;
	 else
	 return OpStatus::ERR;
	 */
}

#endif


