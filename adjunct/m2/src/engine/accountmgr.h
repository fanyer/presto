// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef ACCOUNTMGR_H
#define ACCOUNTMGR_H

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/adt/opproperty.h"
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/headerdisplay.h"

class MessageDatabase;

class AccountManager
{
private:
    Head m_accountlist;

    PrefsFile*			m_prefs_file;
	UINT				m_m2_init_failed:1;
    UINT16				m_next_available_account_id;
	UINT16				m_default_news_account;
	UINT16				m_default_chat_account;
	UINT16				m_last_used_mail_account;
    int					m_active_account; // -4=news, -3=mail, (-2=occupied), -1=custom category, 0=all, >0=account_id of active account
	OpString			m_active_category;
	HeaderDisplay*		m_headerdisplay;

	OpAutoVector<UINT16> m_temporary_accounts;

	int m_old_version;

	MessageDatabase& m_database;

public:

	OpProperty<INT32>	m_default_mail_account; ///< Used in ComposeWindowOptionsController and saved to disk

    AccountManager(MessageDatabase& database);
    ~AccountManager();

	void M2InitFailed();
    OP_STATUS    Init(OpString8& status);
	OP_STATUS	 OnAllMessagesAvailable();

	OP_STATUS    SetPrefsFile(const OpStringC& filename);
    PrefsFile*   GetPrefsFile() const {return m_prefs_file;}
	OP_STATUS	 CommitPrefsFile(BOOL force = FALSE, BOOL flush = TRUE);
	void		 CommitDefaultMailAccount(INT32);

	HeaderDisplay* GetHeaderDisplay() const {return m_headerdisplay;}

	OP_STATUS	 SetLastUsedAccount(IN UINT16 account_id);
	UINT16		 GetDefaultAccountId(AccountTypes::AccountTypeCategory type_category) const;

	Account* GetFirstAccountWithType(AccountTypes::AccountType type);
	Account* GetFirstAccountWithCategory(AccountTypes::AccountTypeCategory type_category) const;

	OP_STATUS    SetActiveAccount(int account_id, const OpStringC& category); // account_id=-1: Use category instead
	int     	 GetActiveAccount() { return m_active_account; }
    OP_STATUS    GetActiveAccount(int& account_id, OpString& category) const; // account_id=-1: Use category instead
    BOOL         IsAccountActive(UINT16 account_id) const;

    OP_STATUS    AddAccount(IN Account* account, BOOL reserve_new_id=FALSE);
    OP_STATUS    AddTemporaryAccount(Account*& m2_account, AccountTypes::AccountType protocol, const OpStringC16& servername,
                                     UINT16 port, BOOL secure, const OpStringC16& username, const OpStringC& password);

    OP_STATUS    RemoveAccount(IN UINT16 account_id, BOOL remove_account_files=FALSE);
    OP_STATUS    GetAccountById(IN UINT16 account_id, OUT Account*& account) const;
    Account* GetAccountByIndex(UINT16 index) const;
	Account* GetRSSAccount(BOOL create_if_needed = TRUE, INT32* rss_parent_index_id = NULL);
    OP_STATUS    GetAccountByProperties(Account*& m2_account, AccountTypes::AccountType protocol,
                                        const OpStringC16& server, UINT16 port, BOOL secure,
                                        const OpStringC16& username) const;
	OP_STATUS	GetChatAccountByServer(OUT Account*& m2_account, const OpStringC16& server) const;
    UINT16		FindAccountId(const OpStringC& account_name) const;
	OP_STATUS	UpdateAccountStores();
	OP_STATUS	RemoveTemporaryAccounts();

    UINT16		GetAccountCount() const;
	UINT16		GetFetchAccountCount() const;
	UINT16		GetComposeAccountCount() const;
	UINT16		GetMailNewsAccountCount() const;
	UINT16		GetSignatureCount(OpStringC& signature_file) const;
    Account*	GetFirstAccount() const {return (Account*)m_accountlist.First();}
    UINT16		GetNextAvailableAccountId() const {return m_next_available_account_id;}
    OP_STATUS	GetAndIncrementNextAvailableAccountId(UINT16& next_available_account_id);

	void		ClearAccountPasswords();

	BOOL		GetUpdatesNeeded() const { return m_old_version != 0; }
	int			GetOldVersion() const { return m_old_version; }

	OP_STATUS	CloseAllConnections();
	OP_STATUS	ToOnlineMode();

	/**
	  * Whether all accounts are configured to use low bandwidth mode
	  */
	BOOL		 IsLowBandwidthModeEnabled();

	/**
	  * Set whether all accounts should use low bandwidth mode or not
	  */
	void		 SetLowBandwidthMode( BOOL enabled );

	/** Check whether there is an account busy sending or receiving messages
	  * @param incoming Whether to check receiving (instead of sending)
	  * @return Whether there is an account busy at this moment
	  */
	BOOL		 IsBusy(BOOL incoming);

	INT16 GetNumberOfKnownAccountsProviders();
private:
    AccountManager(const AccountManager&);
    AccountManager& operator =(const AccountManager&);
};

#endif //M2_SUPPORT

#endif // ACCOUNTMGR_H
