/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef SSL_EXT_CERTREPOSITORY_H
#define SSL_EXT_CERTREPOSITORY_H

#ifdef _NATIVE_SSL_SUPPORT_
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

#include "modules/libssl/ssl_api.h"

/** The base class for external certificate repositories 
 *	This class does not expose a usable API, but is intended to
 *	allow a implementation independent repository to be created.
 *
 *	Actual implementations, identified by the library_type enum, implement 
 *	their API freely, depending on the structures and APIs they use.
 *
 *	The implementations MUST implement a reasonable timeout in case they 
 *	access remote sources for the certificates. In case of a timeout the
 *	implementation MUST disable further remote lookups for the failed 
 *	certificates for some time afterwards to allow the connection to continue;
 *	the suspension period should be at least 60 seconds.
 *
 *	To register the implementation, call the method Register(), to unregister call Unregister()
 *	Caller retains ownership of the implementation, and destruction automatically unsubscribes the implementation
 */
class SSL_External_CertRepository_Base : public Link
{
public:
	/** List of implementations supported, depends on what libraries the libssl module implements */
	enum library_type{
		/** Unknown implementation */
		Unknown,
		/** Implementation is for OpenSSL */
		OpenSSL
	};

private:

	/** Stored repository type */
	library_type repository_type;

protected:

	/** Constructor. Implementation must specify library type */
	SSL_External_CertRepository_Base(library_type typ):repository_type(typ){}

public:
	virtual ~SSL_External_CertRepository_Base()
		{
			if(InList())
				Out();
			if(g_ssl_external_repository.Empty())
				g_ssl_disable_internal_repository = FALSE;

		}

	/** Returns the library type of this repository. Used in case there are multiple repositories in use */
	library_type GetRepositoryType() const {return repository_type;}

	/** Called by implementation when the asynchronous lookup is completed for a certificate, or it timed out */
	void SignalLookupCompleted(){g_main_message_handler->PostMessage(MSG_SSL_EXTERNAL_REPOSITORY_LOADED, 0, 0);}

	/** Register this manager in the main manager */
	void Register(){g_ssl_api->RegisterExternalCertLookupRepository((SSL_External_CertRepository_Base *) this);};

	/** Unregister this manager from the main manager */
	void Unregister(){if(InList()) Out();};
};

#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

#endif	// _NATIVE_SSL_SUPPORT_

#endif /* SSL_EXT_CERTREPOSITORY_H */
