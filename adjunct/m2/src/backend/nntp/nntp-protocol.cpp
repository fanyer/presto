/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/

#include "core/pch.h"

#ifdef M2_SUPPORT

#include <ctype.h>

#include "nntp-protocol.h"
#include "nntpmodule.h"
#include "nntprange.h"

#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/engine/progressinfo.h"
#include "adjunct/m2/src/engine/store.h"
#include "adjunct/m2/src/util/authenticate.h"

#define CONNECTION_TIMEOUT		1*60*1000 //1 minute timeout for the first two connections
#define NICE_CONNECTION_TIMEOUT 10*1000   //10 seconds for the last two connections

#define EMPTY_NETWORK_BUFFER_BAILOUT_COUNT	 (m_use_ssl ? 200 : 0) //How many empty network buffers in a row do we accept?


#define NNTP_100_help_text_follows      100 // help text follows
#define NNTP_199_debug_output           199 // debug output

#define NNTP_200_ready_posting_ok       200 // server ready - posting allowed
#define NNTP_201_ready_no_posting       201 // server ready - no posting allowed
#define NNTP_202_slave_status_noted     202 // slave status noted
#define NNTP_205_closing_connection     205 // closing connection - goodbye
#define NNTP_211_group_selected         211 // n f l s group selected
#define NNTP_215_newsgroup_list         215 // list of newsgroup follows
#define NNTP_220_art_retr_head_body     220 // n <a> article retrieved - head and body follows
#define NNTP_221_art_retr_head          221 // n <a> article retrieved - head follows
#define NNTP_222_art_retr_body          222 // n <a> article retrieved - body follows
#define NNTP_223_art_retrieved          223 // n <a> article retrieved - request text separately
#define NNTP_224_overview_info_follows  224 // overview information follows
#define NNTP_230_new_article_list       230 // list of new articles by message-id follows
#define NNTP_231_new_newsgroup_list     231 // list of new newsgroups follows
#define NNTP_235_art_transferred_ok     235 // articles transferred ok
#define NNTP_240_article_posted_ok      240 // article posted ok
#define NNTP_250_accept_authentication  250 // username/password accepted
#define NNTP_281_accept_authentication  281 // username/password accepted

#define NNTP_335_transfer_article       335 // send article to be transferred. End with <CR-LF>.<CR-LF>
#define NNTP_340_post_article           340 // send article to be posted. End with <CR-LF>.<CR-LF>
#define NNTP_350_accept_method          350 // method accepted, password needed
#define NNTP_381_accept_method          381 // method accepted, password needed

#define NNTP_400_service_discont        400 // service discontinued
#define NNTP_411_no_such_news_group     411 // no such news group
#define NNTP_412_no_group_selected      412 // no newsgroup has been selected
#define NNTP_420_no_current_article     420 // no current article has been selected
#define NNTP_421_no_next_article        421 // no next article in this group
#define NNTP_422_no_prev_article        422 // no previous article in this group
#define NNTP_423_no_such_art_number     423 // no such article number in this group
#define NNTP_430_no_such_article        430 // no such article found
#define NNTP_435_article_not_wanted     435 // article not wanted - do not send it
#define NNTP_436_transfer_failed        436 // transfer failed - try again later
#define NNTP_437_article_rejected       437 // article rejected - do not try again
#define NNTP_440_posting_not_allowd     440 // posting not allowed
#define NNTP_441_posting_failed         441 // posting failed
#define NNTP_450_require_authentication 450 // password needed
#define NNTP_452_Illegal_password       452 // Illegal password
#define NNTP_480_require_authentication 480 // password needed
#define NNTP_482_Illegal_password       482 // Illegal password
#define NNTP_490_maybe_authenticate		490 // M2 internal message requesting authentication if authentication method != NONE

#define NNTP_500_cmd_not_recognized     500 // command not recognized
#define NNTP_501_cmd_syntax_error       501 // command syntax error
#define NNTP_502_permission_denied      502 // access restriction or permission denied
#define NNTP_503_program_fault          503 // program fault - command not performed


NNTP::NNTP(NntpBackend* backend)
: m_nntp_backend(backend),
  m_use_ssl(FALSE),
  m_empty_network_buffer_count(0),
  m_authentication_method(AccountTypes::AUTOSELECT),
  m_is_connected(FALSE),
  m_allow_more_commands(TRUE),
  m_connection_timer(NULL),
  m_commandlist(NULL),
  m_last_command_sent(CommandItem::END_OF_COMMAND),
  m_last_flags(0),
  m_status(ready),
  m_ignore_messages(FALSE),
  m_current_count(0),
  m_total_count(0),
  m_current_xover_count(0),
  m_min_xover_count(0),
  m_max_xover_count(0),
  m_available_range_from(-1),
  m_available_range_to(-1),
  m_receiving_nntp_number(0),
  m_can_post(TRUE)
{
}

NNTP::~NNTP()
{
    OP_DELETE(m_connection_timer);
    m_connection_timer = NULL;

    m_nntp_backend = NULL;

    if (m_is_connected)
        Disconnect();

#if defined NNTP_LOG_QUEUE
    if (m_nntp_backend) m_nntp_backend->Log("Before NNTP::~NNTP Clear", "");
    if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

    if (m_commandlist)
    {
        m_commandlist->Clear();
        OP_DELETE(m_commandlist);
    }
}

OP_STATUS NNTP::Init()
{
    if (!m_nntp_backend)
        return OpStatus::ERR_NULL_POINTER;

    OP_ASSERT(m_commandlist==NULL);
    m_commandlist = OP_NEW(Head, ());
    if (!m_commandlist)
        return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(m_progress.AddListener(m_nntp_backend));

    return OpStatus::OK;
}

void NNTP::OnTimeOut(OpTimer* timer)
{
    if (timer != m_connection_timer)
        return;

    OP_DELETE(m_connection_timer);
    m_connection_timer = NULL;

    if (!m_commandlist->Empty())
    {
        OP_ASSERT(0); //FG: This should never happen
        SendNextCommand();
    }
    else
        Disconnect();
}

OP_STATUS NNTP::ResetProtocolState()
{
	m_last_command_sent = CommandItem::END_OF_COMMAND;
    m_last_command_param1.Empty();
    m_last_command_param2.Empty();
	m_last_flags = 0;
    m_reply_buffer.Empty();
    m_requested_newsgroup.Empty();
    m_confirmed_newsgroup.Empty();
    m_requested_range.Empty();
    m_requested_optimized_range.Empty();
    m_authentication_method = AccountTypes::AUTOSELECT;
    return OpStatus::OK;
}

void NNTP::OnClose(OP_STATUS rc)
{
	if (rc != OpStatus::OK)
	{
		OpStatus::Ignore(Disconnect());
	}
}

void NNTP::OnRestartRequested()
{
	OpStatus::Ignore(StopLoading());
	OpStatus::Ignore(Connect());
}

OP_STATUS NNTP::Connect()
{
	if (!m_nntp_backend->HasRetrievedPassword())
		return m_nntp_backend->RetrievePassword();

	OP_STATUS ret;
    OpString8 server;
    UINT16  port;

    m_allow_more_commands = TRUE;

    OP_ASSERT(m_commandlist); //This should be initialized in Init
    if (!m_commandlist)
        return OpStatus::ERR_NULL_POINTER;

    if ((ret=InsertCommandFromGlobalQueue()) != OpStatus::OK)
        return ret;

    if (m_commandlist->Empty()) //No need to connect if the command-queue is empty
        return OpStatus::OK;

    //We have commands -> connect!
    if ((ret=m_nntp_backend->GetServername(server)) != OpStatus::OK)
        return ret;

	if (server.IsEmpty())
		return OpStatus::ERR_NULL_POINTER;

    if ((ret=m_nntp_backend->GetPort(port)) != OpStatus::OK)
        return ret;

	m_nntp_backend->GetUseSecureConnection(m_use_ssl);
	m_empty_network_buffer_count = 0;

	m_nntp_backend->Log("Connecting...", "");
    if ((ret=StartLoading(server.CStr(), "nntp", port, m_use_ssl, m_use_ssl)) != OpStatus::OK)
        return ret;

	m_progress.SetCurrentAction(ProgressInfo::CONNECTING);

    return OpStatus::OK;
}

OP_STATUS NNTP::Disconnect()
{
    m_allow_more_commands = FALSE;

#if defined NNTP_LOG_QUEUE
    if (m_nntp_backend) m_nntp_backend->Log("Before NNTP::Disconnect Clear", "");
    if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

	if (m_status==fetching_xover)
	{
        //Mark the articles read in the .newsrc-file
        if (m_nntp_backend && !m_nntp_backend->IsMessageId(m_requested_range))
        {
			char xover_range[22];
			sprintf(xover_range, "%u-%u", m_min_xover_count, m_current_xover_count);

			OP_ASSERT(m_confirmed_newsgroup.HasContent());
			OpStatus::Ignore(m_nntp_backend->AddReadRange(m_confirmed_newsgroup, xover_range));
        }

		//Some cleanup
        m_requested_range.Empty();
        m_requested_optimized_range.Empty();
	}

    m_commandlist->Clear(); //Remove all pending commands;

    OP_STATUS ret = OpStatus::OK;
    if (m_is_connected && (m_status == ready || m_status == aborting)) //Gracefully quit if we have the possibility
    {
        CommandItem* item = OP_NEW(CommandItem, ());
        if (!item)
            return OpStatus::ERR_NO_MEMORY;

        item->m_command = CommandItem::QUIT;
        item->Into(m_commandlist);

#if defined NNTP_LOG_QUEUE
        if (m_nntp_backend) m_nntp_backend->Log("After NNTP::Disconnect Into", "");
        if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif
        ret = SendNextCommand();
    }

    if (m_status != aborting)
        m_status = ready;

	m_ignore_messages = FALSE;

    OP_DELETE(m_connection_timer);
    m_connection_timer = NULL;

    m_is_connected = FALSE;

    OpStatus::Ignore(StopLoading());

    OpStatus::Ignore(ResetProtocolState());

	m_progress.Reset();
	m_current_count = m_total_count = m_current_xover_count = m_min_xover_count = m_max_xover_count = 0;
	if (m_nntp_backend && m_nntp_backend->IsInSession(this))
		m_nntp_backend->SignalEndSession(this);

    return ret;
}


