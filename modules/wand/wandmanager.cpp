/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef WAND_SUPPORT

#include "modules/wand/wandmanager.h"

#include "modules/wand/wand_internal.h"
#include "modules/wand/suspendedoperation.h"
#include "modules/dochand/fdelm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"
#include "modules/forms/piforms.h"
#include "modules/forms/formvalue.h"
#include "modules/forms/formvaluelist.h"
#include "modules/forms/formvalueradiocheck.h"
#include "modules/forms/formvaluelistener.h"
#include "modules/doc/frm_doc.h"
#include "modules/doc/html_doc.h"

#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/inputmanager/inputmanager.h"
#include "modules/util/opfile/opsafefile.h"
#include "modules/locale/oplanguagemanager.h"

#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/src/WindowCommander.h" // to be able to cast from GetWindow()->GetWindowCommander() without warnings on brew-gogi. :-(
#include "modules/windowcommander/src/SSLSecurtityPasswordCallbackImpl.h"

#define WANDFILEVERSION 6

#define SWAP_ENDIAN_16(v)  ((((unsigned short)(v) & 0xff00) >> 8) | (((unsigned short)(v) & 0x00ff) << 8))

#include "modules/ecmascript_utils/esthread.h"

#ifdef SYNC_HAVE_PASSWORD_MANAGER
#include "modules/libcrypto/include/CryptoBlob.h"
#include "modules/sync/sync_coordinator.h"
#include "modules/sync/sync_util.h"
#endif // SYNC_HAVE_PASSWORD_MANAGER

#include "modules/libcrypto/include/OpRandomGenerator.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/wand/wand_sync_item.h"
#include "modules/libcrypto/include/CryptoMasterPasswordEncryption.h"


#define preleminary_dataitems g_opera->wand_module.preleminary_dataitems

// == Helper class ======================================================================


void WandSecurityWrapper::Disable()
{
	if (m_enabled)
	{

		g_opera->wand_module.m_wand_ssl_setup_count--;
		if (g_opera->wand_module.m_wand_ssl_setup_count == 0)
		{
			g_opera->wand_module.m_wand_ssl_setup_is_sync = FALSE;
		}
		m_enabled = FALSE;
	}
}

OP_STATUS WandSecurityWrapper::EnableWithoutWindow(BOOL force_master_password /* =FALSE*/)
{
	OP_ASSERT(!m_enabled);

	return EnableInternal(NULL, force_master_password);
}

OP_STATUS WandSecurityWrapper::Enable(Window* window, BOOL force_master_password/* =FALSE */)
{
	OP_ASSERT(!m_enabled);
	OP_ASSERT(window);

	return EnableInternal(const_cast<OpWindow*>(window->GetOpWindow()), force_master_password);
}

OP_STATUS WandSecurityWrapper::EnableInternal(OpWindow* platform_window,  BOOL force_master_password)
{
	if (!m_enabled)
		g_opera->wand_module.m_wand_ssl_setup_count++;

	m_enabled = TRUE;

	// If this is the outer layer, we have to check the password existance
	if (!g_opera->wand_module.m_wand_ssl_setup_is_sync &&
		(force_master_password || g_wand_manager->GetIsSecurityEncrypted()))
	{
		// Check if we need to ask for password

		OP_STATUS status;
		RETURN_IF_MEMORY_ERROR(status = g_libcrypto_master_password_handler->RetrieveMasterPassword(g_wand_manager, platform_window, OpSSLListener::SSLSecurityPasswordCallback::WandEncryption));
		// We need to ask for password
		if (status == OpStatus::ERR_YIELD)
			return OpStatus::ERR_YIELD;

		OP_DELETE(g_wand_encryption_password);
		g_wand_encryption_password = ObfuscatedPassword::CreateCopy(g_libcrypto_master_password_handler->GetRetrievedMasterPassword());

		g_opera->wand_module.m_wand_ssl_setup_is_sync = TRUE;
	}
	return OpStatus::OK;
}

WandSecurityWrapper::~WandSecurityWrapper()
{
	Disable();
}



// == WandInfo ======================================================================

WandInfo::WandInfo(WandManager* wand_manager,
				   WandPage* page,
				   Window* window)
	: page(page)
	, m_window(window)
    , wand_manager(wand_manager)
{
	OP_ASSERT(page);
	OP_ASSERT(window);
	OP_ASSERT(wand_manager);

	m_window->IncrementeWandInProgressCounter();

	Into(&wand_manager->wand_infos);
}
WandInfo::~WandInfo()
{
	Out();
	OP_DELETE(page);
	if (m_window)
		m_window->DecrementWandInProgressCounter();
}

void WandInfo::UnreferenceWindow(Window* window)
{
	OP_ASSERT(window);
	if (window == m_window)
	{
		m_window->DecrementWandInProgressCounter();
		m_window = NULL;
	}
}


void WandInfo::ReportAction(WAND_ACTION generic_action
#ifdef WAND_ECOMMERCE_SUPPORT
							, WAND_ECOMMERCE_ACTION eCommerce_action /* =  = WAND_DONT_STORE_ECOMMERCE_VALUES */
#endif // WAND_ECOMMERCE_SUPPORT
							)
{
	if (m_window && // If not, the window with this WandInfo has gone away
		(generic_action != WAND_DONT_STORE
#ifdef WAND_ECOMMERCE_SUPPORT
		|| eCommerce_action != WAND_DONT_STORE_ECOMMERCE_VALUES
#endif //  WAND_ECOMMERCE_SUPPORT
		))
	{
		WandSecurityWrapper security;
		OP_STATUS status = security.Enable(m_window);
		if (status == OpStatus::ERR_YIELD)
		{
			// Have to do this again some time
			int int2;
#ifdef WAND_ECOMMERCE_SUPPORT
			int2 = eCommerce_action;
#else
			int2 = 0;
#endif // WAND_ECOMMERCE_SUPPORT
			status = wand_manager->SetSuspendedInfoOperation(SuspendedWandOperation::WAND_OPERATION_REPORT_ACTION,
				m_window,
				NULL, NO, this, generic_action, int2);
			if (OpStatus::IsSuccess(status))
			{
				// |this| is owned by the SuspendedWandOperation
				return;
			}
		}
		
		if (OpStatus::IsError(status))
		{
			WandInfo* this_ = this;
			OP_DELETE(this_);
			return;
		}

		// If the paranoidsetting has been changed "by hand" in the prefsinifile, this will update the data.
		wand_manager->UpdateSecurityStateInternal(m_window, TRUE);

		UINT32 flags = 0;
		if (generic_action == WAND_NEVER_STORE_ON_THIS_PAGE)
			flags |= WAND_FLAG_NEVER_ON_THIS_PAGE;
		else if (generic_action == WAND_STORE_ENTIRE_SERVER)
			flags |= WAND_FLAG_ON_THIS_SERVER;
		else if (generic_action == WAND_NEVER_STORE_ON_ENTIRE_SERVER)
			flags |= WAND_FLAG_ON_THIS_SERVER | WAND_FLAG_NEVER_ON_THIS_PAGE;

#ifdef WAND_ECOMMERCE_SUPPORT
/*		Disabled saving of data in ecommerce, until we have a UI to remove/edit it with.
		Data will be read from personal-info in prefs to as much as there is info available.
		When enabling, also put back the code in WandPage::HasChangedECommerceData()

		if (eCommerce_action == WAND_STORE_ECOMMERCE_VALUES)
			flags |= WAND_FLAG_STORE_ECOMMERCE;
		if (generic_action == WAND_DONT_STORE)
			flags |= WAND_FLAG_STORE_NOTHING_BUT_ECOMMERCE;*/
#endif // WAND_ECOMMERCE_SUPPORT

		page->SetUserChoiceFlags(flags);
		if (OpStatus::IsSuccess(page->EncryptPasswords()))
		{
			OpStatus::Ignore(wand_manager->Store(m_window, page));
			page = NULL; // Now owned by the wand_manager
		}
	}

	WandInfo* this_ = this;
	OP_DELETE(this_); // We are finished
}

OpWindowCommander * WandInfo::GetOpWindowCommander()
{
	return m_window ? static_cast<OpWindowCommander *>(m_window->GetWindowCommander()) : NULL;
}

// == WandMatchInfo ======================================================================

WandMatchInfo::WandMatchInfo(FramesDocument* doc, WandProfile* profile)
	: m_doc(doc)
	, m_profile(profile)
{
	Into(&g_wand_manager->wand_match_infos);
}

WandMatchInfo::~WandMatchInfo()
{
	Out();
	while(m_matches.GetCount())
	{
		WandMatch* removed_match = m_matches.Remove(m_matches.GetCount() - 1);
		OP_DELETE(removed_match);
	}
}

//#ifdef WAND_FUNCTIONS_NOT_MADE_ASYNC_YET
void WandMatchInfo::Select(INT32 index, BOOL3 select)
{
	ProcessMatchSelectionAndDelete(index, select);
}
//#endif // WAND_FUNCTIONS_NOT_MADE_ASYNC_YET

OP_STATUS WandMatchInfo::ProcessMatchSelectionAndDelete(INT32 index, BOOL3 select)
{
	OP_ASSERT(index >= 0 && index < (INT32)m_matches.GetCount());

	OP_STATUS status = OpStatus::OK;
	WandMatch* match = m_matches.Get(index);
	if (match->page && m_profile->GetType() == WAND_LOG_PROFILE)
		status = g_wand_manager->Fetch(m_doc, NULL, match->page, select, FALSE);
#ifdef WAND_EXTERNAL_STORAGE
	else
		g_wand_manager->UseExternalStorage(m_doc, match->index);
#endif
	WandMatchInfo* this_ = this;
	OP_DELETE(this_); // We are finished
	return status;
}

void WandMatchInfo::Cancel()
{
	WandMatchInfo* this_ = this;
	OP_DELETE(this_); // We are finished
}

BOOL WandMatchInfo::Delete(INT32 index)
{
	OP_ASSERT(index >= 0 && index < (INT32)m_matches.GetCount());

	WandMatch* match = m_matches.Get(index);
	if (!match->page)
	{
#ifdef WAND_EXTERNAL_STORAGE
		if(OpStatus::IsError(g_wand_manager->DeleteFromExternalStorage(m_doc, GetMatchString(index))))
		{
			return FALSE;
		}
		else
		{
			// Ok, check if there were duplicates, in that case remove them too
			INT32 i = 0;
			WandPage* page = NULL;
			const uni_char* username = GetMatchString(index);
			while(page = m_profile->FindPage(m_doc, i))
			{
				if (!page->GetOnThisServer() || (page->GetOnThisServer() && page->CountMatches(m_doc) > 0))
				{
					OP_ASSERT(username);
					if(uni_strcmp(page->FindUserName(), username) == 0)
					{
						m_profile->DeletePage(m_doc, i);
					}
				}
				i++;
			}
		}
#else
		return FALSE;
#endif
	}
	else
	{
		INT32 pageindex = 0;
		while(m_profile->FindPage(m_doc, pageindex) != match->page)
			pageindex++;
		m_profile->DeletePage(m_doc, pageindex);
	}
	WandMatch* removed_match = m_matches.Remove(index);
	OP_DELETE(removed_match);
	StoreWandInfo();
	return TRUE;
}

const uni_char* WandMatchInfo::GetMatchString(INT32 index)
{
	OP_ASSERT(index >= 0 && index < (INT32)m_matches.GetCount());

	return m_matches.Get(index)->string.CStr();
}

OpWindowCommander* WandMatchInfo::GetOpWindowCommander() const
{
	if (m_doc)
		return m_doc->GetWindow()->GetWindowCommander();

	return NULL;
}

// == Misc. loading/saving stuff ====================================================

// !!!! NOTE !!!! IMPORTANT !!!! Must overwrite allocated memory and free it after use!!!!
#ifdef _SSL_SUPPORT_
#if !defined LIBSSL_DECRYPT_WITH_SECURITY_PASSWD // Old code
extern unsigned char *DecryptWithSecurityPassword(const unsigned char *in_data,
										unsigned long in_len, unsigned long &out_len, const char* password = NULL
										, OpWindow* parent_window = NULL
																				);
extern unsigned char *EncryptWithSecurityPassword(const unsigned char *in_data,
										unsigned long in_len, unsigned long &out_len, const char* password = NULL
										, OpWindow* parent_window = NULL
										);
extern void StartSecurityPasswordSession();
extern void EndSecurityPasswordSession();
#endif // !LIBSSL_DECRYPT_WITH_SECURITY_PASSWD
#endif

BOOL HasPreparedSSLForCrypto()
{
	return g_opera->wand_module.m_wand_ssl_setup_count > 0;
}

void WandManager::ReEncryptDatabase(const ObfuscatedPassword* old_password, const ObfuscatedPassword* new_password)
{
	// Fix: personal profiles
	UINT32 i;
	for(i = 0; i < log_profile.pages.GetCount(); i++)
	{
		WandPage* page = log_profile.pages.Get(i);
		for(unsigned int j = 0; j < page->objects.GetCount(); j++)
		{
			WandObjectInfo* objinfo = page->objects.Get(j);
			if (objinfo->is_password)
			{
				OpStatus::Ignore(objinfo->password.ReEncryptPassword(old_password, new_password));
			}
		}
	}

	// Logins
	for(i = 0; i < logins.GetCount(); i++)
	{
		OpStatus::Ignore(logins.Get(i)->password.ReEncryptPassword(old_password, new_password));
	}

	if (new_password == g_wand_obfuscation_password)
		is_security_encrypted = FALSE;
	else
		is_security_encrypted = TRUE;

	StoreWandInfo();
}

