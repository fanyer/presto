/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)

#ifdef LIBSSL_SECURITY_PASSWD

#include "modules/libssl/sslbase.h"
#include "modules/libssl/options/sslopt.h"
#include "modules/libssl/sslrand.h"
#include "modules/libssl/methods/sslpubkey.h"
#include "modules/libssl/methods/sslhash.h"
#include "modules/libssl/methods/sslphash.h"

#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/prefs/prefsmanager/prefsmanager.h"

#include "modules/libssl/askpasswd.h"


#ifdef _DEBUG
#ifdef YNP_WORK
//#define SSL_TST_DUMP
#endif
#endif

#ifdef WAND_SUPPORT
#include "modules/wand/wandmanager.h"
#endif
#include "modules/libssl/options/passwd_internal.h"
#include "modules/libcrypto/include/CryptoMasterPasswordEncryption.h"

OP_STATUS PreliminaryStoreExternalItem(int i, SSL_varvector32 &enc_out, SSL_varvector32 &enc_out_salt);
BOOL RetrieveExternalItem(int i, SSL_varvector32 &enc_in, SSL_varvector32 &enc_in_salt);

static const char SSL_PASSWORD_VERIFICATION[] = "Opera SSL Password Verification";
static const char SSL_PRIVATE_KEY_VERIFICATION[] = "Opera SSL Private Key Verification";
static const char SSL_EMAIL_PASSWORD_VERIFICATION[] = "Opera Email Password Verification";

const char *GetPrivateKeyverificationString()
{
	return SSL_PRIVATE_KEY_VERIFICATION;
}

