/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#ifdef _DEBUG
#include <assert.h>
#endif

#include "modules/prefs/prefsmanager/collections/pc_m2.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"

#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/util/accountcreator.h"
#include "adjunct/m2_ui/models/chatroomsmodel.h"
#include "adjunct/m2_ui/models/accountsmodel.h"
#include "adjunct/quick/Application.h"
#include "adjunct/quick/models/DesktopWindowCollection.h"
#include "adjunct/quick_toolkit/windows/DesktopWindow.h"

AccountManager::AccountManager(MessageDatabase& database)
: m_prefs_file(NULL),
  m_m2_init_failed(FALSE),
  m_next_available_account_id(1),
  m_default_news_account(0),
  m_default_chat_account(0),
  m_last_used_mail_account(0),
  m_active_account(0),
  m_headerdisplay(NULL),
  m_old_version(0),
  m_database(database)
{
}

AccountManager::~AccountManager()
{
	Account* account = (Account*)(m_accountlist.First());
	Account* next_account;
	while (account)
	{
		next_account = (Account*)(account->Suc());
		account->Out();
		if (!m_m2_init_failed) //Only store accounts.ini if we know we didn't fail reading it!
		{
			account->SaveSettings(FALSE); // don't commit, it will happen below
		}
		OP_DELETE(account);
		account = next_account;
	}

	if (!m_m2_init_failed && m_headerdisplay)
	{
		m_headerdisplay->SaveSettings();
	}
	OP_DELETE(m_headerdisplay);

	if (!m_m2_init_failed) //Only store accounts.ini if we know we didn't fail reading it!
	{
		// write prefs file
		OpStatus::Ignore(CommitPrefsFile(TRUE)); //We need to get this on disk (not much we can do if Commit fails)
	}

    if (m_prefs_file)
		MessageEngine::GetInstance()->GetGlueFactory()->DeletePrefsFile(m_prefs_file); //If m_m2_init_failed, this will assert, even though everything is ok
}

void AccountManager::M2InitFailed()
{
	m_m2_init_failed = TRUE;
}

