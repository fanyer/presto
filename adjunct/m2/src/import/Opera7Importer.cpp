/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 * 
 * George Refseth, rfz@opera.com
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

# include "adjunct/desktop_util/opfile/desktop_opfile.h"
# include "adjunct/desktop_util/prefs/PrefsUtils.h"
# include "adjunct/m2/src/engine/account.h"
# include "adjunct/m2/src/engine/engine.h"
# include "adjunct/m2/src/engine/message.h"
# include "adjunct/m2/src/engine/store/mboxstore.h"
# include "adjunct/m2/src/engine/store/storereaders.h"
# include "adjunct/m2/src/import/ImporterModel.h"
# include "adjunct/m2/src/import/Opera7Importer.h"
# include "modules/locale/oplanguagemanager.h"
# include "modules/prefsfile/prefsfile.h"
# include "modules/util/filefun.h"
# include "modules/util/path.h"

Opera7Importer::Opera7Importer()
:	m_accounts_ini(NULL),
	m_treePos(-1),
	m_inProgress(FALSE),
	m_account(NULL),
	m_store_reader(NULL),
	m_should_import_messages(FALSE)
{
}

Opera7Importer::~Opera7Importer()
{
	OP_DELETE(m_accounts_ini);
	OP_DELETE(m_store_reader);
}

BOOL Opera7Importer::OnContinueImport()
{
	OP_NEW_DBG("Opera7Importer::OnContinueImport", "m2");

	// Opera 7/8/9 import is based on importing
	// - one account at a time, first the settings, then
	// - all mail associated with the account

	if (m_stopImport)
	{
		m_account = NULL;
		return FALSE;
	}

	if (!m_inProgress)
	{
		if (GetModel()->SequenceIsEmpty(m_sequence))
			return FALSE;

		ImporterModelItem* item = GetModel()->PullItem(m_sequence);
		if (item)
		{
			// We only handle import of whole accounts
			if (item->GetType() == OpTypedObject::IMPORT_ACCOUNT_TYPE)
			{
				ImportAccount(item);
			}
		}
	}
	else
	{
		OP_DBG(("Opera7Importer::OnContinueImport() msg# %u of %u", m_count, m_totalCount));
		OpStatus::Ignore(ImportSingle());
	}
	return TRUE;
}

void Opera7Importer::OnCancelImport()
{
	OP_NEW_DBG("Opera7Importer::OnCancelImport", "m2");
	OP_DBG(("Opera7Importer::OnCancelImport()"));

	m_inProgress = FALSE;
}

OP_STATUS Opera7Importer::ImportAccount(const ImporterModelItem* item)
{
	OpString8 accountId;
	OpString8 string_value8;
	OP_STATUS err;

	// Read the settings of the account from accounts.ini, section is Account<item->GetAccountNumber()>
	// and set the target account (the account imported into)
	RETURN_IF_ERROR(accountId.Set("Account"));
	RETURN_IF_ERROR(accountId.Append(item->GetAccountNumber()));

	// Establish an account to import to
	Account * account = MessageEngine::GetInstance()->CreateAccount();
	if (!account) return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsSuccess(err = account->ReadAccountSettings(m_accounts_ini, accountId.CStr(), TRUE)) &&
		OpStatus::IsSuccess(err = account->SetAccountName(item->GetName())) &&
		OpStatus::IsSuccess(err = account->Init()))
	{
		// If we are importing a Merlin account, the following call could pick up the passwords
		OpStatus::Ignore(account->ReadPasswordsFromAccounts(m_accounts_ini, accountId.CStr(), TRUE));
		// if we shouldn't import messages, let's move on to the next account
		if (!m_should_import_messages)
		{
			return ContinueImport();
		}
		// let's import all the message for that account
		return ImportAccountMessages(item, account);
	}
	else
	{
		OP_DELETE(account);
		return err;
	}
}

