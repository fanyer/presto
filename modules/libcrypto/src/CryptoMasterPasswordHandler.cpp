/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 **
 ** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
 **
 ** This file is part of the Opera web browser.  It may not be distributed
 ** under any circumstances.
 **
 */

#include "core/pch.h"

#ifdef CRYPTO_MASTER_PASSWORD_SUPPORT
#include "modules/libcrypto/include/CryptoMasterPasswordEncryption.h"
#include "modules/libcrypto/include/CryptoMasterPasswordHandler.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"
#include "modules/windowcommander/src/WindowCommander.h"
#include "modules/dochand/win.h"
#include "modules/windowcommander/OpWindowCommanderManager.h"
#include "modules/dochand/winman.h"
#include "modules/libcrypto/include/CryptoBlob.h"

#ifdef _NATIVE_SSL_SUPPORT_
// Used to store the master password check codes in libssl files.
// This is done for backward compatibility.
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/libssl_module.h"
#endif // _NATIVE_SSL_SUPPORT_

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/wand/wandmanager.h"

// password used for ubfuscation. Must be 16 bytes long.
#define OBFUSCATING_PASSWORD {0x010, 0x32, 0x36, 0xda, 0xdd, 0x1a, 0x22, 0x19, 0xfa, 0x3f, 0x6a, 0xb1, 0x2a, 0x31, 0x81, 0xcd}

#define SSL_PASSWORD_VERIFICATION       "Opera SSL Password Verification"
#define MINIMUM_PASSWORD_LIFETIME		20

ObfuscatedPassword *ObfuscatedPassword::Create(const UINT8 *password, int length, BOOL use_password_aging, UINT password_lifetime)
{
	OpAutoPtr<ObfuscatedPassword> obfuscated_master_password(OP_NEW(ObfuscatedPassword, ()));

	const UINT8 key[] = OBFUSCATING_PASSWORD; /* ARRAY OK 2012-01-20 haavardm*/
	OpAutoPtr<CryptoBlobEncryption> obfuscator(CryptoBlobEncryption::Create(ARRAY_SIZE(key)));

	if (obfuscator.get())
	{
		RETURN_VALUE_IF_ERROR(obfuscator->SetKey(key), NULL);
		RETURN_VALUE_IF_ERROR(obfuscator->SetFixedSalt(key), NULL);
		RETURN_VALUE_IF_ERROR(obfuscator->Encrypt(password, length, obfuscated_master_password->m_obfuscated_master_password_blob, obfuscated_master_password->m_obfuscated_password_blob_length), NULL);

		if (use_password_aging)
		{
			obfuscated_master_password->m_last_verified_time = g_timecache->CurrentTime();
			obfuscated_master_password->m_password_lifetime = password_lifetime;
		}
		RETURN_VALUE_IF_ERROR(g_pcnet->RegisterListener(obfuscated_master_password.get()),NULL);
		return obfuscated_master_password.release();
	}
	return NULL;
}

ObfuscatedPassword *ObfuscatedPassword::CreateCopy(const ObfuscatedPassword *original)
{
	ObfuscatedPassword *copy;
	int length;
	UINT8 *original_password = original->GetMasterPassword(length);
	RETURN_VALUE_IF_NULL(original_password, NULL);

	copy = ObfuscatedPassword::Create(original_password, length);
	copy->m_last_verified_time = original->m_last_verified_time;
	copy->m_password_lifetime = original->m_password_lifetime;

	op_memset(original_password, length, 0);
	OP_DELETEA(original_password);
	return copy;
}

BOOL ObfuscatedPassword::CheckAging() const
{
	if (!m_last_verified_time) // If m_last_verified_time == 0, we are not using password aging
		return TRUE;

	int lifetime;

	lifetime = MAX(m_password_lifetime, MINIMUM_PASSWORD_LIFETIME);

	if (m_last_verified_time + lifetime >= g_timecache->CurrentTime())
		return TRUE;

	return FALSE;
}

