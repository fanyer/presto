// -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
//
//
// Copyright (C) 1995-2000 Opera Software AS.  All rights reserved.
//
// This file is part of the Opera web browser.  It may not be distributed
// under any circumstances.
//

#ifndef NNTPMODULE_H
#define NNTPMODULE_H

#include "adjunct/m2/src/backend/ProtocolBackend.h"
#include "modules/util/opfile/opfile.h"

//#define NNTP_LOG_QUEUE

// ----------------------------------------------------

class NNTP;
class Message;

class CommandItem : public Link
{
public:
    enum Commands
	{
		MODE_READER,
        AUTHINFO_CRAMMD5_REQUEST,
        AUTHINFO_CRAMMD5,
        AUTHINFO_USER,
        AUTHINFO_PASS,
        LIST,
        NEWGROUPS,
        GROUP,
        HEAD,
        HEAD_LOOP,
        XOVER,
        XOVER_LOOP,
        ARTICLE,
        ARTICLE_LOOP,
        POST, //Initialize post-command
        POST_MESSAGE, //The message (will be dot-stuffed) to send after having initialized the post-command
        QUIT,
        END_OF_COMMAND
    };

	enum Flags
	{
		ENABLE_SIGNALLING,	//If this is on and messages are fetched, play the bell when going to idle mode
		FIRST_REDIRECT,		//When asking for a spesific message, this flag is false if the message wasn't found on the original server and the request has been forwarded
		IGNORE_RCFILE		//When asking for a range, don't remove read range from it
	};

public:
    CommandItem() : m_flags(0) {;}

    Commands        m_command;
    OpString8       m_param1;
    OpString8       m_param2;
	UINT8			m_flags;

private:
	CommandItem(const CommandItem&);
	CommandItem& operator =(const CommandItem&);
};

class NntpBackend;
class NewsRCItem : public Link
{
public:
    enum Status
	{
		SUBSCRIBED,
        NOT_SUBSCRIBED,
        NEW_NEWSGROUP
    };
public:
    OpString8 newsgroup;
    OpString8 read_range;
    OpString8 kept_range;
    Status    status;

public:
    NewsRCItem() {;}

    OP_STATUS GetUnicodeName(OpString& name, NntpBackend* backend) const;

private:
	NewsRCItem(const NewsRCItem&);
	NewsRCItem& operator =(const NewsRCItem&);
};

// ----------------------------------------------------

class NntpBackend : public ProtocolBackend, public ProgressInfoListener
{
public:

	NntpBackend(MessageDatabase& database);
    ~NntpBackend();

	// MessageBackend:
    OP_STATUS Init(Account* account);
    AccountTypes::AccountType GetType() const {return AccountTypes::NEWS;}
    OP_STATUS SettingsChanged(BOOL startup=FALSE);
	BOOL	  IsBusy() const;
    UINT16  GetDefaultPort(BOOL secure) const { return secure ? 563 : 119; }
	IndexTypes::Type GetIndexType() const {return IndexTypes::NEWSGROUP_INDEX;}
	UINT32    GetAuthenticationSupported() {return (UINT32)1 << AccountTypes::NONE |
												   (UINT32)1 << AccountTypes::PLAINTEXT |
												   /*1<<Account::SIMPLE |*/ //According to rfc2980, "It is recommended that this command not be implemented"
												   /*1<<Account::GENERIC |*/ //CRAM-MD5 will be using this. No need to support it in UI
												   (UINT32)1 << AccountTypes::CRAM_MD5 |
                                                   (UINT32)1 << AccountTypes::AUTOSELECT;}
	BOOL	  SupportsStorageType(AccountTypes::MboxType type)
		{ return type < AccountTypes::MBOX_TYPE_COUNT; }
    OP_STATUS PrepareToDie();