void WandManager::UpdateSecurityStateInternal(Window *possible_parent_window, BOOL silent_about_nops)
{
	BOOL prefs_paranoid = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword);

	BOOL change_normal_to_paranoid = !is_security_encrypted && prefs_paranoid;
	BOOL change_paranoid_to_normal = is_security_encrypted && !prefs_paranoid;

	if (!change_normal_to_paranoid && !change_paranoid_to_normal)
	{
		if (!silent_about_nops)
		{
			for(UINT32 i = 0; i < listeners.GetCount(); i++)
			{
				listeners.Get(i)->OnSecurityStateChange(TRUE /* successful*/, FALSE /*changed */, is_security_encrypted);
			}
		}
		return;
	}

	WandSecurityWrapper security;
	OP_STATUS status;
	if (possible_parent_window)
	{
		status = security.Enable(possible_parent_window, TRUE /* force master password enabled */);
	}
	else
	{
		status = security.EnableWithoutWindow(TRUE /* force master password enabled */);
	}

	if (status == OpStatus::ERR_YIELD)
	{
		// Have to do this again some time
		status = SetSuspendedOperation(
			possible_parent_window ? SuspendedWandOperation::WAND_OPERATION_CHANGE_DB_ENCRYPTION : SuspendedWandOperation::WAND_OPERATION_CHANGE_DB_ENCRYPTION_NO_WINDOW,
				possible_parent_window,
				NULL, NO);

		if (OpStatus::IsSuccess(status))
		{
			return;
		}
	}

	if (OpStatus::IsError(status))
	{
		for(UINT32 i = 0; i < listeners.GetCount(); i++)
		{
			listeners.Get(i)->OnSecurityStateChange(FALSE /* successful*/, FALSE /*changed */, is_security_encrypted);
		}
		return;
	}

	BOOL send_data_base_changed = TRUE;

	// Update data
	if (change_normal_to_paranoid)
	{
		ReEncryptDatabase(g_wand_obfuscation_password, g_wand_encryption_password);
		OP_ASSERT(is_security_encrypted == TRUE);
	}
	else if (change_paranoid_to_normal)
	{
		OpAutoPtr<ObfuscatedPassword> temp_copy(ObfuscatedPassword::CreateCopy(g_wand_obfuscation_password));

		if (temp_copy.get())
		{
			ReEncryptDatabase(g_wand_encryption_password, g_wand_obfuscation_password);
			OP_ASSERT(is_security_encrypted == FALSE);

			OP_DELETE(g_wand_encryption_password);
			g_wand_encryption_password = temp_copy.release();
		}
		else
			send_data_base_changed = FALSE;
	}

	for(UINT32 i = 0; i < listeners.GetCount(); i++)
	{
		listeners.Get(i)->OnSecurityStateChange(send_data_base_changed /* successful*/, send_data_base_changed /*changed */, is_security_encrypted);
	}
}


OP_STATUS StoreWandInfo()
{
	// Fix: if the saving fails, it will be a trashed wandfile or containing info encrypted with the wrong password.
	OpFile wand_file;
	TRAPD(rc, g_pcfiles->GetFileL(PrefsCollectionFiles::WandFile, wand_file));
	if (OpStatus::IsSuccess(rc))
		return g_wand_manager->Save(wand_file);
	else
		return rc;
}

OP_STATUS ReadWandString(OpFile &file, OpString &str, long version)
{
	long in_len_long = 0;
	UINT8* in_data = NULL;
	OP_STATUS status = OpStatus::OK;

	RETURN_IF_ERROR(str.Set(UNI_L("")));
	RETURN_IF_ERROR(file.ReadBinLong(in_len_long));
	int in_len = (int)in_len_long;
	if (in_len > 0)
	{
		in_data = OP_NEWA(unsigned char, in_len);
		ANCHOR_ARRAY(unsigned char, in_data);
		if (in_data == NULL)
			return OpStatus::ERR_NO_MEMORY;
		OpFileLength bytes_read;
		status = file.Read(in_data, in_len, &bytes_read);
		if (OpStatus::IsSuccess(status) && bytes_read != (OpFileLength)in_len)
			status = OpStatus::ERR;

		if (OpStatus::IsSuccess(status))
		{
			int decrypted_data_length = 0;
			UINT8 *decrypted_data;

			RETURN_IF_ERROR(g_libcrypto_master_password_encryption->DecryptPasswordWithSecurityMasterPassword(decrypted_data, decrypted_data_length, in_data, in_len, g_wand_obfuscation_password));
			ANCHOR_ARRAY(UINT8, decrypted_data);
#ifdef OPERA_BIG_ENDIAN
			if (version > 2)
			{
				// Version 2 is stored as the endian the platform uses. Newer versions is always stored as little endian.
				uni_char* unichardata = (uni_char*) decrypted_data;
				for(int i = 0; i < decrypted_data_length/sizeof(uni_char); i++)
					unichardata[i] = SWAP_ENDIAN_16(unichardata[i]);
			}
#endif
			status = str.Set((uni_char*)decrypted_data, decrypted_data_length / sizeof(uni_char));
			op_memset(decrypted_data, 0, decrypted_data_length);
		}
	}

	// The string didn't contain any data. Set to empty string so we don't need to check for NULL content everywhere.
	return status;
}

OP_STATUS WriteWandString(OpFile &file, OpString &str)
{
	int str_len = str.Length();
	if (str_len == 0)
		RETURN_IF_ERROR(file.WriteBinLong(0));
	else
	{
		int in_len = (str_len + 1) * sizeof(uni_char);
		int data_len = 0;
		UINT8* data = NULL;
		int unichardatabuflen = str_len + 1;
		uni_char* unichardata = OP_NEWA(uni_char, unichardatabuflen);
		ANCHOR_ARRAY(uni_char, unichardata);

		if (unichardata)
		{
			op_memcpy(unichardata, str.CStr(), in_len);
#ifdef OPERA_BIG_ENDIAN
			int len = str_len;
			for(int i = 0; i < len; i++)
				unichardata[i] = SWAP_ENDIAN_16(unichardata[i]);
#endif
			OP_STATUS status;
			RETURN_IF_MEMORY_ERROR(status = g_libcrypto_master_password_encryption->EncryptPasswordWithSecurityMasterPassword(data, data_len, (UINT8*)unichardata, in_len, g_wand_obfuscation_password));
			if (OpStatus::IsSuccess(status))
			{
				RETURN_IF_ERROR(file.WriteBinLong(data_len));
				RETURN_IF_ERROR(file.Write(data, data_len));

				op_memset(data, 0, data_len);
				OP_DELETEA(data);
			}
			else
				RETURN_IF_ERROR(file.WriteBinLong(0));
		}
	}

	return OpStatus::OK;
}

OP_STATUS GetWandUrl(FramesDocument* doc, OpString &url)
{
	unsigned short dummy_charset_id = 0; // not used since we convert to a *_Escaped string.
	RETURN_IF_ERROR(doc->GetURL().GetAttribute(URL::KUniName_With_Fragment_Username_Password_Hidden_Escaped, dummy_charset_id, url));
	MakeWandUrl(url);
	return OpStatus::OK;
}

void MakeWandUrl(OpString &url)
{
	int pos = url.FindLastOf('?');
	if (pos != KNotFound)
		url.Delete(pos);
}

FramesDocument* FindWandSubDoc(FramesDocElm* fdelm)
{
	FramesDocument* fdoc = fdelm->GetCurrentDoc();
	if (fdoc) // Unnecessary check?
	{
		if (fdoc->GetHasWandMatches())
		{
			return fdoc;
		}
		if (fdoc->GetFrmDocRoot())
		{
			return FindWandSubDoc(fdoc->GetFrmDocRoot());
		}
	}

	fdelm = fdelm->FirstChild();
	while (fdelm)
	{
		FramesDocument* subdoc = FindWandSubDoc(fdelm);
		if (subdoc)
			return subdoc;

		fdelm = fdelm->Suc();
	}
	return NULL;
}

FramesDocument* FindWandDoc(FramesDocument* doc)
{
	// First try, return document itself if it has wandmatches
	if (doc->GetHasWandMatches())
		return doc;

	// Try with the focused frame
	if (FramesDocument* subdoc = doc->GetActiveSubDoc())
	{
		if (subdoc->GetHasWandMatches())
			return subdoc;
	}

	// Run through all framesdocs and return the first found with wandmatches
	FramesDocElm* frameset_root_root_elm = doc->GetTopFramesDoc()->GetFrmDocRoot();
	if (frameset_root_root_elm)
		return FindWandSubDoc(frameset_root_root_elm);

	// Run through all iframes and return the first found with wandmatches
	if (doc->GetIFrmRoot())
		return FindWandSubDoc(doc->GetIFrmRoot());

	return NULL;
}

// == class WandPassword =================================================

WandPassword::~WandPassword()
{
	if (password_len && password)
		op_memset(password, 0, password_len);
	OP_DELETEA(password);
}

OP_STATUS WandPassword::Save(OpFile &file)
{
	RETURN_IF_ERROR(file.WriteBinLong(password_len));
	if (password_len)
	{
		OP_ASSERT(password_encryption_mode == PASSWORD_ENCRYPTED);
		return file.Write(password, password_len);
	}
	return OpStatus::OK;
}

OP_STATUS WandPassword::Open(OpFile &file, long version)
{
	long tmp;
	RETURN_IF_ERROR(file.ReadBinLong(tmp));
	password_len = (UINT16) tmp;

	if (password_len)
	{
		password = OP_NEWA(unsigned char, password_len);
		if (password == NULL)
			return OpStatus::ERR_NO_MEMORY;
		OpFileLength bytes_read;
		RETURN_IF_ERROR(file.Read(password, password_len, &bytes_read));
		if (bytes_read != password_len)
			return OpStatus::ERR;
	}
#ifdef OPERA_BIG_ENDIAN
	if (version <= 2)
	{
		// Version 2 is stored as the endian the platform uses. Newer versions is always stored as little endian.
		// We have to decrypt and encrypt it again (to store it in little endian).

		OpString str;
		RETURN_IF_ERROR(GetPassword(str));
		if (!str.IsEmpty())
		{
			uni_char* unichardata = str.CStr();
			for(int i = 0; i < str.Length(); i++)
				unichardata[i] = SWAP_ENDIAN_16(unichardata[i]);

			RETURN_IF_ERROR(Set(unichardata, TRUE));
			str.Wipe();
		}
	}
#endif

	password_encryption_mode = PASSWORD_ENCRYPTED;

	return OpStatus::OK;
}

OP_STATUS WandPassword::CopyTo(WandPassword* clone)
{
	OP_ASSERT(clone);
	unsigned char* new_password = OP_NEWA(unsigned char, this->password_len);
	if (!new_password)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	op_memcpy(new_password, this->password, this->password_len);

	if (clone->password_len && clone->password)
		op_memset(clone->password, 0, clone->password_len);
	OP_DELETEA(clone->password);

	clone->password = new_password;
	clone->password_len = this->password_len;
	clone->password_encryption_mode = this->password_encryption_mode;
	return OpStatus::OK;
}

OP_STATUS WandPassword::Set(const unsigned char* data, UINT16 length)
{
	OP_DELETEA(password);
	password_len = 0;

	password = OP_NEWA(unsigned char, length);
	if (password == NULL)
		return OpStatus::ERR_NO_MEMORY;
	password_len = length;
	op_memcpy(password, data, length);
	return OpStatus::OK;
}

void WandPassword::Switch(WandPassword* pwd)
{
	UINT16 tmp1 = pwd->password_len;
	pwd->password_len = password_len;
	password_len = tmp1;

	unsigned char* tmp2 = pwd->password;
	pwd->password = password;
	password = tmp2;
}

OP_STATUS WandPassword::Set(const uni_char* inpass, BOOL add_encrypted)
{
	if (add_encrypted)
	{
		int inlen = uni_strlen(inpass);
		uni_char* unichardata = OP_NEWA(uni_char, inlen + 1);
		RETURN_OOM_IF_NULL(unichardata);
		ANCHOR_ARRAY(uni_char, unichardata);

		op_memcpy(unichardata, inpass, (inlen + 1) * sizeof(uni_char));
#ifdef OPERA_BIG_ENDIAN
		for(int i = 0; i < inlen; i++)
			unichardata[i] = SWAP_ENDIAN_16(unichardata[i]);
#endif
		OP_DELETEA(password);

		OP_ASSERT(HasPreparedSSLForCrypto());

		int password_length;
		OP_STATUS status = g_libcrypto_master_password_encryption->EncryptPasswordWithSecurityMasterPassword(password, password_length, (UINT8*)unichardata, inlen * sizeof(uni_char), g_wand_encryption_password);
		password_len = (UINT16) password_length;

		op_memset(unichardata, 0, (inlen + 1) * sizeof(uni_char));

		if (OpStatus::IsSuccess(status))
			password_encryption_mode = PASSWORD_ENCRYPTED;

		return status;
	}
	else
	{
		SetObfuscated(inpass);
	}

	return OpStatus::OK;
}

OP_STATUS WandPassword::Encrypt()
{
	OP_ASSERT(g_opera->wand_module.m_wand_ssl_setup_count > 0);

	if (password_encryption_mode != PASSWORD_ENCRYPTED)
	{
		OpString plain_text_pwd;
		RETURN_IF_ERROR(DeObfuscate(plain_text_pwd));
		const uni_char* pwd = plain_text_pwd.IsEmpty() ? UNI_L("") : plain_text_pwd.CStr();
		RETURN_IF_ERROR(Set(pwd, TRUE));
		plain_text_pwd.Wipe();
		password_encryption_mode = PASSWORD_ENCRYPTED;
	}

	return OpStatus::OK;
}

OP_STATUS WandPassword::SetObfuscated(const uni_char* in_password)
{
	OP_DELETEA(password);

	password_encryption_mode = PASSWORD_TEMPORARY_OBFUSCATED;
	OP_STATUS encrypt_status = g_libcrypto_master_password_encryption->EncryptPasswordWithSecurityMasterPassword(password, password_len, (UINT8*)in_password, sizeof(uni_char)*uni_strlen(in_password), g_wand_obfuscation_password);

	return encrypt_status;
}

OP_STATUS WandPassword::DeObfuscate(OpString &plain_password) const
{
	OP_ASSERT(password_encryption_mode == PASSWORD_TEMPORARY_OBFUSCATED);
	int plain_password_bytes_length = 0;
	RETURN_IF_ERROR(plain_password.Set(""));
	UINT8 *plain_password_bytes;
	RETURN_IF_ERROR(g_libcrypto_master_password_encryption->DecryptPasswordWithSecurityMasterPassword(plain_password_bytes, plain_password_bytes_length, (UINT8*)password, password_len, g_wand_obfuscation_password));
	ANCHOR_ARRAY(UINT8, plain_password_bytes);

	OP_STATUS status = plain_password.Set((uni_char*) plain_password_bytes, plain_password_bytes_length/sizeof(uni_char));
	op_memset(plain_password_bytes, 0, plain_password_bytes_length);
	return status;
}

