/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * George Refseth (rfz)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"
#include "modules/util/opfile/opfile.h"
#include "adjunct/m2/src/engine/store/storereaders.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/store/mboxstoremanager.h"
#include "adjunct/m2/src/engine/store/store3.h"

class Store_2_X_Reader : public StoreReader
{
protected:
	int					m_current_block;
	BlockFile *			m_mail_database;
	MboxStoreManager	m_store_manager;
	OpString			m_mail_path;

#ifdef OPERA_BIG_ENDIAN
	struct eight_eight_sixteen {
		UINT16 sixteen;
		UINT8  eight2;
		UINT8  eight1;
	} m_ees;
#else
	struct eight_eight_sixteen {
		UINT8  eight1;
		UINT8  eight2;
		UINT16 sixteen;
	} m_ees;
#endif
public:
							Store_2_X_Reader(BlockFile * maildatabase)
								:	m_current_block(0),
									m_mail_database(maildatabase) {}
	virtual					~Store_2_X_Reader() { OP_DELETE(m_mail_database); }
	virtual OP_STATUS		Init(OpString& error_message, const OpStringC& path_to_maildir);
	BOOL					MoreMessagesLeft();
};

class Store_2_4_Reader : public Store_2_X_Reader
{
public:
					Store_2_4_Reader(BlockFile * maildatabase) : Store_2_X_Reader(maildatabase) {}
	virtual 		~Store_2_4_Reader(){}
	StoreRevision	GetStoreRevision() { return REVISION_2_4; }
	OP_STATUS		GetNextAccountMessage(Message& message, UINT16 account, BOOL override, BOOL import);
	OP_STATUS		GetNextMessage(Message& message, BOOL override, BOOL import);
	OP_STATUS		InitializeAccount(UINT16 accountID, Account * account, OpFileLength *total_message_count = NULL);

private:
	OP_STATUS		GetNumberOfMessages(UINT16 account, OpFileLength &message_count);

	struct M2idUIDL {
		message_gid_t id;
		OpString8	uidl;
	};
	BOOL			m_uidl_list_valid;
	OpAutoVector<M2idUIDL> m_uidl_list;
};

class Store_2_3_Reader : public Store_2_X_Reader
{
public:
					Store_2_3_Reader(BlockFile * maildatabase) : Store_2_X_Reader(maildatabase) {}
	virtual			~Store_2_3_Reader(){}
	StoreRevision 	GetStoreRevision() { return REVISION_2_3; }
	OP_STATUS		GetNextAccountMessage(Message& message, UINT16 account, BOOL override, BOOL import) { return OpStatus::ERR; }
	OP_STATUS		GetNextMessage(Message& message, BOOL override, BOOL import) { return OpStatus::ERR; }
	OP_STATUS		InitializeAccount(UINT16 accountID, Account * account, OpFileLength *total_message_count = NULL) { return OpStatus::ERR;}
};

class Store_2_2_Reader : public Store_2_X_Reader
{
public:
					Store_2_2_Reader(BlockFile * maildatabase) : Store_2_X_Reader(maildatabase) {}
	virtual 		~Store_2_2_Reader(){}
	StoreRevision	GetStoreRevision() { return REVISION_2_2; }
	OP_STATUS		GetNextAccountMessage(Message& message, UINT16 account, BOOL override, BOOL import) { return OpStatus::ERR; }
	OP_STATUS		GetNextMessage(Message& message, BOOL override, BOOL import) { return OpStatus::ERR; }
	OP_STATUS		InitializeAccount(UINT16 accountID, Account * account, OpFileLength *total_message_count = NULL) { return OpStatus::ERR;}
};

class Store_2_1_Reader : public Store_2_X_Reader
{
public:
					Store_2_1_Reader(BlockFile * maildatabase) : Store_2_X_Reader(maildatabase) {}
	virtual 		~Store_2_1_Reader(){}
	StoreRevision	GetStoreRevision() { return REVISION_2_1; }
	OP_STATUS		GetNextAccountMessage(Message& message, UINT16 account, BOOL override, BOOL import) { return OpStatus::ERR; }
	OP_STATUS		GetNextMessage(Message& message, BOOL override, BOOL import) { return OpStatus::ERR; }
	OP_STATUS		InitializeAccount(UINT16 accountID, Account * account, OpFileLength *total_message_count = NULL) { return OpStatus::ERR;}
};

