/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/opfile/desktop_opfile.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/index/indeximage.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/accountmgr.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/util/misc.h"

#include "adjunct/quick/managers/FavIconManager.h"

#include "modules/util/opfile/opfile.h"

void IndexSearch::Copy(const IndexSearch& search)
{
	m_search_text.Set(search.m_search_text.Get());
	m_option.Set(search.m_option.Get());
	m_search_body.Set(search.m_search_body.Get());
	m_start_date = search.m_start_date;
	m_end_date = search.m_end_date;
	m_current_id = search.m_current_id;
	m_operator.Set(search.m_operator.Get());
	m_search_only_in = search.m_search_only_in;
}

IndexSearch::IndexSearch()
	:	m_start_date(0),
		m_end_date(0),
		m_search_only_in(0),
		m_current_id(0)
{
	m_option.Set(SearchTypes::EXACT_PHRASE);
	m_search_body.Set(SearchTypes::CACHED_SUBJECT);
	m_operator.Set(SearchTypes::OR);
}

Index::Index()
		: m_auto_filter_file ( 0 )
		, m_id ( 0 )
		, m_parent_id ( 0 )
		, m_mirror_id ( 0 )
		, m_search_index_id ( 0 )
		, m_account_id ( 0 )
		, m_unread ( UINT_MAX )
		, m_new_messages ( 0 )
		, m_model_flags ( ( 1 << IndexTypes::MODEL_FLAG_READ ) |
		                  ( 1 << IndexTypes::MODEL_FLAG_SPAM ) |
		                  ( 1 << IndexTypes::MODEL_FLAG_MAILING_LISTS ) |
		                  ( 1 << IndexTypes::MODEL_FLAG_NEWSGROUPS ) |
		                  ( 1 << IndexTypes::MODEL_FLAG_NEWSFEEDS ) |
		                  ( 1 << IndexTypes::MODEL_FLAG_HIDDEN ) )
		, m_model_selected_message ( 0 )
		, m_image( NULL )
		, m_accessed ( 0 )
		, m_update_frequency ( 10800 )
		, m_last_update_time ( 0 )
		, m_index_flags(0x103)
		, m_type ( IndexTypes::FOLDER_INDEX )
		, m_model_type ( IndexTypes::MODEL_TYPE_FLAT )
		, m_model_age ( IndexTypes::MODEL_AGE_FOREVER )
		, m_special_use_type( AccountTypes::FOLDER_NORMAL)
		, m_model_sort ( IndexTypes::MODEL_SORT_BY_SENT )
		, m_model_grouping (IndexTypes::GROUP_BY_DATE)
		, m_is_readonly(false) 
		, m_is_prefetched(false) 
		, m_delayed_prefetch(false)
		, m_include_subfolders(false)
		, m_update_name_manually(false)
		, m_save_to_disk(false)
{
	m_name.Subscribe(MAKE_DELEGATE(*this, &Index::NameChanged));
}


Index::~Index()
{
	g_main_message_handler->UnsetCallBacks(this);

	if ( m_auto_filter_file )
	{
		TRAPD ( err, m_auto_filter_file->CommitL() );
		g_m2_engine->GetGlueFactory()->DeletePrefsFile ( m_auto_filter_file );
	}

	OP_DELETE(m_image);

	m_searches.DeleteAll();
}

OP_STATUS Index::NewMessage ( message_gid_t message, bool justwrite, bool setting_keyword )
{

	if (GetIndexFlag(IndexTypes::INDEX_FLAGS_MARK_MATCH_AS_READ))
	{
		OpINT32Vector message_vector;
		message_vector.Add(message);
		g_m2_engine->MessagesRead(message_vector,TRUE);
	}

	if ( !justwrite )
	{
		PreFetch(); // we need to prefetch before adding something

		if ( Contains ( message ) )
		{
			return OpStatus::OK; // no need to add twice
		}

		RETURN_IF_ERROR(m_messages.Add(message));
	}

	RETURN_IF_ERROR ( WriteData ( message ) );

	// Check if we want to save new message information
	if ( m_id > IndexTypes::LAST_IMPORTANT )
	{
		int flags = MessageEngine::GetInstance()->GetStore()->GetMessageFlags ( message );

		// Save new message if not spam or outgoing
		Index* spam_index = MessageEngine::GetInstance()->GetIndexById ( IndexTypes::SPAM );

		if ( spam_index && !spam_index->Contains ( message ) &&
		        ! ( flags & ( ( 1 << Message::IS_OUTGOING ) | ( 1 << Message::IS_READ ) ) ) )
		{
			if ( 0 <= m_new_messages && m_new_messages < (int)RECENT_MESSAGES )
				m_recent_messages[m_new_messages] = message;

			m_new_messages++;
		}
	}

	if (m_id == 0)
		return OpStatus::OK; // no need to alert observers

	return MessageAdded ( message, setting_keyword );
}


OP_STATUS Index::RemoveMessage ( message_gid_t message, bool setting_keyword )
{
	if ( m_is_prefetched && !Contains ( message ) )
		return OpStatus::OK;

	m_messages.Remove ( message );

	// remove from disk, add a negative number to the file:
	RETURN_IF_ERROR ( WriteData ( 0-message ) );

	return MessageRemoved ( message, setting_keyword );
}