OP_STATUS WandPassword::GetPassword(OpString& str) const
{
	if (password_encryption_mode == PASSWORD_ENCRYPTED)
		return Decrypt(str);
	else if (password_encryption_mode == PASSWORD_TEMPORARY_OBFUSCATED)
		return DeObfuscate(str);

	// No password set.
	return str.Set("");
}

OP_STATUS WandPassword::Decrypt(OpString& str) const
{
	OP_ASSERT(HasPreparedSSLForCrypto());
	str.Set("");
	OP_STATUS status = OpStatus::OK;
	if (password && password_len)
	{
		int out_len = 0;
		UINT8 *tmpstr;
		RETURN_IF_ERROR(g_libcrypto_master_password_encryption->DecryptPasswordWithSecurityMasterPassword(tmpstr, out_len, (UINT8*)password, password_len, g_wand_encryption_password));
		ANCHOR_ARRAY(UINT8, tmpstr);

#ifdef OPERA_BIG_ENDIAN
		uni_char* unichardata = (uni_char*) tmpstr;
		for(int i = 0; i < out_len/sizeof(uni_char); i++)
			unichardata[i] = SWAP_ENDIAN_16(unichardata[i]);
#endif
		status = str.Set((uni_char *)tmpstr, out_len / sizeof(uni_char));
		op_memset(tmpstr, 0, out_len);
	}

	return status;
}

OP_STATUS WandPassword::ReEncryptPassword(const ObfuscatedPassword* old_masterpassword, const ObfuscatedPassword* new_masterpassword)
{
	OP_STATUS  status = OpStatus::OK;
	if (password && password_len)
	{
		OP_ASSERT(password_encryption_mode == PASSWORD_ENCRYPTED);

		int plain_text_pwd_len = 0;

		UINT8* plain_text_pwd;
		RETURN_IF_ERROR(g_libcrypto_master_password_encryption->DecryptPasswordWithSecurityMasterPassword(plain_text_pwd, plain_text_pwd_len, (UINT8*)password, password_len, old_masterpassword));
		ANCHOR_ARRAY(UINT8, plain_text_pwd);

		OP_DELETEA(password);
		status = g_libcrypto_master_password_encryption->EncryptPasswordWithSecurityMasterPassword(password, password_len, plain_text_pwd, plain_text_pwd_len, new_masterpassword);

		op_memset(plain_text_pwd, 0, plain_text_pwd_len);

		return status;
	}
	return status;
}

// == class WandObjectInfo =================================================

WandObjectInfo::~WandObjectInfo()
{
}

OP_STATUS WandObjectInfo::Init(const uni_char* name, const uni_char* value, BOOL is_password, BOOL is_textfield, BOOL is_changed, BOOL add_encrypted)
{
	RETURN_IF_ERROR(this->name.Set(name));
	this->is_password = is_password;
	this->is_changed = is_changed;
	this->is_textfield_for_sure = is_textfield;

	if (is_password)
		RETURN_IF_ERROR(password.Set(value, add_encrypted));
	else
		RETURN_IF_ERROR(this->value.Set(value));

	return OpStatus::OK;
}

#define WAND_OBJECTINFO_IS_PASSWORD 0x01
#define WAND_OBJECTINFO_IS_UNCHANGED 0x02
#define WAND_OBJECTINFO_IS_TEXTFIELD 0x04
#define WAND_OBJECTINFO_IS_GUESSED_USERNAME 0x08

OP_STATUS WandObjectInfo::Save(OpFile &file)
{
	BYTE tmp = 0;
	if (is_password)
		tmp |= WAND_OBJECTINFO_IS_PASSWORD;
	if (!is_changed)
		tmp |= WAND_OBJECTINFO_IS_UNCHANGED;
	if (is_textfield_for_sure)
		tmp |= WAND_OBJECTINFO_IS_TEXTFIELD;
	if (is_guessed_username)
		tmp |= WAND_OBJECTINFO_IS_GUESSED_USERNAME;

	RETURN_IF_ERROR(file.WriteBinByte(tmp));

	RETURN_IF_ERROR(WriteWandString(file, name));
	RETURN_IF_ERROR(WriteWandString(file, value));
	RETURN_IF_ERROR(password.Save(file));
	return OpStatus::OK;
}

OP_STATUS WandObjectInfo::Open(OpFile &file, long version)
{
	BYTE tmp;
	OpFileLength bytes_read;
	RETURN_IF_ERROR(file.Read(&tmp, 1, &bytes_read));
	if (bytes_read != 1)
		return OpStatus::ERR;
	is_password = (tmp & WAND_OBJECTINFO_IS_PASSWORD ? TRUE : FALSE);
	is_changed = (tmp & WAND_OBJECTINFO_IS_UNCHANGED ? FALSE : TRUE);
	is_textfield_for_sure = (tmp & WAND_OBJECTINFO_IS_TEXTFIELD ? TRUE : FALSE);
	is_guessed_username = (tmp & WAND_OBJECTINFO_IS_GUESSED_USERNAME ? TRUE : FALSE);

	RETURN_IF_ERROR(ReadWandString(file, name, version));
	RETURN_IF_ERROR(ReadWandString(file, value, version));
	RETURN_IF_ERROR(password.Open(file, version));
	return OpStatus::OK;
}

// == class WandLogin =================================================

BOOL MatchingLoginID(const OpStringC& _id1, const OpStringC& _id2)
{
	OpString id1, id2;
	BOOL server1 = _id1.HasContent() && _id1[0] == '*';
	BOOL server2 = _id2.HasContent() && _id2[0] == '*';
	if (OpStatus::IsError(id1.Set(_id1.SubString(server1?1:0))) ||
		OpStatus::IsError(id2.Set(_id2.SubString(server2?1:0))))
		return FALSE;

	if (server1 || server2)
	{
		MakeWandServerUrl(id1);
		MakeWandServerUrl(id2);
	}

	return id1.Compare(id2) == 0;
}

OP_STATUS WandLogin::Set(const uni_char* id, const uni_char* username, const uni_char* password, BOOL encrypt_password)
{
	OP_ASSERT(id);
	OP_ASSERT(username);
	RETURN_IF_ERROR(this->id.Set(id));
	RETURN_IF_ERROR(this->username.Set(username));
	RETURN_IF_ERROR(this->password.Set(password, encrypt_password));
	return OpStatus::OK;
}

OP_STATUS WandLogin::Save(OpFile &file)
{
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	RETURN_IF_ERROR(WandSyncItem::SaveSyncData(file));
#else
	// write dummy data to keep the file format version >=6 consistent
	OpString dummy_string;
	RETURN_IF_ERROR(file.WriteBinLong(0L));
	RETURN_IF_ERROR(WriteWandString(file, dummy_string));
	RETURN_IF_ERROR(WriteWandString(file, dummy_string));
#endif // SYNC_HAVE_PASSWORD_MANAGER

	RETURN_IF_ERROR(WriteWandString(file, id));
	RETURN_IF_ERROR(WriteWandString(file, username));


	return password.Save(file);
}

OP_STATUS WandLogin::Open(OpFile &file, long version)
{
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	RETURN_IF_ERROR(WandSyncItem::OpenSyncData(file, version));
#else
	if (version >= 6)
	{
		// Read dummy data to keep the file format version >=6 consistent
		long dummy;
		RETURN_IF_ERROR(file.ReadBinLong(dummy));
		OpString dummy_string;
		RETURN_IF_ERROR(ReadWandString(file, dummy_string, version));
		RETURN_IF_ERROR(ReadWandString(file, dummy_string, version));
	}
#endif // SYNC_HAVE_PASSWORD_MANAGER

	RETURN_IF_ERROR(ReadWandString(file, id, version));
	RETURN_IF_ERROR(ReadWandString(file, username, version));
	return password.Open(file, version);
}

WandLogin* WandLogin::Clone()
{
	WandLogin* clone = OP_NEW(WandLogin, ());
	if (clone)
	{
		if (OpStatus::IsError(clone->id.Set(this->id)) ||
			OpStatus::IsError(clone->username.Set(this->username)) ||
			OpStatus::IsError(this->password.CopyTo(&clone->password))
#ifdef SYNC_HAVE_PASSWORD_MANAGER
			|| OpStatus::IsError(WandSyncItem::CopyTo(clone))
#endif // SYNC_HAVE_PASSWORD_MANAGER
		)
		{
			OP_DELETE(clone);
			clone = NULL;
		}
	}
	return clone;
}

OP_STATUS WandLogin::EncryptPassword()
{
	return password.Encrypt();
}

BOOL WandLogin::IsEqualTo(WandLogin *login)
{
	OP_ASSERT(login);
	if (!MatchingLoginID(this->id, login->id))
		return FALSE;

	if (this->username.Compare(login->username))
		return FALSE;

	OpString this_decrypted_password;
	if (OpStatus::IsError(this->password.GetPassword(this_decrypted_password)))
		return FALSE;

	OpString decrypted_password;
	if (OpStatus::IsError(login->password.GetPassword(decrypted_password)))
		return FALSE;

	if (this_decrypted_password.Compare(decrypted_password))
		return FALSE;

	return TRUE;
}

#ifdef SYNC_HAVE_PASSWORD_MANAGER

OP_STATUS WandLogin::InitAuthTypeSpecificElements(OpSyncItem *item, BOOL modify)
{
	OP_ASSERT(HasPreparedSSLForCrypto());
	OpAutoPtr<CryptoBlobEncryption> blob_encryptor(CryptoBlobEncryption::Create());
	RETURN_OOM_IF_NULL(blob_encryptor.get());
	RETURN_IF_ERROR(blob_encryptor->SetKey(g_wand_manager->GetSyncPasswordEncryptionKey()));

	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PAGE_URL, id));
	if (!modify && id.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(blob_encryptor->DecryptBase64(id.CStr(), id));
	MakeWandUrl(id);

	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_USERNAME, username));
	if (!modify && username.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(blob_encryptor->DecryptBase64(username.CStr(), username));
	if (!modify && username.IsEmpty())
		return OpStatus::ERR;

	OpString password_string;
	RETURN_IF_ERROR(item->GetData(OpSyncItem::SYNC_KEY_PASSWORD, password_string));
	if (!modify && password_string.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(blob_encryptor->DecryptBase64(password_string.CStr(), password_string));
	/* Note: the decrypted password may be empty ... */
	return password.Set(password_string.CStr(), TRUE);
}

OP_STATUS WandLogin::ConstructSyncItemAuthTypeSpecificElements(OpSyncItem *sync_item)
{
	OP_ASSERT(HasPreparedSSLForCrypto());
	OP_ASSERT(g_wand_manager->HasSyncPasswordEncryptionKey() && "This should only be called if we have the encryption-key!");

	OpAutoPtr<CryptoBlobEncryption> blob_encryptor(CryptoBlobEncryption::Create());
	RETURN_OOM_IF_NULL(blob_encryptor.get());
	RETURN_IF_ERROR(blob_encryptor->SetKey(g_wand_manager->GetSyncPasswordEncryptionKey()));

	OpString blob;
	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(id, blob));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PAGE_URL, blob.CStr()));

	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(username, blob));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_USERNAME, blob.CStr()));

	RETURN_IF_ERROR(password.GetPassword(blob));
	RETURN_IF_ERROR(blob_encryptor->EncryptBase64(blob, blob));
	RETURN_IF_ERROR(sync_item->SetData(OpSyncItem::SYNC_KEY_PASSWORD, blob.CStr()));

	OP_ASSERT(sync_data_modified_date.HasContent());
	return sync_item->SetData(OpSyncItem::SYNC_KEY_MODIFIED, sync_data_modified_date.CStr());
}

BOOL WandLogin::PreventSync()
{
	if (id.CompareI("opera:", 6) == 0)
	{
		if (id.CompareI("opera:mail", 10) == 0) // The email password should be synced.
			return FALSE;
		return TRUE;
	}
	return FALSE;
}

#endif // SYNC_HAVE_PASSWORD_MANAGER

OP_STATUS WandManager::GetLoginPasswordInternal(Window* window, WandLogin* wand_login, WandLoginCallback* callback)
{
	OP_ASSERT(wand_login);
	OP_ASSERT(callback);

	WandSecurityWrapper security;
	OP_STATUS status = window ? security.Enable(window) : security.EnableWithoutWindow();
	if (status == OpStatus::ERR_YIELD)
	{
		OpAutoPtr<WandLogin> temporary_login(wand_login->Clone());
		if (!temporary_login.get())
		{
			return OpStatus::ERR_NO_MEMORY;
		}
		RETURN_IF_ERROR(SetSuspendedLoginOperation(
			window ? SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD : SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD_NO_WINDOW,
			window,
			NULL, NO, temporary_login.get(), callback));
		temporary_login.release();
		return OpStatus::OK; // The caller doesn't have to know that we're not quite done yet
	}
	else if (OpStatus::IsError(status))
	{
		return status;
	}

	OpString decrypted_pwd;
	status = wand_login->password.GetPassword(decrypted_pwd);
	if (OpStatus::IsSuccess(status))
	{
		callback->OnPasswordRetrieved(decrypted_pwd.CStr());
		decrypted_pwd.Wipe();
	}
	else
	{
		callback->OnPasswordRetrievalFailed();
	}

	return OpStatus::OK;
}


#ifdef WAND_EXTERNAL_STORAGE

// == class WandDataStorageEntry =================================================

WandDataStorageEntry::~WandDataStorageEntry()
{
	password.Wipe();
}

OP_STATUS WandDataStorageEntry::Set(const uni_char* username, const uni_char* password)
{
	RETURN_IF_ERROR(this->username.Set(username));
	RETURN_IF_ERROR(this->password.Set(password));
	return OpStatus::OK;
}

#endif // WAND_EXTERNAL_STORAGE

// == class WandManager =================================================

WandManager::WandManager()
	: log_profile(this, WAND_LOG_PROFILE)
	, curr_profile(0)
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	, m_sync_block(FALSE)
	, m_key(NULL)
#endif // SYNC_HAVE_PASSWORD_MANAGER
	, is_active(FALSE)
	, is_security_encrypted(FALSE)
#ifdef WAND_EXTERNAL_STORAGE
	, data_storage(NULL)
#endif
{
	g_main_message_handler->SetCallBack(this, MSG_WAND_RESUME_OPERATION, 0);
	g_pccore->RegisterListenerL(this);
	g_pcnet->RegisterListenerL(this);
}

