// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// Copyright (C) 1995-2005 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#include "core/pch.h"

#ifdef M2_SUPPORT

#include "rssmodule.h"
#include "rss-protocol.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/indexer.h"
#include "adjunct/m2/src/include/mailfiles.h"
#include "adjunct/m2/src/util/misc.h"

#include "modules/util/htmlify.h"
#include "adjunct/desktop_util/adt/hashiterator.h"
#include "adjunct/desktop_util/string/htmlify_more.h"
#include "adjunct/desktop_util/string/stringutils.h"
#include "adjunct/desktop_util/string/hash.h"

#include "modules/locale/oplanguagemanager.h"
#include "modules/search_engine/SingleBTree.h"

#if NEWSFEEDS_SUPPORT_ENCLOSURES
#include "modules/url/url_man.h"
#endif

#include "modules/windowcommander/src/TransferManager.h"

struct NewsfeedConstants
{
	static const long NewsfeedTimeout          = 60000;
	static const int  MaxSimultaneousDownloads = 4;
};

//****************************************************************************
//
//	RSSProtocol
//
//****************************************************************************

RSSProtocol::RSSProtocol(RSSBackend& backend)
:	m_backend(backend)
{
}


RSSProtocol::~RSSProtocol()
{
	// Clear all the waiting transfers. Must do this before stopping the
	// running transfers.
	UINT32 transfers_count = m_transfers_waiting.GetCount();
	while (transfers_count)
	{
		transfers_count--;
		DeleteTransferItem(m_transfers_waiting.Get(transfers_count));
	}
	m_transfers_waiting.Clear();

	// Stop all the active downloads.
	for (PointerSetIterator<OpTransferItem> it(m_active_downloads); it; it++)
	{
		DeleteTransferItem(it.GetData());
	}
	m_active_downloads.RemoveAll();

	g_main_message_handler->UnsetCallBacks(this);
}


OP_STATUS RSSProtocol::Init()
{
	g_main_message_handler->SetCallBack(this, MSG_M2_TIMEOUT, reinterpret_cast<MH_PARAM_1>(this));

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FetchMessages(const OpStringC& feed_url)
{
	OpTransferItem* transfer_item = 0;
	RETURN_IF_ERROR(CreateTransferItem(transfer_item, feed_url));

	if (transfer_item == 0)
		return OpStatus::ERR;

	// If we are already fetching, or scheduled to fetch this item, we
	// don't want to do anything.
	if (transfer_item->GetIsRunning())
		return OpStatus::ERR;
	else if (m_transfers_waiting.Find(transfer_item) != -1)
		return OpStatus::ERR;

	// Enqueue the download if we are already downloading enough feeds
	// as it is.
	if (m_active_downloads.GetCount() < NewsfeedConstants::MaxSimultaneousDownloads)
		RETURN_IF_ERROR(StartLoading(transfer_item));
	else
		OpStatus::Ignore(m_transfers_waiting.Add(transfer_item));

	OpStatus::Ignore(UpdateFeedCheckedTime(0, feed_url));
	return OpStatus::OK;
}


OP_STATUS RSSProtocol::StopFetching()
{
	// Clear all the waiting transfers. Must do this before stopping the
	// running transfers.
	m_transfers_waiting.Clear();

	// Stop all the active downloads.
	for (PointerSetIterator<OpTransferItem> it(m_active_downloads); it; it++)
	{
		OpTransferItem* current_transfer = it.GetData();
		OP_ASSERT(current_transfer != 0);

		current_transfer->Clear();
	}

	// Clear the active download set.
	m_active_downloads.RemoveAll();

	return OpStatus::OK;
}


void RSSProtocol::OnProgress(URL& url, TransferStatus status)
{
	HandleProgress(0, url, status);
}


void RSSProtocol::OnProgress(OpTransferItem* transfer_item,
	TransferStatus status)
{
	OP_ASSERT(transfer_item != 0);

	// We received something; reset timeout
	g_main_message_handler->RemoveDelayedMessage(MSG_M2_TIMEOUT,
											 reinterpret_cast<MH_PARAM_1>(this),
											 reinterpret_cast<MH_PARAM_2>(transfer_item));

	if (status == TRANSFER_DONE ||
		status == TRANSFER_ABORTED ||
		status == TRANSFER_FAILED)
	{
		// Unset the listener.
		transfer_item->SetTransferListener(0);

		// Remove this transfer from the set of active downloads.
		OpStatus::Ignore(m_active_downloads.Remove(transfer_item));

		// There should be room to start another download now.
		OP_ASSERT(m_active_downloads.GetCount() < NewsfeedConstants::MaxSimultaneousDownloads);

		if (m_transfers_waiting.GetCount() > 0)
		{
			OpTransferItem* next_transfer = m_transfers_waiting.Remove(0);
			OP_ASSERT(next_transfer != 0);

			OpStatus::Ignore(StartLoading(next_transfer));
		}
	}
	else
	{
		// Restart timeout
		g_main_message_handler->PostMessage(MSG_M2_TIMEOUT,
										reinterpret_cast<MH_PARAM_1>(this),
										reinterpret_cast<MH_PARAM_2>(transfer_item),
										NewsfeedConstants::NewsfeedTimeout);
	}

	URL* url = transfer_item->GetURL();
	HandleProgress(transfer_item, *url, status);
}


void RSSProtocol::OnRedirect(OpTransferItem* transferItem, URL* redirect_from,
	URL* redirect_to)
{
	// const short redirect_code = redirect_from->GetRep()->GetDataStorage()->GetAttribute(URL::KHTTP_Response_Code);

	// Remember the original url.
	OpString original_url_string;
	OpStatus::Ignore(redirect_from->GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI, original_url_string));

	if (original_url_string.IsEmpty())
		return;

	RedirectedNewsfeedInfo *redirect_info = OP_NEW(RedirectedNewsfeedInfo, (transferItem));
	if (!redirect_info)
		return;

	OpStatus::Ignore(redirect_info->Init(original_url_string));

	if (OpStatus::IsError(m_redirect_info_transfer_item.Add(transferItem, redirect_info)))
	{
		OP_DELETE(redirect_info);
		return;
	}
	OpStatus::Ignore(m_redirect_info_url.Add(redirect_info->OriginalURL().CStr(), redirect_info));
}