OP_STATUS NNTP::SendQueuedMessages()
{
    if (IsConnected() &&
		m_last_command_sent != CommandItem::END_OF_COMMAND &&
		m_last_command_sent != CommandItem::QUIT)
	{
        return OpStatus::OK; //Already busy sending messages
	}

	if (m_nntp_backend && !m_nntp_backend->IsInSession(this))
		OpStatus::Ignore(m_nntp_backend->SignalStartSession(this));

    if (!IsConnected())
        return Connect();

    return SendNextCommand();
}


UINT32 NNTP::atoUINT32(char*& buffer, BOOL update_buffer_ptr) const
{
    if (!buffer)
        return 0;

    char* tmp = buffer;
    UINT32 reply_code = 0;
    while (*tmp>='0' && *tmp<='9')
    {
        reply_code = reply_code*10 + (*(tmp++)-'0');
    }

    if (update_buffer_ptr)
        buffer = tmp;

    return reply_code;
}


OP_STATUS NNTP::ParseAndAppendBuffer(char*& buffer, Status busy_status, OUT UINT32& nntp_number)
{
    OP_STATUS ret;

    if (m_status==ready)
    {
        if (busy_status==fetching_headers ||
            busy_status==fetching_body ||
            busy_status==fetching_article) //Extract the NNTP number from the reply
        {
            char* tmp_ptr = buffer;
            while (tmp_ptr && *tmp_ptr && *tmp_ptr!='\r' && *tmp_ptr!='\n' && (*tmp_ptr<'0' || *tmp_ptr>'9')) tmp_ptr++; //Skip any leading garbage
            while (tmp_ptr && *tmp_ptr>='0' && *tmp_ptr<='9') tmp_ptr++; //Skip response code (should be 220, 221, 222 or 223)
            while (tmp_ptr && *tmp_ptr && *tmp_ptr!='\r' && *tmp_ptr!='\n' && (*tmp_ptr<'0' || *tmp_ptr>'9')) tmp_ptr++; //Skip space between response code and article number
            nntp_number = (tmp_ptr && *tmp_ptr && *tmp_ptr!='\r' && *tmp_ptr!='\n') ? atoUINT32(tmp_ptr, FALSE) : 0;
        }
        else
        {
            nntp_number = 0;
        }

        if ((ret=SkipCurrentLine(buffer)) != OpStatus::OK)
            return ret;

        m_reply_buffer.Empty();
    }

    m_status = busy_status;
	if (m_status == ready)
	{
		m_ignore_messages = FALSE;
	}

    BOOL done = FALSE;
    char* buffer_start = m_reply_buffer.CStr();
    char* buffer_end = buffer_start;
    if (m_reply_buffer.HasContent())
        buffer_end += m_reply_buffer.Length() - 1;

    char* pending_buffer = NULL; //Used if two messages are sent in one buffer, seperated by CRLF.CRLF
    char* tmp_ptr = buffer;
    while (!done)
    {
        if ((*tmp_ptr=='.' && (*(tmp_ptr+1)=='\r' || *(tmp_ptr+1)=='\n')) &&
			(buffer_start==NULL || //end-of-text marker in an empty message
			(*buffer_end=='\r' || *buffer_end=='\n')) ) //end-of-text marker in a message
        {
            *tmp_ptr = 0; //Remove end-of-text marker
            m_status = ready;
			m_ignore_messages = FALSE;
            done = TRUE;

            pending_buffer = tmp_ptr+1; //Skip zero-terminator (We know there will be content or another zero-terminator after this one)
            if ((ret=SkipCurrentLine(pending_buffer)) != OpStatus::OK)
                return ret;
        }
        else if (tmp_ptr==buffer && (*tmp_ptr=='\r' || *tmp_ptr=='\n') && buffer_start!=NULL) //We might have a end-of-text marker across two buffers
        {
            char* tmp_buffer_end = buffer_end;
            while ((*tmp_buffer_end=='\r' || *tmp_buffer_end=='\n') && tmp_buffer_end>buffer_start) tmp_buffer_end--;
            if (*tmp_buffer_end=='.' &&
                ( tmp_buffer_end==buffer_start || //empty message
                 (*(tmp_buffer_end-1)=='\r' || *(tmp_buffer_end-1)=='\n') ) ) //end-of-text
            {
                *tmp_buffer_end = 0; //Remove end-of-text marker
                m_status = ready;
				m_ignore_messages = FALSE;
                done = TRUE;

                pending_buffer = tmp_ptr;
                if ((ret=SkipCurrentLine(pending_buffer)) != OpStatus::OK)
                    return ret;
            }
        }

        if (!done) //Find next line
        {
            if (*tmp_ptr=='.' && *(tmp_ptr+1)=='.' && //Dot-stuffed
                (buffer_start==NULL ||
                (*buffer_end=='\r' || *buffer_end=='\n')) ) // (Only at the start of a line)
            {
                if (m_reply_buffer.Capacity() <= (m_reply_buffer.Length()+(tmp_ptr-buffer)))
                {
                    if (!m_reply_buffer.Reserve(m_reply_buffer.Capacity()*2 +1))
                        return OpStatus::ERR_NO_MEMORY;
                }

                if ((ret=m_reply_buffer.Append(buffer, tmp_ptr-buffer)) != OpStatus::OK)
                    return ret;

                buffer = ++tmp_ptr;
            }

            //Skip to next line
            if ((ret=SkipCurrentLine(tmp_ptr)) != OpStatus::OK)
                return ret;

            buffer_end = tmp_ptr-1;

            if (*tmp_ptr==0)
                done = TRUE;
        }

        if (done && (tmp_ptr != buffer))
        {
            if (m_reply_buffer.Capacity() <= (m_reply_buffer.Length()+(tmp_ptr-buffer)))
            {
                if (!m_reply_buffer.Reserve(m_reply_buffer.Capacity()*2 +1))
                    return OpStatus::ERR_NO_MEMORY;
            }

            if ((ret=m_reply_buffer.Append(buffer, tmp_ptr-buffer)) != OpStatus::OK)
                return ret;

            buffer = pending_buffer ? pending_buffer : tmp_ptr;
        }
    }

    return OpStatus::OK;
}

OP_STATUS NNTP::SkipCurrentLine(char*& buffer)
{
    if (!buffer)
        return OpStatus::ERR_NULL_POINTER;

    while (*buffer!='\r' && *buffer!='\n' && *buffer!=0) buffer++;
    while (*buffer=='\r' || *buffer=='\n') buffer++; //This will skip multiple linefeeds, but this should never be present in nntp-buffers
    return OpStatus::OK;
}


OP_STATUS NNTP::OnError(const char* error_message) const
{
	if (!m_nntp_backend || !error_message)
		return OpStatus::ERR_NULL_POINTER;

	Account* account_ptr = m_nntp_backend->GetAccountPtr();
	if (!account_ptr)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS ret;
	OpString message;
	if ((ret=message.Set(error_message, strcspn(error_message, "\r\n"))) != OpStatus::OK)
		return ret;

	m_nntp_backend->OnError(message);

	return OpStatus::OK;
}


OP_STATUS NNTP::StartTimeoutTimer(BOOL& timer_started)
{
    timer_started = FALSE;

    if (m_nntp_backend && m_commandlist->Empty() && m_allow_more_commands) //Don't call Disconnect if we already are in a disconnect operation
    {
        if (!m_connection_timer)
        {
			m_connection_timer = OP_NEW(OpTimer, ());
            if (!m_connection_timer)
                return OpStatus::ERR_NO_MEMORY;

            m_connection_timer->SetTimerListener(this);
        }

        m_connection_timer->Start((m_nntp_backend->GetConnectionId(this)<2) ? CONNECTION_TIMEOUT : NICE_CONNECTION_TIMEOUT);
        timer_started = TRUE;

        return OpStatus::OK; //Connection 0 and 1 should be able to stay connected for 5 idle minutes
    }

    if (m_connection_timer) //We have a command while in idle mode. Stop timer
    {
        m_connection_timer->Stop();
    }

    return OpStatus::OK;
}

OP_STATUS NNTP::InsertCommandFromGlobalQueue()
{
    if (!m_nntp_backend || !m_allow_more_commands) //We are in the prosess of disconnecting. Let the queue go dry
        return OpStatus::OK;

    OP_ASSERT(m_commandlist);
    if (!m_commandlist)
        return OpStatus::ERR_NULL_POINTER;

#if defined NNTP_LOG_QUEUE
    if (m_nntp_backend) m_nntp_backend->Log("Before NNTP::InsertCommandFromGlobalQueue", "");
    if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

    OP_STATUS ret;
    CommandItem* item = (CommandItem*)(m_commandlist->First());
    while (item && item->m_command==CommandItem::END_OF_COMMAND)
        item = (CommandItem*)(item->Suc());
    if (!item || item->m_command==CommandItem::QUIT)
    {
        if ((ret=m_nntp_backend->MoveQueuedCommand(m_commandlist, m_total_count, TRUE)) != OpStatus::OK)
            return ret;
    }
    return OpStatus::OK;
}


