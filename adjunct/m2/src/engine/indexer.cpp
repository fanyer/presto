/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include <ctype.h>
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/index/indeximage.h"
#include "adjunct/m2/src/engine/index/indexgroup.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/MessageDatabase/MessageDatabase.h"
#include "adjunct/m2/src/util/str/m2tokenizer.h"
#include "adjunct/m2/src/util/misc.h"

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"
#include "adjunct/desktop_util/prefs/PrefsCollectionM2.h"
#include "adjunct/desktop_util/prefs/PrefsUtils.h"
#include "adjunct/desktop_util/version/operaversion.h"

#include "adjunct/quick/Application.h"
#include "adjunct/quick_toolkit/widgets/Dialog.h"
#include "adjunct/quick/dialogs/SimpleDialog.h"

#include "modules/locale/oplanguagemanager.h"

#include "modules/util/opfile/opfile.h"
#include "modules/util/str.h"
#include "modules/util/adt/opvector.h"

#ifdef M2_MERLIN_COMPATIBILITY
# include "adjunct/m2/src/engine/lexicon.h"
#endif // M2_MERLIN_COMPATIBILITY


#define INDEX_FLUSH_DELAY 15000
#define TEXT_FLUSH_DELAY  15000
#define TEXT_FLUSH_FOLLOWUP_DELAY 4000
#define TEXT_FLUSH_RETRY_DELAY 100

#define INDEXER_VERSION 13
#define NEEDED_AUTOFILTER_MSG_COUNT 30

//#define DEBUG_INDEXER_AUTOFILTER

// ---------------------------------------------------------------------------------

Indexer::Indexer(MessageDatabase* message_database)
  : m_prefs(NULL)
  , m_loop(NULL)
  , m_unread_group(NULL)
  , m_hidden_group(NULL)
  , m_index_count(0)
  , m_next_thread_id(IndexTypes::FIRST_THREAD)
  , m_next_contact_id(IndexTypes::FIRST_CONTACT)
  , m_next_search_id(IndexTypes::FIRST_SEARCH)
  , m_next_folder_id(IndexTypes::FIRST_FOLDER)
  , m_next_newsgroup_id(IndexTypes::FIRST_NEWSGROUP)
  , m_next_imap_id(IndexTypes::FIRST_IMAP)
  , m_next_newsfeed_id(IndexTypes::FIRST_NEWSFEED)
  , m_next_archive_id(IndexTypes::FIRST_ARCHIVE)
  , m_next_folder_group_id(IndexTypes::FIRST_UNIONGROUP)
  , m_verint(0)
  , m_removing_message(0)
  , m_rss_account_index_id(0)
  , m_no_autofilter_update(false)
  , m_save_requested(FALSE)
  , m_text_lexicon_flush_requested(FALSE)
  , m_index_lexicon_flush_requested(FALSE)
  , m_currently_indexing(0)
  , m_text_table(NULL)
  , m_message_database(message_database)
  , m_debug_text_lexicon_enabled(TRUE)
  , m_debug_contact_list_enabled(TRUE)
  , m_debug_filters_enabled(TRUE)
  , m_debug_spam_filter_enabled(TRUE)
{
	m_message_database->GroupSearchEngineFile(m_index_table.GetGroupMember());
}


Indexer::~Indexer()
{
	if (m_prefs)
	{
		if (m_save_requested)
		{
			OP_PROFILE_METHOD("Prefs saved to index.ini");
			// save search indexes when needed
			SaveAllToFile();
		}
		else
		{
			TRAPD(err, m_prefs->CommitL());
		}
	}

	m_categories.DeleteAll();
	OP_DELETE(m_hidden_group);
	m_hidden_group = NULL;
	OP_DELETE(m_unread_group);
	m_pop_indexes.DeleteAll();
	m_folder_indexgroups.DeleteAll();

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	glue_factory->DeleteMessageLoop(m_loop);
	MessageEngine::GetInstance()->RemoveAccountListener(this);
	glue_factory->DeletePrefsFile(m_prefs);

	OP_DELETE(m_text_table);
}


void Indexer::PrepareToDie()
{
	if (m_text_table) // crash was detected here
	{
		OP_DELETE(m_text_table);
		m_text_table = NULL;
	}

	OP_PROFILE_METHOD("Closing indexer table");
	OpStatus::Ignore(m_index_table.Close());

	// lexicon will now flush itself anyway:
	m_index_lexicon_flush_requested = FALSE;
	m_text_lexicon_flush_requested = FALSE;
}

OP_STATUS Indexer::Init(OpStringC &indexer_filename, OpString8& status)
{
	// Indexer listens to store
	if (!GetStore())
		return OpStatus::ERR_NULL_POINTER;

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();

	OpString text_lexicon_path, index_lexicon_path;

	RETURN_IF_ERROR(MailFiles::GetLexiconPath(text_lexicon_path));

	RETURN_IF_ERROR(MailFiles::GetIndexerPath(index_lexicon_path));

	BOOL index_lexicon_existed;
	OP_BOOLEAN err;
	{
		OP_PROFILE_METHOD("Init Lexicon");
		if (!(m_text_table = OP_NEW(SELexicon, ())))
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(m_text_table->Init(text_lexicon_path.CStr(),LEXICON_FILENAME));
	}
	{
		OP_PROFILE_METHOD("Init Indexer Base");
		err = m_index_table.Open(index_lexicon_path.CStr(), INDEXER_FILENAME);
		if (OpStatus::IsError(err))
		{
			OpString failure_message, error_message;
			if (err == OpStatus::ERR_NO_ACCESS)
				OpStatus::Ignore(g_languageManager->GetString(Str::S_M2_FILE_NO_ACCESS, failure_message));
			else
				OpStatus::Ignore(g_languageManager->GetString(Str::S_M2_CORRUPTED_FILE, failure_message));
			OpStatus::Ignore(error_message.AppendFormat(failure_message.CStr(), index_lexicon_path.CStr()));
			OpStatus::Ignore(status.SetUTF8FromUTF16(error_message.CStr()));
			return err;
		}
		index_lexicon_existed = err == OpBoolean::IS_TRUE;
	}
#ifdef M2_MERLIN_COMPATIBILITY
	// Update old-style indexer
	if (Lexicon::Exists(index_lexicon_path.CStr(), INDEXER_FILENAME))
	{
		if (index_lexicon_existed)
		{
			RETURN_IF_ERROR(m_index_table.Clear());
		}

		Lexicon *lex = OP_NEW(Lexicon, ());
		if (!lex)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsSuccess(lex->Init(index_lexicon_path.CStr(), INDEXER_FILENAME)))
		{
			if (OpStatus::IsError(err = lex->ConvertTo(m_index_table)))
			{
				OP_DELETE(lex);
				return err;
			}
			Lexicon::Erase(lex);
			index_lexicon_existed = TRUE;
		}
	}

	// Erase old-style lexicon (part of update)
	if (Lexicon::Exists(text_lexicon_path.CStr(), LEXICON_FILENAME))
	{
		Lexicon* text_lex = OP_NEW(Lexicon, ());
		if (!text_lex)
			return OpStatus::ERR_NO_MEMORY;

		if (OpStatus::IsSuccess(text_lex->Init(text_lexicon_path.CStr(), LEXICON_FILENAME, FALSE)))
			Lexicon::Erase(text_lex);
		else
			OP_DELETE(text_lex);
	}
#endif // M2_MERLIN_COMPATIBILITY

	/*
	for (UINT32 pos = 0; pos < IndexTypes::LAST_IMPORTANT; pos++)
	{
		Index* temp_index = OP_NEW(Index,());
		if (!temp_index)
			return OpStatus::ERR_NO_MEMORY;
		temp_index->SetId(pos);
		RETURN_IF_ERROR(m_sorted_indexes.Insert(temp_index));
	}
	*/

	// read in index.ini and set up list of indexes..

	OpFile* index_ini = glue_factory->CreateOpFile(indexer_filename);
	BOOL index_ini_existed;
	index_ini->Exists(index_ini_existed);

	m_prefs = glue_factory->CreatePrefsFile(indexer_filename);
    if (!m_prefs)
        return OpStatus::ERR_NO_MEMORY;

	glue_factory->DeleteOpFile(index_ini);

	int m_verint = 0;
	if (index_ini_existed)
	{	
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Indexer Version", 0, m_verint));
		if(m_verint == 0)
		{
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", 1));
		}
		if (m_verint > INDEXER_VERSION)
		{
			status.Append("Not possible to run old Opera version with new Opera mail files.\r\n");
			return OpStatus::ERR;
		}
	}
	{
		OP_PROFILE_METHOD("Read index.ini");
		OP_STATUS ret;
		if ((ret=DeserializeData()) != OpStatus::OK)
		{
			status.Append("Reading index.ini failed\n");
			return ret;
		}
	}

	


	if (m_verint <= 5)
	{
		// from version 5 to version 6 we moved from using an index in the indexer table for storing all received messages
		// to load everything directly from store (to be able to show sent messages in the received view)

		// if we upgrade from indexer version 5, we need to enable sent view for most views
		Index* index = NULL;

		for (INT32 it = -1; (index = GetRange(0, IndexTypes::LAST_INDEX, it)) != NULL; )
		{
			if (index->GetId() == IndexTypes::UNREAD_UI || index->GetId() == IndexTypes::UNREAD || index->GetId() == IndexTypes::RECEIVED)
			{
				// we need to show sent if they use threaded view
				if (index->GetModelType() == IndexTypes::MODEL_TYPE_THREADED)
				{
					index->EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);
				}
				if (index->GetId() == IndexTypes::RECEIVED)
					index->SetSaveToDisk(FALSE);
			}
			else
			{
				// we need to show sent or else a lot of mail will be hidden
				index->EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);
			}
		}

		// Because of a bug where messages were added to store but not to the received index
		// we need to delete all of those when upgrading from indexer version 5
		// so that we don't get a lot of weird messages in the unread and received view
		// More information here: DSK-252235
		//
		// Basically the upgrade deletes everything in the store that is not in the m2_received.idx or the sent index
		//

		// Received and outgoing access points have been populated, received with all store items
		Index *received_loaded_from_store = GetIndexById(IndexTypes::RECEIVED);
		Index *sent = GetIndexById(IndexTypes::SENT);
		Index *outbox = GetIndexById(IndexTypes::OUTBOX);
		Index *drafts = GetIndexById(IndexTypes::DRAFTS);

		OpINT32Set m2_received_idx_messages;

		// we need the sent index to be complete, if not it's really dangerous to remove items...
		if (OpStatus::IsSuccess(sent->PreFetch()) && 
			OpStatus::IsSuccess(outbox->PreFetch()) &&
			OpStatus::IsSuccess(drafts->PreFetch()) &&
			OpStatus::IsSuccess(FindIndexWord(UNI_L("m2_received.idx"),m2_received_idx_messages)))
		{
			// loop through all the messages that were added from store
			for (INT32SetIterator it(received_loaded_from_store->GetIterator()); it; it++)
			{
				// if they are _not_ in the old received index and _not_ in the sent index
				// then they shouldn't be in store or any index, schedule them for deletion
				message_gid_t m2_id = it.GetData();

				if (!m2_received_idx_messages.Contains(m2_id) &&
					!sent->Contains(m2_id) &&
					!outbox->Contains(m2_id) &&
					!drafts->Contains(m2_id))
				{
					if (OpStatus::IsError(RemoveMessage(m2_id)))
					{
						OpStatus::Ignore(GetStore()->MarkAsRemoved(m2_id));
					}
				}

			}
		}
		// we also need to delete all entries in m2_received.idx, since it's not used anymore
		// but that functions doesn't exist yet in search_engine
		// RETURN_IF_ERROR(m_index_table.DeleteBlock(UNI_L("m2_received.idx"),-1));
		
		// Up to the developer of the next upgrade to find out if he wants to change 6 to INDEXER_VERSION or not
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", 6));
		RETURN_IF_LEAVE(m_prefs->CommitL());
	}

	if (m_verint < 7)
	{
		RETURN_IF_ERROR(ResetToDefaultIndexes());
	
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", 7));

		RETURN_IF_LEAVE(m_prefs->CommitL());
	}
	
	// force viewing of Trash in the Trash folder
	Index* trash = GetIndexById(IndexTypes::TRASH);
	INT32 flags = trash->GetModelFlags();
	flags = flags|(1<<IndexTypes::MODEL_FLAG_TRASH);
	flags = flags|(1<<IndexTypes::MODEL_FLAG_HIDDEN);
	trash->SetModelFlags(flags);

	// force viewing of outgoing in the Outbox, Sent, Drafts and Trash
	for (int i = IndexTypes::OUTBOX; i <= IndexTypes::TRASH; i++)
	{
		Index *force_show_sent_index = GetIndexById(i);
		if (force_show_sent_index)
		{
			flags = force_show_sent_index->GetModelFlags();
			flags = flags|(1<<IndexTypes::MODEL_FLAG_SENT);
			force_show_sent_index->SetModelFlags(flags);
		}
	}

	// force only viewing of unread in Unread folder
	Index* unread = GetIndexById(IndexTypes::UNREAD);
	flags = unread->GetModelFlags();
	flags = flags&~(1<<IndexTypes::MODEL_FLAG_READ);
	unread->SetModelFlags(flags);

	// check open/closed top level folders
	
	int num_categories;
	PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Panel Category Count", 0, num_categories);

	if (num_categories > 0)
	{
		for (int i = 0; i < num_categories; i++)
		{
			OpString8 current_category;
			index_gid_t id;
			RETURN_IF_ERROR(current_category.AppendFormat("Category %i", i));
			
			RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_category, "id", FALSE, id));
			
			if (GetCategory(id))
				continue;

			MailCategory * category = OP_NEW(MailCategory, (id));

			if (!category || OpStatus::IsError(m_categories.Add(category)))
			{
				OP_DELETE(category);
				return OpStatus::ERR_NO_MEMORY;
			}

			RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_category, "open", TRUE, category->m_open));
			RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_category, "unread", FALSE, category->m_unread_messages));
			RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_category, "visible", TRUE, category->m_visible));
			
		}

	}
	else
	{
		RETURN_IF_ERROR(AddDefaultCategories());
	}

	if (m_verint < 8)
	{
		// we need to run the upgrade code after reading categories since we are removing some of them
		RETURN_IF_ERROR(SetDefaultParentIds()); // Updates filters and searches to be inside the label category

		Index* index = NULL;

		// Feed folders should have the account id so they get the published string in the indexmodel
		for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_UNIONGROUP, IndexTypes::LAST_UNIONGROUP, it)) != NULL; )
		{
			Index* parent_index = GetIndexById(index->GetParentId());
			if (parent_index && parent_index->GetAccountId())
				index->SetAccountId(parent_index->GetAccountId());
		}
		

		RETURN_IF_ERROR(RemoveCategory(IndexTypes::CATEGORY_ATTACHMENTS + 1)); // Remove the old searches category
		index = GetIndexById(IndexTypes::CATEGORY_ATTACHMENTS + 1);
		if (index)
			RETURN_IF_ERROR(RemoveIndex(index));
		RETURN_IF_ERROR(RemoveCategory(IndexTypes::CATEGORY_ATTACHMENTS + 2)); // Remove the old filter category
		index = GetIndexById(IndexTypes::CATEGORY_ATTACHMENTS + 2);
		if (index)
			RETURN_IF_ERROR(RemoveIndex(index));

		RETURN_IF_ERROR(AddDefaultLabels());
	
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", 8));

		RETURN_IF_LEAVE(m_prefs->CommitL());
	}

	if (m_verint == 8)
	{
		// during version 8 we had broken POP inbox and sent views. Regenerating them fixes it
		m_pop_indexes.DeleteAll();

		Index* pop_index;
		for (INT32 it = -1; (pop_index = GetRange(IndexTypes::FIRST_POP, IndexTypes::LAST_POP, it)) != NULL; )
		{
			RemoveIndex(pop_index, FALSE);
		}

		AccountManager* accountmgr = MessageEngine::GetInstance()->GetAccountManager();
		for (UINT16 idx = 0; idx < accountmgr->GetAccountCount(); idx++)
		{
			Account* account = accountmgr->GetAccountByIndex(idx);
			if (account->GetIncomingProtocol() == AccountTypes::POP)
			{
				RETURN_IF_ERROR(AddPOPInboxAndSentIndexes(account->GetAccountId()));
			}
		}

		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", 9));
		RETURN_IF_LEAVE(m_prefs->CommitL());
	}

	if (m_verint < 11)
	{
		// this version upgrade is different
		// it's upgraded while reading the different sections and when written the first time
		// we just need to remember to write it during the first flush
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", 11));
	}

	for (UINT32 i = 0; i < m_categories.GetCount(); i++)
	{
		GetIndexById(m_categories.Get(i)->m_id);
	}
	GetIndexById(IndexTypes::HIDDEN);

	m_loop = glue_factory->CreateMessageLoop();
	if (m_loop == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	RETURN_IF_ERROR(m_loop->SetTarget(this));

	RETURN_IF_ERROR(MessageEngine::GetInstance()->AddAccountListener(this));

	return OpStatus::OK;
}

OP_STATUS Indexer::UpgradeFromVersion11()
{
	// Fixes consequences of DSK-340672  Mail in IMAP sent folder not marked as sent
	Index* imap_index;
	for (INT32 it = -1; (imap_index = GetRange(IndexTypes::FIRST_IMAP, IndexTypes::LAST_IMAP, it)) != NULL; )
	{
		if (imap_index->GetSpecialUseType() == AccountTypes::FOLDER_SENT)
		{
			RETURN_IF_ERROR(imap_index->PreFetch());
			for (INT32SetIterator it(imap_index->GetIterator()); it; it++)
			{
				if (!GetStore()->GetMessageFlag(it.GetData(), StoreMessage::IS_SENT))
				{
					UINT64 flags = GetStore()->GetMessageFlags(it.GetData());
					flags |= StoreMessage::IS_SENT;
					flags |= StoreMessage::IS_OUTGOING;
					RETURN_IF_ERROR(GetStore()->SetMessageFlags(it.GetData(), flags));
					RETURN_IF_ERROR(GetIndexById(IndexTypes::SENT)->NewMessage(it.GetData()));
				}
			}
		}
	}


	return PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", INDEXER_VERSION);
}

OP_STATUS Indexer::SetDefaultParentIds()
{
	// We need to set all the parent ids to be correct, the new mail panel relies on that
	Index *index;
		
	for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_FOLDER, IndexTypes::LAST_FOLDER, it)) != NULL; )
	{
		// but don't set the parent id on nested filters
		if (index->GetParentId() == 0 || index->GetParentId() < IndexTypes::FIRST_FOLDER || index->GetParentId() > IndexTypes::LAST_FOLDER)
			index->SetParentId(IndexTypes::CATEGORY_LABELS);
	}

	for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_LABEL, IndexTypes::LAST_LABEL, it)) != NULL; )
	{
		// only add labels that are actually used
		index->SetParentId(IndexTypes::CATEGORY_LABELS);
	}

	// set parent id on mailing lists and followed contacts
	for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_CONTACT, IndexTypes::LAST_CONTACT, it)) != NULL; )
	{
		IndexSearch *search = index->GetSearch();
		if (search && search->GetSearchText().Find("@") == KNotFound && search->GetSearchText().Find(".") != KNotFound)
		{
			Index* parent = GetIndexById(index->GetParentId());
			if (!parent || parent->GetType() != IndexTypes::UNIONGROUP_INDEX)
				index->SetParentId(IndexTypes::CATEGORY_MAILING_LISTS);
		}
		else
		{
			if (index->IsWatched())
			{
				index->SetParentId(IndexTypes::CATEGORY_ACTIVE_CONTACTS);
			}
		}
	}

	for (INT32 it = -1; (index = GetRange(IndexTypes::UNREAD_UI, IndexTypes::RECEIVED_LIST, it)) != NULL; )
	{
		if (index->GetId() != IndexTypes::UNREAD)
		{
			index->SetParentId(IndexTypes::CATEGORY_MY_MAIL);
		}
	}

	// followed threads
	for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_THREAD, IndexTypes::LAST_THREAD, it)) != NULL; )
	{
		if (index->IsWatched())
		{
			index->SetParentId(IndexTypes::CATEGORY_ACTIVE_THREADS);
		}
	}

	// attachments
	for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_ATTACHMENT, IndexTypes::LAST_ATTACHMENT, it)) != NULL; )
	{
		index->SetParentId(IndexTypes::CATEGORY_ATTACHMENTS);
	}
	
	return OpStatus::OK;
}

OP_STATUS Indexer::SetPanelIndexesVisible()
{
	for (UINT32 i = 0; i < m_categories.GetCount(); i++)
	{ 
		OpINT32Vector children;
		RETURN_IF_ERROR(GetChildren(m_categories.Get(i)->m_id, children));
		
		for (UINT32 child_i = 0; child_i < children.GetCount(); child_i++)
		{
			Index* index = GetIndexById(children.Get(child_i));
			if (index)
				index->SetVisible(true);
		}
	}
	return OpStatus::OK;
}

OP_STATUS Indexer::AddDefaultLabels()
{
	for (int label_id = IndexTypes::FIRST_LABEL; label_id <= IndexTypes::LABEL_VALUABLE; label_id++)
	{
		OpString languagestring;
		OpString8 keyword, image;
		
		Index* index, *old_index;
		switch ( label_id )
		{
			// Labels like Thunderbird
			case IndexTypes::LABEL_IMPORTANT:
			{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_IMPORTANT, languagestring));
					RETURN_IF_ERROR(keyword.Set("$Label1"));
					RETURN_IF_ERROR(image.Set("Label Important"));
					break;
			}
			case IndexTypes::LABEL_TODO:
			{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_TODO, languagestring));
					RETURN_IF_ERROR(keyword.Set("$Label4"));
					RETURN_IF_ERROR(image.Set("Label Todo"));
					break;
			}
			case IndexTypes::LABEL_MAIL_BACK:
			{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_MAILBACK, languagestring));
					RETURN_IF_ERROR(keyword.Set("$Label5"));
					RETURN_IF_ERROR(image.Set("Label Mail back"));
					break;
			}
			case IndexTypes::LABEL_CALL_BACK:
			{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_CALLBACK, languagestring));
					RETURN_IF_ERROR(keyword.Set("$Label6"));
					RETURN_IF_ERROR(image.Set("Label Call back"));
					break;
			}
			case IndexTypes::LABEL_MEETING:
			{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_MEETING, languagestring));
					RETURN_IF_ERROR(keyword.Set("$Label2"));
					RETURN_IF_ERROR(image.Set("Label Meeting"));
					break;
			}
			case IndexTypes::LABEL_FUNNY:
			{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_FUNNY, languagestring));
					RETURN_IF_ERROR(keyword.Set("$Label7"));
					RETURN_IF_ERROR(image.Set("Label Funny"));
					break;
			}
			case IndexTypes::LABEL_VALUABLE:
			{
					RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_VALUABLE, languagestring));
					RETURN_IF_ERROR(keyword.Set("$Label3"));
					RETURN_IF_ERROR(image.Set("Label Valuable"));
					break;
			}
			default:
				return OpStatus::ERR;
		}
	
		// First add the label indexes, then check if we need to copy over mails from the old label indexes
		RETURN_IF_ERROR(AddFolderIndex(m_next_folder_id, languagestring, TRUE));
		index = GetIndexById(m_next_folder_id);
		if (!index)
			return OpStatus::ERR;
		RETURN_IF_ERROR(index->SetKeyword(keyword));
		RETURN_IF_ERROR(index->SetSkinImage(image));

		m_next_folder_id++;
		if (index && (old_index = GetIndexById(label_id)) != NULL)
		{
			RETURN_IF_ERROR(old_index->PreFetch());
			for (INT32SetIterator it(old_index->GetIterator()); it; it++)
			{
				RETURN_IF_ERROR(index->NewMessage(it.GetData()));
			}
			// TODO copy over model flags, type, etc
			RETURN_IF_ERROR(RemoveIndex(old_index));
		}
	}

	return OpStatus::OK;
}