OP_STATUS Index::WriteData ( UINT32 data )
{
	if ( !m_save_to_disk )
	{
		// memory only Index. Typically used to compare Indexes
		return OpStatus::OK;
	}
	
	OpString unique;
	if (OpStatus::IsError(GetUniqueName(unique)))
	{
		// this should never happen, find out what causes a save to disk index to not have a unique name!
		OP_ASSERT(0);
		return OpStatus::OK;
	}

	if ( ( int ) data > 0 )
		RETURN_IF_ERROR ( g_m2_engine->GetIndexer()->AddIndexWord ( unique.CStr(), data ) );
	else if ( ( int ) data < 0 )
		RETURN_IF_ERROR ( g_m2_engine->GetIndexer()->RemoveIndexWord ( unique.CStr(), - ( int ) data ) );
	else
		return OpStatus::OK;

	return MessageEngine::GetInstance()->GetStore()->RequestCommit();
}


OP_STATUS Index::Empty()
{
	OpINT32Vector ids;
	for (INT32SetIterator it(m_messages); it; it++)
		RETURN_IF_ERROR(ids.Add(it.GetData()));

	for (unsigned i = 0; i < ids.GetCount(); i++)
	{
		message_gid_t message = ids.Get(i);
		RETURN_IF_ERROR(RemoveMessage(message));
	}

	m_unread = 0;
	StatusChanged();

	return OpStatus::OK;
}

UINT32 Index::UnreadCount()
{
	if ( 1 + m_unread == 0 )
	{
		if ( !m_is_prefetched )
		{
			return 0;
		}

		Indexer* indexer = MessageEngine::GetInstance()->GetIndexer();
		Index* unread = indexer->GetIndexById ( IndexTypes::UNREAD );

		m_unread = 0;

		if ( unread )
		{
			// copied from Indexer::AndIndexes:

			UINT32 first_end;
			UINT32 second_end;

			message_gid_t message;

			Index* first = this;
			Index* second = unread;

			first_end = first->MessageCount();
			second_end = second->MessageCount();

			if ( first_end > second_end )
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

				if ( message && second->Contains ( message ) )
				{
					if ( !MessageHidden ( message ) )
					{
						m_unread++;
					}
				}
			}
		}
	}
	return m_unread;
}

bool Index::MessageNotVisible ( message_gid_t message_id ) 
{
	// if the index doesn't contain the message then it's "hidden"
	if (m_is_prefetched && !Contains(message_id))
		return TRUE;

	return MessageHidden(message_id);
}