OP_STATUS NNTP::ReplaceLoopCommand()
{
    OP_ASSERT(m_commandlist);

#if defined NNTP_LOG_QUEUE
    if (m_nntp_backend) m_nntp_backend->Log("Before NNTP::ReplaceLoopCommand", "");
    if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

    //Skip end_of_command commands
    CommandItem* item = (CommandItem*)(m_commandlist->First());
    while (item && item->m_command==CommandItem::END_OF_COMMAND)
    {
        item->Out();
        OP_DELETE(item);
        item = (CommandItem*)(m_commandlist->First());
    }

    //Do we have commands to process?
    if (m_commandlist->Empty() ||
        (item->m_command!=CommandItem::HEAD_LOOP &&
         item->m_command!=CommandItem::XOVER_LOOP &&
         item->m_command!=CommandItem::ARTICLE_LOOP) )
        return OpStatus::OK; //Nothing to do

    //Initialize range
    if (m_available_range_from==-1 || m_available_range_to==-1) //If range is not set (newsgroup not found, etc), don't expand loop-commands
    {
        item->Out();
        OP_DELETE(item);
        return ReplaceLoopCommand();
    }
    else
    {
        OP_STATUS ret;
        NNTPRange range;
        range.SetAvailableRange(m_available_range_from, m_available_range_to); //The member-variables should have been set when NNTP got response from the GROUP command
		if (item->m_param1.HasContent() && item->m_param1.HasContent())
        {
			if (item->m_flags&(1<<CommandItem::IGNORE_RCFILE))
			{
				ret = range.SetAvailableRange(item->m_param1); //User has asked for a spesific range
			}
			else
			{
				ret = range.SetReadRange(item->m_param1); //If a read-range exists, set it
			}

            if (ret != OpStatus::OK)
            {
                item->Out(); //Something is wrong. Delete this item to prevent infinite loop
                OP_DELETE(item);
                return ret;
            }
        }

        //What command should we insert?
		UINT8 loop_flags = item->m_flags;
        CommandItem::Commands loop_command;
        switch (item->m_command)
        {
        case CommandItem::HEAD_LOOP: loop_command = CommandItem::HEAD; break;
        case CommandItem::XOVER_LOOP: loop_command = CommandItem::XOVER; break;
        case CommandItem::ARTICLE_LOOP: loop_command = CommandItem::ARTICLE; break;
        default: loop_command = CommandItem::END_OF_COMMAND; break;
        }

        //Replace loop-command with multiple real commands
        CommandItem* previous_item = NULL;
        INT32 low_limit = 0;
        OpString8 unread_string;
        OpString8 optimized_string;

		INT32 available_messages = range.GetUnreadCount();

		INT32 max_download;
		if (m_nntp_backend->GetMaxDownload(available_messages, m_confirmed_newsgroup, FALSE, max_download))
		{
			// the user hasn't cancelled the dialog
			INT32 skip = available_messages - max_download;

			while (skip > 0) {
				INT32 from, to, count;
				RETURN_IF_ERROR(range.GetNextUnreadRange(low_limit, FALSE, FALSE, unread_string, optimized_string, &count, &to));
				OP_ASSERT(count > 0);
				from = to - count + 1;
				if (count > skip) {
					count = skip;
					to = from + skip - 1;
				}
				unread_string.Empty();
				RETURN_IF_ERROR(unread_string.AppendFormat("%d-%d", from, to));
				m_nntp_backend->AddReadRange(m_confirmed_newsgroup, unread_string);
				skip -= count;
				low_limit = to + 1;
			}

			while (true)
			{
				RETURN_IF_ERROR(range.GetNextUnreadRange(low_limit,
														 loop_command!=CommandItem::ARTICLE, /*Article shouldn't be optimized, one article can be large*/
														 (loop_command==CommandItem::HEAD || loop_command==CommandItem::ARTICLE), /*Head and Article does not support ranges*/
														 unread_string,
														 optimized_string));
				if (unread_string.IsEmpty())
					break;

				CommandItem* new_item = OP_NEW(CommandItem, ());
				if (!new_item)
					return OpStatus::ERR_NO_MEMORY;

				new_item->m_command = loop_command;
				new_item->m_flags = loop_flags;

				if ( ((ret=new_item->m_param1.Set(unread_string)) != OpStatus::OK) ||
					 ((ret=new_item->m_param2.Set(optimized_string)) != OpStatus::OK) )
				{
					OP_DELETE(new_item);
					return ret;
				}

				if (previous_item==NULL)
				{
					new_item->Follow(item);
					item->Out();
					OP_DELETE(item);
				}
				else
				{
					new_item->Follow(previous_item);
				}

				if (loop_command==CommandItem::HEAD ||
					loop_command==CommandItem::ARTICLE)
				{
					m_total_count++;
				}

				previous_item = new_item;
			}
		}

        if (previous_item==NULL) //The loop-command had an empty range. Delete it and continue with next command
        {
            item->Out();
            OP_DELETE(item);
            return ReplaceLoopCommand();
        }
    }

#if defined NNTP_LOG_QUEUE
    if (m_nntp_backend) m_nntp_backend->Log("After NNTP::ReplaceLoopCommand", "");
    if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

    return OpStatus::OK;
}


