/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "rssmodule.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/quick/Application.h"

#include "adjunct/m2/src/backend/rss/rss-protocol.h"

#include "modules/prefsfile/prefsfile.h"

// ***************************************************************************
//
//	RSSBackend
//
// ***************************************************************************

RSSBackend::RSSBackend(MessageDatabase& database)
  : ProtocolBackend(database),
	m_protocol(NULL),
	m_file(NULL),
	m_feed_index(0),
	m_no_dialogs(0)
{
}


RSSBackend::~RSSBackend()
{
	MessageEngine::GetInstance()->GetGlueFactory()->DeletePrefsFile(m_file);
}


OP_STATUS RSSBackend::MailCommand(URL& url)
{
	// If the account does not exist yet, this might be NULL.
	if (m_protocol.get() == 0)
		RETURN_IF_ERROR(SettingsChanged(FALSE)); // Initialize the account.

	if (m_protocol.get() != 0)
		m_protocol->OnProgress(url, OpTransferListener::TRANSFER_DONE);

	return OpStatus::OK;
}


OP_STATUS RSSBackend::UpdateFeed(const OpStringC& url, BOOL display_dialogs)
{
	if (!display_dialogs)
		m_no_dialogs++;

	// If the account does not exist yet, this might be NULL.
	if (m_protocol.get() == 0)
		RETURN_IF_ERROR(SettingsChanged(FALSE)); // Initialize the account.

	// Check if we already have the feed
	Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(GetAccountPtr(), url, 0, 0, FALSE, FALSE);
	if (index && GetRSSDialogs())
		g_application->GoToMailView(index->GetId());

	RETURN_IF_ERROR(m_protocol->FetchMessages(url));

	// Update progress.
	RETURN_IF_ERROR(m_progress.SetCurrentAction(ProgressInfo::UPDATING_FEEDS, m_progress.GetTotalCount() + 1));

	return OpStatus::OK;
}


void RSSBackend::FeedCheckingCompleted()
{
	if (m_no_dialogs)
		m_no_dialogs--;

	// Update progress.
	m_progress.UpdateCurrentAction();

	// Have we checked all feeds now?
	if (m_progress.GetCount() >= m_progress.GetTotalCount())
	{
		m_progress.EndCurrentAction(TRUE);
		// Give a notification
		MessageEngine::GetInstance()->GetMasterProgress().NotifyReceived(GetAccountPtr());
	}
}


OP_STATUS RSSBackend::Init(Account* account)
{
    if (!account)
        return OpStatus::ERR_NULL_POINTER;

    m_account = account;

	OpString file_path;
	RETURN_IF_ERROR(m_account->GetIncomingOptionsFile(file_path));

	GlueFactory* glue_factory = MessageEngine::GetInstance()->GetGlueFactory();
	if (!file_path.IsEmpty())
		m_file = glue_factory->CreatePrefsFile(file_path);

    return OpStatus::OK;
}


OP_STATUS RSSBackend::SettingsChanged(BOOL startup)
{
	// Init the protocol.
	m_protocol = OP_NEW(RSSProtocol, (*this));
	if (m_protocol.get() == 0)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_protocol->Init());
	return OpStatus::OK;
}


OP_STATUS RSSBackend::FetchMessages(BOOL enable_signalling)
{
	OP_NEW_DBG("RSSBackend::FetchMessages", "m2");
	OP_DBG(("retrieving messages with rss"));
    if (m_account == 0 || m_protocol.get() == 0)
        return OpStatus::ERR_NULL_POINTER;

	// Check which feeds we need to update
	Indexer* indexer = MessageEngine::GetInstance()->GetIndexer();
	OP_ASSERT(indexer != 0);

	Index* index;
	for (INT32 it = -1; (index = indexer->GetRange(IndexTypes::FIRST_NEWSFEED, IndexTypes::LAST_NEWSFEED, it)) != NULL; )
	{
		if ((index->GetAccountId() == m_account->GetAccountId()) && (index->GetSearch() != 0))
		{
			OpStatus::Ignore(UpdateFeedIfNeeded(index));
		}
	}

	return OpStatus::OK;
}


