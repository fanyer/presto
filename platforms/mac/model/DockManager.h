// -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
//
// Copyright (C) 2007 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//
// Adam Minchinton
//

#ifndef __DOCK_MANAGER_H__
#define __DOCK_MANAGER_H__

#ifdef M2_SUPPORT
#include "adjunct/m2/src/include/defs.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/index.h"
#include "adjunct/m2/src/engine/listeners.h"
#endif

#define g_dock_manager (&DockManager::GetInstance())

//////////////////////////////////////////////////////////////////////////////////////////////////////////////

class DockManager 
	: public EngineListener
#ifdef IRC_SUPPORT
	, public ChatListener
#endif
#ifdef WEBSERVER_SUPPORT
	, public OpTreeModel::Listener
#endif
{
private:
	DockManager();
	~DockManager();

public:
	static DockManager &GetInstance();

	// Initialise all the things the Opera Account Manager needs
	OP_STATUS	Init();
	BOOL		IsInited() { return m_inited; }
	void		CleanUp();

	// Bounces the dock icon
	void		BounceDockIcon();

	// EngineListener listeners
	virtual void OnUnreadMailCountChanged() {}
	virtual void OnProgressChanged(const ProgressInfo& progress, const OpStringC& progress_text) {}
	virtual void OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE) {}
	virtual void OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) {}
	virtual void OnIndexChanged(UINT32 index_id);
	virtual void OnActiveAccountChanged() {}
	virtual void OnReindexingProgressChanged(INT32 progress, INT32 total) {}
	virtual void OnNewMailArrived() {}

#ifdef IRC_SUPPORT
	// ChatListener listeners
	virtual void OnChatStatusChanged(UINT16 account_id) {}
	virtual void OnChatReconnecting(UINT16 account_id, const ChatInfo&  room) {}
	virtual void OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat) {}
	virtual void OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information) {}
	virtual void OnChatRoomJoining(UINT16 account_id, const ChatInfo& room) {}
	virtual void OnChatRoomJoined(UINT16 account_id, const ChatInfo& room) {}
	virtual void OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
	virtual void OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) {}
	virtual BOOL OnChatterJoining(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) { return TRUE; }
	virtual void OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) {}
	virtual void OnChatterLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {}
	virtual void OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) {}
	virtual void OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value) {}
	virtual void OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host, const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message, const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels) {}
	virtual void OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room) {}
	virtual void OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id) {}
	virtual void OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread);
#endif // IRC_SUPPORT

#ifdef WEBSERVER_SUPPORT
	// OpTreeModel listeners
	virtual void OnItemAdded(OpTreeModel* tree_model, INT32 pos) {}
	virtual void OnItemChanged(OpTreeModel* tree_model, INT32 pos, BOOL sort);
	virtual void OnItemRemoving(OpTreeModel* tree_model, INT32 pos) {}
	virtual void OnSubtreeRemoving(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
	virtual void OnSubtreeRemoved(OpTreeModel* tree_model, INT32 parent, INT32 pos, INT32 subtree_size) {}
	virtual void OnTreeChanging(OpTreeModel* tree_model) {}
	virtual void OnTreeChanged(OpTreeModel* tree_model) {}
	virtual void OnTreeDeleted(OpTreeModel* tree_model) {}
#endif

private:
	// Draws the badges based on the current counts
	OP_STATUS 	UpdateBadges();

	BOOL 		DrawBadge(OpBitmap* bm, uni_char *string, UINT32 colour, int badgeTop, BOOL align_right = TRUE);

private:
	BOOL m_inited;						// Set to TRUE after a successful Init() call

	int m_unread_mails;
	int m_unread_chats;
#ifdef WEBSERVER_SUPPORT
	int m_show_unite_notification;
#endif
};

#endif // __DOCK_MANAGER_H__
