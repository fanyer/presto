/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/mailfiles.h"

#include "adjunct/m2/src/backend/pop/UidlManager.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/string/hash.h"

#ifdef M2_KESTREL_BETA_COMPATIBILITY
# include "modules/search_engine/ACT.h"
#endif // M2_KESTREL_BETA_COMPATIBILITY

// remove this before peregrine alpha 1:
#ifdef M2_FIRST_PEREGRINE_COMPATIBILITY
    class OldPeregrineUidlKey 
    { 
    public:  
        OpStringC8    GetUIDL() const { return m_uidl; } 
        message_gid_t GetM2ID() const { return m_data[1]; } 
        time_t        GetTime() const { return m_data[2]; } 
 
        BOOL operator<(const OldPeregrineUidlKey& rhs) const
		{ 
			if (m_data[0] < rhs.m_data[0]) 
			{ 
				return TRUE; 
			} 
			else if (m_data[0] == rhs.m_data[0]) 
			{ 
				return op_strncmp(m_uidl, rhs.m_uidl, MaxUIDLLength) < 0; 
			} 
		 
			return FALSE; 
		} 
 
    private: 
        static const size_t MaxUIDLLength = 70; 
 
        unsigned m_data[3]; 
        char     m_uidl[MaxUIDLLength]; 
    }; 
#endif // M2_FIRST_PEREGRINE_COMPATIBILITY

#ifdef M2_95X_COMPATIBILITY
class Old95xUidlKey
{
	public:
		OpStringC8		GetUIDL() const { return m_uidl; }
		message_gid_t	GetM2ID() const { return m_data[1]; }

		BOOL operator<(const Old95xUidlKey& rhs) const
		{
			if (m_data[0] < rhs.m_data[0])
			{
				return TRUE;
			}
			else if (m_data[0] == rhs.m_data[0])
			{
				return op_strncmp(m_uidl, rhs.m_uidl, MaxUIDLLength) < 0;
			}

			return FALSE;
		}

	private:
		static const size_t MaxUIDLLength = 70;

		unsigned m_data[2];
		char     m_uidl[MaxUIDLLength];
};
#endif // M2_95X_COMPATIBILITY



/***********************************************************************************
 ** Constructor
 **
 ** UidlManager::UidlManager
 **
 ***********************************************************************************/
UidlManager::UidlManager(MessageDatabase& database)
	: m_database(database)
{
	// Ideally we want to group this file, but we can't because of backend initialization timing
	// m_database.GroupSearchEngineFile(m_tree.GetGroupMember());
}


/***********************************************************************************
 ** Destructor
 **
 ** UidlManager::~UidlManager
 **
 ***********************************************************************************/
UidlManager::~UidlManager()
{
	OpStatus::Ignore(m_tree.Close());
	m_database.RemoveCommitListener(this);
}


/***********************************************************************************
 ** Initialization function, run before using any other function
 **
 ** UidlManager::Init
 ** @param account_id Account ID of the account this UidlManager manager will manage
 **
 ***********************************************************************************/
OP_STATUS UidlManager::Init(int account_id)
{
	// Construct path of ACT file
	OpString filename;

	// let's try to keep the version number of this file in sync with the account version
	RETURN_IF_ERROR(MailFiles::GetPOPUIDLFilename(account_id,filename));

	// Open ACT file
	RETURN_IF_ERROR(m_tree.Open(filename.CStr(), BlockStorage::OpenReadWrite, M2_BS_BLOCKSIZE));

	// Listen for commits
	RETURN_IF_ERROR(m_database.AddCommitListener(this));

	return OpStatus::OK;
}


/***********************************************************************************
 ** Call this function when a UIDL has been successfully fetched
 **
 ** UidlManager::AddUIDL
 ** @param uidl The UIDL that has been fetched
 **
 ***********************************************************************************/
OP_STATUS UidlManager::AddUIDL(const OpStringC8& uidl, message_gid_t m2_id)
{
	if (uidl.IsEmpty())
		return OpStatus::OK;
	
	UidlKey key(uidl, m2_id);

	if (g_m2_engine->GetStore()->GetMessageFlag(m2_id,Message::IS_READ))
		key.SetFlags(key.GetFlags() | (1 << IS_READ));
	// Add to UidlManager index
	RETURN_IF_ERROR(m_tree.Insert(key));

	// Request commit of changes
	return m_database.RequestCommit();
}


/***********************************************************************************
 ** Remove a UIDL
 **
 ** UidlManager::RemoveUIDL
 ** @param uidl The UIDL that should be removed
 **
 ***********************************************************************************/