OP_STATUS RSSBackend::FetchMessage(const OpStringC8& internet_location, message_index_t index)
{
	RETURN_IF_ERROR(UpdateFeed(index));
    return OpStatus::OK;
}


OP_STATUS RSSBackend::SelectFolder(UINT32 index_id, const OpStringC16& folder)
{
	RETURN_IF_ERROR(UpdateFeedIfNeeded(index_id, folder));
	return OpStatus::OK;
}

OP_STATUS RSSBackend::RefreshAll()
{
    Indexer* indexer = MessageEngine::GetInstance()->GetIndexer();
    Index* index;
    for (INT32 it = -1; (index = indexer->GetRange(IndexTypes::FIRST_NEWSFEED, IndexTypes::LAST_NEWSFEED, it)) != NULL; )
    {
        // Force all feeds to update
        if ((index->GetAccountId() == m_account->GetAccountId()) && (index->GetSearch() != 0))
        {
            OpStatus::Ignore(UpdateFeed(index->GetId()));
        }
    }
    return OpStatus::OK;
}

OP_STATUS RSSBackend::StopFetchingMessages()
{
	if (m_protocol.get() == 0)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS ret = m_protocol->StopFetching();

	m_progress.EndCurrentAction(TRUE);

	return ret;
}


OP_STATUS RSSBackend::RemoveSubscribedFolder(UINT32 index_id)
{
	Index* index = MessageEngine::GetInstance()->GetIndexById(index_id);
	if (!index)
		return OpStatus::ERR_NULL_POINTER;

	// Remove both the messages inside this folder and the folder
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetIndexer()->RemoveMessagesInIndex(index));

	// Delete the search_engine file associated to the feed index:
	OpString filename;
	OpFile btree_file;
	RETURN_IF_ERROR(MailFiles::GetNewsfeedFilename(index_id,filename));
	RETURN_IF_ERROR(btree_file.Construct(filename.CStr()));
	RETURN_IF_ERROR(btree_file.Delete());
	return MessageEngine::GetInstance()->GetIndexer()->RemoveIndex(index);
}


void RSSBackend::GetAllFolders()
{
	Indexer* indexer = MessageEngine::GetInstance()->GetIndexer();
	time_t update_frequency = 3600;
	Index* index;

	for (INT32 it = -1; (index = indexer->GetRange(IndexTypes::FIRST_NEWSFEED, IndexTypes::LAST_NEWSFEED, it)) != NULL; )
	{
		if (index->GetAccountId() == m_account->GetAccountId() && index->GetSearch())
		{
			OpString feed_name;
			BOOL subscribed = TRUE;

			IndexSearch* search = index->GetSearch();
			if (search && search->GetSearchText().HasContent())
			{
				index->GetName(feed_name);

				GetAccountPtr()->OnFolderAdded(feed_name, search->GetSearchText(), subscribed, subscribed);
				AddToFile(feed_name, search->GetSearchText(), update_frequency, subscribed);
			}
		}
	}

	UINT32 next_feed_id;
	TRAPD(err, next_feed_id = m_file->ReadIntL("Feeds", "Next Feed ID", 0));
    // FIXME: do what if err is bad ?

	OpString feed_url, feed_name;
	OpString8 current;
	if (!current.Reserve(128))
		return;

	for (UINT32 i = 0; i <= next_feed_id; i++)
	{
		BOOL subscribed = 0;

		op_sprintf(current.CStr(), "Feed %d", i);

		TRAP(err,	m_file->ReadStringL(current.CStr(), "Name", feed_name); 
					m_file->ReadStringL(current.CStr(), "URL", feed_url));
        // FIXME: do what if err is bad ?

		if (feed_url.IsEmpty())
			continue;

		if (NULL == indexer->GetSubscribedFolderIndex(m_account, feed_url, 0, feed_name, FALSE, FALSE))
		{
			GetAccountPtr()->OnFolderAdded(feed_name, feed_url, subscribed, subscribed);
		}
	}
}