class Store_3_0_Reader : public StoreReader, public Store3
{
	UINT16			m_account;				// current importing account
	OpINT32Vector	m_account_list;			// list of all messages associated with this account
	BOOL			m_valid_account_list;	// the m_account_list is populated
public:
					Store_3_0_Reader() : Store3(NULL), m_account(0), m_valid_account_list(FALSE) {}
	virtual			~Store_3_0_Reader(){}
	StoreRevision	GetStoreRevision() { return REVISION_3_0; }
	OP_STATUS		GetNextAccountMessage(Message& message, UINT16 account, BOOL override, BOOL import);
	OP_STATUS		GetNextMessage(Message& message, BOOL override, BOOL import);
	OP_STATUS		Init(OpString& error_message, const OpStringC& path_to_maildir);
	OP_STATUS		InitializeAccount(UINT16 accountID, Account * account, OpFileLength *total_message_count = NULL);
	OP_STATUS		CreateAccountMessageList(int account);
	BOOL			MoreMessagesLeft();
};

/* static */
StoreReader* StoreReader::Create(OpString& error_message, const OpStringC& path_to_maildir)
{
	StoreReader * store_reader = NULL;
	OpString maildatabasepath;
	OpFile store3;
	BOOL exists;
	RETURN_VALUE_IF_ERROR(maildatabasepath.Set(path_to_maildir), store_reader);
	RETURN_VALUE_IF_ERROR(maildatabasepath.Append(PATHSEP), store_reader);
	RETURN_VALUE_IF_ERROR(maildatabasepath.Append(UNI_L("omailbase.dat")), store_reader);
	if (	OpStatus::IsSuccess(store3.Construct(maildatabasepath.CStr()))
		&&	OpStatus::IsSuccess(store3.Exists(exists))
		&&	exists)
	{
		store_reader = OP_NEW(Store_3_0_Reader, ());
		if (store_reader)
		{
			if (OpStatus::IsError(store_reader->Init(error_message, path_to_maildir)))
			{
				OP_DELETE(store_reader);
				store_reader = NULL;
			}
		}
	}
	else
	{
		OpFile store2;
		RETURN_VALUE_IF_ERROR(maildatabasepath.Set(path_to_maildir), store_reader);
		RETURN_VALUE_IF_ERROR(maildatabasepath.Append(PATHSEP), store_reader);
		RETURN_VALUE_IF_ERROR(maildatabasepath.Append(UNI_L("mailbase.dat")), store_reader);
		if (	OpStatus::IsSuccess(store2.Construct(maildatabasepath.CStr()))
			&&	OpStatus::IsSuccess(store2.Exists(exists))
			&&	exists)
		{
			BlockFile * t_store = OP_NEW(BlockFile, ());
			if (	t_store
				&&	OpStatus::IsSuccess(t_store->Init(path_to_maildir.CStr(), UNI_L("mailbase"), 0, 48, FALSE, NULL, TRUE)))
			{
				t_store->Seek(0, 0);
				INT32 format;
				if (OpStatus::IsError(t_store->ReadValue(format)))
				{
					OP_DELETE(t_store);
				}
				else
				{
					switch(format)
					{
					case 4:
						store_reader = OP_NEW(Store_2_4_Reader, (t_store));
						break;
					case 3:
						store_reader = OP_NEW(Store_2_3_Reader, (t_store));
						break;
					case 2:
						store_reader = OP_NEW(Store_2_2_Reader, (t_store));
						break;
					case 1:
						store_reader = OP_NEW(Store_2_1_Reader, (t_store));
						break;
					default:
						OP_DELETE(t_store);
						break;
					}
					if (store_reader)
					{
						if (OpStatus::IsError(store_reader->Init(error_message, path_to_maildir)))
						{
							OP_DELETE(store_reader);
							store_reader = NULL;
						}
					}
					else
						OP_DELETE(t_store);
				}
			}
		}
	}
	return store_reader;
}

/***************************************************************************************
 * Store2 import
 **************************************************************************************/
BOOL Store_2_X_Reader::MoreMessagesLeft()
{
	return !(m_current_block >= m_mail_database->GetBlockCount());
}

OP_STATUS Store_2_X_Reader::Init(OpString& error_message, const OpStringC& path_to_maildir)
{
	RETURN_IF_ERROR(m_mail_path.Set(path_to_maildir));
	RETURN_IF_ERROR(error_message.Set(path_to_maildir));
	m_current_block = 1;
	RETURN_IF_ERROR(m_mail_database->Seek(1, 0));
	return m_store_manager.SetStorePath(path_to_maildir);
}