OP_STATUS AccountManager::Init(OpString8& status)
{
    if (!m_prefs_file)
        return OpStatus::ERR;

	const char *accounts_str = "Accounts";
	int account_count, accounts_version;

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, "Count", 0, account_count));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, "Version", -1, accounts_version));

	// if we have no information, we simply must assume it's the current version
	if (accounts_version == -1)
	{
		m_old_version = CURRENT_M2_ACCOUNT_VERSION;
		accounts_version = CURRENT_M2_ACCOUNT_VERSION;
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, accounts_str, "Version", CURRENT_M2_ACCOUNT_VERSION));
	}
	if (accounts_version > CURRENT_M2_ACCOUNT_VERSION && account_count)
	{
		status.Append("Not possible to run old Opera version with new Opera mail files.\r\n");
		return OpStatus::ERR; // refuse to start
	}

	if (accounts_version < CURRENT_M2_ACCOUNT_VERSION)
	{
		m_old_version = accounts_version;
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, accounts_str, "Version", CURRENT_M2_ACCOUNT_VERSION));
	}
	
	INT32 default_mail_account;
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, "Next Available Id"			, 1, m_next_available_account_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, "Default Mail Account"			, 0, m_last_used_mail_account));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, "Default Outgoing Mail Account", 0, default_mail_account));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, "Default News Account"			, 0, m_default_news_account));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, "Default Chat Account"			, 0, m_default_chat_account));
	m_default_mail_account.Set(default_mail_account);

	m_default_mail_account.Subscribe(MAKE_DELEGATE(*this, &AccountManager::CommitDefaultMailAccount));

	if (m_active_account!=-1) //Category is only available for active account -1 ('custom category')
		m_active_category.Empty();

    UINT16 account_id = 0;
	OpINTSortedVector valid_account_ids;
	char account_key[13];

	OP_STATUS ret;
    for (UINT16 i=1; i<=account_count; i++)
    {
		op_sprintf(account_key, "Account%u", i);

		// first get the account id
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs_file, accounts_str, account_key, 0, account_id));

		OpString8 account_section;
		RETURN_IF_ERROR(account_section.AppendFormat("Account%d", account_id));; 
		OpString8 protocol;
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs_file, account_section, "Incoming Protocol", protocol));
		// check if it's a known type or an old unsupported type
		if (Account::GetProtocolFromName(protocol) == AccountTypes::UNDEFINED)
			continue;

        if (account_id != 0)
        {
            Account* account = OP_NEW(Account, (m_database));
            if (!account)
                return OpStatus::ERR_NO_MEMORY;

            Account* does_it_exist = NULL;
			if (OpStatus::IsError(ret=GetAccountById(account_id, does_it_exist)))
            {
                OP_DELETE(account);
                return ret;
            }
            if (does_it_exist)
            {
                OP_DELETE(account);
                return OpStatus::ERR; //Already exists in list!
            }

            account->Into(&m_accountlist);

            if (OpStatus::IsError(ret=account->Init(account_id, status)))
            {
                status.Append("Initializing ");
                status.Append(account_key);
                status.Append(" failed\n");
                return ret;
            }

            if (account->GetIsTemporary())
            {
				UINT16* temporary_id = OP_NEW(UINT16, ());
				if(!temporary_id)
					return OpStatus::ERR_NO_MEMORY;
				*temporary_id = account->GetAccountId();
				m_temporary_accounts.Add(temporary_id);
            }
			else
			{
				valid_account_ids.Insert(account_id);
			}
        }
        else //accounts.ini reports more accounts than present. Adjust account-count and continue as if nothing happened
		{
			account_count = i-1;
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, accounts_str, "Count", account_count));
            break;
		}
    }

	// clean up broken and old account.inis.. code can be removed when 7.5 is used by everyone

	for (account_id = 1; account_id < m_next_available_account_id; account_id++)
	{
		if (!valid_account_ids.Contains(account_id))
		{
			uni_char account_key_uni[13];
			uni_sprintf(account_key_uni, UNI_L("Account%u"), account_id);
		    TRAP(ret, m_prefs_file->DeleteSectionL(account_key_uni));
		}
	}

	valid_account_ids.Clear();

	//Initialize Header Display
	m_headerdisplay = OP_NEW(HeaderDisplay, ());
	if (!m_headerdisplay)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(ret=m_headerdisplay->Init(m_prefs_file)))
	{
		OP_DELETE(m_headerdisplay);
		return ret;
	}

	if(account_count > 0 && g_application)
	{
		g_application->SettingsChanged(SETTINGS_MENU_SETUP);
	}

    return OpStatus::OK;
}

OP_STATUS AccountManager::OnAllMessagesAvailable()
{
	Account* account = GetFirstAccount();
	while (account)
	{
#ifdef IRC_SUPPORT
		// IRC incoming backend already created at this point
		if (account->GetIncomingProtocol() != AccountTypes::IRC)
#endif //IRC_SUPPORT
			RETURN_IF_ERROR(account->InitBackends());

		account = static_cast<Account*>(account->Suc());
	}

	RETURN_IF_ERROR(UpdateAccountStores());

	BOOL added_1160_upgrade_mail;
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt<BOOL>(m_prefs_file, "Accounts", "Added 11.60 Upgrade Mail", 0, added_1160_upgrade_mail));

	if (!added_1160_upgrade_mail && (GetMailNewsAccountCount() > 0 || GetRSSAccount(FALSE)))
	{
		Account* account = GetFirstAccountWithCategory(AccountTypes::TYPE_CATEGORY_MAIL);
		
		if (!account)
			account = GetFirstAccountWithCategory(AccountTypes::TYPE_CATEGORY_NEWS);
		if (!account)
			account = GetRSSAccount(FALSE);

		OP_ASSERT(account);
		RETURN_IF_ERROR(account->AddOperaGeneratedMail(UPGRADE_1160_MESSAGE_FILENAME, Str::S_M2_UPGRADE_1160_SUBJECT));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Added 11.60 Upgrade Mail", TRUE));
		RETURN_IF_ERROR(CommitPrefsFile(FALSE));
	}

	return OpStatus::OK;
}