void ObfuscatedPassword::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (pref != PrefsCollectionNetwork::SecurityPasswordLifeTime || newvalue < 0)
		return;

	m_password_lifetime = newvalue * 60;
}

ObfuscatedPassword::~ObfuscatedPassword()
{
	OP_DELETEA(m_obfuscated_master_password_blob);
	if (g_pcnet)
		g_pcnet->UnregisterListener(this);
}

UINT8 *ObfuscatedPassword::GetMasterPassword(int &length) const
{
	length = 0;

	if (!CheckAging())
		return NULL;

	const UINT8 key[] = OBFUSCATING_PASSWORD; /* ARRAY OK 2012-01-20 haavardm*/
	OpAutoPtr<CryptoBlobEncryption> obfuscator(CryptoBlobEncryption::Create(ARRAY_SIZE(key)));
	OP_STATUS status;
	if (obfuscator.get())
	{
		status = obfuscator->SetKey(key);
		if (OpStatus::IsSuccess(status))
		{
			UINT8 *deobfuscated_password = NULL;
			status = obfuscator->Decrypt(m_obfuscated_master_password_blob, m_obfuscated_password_blob_length, deobfuscated_password, length);
			if (OpStatus::IsSuccess(status))
			{
				if (m_last_verified_time) // Password aging is only used if m_last_verified_time != 0. If so, update password aging.
					m_last_verified_time = g_timecache->CurrentTime();

				return deobfuscated_password;
			}
		}
	}

	return NULL;
}

ObfuscatedPassword::ObfuscatedPassword()
	: m_obfuscated_master_password_blob(NULL)
	, m_obfuscated_password_blob_length(0)
	, m_last_verified_time(0)
	, m_password_lifetime(0)
{
}

CryptoMasterPasswordHandler::CryptoMasterPasswordHandler()
	: m_dialogule_origin_window(NULL)
	, m_obfuscated_complete_master_password(NULL)
	, m_current_callback_title(Str::NOT_A_STRING)
	, m_current_callback_message(Str::NOT_A_STRING)
	, m_current_callback_mode(SSLSecurityPasswordCallback::JustPassword)
	, m_current_callback_reason(WandEncryption)
	, m_use_password_aging(FALSE)
{
#ifndef	_NATIVE_SSL_SUPPORT_
	NullifyPasswordCheckCodeData();
#endif // _NATIVE_SSL_SUPPORT_
}

void CryptoMasterPasswordHandler::InitL(BOOL use_password_aging)
{
	m_use_password_aging = use_password_aging;
#ifndef _NATIVE_SSL_SUPPORT_
	// When native ssl is not used, the master password check codes are stored in preferences.
	// If this fails, something is wrong with the pref, and it is removed.
	OpStatus::Ignore(FetchPasswordCheckCodeFromPrefs());
#endif // !_NATIVE_SSL_SUPPORT_
}

OP_STATUS CryptoMasterPasswordHandler::RetrieveMasterPassword(MasterPasswordRetrivedCallback *callback, OpWindow *window, PasswordRequestReason reason)
{
	if (m_obfuscated_complete_master_password && !m_obfuscated_complete_master_password->CheckAging())
	{
		OP_DELETE(m_obfuscated_complete_master_password);
		m_obfuscated_complete_master_password = NULL;
	}

	if (m_obfuscated_complete_master_password)
		return OpStatus::OK;

	RETURN_IF_ERROR(m_password_retrieved_listeners.Add(callback));

	// signal to window commander to ask for password, if we have not done it already
	if (m_password_retrieved_listeners.GetCount() <= 1)
	{
		OP_DELETE(m_obfuscated_complete_master_password);
		m_obfuscated_complete_master_password = NULL;

		if (HasMasterPassword())
			m_current_callback_mode = JustPassword;
		else
			m_current_callback_mode = NewPassword;

		m_current_callback_reason = reason;

		WindowCommander *wic = GetWindowCommander(window);
		OpSSLListener *ssl_listener = wic ? wic->GetSSLListener() : g_windowCommanderManager->GetSSLListener();

		// wic == NULL is correct here when using g_windowCommanderManager
		ssl_listener->OnSecurityPasswordNeeded(wic, this);
	}
	return OpStatus::ERR_YIELD;
}