void RSSProtocol::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	// Received  a timeout; stop this transfer
	if (msg == MSG_M2_TIMEOUT)
		OnProgress(reinterpret_cast<OpTransferItem*>(par2), TRANSFER_FAILED);
}


OP_STATUS RSSProtocol::FinishedParsing(FeedParser* parser)
{
	m_backend.FeedCheckingCompleted();
	return m_current_parsers.Delete(parser);
}


OP_STATUS RSSProtocol::CreateTransferItem(OpTransferItem*& transfer_item,
	const OpStringC& url) const
{
	// Any transfer item we ask the transfer manager for will live as long as
	// RSSProtocol lives, they are always reused.

	// First check if we have an item that was redirected.
	if (m_redirect_info_url.Contains(url.CStr()))
	{
		// We do have an item, but we can't ask the transfer manager for it,
		// because it has changed the associated url, while we try to create
		// one using the original url.
		RedirectedNewsfeedInfo* redirect_info = 0;
		OpStatus::Ignore(m_redirect_info_url.GetData(url.CStr(), &redirect_info));

		if (redirect_info == 0)
			return OpStatus::ERR;

		// This will create a transfer item that will try to retrieve the feed
		// from the redirect url, not the original url. But for this session,
		// this is a great thing. That way we use the updated url without
		// saving it when Opera exits.
		transfer_item = redirect_info->TransferItem();
	}
	else
	{
		// Create a new one, or get one already created from the transfer
		// manager.
		BOOL already_created = FALSE;
		RETURN_IF_ERROR(g_transferManager->GetTransferItem(&transfer_item,
			url.CStr(), &already_created));
		((TransferItem*&)(transfer_item))->SetShowTransfer(FALSE);
#ifdef WIC_CAP_TRANSFER_WEBFEED
		((TransferItem*&)(transfer_item))->SetType(TransferItem::TRANSFERTYPE_WEBFEED_DOWNLOAD);
#endif
	}

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::DeleteTransferItem(OpTransferItem* transfer_item) const
{
	g_transferManager->ReleaseTransferItem(transfer_item);
	return OpStatus::OK;
}


OP_STATUS RSSProtocol::GetOriginalFeedURLString(OpString& url_string,
	OpTransferItem* transfer_item, URL& url, BOOL delete_redirect_info)
{
	// Do we have any redirect info saved for this item?
	RedirectedNewsfeedInfo* redirect_info = 0;
	m_redirect_info_transfer_item.GetData(transfer_item, &redirect_info);

	if (redirect_info != 0)
	{
		RETURN_IF_ERROR(url_string.Set(redirect_info->OriginalURL()));
		if (delete_redirect_info)
		{
			m_redirect_info_transfer_item.Remove(transfer_item, &redirect_info);
			m_redirect_info_url.Remove(redirect_info->OriginalURL().CStr(), &redirect_info);

			m_redirect_info_transfer_item.Delete(redirect_info);
		}
	}
	else
	{
		// Use the url string from the passed url.
		RETURN_IF_ERROR(url.GetAttribute(URL::KUniName_Username_Password_Escaped_NOT_FOR_UI, url_string));
	}

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::HandleProgress(OpTransferItem* transfer_item,
	URL& url, TransferStatus status)
{
	// This function only returns OpStatus::OK (and doesn't call m_backend.FeedCheckingCompleted()
	// if an asynchronous parser was successfully setup.
	//
	// In all other cases, we should notify the backend that we're not
	// going to do anything more with this TransferItem by calling FeedCheckingCompleted().

	OpString original_feed_url;
	if (OpStatus::IsSuccess(GetOriginalFeedURLString(original_feed_url, transfer_item, url, FALSE)))
	{
		switch (status)
		{
			case TRANSFER_DONE :
			{
				WriteToLog(original_feed_url, "Feed loading finished");
	
				// Create the parser and start parsing.
				OpAutoPtr<FeedParser> parser(OP_NEW(FeedParser, (*this)));
				if (!parser.get())
					break;

				if (OpStatus::IsSuccess(parser->Init(url, original_feed_url)) &&
					OpStatus::IsSuccess(m_current_parsers.Add(parser.get())))
				{
					parser.release();
					return OpStatus::OK;
				}
				else
				{
					WriteToLog(original_feed_url, "Feed parsing failed");
				}
				break;
			}
			case TRANSFER_ABORTED :
			case TRANSFER_FAILED :
			{
				WriteToLog(original_feed_url, "Feed loading failed / aborted");
				break;
			}
			default:
			{
				return OpStatus::OK;
			}
		}
	}


	m_backend.FeedCheckingCompleted();
	return OpStatus::ERR;
}


OP_STATUS RSSProtocol::StartLoading(OpTransferItem* transfer_item)
{
	if (transfer_item == 0)
		return OpStatus::ERR_NULL_POINTER;

	// Start the transfer.
	transfer_item->Clear();

	URL* url = transfer_item->GetURL();
	OP_ASSERT(url != 0);

	OpString url_string;
	OpStatus::Ignore(url->GetAttribute(URL::KUniName, url_string));
	WriteToLog(url_string, "Starting to load feed");

#ifdef _DEBUG // don't bother asking for status if we don't need it
	const URLStatus url_status = url->Status(TRUE);
	OP_ASSERT(url_status != URL_LOADING &&
		url_status != URL_LOADING_WAITING &&
		url_status != URL_LOADING_FROM_CACHE);
#endif

	url->SetAttribute(URL::KDisableProcessCookies, TRUE);

	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->StartLoading(url));

	// Register ourselves as listener.
	transfer_item->SetTransferListener(this);

	// Add transfer to set of active downloads.
	RETURN_IF_ERROR(m_active_downloads.Add(transfer_item));

	// Start timeout
	g_main_message_handler->PostMessage(MSG_M2_TIMEOUT,
									reinterpret_cast<MH_PARAM_1>(this),
									reinterpret_cast<MH_PARAM_2>(transfer_item),
									NewsfeedConstants::NewsfeedTimeout);

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::UpdateFeedCheckedTime(Index* index, const OpStringC& feed_url)
{
	if (index == 0 && feed_url.HasContent())
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(
			m_backend.GetAccountPtr(), feed_url, 0, UNI_L(""), FALSE, FALSE);
	}

	if (index == 0)
		return OpStatus::ERR_NULL_POINTER;

	const time_t current_time = MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->CurrentTime();
	index->SetLastUpdateTime(current_time);

	return OpStatus::OK;
}


void RSSProtocol::WriteToLog(const OpStringC& feed_url, const OpStringC& text) const
{
	OpString8 text8;
	OpStatus::Ignore(text8.Set(text.CStr()));

	WriteToLog(feed_url, text8);
}


void RSSProtocol::WriteToLog(const OpStringC& feed_url, const OpStringC8& text) const
{
	OpString8 feed_url8;
	OpStatus::Ignore(feed_url8.Set(feed_url.CStr()));

	if (feed_url8.IsEmpty())
		OpStatus::Ignore(feed_url8.Set("None"));

	OpString8 heading;
	heading.AppendFormat("Newsfeed url: %s", feed_url8.CStr());
	OpStatus::Ignore(m_backend.Log(heading, text));
}


//****************************************************************************
//
//	RSSProtocol::RedirectedNewsfeedInfo
//
//****************************************************************************

RSSProtocol::RedirectedNewsfeedInfo::RedirectedNewsfeedInfo(
	OpTransferItem* transfer_item)
:	m_transfer_item(transfer_item)
{
}


OP_STATUS RSSProtocol::RedirectedNewsfeedInfo::Init(
	const OpStringC& original_url)
{
	RETURN_IF_ERROR(m_original_url.Set(original_url));
	return OpStatus::OK;
}


//****************************************************************************
//
//	RSSProtocol::NewsfeedID
//
//****************************************************************************
RSSProtocol::NewsfeedID::NewsfeedID(const OpStringC16& unique_id,
									message_gid_t m2_id)
{
	m_data[0] = unique_id.HasContent() ? Hash::String(unique_id.CStr()) & 0x7FFFFFFF : 0;
	m_data[1] = m2_id;

	OpString8 md5;

	if (OpStatus::IsSuccess(OpMisc::CalculateMD5Checksum(reinterpret_cast<const char*>(unique_id.CStr()), UNICODE_SIZE(unique_id.Length()), md5)))
		op_memcpy(m_md5, md5.CStr(), 32);
	else
		op_memset(m_md5, 0, 32);
}


BOOL RSSProtocol::NewsfeedID::operator<(const NewsfeedID& rhs) const
{
	if (m_data[0] < rhs.m_data[0])
		return TRUE;

	if (m_data[0] == rhs.m_data[0])
		return op_strncmp(m_md5, rhs.m_md5, 32) < 0;

	return FALSE;
}


//****************************************************************************
//
//	RSSProtocol::FeedParser
//
//****************************************************************************
RSSProtocol::FeedParser::FeedParser(RSSProtocol& parent)
	: m_parser(this)
	, m_current_chunk_pos(0)
	, m_parent(parent)
	, m_index_id(0)
	, m_index_created(FALSE)
#if defined(M2_KESTREL_BETA_COMPATIBILITY) || defined(M2_MERLIN_COMPATIBILITY)
	, m_in_upgrade(FALSE)
#endif // M2_KESTREL_BETA_COMPATIBILITY || M2_MERLIN_COMPATIBILITY
{
}


RSSProtocol::FeedParser::~FeedParser()
{
	OpStatus::Ignore(m_newsfeed_id_tree.Close());
	g_main_message_handler->UnsetCallBacks(this);
}


OP_STATUS RSSProtocol::FeedParser::Init(URL& feed_url, const OpStringC& original_feed_url)
{
	RETURN_IF_ERROR(m_original_url.Set(original_feed_url));
	
	// Make sure we try to interpret the content as xml (to get
	// characters decoded to utf-16 by the data descriptor).
	if (feed_url.GetAttribute(URL::KContentType, TRUE) != URL_XML_CONTENT)
		RETURN_IF_ERROR(feed_url.SetAttribute(URL::KMIME_ForceContentType, "text/xml"));

	RETURN_IF_ERROR(m_input.Init(feed_url));
	if (!m_input.HasMoreChunks())
		return OpStatus::ERR;

	// Fetch the content location.
	OpString content_location;
	URL content_loc_url = feed_url.GetAttribute(URL::KHTTPContentLocationURL);

	if (!content_loc_url.IsEmpty())
		RETURN_IF_ERROR(content_loc_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, content_location, TRUE));

	if (content_location.IsEmpty())
		RETURN_IF_ERROR(feed_url.GetAttribute(URL::KUniName_Username_Password_NOT_FOR_UI, content_location, TRUE));

	RETURN_IF_ERROR(m_parser.Init(content_location));

	// Start the parse
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_M2_PARSE_FEED, reinterpret_cast<MH_PARAM_1>(this)));
	g_main_message_handler->PostMessage(MSG_M2_PARSE_FEED, reinterpret_cast<MH_PARAM_1>(this), 0);
	
	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::HandleFeedParsingDone(WebFeed* feed)
{
	BOOL   show_dialogs = m_parent.m_backend.GetRSSDialogs();
	Index* index		= MessageEngine::GetInstance()->GetIndexById(m_index_id);

	if (index)
	{
		// Update the feed name.
		if (index->GetUpdateNameManually() || !index->HasName())
		{
			if (feed != 0 && feed->GetTitle() && feed->GetTitle()->Data())
			{
				OpString feed_title;
				OpStatus::Ignore(FormatTitle(feed->GetTitle()->Data(), feed_title));

				OpStatus::Ignore(index->SetName(feed_title.CStr()));
				index->SetUpdateNameManually(FALSE);
			}
		}

		// Update time to live setting, if it exists.
		if ((feed != 0) &&
			(index->GetUpdateFrequency() != -1) &&
			((time_t)feed->GetMinUpdateInterval() * 60 > index->GetUpdateFrequency()))
		{
			index->SetUpdateFrequency(feed->GetMinUpdateInterval() * 60);
		}

		// Signal backend to tell that the feed checking is completed.
		OpString index_name;
		OpStatus::Ignore(index->GetName(index_name));

#if defined(M2_KESTREL_BETA_COMPATIBILITY) || defined(M2_MERLIN_COMPATIBILITY)
		if (m_in_upgrade)
		{
			// Upgrade is now finished, remove old search indexes that were used to store hashes
			// Don't remove search 0, contains feed URL
			for (int i = index->GetSearchCount() - 1; i >= 1; i--)
				index->RemoveSearch(i);
		}
#endif // M2_KESTREL_BETA_COMPATIBILITY || M2_MERLIN_COMPATIBILITY
	}

	if (m_index_created && m_index_id && show_dialogs)
		MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->ShowIndex(m_index_id, FALSE);

	return OpStatus::OK;
}


