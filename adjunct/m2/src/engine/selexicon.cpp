/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "adjunct/m2/src/include/defs.h"

#include "adjunct/desktop_util/adt/opsortedvector.h"
#include "adjunct/desktop_util/filelogger/desktopfilelogger.h"

#include "adjunct/m2/src/recovery/recovery.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/engine/selexicon.h"
#include "adjunct/m2/src/engine/store.h"

#include "modules/pi/OpThreadTools.h"

// Delay (in ms) for running the commit action
#define COMMIT_DELAY 60000


/***********************************************************************************
 ** Destructor
 **
 ** SELexicon::~SELexicon
 ***********************************************************************************/
SELexicon::~SELexicon()
{
	OP_PROFILE_METHOD("Destructed Lexicon");
	// We stop ASAP, it might take a long time and we will continue on next
	// startup if necessary
	OpStatus::Ignore(Stop(TRUE));

	OpStatus::Ignore(Commit());
	OpStatus::Ignore(m_text_table.Close());

	CleanupIndexedMessages();

	// Cleanup callbacks
	g_main_message_handler->UnsetCallBacks(this);
}


/***********************************************************************************
 ** Initialization - run this function before using other functions!
 **
 ** SELexicon::Init
 ***********************************************************************************/
OP_BOOLEAN SELexicon::Init(const uni_char * path, const uni_char * table_name)
{
	RETURN_IF_ERROR(m_path.Set(path));
	RETURN_IF_ERROR(m_table_name.Set(table_name));
	// Open the string table
	OP_BOOLEAN ret = m_text_table.Open(path, table_name);
	RETURN_IF_ERROR(ret);
	m_text_table.ConfigurePhraseSearch(&m_message_source, 2048);

	m_flush_mode = 0;

	// Set callback for flush message
	g_main_message_handler->SetCallBack(this, MSG_M2_LEXICON_FLUSH, (MH_PARAM_1)this);

	// Initialize mutex
	RETURN_IF_ERROR(m_table_mutex.Init());

	// Initialize thread
	RETURN_IF_ERROR(DesktopThread::Init());

	// Start the thread
	RETURN_IF_ERROR(Start());

	// Check for reindexing: if the lexicon didn't exist, reindex all messages
	if (ret == OpBoolean::IS_FALSE)
		MessageEngine::GetInstance()->GetStore()->Reindex();
	
	return ret;
}


/***********************************************************************************
 ** Find all the file IDs belonging to at least one of the words, Flushes all cached
 ** data before the search
 **
 ** SELexicon::MultiSearch
 ***********************************************************************************/
OP_STATUS SELexicon::MultiSearch(const uni_char * words, OpINT32Vector * result, BOOL match_any, int prefix_search)
{
	DesktopMutexLock mutex(m_table_mutex);

	return m_text_table.MultiSearch(words, result, match_any, prefix_search, PhraseMatcher::AllPhrases);
}


/***********************************************************************************
 ** Find all the message IDs belonging to at least one of the words specified
 ** (partial matches allowed)
 ** Will call back with a MSG_M2_SEARCH_RESULTS message with results of search
 **
 ** SELexicon::AsyncMultiSearch
 ** @param words Words to search for
 ** @param callback Where to deliver the result
 ***********************************************************************************/
OP_STATUS SELexicon::AsyncMultiSearch(const OpStringC& words, MH_PARAM_1 callback)
{
	// Copy the words so that they can be passed on to the thread
	OpAutoPtr<OpString> words_copy (OP_NEW(OpString, ()));
	if (!words_copy.get())
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(words_copy->Set(words));

	// Send search message with higher priority than indexing messages, so that users
	// can search while messages are being indexed
	return PostMessage(MSG_M2_SEARCH, callback, (MH_PARAM_2)words_copy.release(), 1);
}


/***********************************************************************************
 ** Find the indexed words, Flushes all cached data before the search
 **
 ** SELexicon::WordSearch
 ***********************************************************************************/
OP_STATUS SELexicon::WordSearch(const uni_char * word, uni_char ** result, int * result_size)
{
	DesktopMutexLock mutex(m_table_mutex);

	return m_text_table.WordSearch(word, result, result_size);
}


/***********************************************************************************
 ** Handle messages for thread
 **
 ** SELexicon::HandleMessage
 ***********************************************************************************/