OP_STATUS UidlManager::RemoveUIDL(const OpStringC8& uidl)
{
	if (uidl.IsEmpty())
		return OpStatus::OK;

	UidlKey search_key(uidl, 0);

	// Remove from UidlManager index
	RETURN_IF_ERROR(m_tree.Delete(search_key));

	// Request a commit
	return m_database.RequestCommit();
}


/***********************************************************************************
 ** Get M2 ID for a UIDL
 **
 ** UIDL::GetUIDL
 ** @param uidl UIDL to check for
 ** @return the id, or 0 if not found
 **
 ***********************************************************************************/
message_gid_t UidlManager::GetUIDL(const OpStringC8& uidl)
{
	if (uidl.IsEmpty())
		return 0;

	UidlKey search_key(uidl, 0);

	if (m_tree.Search(search_key) == OpBoolean::IS_TRUE)
		return search_key.GetM2ID();

	return 0;
}
/***********************************************************************************
 ** Get the received time for a UIDL
 **
 ** UIDL::GetUIDL
 ** @param uidl UIDL to check for
 ** @return the time the message was received, or 0 if not found
 **
 ***********************************************************************************/
time_t UidlManager::GetReceivedTime(const OpStringC8& uidl)
{
	if (uidl.IsEmpty())
		return 0;

	UidlKey search_key(uidl, 0);

	if (m_tree.Search(search_key) == OpBoolean::IS_TRUE)
		return search_key.GetTime();

	return 0;
}
/***********************************************************************************
** Get a flag for a UIDL
**
** @param uidl UIDL to check for
** @param flag - the flag you want to know the value of
** @return the value of the flag
************************************************************************************/
BOOL UidlManager::GetUIDLFlag(const OpStringC8& uidl, Flags flag)
{ 
	if (uidl.IsEmpty())
		return FALSE;

	UidlKey search_key(uidl, 0);

	if (m_tree.Search(search_key) == OpBoolean::IS_TRUE)
		return search_key.GetFlags() & (1 << flag); 

	return FALSE;
}
	
/***********************************************************************************
** Set a flag for a UIDL
** @param uidl UIDL to check for
** @param flag - the flag you want to know the value of
** @param value - TRUE or FALSE
** @return the value of the flag
************************************************************************************/
OP_STATUS UidlManager::SetUIDLFlag(const OpStringC8& uidl, Flags flag, BOOL value)
{
	if (uidl.IsEmpty())
		return OpStatus::ERR;

	UidlKey search_key(uidl, 0);

	if (m_tree.Search(search_key) == OpBoolean::IS_TRUE)
	{
		// Fetch existing flags and set new flags
		INT32 oldflags = search_key.GetFlags();
		INT32 newflags = value ? oldflags | (1 << flag) : oldflags & ~(1 << flag);
		search_key.SetFlags(newflags);
		return m_tree.Insert(search_key,TRUE);
	}

	return OpStatus::ERR;
}

/***********************************************************************************
 ** Update old UIDL storages
 **
 ** UIDL::UpdateOldUIDL
 ***********************************************************************************/
OP_STATUS UidlManager::UpdateOldUIDL(Account* account, int old_version)
{
	if (!old_version)
		return OpStatus::OK;

#ifdef M2_MERLIN_COMPATIBILITY
	if (old_version < 4)
		RETURN_IF_ERROR(UpdateMerlinUIDL(account));
#endif

#ifdef M2_KESTREL_BETA_COMPATIBILITY
	if (old_version == 4 || old_version == 5)
		RETURN_IF_ERROR(UpdateKestrelBetaUIDL(account));
#endif

#ifdef M2_95X_COMPATIBILITY
	if (old_version == 6)
		RETURN_IF_ERROR(UpdateKestrelFinalUIDL(account));
#endif

#ifdef M2_FIRST_PEREGRINE_COMPATIBILITY
	if (old_version == 7)
		RETURN_IF_ERROR(UpdateFirstPeregrineUIDL(account));
#endif

	return OpStatus::OK;
}


/***********************************************************************************
 ** Receive commit message from store
 **
 ***********************************************************************************/
void UidlManager::OnCommitted()
{
	// Make sure our UidlManager database is committed
	OpStatus::Ignore(m_tree.Commit());
}


#ifdef M2_MERLIN_COMPATIBILITY
/***********************************************************************************
 ** Update old UIDL storage - Merlin
 **
 ** UIDL::UpdateMerlinUIDL
 ** @param account_id Account we're updating
 ***********************************************************************************/