void RSSProtocol::FeedParser::OnEntryLoaded ( OpFeedEntry* entry, OpFeedEntry::EntryStatus, BOOL new_entry )
{
	if ( !entry )
		return;

	RETURN_VOID_IF_ERROR(CreateIndexIfNeeded(entry->GetFeed()));

	// See if we already have an existing message for this entry
	if (MessageExists(entry))
		return;

	// Create a message ID for the item
	OpString8 message_id;
	RETURN_VOID_IF_ERROR ( CreateMessageIDForEntry ( entry, message_id ) );

	// Create a multipart e-mail message
	MultipartAlternativeMessageCreator message_creator;
	RETURN_VOID_IF_ERROR ( message_creator.Init() );

	// Add text/html content.
	TempBuffer html;
	RETURN_VOID_IF_ERROR ( FormatHTMLEntry ( entry, html ) );
	RETURN_VOID_IF_ERROR ( message_creator.AddMultipart ( "text/html", html.GetStorage() ) );

	OpAutoPtr<Message> msg ( message_creator.CreateMessage ( m_parent.m_backend.GetAccountPtr()->GetAccountId() ) );
	if ( !msg.get() )
		return;

	// Set various header values
	// Don't return on author errors, we're fine without it as well
	OpString author_name, author_email, title;

	if ( entry->GetAuthor() 
#ifdef WEBFEEDS_CAP_FEED_ENTRY_HAS_AUTHOR_EMAIL
		 || entry->GetAuthorEmail()
#endif // WEBFEEDS_CAP_FEED_ENTRY_HAS_AUTHOR_EMAIL
	   )
	{
		OpStatus::Ignore ( FormatTitle ( entry->GetAuthor(), author_name ) );
#ifdef WEBFEEDS_CAP_FEED_ENTRY_HAS_AUTHOR_EMAIL
		OpStatus::Ignore ( FormatTitle ( entry->GetAuthorEmail(), author_email ) );
#endif // WEBFEEDS_CAP_FEED_ENTRY_HAS_AUTHOR_EMAIL
	}
	else if ( entry->GetFeed()->GetAuthor() )
	{
		OpStatus::Ignore ( FormatTitle ( entry->GetFeed()->GetAuthor(), author_name ) );
	}
	else if ( entry->GetFeed()->GetTitle() )
	{
		OpStatus::Ignore ( FormatTitle ( entry->GetFeed()->GetTitle()->Data(), author_name ) );
	}

	// Make valid 'From' address
	OpString author;
	OpStatus::Ignore(Header::From::GetFormattedEmail(author_name, author_email, 0, author));
	OpStatus::Ignore(msg->SetHeaderValue(Header::FROM, author));

	OpString Title;
	RETURN_VOID_IF_ERROR ( FormatTitle ( entry->GetTitle()->Data(), title ) );
	RETURN_VOID_IF_ERROR ( msg->SetHeaderValue ( Header::SUBJECT, title ) );

	RETURN_VOID_IF_ERROR ( msg->SetHeaderValue ( Header::MESSAGEID, message_id ) );
	msg->SetFlag ( Message::IS_NEWSFEED_MESSAGE, TRUE );

	// Convert publication date from ms to s
	time_t pub_date = ( time_t ) ( entry->GetPublicationDate() / 1000 );
	RETURN_VOID_IF_ERROR ( msg->SetDateHeaderValue ( Header::DATE, pub_date ) );

	RETURN_VOID_IF_ERROR ( msg->CopyCurrentToOriginalHeaders() );

	// Create message.
	if ( m_parent.m_backend.GetAccountPtr() )
	{
		// Add to account
		RETURN_VOID_IF_ERROR ( m_parent.m_backend.GetAccountPtr()->Fetched ( *msg.get() ) );

		// Add to indexed tree of fetched messages
		OpString unique_id;
		RETURN_VOID_IF_ERROR ( CreateUniqueID ( entry, unique_id ) );

		NewsfeedID entry_id ( unique_id, msg->GetId() );
		RETURN_VOID_IF_ERROR ( m_newsfeed_id_tree.Insert ( entry_id ) );

		// Add to index that belongs to this RSS feed
		Index* index = MessageEngine::GetInstance()->GetIndexById(m_index_id);
		if (!index)
			return;
		RETURN_VOID_IF_ERROR ( index->NewMessage ( msg->GetId() ) );
	}
}


