/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef LISTENERS_H
#define LISTENERS_H

#include "adjunct/m2/src/include/defs.h"

class Importer;
class ProgressInfo;
class Account;
class ChatInfo;
class Indexer;
class Index;
class OpWindow;
class MailDesktopWindow;
class Message;
class StoreMessage;

class EngineListener
{
public:
	virtual ~EngineListener() {}

	virtual void OnImporterProgressChanged(const Importer* importer, const OpStringC& infoMsg, OpFileLength current, OpFileLength total, BOOL simple = FALSE) = 0;
	virtual void OnImporterFinished(const Importer* importer, const OpStringC& infoMsg) = 0;
	virtual void OnIndexChanged(UINT32 index_id) = 0;
	virtual void OnActiveAccountChanged() = 0;
	virtual void OnReindexingProgressChanged(INT32 progress, INT32 total) = 0;
};

class AccountListener
{
public:
	virtual ~AccountListener() {}

	virtual void OnAccountAdded(UINT16 account_id) = 0;
	virtual void OnAccountRemoved(UINT16 account_id, AccountTypes::AccountType account_type) = 0;
	virtual void OnFolderAdded(UINT16 account_id, const OpStringC& name, const OpStringC& path, BOOL subscribedLocally, INT32 subscribedOnServer, BOOL editable) = 0;
	virtual void OnFolderRemoved(UINT16 account_id, const OpStringC& path) = 0;
	virtual void OnFolderRenamed(UINT16 account_id, const OpStringC& old_path, const OpStringC& new_path) = 0;
	virtual void OnFolderLoadingCompleted(UINT16 account_id) = 0;
};

class MessageListener
{
public:
	virtual ~MessageListener() {}

	virtual void OnMessageBodyChanged(message_gid_t message_id) = 0;	///< will get message_id = -1 after multiple changes, used in UI
	virtual void OnMessageChanged(message_gid_t message_id) = 0;	///< will get message_id = -1 after multiple changes, used in UI
	virtual void OnMessagesRead(const OpINT32Vector& message_ids, BOOL read) {};		///< will always be called, even during multiple changes : optional.
	virtual void OnMessageReplied(message_gid_t message_id) {};		///< will always be called, even during multiple changes : optional.
};

class InteractionListener
{
public:
	virtual ~InteractionListener() {}

	virtual void OnChangeNickRequired(UINT16 account_id, const OpStringC& old_nick) = 0;
	virtual void OnRoomPasswordRequired(UINT16 account_id, const OpStringC& room) = 0;
	virtual void OnError(UINT16 account_id, const OpStringC& errormessage, const OpStringC& context, EngineTypes::ErrorSeverity severity = EngineTypes::GENERIC_ERROR) = 0;
	virtual void OnYesNoInputRequired(UINT16 account_id, EngineTypes::YesNoQuestionType type, OpString* sender=NULL, OpString* param=NULL) = 0;
};

#ifdef IRC_SUPPORT
class ChatListener
{
public:
	virtual ~ChatListener() {}