OP_STATUS Indexer::AddDefaultCategories()
{
	m_categories.DeleteAll();

	if (MessageEngine::GetInstance()->GetAccountManager()->GetMailNewsAccountCount()) 
    { 
        // we have a mail account, show all default categories 
		for (int id= IndexTypes::FIRST_CATEGORY; id < IndexTypes::LAST_CATEGORY; id++)
		{
			MailCategory* category = OP_NEW(MailCategory, (id));
			
			if (!category || OpStatus::IsError(m_categories.Add(category)))
			{
				OP_DELETE(category);
				return OpStatus::ERR_NO_MEMORY;
			}

			OpINT32Vector child_indexes;
			if (OpStatus::IsSuccess(GetChildren(id, child_indexes)) && child_indexes.GetCount() == 0)
				category->m_visible = FALSE;

			category->m_open = TRUE;
		}
    } 

	// Add all accounts
	AccountManager* accountmgr = MessageEngine::GetInstance()->GetAccountManager();
	for (UINT16 idx = 0; idx < accountmgr->GetAccountCount(); idx++)
	{
		MailCategory* category = OP_NEW( MailCategory, (IndexTypes::FIRST_ACCOUNT + accountmgr->GetAccountByIndex(idx)->GetAccountId()));
			
		if (!category || OpStatus::IsError(m_categories.Add(category)))
		{
			OP_DELETE(category);
			return OpStatus::ERR_NO_MEMORY;
		}
		
		category->m_open = TRUE;
    } 

	if (MessageEngine::GetInstance()->GetAccountManager()->GetMailNewsAccountCount() == 0 && MessageEngine::GetInstance()->GetAccountManager()->GetRSSAccount(FALSE))
	{
		MailCategory* category = OP_NEW(MailCategory, (IndexTypes::CATEGORY_LABELS));
			
		if (!category || OpStatus::IsError(m_categories.Add(category)))
		{
			OP_DELETE(category);
			return OpStatus::ERR_NO_MEMORY;
		}
		
		category->m_visible = FALSE;
	}

	// Save all the new categories
	SaveRequest();
	
	return OpStatus::OK;
}

OP_STATUS Indexer::GetCategories(OpINT32Vector& categories)
{
	for (UINT32 i = 0; i < m_categories.GetCount(); i++)
	{ 
		RETURN_IF_ERROR(categories.Add(m_categories.Get(i)->m_id));
	}
	return OpStatus::OK;
}

OP_STATUS Indexer::UpdateTranslatedStrings()
{
	RETURN_IF_ERROR(AddBasicFolders());

	if (m_unread_group)
	{
		OpString name;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_UNREAD, name));

		m_unread_group->GetIndex()->SetName(name.CStr());
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::AddBasicFolders()
{
	OpString languagestring;

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_UNREAD, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::UNREAD, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_RECEIVED, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::RECEIVED, languagestring, FALSE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_RECEIVED_NEWS, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::RECEIVED_NEWS, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_RECEIVED_LIST, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::RECEIVED_LIST, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_OUTBOX, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::OUTBOX, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_SENT, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::SENT, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_DRAFTS, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::DRAFTS, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_SPAM, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::SPAM, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_PIN_BOARD, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::PIN_BOARD, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_TRASH, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::TRASH, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_MUSIC, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::AUDIO_ATTACHMENT, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_IMAGES, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::IMAGE_ATTACHMENT, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_VIDEO, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::VIDEO_ATTACHMENT, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_DOCUMENTS, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::DOC_ATTACHMENT, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_ZIPFILES, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::ZIP_ATTACHMENT, languagestring, TRUE));

	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_CLIPBOARD, languagestring));
	RETURN_IF_ERROR(AddFolderIndex(IndexTypes::CLIPBOARD, languagestring, FALSE));

	
	AccountManager* accountmgr = MessageEngine::GetInstance()->GetAccountManager();
	for (UINT16 idx = 0; idx < accountmgr->GetAccountCount(); idx++)
	{
		Account* account = accountmgr->GetAccountByIndex(idx);
		// If the account index isn't present, we need to add it (eg in case of a blank index.ini or recovering from a crash)
		if (!GetIndexById(IndexTypes::FIRST_ACCOUNT + account->GetAccountId()))
			OnAccountAdded(account->GetAccountId());
		
		// Add Inbox and Sent indexes for POP Accounts if they don't exist already
		if (account->GetIncomingProtocol() == AccountTypes::POP && 
			( !GetIndexById(IndexTypes::GetPOPInboxIndexId(account->GetAccountId())) || 
			  !GetIndexById(IndexTypes::GetPOPSentIndexId(account->GetAccountId())) ))
		{
			RETURN_IF_ERROR(AddPOPInboxAndSentIndexes(account->GetAccountId()));
		}
	}

	return OpStatus::OK;
}

OP_STATUS Indexer::LoadIndexerData()
{
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Indexer Version", 0, m_verint));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Index Count", 0, m_index_count));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Thread ID", IndexTypes::FIRST_THREAD, m_next_thread_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Contact ID", IndexTypes::FIRST_CONTACT, m_next_contact_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Search ID", IndexTypes::FIRST_SEARCH + 1, m_next_search_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Folder ID", IndexTypes::FIRST_FOLDER, m_next_folder_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Newsgroup ID", IndexTypes::FIRST_NEWSGROUP, m_next_newsgroup_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next IMAP ID", IndexTypes::FIRST_IMAP, m_next_imap_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Newsfeed ID", IndexTypes::FIRST_NEWSFEED, m_next_newsfeed_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Archive ID", IndexTypes::FIRST_ARCHIVE, m_next_archive_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Next Folder Group ID", IndexTypes::FIRST_UNIONGROUP, m_next_folder_group_id));

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Debug", "Text Lexicon Enabled", TRUE, m_debug_text_lexicon_enabled));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Debug", "Contact List Enabled", TRUE, m_debug_contact_list_enabled));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Debug", "Filters Enabled", TRUE, m_debug_filters_enabled));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, "Debug", "Spam Filter Enabled", TRUE, m_debug_spam_filter_enabled));

	return OpStatus::OK;
}

OP_STATUS Indexer::LoadBasicIndexData(const OpStringC8 current_index, index_gid_t& index_id, OpString& index_name, IndexTypes::Type& index_type, bool& goto_next_index)
{
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Id", index_id, index_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs, current_index, "Name", index_name));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Type", IndexTypes::FOLDER_INDEX, index_type)); 

	if (index_name.IsEmpty() || index_id == 1 || GetIndexById(index_id) != NULL)
	{
		// don't allow duplicates, empty names or read data for Unread
		goto_next_index = true;
		return OpStatus::OK;
	}

	if (index_id >= IndexTypes::FIRST_ACCOUNT && index_id < IndexTypes::LAST_ACCOUNT)
	{
		if (MessageEngine::GetInstance()->GetAccountById((UINT16)(index_id-IndexTypes::FIRST_ACCOUNT)) == NULL)
		{
			goto_next_index = false;
			return OpStatus::OK;
		}
	}
	return OpStatus::OK;
}

OP_STATUS Indexer::CreateFolderIndexGroup(Index*& index, index_gid_t index_id)
{
	FolderIndexGroup* group = OP_NEW(FolderIndexGroup,(index_id));
	if (!group || OpStatus::IsError(m_folder_indexgroups.Add(group)))
	{
		OP_DELETE(group);
		return OpStatus::ERR_NO_MEMORY;
	}
	index = group->GetIndex();
	return OpStatus::OK;
}

OP_STATUS Indexer::CreateIndexGroup(Index*& index, const OpStringC8& current_index, index_gid_t index_id, IndexTypes::Type index_type, bool& goto_next_index )
{
	UINT32 account_id;
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Account Id", 0, account_id));
	IndexGroup* group;
	if (index_type == IndexTypes::COMPLEMENT_INDEX)
	{
		group = OP_NEW(ComplementIndexGroup,(index_id));
	}
	else if (index_type == IndexTypes::INTERSECTION_INDEX)
	{
		group = OP_NEW(IntersectionIndexGroup,(index_id));
	}
	else
	{
		OP_ASSERT(0);
		goto_next_index = true;
		return OpStatus::OK;
	}

	if (!group || OpStatus::IsError(m_pop_indexes.Add(group)))
	{
		OP_DELETE(group);
		return OpStatus::ERR_NO_MEMORY;
	}
	index = group->GetIndex();
	RETURN_IF_ERROR(group->SetBaseWithoutAddingMessages(IndexTypes::GetAccountIndexId(account_id)));
	RETURN_IF_ERROR(group->AddIndexWithoutAddingMessages(IndexTypes::SENT));
	return OpStatus::OK;
}

OP_STATUS Indexer::LoadIndexFlags(const OpStringC8& current_index, UINT32& index_flags)
{
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Flags", 0, index_flags));

	if (m_verint < 10) 
	{
		bool ascending;
		bool mark_as_read;
		bool watched;
		bool ignored;
		bool inherit_filter;
		bool visible;
		bool hide_from_other;
		bool view_expanded;
		bool search_new_messages_only;
		bool auto_filter;

		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Visible", FALSE, visible));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Sort Ascending", TRUE, ascending));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Mark Match As Read", FALSE, mark_as_read));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Watched", 0, watched));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Ignored", 0, ignored));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Inherit Filter", 0, inherit_filter));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Hide from other", FALSE, hide_from_other));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Expanded", FALSE, view_expanded));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Search New Messages Only", TRUE, search_new_messages_only));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsBool  (m_prefs, current_index, "Auto filter", FALSE, auto_filter));

		index_flags = 0;
		index_flags |= ascending << IndexTypes::INDEX_FLAGS_ASCENDING;
		index_flags |= mark_as_read << IndexTypes::INDEX_FLAGS_MARK_MATCH_AS_READ;
		index_flags |= watched << IndexTypes::INDEX_FLAGS_WATCHED;
		index_flags |= ignored << IndexTypes::INDEX_FLAGS_IGNORED;
		index_flags |= inherit_filter << IndexTypes::INDEX_FLAGS_INHERIT_FILTER_DEPRECATED;
		index_flags |= visible << IndexTypes::INDEX_FLAGS_VISIBLE;
		index_flags |= hide_from_other << IndexTypes::INDEX_FLAGS_HIDE_FROM_OTHER;
		index_flags |= view_expanded << IndexTypes::INDEX_FLAGS_VIEW_EXPANDED;
		index_flags |= search_new_messages_only << IndexTypes::INDEX_FLAGS_SERCH_NEW_MESSAGE_ONLY;
		index_flags |= auto_filter << IndexTypes::INDEX_FLAGS_AUTO_FILTER;
	}

	return OpStatus::OK;
}

OP_STATUS Indexer::LoadSearchData(const OpStringC8& current_index, Index* search_index)
{
	IndexSearch search;
	OpString search_text;
	SearchTypes::Field search_field;
	SearchTypes::Operator search_operator;
	SearchTypes::Option search_option;
	message_gid_t current_message;
	UINT32 start_date;
	UINT32 end_date;

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs, current_index, "Search Text", search_text));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search Field", SearchTypes::CACHED_SUBJECT, search_field));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search Current Message", 0, current_message));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search Start Date", 0, start_date));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search End Date", 0, end_date));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search Option", FALSE, search_option));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search Operator", SearchTypes::OR, search_operator));

	search.SetSearchText(search_text);
	search.SetSearchBody(search_field);
	search.SetCurrentId(current_message);
	search.SetStartDate(start_date);
	search.SetEndDate(end_date);
	search.SetOption(search_option);
	search.SetOperator(search_operator);

	if (search_text.HasContent())
	{
		RETURN_IF_ERROR(search_index->AddSearch(search));
	}

	OpString8 prefs;
	UINT32 search_count;
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_index, "Search count", 1, search_count));
	for (UINT32 pos = 2; pos <= search_count; pos++)
	{
		IndexSearch additional_search;

		RETURN_IF_ERROR(prefs.AppendFormat("Search Text %d", pos));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs, current_index, prefs, search_text));
		prefs[0] = '\0';

		RETURN_IF_ERROR(prefs.AppendFormat("Search Field %d", pos));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_index, prefs, 0, search_field));
		prefs[0] = '\0';

		RETURN_IF_ERROR(prefs.AppendFormat("Search Option %d", pos));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_index, prefs, 0, search_option));
		prefs[0] = '\0';

		RETURN_IF_ERROR(prefs.AppendFormat("Search Operator %d", pos));
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(m_prefs, current_index, prefs, 0, search_operator));
		prefs[0] = '\0';

		search.SetSearchText(search_text);
		search.SetSearchBody(search_field);
		search.SetOption((SearchTypes::Option)search_option);
		search.SetOperator(search_operator);

		RETURN_IF_ERROR(search_index->AddSearch(search));
	}
	return OpStatus::OK;
}

OP_STATUS Indexer::LoadIndexData(const OpStringC8& current_index, Index* index, INT32 index_flags, INT32 default_model_flags)
{
	OpString keyword;
	OpString8 image, skin_image;
	index_gid_t parent_id, mirror_id;
	UINT32 account_id;
	message_gid_t selected_message;
	AccountTypes::FolderPathType special_use_type;
	IndexTypes::ModelType model_type;
	IndexTypes::ModelAge model_age;
	IndexTypes::ModelSort sort;
	IndexTypes::GroupingMethods grouping;
	INT32 model_flags;
	time_t update_frequency;
	time_t last_update_time;

	
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs, current_index, "Keyword", keyword));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Parent Id", 0, parent_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Mirror Id", 0, mirror_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Special Use Type", AccountTypes::FOLDER_NORMAL, special_use_type));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Account Id", 0, account_id));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Model Type", IndexTypes::MODEL_TYPE_FLAT, model_type));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Model Age", IndexTypes::MODEL_AGE_FOREVER, model_age));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Model Flags", default_model_flags, model_flags)); 
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Sort Type", IndexTypes::MODEL_SORT_BY_NONE, sort));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Model Group Type", IndexTypes::GROUP_BY_DATE, grouping));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Selected Message", 0, selected_message));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Update Frequency", 3600, update_frequency));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Last Update Time", 0, last_update_time));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs, current_index, "Image", image));
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs, current_index, "Skin Image", skin_image));	

	if (m_verint < 11)
	{
		if (index->GetId() < IndexTypes::LAST_IMPORTANT)
		{
			model_flags |= (1<<IndexTypes::MODEL_FLAG_DUPLICATES);
		}
	}
	
	if (m_verint < 13)
	{
		model_age = IndexTypes::MODEL_AGE_FOREVER;
	}

	index->SetAllIndexFlags(index_flags);
	index->SetVisible(index->GetIndexFlag(IndexTypes::INDEX_FLAGS_VISIBLE));
	index->SetMirrorId(mirror_id);
	index->SetAccountId(account_id);
	index->SetModelType(model_type,FALSE);
	index->SetModelAge(model_age);
	index->SetModelFlags(model_flags);
	index->SetModelSort(sort);
	index->SetModelGroup(grouping);
	index->SetModelSelectedMessage(selected_message);
	if (index->GetIndexFlag(IndexTypes::INDEX_FLAGS_HIDE_FROM_OTHER))
	{
		index->SetHideFromOther(true);
	}
	index->SetWatched(index->GetIndexFlag(IndexTypes::INDEX_FLAGS_WATCHED));
	index->SetParentId(parent_id);
	index->SetUpdateFrequency(update_frequency);
	index->SetLastUpdateTime(last_update_time);
	index->SetSpecialUseType(special_use_type);
		
	if (skin_image.HasContent())
		RETURN_IF_ERROR(index->SetSkinImage(skin_image));
	if (image.HasContent())
		RETURN_IF_ERROR(index->SetCustomBase64Image(image));
	
	OpString8 keyword8;
	RETURN_IF_ERROR(keyword8.Set(keyword.CStr()));
	RETURN_IF_ERROR(SetKeyword(index, keyword8));

	if (index->GetIndexFlag(IndexTypes::INDEX_FLAGS_AUTO_FILTER))
	{
		OpStatus::Ignore(index->SetAutoFilterFile()); // allow to fail, not that critical
	}
	return OpStatus::OK;
}

OP_STATUS Indexer::CreateIntersectionIndexGroup(Index*& index, Index*& search_index, index_gid_t index_id, index_gid_t search_id, index_gid_t search_only_in)
{
	IntersectionIndexGroup* group = OP_NEW(IntersectionIndexGroup,(index_id));

	if (!group || OpStatus::IsError(m_folder_indexgroups.Add(group)))
	{
		OP_DELETE(group);
		return OpStatus::ERR_NO_MEMORY;
	}

	index = group->GetIndex();

	index->SetSearchIndexId(search_id);
	// set up the search index
	search_index = OP_NEW(Index, ());        
	if (!search_index)
		return OpStatus::ERR_NO_MEMORY;
	search_index->SetType(IndexTypes::SEARCH_INDEX);
	search_index->SetId(search_id);
	RETURN_IF_ERROR(NewIndex(search_index,FALSE));

	// and add them to the intersection group
	group->SetBaseWithoutAddingMessages(search_id);
	group->AddIndexWithoutAddingMessages(search_only_in); // TODO handle the case where this isn't read from disk yet
	return OpStatus::OK;
}

OP_STATUS Indexer::PrefetchImportantIndexes()
{
	Index* index;
	OP_PROFILE_METHOD("Prefetch important mail views");
	// populate trash with all deleted messages
	Index* trash = GetIndexById(IndexTypes::TRASH);
	RETURN_IF_ERROR(trash->PreFetch());

	// populate spam so UNREAD_UI can hide them
	index = GetIndexById(IndexTypes::SPAM);
	RETURN_IF_ERROR(index->PreFetch());

	// initialize unread and received indexes
	index = GetIndexById(IndexTypes::UNREAD);
	index->SetPrefetched();
	index = GetIndexById(IndexTypes::RECEIVED);
	index->SetPrefetched();

	// need to prefetch some indexes right away, since they hide messages from other views and cause incorrect unread counts 
	
	// initialize newsgroups folder
	index = GetIndexById(IndexTypes::RECEIVED_NEWS);
	RETURN_IF_ERROR(index->PreFetch());

	index = GetIndexById(GetRSSAccountIndexId());
	if (index)
		RETURN_IF_ERROR(index->PreFetch());

	// initialize mailing list folder
	index = GetIndexById(IndexTypes::RECEIVED_LIST);
	RETURN_IF_ERROR(index->PreFetch());
			
	for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_FOLDER, IndexTypes::LAST_FOLDER, it)) != NULL; )
	{
		// we don't need to prefetch these right away because the hidden group prefetches the indexes it needs
		OpStatus::Ignore(index->DelayedPreFetch());
	}

	return OpStatus::OK;
}

OP_STATUS Indexer::DeserializeData()
{
	RETURN_IF_ERROR(LoadIndexerData());

	Index *index;
	Message message;

	Index default_index;
	INT32 default_model_flags = default_index.GetModelFlags();
	bool unread_ui_visible = false;

	int i;
	for (i = 1; i <= m_index_count; i++)
	{
		OpString8 current_index;
		RETURN_IF_ERROR(current_index.AppendFormat("Index %i", i));
		
		index_gid_t index_id = i, search_id = 0, search_only_in = 0;
		OpString index_name;
		IndexTypes::Type index_type;
		bool goto_next_index = false;
		Index* search_index;		
		UINT32 index_flags;
		
		RETURN_IF_ERROR(LoadBasicIndexData(current_index, index_id, index_name, index_type, goto_next_index));
		RETURN_IF_ERROR(LoadIndexFlags(current_index, index_flags));

		if (m_verint < 11)
		{
			if ((index_id >= IndexTypes::FIRST_FOLDER && index_id < IndexTypes::LAST_FOLDER || 
				index_id >= IndexTypes::FIRST_LABEL && index_id < IndexTypes::LAST_INDEX) &&
				index_flags & (1<<IndexTypes::INDEX_FLAGS_INHERIT_FILTER_DEPRECATED))
			{
				index_type = IndexTypes::INTERSECTION_INDEX;
				index_gid_t parent_id;
				RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Parent Id", 0, parent_id));
				search_only_in = parent_id;
				search_id = m_next_search_id++;
			}
		}

		if (index_id == 1)
		{
			unread_ui_visible = (index_flags & (1 << IndexTypes::INDEX_FLAGS_VISIBLE)) != 0;
			continue;
		}
		else if (goto_next_index)
		{
			continue;
		}
		
		OpAutoPtr<Index> index_ap;
		if (index_type == IndexTypes::UNIONGROUP_INDEX)
		{
			RETURN_IF_ERROR(CreateFolderIndexGroup(index, index_id));
			search_index = index;
		}
		else if (index_id >= IndexTypes::FIRST_POP && index_id < IndexTypes::LAST_POP)
		{
			RETURN_IF_ERROR(CreateIndexGroup(index, current_index, index_id, index_type, goto_next_index));
			if (goto_next_index)
				continue;
			search_index = index;
		}
		else if (index_type == IndexTypes::INTERSECTION_INDEX)
		{
			if (search_only_in == 0)
			{
				RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search Only In", 0, search_only_in));
				RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt   (m_prefs, current_index, "Search Index Id", 0, search_id));
			}

			if (search_only_in == 0)
				continue;

			if (search_id == 0)
				search_id = m_next_search_id++;

			RETURN_IF_ERROR(CreateIntersectionIndexGroup(index, search_index, index_id, search_id, search_only_in));
			
		}
		else
		{
			index = OP_NEW(Index, ()); // removed when deleted from list.
			search_index = index;
			index_ap = index;
		}

        if (!index)
            return OpStatus::ERR_NO_MEMORY;

		index->SetId(index_id);
		RETURN_IF_ERROR(index->SetName(index_name.CStr()));
		index->SetType(index_type);

		RETURN_IF_ERROR(LoadIndexData(current_index, index, index_flags, default_model_flags));
		
		// Add searches/filters:
		RETURN_IF_ERROR(LoadSearchData(current_index, search_index));

		OpString file_name;
		RETURN_IF_ERROR(PrefsUtils::ReadPrefsString(m_prefs, current_index, "File", file_name));
		if (!file_name.IsEmpty())
			index->SetSaveToDisk(TRUE);

		// union, intersection, complement indexes are added when the indexgroup is created (todo this could be avoided by passing in the index pointer)
		switch (index->GetType())
		{
		case IndexTypes::UNIONGROUP_INDEX:
		case IndexTypes::INTERSECTION_INDEX:
		case IndexTypes::COMPLEMENT_INDEX:
			break;
		default:
			RETURN_IF_ERROR(NewIndex(index,FALSE));
		}

		// all indexes are populated with messages when displayed in the UI or below for important indexes
		// do it for ignored and watched indexes so that the icons are correct
		if (index->GetIndexFlag(IndexTypes::INDEX_FLAGS_IGNORED) || index->GetIndexFlag(IndexTypes::INDEX_FLAGS_WATCHED))
		{
			index->PreFetch();
		}

		index_ap.release();
	}

	// add typical folders, make sure they are translated
	{
		OP_PROFILE_METHOD("Add basic mail views");
		RETURN_IF_ERROR(UpdateTranslatedStrings());
	}
	
	RETURN_IF_ERROR(PrefetchImportantIndexes());
	
	// Set unread count for each index

	Index *unread = GetIndexById(IndexTypes::UNREAD);
	Index *trash = GetIndexById(IndexTypes::TRASH);
	Index *unread_ui = GetIndexById(IndexTypes::UNREAD_UI);

	if (!unread || !trash || !unread_ui)
	{
		return OpStatus::ERR;
	}

	unread_ui->SetVisible(unread_ui_visible);

	return OpStatus::OK;
}