bool Index::MessageHidden ( message_gid_t message_id, IndexTypes::Id ignore )
{
	Index* index;

	if ( ( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_TRASH ) ) ==0
	        && ignore != IndexTypes::TRASH )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::TRASH );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}
	if ( ( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_SPAM ) ) ==0
	        && ignore != IndexTypes::SPAM )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::SPAM );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}
	if ( ( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_NEWSGROUPS ) ) ==0 )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::RECEIVED_NEWS );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}
	if ( ( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_NEWSFEEDS ) ) ==0 )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( MessageEngine::GetInstance()->GetIndexer()->GetRSSAccountIndexId() );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}
	if ( ( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_MAILING_LISTS ) ) ==0 )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::RECEIVED_LIST );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}
	if ( ( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_READ ) ) ==0
	        && ignore != IndexTypes::UNREAD )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::UNREAD );
		if ( index && !index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}

	if ( g_m2_engine->GetStore()->GetMessageFlag(message_id,Message::IS_OUTGOING) && (m_id < IndexTypes::OUTBOX || m_id > IndexTypes::SENT) && m_special_use_type != AccountTypes::FOLDER_SENT)
	{
		if (( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_SENT ) ) ==0
				&& ignore != IndexTypes::SENT )
		{
			// sent messages are hidden
			return TRUE;
		}
		else
		{
			// we should never hide sent messages for thread or contact indexes (this message might be the only one)
			if (m_model_type == IndexTypes::MODEL_TYPE_THREADED &&
				! (m_id > IndexTypes::FIRST_THREAD && m_id < IndexTypes::LAST_THREAD) &&
				! (m_id > IndexTypes::FIRST_CONTACT && m_id < IndexTypes::LAST_CONTACT))
			{

				// we only want to show this outgoing message if it's a reply to a visible message
				// or there is a visible reply to it
				bool should_be_hidden = TRUE;
				message_gid_t parent_id = g_m2_engine->GetStore()->GetParentId(message_id);
				
				if (parent_id != 0 && 
					!g_m2_engine->GetStore()->GetMessageFlag(parent_id,Message::IS_OUTGOING) && // avoid infinite loops
					!MessageHidden(parent_id) ||
					m_model_flags & ( 1<<IndexTypes::MODEL_FLAG_SENT ) && IsFilter())
				{
					should_be_hidden  = FALSE;
				}

				if (should_be_hidden)
				{
					OpINTSet children_ids;
					
					OpStatus::Ignore(g_m2_engine->GetStore()->GetChildrenIds(message_id, children_ids));
					
					UINT32 i = 0;
					while( i < children_ids.GetCount() && should_be_hidden )
					{
						if (!g_m2_engine->GetStore()->GetMessageFlag(children_ids.GetByIndex(i),Message::IS_OUTGOING) && // avoid infinite loops
							!MessageHidden(children_ids.GetByIndex(i)))
							should_be_hidden = FALSE;
						i++;
					}
				}

				if (should_be_hidden)
					return TRUE;
			}
		}

	}
	// Hide messages in views with Hide from other set
	if ( ( m_model_flags& ( 1<<IndexTypes::MODEL_FLAG_HIDDEN ) ) ==0
	        && ignore != IndexTypes::HIDDEN )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::HIDDEN );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}
	if ( m_model_type == IndexTypes::MODEL_TYPE_TO )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::SENT );
		if ( index && !index->Contains ( message_id ) )
		{
			index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::OUTBOX );
			if ( index && !index->Contains ( message_id ) )
			{
				return TRUE;
			}
		}
	}
	else if ( m_model_type == IndexTypes::MODEL_TYPE_FROM )
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::SENT );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::OUTBOX );
		if ( index && index->Contains ( message_id ) )
		{
			return TRUE;
		}
	}

	if ( GetModelAge() != IndexTypes::MODEL_AGE_FOREVER )
	{
		time_t sent_date = MessageEngine::GetInstance()->GetStore()->GetMessageDate ( message_id );
		if ( GetTimeByAge ( GetModelAge() ) > sent_date )
		{
			return TRUE;
		}
	}

	// Hide non-active accounts

	int active_account;
	active_account = MessageEngine::GetInstance()->GetAccountManager()->GetActiveAccount();

	// not hide newsfeed messages from newsfeed views

	bool is_newsfeed_access_point = ( m_id == MessageEngine::GetInstance()->GetIndexer()->GetRSSAccountIndexId() ) ||
	                                ( m_type == IndexTypes::NEWSFEED_INDEX );

	if ( active_account > 0 ) //Account_id
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::FIRST_ACCOUNT + active_account );
		if ( index )
		{
			OpStatus::Ignore ( index->PreFetch() );

			if ( !index->Contains ( message_id ) && !is_newsfeed_access_point )
				return TRUE;
		}
	}
	else if ( active_account < 0 && !is_newsfeed_access_point )
	{
		bool message_is_hidden = TRUE;

		Account* account = ( ( AccountManager* ) ( MessageEngine::GetInstance()->GetAccountManager() ) )->GetFirstAccount();
		while ( account )
		{
			if ( MessageEngine::GetInstance()->GetAccountManager()->IsAccountActive ( account->GetAccountId() ) )
			{
				index = MessageEngine::GetInstance()->GetIndexer()->GetIndexById ( IndexTypes::FIRST_ACCOUNT + account->GetAccountId() );
				if ( index )
				{
					OpStatus::Ignore ( index->PreFetch() );

					if ( index->Contains ( message_id ) )
					{
						message_is_hidden = FALSE;
					}
				}
			}

			account = ( Account* ) ( account->Suc() );
		}

		return message_is_hidden;
	}

	// Hide duplicate messages in views where show duplicates set to false
	if ( (m_model_flags & (1<<IndexTypes::MODEL_FLAG_DUPLICATES)) == 0 && MessageEngine::GetInstance()->GetStore()->GetMessageHasDuplicates(message_id))
	{
		const DuplicateMessage* dupe = MessageEngine::GetInstance()->GetStore()->GetMessageDuplicates(message_id);

		// show the first dupe in the list (and start with the master)
		while (dupe)
		{
			if (dupe->m2_id == message_id)
				break;

			if (m_messages.Contains(dupe->m2_id) && !MessageHidden(dupe->m2_id))
				return TRUE;

			dupe = dupe->next;
		}
	}

	return FALSE;
}


UINT32 Index::CommonCount ( index_gid_t index_id )
{
	Index counter;

	Indexer* indexer = MessageEngine::GetInstance()->GetIndexer();
	Index* other = indexer->GetIndexById ( index_id );

	if ( other )
	{
		if ( indexer->IntersectionIndexes ( counter,other,this,0 ) != OpStatus::OK )
		{
			OP_ASSERT ( 0 );
			return 0;
		}
	}
	return counter.MessageCount();
}

OP_STATUS Index::GetName ( OpString &name )
{
	return name.Set ( m_name.Get() );
}

void Index::NameChanged(const OpStringC& new_name)
{
	// Alert observers
	for ( OpListenersIterator iterator ( m_observers ); m_observers.HasNext ( iterator ); )
		m_observers.GetNext ( iterator )->NameChanged ( this );

	// Alert indexer, the permanent observer
	MessageEngine::GetInstance()->GetIndexer()->NameChanged ( this );
}

void Index::SetParentId(index_gid_t parent_id)
{
	index_gid_t old_parent_id = m_parent_id;
	m_parent_id = parent_id;
	MessageEngine::GetInstance()->GetIndexer()->OnParentChanged(m_id, old_parent_id, parent_id);
}