const ObfuscatedPassword *CryptoMasterPasswordHandler::GetRetrievedMasterPassword()
{
	return m_obfuscated_complete_master_password;
}

void CryptoMasterPasswordHandler::ForgetRetrievedMasterPassword()
{
	OP_DELETE(m_obfuscated_complete_master_password);
	m_obfuscated_complete_master_password = NULL;
}

void CryptoMasterPasswordHandler::CancelGettingPassword(MasterPasswordRetrivedCallback *callback)
{
	if (callback)
	{
		for (;m_password_retrieved_listeners.Find(callback) != -1; m_password_retrieved_listeners.RemoveByItem(callback))
			callback->OnMasterPasswordRetrieved(PASSWORD_RETRIVED_CANCEL);
	}
}

CryptoMasterPasswordHandler::~CryptoMasterPasswordHandler()
{
	OP_DELETE(m_obfuscated_complete_master_password);
#ifndef _NATIVE_SSL_SUPPORT_
	OP_DELETEA(m_system_part_password);
#endif // _NATIVE_SSL_SUPPORT_
}

BOOL CryptoMasterPasswordHandler::HasMasterPassword()
{
#ifdef _NATIVE_SSL_SUPPORT_
	return g_securityManager->SystemPartPassword.GetDirect() != NULL;
#else
	return m_system_part_password != NULL;
#endif // _NATIVE_SSL_SUPPORT_
}

OP_STATUS CryptoMasterPasswordHandler::SetMasterPassword(const char *password)
{
	if (HasMasterPassword() || !password)
		return OpStatus::ERR; // Must call changePassword instead

	RETURN_IF_ERROR(CheckPasswordPolicy(password));

	RETURN_IF_ERROR(CreateLibsslPasswordCheckcode(password));

	OpAutoPtr<ObfuscatedPassword> complete_password(CreateCompletePassword(password));

	if (!complete_password.get())
		return OpStatus::ERR_NO_MEMORY;


#ifdef _NATIVE_SSL_SUPPORT_
	// When master password is used, client certificates are encrypted
	g_securityManager->ReEncryptDataBase(NULL, complete_password.get());
#endif // _NATIVE_SSL_SUPPORT_

	return StoreCompleteMasterPassword(complete_password.get());
}

OP_STATUS CryptoMasterPasswordHandler::ChangeMasterPassword(const char *old_password, const char *new_password)
{
	if (!old_password || !new_password)
		return OpStatus::OK;

	RETURN_IF_ERROR(CheckPasswordPolicy(new_password));
	RETURN_IF_ERROR(CheckLibsslPasswordCheckcode(old_password));

	OpAutoPtr<ObfuscatedPassword> old_complete_password(CreateCompletePassword(old_password));

	RETURN_IF_ERROR(CreateLibsslPasswordCheckcode(new_password));

	OpAutoPtr<ObfuscatedPassword> new_complete_password(CreateCompletePassword(new_password));

	if (!new_complete_password.get())
		return OpStatus::ERR_NO_MEMORY;

#ifdef WAND_SUPPORT
	// Re-encrypt databases
	if (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword))
		g_wand_manager->ReEncryptDatabase(old_complete_password.get(), new_complete_password.get());
#endif // WAND_SUPPORT

#ifdef _NATIVE_SSL_SUPPORT_
	g_securityManager->ReEncryptDataBase(old_complete_password.get(), new_complete_password.get());
#endif // _NATIVE_SSL_SUPPORT_

	return StoreCompleteMasterPassword(new_complete_password.get());
}