OP_STATUS AccountManager::SetPrefsFile(const OpStringC& filename)
{
    OP_ASSERT(m_prefs_file == NULL);

	m_prefs_file = MessageEngine::GetInstance()->GetGlueFactory()->CreatePrefsFile(filename);
    return m_prefs_file?OpStatus::OK:OpStatus::ERR_NO_MEMORY;
}

OP_STATUS AccountManager::CommitPrefsFile(BOOL force, BOOL flush)
{
	if (!m_m2_init_failed) //Only store accounts.ini if we know we didn't fail reading it!
	{
		TRAPD(ret, m_prefs_file->CommitL(force, flush));
		return ret;
	}
	return OpStatus::OK;
}

void AccountManager::CommitDefaultMailAccount(INT32) 
{ 
	RETURN_VOID_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Default Outgoing Mail Account", m_default_mail_account.Get()));
	OpStatus::Ignore(CommitPrefsFile(FALSE)); 
}

OP_STATUS AccountManager::SetLastUsedAccount(IN UINT16 account_id)
{
	Account* account;
	RETURN_IF_ERROR(GetAccountById(account_id, account));

	if (!account)
		return OpStatus::OK; //Account does not exist. Don't write to prefs

	switch(account->GetIncomingProtocol())
	{
		case AccountTypes::POP:
		case AccountTypes::IMAP:
			m_last_used_mail_account = account_id;
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Default Mail Account", m_last_used_mail_account));
			break;
		case AccountTypes::NEWS:
			m_default_news_account = account_id;
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Default News Account", m_default_news_account));
			break;
#ifdef IRC_SUPPORT
		case AccountTypes::IRC:
			m_default_chat_account = account_id;
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Default Chat Account", m_default_chat_account));
			break;
#endif // IRC_SUPPORT
		default:
			return OpStatus::ERR;
	}

	return CommitPrefsFile(FALSE); //No rush to get this on disk
}

UINT16 AccountManager::GetDefaultAccountId(AccountTypes::AccountTypeCategory type_category) const
{
	UINT16 account_id = 0;

	switch (type_category)
	{
	case AccountTypes::TYPE_CATEGORY_MAIL: 
	{
		account_id = m_default_mail_account.Get();

		if (account_id == 0)
		{
			account_id = m_last_used_mail_account;
		}
		break;
	}
	case AccountTypes::TYPE_CATEGORY_NEWS: account_id = m_default_news_account; break;
	case AccountTypes::TYPE_CATEGORY_CHAT: account_id = m_default_chat_account; break;
	default: break;
	}

	if (account_id != 0)
	{
		Account* dummy_account = NULL;
		if (OpStatus::IsError(GetAccountById(account_id, dummy_account)) ||
			dummy_account == NULL)
		{
			account_id = 0;
		}
	}

	if (account_id == 0)
	{
		Account* dummy_account = GetFirstAccountWithCategory(type_category);
		if (dummy_account)
			account_id = dummy_account->GetAccountId();
	}

	return account_id;
}

OP_STATUS AccountManager::SetActiveAccount(int account_id, const OpStringC& category) // account_id=-1: Use category instead
{
	if (m_active_account==account_id && !(account_id==-1 && m_active_category.CompareI(category)!=0))
		return OpStatus::OK; //Nothing to do

	m_active_account = account_id;

	OP_STATUS ret;
	if (account_id==-1)
	{
		ret = m_active_category.Set(category);
	}
	else
	{
		m_active_category.Empty();
		ret = OpStatus::OK;
	}

	if (account_id > 0)
	{
		SetLastUsedAccount(account_id);
	}
	else
	{
		ret = CommitPrefsFile(FALSE);
	}

	MessageEngine::GetInstance()->OnActiveAccountChanged();
	return ret;
}

