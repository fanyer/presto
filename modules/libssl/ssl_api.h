/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SSL_API_H_
#define _SSL_API_H_

#if defined(_NATIVE_SSL_SUPPORT_)

#include "modules/libssl/base/sslenum.h"

class URL;
class SSL_Options;
class ProtocolComm;
class MessageHandler;
class ServerName;
class SSL_PublicKeyCipher;
class SSL_GeneralCipher;
class SSL_Hash;
class SSL_CertificateHandler;
class SSL_Certificate_Installer_Base;
class SSL_varvector32;
struct SSL_Certificate_Installer_flags;
struct SSL_dialog_config;
class SSL_Private_Key_Generator;
#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
class SSL_External_CertRepository_Base;
#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
class SSL_KeyManager;
class SSL_CertificateHandler_ListHead;
class OpWindow;


/** Keysize storage, used to determine lowest keysize among a number of PKI keys */
struct SSL_keysizes
{
	/** The keysize for RSA, DH and DSA, 0 means not yet set */
	uint32 RSA_keysize;

	SSL_keysizes():RSA_keysize(0){}
	void Update(const SSL_keysizes &old){if(!RSA_keysize || RSA_keysize> old.RSA_keysize) RSA_keysize = old.RSA_keysize;}
};


/** This class functions as the API to several libssl operations
 *	that are available to external modules
 *	It must be referenced as g_ssl_api
 */
class SSL_API
{
#ifdef SELFTEST
	BOOL lock_security_manager;
#endif

public:
	/** Constructor */
	SSL_API(){
#ifdef SELFTEST
	lock_security_manager = FALSE;
#endif
	};
	/** Destructor */
	~SSL_API(){};

	static void RemoveURLReferences(ProtocolComm *comm);

	/** Create an independent SSL_Options security manager object, optionally defining a folder and that
	 *	it register changes that can later be committed to the g_security_manager object
	 */
	SSL_Options *CreateSecurityManager(BOOL register_changes = FALSE, OpFileFolder folder = OPFILE_HOME_FOLDER);

	/** Commit all changes in the optionsManager object into the current SSL preference status */
	void CommitOptionsManager(SSL_Options *optionsManager);

	/** Check if there is a global security manager, and if necessary create it. Returns FALSE if it was not able to create it */
	BOOL CheckSecurityManager();

#ifdef SELFTEST
	void UnLoadSecurityManager(BOOL finished =FALSE);
	void LockSecurityManager(BOOL flag){lock_security_manager = flag;};
#endif

	/** Remove all information that has expired, such as the security password if it has passed the expiration date */
	void CheckSecurityTimeouts();

	/** Create a SSL protocol object for the specified server */
	ProtocolComm *Generate_SSL(MessageHandler* msg_handler, ServerName* hostname,
            unsigned short portnumber, BOOL V3_handshake=FALSE, BOOL do_record_splitting = FALSE);

	/**
	 *	Create a handler for the the specified Symmetric encryption method
	 *	Implemented in the actual library interface code
	 */
	DEPRECATED(SSL_GeneralCipher *CreateSymmetricCipherL(SSL_BulkCipherType cipher));
	SSL_GeneralCipher *CreateSymmetricCipher(SSL_BulkCipherType cipher, OP_STATUS &op_err);

	/**
	 *	Create a handler for the the specified Public Key encryption method
	 *	Implemented in the actual library interface code
	 */
	DEPRECATED(SSL_PublicKeyCipher *CreatePublicKeyCipherL(SSL_BulkCipherType cipher));
	SSL_PublicKeyCipher *CreatePublicKeyCipher(SSL_BulkCipherType cipher, OP_STATUS &op_err);

	/**
	 *	Create a handler for the the specified Mesage Digest method
	 *	Implemented in the actual library interface code
	 */
	SSL_Hash *CreateMessageDigest(SSL_HashAlgorithmType digest, OP_STATUS &op_err);

	/**
	 *	Create a handler for a certificate
	 *	Implemented in the actual library interface code
	 */
	SSL_CertificateHandler *CreateCertificateHandler();

#ifdef USE_SSL_CERTINSTALLER
	/** Create a Certificate Installation handler
	 *  The source of the certificate is the specified URL object
	 *	If no SSL_Options object is specified the certificate is committed directly to the security manager
	 *	Interactive mode is implicit by a non-NULL pointer to a SSL_dialog_config object
	 *	Caller must delete
	 */
	SSL_Certificate_Installer_Base *CreateCertificateInstallerL(URL &source, const SSL_Certificate_Installer_flags &install_flags, SSL_dialog_config *config=NULL, SSL_Options *optManager=NULL);

