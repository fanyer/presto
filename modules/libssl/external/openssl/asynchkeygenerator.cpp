#include "core/pch.h"


#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_OPENSSL_ 
# if defined API_LIBOPEAY_ASYNCHRONOUS_KEYGENERATION && defined LIBSSL_ENABLE_KEYGEN

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/optenums.h"
#include "modules/libssl/ssl_api.h"
#include "modules/libssl/external/openssl/eayhead.h"
#include "modules/libssl/external/openssl/asynchkeygenerator.h"
#include "modules/formats/encoder.h"
#include "modules/util/cleanse.h"

#define SSL_SHA_LENGTH 20
OP_STATUS FeedTheRandomAnimal();

SSL_Private_Key_Generator *SSL_API::CreatePrivateKeyGenerator(
					SSL_dialog_config &config, URL &target, SSL_Certificate_Request_Format format,
					SSL_BulkCipherType type, const OpStringC8 &challenge, unsigned int keygen_size, SSL_Options *opt)
{
	AsynchKeyPairGenerator *generator = OP_NEW(AsynchKeyPairGenerator, ());

	if(!generator)
		return NULL;

	if(OpStatus::IsError(generator->Construct(config, target, format, type, challenge, keygen_size, opt)))
	{
		OP_DELETE(generator);
		return NULL;
	}

	return generator;
}


AsynchKeyPairGenerator::AsynchKeyPairGenerator()
	:	m_state_handle(NULL)
		,m_progress(0)
{
}

AsynchKeyPairGenerator::~AsynchKeyPairGenerator()
{
}

OP_STATUS AsynchKeyPairGenerator::InitiateKeyGeneration()
{
	OP_ASSERT(m_type == SSL_RSA);
	if (m_type != SSL_RSA || m_keysize < 1024)
	{
		/*we only handle rsa for now*/
		return OpStatus::ERR_OUT_OF_RANGE;	
	}
	
	if (m_state_handle != NULL)
	{
		return OpStatus::ERR;			
	}
	
	RETURN_IF_ERROR(FeedTheRandomAnimal());
    	
	RSA_KEYGEN_STATE_HANDLE	*state_handle = RSA_generate_key_ex_asynch_init(m_keysize);	

	if (state_handle == NULL)
		return OpStatus::ERR_NO_MEMORY;
	OP_ASSERT(m_state_handle == NULL);
	m_state_handle = state_handle;
	
/*This code can be used for synchronical testing without using the message loop
 * Also remove all the m_KeyGenparent_window g_main_message_handler->PostMessage() below 
 * 
 * 	do
	{
		HandleCallback(MSG_LIBSSL_RUN_KEYALGORITHM_ITERATION, 0, 0);	
	} 	while (m_state_handle && m_state_handle->state != RSA_KEYGEN_DONE);

*/
		
	return InstallerStatus::KEYGEN_WORKING;
}

OP_STATUS AsynchKeyPairGenerator::IterateKeyGeneration()
{
	OP_ASSERT(m_state_handle);

	RSA_generate_key_ex_asynch_runslice(m_state_handle);
	
	if (m_progress < m_state_handle->progress)
	{
		m_progress = m_state_handle->progress;
	}
	
	if (m_state_handle->error == TRUE)
	{	
		SSL_secure_varvector32 privkey;
		SSL_varvector16 pub_key_hash,tempvec;	
		RSA_keygen_asynch_clean_up(m_state_handle);
		m_state_handle = NULL;
		return OpStatus::ERR;
	}
	else if (m_state_handle->state == RSA_KEYGEN_DONE && m_state_handle->error == FALSE)
	{
		OP_STATUS status = OpStatus::ERR;
		unsigned long spki_len1;
		SSL_secure_varvector32 privkey;
		SSL_varvector16 pub_key_hash,tempvec;	

		tempvec.Resize(16*1024);
		if(tempvec.Error())
			return tempvec.GetOPStatus();

		spki_len1 = tempvec.GetLength();

		unsigned long PrivateKeylength=0;
		unsigned char *PrivateKey = NULL;
		
		do
		{
			PrivateKey = RSA_keygen_asynch_get_public_key(m_format,m_challenge.CStr(),tempvec.GetDirect(),&spki_len1,&PrivateKeylength, m_state_handle);

			if(PrivateKey == NULL || spki_len1 == 0)
				break;

			char *b64_encoded_spki = NULL;
			int b64_encoded_spki_len = 0;

			MIME_Encode_SetStr(b64_encoded_spki, b64_encoded_spki_len, (const char *) tempvec.GetDirect(), spki_len1, NULL, GEN_BASE64_ONLY_LF);

			if(b64_encoded_spki == NULL || b64_encoded_spki_len == 0)
			{
				OP_DELETEA(b64_encoded_spki);
				break;
			}

			status = m_spki_string.Set(b64_encoded_spki, b64_encoded_spki_len);
			OP_DELETEA(b64_encoded_spki);

			pub_key_hash.Set(PrivateKey,SSL_SHA_LENGTH);
			if(pub_key_hash.ErrorRaisedFlag || pub_key_hash.GetLength() == 0)
				goto err;	

			privkey.Set(PrivateKey + SSL_SHA_LENGTH, PrivateKeylength - SSL_SHA_LENGTH);
			if(privkey.ErrorRaisedFlag || privkey.GetLength() == 0)
				goto err;	

			status = StoreKey(privkey, pub_key_hash);
		}while(0);
err:

		if(PrivateKey != NULL)
		{
			OPERA_cleanse_heap(PrivateKey,(size_t) PrivateKeylength);
			CRYPTO_free(PrivateKey);
		}

		RSA_keygen_asynch_clean_up(m_state_handle);
		m_state_handle = NULL;
		return status;
	}
	return InstallerStatus::KEYGEN_WORKING;
}

# endif //!OPENSSL_ASYNCHRONOUS_KEYGENERATION
#endif // _NATIVE_SSL_SUPPORT_ && _SSL_USE_OPENSSL_ && LIBOPEAY_ASYNCHRONOUS_KEYGENERATION
