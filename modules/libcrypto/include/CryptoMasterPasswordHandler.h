/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#ifndef CRYPTOMASTERPASSWORDHANDLER_H_
#define CRYPTOMASTERPASSWORDHANDLER_H_

#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
#include "modules/hardcore/mh/messobj.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/locale/locale-enum.h"

class OpWindow;
class WindowCommander;

/** @class ObfuscatedPassword.
 *
 * Password obfuscater, used to safely distribute a secret password without risking password leakage.
 *
 * Usage:
 * @see CryptoMasterPasswordHandler below.
 */
class ObfuscatedPassword : private OpPrefsListener
{
public:

	/** Creates an obfuscated password.
	 *
	 * @param password           The password to obfuscate.
	 * @param length             The length of the password.
	 * @param use_password_aging If TRUE password will have a lifetime
	 *                           set by password_lifetime
	 * @param password_lifetime  Time in seconds the password should live.
	 * @return The obfuscated password, or NULL if OOM.
	 */
	static ObfuscatedPassword *Create(const UINT8 *password, int length, BOOL use_password_aging = FALSE, UINT password_lifetime = 0);


	/** Creates a copy of an obfuscated password.
	 *
	 * The timeout will not be reset.
	 *
	 * @param original The password to copy.
	 * @return The copied password.
	 */
	static ObfuscatedPassword *CreateCopy(const ObfuscatedPassword *original);

	/** Check if password has not passed it's lifetime.
	 *
	 * A password may have limited lifetime. After a certain amount
	 * of time given in the Create() the password will no longer work.
	 *
	 * @return TRUE if password is still OK.
	 */
	BOOL CheckAging() const;

	~ObfuscatedPassword();

private:
	ObfuscatedPassword();

	/**
	 * Get a copy of the password in clear.
	 *
	 * Caller takes owner ship and must delete array using OP_DELETEA.
	 *
	 * @param length[out] Length of the password.
	 * @return The password in bytes, or NULL if lifetime has passed.
	 */
	UINT8 *GetMasterPassword(int &length) const;


	/* Implementation of OpPrefsListener. */
	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	UINT8 *m_obfuscated_master_password_blob;
	int m_obfuscated_password_blob_length;

	// Used for password age checking. Time for last time the password was either used or verified.
	// Password aging is only used and updated if m_last_verified_time != 0
	// We let this be mutable as is not a part of the password per se. Thus changing this does not change a password.
	mutable time_t m_last_verified_time;

	// time in seconds the password is allowed to be used.
	int m_password_lifetime;

	// Add classes that are allowed to GetMasterPassword to this friends list.
	// Only encryption code from libcrypto and libssl can be allowed access.
	friend class CryptoMasterPasswordEncryption;
	friend class CryptoMasterPasswordHandler;
#ifdef _NATIVE_SSL_SUPPORT_
	friend class SSL_Options;
#endif //  _NATIVE_SSL_SUPPORT_
};

/** @class CryptoMasterPasswordHandler.
 *
 * Takes care of retrieving the master password from platform (and most likely, the user although
 * that is strictly not defined).
 *
 * Usage:
 * @code
 * class SomeClassThatNeedsMasterPassword : public CryptoMasterPasswordHandler::MasterPasswordRetrivedCallback
 * {
 * public:
 *      virtual void MasterPasswordRetrieved(CryptoMasterPasswordHandler::PasswordState state);
 *      {
 *          if (PasswordState != PASSWORD_RETRIVED_CANCEL)
 *              SomeFunctionNeedingPassword();
 *      }
 *
 *      void SomeFunctionNeedingPassword()
 *      {
 *          OP_STATUS status = g_libcrypto_master_password_handler->RetrieveMasterPassword(this, NULL);
 *          if (status == OpStatus::ERR_YIELD)
 *                return;
 *          }
 *
 *          const ObfuscatedPassword *master_password = g_libcrypto_master_password_handler->GetRetrievedMasterPassword();
 *
 *          UINT8 *encrypted_password;
 *          int encrypted_password_length;
 *          g_libcrypto_master_password_encryption->EncryptPasswordWithSecurityMasterPassword
 *          (
 *                encrypted_password, encrypted_password_length,
 *                (const UINT8*)password_to_encrypt, op_strlen(password_to_encrypt),
 *                master_password
 *          );
 *
 *         ANCHOR_ARRAY(UINT8, encrypted_password);
 *
 *         // Do whatever with encrypted password
 *      }
 *
 *      const char *password_to_encrypt;
 * };
 * @endcode
 */