void CryptoMasterPasswordHandler::OnSecurityPasswordDone(BOOL ok, const char *old_password, const char *new_password)
{
	OpVector<MasterPasswordRetrivedCallback> password_retrieved_listeners;
	password_retrieved_listeners.DuplicateOf(m_password_retrieved_listeners);

	m_password_retrieved_listeners.Clear();

	if (!ok)
		return SignalPasswordRetrievedToListeners(PASSWORD_RETRIVED_CANCEL, password_retrieved_listeners);

	switch (GetMode())
	{
		case JustPassword:
		{
			PasswordState password_state = PASSWORD_RETRIVED_SUCCESS;

			if (OpStatus::IsError(CheckLibsslPasswordCheckcode(old_password)))
				password_state = PASSWORD_RETRIVED_WRONG_PASSWORD;
			else
			{
				OpAutoPtr<ObfuscatedPassword> complete_password(CreateCompletePassword(old_password));

				if (!complete_password.get() || OpStatus::IsError(StoreCompleteMasterPassword(complete_password.get())))
					password_state = PASSWORD_RETRIVED_CANCEL; // In case of memory problems we simply cancel
			}

			SignalPasswordRetrievedToListeners(password_state, password_retrieved_listeners);
			break;
		}

		case NewPassword:
		{
			OP_ASSERT((old_password == NULL || *old_password == '\0') && "old password given when setting password first time");

			OP_ASSERT(!HasMasterPassword() && "Cannot set new password when an old password already exist. Use change password.");

			PasswordState password_state = PASSWORD_RETRIVED_SUCCESS;
			if (OpStatus::IsError(SetMasterPassword(new_password)))
				password_state = PASSWORD_RETRIVED_CANCEL;

			SignalPasswordRetrievedToListeners(password_state, password_retrieved_listeners);
			break;
		}

		default:
		OP_ASSERT(!"UKNOWN MODE");
		break;
	}
}

void CryptoMasterPasswordHandler::SignalPasswordRetrievedToListeners(CryptoMasterPasswordHandler::PasswordState state, OpVector<MasterPasswordRetrivedCallback> &listeners)
{
	unsigned count = listeners.GetCount();
	for (unsigned idx = 0; idx < count; idx++)
		listeners.Get(idx)->OnMasterPasswordRetrieved(state);
}

WindowCommander* CryptoMasterPasswordHandler::GetWindowCommander(OpWindow *dialogule_origin_window)
{
	if (m_dialogule_origin_window)
	{
		Window* win;

		// Finds matching Window or NULL if no window is matching
		for(win = g_windowManager->FirstWindow(); win && win->GetOpWindow() != dialogule_origin_window ; win = win->Suc()) {}

		if (win)
			return win->GetWindowCommander();
	}

	return NULL;
}


unsigned char *CryptoMasterPasswordHandler::GetMasterPasswordCheckCode(int &length)
{
#ifdef _NATIVE_SSL_SUPPORT_
	length = g_securityManager->SystemPartPassword.GetLength();
	return  g_securityManager->SystemPartPassword.GetDirect();
#else
	length = m_system_part_password_length;
	return  m_system_part_password;
#endif // _NATIVE_SSL_SUPPORT_
}

unsigned char *CryptoMasterPasswordHandler::GetMasterPasswordCheckCodeSalt(int &length)
{
#ifdef _NATIVE_SSL_SUPPORT_
	length = 8;
	OP_ASSERT(g_securityManager->SystemPartPasswordSalt.GetLength() == 8);
	return g_securityManager->SystemPartPasswordSalt.GetDirect();

#else
	length = 8;
	return  m_system_part_password_salt;
#endif // _NATIVE_SSL_SUPPORT_
}

