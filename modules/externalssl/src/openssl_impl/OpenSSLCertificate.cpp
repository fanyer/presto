/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

/**
 * @file OpenSSLCertificate.cpp
 *
 * SSL certificate implementation using OpenSSL.
 *
 * @author Alexei Khlebnikov <alexeik@opera.com>
 *
 */

#include "core/pch.h"

#ifdef EXTERNAL_SSL_OPENSSL_IMPLEMENTATION

#include "modules/externalssl/src/openssl_impl/OpenSSLCertificate.h"


OpenSSLCertificate* OpenSSLCertificate::Create(X509* x509)
{
	OpenSSLCertificate* cert = OP_NEW(OpenSSLCertificate, ());
	BIO* bio = BIO_new(BIO_s_mem());

	OP_STATUS status = CreateStep2(x509, bio, cert);
	
	if (bio)
	{
		BIO_free(bio);
		bio = 0;
	}
	
	if (status != OpStatus::OK)
	{
		OP_DELETE(cert);
		cert = 0;
	}
	
	return cert;
}


OP_STATUS OpenSSLCertificate::CreateStep2(X509* x509, BIO* bio, OpenSSLCertificate* cert)
{
	if (!x509 || !bio || !cert)
		return OpStatus::ERR_NULL_POINTER;

	// Validity period, issuer, subject and subject' CN.
	{
		enum
		{
			// Not used.
			FIELD_FIRST,
			// Validity period.
			FIELD_VALID_FROM,
			FIELD_VALID_TO,
			// Issuer.
			FIELD_ISSUER,
			// Subject must be the last meaningful field, it's important.
			FIELD_SUBJECT,
			// Not used.
			FIELD_LAST
		};

		ASN1_TIME* asn1_time;
		X509_NAME* x509_name;
		char* data;
		long len;
	
		// Set validity period, issuer, subject.
		for (int field_num = FIELD_FIRST + 1; field_num < FIELD_LAST; field_num++)
		{
			// Reset BIO.
			int res = BIO_reset(bio);
			OP_ASSERT(res == 1);

			// Get the needed structure.
			switch (field_num)
			{
				case FIELD_VALID_FROM: asn1_time = X509_get_notBefore(x509);    break;
				case FIELD_VALID_TO:   asn1_time = X509_get_notAfter(x509);     break;
				case FIELD_ISSUER:     x509_name = X509_get_issuer_name(x509);  break;
				case FIELD_SUBJECT:    x509_name = X509_get_subject_name(x509); break;
				default: return OpStatus::ERR;
			}
			// asn1_time or x509_name is owned by x509.
	
			// Print the structure to bio.
			switch (field_num)
			{
				case FIELD_VALID_FROM:
				case FIELD_VALID_TO:
					OP_ASSERT(asn1_time);
					// The commented variant is more standard,
					// but less pretty.
					//res = ASN1_STRING_print_ex(bio, asn1_time,
					//	ASN1_STRFLGS_RFC2253 & ~ASN1_STRFLGS_ESC_MSB);
					res = ASN1_TIME_print(bio, asn1_time);
					// On success: res == 1.
					// On failure: res == 0.
					break;

				case FIELD_ISSUER:
				case FIELD_SUBJECT:
					OP_ASSERT(x509_name);
					res = X509_NAME_print_ex(bio, x509_name, 0,
						XN_FLAG_ONELINE & ~ASN1_STRFLGS_ESC_MSB);
					// On success: res == number of bytes written.
					// On failure: res == -1.
					break;

				default: return OpStatus::ERR;
			}

			// Check if printing succeeded.
			if (res <= 0)
				return OpStatus::ERR;

			// Get printed text from bio.
			len = BIO_get_mem_data(bio, &data);
			OP_ASSERT(data);
			OP_ASSERT(len <= INT_MAX);
			OP_ASSERT(res == 1 || len == res);
	
			// Get the field string.
			OpString* field;
			switch (field_num)
			{
				case FIELD_VALID_FROM: field = &(cert->m_valid_from); break;
				case FIELD_VALID_TO:   field = &(cert->m_valid_to);   break;
				case FIELD_ISSUER:     field = &(cert->m_issuer);     break;
				case FIELD_SUBJECT:    field = &(cert->m_full_name);  break;
				default: return OpStatus::ERR;
			}

			// Remember the field.
			OP_STATUS status = field->SetFromUTF8(data, len);
			RETURN_IF_ERROR(status);
		}

		// Subject's common name aka short name.
		// Now we get advatage of the fact that the subject has been processed
		// last in the previous cycle.
		// Reusing x509_name: it's still a pointer to the subject structure.
		// Reusing subject's buffer at "data" of length "len". It has space
		// to hold the whole subject. CN is a part of the subject, thus
		// the buffer must be more than enough to hold CN.
		int res = X509_NAME_get_text_by_NID(x509_name, NID_commonName, data, len);
		// Allow empty CN. Don't exit with error in this case.
		if (res > 0)
		{
			OP_STATUS status = cert->m_short_name.SetFromUTF8(data, res);
			RETURN_IF_ERROR(status);
		}
	}

	// Hash.
	op_memcpy(cert->m_sha1_hash, x509->sha1_hash, SHA_DIGEST_LENGTH);

	return OpStatus::OK;
}

#endif // EXTERNAL_SSL_OPENSSL_IMPLEMENTATION