/***************************************************************************************
 * Store2 maildatabase revision 4 import
 **************************************************************************************/
OP_STATUS Store_2_4_Reader::InitializeAccount(UINT16 accountID, Account * account, OpFileLength* total_message_count)
{
	m_uidl_list_valid = FALSE;
	OP_STATUS sts = OpStatus::OK;

	if (account->GetIncomingProtocol() == AccountTypes::POP)
	{
		m_uidl_list.Clear();
		OpFile incoming;
		OpString incoming_name;
		BOOL exists;
		RETURN_IF_ERROR(incoming_name.Set(m_mail_path));
		RETURN_IF_ERROR(incoming_name.Append(PATHSEP));
		RETURN_IF_ERROR(incoming_name.AppendFormat(UNI_L("incoming%d.txt"), (int)accountID));
		if (	OpStatus::IsSuccess(incoming.Construct(incoming_name.CStr()))
			&&	OpStatus::IsSuccess(incoming.Exists(exists))
			&&	exists)
		{
			RETURN_IF_ERROR(incoming.Open(OPFILE_READ));

			OpString8 uidl_line;
			OpString8 uidl;

			if (!uidl.Reserve(70)) // UIDLs are max 70 characters
				return OpStatus::ERR_NO_MEMORY;

			// Read UIDLs one by one
			while (!incoming.Eof())
			{
				// Read line from incoming
				if (OpStatus::IsSuccess(sts = incoming.ReadLine(uidl_line)))
				{
					if (uidl_line.HasContent())
					{
						// Parse m2 id and uidl
						message_gid_t m2_id;

						if (op_sscanf(uidl_line.CStr(), "%u %70s", &m2_id, uidl.CStr()) >= 2)
						{
							M2idUIDL * mu = OP_NEW(M2idUIDL, ());
							if (mu && OpStatus::IsSuccess(sts = mu->uidl.Set(uidl)))
							{
								mu->id = m2_id;
								sts = m_uidl_list.Add(mu);
								if (OpStatus::IsError(sts))
								{
									OP_DELETE(mu);
									return sts;
								}
							}
							else
							{
								OP_DELETE(mu);
								return sts;
							}
						}
					}
				}
			}

			RETURN_IF_ERROR(incoming.Close());

			m_uidl_list_valid = TRUE;
		}
	}

	// Find total number of messages
	if(total_message_count)
	{
		GetNumberOfMessages(accountID, *total_message_count);
		OpString error_message;
		Init(error_message, m_mail_path);
	}
	return sts;
}

OP_STATUS Store_2_4_Reader::GetNumberOfMessages(UINT16 account, OpFileLength &message_count)
{
	message_count = 0;

	while(MoreMessagesLeft())
	{
		m_mail_database->Seek(m_current_block, 0);

		message_gid_t message_id = 0;

		// Read message ID
		m_mail_database->ReadValue((INT32&)message_id);

		// Check if this was an empty entry
		if (message_id != 0)
		{
			// Read message account id

			m_mail_database->ReadValue(m_current_block, 5*sizeof(INT32), (INT32&)m_ees);

			if (account && account == m_ees.sixteen)
			{
				message_count++;
			}
		}
		m_current_block++;
	}
	return OpStatus::OK;
}

