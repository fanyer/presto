/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2006 Opera Software AS.   All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef _SMTP_H_
#define _SMTP_H_

#include "adjunct/m2/src/backend/compat/crcomm.h"
#include "adjunct/m2/src/engine/header.h"
#include "adjunct/m2/src/engine/message.h"

class SmtpBackend;

enum {
    SMTP_SEND_NOTHING=0,
    SMTP_SEND_STARTTLS,
    SMTP_SEND_HELO,
    SMTP_SEND_EHLO,
    SMTP_SEND_MAIL,
    SMTP_SEND_RCPT,
    SMTP_SEND_DATA,
    SMTP_SEND_content,
    SMTP_SEND_RSET,
    SMTP_SEND_AUTH,
	SMTP_SEND_AUTH_LOGIN_USER,
	SMTP_SEND_AUTH_LOGIN_PASS,
	SMTP_SEND_AUTH_PLAIN,
	SMTP_SEND_AUTH_CRAMMD5,
//	SMTP_SEND_AUTH_DIGESTMD5,
    SMTP_SEND_QUIT
};

#define SMTP_RCPT_STATUS_NULL 0
#define SMTP_RCPT_STATUS_TO 1
#define SMTP_RCPT_STATUS_CC 2
#define SMTP_RCPT_STATUS_BCC    3
#define SMTP_RCPT_STATUS_FIN    4

#define ERR_SMTP_SERVICE_UNAVAILABLE    1
#define ERR_SMTP_INTERNAL_ERROR         2
#define ERR_SMTP_SERVER_TMP_ERROR       3
#define ERR_SMTP_SERVER_ERROR           4
#define ERR_SMTP_RCPT_SYNTAX_ERROR      5
#define ERR_SMTP_RCPT_UNAVAILABLE_ERROR 6
#define ERR_SMTP_RCPT_NOT_LOCAL_ERROR   7
#define ERR_SMTP_NO_SERVER_SPEC         8
#define ERR_SMTP_ERROR                  9
#define ERR_SMTP_RCPT_ERROR             10
#define ERR_SMTP_AUTHENTICATION_ERROR	11
#define ERR_SMTP_SERVER_UNAVAILABLE		12
#define ERR_SMTP_TLS_UNAVAILABLE		13
#define ERR_SMTP_AUTH_UNAVAILABLE		14
#define ERR_SMTP_CONNECTION_DROPPED     15

#define USER_AGENT_STR_LEN              128
#define SMTP_REQ_BUF_MIN_LEN            512

#define SMTP_NO_REPLY                             0 // no reply yet
#define SMTP_211_system_status                  221 // system status, or system help reply
#define SMTP_214_help_message                   214 // help message
#define SMTP_220_service_ready                  220 // <domain> service ready
#define SMTP_221_service_closing                221 // <domain> service closing transmission channel
#define SMTP_235_go_ahead						235 // <domain> service closing transmission channel
#define SMTP_250_mail_ok                        250 // requested mail action okay, completed
#define SMTP_251_user_not_local                 251 // user not local, will forward to <forward-path>
#define SMTP_334_continue_req					334 // start mail input; end with <crlf>.<crlf>
#define SMTP_354_start_input                    354 // start mail input; end with <crlf>.<crlf>
#define SMTP_421_unavailable                    421 // <domain> service unavailable, closing transmission channel
#define SMTP_432_password_transition_needed     432 // A password transition is needed
#define SMTP_450_mailbox_unavailable            450 // requested mail action not taken: mailbox unavailable
#define SMTP_451_local_error                    451 // requested action aborted: local error in processing
#define SMTP_452_insuff_sys_storage             452 // requested action not taken: insufficient system storage
#define SMTP_454_TLS_not_available              456 // tls not available due to temporary reason
#define SMTP_500_syntax_error                   500 // syntax error, command unrecognized
#define SMTP_501_parm_syntax_error              501 // syntax error in parameters or arguments
#define SMTP_502_command_not_implemented        502 // command not implemented
#define SMTP_503_bad_cmd_sequence               503 // bad sequence of commands
#define SMTP_504_cmd_not_implemented            504 // command not implemented
#define SMTP_530_must_issue_STARTTLS_1st        530 // this server requires tls authentication
#define SMTP_534_auth_mechanism_too_weak		534 //
#define SMTP_535_auth_failure					535 // auth failure, send again
#define SMTP_538_Encryption_required			538 //
#define SMTP_550_mailbox_unavailable            550 // requested action not taken: mailbox unavailable
#define SMTP_551_user_not_local                 551 // user not local; please try <foward-path>
#define SMTP_552_storage_alloc_exeed            552 // requested mail action aborted: exceeded storage allocation
#define SMTP_553_mailbox_not_allowed            553 // requested action not taken: mailbox name not allowed
#define SMTP_554_transaction_failed             554 // transaction failed
#define SMTP_571_relay_not_allowed              571 // relaying not allowed

