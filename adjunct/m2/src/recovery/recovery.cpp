/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2002 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Alexander Remen (alexr)
*/
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"

#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/indexer.h"				// for the ghost buster code
#include "adjunct/m2/src/engine/store/store3.h"			// for the Store3::MessageIdKey
#include "adjunct/m2/src/backend/imap/imap-uid-manager.h" // for the ImapUidManager::ImapUidKey
#include "adjunct/m2/src/backend/pop/UidlManager.h"		// for the UidlManager::UidlKey
#include "adjunct/m2/src/backend/rss/rss-protocol.h"	// for the RSSProtocol::NewsfeedID
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/recovery/recovery.h"

#include "modules/prefsfile/prefsfile.h"				// for going through the accounts.ini file
#include "modules/search_engine/SingleBTree.h"
#include "modules/search_engine/StringTable.h"
#include "modules/search_engine/ACT.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/opstrlst.h"

/***********************************************************************************
 **
 **
 ** MailRecovery::MailRecovery
 ***********************************************************************************/
MailRecovery::MailRecovery()
: m_recovery_log()
{
	OpString filename;
	MailFiles::GetRecoveryLogFilename(filename);
	m_recovery_log = OP_NEW(OpFile,());
	m_recovery_log->Construct(filename.CStr());
}
/***********************************************************************************
 **
 **
 ** MailRecovery::~MailRecovery
 ***********************************************************************************/