void RSSProtocol::FeedParser::ParsingDone(WebFeed* feed, OpFeed::FeedStatus status)
{
	if (feed)
	{
		if (!OpFeed::IsFailureStatus(status))
			CreateIndexIfNeeded(feed);
		HandleFeedParsingDone(feed);
	}
	
}


void RSSProtocol::FeedParser::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_M2_PARSE_FEED)
	{	
		if (OpStatus::IsError(ParseFeed()))
		{
			m_parent.FinishedParsing(this);
		}
	}
}


OP_STATUS RSSProtocol::FeedParser::ParseFeed()
{
	// If there is no current_chunk_pos, we need to get a new chunk
	if (m_current_chunk_pos == 0)
		RETURN_IF_ERROR(m_input.NextChunk(m_current_chunk));
	
	if (m_current_chunk.IsEmpty())
		return OpStatus::ERR;
	
	const uni_char* input		 = m_current_chunk.CStr() + m_current_chunk_pos;
	unsigned		input_length = uni_strlen(input);
	unsigned		consumed	 = 0;
	
	// Do a (partial) parse
	RETURN_IF_ERROR(m_parser.Parse(input,
								   input_length,
								   !m_input.HasMoreChunks(),
								   &consumed));
	
	if (consumed >= input_length)
		m_current_chunk_pos = 0;
	else
		m_current_chunk_pos += consumed;
	
	// Check if there's more to do, or if we're finished now
	if (m_current_chunk_pos > 0 || m_input.HasMoreChunks())
		g_main_message_handler->PostMessage(MSG_M2_PARSE_FEED, reinterpret_cast<MH_PARAM_1>(this), 0);
	else
		m_parent.FinishedParsing(this);

	return OpStatus::OK;
}


