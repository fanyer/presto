/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
**
** Copyright (C) 2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#ifndef _NNTPPROTOCOL_
#define _NNTPPROTOCOL_

#include "nntpmodule.h"
#include "adjunct/m2/src/backend/compat/crcomm.h"
#include "adjunct/m2/src/util/autodelete.h"
#include "adjunct/m2/src/engine/message.h"

#include "modules/hardcore/timer/optimer.h"

class NntpBackend;

//MODULE_FILE_START(nntp)
//    class Backend;
//MODULE_FILE_END


class NNTP : public ClientRemoteComm , public OpTimerListener
{
private:
    enum Status
	{
        aborting,
		ready,
        fetching_newgroups,
        fetching_headers,
        fetching_body,
        fetching_article,
        fetching_stat, //STAT isn't implemented
        fetching_xover
    };

private:
    NntpBackend*	m_nntp_backend;
	BOOL			m_use_ssl;
	int				m_empty_network_buffer_count;
    AccountTypes::AuthenticationType m_authentication_method;
    BOOL            m_is_connected;
    BOOL            m_allow_more_commands;
    OpTimer*        m_connection_timer;
    Head*           m_commandlist;
    CommandItem::Commands m_last_command_sent;
    OpString8       m_last_command_param1;
    OpString8       m_last_command_param2;
	UINT8			m_last_flags;
    Status          m_status;
	BOOL			m_ignore_messages;
    UINT32        m_current_count;
    UINT32        m_total_count;
    UINT32        m_current_xover_count;
    UINT32        m_min_xover_count;
    UINT32        m_max_xover_count;

    OpString8       m_reply_buffer;

    //Variables used to store information across multiple NNTP commands
    INT32         m_available_range_from;
    INT32         m_available_range_to;
    OpString8       m_requested_newsgroup; //Newsgroup we have asked for
    OpString8       m_confirmed_newsgroup; //Newsgroup we have got a positive response for
    OpString8       m_requested_range; //Range we have asked for
    OpString8       m_requested_optimized_range; //Range that is part of the requested range, but that is part of optimization and should be removed when receiving messages
    UINT32        m_receiving_nntp_number; //The nntp number of the buffer that is being received, if available (0 if not)

    BOOL            m_can_post;

	ProgressInfo	m_progress;

private:
    /* buffer could be considered const char* if update_buffer_ptr=FALSE */
    UINT32  atoUINT32(char*& buffer, BOOL update_buffer_ptr=TRUE) const;

    /* Uses m_status and m_reply_buffer */
    OP_STATUS ParseAndAppendBuffer(char*& buffer, Status busy_status, OUT UINT32& nntp_number);
    OP_STATUS SkipCurrentLine(char*& buffer);

	OP_STATUS OnError(const char* error_message) const;

    OP_STATUS StartTimeoutTimer(BOOL& timer_started);
    OP_STATUS InsertCommandFromGlobalQueue(); //Inserts a command only if the local queue is empty or next command is Quit
    OP_STATUS ReplaceLoopCommand();
    OP_STATUS SendNextCommand();

    OP_STATUS CheckForAuthenticationRequest(int code, char*& buffer, BOOL& autentication_requested);
    OP_STATUS HandleConnect(int code, char*& buffer);
    OP_STATUS HandleModeReader(int code, char*& buffer);
    OP_STATUS HandleAuthinfoCRAMMD5req(int code, char*& buffer);
    OP_STATUS HandleAuthinfoCRAMMD5(int code, char*& buffer);
    OP_STATUS HandleAuthinfoUser(int code, char*& buffer);
    OP_STATUS HandleAuthinfoPass(int code, char*& buffer);
    OP_STATUS HandleNewgroups(int code, char*& buffer);
    OP_STATUS HandleGroup(int code, char*& buffer);
    OP_STATUS HandleArticle(int code, char*& buffer);
    OP_STATUS HandleXover(int code, char*& buffer);
    OP_STATUS HandlePost(int code, char*& buffer);
    OP_STATUS HandleQuit(int code, char*& buffer);

    OP_STATUS ResetProtocolState();

protected:
	void	  OnClose(OP_STATUS);
	void	  OnRestartRequested();

public:
	NNTP(NntpBackend* backend);
	~NNTP();

    OP_STATUS ProcessReceivedData();
    void      RequestMoreData() {}
    OP_STATUS Init();
	void      OnTimeOut(OpTimer* timer);
    OP_STATUS Connect();
    OP_STATUS Disconnect();
    OP_STATUS SendQueuedMessages();
	OP_STATUS HandleUnsubscribe(const OpStringC& newsgroup);

    Head*     GetCommandListPtr() const {return m_commandlist;}
    BOOL      CurrentCommandMatches(const OpStringC8& newsgroup, CommandItem::Commands command, OpString8* parameter);
    OP_STATUS GetCurrentNewsgroup(OpString8& newsgroup) const {return newsgroup.Set(m_confirmed_newsgroup);}

    BOOL      IsBusy() const {return m_commandlist && !m_commandlist->Empty();}
    BOOL      IsConnected() const {return m_is_connected;}
//	Account::AccountStatus GetStatus() const {return m_connection_status;}
//    UINT32  GetCurrentCount() const {return m_current_count + (m_current_xover_count-m_min_xover_count);}
//    UINT32  GetTotalCount() const {return m_total_count + (m_max_xover_count-m_min_xover_count);}
    void      IncreaseTotalCount(UINT32 count);

	ProgressInfo& GetProgress() { return m_progress; }
};

#endif /* _NNTPPROTOCOL_ */
