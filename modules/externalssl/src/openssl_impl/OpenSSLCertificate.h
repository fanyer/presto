/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpenSSLCertificate.h
 *
 * SSL certificate, able to create itself using OpenSSL X509 object.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#ifndef OPENSSL_CERTIFICATE_H
#define OPENSSL_CERTIFICATE_H

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/pi/network/OpCertificate.h"
#include <openssl/ssl.h>


class OpenSSLCertificate: public OpCertificate
{
public:
	static OpenSSLCertificate* Create(X509* x509);

public:
	// OpCertificate methods
    virtual const uni_char* GetShortName() const { return m_short_name.CStr(); }
    virtual const uni_char* GetFullName()  const { return m_full_name.CStr();  }
    virtual const uni_char* GetIssuer()    const { return m_issuer.CStr();     }
    virtual const uni_char* GetValidFrom() const { return m_valid_from.CStr(); }
    virtual const uni_char* GetValidTo()   const { return m_valid_to.CStr();   }
    virtual const uni_char* GetInfo()      const { return 0; }
    virtual const char* GetCertificateHash(unsigned int& length) const
		{ length = SHA_DIGEST_LENGTH; return (const char*)m_sha1_hash; }

private:
	static OP_STATUS CreateStep2(X509* x509, BIO* bio, OpenSSLCertificate* cert);

private:
	// Subject' CN.
    OpString m_short_name;
	// Subject.
    OpString m_full_name;
	// Issuer.
    OpString m_issuer;
	// Not before.
    OpString m_valid_from;
	// Not after.
    OpString m_valid_to;
	// Hash. Copied from X509::sha1_hash.
	unsigned char m_sha1_hash[SHA_DIGEST_LENGTH]; /* ARRAY OK 2009-10-02 alexeik */
};

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#endif // OPENSSL_CERTIFICATE_H