OP_STATUS NNTP::SendNextCommand()
{
    OP_ASSERT(m_status==ready || m_status==aborting);
    OP_ASSERT(m_commandlist);

    OP_STATUS ret;
    BOOL more_commands = TRUE;
    CommandItem* item = NULL;

    while (!item && more_commands) //Needs this loop, as ReplaceLoopCommand can erase commands from queue and more commands will have to be inserted
    {
        //Insert commands if needed/possible
        if ((ret=InsertCommandFromGlobalQueue()) != OpStatus::OK)
            return ret;

        more_commands = !m_commandlist->Empty();

        //Replace loop-commands with real commands and ranges
        if ((ret=ReplaceLoopCommand()) != OpStatus::OK)
            return ret;

        //Find command to send
        item = (CommandItem*)(m_commandlist->First());
        while (item && item->m_command==CommandItem::END_OF_COMMAND)
        {
            CommandItem* tmp_item = (CommandItem*)(item->Suc());
            item->Out();
            OP_DELETE(item);
            item = tmp_item;
        }
    }

#if defined NNTP_LOG_QUEUE
    if (m_nntp_backend) m_nntp_backend->Log("Before NNTP::SendNextCommand is sent", "");
    if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

    if (item)
    {
        //Allocate memory (will be deleted by Comm::)
        int alloc_length;
        if (item->m_command == CommandItem::POST_MESSAGE)
        {
            alloc_length = 513; //Just a quite large buffer. It will be enlarged if needed.
        }
        else //Other commands need room for the command (set to max 30 chars) and the parameter if given
        {
            alloc_length = 30 + (item->m_param1.CStr() ? item->m_param1.Length() : 0);
        }

        char* request = AllocMem(alloc_length); //A line should never have more than 512 chars (RFC977, §2.3)
        if (!request)
        {
            item->Out();
            OP_DELETE(item);
            return OpStatus::ERR_NO_MEMORY;
        }

        *request = *(request+alloc_length-1) = 0;

        switch(item->m_command)
        {
        case CommandItem::MODE_READER:
            {
                strcpy(request, "MODE READER\r\n");
                break;
            }

        case CommandItem::AUTHINFO_CRAMMD5_REQUEST:
            {
                strcpy(request, "AUTHINFO GENERIC CRAM-MD5\r\n"); //Should we use SASL instead of GENERIC?

				m_progress.SetCurrentAction(ProgressInfo::AUTHENTICATING);
				break;
            }

        case CommandItem::AUTHINFO_CRAMMD5:
            {
                OP_ASSERT(item->m_param1.CStr());
				if (item->m_param1.CStr() && !item->m_param1.IsEmpty())
				{
					snprintf(request, alloc_length-1, "%s\r\n", item->m_param1.CStr());

					m_progress.SetCurrentAction(ProgressInfo::AUTHENTICATING);
				}
				else
					*request = 0;

				break;
            }

        case CommandItem::AUTHINFO_USER:
            {
                OP_ASSERT(item->m_param1.CStr());
                snprintf(request, alloc_length-1, "AUTHINFO USER %s\r\n", item->m_param1.CStr() && item->m_param1.HasContent() ? item->m_param1.CStr() : "''");

				m_progress.SetCurrentAction(ProgressInfo::AUTHENTICATING);

				break;
            }

        case CommandItem::AUTHINFO_PASS:
            {
//                OP_ASSERT(item->m_param1);
                snprintf(request, alloc_length-1, "AUTHINFO PASS %s\r\n", item->m_param1.CStr() && item->m_param1.HasContent() ? item->m_param1.CStr() : "''");

				m_progress.SetCurrentAction(ProgressInfo::AUTHENTICATING);

				break;
            }

        case CommandItem::LIST:
            {
                strcpy(request, "LIST\r\n");

				m_progress.SetCurrentAction(ProgressInfo::FETCHING_GROUPS);

                break;
            }

        case CommandItem::NEWGROUPS:
            {
                OP_ASSERT(item->m_param1.CStr());
				if (item->m_param1.CStr() && !item->m_param1.IsEmpty())
				{
					snprintf(request, alloc_length-1, "NEWGROUPS %s\r\n", item->m_param1.CStr());

					m_progress.SetCurrentAction(ProgressInfo::FETCHING_GROUPS);
				}
				else
				{
					*request = 0;
				}
				break;
            }

        case CommandItem::GROUP:
            {
                OP_ASSERT(item->m_param1.CStr());
				if (item->m_param1.CStr() && !item->m_param1.IsEmpty())
				{
                    if (m_confirmed_newsgroup.CompareI(item->m_param1)==0) /* Skip this command, we are already in the correct group */
                    {
                        FreeMem(request);

                        item->Out();
                        OP_DELETE(item);

                        return SendNextCommand();
                    }

					snprintf(request, alloc_length-1, "GROUP %s\r\n", item->m_param1.CStr());
					if ((ret=m_requested_newsgroup.Set(item->m_param1)) != OpStatus::OK)
					{
						FreeMem(request);
						return ret;
					}
					OP_ASSERT(m_requested_newsgroup.HasContent());
				}
				else
					*request = 0;

                break;
            }

        case CommandItem::HEAD:
            {
                OP_ASSERT(item->m_param1.CStr());
				if (item->m_param1.CStr() && !item->m_param1.IsEmpty())
				{
					snprintf(request, alloc_length-1, "HEAD %s\r\n", item->m_param1.CStr());
					if ( ((ret=m_requested_range.Set(item->m_param1)) != OpStatus::OK) ||
						 ((ret=m_requested_optimized_range.Set(item->m_param2)) != OpStatus::OK) )
					{
						FreeMem(request);
						return ret;
					}

					m_progress.SetCurrentAction(ProgressInfo::FETCHING_HEADERS, m_total_count + (m_max_xover_count-m_min_xover_count));
				}
				else
					*request = 0;

                break;
            }

        case CommandItem::XOVER:
            {
                OP_ASSERT(item->m_param1.CStr());
				if (item->m_param1.CStr() && !item->m_param1.IsEmpty())
				{
					snprintf(request, alloc_length-1, "XOVER %s\r\n", item->m_param1.CStr());
					if ( ((ret=m_requested_range.Set(item->m_param1)) != OpStatus::OK) ||
						 ((ret=m_requested_optimized_range.Set(item->m_param2)) != OpStatus::OK) )
					{
						FreeMem(request);
						return ret;
					}

                    NNTPRange requested_range;
                    requested_range.SetAvailableRange(m_requested_range);
                    requested_range.GetAvailableRange(m_min_xover_count, m_max_xover_count);
                    m_current_xover_count = m_min_xover_count;

					m_progress.SetCurrentAction(ProgressInfo::FETCHING_HEADERS, m_total_count + (m_max_xover_count-m_min_xover_count));
				}
				else
					*request = 0;

                break;
            }

        case CommandItem::ARTICLE:
            {
                OP_ASSERT(item->m_param1.CStr());
				if (item->m_param1.CStr() && !item->m_param1.IsEmpty())
				{
					snprintf(request, alloc_length-1, "ARTICLE %s\r\n", item->m_param1.CStr());
					if ( ((ret=m_requested_range.Set(item->m_param1)) != OpStatus::OK) ||
						 ((ret=m_requested_optimized_range.Set(item->m_param2)) != OpStatus::OK) ) //Optimized_range should never be used for articles...
					{
						FreeMem(request);
						return ret;
					}

					m_progress.SetCurrentAction(ProgressInfo::FETCHING_MESSAGES, m_total_count + (m_max_xover_count-m_min_xover_count));
				}
				else
					*request = 0;

                break;
            }

        case CommandItem::POST:
            {
                strcpy(request, "POST\r\n");

				m_progress.SetCurrentAction(ProgressInfo::SENDING_MESSAGES);

				break;
            }

        case CommandItem::POST_MESSAGE:
            {
                OP_ASSERT(item->m_param1.CStr());
                OpString8 dotstuffed;
                char* next_dot = item->m_param1.CStr() ? strstr(item->m_param1.CStr(), "\n.") : NULL;
                if (next_dot!=NULL) //Do we need to dot-stuff message?
                {
                    char* buffer_start = item->m_param1.CStr();
                    dotstuffed.Empty();
                    int dotstuffed_length = 0;
                    while (next_dot) //Do the dotstuffing
                    {
                        if (!dotstuffed.Reserve(dotstuffed_length+(next_dot-buffer_start)+3+1)) //original length + new string + "\n..\0"
                        {
                            FreeMem(request);
                            return OpStatus::ERR_NO_MEMORY;
                        }

                        if ( ((ret=dotstuffed.Append(buffer_start, next_dot-buffer_start)) != OpStatus::OK) ||
                             ((ret=dotstuffed.Append("\n..")) != OpStatus::OK) )
                        {
                            FreeMem(request);
                            return ret;
                        }

                        dotstuffed_length += (next_dot-buffer_start)+3;
                        buffer_start = next_dot+2; //Skip \n."
                        next_dot = strstr(buffer_start, "\n.");
                    }

                    if (buffer_start && *buffer_start!=0) //Append the rest of the buffer
                    {
                        if ((ret=dotstuffed.Append(buffer_start)) != OpStatus::OK)
                        {
                            FreeMem(request);
                            return ret;
                        }
                    }
                }

                int length = max(dotstuffed.Length(), item->m_param1.CStr() ? item->m_param1.Length() : 0);
                if (length>512)
                {
                    FreeMem(request);
                    request = AllocMem(length + 6); // CRLF.CRLF\0
                    if (!request)
                    {
                        item->Out();
                        OP_DELETE(item);
                        return OpStatus::ERR_NO_MEMORY;
                    }
                }
                strcpy(request, dotstuffed.IsEmpty() ? item->m_param1.CStr() : dotstuffed.CStr());

                char end_of_command[6]; //Needs to add this to the text if it is not already present
                strcpy(end_of_command, "\r\n.\r\n");

                //Find where and what to append
                length = strlen(request);
                int i;
                for (i=5; i>0; i--)
                {
                    if (i>length) //Avoid buffer underflows for *really* short messages...
                        continue;

                    if (strncmp(request+length-i, end_of_command, i)==0)
                        break;
                }
                strcpy(request+length-i, end_of_command); //Copy/overwrite the end-of-message dot
                break;
            }

        case CommandItem::QUIT:
			{
				strcpy(request, "QUIT\r\n");
				break;
			}

        default: break;
        }

        //Send command
        if (*request)
        {
            OpString8 log_heading;
            if (m_nntp_backend && log_heading.Reserve(14)!=NULL)
            {
                sprintf(log_heading.CStr(), "NNTP OUT [#%01d]", m_nntp_backend->GetConnectionId(this));
                if (m_nntp_backend) m_nntp_backend->Log(log_heading, request); //Ignore errors here
            }
            SendData(request, strlen(request));
        }
        else
        {
            FreeMem(request);
        }

        m_last_command_sent = item->m_command;
        m_last_command_param1.Set(item->m_param1); //Ignore errors
        m_last_command_param2.Set(item->m_param2); //Ignore errors
		m_last_flags = item->m_flags;

        item->Out();
        OP_DELETE(item);
    }
	else if (m_last_command_sent!=CommandItem::END_OF_COMMAND || m_progress.GetCurrentAction() != ProgressInfo::NONE) //Report that we are now going from busy into idle mode
	{
        m_last_command_sent = CommandItem::END_OF_COMMAND;
		m_last_flags = 0;
		m_progress.EndCurrentAction(FALSE);
		m_current_count = m_total_count = m_current_xover_count = m_min_xover_count = m_max_xover_count = 0;
		if (m_nntp_backend)
			m_nntp_backend->SignalEndSession(this);
	}

#if defined NNTP_LOG_QUEUE
    if (m_nntp_backend) m_nntp_backend->Log("After NNTP::SendNextCommand is sent", "");
    if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

    //Should we start the timeout timer?
    BOOL timer_started;
    if ((ret=StartTimeoutTimer(timer_started)) != OpStatus::OK)
        return ret;

    return OpStatus::OK;
}


