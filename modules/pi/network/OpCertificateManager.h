/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2006-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef OPCERTIFICATEMANAGER_H
#define OPCERTIFICATEMANAGER_H


class OpCertificate;

/** @short External SSL certificate manager.
 *
 * This is a class used to browse device certificates, like root certificates
 * and personal certificates installed on device.
 *
 * The intended life cycle is like this:
 *
 * 1) Somewhen, for example on initialization or on first Get() call or
 *    whenever it is convenient, the platform creates the certificate manager
 *    instance.
 *
 * 2) Core calls OpCertificateManager::Get() and gets a pointer to this
 *    instance. The platform increments the usage counter.
 *
 * 3) Core is done with the manager and calls OpCertificateManager::Release()
 *    to release the instance. The platform decrements the usage counter.
 *
 * 4) On shutdown or on low memory or whenever it is convenient, the platform
 *    destructs the instance, if the usage counter is 0.
 */
class OpCertificateManager
{
public:
	/** Type of certificate. */
	enum CertificateType
	{
		/** Personal/client certificate. */
		CertTypePersonal,

		/** Root CA certificate. */
		CertTypeAuthorities,

		/** Intermediate CA certificate. */
		CertTypeIntermediate,

		/** Server certificate, verified successfully or approved by user. */
		CertTypeApproved,

		/** Server certificate, failed to verify and rejected by user. */
		CertTypeRejected
	};

	/** Certificate settings. */
	struct Settings
	{
		/** Warn when this certificate is encountered. */
		BOOL warn;
		/** Allow connections that use this certificate. */
		BOOL allow;
	};

	/** Get certificate manager instance from the platform.
	 *  The platform may allocate some resources on this call. */
	static OP_STATUS Get(OpCertificateManager** cert_manager, CertificateType type);
	/** Release certificate manager instance.
	 *  The platform may deallocate some resources on this call. */
	virtual void Release() = 0;

	/** Count the certificates being managed by this instance. */
	virtual unsigned int Count() const = 0;
	/** Get certificate by index.
	 *  Certificates are indexed sequentially, starting from zero.
	 *  The returned certificate remains owned by the certificate manager,
	 *  the caller doesn't take over ownership. The returned pointer
	 *  is valid until the first function call that changes the certificate
	 *  manager, for example @ref Release() or @ref RemoveCertificate().
	 */
	virtual OpCertificate* GetCertificate(unsigned int index) = 0;

	/** Get certificate settings. */
	virtual OP_STATUS GetSettings(unsigned int index, Settings& settings) const = 0;
	/** Update certificate settings. */
	virtual OP_STATUS UpdateSettings(unsigned int index, const Settings& settings) = 0;

	/** Remove certificate by index.
	 *  The manager is at liberty to now forget it ever knew anything
	 *  about this certificate. */
	virtual OP_STATUS RemoveCertificate(unsigned int index) = 0;

protected:
	virtual ~OpCertificateManager() {}
};

#endif //OPCERTIFICATEMANAGER_H