//server capabilities
#define SMTP_SUPPORT_AUTH_F						0	///< server supports AUTH
#define SMTP_SUPPORT_AUTH_EXTERNAL_F			1	///< server supports AUTH and EXTERN logins
#define SMTP_SUPPORT_AUTH_DIGESTMD5_F			2	///< server supports AUTH and DIGEST MD5 logins
#define SMTP_SUPPORT_AUTH_CRAMMD5_F				3	///< server supports AUTH and CRAM MD5 logins
#define SMTP_SUPPORT_AUTH_PLAIN_F				4	///< server supports AUTH and PLAIN logins
#define SMTP_SUPPORT_AUTH_LOGIN_F				5	///< server supports AUTH and LOGIN logins
#define SMTP_SUPPORT_STARTTLS_F					6	///< server supports STARTTLS

#ifndef STRINGLENGTH
#define STRINGLENGTH(str) ((sizeof(str)/sizeof(*str))-1)
#endif

class SMTP : public ClientRemoteComm
{
	class MessageInfo
	{
	public:
		message_gid_t m_id;
		BOOL		  m_anonymous;
	};

public:
    SMTP(SmtpBackend* smtp_backend);

	~SMTP();

    OP_STATUS Init(const OpStringC8& servername, UINT16 port);

    OP_STATUS SendMessage(Message& message, BOOL anonymous);

	OP_STATUS SendFirstQueuedMessage();

	int Finished();

	int	GetUploadCount() const;

private:
    SmtpBackend*	const m_smtp_backend;
    OpString8       m_servername;
    UINT16          m_port;

	BOOL			m_is_connected;
    BOOL            m_is_secure_connection;
    BOOL            m_is_authenticated;

    BOOL            m_is_sending_mail;
	MessageInfo*    m_current_message_info;
    OpString8       m_from;
    Header          m_to_header;
    Header          m_cc_header;
    Header          m_bcc_header;
    const Header::From*	m_to_item;
    const Header::From*	m_cc_item;
    const Header::From*	m_bcc_item;

	BOOL			m_first_command_after_starttls; //If STARTTLS causes connection negotiation, use this to know that disconnect is expected
	BOOL			m_connection_has_failed;
    BOOL            m_is_uploading;					//This is set to FALSE when GetNextBlock returns done=TRUE
    BOOL            m_content_finished;				//This is set to TRUE when comm is finished sending data
    char            m_previous_buffer_end[2];

    char*           m_reply_buf;
    int             m_reply_buf_len;
    int             m_reply_buf_loaded;

    int             m_what_to_send;
    int             m_error;

    int             m_successful_rcpt;

    char*           m_request;
    int             m_request_buf_len;

    char*           m_rfc822_message_start;
    char*           m_rfc822_message_remaining;

	int				m_message_size;					///<total size of message being sent
    int             m_message_size_remaining;

	int				m_servercapabilities;	///< this is a bit-field collection of server capabilities, will only set known types

	OpString8		m_server_challenge; ///< Used to store challenge sent from server (when authenticating using CRAM-MD5 etc)

	int             const m_network_buffer_size;	///< size used in preferences

	int				m_sent;

	OpAutoVector<MessageInfo> m_message_queue;

private:
    OP_STATUS		CheckRequestBuf(int min_len);
    int				CheckReply();
	OP_STATUS		ReportError(int errorcode, const OpStringC& server_string);

protected:

    char*			ComposeRequest(int& len);
    void			RequestMoreData();
    OP_STATUS		ProcessReceivedData();

	/**
	 * Takes care of the messages MSG_COMM_LOADING_FINISHED and
	 * MSG_COMM_LOADING_FAILED. Is called from the comm system and
	 * also from a function in this class.
	 */
	void			OnClose(OP_STATUS rc);

	void			OnRestartRequested();

    OP_STATUS		AddDotPrefix(IO char** allocated_buffer);

private:
    SMTP(const SMTP&);
    SMTP&			operator =(const SMTP&);

    void            AskForPassword(const OpStringC8& server_message=NULL);
	OP_STATUS		Parse250Response();
	OP_STATUS		DetermineNextCommand(int currentcommand);
    AccountTypes::AuthenticationType GetNextAuthenticationMethod(AccountTypes::AuthenticationType current_method) const;
	BOOL			MessageIsInQueue(message_gid_t id) const;
};

#endif  // _SMTP_H_