class CryptoMasterPasswordHandler : private OpSSLListener::SSLSecurityPasswordCallback
{
public:
	CryptoMasterPasswordHandler();

	/** Init the API.
	 *
	 * Leaves with OpStatus::ERR_NO_MEMORY in case of memory problems.
	 *
	 * @param use_password_aging Set to TRUE, if you the password should stop work
	 *                           after a certain lifetime. Lifetime is set in seconds
	 *                           in preference PrefsCollectionNetwork::SecurityPasswordLifeTime.
	 */
	void InitL(BOOL use_password_aging = FALSE);

	enum PasswordState
	{
		PASSWORD_RETRIVED_SUCCESS,        // User gave correct password.
		PASSWORD_RETRIVED_WRONG_PASSWORD, // User gave wrong password.
		PASSWORD_RETRIVED_CANCEL          // User canceled.
	};

	class MasterPasswordRetrivedCallback
	{
	public:
		/**
		 * Callback sent to caller of RetrieveMasterPassword() when password has been retrieved.
		 *
		 * @param state                 The state of the password.
		 */
		virtual void OnMasterPasswordRetrieved(CryptoMasterPasswordHandler::PasswordState state) = 0;
	};

	/**
	 * Retrieve the master password from user.
	 *
	 * Asynchronous function.
	 *
	 * The caller of this function will receive a notification in
	 * OnMasterPasswordRetrieved() when the password has been retrieved.
	 *
	 * If this function returns OK, GetRetrievedMasterPassword() can be used
	 * to get the retrieved password.
	 *
	 * If function returns OpStatus::ERR_YIELD, you need to wait for
	 * OnMasterPasswordRetrieved callback before calling GetRetrievedMasterPassword().
	 *
	 * @param callback OnMasterPasswordRetrieved callback that will be called
	 *                 when password is ready. Might happen right away, or
	 *                 later if function returns OpStatus::ERR_YIELD.
	 * @param window   The window which needs to password. Can be NULL.
	 * @param reason   The reason for asking for password.
	 * @retval OpStatus::OK if password is ready.
	 * @retval OpStatus::ERR_YIELD If OnMasterPasswordRetrieved callback will be sent later.
	 * @retval OpStatus::ERR_NO_MEMORY for OOM.
	 */
	OP_STATUS RetrieveMasterPassword(MasterPasswordRetrivedCallback *callback, OpWindow *window, OpSSLListener::SSLSecurityPasswordCallback::PasswordRequestReason reason);

	/** Get the master password previously retrieved.
	 *
	 * If RetrieveMasterPassword() has been called previously with success this
	 * can be called to get the master password.
	 *
	 * @return The master password, or NULL if master password has not yet been retrieved.
	 */
	const ObfuscatedPassword *GetRetrievedMasterPassword();

	/**
	 * Forget the retrieved master password.
	 *
	 * Calling this function will invalidate the cached master password given by
	 * user, such that the dialogue will be triggered on next master password
	 * usage.
	 *
	 * Note that the master password check codes are still set, so this
	 * will not remove the password completely from the system.
	 */
	void ForgetRetrievedMasterPassword();

	/**
	 * Cancel retrieving password from user.
	 *
	 * @param callback The callback for which the retrieval was canceled.
	 */
	void CancelGettingPassword(MasterPasswordRetrivedCallback *callback);

	/**
	 * Check if the master password has been set.
	 *
	 * @return TRUE if this manager knows the password.
	 */
	BOOL HasMasterPassword();

	/**
	 * Set the new master password.
	 *
	 * Can only be called if no password is already set.
	 *
	 * @param password The new password to use as master password.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY or OpStatus::ERR if password has been set before.
	 */
	OP_STATUS SetMasterPassword(const char* password);

	/**
	 * Change the master password.
	 *
	 * @param old_password The old password
	 * @param new_password The new password
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY, or OpStatus::ERR for wrong old password.
	 */
	OP_STATUS ChangeMasterPassword(const char* old_password, const char *new_password);