OP_STATUS UidlManager::UpdateMerlinUIDL(Account* account)
{
	if (!account)
		return OpStatus::ERR_NULL_POINTER;

	// Update old-style options file with UIDLs
	OpString old_options_file;

	RETURN_IF_ERROR(account->GetIncomingOptionsFile(old_options_file));

	if (old_options_file.IsEmpty())
		return OpStatus::OK;

	// Try to open options file
	BOOL    file_exists;
	OpFile  file;

	RETURN_IF_ERROR(file.Construct(old_options_file.CStr()));
	RETURN_IF_ERROR(file.Exists(file_exists));
	if (!file_exists)
		return OpStatus::OK;

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	OpString8 uidl_line;
	OpString8 uidl;

	if (!uidl.Reserve(70)) // UIDLs are max 70 characters
		return OpStatus::ERR_NO_MEMORY;

	// Read UIDLs one by one
	while (!file.Eof())
	{
		// Read line from file
		RETURN_IF_ERROR(file.ReadLine(uidl_line));
		if (uidl_line.HasContent())
		{
			// Parse m2 id and uidl
			int m2_id;

			if (op_sscanf(uidl_line.CStr(), "%d %70s", &m2_id, uidl.CStr()) >= 2)
			{
				// Set UIDL on message
				Message message;

				if (OpStatus::IsSuccess(m_database.GetMessage(m2_id, FALSE, message)))
				{
					OpStatus::Ignore(message.SetInternetLocation(uidl));
					OpStatus::Ignore(m_database.UpdateMessageMetaData(message));
				}

				// Set UIDL in UIDL manager
				RETURN_IF_ERROR(AddUIDL(uidl, m2_id));
			}
		}
	}

	// Close and delete file that has been converted
	RETURN_IF_ERROR(file.Close());
	RETURN_IF_ERROR(file.Delete());

	return OpStatus::OK;
}
#endif // M2_MERLIN_COMPATIBILITY


#ifdef M2_KESTREL_BETA_COMPATIBILITY
/***********************************************************************************
 ** Update old UIDL storage - Kestrel Beta
 **
 ** UIDL::UpdateKestrelBetaUIDL
 ** @param account_id Account we're updating
 ***********************************************************************************/
OP_STATUS UidlManager::UpdateKestrelBetaUIDL(Account* account)
{
	// Construct path of ACT file
	OpString filename;

	OpString tmp_storage;
	RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%s%cpop3%cuid_account%d"),
					g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MAIL_FOLDER, tmp_storage),
					PATHSEPCHAR, PATHSEPCHAR,
					account->GetAccountId()));

	// Open ACT file
	ACT act;
	if (OpStatus::IsError(act.Open(filename.CStr(), BlockStorage::OpenRead)))
		return OpStatus::OK;

	// We don't simply copy the old database to the new one, because the old database was
	// case-insensitive, while the new one isn't (and shouldn't be). To get the original
	// uidl, we rely on the internet location saved in the store. This means we miss
	// messages that were deleted from Opera but not from the server when upgrading this
	// way.

	// Get all mails in account
	Index* account_index = m_database.GetIndex(IndexTypes::FIRST_ACCOUNT + account->GetAccountId());
	if (!account_index)
		return OpStatus::ERR_NULL_POINTER;

	// Make sure the index is read into memory
	RETURN_IF_ERROR(account_index->PreFetch());

	// Walk through messages in index
	for (INT32SetIterator it(account_index->GetIterator()); it; it++)
	{
		Message message;
		message_gid_t m2_id = it.GetData();

		RETURN_IF_ERROR(m_database.GetMessage(m2_id, FALSE, message));

		// Only insert the UIDL into the new database if it's found in the old one
		if (act.Search(message.GetInternetLocation().CStr()))
			RETURN_IF_ERROR(AddUIDL(message.GetInternetLocation(), m2_id));
	}

	// Close and delete the ACT file
	RETURN_IF_ERROR(act.Close());

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Delete());

	return OpStatus::OK;
}
#endif // M2_KESTREL_BETA_COMPATIBILITY

#ifdef M2_95X_COMPATIBILITY
/***********************************************************************************
 ** Update old UIDL storage - Kestrel Beta
 **
 ** UIDL::UpdateKestrelBetaUIDL
 ** @param account_id Account we're updating
 ***********************************************************************************/