    OP_STATUS FetchHeaders(BOOL enable_signalling) { return OpStatus::ERR; }
    OP_STATUS FetchHeaders(const OpStringC8& internet_location, message_index_t index=0);
	OP_STATUS FetchMessage(message_index_t index, BOOL user_request, BOOL force_complete);
    OP_STATUS FetchMessages(BOOL enable_signalling);
    OP_STATUS FetchMessage(const OpStringC8& internet_location, message_index_t index=0);

	OP_STATUS SelectFolder(UINT32 index_id);
	OP_STATUS RefreshFolder(UINT32 index_id) { return SelectFolder(index_id); }

	OP_STATUS StopFetchingMessages();

    OP_STATUS SendMessage(Message& message, BOOL anonymous);
	OP_STATUS StopSendingMessage() { return OpStatus::ERR; }

    OP_STATUS FetchNewGroups() {return FetchNNTPNewGroups();}

	OP_STATUS ServerCleanUp() {return OpStatus::ERR;}

	OP_STATUS AcceptReceiveRequest(UINT port_number, UINT id) {return OpStatus::ERR;}

    OP_STATUS AddSubscribedFolder(OpString& newsgroup);
	OP_STATUS RemoveSubscribedFolder(UINT32 index_id);
    OP_STATUS CommitSubscribedFolders();

	OP_STATUS InsertExternalMessage(Message& message) { return OpStatus::OK; }

	void      GetAllFolders();
	void      StopFolderLoading();

	const char* GetIcon(BOOL progress_icon) { return progress_icon ? NULL : "Mail Newsgroups"; }

    OP_STATUS Connect();
    OP_STATUS Disconnect();

	OP_STATUS SignalStartSession(const NNTP* connection);
	OP_STATUS SignalEndSession(const NNTP* connection);
	BOOL	  IsInSession(const NNTP* connection) const;

	// ProgressInfoListener:
	void	  OnProgressChanged(const ProgressInfo& progress);
	void	  OnSubProgressChanged(const ProgressInfo& progress)
		{ m_progress.SetSubProgress(progress.GetSubCount(), progress.GetSubTotalCount()); }

public: //Functions used by protocol
    OP_STATUS Fetched(Message* message, BOOL headers_only);
	UINT8	  MaxConnectionCount() const {return m_max_connection_count;}
    int       GetConnectionId(const NNTP* connection) const;
    NNTP*     GetConnectionPtr(int id);
	BOOL      GetMaxDownload(int available_messages, const OpStringC8& group, BOOL force_dont_ask, int& max_download);
    BOOL      IHave(const OpStringC8& message_id) const; //Is message_id already in messageID-cache? (and if it is, is it outgoing?)
    void      AuthenticationFailed(const NNTP* current_connection);
	void	  ConnectionDenied(NNTP* current_connection); //Some servers have a cap on connections (typically 3)
    OP_STATUS MoveQueuedCommand(Head* destination_list, OUT UINT32& article_count, BOOL only_first_command=TRUE) {return MoveQueuedCommand(m_command_list, destination_list, article_count, only_first_command);}
    OP_STATUS AddNewNewsgroups(OpString8& newsgroups); //Will parse all available newsgroups, _and remove them from the buffer_!
    OP_STATUS AddReadRange(const OpStringC8& newsgroup, const OpStringC8& range);
    OP_STATUS NNTPNewGroups() {return FetchNNTPNewGroups();}
	OP_STATUS HandleUnsubscribe(const OpStringC& newsgroup, Head* queue, const OpStringC& current_newsgroup);

    OP_STATUS FindNewsgroupRange(IO OpString8& parameter, OUT OpString8& range, BOOL& set_ignore_rcfile_flag) const;

    OP_STATUS ParseXref(IN const OpStringC8& xref, OUT OpString8& message_id, OUT OpString8& newsserver, OUT OpString8& newsgroup, OUT int& nntp_number) const;

    Account*  GetNextSearchAccount(BOOL first_search=FALSE) const;

    BOOL      IsMessageId(OpString8& string) const;
    BOOL      IsXref(const OpStringC8& string) const;

