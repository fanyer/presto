/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_LIBCRYPTO_MODULE_H
#define MODULES_LIBCRYPTO_MODULE_H

#include "modules/util/adt/opvector.h"

class CryptoCertificateStorage;
class CryptoMasterPasswordEncryption;
class CryptoMasterPasswordHandler;
class OpRandomGenerator;

class LibcryptoModule : public OperaModule
{
public:
	LibcryptoModule();
	virtual void InitL(const OperaInitInfo& info);
	virtual void Destroy();
	virtual ~LibcryptoModule();

public:
	OpRandomGenerator *m_random_generator;

#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
	CryptoMasterPasswordEncryption *m_master_password_encryption;
	CryptoMasterPasswordHandler *m_master_password_handler;
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT

#ifndef _NATIVE_SSL_SUPPORT_
	OpRandomGenerator *m_ssl_random_generator;
#endif// !_NATIVE_SSL_SUPPORT_
private:
	OpVector<OpRandomGenerator> *m_random_generators;

#if defined(CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION) && !defined(_SSL_USE_OPENSSL_)
	OP_STATUS InitOpenSSLLibrary();
	OP_STATUS DestroyOpenSSLLibrary();
#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION && !_SSL_USE_OPENSSL_

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT
public:
	CryptoCertificateStorage* GetCertificateStorage();

private:
	void LoadCertificatesL();

private:
	CryptoCertificateStorage* m_cert_storage;
#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT

friend class OpRandomGenerator;
};


// Inlines implementation and defines.
#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
#define g_libcrypto_master_password_encryption (g_opera->libcrypto_module.m_master_password_encryption)
#define g_libcrypto_master_password_handler (g_opera->libcrypto_module.m_master_password_handler)
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT

#define g_libcrypto_random_generator (g_opera->libcrypto_module.m_random_generator)

#ifndef _NATIVE_SSL_SUPPORT_
#define g_libcrypto_ssl_random_generator (g_opera->libcrypto_module.m_ssl_random_generator)
#endif // !_NATIVE_SSL_SUPPORT_

#ifdef CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT

inline CryptoCertificateStorage* LibcryptoModule::GetCertificateStorage()
{ return m_cert_storage; }

#define g_libcrypto_cert_storage (g_opera->libcrypto_module.GetCertificateStorage())

#endif // CRYPTO_CERTIFICATE_VERIFICATION_SUPPORT

#define LIBCRYPTO_MODULE_REQUIRED

#endif // !MODULES_LIBCRYPTO_MODULE_H