void Index::SetModelType(IndexTypes::ModelType type, bool user_initiated)
{ 
	m_model_type = type; 
	m_unread = UINT_MAX; 

	// when switching to threaded view, we should default to show sent mail
	if (type == IndexTypes::MODEL_TYPE_THREADED && user_initiated )
	{
		// we should force show sent mail when the type is threaded
		EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);
	}
	else if (type == IndexTypes::MODEL_TYPE_FLAT && user_initiated && m_type == IndexTypes::CONTACTS_INDEX)
	{
		// the flat type for contacts is actually To and From...
		// we should show sent mail in flat mode by default
		EnableModelFlag(IndexTypes::MODEL_FLAG_SENT);
	}
	// when switching to flat view, we should default to not show sent mail (except for the sent, draft and outbox view)
	else if (type == IndexTypes::MODEL_TYPE_FLAT && user_initiated && m_id != IndexTypes::SENT && m_id != IndexTypes::OUTBOX && m_id != IndexTypes::DRAFTS && !IsFilter())
	{
		// we should not show sent mail in flat mode by default
		DisableModelFlag(IndexTypes::MODEL_FLAG_SENT);
	}

}

void Index::SetHideFromOther (bool hide_messages)
{
	SetIndexFlag(IndexTypes::INDEX_FLAGS_HIDE_FROM_OTHER, hide_messages);
	if ( hide_messages )
	{
		m_model_flags = m_model_flags| ( 1<<IndexTypes::MODEL_FLAG_HIDDEN );
	}
	else
	{
		m_model_flags = m_model_flags&~ ( 1<<IndexTypes::MODEL_FLAG_HIDDEN );
	}
}

OP_STATUS Index::GetContactAddress ( OpString& address )
{
	if ( m_type != IndexTypes::CONTACTS_INDEX )
		return OpStatus::ERR;

	IndexSearch *search = m_searches.Get(0);
	if (!search)
		return OpStatus::ERR;

	return address.Set(search->GetSearchText());
}


IndexSearch* Index::GetSearch ( UINT32 position )
{
	return m_searches.Get ( position );
}


UINT32 Index::GetSearchCount()
{
	return m_searches.GetCount();
}


OP_STATUS Index::RemoveSearch ( UINT32 position )
{
	return ( m_searches.Remove ( position ) ? OpStatus::OK : OpStatus::ERR );
}


OP_STATUS Index::AddSearch ( const IndexSearch &search )
{
	OpAutoPtr<IndexSearch> search_copy (OP_NEW(IndexSearch, (search)));
	if (!search_copy.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_searches.Add(search_copy.get()));
	search_copy.release();
	
	return OpStatus::OK;
}


UINT32 Index::GetAccountId()
{
	if (IndexTypes::FIRST_ACCOUNT <= m_id && m_id < IndexTypes::LAST_ACCOUNT)
		return m_id - IndexTypes::FIRST_ACCOUNT;
	else
		return m_account_id;
}


OP_STATUS Index::MessageAdded ( message_gid_t message, BOOL setting_keyword )
{
	// Alert observers
	for ( OpListenersIterator iterator ( m_observers ); m_observers.HasNext ( iterator ); )
		m_observers.GetNext ( iterator )->MessageAdded ( this, message, setting_keyword );

	// Alert indexer, the permanent observer
	MessageEngine::GetInstance()->GetIndexer()->MessageAdded ( this, message, setting_keyword );

	return OpStatus::OK;
}


OP_STATUS Index::MessageRemoved ( message_gid_t message, BOOL setting_keyword )
{
	// Alert observers
	for ( OpListenersIterator iterator ( m_observers ); m_observers.HasNext ( iterator ); )
		m_observers.GetNext ( iterator )->MessageRemoved ( this, message, setting_keyword );

	// Alert indexer, the permanent observer
	MessageEngine::GetInstance()->GetIndexer()->MessageRemoved ( this, message, setting_keyword );

	return OpStatus::OK;
}


OP_STATUS Index::StatusChanged()
{
	// Alert observers
	for ( OpListenersIterator iterator ( m_observers ); m_observers.HasNext ( iterator ); )
		m_observers.GetNext ( iterator )->StatusChanged ( this );

	// Alert indexer, the permanent observer
	MessageEngine::GetInstance()->GetIndexer()->StatusChanged ( this );

	return OpStatus::OK;
}

/***********************************************************************************
 ** Finds the first mail with a given date
 **
 ** Index::GetTimeByAge
 ***********************************************************************************/
time_t Index::GetTimeByAge(IndexTypes::ModelAge age) const
{
	time_t now = g_timecache->CurrentTime();

	switch (age)
	{
		case IndexTypes::MODEL_AGE_TODAY:
			return now-86400;
		case IndexTypes::MODEL_AGE_WEEK:
			return now-604800;
		case IndexTypes::MODEL_AGE_MONTH:
			return now-2678400;
		case IndexTypes::MODEL_AGE_3_MONTHS:
			return now-8035200;
		case IndexTypes::MODEL_AGE_YEAR:
			return now-31622400;
		case IndexTypes::MODEL_AGE_FOREVER:
		default:
			return 0;
	}
}