OP_STATUS NNTP::ProcessReceivedData()
{
	OpString8 reply_buf;
    if (!reply_buf.Reserve(1024))
        return OpStatus::ERR_NO_MEMORY;

    //Read reply
	unsigned int contentLoaded = ReadData(reply_buf.CStr(), reply_buf.Capacity() - 1);

    if (m_status == aborting) //Nothing to do, waiting to be killed
        return OpStatus::OK;

    if (!contentLoaded)
    {
		if (m_empty_network_buffer_count++<EMPTY_NETWORK_BUFFER_BAILOUT_COUNT)
			return OpStatus::OK;

        OpStatus::Ignore(Disconnect());
        return OpStatus::ERR;
    }
	else
	{
		m_empty_network_buffer_count = 0;
	}

	*(reply_buf.CStr()+contentLoaded) = '\0';

	//Convert any \0 to [space], to avoid confusion between M2 and NNTP-server.
	unsigned int i = static_cast<unsigned int>(reply_buf.Length());
	while (i < contentLoaded)
	{
		*(reply_buf.CStr()+i) = ' ';
		i += strlen(reply_buf.CStr()+i);
	}

    OpString8 log_heading;
    if (m_nntp_backend && log_heading.Reserve(13)!=NULL)
    {
        sprintf(log_heading.CStr(), "NNTP IN [#%01d]", m_nntp_backend->GetConnectionId(this));
        if (m_nntp_backend) m_nntp_backend->Log(log_heading, reply_buf); //Ignore errors here
    }

    //Just to be safe...
	OP_ASSERT((UINT32)reply_buf.Length() == contentLoaded);

    char* reply_buf_ptr = (char*)(reply_buf.CStr());
    int reply_code = 0;
	OP_STATUS ret = OpStatus::OK;
	while (reply_buf_ptr && *reply_buf_ptr && m_status!=aborting && ret == OpStatus::OK)
    {
        switch (m_status)
        {
        case fetching_newgroups: reply_code = NNTP_231_new_newsgroup_list; break;
        case fetching_headers:   reply_code = NNTP_221_art_retr_head; break;
        case fetching_body:      reply_code = NNTP_222_art_retr_body; break;
        case fetching_article:   reply_code = NNTP_220_art_retr_head_body; break;
        case fetching_stat:      reply_code = NNTP_223_art_retrieved; break;
        case fetching_xover:     reply_code = NNTP_224_overview_info_follows; break;

        case ready:              m_receiving_nntp_number = 0;
                                 reply_code = atoUINT32(reply_buf_ptr, FALSE); break;

        default: OP_ASSERT(0); break;
        }

        if (m_status==ready && reply_code==0)
            break;

        //News-servers can ask for authentication at any given time.
        BOOL autentication_requested = FALSE;
        if ((ret=CheckForAuthenticationRequest(reply_code, reply_buf_ptr, autentication_requested)) != OpStatus::OK)
            return ret;

        if (!autentication_requested)
        {
            if (m_last_command_sent==CommandItem::END_OF_COMMAND ||
				m_last_command_sent==CommandItem::QUIT) //Only legal value when first response from server
            {
                if ((ret=HandleConnect(reply_code, reply_buf_ptr)) != OpStatus::OK)
                    return ret;
            }
            else
            {
                switch(m_last_command_sent)
                {
                case CommandItem::MODE_READER:   ret=HandleModeReader(reply_code, reply_buf_ptr); break;
                case CommandItem::AUTHINFO_CRAMMD5_REQUEST: ret=HandleAuthinfoCRAMMD5req(reply_code, reply_buf_ptr); break;
                case CommandItem::AUTHINFO_CRAMMD5: ret=HandleAuthinfoCRAMMD5(reply_code, reply_buf_ptr); break;
                case CommandItem::AUTHINFO_USER: ret=HandleAuthinfoUser(reply_code, reply_buf_ptr); break;
                case CommandItem::AUTHINFO_PASS: ret=HandleAuthinfoPass(reply_code, reply_buf_ptr); break;
                case CommandItem::LIST:          ret=HandleNewgroups(reply_code, reply_buf_ptr); break;
                case CommandItem::NEWGROUPS:     ret=HandleNewgroups(reply_code, reply_buf_ptr); break;
                case CommandItem::GROUP:         ret=HandleGroup(reply_code, reply_buf_ptr); break;

                case CommandItem::HEAD: //Fallthrough
                case CommandItem::ARTICLE:       ret=HandleArticle(reply_code, reply_buf_ptr); break;

                case CommandItem::XOVER:         ret=HandleXover(reply_code, reply_buf_ptr); break;

                case CommandItem::POST: //Fallthrough
                case CommandItem::POST_MESSAGE:  ret=HandlePost(reply_code, reply_buf_ptr); break;

                case CommandItem::QUIT:          ret=HandleQuit(reply_code, reply_buf_ptr); break;

                //These commands should not be sent, and it is an error if they are present as last command sent
                case CommandItem::HEAD_LOOP:
                case CommandItem::XOVER_LOOP:
                case CommandItem::ARTICLE_LOOP:
                case CommandItem::END_OF_COMMAND: OP_ASSERT(0); reply_buf_ptr = NULL; break;
                default: OP_ASSERT(0); reply_buf_ptr = NULL; break;
                }
            }
        }
    }

    if (m_status==ready)
    {
		if (m_connection)
			m_connection->StopConnectionTimer();
        return SendNextCommand();
    }

	return OpStatus::OK;
}


OP_STATUS NNTP::CheckForAuthenticationRequest(int code, char*& buffer, BOOL& autentication_requested)
{
    autentication_requested = (code==NNTP_450_require_authentication ||
                               code==NNTP_480_require_authentication ||
							   code==NNTP_490_maybe_authenticate /*||
                               code==NNTP_350_accept_method ||
                               code==NNTP_381_accept_method*/);

    if (m_nntp_backend && autentication_requested) //If we are asked to authenticate, put authinfo at the start of the queue
    {
        if (!m_commandlist)
            return OpStatus::ERR_NULL_POINTER;

        //Find authentication method
        OP_STATUS ret;
        AccountTypes::AuthenticationType authmethod = m_nntp_backend->GetCurrentAuthMethod();
        if (authmethod==AccountTypes::AUTOSELECT)
        {
            authmethod = m_nntp_backend->GetAuthenticationMethod();

            if (authmethod==AccountTypes::NONE) //Shouldn't get here if no authentication is requested - use autoselect instead
                authmethod = AccountTypes::AUTOSELECT;

            if (authmethod==AccountTypes::AUTOSELECT)
            {
                authmethod = m_nntp_backend->GetNextAuthenticationMethod(m_authentication_method, m_nntp_backend->GetAuthenticationSupported());
            }
        }

        //Is it the same as the one we already tried, or have we tried all methods we support?
        if (authmethod==AccountTypes::NONE || authmethod==m_authentication_method)
        {
			if (code==NNTP_490_maybe_authenticate) //We didn't need to authenticate. Continue as if nothing happened. Trallala!
			{
				autentication_requested = FALSE;
				return OpStatus::OK;
			}
			else //Server _really_ wants authentication, no matter what the user says!
			{
				m_nntp_backend->AuthenticationFailed(this);
				m_status = aborting;

				if (m_nntp_backend)
					m_nntp_backend->OnAuthenticationRequired();

				return OpStatus::OK;
			}
        }

        //Create item and add it to the queue
        m_authentication_method = authmethod;

        CommandItem::Commands auth_command;
        switch (m_authentication_method)
        {
        case AccountTypes::CRAM_MD5:  auth_command = CommandItem::AUTHINFO_CRAMMD5_REQUEST; break;
        case AccountTypes::PLAINTEXT: auth_command = CommandItem::AUTHINFO_USER; break;
        default:                     auth_command = CommandItem::END_OF_COMMAND; break;
        }

		BOOL add_command_to_queue = FALSE;
        CommandItem* item = OP_NEW(CommandItem, ());
        if (!item)
            return OpStatus::ERR_NO_MEMORY;

		CommandItem* first_in_queue = (CommandItem*)(m_commandlist->First());

		if (auth_command!=CommandItem::END_OF_COMMAND && first_in_queue->m_command!=auth_command)
		{
			item->m_command = auth_command;

            if (auth_command == CommandItem::AUTHINFO_USER) //AUTHINFO USER needs username
            {
				if ((ret=m_nntp_backend->GetUsername(item->m_param1)) != OpStatus::OK)
				{
					OP_DELETE(item);
					return ret;
				}
            }

			add_command_to_queue = TRUE;
		}

        if (add_command_to_queue)
		{
			item->IntoStart(m_commandlist);

            //If authentication is caused by a issuing a non-authentication command, re-insert the command into the queue
            if (m_last_command_sent!=CommandItem::END_OF_COMMAND &&
                m_last_command_sent!=CommandItem::AUTHINFO_CRAMMD5_REQUEST &&
                m_last_command_sent!=CommandItem::AUTHINFO_USER)
            {
                CommandItem* original_item = OP_NEW(CommandItem, ());
                if (!original_item)
                    return OpStatus::ERR_NO_MEMORY;

                original_item->m_command = m_last_command_sent;
                original_item->m_flags = m_last_flags;
                if ((ret=original_item->m_param1.Set(m_last_command_param1))!=OpStatus::OK ||
                    (ret=original_item->m_param2.Set(m_last_command_param2))!=OpStatus::OK)
                {
                    OP_DELETE(original_item);
                    return ret;
                }

                original_item->Follow(item);
            }
		}
		else
		{
			OP_DELETE(item);
		}

#if defined NNTP_LOG_QUEUE
        if (m_nntp_backend) m_nntp_backend->Log("After NNTP::CheckForAuthenticationRequest", "");
        if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif

        return (buffer && *buffer) ? SkipCurrentLine(buffer) : OpStatus::OK;
    }

    return OpStatus::OK;
}

OP_STATUS NNTP::HandleConnect(int code, char*& buffer)
{
    if (code==NNTP_205_closing_connection ||
        code==NNTP_502_permission_denied ||
        code==NNTP_503_program_fault)
    {
		OpStatus::Ignore(OnError(buffer));
        m_is_connected = FALSE;
    }
    else if (m_allow_more_commands && code>=100 && code <=399) //Everything OK
    {
        m_can_post = (code==NNTP_200_ready_posting_ok);
        m_is_connected = TRUE;
    }
    else //Somthing failed when connecting. Disconnect.
    {
		if (code>=400 && code<=599)
			OpStatus::Ignore(OnError(buffer));

        m_is_connected = FALSE;
    }

    OP_STATUS ret;
    if ((ret=SkipCurrentLine(buffer)) != OpStatus::OK)
        return ret;

	m_progress.SetConnected(m_is_connected);
	m_current_count = m_total_count = m_current_xover_count = m_min_xover_count = m_max_xover_count = 0;

	if (m_nntp_backend && m_is_connected && m_commandlist)
	{
        OpStatus::Ignore(m_nntp_backend->NNTPNewGroups()); //Not critical if this fails

        CommandItem* modereader_item = OP_NEW(CommandItem, ());
        if (modereader_item) //Not critical if this fails, either (only provided to let server do optimization)
        {
            modereader_item->m_command = CommandItem::MODE_READER;
		    modereader_item->IntoStart(m_commandlist);
        }

        AccountTypes::AuthenticationType authmethod = m_nntp_backend->GetAuthenticationMethod();
        if (authmethod!=AccountTypes::NONE)
        {
            BOOL dummy;
            char* dummy_buffer = NULL;
            OpStatus::Ignore(CheckForAuthenticationRequest(NNTP_490_maybe_authenticate, dummy_buffer, dummy));
        }
		else
		{
			m_nntp_backend->SetCurrentAuthMethod(AccountTypes::NONE);
		}

#if defined NNTP_LOG_QUEUE
        if (m_nntp_backend) m_nntp_backend->Log("After NNTP::Connect", "");
        if (m_nntp_backend) m_nntp_backend->LogQueue();
#endif
	}

    return m_is_connected ? OpStatus::OK : Disconnect();
}