OP_STATUS Indexer::UpdateNewsGroupIndexList()
{
	UINT32 g;
	UINT16 a;

	AccountManager* manager = MessageEngine::GetInstance()->GetAccountManager();

	// per NNTP account
	for (a = 0; a < manager->GetAccountCount(); a++)
	{
		Account* account = manager->GetAccountByIndex(a);
		if (account == NULL)
		{
			continue;
		}
		if (account->GetIncomingProtocol() == AccountTypes::NEWS)
		{
			continue;
		}

		// per group
		for (g = 0; g < account->GetSubscribedFolderCount(); g++)
		{
			OpString group_path;
			RETURN_IF_ERROR(account->GetSubscribedFolderName(g, group_path));
			if (GetSubscribedFolderIndex(account, group_path, 0, group_path, TRUE, FALSE) == NULL)
			{
				return OpStatus::ERR;
			}
		}
	}
	return OpStatus::OK;
}


OP_STATUS Indexer::AddFolderIndex(index_gid_t id, OpStringC folder_name, BOOL create_file)
{
	// add sent mail
	Index *index = GetIndexById(id);
	OP_STATUS ret;
	OpString name;

	RETURN_IF_ERROR(name.Set(folder_name));

	if (index == NULL)
	{
		index = OP_NEW(Index, ());
        if (!index)
            return OpStatus::ERR_NO_MEMORY;

		IndexTypes::Type type = IndexTypes::FOLDER_INDEX;

		if (id >= IndexTypes::FIRST_NEWSGROUP && id < IndexTypes::LAST_NEWSGROUP)
		{
			type = IndexTypes::NEWSGROUP_INDEX;
			IndexSearch search;
			RETURN_IF_ERROR(search.SetSearchText(name));
			RETURN_IF_ERROR(index->AddSearch(search));
		}
		if (id < IndexTypes::LAST_IMPORTANT)
		{
			INT32 flags = index->GetModelFlags();

			flags |= (1<<IndexTypes::MODEL_FLAG_DUPLICATES);

			if (id == IndexTypes::UNREAD)
			{
				// default to unread only in Unread access point
				flags &= ~(1<<IndexTypes::MODEL_FLAG_READ);
				flags &= ~(1<<IndexTypes::MODEL_FLAG_NEWSGROUPS);
				flags &= ~(1<<IndexTypes::MODEL_FLAG_NEWSFEEDS);
				flags &= ~(1<<IndexTypes::MODEL_FLAG_HIDDEN);
				flags &= ~(1<<IndexTypes::MODEL_FLAG_SPAM);
			}
			else if (id == IndexTypes::RECEIVED)
			{
				// default to hide newsgroup messages in Received
				flags &= ~(1<<IndexTypes::MODEL_FLAG_NEWSGROUPS);
				flags &= ~(1<<IndexTypes::MODEL_FLAG_NEWSFEEDS);
				flags &= ~(1<<IndexTypes::MODEL_FLAG_HIDDEN);
			}
			else if (id == IndexTypes::SENT || id == IndexTypes::OUTBOX)
			{
				// default to show sent messages
				flags |= (1<<IndexTypes::MODEL_FLAG_HIDDEN);
				flags |= (1<<IndexTypes::MODEL_FLAG_SENT);
			}
			else if (id == IndexTypes::SPAM)
			{
				flags |= (1<<IndexTypes::MODEL_FLAG_HIDDEN);
			} 
			else if (id == IndexTypes::PIN_BOARD)
			{
				flags &= ~(1<<IndexTypes::MODEL_FLAG_TRASH);
				flags &= ~(1<<IndexTypes::MODEL_FLAG_DUPLICATES);
				flags |= (1<<IndexTypes::MODEL_FLAG_SENT);
				flags |= (1<<IndexTypes::MODEL_FLAG_HIDDEN);
			}

			index->SetModelFlags(flags);
		}
		else if ((id >= IndexTypes::FIRST_LABEL && id < IndexTypes::LAST_LABEL) ||
				 (id >= IndexTypes::DOC_ATTACHMENT && id <= IndexTypes::ZIP_ATTACHMENT))
		{
			// show all labeled/attachment messages
			INT32 flags = index->GetModelFlags();
			flags = flags|(1<<IndexTypes::MODEL_FLAG_HIDDEN);
			index->SetModelFlags(flags);
		}

		index->SetId(id);

		// Set parent id
		if ((id <= IndexTypes::SPAM || id == IndexTypes::PIN_BOARD) && id != IndexTypes::UNREAD)
			index->SetParentId(IndexTypes::CATEGORY_MY_MAIL);
		else if (id >= IndexTypes::FIRST_FOLDER && id <= IndexTypes::LAST_SEARCH)
			index->SetParentId(IndexTypes::CATEGORY_LABELS);
		else if (id >= IndexTypes::FIRST_LABEL && id <= IndexTypes::LAST_LABEL)
			index->SetParentId(IndexTypes::CATEGORY_LABELS);
		else if (id >= IndexTypes::FIRST_ATTACHMENT && id <= IndexTypes::LAST_ATTACHMENT)
			index->SetParentId(IndexTypes::CATEGORY_ATTACHMENTS);

		index->SetSaveToDisk(create_file!=FALSE);
		index->SetType(type);
        if (((ret=index->SetName(name.CStr())) != OpStatus::OK) ||
             ((ret=NewIndex(index, FALSE)) != OpStatus::OK))
        {
			OP_ASSERT(FALSE);
            OP_DELETE(index);
            return ret;
        }

		if (id == IndexTypes::SPAM)
		{
			index->SetHasAutoFilter(TRUE);
		}
	}
	else if (create_file && id != IndexTypes::UNREAD && id != IndexTypes::RECEIVED && !index->GetSaveToDisk())
	{
		index->SetSaveToDisk(TRUE);
	}

	if (index)
	{
		index->SetName(name.CStr());
	}

	SaveRequest();

	return OpStatus::OK;
}


OP_STATUS Indexer::SaveToFile(Index *index, UINT32 index_number, BOOL commit)
{
	if ((int)index_number > m_index_count)
	{
		// adding a new index, not just saving
		m_index_count++;
	}

	// if (index->GetType() == IndexTypes::CONTACTS_INDEX)
	{
		commit = FALSE; // Contact indexes are never THAT important
	}

	// increment counters for index ids, update index::id
	if (index->GetId() == 0)
	{
		switch (index->GetType())
		{
		case IndexTypes::CONTACTS_INDEX:
			{
				index->SetId(m_next_contact_id);
				m_next_contact_id++;
			}
			break;
		case IndexTypes::SEARCH_INDEX:
			{
				index->SetId(m_next_search_id);
				m_next_search_id++;
			}
			break;
		case IndexTypes::FOLDER_INDEX:
			{
				index->SetId(m_next_folder_id);
				m_next_folder_id++;
			}
			break;
		case IndexTypes::NEWSGROUP_INDEX:
			{
				index->SetId(m_next_newsgroup_id);
				m_next_newsgroup_id++;
			}
			break;
		case IndexTypes::IMAP_INDEX:
			{
				index->SetId(m_next_imap_id);
				m_next_imap_id++;
			}
			break;
		case IndexTypes::NEWSFEED_INDEX:
			{
				index->SetId(m_next_newsfeed_id);
				m_next_newsfeed_id++;
			}
			break;
		case IndexTypes::ARCHIVE_INDEX:
			{
				index->SetId(m_next_archive_id);
				m_next_archive_id++;
			}
			break;
		}
	}

	// clean up first, not crucial if fails
	OpStatus::Ignore(RemoveFromFile(index_number));

	OpString index_name;
	RETURN_IF_ERROR(index->GetName(index_name));

	OpString keyword;
	if (index->GetKeyword().HasContent())
		RETURN_IF_ERROR(keyword.Set(index->GetKeyword()));
	else
		RETURN_IF_ERROR(keyword.Set(UNI_L("")));

	OpString8 current_index;
	RETURN_IF_ERROR(current_index.AppendFormat("Index %i", index_number));

	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "Name", index_name));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Id", index->GetId()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Parent Id", index->GetParentId()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Mirror Id", index->GetMirrorId()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Type", index->GetType()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Flags", index->GetAllIndexFlags())); 
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Special Use Type", index->GetSpecialUseType()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "Keyword", keyword));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Account Id", index->GetAccountId()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Model Type", index->GetModelType()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Model Age", index->GetModelAge()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Model Flags", index->GetModelFlags()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Sort Type", index->GetModelSort()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Model Group Type", index->GetModelGrouping()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Selected Message", index->GetModelSelectedMessage()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Update Frequency", index->GetUpdateFrequency()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Last Update Time", index->GetLastUpdateTime()));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "File", ""));
	if (index->GetIndexImage())
	{
		OpString image_base64;
		RETURN_IF_ERROR(index->GetIndexImage()->GetImageAsBase64(image_base64));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "Image", image_base64));
	}
	const char* image = NULL;
	Image dummy_image; 
	RETURN_IF_ERROR(index->GetImages(image, dummy_image));
	if (image)
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "Skin Image", image));

	if (index->GetSaveToDisk())
	{
		OpString unique_name;

		if (OpStatus::IsSuccess(index->GetUniqueName(unique_name)))
			RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "File", unique_name));
	}

	IndexSearch *search = NULL;
	if (index->GetType() == IndexTypes::INTERSECTION_INDEX)
	{
		if (index->GetSearchIndexId() != 0 && GetIndexById(index->GetSearchIndexId()))
		{
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Index Id", index->GetSearchIndexId()));
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Only In", GetSearchInIndex(index->GetId())));

			// changing to the search index here, and write to disk it's search preferences
			index = GetIndexById(index->GetSearchIndexId());
		}
	}

	search = index->GetSearch();

	if (search)
	{
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "Search Text", search->GetSearchText()));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Field", search->SearchBody()));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Current Message", search->GetCurrentId()));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Start Date", search->GetStartDate()));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search End Date", search->GetEndDate()));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Option", search->GetOption()));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Operator",  SearchTypes::OR));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search count", index->GetSearchCount()));

		UINT32 pos;

		OpString8 prefs;

		for (pos = 2; pos <= index->GetSearchCount(); pos++)
		{
			search = index->GetSearch(pos-1);

			RETURN_IF_ERROR(prefs.AppendFormat("Search Text %d", pos));
			RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, prefs, search->GetSearchText()));
			prefs[0] = '\0';

			RETURN_IF_ERROR(prefs.AppendFormat("Search Field %d", pos));
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, current_index, prefs, search->SearchBody()));
			prefs[0] = '\0';

			RETURN_IF_ERROR(prefs.AppendFormat("Search Option %d", pos));
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, current_index, prefs, search->GetOption()));
			prefs[0] = '\0';

			RETURN_IF_ERROR(prefs.AppendFormat("Search Operator %d", pos));
			RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, current_index, prefs, search->GetOperator()));
			prefs[0] = '\0';
		}
	}
	else
	{
		// re-initialize
		RETURN_IF_ERROR(PrefsUtils::WritePrefsString(m_prefs, current_index, "Search Text", ""));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Field",  0));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Current Message",  0));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search Start Date",  0));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search End Date",  0));
		RETURN_IF_ERROR(PrefsUtils::WritePrefsInt   (m_prefs, current_index, "Search is Regexp", 0));
	}

	if (commit)
		RETURN_IF_LEAVE(m_prefs->CommitL());

	return OpStatus::OK;
}


void Indexer::SaveAllToFile(BOOL commit)
{
	Index* index;
	UINT32 file_pos = 1;
	UINT32 count = m_index_container.GetCount();

	m_save_requested = FALSE;

	// save all indexes in memory
	// standard indexes
	for (INT32 it = -1; (index = GetRange(0, IndexTypes::LAST_INDEX, it)) != NULL; )
	{
		if (index->GetSaveToIndexIni())
		{
			SaveToFile(index, file_pos, FALSE); // pretty safe
			file_pos++;
		}
	}

	// save open/closed top level folders
	MailCategory *category;
	for (UINT32 i = 0; i < m_categories.GetCount(); i++)
	{
		category = m_categories.Get(i);
		OpStatus::Ignore(WriteCategory(category, i));

	}

	PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Panel Category Count", m_categories.GetCount());

	// remove all others
	int old_count;
	if (OpStatus::IsSuccess(PrefsUtils::ReadPrefsInt(m_prefs, "Indexer", "Index Count", count, old_count)))
	{
		for (int remove = file_pos; remove <= old_count; remove++)
		{
			RemoveFromFile(remove);
		}
	}

	// save count
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Index Count",  count);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Thread ID", m_next_thread_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Contact ID", m_next_contact_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Search ID", m_next_search_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Folder ID", m_next_folder_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Newsgroup ID", m_next_newsgroup_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next IMAP ID", m_next_imap_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Newsfeed ID", m_next_newsfeed_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Archive ID", m_next_archive_id);
	PrefsUtils::WritePrefsInt   (m_prefs, "Indexer", "Next Folder Group ID", m_next_folder_group_id);

	if (!m_debug_text_lexicon_enabled ||
		!m_debug_contact_list_enabled ||
		!m_debug_filters_enabled ||
		!m_debug_spam_filter_enabled)
	{
		PrefsUtils::WritePrefsInt   (m_prefs, "Debug", "Text Lexicon Enabled", m_debug_text_lexicon_enabled);
		PrefsUtils::WritePrefsInt   (m_prefs, "Debug", "Contact List Enabled" ,m_debug_contact_list_enabled);
		PrefsUtils::WritePrefsInt   (m_prefs, "Debug", "Filters Enabled", m_debug_filters_enabled);
		PrefsUtils::WritePrefsInt   (m_prefs, "Debug", "Spam Filter Enabled", m_debug_spam_filter_enabled);
	}

	PrefsUtils::WritePrefsInt(m_prefs, "Indexer", "Indexer Version", INDEXER_VERSION);

	if (commit)
	{
		TRAPD(err, m_prefs->CommitL(TRUE));
	}
}

OP_STATUS Indexer::WriteCategory(MailCategory* category, INT32 idx)
{
	if (idx == -1 && (idx = m_categories.Find(category)) == -1)
		return OpStatus::ERR;

	OpString8 current_category;
	RETURN_IF_ERROR(current_category.AppendFormat("Category %i", idx));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, current_category.CStr(), "id", category->m_id));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, current_category.CStr(), "open", category->m_open));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(m_prefs, current_category.CStr(), "unread", category->m_unread_messages));
	return PrefsUtils::WritePrefsInt(m_prefs, current_category.CStr(), "visible", category->m_visible);
}

OP_STATUS Indexer::RemoveFromFile(UINT32 index_number)
{
	OpString8 current_index;

	RETURN_IF_ERROR(current_index.AppendFormat("Index %i", index_number));
	RETURN_IF_LEAVE(m_prefs->DeleteSectionL(current_index.CStr()));

	return OpStatus::OK;
}


OP_STATUS Indexer::NewMessage(Message& message)
{
	UINT32 searches_in_progress = 0;
	m_currently_indexing = message.GetId();

	OP_STATUS ret = NewMessage(message, FALSE, searches_in_progress);
	m_currently_indexing = 0;

	return ret;
}


OP_STATUS Indexer::NewMessage(StoreMessage& message)
{
	// TODO Make indexer accept StoreMessage input
	return NewMessage(static_cast<Message&>(message));
}


OP_STATUS Indexer::ChangedMessage(Message& message)
{
	UINT32 searches_in_progress = 0;
	return NewMessage(message, FALSE, searches_in_progress);
}


OP_STATUS Indexer::StartSearch(const OpStringC& search_text, SearchTypes::Option option, SearchTypes::Field field, UINT32 start_date, UINT32 end_date, index_gid_t& id, index_gid_t search_only_in, Index* existing_index)
{
	Index* search_index = 0;

	// if search_only_in != 0
	// then we create a IntersectionIndexGroup where the base has the search and we add the "search only in" index to the group
	// the m_index in IntersectionIndexGroup 

	if (existing_index && search_only_in == 0)
	{
		search_index = existing_index;
	}
	else
	{
		search_index = OP_NEW(Index, ());
		if (!search_index)
			return OpStatus::ERR_NO_MEMORY;

		search_index->SetType(IndexTypes::SEARCH_INDEX);
			
		RETURN_IF_ERROR(NewIndex(search_index));
	}
	
	if (existing_index)
	{
		// we definitely don't want to keep old searches or old messages in the search index!
		existing_index->Empty();
		while (existing_index->GetSearchCount() > 0)
			RETURN_IF_ERROR(existing_index->RemoveSearch(0));
	}

	// show all messages in search indexes
	INT32 flags = UINT_MAX;
	search_index->SetModelFlags(flags);
	search_index->DisableModelFlag(IndexTypes::MODEL_FLAG_OVERRIDE_DEFAULTS_FOR_THIS_INDEX);
	
	// set the name
	OpString search_for;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_SEARCHING_FOR, search_for));
	RETURN_IF_ERROR(search_for.AppendFormat(UNI_L(" %s..."), search_text.CStr()));
	RETURN_IF_ERROR(search_index->SetName(search_for.CStr()));

	IndexSearch search;
	// Regexp searches are async
	if (!HasTextLexicon() || option == SearchTypes::REGEXP)
	{
		search.SetCurrentId(GetStore()->GetLastId());
	}
	RETURN_IF_ERROR(search.SetSearchText(search_text));
	search.SetOption(option);
	search.SetSearchBody(field);
	search.SetStartDate(start_date);
	search.SetEndDate(end_date);

	RETURN_IF_ERROR(search_index->AddSearch(search));

	if (search_only_in)
	{
		id = existing_index ? existing_index->GetId() : m_next_folder_group_id++;
		
		IntersectionIndexGroup* intersection_group = OP_NEW(IntersectionIndexGroup, (id, existing_index));

		if (!intersection_group || OpStatus::IsError(m_folder_indexgroups.Add(intersection_group)))
		{
			OP_DELETE(intersection_group);
			return OpStatus::ERR_NO_MEMORY;
		}

		// set up the index so that it's saved correctly to index.ini
		Index* intersect_index = intersection_group->GetIndex();
		intersect_index->SetType(IndexTypes::INTERSECTION_INDEX);
		intersect_index->SetSearchIndexId(search_index->GetId());

		Index* search_in = GetIndexById(search_only_in);
		intersect_index->SetModelFlags(search_in->GetModelFlags() | 1 << IndexTypes::MODEL_FLAG_HIDDEN); // copy model flags so the search is correct
		intersection_group->SetBase(search_index->GetId());
		intersection_group->AddIndex(search_only_in);
	}
	else
	{
		id = search_index->GetId();
	}

	if (HasTextLexicon() && SearchTypes::REGEXP != option)
	{
		OpINTSortedVector bintree;
		Indexer::LexiconIdSpace id_space;

		switch (field)
		{
			case SearchTypes::CACHED_SUBJECT:
				id_space = Indexer::MESSAGE_SUBJECT;
				break;
			case SearchTypes::CACHED_SENDER:
			case SearchTypes::HEADER_FROM:
				id_space = Indexer::MESSAGE_FROM;
				break;
			case SearchTypes::HEADER_TO:
				id_space = Indexer::MESSAGE_TO;
				break;
			case SearchTypes::HEADER_CC:
				id_space = Indexer::MESSAGE_CC;
				break;
			case SearchTypes::HEADER_REPLYTO:
				id_space = Indexer::MESSAGE_REPLYTO;
				break;
			case SearchTypes::HEADER_NEWSGROUPS:
				id_space = Indexer::MESSAGE_NEWSGROUPS;
				break;
			case SearchTypes::HEADERS:
				id_space = Indexer::MESSAGE_ALL_HEADERS;
				break;
			case SearchTypes::BODY:
				id_space = Indexer::MESSAGE_BODY;
				break;
			case SearchTypes::ENTIRE_MESSAGE:
			default:
				id_space = Indexer::MESSAGE_ENTIRE_MESSAGE;
				break;
		}
		// try synchronous search, if it fails, then do async
		if (OpStatus::IsSuccess(FindWords(search_text, bintree, TRUE, id_space)))
		{
			for (unsigned i = 0; i < bintree.GetCount(); i++)
			{
				if (OpStatus::IsError(search_index->NewMessage(bintree.GetByIndex(i))))
					break;
			}
			return OpStatus::OK;
		}
		else
		{
			search_index->GetSearch()->SetCurrentId(GetStore()->GetLastId());
		}
	}

	return StartSearch();
}


OP_STATUS Indexer::StartSearch()
{
	return m_loop->Post(MSG_M2_CONTINUESEARCH);
}

OP_STATUS Indexer::SaveRequest()
{
	if (!m_loop)
		return OpStatus::OK;

	if (m_save_requested)
		return OpStatus::OK;

	m_save_requested = TRUE;

	return m_loop->Post(MSG_M2_DELAYED_CLOSE, 10000);
}


/***********************************************************************************
 ** Get the ID for a keyword (will create the ID if it doesn't exist yet)
 **
 ** Indexer::GetKeywordID
 ** @param keyword Keyword to get ID for
 ** @return An ID if one could be found/created, or -1 on error
 **
 ***********************************************************************************/
int Indexer::GetKeywordID(const OpStringC8& keyword)
{
	if (keyword.IsEmpty())
		return -1;

	OpString8	unquoted_keyword;
	const char* keyword_ptr = keyword.CStr();

	// Check if keyword is quoted
	if (*keyword_ptr == '"')
	{
		// Unquote quoted string
		RETURN_VALUE_IF_ERROR(OpMisc::UnQuoteString(keyword, unquoted_keyword), -1);
		keyword_ptr = unquoted_keyword.CStr();
	}

	Keyword* keyword_data;

	// Check for keyword in hash table
	if (OpStatus::IsSuccess(m_keywords.GetData(keyword_ptr, &keyword_data)))
		return keyword_data->id;

	// No existing keyword found, create a new keyword
	keyword_data = OP_NEW(Keyword, (m_keywords_by_id.GetCount()));
	if (!keyword_data ||
		OpStatus::IsError(keyword_data->keyword.Set(keyword_ptr)) ||
		OpStatus::IsError(m_keywords_by_id.Add(keyword_data)))
	{
		OP_DELETE(keyword_data);
		return -1;
	}

	if (OpStatus::IsError(m_keywords.Add(keyword_data->keyword.CStr(), keyword_data)))
	{
		m_keywords_by_id.Delete(keyword_data->id);
		OP_DELETE(keyword_data);
		return -1;
	}

	return keyword_data->id;
}


/***********************************************************************************
 **
 **
 ** Indexer::CommitData
 ***********************************************************************************/