OP_STATUS Index::GetUniqueName ( OpString& name )
{
	if ( !m_id )
	{
		switch ( m_type )
		{
			case IndexTypes::CONTACTS_INDEX:
			case IndexTypes::NEWSGROUP_INDEX:
				break;
			default:
				// not possible to save without Id
				return OpStatus::ERR;
		}
	}

	switch ( m_type )
	{
		// UnionGroupIndexes should never be saved because they are generated on the fly (to make sure new mail is added to the group)
		// On start up they are recreated from index.ini by adding all child indexes
		case IndexTypes::UNIONGROUP_INDEX:
			name.Empty();
			return OpStatus::ERR;
		case IndexTypes::SEARCH_INDEX:
			return name.AppendFormat ( UNI_L ( "search_%i.idx" ), m_id );

		case IndexTypes::FOLDER_INDEX:
			if ( m_id >= IndexTypes::FIRST_LABEL && m_id < IndexTypes::LAST_LABEL )
			{
				return name.AppendFormat ( UNI_L ( "label_%i.idx" ), m_id );
			}
			else if ( m_id >= IndexTypes::FIRST_THREAD && m_id < IndexTypes::LAST_THREAD )
			{
				return name.AppendFormat ( UNI_L ( "thread_%i.idx" ), m_id );
			}
			else
			{
				switch ( m_id )
				{
					case IndexTypes::UNREAD_UI:
						return name.Set ( UNI_L ( "m2_unread_ui.idx" ) );
					case IndexTypes::RECEIVED:
						name.Empty();
						return OpStatus::ERR;
					case IndexTypes::RECEIVED_NEWS:
						return name.Set ( UNI_L ( "m2_news.idx" ) );
					case IndexTypes::OUTBOX:
						return name.Set ( UNI_L ( "m2_outbox.idx" ) );
					case IndexTypes::SENT:
						return name.Set ( UNI_L ( "m2_sent.idx" ) );
					case IndexTypes::DRAFTS:
						return name.Set ( UNI_L ( "m2_drafts.idx" ) );
					case IndexTypes::SPAM:
						return name.Set ( UNI_L ( "m2_spam.idx" ) );
					case IndexTypes::PIN_BOARD:
						return name.Set( UNI_L ( "m2_pin_board.idx" ) );
					case IndexTypes::TRASH:
						return name.Set ( UNI_L ( "m2_trash.idx" ) );
					case IndexTypes::UNREAD:
						return name.Set ( UNI_L ( "m2_unread.idx" ) );
					case IndexTypes::RECEIVED_LIST:
						return name.Set ( UNI_L ( "m2_received_list.idx" ) );
					case IndexTypes::CLIPBOARD:
						return name.Set ( UNI_L ( "m2_clipboard.idx" ) );
					case IndexTypes::DOC_ATTACHMENT:
						return name.Set ( UNI_L ( "attachment_doc.idx" ) );
					case IndexTypes::IMAGE_ATTACHMENT:
						return name.Set ( UNI_L ( "attachment_image.idx" ) );
					case IndexTypes::AUDIO_ATTACHMENT:
						return name.Set ( UNI_L ( "attachment_audio.idx" ) );
					case IndexTypes::VIDEO_ATTACHMENT:
						return name.Set ( UNI_L ( "attachment_video.idx" ) );
					case IndexTypes::ZIP_ATTACHMENT:
						return name.Set ( UNI_L ( "attachment_zip.idx" ) );
					default:
						return name.AppendFormat ( UNI_L ( "folder_%i.idx" ), m_id );
				}
			}

		case IndexTypes::IMAP_INDEX:
			return name.AppendFormat ( UNI_L ( "imap_%i.idx" ), m_id );

		case IndexTypes::NEWSFEED_INDEX:
			return name.AppendFormat ( UNI_L ( "newsfeed_%i.idx" ), m_id );

		case IndexTypes::ARCHIVE_INDEX:
			return name.AppendFormat ( UNI_L ( "archive_%i.idx" ), m_id );
		
		case IndexTypes::INTERSECTION_INDEX:
			// unfortunately "inherit filters" weren't intersection indexes before and used folder_%i.idx for their unique names
			if (m_index_flags & (1<<IndexTypes::INDEX_FLAGS_INHERIT_FILTER_DEPRECATED))
				return name.AppendFormat ( UNI_L ( "folder_%i.idx" ), m_id );
			else
				return name.AppendFormat ( UNI_L ( "intersection_group_%i.idx" ), m_id );

		case IndexTypes::COMPLEMENT_INDEX:
			return name.AppendFormat ( UNI_L ( "complement_group_%i.idx" ), m_id );

		case IndexTypes::CONTACTS_INDEX:
		{
			IndexSearch *search = GetSearch();
			if (!search)
				return OpStatus::ERR;

			if (search->GetSearchText().HasContent())
			{
				OpString8 search_IMAA;
				OpMisc::ConvertToIMAAAddress(search->GetSearchText(), TRUE, search_IMAA);
				OpString search_IMAA16;
				search_IMAA16.Set ( search_IMAA );

				if (search->GetSearchText().Find("@") != KNotFound)
					return name.AppendFormat ( UNI_L ( "contact_%s.idx" ), search_IMAA16.CStr() );
				else
					return name.AppendFormat ( UNI_L ( "list_%s.idx" ), search_IMAA16.CStr() );
			}
			return OpStatus::ERR;
		}
		case IndexTypes::NEWSGROUP_INDEX:
		{
			IndexSearch *search = GetSearch();
			if (!search)
				return OpStatus::ERR;

			return name.AppendFormat ( UNI_L ( "news_%s.idx" ), search->GetSearchText().CStr() );
		}
		default:
			return name.AppendFormat ( UNI_L ( "index_%i.idx" ), m_id );
	}
}