	void	  SetSignalMessage() {m_signal_new_message = TRUE;}

	void	  AlertNewsgroupEncodingOverride();

#if defined NNTP_LOG_QUEUE
    void      LogQueue();
#endif

	NewsRCItem* FindNewsgroupItem(const OpStringC8& newsgroup, BOOL allow_filesearch = FALSE) const;

private:
    OP_STATUS KillAllConnections(const NNTP* current_connection);
    OP_STATUS MoveQueuedCommand(Head* source_list, Head* destination_list, OUT UINT32& article_count, BOOL only_first_command);
    BOOL      CommandExistsInQueue(const OpStringC8& newsgroup, CommandItem::Commands command, OpString8* parameter);
    BOOL      CommandExistsInQueue(Head* queue, const OpStringC8& newsgroup, const OpStringC8& current_queue_newsgroup,
                                   CommandItem::Commands command, OpString8* parameter);

    OP_STATUS ReadSettings();
    OP_STATUS WriteSettings();
    OP_STATUS CreateNewsrcFileName();

    OP_STATUS ReadRCFile(OUT Head* lists, BOOL only_subscribed=TRUE, UINT16 hash_size=0 /* 0 == no hashing, 'Head* lists' points to the list itself */);
    OP_STATUS WriteRCFile(IN Head* list); //This function will *empty* the incoming list!!! (Use CommitSubscribedFolders() if you want to save subscribed groups)
    void      FreeRCHashedList(Head* list, UINT16 hash_size) const;
    OP_STATUS ParseNextNewsRCItem(OpFile* file, BOOL only_subscribed, OpString8& parse_buffer, char*& parse_ptr, NewsRCItem* newsrc_item) const;
    OP_STATUS AnyNewsgroup(OpString8& newsgroup); //Just return any newsgroup... (Needed when fetching using message-IDs)

    OP_STATUS AddCommands(int count, ...);
    INT8    GetAvailableConnectionId();

    OP_STATUS FetchNNTPNewGroups();
    OP_STATUS FetchNNTPHeaders(OpString8& newsgroup, BOOL enable_signalling);
    OP_STATUS FetchNNTPMessages(OpString8& newsgroup, BOOL enable_signalling);
    OP_STATUS FetchNNTPMessage(const OpStringC8& internet_location, message_index_t index=0, BOOL user_request=TRUE);
    OP_STATUS FetchNNTPMessageByLocation(BOOL headers_only, const OpStringC8& internet_location, const OpStringC8& store_id=NULL);
    OP_STATUS FetchNNTPMessageById(BOOL headers_only, const OpStringC8& message_id, const OpStringC8& store_id=NULL);
    OP_STATUS PostMessage(message_gid_t id);

private:
    NNTP**   m_connections_ptr; //4 connections, according to GNKSA
    UINT8  m_open_connections; //bitmask of open connections
	UINT8	 m_max_connection_count; //Some servers have a cap on connections (typically 3)
	UINT8	 m_current_connections_count;
    BOOL     m_accept_new_connections;

    Head*    m_command_list;

    Head*    m_subscribed_list;
    time_t	 m_last_updated;
    time_t	 m_last_update_request; //Temporary variable. Value is moved to m_last_updated when nntp responds to the newgroups command
    OpString m_newsrc_file;
    BOOL     m_newsrc_file_changed;

    BOOL     m_checked_newgroups;

	int		 m_download_count;
	int		 m_max_download;
	BOOL	 m_ask_for_max_download;
	BOOL	 m_signal_new_message;
	BOOL	 m_send_queued_when_done;

	BOOL	 m_has_alerted_newsgroup_encoding_override;
	BOOL	 m_in_session;

    //Listener for subscribe groups etc
    Head*    m_report_to_ui_list;

	NntpBackend(const NntpBackend&);
	NntpBackend& operator =(const NntpBackend&);
};

// ----------------------------------------------------

#endif // NNTPMODULE_H