WandManager::~WandManager()
{
	OP_NEW_DBG("WandManager::~WandManager()", "wand");

	if (g_libcrypto_master_password_handler)
		g_libcrypto_master_password_handler->CancelGettingPassword(this);

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	WandManager::SyncBlocker sync_blocker;
	g_sync_encryption_key_manager->RemoveEncryptionKeyListener(this);
	g_sync_coordinator->RemoveSyncDataClient(this, OpSyncDataItem::DATAITEM_PM_HTTP_AUTH);
	g_sync_coordinator->RemoveSyncDataClient(&log_profile, OpSyncDataItem::DATAITEM_PM_FORM_AUTH);
#endif // SYNC_HAVE_PASSWORD_MANAGER

	g_main_message_handler->UnsetCallBacks(this);
	profiles.DeleteAll();
	logins.DeleteAll();
	while(listeners.GetCount())
		listeners.Remove(listeners.GetCount() - 1);

	if (g_pccore)
		g_pccore->UnregisterListener(this);

	OP_ASSERT(!wand_infos.Cardinal());
	OP_ASSERT(!wand_match_infos.Cardinal());
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	OpStatus::Ignore(SetSyncPasswordEncryptionKey(0));
#endif // SYNC_HAVE_PASSWORD_MANAGER
}

BOOL WandManager::IsUsing(FramesDocument* doc)
{
	WandMatchInfo* info = (WandMatchInfo*) wand_match_infos.First();
	while (info)
	{
		if (info->m_doc == doc)
			return TRUE;
		info = (WandMatchInfo*) info->Suc();
	}
	return FALSE;
}

BOOL WandManager::IsWandOperationInProgress(Window* window)
{
	WandInfo* info = (WandInfo*) wand_infos.First();
	while (info)
	{
		if (info->m_window == window)
			return TRUE;
		info = (WandInfo*) info->Suc();
	}
	return FALSE;
}

void WandManager::UnreferenceDocument(FramesDocument* doc)
{
	for (unsigned i = 0; i < m_suspended_operations.GetCount(); i++)
	{
		m_suspended_operations.Get(i)->ClearReferenceToDoc(doc);
	}
}

void WandManager::UnreferenceWindow(Window* window)
{
	OP_ASSERT(window);
	for (unsigned i = 0; i < m_suspended_operations.GetCount(); i++)
	{
		m_suspended_operations.Get(i)->ClearReferenceToWindow(window);
	}

	WandInfo* info = (WandInfo*) wand_infos.First();
	while (info)
	{
		info->UnreferenceWindow(window);
		info = (WandInfo*) info->Suc();
	}
}

OP_STATUS WandManager::MaybeStorePageInDatabase(Window* window, WandPage* page)
{
	OpAutoPtr<WandPage> page_owner(page);

#ifdef WAND_ECOMMERCE_SUPPORT
	if (!page->HasChangedECommerceData())
#endif // WAND_ECOMMERCE_SUPPORT
	{
		// If it doesn't contain exactly 1 password, we don't ask to save it. It's most probably a search or a changepassword page.
		if (page->CountPasswords() != 1)
		{
			return OpStatus::OK;
		}

		// If the info is already stored and equals the current, we shouldn't ask to save it again.
		int nr = 0;
		while (WandPage* stored_page = log_profile.FindPage(page, nr))
		{
			FramesDocument* doc = NULL;
			if (stored_page->IsSameFormAndUsername(page, doc))
			{
#ifdef SYNC_HAVE_PASSWORD_MANAGER
				WandSyncItem::SyncStatus previous_sync_status = stored_page->GetLocalSyncStatus();
#endif // SYNC_HAVE_PASSWORD_MANAGER

				// Update the existing page with the new info (in case there are subtle changes)
				if (!GetIsSecurityEncrypted())
				{
					WandSecurityWrapper security;
					OP_STATUS status = security.Enable(window);
					RETURN_IF_ERROR(status); // Will not yield since GetIsSecurityEncrypted() is FALSE
					RETURN_IF_ERROR(page->Encrypt()); // Will merely update the "is_encrypted" flag to avoid asserts
					status = stored_page->ReplaceWithOtherPage(page);

#ifdef SYNC_HAVE_PASSWORD_MANAGER
					if (previous_sync_status != WandSyncItem::SYNC_ADD)
						stored_page->SetLocalSyncStatus(WandSyncItem::SYNC_MODIFY);

					RETURN_IF_ERROR(stored_page->SetModifiedDate());
					OpStatus::Ignore(stored_page->SyncItem());
#endif // SYNC_HAVE_PASSWORD_MANAGER
					if (OpStatus::IsSuccess(status))
						status = StoreWandInfo();

					return status;
				}
				else
				{
					// Dilemma. We don't want to trigger a master password dialog whenever someone is submitting.
					// On the other hand, if we don't we won't save the new password if the user has changed it
					// and we can't even detect that case since passwords are encrypted.

					// Ok, do nothing. :-(
					return OpStatus::OK;
				}
			}
			nr++;
		}
	}
#ifdef WAND_EXTERNAL_STORAGE
	if (!existingpage && data_storage)
	{
		OpString docurl;
		RETURN_IF_ERROR(GetWandUrl(doc, docurl));

		WandDataStorageEntry entry;
		entry.username.Set(page->FindUserName());

		INT32 i = 0;
		while (data_storage->FetchData(docurl, &entry, i++))
		{
			if (entry.username.Compare(page->FindUserName()) == 0)
			{
				return;
			}
		}
	}
#endif // WAND_EXTERNAL_STORAGE

	WandInfo* info = OP_NEW(WandInfo, (this, page, window));
	if (info == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	page_owner.release(); // now owned by the info object

	if (listeners.GetCount() == 0)
		info->ReportAction(WAND_DONT_STORE); // deletes |info|
	else
	{
#ifdef DEBUG_ENABLE_OPASSERT
		BOOL listener_handled_event = FALSE;
#endif // DEBUG_ENABLE_OPASSERT
		for(UINT32 i = 0; i < listeners.GetCount(); i++)
		{
			OP_STATUS status = listeners.Get(i)->OnSubmit(info);
			// Now |info| may be deleted since it's the job of the listener to call ReportAction which deletes it. That can be done synchronously in OnSubmit.
#ifdef DEBUG_ENABLE_OPASSERT
			OP_ASSERT(!listener_handled_event || status == OpStatus::ERR_NOT_SUPPORTED || !"Only one listener must handle the event.");
			listener_handled_event = listener_handled_event || status != OpStatus::ERR_NOT_SUPPORTED;
#endif // DEBUG_ENABLE_OPASSERT
			RETURN_IF_MEMORY_ERROR(status);
		}
#ifdef DEBUG_ENABLE_OPASSERT
		OP_ASSERT(listener_handled_event);
#endif // DEBUG_ENABLE_OPASSERT

		// Now |info| may be deleted since it's the job of the listener to call ReportAction which deletes it. That can be done synchronously in OnSubmit.
	}

	return OpStatus::OK;
}

OP_STATUS WandManager::SubmitPage(FramesDocument* doc, HTML_Element* he_submit, HTML_Element* he_form,
                                  int offset_x, long offset_y,
                                  int document_x, long document_y,
                                  ShiftKeyState modifiers,
                                  BOOL handle_directly,
                                  ES_Thread *interrupt_thread,
								  BOOL synthetic /* = FALSE */
								  )
{
	OP_ASSERT(doc);
	OP_ASSERT(he_form);

	if (is_active &&
		!IsWandOperationInProgress(doc->GetWindow())) //Don't try to save data twice (resulting in 2 wand dialogs)
	{
		OpAutoPtr<WandPage> page(NULL);
		WandPage *existingpage;
		UINT32 i = 0;

		// If this page was previously stored with the NEVER_ON_THIS_PAGE just submit and return.
		BOOL allow_save = TRUE;
		while ((existingpage = log_profile.FindPage(doc, i)) != 0)
		{
			if (existingpage && existingpage->GetNeverOnThisPage())
			{
				allow_save = FALSE;
				break;
			}
			i++;
		}

		if (allow_save)
		{
			page.reset(OP_NEW(WandPage, ()));
			if (!page.get() ||
				OpStatus::IsError(page->SetUrl(doc, he_form)))
			{
				return OpStatus::ERR_NO_MEMORY;
			}

#ifdef SYNC_HAVE_PASSWORD_MANAGER
			RETURN_IF_ERROR(page->SetModifiedDate());
			RETURN_IF_ERROR(page->AssignSyncId());
#endif // SYNC_HAVE_PASSWORD_MANAGER

			RETURN_IF_ERROR(page->CollectPlainTextDataFromDocument(doc, he_submit, he_form, offset_x, offset_y, document_x, document_y));

			// Leave page to MaybeStorePageInDatabase which might delete it.
			RETURN_IF_ERROR(MaybeStorePageInDatabase(doc->GetWindow(), page.release()));
		}
	}

	if (handle_directly)
	{
		HTML_Element::HandleEventOptions opts;
		opts.related_target = he_submit;
		opts.offset_x = offset_x;
		opts.offset_y = offset_y;
		opts.document_x = document_x;
		opts.document_y = document_y;
		opts.sequence_count_and_button_or_key_or_delta = MOUSE_BUTTON_1;
		opts.modifiers = modifiers;
		opts.thread = interrupt_thread;
		opts.synthetic = synthetic;
		he_form->HandleEvent(ONSUBMIT, doc, he_form, opts);
		return OpStatus::OK;
	}
	else
		return doc->HandleMouseEvent(ONSUBMIT, he_submit, he_form, NULL,
		                             offset_x, offset_y,
		                             document_x, document_y,
		                             modifiers, MOUSE_BUTTON_1,
		                             interrupt_thread);
}

OP_STATUS WandManager::Save(OpFile &file)
{
	OpSafeFile safe;
	RETURN_IF_ERROR(safe.Construct(&file));

	RETURN_IF_ERROR(safe.Open(OPFILE_WRITE));

	RETURN_IF_ERROR(safe.WriteBinLong(WANDFILEVERSION));
	RETURN_IF_ERROR(safe.WriteBinLong(static_cast<long>(is_security_encrypted)));
	RETURN_IF_ERROR(safe.WriteBinLong(0)); // This used to (version <= 4) be CountProfiles() but old Operas hang if this value is too large
	RETURN_IF_ERROR(safe.WriteBinLong(curr_profile));
	// Here we used to (version <= 4) have all profiles
	RETURN_IF_ERROR(safe.WriteBinLong(0)); // This used to (version <= 4) be Profile with page counts
	RETURN_IF_ERROR(safe.WriteBinLong(0)); // This used to (version <= 4) be Profile with page counts
	RETURN_IF_ERROR(safe.WriteBinLong(0)); // This used to (version <= 4) be Profile with page counts
	RETURN_IF_ERROR(safe.WriteBinLong(0)); // This used to (version <= 4) be CountLogins() but old Operas hang if this value is too large
	// Here we used to (version <= 4) have all logins
	OP_ASSERT(CountProfiles() <= 1); // There shouldn't be more than 1 custom profiles (personal info)
	RETURN_IF_ERROR(safe.WriteBinLong(CountProfiles()));

	int i;
	for(i = 0; i < CountProfiles(); i++)
	{
		RETURN_IF_ERROR(GetProfile(i)->Save(safe));
	}
	RETURN_IF_ERROR(log_profile.Save(safe));

	// Logins
	RETURN_IF_ERROR(safe.WriteBinLong(logins.GetCount()));
	for(i = 0; i < (int)logins.GetCount(); i++)
	{
		RETURN_IF_ERROR(logins.Get(i)->Save(safe));
	}

	RETURN_IF_ERROR(safe.Flush());
	return safe.SafeClose();
}

class FileCloser
{
	OpFile& m_file;
public:
	FileCloser(OpFile& file) : m_file(file) {}
	~FileCloser() { m_file.Close(); }
};

OP_STATUS WandManager::Open(OpFile &file, BOOL during_startup, BOOL store_converted_file)
{
	OP_NEW_DBG("WandManager::Open()", "wand");
	OP_DBG((UNI_L("file: '%s'%s"), file.GetFullPath(), during_startup?UNI_L("; during startup"):UNI_L("")));

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	SyncBlocker block_sync; // Don't sync add events while loading items
#endif // SYNC_HAVE_PASSWORD_MANAGER

	volatile BOOL create_default = FALSE;
	long version = -1;
	BOOL exists;
	RETURN_IF_ERROR(file.Exists(exists));

	if (exists && OpStatus::IsSuccess(file.Open(OPFILE_READ)))
	{
		FileCloser file_closer(file); // Close file when we leave this block
		// Remember where we read from in case we have to resume the opening process later on
		if (&file != &m_source_database_file)
			RETURN_IF_ERROR(m_source_database_file.Copy(&file));

		RETURN_IF_ERROR(file.ReadBinLong(version));
		{
			long tmp;
			RETURN_IF_ERROR(file.ReadBinLong(tmp));
			is_security_encrypted = tmp != 0;
		}

#ifdef OPERA_BIG_ENDIAN
		WandSecurityWrapper security;
		if (is_security_encrypted)
		{
			Window* window = g_windowManager->FirstWindow();
			if (version <= 2 && during_startup || !window)
			{
				// Can't throw up questions about master passwords at startup.
				// Have to delay this somewhat (1 second)
				g_main_message_handler->SetCallBack(this, MSG_WAND_DELAYED_FILE_OPEN, 0);
				g_main_message_handler->PostMessage(MSG_WAND_DELAYED_FILE_OPEN, 0, 0, 1000);
				return OpStatus::ERR_YIELD;
			}
			// Must initiate a passwordsession, since all passwords will be re-encrypted as little endian during loading.

			OP_STATUS status = security.Enable(window);
			if (status == OpStatus::ERR_YIELD)
			{
				// Have to do this again some time
				RETURN_IF_ERROR(SetSuspendedOperation(SuspendedWandOperation::WAND_OPERATION_OPEN_DB,
					window, NULL, NO));
				return OpStatus::OK; // The caller doesn't have to know that we're not quite done yet
			}
			else if (OpStatus::IsError(status))
			{
				return status;
			}
		}
#endif // OPERA_BIG_ENDIAN

//		WandAutoPasswordSession pwd_session; // FIXME - this is only necessary for big endian and version 2

		if (version <= WANDFILEVERSION && version >= 2) // From version 2, we should be backwards-compatible
		{
			long num_profiles = 0, tmp_curr_profile, num_logins = 0, dummy;
			if (version >= 5)
			{
				RETURN_IF_ERROR(file.ReadBinLong(dummy));
				RETURN_IF_ERROR(file.ReadBinLong(tmp_curr_profile));
				RETURN_IF_ERROR(file.ReadBinLong(dummy));
				RETURN_IF_ERROR(file.ReadBinLong(dummy));
				RETURN_IF_ERROR(file.ReadBinLong(dummy));
				RETURN_IF_ERROR(file.ReadBinLong(dummy));
				RETURN_IF_ERROR(file.ReadBinLong(num_profiles));
			}
			else
			{
				// wand file version 2-4
				RETURN_IF_ERROR(file.ReadBinLong(num_profiles));
				RETURN_IF_ERROR(file.ReadBinLong(tmp_curr_profile));
			}
			OP_ASSERT(num_profiles <= 1);
			for(int i = 0; i < num_profiles; i++)
			{
				if(file.Eof())
				{
					//something is wrong like ridiculously high num_profiles due to corrupt file.
					create_default = TRUE;
#ifdef SYNC_HAVE_PASSWORD_MANAGER
					ClearAll(TRUE, FALSE, FALSE, TRUE);
#else
					ClearAll();
#endif // SYNC_HAVE_PASSWORD_MANAGER
					break;
				}
				RETURN_IF_ERROR(CreateProfile());
				RETURN_IF_ERROR(GetProfile(i)->Open(file, version));
			}
			RETURN_IF_ERROR(log_profile.Open(file, version));
			curr_profile = tmp_curr_profile;

			// Logins
			RETURN_IF_ERROR(file.ReadBinLong(num_logins));
			if (!file.Eof()) ///< Backward compability
			{
				for(int i = 0; i < num_logins; i++)
				{
					if(file.Eof())
					{
						//something is wrong like ridiculously high num_logins due to corrupt file.
						create_default = TRUE;
#ifdef SYNC_HAVE_PASSWORD_MANAGER
						ClearAll(TRUE, FALSE, FALSE, TRUE);
#else
						ClearAll();
#endif // SYNC_HAVE_PASSWORD_MANAGER
						break;
					}
					WandLogin* new_login = OP_NEW(WandLogin, ());
					if (!new_login || OpStatus::IsError(logins.Add(new_login)))
					{
						OP_DELETE(new_login);
						return OpStatus::ERR_NO_MEMORY;
					}
					new_login->Open(file, version);
					OnWandLoginAdded(logins.GetCount() - 1);
				}
			}
		}
		else
			create_default = TRUE;

		if (create_default)
		{
			/* The wand-file is corrupted somehow: so before we overwrite it
			 * with an empty initial wand-file, create a copy of the corrupted
			 * file. Maybe there is some magician who can restore the data... */
			OpFile copy;
			OpString backupFilename;
			if (OpStatus::IsSuccess(backupFilename.Set(file.GetFullPath())) &&
				OpStatus::IsSuccess(backupFilename.Append(UNI_L(".save"))) &&
				OpStatus::IsSuccess(copy.Construct(backupFilename.CStr())))
				OpStatus::Ignore(copy.CopyContents(&file, FALSE));
		}
		// Here FileCloser goes out of scope and closes file
	}
	else
		create_default = TRUE;

	if (create_default)
	{
		is_security_encrypted = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword);

		// Set up the log profile
		log_profile.SetName(UNI_L("Log profile"));

		// Set up a default personal profile
		CreateProfile();
		WandProfile* profile = GetProfile(0);
		profile->CreatePage(NULL, NULL);
		profile->SetName(UNI_L("Personal profile"));
#ifdef WAND_ECOMMERCE_SUPPORT
		WandPage* page = profile->pages.Get(0);
		page->AddObjectInfo(UNI_L("Ecom_SchemaVersion"), UNI_L("http://www.ecml.org/version/1.1"), FALSE, FALSE, TRUE, FALSE);
#endif // WAND_ECOMMERCE_SUPPORT
	}

	// If we read an file having old format, we store the file in new format.
	if (store_converted_file && version >= 0 && version < WANDFILEVERSION )
		WandManager::Save(file);
	return OpStatus::OK;
}