OP_STATUS Index::SetAutoFilterFile()
{
	if ( m_auto_filter_file )
	{
		// file already exists
		return OpStatus::OK;
	}

	if ( !m_id )
	{
		// not possible to save without Id
		return OpStatus::ERR;
	}

	OpString autofiltername;
	RETURN_IF_ERROR(MailFiles::GetAutofilterFileName(m_id, autofiltername));

	// deleted by the Index:
	PrefsFile* file = g_m2_engine->GetGlueFactory()->CreatePrefsFile ( autofiltername );

	if ( !file )
	{
		return OpStatus::ERR;
	}
	SetIndexFlag(IndexTypes::INDEX_FLAGS_AUTO_FILTER, true);
	m_auto_filter_file = file;
	return OpStatus::OK;
}


OP_STATUS Index::RemoveAutoFilterFile()
{
	if ( m_auto_filter_file )
	{
		// consider cleaning/deleting the file here

		TRAPD ( err,	m_auto_filter_file->DeleteSectionL ( UNI_L ( "Messages" ) ); 
		        m_auto_filter_file->DeleteSectionL ( UNI_L ( "Include" ) ); 
		        m_auto_filter_file->DeleteSectionL ( UNI_L ( "Exclude" ) ); 
		        m_auto_filter_file->CommitL() );

		MessageEngine::GetInstance()->GetGlueFactory()->DeletePrefsFile ( m_auto_filter_file );
		m_auto_filter_file = NULL;
		SetIndexFlag(IndexTypes::INDEX_FLAGS_AUTO_FILTER, false);
	}
	return OpStatus::OK;
}

void Index::SetPrefetched()
{
	m_is_prefetched = TRUE;

	ResetUnreadCount();
	StatusChanged();
}


OP_STATUS Index::PreFetch()
{
	if ( !m_save_to_disk )
	{
		m_is_prefetched = TRUE;
		return OpStatus::OK; // memory only Index
	}

	// no need to do it twice
	if (m_is_prefetched )
		return OpStatus::OK;

	OP_ASSERT(m_messages.GetCount() == 0);
	if (m_messages.GetCount() > 0)
		m_messages.RemoveAll();

	OpString unique;
	RETURN_IF_ERROR ( GetUniqueName ( unique ) );
	RETURN_IF_ERROR ( g_m2_engine->GetIndexer()->FindIndexWord ( unique.CStr(), m_messages ) );

	m_is_prefetched = TRUE;

	ResetUnreadCount(); // OK, doesn't need optimization
	StatusChanged();

	return OpStatus::OK;
}


OP_STATUS Index::DelayedPreFetch()
{
	// no need to do it twice
	if (m_is_prefetched || m_delayed_prefetch)
		return OpStatus::OK;

	const MH_PARAM_1 par1 = reinterpret_cast<MH_PARAM_1>(this);
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_M2_DELAYED_PREFETCH, par1));

	m_delayed_prefetch = g_main_message_handler->PostMessage(MSG_M2_DELAYED_PREFETCH, par1, 0, 100) ? true : false;

	return OpStatus::OK;
}


void Index::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(msg == MSG_M2_DELAYED_PREFETCH && par1 == reinterpret_cast<MH_PARAM_1>(this));

	m_delayed_prefetch = false;
	g_main_message_handler->UnsetCallBacks(this);

	OpStatus::Ignore(PreFetch());
}


const char* Index::GetDefaultKeyword() const
{
	// Special preset keywords
	switch ( m_id )
	{
			// Labels like Thunderbird
		case IndexTypes::LABEL_IMPORTANT:
			return "$Label1";
		case IndexTypes::LABEL_TODO:
			return "$Label4";
		case IndexTypes::LABEL_MAIL_BACK:
			return "$Label5";
		case IndexTypes::LABEL_CALL_BACK:
			return "$Label6";
		case IndexTypes::LABEL_MEETING:
			return "$Label2";
		case IndexTypes::LABEL_FUNNY:
			return "$Label7";
		case IndexTypes::LABEL_VALUABLE:
			return "$Label3";

			// Generally accepted keywords
			// TODO better way to do this. When setting up new account,
			// this will backfire; messages with the keyword are immediately
			// added to spam, making the spam filter imprecise for the next
			// message to arrive.
			//
			// case IndexTypes::SPAM:
			// 	return "$Spam";

			// others don't get any keywords
		default:
			return NULL;
	}
}


OP_STATUS Index::GetAllMessages ( OpINT32Vector& message_ids )
{
	message_ids.Clear();

	for (INT32SetIterator it(m_messages); it; it++)
		RETURN_IF_ERROR(message_ids.Add(it.GetData()));

	return OpStatus::OK;
}


unsigned Index::GetNewMessagesCount()
{
	if ( m_new_messages >= 0 )
	{
		unsigned new_messages = m_new_messages;

		m_new_messages = 0;
		return new_messages;
	}

	return 0;
}