OP_STATUS CryptoMasterPasswordHandler::CreateLibsslPasswordCheckcode(const char *master_password)
{
	CryptoMasterPasswordEncryption master_password_encryption;
	RETURN_IF_LEAVE(master_password_encryption.InitL());

	UINT8 system_part_password_plain[128]; /* ARRAY OK 2012-01-20 haavardm*/

	// We create a random plain text that will be encrypted using salt and master password
	// This will later be decrypted to check if user gives correct master password.
	// Note that we do not have to store this plain text anywhere, since we will detect
	// when decrypting m_system_part_password that the master password given is wrong,
	// without needing to have the original plain text.
	g_libcrypto_random_generator->GetRandom(system_part_password_plain, 128);


	int system_part_password_length;
	UINT8 *system_part_password_salt = OP_NEWA(UINT8, (8));
	ANCHOR_ARRAY(UINT8, system_part_password_salt);

	RETURN_OOM_IF_NULL(system_part_password_salt);


	UINT8 *system_part_password;
	OP_STATUS status = master_password_encryption.EncryptWithPassword(system_part_password, system_part_password_length, system_part_password_plain, 128, system_part_password_salt, (const UINT8*)master_password, op_strlen(master_password), (const UINT8*)SSL_PASSWORD_VERIFICATION, op_strlen(SSL_PASSWORD_VERIFICATION));
	ANCHOR_ARRAY(UINT8, system_part_password);

	RETURN_IF_ERROR(status);

	// Takes over system_part_password and system_part_password_salt
	RETURN_IF_ERROR(status = StorePasswordCheckCode(system_part_password, system_part_password_length, system_part_password_salt, 8));

	// Apart from releasing do not touch system_part_password or system_part_password_salt from here.
	ANCHOR_ARRAY_RELEASE(system_part_password_salt);
	ANCHOR_ARRAY_RELEASE(system_part_password);
	return status;
}

OP_STATUS CryptoMasterPasswordHandler::CheckLibsslPasswordCheckcode(const char *master_password)
{
	CryptoMasterPasswordEncryption master_password_encryption;

	RETURN_IF_LEAVE(master_password_encryption.InitL());

	OP_STATUS status = OpStatus::ERR_NO_ACCESS;

	int system_part_password_length;
	UINT8 *system_part_password = GetMasterPasswordCheckCode(system_part_password_length);

	int system_part_password_salt_length;
	UINT8 *system_part_password_salt = GetMasterPasswordCheckCodeSalt(system_part_password_salt_length);

	if (system_part_password && system_part_password_salt && system_part_password_length > 0 && system_part_password_salt_length > 0)
	{
		// We test a decrypt of the check code (m_system_part_password) using the master password
		int decrypted_part_password_length;
		UINT8 *decrypted_part_password;
		status = master_password_encryption.DecryptWithPassword(decrypted_part_password, decrypted_part_password_length,
				system_part_password, system_part_password_length,
				system_part_password_salt, system_part_password_salt_length,
				(const UINT8*)master_password, op_strlen(master_password),
				(const UINT8*)SSL_PASSWORD_VERIFICATION, op_strlen(SSL_PASSWORD_VERIFICATION));

		OP_DELETEA(decrypted_part_password);
	}

	return status;
}

ObfuscatedPassword* CryptoMasterPasswordHandler::CreateCompletePassword(const char *master_password)
{
	// Port from libssl for backward compatibility. The actual master password used for encryption
	// is the user defined password appended with the check code (part password).

	// Create 'complete password' which is used for the actual password for encryption.
	// The complete password is a concatenation of password and the decryption of the
	// password check code 'm_system_part_password'

	int master_password_length = op_strlen(master_password);
	int system_part_password_length;
	UINT8 *system_part_password = GetMasterPasswordCheckCode(system_part_password_length);
	int system_part_password_salt_length;
	UINT8 *system_part_password_salt = GetMasterPasswordCheckCodeSalt(system_part_password_salt_length);

	CryptoMasterPasswordEncryption master_password_encryption;
	TRAPD(status, master_password_encryption.InitL());
	if (OpStatus::IsError(status))
		return NULL;

	int decrypted_part_password_length;
	UINT8 *decrypted_part_password;
	RETURN_VALUE_IF_ERROR(master_password_encryption.DecryptWithPassword(decrypted_part_password, decrypted_part_password_length,
			system_part_password, system_part_password_length,
			system_part_password_salt, system_part_password_salt_length,
			(const UINT8*)master_password, op_strlen(master_password),
			(const UINT8*)SSL_PASSWORD_VERIFICATION, op_strlen(SSL_PASSWORD_VERIFICATION)), NULL);

	ANCHOR_ARRAY(UINT8, decrypted_part_password);

	int complete_password_length = master_password_length +  decrypted_part_password_length;

	UINT8 *complete_password = OP_NEWA(UINT8, complete_password_length);
	if (!complete_password)
		return NULL;

	ANCHOR_ARRAY(UINT8, complete_password);

	op_memcpy(complete_password, master_password, master_password_length);
	op_memcpy(complete_password + master_password_length, decrypted_part_password, decrypted_part_password_length);

	return ObfuscatedPassword::Create(complete_password, complete_password_length, m_use_password_aging, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SecurityPasswordLifeTime) * 60);
}

