// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// $Id$
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef RSS_PROTOCOL_H
#define RSS_PROTOCOL_H

#include "modules/util/adt/opvector.h"

#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/glue/util.h"
#include "adjunct/m2/src/engine/message.h"
#include "modules/search_engine/SingleBTree.h"
#include "modules/webfeeds/src/webfeedparser.h"
#include "modules/windowcommander/OpTransferManager.h"

class Account;
class Index;
class RSSBackend;

//****************************************************************************
//
//	RSSProtocol
//
//****************************************************************************

class RSSProtocol
:	public OpTransferListener,
	public MessageObject
{
public:
	// Constructor / destructor.
	RSSProtocol(RSSBackend& backend);
	~RSSProtocol();

	OP_STATUS Init();

	// Methods.
	OP_STATUS FetchMessages(const OpStringC& feed_url);
	OP_STATUS StopFetching();

	void OnProgress(URL& url, TransferStatus status);

private:
	class FeedParser;
	friend class FeedParser;

	// OpTransferListener
	virtual void OnProgress(OpTransferItem* transferItem, TransferStatus status);
	virtual void OnReset(OpTransferItem* transferItem) { }
	virtual void OnRedirect(OpTransferItem* transferItem, URL* redirect_from, URL* redirect_to);

	// MessageObject
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// Methods.
	OP_STATUS FinishedParsing(FeedParser* parser);
	OP_STATUS CreateTransferItem(OpTransferItem*& transfer_item, const OpStringC& url) const;
	OP_STATUS DeleteTransferItem(OpTransferItem* transfer_item) const;
	OP_STATUS GetOriginalFeedURLString(OpString& url_string, OpTransferItem* transfer_item, URL& url, BOOL delete_redirect_info);
	OP_STATUS HandleProgress(OpTransferItem* transfer_item, URL& url, TransferStatus status);
	OP_STATUS StartLoading(OpTransferItem* transfer_item);
	OP_STATUS UpdateFeedCheckedTime(Index* index, const OpStringC& feed_url = OpStringC());

	OP_STATUS UpdateLanguageStrings();

	void WriteToLog(const OpStringC& feed_url, const OpStringC& text) const;
	void WriteToLog(const OpStringC& feed_url, const OpStringC8& text) const;

	class RedirectedNewsfeedInfo
	{
	public:
		// Construction.
		RedirectedNewsfeedInfo(OpTransferItem* transfer_item);
		OP_STATUS Init(const OpStringC& original_url);

		// Methods.
		OpTransferItem* TransferItem() { return m_transfer_item; }
		const OpTransferItem* TransferItem() const { return m_transfer_item; }
		const OpStringC& OriginalURL() const { return m_original_url; }

	private:
		// No copy or assignment.
		RedirectedNewsfeedInfo(const RedirectedNewsfeedInfo& other);
		RedirectedNewsfeedInfo& operator=(const RedirectedNewsfeedInfo& other);

		// Members.
		OpTransferItem* m_transfer_item;
		OpString m_original_url;
	};

protected:
	friend class MailRecovery;
	class NewsfeedID
	{
	public:
		NewsfeedID(const OpStringC16& unique_id, message_gid_t m2_id = 0);

		unsigned	  GetNewsfeedHash() const { return m_data[0]; }
		message_gid_t GetM2ID()			const { return m_data[1]; }

		BOOL operator<(const NewsfeedID& rhs) const;

	private:
		UINT32 m_data[2];
		char   m_md5[32];
	};
private:	
	class FeedParser : public WebFeedParserListener, public MessageObject
	{
	public:
		FeedParser(RSSProtocol& parent);
		~FeedParser();

		OP_STATUS	Init(URL& feed_url, const OpStringC& original_feed_url);

		OP_STATUS	HandleFeedParsingDone(WebFeed* feed);
		
		// WebFeedParserListener.
		virtual void OnEntryLoaded(OpFeedEntry* entry, OpFeedEntry::EntryStatus, BOOL new_entry);
		virtual void ParsingDone(WebFeed* feed, OpFeed::FeedStatus status);
		virtual void ParsingStarted() {}

		// MessageObject
		virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	private:
		OP_STATUS	ParseFeed();
		BOOL		MessageExists(OpFeedEntry* entry);
		OP_STATUS	CreateIndexIfNeeded(OpFeed* feed);
		OP_STATUS	SetIndex(Index* index);
		OP_STATUS	CreateMessageIDForEntry(OpFeedEntry* entry, OpString8& message_id) const;
		OP_STATUS	CreateUniqueID(OpFeedEntry* entry, OpString& unique_id) const;
		OP_STATUS	FormatTitle(const OpStringC& title, OpString& formatted_title) const;
		OP_STATUS	FormatHTMLEntry(OpFeedEntry* entry, TempBuffer& html) const;
		OP_STATUS	CreateHTMLHeader(OpFeedEntry* entry, TempBuffer& html) const;
		OP_STATUS	CreateHTMLContent(OpFeedEntry* entry, TempBuffer& html) const;
		OP_STATUS	CreateHTMLLinks(OpFeedEntry* entry, TempBuffer& html) const;
		OP_STATUS   AddEmbeddedEnclosures(OpFeedEntry* entry, TempBuffer& html) const;
		OP_STATUS	CreateHTMLFooter(OpFeedEntry* entry, TempBuffer& html) const;
		OP_STATUS	AddHTMLLinks(OpFeedEntry* entry, OpFeedLinkElement::LinkRel relationship, const OpStringC& prefix_singular, const OpStringC& prefix_plural, const OpStringC& classname, TempBuffer& html) const;
		OP_STATUS	HTMLifyText(OpString& text, BOOL no_link) const;
		OP_STATUS	HTMLifyText(const OpStringC& input, OpString& output, BOOL no_link) const;
		OP_STATUS	UnquoteText(OpString& text) const;
		OP_STATUS	UnquoteText(const OpStringC& input, OpString& output) const;

		// Parser and accessories
		WebFeedParser			m_parser;
		BrowserUtilsURLStream	m_input;
		OpString				m_current_chunk;
		unsigned				m_current_chunk_pos;

		// Variables needed when receiving parsed input
		RSSProtocol&			m_parent;
		index_gid_t				m_index_id;							///< ID of the index for this feed
		BOOL					m_index_created;					///< Whether an index was created for this feed
		SingleBTree<NewsfeedID> m_newsfeed_id_tree;					///< Newsfeed tree with information about fetched feeds
		OpString				m_original_url;						///< Original URL (before redirects)
#if defined(M2_KESTREL_BETA_COMPATIBILITY) || defined(M2_MERLIN_COMPATIBILITY)
		BOOL					m_in_upgrade;						///< Whether we are currently upgrading an installation without proper rss hashes
#endif // M2_KESTREL_BETA_COMPATIBILITY || M2_MERLIN_COMPATIBILITY
	};

	// Members.
	RSSBackend&	m_backend;

	OpAutoVector<FeedParser> m_current_parsers;

	OpAutoPointerHashTable<OpTransferItem, RedirectedNewsfeedInfo> m_redirect_info_transfer_item;
	OpStringHashTable<RedirectedNewsfeedInfo> m_redirect_info_url;

	OpPointerSet<OpTransferItem> m_active_downloads;

	OpVector<OpTransferItem> m_transfers_waiting;
};

#endif // RSS_PROTOCOL_H