void SELexicon::HandleMessage(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
		case MSG_M2_LEXICON_FLUSH:
			OpStatus::Ignore(Commit());
			break;
		case MSG_M2_INSERT_MESSAGE:
			OpStatus::Ignore(ChangeLexicon((message_gid_t)par1, TRUE, (BOOL)par2));
			break;
		case MSG_M2_DELETE_MESSAGE:
			OpStatus::Ignore(ChangeLexicon((message_gid_t)par1, FALSE, FALSE));
			// par1 is expected to be the callback and par2 the message id (hence the intended parameter swap)
			g_thread_tools->PostMessageToMainThread(MSG_M2_MESSAGE_DELETED_FROM_LEXICON, (MH_PARAM_1)par2, (MH_PARAM_2)par1);
			break;
		case MSG_M2_SEARCH:
			OpStatus::Ignore(HandleSearch((OpString*)par2, par1));
			break;
		case MSG_M2_CHECK_LEXICON_CONSISTENCY:
			CheckConsistency();
			break;
		default:
			OP_ASSERT(!"Unknown message received!");
	}
}


/***********************************************************************************
 ** Change the lexicon, adding or removing a message
 **
 ** SELexicon::ChangeLexicon
 ** @param message_id Message to add or remove
 ** @param add TRUE to add message, FALSE to remove
 ** @param headers_only Whether to only considers headers of this message
 ***********************************************************************************/
OP_STATUS SELexicon::ChangeLexicon(message_gid_t message_id, BOOL add, BOOL headers_only)
{
	Message  message;
	Store*	 store	= MessageEngine::GetInstance()->GetStore();

	// Get the message contents
	RETURN_IF_ERROR(store->GetMessage(message, message_id));
	RETURN_IF_ERROR(store->GetMessageData(message));

	// add decoded body words
	if (!headers_only)
	{
		OpString body;

		// Ignore errors, we want as much as we can get
		OpStatus::Ignore(message.QuickGetBodyText(body));

		if (body.HasContent())
			RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId(), body, add));
	}

	// add important headers
	Header::HeaderValue from_header;
	message.GetFrom(from_header);

	RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId() + Indexer::MESSAGE_FROM, from_header, add));

	Header::HeaderValue subject_header;
	message.GetHeaderValue(Header::SUBJECT, subject_header);

	RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId() + Indexer::MESSAGE_SUBJECT, subject_header, add));

	Header::HeaderValue to_header;
	message.GetHeaderValue(Header::TO, to_header);

	RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId() + Indexer::MESSAGE_TO, to_header, add));

	Header::HeaderValue cc_header;
	message.GetHeaderValue(Header::CC, cc_header);

	RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId() + Indexer::MESSAGE_CC, cc_header, add));

	Header::HeaderValue newsgroups_header;
	message.GetHeaderValue(Header::NEWSGROUPS, newsgroups_header);

	RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId() + Indexer::MESSAGE_NEWSGROUPS, newsgroups_header, add));

	Header::HeaderValue replyto_header;
	message.GetHeaderValue(Header::REPLYTO, replyto_header);

	RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId() + Indexer::MESSAGE_REPLYTO, replyto_header, add));

	// other important headers are combined:
	Header::HeaderValue organization_header, keywords_header;
	OpString other_headers;

	message.GetHeaderValue(Header::ORGANIZATION, organization_header);
	other_headers.Append(organization_header);

	message.GetHeaderValue(Header::KEYWORDS, keywords_header);
	other_headers.Append(keywords_header);

	RETURN_IF_ERROR(ChangeWordsInLexicon(message.GetId() + Indexer::MESSAGE_OTHER_HEADERS, other_headers, add));

	if (add)
	{
		// Add message to indexed messages, if we were adding
		m_indexed_messages = OP_NEW(IndexedMessage, (message_id, m_indexed_messages));
		if (!m_indexed_messages)
			return OpStatus::ERR_NO_MEMORY;
	}
	else
	{
		// Remove message from store, we're completely done with it now
		return store->RemoveMessage(message_id);
	}

	return OpStatus::OK;
}


/***********************************************************************************
 ** Change the lexicon, adding or removing words
 **
 ** SELexicon::ChangeWordsInLexicon
 ** @param message_id Message to associate words with
 ** @param words Words to add or remove
 ** @param add TRUE to add words, FALSE to remove
 ***********************************************************************************/