BOOL RSSProtocol::FeedParser::MessageExists(OpFeedEntry* entry)
{
	OpString unique_id;

	if (OpStatus::IsError(CreateUniqueID(entry, unique_id)))
		return FALSE;

#if defined(M2_KESTREL_BETA_COMPATIBILITY) || defined(M2_MERLIN_COMPATIBILITY)
	// When we're upgrading, we don't have the hash data in the newsfeed id tree. Instead, we
	// rely on the message-id to see if we already have the message locally. This may
	// lead to some 'redownloads' when messages were locally deleted, but are still available
	// from the RSS feed - this should be a minor inconvenience (arjanl 20080331)
	if (m_in_upgrade)
	{
		OpString8 message_id;

		if (OpStatus::IsError(CreateMessageIDForEntry(entry, message_id)))
			return FALSE;

		message_gid_t m2_id = MessageEngine::GetInstance()->GetStore()->GetMessageByMessageId(message_id);
		if (!m2_id)
			return FALSE;

		// The message exists, create an entry for it
		NewsfeedID new_key(unique_id, m2_id);
		if (OpStatus::IsError(m_newsfeed_id_tree.Insert(new_key)))
			return FALSE;

		return TRUE;
	}
#endif // M2_KESTREL_BETA_COMPATIBILITY || M2_MERLIN_COMPATIBILITY

	NewsfeedID key(unique_id);

	return m_newsfeed_id_tree.Search(key) == OpBoolean::IS_TRUE;
}