OP_STATUS CryptoMasterPasswordHandler::StoreCompleteMasterPassword(const ObfuscatedPassword *complete_master_password)
{
	UINT8 *complete_password;
	int complete_password_length;
	complete_password = complete_master_password->GetMasterPassword(complete_password_length);

	if (!complete_password)
		return OpStatus::ERR;

	ANCHOR_ARRAY(UINT8, complete_password);

#ifdef _NATIVE_SSL_SUPPORT_
	// If native SSL is used we store the complete password, for backward compatibility.
	if (complete_master_password)
	{
		g_securityManager->SystemCompletePassword.Set(complete_password, complete_password_length);

		if(g_securityManager->SystemCompletePassword.Valid())
		{
			g_securityManager->SystemPasswordVerifiedLast = g_timecache->CurrentTime();
			g_securityManager->Obfuscate();
			g_securityManager->CheckPasswordAging();
		}
		else
			g_securityManager->SystemCompletePassword.Resize(0);


		g_securityManager->Save();
	}
#endif // _NATIVE_SSL_SUPPORT_

	op_memset(complete_password, 0, complete_password_length);

	if (!complete_master_password)
		return OpStatus::ERR_NO_MEMORY;

	OP_DELETE(m_obfuscated_complete_master_password);
	m_obfuscated_complete_master_password = ObfuscatedPassword::CreateCopy(complete_master_password);

	if (!m_obfuscated_complete_master_password)
		return OpStatus::ERR_NO_MEMORY;

	return OpStatus::OK;
}

OP_STATUS CryptoMasterPasswordHandler::StorePasswordCheckCode(UINT8 *check_code, int check_code_length, UINT8 *salt, int salt_length)
{
#ifdef _NATIVE_SSL_SUPPORT_
	// When using native ssl we store the check codes in libssl to be backward compatible with stored master password
	g_securityManager->SystemPartPassword.SetExternalPayload(check_code, TRUE, check_code_length);
	g_securityManager->SystemPartPasswordSalt.SetExternalPayload(salt, TRUE, salt_length);
#else

	// We store this in preferences.
	OpString8 check_code_base64;

	RETURN_IF_ERROR(CryptoUtility::ConvertToBase64(check_code, check_code_length, check_code_base64));

	OpString8 salt_base64;
	RETURN_IF_ERROR(CryptoUtility::ConvertToBase64(salt, salt_length, salt_base64));

	OpString8 pref_string8;
	RETURN_IF_ERROR(pref_string8.AppendFormat("%s %s", check_code_base64.CStr(), salt_base64.CStr()));
	OpString pref_string;

	RETURN_IF_ERROR(pref_string.SetFromUTF8(pref_string8.CStr()));

	RETURN_IF_LEAVE(g_pcnet->WriteStringL(PrefsCollectionNetwork::MasterPasswordCheckCodeAndSalt, pref_string.CStr()));

	OP_DELETEA(m_system_part_password);
	m_system_part_password = check_code;
	m_system_part_password_length = check_code_length;
	op_memcpy(m_system_part_password_salt, salt, salt_length);
	OP_DELETEA(salt);
#endif // _NATIVE_SSL_SUPPORT_

	return OpStatus::OK;
}