OP_STATUS SELexicon::ChangeWordsInLexicon(message_gid_t id, const OpStringC16& words, BOOL add)
{
	if (words.IsEmpty())
		return OpStatus::OK;

	DesktopMutexLock lock(m_table_mutex);

	if (add)
		RETURN_IF_ERROR(m_text_table.InsertBlock(words.CStr(), id));
	else
		RETURN_IF_ERROR(m_text_table.DeleteBlock(words.CStr(), id));

	lock.Release();

	return RequestCommit();
}


/***********************************************************************************
 ** Do a search in the lexicon for partial matches
 **
 ** SELexicon::HandleSearch
 ***********************************************************************************/
OP_STATUS SELexicon::HandleSearch(OpString* words, MH_PARAM_1 callback)
{
	OpAutoPtr<OpString>          words_anchor(words);
	OpAutoPtr<OpINTSortedVector> results(OP_NEW(OpINTSortedVector, ()));
	OpINT32Vector				 unsorted_results;

	if (!results.get())
		return OpStatus::ERR_NO_MEMORY;

	// Do the search
	DesktopMutexLock lock(m_table_mutex);
	RETURN_IF_ERROR(m_text_table.MultiSearch(words->CStr(), &unsorted_results, FALSE, 4096, PhraseMatcher::AllPhrases));
	RETURN_IF_ERROR(lock.Release());
	
	// Search results should be sorted
	for (unsigned i = 0; i < unsorted_results.GetCount(); i++)
		RETURN_IF_ERROR(results->Insert(unsorted_results.Get(i)));

	// Post message for result
	return g_thread_tools->PostMessageToMainThread(MSG_M2_SEARCH_RESULTS, callback, (MH_PARAM_2)results.release());
}


/***********************************************************************************
 ** Request a commit of the lexicon
 **
 ** SELexicon::RequestCommit
 ***********************************************************************************/