OP_STATUS Indexer::CommitData()
{
	m_index_lexicon_flush_requested = FALSE;

	while (!m_index_table.CacheEmpty())
	{
		RETURN_IF_ERROR(m_index_table.Commit());
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::Receive(OpMessage message)
{
	if (message == MSG_M2_CONTINUESEARCH)
	{
		if (ContinueSearch())
		{
			return m_loop->Post(MSG_M2_CONTINUESEARCH, 0);
		}
	}
	if (message == MSG_M2_DELAYED_CLOSE)
	{
		SaveAllToFile();
	}
	if (message == MSG_M2_INDEX_FLUSH)
	{
		return CommitData();
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::UpdateAutoFilter(Message* message, Index* index, BOOL remove_message)
{
	PrefsFile* file = index->GetAutoFilterFile();

	if (file == NULL)
	{
		return OpStatus::OK;
	}

	OpString body_text;

	message->QuickGetBodyText(body_text);

	OpString header_text;

	// add important headers
	Header::HeaderValue header;
	message->GetFrom(header);
	header_text.Set(header);
	message->GetHeaderValue(Header::SUBJECT, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::TO, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::CC, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::NEWSGROUPS, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::REPLYTO, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::LISTID, header);
	header_text.Append(" ");
	header_text.Append(header);
	header_text.Append(" ");

	header_text.Append(body_text);

	if (header_text.IsEmpty())
	{
		return OpStatus::OK;
	}

	MessageTokenizer tokenizer(header_text);

	OpString  key;
	OpStringC section(remove_message ? UNI_L("Exclude") : UNI_L("Include"));

	int value = 0;
	int counter = 0;

	OpAutoStringHashTable<OpString> keys;

	while (tokenizer.GetNextToken(key) == OpStatus::OK &&
			key.HasContent() && counter < 200)
	{
		if (key.Length() > 30 || keys.Contains(key.CStr()))
		{
			continue;
		}

		OpAutoPtr<OpString> keep (OP_NEW(OpString, ()));
		if (!keep.get())
			return OpStatus::ERR_NO_MEMORY;

		RETURN_IF_ERROR(keep->Set(key));
		RETURN_IF_ERROR(keys.Add(keep->CStr(), keep.get()));
		keep.release();

		RETURN_IF_LEAVE(value = file->ReadIntL(section, key, 0);
						file->WriteIntL(section, key, ++value));
		counter++;
	}

	OpStringC8 messages_key(remove_message ? "Excluded Messages" : "Included Messages");

	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(file, "Messages", messages_key, 0, value));
	RETURN_IF_ERROR(PrefsUtils::WritePrefsInt(file, "Messages", messages_key, ++value));

	return OpStatus::OK;
}


class AutoFilterString
{
	public:
		OpString m_string;
		float m_value;

		bool operator>  (const AutoFilterString& item) const
		{
			return m_value != item.m_value ? m_value > item.m_value : m_string.Compare(item.m_string) > 0;
		};
		bool operator<  (const AutoFilterString& item) const
		{
			return m_value != item.m_value ? m_value <  item.m_value : m_string.Compare(item.m_string) < 0;
		};
		bool operator== (const AutoFilterString& item) const
		{
			return m_value != item.m_value ? m_value == item.m_value : !m_string.Compare(item.m_string);
		};
};


OP_STATUS Indexer::AutoFilter(Message* message, const OpStringC& body_text, Index* index, double& score)
{
	// Set filtering, so that we don't run the message through the autofilter process
	// when it's added to the index (i.e. Don't run UpdateAutoFilter on the message)
	OpMisc::Resetter<bool> filter_resetter(m_no_autofilter_update, false);
	m_no_autofilter_update = true;

	score = 0.5;
	
	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	PrefsFile* file = index->GetAutoFilterFile();

	if (file == NULL)
	{
		return OpStatus::OK;
	}

	OpString local_body_text;
	if (body_text.IsEmpty())
	{
		message->QuickGetBodyText(local_body_text);
	}

	OpString header_text;

	// add important headers
	Header::HeaderValue header;
	message->GetFrom(header);
	header_text.Set(header);
	message->GetHeaderValue(Header::SUBJECT, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::TO, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::CC, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::NEWSGROUPS, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::REPLYTO, header);
	header_text.Append(" ");
	header_text.Append(header);
	message->GetHeaderValue(Header::LISTID, header);
	header_text.Append(" ");
	header_text.Append(header);
	header_text.Append(" ");

	header_text.Append(local_body_text.HasContent() ? (OpStringC)local_body_text : (OpStringC)body_text);

	if (header_text.IsEmpty())
	{
		return OpStatus::OK;
	}

	MessageTokenizer tokenizer(header_text);

	OpString key;
	OpString section;

	int counter = 0;
	float value = 0;
	float include = 0;
	float exclude = 0;
	int times_included = 0;
	int times_excluded = 0;

	int included_messages = 1, excluded_messages = 1;
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(file, "Messages", "Included Messages", 0, included_messages));
	included_messages++;
	RETURN_IF_ERROR(PrefsUtils::ReadPrefsInt(file, "Messages", "Excluded Messages", 0, excluded_messages));
	excluded_messages++;

	// make sure we have something reasonable to filter on
	if (included_messages < NEEDED_AUTOFILTER_MSG_COUNT && index->MessageCount() >= NEEDED_AUTOFILTER_MSG_COUNT)
	{
		for (INT32SetIterator it(index->GetIterator()); it && included_messages < NEEDED_AUTOFILTER_MSG_COUNT; it++)
		{
			Message included_message;
			if (OpStatus::IsSuccess(MessageEngine::GetInstance()->GetMessage(included_message, it.GetData(), TRUE)))
			{
				UpdateAutoFilter(&included_message, index, FALSE);
				included_messages++;
			}
		}
	}
	if (excluded_messages < NEEDED_AUTOFILTER_MSG_COUNT && included_messages >= NEEDED_AUTOFILTER_MSG_COUNT)
	{
		// no excluded messages
		Index* received = GetIndexById(IndexTypes::RECEIVED);
		if (received && received->MessageCount() >= NEEDED_AUTOFILTER_MSG_COUNT)
		{
			for (INT32SetIterator it(received->GetIterator()); it && excluded_messages < NEEDED_AUTOFILTER_MSG_COUNT; it++)
			{
				if (!index->Contains(it.GetData()))
				{
					Message excluded_message;
					if (OpStatus::IsSuccess(MessageEngine::GetInstance()->GetMessage(excluded_message, it.GetData(), TRUE)))
					{
						UpdateAutoFilter(&excluded_message, index, TRUE);
						excluded_messages++;
					}
				}
			}
		}
	}

	if (excluded_messages < NEEDED_AUTOFILTER_MSG_COUNT || included_messages < NEEDED_AUTOFILTER_MSG_COUNT)
	{
		score = 0.51; // we want to give the Spam filter a second chance
		return OpStatus::OK; // Sorry, nothing to filter on yet.
	}

	OpAutoSortedVector<AutoFilterString> keys;

	while (tokenizer.GetNextToken(key) == OpStatus::OK &&
			key.HasContent() && counter < 200)
	{
		RETURN_IF_LEAVE(times_included = file->ReadIntL(UNI_L("Include"), key, 0);
						times_excluded = file->ReadIntL(UNI_L("Exclude"), key, 0));

		include = ((float)times_included) / (float)(included_messages);
		exclude = ((float)times_excluded) / (float)(excluded_messages);

		value = (include + (float)0.00001) / (include + exclude + (float)0.00002);

		AutoFilterString* filter = OP_NEW(AutoFilterString, ());
		if (filter)
		{
			filter->m_string.Set(key);
			filter->m_value = value;

			if (keys.Contains(filter))
			{
				OP_DELETE(filter);
			}
			else
			{
				keys.Insert(filter);
			}
		}
		counter++;
	}

	double total_negative = 0.5;
	double total_positive = 0.5;

	for (int high = keys.GetCount()-1; high >= 0 && high >= (int)keys.GetCount() - 11; high--)
	{
		AutoFilterString* filter = keys.GetByIndex(high);

		if (filter->m_value > 0.5)
		{
			total_positive *= filter->m_value;
			total_negative *= (1-filter->m_value);
		}
	}
	for (unsigned low = 0; low < 10 && low < keys.GetCount(); low++)
	{
		AutoFilterString* filter = keys.GetByIndex(low);

		if (filter->m_value < 0.5)
		{
			total_positive *= filter->m_value;
			total_negative *= (1-filter->m_value);
		}
	}

	score = total_positive / (total_positive + total_negative);

	BOOL match = score > 0.995;

	if (match)
	{
		// we have a hit
		if (index->GetId() == IndexTypes::SPAM)
		{
			OpINT32Vector vector;
			RETURN_IF_ERROR(vector.Add(message->GetId()));
			RETURN_IF_ERROR(MessageEngine::GetInstance()->MarkAsSpam(vector, TRUE));
		}
		else
		{
			RETURN_IF_ERROR(index->NewMessage(message->GetId()));
		}

		// update icon etc.
		MessageEngine::GetInstance()->OnMessageChanged(message->GetId());
	}
#if defined(_DEBUG) && defined (MSWIN) && defined (DEBUG_INDEXER_AUTOFILTER)
	OpString buf;
	buf.Reserve(30);
	OutputDebugString(UNI_L("\nTotal: "));
	OutputDebugString(uni_ltoa((int)(score*1000), buf.CStr(), 10));
	OutputDebugString(UNI_L("/1000\nMatch: "));
	OutputDebugString(uni_ltoa(match, buf.CStr(), 10));
#endif // _DEBUG

	return OpStatus::OK;
}


BOOL Indexer::Match(const OpStringC16& haystack, const OpStringC16& needle, SearchTypes::Option option)
{
	switch (option)
	{
		case SearchTypes::ANY_WORD:
		{
			BOOL more = needle.HasContent();
			BOOL match = FALSE;
			OpString search;
			search.Set(needle);

			while (more)
			{
				BOOL negated = FALSE;
				if (search[0] && search[0] == '-')
				{
					negated = TRUE;
					search.Set(search.CStr()+1);
				}

				int pos = search.FindFirstOf(UNI_L(" "));
				if (pos == KNotFound)
				{
					BOOL current = (haystack.FindI(search) != KNotFound);
					if (current && negated)
					{
						return FALSE;
					}
					else if (current)
					{
						return TRUE;
					}
					return match;
				}
				else
				{
					search[pos] = 0;

					BOOL current = (haystack.FindI(search) != KNotFound);
					if (current && negated)
					{
						return FALSE;
					}
					else if (current)
					{
						match = TRUE;
					}

					search.Set(search.CStr()+pos+1);
				}

				more = search.HasContent();
			}

			return match;
		}
		case SearchTypes::DOESNT_CONTAIN:
		{
			return (haystack.FindI(needle) == KNotFound);
		}
		case SearchTypes::REGEXP:
		{
			return MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->MatchRegExp(needle, haystack);
		}
		case SearchTypes::EXACT_PHRASE:
		default:
		{
			return (haystack.FindI(needle) != KNotFound);
		}
	}
}


BOOL Indexer::ContinueSearch()
{
	UINT32 searches_in_progress = 0;
	Message message;

	OpStatus::Ignore(NewMessage(message, TRUE, searches_in_progress));

	return searches_in_progress > 0;
}


OP_STATUS Indexer::NewMessage(Message& message, BOOL continue_search, UINT32 &searches_in_progress)
{
    OP_STATUS ret;
	Index *current;
	Header::HeaderValue header;
	OpString list_id;
	OpString list_name;
	message_gid_t message_id = message.GetId();

	BOOL contact_found = FALSE;
	BOOL list_found = FALSE;
	BOOL is_spam = FALSE;

	OpString body_text;
	BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();

	// Simply add a new message to the indexes:
	if (!continue_search)
	{
		// add to sent messages
		if (message.IsFlagSet(Message::IS_OUTGOING))
		{
			if (message.IsFlagSet(Message::IS_SENT))
			{
				current = GetIndexById(IndexTypes::SENT);
			}
			else if (message.IsFlagSet(Message::IS_IMPORTED) &&
					!message.IsFlagSet(Message::IS_TIMEQUEUED))	// no support for time deferred msgs yet, put in drafts
			{
				current = GetIndexById(IndexTypes::OUTBOX);
			}
			else
			{
				current = GetIndexById(IndexTypes::DRAFTS);
			}
			if (current)
			{
				// Add mail message to contacts if necessary
				RETURN_IF_ERROR(AddMessageToContacts(&message, message.GetHeader(Header::TO)));
				RETURN_IF_ERROR(AddMessageToContacts(&message, message.GetHeader(Header::CC)));
				RETURN_IF_ERROR(AddMessageToContacts(&message, message.GetHeader(Header::BCC)));

				// add to sent messages
				RETURN_IF_ERROR(current->NewMessage(message.GetId()));
			}
			else
			{
				return OpStatus::ERR;
			}
		}

		message.QuickGetBodyText(body_text);

		// add to junk mail folder if spam
		if (!message.IsConfirmedNotSpam())
			RETURN_IF_ERROR(SpamFilter(&message, is_spam, body_text));


		// add to active threads if needed
		if (!message.IsFlagSet(Message::IS_IMPORTED))
		{
			RETURN_IF_ERROR(AddToActiveThreads(message));
		}

		// add to attachment points if avail:
		if (message.IsFlagSet(Message::HAS_ATTACHMENT))
		{
			BOOL special_attachment = FALSE;
			Index* attachments = NULL;

			if (message.IsFlagSet(Message::HAS_IMAGE_ATTACHMENT))
			{
				special_attachment = TRUE;
				attachments = GetIndexById(IndexTypes::IMAGE_ATTACHMENT);
				if (attachments)
				{
					attachments->NewMessage(message.GetId());
				}
			}
			if (message.IsFlagSet(Message::HAS_VIDEO_ATTACHMENT))
			{
				special_attachment = TRUE;
				attachments = GetIndexById(IndexTypes::VIDEO_ATTACHMENT);
				if (attachments)
				{
					attachments->NewMessage(message.GetId());
				}
			}
			if (message.IsFlagSet(Message::HAS_AUDIO_ATTACHMENT))
			{
				special_attachment = TRUE;
				attachments = GetIndexById(IndexTypes::AUDIO_ATTACHMENT);
				if (attachments)
				{
					attachments->NewMessage(message.GetId());
				}
			}
			if (message.IsFlagSet(Message::HAS_ZIP_ATTACHMENT))
			{
				special_attachment = TRUE;
				attachments = GetIndexById(IndexTypes::ZIP_ATTACHMENT);
				if (attachments)
				{
					attachments->NewMessage(message.GetId());
				}
			}
			if (!special_attachment)
			{
				attachments = GetIndexById(IndexTypes::DOC_ATTACHMENT);
				if (attachments)
				{
					attachments->NewMessage(message.GetId());
				}
			}
		}

		// check if we have a mailing list id

		GetListId(&message, list_found, list_id, list_name);
	}

	// go through all indexes in memory
	for (INT32 it = -1; (current = GetRange(0, IndexTypes::LAST_INDEX, it)) != NULL; )
	{
		IndexSearch *search = current->GetSearch();

		// Perform a regular search:
		if (continue_search)
		{
			if (!search)
				continue;

			message_id = search->GetCurrentId();
			if (!message_id)
				continue;

			search->SetCurrentId(message_id-1);
			if (message_id-1 == 0)
			{
				for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
				{
					RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexChanged(this, current->GetId()));
				}
			}

			searches_in_progress++;

			if (OpStatus::IsError(GetStore()->GetMessage(message, message_id)))
				continue;
		}

		// add to inbox if not a draft
		// or add to other special indexes 

		if (current->GetId() == IndexTypes::RECEIVED
			&& !continue_search)
		{
			RETURN_IF_ERROR(current->NewMessage(message_id));
		}
		else if (current->GetId() == IndexTypes::RECEIVED_NEWS
				&& !continue_search
				&& message.IsFlagSet(Message::IS_NEWS_MESSAGE))
		{
			RETURN_IF_ERROR(current->NewMessage(message_id));
		}


		if (current->GetId() >= IndexTypes::FIRST_ACCOUNT && current->GetId() < IndexTypes::LAST_ACCOUNT)
		{
			if (message.GetAccountId() == (current->GetId() - IndexTypes::FIRST_ACCOUNT))
			{
				RETURN_IF_ERROR(current->NewMessage(message_id));
			}
		}

		// last special case before normal searches: AutoFilter
		if (current->GetHasAutoFilter() && current->GetId() != IndexTypes::SPAM)
		{
			double score;
			AutoFilter(&message, body_text, current, score);
		}

		if (!search)
			continue;

		OpStringC search_text = search->GetSearchText();

		// add to contact indexes
		// contact indexes are only added to m_index_container when needed

		if (current->GetType() == IndexTypes::CONTACTS_INDEX)
		{
			Header *from_header = message.GetHeader(Header::FROM);
			if (!from_header)
			{
				from_header = message.GetHeader(Header::SENDER);
				if (!from_header)
				{
					from_header = message.GetHeader(Header::REPLYTO);
				}
			}

			if (from_header)
			{
				const Header::From* from = from_header->GetFirstAddress();

				if (from && from->CompareAddress(search_text)==0)
				{
					contact_found = TRUE;
					RETURN_IF_ERROR(current->NewMessage(message_id));

					if (!continue_search && m_debug_contact_list_enabled &&
						browser_utils->IsContact(search_text))
					{
						current->SetVisible(TRUE);
					}
				}
			}

			// check for mailing list
			if (!list_id.IsEmpty() && !search_text.CompareI(list_id))
			{
				list_found = TRUE;
				RETURN_IF_ERROR(current->NewMessage(message_id));

				/* This causes the mailing list category to become visible
				
				if (!continue_search)
				{
					current->SetVisible(TRUE);
				}*/
			}
		}

		// add to newsgroup indexes

		else if (current->GetType() == IndexTypes::NEWSGROUP_INDEX)
		{

			// FIXME: Should use GetNewsGroup on Header, but this wasn't avail.

			Header *newsgroup_header = message.GetHeader(Header::NEWSGROUPS);

			if (newsgroup_header && current->GetAccountId() == message.GetAccountId())
			{
                int i = 0;
				OpString8 newsgroup;
                while (newsgroup_header->GetNewsgroup(newsgroup, i++) == OpStatus::OK)
                {
                    if (newsgroup.IsEmpty())
                        break;

					if (search_text.CompareI(newsgroup) == 0 &&
						!message.IsFlagSet(Message::IS_OUTGOING))
					{
						RETURN_IF_ERROR(current->NewMessage(message_id));
					}
                }
			}
			else if (newsgroup_header && search_text.Find(UNI_L("@")) != KNotFound)
			{
				// single message

				Header::HeaderValue msgid;
				message.GetHeaderValue(Header::MESSAGEID, msgid);

				if (msgid.HasContent())
				{
					if (search_text.FindI(msgid) != KNotFound)
					{
						RETURN_IF_ERROR(current->NewMessage(message_id));
					}
				}
			}
		}

		// add to search indexes

		else if (current->GetType() == IndexTypes::SEARCH_INDEX || current->GetId() == IndexTypes::SPAM ||
				(current->GetId() >= IndexTypes::FIRST_FOLDER && current->GetId() < IndexTypes::LAST_FOLDER) )
		{
			if (!m_debug_filters_enabled)
			{
				continue;
			}

			char *raw_message;
			BOOL current_matched = FALSE;
			BOOL all_and_matched = TRUE;

			// check date first

			// Use 32-bit values for time, we save 32-bit values in the prefsfile. Don't change this!
			UINT32 recv_date = message.GetRecvTime();
			UINT32 start_date = search->GetStartDate();
			UINT32 end_date = search->GetEndDate();

			if (!(recv_date && start_date <= recv_date && recv_date <= end_date))
			{
				if (continue_search && recv_date && start_date > recv_date)
				{
					// Search finished
					search->SetCurrentId(0);
					for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
					{
						RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexChanged(this, current->GetId()));
					}
				}
				continue;
			}

			UINT32 pos;

			for (pos = 0; pos < current->GetSearchCount(); pos++)
			{
				// pure date searches not allowed:

				IndexSearch *cur_search = current->GetSearch(pos);

				if (cur_search->GetSearchText().IsEmpty())
					continue;

				SearchTypes::Operator op = cur_search->GetOperator();
				if (op == SearchTypes::OR && current_matched && all_and_matched)
				{
					// we have a final result
					break;
				}
				if (op == SearchTypes::AND)
				{
					current_matched = FALSE;
				}
				else
				{
					// reset AND counter
					current_matched = FALSE;
					all_and_matched = TRUE;
				}

				Header::HeaderType header_type = Header::UNKNOWN;

				switch( cur_search->SearchBody() )
				{
				case SearchTypes::CACHED_HEADERS:
					{
						// Search in from/subject only
						RETURN_IF_ERROR(message.GetHeaderValue(Header::FROM, header));
						if (Match(header, cur_search->GetSearchText(), cur_search->GetOption()))
						{
							current_matched = TRUE;
						}

						RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, header));
						if (Match(header, cur_search->GetSearchText(), cur_search->GetOption()))
						{
							current_matched = TRUE;
						}
					}
					break;
				case SearchTypes::CACHED_SUBJECT:
					{
						RETURN_IF_ERROR(message.GetHeaderValue(Header::SUBJECT, header));
						if (Match(header, cur_search->GetSearchText(), cur_search->GetOption()))
						{
							current_matched = TRUE;
						}
					}
					break;

				case SearchTypes::BODY:
				case SearchTypes::ENTIRE_MESSAGE:
					{
						if (!message.GetRawBody())
						{
							RETURN_IF_ERROR(GetStore()->GetMessageData(message));
						}

						SearchTypes::Option option = cur_search->GetOption();

						if ((raw_message=(char*)message.GetRawBody()) != NULL)
						{
							if (body_text.IsEmpty())
							{
								RETURN_IF_ERROR(body_text.Set(raw_message));
							}
							if (Match(body_text, cur_search->GetSearchText(), option) && option!=SearchTypes::DOESNT_CONTAIN)
							{
								current_matched = TRUE;
							}
						}
					}
					// Don't break, fall through and search headers as well:
					if (cur_search->SearchBody() == SearchTypes::BODY)
					{
						break;
					}
				case SearchTypes::HEADERS:
					{
						// Search on disk - sloooow.

						OpString8 current_headers;
						OpString raw_header;
						BOOL dummy_dropped_illegal_headers;

						if (!message.GetOriginalRawHeaders())
						{
							RETURN_IF_ERROR(GetStore()->GetMessageData(message));
						}
						if ((raw_message=(char*)message.GetOriginalRawHeaders()) != NULL)
						{
							RETURN_IF_ERROR(raw_header.Set(raw_message));
							if (Match(raw_header, cur_search->GetSearchText(), cur_search->GetOption()))
							{
								current_matched = TRUE;
							}
						}
						else if (message.GetCurrentRawHeaders(current_headers, dummy_dropped_illegal_headers) == OpStatus::OK
							&& !current_headers.IsEmpty())
						{
							RETURN_IF_ERROR(raw_header.Set(current_headers));
							if (Match(raw_header, cur_search->GetSearchText(), cur_search->GetOption()))
							{
								current_matched = TRUE;
							}
						}
					}
					break;

				case SearchTypes::CACHED_SENDER:
				case SearchTypes::HEADER_FROM:
					if (header_type == Header::UNKNOWN)
						header_type = Header::FROM;
				case SearchTypes::HEADER_TO:
					if (header_type == Header::UNKNOWN)
						header_type = Header::TO;
				case SearchTypes::HEADER_CC:
					if (header_type == Header::UNKNOWN)
						header_type = Header::CC;
				case SearchTypes::HEADER_REPLYTO:
					if (header_type == Header::UNKNOWN)
						header_type = Header::REPLYTO;
				case SearchTypes::HEADER_NEWSGROUPS:
					if (header_type == Header::UNKNOWN)
						header_type = Header::NEWSGROUPS;
					{
						// Search on disk - sloooow.

						OpString8 current_headers;

						if (!message.GetOriginalRawHeaders())
						{
							if (message.GetHeader(header_type) == NULL)
							{
								RETURN_IF_ERROR(GetStore()->GetMessageData(message));
							}
						}

						RETURN_IF_ERROR(message.GetHeaderValue(header_type, header));

						if (Match(header, cur_search->GetSearchText(), cur_search->GetOption()))
						{
							current_matched = TRUE;
						}
					}
					break;
				case SearchTypes::FOLDER_PATH:
					{
						// only for IMAP or during import
					}
					break;
				default:
					{
						OP_ASSERT(0); // not defined
					}
					break;
				}

				if (!current_matched)
				{
					all_and_matched = FALSE;
				}
			}

			if (current_matched && all_and_matched)
			{
				BOOL message_hidden = FALSE;

				if (current->GetHideFromOther())
				{
					if (!m_hidden_group || !m_hidden_group->GetIndex()->Contains(message_id))
					{
						message_hidden = TRUE;
					}
				}
		
				if (current->GetId() == IndexTypes::SPAM)
				{
					OpINT32Vector vector;
					RETURN_IF_ERROR(vector.Add(message_id));
					RETURN_IF_ERROR(MessageEngine::GetInstance()->MarkAsSpam(vector, TRUE));
				}
				else
				{
					RETURN_IF_ERROR(current->NewMessage(message_id));
				}

				if (message_hidden && continue_search)
				{
					MessageEngine::GetInstance()->OnMessageHidden(message_id, TRUE);
				}
			}
		}
	}


	// then check for mailing list indexes on disk and add those to
	// the internal list for faster access next time..

	if (!list_found && !continue_search && !list_id.IsEmpty() && !is_spam)
	{
		Index *index = OP_NEW(Index, ());
		if (!index)
			return OpStatus::ERR_NO_MEMORY;

        IndexSearch search;
		search.SetSearchBody(SearchTypes::CACHED_HEADERS);
		index->SetParentId(IndexTypes::CATEGORY_MAILING_LISTS);
		index->SetModelType(IndexTypes::MODEL_TYPE_THREADED); // thread new mailing lists
		index->SetSaveToDisk(TRUE);
		index->SetType(IndexTypes::CONTACTS_INDEX);

        if ( ((ret=index->SetName(list_name.CStr())) != OpStatus::OK) ||
             ((ret=search.SetSearchText(list_id)) != OpStatus::OK) ||
             ((ret=index->AddSearch(search)) != OpStatus::OK) ||
             ((ret=NewIndex(index)) != OpStatus::OK) ||
             ((ret=index->NewMessage(message_id)) != OpStatus::OK) )
        {
			OP_ASSERT(FALSE);
            OP_DELETE(index);
            return ret;
        }
	}
	
	if (!continue_search)
	{
		if (!list_id.IsEmpty())
		{
			Index* received_list = GetIndexById(IndexTypes::RECEIVED_LIST);
			received_list->NewMessage(message_id);
		}

		// add to read/unread messages
		// last thing to do when all indexes are loaded
		if (!MessageEngine::GetInstance()->GetStore()->GetMessageFlag(message_id,Message::IS_READ))
		{
			RETURN_IF_ERROR(MessageRead(message_id, FALSE, m_currently_indexing == message_id)); // this is only a change if message is unread
		}
	}

	// then check for contact indexes on disk and add those to
	// the internal list for faster access next time..

	if (!contact_found && !continue_search && m_debug_contact_list_enabled)
	{
		Header *from_header = message.GetHeader(Header::FROM);
		if (!from_header)
		{
			from_header = message.GetHeader(Header::SENDER);
			if (!from_header)
			{
				from_header = message.GetHeader(Header::REPLYTO);
				if (!from_header)
				{
					MessageEngine::GetInstance()->OnMessageChanged(message_id); // make sure unread counts etc update correctly
					return OpStatus::OK;
				}
			}
		}
		const Header::From* from = from_header->GetFirstAddress();
		if (!from || !from->HasAddress())
		{
			MessageEngine::GetInstance()->OnMessageChanged(message_id); // make sure unread counts etc update correctly
			return OpStatus::OK;
		}

		const OpStringC16& address = from->GetAddress();

		// only saving news postings for real contacts
		if (message.IsFlagSet(Message::IS_NEWS_MESSAGE) &&
			!browser_utils->IsContact(address))
		{
			MessageEngine::GetInstance()->OnMessageChanged(message_id); // make sure unread counts etc update correctly
			return OpStatus::OK;
		}

		OpAutoPtr<Index> index (OP_NEW(Index, ()));
		if (!index.get())
			return OpStatus::ERR_NO_MEMORY;

		index->SetType(IndexTypes::CONTACTS_INDEX);

		OpString name;
		if (m_debug_contact_list_enabled)
			RETURN_IF_ERROR(browser_utils->GetContactName(address,name));
		if (!address.CompareI(name))
		{
			// contact not found in contact list...
			if (from->HasName())
			{
				RETURN_IF_ERROR(index->SetName(from->GetName().CStr()));
			}
			else
			{
				RETURN_IF_ERROR(index->SetName(from->GetAddress().CStr()));
			}
		}
		else
		{
			RETURN_IF_ERROR(index->SetName(name.CStr()));
		}

		index->SetVisible(FALSE);


		IndexSearch search;
		search.SetSearchBody(SearchTypes::CACHED_HEADERS);
        OpString tmp_searchtext;

		index->SetSaveToDisk(TRUE);

        if ( ((ret=search.SetSearchText(from->GetAddress())) != OpStatus::OK) ||
             ((ret=index->AddSearch(search)) != OpStatus::OK) ||
             (index->IsVisible() && (ret=NewIndex(index.get())) != OpStatus::OK) ||
             ((ret=index->NewMessage(message_id, TRUE)) != OpStatus::OK) )
        {
			OP_ASSERT(FALSE);
            return ret;
        }

		if (!index->IsVisible())
			index.reset();

		index.release();

		// prefetch ? Probably not until displayed by UI

	}
	if (!continue_search || message_id != 0)
		MessageEngine::GetInstance()->OnMessageChanged(message_id); // make sure unread counts etc update correctly

	return m_message_database->RequestCommit();
}