OP_STATUS Opera7Importer::ImportAccountMessages(const ImporterModelItem* item, Account * account)
{
	m_account = account;
	m_import_accountID = op_atoi(item->GetAccountNumber().CStr());
	if (m_store_reader || ((m_store_reader = StoreReader::Create(m_error_message, m_folder_path)) != NULL))
	{
		m_count = 0;
		m_store_reader->InitializeAccount(m_import_accountID, account, &m_totalCount);
		m_inProgress = TRUE;
		return OpStatus::OK;
	}
	else
	{
		return OpStatus::ERR_NOT_SUPPORTED;
	}
}


OP_STATUS Opera7Importer::ImportSingle()
{
	if (m_store_reader)
	{
		if (m_store_reader->MoreMessagesLeft())
		{
			Message message;
			OP_STATUS err;
			RETURN_IF_ERROR(message.Init(m_account->GetAccountId()));
			if (OpStatus::IsError(err = m_store_reader->GetNextAccountMessage(message, m_import_accountID, TRUE, TRUE)))
			{
				if (err == OpStatus::ERR_YIELD)
					return OpStatus::OK;
				else
					return err;
			}
			if (m_account->HasExternalMessage(message))
				m_duplicates++;
			else
			{
				if (!message.GetRawBody())
					message.SetFlag(Message::PARTIALLY_FETCHED, TRUE);
				RETURN_IF_ERROR(m_account->Fetched(message, !message.GetRawBody()));
				if (OpStatus::ERR_NO_MEMORY == m_account->InsertExternalMessage(message))
					return OpStatus::ERR_NO_MEMORY;
			}
			m_count++;
			m_grandTotal++;
			MessageEngine::GetInstance()->OnImporterProgressChanged(this, m_currInfo, m_count, m_totalCount);
		}
		else
		{
			OP_DELETE(m_store_reader);
			m_store_reader = NULL;
			m_inProgress = FALSE;
		}
	}
	else
	{
		m_inProgress = FALSE;
	}

	return OpStatus::OK;
}

OP_STATUS Opera7Importer::ImportAccounts()
{
	OpenPrefsFile();

	m_inProgress = FALSE;

	return StartImport();
}

OP_STATUS Opera7Importer::Init()
{
	OP_STATUS ret = OpStatus::ERR;

	GetModel()->DeleteAll();

	const char *accounts_str = "Accounts";
	int account_count, accounts_version;

	if (m_accounts_ini)
	{
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_accounts_ini, accounts_str, "Count", 0, account_count));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_accounts_ini, accounts_str, "Version", 1, accounts_version));

		for (int i = 1; i <= account_count; i++)
		{
			OpString8	specific_account;
			OpString8	specific_account_number;
			OpString8	specific_account_name;
			OpString8	specific_account_enum;
			RETURN_IF_ERROR(specific_account_enum.Set("Account"));
			RETURN_IF_ERROR(specific_account_enum.AppendFormat("%d", i));
			RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_accounts_ini, accounts_str, specific_account_enum, specific_account_number));
			RETURN_IF_ERROR(specific_account.Set("Account"));
			RETURN_IF_ERROR(specific_account.Append(specific_account_number));
			RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_accounts_ini, specific_account, "Account Name", specific_account_name));

			OpString m2FolderPath, imported;
			m2FolderPath.Set(specific_account_name);
			OpStatus::Ignore(g_languageManager->GetString(Str::S_IMPORTED, imported));
			OpStatus::Ignore(imported.Insert(0, UNI_L(" (")));
			OpStatus::Ignore(imported.Append(UNI_L(")")));
			OpStatus::Ignore(m2FolderPath.Append(imported));

			OpString account_name, account_numeral;
			if (OpStatus::IsSuccess(account_name.SetFromUTF8(specific_account_name.CStr())))
			{
				ImporterModelItem* item = OP_NEW(ImporterModelItem, (OpTypedObject::IMPORT_ACCOUNT_TYPE, account_name, NULL, m2FolderPath, UNI_L("")));
				if (item)
				{
					item->SetAccountNumber(specific_account_number);
					GetModel()->AddLast(item);
				}
			}
		}
		return OpStatus::OK;
	}
	return ret;
}