OP_STATUS Index::DecreaseNewMessageCount ( unsigned decrease )
{
	m_new_messages -= decrease;

	if ( m_account_id )
	{
		Index* account_index = MessageEngine::GetInstance()->GetIndexById ( IndexTypes::FIRST_ACCOUNT + m_account_id );

		if ( account_index && account_index != this )
		{
			return account_index->DecreaseNewMessageCount ( decrease );
		}
	}

	return OpStatus::OK;
}

bool Index::IsFilter() const
{
	return !m_account_id && IndexTypes::FIRST_FOLDER <= m_id && m_id < IndexTypes::LAST_FOLDER;
}

bool Index::IsContact() const
{
	if (m_id < IndexTypes::FIRST_CONTACT || m_id > IndexTypes::LAST_CONTACT)
		return false;

	IndexSearch* search = m_searches.Get(0);
	if (search && search->GetSearchText().Find("@") != KNotFound && search->GetSearchText().Find(".") == KNotFound)
		return true;

	return false;
}

OP_STATUS Index::GetImages ( const char*& image, Image &bitmap_image )
{
	if (m_skin_image.IsEmpty())
	{
		switch ( m_id )
		{
			case IndexTypes::UNREAD_UI:
				image = "Mail Unread";
				break;
			case IndexTypes::NEW_UI:
				image = "Mail New";
				break;
			case IndexTypes::RECEIVED:
				image = "Mail Inbox";
				break;
			case IndexTypes::PIN_BOARD:
				image = "Mail Pin Board";
				break;
			case IndexTypes::OUTBOX:
				image = "Mail Outbox";
				break;
			case IndexTypes::SENT:
				image = "Mail Sentbox";
				break;
			case IndexTypes::DRAFTS:
				image = "Mail Drafts";
				break;
			case IndexTypes::TRASH:
				image = "Mail Trash";
				break;
			case IndexTypes::SPAM:
				image = "Mail Spam";
				break;
			case IndexTypes::DOC_ATTACHMENT:
				image = "Attachment Documents";
				break;
			case IndexTypes::ZIP_ATTACHMENT:
				image = "Attachment Archives";
				break;
			case IndexTypes::AUDIO_ATTACHMENT:
				image = "Attachment Music";
				break;
			case IndexTypes::VIDEO_ATTACHMENT:
				image = "Attachment Video";
				break;
			case IndexTypes::IMAGE_ATTACHMENT:
				image = "Attachment Images";
				break;
			case IndexTypes::CATEGORY_MY_MAIL:
				image = "Mail All Messages";
				break;
			case IndexTypes::CATEGORY_MAILING_LISTS:
				image = "Mail Mailing Lists";
				break;
			case IndexTypes::CATEGORY_ACTIVE_CONTACTS:
				image = "Mail Active Contacts";
				break;
			case IndexTypes::CATEGORY_ACTIVE_THREADS:
				image = "Mail Active Threads";

				break;
			case IndexTypes::CATEGORY_LABELS:
				image = "Mail Labels";
				break;
			case IndexTypes::CATEGORY_ATTACHMENTS:
				image = "Mail Attachments";
				break;
		}

		if ( m_id >= IndexTypes::FIRST_SEARCH && m_id < IndexTypes::LAST_SEARCH )
		{
			image = "Mail Search";
		}
		else if ( m_id >= IndexTypes::FIRST_NEWSGROUP && m_id < IndexTypes::LAST_NEWSGROUP )
		{
			image = "News Subscribed";
		}
		else if ( m_id >= IndexTypes::FIRST_IMAP && m_id < IndexTypes::LAST_IMAP )
		{
			switch(m_special_use_type)
			{
			case AccountTypes::FOLDER_DRAFTS:
				image = "Mail IMAP Drafts";
				break;
			case AccountTypes::FOLDER_FLAGGED:
				image = "Mail IMAP Flagged";
				break;
			case AccountTypes::FOLDER_INBOX:
				image = "Mail IMAP Inbox";
				break;
			case AccountTypes::FOLDER_SENT:
				image = "Mail IMAP Sent";
				break;
			case AccountTypes::FOLDER_SPAM:
				image = "Mail IMAP Spam";
				break;
			case AccountTypes::FOLDER_TRASH:
				image = "Mail IMAP Trash";
				break;
			default:
				image = "Folder";
				break;
			}
		}
		else if ( m_id >= IndexTypes::FIRST_POP && m_id < IndexTypes::LAST_POP )
		{
			if (m_id % 2 == 0)
			{
				image = "Mail Inbox";
			}
			else
			{
				image = "Mail Sentbox";
			}
		}
		else if ( m_id >= IndexTypes::FIRST_ARCHIVE && m_id < IndexTypes::LAST_ARCHIVE )
		{
			image = "Folder";
		}
		else if ( m_id >= IndexTypes::FIRST_NEWSFEED && m_id < IndexTypes::LAST_NEWSFEED )
		{
			image = "Newsfeed Subscribed";
		}
		else if ( m_id >= IndexTypes::FIRST_THREAD && m_id < IndexTypes::LAST_THREAD )
		{
			image = "Mail Thread";
		}
		else if ( m_id >= IndexTypes::FIRST_FOLDER && m_id < IndexTypes::LAST_FOLDER )
		{
			image = "Mail Label";
		}
		else if ( m_id >= IndexTypes::FIRST_UNIONGROUP && m_id < IndexTypes::LAST_UNIONGROUP )
		{
			image = "Folder";
		}
		if ( image==NULL )
		{
			Account *account = NULL;
			if ( OpStatus::IsSuccess ( MessageEngine::GetInstance()->GetAccountManager()->GetAccountById ( m_id - IndexTypes::FIRST_ACCOUNT, account ) ) && account )
			{
				switch ( account->GetIncomingProtocol() )
				{
					case AccountTypes::SMTP:
					case AccountTypes::POP:
						image = "Account POP";
						break;
					case AccountTypes::IMAP:
						image = "Account IMAP";
						break;
					case AccountTypes::NEWS:
						image = "Account News";
						break;
					case AccountTypes::RSS:
						image = "Mail Newsfeeds";
						break;
				}
			}
		}
		m_skin_image.Set(image);
	}
	else
	{
		image = m_skin_image.CStr();
	}

	if ( m_id >= IndexTypes::FIRST_CONTACT && m_id < IndexTypes::LAST_CONTACT )
	{
		image = "Contact Unknown";

		IndexSearch* search = GetSearch();
		if (search)
		{
			if (search->GetSearchText().Find("@") == KNotFound)
			{
				image = "Mailing List Unknown";
			}

			MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->GetContactImage ( search->GetSearchText(), image );
		}
	}

	if ( m_id >= IndexTypes::FIRST_NEWSFEED && m_id < IndexTypes::LAST_NEWSFEED )
	{
		IndexSearch* search = GetSearch();
		if (search)
		{
			bitmap_image = g_favicon_manager->Get(search->GetSearchText().CStr(), TRUE);
		}
	}

	if (m_image)
		bitmap_image = m_image->GetBitmapImage();

	return OpStatus::OK;
}