OP_STATUS Indexer::AddMessageToContacts(Message* message, Header *header)
{
	Account* account = MessageEngine::GetInstance()->GetAccountById(message->GetAccountId());
	BOOL addcontact = account && account->GetAddContactWhenSending();
	const Header::From* singleaddress = header ? header->GetFirstAddress() : NULL;

	while (singleaddress)
	{
		const OpStringC16& address = singleaddress->GetAddress();
		const OpStringC16& name = singleaddress->GetName().HasContent() ?  singleaddress->GetName() : address;
		if (address.HasContent())
		{
			// add to contacts if not there already
			if (address.Find(UNI_L("@")) != KNotFound)
			{
				if (addcontact)
				{
					RETURN_IF_ERROR(MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->AddContact(address,name));
				}

				Index* contact_index = GetContactIndex(address);
				if (contact_index)
				{
					RETURN_IF_ERROR(contact_index->NewMessage(message->GetId()));
				}
			}
		}
		singleaddress = (Header::From*)(singleaddress->Suc());
	}

	return OpStatus::OK;
}

void Indexer::OnMessageBodyChanged(StoreMessage& message)
{
	// We don't want to add drafts through the Indexer::NewIndex just yet, this should only happen when it's sent
	if (!message.IsFlagSet(Message::IS_OUTGOING) || message.IsFlagSet(Message::IS_SENT))
	{
		// TODO: adapt indexer to StoreMessage as well, cast should disappear
		ChangedMessage(static_cast<Message&>(message));
	}
}

void Indexer::OnMessageMadeAvailable(message_gid_t message_id, BOOL read)
{
	Index* received = GetIndexById(IndexTypes::RECEIVED);
	if (received)
		received->NewMessage(message_id);

	if (!read)
		OpStatus::Ignore(MessageRead(message_id, FALSE, TRUE));
}

void Indexer::OnAllMessagesAvailable()
{
	if (m_verint == 11)
	{
		OpStatus::Ignore(UpgradeFromVersion11());
	}
}

OP_STATUS Indexer::RemoveMessage(message_gid_t message_id, index_gid_t except_from)
{
	Index *current;
	Message message;
	OpString list_id;
	OpString list_name;
	OpMisc::Resetter<message_gid_t> reset(m_removing_message, 0);

	BOOL list_found = FALSE;

	// remove from unread mails..
	if (!message.IsFlagSet(Message::IS_READ))
	{
		OpStatus::Ignore(MessageRead(message_id, TRUE));
		OpStatus::Ignore(GetStore()->SetMessageFlag(message_id, Message::IS_READ, TRUE));
	}

	// Let other functions know that we're removing this message permanently now
	m_removing_message = message_id;

	// check if we have a mailing list id

	GetListId(&message, list_found, list_id, list_name);

	Header *from_header = message.GetHeader(Header::FROM);
	if (!from_header)
	{
		from_header = message.GetHeader(Header::SENDER);
		if (!from_header)
		{
			from_header = message.GetHeader(Header::REPLYTO);
			if (!from_header)
			{
				// continue..
			}
		}
	}


	const Header::From* from = NULL;
	if (from_header)
	{
		from = from_header->GetFirstAddress();
	}

	for (INT32 it = -1; (current = GetRange(0, IndexTypes::LAST, it)) != NULL; )
	{
		if (current->GetId() != except_from)
		{
			if (current->GetType() == IndexTypes::CONTACTS_INDEX)
			{
				IndexSearch *search = current->GetSearch();
				if (!search)
					continue;

				if (from && from->HasAddress())
				{
					if (from->CompareAddress(search->GetSearchText())==0)
					{
						OpStatus::Ignore(current->PreFetch()); // need to prefetch index so we can empty it on exit
					}
				}
				// check for mailing list
				if (!list_id.IsEmpty() && !search->GetSearchText().CompareI(list_id))
				{
					list_found = TRUE;
					OpStatus::Ignore(current->PreFetch()); // need to prefetch index so we can empty it on exit
				}
			}

			RETURN_IF_ERROR(current->RemoveMessage(message_id));
		}
	}

	if (!except_from)
	{
		// Let everyone know that trash has changed
		MessageEngine::GetInstance()->OnIndexChanged(IndexTypes::TRASH);
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::GetListId(Message* message, BOOL& list_found, OpString& list_id, OpString& list_name)
{
	// ATTENTION: list_found will be set to TRUE if we DON'T find a mailing list to avoid more searching
	Header *mailing_header = message->GetHeader("List-Id");
	const Header::From* mailing_from = NULL;

	if (mailing_header)
	{
		mailing_from = mailing_header->GetFirstAddress();
		if (mailing_from)
		{
			RETURN_IF_ERROR(list_id.Set(mailing_from->GetAddress()));

			if (list_id.HasContent())
			{
				if (list_id.Find(UNI_L(".")) == KNotFound)
				{
					list_id.Empty(); // avoids bug #92974
				}
				else
				{
					if (mailing_from->HasName())
					{
						RETURN_IF_ERROR(list_name.Set(mailing_from->GetName()));
					}
					else
					{
						RETURN_IF_ERROR(list_name.Set(mailing_from->GetAddress()));
					}
					return OpStatus::OK;
				}
			}
			// else fall through
		}
		// else fall through
	}

	mailing_header = message->GetHeader("List-post");
	if (mailing_header == NULL)
	{
		mailing_header = message->GetHeader("X-Mailing-List");
	}

	if (mailing_header)
	{
		Header::HeaderValue mailto;
		mailing_header->GetValue(mailto);

		int start_pos = mailto.Find(UNI_L(":")); // List-post: <mailto:email@address>
		if (start_pos == KNotFound)
		{
			// X-Mailing-List: <email@address> some/backup/reference
			start_pos = mailto.Find(UNI_L("<"));
		}

		int end_pos = mailto.Find(UNI_L(">"));
		int at_pos = mailto.Find(UNI_L("@"));

		if (start_pos != KNotFound && end_pos != KNotFound && start_pos < end_pos)
		{
			RETURN_IF_ERROR(list_id.Set(mailto));
			list_id[end_pos] = 0;
			if (at_pos != KNotFound)
			{
				list_id[at_pos] = '.';
			}
			RETURN_IF_ERROR(list_id.Set(list_id.CStr() + start_pos + 1));
			RETURN_IF_ERROR(list_name.Set(list_id));

			return OpStatus::OK;
		}
		// else fall through
	}

	// Also check if mail contains both Mailing-list and Delivered-to headers and parse those.
	// Needed for yahoo groups among others.

	mailing_header = message->GetHeader("Mailing-list");
	if (mailing_header)
	{
		mailing_header = message->GetHeader("Delivered-to");
	}

	if (mailing_header)
	{
		Header::HeaderValue header_value;
		OpString deliveredto;

		RETURN_IF_ERROR(mailing_header->GetValue(header_value));
		RETURN_IF_ERROR(deliveredto.Set(header_value));

		int start_pos = deliveredto.Find(UNI_L("mailing list "));
		int at_pos = deliveredto.Find(UNI_L("@"));

		if (start_pos != KNotFound && at_pos != KNotFound && start_pos < at_pos)
		{
			if (at_pos != KNotFound)
			{
				deliveredto[at_pos] = '.';
			}

			deliveredto.Set(deliveredto.CStr()+start_pos+13);
			int end_pos = deliveredto.Find(UNI_L(" "));
			if (end_pos != KNotFound)
			{
				deliveredto[end_pos] = 0;
			}

			list_id.Set(deliveredto);
			list_name.Set(deliveredto);

			return OpStatus::OK;
		}
		// else fall through
	}

	list_found = TRUE; // we have no mailing list, so don't search.
	return OpStatus::OK;
}


OP_STATUS Indexer::MessageSent(message_gid_t id)
{
	Index *outbox = GetIndexById(IndexTypes::OUTBOX);
	Index *sent = GetIndexById(IndexTypes::SENT);

	if (!sent || !outbox)
	{
		return OpStatus::ERR;
	}

	RETURN_IF_ERROR(sent->NewMessage(id));
	RETURN_IF_ERROR(outbox->RemoveMessage(id));

	Message message;
	RETURN_IF_ERROR(GetStore()->GetMessage(message, id));

	return AddToActiveThreads(message);
}


OP_STATUS Indexer::MessageRead(message_gid_t id, BOOL read, BOOL new_message)
{
	Index *unread = GetIndexById(IndexTypes::UNREAD);

	if (unread)
	{
		if (!read)
		{
			if (unread->Contains(id))
				return OpStatus::OK;

			RETURN_IF_ERROR(unread->NewMessage(id));
		}
		else
		{
			if (!unread->Contains(id))
				return OpStatus::OK;

			RETURN_IF_ERROR(unread->RemoveMessage(id));
		}
	}
	else
	{
		return OpStatus::ERR;
	}
	return OpStatus::OK;
}


OP_STATUS Indexer::NotSpam(message_gid_t message, BOOL only_add_contacts, int parent_folder_id)
{
	// add to contacts..
	OpString address;
	OpString name;
	BOOL dummy;

	Message object;
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetMessage(object, message, TRUE));

	BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();

	GetListId(&object, dummy, address, name);
	if (address.HasContent() && only_add_contacts)
	{
		RETURN_IF_ERROR(browser_utils->AddContact(address,name, TRUE));
	}

	Header *from_header = NULL;

	if (object.IsFlagSet(Message::IS_OUTGOING))
	{
		from_header = object.GetHeader(Header::TO);
	}
	else
	{
		from_header = object.GetHeader(Header::FROM);
	}

	if (from_header == NULL)
	{
		from_header = object.GetHeader(Header::SENDER);
		if (from_header == NULL)
		{
			from_header = object.GetHeader(Header::REPLYTO);
		}
	}

	if (from_header)
	{
		const Header::From* from = from_header->GetFirstAddress();
		if (from && from->HasAddress())
		{
			RETURN_IF_ERROR(address.Set(from->GetAddress()));
			if (from->HasName())
			{
				RETURN_IF_ERROR(name.Set(from->GetName()));
			}
			else
			{
				RETURN_IF_ERROR(name.Set(from->GetAddress()));
			}
			if (only_add_contacts)
			{
				RETURN_IF_ERROR(browser_utils->AddContact(address,name,FALSE,parent_folder_id));
				if (address.HasContent())
				{
					// just make sure the address is shown in active contacts
					GetContactIndex(address);
				}
			}
		}
	}

	if (!only_add_contacts)
	{
		// index all words in message
		//RETURN_IF_ERROR(ChangeLexicon(object, body_text, TRUE));
		RETURN_IF_ERROR(m_text_table->InsertMessage(message));
	}

	if (only_add_contacts)
	{
		return OpStatus::OK;
	}

	Index *junk = GetIndexById(IndexTypes::SPAM);

	if (junk == NULL)
	{
		return OpStatus::ERR;
	}

	// remove from junk folder

	RETURN_IF_ERROR(junk->RemoveMessage(message));
	MessageEngine::GetInstance()->OnMessageChanged(message);

	// un-hide other messages from same sender:

	if (!address.IsEmpty())
	{

		Index common_hidden;
		Index* contact_index = GetContactIndex(address);

		if (contact_index)
		{
			contact_index->PreFetch();

			// remove from junk folder

			Index common_junk;
			common_junk.SetVisible(FALSE);
			OpINT32Vector common_messages;

			RETURN_IF_ERROR(IntersectionIndexes(common_junk,contact_index,junk,0));
			RETURN_IF_ERROR(common_junk.GetAllMessages(common_messages));
			RETURN_IF_ERROR(MessageEngine::GetInstance()->MarkAsSpam(common_messages, FALSE, FALSE));
		}
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::Spam(message_gid_t message)
{
	Message object;
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetMessage(object, message, TRUE));

	Index *junk = GetIndexById(IndexTypes::SPAM);

	if (junk == NULL)
	{
		return OpStatus::ERR;
	}

	// add to junk folder
	RETURN_IF_ERROR(junk->NewMessage(message));
	MessageEngine::GetInstance()->OnMessageChanged(message);

	return OpStatus::OK;
}

INT32 Indexer::GetSpamLevel()
{
	INT32 spam_level;
	TRAPD(err, spam_level = m_prefs->ReadIntL(UNI_L("Spam Filter"), UNI_L("Start Score"), 30));
	return spam_level;
}

OP_STATUS Indexer::ChangeSpecialStatus(Index* index, AccountTypes::FolderPathType type, BOOL value)
{
	if (!index)
		return OpStatus::OK; // nothing to do

	switch (type)
	{
		case AccountTypes::FOLDER_SENT:
		{
			if (value)
				index->EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);

			for (INT32SetIterator it(index->GetIterator()); it; it++)
			{
				Message message;
				OpStatus::Ignore(GetStore()->SetMessageFlag(it.GetData(), Message::IS_OUTGOING, value));
				OpStatus::Ignore(GetStore()->SetMessageFlag(it.GetData(), Message::IS_SENT, value));

				// To make this message appear in the right indexes, remove it and then add again. TODO: better way to do this? Slow!
				if (OpStatus::IsSuccess(GetStore()->GetMessage(message, it.GetData())))
				{
					OpStatus::Ignore(RemoveMessage(it.GetData(), index->GetId()));
					OpStatus::Ignore(NewMessage(message));
				}
			}
			break;
		}
		case AccountTypes::FOLDER_TRASH:
		{
			if (value)
			{
				index->EnableModelFlag(IndexTypes::MODEL_FLAG_TRASH);

				OpINT32Vector all_messages;
				if (OpStatus::IsSuccess(index->GetAllMessages(all_messages)))
					OpStatus::Ignore(MessageEngine::GetInstance()->RemoveMessages(all_messages, FALSE));
			}
			break;
		}
		case AccountTypes::FOLDER_SPAM:
		{
			for (INT32SetIterator it(index->GetIterator()); it; it++)
			{
				// we don't want the spam filter to learn from these messages
				if (value)
					OpStatus::Ignore(GetIndexById(IndexTypes::SPAM)->InsertDirect(it.GetData()));
				else
					OpStatus::Ignore(GetIndexById(IndexTypes::SPAM)->RemoveMessage(it.GetData()));
			}
			
			if (value)
			{
				index->EnableModelFlag(IndexTypes::MODEL_FLAG_SPAM);
				if (index->GetHideFromOther())
				{
					index->SetHideFromOther(FALSE);
					RETURN_IF_ERROR(UpdateIndex(index->GetId()));
				}
			}
			break;
		}
	}

	index->SetSpecialUseType(value ? type : AccountTypes::FOLDER_NORMAL);

	return OpStatus::OK;
}

Indexer::MailCategory* Indexer::GetCategory(index_gid_t category) const
{
	for (UINT32 i = 0; i < m_categories.GetCount(); i++)
	{ 
		if (m_categories.Get(i)->m_id == category)
		{
			return m_categories.Get(i);
		}
	}
	return NULL;
}

INT32 Indexer::GetCategoryPosition(index_gid_t category) const
{
	INT32 position = 0;
	for (UINT32 i = 0; i < m_categories.GetCount(); i++)
	{ 
		if (m_categories.Get(i)->m_id == category)
		{
			return position;
		}
		else if (m_categories.Get(i)->m_visible)
			position++;
	}
	return -1;
}


void Indexer::SetCategoryOpen(index_gid_t index_id, BOOL open)
{
	MailCategory *category = GetCategory(index_id);
	if (category && category->m_open != open)
	{
		category->m_open = open;
		OpStatus::Ignore(WriteCategory(category));
	}
}

BOOL Indexer::GetCategoryOpen(index_gid_t index_id) const
{
	MailCategory *category = GetCategory(index_id);
	return category ? category->m_open : TRUE;
}

void Indexer::UpdateCategoryUnread(index_gid_t index_id)
{
	if (!m_message_database->HasFinishedLoading())
		return;

	MailCategory *category = GetCategory(index_id);
	Index* index = GetIndexById(index_id == IndexTypes::CATEGORY_MY_MAIL ? IndexTypes::UNREAD_UI : index_id);
	if (index && OpStatus::IsSuccess(index->PreFetch()) && category && category->m_unread_messages != index->UnreadCount())
	{
		SetCategoryUnread(index_id, index->UnreadCount());
	}
}

void Indexer::SetCategoryUnread(index_gid_t index_id, UINT32 unread_messages)
{
	MailCategory *category = GetCategory(index_id);
	if (category && category->m_unread_messages != unread_messages)
	{
		category->m_unread_messages = unread_messages;
		for (UINT32 idx = 0; idx < m_category_listeners.GetCount(); idx++)
		{
			OpStatus::Ignore(m_category_listeners.Get(idx)->CategoryUnreadChanged(index_id, unread_messages));
		}
		OpStatus::Ignore(WriteCategory(category));
	}
}

UINT32 Indexer::GetCategoryUnread(index_gid_t index_id) const
{
	MailCategory *category = GetCategory(index_id);
	return category ? category->m_unread_messages : 0;
}

void Indexer::SetCategoryVisible(index_gid_t index_id, BOOL visible)
{
	MailCategory *category = GetCategory(index_id);
	if (category && category->m_visible != visible)
	{
		category->m_visible = visible;
		if (visible)
			category->m_open = TRUE;
		for (UINT32 idx = 0; idx < m_category_listeners.GetCount(); idx++)
		{
			OpStatus::Ignore(m_category_listeners.Get(idx)->CategoryVisibleChanged(index_id, visible));
		}
		OpStatus::Ignore(WriteCategory(category));
	}
}

BOOL Indexer::GetCategoryVisible(index_gid_t index_id) const
{
	MailCategory *category = GetCategory(index_id);
	return category ? category->m_visible : FALSE;
}

OP_STATUS Indexer::MoveCategory(index_gid_t id, UINT32 new_position)
{
	for (UINT32 i = 0; i < m_categories.GetCount(); i++)
	{ 
		if (m_categories.Get(i)->m_id == id)
		{
			MailCategory *category = m_categories.Remove(i);
			
			BOOL added = FALSE;

			UINT32 j = 0, visible_items = 0;
			do
			{
				if (visible_items == new_position)
				{
					RETURN_IF_ERROR(m_categories.Insert(j, category));
					added = TRUE;
					break;
				}

				if (m_categories.Get(j)->m_visible)
				{
					visible_items++;
				}
				j++;
			}
			while (j < m_categories.GetCount());

			if (!added)
			{
				RETURN_IF_ERROR(m_categories.Insert(j, category));
			}

			break;
		}
		
	}
	return SaveRequest();
}

OP_STATUS Indexer::NewIndex(Index* index, BOOL save)
{
	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	index_gid_t id = index->GetId();

	if (0 < id && id < IndexTypes::LAST_IMPORTANT)
	{
		RETURN_IF_ERROR(m_index_container.AddIndex(id, index));
	}
	else
	{
		if (index->GetId() == 0)
		{
			switch (index->GetType())
			{
			case IndexTypes::CONTACTS_INDEX:
				{
					index->SetId(m_next_contact_id);
					m_next_contact_id++;
				}
				break;
			case IndexTypes::SEARCH_INDEX:
				{
					index->SetId(m_next_search_id);
					m_next_search_id++;
				}
				break;
			case IndexTypes::FOLDER_INDEX:
				{
					index->SetId(m_next_folder_id);
					m_next_folder_id++;
					if (!index->GetParentId())
						index->SetParentId(IndexTypes::CATEGORY_LABELS);
				}
				break;
			case IndexTypes::NEWSGROUP_INDEX:
				{
					index->SetId(m_next_newsgroup_id);
					m_next_newsgroup_id++;
				}
				break;
			case IndexTypes::IMAP_INDEX:
				{
					index->SetId(m_next_imap_id);
					m_next_imap_id++;
				}
				break;
			case IndexTypes::NEWSFEED_INDEX:
				{
					index->SetId(m_next_newsfeed_id);
					m_next_newsfeed_id++;
				}
				break;
			case IndexTypes::ARCHIVE_INDEX:
				{
					index->SetId(m_next_archive_id);
					m_next_archive_id++;
				}
				break;
			}
		}

		RETURN_IF_ERROR(m_index_container.AddIndex(index->GetId(), index));
	}

	RETURN_IF_ERROR(SetKeyword(index, index->GetKeyword()));
	RETURN_IF_ERROR(HandleHeadersOnNewIndex(index));

	// add to observer list
	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
    {
        RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexAdded(this, index->GetId()));
			
	}
	
	// if the parent is a category and it has only one child we need to notify categorylisteners
	UINT32 parent_id = index->GetParentId();
	OnParentChanged(index->GetId(), 0, parent_id);

	return SaveRequest();
}