#ifdef SYNC_HAVE_PASSWORD_MANAGER
OP_STATUS WandManager::ClearAll(BOOL include_opera_logins /* = TRUE */, BOOL also_clear_passwords_on_server /* = FALSE */, BOOL stop_password_sync /* = TRUE */, BOOL reset_sync_state /* = TRUE */)
#else
	OP_STATUS WandManager::ClearAll(BOOL include_opera_logins /* = TRUE */)
#endif // SYNC_HAVE_PASSWORD_MANAGER
{
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	SyncBlocker blocker; // Will prevent delete sync messages from been sent to server

	if (also_clear_passwords_on_server)
		blocker.ResetBlocker(); // we send the delete events after all

	if (reset_sync_state || stop_password_sync)
		g_sync_coordinator->ResetSupportsState(SYNC_SUPPORTS_PASSWORD_MANAGER, stop_password_sync);
#endif // SYNC_HAVE_PASSWORD_MANAGER

	log_profile.DeleteAllPages();

	for (UINT32 i = logins.GetCount(); i > 0; i--)
	{
		if (include_opera_logins || logins.Get(i-1)->id.Compare("opera:", 6) != 0)
		{
			RemoveLogin(i - 1);
		}
	}

	return StoreWandInfo();
}

OP_STATUS WandManager::CreateProfile()
{
	OP_ASSERT(CountProfiles() <= 0);
	WandProfile* profile = OP_NEW(WandProfile, (this));
	if (profile == NULL)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(profile->SetName(UNI_L("New profile"))) ||
		OpStatus::IsError(profiles.Add(profile)))
	{
		OP_DELETE(profile);
		return OpStatus::ERR_NO_MEMORY;
	}

	OP_ASSERT(CountProfiles() <= 1);

	return OpStatus::OK;
}

OP_STATUS WandManager::Store(Window* window, WandPage* page)
{	
	OP_ASSERT(window);
	OP_ASSERT(page);

	OpAutoPtr<WandPage> page_owner(page);
	if (!is_active)
		return OpStatus::OK;

	if (page->GetNeverOnThisPage())
	{
		// No need to request password. Just remove all WandObjectInfo objects and
		// store the empty page
		page->Clear();
		RETURN_IF_ERROR(log_profile.Store(page_owner.release()));
		return StoreWandInfo();
	}

	WandSecurityWrapper security;
	OP_STATUS status = security.Enable(window);
	if (status == OpStatus::ERR_YIELD)
	{
		// Have to do this again some time
		RETURN_IF_ERROR(SetSuspendedPageOperation(SuspendedWandOperation::WAND_OPERATION_STORE_PAGE,
			window,
			NULL, NO, page, TRUE));
		page_owner.release();
		return OpStatus::OK; // The caller doesn't have to know that we're not quite done yet
	}
	else if (OpStatus::IsError(status))
	{
		return status;
	}

#ifdef WAND_ECOMMERCE_SUPPORT
	if ((page->flags & WAND_FLAG_STORE_NOTHING_BUT_ECOMMERCE) == 0)
#endif // WAND_ECOMMERCE_SUPPORT
	{
		RETURN_IF_ERROR(page->Encrypt());
		OP_STATUS store_status = log_profile.Store(page_owner.release());

		RETURN_IF_ERROR(store_status); // If there was an error, page will have been deleted
	}

#ifdef WAND_ECOMMERCE_SUPPORT
	if (page->flags & WAND_FLAG_STORE_ECOMMERCE)
	{
		WandProfile* personal_profile = GetProfile(0);
		OP_ASSERT(personal_profile->type == WAND_PERSONAL_PROFILE);
		personal_profile->StoreECommerce(page/*he_form*/);
	}
#endif // WAND_ECOMMERCE_SUPPORT

	return StoreWandInfo();
}

#ifdef WAND_EXTERNAL_STORAGE

BOOL FindTargetForms(FormObject **userobj, FormObject **passobj, FormObject **submitobj,
					HTML_Element **formelm, FramesDocument* doc)
{
	HTML_Element *helm = doc->GetHTML_LogDoc() ? doc->GetHTML_LogDoc()->GetRoot() : NULL;
	while(helm)
	{
		if (helm->Type() == HE_FORM)
		{
			*userobj = NULL;
			*passobj = NULL;
			*submitobj = NULL;
			*formelm = helm;
		}
		if (helm->Type() == HE_INPUT)
		{
			if (helm->GetInputType() == INPUT_TEXT)
				*userobj = helm->GetFormObject();
			if (helm->GetInputType() == INPUT_PASSWORD)
				*passobj = helm->GetFormObject();
			if (helm->GetInputType() == INPUT_SUBMIT)
				*submitobj = helm->GetFormObject();
		}
		if (*userobj && *passobj)
		{
			return TRUE;
		}
		helm = (HTML_Element*) helm->NextActual();
	}
	return FALSE;
}

void WandManager::UpdateMatchingFormsFromExternalStorage(FramesDocument* doc)
{
	if (doc->GetHasWandMatches())
		return; // Already has matches, fetched from the real wand storage.

	OpString docurl;
	if (OpStatus::IsError(GetWandUrl(doc, docurl)))
		return;

	FormObject *userobj = NULL, *passobj = NULL, *submitobj = NULL;
	HTML_Element *formelm = NULL;
	if (FindTargetForms(&userobj, &passobj, &submitobj, &formelm, doc))
	{
		WandDataStorageEntry entry;
		if (data_storage->FetchData(docurl, &entry, 0))
		{
			userobj->SetExistInWand(WAND_HAS_MATCH_IN_STOREDPAGE);
			passobj->SetExistInWand(WAND_HAS_MATCH_IN_STOREDPAGE);
			doc->SetHasWandMatches(TRUE);
		}
	}
}