	/** Create a Certificate Installation handler
	 *  The source of the certificate is the specified varvector
	 *	If no SSL_Options object is specified the certificate is committed directly to the security manager
	 *	Interactive mode is implicit by a non-NULL pointer to a SSL_dialog_config object
	 *	Caller must delete
	 */
	SSL_Certificate_Installer_Base *CreateCertificateInstallerL(SSL_varvector32 &source, const SSL_Certificate_Installer_flags &install_flags, SSL_dialog_config *config=NULL, SSL_Options *optManager=NULL);
#endif
#ifdef LIBSSL_ENABLE_KEYGEN
	/**
	 *	Prepares the keygeneration procedure controller. The caller must start the process.
	 *
	 *	@param	config	Configuration for dialogs to be displayed as part of the key generation process, as well as the finished message to be sent to the caller
	 *	@param	target	The form action URL to which the request will be sent. This will be used for the comment in the key entry
	 *	@param	format	Which certificate request format should be used?
	 *	@param	type	Which public key encryption algorithm should the key be created for?
	 *	@param	challenge	A textstring to be included and signed as part of the request
	 *	@param	keygen_size	The selected length of the key
	 *	@param	opt		Optional SSL_Options object. If a non-NULL opt is provided, then CommitOptionsManager MUST be used by the caller
	 */
	SSL_Private_Key_Generator *CreatePrivateKeyGenerator(SSL_dialog_config &config, URL &target, SSL_Certificate_Request_Format format,
										SSL_BulkCipherType type, const OpStringC8 &challenge,
										unsigned int keygen_size, SSL_Options *opt);

	/** Returns (starting at 1) the i'th private keysize availabe for the given cipher type. Zero (0) means end of list */
	unsigned int SSL_GetKeygenSize(SSL_BulkCipherType type, int i);

	/** Returns the default keygensize for the specified keygen algorithm */
	unsigned int SSL_GetDefaultKeyGenSize(SSL_BulkCipherType type);
#endif
#ifdef LIBSSL_SECURITY_PASSWD
	/** This operation initiate a reference_counted block of the security password,
	 *	ensuring that the password is not destroyed while performing a longrunning security
	 *	password  operation (like multiple encrypt/decrypt operations).
	 */
	void StartSecurityPasswordSession();

	/** Encrypt data with either a specified password or the security password
	 *
	 *	@param	op_err		Returns the status of the operation.
	 *						 - If successful this contains an OpStatus::OK status
	 *						 - If a password was needed InstallerStatus::ERR_PASSWORD_NEEDED is the status.
	 *								The caller should wait until the dialog posts a success or failure message before trying the operation again
	 *								Note: if the dialog configuration finished_message was MSG_NO_MESSAGE no dialog is opened.
	 *						 - Other failures result in various error.
	 *
	 *	@param	in_data		Pointer to the binary data of in_len bytes to be encrypted
	 *
	 *	@param	in_len		Number of bytes in the in_data buffer
	 *
	 *	@param	out_len		The number of bytes in the returned buffer is put in this parameter
	 *
	 *	@param	password	If non-NULL, this contains the null-terminated password to be used when encrypting the data
	 *						If NULL, the security password is used, and if necessary a dialog is opened according to the
	 *						specification in the config parameter
	 *
	 *	@param	config		Dialog configuration to be used if a security password dialog has to be opened.
	 *						If config.finished_message is MSG_NO_MESSAGE no dialog is opened
	 *
	 *	@return				If successful return the pointer to the allocated memory used to store the encrypted data.
	 *						The length of the allocated buffer is specified in out_len. The data must be decrypted
	 *						by DecryptWithSecurityPassword using the same configuration
	 */
	unsigned char *EncryptWithSecurityPassword(OP_STATUS &op_err,
				const unsigned char *in_data, unsigned long in_len, unsigned long &out_len,
				const char* password, SSL_dialog_config &config);