OP_STATUS Indexer::RemoveFromChildVectors(Index* index, BOOL commit)
{
	// Loop the children and remove them.
	IndexChilds* index_children = 0;
	OpStatus::Ignore(m_index_childs.GetData(index->GetId(), &index_children));

	if (index_children != 0)
	{
		for (int idx = index_children->GetChildCount() - 1; idx >= 0; idx--)
		{
			const index_gid_t child_index_id = index_children->GetChild(idx);

			Index* child_index = GetIndexById(child_index_id);
			if (child_index != 0)
				RETURN_IF_ERROR(RemoveIndex(child_index, commit));
		}

		// Remove index childs container
		IndexChilds* index_childs;
		if (OpStatus::IsSuccess(m_index_childs.Remove(index->GetId(), &index_childs)))
			OP_DELETE(index_childs);
	}

	// Remove this index from parent's children
	return OnParentChanged(index->GetId(), index->GetParentId(), 0);
}

OP_STATUS Indexer::RemoveIndex(Index* index, BOOL commit)
{
	if (index->GetType() == IndexTypes::UNIONGROUP_INDEX || index->GetType() == IndexTypes::INTERSECTION_INDEX)
	{
		for (UINT32 i = 0; i < m_folder_indexgroups.GetCount(); i++)
		{
			if (m_folder_indexgroups.Get(i)->GetIndex() == index)
			{
				m_folder_indexgroups.Delete(i);
			}
		}
	}

	// Tell the observers
	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
    {
        RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexRemoved(this, index->GetId()));
	}
	
	UINT32 category_id = GetIndexById(index->GetId())->GetParentId();
	if (category_id >= IndexTypes::FIRST_CATEGORY && category_id <= IndexTypes::LAST_CATEGORY)
	{
		// we want to remove the category if it doesn't have other child indexes
		OpINT32Vector children;
		if (OpStatus::IsSuccess(GetChildren(category_id, children)) && children.GetCount() == 0)
		{
			SetCategoryVisible(category_id, FALSE);
		}
	}

	if (index->GetKeyword().CStr())
	{
		Keyword* keyword = 0;

		if (OpStatus::IsSuccess(m_keywords.GetData(index->GetKeyword().CStr(), &keyword)))
			keyword->index = 0;
	}

	RETURN_IF_ERROR(RemoveFromChildVectors(index, commit));

	// update (un)hidden messages:
	if (index->GetId() >= IndexTypes::FIRST_FOLDER && index->GetId() < IndexTypes::LAST_FOLDER)
	{
		if (m_hidden_group && m_hidden_group->IsIndexInGroup(index->GetId()) != static_cast<BOOL>(index->GetHideFromOther()))
		{
			RETURN_IF_ERROR(UpdateHiddenViews());
			MessageEngine::GetInstance()->OnMessageHidden(static_cast<message_gid_t>(-1), TRUE);
			MessageEngine::GetInstance()->OnIndexChanged(static_cast<UINT32>(-1));
		}
	}

	// remove the file if needed:
	if ((index->GetType() == IndexTypes::SEARCH_INDEX) ||
		(index->GetId() >= IndexTypes::FIRST_FOLDER && index->GetId() < IndexTypes::LAST_FOLDER) ||
		(index->GetType() == IndexTypes::IMAP_INDEX) ||
		(index->GetType() == IndexTypes::NEWSFEED_INDEX) ||
		(index->GetType() == IndexTypes::ARCHIVE_INDEX))
	{
		if (index->GetId() <= IndexTypes::LAST_IMPORTANT)
		{
			OP_ASSERT(0);
			return OpStatus::ERR;
		}
	}

	// Remove from tables
	m_index_container.DeleteIndex(index->GetId());

	if (commit)
	{
		SaveAllToFile();
	}
	else
	{
		RETURN_IF_ERROR(SaveRequest());
	}

	return OpStatus::OK;
}

OP_STATUS Indexer::UpdateIndex(index_gid_t id)
{
	//BOOL start_search = FALSE;
	Index* index = GetIndexById(id);
	if (index)
	{
		IndexSearch *search = index->GetSearch();
		// if current Id is set to -1, update with correct value
		if (search && search->GetCurrentId() == UINT_MAX)
		{
			search->SetCurrentId(GetStore()->GetLastId());
			StartSearch();
		}

		if (index->IsVisible())
		{
			for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
			{
				RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexChanged(this, id));
			}
		}

		if (m_hidden_group && m_hidden_group->IsIndexInGroup(index->GetId()) != static_cast<BOOL>(index->GetHideFromOther()))
		{
			RETURN_IF_ERROR(UpdateHiddenViews());
			MessageEngine::GetInstance()->OnMessageHidden(static_cast<message_gid_t>(-1), TRUE);
			MessageEngine::GetInstance()->OnIndexChanged(static_cast<UINT32>(-1));
		}
		
		RETURN_IF_ERROR(HandleHeadersOnNewIndex(index));
	}

	return SaveRequest();
}

OP_STATUS Indexer::RemoveMessagesInIndex(Index* index)
{
	// Make sure index is in memory
	index->PreFetch();

	OpINT32Vector all_message_ids;
	RETURN_IF_ERROR(index->GetAllMessages(all_message_ids));
	RETURN_IF_ERROR(g_m2_engine->RemoveMessages(all_message_ids, TRUE));

	return OpStatus::OK;
}


void Indexer::GetAccountRange(const Account* account, UINT32& range_start, UINT32& range_end)
{
	OP_ASSERT(account);

	switch(account->GetIncomingProtocol())
	{
		case AccountTypes::IMAP:
			range_start = IndexTypes::FIRST_IMAP;
			range_end = IndexTypes::LAST_IMAP;
			break;
		case AccountTypes::NEWS:
			range_start = IndexTypes::FIRST_NEWSGROUP;
			range_end = IndexTypes::LAST_NEWSGROUP;
			break;
		case AccountTypes::RSS:
			range_start = IndexTypes::FIRST_NEWSFEED;
			range_end = IndexTypes::LAST_NEWSFEED;
			break;
		case AccountTypes::POP:
			range_start = IndexTypes::FIRST_POP;
			range_end = IndexTypes::LAST_POP;
			break;
		case AccountTypes::ARCHIVE:
			range_start = IndexTypes::FIRST_ARCHIVE;
			range_end = IndexTypes::LAST_ARCHIVE;
			break;
	}
}


Index* Indexer::GetSpecialIndexById(index_gid_t index_id)
{
	if (index_id == IndexTypes::UNREAD_UI)
	{
		// special case, not yet created
		m_unread_group = OP_NEW(UnionIndexGroup, (IndexTypes::UNREAD_UI));
		if (m_unread_group)
		{
			OpStatus::Ignore(UpdateHideFromUnread());
			Index* index = m_unread_group->GetIndex();
			index->SetParentId(IndexTypes::CATEGORY_MY_MAIL);
			index->SetVisible(TRUE);
			return index;
		}
	}
	else if (index_id == IndexTypes::HIDDEN)
	{
		// special case: HIDDEN (from normal views)
		m_hidden_group = OP_NEW(UnionIndexGroup, (IndexTypes::HIDDEN));
		if (m_hidden_group)
		{
			OpStatus::Ignore(UpdateHiddenViews());
			return m_hidden_group->GetIndex();
		}
	}
	else if (IndexTypes::FIRST_CATEGORY <= index_id && index_id < IndexTypes::LAST_CATEGORY)
	{
		// special case: Categories
		if (OpStatus::IsSuccess(UpdateCategory(index_id)))
		{
			return GetCategory(index_id)->m_index_group->GetIndex();
		}
	}

	return 0;
}


OP_STATUS Indexer::UpdateHideFromUnread()
{
	Index* index = GetIndexById(IndexTypes::UNREAD);

	if (index && m_unread_group)
	{
		INT32 flags = index->GetModelFlags();
		IndexTypes::ModelAge age = index->GetModelAge();
		IndexTypes::ModelType type = index->GetModelType();

		m_unread_group->Empty();
		m_unread_group->AddIndex(IndexTypes::RECEIVED);

		((Index*)m_unread_group->GetIndex())->SetModelFlags(flags);
		((Index*)m_unread_group->GetIndex())->SetModelAge(age);
		((Index*)m_unread_group->GetIndex())->SetModelType(type,FALSE);

		OpString name;
		RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_UNREAD, name));

		m_unread_group->GetIndex()->SetName(name.CStr());
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::UpdateHiddenViews()
{
	if (m_hidden_group)
	{
		m_hidden_group->Empty();

		Index* index;
		for (INT32 it = -1; (index = GetRange(0, IndexTypes::LAST, it)) != NULL; )
		{
			if (index->GetHideFromOther())
			{
				index->PreFetch();
				m_hidden_group->AddIndex(index->GetId());
			}
		}
	}
	return OpStatus::OK;
}


Index* Indexer::GetContactIndex(const OpStringC16& address, BOOL visible)
{
	Index *current;
	Index *index = NULL;

	OpString list_address;
	list_address.Set(address);

	int pos;
	if ((pos = list_address.FindFirstOf(UNI_L("@"))) != KNotFound)
	{
		list_address[pos] = '.';
	}

	BOOL contact_found = FALSE;
	BOOL create_new = TRUE;
	for (INT32 it = -1; (current = GetRange(IndexTypes::FIRST_CONTACT, IndexTypes::LAST_CONTACT, it)) != NULL; )
	{
		IndexSearch *search = current->GetSearch();
		if (!search)
			continue;

		if (!search->GetSearchText().CompareI(address))
		{
			contact_found = TRUE;
			index = current;
			create_new = FALSE;
			break;
		}
		if (!search->GetSearchText().CompareI(list_address))
		{
			// not show a mailing list in active contacts as well
			visible = FALSE;
			create_new = FALSE;
			break;
		}
	}

	// then check for contact indexes on disk and add those to
	// the internal list for faster access next time..

	if (!contact_found && m_debug_contact_list_enabled && create_new)
	{
		index = OP_NEW(Index, ());
		if (!index)
		{
			return NULL;
		}
		index->SetVisible(visible!=FALSE);

		OpString name;
		index->SetType(IndexTypes::CONTACTS_INDEX);

		if (OpStatus::OK != MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->GetContactName(address,name) ||
			OpStatus::OK != index->SetName(name.CStr()))
		{
            OP_DELETE(index);
			return NULL;
		}
		index->EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);

		IndexSearch search;
		search.SetSearchBody(SearchTypes::CACHED_HEADERS);
		if (OpStatus::OK != search.SetSearchText(address) ||
			OpStatus::OK != index->AddSearch(search) ||
			OpStatus::OK != NewIndex(index) )
		{
			OP_ASSERT(FALSE);
            OP_DELETE(index);
			return NULL;
		}

		index->SetSaveToDisk(TRUE);
	}

	if (visible && !index->IsVisible())
	{
		index->SetVisible(TRUE);
		OpStatus::Ignore(UpdateIndex(index->GetId()));
	}
	return index;
}


Index* Indexer::GetCombinedContactIndex(OpString& address)
{
	int pos;

	if ((pos = address.FindFirstOf(UNI_L("@"))) != KNotFound)
	{
		// make sure we show mailing list as well as contact
		OpString list_id;
		list_id.Set(address);
		list_id[pos] = '.';

		if ((pos = list_id.FindFirstOf(UNI_L(","))) != KNotFound)
		{
			list_id[pos] = 0;
		}

		Index* current;
		for (INT32 it = -1; (current = GetRange(IndexTypes::FIRST_CONTACT, IndexTypes::LAST_CONTACT, it)) != NULL; )
		{
			IndexSearch *search = current->GetSearch();
			if (!search)
				continue;

			if (!search->GetSearchText().CompareI(list_id))
			{
				address.Append(UNI_L(","));
				address.Append(list_id);
				break;
			}
		}
	}

	if (address.FindFirstOf(UNI_L(",")) == KNotFound)
	{
		return GetContactIndex(address, FALSE);
	}

	UnionIndexGroup* group = OP_NEW(UnionIndexGroup, ());
	if (!group)
		return NULL;

	Index* index = NULL;

	while ((pos = address.FindFirstOf(UNI_L(","))) != KNotFound)
	{
		uni_char* buf = address.CStr();
		if (buf[pos+1])
		{
			address[pos] = 0;

			index = GetContactIndex(address, FALSE);
			if (index)
			{
				index->PreFetch();
				group->AddIndex(index->GetId());
			}
			address.Set(buf+pos+1);
		}
        else
        {
            break; // A single comma in address
        }
	}
	if (address.Length())
	{
		index = GetContactIndex(address, FALSE);
		if (index)
		{
			index->PreFetch();
			group->AddIndex(index->GetId());
		}
	}

	if (index)
	{
		OpString name;
		index->GetName(name);
		name.Append("...");
		group->GetIndex()->SetName(name.CStr());
	}

	return (Index*)group->GetIndex();
}


Index* Indexer::GetSubscribedFolderIndex(const Account* account, const OpStringC& folder_path, char path_delimiter, const OpStringC& folder_name, BOOL create_if_missing, BOOL ask_before_creating)
{
	// TODO This function is ugly
	UINT16 account_id = account ? account->GetAccountId() : 0;
	IndexTypes::Type index_type = account ? account->GetIncomingIndexType() : IndexTypes::FOLDER_INDEX;

	Index *current;
	Index *index = NULL;

	BOOL folder_found = FALSE;

	UINT32 range_start = IndexTypes::FIRST_FOLDER;
	UINT32 range_end = IndexTypes::LAST_FOLDER;

	// Check which ranges to search in
	if (account)
		GetAccountRange(account, range_start, range_end);

	for (INT32 it = -1; (current = GetRange(range_start, range_end, it)) != NULL; )
	{
		IndexSearch *search = current->GetSearch();
		if (!search)
			continue;

		if (current->GetType() == index_type)
		{
			if (current->GetAccountId() == account_id &&
				!search->GetSearchText().CompareI(folder_path))
			{
				folder_found = TRUE;
				index = current;
				break;
			}
		}
	}

	if (!folder_found)
	{
		index = NULL;
	}

	if (!folder_found && create_if_missing)
	{
		if (ask_before_creating)
		{
			OpString accept_new_subscription, new_subscription, part1, temp_var;

			g_languageManager->GetString(Str::D_NEW_SUBSCRIPTION_WARNING, new_subscription);
			g_languageManager->GetString(Str::D_NEW_SUBSCRIPTION_WARNING_TEXT_1, part1);

        	temp_var.AppendFormat(part1.CStr(), folder_name.HasContent() ? folder_name.CStr() : folder_path.CStr());
			accept_new_subscription.Set(temp_var.CStr());

			if (index_type == IndexTypes::NEWSFEED_INDEX)
			{
				OpString part2, lnBreaks;
				g_languageManager->GetString(Str::D_NEW_SUBSCRIPTION_DOUBLE_LINEBREAK, lnBreaks);
				g_languageManager->GetString(Str::D_NEW_SUBSCRIPTION_WARNING_TEXT_2, part2);
				accept_new_subscription.Append(lnBreaks);
				accept_new_subscription.Append(part2);
			}


			OP_ASSERT(g_application);
			DesktopWindow* parent = g_application ? g_application->GetActiveDesktopWindow() : NULL;
			if (SimpleDialog::ShowDialog(WINDOW_NAME_YES_NO, parent, accept_new_subscription.CStr(), 
				new_subscription.CStr(), Dialog::TYPE_YES_NO, Dialog::IMAGE_QUESTION)== Dialog::DIALOG_RESULT_NO)
			{
				return NULL;
			}
		}

		index = OP_NEW(Index, ());
		if (!index)
		{
			return NULL;
		}

		BOOL finished = FALSE;

		do
		{
			index->SetAccountId(account_id);
			index->SetParentId(IndexTypes::FIRST_ACCOUNT + account_id);
			index->SetType(index_type);
			if (OpStatus::IsError(index->SetName(folder_name.CStr())))
				break;

			if (index_type == IndexTypes::NEWSGROUP_INDEX)
			{
				index->SetModelType(IndexTypes::MODEL_TYPE_THREADED);

				/* Johan TODO: What was this?
				if (path.Find("@") != KNotFound)
				{
					INT32 flags = index->GetModelFlags();
					flags |= (1<<IndexTypes::MODEL_FLAG_TRASH);
					index->SetModelFlags(flags);
				}*/
			}

			// Find parent
			if (path_delimiter)
			{
				int pos = folder_path.FindLastOf(path_delimiter);

				if (pos != KNotFound)
				{
					OpString parent_path, parent_name;
					Index* parent_index;

					// Determine parent path and name of folder
					if (OpStatus::IsError(parent_path.Set(folder_path.CStr(), pos)))
						break;

					pos = parent_path.FindLastOf(path_delimiter);

					if (OpStatus::IsError(parent_name.Set(pos == KNotFound ? parent_path.CStr() : parent_path.CStr() + pos + 1)))
						break;

					// Get parent index
					parent_index = GetSubscribedFolderIndex(account, parent_path, path_delimiter, parent_name, TRUE, FALSE);
					if (!parent_index)
						break;

					// Set parent index
					index->SetParentId(parent_index->GetId());
				}
			}

			// Set search text, save new index
			IndexSearch search;

			search.SetSearchBody(SearchTypes::FOLDER_PATH);

			if (OpStatus::IsError(search.SetSearchText(folder_path)) ||
				OpStatus::IsError(index->AddSearch(search)) ||
				OpStatus::IsError(NewIndex(index)))
				break;

			index->SetVisible(TRUE);

			index->SetSaveToDisk(TRUE);

			finished = TRUE;
		} while (0);

		if (!finished)
		{
			OP_DELETE(index);
			return NULL;
		}
	}

	return index;
}

index_gid_t Indexer::GetRSSAccountIndexId()
{
	if (m_rss_account_index_id)
		return m_rss_account_index_id;

	m_rss_account_index_id = UINT_MAX;

	Account* account = MessageEngine::GetInstance()->GetAccountManager()->GetRSSAccount(FALSE);

	if (account)
	{
		Index* index = GetIndexById(account->GetAccountId() + IndexTypes::FIRST_ACCOUNT);
		if (index)
			m_rss_account_index_id = index->GetId();
	}
	return m_rss_account_index_id;
}


OP_STATUS Indexer::StatusChanged(Index* index)
{
	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
    {
		RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexChanged(this, index->GetId()));
	}
	return OpStatus::OK;
}


OP_STATUS Indexer::NameChanged(Index* index)
{
	index_gid_t id = index->GetId();
	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
   	{
		RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexNameChanged(this, id));
	}
	
	if (id >= IndexTypes::FIRST_ACCOUNT && id <= IndexTypes::LAST_ACCOUNT)
	{
		for (UINT32 idx = 0; idx < m_category_listeners.GetCount(); idx++)
		{
			RETURN_IF_ERROR(m_category_listeners.Get(idx)->CategoryNameChanged(id));
		}
	}

	return SaveRequest();
}


OP_STATUS Indexer::VisibleChanged(Index* index)
{
	UINT32 category_id = index->GetParentId();


	if (index->IsVisible() && IsCategory(category_id) && !GetCategoryVisible(category_id))
	{
		SetCategoryVisible(category_id, TRUE);
	}
	else if (!index->IsVisible() && IsCategory(category_id) && GetCategoryVisible(category_id))
	{
		OpINT32Vector children;
		GetChildren(category_id, children);
		if (!index->IsVisible() && children.GetCount() == 1)
		{
			SetCategoryVisible(category_id, FALSE);
		}
	}

	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
	{
		RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexVisibleChanged(this, index->GetId()));
	}

	return SaveRequest();
}


OP_STATUS Indexer::IconChanged(Index* index)
{
	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
	{
		RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexIconChanged(this, index->GetId()));
	}

	return SaveRequest();
}


void Indexer::OnAccountAdded(UINT16 account_id)
{
	index_gid_t index_id = IndexTypes::FIRST_ACCOUNT + account_id;
	Account* account = NULL;
	
	if (OpStatus::IsError(MessageEngine::GetInstance()->GetAccountManager()->GetAccountById(account_id, account)))
		return;

	if (account)
	{
		OpString  account_name;
		OpString8 account_protocol;

		account_name.Set(account->GetAccountName());

		switch (account->GetIncomingProtocol())
		{
			case AccountTypes::NEWS:
			{
				OpStatus::Ignore(AddFolderIndex(index_id, account_name, TRUE));
				GetIndexById(index_id)->SetModelType(IndexTypes::MODEL_TYPE_THREADED);
				break;
			}
			case AccountTypes::IMPORT:
			case AccountTypes::POP:
			{
				
				OpStatus::Ignore(AddFolderIndex(index_id, account_name, TRUE));

				OpStatus::Ignore(AddPOPInboxAndSentIndexes(account_id));

				break;
			}
			case AccountTypes::IMAP:
			{
				OpStatus::Ignore(AddFolderIndex(index_id, account_name, TRUE));
				break;
			}
			case AccountTypes::RSS:
			{
				m_rss_account_index_id = 0; // recalculate
				g_languageManager->GetString(Str::S_NEWSFEEDS_ACCOUNT_NAME, account_name);
				OpStatus::Ignore(AddFolderIndex(index_id, account_name, TRUE));
				break;
			}
			case AccountTypes::ARCHIVE:
			{
				OpStatus::Ignore(AddFolderIndex(index_id, UNI_L("Archive"), TRUE));
				break;
			}
				
		}
	}

	OpStatus::Ignore(AddCategory(index_id));
}

OP_STATUS Indexer::AddPOPInboxAndSentIndexes(UINT16 account_id)
{
	OpString sent, inbox;
	index_gid_t inbox_index_id = IndexTypes::GetPOPInboxIndexId(account_id);
	index_gid_t index_id = IndexTypes::GetAccountIndexId(account_id);

	// Add inbox
	ComplementIndexGroup* inbox_group = OP_NEW(ComplementIndexGroup, (inbox_index_id));
	if (!inbox_group)
		return OpStatus::ERR_NO_MEMORY;
	inbox_group->GetIndex()->SetSaveToDisk(TRUE);
	inbox_group->GetIndex()->SetParentId(index_id);
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_POP_INBOX, inbox));
	RETURN_IF_ERROR(inbox_group->GetIndex()->SetName(inbox.CStr()));
	inbox_group->GetIndex()->SetVisible(TRUE);
	inbox_group->GetIndex()->SetType(IndexTypes::COMPLEMENT_INDEX);
	inbox_group->GetIndex()->SetAccountId(account_id);

	RETURN_IF_ERROR(inbox_group->SetBase(index_id));
	RETURN_IF_ERROR(inbox_group->AddIndex(IndexTypes::SENT));
	
	RETURN_IF_ERROR(m_pop_indexes.Add(inbox_group));

	// Add sent
	IntersectionIndexGroup* sent_group = OP_NEW(IntersectionIndexGroup, (IndexTypes::GetPOPSentIndexId(account_id)));
	if (!sent_group)
		return OpStatus::ERR_NO_MEMORY;
	sent_group->GetIndex()->EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);
	sent_group->GetIndex()->SetVisible(TRUE);
	sent_group->GetIndex()->SetType(IndexTypes::INTERSECTION_INDEX);
	sent_group->GetIndex()->SetAccountId(account_id);
	sent_group->GetIndex()->SetSaveToDisk(TRUE);
	sent_group->GetIndex()->SetParentId(index_id);
	RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_FOLDER_IDX_SENT, sent));
	RETURN_IF_ERROR(sent_group->GetIndex()->SetName(sent.CStr()));
	
	RETURN_IF_ERROR(sent_group->SetBase(index_id));
	RETURN_IF_ERROR(sent_group->AddIndex(IndexTypes::SENT));

	return m_pop_indexes.Add(sent_group);
}