OP_STATUS WandManager::UseExternalStorage(FramesDocument* doc, INT32 index)
{
	if (data_storage)
	{
		OpString docurl;
		RETURN_IF_ERROR(GetWandUrl(doc, docurl));

		WandDataStorageEntry entry;

		if (data_storage->FetchData(docurl, &entry, index))
		{
			FormObject *userobj = NULL, *passobj = NULL, *submitobj = NULL;
			HTML_Element *formelm = NULL;
			if (FindTargetForms(&userobj, &passobj, &submitobj, &formelm, doc))
			{
				SetFormObjectValue(userobj, entry.username);
				SetFormObjectValue(passobj, entry.password);

#if 1 // Not Tested yet.
				if (submitobj && formelm)
				{
					OP_STATUS status = doc->HandleMouseEvent(ONSUBMIT, submitobj->GetHTML_Element(), formelm, NULL,
															 0, 0, 0, 0, 0, MOUSE_BUTTON_1, NULL);
					if (OpStatus::IsMemoryError(status))
						doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
				}
				else if (formelm)
				{
					formelm->HandleEvent(ONSUBMIT, doc, NULL, formelm, NULL,
										 0, 0, 0, 0, MOUSE_BUTTON_1, 0, FALSE, FALSE);
				}
#endif
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS WandManager::DeleteFromExternalStorage(FramesDocument* doc, const uni_char* id)
{
	OP_ASSERT(id);
	OpString docurl;
	RETURN_IF_ERROR(GetWandUrl(doc, docurl));

	if(data_storage)
	{
		RETURN_IF_ERROR(data_storage->DeleteData(docurl, id));
	}

	return OpStatus::OK;
}

#endif // WAND_EXTERNAL_STORAGE

OP_STATUS WandManager::InsertWandDataInDocument(FramesDocument* doc, HTML_Element* he, BOOL3 submit /* = MAYBE */, BOOL only_if_matching_username /* = FALSE */)
{
	if (!is_active || IsUsing(doc))
		return OpStatus::OK;

	WandSecurityWrapper security;
	OP_STATUS status = security.Enable(doc->GetWindow());
	if (status == OpStatus::ERR_YIELD)
	{
		// Have to do this again some time
		RETURN_IF_ERROR(SetSuspendedOperation(SuspendedWandOperation::WAND_OPERATION_USE,
			doc->GetWindow(),
			doc, submit));
		return OpStatus::OK; // The caller doesn't have to know that we're not quite done yet
	}
	else if (OpStatus::IsError(status))
	{
		return status;
	}

	// If the paranoidsetting has been changed "by hand" in the
	// prefsinifile, this will update the data.
	UpdateSecurityStateInternal(doc->GetWindow(), TRUE);

	doc = FindWandDoc(doc);
	if (doc == NULL)
		return OpStatus::OK;

	OpVector<WandPage> matching_pages;
#ifdef WAND_EXTERNAL_STORAGE
	BOOL using_personal_profile = FALSE;
#endif // WAND_EXTERNAL_STORAGE

	WandProfile* profile = &log_profile;
	if (log_profile.FindPage(doc) == NULL && CountProfiles() > 0)
	{
		profile = profiles.Get(curr_profile);
		if(profile->FindPage(doc, 0))
		{
			RETURN_IF_ERROR(matching_pages.Add(profile->FindPage(doc, 0))); // always 1 page in the personal profiles
#ifdef WAND_EXTERNAL_STORAGE
			using_personal_profile = TRUE;
#endif // WAND_EXTERNAL_STORAGE
		}
	}
	else
	{
		INT32 i = 0;
		WandPage* page = NULL;
		while((page = profile->FindPage(doc, i)) != NULL)
		{
			if (!page->GetOnThisServer() || (page->GetOnThisServer() && page->CountMatches(doc) > 0))
			{
				RETURN_IF_ERROR(matching_pages.Add(page));
			}
			i++;
		}
	}

	INT32 num_external_pages = 0;
#ifdef WAND_EXTERNAL_STORAGE
	// Check in external wandstorage first.
	if (data_storage)
	{
		OpString docurl;
		RETURN_IF_ERROR(GetWandUrl(doc, docurl));

		WandDataStorageEntry entry;
		while (data_storage->FetchData(docurl, &entry, num_external_pages))
		{
			if (!using_personal_profile)
			{
				// Don't include matches from wand.dat that are equal
				// to the imported.
				for(int i = 0; i < matching_pages.GetCount(); i++)
				{
					OpString username;
					username.Set(matching_pages.Get(i)->FindUserName());
					if (username == entry.username)
					{
						matching_pages.Remove(i--);
					}
				}
			}
			num_external_pages++;
		}

		if (num_external_pages && using_personal_profile)
			matching_pages.Remove(0);
	}
#endif // WAND_EXTERNAL_STORAGE

	// Several pages. or if wand is requested and the page is marked
	// "never on this page".  Run WandListener so user can select
	// which one to use or delete it.
	if (matching_pages.GetCount() + num_external_pages > 1 ||
		(matching_pages.GetCount() == 1 && matching_pages.Get(0)->GetNeverOnThisPage()))
	{
		WandMatchInfo* matchinfo = OP_NEW(WandMatchInfo, (doc, profile));
		if (matchinfo == NULL)
			return OpStatus::ERR_NO_MEMORY;

		UINT32 i;
		for(i = 0; i < matching_pages.GetCount(); i++)
		{
			WandPage* page = matching_pages.Get(i);

			const uni_char* user_name;
			OpString user_name_obj;
			if (page->FindUserName(user_name, doc) != WandPage::FIND_USER_OK)
			{
#if LANGUAGE_DATABASE_VERSION > 884
				// Pages with no username, how to show to the user?
				if (OpStatus::IsSuccess(g_languageManager->GetString(Str::S_WAND_USER_NAME_WHEN_USER_NAME_IS_MISSING, user_name_obj)))
				{
					user_name = user_name_obj.CStr();
				}
				else
				{
					OP_ASSERT(!"Missing string in the translation: S_WAND_USER_NAME_WHEN_USER_NAME_IS_MISSING");
					user_name = NULL;
				}
#else
				user_name = UNI_L("<no username found>");
#endif
			}
			WandMatchInfo::WandMatch* match = OP_NEW(WandMatchInfo::WandMatch, ());
			if (match &&
				OpStatus::IsSuccess(match->string.Set(user_name)) &&
				OpStatus::IsSuccess(matchinfo->m_matches.Add(match)))
			{
				match->page = page;
			}
			else
			{
				OP_DELETE(match);
				// FIX: Might want to report OOM if that was what happened
			}
		}

#ifdef WAND_EXTERNAL_STORAGE
		if (data_storage)
		{
			OpString docurl;
			RETURN_IF_ERROR(GetWandUrl(doc, docurl));

			num_external_pages = 0;
			WandDataStorageEntry entry;
			while (data_storage->FetchData(docurl, &entry, num_external_pages))
			{
				WandMatchInfo::WandMatch* match = OP_NEW(WandMatchInfo::WandMatch, ());
				if (match &&
					OpStatus::IsSuccess(match->string.Set(entry.username)) &&
					OpStatus::IsSuccess(matchinfo->matches.Add(match)))
				{
					match->page = NULL;
					match->index = num_external_pages;
					num_external_pages++;
				}
				else
				{
					OP_DELETE(match);
					// FIX: Might want to report the OOM
				}
			}
		}
#endif // WAND_EXTERNAL_STORAGE

		if (listeners.GetCount() == 0)
			matchinfo->ProcessMatchSelectionAndDelete(0);
		else
		{
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL listener_handled_event = FALSE;
#endif // DEBUG_ENABLE_OPASSERT
			for(i = 0; i < listeners.GetCount(); i++)
			{
				OP_STATUS listener_status = listeners.Get(i)->OnSelectMatch(matchinfo);
#ifdef DEBUG_ENABLE_OPASSERT
				OP_ASSERT(!listener_handled_event || listener_status == OpStatus::ERR_NOT_SUPPORTED || !"Only one listener must handle the event.");
				listener_handled_event = listener_handled_event || listener_status != OpStatus::ERR_NOT_SUPPORTED;
#endif // DEBUG_ENABLE_OPASSERT
				RETURN_IF_MEMORY_ERROR(listener_status);
			}
#ifdef DEBUG_ENABLE_OPASSERT
			OP_ASSERT(listener_handled_event);
#endif // DEBUG_ENABLE_OPASSERT
		}
	}
	else if (matching_pages.GetCount() == 1) // One page. Fetch it!
	{
		return Fetch(doc, he, matching_pages.Get(0), submit, only_if_matching_username);
	}
#ifdef WAND_EXTERNAL_STORAGE
	else if (num_external_pages == 1)
	{
		UseExternalStorage(doc, 0);
	}
#endif

	return OpStatus::OK;
}

BOOL WandManager::Usable(FramesDocument* doc, BOOL/* unused = FALSE */)
{
	doc = FindWandDoc(doc);

	if (doc)
	{
		if (!log_profile.FindPage(doc) && CountProfiles() > 0)
		{
			if(profiles.Get(curr_profile)->FindPage(doc, 0))
				return TRUE;
		}
		else
		{
			INT32 i = 0;
			WandPage* page = NULL;
			while((page = log_profile.FindPage(doc, i)) != NULL)
			{
				if (!page->GetOnThisServer() || (page->GetOnThisServer() && page->CountMatches(doc) > 0))
				{
					return TRUE;
				}
				i++;
			}
		}

#ifdef WAND_EXTERNAL_STORAGE
		OpString docurl;
		WandDataStorageEntry entry;
		RETURN_IF_ERROR(GetWandUrl(doc, docurl));
		return data_storage->FetchData(docurl, &entry, 0);
#endif//WAND_EXTERNAL_STORAGE
	}

	return doc != NULL;
}

void WandManager::AddListener(WandListener* listener)
{
#ifdef DEBUG_ENABLE_OPASSERT
	for(UINT32 i = 0; i < listeners.GetCount(); i++)
		OP_ASSERT(listeners.Get(i) != listener);

	OP_STATUS status =
#endif // DEBUG_ENABLE_OPASSERT
		listeners.Add(listener); // Fixme: If we can't add a listener it's possible that all submits will hang since we need a listener to push us forward
#ifdef DEBUG_ENABLE_OPASSERT
	OP_ASSERT(OpStatus::IsSuccess(status) || !"OOM when registering wand listener, possible that form submits will silently hang if only one of several listeners were added");
#endif // DEBUG_ENABLE_OPASSERT
}

void WandManager::RemoveListener(WandListener* listener)
{
	for(UINT32 i = 0; i < listeners.GetCount(); i++)
		if (listeners.Get(i) == listener)
		{
			listeners.Remove(i);
			return;
		}
}

OP_STATUS WandManager::Fetch(FramesDocument* doc, HTML_Element* he, WandPage* page, BOOL3 submit, BOOL only_if_matching_username)
{
	if (!is_active)
		return OpStatus::OK;

	WandSecurityWrapper security;
	OP_STATUS status = security.Enable(doc->GetWindow());
	if (status == OpStatus::ERR_YIELD)
	{
		// Have to do this again some time
		RETURN_IF_ERROR(SetSuspendedPageOperation(SuspendedWandOperation::WAND_OPERATION_FETCH_PAGE,
			doc->GetWindow(),
			doc, submit, page, FALSE));
		return OpStatus::OK; // The caller doesn't have to know that we're not quite done yet
	}
	else if (OpStatus::IsError(status))
	{
		return status;
	}

	BOOL auto_submit = submit == NO ? FALSE: TRUE;
	if (submit == MAYBE && !g_pccore->GetIntegerPref(PrefsCollectionCore::WandAutosubmit))
		auto_submit = FALSE;

	if (!page)
	{
		OP_ASSERT(FALSE); // Why, oh why?
		return OpStatus::ERR_NULL_POINTER;
	}

	int num_matching;
	RETURN_IF_ERROR(page->Fetch(doc, he, num_matching, only_if_matching_username));



	HTML_Element *submit_he = page->FindTargetSubmit(doc);
	HTML_Element *form_helm = page->FindTargetForm(doc);

	// Submit automatically
	if (form_helm && num_matching >= 1 && auto_submit)
	{
		ShiftKeyState modifiers = 0;

/*		For now, don't send with modifiers, to avoid Shift-X as fastforward causing new page
		if (doc->GetVisualDevice() && doc->GetVisualDevice()->GetView())
			modifiers = doc->GetVisualDevice()->GetView()->GetShiftKeys();*/

		// Submit it!
/*		if (handle_directly)   Todo: we may need to store handle_directly so we can do this when fetching.
		{
			form_helm->HandleEvent(ONSUBMIT, doc, submit_he, form_helm, NULL,
							 page->offset_x, page->offset_y, page->document_x, page->document_y,
							 MOUSE_BUTTON_1, modifiers, FALSE, FALSE);
		}
		else
		{*/

		// If we found an enabled submit target then click
		// on it to handle
		// input that is changed in javascript event handlers, else
		// do the submit directly, bypassing much of the script code.
		if (submit_he && !submit_he->IsDisabled(doc))
		{
			if (doc->HandleMouseEvent(ONCLICK, NULL, submit_he, NULL,
									  page->offset_x, page->offset_y,
									  page->document_x, page->document_y,
									  modifiers, MOUSE_BUTTON_1) == OpStatus::ERR_NO_MEMORY)
				doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}
		else
		{
			if (doc->HandleMouseEvent(ONSUBMIT, submit_he, form_helm, NULL,
									  page->offset_x, page->offset_y,
									  page->document_x, page->document_y,
									  modifiers, MOUSE_BUTTON_1) == OpStatus::ERR_NO_MEMORY)
				doc->GetWindow()->RaiseCondition(OpStatus::ERR_NO_MEMORY);
		}

		//}
	}
	else if (submit_he)
	{
		// Atleast focus the element that was used to submit, so it's easily done by the user.
		HTML_Document* htmldoc = doc->GetHtmlDocument();
		if (htmldoc && htmldoc->GetVisualDevice())
		{
			htmldoc->HighlightElement(submit_he, HTML_Document::FOCUS_ORIGIN_KEYBOARD, TRUE, TRUE);
			htmldoc->GetVisualDevice()->SetDrawFocusRect(TRUE);
		}
	}

	return OpStatus::OK;
}

int WandManager::HasMatch(FramesDocument* doc, HTML_Element* he)
{
	OP_ASSERT(doc);
	OP_ASSERT(he);

	if (!is_active)
		return 0;

	WandObjectInfo* object_info = log_profile.FindMatch(doc, he);
	if (object_info)
		return WAND_HAS_MATCH_IN_STOREDPAGE;
	if (CountProfiles() > 0 && !log_profile.FindPage(doc))
	{
#ifdef WAND_ECOMMERCE_SUPPORT
		if (WandPage *wand_page = profiles.Get(curr_profile)->GetWandPage(0))
		{
			if (wand_page->HasECommerceMatch(he))
				return WAND_HAS_MATCH_IN_ECOMMERCE;
		}
#endif // WAND_ECOMMERCE_SUPPORT
	}
	return 0;
}

/* static */
BOOL WandManager::SetFormValue(FramesDocument* doc, HTML_Element* he, const uni_char* value)
{
	if (he->GetInputType() == INPUT_FILE)
		return FALSE;

	FormValue* form_value = he->GetFormValue();
	OP_ASSERT(form_value);
	if (form_value->GetType() == FormValue::VALUE_RADIOCHECK)
	{
		FormValueRadioCheck* radiocheck_value = FormValueRadioCheck::GetAs(form_value);
		const uni_char* he_value = he->GetStringAttr(ATTR_VALUE);
		if (he_value ? uni_stri_eq(he_value, value) : uni_str_eq(value, "on"))
			radiocheck_value->SetIsChecked(he, TRUE, doc, TRUE); // Don't trigger changes in other connected buttons
	}
	else if (form_value->GetType() == FormValue::VALUE_LIST_SELECTION)
	{
		FormValueList* list = FormValueList::GetAs(form_value);

		const uni_char *tmp = value;
		int selIndex = uni_atoi(tmp);
		BOOL select_none = FALSE;
		if(*tmp == 0)
		{
			select_none = TRUE;
		}

		int num = list->GetOptionCount(he);
		for (int i=0; i<num; i++)
		{
			BOOL selected = (i == selIndex);
			if(select_none)
				selected = FALSE;
			list->SelectValue(he, i, selected);
			if (selected && tmp)
			{
				tmp = uni_strchr(tmp, uni_char(','));
				if (tmp)
				{
					tmp++;
					selIndex = uni_atoi(tmp);
				}
				else
				{
					selIndex = -1;
				}
			}
		}
	}
	else
	{
		form_value->SetValueFromText(he, value);
	}
#if !defined(PREFILLED_FORM_WAND_SUPPORT)
	// Remove all indications of the match since it has been used,
	// unless it was auto filled in which case we still want
	// the user to be aware of the fields.
	form_value->SetExistsInWand(0);
#endif//!PREFILLED_FORM_WAND_SUPPORT

	FormValueListener::HandleValueChanged(doc, he, FALSE, FALSE, NULL);

	return TRUE;
}

/* private */
OP_STATUS WandManager::StoreLoginInternal(Window* window, WandLogin* login)
{
	WandSecurityWrapper security;
	OP_STATUS status = window ? security.Enable(window) : security.EnableWithoutWindow();
	if (status == OpStatus::ERR_YIELD)
	{
		// Have to do this again some time
		status = SetSuspendedLoginOperation(window ? SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN : SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN_NO_WINDOW,
			window, NULL, NO, login, NULL);

		if (OpStatus::IsError(status))
			OP_DELETE(login);

		return status;
	}

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	RETURN_IF_ERROR(login->AssignSyncId());
	if (login->GetModifiedDateMilliseconds() == 0)
		RETURN_IF_ERROR(login->SetModifiedDate()); // if sync_modified is NULL, the current date is used
#endif // SYNC_HAVE_PASSWORD_MANAGER
	login->EncryptPassword();

#ifdef WAND_EXTERNAL_STORAGE
	WandDataStorageEntry entry;
	entry.Set(username, password);
	if (data_storage)
		data_storage->StoreData(id, &entry);
#endif // WAND_EXTERNAL_STORAGE
	WandLogin* previous_login = FindLogin(login->id, login->username);

	if (previous_login)
	{
		if (previous_login->IsEqualTo(login))
		{
			OP_DELETE(login);
			return OpStatus::OK;
		}

		INT32 index = logins.Find(previous_login);
#ifdef SYNC_HAVE_PASSWORD_MANAGER
		if (login->GetLocalSyncStatus() == WandSyncItem::SYNCED && !g_wand_manager->GetBlockOutgoingSyncMessages())
			login->SetLocalSyncStatus(WandSyncItem::SYNC_MODIFY);

		SyncBlocker block_sync; // don't sync this event
#endif // SYNC_HAVE_PASSWORD_MANAGER
		RemoveLogin(index);
	}

	if (OpStatus::IsError(logins.Add(login)))
	{
		OP_DELETE(login);
		return OpStatus::ERR_NO_MEMORY;
	}

	OnWandLoginAdded(logins.GetCount() - 1);


	return StoreWandInfo();
}


OP_STATUS WandManager::StoreLoginCommon(Window* window, const uni_char* _id, const uni_char* username, const uni_char* password)
{
	OpString id;
	RETURN_IF_ERROR(id.Set(_id));
#ifdef WAND_EXTERNAL_STORAGE
	if (data_storage)
	{
		WandDataStorageEntry entry;
		INT32 i = 0;
		while(data_storage->FetchData(id, &entry, i++))
		{
			if (entry.username.Compare(username) == 0 && entry.password.Compare(password) == 0)
			{
				// It already exists in the external storage. We skip storing it in wand too since
				// the dialog would show duplicated entries when it shows up next time then.
				return OpStatus::OK;
			}
		}
	}
#endif // WAND_EXTERNAL_STORAGE

	// Look for an existing entry with the same id and username
	WandLogin *old_info = FindLogin(_id, username);
	WandLogin *new_info;

	if (old_info)
		new_info = old_info->Clone();
	else
		new_info = OP_NEW(WandLogin, ());

	if (!new_info)
		return OpStatus::ERR_NO_MEMORY;

	// First add the data non-encrypted, then encrypt when we have the
	// master password (if needed).
	OP_STATUS status = new_info->Set(id.CStr(), username, password, FALSE);
	if (OpStatus::IsError(status))
	{
		OP_DELETE(new_info);
		return status;
	}
#ifdef SYNC_HAVE_PASSWORD_MANAGER
	new_info->SetModifiedDate();
#endif // SYNC_HAVE_PASSWORD_MANAGER

	return StoreLoginInternal(window, new_info);
}

void WandManager::RemoveLogin(int index)
{
	WandLogin* login = logins.Get(index);
	OP_ASSERT(login);
	if (login)
	{
		OnWandLoginRemoved(index, login);
		WandLogin* removed_login = logins.Remove(index);
		OP_ASSERT(removed_login == login);
		OP_DELETE(removed_login);
	}
}

void WandManager::DeleteLogin(const uni_char* id, const uni_char* username)
{
	OP_ASSERT(id);
	OP_ASSERT(username);
	for(UINT32 i = 0; i < logins.GetCount(); i++)
		if (MatchingLoginID(logins.Get(i)->id, id) && logins.Get(i)->username.Compare(username) == 0)
		{
			RemoveLogin(i);
			break;
		}
	StoreWandInfo();

#ifdef WAND_EXTERNAL_STORAGE
	if (data_storage)
	{
		data_storage->DeleteData(id, username);
	}
#endif // WAND_EXTERNAL_STORAGE
}

void WandManager::DeleteLogin(int index)
{
	RemoveLogin(index);
	StoreWandInfo();

#ifdef WAND_EXTERNAL_STORAGE
	OP_ASSERT(NULL); // Needs to cal the external storage
#endif // WAND_EXTERNAL_STORAGE
}

OP_STATUS WandManager::DeleteLoginsWithIdPrefix(const char* id_prefix)
{
	OP_ASSERT(id_prefix && *id_prefix);
	int length = op_strlen(id_prefix);

	for (UINT32 i = logins.GetCount(); i > 0; i--)
	{
		if (logins.Get(i-1)->id.Compare(id_prefix, length) == 0)
		{
			RemoveLogin(i-1);
		}
	}

	return StoreWandInfo();
}

int WandManager::FindLoginInternal(const uni_char* id, INT32 index, BOOL include_external_data)
{
	OP_ASSERT(id);
	int nr = 0;
	for(UINT32 i = 0; i < logins.GetCount(); i++)
		if (MatchingLoginID(logins.Get(i)->id, id))
		{
			if (nr == index)
				return i;
			nr++;
		}

#ifdef WAND_EXTERNAL_STORAGE
	if (data_storage && include_external_data)
	{
		INT32 external_index = index - nr;
		WandDataStorageEntry entry;
		if (data_storage->FetchData(id, &entry, external_index))
		{
			tmp_wand_login.Init(id, entry.username, entry.password);
			return logins.GetCount();
		}
	}
#endif // WAND_EXTERNAL_STORAGE

	return -1;
}

WandLogin* WandManager::FindLogin(const uni_char* id, INT32 index, BOOL include_external_data)
{
	OP_ASSERT(id);
	int i = FindLoginInternal(id, index, include_external_data);
	if (i < 0)
	{
		return NULL;
	}

#ifdef WAND_EXTERNAL_STORAGE
	if (i == logins.GetCount())
	{
		return &tmp_wand_login;
	}
#endif // WAND_EXTERNAL_STORAGE

	return logins.Get(i);
}

WandLogin* WandManager::FindLogin(const uni_char* id, const uni_char *username)
{
	OP_ASSERT(id);
	OP_ASSERT(username);
	for(UINT32 i = 0; i < logins.GetCount(); i++)
	{
		if (MatchingLoginID(logins.Get(i)->id, id) && logins.Get(i)->username.Compare(username) == 0)
		{
			return logins.Get(i);
		}
	}
	return NULL;
}

WandLogin* WandManager::ExtractLogin(const uni_char* id, INT32 index, BOOL include_external_data)
{
	OP_ASSERT(id);
	int i = FindLoginInternal(id, index, include_external_data);
	if (i < 0)
	{
		return NULL;
	}

#ifdef WAND_EXTERNAL_STORAGE
	if (i == logins.GetCount())
	{
		return &tmp_wand_login;
	}
#endif // WAND_EXTERNAL_STORAGE

	WandLogin *login = logins.Get(i);
	RemoveLogin(i);
	return login;
}

WandLogin* WandManager::GetWandLogin(int index)
{
	return logins.Get(index);
}

WandPage* WandManager::GetWandPage(int index)
{
	return log_profile.GetWandPage(index);
}

void WandManager::DeleteWandPage(int index)
{
	log_profile.DeletePage(index);

	StoreWandInfo();
}

void WandManager::DeleteWandPages(FramesDocument* doc)
{
	while (log_profile.FindPage(doc, 0))
		log_profile.DeletePage(doc, 0);
	StoreWandInfo();
}

void WandManager::OnWandLoginAdded(int index)
{
	OP_NEW_DBG("WandManager::OnWandLoginAdded()", "wand");
	OP_DBG(("index: %d", index));

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	OpStatus::Ignore(logins.Get(index)->SyncItem());
#endif // SYNC_HAVE_PASSWORD_MANAGER

	for(UINT32 i = 0; i < listeners.GetCount(); i++)
		listeners.Get(i)->OnWandLoginAdded(index);
}

void WandManager::OnWandLoginRemoved(int index, WandLogin *removed_login)
{
	OP_NEW_DBG("WandManager::OnWandLoginRemoved()", "wand");
	OP_DBG(("index: %d", index));

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	OpStatus::Ignore(removed_login->SyncDeleteItem());
#endif // SYNC_HAVE_PASSWORD_MANAGER

	for(UINT32 i = 0; i < listeners.GetCount(); i++)
		listeners.Get(i)->OnWandLoginRemoved(index);
}

void WandManager::OnWandPageAdded(int index)
{
	OP_NEW_DBG("WandManager::OnWandPageAdded()", "wand");
	OP_DBG(("index: %d", index));

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	WandPage *page = log_profile.GetWandPage(index);

	if (!page->GetNeverOnThisPage())
		OpStatus::Ignore(page->SyncItem());
#endif // SYNC_HAVE_PASSWORD_MANAGER

	for(UINT32 i = 0; i < listeners.GetCount(); i++)
		listeners.Get(i)->OnWandPageAdded(index);
}

void WandManager::OnWandPageRemoved(int index, WandPage *removed_page)
{
	OP_NEW_DBG("WandManager::OnWandPageRemoved()", "wand");
	OP_DBG(("index: %d", index));

#ifdef SYNC_HAVE_PASSWORD_MANAGER
	OpStatus::Ignore(removed_page->SyncDeleteItem());
#endif // SYNC_HAVE_PASSWORD_MANAGER

	for(UINT32 i = 0; i < listeners.GetCount(); i++)
		listeners.Get(i)->OnWandPageRemoved(index);
}

// == Some stuff to handle the changepassword procedure =====================================

const WandPassword* WandManager::RetrieveDataItem(int index)
{
	int i, counter = 0;
	for(i = 0; i < (int)log_profile.pages.GetCount(); i++)
	{
		WandPage* page = log_profile.pages.Get(i);
		for(unsigned int j = 0; j < page->objects.GetCount(); j++)
		{
			WandObjectInfo* objinfo = page->objects.Get(j);
			if (objinfo->is_password)
			{
				if (counter == index)
					return &objinfo->password;
				counter++;
			}
		}
	}
	// Logins
	for(i = 0; i < (int)logins.GetCount(); i++, counter++)
	{
		if (counter == index)
			return &logins.Get(i)->password;
	}
	return NULL;
}

//#ifdef WAND_FUNCTIONS_NOT_MADE_ASYNC_YET
BOOL WandManager::PreliminaryStoreDataItem(int index, const unsigned char* data, UINT16 length)
{
	WandPassword* pwd = OP_NEW(WandPassword, ());
	if (pwd == NULL ||
		OpStatus::IsError(pwd->Set(data, length)) ||
		OpStatus::IsError(preleminary_dataitems.Add(pwd)))
	{
		OP_DELETE(pwd);
		return FALSE;
	}

	return TRUE;
}
//#endif // WAND_FUNCTIONS_NOT_MADE_ASYNC_YET

void WandManager::CommitPreliminaryDataItems()
{
	if (preleminary_dataitems.GetCount() == 0)
		return;

	UINT32 i, counter = 0;
	for(i = 0; i < log_profile.pages.GetCount(); i++)
	{
		WandPage* page = log_profile.pages.Get(i);
		for(unsigned int j = 0; j < page->objects.GetCount(); j++)
		{
			WandObjectInfo* objinfo = page->objects.Get(j);
			if (objinfo->is_password)
			{
				objinfo->password.Switch(preleminary_dataitems.Get(counter));
				counter++;
			}
		}
	}
	// Logins
	for(i = 0; i < logins.GetCount(); i++, counter++)
	{
		logins.Get(i)->password.Switch(preleminary_dataitems.Get(counter));
	}

	DestroyPreliminaryDataItems();

	StoreWandInfo();
}

void WandManager::DestroyPreliminaryDataItems()
{
	for(UINT32 i = 0; i < preleminary_dataitems.GetCount(); i++)
	{
		WandPassword* removed_dataitem = preleminary_dataitems.Remove(0);
		OP_DELETE(removed_dataitem);
	}
}

UINT32 WandManager::GetWandPageCount()
{
	return log_profile.pages.GetCount();
}

// Really IDCANCEL but I don't want this code to depend on a Windows define
#define MASTER_PASSWORD_WAS_CANCELED 2

/*virtual*/ void WandManager::OnMasterPasswordRetrieved(CryptoMasterPasswordHandler::PasswordState state)
{
	switch (state) {
		case CryptoMasterPasswordHandler::PASSWORD_RETRIVED_CANCEL:
		{
			m_suspended_operations.DeleteAll();
			OP_STATUS err;
			if (GetIsSecurityEncrypted() != g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseParanoidMailpassword))
				TRAP(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::UseParanoidMailpassword,GetIsSecurityEncrypted()));
			break;
		}
		case CryptoMasterPasswordHandler::PASSWORD_RETRIVED_SUCCESS:
		{
			OP_DELETE(g_wand_encryption_password);
			g_wand_encryption_password = ObfuscatedPassword::CreateCopy(g_libcrypto_master_password_handler->GetRetrievedMasterPassword());

			RunSuspendedOperation();
			break;
		}

		case CryptoMasterPasswordHandler::PASSWORD_RETRIVED_WRONG_PASSWORD:
		{
			RunSuspendedOperation();
			break;
		}
		default:
			OP_ASSERT(!"Unknown message");
			break;
	}
}

void WandManager::RunSuspendedOperation()
{
	unsigned number_of_resumable_ops = m_suspended_operations.GetCount();
	// Operations can be added so we have to stop after the operations
	// that we have now or we will be stuck in an infinite loop
	while (number_of_resumable_ops > 0)
	{
		OpAutoPtr<SuspendedWandOperation> suspended_operation(m_suspended_operations.Remove(0));
		number_of_resumable_ops--;
		switch(suspended_operation->GetType())
		{
		case SuspendedWandOperation::WAND_OPERATION_USE:
			{
				FramesDocument* doc = suspended_operation->GetDocument();
				BOOL3 submit = suspended_operation->GetBool3();
				Use(doc, submit);
			}
			break;
		case SuspendedWandOperation::WAND_OPERATION_REPORT_ACTION:
			{
				SuspendedWandOperationWithInfo* susp_op;
				susp_op = static_cast<SuspendedWandOperationWithInfo*>(suspended_operation.get());
				WandInfo* info = susp_op->TakeWandInfo();
				WAND_ACTION generic_action = susp_op->GetGenericAction();
#ifdef WAND_ECOMMERCE_SUPPORT
				WAND_ECOMMERCE_ACTION eCommerce_action = susp_op->GetECommerceAction();
#endif // WAND_ECOMMERCE_SUPPORT

#ifdef WAND_ECOMMERCE_SUPPORT
				info->ReportAction(generic_action, eCommerce_action);
#else
				info->ReportAction(generic_action);
#endif // WAND_ECOMMERCE_SUPPORT
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN:
			{
				Window* window = suspended_operation->GetWindow();
				WandLogin* wand_login = static_cast<SuspendedWandOperationWithLogin*>(suspended_operation.get())->TakeWandLogin();
				OpStatus::Ignore(StoreLoginInternal(window, wand_login)); // Nobody to tell about a failure
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN_NO_WINDOW:
			{
				WandLogin* wand_login = static_cast<SuspendedWandOperationWithLogin*>(suspended_operation.get())->TakeWandLogin();
				OpStatus::Ignore(StoreLoginInternal(NULL, wand_login)); // Nobody to tell about a failure
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_CHANGE_DB_ENCRYPTION:
			{
				Window* window = suspended_operation->GetWindow();
				UpdateSecurityState(window);
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_CHANGE_DB_ENCRYPTION_NO_WINDOW:
			{
				UpdateSecurityStateWithoutWindow();
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD:
			{
				Window* window = suspended_operation->GetWindow();
				WandLoginCallback* callback = static_cast<SuspendedWandOperationWithLogin*>(suspended_operation.get())->TakeCallback();
				WandLogin* login = static_cast<SuspendedWandOperationWithLogin*>(suspended_operation.get())->TakeWandLogin();
				GetLoginPassword(window, login, callback);
				OP_DELETE(login);
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD_NO_WINDOW:
			{
				WandLoginCallback* callback = static_cast<SuspendedWandOperationWithLogin*>(suspended_operation.get())->TakeCallback();
				WandLogin* login = static_cast<SuspendedWandOperationWithLogin*>(suspended_operation.get())->TakeWandLogin();
				GetLoginPasswordWithoutWindow(login, callback);
				OP_DELETE(login);
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_STORE_PAGE:
			{
				Window* window = suspended_operation->GetWindow();
				WandPage* plain_text_page = static_cast<SuspendedWandOperationWithPage*>(suspended_operation.get())->TakeWandPage();
				Store(window, plain_text_page);
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_FETCH_PAGE:
			{
				//Window* window = suspended_operation->GetWindow();
				FramesDocument* doc = suspended_operation->GetDocument();
				WandPage* page = static_cast<SuspendedWandOperationWithPage*>(suspended_operation.get())->TakeWandPage();
				BOOL3 submit = suspended_operation->GetBool3();
				Fetch(doc, NULL, page, submit, FALSE);
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_OPEN_DB:
			{
				Open(m_source_database_file, FALSE);
			}
			break;
#ifdef SYNC_HAVE_PASSWORD_MANAGER
		case SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_AVAILABLE_NO_WINDOW:
			{
				g_sync_coordinator->ContinueSyncData(SYNC_SUPPORTS_PASSWORD_MANAGER);
				SyncPasswordItemSyncStatuses();
			}
			break;

		case SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_FLUSH_HTTP_NO_WINDOW:
			{
				SuspendedWandOperationWithSync *suspend_sync = static_cast<SuspendedWandOperationWithSync*>(suspended_operation.get());
				OpStatus::Ignore(SyncDataFlush(OpSyncDataItem::DATAITEM_PM_HTTP_AUTH, suspend_sync->GetFirstSync(), suspend_sync->GetDirtySync()));
				break;
			}

		case SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_FLUSH_FORM_NO_WINDOW:
			{
				SuspendedWandOperationWithSync *suspend_sync = static_cast<SuspendedWandOperationWithSync*>(suspended_operation.get());
				OpStatus::Ignore(log_profile.SyncDataFlush(OpSyncDataItem::DATAITEM_PM_FORM_AUTH, suspend_sync->GetFirstSync(), suspend_sync->GetDirtySync()));
			}
			break;
#endif // SYNC_HAVE_PASSWORD_MANAGER
		default:
			OP_ASSERT(!"Not implemented");
		case SuspendedWandOperation::NO_WAND_OPERATION:
			break;
		}
	}
}

/* virtual */void WandManager::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_WAND_DELAYED_FILE_OPEN:
		{
			g_main_message_handler->UnsetCallBack(this, MSG_WAND_DELAYED_FILE_OPEN, 0);
			Open(m_source_database_file, FALSE, TRUE);
		}
		break;
	default:
			OP_ASSERT(!"Unexpected message");
	}
}

OP_STATUS WandManager::SetSuspendedPageOperation(SuspendedWandOperation::WandOperation type,
			Window* window,
			FramesDocument* doc,
			BOOL3 bool3,
			WandPage* wand_page,
			BOOL owns_wand_page)
{
	OP_ASSERT(type == SuspendedWandOperation::WAND_OPERATION_FETCH_PAGE ||
		type == SuspendedWandOperation::WAND_OPERATION_STORE_PAGE);
	SuspendedWandOperationWithPage* page_suspended_operation = OP_NEW(SuspendedWandOperationWithPage, ());
	if (!page_suspended_operation)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	page_suspended_operation->Set(type, window, doc, bool3, wand_page, owns_wand_page);

	if (OpStatus::IsError(m_suspended_operations.Add(page_suspended_operation)))
	{
		OP_DELETE(page_suspended_operation);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS WandManager::SetSuspendedLoginOperation(SuspendedWandOperation::WandOperation type,
			Window* window,
			FramesDocument* doc,
			BOOL3 bool3,
			WandLogin* wand_login,
			WandLoginCallback* callback)
{
	OP_ASSERT(type == SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN ||
		type == SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN_NO_WINDOW ||
		type == SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD ||
		type == SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD_NO_WINDOW);

	OP_ASSERT(window ||
		type == SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD_NO_WINDOW ||
		type == SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN_NO_WINDOW);

	SuspendedWandOperationWithLogin* login_suspended_operation = OP_NEW(SuspendedWandOperationWithLogin, ());
	if (!login_suspended_operation)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	login_suspended_operation->Set(type, window, doc, bool3, wand_login, callback);

	if (OpStatus::IsError(m_suspended_operations.Add(login_suspended_operation)))
	{
		OP_DELETE(login_suspended_operation);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}
OP_STATUS WandManager::SetSuspendedInfoOperation(SuspendedWandOperation::WandOperation type,
											 Window* window,
			 FramesDocument* doc,
			BOOL3 bool3,
			WandInfo* wand_info,
			WAND_ACTION generic_action, int int2)
{
	OP_ASSERT(type == SuspendedWandOperation::WAND_OPERATION_REPORT_ACTION);
	SuspendedWandOperationWithInfo* info_suspended_operation = OP_NEW(SuspendedWandOperationWithInfo, ());
	if (!info_suspended_operation)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	info_suspended_operation->Set(type, window, doc, bool3, wand_info, generic_action, int2);
	if (OpStatus::IsError(m_suspended_operations.Add(info_suspended_operation)))
	{
		OP_DELETE(info_suspended_operation);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

OP_STATUS WandManager::SetSuspendedOperation(SuspendedWandOperation::WandOperation type,
											 Window* window, FramesDocument* doc,
											 BOOL3 bool3)
{
	OP_ASSERT(type != SuspendedWandOperation::NO_WAND_OPERATION);
	OP_ASSERT(type != SuspendedWandOperation::WAND_OPERATION_FETCH_PAGE &&
		type != SuspendedWandOperation::WAND_OPERATION_STORE_PAGE); // SetSuspendedPageOperation
	OP_ASSERT(type != SuspendedWandOperation::WAND_OPERATION_STORE_LOGIN &&
		type != SuspendedWandOperation::WAND_OPERATION_RETRIEVE_PASSWORD); // SetSuspendedLoginOperation
	OP_ASSERT(type != SuspendedWandOperation::WAND_OPERATION_REPORT_ACTION); // SetSuspendedInfoOperation

	OP_ASSERT(window ||
	          	  	  (type == SuspendedWandOperation::WAND_OPERATION_CHANGE_DB_ENCRYPTION_NO_WINDOW ||
	          	  	   type == SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_AVAILABLE_NO_WINDOW)
		 	 );


	SuspendedWandOperation* suspended_operation = OP_NEW(SuspendedWandOperation, ());
	if (!suspended_operation)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	suspended_operation->Set(type, window, doc, bool3);

	if (OpStatus::IsError(m_suspended_operations.Add(suspended_operation)))
	{
		OP_DELETE(suspended_operation);
		return OpStatus::ERR_NO_MEMORY;
	}

	return OpStatus::OK;
}

#ifdef SYNC_HAVE_PASSWORD_MANAGER
OP_STATUS WandManager::SetSuspendedSyncOperation(SuspendedWandOperation::WandOperation type, BOOL first_sync, BOOL dirty_sync)
{
	OP_ASSERT(
		type == SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_FLUSH_HTTP_NO_WINDOW  ||
		type == SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_AVAILABLE_NO_WINDOW ||
		type == SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_FLUSH_FORM_NO_WINDOW
	);
	OpAutoPtr<SuspendedWandOperationWithSync> suspended_operation(OP_NEW(SuspendedWandOperationWithSync, ()));
	if (!suspended_operation.get())
		return OpStatus::ERR_NO_MEMORY;

	suspended_operation->Set(type, first_sync, dirty_sync);
	RETURN_IF_ERROR(m_suspended_operations.Add(suspended_operation.get()));
	suspended_operation.release();
	return OpStatus::OK;
}

#endif // SYNC_HAVE_PASSWORD_MANAGER


/* virtual */
void WandManager::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id == OpPrefsCollection::Core)
	{
		switch (pref)
		{
		case PrefsCollectionCore::EnableWand:
			SetActive(!!newvalue);
			break;
		}
	}
	else if (id == OpPrefsCollection::Network)
	{
		switch (pref)
		{
		case PrefsCollectionNetwork::UseParanoidMailpassword:

			// Remove the cached password given by user, to trigger new dialogue
			OP_DELETE(g_wand_encryption_password);
			g_wand_encryption_password = NULL;
			g_libcrypto_master_password_handler->ForgetRetrievedMasterPassword();


			UpdateSecurityStateWithoutWindow();
			break;
		}

	}
}


#ifdef SYNC_HAVE_PASSWORD_MANAGER
OP_STATUS WandManager::InitSync()
{
	RETURN_IF_ERROR(g_sync_encryption_key_manager->AddEncryptionKeyListener(this));
	RETURN_IF_ERROR(g_sync_coordinator->SetSyncDataClient(&log_profile, OpSyncDataItem::DATAITEM_PM_FORM_AUTH));
	return g_sync_coordinator->SetSyncDataClient(this, OpSyncDataItem::DATAITEM_PM_HTTP_AUTH);
}

/* virtual */ OP_STATUS WandManager::SyncDataFlush(OpSyncDataItem::DataItemType item_type, BOOL first_sync, BOOL is_dirty)
{
	if (item_type != OpSyncDataItem::DATAITEM_PM_HTTP_AUTH)
	{
		OP_ASSERT(!"Wrong sync data item type");
		return OpStatus::OK;
	}

	WandSecurityWrapper security;
	OP_STATUS security_status = security.EnableWithoutWindow();
	RETURN_IF_MEMORY_ERROR(security_status);
	if (security_status == OpStatus::ERR_YIELD)
		return SetSuspendedSyncOperation(SuspendedWandOperation::WAND_OPERATION_SYNC_DATA_FLUSH_HTTP_NO_WINDOW, first_sync, is_dirty);

	UINT32 count = logins.GetCount();
	unsigned index;

	for (index = 0; index < count; index++)
	{
		OpStatus::Ignore(logins.Get(index)->SyncItem(TRUE, is_dirty));
	}

	return OpStatus::OK;
}

WandSyncItem* WandManager::FindWandSyncItem(const uni_char* sync_id, int &index)
{
	index = -1;
	for(UINT32 i = 0; i < logins.GetCount(); i++)
	{
		if (uni_strcmp(logins.Get(i)->GetSyncId(), sync_id) == 0)
		{
			index = i;
			return logins.Get(i);
		}
	}
	return NULL;
}

WandSyncItem* WandManager::FindWandSyncItemWithSameUser(WandSyncItem *item, int &index)
{
	OP_ASSERT(item && item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_HTTP_AUTH);

	if (item && item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_HTTP_AUTH)
	{
		WandLogin *login = static_cast<WandLogin*>(item);
		WandLogin *found_similar_login = FindLogin(login->id, login->username);
		if (found_similar_login)
			index = logins.Find(found_similar_login);
		else
			index = -1;
		return found_similar_login;
	}
	return NULL;
}


void WandManager::DeleteWandSyncItem(int index)
{
	DeleteLogin(index);
}


OP_STATUS WandManager::StoreWandSyncItem(WandSyncItem *item)
{
	OP_ASSERT(item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_HTTP_AUTH);
	if (item->GetAuthType() == OpSyncDataItem::DATAITEM_PM_HTTP_AUTH)
	{
		return StoreLoginInternal(NULL, static_cast<WandLogin*>(item));
	}
	return OpStatus::ERR;
}

void WandManager::SyncWandItemsSyncStatuses()
{
	// Sync http logins
	for (unsigned index = 0; index < logins.GetCount(); index++)
	{
		OpStatus::Ignore(logins.Get(index)->SyncItem());
	}
}

OP_STATUS WandManager::SetSyncPasswordEncryptionKey(const UINT8 *key)
{
	if (m_key) op_memset(m_key, 0, 16);
	OP_DELETEA(m_key);
	m_key = 0;
	if (key)
	{
		RETURN_OOM_IF_NULL(m_key = OP_NEWA(UINT8, 16));
		op_memcpy(m_key, key, 16);
	}
	return OpStatus::OK;
}

/* virtual */
void WandManager::OnEncryptionKey(const SyncEncryptionKey& key)
{
	UINT8 encryption_key[16]; /* ARRAY OK 2011-02-08 haavardm */
	OpString username;
	OpString password;
	if (OpStatus::IsSuccess(g_sync_coordinator->GetLoginInformation(username, password)) &&
		OpStatus::IsSuccess(key.Decrypt(encryption_key, username, password)))
		SetSyncPasswordEncryptionKey(encryption_key);
	op_memset(encryption_key, 0, 16);
	username.Wipe();
	password.Wipe();


	SyncPasswordItemSyncStatuses();
	StoreWandInfo();
}

/* virtual */
void WandManager::OnEncryptionKeyDeleted()
{
	SetSyncPasswordEncryptionKey(0);
}

void WandManager::SyncPasswordItemSyncStatuses()
{
	WandSecurityWrapper security;
	OP_STATUS status = security.EnableWithoutWindow();
	if (status == OpStatus::ERR_YIELD)
		return;

	// Sync http passwords
	SyncWandItemsSyncStatuses();
	// Sync form logins
	log_profile.SyncWandItemsSyncStatuses();
}
#endif // SYNC_HAVE_PASSWORD_MANAGER

#endif // WAND_SUPPORT