	/**
	 *  Check password policy.
	 *
	 *  Checks if password has more than 6 characters, and that either
	 *  1) if ascii only:at least one alpha and one non-alpha character.
	 *  2) at least one non ascii character.
	 *
	 * @param master_password password to check
	 * @return OpStatus::ERR for bad password, OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	OP_STATUS CheckPasswordPolicy(const char *master_password);

	/**
	 * Legacy algorithm ported from libssl.
	 *
	 * Check that user has provided correct password.
	 *
	 * @param master_password The password to check.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 *         or OpStatus::OpStatus::ERR_NO_ACCESS for wrong password.
	 */
	OP_STATUS CheckLibsslPasswordCheckcode(const char *master_password);

	/**
	 * Legacy algorithm ported from libssl.
	 *
	 * Creates the complete master password encryption key
	 *
	 * The complete password is an concatenation of the password
	 * and the decrypted password check code (m_system_part_password).
	 *
	 * Side effect of this function is to create and store the password check codes and salt.
	 *
	 * @param master_password The master password from which complete password is created.
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	ObfuscatedPassword *CreateCompletePassword(const char *master_password);

	virtual ~CryptoMasterPasswordHandler();

private:
	// OpSSLListener::SSLSecurityPasswordCallback implementation
	virtual Str::LocaleString GetTitle() const { return m_current_callback_title; };
	virtual Str::LocaleString GetMessage() const { return m_current_callback_message; }
	virtual OpSSLListener::SSLSecurityPasswordCallback::PasswordDialogMode GetMode() const { return m_current_callback_mode; }
	virtual OpSSLListener::SSLSecurityPasswordCallback::PasswordRequestReason GetReason() const { return m_current_callback_reason; }

	virtual void OnSecurityPasswordDone(BOOL ok, const char* old_password, const char* new_password);

	// Internal methods
	void SignalPasswordRetrievedToListeners(CryptoMasterPasswordHandler::PasswordState state, OpVector<MasterPasswordRetrivedCallback> &listeners);
	WindowCommander* GetWindowCommander(OpWindow *dialogule_origin_window);
	/* owned by CryptoMasterPasswordHandler, do not delete */

	UINT8 *GetMasterPasswordCheckCode(int &length);
	UINT8 *GetMasterPasswordCheckCodeSalt(int &length);

	/**
	 * Legacy algorithm ported from libssl.
	 *
	 * Creates the check codes used to check that user
	 * has provided correct password.
	 *
	 * @param master_password The password from which the check codes are created.
	 * @return OpStatus::OK, OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS CreateLibsslPasswordCheckcode(const char *master_password);

	OP_STATUS StoreCompleteMasterPassword(const ObfuscatedPassword *complete_master_password);

	/**
	 * Store password check codes
	 *
	 * Takes over check_code and salt.
	 *
	 * @param check_code         The check code to store.
	 * @param check_code_length  The length of the check code.
	 * @param salt				 The salt.
	 * @param salt_length		 Length of the salt.
	 * @return opStatus::OK or OpStatus::ERR_NO_MEMORY
	 */
	OP_STATUS StorePasswordCheckCode(UINT8 *check_code, int check_code_length, UINT8 *salt, int salt_length);

	void ClearPasswordData();

	OpWindow *m_dialogule_origin_window;
	OpVector<MasterPasswordRetrivedCallback> m_password_retrieved_listeners;
	ObfuscatedPassword *m_obfuscated_complete_master_password;

	Str::LocaleString  m_current_callback_title;
	Str::LocaleString  m_current_callback_message;
	PasswordDialogMode m_current_callback_mode;
	PasswordRequestReason m_current_callback_reason;

	BOOL m_use_password_aging;
#ifndef _NATIVE_SSL_SUPPORT_
	void NullifyPasswordCheckCodeData();
	OP_STATUS FetchPasswordCheckCodeFromPrefs();
	// Old libssl master password check codes
	UINT8 m_system_part_password_salt[8]; /* ARRAY OK 2012-01-19 haavardm */
	UINT8 *m_system_part_password;
	int m_system_part_password_length;
#endif // _NATIVE_SSL_SUPPORT_
};

#endif // CRYPTO_MASTER_PASSWORD_SUPPORT
#endif /* CRYPTOMASTERPASSWORDHANDLER_H_ */