OP_STATUS AccountManager::GetActiveAccount(int& account_id, OpString& category) const // account_id=-1: Use category instead
{
	account_id = m_active_account;
	return category.Set(m_active_category);
}

BOOL AccountManager::IsAccountActive(UINT16 account_id) const
{
    if (m_active_account==0) //All accounts active
        return TRUE;

    Account* account_ptr;
	if (OpStatus::IsError(GetAccountById(account_id, account_ptr)) || !account_ptr)
        return FALSE;

	if (m_active_account > 0)
	{
        return (m_active_account==account_id || account_ptr->GetIncomingProtocol() == AccountTypes::RSS );
	}
	else if (m_active_account==-3)
	{
		return account_ptr->GetIncomingProtocol() == AccountTypes::POP || account_ptr->GetIncomingProtocol() == AccountTypes::IMAP;
	}
	else if (m_active_account==-4)
	{
		return account_ptr->GetIncomingProtocol() == AccountTypes::NEWS;
	}
	else if (m_active_account==-5)
	{
		return account_ptr->GetIncomingProtocol() == AccountTypes::RSS;
	}

	OpString account_category;
	RETURN_VALUE_IF_ERROR(account_ptr->GetAccountCategory(account_category), FALSE);
	return account_category.CompareI(m_active_category) == 0;
}

OP_STATUS AccountManager::AddAccount(IN Account* account, BOOL reserve_new_id)
{
    if (!account || !m_prefs_file)
        return OpStatus::ERR_NULL_POINTER;

	if (account->GetAccountName().IsEmpty())
        return OpStatus::ERR;

	UINT16 does_it_exist_id = FindAccountId(account->GetAccountName());
    if (does_it_exist_id != 0)
        return OpStatus::ERR; //Already exists in list!

    if (reserve_new_id)
    {
        UINT16 new_account_id = 0;
        RETURN_IF_ERROR(GetAndIncrementNextAvailableAccountId(new_account_id));
        account->SetAccountId(new_account_id);
    }

    UINT16 account_id = account->GetAccountId();
    Account* does_it_exist = NULL;
    RETURN_IF_ERROR(GetAccountById(account_id, does_it_exist));

    if (does_it_exist)
        return OpStatus::ERR; //Already exists in list!

    account->Into(&m_accountlist);

    UINT16 account_count = GetAccountCount();
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Count", account_count));

	char account_key[13];
	op_sprintf(account_key, "Account%u", account_count);
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", account_key, account_id));

	// We only want this mail on upgrade
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Added 11.60 Upgrade Mail", TRUE));

    RETURN_IF_ERROR(account->SaveSettings());
    RETURN_IF_ERROR(CommitPrefsFile(TRUE)); //We need to get this on disk
    RETURN_IF_ERROR(account->SettingsChanged(FALSE));

    return OpStatus::OK;
}

OP_STATUS AccountManager::AddTemporaryAccount(Account*& m2_account, AccountTypes::AccountType protocol, const OpStringC16& servername,
                                              UINT16 port, BOOL secure, const OpStringC16& username, const OpStringC& password)
{
    m2_account = NULL;

	Account* account = OP_NEW(Account, (m_database));
    if (!account)
        return OpStatus::ERR_NO_MEMORY;

    account->SetIsTemporary(TRUE);

	RETURN_IF_ERROR(account->SetAccountName(servername));
	account->SetIncomingProtocol(protocol);
	RETURN_IF_ERROR(account->SetDefaults());
	RETURN_IF_ERROR(account->SetIncomingServername(servername));
	RETURN_IF_ERROR(account->SetIncomingUsername(username));
	RETURN_IF_ERROR(account->SetIncomingPassword(password));

	if (protocol == AccountTypes::NEWS || protocol == AccountTypes::POP)
		account->SetOutgoingProtocol(AccountTypes::SMTP);

    account->SetIncomingPort(port);
    account->SetUseSecureConnectionIn(secure);

	//Don't let the account try to fetch messages automatically
	account->SetInitialPollDelay(0);
	account->SetPollInterval(0);
	account->SetManualCheckEnabled(FALSE);

	if (username.IsEmpty() && password.IsEmpty())
		account->SetIncomingAuthenticationMethod(AccountTypes::NONE);

    RETURN_IF_ERROR(account->Init()); //Init will also Add the account

    m2_account = (Account*)account;
    return OpStatus::OK;
}