	virtual void OnChatStatusChanged(UINT16 account_id) = 0;
	virtual void OnChatMessageReceived(UINT16 account_id, EngineTypes::ChatMessageType type, const OpStringC& message, const ChatInfo& chat_info, const OpStringC& chatter, BOOL is_private_chat) = 0;
	virtual void OnChatServerInformation(UINT16 account_id, const OpStringC& server, const OpStringC& information) = 0;
	virtual void OnChatRoomJoining(UINT16 account_id, const ChatInfo& room) = 0;
	virtual void OnChatRoomJoined(UINT16 account_id, const ChatInfo& room) = 0;
	virtual void OnChatRoomLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) = 0;
	virtual void OnChatRoomLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& kicker, const OpStringC& kick_reason) = 0;
	virtual BOOL OnChatterJoining(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) = 0;
	virtual void OnChatterJoined(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, BOOL is_operator, BOOL is_voiced, const OpStringC& prefix, BOOL initial) = 0;
	virtual void OnChatterLeaving(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) = 0;
	virtual void OnChatterLeft(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& message, const OpStringC& kicker) = 0;
	virtual void OnChatPropertyChanged(UINT16 account_id, const ChatInfo& room, const OpStringC& chatter, const OpStringC& changed_by, EngineTypes::ChatProperty property, const OpStringC& property_string, INT32 property_value) = 0;
	virtual void OnWhoisReply(UINT16 account_id, const OpStringC& nick, const OpStringC& user_id, const OpStringC& host, const OpStringC& real_name, const OpStringC& server, const OpStringC& server_info, const OpStringC& away_message, const OpStringC& logged_in_as, BOOL is_irc_operator, int seconds_idle, int signed_on, const OpStringC& channels) = 0;
	virtual void OnInvite(UINT16 account_id, const OpStringC& nick, const ChatInfo& room) = 0;
	virtual void OnFileReceiveRequest(UINT16 account_id, const OpStringC& sender, const OpStringC& filename, UINT32 file_size, UINT port_number, UINT id) = 0;
	virtual void OnUnattendedChatCountChanged(OpWindow* op_window, UINT32 unread) = 0;
	virtual void OnChatReconnecting(UINT16 account_id, const ChatInfo& room) = 0;
};

#endif // IRC_SUPPORT

class IndexerListener
{
	friend class Indexer;
	virtual OP_STATUS IndexAdded(Indexer *indexer, UINT32 index_id) = 0;
	virtual OP_STATUS IndexRemoved(Indexer *indexer, UINT32 index_id) = 0;
	virtual OP_STATUS IndexChanged(Indexer *indexer, UINT32 index_id) = 0;
	virtual OP_STATUS IndexNameChanged(Indexer *indexer, UINT32 index_id) = 0;
	virtual OP_STATUS IndexIconChanged(Indexer* indexer, UINT32 index_id) = 0;
	virtual OP_STATUS IndexVisibleChanged(Indexer *indexer, UINT32 index_id) = 0;
	virtual OP_STATUS IndexParentIdChanged(Indexer *indexer, UINT32 index_id,  UINT32 old_parent_id, UINT32 new_parent_id) { return OpStatus::OK; }
	virtual OP_STATUS IndexKeywordChanged(Indexer *indexer, UINT32 index_id, const OpStringC8& old_keyword, const OpStringC8& new_keyword) = 0;
	virtual OP_STATUS KeywordAdded(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword) = 0;
	virtual OP_STATUS KeywordRemoved(Indexer* indexer, message_gid_t message_id, const OpStringC8& keyword) = 0;


public:
	virtual ~IndexerListener() {}
};

class CategoryListener
{
public:
	virtual ~CategoryListener() {}
	virtual OP_STATUS CategoryAdded(UINT32 index_id) = 0;
	virtual OP_STATUS CategoryRemoved(UINT32 index_id) = 0;
	virtual OP_STATUS CategoryUnreadChanged(UINT32 index_id, UINT32 unread_messages) = 0;
	virtual OP_STATUS CategoryNameChanged(UINT32 index_id) = 0;
	virtual OP_STATUS CategoryVisibleChanged(UINT32 index_id, BOOL visible) = 0;
};

class MailNotificationListener
{
public:
	virtual ~MailNotificationListener() {}
	virtual void NeedNewMessagesNotification(Account* account, unsigned count) = 0;
	virtual void NeedNoMessagesNotification(Account* account) = 0;
	virtual void NeedSoundNotification(const OpStringC& sound_path) = 0;
	virtual void OnMailViewActivated(MailDesktopWindow* mail_window, BOOL active) = 0;
};

class ProgressInfoListener
{
public:
	virtual ~ProgressInfoListener() {}

	/** Called when anything in progress has changed
	  */
	virtual void OnProgressChanged(const ProgressInfo& progress) = 0;
	/** Called when the sub progress has changed
	  */
	virtual void OnSubProgressChanged(const ProgressInfo& progress) = 0;
	/** Called when the status (connected/disconnected or busyness) has changed
	  */
	virtual void OnStatusChanged(const ProgressInfo& progress) {}
};

#endif // LISTENERS_H