OP_STATUS CryptoMasterPasswordHandler::CheckPasswordPolicy(const char *master_password)
{
	// Password policy ported from libssl. Password must be at least 6 characters,
	// and have at least one alphabetic character, and at least  one non-alphabetic character.

	OpString master_password_utf16;

	// Convert from UTF8 to UTF-16
	RETURN_IF_ERROR(master_password_utf16.SetFromUTF8(master_password));

	int password_length = master_password_utf16.Length();
	if(password_length < 6)
	{
		master_password_utf16.Wipe();
		return OpStatus::ERR;
	}

	// if ascii only, we require both alpha and non-alpha characters.
	BOOL has_alpha_ascii = FALSE;
	BOOL has_other_ascii = FALSE;

	// For non ascii (say chinese) we cannot require ascii characters.
	BOOL has_none_ascii = FALSE;

	for (int i = 0; i < password_length; i++)
	{
		uni_char token = master_password_utf16[i];
		if (uni_isalpha(token))
			has_alpha_ascii = TRUE;
		else if (token > 255)
			has_none_ascii = TRUE;
		else
			has_other_ascii = TRUE;
	}

	master_password_utf16.Wipe();

	if ((has_alpha_ascii && has_other_ascii) || has_none_ascii)
		return OpStatus::OK;

	return OpStatus::ERR;
}

void CryptoMasterPasswordHandler::ClearPasswordData()
{
	OP_DELETE(m_obfuscated_complete_master_password);
	m_obfuscated_complete_master_password = NULL;

#ifdef _NATIVE_SSL_SUPPORT_
	g_securityManager->SystemPartPassword.Resize(0);
	g_securityManager->SystemPartPasswordSalt.Resize(0);
	g_securityManager->SystemCompletePassword.Resize(0);
#else
	OP_DELETEA(m_system_part_password);
	m_system_part_password = NULL;

#endif // _NATIVE_SSL_SUPPORT_
}

#ifndef _NATIVE_SSL_SUPPORT_
void CryptoMasterPasswordHandler::NullifyPasswordCheckCodeData()
{
	m_system_part_password = NULL;
	m_system_part_password_length = 0;
	op_memset(m_system_part_password_salt, 0, sizeof(m_system_part_password_salt));
}

OP_STATUS CryptoMasterPasswordHandler::FetchPasswordCheckCodeFromPrefs()
{
	OP_DELETEA(m_system_part_password);
	NullifyPasswordCheckCodeData();

	OpString pref_string;
	RETURN_IF_LEAVE(g_pcnet->GetStringPrefL(PrefsCollectionNetwork::MasterPasswordCheckCodeAndSalt, pref_string));
	OpString8 pref_string8;
	RETURN_IF_ERROR(pref_string8.SetUTF8FromUTF16(pref_string.CStr()));

	int split_position = pref_string8.FindFirstOf(' ');

	if (split_position == KNotFound)
		return OpStatus::ERR;

	OpString8 salt_base64;
	RETURN_IF_ERROR(salt_base64.Set(pref_string8.SubString(split_position + 1)));
	pref_string8[split_position] = '\0';

	OP_STATUS status = CryptoUtility::ConvertFromBase64(
		pref_string8.CStr(), m_system_part_password, m_system_part_password_length);
	if (OpStatus::IsError(status))
	{
		// m_system_part_password and m_system_part_password_length
		// may contain garbage.
		NullifyPasswordCheckCodeData();
		return status;
	}

	UINT8 *salt = NULL;
	int salt_length;
	status = CryptoUtility::ConvertFromBase64(salt_base64.CStr(), salt, salt_length);
	ANCHOR_ARRAY(UINT8, salt);

	if (OpStatus::IsError(status) || salt_length != 8)
	{
		OP_DELETEA(m_system_part_password);
		m_system_part_password = NULL;
		RETURN_IF_ERROR(status);
		return OpStatus::ERR;
	}

	op_memcpy(m_system_part_password_salt, salt, 8);
	return OpStatus::OK;
}
#endif // !_NATIVE_SSL_SUPPORT_
#endif // CRYPTO_MASTER_PASSWORD_SUPPORT