OP_STATUS AccountManager::RemoveAccount(IN UINT16 account_id, BOOL remove_account_files)
{
    Account* account = NULL;
	AccountTypes::AccountType account_type;

    RETURN_IF_ERROR(GetAccountById(account_id, account));

    if (!account)
        return OpStatus::ERR;

	account_type = account->GetIncomingProtocol();

	//Remove the account settings
    account->PrepareToDie();
    account->Out();

	uni_char account_key_uni[13];
	uni_sprintf(account_key_uni, UNI_L("Account%u"), account->GetAccountId());
	RETURN_IF_LEAVE(m_prefs_file->DeleteSectionL(account_key_uni));

    OP_DELETE(account); //Die, account, die!


	//Write new references to accounts in accounts.ini
	char account_key[13];
	UINT16 account_count = 0;
	account = GetFirstAccount();
	while (account)
	{
		account_count++;
		op_sprintf(account_key, "Account%u", account_count);
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", account_key, account->GetAccountId()));
		account = (Account*)account->Suc();
	}

	//Update account count
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Count", account_count));

	//Remove the last (now unused) item
	op_sprintf(account_key, "Account%u", account_count+1);
    RETURN_IF_LEAVE(m_prefs_file->DeleteKeyL("Accounts", account_key));

	//We need to get this on disk ASAP
    RETURN_IF_ERROR(CommitPrefsFile(TRUE));

	// Remove search_engine files (POP, IMAP specific only)
	if (account_type == AccountTypes::IMAP)
	{
			OpString filename;
			OpFile btree_file;
			RETURN_IF_ERROR(MailFiles::GetIMAPUIDFilename(account_id,filename));
			RETURN_IF_ERROR(btree_file.Construct(filename.CStr()));
			RETURN_IF_ERROR(btree_file.Delete());
	}
	else if (account_type == AccountTypes::POP)
	{
			OpString filename;
			OpFile btree_file;
			RETURN_IF_ERROR(MailFiles::GetPOPUIDLFilename(account_id,filename));
			RETURN_IF_ERROR(btree_file.Construct(filename.CStr()));
			RETURN_IF_ERROR(btree_file.Delete());
	}

	if (account_count == 0)
	{
		// close all mail windows and mail panels
		g_application->GetDesktopWindowCollection().CloseDesktopWindowsByType(OpTypedObject::WINDOW_TYPE_MAIL_VIEW);
		g_input_manager->InvokeAction(OpInputAction::ACTION_HIDE_PANEL, 0, UNI_L("Mail"), g_application->GetActiveDesktopWindow());
	}

	MessageEngine::GetInstance()->OnAccountRemoved(account_id, account_type);

    return OpStatus::OK;
}

OP_STATUS AccountManager::GetAccountById(IN UINT16 account_id, OUT Account*& m2_account) const
{
    if (account_id==0) //Legal account_id should be >= 1
    {
        m2_account = NULL;
        return OpStatus::ERR;
    }

    Account* account = (Account*)(m_accountlist.First());
    while (account && account->GetAccountId()!=account_id)
    {
        account = (Account*)(account->Suc());
    }
    m2_account = (Account*)account;
    return OpStatus::OK;
}

Account* AccountManager::GetAccountByIndex(UINT16 index) const
{
    Account* account = (Account*)(m_accountlist.First());
    while (account && index>0)
    {
        account = (Account*)(account->Suc());
        index--;
    }
    return (Account*)account;
}