OP_STATUS RSSProtocol::FeedParser::CreateIndexIfNeeded(OpFeed* feed)
{
	if (m_index_id)
		return OpStatus::OK;

	OpString feed_title;

	OpFeedContent* feed_title_content = feed->GetTitle();
	if (feed_title_content)
		RETURN_IF_ERROR(FormatTitle(feed_title_content->Data(), feed_title));

	// Create an index for the feed we're parsing, if needed.
	Index* index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(m_parent.m_backend.GetAccountPtr(), m_original_url, 0, feed_title, FALSE, FALSE);
	if (!index)
	{
		index = MessageEngine::GetInstance()->GetIndexer()->GetSubscribedFolderIndex(m_parent.m_backend.GetAccountPtr(), m_original_url, 0, feed_title, TRUE, FALSE);
		if (!index)
			return OpStatus::ERR;
		m_index_created = TRUE;
		index->SetUpdateNameManually(TRUE);
	}

	return SetIndex(index);
}


OP_STATUS RSSProtocol::FeedParser::SetIndex(Index* index)
{
	if (m_index_id == index->GetId())
		return OpStatus::OK;

	// Set the index
	m_index_id = index->GetId();

#if defined(M2_KESTREL_BETA_COMPATIBILITY) || defined(M2_MERLIN_COMPATIBILITY)
	// Set if we're in the process of upgrading
	m_in_upgrade = index ? index->GetSearchCount() > 1 : FALSE;
#endif // M2_KESTREL_BETA_COMPATIBILITY || M2_MERLIN_COMPATIBILITY

	// Open new newsfeed-id tree
	// Construct path of btree file
	OpString filename;
	RETURN_IF_ERROR(MailFiles::GetNewsfeedFilename(m_index_id,filename));

	// Open btree file
	return m_newsfeed_id_tree.Open(filename.CStr(), BlockStorage::OpenReadWrite, M2_BS_BLOCKSIZE);
}


OP_STATUS RSSProtocol::FeedParser::CreateMessageIDForEntry(OpFeedEntry* entry, OpString8& message_id) const
{
	OpString8 hash;
	OpString  input;

	// Message ID contains a hash of the unique ID
	RETURN_IF_ERROR(CreateUniqueID(entry, input));
	RETURN_IF_ERROR(OpMisc::CalculateMD5Checksum(
					reinterpret_cast<char*>(input.CStr()), UNICODE_SIZE(input.Length()), hash));

	return message_id.AppendFormat("<%s@rss.opera.com>", hash.CStr());
}