OP_STATUS Store_2_4_Reader::GetNextAccountMessage(Message& message, UINT16 account, BOOL override, BOOL import)
{
	message_gid_t message_id = 0;
	UINT16 t_account = message.GetAccountId();
	// Write to update log
	//RETURN_IF_ERROR(WriteLog());

	// Read message ID
	m_mail_database->ReadValue((INT32&)message_id);

	m_current_block++;
	// Check if this was an empty entry
	if (message_id == 0
		// || m_new_store.MessageExists(message_id) TODO: override???
		)
	{
		m_mail_database->Seek(m_current_block, 0);
	}
	else
	{
		// Read all message details
		INT32      tmp_int;
		INT64      mbx_data;
		MboxStore* store;

		m_mail_database->ReadValue(tmp_int);
		message.SetDateHeaderValue(Header::DATE, tmp_int);
		m_mail_database->ReadValue(tmp_int);
		message.SetMessageSize(tmp_int);
		m_mail_database->ReadValue(tmp_int);
		message.SetAllFlags(tmp_int);
		m_mail_database->ReadValue(tmp_int);
		mbx_data = tmp_int;
		m_mail_database->ReadValue((INT32&)m_ees);

		message.SetAccountId(m_ees.sixteen, FALSE);
		message.SetId(message_id);

		if (account && account != m_ees.sixteen)
		{
			m_mail_database->Seek(m_current_block, 0);
			return OpStatus::ERR;
		}

		m_mail_database->ReadValue(tmp_int);
		message.SetParentId(tmp_int);

		// Unused items
		m_mail_database->ReadValue(tmp_int); // prev_from
		m_mail_database->ReadValue(tmp_int); // prev_to
		m_mail_database->ReadValue(tmp_int); // prev_subject
		m_mail_database->ReadValue(tmp_int); // message id hash
		m_mail_database->ReadValue(tmp_int); // message location hash

		RETURN_IF_ERROR(m_store_manager.GetMessage(mbx_data, message, store, !import));

		if (import)
		{
			message.SetAccountId(t_account, FALSE);
			message.SetId(0);
		}

		if (m_uidl_list_valid)
		{
			for (UINT32 i = 0; i < m_uidl_list.GetCount(); i++)
			{
				M2idUIDL * mu = m_uidl_list.Get(i);
				if (mu->id == message_id)
				{
					RETURN_IF_ERROR(message.SetInternetLocation(mu->uidl));
					m_uidl_list.Delete(mu);
				}
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS Store_2_4_Reader::GetNextMessage(Message& message, BOOL override, BOOL import)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

/***************************************************************************************
 * Store3 import
 **************************************************************************************/
OP_STATUS Store_3_0_Reader::InitializeAccount(UINT16 account, Account *, OpFileLength *total_message_count)
{
	OP_STATUS sts;
	m_account = account;
	if (OpStatus::IsSuccess(sts = CreateAccountMessageList(account)))
		m_valid_account_list = TRUE;
	else
		m_valid_account_list = FALSE;

	if(total_message_count && m_valid_account_list)
	{
		*total_message_count = m_account_list.GetCount();
	}
	return sts;
}

OP_STATUS Store_3_0_Reader::CreateAccountMessageList(int account)
{
	INT32 account_id;
	m_account_list.Clear();

	// Loop through all messages
	int          block_size = m_maildb_backend->GetBlockSize();
	OpFileLength file_size  = m_maildb_backend->GetFileSize();

	for (OpFileLength pos = 0; pos < file_size; pos += block_size)
	{
		if (m_maildb_backend->IsStartBlock(pos) &&
			OpStatus::IsSuccess(m_maildb->Goto(pos / block_size)))
		{
			// Build M2 ID index
			message_gid_t m2_id;

			m_maildb->GetField(STORE_M2_ID).GetValue(&m2_id);
			m_current_id = m2_id;

			m_maildb->GetField(STORE_ACCOUNT_ID).GetValue(&account_id);
			if (account_id == account)
				m_account_list.Add(m_current_id);
		}
	}

	return OpStatus::OK;
}

OP_STATUS Store_3_0_Reader::GetNextAccountMessage(Message& message, UINT16 account, BOOL override, BOOL import)
{
	if (m_valid_account_list && m_account_list.GetCount() > 0)
	{
		if (!HasFinishedLoading())
			return OpStatus::ERR_YIELD;

		int message_id = m_account_list.Get(0);
		m_account_list.Remove(0);
		RETURN_IF_ERROR(GetMessage(message, message_id, import));
		UINT16 t_account = 0;
		if (import)
		{
			t_account = message.GetAccountId();
			message.SetAccountId(account, FALSE);
		}
		RETURN_IF_ERROR(GetMessageData(message));
		if (import)
		{
			message.SetAccountId(t_account, FALSE);
			message.SetId(0);
		}
	}
	else
	{
		m_valid_account_list = FALSE;
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}

OP_STATUS Store_3_0_Reader::GetNextMessage(Message& message, BOOL override, BOOL import)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS Store_3_0_Reader::Init(OpString& error_message, const OpStringC& path_to_maildir)
{
	Store3::SetReadOnly();
	return Store3::Init(error_message, path_to_maildir);
}

BOOL Store_3_0_Reader::MoreMessagesLeft()
{
	if (!m_valid_account_list)
	{
		return TRUE;
	}
	else
	{
		return m_account_list.GetCount() > 0;
	}
}

#endif // M2_SUPPORT