OP_STATUS RSSBackend::DeleteFolder(OpString& completeFolderPath)
{
	Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(m_account, completeFolderPath, '\0', UNI_L(""), FALSE, FALSE);

	if (index)
		OpStatus::Ignore(RemoveSubscribedFolder(index->GetId()));

	int next_feed_id = 0;
	RETURN_IF_LEAVE(next_feed_id = m_file->ReadIntL("Feeds", "Next Feed ID", 0));

	for (int i = 0; i <= next_feed_id; i++)
	{
		OpString feed_url;
		OpString8 current;

		current.Reserve(128);
		op_sprintf(current.CStr(), "Feed %d", i);

		RETURN_IF_LEAVE(m_file->ReadStringL(current.CStr(), "URL", feed_url));

		if (feed_url.CompareI(completeFolderPath) == 0)
		{
			RETURN_IF_LEAVE(m_file->DeleteSectionL(current.CStr()));
			RETURN_IF_LEAVE(m_file->CommitL());

			return OpStatus::OK;
		}
	}

	return OpStatus::ERR;
}


OP_STATUS RSSBackend::AddToFile(const OpStringC& feed_name, const OpStringC& feed_url, time_t update_frequency, BOOL subscribed)
{
	OpString8 section;
	if (!section.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

	UINT32 next_feed_id;
	RETURN_IF_LEAVE(next_feed_id = m_file->ReadIntL("Feeds", "Next Feed ID", 0));

	OpString current_url;
	OpString8 current_section;
	if (!current_section.Reserve(128))
		return OpStatus::ERR_NO_MEMORY;

	for (UINT32 i = 0; i <= next_feed_id; i++)
	{
		op_sprintf(current_section.CStr(), "Feed %d", i);

		RETURN_IF_LEAVE(m_file->ReadStringL(current_section.CStr(), "URL", current_url));

		if (current_url.CompareI(feed_url) == 0)
		{
			// already in list
			return OpStatus::OK;
		}
	}

	RETURN_IF_LEAVE(m_file->WriteIntL("Feeds", "Next Feed ID", ++next_feed_id); 
					op_sprintf(section.CStr(), "Feed %d", next_feed_id); 
					m_file->WriteStringL(section.CStr(), "Name", feed_name); 
					m_file->WriteStringL(section.CStr(), "URL", feed_url); 
					m_file->WriteIntL(section.CStr(), "Update Frequency", update_frequency); 
					m_file->WriteIntL(section.CStr(), "Subscribed", subscribed); 
					m_file->CommitL());

	return OpStatus::OK;
}


OP_STATUS RSSBackend::UpdateFeed(UINT32 index_id)
{
	Index* index = MessageEngine::GetInstance()->GetIndexById(index_id);
	if (!index)
		return OpStatus::ERR;

	IndexSearch* search = index->GetSearch();
	if (!search)
		return OpStatus::ERR;

	return UpdateFeed(search->GetSearchText(), FALSE);
}


OP_STATUS RSSBackend::UpdateFeedIfNeeded(Index* index, const OpStringC& url)
{
	OP_ASSERT(index != 0);
	const time_t update_frequency = index->GetUpdateFrequency();

	if (update_frequency != -1)
	{
		const time_t current_time = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();

		const time_t last_update_time = index->GetLastUpdateTime();
		const time_t next_update_time = last_update_time + update_frequency;

		if (current_time >= next_update_time)
		{
			if (url.IsEmpty())
			{
				IndexSearch* search = index->GetSearch();
				if (search && search->GetSearchText().HasContent())
					OpStatus::Ignore(UpdateFeed(search->GetSearchText(), FALSE));
			}
			else
				OpStatus::Ignore(UpdateFeed(url, FALSE));
		}
	}

	return OpStatus::OK;
}


OP_STATUS RSSBackend::UpdateFeedIfNeeded(UINT32 index_id, const OpStringC& url)
{
	Indexer* indexer = MessageEngine::GetInstance()->GetIndexer();
	OP_ASSERT(indexer != 0);

	Index* index = indexer->GetIndexById(index_id);
	if (index == 0)
		return OpStatus::ERR;

	RETURN_IF_ERROR(UpdateFeedIfNeeded(index, url));
	return OpStatus::OK;
}

#endif //M2_SUPPORT