OP_STATUS Opera7Importer::AddImportFile(const OpStringC& path)
{
	RETURN_IF_ERROR(Importer::AddImportFile(path));

    OP_DELETE(m_accounts_ini);
    m_accounts_ini = NULL;

	OpFile file;
	RETURN_IF_ERROR(file.Construct(path.CStr()));

	PrefsFile* OP_MEMORY_VAR prefs = OP_NEW(PrefsFile, (PREFS_STD));

    if (!prefs)
        return OpStatus::ERR_NO_MEMORY;

	TRAPD(err,	prefs->ConstructL(); 
				prefs->SetFileL(&file));

	if (OpStatus::IsError(err))
    {
        OP_DELETE(prefs);
        return err;
    }

	prefs->SetReadOnly();

	m_accounts_ini = prefs;

	// Define folder path since it is used abundantly
	OpString dir;
	TRAP(err, SplitFilenameIntoComponentsL(path, &dir, NULL, NULL));
	RETURN_IF_ERROR(err);

	return SetImportFolderPath(dir);
}

void Opera7Importer::SetImportItems(const OpVector<ImporterModelItem>& items)
{
	GetModel()->EmptySequence(m_sequence);

	UINT32 count = items.GetCount();

	for (UINT32 i = 0; i < count; i++)
	{
		ImporterModelItem* item = items.Get(i);

		GetModel()->PushItem(m_sequence, item);
	}
}


void Opera7Importer::GetDefaultImportFolder(OpString& path)
{
	OpString iniPath;
	if (GetOperaIni(iniPath))
	{
/*		TODO:!!!!!!!
 * 		PrefsFile opera_ini;
		opera_ini.SetFile(iniPath);
		OpString8 mailRoot8;
		if (opera_ini.GetStringValue("Mail", "Mail Root Directory", mailRoot8) && !mailRoot8.IsEmpty())
		{
			OpString mailRoot;
			mailRoot.Set(mailRoot8);
			OpStringC iniName(UNI_L("accounts.ini"));
			OpPathDirFileCombine(path, mailRoot, iniName);
		}
			*/
	}
}


BOOL Opera7Importer::GetOperaIni(OpString& fileName)
{
	BOOL exists = FALSE;
#ifdef MSWIN
	unsigned long size = 1024;
	OpString path;
	path.Reserve(size);

	BrowserUtils* bu = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();
	if (bu->OpRegReadStrValue(HKEY_CURRENT_USER, UNI_L("Software\\Opera Software"),
		UNI_L("Last CommandLine"), path.CStr(), &size) != OpStatus::OK)
	{
		return FALSE;
	}

	OP_STATUS ret = OpStatus::OK;
	// check if we have a custom commandline with .ini file as parameter
	int first = path.FindFirstOf('"');
	if (KNotFound != first)
	{
		int last = path.FindLastOf('"');
		OpString iniPath;
		path.Set(path.CStr() + first + 1, last - (first + 1));

		ret = DesktopOpFileUtils::Exists(path, exists);
	}
	else
	{
		first = path.FindI(UNI_L("\\opera.exe"));
		if (KNotFound != first)
		{
			path.CStr()[first] = 0;	// get rid of exe-filename

			OpString keep;
			keep.Set(path);

			path.Append(UNI_L("\\opera6.ini"));

			ret = DesktopOpFileUtils::Exists(path, exists);

			if (ret==OpStatus::OK && !exists)	// try older version
			{
				path.Set(keep);
				path.Append(UNI_L("\\opera5.ini"));

				ret = DesktopOpFileUtils::Exists(path, exists);
			}
		}
	}
	fileName.Set(path);
#endif // MSWIN, TODO: find ini file for other platforms
	return exists;
}


BOOL Opera7Importer::GetContactsFileName(OpString& fileName)
{
	if (m_folder_path.IsEmpty())
		return FALSE;

	fileName.Set(m_folder_path);

	int sep = fileName.FindLastOf(PATHSEPCHAR);
	if (sep != KNotFound)
	{
		fileName.CStr()[sep + 1] = 0;
		fileName.Append("contacts.adr");

		BOOL exists;
		if (DesktopOpFileUtils::Exists(fileName, exists) == OpStatus::OK &&
			exists)
		{
			return TRUE;
		}
	}

	return FALSE;
}

#endif //M2_SUPPORT