OP_STATUS SELexicon::RequestCommit()
{
	if (m_req_commit)
		return OpStatus::OK;

	RETURN_IF_ERROR(g_thread_tools->PostMessageToMainThread(MSG_M2_LEXICON_FLUSH, (MH_PARAM_1)this, 0, COMMIT_DELAY));
	m_req_commit = TRUE;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Commit the lexicon changes
 **
 ** SELexicon::Commit
 ***********************************************************************************/
OP_STATUS SELexicon::Commit()
{
	m_req_commit = FALSE;

	DesktopMutexLock lock(m_table_mutex);

	// Commit
	RETURN_IF_ERROR(m_text_table.Commit());

	lock.Release();

	// Flag messages as indexed
	for (IndexedMessage* msg = m_indexed_messages; msg; msg = msg->next)
		MessageEngine::GetInstance()->GetStore()->SetMessageFlag(msg->id, Message::IS_WAITING_FOR_INDEXING, FALSE);

	CleanupIndexedMessages();

	return OpStatus::OK;
}


/***********************************************************************************
 **
 **
 ** SELexicon::CleanupIndexedMessages
 ***********************************************************************************/
void SELexicon::CleanupIndexedMessages()
{
	// Remove linked list of messages
	IndexedMessage* msg = m_indexed_messages;
	while (msg)
	{
		IndexedMessage* next = msg->next;
		OP_DELETE(msg);
		msg = next;
	}
	m_indexed_messages = NULL;
}

/***********************************************************************************
 **
 **
 ** SELexicon::CheckConsistency
 ***********************************************************************************/
void SELexicon::CheckConsistency()
{
	DesktopMutexLock mutex(m_table_mutex);
	OpStatus::Ignore(m_text_table.Close());
	MailRecovery recovery;
	OpStatus::Ignore(recovery.CheckAndFixLexicon());
}

/***********************************************************************************
 ** Looks up text in the message store for post-processing results in phrase search
 **
 ** SELexicon::MessageSource::GetDocument
 ** @param id Indexing id that the message is associated with
 ***********************************************************************************/
const uni_char* SELexicon::MessageSource::GetDocument(const INT32& id)
{
	// TODO: we should return NULL here if the search is aborted
	Message message;
	Header::HeaderValue header;
	Store* store = MessageEngine::GetInstance()->GetStore();
	message_gid_t message_id = (message_gid_t)(id & 0x1fffffff);
	Indexer::LexiconIdSpace id_space = (Indexer::LexiconIdSpace)(id & 0xE0000000);

	// Ignoring all OP_STATUS return values below since the result is the same: returning NULL

	// Get the message contents
	store->GetMessage(message, message_id);
	store->GetMessageData(message);

	m_doc.Reset();

	switch (id_space) {
		case Indexer::MESSAGE_BODY:
		{
			// Internally in search_engine, all "id space" records are also added to MESSAGE_BODY,
			// so if there is a hit on MESSAGE_BODY, we must provide all the other stuff
			OpString body;
			Header::HeaderValue subj,from,to,cc,rt,ng,org,keyw;
			int body_len,subj_len,from_len,to_len,cc_len,rt_len,ng_len,org_len,keyw_len;
			message.QuickGetBodyText(body);
			message.GetHeaderValue(Header::SUBJECT, subj);
			message.GetFrom(from);	
			message.GetHeaderValue(Header::TO, to);
			message.GetHeaderValue(Header::CC, cc);
			message.GetHeaderValue(Header::REPLYTO, rt);
			message.GetHeaderValue(Header::NEWSGROUPS, ng);
			message.GetHeaderValue(Header::ORGANIZATION, org);
			message.GetHeaderValue(Header::KEYWORDS, keyw);
			// Efficiency is essential here. We must avoid using OpString::Append for concatenation
			body_len = body.Length();
			subj_len = subj.Length();
			from_len = from.Length();
			to_len   = to  .Length();
			cc_len   = cc  .Length();
			rt_len   = rt  .Length();
			ng_len   = ng  .Length();
			org_len  = org .Length();
			keyw_len = keyw.Length();
			m_doc.Reserve(body_len+subj_len+from_len+to_len+cc_len+rt_len+ng_len+org_len+keyw_len+8);

			if (body.HasContent()) { m_doc.Append(body.CStr(),	body_len); m_doc.Append(UNI_L("\f"), 1); }
			if (subj.HasContent()) { m_doc.Append(subj.CStr(),	subj_len); m_doc.Append(UNI_L("\f"), 1); }
			if (from.HasContent()) { m_doc.Append(from.CStr(),	from_len); m_doc.Append(UNI_L("\f"), 1); }
			if (to  .HasContent()) { m_doc.Append(to.CStr(),	to_len);   m_doc.Append(UNI_L("\f"), 1); }
			if (cc  .HasContent()) { m_doc.Append(cc.CStr(),	cc_len);   m_doc.Append(UNI_L("\f"), 1); }
			if (rt  .HasContent()) { m_doc.Append(rt.CStr(),	rt_len);   m_doc.Append(UNI_L("\f"), 1); }
			if (ng  .HasContent()) { m_doc.Append(ng.CStr(),	ng_len);   m_doc.Append(UNI_L("\f"), 1); }
			if (org .HasContent()) { m_doc.Append(org.CStr(),	org_len);  m_doc.Append(UNI_L("\f"), 1); }
			if (keyw.HasContent()) { m_doc.Append(keyw.CStr(),	keyw_len); }
			break;
		}

		case Indexer::MESSAGE_SUBJECT:
			message.GetHeaderValue(Header::SUBJECT, header);
			m_doc.Append(header.CStr(), header.Length());
			break;

		case Indexer::MESSAGE_FROM:
			message.GetFrom(header);	
			m_doc.Append(header.CStr(), header.Length());
			break;

		case Indexer::MESSAGE_TO:
			message.GetHeaderValue(Header::TO, header);
			m_doc.Append(header.CStr(), header.Length());
			break;

		case Indexer::MESSAGE_CC:
			message.GetHeaderValue(Header::CC, header);
			m_doc.Append(header.CStr(), header.Length());
			break;

		case Indexer::MESSAGE_REPLYTO:
			message.GetHeaderValue(Header::REPLYTO, header);
			m_doc.Append(header.CStr(), header.Length());
			break;

		case Indexer::MESSAGE_NEWSGROUPS:
			message.GetHeaderValue(Header::NEWSGROUPS, header);
			m_doc.Append(header.CStr(), header.Length());
			break;

		case Indexer::MESSAGE_OTHER_HEADERS:
			message.GetHeaderValue(Header::ORGANIZATION, header);
			m_doc.Append(header.CStr(), header.Length());
			message.GetHeaderValue(Header::KEYWORDS, header);
			m_doc.Append(header.CStr(), header.Length());
			break;
	}

	return m_doc.GetData();
}

#endif // M2_SUPPORT
