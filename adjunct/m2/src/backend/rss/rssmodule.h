// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
// $Id$
//
// Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.

#ifndef RSSMODULE_H
#define RSSMODULE_H

#include "adjunct/m2/src/backend/ProtocolBackend.h"
#include "adjunct/m2/src/engine/message.h"
#include "adjunct/m2/src/backend/rss/rss-protocol.h"

class Account;
class Index;
class PrefsFile;


// ***************************************************************************
//
//	RSSBackend
//
// ***************************************************************************

class RSSBackend : public ProtocolBackend
{
public:
	// Construction / destruction.
	RSSBackend(MessageDatabase& database);
    ~RSSBackend();

	// Methods called by engine.
	OP_STATUS MailCommand(URL& url);
	OP_STATUS UpdateFeed(const OpStringC& url, BOOL display_dialogs = TRUE);
	OP_STATUS FetchMessage(const OpStringC8& internet_location, message_index_t index = 0);

	// Methods called by RSSProtocol.
	void FeedCheckingCompleted();
	BOOL GetRSSDialogs() { return m_no_dialogs == 0; }

protected:
	// ProtocolBackend.
    virtual OP_STATUS Init(Account* account);
    virtual AccountTypes::AccountType GetType() const { return AccountTypes::RSS; }
    virtual OP_STATUS SettingsChanged(BOOL startup=FALSE);
    virtual UINT16  GetDefaultPort(BOOL secure) const { return 80; }
	virtual IndexTypes::Type GetIndexType() const { return IndexTypes::NEWSFEED_INDEX; }
	virtual UINT32  GetAuthenticationSupported() { return 1 << AccountTypes::NONE; }
    virtual OP_STATUS PrepareToDie() { return OpStatus::ERR; }
	virtual OP_STATUS FetchHeaders(BOOL enable_signalling) { return OpStatus::ERR_NOT_SUPPORTED; }
	virtual OP_STATUS FetchHeaders(const OpStringC8& internet_location, message_index_t index = 0) { return OpStatus::ERR; }
    virtual OP_STATUS FetchMessages(BOOL enable_signalling);
	virtual OP_STATUS SelectFolder(UINT32 index_id, const OpStringC16& folder);
	virtual OP_STATUS RefreshFolder(UINT32 index_id) { return UpdateFeed(index_id); }
	virtual OP_STATUS RefreshAll();
	virtual OP_STATUS StopFetchingMessages();
	virtual OP_STATUS StopSendingMessage() { return OpStatus::ERR; }
	virtual OP_STATUS Connect() { return OpStatus::OK; }
    virtual OP_STATUS Disconnect() { return OpStatus::OK; }
	virtual OP_STATUS Select(const char* foldername) { return OpStatus::ERR; }
	virtual OP_STATUS FetchMessage(message_index_t idx, BOOL user_request, BOOL force_complete) { return OpStatus::OK; }
	virtual OP_STATUS ServerCleanUp() { return OpStatus::OK; }
	virtual OP_STATUS AcceptReceiveRequest(UINT port_number, UINT id) { return OpStatus::ERR; }
	virtual OP_STATUS InsertExternalMessage(Message& message) { return OpStatus::ERR; }

	virtual const char* GetIcon(BOOL progress_icon) { return progress_icon ? NULL : "Mail Newsfeeds"; }

	virtual UINT32 GetSubscribedFolderCount() { return 0; }
	virtual OP_STATUS GetSubscribedFolderName(UINT32 index, OpString& name) { return OpStatus::OK; }
	virtual OP_STATUS AddSubscribedFolder(OpString& path) { return OpStatus::OK; }
	virtual OP_STATUS RemoveSubscribedFolder(UINT32 index_id);
	virtual void GetAllFolders();

	virtual OP_STATUS CreateFolder(OpString& completeFolderPath, BOOL subscribed) { return OpStatus::OK; }
	virtual OP_STATUS DeleteFolder(OpString& completeFolderPath);

private:
	// No copy or assignment.
    RSSBackend(const RSSBackend&);
	RSSBackend& operator =(const RSSBackend&);

	// Methods.
	OP_STATUS AddToFile(const OpStringC& feed_name, const OpStringC& feed_url, time_t update_frequency, BOOL subscribed);
	OP_STATUS UpdateFeed(UINT32 index_id);
	OP_STATUS UpdateFeedIfNeeded(Index* index, const OpStringC& url = OpStringC());
	OP_STATUS UpdateFeedIfNeeded(UINT32 index_id, const OpStringC& url);

	// Members.
    OpAutoPtr<RSSProtocol>		m_protocol;
	PrefsFile*					m_file;
	OpString					m_new_messages_string;
	index_gid_t					m_feed_index;
	UINT32						m_no_dialogs;
};

#endif