Account* AccountManager::GetRSSAccount(BOOL create_if_needed, INT32* rss_parent_index_id)
{
#ifdef RSS_SUPPORT
	Account* account = NULL;
	for (int i = 0; i < GetAccountCount(); i++)
	{
		account = GetAccountByIndex(i);

		if (account && account->GetIncomingProtocol() == AccountTypes::RSS)
		{
			if (rss_parent_index_id)
				*rss_parent_index_id = account->GetAccountId() + IndexTypes::FIRST_ACCOUNT;
			return account;
		}
	}

	if (!create_if_needed)
		return NULL;

	account = OP_NEW(Account, (m_database));

	if (account)
	{
		account->SetIncomingProtocol(AccountTypes::RSS);
		account->SetOutgoingProtocol(AccountTypes::UNDEFINED);
		account->SetDefaults();

		OpString string;
		string.Set("RSS");
		account->SetAccountName(string);
		account->Init();

		if (rss_parent_index_id)
			*rss_parent_index_id = account->GetAccountId() + IndexTypes::FIRST_ACCOUNT;
		
		if (g_m2_engine->GetAccountManager()->GetMailNewsAccountCount() == 0)
		{
			g_m2_engine->GetIndexer()->ResetToDefaultIndexes();
			time_t now = g_timecache->CurrentTime();
			TRAPD(err, g_pcm2->WriteIntegerL(PrefsCollectionM2::LastDatabaseCheck, now));
		}

		return account;
	}
#endif // RSS_SUPPORT
	return NULL;
}

OP_STATUS AccountManager::GetAccountByProperties(Account*& m2_account, AccountTypes::AccountType protocol,
                                        const OpStringC16& server, UINT16 port, BOOL secure,
                                        const OpStringC16& username) const
{
    UINT16  account_port;

    Account* account = (Account*)(m_accountlist.First());
    while (account)
    {
		if (account->GetIncomingProtocol() == protocol)
		{
			account_port = account->GetIncomingPort();

			if ((server.IsEmpty() || account->GetIncomingServername().CompareI(server)==0) &&
				(port==0 || account_port==port) &&
				account->GetUseSecureConnectionIn()==secure &&
				(username.IsEmpty() || account->GetIncomingUsername().Compare(username)==0) )
			{
				break; //We have found a match
			}
		}

        account = (Account*)(account->Suc());
    }
    m2_account = (Account*)account;
    return OpStatus::OK;
}

OP_STATUS AccountManager::GetChatAccountByServer(OUT Account*& m2_account,
	const OpStringC16& server) const
{
	m2_account = 0;

	Account* account = (Account*)(m_accountlist.First());
    while (account != 0 && m2_account == 0)
    {
		if (account->GetIncomingProtocol() >= AccountTypes::_FIRST_CHAT &&
			account->GetIncomingProtocol() <= AccountTypes::_LAST_CHAT)
		{
			OpAutoVector<OpString> server_collection;
			OpStatus::Ignore(account->GetIncomingServernames(server_collection));

			for (UINT32 index = 0, count = server_collection.GetCount();
				index < count; ++index)
			{
				const OpStringC& account_server = *server_collection.Get(index);
				if (account_server.CompareI(server) == 0)
				{
					m2_account = account;
					break;
				}
			}
		}

        account = (Account*)(account->Suc());
    }

	return OpStatus::OK;
}

UINT16 AccountManager::FindAccountId(const OpStringC& account_name) const
{
    if (account_name.IsEmpty())
        return 0;

    Account* account = (Account*)(m_accountlist.First());
    while (account)
    {
        if (account->GetAccountName().Compare(account_name)==0)
            return account->GetAccountId();

        account = (Account*)(account->Suc());
    }
    return 0;
}

