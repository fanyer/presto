/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef __SMC_MAN_H__
#define __SMC_MAN_H__

#if defined _NATIVE_SSL_SUPPORT_ && defined _SSL_USE_EXTERNAL_KEYMANAGERS_

#include "modules/util/smartptr.h"
#include "modules/hardcore/base/periodic_task.h"
#include "modules/libssl/handshake/hand_types.h"

class OpKeyManStatus : public OpStatus
{
public:
    enum
    {
        UNSUPPORTED_VERSION = USER_ERROR + 1,
        DISABLED_MODULE = USER_ERROR + 2,
    };
};


/** Client Key Manager Base Class
 *	This Base class is used to administrate external public key
 *	systems, like smart cards
 */
class SSL_KeyManager : public ListElement<SSL_KeyManager>, public OpReferenceCounter
{
public:
	SSL_KeyManager();
	~SSL_KeyManager();

	/** Retrieve a list of the Public Keys and certificates available from this provider, items are added to cipherlist 
	 *	Optionally, the provider can select certificates based on a list of DER-encoded CA issuer names provided by the server,
	 *	or based on the the server's name and port.
	 *
	 *	Implementations should use SetupCertificateEntry() to add the certificate and key to the list, and to 
	 *	use AllowedCertificate() to check if a certificate issuer is permitted.
	 *
	 *	@param	cipherlist	Pointer to list in which to add the combined certificate and key handlers
	 *	@param	ca_names	List of DER encoded certificate issuer names, as provided by the server. Empty list means no restrictions.
	 *	@param	sn			Servername object for the server being accessed.
	 *	@param	port		The port number on the server we are connecting to.
	 */
	virtual OP_STATUS	GetAvailableKeys(SSL_CertificateHandler_ListHead *cipherlist,  SSL_DistinguishedName_list &ca_names, ServerName *sn, uint16 port) = 0;


#ifdef _SSL_USE_SMARTCARD_
	/** Checks that all keys are present in their locations, and takes necessary actions for 
	 *	those keys that are no longer present, such as removing content authorized by the key 
	 *
	 *	This operation MUST NOT block, and MUST return immediately after checking the status.
	 */
	virtual void ConfirmKeysPresent() = 0;
#endif

	/** Returns TRUE if this interface is not presently in use */
	//BOOL	Idle();

	/** Returns last time used */
	//time_t	LastUsed();

	/** Updates the last used timestamp */
	//void	WasUsedNow();

	/** Register this manager in the main manager */
	void Register(){g_ssl_api->RegisterExternalKeyManager((SSL_KeyManager *) this);};

	/** Unregister this manager from the main manager */
	void Unregister(){if(InList()) Out();};


protected:

	/** Easy to use function to add a new cert and key item to the list, along with the required information
	 *
	 *	@param	cipherlist	Pointer to list in which to add the combined certificate and key handlers
	 *	@param	lbl			String with a label for the certificate
	 *	@param	shortname	A short name for the key, used in list of available certificates
	 *	@param	certlist	List of certificates, the first is the key's certificate, the others are issuer 
	 *						certificates, where the second signed the immediately preceding certificate
	 *	@param	pkey		The handler of the private key to be used to sign the authentication. This key may or may not be login protected.
	 */
	OP_STATUS SetupCertificateEntry(SSL_CertificateHandler_ListHead *cipherlist, const OpStringC &lbl, const OpStringC &shortname, SSL_ASN1Cert_list &cert, SSL_PublicKeyCipher *pkey);


	/** Checks if the issuer_candidate is in the list of issusers listed by the server
	 *
	 *	@param	ca_names	List of DER encoded certificate issuer names, as provided by the server. Empty list means no restrictions.
	 *	@param	issuer_candidate	DER encoded issuer name to be checked against ca_names list
	 *	@return	BOOL	TRUE if allowed by the list.
	 */
	BOOL	AllowedCertificate(SSL_DistinguishedName_list &ca_names, SSL_DistinguishedName &issuer_candidate);
};

typedef AutoDeleteList<SSL_KeyManager> SSL_Keymanager_Head;


/** The central manager of the key provider managers */
class SSL_ExternalKeyManager 
#ifdef _SSL_USE_SMARTCARD_
	: public PeriodicTask
#endif
{
private: 
	/** List of providers */
	SSL_Keymanager_Head key_masters;

public:

	SSL_ExternalKeyManager();
	~SSL_ExternalKeyManager();


	/** Intialize the Object */
	OP_STATUS InitL();

private:
	/** Retrieve a list of the Public Keys and certificates available from this provider, items are added to cipherlist 
	 *	Optionally, the provider can select certificates based on a list of DER-encoded CA issuer names provided by the server,
	 *	or based on the the server's name and port.
	 *
	 *	Implementations should use the SetupCertificateEntry to add the certificate and key to the list.
	 *
	 *	@param	cipherlist	Pointer to list in which to add the combined certificate and key handlers
	 *	@param	ca_names	List of DER encoded certificate issuer names, as provided by the server. Empty list means no restrictions.
	 *	@param	sn			Servername object for the server being accessed.
	 *	@param	port		The port number on the server we are connecting to.
	 */
	OP_STATUS	GetAvailableKeys(SSL_CertificateHandler_ListHead *cipherlist, SSL_DistinguishedName_list &ca_names, ServerName *sn, uint16 port);

public:
#ifdef _SSL_USE_SMARTCARD_
	/** Checks that all keys are present in their locations, and takes necessary actions for 
	 *	those keys that are no longer present, such as removing content authorized by the key 
	 *
	 *	This operation MUST NOT block, and MUST return immediately after checking the status.
	 */
	void ConfirmKeysPresent();

	/** Action for the periodic task of checking if there are some keys removed from the system */
	virtual void Run ();
#endif

	/** Remove any masters that have been idle for more than 5 minutes */
	//void RemoveIdle();

	/** Are there any key providers? */
	BOOL ActiveKeys(){return !key_masters.Empty();}

	/** Add a new provider to the list of providers */
	void RegisterKeyManager(SSL_KeyManager *provider){provider->Into(&key_masters);}

	/** Retrieve a list of the Public Keys and certificates available from this provider, items are added to cipherlist 
	 *	Optionally, the provider can select certificates based on a list of DER-encoded CA issuer names provided by the server,
	 *	or based on the the server's name and port.
	 *
	 *	Implementations should use the SetupCertificateEntry to add the certificate and key to the list.
	 *
	 *	@param	cipherlist	Pointer to list in which to add the combined certificate and key handlers
	 *	@param	ca_names	List of DER encoded certificate issuer names, as provided by the server. Empty list means no restrictions.
	 *	@param	sn			Servername object for the server being accessed.
	 *	@param	port		The port number on the server we are connecting to.
	 */
	OP_STATUS GetAvailableKeysAndCertificates(SSL_CertificateHandler_ListHead *certs, SSL_DistinguishedName_list &ca_names, ServerName *sn, uint16 port);
};
#endif
#endif
