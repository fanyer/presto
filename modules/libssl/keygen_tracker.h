/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#ifndef _KEYGEN_TRACKER_H_
#define _KEYGEN_TRACKER_H_

#if defined _NATIVE_SSL_SUPPORT_ && defined LIBSSL_ENABLE_KEYGEN

#include "modules/url/url2.h"

class OpWindow;
class SSLSecurtityPasswordCallbackImpl;
struct SSL_dialog_config;

/** This object manages the generation of a private key 
 *	The result is a Base64 encoded 
 */
class SSL_Private_Key_Generator : public MessageObject
{
protected:
	SSL_Certificate_Request_Format m_format;
	SSL_BulkCipherType m_type;
	unsigned int m_keysize;
	OpString8 m_challenge;
	OpString8 m_spki_string;

	URL  m_target_url;

	OpWindow *window;
	MessageHandler *msg_handler;
	OpMessage finished_message;
	MH_PARAM_1 finished_id;

	SSL_Options *optionsManager;
	BOOL external_opt; 

	SSL_secure_varvector32 m_pkcs8_private_key;
	SSL_varvector16 m_public_key_hash;
	SSLSecurtityPasswordCallbackImpl *asking_password;

public: 
	SSL_Private_Key_Generator();
	virtual ~SSL_Private_Key_Generator();

	/**
	 *	Prepares the keygeneration procedure
	 *
	 *	@param	config	Configuration for dialogs to be displayed as part of the key generation process, as well as the finished message to be sent to the caller
	 *					The finished message's par2 will be TRUE if the operation succeded, FALSE if the operation failed or was cancelled
	 *	@param	target	The form action URL to which the request will be sent. This will be used for the comment in the key entry
	 *	@param	format	Which certificate request format should be used?
	 *	@param	type	Which public key encryption algorithm should the key be created for?
	 *	@param	challenge	A textstring to be included and signed as part of the request
	 *	@param	keygen_size	The selected length of the key
	 *	@param	opt		Optional SSL_Options object. If a non-NULL opt is provided, then CommitOptionsManager MUST be used by the caller
	 */
	OP_STATUS Construct(SSL_dialog_config &config, URL &target, SSL_Certificate_Request_Format format, SSL_BulkCipherType type, const OpStringC8 &challenge, 
										unsigned int keygen_size, SSL_Options *opt);

	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/** Get the generated SPKI string with the certificate requeste */
	OpStringC8 &GetSPKIString(){return m_spki_string;}

	/** Start the key generation process 
	 *  
	 *	Returns: 
	 *		InstallerStatus::KEYGEN_FINISHED if the entire process is finished; a message will be posted, but need not be handled
	 *		OpStatus::OK  if the process continues, wait for finished message
	 */
	OP_STATUS StartKeyGeneration();

protected:

	/** Implementation specific
	 *
	 *  Set up the key generation process, 
	 *  If generation is finished, the implementation MUST call StoreKey
	 *
	 *	returns:
	 *
	 *		InstallerStatus::KEYGEN_FINISHED if the entire process is finished; a message will be posted, but need not be handled
	 *		OpStatus::OK  if the process continues, wait for finished message
	 */
	virtual OP_STATUS InitiateKeyGeneration()=0;

	/** Implementation specific
	 * 
	 *	Execute the next step of the generation procedure
	 *  If generation is finished, the implementation MUST call StoreKey
	 *
	 *	returns:
	 *
	 *		OpStatus::OK  if the process continues, wait for finished message
	 *		InstallerStatus::ERR_PASSWORD_NEEDED if the security password is needed
	 */
	virtual OP_STATUS IterateKeyGeneration()=0;

	/** Stores the key with the associate public key hash */
	OP_STATUS StoreKey(SSL_secure_varvector32 &pkcs8_private_key, SSL_varvector32 &public_key_hash);

	/** Posts the finished message and if the generation completed successfully 
	 *	and if no external option manager is used, commits the options
	 */
	void Finished(BOOL success);

private:

	/** The internal store key operation */
	OP_STATUS InternalStoreKey();

};

#endif // LIBSSL_ENABLE_KEYGEN

#endif // _KEYGEN_TRACKER_H_