	/** Decrypt data with either a specified password or the security password
	 *
	 *	@param	op_err		Returns the status of the operation.
	 *						 - If successful this contains an OpStatus::OK status
	 *						 - If a password was needed InstallerStatus::ERR_PASSWORD_NEEDED is the status.
	 *								The caller should wait until the dialog posts a success or failure message before trying the operation again
	 *								Note: if the dialog configuration finished_message was MSG_NO_MESSAGE no dialog is opened.
	 *						 - Other failures result in various error.
	 *
	 *	@param	in_data		Pointer to the binary data of in_len bytes to be decrypted (the data must have been created
							by EncryptWithSecurityPassword using the same configuration)
	 *
	 *	@param	in_len		Number of bytes in the in_data buffer
	 *
	 *	@param	out_len		The number of bytes in the returned buffer is put in this parameter
	 *
	 *	@param	password	If non-NULL, this contains the null-terminated password to be used when decrypting the data
	 *						If NULL, the security password is used, and if necessary a dialog is opened according to the
	 *						specification in the config parameter
	 *
	 *	@param	config		Dialog configuration to be used if a security password dialog has to be opened.
	 *						If config.finished_message is MSG_NO_MESSAGE no dialog is opened
	 *
	 *	@return				If successful return the pointer to the allocated memory used to store the decrypted data.
	 *						The length of the allocated buffer is specified in out_len.
	 */
	unsigned char *DecryptWithSecurityPassword(OP_STATUS &op_err,
					const unsigned char *in_data, unsigned long in_len, unsigned long &out_len,
					const char* password, SSL_dialog_config &config);

	/** This operation ends a reference_counted block of the security password,
	 *	that ensured that the password was not destroyed while performing a longrunning security
	 *	password  operation (like multiple encrypt/decrypt operations).
	 *	If the password has expired it will be destroyed if this count reaches zero.
	 */
	void EndSecurityPasswordSession();
#endif

	/** Determined the security level based on the public Key provided, returns the result in the 
	 *	security level and security reason parameters
	 *
	 *	@param	key		The public cipher key being checked
	 *	@param	lowest_key	Lowest keysize encountered so far, updated with information from key.
	 *	@param	security_level	In: Current security level (must be SECURITY_STATE_UNKNOWN first time)
	 *							Out: The resulting security level, including the key being tested
	 *	@param	security_reason	In: Current security reasons logged (must be SECURITY_REASON_NOT_NEEDED first time)
	 *							Out: The resulting security reasons logged, including for the key being tested.
	 *
	 *	@return	uint32		Length of current key
	 */
	uint32 DetermineSecurityStrength(SSL_PublicKeyCipher *key, SSL_keysizes &lowest_key, int &security_rating, int &low_security_reason);

#ifdef SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY
	/** Register the external Certificate lookup repository implemented for the platform
	 *	The new repository is pushed on top of the stack and used instead (implementations may use
	 *	the Suc() call to dicover and use the next repository).
	 *
	 *	The repository is only referenced by the module, it does not take ownership, and 
	 *	deleting the repository handler will immediately unregister the handler.
	 *
	 *	@param	repository	The repository to be registered.
	 */
	void RegisterExternalCertLookupRepository(SSL_External_CertRepository_Base *repository);

	/** If set to TRUE, the Opera SSL stack will stop looking up Opera's own 
	 *	certificate repository, only use the external one. Will only work if there
	 *	is a repository registered.
	 *	
	 *	@param	disable	TRUE if the internal repository will be disconnected, FALSE if it is to be used (default)
	 */
	void SetDisableInternalLookupRepository(BOOL disable);

	/** Is internal lookup disabled? */
	BOOL GetDisableInternalLookupRepository(){return g_ssl_disable_internal_repository;};
#endif // SSL_API_REGISTER_LOOKUP_CERT_REPOSITORY

#ifdef _SSL_USE_EXTERNAL_KEYMANAGERS_
public:
	void RegisterExternalKeyManager(SSL_KeyManager *);
#endif // _SSL_USE_EXTERNAL_KEYMANAGERS_

};

inline SSL_PublicKeyCipher *SSL_API::CreatePublicKeyCipherL(SSL_BulkCipherType cipher){OP_STATUS op_err = OpStatus::OK; SSL_PublicKeyCipher *ret = CreatePublicKeyCipher(cipher, op_err); LEAVE_IF_ERROR(op_err); return ret;}
inline SSL_GeneralCipher *SSL_API::CreateSymmetricCipherL(SSL_BulkCipherType cipher){OP_STATUS op_err = OpStatus::OK; SSL_GeneralCipher *ret = CreateSymmetricCipher(cipher, op_err); LEAVE_IF_ERROR(op_err); return ret;}
#endif

#endif // _SSL_API_H_
