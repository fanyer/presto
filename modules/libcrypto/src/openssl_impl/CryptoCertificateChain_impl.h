/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Haavard Molland   <haavardm@opera.com>
** @author Alexei Khlebnikov <alexeik@opera.com>
**
*/

#ifndef CRYPTO_CERTIFICATE_CHAIN_IMPL_H_
#define CRYPTO_CERTIFICATE_CHAIN_IMPL_H_

#ifdef  CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION

#include "modules/libcrypto/include/CryptoCertificateChain.h"
#include "modules/libopeay/openssl/cryptlib.h"
#include "modules/libopeay/openssl/x509.h"
#include "modules/libopeay/openssl/x509v3.h"


class CryptoCertificateChain_impl : public CryptoCertificateChain
{
public:
	~CryptoCertificateChain_impl();
	static OP_STATUS Make(CryptoCertificateChain *&chain);

	virtual OP_STATUS AddToChainBase64(const char *certificate_der);
	virtual OP_STATUS AddToChainBinary(const UINT8 *certificate_der, int length);

	virtual int GetNumberOfCertificates() const { return m_certificate_list.GetCount(); }
	virtual const CryptoCertificate* GetCertificate(int chain_index) const { return m_certificate_list.Get(chain_index); }
	virtual CryptoCertificate* GetCertificate(int chain_index) { return m_certificate_list.Get(chain_index); }
	virtual CryptoCertificate* RemoveCertificate(int chain_index) { return m_certificate_list.Remove(chain_index); }

	virtual OP_STATUS VerifyChainSignatures(VerifyStatus& reason, const CryptoCertificateStorage* ca_storage);

	/** Implementation-specific methods. */
	STACK_OF(X509)* GetStackOfX509() const { return m_certificate_chain; }

private:
	CryptoCertificateChain_impl();
	OP_STATUS UpdateCertificateList();

	OpAutoVector<CryptoCertificate> m_certificate_list;
	STACK_OF(X509) *m_certificate_chain;
	X509_STORE_CTX *m_certificate_storage;
};

#endif // CRYPTO_CERTIFICATE_VERIFICATION_USE_CORE_IMPLEMENTATION
#endif /* CRYPTO_CERTIFICATE_CHAIN_IMPL_H_ */