MailRecovery::~MailRecovery()
{
	OP_DELETE(m_recovery_log);
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckConsistencyAndFixAllMailDBs
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckConsistencyAndFixAllMailDBs()
{
	OpStringC8 log_heading("Consistency check and repair");
	OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Initialized"));

	OP_STATUS ret = CheckAndFixAccountSpecificDBs();
	
	if (ret == OpStatus::ERR_OUT_OF_RANGE)
	{
		// m2 should run as usual so that the error messages are shown to the user
		return OpStatus::OK;
	}
	if (ret != OpStatus::OK)
	{
		// we have to handle this error
		OP_ASSERT(!"something went wrong when fixing account databases, implement error handling");
	}

	if (OpStatus::IsError(CheckAndFixMessageIdDatabase()))
	{
		// we have to handle this error
		OP_ASSERT(!"message id database is broken, implement fallback");
	}


	if (OpStatus::IsError(CheckAndFixIndexerStringTable()))
	{
		// we have to handle this error
		OP_ASSERT(!"indexer database is really broken and unfixable, implement fallback");
	}

	OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Finished"));

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckAndFixMessageIdDatabase
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckAndFixMessageIdDatabase()
{
	OpString filename;
	RETURN_IF_ERROR(MailFiles::GetMessageIDFilename(filename));

	SingleBTree<Store3::MessageIdKey> btree;
	RETURN_IF_ERROR(btree.Open(filename.CStr(), BlockStorage::OpenRead, M2_BS_BLOCKSIZE));

	OpStringC8 log_heading("Message ID");
	if (btree.CheckConsistency(2) == OpBoolean::IS_FALSE)
	{
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database not consistent, starting recovery."));

		RETURN_IF_ERROR(btree.Close());
#ifdef SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
		if (OpStatus::IsSuccess(btree.Recover(filename.CStr(), M2_BS_BLOCKSIZE)))
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery succeeded"));
		}
		else
#endif // SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery failed"));
		}

	}
	else
	{
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database consistent"));
		RETURN_IF_ERROR(btree.Close());
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckAndFixAccountSpecificDBs
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckAndFixAccountSpecificDBs()
{
	OpString account_filename;
	RETURN_IF_ERROR(MailFiles::GetAccountsINIFilename(account_filename));
	
	OpFile file;
	RETURN_IF_ERROR(file.Construct(account_filename.CStr()));
	PrefsFile account_file(PREFS_INI);	
	RETURN_IF_LEAVE(account_file.ConstructL());
	RETURN_IF_LEAVE(account_file.SetFileL(&file));

	RETURN_IF_LEAVE(account_file.LoadAllL());
 
	int accounts_version;

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(&account_file, "Accounts", "Version", CURRENT_M2_ACCOUNT_VERSION, accounts_version));

	OpStringC8 log_heading("Account version issue");

	if (accounts_version > CURRENT_M2_ACCOUNT_VERSION)
	{
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Can't recover newer files than currently supported, you need to upgrade"));
		return OpStatus::ERR_OUT_OF_RANGE; // refuse to recover
	}

	if (accounts_version < CURRENT_M2_ACCOUNT_VERSION)
	{
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Can't recover old files, you need to let m2 upgrade your files first"));
		return OpStatus::ERR_OUT_OF_RANGE; // refuse to recover
	}

	// Read sections, each section is an account, except the first one
	OpString_list section_list;
	RETURN_IF_LEAVE(account_file.ReadAllSectionsL(section_list));
 
	// in the first section read the version number, if m2 needs an upgrade then we can't do a recovery yet ( but on next startup )
	for (unsigned long i = 1; i < section_list.Count(); i++)
	{
		OpString8 account_str, protocol;
		RETURN_IF_ERROR(account_str.Set(section_list.Item(i).CStr()));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(&account_file, account_str, "Incoming Protocol", protocol));

		int account_number;
		if (sscanf(account_str.CStr(), "Account%d", &account_number) == 1)
		{
			AccountTypes::AccountType type = Account::GetProtocolFromName(protocol);
			switch (type)
			{
				case AccountTypes::POP:
					{
						if (OpStatus::IsError(CheckAndFixPOPDatabase(account_number)))
						{
							// implement error handling
						}
						break;
					}
				case AccountTypes::IMAP:
					{
						if (OpStatus::IsError(CheckAndFixIMAPDatabase(account_number)))
						{
							// implement error handling
						}
						break;
					}
				case AccountTypes::RSS:
					{
						if (OpStatus::IsError(CheckAndFixFeedDatabases()))
						{
							// implement error handling
						}
						break;
					}
				default:
					break;
			}
		} 
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckAndFixFeedDatabases
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckAndFixFeedDatabases()
{
	OpString feed_filename, foldername;
	OpString8 feed_filename8, log_text;

	OpStringC8 log_heading("Newsfeed account");

	RETURN_IF_ERROR(MailFiles::GetNewsfeedfolder(foldername));
	OpFolderLister* dummy_folder_lister;
	RETURN_IF_ERROR(OpFolderLister::Create(&dummy_folder_lister));
	
	OpAutoPtr<OpFolderLister> folder_lister(dummy_folder_lister);
	if (!folder_lister.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(folder_lister->Construct(foldername.CStr(), UNI_L("feed_*")));
	
	// iterate through all feed_* files in the newsfeed folder
	while (folder_lister->Next())
	{
		log_text.Empty();
		RETURN_IF_ERROR(feed_filename.Set(folder_lister->GetFullPath()));
		RETURN_IF_ERROR(feed_filename8.Set(folder_lister->GetFileName()));

		SingleBTree<RSSProtocol::NewsfeedID> btree;
		RETURN_IF_ERROR(btree.Open(feed_filename.CStr(), BlockStorage::OpenRead, M2_BS_BLOCKSIZE));

		if (btree.CheckConsistency(2) == OpBoolean::IS_FALSE)
		{
			RETURN_IF_ERROR(log_text.AppendFormat("Database not consistent, starting recovery of file %s", feed_filename8.CStr()));
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log, log_heading, log_text));

			RETURN_IF_ERROR(btree.Close());
			
			log_text.Empty();
#ifdef SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
			if (OpStatus::IsSuccess(btree.Recover(feed_filename.CStr(), M2_BS_BLOCKSIZE)))
			{
				RETURN_IF_ERROR(log_text.AppendFormat("Database recovery succeeded of file %s", feed_filename8.CStr()));
				OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log, log_heading, log_text));
			}
			else
#endif // SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
			{
				RETURN_IF_ERROR(log_text.AppendFormat("Database recovery failed of file %s", feed_filename8.CStr()));
				OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log, log_heading, log_text));
			}
		}
		else
		{
			RETURN_IF_ERROR(log_text.AppendFormat("Database %s consistent",feed_filename8.CStr()));
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log, log_heading, log_text));
			RETURN_IF_ERROR(btree.Close());
		}
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckAndFixPOPDatabase
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckAndFixPOPDatabase(int account_id)
{

	OpString filename;
	OpString8 log_heading;
	RETURN_IF_ERROR(log_heading.AppendFormat("POP account %d",account_id));
	RETURN_IF_ERROR(MailFiles::GetPOPUIDLFilename(account_id,filename));

	SingleBTree<UidlManager::UidlKey> btree;
	RETURN_IF_ERROR(btree.Open(filename.CStr(), BlockStorage::OpenRead, M2_BS_BLOCKSIZE));

	if (btree.CheckConsistency(2) == OpBoolean::IS_FALSE)
	{

		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database not consistent, starting recovery."));

		RETURN_IF_ERROR(btree.Close());
		
#ifdef SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
		if (OpStatus::IsSuccess(btree.Recover(filename.CStr(), M2_BS_BLOCKSIZE)))
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery succeeded"));
		}
		else
#endif // SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery failed"));
		}
	}
	else
	{
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database consistent"));
		RETURN_IF_ERROR(btree.Close());
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckAndFixIMAPDatabase
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckAndFixIMAPDatabase(int account_id)
{

	OpString filename;
	OpString8 log_heading, log_text;
	RETURN_IF_ERROR(log_heading.AppendFormat("IMAP account %d",account_id));
	RETURN_IF_ERROR(MailFiles::GetIMAPUIDFilename(account_id,filename));

	SingleBTree<ImapUidManager::ImapUidKey> btree;
	RETURN_IF_ERROR(btree.Open(filename.CStr(), BlockStorage::OpenRead, M2_BS_BLOCKSIZE));

	if (btree.CheckConsistency(2) == OpBoolean::IS_FALSE)
	{
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database not consistent, starting recovery."));

		RETURN_IF_ERROR(btree.Close());

#ifdef SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
		if (OpStatus::IsSuccess(btree.Recover(filename.CStr(), M2_BS_BLOCKSIZE)))
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery succeeded"));
		}
		else
#endif // SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery failed"));
		}
	}
	else
	{
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database consistent"));
		RETURN_IF_ERROR(btree.Close());
	}
	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckAndFixLexicon
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckAndFixLexicon()
{

	OpString path;
	RETURN_IF_ERROR(MailFiles::GetLexiconPath(path));
	StringTable lexicon;
	// Open the string table
	if (lexicon.Open(path.CStr(), LEXICON_FILENAME) == OpBoolean::IS_TRUE)
	{
		OpStringC8 log_heading("Lexicon");
				OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Checking consistency"));

		if (lexicon.CheckConsistency(FALSE) == OpBoolean::IS_FALSE)
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database not consistent, deleting file and reindexing."));

			// close Lexicon stringtable and delete file
			RETURN_IF_ERROR(lexicon.Close(1));
			OpFile lexicon_file;
			RETURN_IF_ERROR(lexicon_file.Construct(path.CStr()));
			RETURN_IF_ERROR(lexicon_file.Delete(TRUE));
		}
		else
		{
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database consistent"));
			RETURN_IF_ERROR(lexicon.Close());
		}

	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 **
 ** MailRecovery::CheckAndFixIndexerStringTable
 ***********************************************************************************/
OP_STATUS MailRecovery::CheckAndFixIndexerStringTable() 
{ 
	OpString path;
	RETURN_IF_ERROR(MailFiles::GetIndexerPath(path));
	StringTable indexer_table;
	// Open the string table
	if (indexer_table.Open(path.CStr(), INDEXER_FILENAME) == OpBoolean::IS_TRUE)
	{		
		OpStringC8 log_heading("Indexer");
		OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Checking consistency"));

		if (indexer_table.CheckConsistency() == OpBoolean::IS_FALSE)
		{
			// The StringTable is not consistent, log and try to recover
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database not consistent"));

			OpStatus::Ignore(indexer_table.Close());
			
#ifdef SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
			if (OpStatus::IsSuccess(indexer_table.Recover(path.CStr(),INDEXER_FILENAME)))
			{
				OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery succeeded"));
			}
			else
#endif // SEARCH_ENGINE_CAP_CONTAINS_RECOVERY
			{
				OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database recovery failed"));
			}
			
		}
		else
		{
			// The StringTable is consistent, close the table
			OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Database consistent"));
			RETURN_IF_ERROR(indexer_table.Close());
		}

	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 ** MailRecovery::GhostBuster
 ***********************************************************************************/
OP_STATUS MailRecovery::GhostBuster() 
{ 
	Store* store = g_m2_engine->GetStore();
	// we can only run this if we have loaded all messages!
	if (!store->HasFinishedLoading())
		return OpStatus::OK;

	OpString8 indexes_with_ghosts;
	OpStringC8 log_heading("Ghostbuster");
	OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,"Started"));
	INT32 num_ghosts = 0;
	Index* current;
	// go through all indexes 
	for (INT32 it = -1; (current = g_m2_engine->GetIndexer()->GetRange(0, IndexTypes::LAST, it)) != NULL; )
	{
		OpINT32Vector message_ids;

		if (OpStatus::IsError(current->PreFetch()) || OpStatus::IsError(current->GetAllMessages(message_ids)))
			continue;

		// go through all messages in all indexes
		for (UINT32 nb = 0; nb < message_ids.GetCount(); nb++)
		{
			INT32 message_id = message_ids.Get(nb); 
			// check if the message really exists in store
			if (!store->MessageAvailable(message_id))
			{
				// we have a ghost, a message that doesn't exist in store, but exists in an index
				current->RemoveMessage(message_id);
				num_ghosts++;
				indexes_with_ghosts.AppendFormat(" %d",current->GetId());
			}
		}
	}
	OpStatus::Ignore(store->CommitData());
	OpString8 log_text;
	OpStatus::Ignore(log_text.AppendFormat("Finished: Deleted %d ghosts from the following indexes:%s", num_ghosts, indexes_with_ghosts.CStr()));
	OpStatus::Ignore(DesktopFileLogger::Log(m_recovery_log,log_heading,log_text));
	return OpStatus::OK;
}

#endif //M2_SUPPORT
