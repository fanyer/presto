/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef OP_CERTIFICATE_H
#define OP_CERTIFICATE_H

/** @short Information about an X.509 certificate.
 *
 *  This is an interface that was created to hold X.509 certificates from
 *  an External SSL stack. In External SSL mode certificate instances
 *  originating from connections to HTTPS servers are both retrieved
 *  and destructed in External_SSL_Comm class. In this case the intended
 *  life cycle of the certificate is like this:
 *
 *  1) Core makes a HTTPS connection to a server.
 *
 *  2) The server presents its certificate. Core retrieves the certificate
 *     instance by calling OpSocket::ExtractCertificate(). After that Core
 *     owns the certificate.
 *
 *  3) The certificate is presented to the user on loading if it [certificate]
 *     can't be verified automatically or when the user invokes its display
 *     via UI.
 *
 *  4) When the user leaves the site where the certificate belongs, it is
 *     destructed by Core by calling the destructor.
 *
 *  In this case certificate instances are created and destructed while Opera
 *  is initialized.
 *
 *  Alternatively, a certificate can be retrieved from certificate manager,
 *  i.e. local certificate database. In this case certificates are owned
 *  by the certificate manager. Please refer to @ref OpCertificateManager
 *  documentation on the validity of the retrieved pointer to the certificate.
 *
 *  There are plans to extend this interface beyond External SSL, so that
 *  both Native and External SSL would share the same interface for certificates.
 *  So far OpSSLListener::SSLCertificate is made inherited from this class,
 *  but Native SSL still uses OpSSLListener::SSLCertificate everywhere.
 *
 */
class OpCertificate
{
public:
	/** Empty virtual destructor. */
	virtual ~OpCertificate() {}

	/** @name Getters.
	 *  Pointed data is owned by the certificate instance.
	 *  Pointers are valid as long as the instance is not destructed.
	 *  @{
	 */

	/** Get the short name of the certificate. */
	virtual const uni_char* GetShortName() const = 0;

	/** Get the full name of the certificate. */
	virtual const uni_char* GetFullName() const = 0;

	/** Get the issuer of the certificate. */
	virtual const uni_char* GetIssuer() const = 0;

	/** Get start date/time of the certificate validity. */
	virtual const uni_char* GetValidFrom() const = 0;

	/** Get end date/time of the certificate validity. */
	virtual const uni_char* GetValidTo() const = 0;

	/** Get any extra data related to the certificate.
	 *
	 * This can include public keys, fingerprints, etc.
	 */	
	virtual const uni_char* GetInfo() const = 0;
	
	/** used to uniquely identify the certificate */
	virtual const char* GetCertificateHash(unsigned int &length) const = 0;

	/** @} */
};

#endif // OP_CERTIFICATE_H