OP_STATUS UidlManager::UpdateKestrelFinalUIDL(Account* account)
{
	// Construct path of ACT file
	OpString filename;

	OpString tmp_storage;
	RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%s%cpop3%cuidl_account%d"),
					g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MAIL_FOLDER, tmp_storage),
					PATHSEPCHAR, PATHSEPCHAR,
					account->GetAccountId()));

	SingleBTree<Old95xUidlKey> old_tree;
	// Open ACT file
	RETURN_IF_ERROR(old_tree.Open(filename.CStr(), BlockStorage::OpenRead, M2_BS_BLOCKSIZE));

	OpAutoPtr <SearchIterator <Old95xUidlKey> > tree_iterator (old_tree.SearchFirst());
	if (!tree_iterator.get())
		return OpStatus::ERR_NO_MEMORY;

	if (!tree_iterator->Empty())
	{
		// loop through the old Uidls and create a new one for all of them
		do
		{
			RETURN_IF_ERROR(AddUIDL(tree_iterator->Get().GetUIDL(), tree_iterator->Get().GetM2ID()));
		} while (tree_iterator->Next());
	}

	// open iterators need to be closed before closing the tree
	tree_iterator.reset();

	// close and delete the old tree
	RETURN_IF_ERROR(old_tree.Close());

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Delete());

	return OpStatus::OK;
}
#endif // M2_95X_COMPATIBILITY


// remove this before first peregrine alpha release:
#ifdef M2_FIRST_PEREGRINE_COMPATIBILITY
/***********************************************************************************
 ** Update old UIDL storage - Kestrel Beta
 **
 ** UIDL::UpdateFirstPeregrineUIDL
 ** @param account_id Account we're updating
 ***********************************************************************************/
OP_STATUS UidlManager::UpdateFirstPeregrineUIDL(Account* account)
{
	// Construct path of ACT file
	OpString filename;

	OpString tmp_storage;
	RETURN_IF_ERROR(filename.AppendFormat(UNI_L("%s%cpop3%cuidl_account%d_ver2"),
					g_folder_manager->GetFolderPathIgnoreErrors(OPFILE_MAIL_FOLDER, tmp_storage),
					PATHSEPCHAR, PATHSEPCHAR,
					account->GetAccountId()));

	SingleBTree<OldPeregrineUidlKey> old_tree;
	// Open ACT file
	RETURN_IF_ERROR(old_tree.Open(filename.CStr(), BlockStorage::OpenRead, M2_BS_BLOCKSIZE));

	OpAutoPtr <SearchIterator <OldPeregrineUidlKey> > tree_iterator (old_tree.SearchFirst());
	if (!tree_iterator.get())
		return OpStatus::ERR_NO_MEMORY;

	if (!tree_iterator->Empty())
	{
		// loop through the old Uidls and create a new one for all of them
		do
		{
			RETURN_IF_ERROR(AddUIDL(tree_iterator->Get().GetUIDL(), tree_iterator->Get().GetM2ID()));
		} while (tree_iterator->Next());
	}

	// open iterators need to be closed before closing the tree
	tree_iterator.reset();

	// close and delete the old tree
	RETURN_IF_ERROR(old_tree.Close());

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr()));
	RETURN_IF_ERROR(file.Delete());

	return OpStatus::OK;
}
#endif // M2_FIRST_PEREGRINE_COMPATIBILITY


/***********************************************************************************
 ** Constructor
 **
 ** UidlManager::UidlKey::UidlKey
 ***********************************************************************************/
UidlManager::UidlKey::UidlKey(const OpStringC8& uidl, message_gid_t m2_id)
{
	m_data[0] = Hash::String(uidl.CStr(), uidl.Length()) & 0x7FFFFFFF;
	m_data[1] = m2_id;
	m_data[2] = g_timecache->CurrentTime();
	m_data[3] = 0;

	if (uidl.HasContent())
		op_strncpy(m_uidl, uidl.CStr(), MaxUIDLLength);
	else
		op_memset(m_uidl, 0, MaxUIDLLength);
}

/***********************************************************************************
 ** Comparison
 **
 ** UidlManager::UidlKey::operator<
 ***********************************************************************************/
BOOL UidlManager::UidlKey::operator<(const UidlManager::UidlKey& rhs) const
{
	if (m_data[0] < rhs.m_data[0])
	{
		return TRUE;
	}
	else if (m_data[0] == rhs.m_data[0])
	{
		return op_strncmp(m_uidl, rhs.m_uidl, MaxUIDLLength) < 0;
	}

	return FALSE;
}

#endif //M2_SUPPORT