OP_STATUS SSL_Options::DecryptPrivateKey(int cert_number, SSL_secure_varvector32 &decryptedkey, SSL_dialog_config &config)
{
	int status;

	decryptedkey.Resize(0);

	SSL_CertificateItem *cert_item = Find_Certificate(SSL_ClientStore, cert_number);
	if(cert_item == NULL)
		return OpStatus::OK;

	RETURN_IF_ERROR(GetPassword(config));

	status = DecryptWithPassword(cert_item->private_key, cert_item->private_key_salt, decryptedkey, SystemCompletePassword,
		SSL_PRIVATE_KEY_VERIFICATION);

	CheckPasswordAging();

	if(status == -2 || status == -1)
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

OP_STATUS SSL_Options::DecryptPrivateKey(SSL_varvector32 &encryptedkey, SSL_varvector32 &encryptedkey_salt, SSL_secure_varvector32 &decryptedkey, SSL_PublicKeyCipher *key, SSL_dialog_config &config)
{
	int status;

	if(key == NULL)
		return OpStatus::OK;

	RETURN_IF_ERROR(GetPassword(config));

	status = DecryptWithPassword(encryptedkey, encryptedkey_salt, decryptedkey, SystemCompletePassword,
		SSL_PRIVATE_KEY_VERIFICATION);

	CheckPasswordAging();

	if(status == -2 || status == -1)
	{  // No retry
		key->RaiseAlert(SSL_Internal, SSL_InternalError);
		return key->GetOPStatus();
	}

	key->LoadAllKeys(decryptedkey);
	return OpStatus::OK;
}

#if defined (API_LIBSSL_DECRYPT_WITH_SECURITY_PASSWD)
OP_STATUS SSL_Options::DecryptData(const SSL_varvector32 &encrypteddata,
							  const SSL_varvector32 &encrypteddata_salt,
							  SSL_secure_varvector32 &decrypted_data,
							  const char* pwd,
							  SSL_dialog_config &config)
{
	SSL_secure_varvector32 password,fullpassword,decryptedkey,PartPassword;
	SSL_Hash_Pointer sha(SSL_SHA);
	int status;

	Init(SSL_LOAD_CLIENT_STORE);

	if (pwd)
	{
		SSL_secure_varvector32 pass;

		pass.ForwardTo(&decrypted_data);
		pass.Set(pwd);
		if(pass.ErrorRaisedFlag)
			return pass.GetOPStatus();

		status = DecryptWithPassword(encrypteddata, encrypteddata_salt, decrypted_data, pass,
			SSL_EMAIL_PASSWORD_VERIFICATION);

		if(status<0)
			return OpStatus::ERR;

		return OpStatus::OK;
	}

	RETURN_IF_ERROR(GetPassword(config));

	status = DecryptWithPassword(encrypteddata, encrypteddata_salt, decrypted_data, SystemCompletePassword,
		SSL_EMAIL_PASSWORD_VERIFICATION);

	CheckPasswordAging();

	if(status<0)
	{
		if(!decrypted_data.Error())
		decrypted_data.RaiseAlert(SSL_Internal, SSL_InternalError);
		return decrypted_data.GetOPStatus();
	}

	return OpStatus::OK;
}
#endif

int SSL_Options::DecryptWithPassword(const SSL_varvector32 &encrypted, const SSL_varvector32 &encrypted_salt,
									 SSL_secure_varvector32 &decrypted, const SSL_secure_varvector32 &password, const char *plainseed)
{
	OP_STATUS status = OpStatus::OK;
	UINT8 *decrypted_array;
	int decrypted_array_length;
	status = g_libcrypto_master_password_encryption->DecryptWithPassword(decrypted_array, decrypted_array_length, encrypted.GetDirect(), encrypted.GetLength(),
			encrypted_salt.GetDirect(), encrypted_salt.GetLength(),
			password.GetDirect(), password.GetLength(),
			(const UINT8*)plainseed, op_strlen(plainseed));

	decrypted.SetExternalPayload(decrypted_array, TRUE, decrypted_array_length);


	if (OpStatus::IsSuccess(status))
		return 0;

	return -1;
}

void SSL_Options::EncryptWithPassword(const SSL_secure_varvector32 &plain,
									  SSL_varvector32 &encrypted, SSL_varvector32 &encrypted_salt,
									  const SSL_secure_varvector32 &password, const char *plainseed_text)
{

	encrypted_salt.Resize(8);
	if (encrypted_salt.GetLength() != 8)
		return encrypted_salt.RaiseAlert(OpStatus::ERR_NO_MEMORY);

	int encrypted_array_length = 0;
	UINT8 *encrypted_array;
	OP_STATUS status = g_libcrypto_master_password_encryption->EncryptWithPassword(encrypted_array, encrypted_array_length, plain.GetDirect(), plain.GetLength(),
			encrypted_salt.GetDirect(),
			password.GetDirect(), password.GetLength(),
			(const UINT8*)plainseed_text, op_strlen(plainseed_text));

	if(OpStatus::IsError(status))
		encrypted.RaiseAlert(status);

	encrypted.SetExternalPayload(encrypted_array, TRUE, encrypted_array_length);

	return;
}

#if defined (API_LIBSSL_DECRYPT_WITH_SECURITY_PASSWD)
OP_STATUS SSL_Options::EncryptData(SSL_secure_varvector32 &plain_data,
							  SSL_varvector32 &encrypteddata,
							  SSL_varvector32 &encrypteddata_salt,
							  const char* pwd,
							  SSL_dialog_config &config)
{
	SSL_secure_varvector32 password,fullpassword,decryptedkey,PartPassword;
	SSL_Hash_Pointer sha(SSL_SHA);

    Init(SSL_LOAD_CLIENT_STORE);

	if (pwd)
	{
		SSL_secure_varvector32 pass;

		int len = op_strlen(pwd);
		pass.Resize(len + 1);
		op_strcpy((char*)pass.GetDirect(), pwd);
		pass.Resize(len);

		EncryptWithPassword(plain_data,encrypteddata, encrypteddata_salt, pass,
			SSL_EMAIL_PASSWORD_VERIFICATION);

		return OpStatus::OK;
	}

	RETURN_IF_ERROR(GetPassword(config));

	EncryptWithPassword(plain_data,encrypteddata, encrypteddata_salt, SystemCompletePassword,
			SSL_EMAIL_PASSWORD_VERIFICATION);

	CheckPasswordAging();

	return OpStatus::OK;
}
#endif

OP_STATUS SSL_Options::GetPassword(SSL_dialog_config &config)
{
	if(ask_security_password && ask_security_password->IsFinished())
	{
		OP_DELETE(ask_security_password);
		ask_security_password = NULL;
	}

	if(SystemCompletePassword.GetLength() == 0)
	{
		DeObfuscate(SystemCompletePassword);
	}

    if(SystemCompletePassword.GetLength() == 0)
	{
		SSL_secure_varvector32 password, PartPassword;

		if(config.finished_message == MSG_NO_MESSAGE || ask_security_password)
		{
			if(ask_security_password)
			{
				ask_security_password->AddMessage(config);
			}
			return InstallerStatus::ERR_PASSWORD_NEEDED;
		}

		OpSSLListener::SSLSecurityPasswordCallback::PasswordDialogMode mode =
			OpSSLListener::SSLSecurityPasswordCallback::JustPassword;
		Str::LocaleString titl = Str::DI_IDM_PASSWORD_BOX;
		Str::LocaleString msg = Str::SI_MSG_SECURE_ASK_PASSWORD_CLIENT_CERT;

		if (SystemPartPassword.GetLength() == 0)
		{
			mode = OpSSLListener::SSLSecurityPasswordCallback::NewPassword;
			msg = Str::SI_MSG_SECURE_ASK_FIRST_PASSWORD;
		}

		ask_security_password = OP_NEW(Options_AskPasswordContext, (mode, titl, msg, config, (this != g_securityManager ? this : NULL)));
		if(!ask_security_password)
			return OpStatus::ERR_NO_MEMORY;

		OP_STATUS op_err = ask_security_password->StartDialog();
		if(OpStatus::IsError(op_err))
		{
			OP_DELETE(ask_security_password);
			ask_security_password = NULL;
			return op_err;
		}

		return InstallerStatus::ERR_PASSWORD_NEEDED;
	}

	return OpStatus::OK;
}

void SSL_Options::UseSecurityPassword(const char *passwd)
{
	ClearObfuscated();
	SystemCompletePassword.Resize(0);

	SSL_secure_varvector32 password, PartPassword;
	int status;

	password.Set(passwd);

	g_libcrypto_master_password_handler->SetMasterPassword(passwd);

	if (SystemPartPassword.GetLength() != 0)
	{
		status = DecryptWithPassword(SystemPartPassword, SystemPartPasswordSalt, PartPassword,
			password,SSL_PASSWORD_VERIFICATION);

		if(status < 0)
			return;
	}

	SystemCompletePassword.Concat(password,PartPassword);
	if(!SystemCompletePassword.Valid())
	{
		SystemCompletePassword.Resize(0);
		return;
	}

	SSL_secure_varvector32 empty;
	if (SystemPartPassword.GetLength() == 0)
		ReEncryptDataBase(empty, SystemCompletePassword);

	SystemPasswordVerifiedLast = g_timecache->CurrentTime();

	Obfuscate();
}

void SSL_Options::ClearSystemCompletePassword()
{
	ClearObfuscated();
	SystemCompletePassword.Resize(0);
}

void SSL_Options::CheckPasswordAging()
{
	if(ask_security_password && ask_security_password->IsFinished())
	{
		OP_DELETE(ask_security_password);
		ask_security_password = NULL;
	}

	if(PasswordLockCount > 0 || ask_security_password)
		return;

	SystemCompletePassword.Resize(0);

	if(PasswordAging == SSL_ASK_PASSWD_EVERY_TIME)
		ClearObfuscated();
	else if(PasswordAging == SSL_ASK_PASSWD_AFTER_TIME && SystemPasswordVerifiedLast &&
		SystemPasswordVerifiedLast + g_pcnet->GetIntegerPref(PrefsCollectionNetwork::SecurityPasswordLifeTime) *60 < g_timecache->CurrentTime())
	{
		ClearObfuscated();
		SystemPasswordVerifiedLast = 0;
	}
}

void SSL_Options::StartSecurityPasswordSession()
{
	PasswordLockCount++;
}

void SSL_Options::EndSecurityPasswordSession()
{
	//This could have been set to 0 if user changed Prefs-settings while sessions were active.
	//All other cases are considered as a serious bug!
	OP_ASSERT(PasswordLockCount > 0);

	if (PasswordLockCount > 0)
		PasswordLockCount--;
}

BOOL SSL_Options::CheckPassword(const SSL_secure_varvector32 &password)
{
	SSL_secure_varvector32 partpassword;

	return DecryptWithPassword(SystemPartPassword, SystemPartPasswordSalt, partpassword,
		password,SSL_PASSWORD_VERIFICATION) == 0;

}

BOOL SSL_Options::ReEncryptDataBase(const ObfuscatedPassword *old_complete_password, const ObfuscatedPassword *new_complete_password)
{
 	SSL_secure_varvector32 password, new_password;

 	if (old_complete_password)
 	{
		int old_length;
		UINT8 *old_password_bytes = old_complete_password->GetMasterPassword(old_length);
		if (!old_password_bytes)
			return FALSE;
		ANCHOR_ARRAY(UINT8, old_password_bytes);

		password.Set(old_password_bytes, old_length);
		op_memset(old_password_bytes, 0, old_length);

		if (password.ErrorRaisedFlag)
			return FALSE;
 	}

	int new_length;
	UINT8 *new_password_bytes = new_complete_password->GetMasterPassword(new_length);
	if (!new_password_bytes)
		return FALSE;
	ANCHOR_ARRAY(UINT8, new_password_bytes);

 	new_password.Set(new_password_bytes, new_length);
 	op_memset(new_password_bytes, 0, new_length);

 	if (new_password.ErrorRaisedFlag)
 		return FALSE;

 	return ReEncryptDataBase(password, new_password);
}

BOOL CheckPasswordPolicy(const SSL_secure_varvector32 &password_8)
{
	OpString password;

	// Convert from UTF8 to UTF-16
	OpStatus::Ignore(password.SetFromUTF8((const char *) password_8.GetDirect(), password_8.GetLength()));
	// Ignore error, always results in length less than 6 and is handled below

	uint32 len = password.Length();
	if(len < 6)
	{
		password.Wipe();
		return FALSE;
	}

	BOOL has_alpha = FALSE;
	BOOL has_other = FALSE;
	uint32 i;
	uni_char token=0;
	const uni_char *pw = password.CStr();

	for(i=0; i< len; i++)
	{
		token = pw[i];
		
		BOOL is_alpha = op_isalpha(token);
		
		if(!has_alpha && is_alpha)
			has_alpha = TRUE;
		else if(!has_other && !is_alpha)
			has_other = TRUE;

		if((has_alpha && has_other) || token > 255 /* non-latin1 character */)
		{
			token = 0;
			pw = NULL;
			password.Wipe();
			return TRUE;
		}
	}

	password.Wipe();
	token = 0;
	pw = NULL;

	return FALSE;
}

BOOL SSL_Options::ReEncryptDataBase(SSL_secure_varvector32 &old_complete_password, SSL_secure_varvector32 &new_complete_password)
{
	SSL_secure_varvector32 plainkey;
	SSL_CertificateItem *item;
	BOOL status = FALSE;
	if(!g_ssl_api->CheckSecurityManager())
		return FALSE;

	Init(SSL_LOAD_CLIENT_STORE);

	if(!loaded_usercerts)
		return FALSE;


	if (System_ClientCerts.Cardinal() > 0)
	{
		status = TRUE;
		item = System_ClientCerts.First();
		while(status && item != NULL)
		{

			if (item->private_key_salt.GetLength() > 0)
			{
				// The certificates was previously encrypted
				status = status && DecryptWithPassword(item->private_key, item->private_key_salt, plainkey,
						old_complete_password, SSL_PRIVATE_KEY_VERIFICATION) == 0;

				if(status && new_complete_password.GetLength() > 0)
				{
					EncryptWithPassword(plainkey, item->private_key, item->private_key_salt,
							new_complete_password, SSL_PRIVATE_KEY_VERIFICATION);
				}
				else
				{
					item->private_key.Set(plainkey);
					item->private_key_salt.Resize(0);
				}
			}
			else
			{
				// The certificates was previously not encrypted
				if (new_complete_password.GetLength() > 0)
				{
					plainkey.Set(item->private_key);

					status = OpStatus::IsSuccess(plainkey.GetOPStatus());

					if(status)
					{
						EncryptWithPassword(plainkey, item->private_key, item->private_key_salt,
								new_complete_password, SSL_PRIVATE_KEY_VERIFICATION);
					}
				}
			}

			if(register_updates && status)
				item->cert_status = Cert_Updated;

			item = item->Suc();
		}
	}
	else
		status = TRUE;

	BOOL set_update_external_items = FALSE;

	if(status && old_complete_password.GetLength() > 0)
	{
#if defined WAND_SUPPORT && defined API_LIBSSL_DECRYPT_WITH_SECURITY_PASSWD
        BOOL paranoid_M2_protection = (BOOL) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword);

        if (status && paranoid_M2_protection!=0) //Only change password if master-password is used
        {
			int i = 0;
			g_wand_manager->DestroyPreliminaryDataItems();

			SSL_secure_varvector32 enc_data_in, enc_data_salt_in;
			SSL_secure_varvector32 plain_data;
			SSL_secure_varvector32 enc_data_out, enc_data_salt_out;
			while(status && RetrieveExternalItem(i, enc_data_in, enc_data_salt_in))
			{
				status = status &&  (DecryptWithPassword(enc_data_in, enc_data_salt_in, plain_data,
						old_complete_password, SSL_EMAIL_PASSWORD_VERIFICATION) == 0);
				if(status)
				{
					EncryptWithPassword(plain_data,enc_data_out, enc_data_salt_out,
							new_complete_password, SSL_EMAIL_PASSWORD_VERIFICATION);
					OpStatus::Ignore(PreliminaryStoreExternalItem(i, enc_data_out, enc_data_salt_out));
				}
				i++;
			}
			if(status)
				set_update_external_items = TRUE;

		}
#endif // WAND_SUPPORT && API_LIBSSL_DECRYPT_WITH_SECURITY_PASSWD

		if(status)
			SystemCompletePassword =  new_complete_password;

		SystemPasswordVerifiedLast = g_timecache->CurrentTime();
		Obfuscate();
		CheckPasswordAging();
	}
	else
		status = TRUE;

	if(status)
	{

		if(status)
		{
			usercert_base_version = SSL_Options_Version;
			updated_external_items = set_update_external_items;

			if(!register_updates)
				Save();
			else
				updated_password = TRUE;
			// mess->ShowOk(0,SSL_MSG_CHANGE_PASS_WORD_OK);

#ifdef WAND_SUPPORT
			g_wand_manager->UpdateSecurityStateWithoutWindow();
#endif // WAND_SUPPORT
		}
        else
        {
#ifdef WAND_SUPPORT
			g_wand_manager->DestroyPreliminaryDataItems();
#endif // WAND_SUPPORT
        }
	}

	return status;
}