OP_STATUS RSSProtocol::FeedParser::CreateUniqueID(OpFeedEntry* entry, OpString& unique_id) const
{
	if (entry->GetGuid())
		return unique_id.Set(entry->GetGuid());

	URL url = entry->GetPrimaryLink();

	RETURN_IF_ERROR(url.GetAttribute(URL::KUniName, unique_id));

	OpFeedContent* title = entry->GetTitle();

	if (title)
		RETURN_IF_ERROR(unique_id.Append(title->Data()));

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::FormatTitle(const OpStringC& title, OpString& formatted_title) const
{
	RETURN_IF_ERROR(UnquoteText(title, formatted_title));

	// Strip away leading / ending newlines, tabs and spaces, they just look bad in
	// the e-mail display.
	RETURN_IF_ERROR(StringUtils::Strip(formatted_title, UNI_L("\r\n\t ")));
	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::FormatHTMLEntry(OpFeedEntry* entry,
									   TempBuffer& html) const
{
	RETURN_IF_ERROR(CreateHTMLHeader(entry, html));
	RETURN_IF_ERROR(AddEmbeddedEnclosures(entry, html));
	RETURN_IF_ERROR(CreateHTMLContent(entry, html));
	RETURN_IF_ERROR(CreateHTMLLinks(entry, html));
	RETURN_IF_ERROR(CreateHTMLFooter(entry, html));

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::CreateHTMLHeader(OpFeedEntry* entry, TempBuffer& html) const
{
	// Create valid html.
	RETURN_IF_ERROR(html.Append("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01//EN\" \"http://www.w3.org/TR/html4/strict.dtd\">\r\n"));
	RETURN_IF_ERROR(html.Append("<html>\r\n<head>\r\n"));

	{
		OpString base_uri;
		RETURN_IF_ERROR(HTMLifyText(entry->GetContent()->GetBaseURI(), base_uri, TRUE));

		RETURN_IF_ERROR(html.AppendFormat(UNI_L("<base href=\"%s\">\r\n"),
						base_uri.CStr()));
	}

	{
		OpString style_file, resolved_style_file;
		RETURN_IF_ERROR(g_folder_manager->GetFolderPath(OPFILE_STYLE_FOLDER, style_file));
		RETURN_IF_ERROR(style_file.Append(OPERA_FEED_CSS_FILENAME));
		if (g_url_api->ResolveUrlNameL(style_file, resolved_style_file))
			RETURN_IF_ERROR(html.AppendFormat(UNI_L("<link rel=\"stylesheet\" href=\"%s\" />\r\n"), resolved_style_file.CStr()));
	}

	{
		OpString title;
		RETURN_IF_ERROR(HTMLifyText(entry->GetTitle()->Data(), title, TRUE));

		RETURN_IF_ERROR(html.AppendFormat(UNI_L("\r\n<title>%s</title>\r\n</head>\r\n<body class=\"feedEntryBody\">\r\n"),
						title.CStr()));
	}

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::CreateHTMLContent(OpFeedEntry* entry, TempBuffer& html) const
{
	OpString feed_content;
	RETURN_IF_ERROR(feed_content.Set(entry->GetContent()->Data()));

	if (entry->GetContent()->IsPlainText())
		RETURN_IF_ERROR(HTMLifyText(feed_content, FALSE));

	if (feed_content.HasContent())
	{
		RETURN_IF_ERROR(html.Append("<div class=\"feedEntryContent\">\r\n"));
		RETURN_IF_ERROR(html.Append(feed_content));
		RETURN_IF_ERROR(html.Append("\r\n</div>\r\n"));
	}

	return OpStatus::OK;
}

OP_STATUS RSSProtocol::FeedParser::AddEmbeddedEnclosures(OpFeedEntry* entry, TempBuffer& html) const
{
	RETURN_IF_ERROR(html.Append("<div class=\"feedEntryEnclosureLinks\">\r\n"));
	RETURN_IF_ERROR(html.Append("<hr>\r\n"));
	OpString enclosure_caption_singular;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_ENCLOSURE_LINKS_HEADER_SINGULAR, enclosure_caption_singular));
	OpString enclosure_caption_plural;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_ENCLOSURE_LINKS_HEADER_PLURAL, enclosure_caption_plural));
	RETURN_IF_ERROR(AddHTMLLinks(entry, OpFeedLinkElement::Enclosure, enclosure_caption_singular, enclosure_caption_plural, UNI_L("feedEntryEnclosureLinks"), html));
	return html.Append("</div>\r\n");
}


OP_STATUS RSSProtocol::FeedParser::CreateHTMLLinks(OpFeedEntry* entry, TempBuffer& html) const
{
	// Links.
	RETURN_IF_ERROR(html.Append("<div class=\"feedEntryLinks\">\r\n"));
	RETURN_IF_ERROR(html.Append("<hr>\r\n"));

	OpString alternate_caption_singular;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_ALTERNATE_LINKS_HEADER_SINGULAR, alternate_caption_singular));

	OpString alternate_caption_plural;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_ALTERNATE_LINKS_HEADER_PLURAL, alternate_caption_plural));

	OpString related_caption_singular;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_RELATED_LINKS_HEADER_SINGULAR, related_caption_singular));

	OpString related_caption_plural;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_RELATED_LINKS_HEADER_PLURAL, related_caption_plural));

	OpString via_caption_singular;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_VIA_LINKS_HEADER_SINGULAR, via_caption_singular));

	OpString via_caption_plural;
	RETURN_IF_ERROR(g_languageManager->GetString(Str::S_NEWSFEED_VIA_LINKS_HEADER_PLURAL, via_caption_plural));

	RETURN_IF_ERROR(AddHTMLLinks(entry, OpFeedLinkElement::Alternate, alternate_caption_singular, alternate_caption_plural, UNI_L("feedEntryAlternateLinks"), html));
	RETURN_IF_ERROR(AddHTMLLinks(entry, OpFeedLinkElement::Related,	  related_caption_singular,	  related_caption_plural,	UNI_L("feedEntryRelatedLinks"),   html));
	RETURN_IF_ERROR(AddHTMLLinks(entry, OpFeedLinkElement::Via,		  via_caption_singular,		  via_caption_plural,		UNI_L("feedEntryViaLinks"),		  html));
	return html.Append("</div>\r\n");
}


