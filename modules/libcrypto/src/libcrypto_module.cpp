/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#include "core/pch.h"

#include "modules/libcrypto/libcrypto_module.h"

#include "modules/libcrypto/include/CryptoExternalApiManager.h"
#include "modules/libcrypto/include/CryptoMasterPasswordEncryption.h"
#include "modules/libcrypto/include/CryptoMasterPasswordHandler.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/libcrypto/include/PEMCertificateLoader.h"

#if defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION) && !defined(_SSL_USE_OPENSSL_)
#include "modules/formats/base64_decode.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/evp.h"
#include "modules/libopeay/openssl/x509v3.h"
#include "modules/libopeay/openssl/rsa.h"
#include "modules/libcrypto/src/openssl_impl/openssl_util.h"
#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION && !_SSL_USE_OPENSSL_


LibcryptoModule::LibcryptoModule()
	:  m_random_generator(NULL)
#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
	 , m_master_password_encryption(NULL)
	 , m_master_password_handler(NULL)
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT
#ifndef _NATIVE_SSL_SUPPORT_
	, m_ssl_random_generator(NULL)
#endif	// !_NATIVE_SSL_SUPPORT_	
	, m_random_generators(NULL)
#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
	, m_cert_storage(NULL)
#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
{
}

void LibcryptoModule::InitL(const OperaInitInfo& info)
{
	m_random_generators = OP_NEW_L(OpVector<OpRandomGenerator>, ());
#ifdef CRYPTO_API_SUPPORT

# ifndef _NATIVE_SSL_SUPPORT_	
	LEAVE_IF_NULL(m_ssl_random_generator = OpRandomGenerator::Create());
# endif // !_NATIVE_SSL_SUPPORT_

	LEAVE_IF_NULL(m_random_generator = OpRandomGenerator::Create());
	
	OpRandomGenerator::AddEntropyFromTimeAllGenerators();

# ifdef _EXTERNAL_SSL_SUPPORT_
	LEAVE_IF_ERROR(CryptoExternalApiManager::InitCryptoLibrary());  // In case of external ssl, CryptoExternalApiManager::InitCryptoLibrary must be implemented by platform
# endif

# if defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION) && !defined(_SSL_USE_OPENSSL_)
	InitOpenSSLLibrary();
# endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION && !_SSL_USE_OPENSSL_
#endif // CRYPTO_API_SUPPORT

#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
	m_master_password_encryption = OP_NEW_L(CryptoMasterPasswordEncryption, ());
	m_master_password_encryption->InitL();

	m_master_password_handler = OP_NEW_L(CryptoMasterPasswordHandler, ());
	m_master_password_handler->InitL(TRUE);
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
	LoadCertificatesL();
#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
}


void LibcryptoModule::Destroy()
{
#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
	OP_DELETE(m_cert_storage);
	m_cert_storage = NULL;
#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT

#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
	OP_DELETE(m_master_password_handler);
	m_master_password_handler = NULL;

	OP_DELETE(m_master_password_encryption);
	m_master_password_encryption = NULL;
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT

#ifdef CRYPTO_API_SUPPORT	

# if defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION) && !defined(_SSL_USE_OPENSSL_)
	DestroyOpenSSLLibrary();
# endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION && !_SSL_USE_OPENSSL_

# ifdef _EXTERNAL_SSL_SUPPORT_
	CryptoExternalApiManager::DestroyCryptoLibrary(); // In case of external ssl, CryptoExternalApiManager::DestroyCryptoLibrary must be implemented by platform
# endif //_EXTERNAL_SSL_SUPPORT_

	OP_DELETE(m_random_generator);
	m_random_generator = NULL;

# ifndef _NATIVE_SSL_SUPPORT_
	OP_DELETE(m_ssl_random_generator);
	m_ssl_random_generator = NULL;
# endif // !_NATIVE_SSL_SUPPORT_

	OP_ASSERT(!m_random_generators || (m_random_generators->GetCount() == 0));
	OP_DELETE(m_random_generators);
	m_random_generators = NULL;
#endif // CRYPTO_API_SUPPORT	
}

#if defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION) && !defined(_SSL_USE_OPENSSL_)

/* If _SSL_USE_OPENSSL_ is off and we want to use some openssl algorithms,
 * we need to initiate parts of openssl.
 */
OP_STATUS LibcryptoModule::InitOpenSSLLibrary()
{
	OpenSSL_add_all_digests();

	OPENSSL_RETURN_IF(ERR_peek_error());
	
	return OpStatus::OK;
}

OP_STATUS LibcryptoModule::DestroyOpenSSLLibrary()
{
	X509_PURPOSE_cleanup();
	EVP_cleanup();

	return OpStatus::OK;
}
#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION && !defined(_SSL_USE_OPENSSL_)

LibcryptoModule::~LibcryptoModule()
{
}

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
void LibcryptoModule::LoadCertificatesL()
{
	OP_ASSERT(!m_cert_storage);
	LEAVE_IF_ERROR(CryptoCertificateStorage::Create(m_cert_storage));
	OP_ASSERT(m_cert_storage);

	OP_ASSERT(g_folder_manager);
	ANCHORD(OpString, folder_path);
	LEAVE_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_TRUSTED_CERTS_FOLDER, folder_path));

	TRAPD(status,
		PEMCertificateLoader loader;
		loader.SetInputDirectoryL(folder_path);
		loader.SetCertificateStorageContainer(m_cert_storage);
		loader.ProcessL();
	);
}
#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