#if defined API_LIBSSL_DECRYPT_WITH_SECURITY_PASSWD
BOOL RetrieveExternalItem(int i, SSL_varvector32 &enc_in, SSL_varvector32 &enc_in_salt)
{
#ifdef WAND_SUPPORT
	const unsigned char *dataitem;
	unsigned long data_len;

	const WandPassword* wandpassword = g_wand_manager->RetrieveDataItem(i);
	if (!wandpassword)
		return FALSE;

	data_len = wandpassword->GetLength();
	dataitem = wandpassword->GetDirect();

	if(dataitem == NULL|| data_len == 0)
		return FALSE;

	DataStream_SourceBuffer data(const_cast<unsigned char *>(dataitem), data_len);

	OP_MEMORY_VAR OP_STATUS op_err = OpRecStatus::OK;
	OP_STATUS op_err1;
	TRAP(op_err1, op_err = enc_in_salt.ReadRecordFromStreamL(&data));
	if(op_err != OpRecStatus::FINISHED)
		return FALSE;
	TRAP(op_err1, op_err = enc_in.ReadRecordFromStreamL(&data));
	if(op_err != OpRecStatus::FINISHED)
		return FALSE;

	return enc_in_salt.Valid() && enc_in.Valid();
#else 
	return FALSE;
#endif
}