OP_STATUS RSSProtocol::FeedParser::CreateHTMLFooter(OpFeedEntry* entry, TempBuffer& html) const
{
	return html.Append("</body>\r\n</html>\r\n");
}


OP_STATUS RSSProtocol::FeedParser::AddHTMLLinks(OpFeedEntry* entry,
									OpFeedLinkElement::LinkRel relationship,
									const OpStringC& prefix_singular, const OpStringC& prefix_plural,
									const OpStringC& classname, TempBuffer& html) const
{
	OpVector<OpFeedLinkElement> links;

	for (unsigned i = 0; i < entry->LinkCount(); i++)
	{
		OpFeedLinkElement* link = entry->GetLink(i);
		if (link && link->Relationship() == relationship)
			RETURN_IF_ERROR(links.Add(link));
	}

	if (links.GetCount() > 0)
	{
		RETURN_IF_ERROR(html.AppendFormat(UNI_L("<em>%s</em>\r\n"),
						links.GetCount() > 1 ? prefix_plural.CStr() : prefix_singular.CStr()));

		RETURN_IF_ERROR(html.AppendFormat(UNI_L("<ul class=\"%s\">\r\n"),
						classname.CStr()));

		for (unsigned i = 0; i < links.GetCount(); i++)
		{
			OpFeedLinkElement* link = links.Get(i);

			if (link->HasURI())
			{
				OpString link_title;
				RETURN_IF_ERROR(HTMLifyText(link->Title(), link_title, TRUE));

				OpString link_uri, link_uri_unformatted;
				RETURN_IF_ERROR(link->URI().GetAttribute(URL::KUniName_With_Fragment, link_uri_unformatted));
				RETURN_IF_ERROR(HTMLifyText(link_uri_unformatted, link_uri, TRUE));

				OpString link_href;
				if (link->HasType() && link->Type().Find("image/") != KNotFound)
				{
					RETURN_IF_ERROR(link_href.AppendFormat(UNI_L("<img src=\"%s\" />"), link_uri.CStr()));
				}
				else if (link->HasType() && (link->Type() == ("audio/ogg") || link->Type().Find("audio/wav") != KNotFound))
				{
					RETURN_IF_ERROR(link_href.AppendFormat(UNI_L("<audio src=\"%s\" controls=\"controls\">"), link_uri.CStr()));
				}
				else if (link->HasType() && (link->Type() == "video/ogg" || link->Type() == "video/webm"))
				{
					RETURN_IF_ERROR(link_href.AppendFormat(UNI_L("<video src=\"%s\" controls=\"controls\">"), link_uri.CStr()));
				}
				else
				{
					RETURN_IF_ERROR(link_href.AppendFormat(UNI_L("<a href=\"%s\">%s</a>"),
									   link_uri.CStr(), link->HasTitle() ? link_title.CStr() : link_uri.CStr()));
				}

				RETURN_IF_ERROR(html.Append("<li>"));
				RETURN_IF_ERROR(html.Append(link_href));
				RETURN_IF_ERROR(html.Append("</li>\r\n"));
			}
		}

		RETURN_IF_ERROR(html.Append("</ul>\r\n"));
	}

	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::HTMLifyText(OpString& text, BOOL no_link) const
{
	OpString output;
	RETURN_IF_ERROR(HTMLifyText(text, output, no_link));

	RETURN_IF_ERROR(text.Set(output));
	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::HTMLifyText(const OpStringC& input, OpString& output,
								   BOOL no_link) const
{
	HTMLify_string(output, input.CStr(), input.Length(), no_link);
	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::UnquoteText(OpString& text) const
{
	RETURN_IF_ERROR(MessageEngine::GetInstance()->GetGlueFactory()->GetBrowserUtils()->ReplaceEscapes(text));
	return OpStatus::OK;
}


OP_STATUS RSSProtocol::FeedParser::UnquoteText(const OpStringC& input, OpString& output) const
{
	RETURN_IF_ERROR(output.Set(input));
	RETURN_IF_ERROR(UnquoteText(output));

	return OpStatus::OK;
}

#endif //M2_SUPPORT