void Index::SetReadOnly ( bool readonly )
{
	if ( m_is_readonly == !readonly )
	{
		m_is_readonly = readonly != FALSE;
		StatusChanged();
	}
}

OP_STATUS Index::SetupWithParent(Index* parent_index)
{
	SetType(parent_index->GetType());
	SetAccountId(parent_index->GetAccountId());
	SetParentId(parent_index->GetId());
	SetSaveToDisk(parent_index->GetSaveToDisk());

	return OpStatus::OK;
}

void Index::SetWatched(bool watched)
{
	SetIndexFlag(IndexTypes::INDEX_FLAGS_WATCHED, watched );

	if (m_id >= IndexTypes::FIRST_THREAD && m_id <= IndexTypes::LAST_THREAD)
		SetParentId(watched ? IndexTypes::CATEGORY_ACTIVE_THREADS : 0);
	else if ( m_id >= IndexTypes::FIRST_CONTACT && m_id <= IndexTypes::LAST_CONTACT)
		SetParentId(watched ? IndexTypes::CATEGORY_ACTIVE_CONTACTS : 0);
}

void Index::SetVisible(bool visible)
{
	SetIndexFlag(IndexTypes::INDEX_FLAGS_VISIBLE, visible);

	if (m_id)
	{
		// Alert observers
		for (OpListenersIterator iterator(m_observers); m_observers.HasNext(iterator);)
			m_observers.GetNext(iterator)->VisibleChanged(this);

		// Alert indexer
		MessageEngine::GetInstance()->GetIndexer()->VisibleChanged(this);
	}
}
void Index::ToggleWatched(bool watched)
{ 
	// SetWatched sets the parent id, which is needed for the VisibleChanged listeners
	// so we have to be careful about the order these functions are called

	if (watched)
	{
		SetWatched(TRUE); 
		SetVisible(TRUE);
	}
	else
	{
		SetIndexFlag(IndexTypes::INDEX_FLAGS_WATCHED, false);
		SetVisible(FALSE);
		SetWatched(FALSE);
	}	
}

OP_STATUS Index::SetCustomImage(const OpStringC& filename)
{
	if (!m_image)
		m_image = OP_NEW(IndexImage, ());
	if (!m_image)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_image->SetImageFromFileName(filename));
	return IconChanged();
}

OP_STATUS Index::SetCustomBase64Image(const OpStringC8& buffer)
{
	if (!m_image)
		m_image = OP_NEW(IndexImage, ());
	if (!m_image)
		return OpStatus::ERR_NO_MEMORY;
	RETURN_IF_ERROR(m_image->SetImageFromBase64(buffer));
	return IconChanged();
}

OP_STATUS Index::SetSkinImage(const OpStringC8& image)
{
	OP_DELETE(m_image);
	m_image = 0;
	RETURN_IF_ERROR(m_skin_image.Set(image));
	return IconChanged();
}

OP_STATUS Index::IconChanged()
{
	// Alert observers
	for ( OpListenersIterator iterator ( m_observers ); m_observers.HasNext ( iterator ); )
		m_observers.GetNext ( iterator )->IconChanged ( this );

	// Alert indexer, the permanent observer
	return MessageEngine::GetInstance()->GetIndexer()->IconChanged ( this );
}

#endif //M2_SUPPORT