OP_STATUS Indexer::AddCategory(index_gid_t index_id)
{
	if (GetCategory(index_id))
		return OpStatus::OK;

	MailCategory* category = OP_NEW( MailCategory, (index_id));

	if (!category || OpStatus::IsError(m_categories.Add(category)))
	{
		OP_DELETE(category);
		return OpStatus::ERR_NO_MEMORY;
	}

	OpINT32Vector child_indexes;
	
	if (OpStatus::IsSuccess(GetChildren(index_id, child_indexes)) && child_indexes.GetCount() == 0)
		category->m_visible = FALSE;

	for (UINT32 idx = 0; idx < m_category_listeners.GetCount(); idx++)
	{
		RETURN_IF_ERROR(m_category_listeners.Get(idx)->CategoryAdded(index_id));
	}
	
	// Delayed saving to index.ini because OnAccountAdded can call this on start up, before reading index.ini and then it will wipe the categories
	SaveRequest();
	
	return OpStatus::OK;
}

OP_STATUS Indexer::RemoveCategory(index_gid_t index_id)
{
	for (UINT32 idx = 0; idx < m_category_listeners.GetCount(); idx++)
	{
		RETURN_IF_ERROR(m_category_listeners.Get(idx)->CategoryRemoved(index_id));
	}

	for (UINT32 i = 0; i< m_categories.GetCount(); i++)
	{
		if (m_categories.Get(i)->m_id == index_id)
		{
			m_categories.Delete(i);
			break;
		}
	}
	
	// Commit changes to index.ini
	SaveAllToFile();

	return OpStatus::OK;
}

void Indexer::OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type)
{
	Index* index;
	index_gid_t index_id = IndexTypes::FIRST_ACCOUNT + account_id;
	
	if (account_type == AccountTypes::POP)
	{
		for (UINT32 i = 0; i < m_pop_indexes.GetCount(); i++)
		{
			if (m_pop_indexes.Get(i)->GetIndex()->GetId() == (index_gid_t)(IndexTypes::FIRST_POP + account_id *2))
			{
				Index *index1 = m_pop_indexes.Get(i)->GetIndex();
				Index *index2= m_pop_indexes.Get(i+1)->GetIndex();
				m_pop_indexes.Delete(i,2);
				OpStatus::Ignore(RemoveIndex(index1, FALSE));
				OpStatus::Ignore(RemoveIndex(index2, FALSE));
				break;
			}
		}
	}

	// Remove the main index for this account
	if((index = GetIndexById(index_id)) != NULL)
	{
		OpStatus::Ignore(RemoveMessagesInIndex(index));
		OpStatus::Ignore(RemoveIndex(index, FALSE));

		OpStatus::Ignore(RemoveCategory(index_id));

		if (MessageEngine::GetInstance()->GetAccountManager()->GetMailNewsAccountCount() == 0)
		{
			g_input_manager->InvokeAction(OpInputAction::ACTION_RESET_MAIL_PANEL);
		}
	}
}

OP_STATUS Indexer::FindIndexWord(const uni_char *word, OpINT32Set &result)
{
	OpINT32Vector temp_result;

	// Do the search
	RETURN_IF_ERROR(m_index_table.Search(word, &temp_result));

	// Add valid ids to sorted result (prevents invalid ids from turning up after a crash)
	for (unsigned i = 0; i < temp_result.GetCount(); i++)
	{
		// Avoid showing ghosts
		// If we have loaded all messages, we can check if the message is in store, before displaying it
		// Otherwise we don't have any choice, we have to show it
		if (!g_m2_engine->GetStore()->HasFinishedLoading()  || (g_m2_engine->GetStore()->HasFinishedLoading() && GetStore()->MessageAvailable(temp_result.Get(i))))
			RETURN_IF_ERROR(result.Add(temp_result.Get(i)));
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::FindWords(const OpStringC& full_text, OpINTSortedVector& id_list, BOOL partial_match_allowed, Indexer::LexiconIdSpace id_space)
{
	OpINT32Vector copy;
	id_list.Clear();

	if (full_text.IsEmpty())
		return OpStatus::OK;

	// We don't have to use our own tokenizer, since search_engine has one built in
	RETURN_IF_ERROR(m_text_table->MultiSearch(full_text.CStr(), &copy, FALSE,
					partial_match_allowed ? 512 : 0));

	// return only wanted fields

	for (unsigned i = 0; i < copy.GetCount(); i++)
	{
		message_gid_t id = copy.Get(i);

		switch (id_space)
		{
			case Indexer::MESSAGE_ENTIRE_MESSAGE:
				break;
			case Indexer::MESSAGE_BODY:
				if ((INT32)id > Indexer::MESSAGE_SUBJECT)
				{
					continue;
				}
				break;
			case Indexer::MESSAGE_ALL_HEADERS:
				if ((INT32)id < Indexer::MESSAGE_SUBJECT)
				{
					continue;
				}
				break;
			default:
			{
				if ((INT32)id < id_space || (INT32)id > id_space + Indexer::MESSAGE_SUBJECT)
				{
					continue;
				}
			}
			break;
		}

		id = id % Indexer::MESSAGE_SUBJECT;

		id_list.Insert(id);
	}

	// add matching Message-Id as well

	switch (id_space)
	{
		case Indexer::MESSAGE_ENTIRE_MESSAGE:
		case Indexer::MESSAGE_ALL_HEADERS:
			{
				OpString8 message_id;

				RETURN_IF_ERROR(message_id.Set(full_text.CStr()));
				message_gid_t id = GetStore()->GetMessageByMessageId(message_id);
				if (id && !id_list.Contains(id))
				{
					id_list.Insert(id);
				}
			}
			break;
		default:
			break;
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::MessageAdded(Index* index, message_gid_t message_id, BOOL setting_keyword)
{
	OpINT32Vector vector;

	IndexTypes::ModelFlags flags = IndexTypes::MODEL_FLAG_READ;
	IndexTypes::Id id = (IndexTypes::Id)0;

	switch (index->GetId())
	{
	case IndexTypes::HIDDEN:
		flags = IndexTypes::MODEL_FLAG_HIDDEN;
		id = IndexTypes::HIDDEN;
		break;
	case IndexTypes::SPAM:
		flags = IndexTypes::MODEL_FLAG_SPAM;
		id = IndexTypes::SPAM;
		break;
	case IndexTypes::TRASH:
		flags = IndexTypes::MODEL_FLAG_TRASH;
		id = IndexTypes::TRASH;
		break;
	case IndexTypes::UNREAD:
		flags = IndexTypes::MODEL_FLAG_READ;
		id = IndexTypes::UNREAD;
		break;
	default:
		break;
	}

	vector.Add(index->GetId());

	if (index->IsVisible() || id)
	{
		Index* unread = (Index*)GetIndexById(IndexTypes::UNREAD);
		if (unread->Contains(message_id))
		{
			if (id)
			{
				Index* current;
				for (INT32 it = -1; (current = GetRange(0, IndexTypes::LAST, it)) != NULL; )
				{
					if (current != index)
					{
						if (id != IndexTypes::UNREAD && current->Contains(message_id))
						{
							if (!current->MessageHidden(message_id, id)
								&& (current->GetModelFlags()&(1<<flags))==0 )
							{
								current->DecUnread();
								vector.Add(current->GetId());
							}
						}
						else if (id == IndexTypes::UNREAD && current->Contains(message_id))
						{
							if (!current->MessageHidden(message_id, IndexTypes::UNREAD))
							{
								current->IncUnread();
								vector.Add(current->GetId());
							}
						}
					}
				}
			}
			if (!index->MessageHidden(message_id))
			{
				index->IncUnread();
			}
		}
	}

	// Add to AutoFilter if necessary
	if (!m_no_autofilter_update && index->GetAutoFilterFile())
	{
		Message message;

		if (OpStatus::IsSuccess(GetStore()->GetMessage(message, message_id)) &&
			OpStatus::IsSuccess(GetStore()->GetMessageData(message)))
		{
			UpdateAutoFilter(&message, index, FALSE);
			MessageEngine::GetInstance()->OnMessageChanged(message_id);
		}
	}

	if (index->GetId() >= IndexTypes::FIRST_FOLDER && index->GetId() < IndexTypes::LAST_FOLDER)
		MessageEngine::GetInstance()->OnMessageChanged(message_id); // update icon in indexmodels

	// Check for hidden messages
	if (index->GetHideFromOther())
	{
		Index* hidden = GetIndexById(IndexTypes::HIDDEN);
		if (hidden->Contains(message_id))
			MessageEngine::GetInstance()->OnMessageHidden(message_id, TRUE);
	}

	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
    {
		if (!setting_keyword && index->GetKeyword().HasContent())
			RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->KeywordAdded(this, message_id, index->GetKeyword()));

		for (UINT32 j = 0; j < vector.GetCount(); j++)
		{
			RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexChanged(this, vector.Get(j)));
		}
	}
	return OpStatus::OK;
}


OP_STATUS Indexer::MessageRemoved(Index* index, message_gid_t message_id, BOOL setting_keyword)
{
	// If this message is completely being removed, do nothing here
	if (message_id == m_removing_message)
		return OpStatus::OK;

	OpINT32Vector vector;

	IndexTypes::ModelFlags flags = IndexTypes::MODEL_FLAG_READ;
	IndexTypes::Id id = (IndexTypes::Id)0;

	switch (index->GetId())
	{
	case IndexTypes::HIDDEN:
		flags = IndexTypes::MODEL_FLAG_HIDDEN;
		id = IndexTypes::HIDDEN;
		break;
	case IndexTypes::SPAM:
		flags = IndexTypes::MODEL_FLAG_SPAM;
		id = IndexTypes::SPAM;
		break;
	case IndexTypes::TRASH:
		flags = IndexTypes::MODEL_FLAG_TRASH;
		id = IndexTypes::TRASH;
		break;
	case IndexTypes::UNREAD:
		flags = IndexTypes::MODEL_FLAG_READ;
		id = IndexTypes::UNREAD;
		break;
	default:
		break;
	}

	vector.Add(index->GetId());

	if (index->IsVisible() || id)
	{
		Index* unread = (Index*)GetIndexById(IndexTypes::UNREAD);
		if (id == IndexTypes::UNREAD || unread->Contains(message_id))
		{
			if (id)
			{
				Index* current;
				for (INT32 it = -1; (current = GetRange(0, IndexTypes::LAST, it)) != NULL; )
				{
					if (current && current != index)
					{
						if (id != IndexTypes::UNREAD && current->Contains(message_id))
						{
							if (!current->MessageHidden(message_id, id)
								&& (current->GetModelFlags()&(1<<flags))==0 )
							{
								current->IncUnread();
								vector.Add(current->GetId());
							}
						}
						else if (id == IndexTypes::UNREAD && current->Contains(message_id))
						{
							if (!current->MessageHidden(message_id, IndexTypes::UNREAD))
							{
								current->DecUnread();
								vector.Add(current->GetId());
							}
						}
					}
				}
			}
			if (!index->MessageHidden(message_id))
			{
				index->DecUnread();
			}
		}
	}

	// Remove from AutoFilter if necessary
	if (!m_no_autofilter_update && index->GetAutoFilterFile())
	{
		Message message;

		if (OpStatus::IsSuccess(GetStore()->GetMessage(message, message_id)) &&
			OpStatus::IsSuccess(GetStore()->GetMessageData(message)))
		{
			UpdateAutoFilter(&message, index, TRUE);
			MessageEngine::GetInstance()->OnMessageChanged(message_id);
		}
	}

	if (index->GetId() >= IndexTypes::FIRST_FOLDER && index->GetId() < IndexTypes::LAST_FOLDER)
		MessageEngine::GetInstance()->OnMessageChanged(message_id); // update icon in indexmodels

	// Check for hidden messages
	if (index->GetHideFromOther())
	{
		Index* hidden = GetIndexById(IndexTypes::HIDDEN);
		if (!hidden->Contains(message_id))
			MessageEngine::GetInstance()->OnMessageHidden(message_id, FALSE);
	}

	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
	{
		if (!setting_keyword && index->GetKeyword().HasContent())
			RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->KeywordRemoved(this, message_id, index->GetKeyword()));

		for (UINT32 j = 0; j < vector.GetCount(); j++)
			RETURN_IF_ERROR(m_indexer_listeners.Get(idx)->IndexChanged(this, vector.Get(j)));
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::IntersectionIndexes(Index &result, Index *first, Index *second, int limit)
{
	UINT32 first_end;
	UINT32 second_end;

	message_gid_t message;

	// make sure we have it all in memory
	RETURN_IF_ERROR(first->PreFetch());
	RETURN_IF_ERROR(second->PreFetch());

	first_end = first->MessageCount();
	second_end = second->MessageCount();

	if (first_end > second_end)
	{
		// make sure the first one is smallest
		Index* smallest = second;
		second = first;
		first = smallest;
		first_end = second_end;
	}

	for (INT32SetIterator it(first->GetIterator()); it; it++)
	{
		message = it.GetData();

		if (message && second->Contains(message))
		{
			RETURN_IF_ERROR(result.NewMessage(message));
		}
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::UnionIndexes(Index &result, Index *first, Index *second, int limit)
{
	// make sure we have it all in memory
	RETURN_IF_ERROR(first->PreFetch());
	RETURN_IF_ERROR(second->PreFetch());

	for (INT32SetIterator it(first->GetIterator()); it; it++)
	{
		message_gid_t message = it.GetData();
		RETURN_IF_ERROR(result.NewMessage(message));
	}

	for (INT32SetIterator it(second->GetIterator()); it; it++)
	{
		message_gid_t message = it.GetData();
		RETURN_IF_ERROR(result.NewMessage(message));
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::ComplementIndexes(Index &result, Index *first, Index *second, int limit)
{
	UINT32 first_end;
	UINT32 second_end;

	message_gid_t message;

	// make sure we have it all in memory
	RETURN_IF_ERROR(first->PreFetch());
	RETURN_IF_ERROR(second->PreFetch());

	first_end = first->MessageCount();
	second_end = second->MessageCount();

	if (first_end > second_end)
	{
		// make sure the first one is smallest
		Index* smallest = second;
		second = first;
		first = smallest;
		first_end = second_end;
	}

	for (INT32SetIterator it(first->GetIterator()); it; it++)
	{
		message = it.GetData();

		if (message && !second->Contains(message))
		{
			RETURN_IF_ERROR(result.NewMessage(message));
		}
	}

	return OpStatus::OK;
}

Index* Indexer::GetThreadIndex(message_gid_t message_id)
{
	Index *index = NULL;

	for (INT32 it = -1; (index = GetRange(IndexTypes::FIRST_THREAD, IndexTypes::LAST_THREAD, it)) != NULL; )
	{
		if (index->Contains(message_id))
		{
			return index;
		}
	}
	return NULL;
}

OP_STATUS Indexer::CreateThreadIndex(const OpINTSet& thread_ids, Index*& index, BOOL save_to_file)
{
	// First get the title of the first message in the thread
	Header::HeaderValue title;
	OpString title_str;
	Message root_message;
	if (OpStatus::IsError(MessageEngine::GetInstance()->GetStore()->GetMessage(root_message,thread_ids.GetByIndex(0))) ||
		OpStatus::IsError(root_message.GetHeaderValue(Header::SUBJECT, title)))
		RETURN_IF_ERROR(g_languageManager->GetString(Str::M_MAIL_ITEM_GOTO_THREAD, title_str));
	else
		RETURN_IF_ERROR(title_str.Set(title));

	// Create a new index with the right id and title
	RETURN_IF_ERROR(AddFolderIndex(m_next_thread_id, title_str, save_to_file));
	index = GetIndexById(m_next_thread_id);

	m_next_thread_id++;

	// Add all messages in the thread to the index
	for (unsigned j = 0; j < thread_ids.GetCount(); j++)
	{
		RETURN_IF_ERROR(index->NewMessage(thread_ids.GetByIndex(j)));
	}
	// this will override the default sorting intentionally
	index->SetModelFlags(UINT_MAX);
	index->DisableModelFlag(IndexTypes::MODEL_FLAG_TRASH);
	index->DisableModelFlag(IndexTypes::MODEL_FLAG_DUPLICATES);
	index->SetModelType(IndexTypes::MODEL_TYPE_THREADED, FALSE);
	index->SetModelSort(IndexTypes::MODEL_SORT_BY_SENT);
	index->SetModelSortAscending(g_pcm2->GetIntegerPref(PrefsCollectionM2::DefaultMailSortingAscending) == 1);
	index->SetModelGroup(IndexTypes::GROUP_BY_NONE);

	return OpStatus::OK;
}

OP_STATUS Indexer::CreateFolderGroupIndex(Index*& folder_index, index_gid_t parent_id)
{
	OpString name;
	FolderIndexGroup* group = OP_NEW(FolderIndexGroup,(m_next_folder_group_id++));
	if (!group)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_folder_indexgroups.Add(group));
	folder_index = group->GetIndex();
	folder_index->SetType(IndexTypes::UNIONGROUP_INDEX);
	if (parent_id && GetIndexById(parent_id))
		folder_index->SetAccountId(GetIndexById(parent_id)->GetAccountId());
	folder_index->SetParentId(parent_id);
	folder_index->SetVisible(TRUE);
	RETURN_IF_ERROR(g_languageManager->GetString(Str::SI_NEW_FOLDER_BUTTON_TEXT, name));
	RETURN_IF_ERROR(folder_index->SetName(name.CStr()));
	return OpStatus::OK;
}

OP_STATUS Indexer::AddToActiveThreads(Message& message)
{
	Index *index = NULL;
	message_gid_t parent_id = message.GetId();
	
	// If the is an index for this thread, the parent of this message is in it
	// check if it's there
	parent_id = MessageEngine::GetInstance()->GetStore()->GetParentId(parent_id);
	if (!parent_id)
		return OpStatus::OK;
	index = GetThreadIndex(parent_id);
	if (!index)
		return OpStatus::OK;

	return index->NewMessage(message.GetId());
}


OP_STATUS Indexer::UpdateCategory(index_gid_t index_id)
{
	MailCategory* category = GetCategory(index_id);
	
	if (!category)
		return OpStatus::ERR;

	UnionIndexGroup*& group = category->m_index_group;

	if (group)
		return OpStatus::OK;

	OpString name;
	
	switch (index_id)
	{
		case IndexTypes::CATEGORY_ACTIVE_THREADS:
			group = OP_NEW(IndexGroupWatched, (index_id, IndexTypes::FIRST_THREAD, IndexTypes::LAST_THREAD));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_FOLLOWED_THREADS, name));
			break;
		
		case IndexTypes::CATEGORY_ACTIVE_CONTACTS:
			group = OP_NEW(IndexGroupWatched, (index_id, IndexTypes::FIRST_CONTACT, IndexTypes::LAST_CONTACT));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::S_MAIL_FOLLOWED_CONTACTS, name));
			break;

		case IndexTypes::CATEGORY_MAILING_LISTS:
			group = OP_NEW(IndexGroupMailingLists, (index_id));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_MAILINGLISTS, name));
			break;

		case IndexTypes::CATEGORY_LABELS:
			group = OP_NEW(FolderIndexGroup, (index_id));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_LABELS, name));
			break;

		case IndexTypes::CATEGORY_ATTACHMENTS:
			group = OP_NEW(IndexGroupRange, (index_id, IndexTypes::FIRST_ATTACHMENT, IndexTypes::LAST_ATTACHMENT));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_ATTACHMENTS, name));
			break;

		case IndexTypes::CATEGORY_MY_MAIL:
			// fails to produce any mail, but that is ok, since we don't use it for viewing mail, but for using the index for name and icon
			group = OP_NEW(IndexGroupRange, (index_id, IndexTypes::RECEIVED, IndexTypes::RECEIVED));
			RETURN_IF_ERROR(g_languageManager->GetString(Str::DI_IDSTR_M2_COL_MYMAIL, name));
			break;
	}

	if (!group)
		return OpStatus::ERR;

	if (group->GetIndex())
	{
		group->GetIndex()->SetName(name.CStr());
		switch (index_id)
		{
			case IndexTypes::CATEGORY_ACTIVE_THREADS:
			case IndexTypes::CATEGORY_MAILING_LISTS:
					group->GetIndex()->SetModelType(IndexTypes::MODEL_TYPE_THREADED);
		}
	}

	return OpStatus::OK;
}