OP_STATUS AccountManager::UpdateAccountStores()
{
	// Update individual stores
	Account* account = static_cast<Account*>(m_accountlist.First());
	while (account)
	{
		RETURN_IF_ERROR(account->UpdateStore(GetOldVersion()));
		account = static_cast<Account*>(account->Suc());
	}

	return OpStatus::OK;
}

OP_STATUS AccountManager::RemoveTemporaryAccounts()
{
	UINT16* account_id;
	UINT32 i;

	for (i = 0; i < m_temporary_accounts.GetCount(); i++)
	{
		account_id = m_temporary_accounts.Get(i);
		RETURN_IF_ERROR(RemoveAccount(*account_id, TRUE));
	}

	m_temporary_accounts.DeleteAll();

	return OpStatus::OK;
}

UINT16 AccountManager::GetAccountCount() const
{
    UINT16 count = 0;
    Account* account = (Account*)(m_accountlist.First());
    while (account)
    {
        count++;
        account = (Account*)(account->Suc());
    }
    return count;
}

UINT16 AccountManager::GetFetchAccountCount() const
{
    UINT16 count = 0;
    Account* account = (Account*)(m_accountlist.First());
    while (account)
    {
		switch (account->GetIncomingProtocol())
		{
			case AccountTypes::POP:
			case AccountTypes::IMAP:
			case AccountTypes::NEWS:
			case AccountTypes::RSS:
				count++;
				break;
		}

        account = (Account*)(account->Suc());
    }
    return count;
}

UINT16 AccountManager::GetComposeAccountCount() const
{
	OpString mail;
    UINT16 count = 0;
    Account* account = (Account*)(m_accountlist.First());
    while (account)
    {
		account->GetEmail(mail);

		if (account->GetIncomingProtocol() == AccountTypes::NEWS ||
			(mail.HasContent() && account->GetOutgoingProtocol() == AccountTypes::SMTP))
		{
			count++;
		}
        account = (Account*)(account->Suc());
    }
    return count;
}


UINT16 AccountManager::GetMailNewsAccountCount() const
{
	UINT16 count = 0;

    Account* account = (Account*)(m_accountlist.First());
    while (account != 0)
    {
		const AccountTypes::AccountType account_type = account->GetIncomingProtocol();

		if ((account_type > AccountTypes::_FIRST_MAIL && account_type < AccountTypes::_LAST_MAIL) ||
			(account_type > AccountTypes::_FIRST_NEWS && account_type < AccountTypes::_LAST_NEWS))
		{
			++count;
		}

        account = (Account*)(account->Suc());
    }

    return count;
}

UINT16 AccountManager::GetSignatureCount(OpStringC& signature_file) const
{
	UINT16 count = 0;
	OpString other_signature_file;

	Account* account = (Account*)(m_accountlist.First());
	while (account != 0)
	{
		if(OpStatus::IsSuccess(account->GetSignatureFile(other_signature_file)) && signature_file == other_signature_file)
			count++;
		account = (Account*)(account->Suc());
	}

	return count;
}


OP_STATUS AccountManager::GetAndIncrementNextAvailableAccountId(UINT16& next_available_account_id)
{
    if (!m_prefs_file)
        return OpStatus::ERR_NULL_POINTER;

    next_available_account_id = m_next_available_account_id++;
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs_file, "Accounts", "Next Available Id", m_next_available_account_id));

    return CommitPrefsFile(TRUE); //We need to get this on disk
}

void AccountManager::ClearAccountPasswords()
{
    Account* account = (Account*)(m_accountlist.First());
    while (account)
    {
		account->ResetRetrievedPassword();
        account = (Account*)(account->Suc());
    }
}

OP_STATUS AccountManager::CloseAllConnections()
{
	// Close connections for all accounts
	Account* account = (Account*)(m_accountlist.First());

	while (account)
	{
		OpStatus::Ignore(account->CloseAllConnections());

		account = (Account*)(account->Suc());
	}

	return OpStatus::OK;
}