OP_STATUS PreliminaryStoreExternalItem(int i, SSL_varvector32 &enc_out, SSL_varvector32 &enc_out_salt)
{
#ifdef WAND_SUPPORT
	SSL_secure_varvector32 temp;

	RETURN_IF_LEAVE(enc_out_salt.WriteRecordL(&temp));
	RETURN_IF_LEAVE(enc_out.WriteRecordL(&temp));

	g_wand_manager->PreliminaryStoreDataItem(i, temp.GetDirect(), temp.GetLength());
#endif
	return OpStatus::OK;
}

unsigned char *SSL_API::DecryptWithSecurityPassword(OP_STATUS &op_err,
					const unsigned char *in_data, unsigned long in_len, unsigned long &out_len,
					const char* password, SSL_dialog_config &config)
{

	op_err = OpRecStatus::OK;
	out_len = 0;
	if(in_data == NULL || in_len == 0)
		return NULL;

	SSL_secure_varvector32 plain_out, enc_in, enc_in_salt;

	DataStream_SourceBuffer src_data((unsigned char *)in_data, in_len);

	OP_STATUS op_err1;
	TRAP(op_err1, op_err = enc_in_salt.ReadRecordFromStreamL(&src_data));
	if(op_err != OpRecStatus::FINISHED)
		return NULL;
	TRAP(op_err1, op_err = enc_in.ReadRecordFromStreamL(&src_data));
	if(OpStatus::IsError(op_err1))
	{
		op_err = op_err1;
		return NULL;
	}
	if(op_err != OpRecStatus::FINISHED)
		return NULL;

	if (!enc_in_salt.Valid() || !enc_in.Valid())
	{
		op_err = OpStatus::ERR_PARSING_FAILED;
		return NULL;
	}

	if(!g_ssl_api->CheckSecurityManager())
	{
		op_err =  OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	op_err = g_securityManager->DecryptData(enc_in, enc_in_salt, plain_out, password, config);
	if(OpStatus::IsError(op_err))
		return NULL;

	if(plain_out.Error())
	{
		op_err = plain_out.GetOPStatus();
		return NULL;
	}

	unsigned char *data;
	unsigned long len;

	len = plain_out.GetLength();
	data = OP_NEWA(unsigned char, (len ? len : 1));
	if(!data)
	{
		op_err = OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	if(len)
		op_memcpy(data, plain_out.GetDirect(), len);
	else
		data[0] = 0;

	op_err = OpStatus::OK;
	out_len = len;
	return data;
}

unsigned char *SSL_API::EncryptWithSecurityPassword(OP_STATUS &op_err,
				const unsigned char *in_data, unsigned long in_len, unsigned long &out_len,
				const char* password, SSL_dialog_config &config)
{
	op_err = OpRecStatus::OK;

	out_len = 0;
	if(in_data == NULL || in_len == 0)
		return NULL;

	SSL_secure_varvector32 plain_in, enc_out, enc_out_salt;

	plain_in.Set(in_data, in_len);
	if (!plain_in.Valid())
	{
		op_err = plain_in.GetOPStatus();
		return NULL;
	}

	if(!g_ssl_api->CheckSecurityManager())
	{
		op_err =  OpStatus::ERR_NO_MEMORY;
		return NULL;
	}

	op_err = g_securityManager->EncryptData(plain_in, enc_out, enc_out_salt, password, config);
	if(OpStatus::IsError(op_err))
		return NULL;

	if(enc_out.Error())
	{
		op_err = enc_out.GetOPStatus();
		return NULL;
	}
	if(enc_out_salt.Error())
	{
		op_err = enc_out_salt.GetOPStatus();
		return NULL;
	}

	SSL_secure_varvector32 target;

	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, enc_out_salt.WriteRecordL(&target), NULL);
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, enc_out.WriteRecordL(&target), NULL);

	out_len = target.GetLength();
	unsigned char * OP_MEMORY_VAR data = NULL;
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, data = target.ReleaseL(), NULL);
	return data;
}
#endif

void SSL_API::StartSecurityPasswordSession()
{
	if(CheckSecurityManager())
		g_securityManager->StartSecurityPasswordSession();
}

void SSL_API::EndSecurityPasswordSession()
{
	if(CheckSecurityManager())
		g_securityManager->EndSecurityPasswordSession();
}
#endif

#endif