OP_STATUS Indexer::SpamFilter(Message *message, BOOL& is_spam, OpString& body_text)
{
	if (!m_debug_spam_filter_enabled)
	{
		return OpStatus::OK;
	}

	if (message->IsFlagSet(Message::IS_DELETED)		||
		message->IsFlagSet(Message::IS_IMPORTED)	||
		message->IsFlagSet(Message::IS_NEWS_MESSAGE)||
		message->IsFlagSet(Message::IS_OUTGOING)	||
		message->IsFlagSet(Message::IS_NEWSFEED_MESSAGE))
	{
		return OpStatus::OK;
	}

	// OnMessageBodyChanged() will call this function again, but no point in checking if already in spam
	if (GetIndexById(IndexTypes::SPAM)->Contains(message->GetId()))
	{
		is_spam = TRUE;
		return OpStatus::OK;
	}

	if (message->IsConfirmedSpam())
	{
		is_spam = TRUE;
		return SilentMarkMessageAsSpam(message->GetId());
	}

	double auto_score;
	AutoFilter(message, body_text, GetIndexById(IndexTypes::SPAM), auto_score);

	if (auto_score > 0.995 || auto_score < 0.98)
	{
		// the message is either filtered by AutoFilter or it's unlikely to be a spam
		is_spam = auto_score > 0.995;
		return OpStatus::OK;
	}

	int score = GetSpamLevel();

	if (message->GetId() == 0 || score >= 1000)
	{
		return OpStatus::OK;
	}

	// subject stuff:

	OpString address;	///< e-mail address

	Account *account;

	UINT16 account_id = message->GetAccountId();
	AccountManager *manager = MessageEngine::GetInstance()->GetAccountManager();
	RETURN_IF_ERROR(manager->GetAccountById(account_id,account));

	if (account)
	{
		RETURN_IF_ERROR(account->GetEmail(address));
	}

	int recipients = 0;

	// + sent to me as first recipient
	Header *header = message->GetHeader(Header::TO);
	if (header)
	{
		const Header::From* to = header->GetFirstAddress();
		if (to)
		{
			if (to->CompareAddress(address)==0)
			{
				score += 10;
			}

			// + sent to me as other to recipient
			to = (Header::From*)(to->Suc());
			while (to)
			{
				if (to->HasAddress() &&
					to->CompareAddress(address)==0)
				{
					score += 10;
					break;
				}
				to = (Header::From*)(to->Suc());

				recipients++;
			}
		}
	}
	else
	{
		// no to address !?
		score -= 10;
	}

	// + sent to me as other cc recipient
	header = message->GetHeader(Header::CC);
	if (header)
	{
		const Header::From* to = header->GetFirstAddress();
		while (to)
		{
			if (to->CompareAddress(address)==0)
			{
				score += 10;
				break;
			}
			to = (Header::From*)(to->Suc());

			recipients++;
		}
	}

	if (recipients > 14)
	{
		score -= 10;
	}

	// + from mailing list

	OpString name;
	BOOL dummy;
	address.Empty();

	BrowserUtils* browser_utils = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils();

	GetListId(message, dummy, address, name);
	if (address.HasContent() && m_debug_contact_list_enabled)
	{
		if (browser_utils->IsContact(address))
		{
			score += 300;
		}
		else
		{
			// usually no valid List-Id header in spam
			score += 20;
		}
	}

	// our domain name in message id
	header = message->GetHeader(Header::MESSAGEID);
	if (header && account)
	{
		OpString fqdn;
		Header::HeaderValue msgid;

		RETURN_IF_ERROR(header->GetValue(msgid));
		RETURN_IF_ERROR(account->GetFQDN(fqdn));

		if (!fqdn.IsEmpty() && msgid.FindI(fqdn) != KNotFound)
		{
			score += 50;
		}
		else if (msgid.IsEmpty() || msgid.FindI("@") == KNotFound)
		{
			score -= 10; // XX% of spam has invalid Message-ID
		}
	}
	else if (header == NULL)
	{
		score -= 10; // XX% of spam has no Message-ID
	}

	Header::HeaderValue subject;
	RETURN_IF_ERROR(message->GetHeaderValue(Header::SUBJECT, subject));

	const uni_char *subject_words[] =
	{
		UNI_L("$"),
		UNI_L("dollar"),
		UNI_L("euro"),
		UNI_L("money"),
		UNI_L("        "),
		UNI_L("sex"),
		UNI_L("adult"),
		UNI_L("porn"),
        UNI_L("free"),
        UNI_L("cable")
	};
	int subject_words_count = 10;

	for (int i = 0; i < subject_words_count; i++)
	{
		if (subject.FindI(subject_words[i]) != KNotFound)
		{
			score -= 20;
		}
	}

	if (subject.FindI(UNI_L("ADV:")) != KNotFound ||
	    subject.FindI(UNI_L("ATTN:")) != KNotFound)
	{
		score -= 50;
	}

	const uni_char *sub = subject.CStr();
	if (sub)
	{
		sub = uni_strpbrk(sub,UNI_L("0123456789"));
		if (sub)
		{
			// - contains numbers in subject
			score -= 10;
		}
	}

	// - from strange e-mail address
	header = message->GetHeader(Header::FROM);
	if (header == NULL)
	{
		score -= 10; // No From field is pretty suspect

		header = message->GetHeader(Header::SENDER);
		if (header == NULL)
		{
			header = message->GetHeader(Header::REPLYTO);
		}
	}
	if (header == NULL)
	{
		score -= 10;
	}
	else
	{
		const Header::From* from = header->GetFirstAddress();
		if (from && from->HasAddress())
		{
			{
				// + from contact
				if (m_debug_contact_list_enabled &&
					browser_utils->IsContact(from->GetAddress()))
				{
					score += 300;
				}
				else
				{
					const uni_char *add = from->GetAddress().CStr();
					add = uni_strpbrk(add,UNI_L("0123456789"));
					if (add)
					{
						// - contains numbers in e-mail sender address. Only slightly suspect.
						score -= 10;
					}
				}
			}
		}
		else
		{
			// no sender ?!
			score -= 10;
		}
	}

	if (score < 0)
	{
		OpINT32Vector spam_message;
		RETURN_IF_ERROR(spam_message.Add(message->GetId()));
		RETURN_IF_ERROR(MessageEngine::GetInstance()->MarkAsSpam(spam_message, TRUE));
		is_spam = TRUE;
	}
	else
	{
		is_spam = FALSE;
	}
	return OpStatus::OK;
}


/***********************************************************************************
 ** Call when parent for an index changes, to correct parent/child relations
 **
 ** Indexer::OnParentChanged
 ** @param index_id ID of index of which parent changed
 ** @param old_parent_id
 ** @param new_parent_id
 ***********************************************************************************/
OP_STATUS Indexer::OnParentChanged(index_gid_t index_id, index_gid_t old_parent_id, index_gid_t new_parent_id)
{
	if (!index_id || old_parent_id == new_parent_id)
		return OpStatus::OK;
	
	IndexChilds* children;
	
	if (old_parent_id)
	{
		if (OpStatus::IsSuccess(m_index_childs.GetData(old_parent_id, &children)))
			children->RemoveChild(index_id);
	}

	if (new_parent_id)
	{
		if (OpStatus::IsError(m_index_childs.GetData(new_parent_id, &children)))
		{
			children = OP_NEW(IndexChilds, (new_parent_id));

			if (!children || OpStatus::IsError(m_index_childs.Add(new_parent_id, children)))
			{
				OP_DELETE(children);
				return OpStatus::ERR_NO_MEMORY;
			}
		}
		
		if (children->GetChildren().Find(index_id) < 0)
			RETURN_IF_ERROR(children->AddChild(index_id));
	}

	for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
	{
		OpStatus::Ignore(m_indexer_listeners.Get(idx)->IndexParentIdChanged(this, index_id, old_parent_id, new_parent_id));
	}

	if (IsCategory(new_parent_id) && !GetCategoryVisible(new_parent_id))
	{		
		SetCategoryVisible(new_parent_id, TRUE);
	}

	return SaveRequest();
}


/***********************************************************************************
 ** Get a list of headers needed for each new message
 **
 ** Indexer::GetNeededHeaders
 ** @param headers Output: space-separated list of needed headers
 ***********************************************************************************/
OP_STATUS Indexer::GetNeededHeaders(OpString8& headers)
{
	for (String8HashIterator<OpString8> it(m_needed_headers); it; it++)
	{
		if (headers.HasContent())
			RETURN_IF_ERROR(headers.Append(" "));
		RETURN_IF_ERROR(headers.Append(it.GetKey()));
	}
	
	return OpStatus::OK;
}


/***********************************************************************************
 ** Set a keyword for an index (empty for no keyword)
 **
 ** Indexer::SetKeyword
 **
 ***********************************************************************************/
OP_STATUS Indexer::SetKeyword(Index* index, const OpStringC8& keyword, BOOL alert_listeners)
{
	// Setting the keyword is done in Indexer (not in Index), because Indexer
	// maintains a table of all keywords (m_keywords / m_keywords_by_id). This function
	// updates both the index and that table.

	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	// Check if the keyword is the same as before
	if (!keyword.Compare(index->GetKeyword()) && (keyword.HasContent() || !index->GetDefaultKeyword()))
		return OpStatus::OK;

	// Save existing keyword if necessary
	OpString8 old_keyword;

	if (alert_listeners)
		RETURN_IF_ERROR(old_keyword.Set(index->GetKeyword()));

	// Change old keyword
	if (index->GetKeyword().CStr())
	{
		Keyword* keyword;
		if (OpStatus::IsSuccess(m_keywords.GetData(index->GetKeyword().CStr(), &keyword)))
			keyword->index = 0;
	}

	// Set keyword
	RETURN_IF_ERROR(index->SetKeyword(keyword.HasContent() ? keyword : OpStringC8(index->GetDefaultKeyword())));

	// If the keyword has content, add the index to the table
	if (index->GetKeyword().HasContent())
	{
		int id = GetKeywordID(index->GetKeyword());
		if (id < 0)
			return OpStatus::ERR;

		Keyword* keyword = m_keywords_by_id.Get(id);
		if (keyword)
			keyword->index = index->GetId();
	}

	if (alert_listeners)
	{
		for (UINT32 idx = 0; idx < m_indexer_listeners.GetCount(); idx++)
			m_indexer_listeners.Get(idx)->IndexKeywordChanged(this, index->GetId(), old_keyword, index->GetKeyword());
	}

	return OpStatus::OK;
}

/***********************************************************************************
 **
 ** Indexer::SilentMarkMessageAsSpam
 **
 ***********************************************************************************/
OP_STATUS Indexer::SilentMarkMessageAsSpam(message_gid_t message_id)
{
	OpMisc::Resetter<bool> filter_resetter(m_no_autofilter_update, false);
	m_no_autofilter_update = true;
	RETURN_IF_ERROR(GetIndexById(IndexTypes::SPAM)->NewMessage(message_id));
	MessageEngine::GetInstance()->OnMessageChanged(message_id); 
	return OpStatus::OK;
}

/***********************************************************************************
 **
 ** Indexer::AddToPinBoard
 **
 ***********************************************************************************/
OP_STATUS Indexer::AddToPinBoard(message_gid_t message_id, BOOL flagged)
{
	Index *pin_board = GetIndexById(IndexTypes::PIN_BOARD);
	if (!pin_board)
		return OpStatus::ERR;
	if (flagged)
		pin_board->NewMessage(message_id);	
	else if (pin_board->Contains(message_id))
		pin_board->RemoveMessage(message_id);	
	MessageEngine::GetInstance()->OnMessageChanged(message_id); 
	return OpStatus::OK;
}

/***********************************************************************************
 ** Call when a new index is added, checks if headers are needed on new messages
 **
 ** Indexer::HandleHeadersOnNewIndex
 **
 ***********************************************************************************/
OP_STATUS Indexer::HandleHeadersOnNewIndex(Index* index)
{
	for (unsigned i = 0; i < index->GetSearchCount(); i++)
	{
		IndexSearch* search = index->GetSearch(i);

		// We are only interested in non-regex header searches
		if (!search ||
			search->SearchBody() != SearchTypes::HEADERS ||
			search->GetOption() == SearchTypes::REGEXP)
			continue;

		OpStringC search_text = search->GetSearchText();

		// Check for headers by finding ':'
		int sep_pos = search_text.FindFirstOf(':');
		if (sep_pos == KNotFound)
			continue;
		
		OpAutoPtr<OpString8> header (OP_NEW(OpString8, ()));
		if (!header.get() || !header->Reserve(sep_pos))
			return OpStatus::ERR_NO_MEMORY;
		
		// Copy relevant characters of header
		int j;
		for (j = 0; j < sep_pos && ((43 <= search_text[j] && search_text[j] <= 57) || (59 <= search_text[j] && search_text[j] <= 91) || (93 <= search_text[j] && search_text[j] <= 126)); j++)
			header->CStr()[j] = op_toupper(search_text[j]);
		header->CStr()[j] = 0;

		// Check if this was a valid header
		if (j != sep_pos)
			continue;

		// Check if we have to add the header to the needed headers
		if (!m_needed_headers.Contains(header->CStr()))
		{
			RETURN_IF_ERROR(m_needed_headers.Add(header->CStr(), header.get()));
			header.release();
		}
	}
	
	return OpStatus::OK;
}


/***********************************************************************************
 ** Convenience function
 **
 ** Indexer::GetStore
 **
 ***********************************************************************************/
inline Store* Indexer::GetStore()
{
	return MessageEngine::GetInstance()->GetStore();
}


/***********************************************************************************
 ** Set the keywords on a message (also removes from existing keywords if it had
 ** any)
 **
 ** Indexer::SetKeywordsOnMessage
 **
 ***********************************************************************************/
OP_STATUS Indexer::SetKeywordsOnMessage(message_gid_t id, const OpINTSortedVector* keywords, bool new_message)
{
	int  flags        = GetStore()->GetMessageFlags(id);
	BOOL has_keywords = FALSE;

	if (flags & (1 << Message::HAS_KEYWORDS) && !new_message)
	{
		// If the message already had keywords, we'll need to walk through all the
		// keyworded indexes, removing the message if necessary
		for (unsigned i = 0; i < m_keywords_by_id.GetCount(); i++)
		{
			// If the message still has the keyword, it doesn't need to be removed
			if (keywords && keywords->Contains(i))
				continue;

			// Check if the keyword has an associated index
			Keyword* keyword = m_keywords_by_id.Get(i);
			if (keyword->index == 0)
				continue;

			Index* index = GetIndexById(keyword->index);
			if (!index)
				continue;

			// Remove message from associated index
			RETURN_IF_ERROR(index->PreFetch());
			index->RemoveMessage(id, TRUE);
		}
	}

	// Add the message to all indexes with specified keywords
	if (keywords)
	{
		for (unsigned i = 0; i < keywords->GetCount(); i++)
		{
			if (keywords->GetByIndex(i) < 0)
				continue;

			Keyword* keyword = m_keywords_by_id.Get(keywords->GetByIndex(i));

			// Check if we should create an index for this keyword
			if (keyword && keyword->index == 0 && keyword->keyword[0] != '$')
			{
				OpAutoPtr<Index> index (OP_NEW(Index, ()));
				if (!index.get())
					return OpStatus::ERR_NO_MEMORY;

				OpString name;

				index->SetType(IndexTypes::FOLDER_INDEX);
				RETURN_IF_ERROR(index->SetKeyword(keyword->keyword));
				RETURN_IF_ERROR(name.Set(index->GetKeyword().CStr()));
				RETURN_IF_ERROR(index->SetName(name.CStr()));
				index->SetSaveToDisk(TRUE);

				RETURN_IF_ERROR(NewIndex(index.get()));

				keyword->index = index->GetId();
				index.release();
			}

			// Add message to index with keyword
			if (keyword && keyword->index != 0)
			{
				Index* index = GetIndexById(keyword->index);
				if (index)
				{
					RETURN_IF_ERROR(index->NewMessage(id, FALSE, TRUE));
					has_keywords = TRUE;
				}
			}
		}
	}

	// Make sure that flags on message are correct
	GetStore()->SetMessageFlag(id, Message::HAS_KEYWORDS, has_keywords);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Get a vector with children for a certain index
 **
 ** Indexer::GetChildren
 ** @param index_id Which index to get the children for
 ** @param only_direct_children Whether to only include direct
 ** @return Vector with children, or NULL if it doesn't exist
 **
 ***********************************************************************************/
OP_STATUS Indexer::GetChildren(index_gid_t index_id, OpINT32Vector& children, BOOL only_direct_children)
{
	IndexChilds* index_childs;

	children.Clear();
	RETURN_IF_ERROR(children.Add(index_id));

	// Copy children and all children of children into the 'children' vector
	for (unsigned pos = 0; pos < children.GetCount(); pos++)
	{
		if (OpStatus::IsSuccess(m_index_childs.GetData(children.Get(pos), &index_childs)))
		{
			for (unsigned i = 0; i < index_childs->GetChildren().GetCount(); i++)
			{
				RETURN_IF_ERROR(children.Add(index_childs->GetChildren().Get(i)));
			}
			if (only_direct_children)
				break;
		}
	}

	children.Remove(0);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Check if an index has children
 **
 ** Indexer::HasChildren
 ** @param index_id Index to check
 ** @return Whether the index has children
 ***********************************************************************************/
BOOL Indexer::HasChildren(index_gid_t index_id)
{
	IndexChilds* index_childs;

	return OpStatus::IsSuccess(m_index_childs.GetData(index_id, &index_childs)) && index_childs->GetChildren().GetCount() > 0;
}

Index* Indexer::GetFeedFolderIndexByName(index_gid_t parent_id, const OpStringC& name)
{
	Index* index = NULL;
	OpINT32Vector children;
	if (OpStatus::IsSuccess(GetChildren(parent_id, children, TRUE)))
	{
		Index* index_i;
		for (UINT32 i = 0; i < children.GetCount(); i++)
		{
			index_i = GetIndexById(children.Get(i));
			if (index_i && index_i->GetName() == name)
			{
				return index_i;
			}
		}
	}
	RETURN_VALUE_IF_ERROR(CreateFolderGroupIndex(index, parent_id), NULL);
	RETURN_VALUE_IF_ERROR(index->SetName(name.CStr()), NULL);

	return index;
}

OP_STATUS Indexer::SetSearchInIndex(index_gid_t label_index_id, index_gid_t search_in_id)
{
	// 1. a label searching in all messages searches in a specific index 
	// 2. a label stops searching in a specific index and searches in all messages (search_in_id == 0)
	// 3. a label changes index to search in, easiest scenario

	Index* label_index = g_m2_engine->GetIndexById(label_index_id);
	IndexGroup* intersection_group;
	
	if (label_index->GetSearchIndexId() == 0 && search_in_id != 0)
	{
		 intersection_group = OP_NEW(IntersectionIndexGroup, (label_index_id, label_index));

		if (!intersection_group || OpStatus::IsError(m_folder_indexgroups.Add(intersection_group)))
		{
			OP_DELETE(intersection_group);
			return OpStatus::ERR_NO_MEMORY;
		}
		
		label_index->SetType(IndexTypes::INTERSECTION_INDEX);
		RETURN_IF_ERROR(label_index->Empty());

	}
	else
	{
		// search for existing intersection group
		intersection_group = NULL; 
		for (UINT32 i = 0; i < m_folder_indexgroups.GetCount(); i++)
		{
			if (m_folder_indexgroups.Get(i)->GetIndex()->GetId() == label_index_id)
			{
				intersection_group = m_folder_indexgroups.Get(i);
				break;
			}
		}

		if (!intersection_group)
			return OpStatus::ERR;
	
		if (search_in_id == 0)
		{
			Index* search_index = GetIndexById(intersection_group->GetIndex()->GetSearchIndexId());

			// copy searches
			for (UINT32 i = 0; i < search_index->GetSearchCount(); i++)
				RETURN_IF_ERROR(label_index->AddSearch(*search_index->GetSearch(i)));

			RETURN_IF_ERROR(RemoveIndex(search_index));

			label_index->SetType(IndexTypes::FOLDER_INDEX);
			label_index->SetSearchIndexId(0);
			return m_folder_indexgroups.Delete(intersection_group);
		}

		intersection_group->Empty();
	}

	Index* search_index;
	if (label_index->GetSearchIndexId())
	{
		search_index = GetIndexById(label_index->GetSearchIndexId());
	}
	else
	{
		// create a new searching index
		search_index = OP_NEW(Index, ());
		if (!search_index)
			return OpStatus::ERR_NO_MEMORY;

		search_index->SetType(IndexTypes::SEARCH_INDEX);
			
		RETURN_IF_ERROR(NewIndex(search_index));

		// copy searches
		for (UINT32 i = 0; i < label_index->GetSearchCount(); i++)
		{
			RETURN_IF_ERROR(search_index->AddSearch(*label_index->GetSearch(i)));
			RETURN_IF_ERROR(label_index->RemoveSearch(0));
		}
		
		label_index->SetSearchIndexId(search_index->GetId());
	}

	RETURN_IF_ERROR(intersection_group->SetBase(search_index->GetId()));
	return intersection_group->AddIndex(search_in_id);
}

index_gid_t Indexer::GetSearchInIndex(index_gid_t label_index_id)
{
	for (UINT32 i = 0; i < m_folder_indexgroups.GetCount(); i++)
	{
		if (m_folder_indexgroups.Get(i)->GetIndex()->GetId() == label_index_id)
		{
			return static_cast<IntersectionIndexGroup*>(m_folder_indexgroups.Get(i))->GetIntersectingIndexid();
		}
	}
	return 0;
}

OP_STATUS MessageTokenizer::GetNextToken(OpString& next_token)
{
	// Contact johan@opera.com before changing here:

	OpString word;

	if (!m_initialized)
	{
		m_current_pos = 0;
		m_current_block = 0;
		m_initialized = TRUE;
		m_original_string_len = m_original_string.Length(); // cache or we're exponentially slow
	}

	for (int pos = m_current_pos ; pos < m_original_string_len; pos++)
	{
		uni_char current = m_original_string.CStr()[pos];
		uni_char next = m_original_string.CStr()[pos+1];

		BOOL end_of_cjk_word = EndOfCJKWord(current, next);
		BOOL in_spacers = m_spacers.InSet(current);

		if (end_of_cjk_word || in_spacers)
		{
			BOOL in_blocklimits = m_blocklimits.InSet(current);
			if (end_of_cjk_word || in_blocklimits)
			{
				if (m_current_block < m_current_pos)
				{
					// end of block (typically e-mail)

					next_token.Set(m_original_string.CStr() + m_current_block, pos - m_current_block + (end_of_cjk_word && !in_blocklimits ? 1 : 0));

					for (int backwards = next_token.Length(); backwards > 0; backwards--)
					{
						if (m_remove_from_end_of_block.InSet(next_token[backwards-1]))
						{
							// remove "..." at end of single words etc...
							next_token.CStr()[backwards-1] = 0;
						}
						else
						{
							break;
						}
					}

					if (m_spacers.InSet(next_token))
					{
						// here we return the block without touching
						// m_current_pos, so next time GetNextToken is called
						// we will return the last word in the block
						m_current_block = pos+1;
						return OpStatus::OK;
					}
				}
				m_current_block = pos+1;
			}
			// we have a delimiter
			if (pos == m_current_pos && in_spacers)
			{
				// we only have a special character
				if (m_current_block == m_current_pos)
				{
					m_current_block++;
				}
				m_current_pos++;
				continue;
			}

			BOOL add_extra_char = end_of_cjk_word && !in_spacers;

			RETURN_IF_ERROR(next_token.Set(m_original_string.CStr() + m_current_pos, pos - m_current_pos + (add_extra_char ? 1 : 0)));
			m_current_pos = pos + 1;
			return OpStatus::OK;
		}
	}

	next_token.Empty();
	return OpStatus::OK;
}

BOOL MessageTokenizer::EndOfCJKWord(uni_char current_char, uni_char next_char)
{
	if ((current_char>=0x3000 && current_char<=0xD7A3) ||
		(current_char>=0xF900 && current_char<=0xFAFF) ||
		(current_char>=0xFF01 && current_char<=0xFF64))
	{
		return TRUE;
	}

	if (next_char==0)
	{
		return TRUE;
	}

	if (current_char > 127) // outside us-ascii
	{
        switch (Unicode::IsLinebreakOpportunity(current_char, next_char, FALSE))
        {
        case LB_YES:
            return TRUE;
        case LB_NO:
            return FALSE;
        case LB_MAYBE: // one of the characters is of class SP or CM
            if (Unicode::GetLineBreakClass(next_char) == LB_CM)
                return FALSE;
            else
                return TRUE;
        }
	}
	return FALSE;
}

TokenSet MessageTokenizer::m_spacers(UNI_L(" =\r\n\t.,:;/()[]{}<>&\?!-\"\'`|@\\"));
TokenSet MessageTokenizer::m_blocklimits(UNI_L(" ,\r\n\t()[]{}<>\"\'`|"));
TokenSet MessageTokenizer::m_remove_from_end_of_block(UNI_L("=.,:;&\?!-@"));

TokenSet::TokenSet(const uni_char* tokens)
{
	if(tokens)
	{
		m_len = uni_strlen(tokens);
		m_tokens = OP_NEWA(uni_char, m_len);
		UINT32 pos = 0;
		for(UINT32 i = 0; i < m_len; i++)
		{
			pos = Search(tokens[i], 0, i, TRUE);
			for(UINT32 j = i; j > pos; j--)
				m_tokens[j] = m_tokens[j-1];
			m_tokens[pos] = tokens[i];
		}
	}
	else
	{
		m_tokens = NULL;
		m_len = 0;
	}
}

BOOL TokenSet::InSet(OpStringC& tokens)
{
	for(int i = tokens.Length() - 1; i >= 0; i--)
	{
		if(InSet(tokens[i]))
			return TRUE;
	}
	return FALSE;
}

INT32 TokenSet::Search(uni_char token, UINT32 start, UINT32 end, BOOL insertion_place)
{
	UINT32 mid = 0;
	while(start < end)
	{
		mid = (end-start) / 2 + start;
		if (token > m_tokens[mid])
			start = mid + 1;
		else if (token < m_tokens[mid])
			end = mid;
		else
			return mid;
	}
	if(insertion_place)
		return start;
	else
		return -1;
}


#endif //M2_SUPPORT