OP_STATUS AccountManager::ToOnlineMode()
{
	// Replay logs for all accounts
	Account* account = (Account*)(m_accountlist.First());

	while (account)
	{
		OpStatus::Ignore(account->ToOnlineMode());

		account = (Account*)(account->Suc());
	}

	return OpStatus::OK;
}

/*************************************************************************
**																		**
**   AccountManager::IsLowBandwidthModeEnabled							**
**																		**
*************************************************************************/
BOOL AccountManager::IsLowBandwidthModeEnabled()
{
	Account* account = (Account*)m_accountlist.First();
	while (account)
	{
		if (!account->GetLowBandwidthMode())
			return FALSE;
		account = (Account*)(account->Suc());
	}
	return TRUE;
}

/*************************************************************************
**																		**
**  AccountManager::SetLowBandwidthMode									**
**																		**
*************************************************************************/
void AccountManager::SetLowBandwidthMode( BOOL enabled )
{
	Account* account = (Account*)m_accountlist.First();

	while (account)
	{
		account->SetLowBandwidthMode(enabled);
		account = (Account*)(account->Suc());
	}
}



/***********************************************************************************
 ** Whether there is an account busy at this moment
 **
 ** AccountManager::IsBusy
 ** @param incoming Whether to check receiving (instead of sending)
 **
 ***********************************************************************************/
BOOL AccountManager::IsBusy(BOOL incoming)
{
	Account* account = (Account*)m_accountlist.First();

	while (account)
	{
		if (account->GetProgress(incoming).GetCurrentAction() != ProgressInfo::NONE)
			return TRUE;
		account = (Account*)(account->Suc());
	}

	return FALSE;
}

Account* AccountManager::GetFirstAccountWithType(AccountTypes::AccountType type)
{
	INT32 i;
	for (i = 0; i < GetAccountCount(); i++)
	{
		Account* account = GetAccountByIndex(i);

		if (account && account->GetIncomingProtocol() == type)
		{
			return account;
		}
	}

	if (type == AccountTypes::RSS)
		return GetRSSAccount(TRUE);

	return NULL;
}

Account* AccountManager::GetFirstAccountWithCategory(AccountTypes::AccountTypeCategory type_category) const
{
	INT32 i;
	for (i = 0; i < GetAccountCount(); i++)
	{
		Account* account = GetAccountByIndex(i);
		if (!account)
			break;

		AccountTypes::AccountType account_type = account->GetIncomingProtocol();
		if ((type_category==AccountTypes::TYPE_CATEGORY_MAIL && (account_type>=AccountTypes::_FIRST_MAIL && account_type<=AccountTypes::_LAST_MAIL)) ||
			(type_category==AccountTypes::TYPE_CATEGORY_NEWS && (account_type>=AccountTypes::_FIRST_NEWS && account_type<=AccountTypes::_LAST_NEWS)) ||
			(type_category==AccountTypes::TYPE_CATEGORY_CHAT && (account_type>=AccountTypes::_FIRST_CHAT && account_type<=AccountTypes::_LAST_CHAT)))
		{
			return account;
		}
	}

	return NULL;
}


INT16 AccountManager::GetNumberOfKnownAccountsProviders()
{
	AccountCreator account_creator;
	RETURN_VALUE_IF_ERROR(account_creator.Init(), 0);
	INT16 number_of_known_providers = 0;
	for(int i = 0; i < GetAccountCount(); i++)
	{
		Account* account = GetAccountByIndex(i);
		OP_ASSERT(account);

		if (account->GetIncomingProtocol() < AccountTypes::_FIRST_MAIL || account->GetIncomingProtocol() > AccountTypes::_LAST_MAIL)
			continue;

		OpString email;
		AccountCreator::Server server;
		if (OpStatus::IsSuccess(account->GetEmail(email)) && OpStatus::IsSuccess(account_creator.GetServer(email, account->GetType(), TRUE, server)))
			++number_of_known_providers;
	}
	return number_of_known_providers;
}
#endif //M2_SUPPORT
