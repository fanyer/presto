/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
**
** Copyright (C) 1995-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Espen Sand
*/

#ifndef __X11_OPTASKBAR_H__
#define __X11_OPTASKBAR_H__

#include "modules/util/adt/oplisteners.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/engine/listeners.h"

class MailDesktopWindow;


class X11OpTaskbarListener
{
public:
	virtual void OnUnattendedMailCount( UINT32 count ) {}
	virtual void OnUnreadMailCount( UINT32 count ) {}
	virtual void OnUnattendedChatCount( OpWindow* op_window, UINT32 count ) {}
	virtual void OnUniteAttentionCount( UINT32 count ) {}
};

class X11OpTaskbar 
	:public EngineListener
	,public MailNotificationListener 
	,public ChatListener
#ifdef WEBSERVER_SUPPORT
    ,public OpTreeModel::Listener
#endif
{
public:
	virtual ~X11OpTaskbar();

	static OP_STATUS Create(X11OpTaskbar** taskbar);
	OP_STATUS Init();

	OP_STATUS AddX11OpTaskbarListener(X11OpTaskbarListener* listener) {return m_listener.Add(listener);}
	OP_STATUS RemoveX11OpTaskbarListener(X11OpTaskbarListener* listener) {return m_listener.Remove(listener);}

	// EngineListener
	virtual void OnIndexChanged(UINT32 index_id);
	virtual void OnProgressChanged(const ProgressInfo& progress, const OpStringC& progress_text) {}
	virtual void OnUnreadMailCountChanged() {}
	virtual void OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE) {}
	virtual void OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) {}
	virtual void OnActiveAccountChanged() {}
	virtual void OnReindexingProgressChanged(INT32 progress, INT32 total) {}
	virtual void OnNewMailArrived() {}


	// MailNotificationListener

	virtual void NeedNewMessagesNotification(Account* account, unsigned count);
	virtual void NeedNoMessagesNotification(Account* account) {}
	virtual void NeedSoundNotification(const OpStringC& sound_path) {}
	virtual void OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active);

	// ChatListener
	virtual void OnChatStatusChanged(UINT16 account_id) {}
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
	virtual void OnChatReconnecting(UINT16 account_id, const ChatInfo& room) {}


#ifdef WEBSERVER_SUPPORT
	// OpTreeModel::Listener 
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
	INT32 m_unite_notification_count;
	BOOL  m_unread_mail;
	OpVector<MailDesktopWindow> m_active_mail_windows;
	OpListeners<X11OpTaskbarListener> m_listener;
};



#endif // __X11_OPTASKBAR_H__