OP_STATUS NNTP::HandleModeReader(int code, char*& buffer)
{
//NNTP_200_ready_posting_ok
//NNTP_201_ready_no_posting
//+NNTP_202_slave_status_noted
//+NNTP_400_service_discont
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//+NNTP_502_permission_denied
//+NNTP_503_program_fault
    if (code==NNTP_200_ready_posting_ok)
    {
        m_can_post=TRUE;
    }
    else if (code==NNTP_201_ready_no_posting)
    {
        m_can_post=FALSE;
    }

    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleAuthinfoCRAMMD5req(int code, char*& buffer)
{
    if (m_authentication_method==AccountTypes::NONE) //Servers shouldn't complain about passwords here. If they do, they are not following RFC2980 §3.1.3.1, and it should be handled as if they don't support CRAM-MD5
    {
        m_nntp_backend->AuthenticationFailed(this);
        m_status = aborting;

		if (m_nntp_backend)
            m_nntp_backend->OnAuthenticationRequired();

        return OpStatus::OK;
    }
	else if (code>=400 && code<=599)
    {
        BOOL authentication_requested;
        char* dummy_buffer = NULL;
        OpStatus::Ignore(CheckForAuthenticationRequest(NNTP_490_maybe_authenticate, dummy_buffer, authentication_requested));
        if (!authentication_requested)
        {
		    OpStatus::Ignore(OnError(buffer));

            if (!authentication_requested) //If authentication failed, drop connection to prevent more commands being sent
            {
                OpStatus::Ignore(Disconnect());
            }
        }
    }
    else /*if (code==NNTP_350_accept_method || //Success
               code==NNTP_381_accept_method)*/
    {
        //Set this method as confirmed, to speed up authentication for other connections
        m_nntp_backend->SetCurrentAuthMethod(m_authentication_method);

        //Generate CRAM-MD5 response
        OpAutoPtr<CommandItem> crammd5_item(OP_NEW(CommandItem, ()));
        if (!crammd5_item.get())
            return OpStatus::ERR_NO_MEMORY;

        crammd5_item->m_command = CommandItem::AUTHINFO_CRAMMD5;

		OpAuthenticate login_info;
		RETURN_IF_ERROR(m_nntp_backend->GetLoginInfo(login_info, buffer));
		RETURN_IF_ERROR(crammd5_item->m_param1.Set(login_info.GetResponse()));

		crammd5_item->IntoStart(m_commandlist);
		crammd5_item.release();
    }

    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleAuthinfoCRAMMD5(int code, char*& buffer)
{
    if (code==NNTP_452_Illegal_password ||
        code==NNTP_482_Illegal_password ||
        code==NNTP_502_permission_denied)
    {
		if (code==NNTP_502_permission_denied &&
			m_nntp_backend->MaxConnectionCount()>1) //If it fails on other connections, it is probably a cap on connections from the server
		{
			m_nntp_backend->ConnectionDenied(this);
			return SkipCurrentLine(buffer);
		}
		else
		{
			m_nntp_backend->AuthenticationFailed(this);
			m_status = aborting;

			if (m_nntp_backend)
				m_nntp_backend->OnAuthenticationRequired();
		}

        return OpStatus::OK;
    }
	else if (code>=400 && code<=599)
		OpStatus::Ignore(OnError(buffer));

    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleAuthinfoUser(int code, char*& buffer)
{
//(+)NNTP_250_accept_authentication
//NNTP_281_accept_authentication
//+NNTP_400_service_discont
//(+)NNTP_452_Illegal_password
//NNTP_482_Illegal_password
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//NNTP_502_permission_denied
//+NNTP_503_program_fault
    if (code==NNTP_452_Illegal_password ||
        code==NNTP_482_Illegal_password ||
        code==NNTP_502_permission_denied)
    {
		if (code==NNTP_502_permission_denied &&
			m_nntp_backend->MaxConnectionCount()>1) //If it fails on other connections, it is probably a cap on connections from the server
		{
			m_nntp_backend->ConnectionDenied(this);
			return SkipCurrentLine(buffer);
		}
		else
		{
			m_nntp_backend->AuthenticationFailed(this);
			m_status = aborting;

			if (m_nntp_backend)
				m_nntp_backend->OnAuthenticationRequired();
		}

        return OpStatus::OK;
    }
	else if (code>=400 && code<=599)
    {
        BOOL authentication_requested;
        char* dummy_buffer = NULL;
        OpStatus::Ignore(CheckForAuthenticationRequest(NNTP_490_maybe_authenticate, dummy_buffer, authentication_requested));
        if (!authentication_requested)
        {
		    OpStatus::Ignore(OnError(buffer));

            if (!authentication_requested) //If authentication failed, drop connection to prevent more commands being sent
            {
                OpStatus::Ignore(Disconnect());
            }
        }
    }
    else /*if (code==NNTP_350_accept_method || //Success
               code==NNTP_381_accept_method)*/
    {
        //Set this method as confirmed, to speed up authentication for other connections
        m_nntp_backend->SetCurrentAuthMethod(m_authentication_method);

        //Generate AUTH PASS response
        OpAutoPtr<CommandItem> pass_item(OP_NEW(CommandItem, ()));
        if (!pass_item.get())
            return OpStatus::ERR_NO_MEMORY;

        pass_item->m_command = CommandItem::AUTHINFO_PASS;

		OpAuthenticate login_info;
		RETURN_IF_ERROR(m_nntp_backend->GetLoginInfo(login_info));
		RETURN_IF_ERROR(pass_item->m_param1.Set(login_info.GetPassword()));
		pass_item.release()->IntoStart(m_commandlist);
    }

    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleAuthinfoPass(int code, char*& buffer)
{
//(+)NNTP_250_accept_authentication
//NNTP_281_accept_authentication
//+NNTP_400_service_discont
//(+)NNTP_452_Illegal_password
//NNTP_482_Illegal_password
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//NNTP_502_permission_denied
//+NNTP_503_program_fault
    if (code==NNTP_452_Illegal_password ||
        code==NNTP_482_Illegal_password ||
        code==NNTP_502_permission_denied)
    {
		if (code==NNTP_502_permission_denied &&
			m_nntp_backend->MaxConnectionCount()>1) //If it fails on other connections, it is probably a cap on connections from the server
		{
			m_nntp_backend->ConnectionDenied(this);
			return SkipCurrentLine(buffer);
		}
		else
		{
			m_nntp_backend->AuthenticationFailed(this);
			m_status = aborting;

			if (m_nntp_backend)
				m_nntp_backend->OnAuthenticationRequired();
		}

        return OpStatus::OK;
    }
	else if (code>=400 && code<=599)
		OpStatus::Ignore(OnError(buffer));

    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleNewgroups(int code, char*& buffer)
{
//NNTP_215_newsgroup_list
//NNTP_231_new_newsgroup_list
//+NNTP_400_service_discont
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//+NNTP_502_permission_denied
//+NNTP_503_program_fault
    if ( m_nntp_backend &&
         (code==NNTP_215_newsgroup_list ||
          code==NNTP_231_new_newsgroup_list) )
    {
        OP_STATUS ret;
        if ((ret=ParseAndAppendBuffer(buffer, fetching_newgroups, m_receiving_nntp_number)) != OpStatus::OK)
            return ret;

        if ((ret=m_nntp_backend->AddNewNewsgroups(m_reply_buffer)) != OpStatus::OK)
            return ret;

        if (m_status==ready)
        {
            OpString8 empty_buffer;
            if ((ret=m_nntp_backend->AddNewNewsgroups(empty_buffer)) != OpStatus::OK) //Signal that we are done
                return ret;
        }
    }
    else
    {
		if (code>=400 && code<=599)
			OpStatus::Ignore(OnError(buffer));

        return SkipCurrentLine(buffer);
    }
    return OpStatus::OK;
}

OP_STATUS NNTP::HandleGroup(int code, char*& buffer)
{
//NNTP_211_group_selected
//+NNTP_400_service_discont
//NNTP_411_no_such_news_group
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//+NNTP_502_permission_denied
//+NNTP_503_program_fault
    m_available_range_from = m_available_range_to = -1;

    if (code==NNTP_211_group_selected && buffer)
    {
        INT32 dummy1, dummy2;
        sscanf(buffer, "%d %d %d %d", &dummy1, &dummy2, &m_available_range_from, &m_available_range_to);

		// FIXME: Check if this is correct behaviour
		(void)m_nntp_backend->FindNewsgroupItem(m_requested_newsgroup, FALSE);

		OP_ASSERT(m_requested_newsgroup.HasContent());
        RETURN_IF_ERROR(m_confirmed_newsgroup.Set(m_requested_newsgroup));
        m_requested_newsgroup.Empty();
    }
	else if (code>=400 && code<=599)
		OpStatus::Ignore(OnError(buffer));

    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleArticle(int code, char*& buffer)
{
	if (!m_nntp_backend)
		return OpStatus::ERR_NULL_POINTER;

//NNTP_220_art_retr_head_body
//NNTP_221_art_retr_head
//NNTP_222_art_retr_body
//NNTP_223_art_retrieved
//+NNTP_400_service_discont
//+NNTP_412_no_group_selected
//NNTP_420_no_current_article
//+NNTP_421_no_next_article
//+NNTP_422_no_prev_article
//NNTP_423_no_such_art_number
//+NNTP_430_no_such_article
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//+NNTP_502_permission_denied
//+NNTP_503_program_fault
    OP_STATUS ret;
	BOOL download = TRUE;
    if (code==NNTP_220_art_retr_head_body ||
        code==NNTP_221_art_retr_head ||
        code==NNTP_222_art_retr_body ||
        code==NNTP_223_art_retrieved)
    {
        Status busy_status = ready;
        switch(code)
        {
        case NNTP_220_art_retr_head_body: busy_status = fetching_article; break;
        case NNTP_221_art_retr_head:      busy_status = fetching_headers; break;
        case NNTP_222_art_retr_body:      busy_status = fetching_body; break;
        case NNTP_223_art_retrieved:      busy_status = fetching_stat; break;
        default: OP_ASSERT(0); break;
        }

        if ((ret=ParseAndAppendBuffer(buffer, busy_status, m_receiving_nntp_number)) != OpStatus::OK)
            return ret;

        if (m_status==ready) //We have the complete headers
        {
			if (m_ignore_messages)
			{
			}
			else
			{
				Message new_message;
				message_index_t store_index = 0;

				if (!m_last_command_param2.IsEmpty())
				{
					char* param2_ptr = (char*)(m_last_command_param2.CStr());
					store_index = atoUINT32(param2_ptr, FALSE);
					OpStatus::Ignore(MessageEngine::GetInstance()->GetStore()->GetMessage(new_message, store_index));
				}

				Account* account_ptr = m_nntp_backend->GetAccountPtr();
				if (!new_message.GetId())
					RETURN_IF_ERROR(new_message.Init(account_ptr ? account_ptr->GetAccountId() : 0));

				new_message.SetFlag(Message::IS_OUTGOING, FALSE);
				new_message.SetFlag(Message::IS_NEWS_MESSAGE, TRUE);

				new_message.SetRawMessage(m_reply_buffer.CStr());

				if (store_index)
				{
					MessageEngine::GetInstance()->GetStore()->SetRawMessage(new_message);
				}
				else
				{
					OpString8 message_id;
					if (OpStatus::IsSuccess(new_message.GetHeaderValue(Header::MESSAGEID, message_id)) &&
						(m_nntp_backend->IHave(message_id)))
					{
						m_total_count--;
						download = FALSE;
					}
				}

				if (download)
				{
					if (new_message.GetHeader(Header::CONTROL) != NULL) //Control-messages might have a newsgroups-header that differs from m_confirmed_newsgroup
					{
						OpString8 newsgroups;
						if (new_message.GetHeaderValue(Header::NEWSGROUPS, newsgroups) == OpStatus::OK)
						{
							if (newsgroups.Find(m_confirmed_newsgroup)==KNotFound) //If m_confirmed_newsgroup isn't among the newsgroups, set it as the one and only newsgroup (indexer will be happy)
							{
								OpStatus::Ignore(new_message.SetHeaderValue(Header::NEWSGROUPS, m_confirmed_newsgroup));
							}
						}
					}

                    OpString8 xref;
                    if (new_message.GetHeaderValue(Header::XREF, xref)==OpStatus::OK && xref.HasContent())
                    {
                        char* xref_start=xref.CStr();
                        while (*xref_start && *xref_start!=' ') xref_start++; //Skip servername
                        while (*xref_start==' ') xref_start++; //Skip whitespace
                        char* xref_end = xref_start;
                        while (*xref_end && *xref_end!=' ') xref_end++;

                        //Generate internet-location
						Account* account_ptr = m_nntp_backend->GetAccountPtr();
                        if (account_ptr)
                        {
                            OpString8 internet_location, message_id, servername;
                            if ((ret=account_ptr->GetIncomingServername(servername))==OpStatus::OK &&
                                (ret=new_message.GetHeaderValue(Header::MESSAGEID, internet_location))==OpStatus::OK &&
                                (internet_location.IsEmpty() || (ret=internet_location.Append(" "))==OpStatus::OK) &&
                                (ret=internet_location.Append(servername))==OpStatus::OK &&
                                (ret=internet_location.Append(" "))==OpStatus::OK &&
                                (ret=internet_location.Append(xref_start, xref_end-xref_start))==OpStatus::OK )
                            {
                                OpStatus::Ignore(new_message.SetInternetLocation(internet_location));
                            }
                        }
                    }

					RETURN_IF_ERROR(m_nntp_backend->Fetched(&new_message, FALSE));
					m_progress.UpdateCurrentAction();

					if ((m_last_flags&(1<<CommandItem::ENABLE_SIGNALLING))!=0)
						m_nntp_backend->SetSignalMessage();
				}
			}

            m_current_count++; //We have processed the HEAD or ARTICLE command
        }
    }
    else if (code==NNTP_430_no_such_article ||
             code==NNTP_412_no_group_selected ||
			 code==NNTP_423_no_such_art_number)
    {
        m_total_count--;
        SkipCurrentLine(buffer);

        if (!m_nntp_backend->IsMessageId(m_last_command_param1) &&
            m_last_command_param2.HasContent())
        {
		    char* param2_ptr = (char*)(m_last_command_param2.CStr());
		    message_index_t store_index = atoUINT32(param2_ptr, FALSE);
			Message original_message;
		    if (OpStatus::IsSuccess(MessageEngine::GetInstance()->GetStore()->GetMessage(original_message, store_index)))
			{
                OpString8 internet_location;
                OpStatus::Ignore(original_message.GetInternetLocation(internet_location));

                OpString8 dummy_server, dummy_newsgroup;
                int dummy_nntp_number;
                OpStatus::Ignore(m_nntp_backend->ParseXref(internet_location, m_last_command_param1, dummy_server, dummy_newsgroup, dummy_nntp_number));
            }
        }

        if (m_nntp_backend->IsMessageId(m_last_command_param1))
        {
            BOOL first_redirect = (m_last_flags&(1<<CommandItem::FIRST_REDIRECT))!=0;

            Account* search_account = m_nntp_backend->GetNextSearchAccount(first_redirect);
            if (search_account==m_nntp_backend->GetAccountPtr())
                search_account = m_nntp_backend->GetNextSearchAccount(FALSE);

            if (search_account)
            {
				NntpBackend* search_backend = (NntpBackend*)search_account->GetIncomingBackend();
                if (m_last_command_sent==CommandItem::HEAD)
                    ret = search_backend->FetchHeaders(m_last_command_param1);
                else
                    ret = search_backend->FetchMessage(m_last_command_param1);

                if (ret != OpStatus::OK)
                    return ret;
            }
            else
            {
//                OP_ASSERT(0); //Notify that message is not found?
            }
        }
    }
    else
    {
		if (code>=400 && code<=599)
			OpStatus::Ignore(OnError(buffer));

        m_total_count--;
        SkipCurrentLine(buffer);
    }

    //Mark the article read in the .newsrc-file
    if (m_status==ready)
    {
        if (!m_nntp_backend->IsMessageId(m_requested_range))
        {
			OP_ASSERT(m_confirmed_newsgroup.HasContent());
			OpStatus::Ignore(m_nntp_backend->AddReadRange(m_confirmed_newsgroup, m_requested_range));
        }

        m_requested_range.Empty();
        m_requested_optimized_range.Empty();
    }

    return OpStatus::OK;
}

OP_STATUS NNTP::HandleXover(int code, char*& buffer)
{
	if (!m_nntp_backend)
		return OpStatus::ERR_NULL_POINTER;

//NNTP_224_overview_info_follows
//+NNTP_400_service_discont
//NNTP_412_no_group_selected
//NNTP_420_no_current_article
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//NNTP_502_permission_denied
//+NNTP_503_program_fault
    OP_STATUS ret;
    if (code==NNTP_224_overview_info_follows)
    {
        if ((ret=ParseAndAppendBuffer(buffer, fetching_xover, m_receiving_nntp_number)) != OpStatus::OK)
            return ret;

        //xover format: (nntp number)\t[subject]\t[from]\t[date]\t[message_id]\t[references]\t[byte count]\t[line count]\t*[other headers according to OVERVIEW.FMT]CRLF
        OpString8 xover_field[9];

        //Parse all lines available in the buffer
        char* xover_start = m_reply_buffer.CStr();
        while (xover_start && (*xover_start=='\r' || *xover_start=='\n')) xover_start++; //Skip any linefeeds at the start

        while (xover_start && *xover_start)
        {
            int i;
            //Clear old fields
            for (i=0; i<9; i++) xover_field[i].Empty();

            //Parse one line
            i=0;
            BOOL found_complete_line = FALSE;
            char* start_ptr = xover_start;
            char* end_ptr = start_ptr;
            while (*end_ptr || m_status==ready) //Allow parsing to the zero-terminator if it is at the end of the last buffer
            {
                if (*end_ptr=='\t' || *end_ptr=='\r' || *end_ptr=='\n' || (!*end_ptr && m_status==ready))
                {
                    if (end_ptr!=start_ptr && i<9)
                    {
                        if ((ret=xover_field[i].Set(start_ptr, end_ptr-start_ptr)) != OpStatus::OK)
                            return ret;
                    }

                    //Are we done with the line?
                    if (*end_ptr=='\r' || *end_ptr=='\n' || (!*end_ptr && m_status==ready)) //A linefeed might be split between two buffers. Need to remove linefeeds at start of a new buffer (see above)
                    {
                        found_complete_line = TRUE;
                        break;
                    }

                    i++;
                    start_ptr = (end_ptr+1);
                }

                end_ptr++;
            }

            if (!found_complete_line)
            {
                break;
            }
            else
            {
                //Skip to next line
                while (*end_ptr=='\r' || *end_ptr=='\n') end_ptr++;
                xover_start = end_ptr;

                NNTPRange optimized_range;
                if (!m_requested_optimized_range.IsEmpty())
                {
                    optimized_range.SetAvailableRange(m_available_range_from, m_available_range_to);
                    if ((ret=optimized_range.SetReadRange(m_requested_optimized_range)) != OpStatus::OK)
                        return ret;
                }

                char* xover_field0_ptr = (char*)(xover_field[0].CStr());
                if (m_requested_optimized_range.IsEmpty() || optimized_range.IsUnread(atoUINT32(xover_field0_ptr, FALSE)) )
                {
					if (m_ignore_messages)
					{
					}
					else
					{
						BOOL download = TRUE;
						Message message;

						if (m_last_command_param2.HasContent())
						{
							char* param2_ptr = (char*)(m_last_command_param2.CStr());
							message_index_t store_index = atoUINT32(param2_ptr, FALSE);
							OpStatus::Ignore(MessageEngine::GetInstance()->GetStore()->GetMessage(message, store_index));
						}

						Account* account_ptr = m_nntp_backend->GetAccountPtr();

						if (!message.GetId() && !m_nntp_backend->IHave(xover_field[4]))
							RETURN_IF_ERROR(message.Init(account_ptr ? account_ptr->GetAccountId() : 0));
						else if (!message.GetId())
							download = FALSE;

						if (download)
						{
							//Generate internet-location (XRef-header)
							OpString8 servername8;

							if (account_ptr)
								RETURN_IF_ERROR(account_ptr->GetIncomingServername(servername8));

							OpString8 internet_location;
							internet_location.AppendFormat("%s %s %s:%s", xover_field[4].CStr(), servername8.CStr(), m_confirmed_newsgroup.CStr(), xover_field[0].CStr());

							if (!account_ptr) //This should NEVER happen, but better safe than sorry
								internet_location.Empty();

							if ( ((ret=message.SetHeaderValue(Header::SUBJECT,    xover_field[1], FALSE)) == OpStatus::OK) &&
								 ((ret=message.SetHeaderValue(Header::FROM,       xover_field[2], FALSE)) == OpStatus::OK) &&
								 ((ret=message.SetHeaderValue(Header::DATE,       xover_field[3], FALSE)) == OpStatus::OK) &&
								 ((ret=message.SetHeaderValue(Header::MESSAGEID,  xover_field[4], FALSE)) == OpStatus::OK) &&
								 ((ret=message.SetHeaderValue(Header::REFERENCES, xover_field[5], FALSE)) == OpStatus::OK) &&
								 ((ret=message.SetHeaderValue(Header::NEWSGROUPS, m_confirmed_newsgroup)) == OpStatus::OK) &&
								 ((ret=message.SetInternetLocation(internet_location)) == OpStatus::OK) )
							{
								// mark all news messages for fast sorting later without lookup of account
								message.SetFlag(Message::IS_OUTGOING, FALSE);
								message.SetFlag(Message::IS_NEWS_MESSAGE, TRUE);

								char* message_size = (char*)(xover_field[6].CStr());
								message.SetMessageSize(atoUINT32(message_size, FALSE));

								RETURN_IF_ERROR(m_nntp_backend->GetAccountPtr()->Fetched(message, TRUE));
								m_progress.UpdateCurrentAction();
							}

							if ((m_last_flags&(1<<CommandItem::ENABLE_SIGNALLING)) != 0)
								m_nntp_backend->SetSignalMessage();
						}
					}

                    char* nntp_number = (char*)(xover_field[0].CStr());
                    m_current_xover_count = atoUINT32(nntp_number, FALSE); //We have processed an XOVER command
                }
            }
        }

        if (m_status!=ready && xover_start!=m_reply_buffer.CStr()) //We have parsed parts of the buffer. Removed parsed part
        {
            //We need to keep linebreaks if we are not done yet (to be able to locate end-of-text marker)
            if (m_reply_buffer.Length()>=1 && (*(xover_start-1)=='\r' || *(xover_start-1)=='\n'))
                xover_start--;

            if ((ret=m_reply_buffer.Set(xover_start)) != OpStatus::OK)
                return ret;
        }

        if (m_status==ready) //We have parsed all the xover headers
        {
            if (!m_nntp_backend->IsMessageId(m_requested_range))
            {
                //Mark the articles read in the .newsrc-file
				OP_ASSERT(m_confirmed_newsgroup.HasContent());
				OpStatus::Ignore(m_nntp_backend->AddReadRange(m_confirmed_newsgroup, m_requested_range));
            }

            m_requested_range.Empty();
            m_requested_optimized_range.Empty();
        }
    }
    else
    { //Something wrong. Skip this line
		if (code>=400 && code<=599 && code!=NNTP_420_no_current_article) //420 is a legal response
			OpStatus::Ignore(OnError(buffer));

        if ((ret=SkipCurrentLine(buffer)) != OpStatus::OK)
            return ret;
    }

    if (m_status==ready) //Reset counters
    {
        m_current_xover_count = m_min_xover_count = m_max_xover_count = 0;
    }

    return OpStatus::OK;
}

OP_STATUS NNTP::HandlePost(int code, char*& buffer)
{
//+NNTP_235_art_transferred_ok
//NNTP_240_article_posted_ok
//+NNTP_335_transfer_article
//NNTP_340_post_article
//+NNTP_400_service_discont
//+NNTP_435_article_not_wanted
//+NNTP_436_transfer_failed
//+NNTP_437_article_rejected
//NNTP_440_posting_not_allowd
//NNTP_441_posting_failed
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//+NNTP_502_permission_denied
//+NNTP_503_program_fault
    if (code==NNTP_235_art_transferred_ok ||
		code==NNTP_240_article_posted_ok ||
		code==NNTP_335_transfer_article)
    {
        m_current_count++;

        if (m_last_command_param2.HasContent())
        {
            char* param2_ptr = (char*)(m_last_command_param2.CStr());
            message_index_t store_index = atoUINT32(param2_ptr, FALSE);

            Account* account = m_nntp_backend->GetAccountPtr();
            if (account)
                account->Sent(store_index, OpStatus::OK); //Don't call this if sending fails. It could lead to an infinite (network-jamming!) loop
        }
    }
	else
	{
		if (code>=400 && code<=599)
			OpStatus::Ignore(OnError(buffer));

        m_total_count--;
	}

    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleQuit(int code, char*& buffer)
{
//NNTP_205_closing_connection
//+NNTP_400_service_discont
//+NNTP_500_cmd_not_recognized
//+NNTP_501_cmd_syntax_error
//+NNTP_502_permission_denied
//+NNTP_503_program_fault

//    if (code==NNTP_205_closing_connection) //No need for this test. Even for failures, we should drop the connection
    {
        //Deny more data in
        m_is_connected = FALSE;
        m_allow_more_commands = FALSE;

        //Free up memory;
        OP_DELETE(m_connection_timer);
        m_connection_timer = NULL;

        OpStatus::Ignore(StopLoading());

        OpStatus::Ignore(ResetProtocolState());

		m_progress.SetConnected(FALSE);
		m_current_count = m_total_count = m_current_xover_count = m_min_xover_count = m_max_xover_count = 0;
    }
    return SkipCurrentLine(buffer);
}

OP_STATUS NNTP::HandleUnsubscribe(const OpStringC& newsgroup)
{
	OP_STATUS ret;
	OpString current_newsgroup;
	if ((ret=current_newsgroup.Set(m_last_command_sent==CommandItem::GROUP ? m_requested_newsgroup : m_confirmed_newsgroup)) != OpStatus::OK)
		return ret;

	if ((current_newsgroup.CompareI(newsgroup)==0) && m_status!=ready) //We are fetching from the unsubscribed group. Ignore received messages.
		m_ignore_messages = TRUE;

	return m_nntp_backend->HandleUnsubscribe(newsgroup, m_commandlist, current_newsgroup);
}

BOOL NNTP::CurrentCommandMatches(const OpStringC8& newsgroup, CommandItem::Commands command, OpString8* parameter)
{
    if (!newsgroup.IsEmpty() && //Are we requesting a newsgroup?
		!(newsgroup.CompareI(m_confirmed_newsgroup)==0 || //Are we already in the newsgroup?..
		 (m_last_command_sent==CommandItem::GROUP && newsgroup.CompareI(m_requested_newsgroup)==0)) ) //..Or are we in the process of getting into it?
	{
        return FALSE; //We haven't handled this group. Current command does not match
	}

    switch(command)
    {
    case CommandItem::XOVER_LOOP:   if (m_last_command_sent==CommandItem::XOVER) return TRUE; //else fallthrough
    case CommandItem::HEAD_LOOP:    if (m_last_command_sent==CommandItem::HEAD) return TRUE; //else fallthrough
    case CommandItem::ARTICLE_LOOP: return ( (m_last_command_sent==CommandItem::ARTICLE) ||
										     (m_last_command_sent==CommandItem::GROUP && newsgroup.CompareI(m_requested_newsgroup)==0) );

    case CommandItem::GROUP: return (m_last_command_sent==CommandItem::GROUP && parameter && parameter->CompareI(m_requested_newsgroup)==0);

    case CommandItem::HEAD:  if (m_last_command_sent==CommandItem::HEAD && parameter && parameter->CompareI(m_last_command_param1)==0)
                                 return TRUE; //else fallthrough
    case CommandItem::ARTICLE: return (m_last_command_sent==CommandItem::ARTICLE && parameter && parameter->CompareI(m_last_command_param1)==0);

    default: break;
    }

    return FALSE;
}

void NNTP::IncreaseTotalCount(UINT32 count)
{
    if (count>0)
    {
        m_total_count += count;
    }
}

#endif //M2_SUPPORT
