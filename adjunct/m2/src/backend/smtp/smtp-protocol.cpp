/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#include "core/pch.h"

#ifdef M2_SUPPORT

#include <time.h>
#include <ctype.h>
#include "adjunct/m2/src/backend/smtp/smtp-protocol.h"
#include "adjunct/m2/src/engine/account.h"
#include "adjunct/m2/src/backend/smtp/smtpmodule.h"
#include "adjunct/m2/src/engine/engine.h"
#include "adjunct/m2/src/util/authenticate.h"
#include "adjunct/m2/src/util/misc.h"
#include "adjunct/m2/src/util/qp.h"
#include "modules/locale/oplanguagemanager.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"
#include "modules/url/protocols/comm.h"
#include "modules/util/str.h"
#include "modules/util/excepts.h"


static const char SMTP_EmptySenderAddr[] = "root@localhost.com";
static const char SMTP_EmptyField[]     = " ";
static const char SMTP_HELO_start[]     = "HELO ";
static const char SMTP_HELO_end[]       = "\r\n";
static const char SMTP_EHLO_start[]		= "EHLO ";
static const char SMTP_EHLO_end[]		= "\r\n";
static const char SMTP_MAIL_start[]     = "MAIL FROM:<";
static const char SMTP_MAIL_end[]       = ">\r\n";
static const char SMTP_RCPT_start[]     = "RCPT TO:<";
static const char SMTP_RCPT_end[]       = ">\r\n";
static const char SMTP_DATA_cmd[]       = "DATA\r\n";
static const char SMTP_content_end[]    = "\r\n.\r\n";
static const char SMTP_RSET_cmd[]       = "RSET\r\n";
static const char SMTP_QUIT_cmd[]       = "QUIT\r\n";
static const char SMTP_STARTTLS_cmd[]   = "STARTTLS\r\n";
static const char SMTP_AUTH_start[]		= "AUTH ";
static const char SMTP_AUTH_end[]		= "\r\n";


#define SMTP_HELO_start_len     STRINGLENGTH(SMTP_HELO_start)
#define SMTP_HELO_end_len       STRINGLENGTH(SMTP_HELO_end)
#define SMTP_EHLO_start_len     STRINGLENGTH(SMTP_EHLO_start)
#define SMTP_EHLO_end_len       STRINGLENGTH(SMTP_EHLO_end)
#define SMTP_MAIL_start_len     STRINGLENGTH(SMTP_MAIL_start)
#define SMTP_MAIL_end_len       STRINGLENGTH(SMTP_MAIL_end)
#define SMTP_RCPT_start_len     STRINGLENGTH(SMTP_RCPT_start)
#define SMTP_RCPT_end_len       STRINGLENGTH(SMTP_RCPT_end)
#define SMTP_DATA_cmd_len       STRINGLENGTH(SMTP_DATA_cmd)
#define SMTP_content_end_len    STRINGLENGTH(SMTP_content_end)
#define SMTP_RSET_cmd_len       STRINGLENGTH(SMTP_RSET_cmd)
#define SMTP_QUIT_cmd_len       STRINGLENGTH(SMTP_QUIT_cmd)
#define SMTP_STARTTLS_cmd_len   STRINGLENGTH(SMTP_STARTTLS_cmd)
#define SMTP_AUTH_start_len     STRINGLENGTH(SMTP_AUTH_start)
#define SMTP_AUTH_end_len       STRINGLENGTH(SMTP_AUTH_end)

//********************************************************************

SMTP::SMTP(SmtpBackend* smtp_backend)
: m_smtp_backend(smtp_backend),
  // m_port ?
  m_is_connected(FALSE),
  m_is_secure_connection(FALSE),
  m_is_authenticated(FALSE),
  m_is_sending_mail(FALSE),
  m_current_message_info(NULL),
  m_to_item(NULL),
  m_cc_item(NULL),
  m_bcc_item(NULL),
  m_first_command_after_starttls(FALSE),
  m_connection_has_failed(FALSE),
  m_is_uploading(FALSE), //This is set to FALSE now and when GetNextBlock returns done=TRUE
  m_content_finished(FALSE), //This is set to TRUE when comm is finished sending data
  m_reply_buf(NULL),
  m_reply_buf_len(0),
  m_reply_buf_loaded(0),
  m_what_to_send(SMTP_SEND_NOTHING),
  m_error(SMTP_NO_REPLY),
  // m_successful_rcpt ?
  m_request(NULL),
  m_request_buf_len(0),
  m_rfc822_message_start(NULL),
  m_rfc822_message_remaining(NULL),
  // m_message_size ?
  // m_message_size_remaining ?
  m_servercapabilities(0),
  m_network_buffer_size(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024),
  m_sent(0)
{
    m_previous_buffer_end[0] = m_previous_buffer_end[1] = 0;
}

SMTP::~SMTP()
{
	StopLoading();

	if (m_request)
    {
        FreeMem(m_request);
        m_request = NULL;
    }

    OP_DELETEA(m_rfc822_message_start);
    OP_DELETEA(m_reply_buf);
}

OP_STATUS SMTP::Init(const OpStringC8& servername, UINT16 port)
{
	m_is_sending_mail = FALSE;
	m_port = port;
    return m_servername.Set(servername);
}

//********************************************************************

OP_STATUS SMTP::CheckRequestBuf(int min_len)
{
    if (!m_request)
    {
        m_request_buf_len = max(SMTP_REQ_BUF_MIN_LEN, min_len);
        m_request = AllocMem(m_request_buf_len);
        if (!m_request)
            return OpStatus::ERR_NO_MEMORY;
    }
    else if (min_len > m_request_buf_len)
    { //Don't we have a realloc?
        char* old_buffer = m_request;
        int   old_buffer_len = m_request_buf_len;

        m_request_buf_len = min_len;
        m_request = AllocMem(m_request_buf_len);
        if (!m_request)
            return OpStatus::ERR_NO_MEMORY;

		op_memcpy(m_request, old_buffer, old_buffer_len); // Copies complete old buffer including NULL-termination
        FreeMem(old_buffer);
    }
    return OpStatus::OK;
}


void SMTP::RequestMoreData()
{
    if (m_what_to_send == SMTP_SEND_content && !m_content_finished)
    {
        char* buffer = NULL;
        int   buffer_len = 0;

        if (m_rfc822_message_start && m_is_uploading)
        {
            buffer = AllocMem(m_network_buffer_size+1);	//Don't use m_request here, as its content might not be sent yet
            if (!buffer)
            {
//#pragma PRAGMAMSG("FG: OOM [20020710]")
                buffer_len = 0;
            }
            else
            {
                buffer_len = min(m_message_size_remaining, m_network_buffer_size);
                memcpy(buffer, m_rfc822_message_remaining, buffer_len);
                buffer[buffer_len]=0;

                if (buffer_len >= m_message_size_remaining)
                {
                    m_is_uploading = FALSE;
                    OP_DELETEA(m_rfc822_message_start);
                    m_rfc822_message_start = m_rfc822_message_remaining = NULL;
					m_message_size_remaining = 0;

                    if (buffer_len==0) //This should not happen...
                    {
                        FreeMem(buffer);
                        buffer_len = 0;
                        buffer = NULL;
                    }
                }
                else
                {
                    m_rfc822_message_remaining += buffer_len;
                    m_message_size_remaining -= buffer_len;
                }

                if (AddDotPrefix(&buffer) != OpStatus::OK)
                {
                    buffer_len = 0;
                    buffer = NULL;
                }
                else
                {
                    buffer_len = op_strlen(buffer);
                }
				//we don't know the length of the queue here, or if there is one, so we don't report count
				//m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::SENDING_MESSAGES);
				m_smtp_backend->GetProgress().SetSubProgress(m_message_size-m_message_size_remaining, m_message_size);
            }
        }

        if (buffer)
        {
            m_previous_buffer_end[0] = buffer_len>1 ? buffer[buffer_len-2] : (char)0;
            m_previous_buffer_end[1] = buffer_len>0 ? buffer[buffer_len-1] : (char)0;
        }
        else
        {
            m_previous_buffer_end[0] = m_previous_buffer_end[1] = 0;

            buffer_len = SMTP_content_end_len;
            buffer = AllocMem(buffer_len+1);
            if (!buffer)
            {
//#pragma PRAGMAMSG("FG: OOM [20020710]")
                buffer_len = 0;
            }
            else
            {
                op_strcpy(buffer, SMTP_content_end); // "\r\n.\r\n"
                m_content_finished = TRUE;
            }
        }

        SendData(buffer, buffer_len);
        buffer = NULL;   //the buffer is taken over by communication, we need a new one
    }
}

//********************************************************************

char* SMTP::ComposeRequest(int& len)
{
    switch (m_what_to_send)
    {
    case SMTP_SEND_STARTTLS:
        {
            len = SMTP_STARTTLS_cmd_len;
            CheckRequestBuf(len+1);
            sprintf(m_request,"%s", SMTP_STARTTLS_cmd);
			m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::AUTHENTICATING);
            break;
        }

	case SMTP_SEND_EHLO:
        {
			//m_rcpt_idx = 0;
			m_successful_rcpt = 0;

			OpString fqdn;
			OpString8 fqdn8;
			Account* account_ptr = m_smtp_backend->GetAccountPtr();
			if (account_ptr)
			{
				if (account_ptr->GetFQDN(fqdn)==OpStatus::OK)
				{
					OpStatus::Ignore(OpMisc::ConvertToIMAAAddress(UNI_L(""), fqdn, fqdn8));
				}
			}

			if (fqdn8.IsEmpty())
			{
				fqdn8.Set("error.opera.illegal");

			}

			len = SMTP_EHLO_start_len + fqdn8.Length() + SMTP_EHLO_end_len;
			CheckRequestBuf(len+1);
			sprintf(m_request,"%s%s%s", SMTP_EHLO_start, fqdn8.HasContent() ? fqdn8.CStr() : "", SMTP_EHLO_end);
			m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::AUTHENTICATING);
			break;
		}

	case SMTP_SEND_HELO:
        {
            m_successful_rcpt = 0;
            const char* localhostname = NULL;
			localhostname = Comm::GetLocalHostName();
            if (!localhostname)
            {
                localhostname = "error.opera.illegal";
            }

            len = op_strlen(localhostname) + SMTP_HELO_start_len + SMTP_HELO_end_len;
            CheckRequestBuf(len+1);
            sprintf(m_request,"%s%s%s", SMTP_HELO_start, localhostname, SMTP_HELO_end);
			m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::CONNECTING);
            break;
        }

    case SMTP_SEND_MAIL:
        {
            const char* senderaddr = 0;
            OpString8   email_adr;
            if (!m_current_message_info->m_anonymous)
            {
                OP_STATUS ret;
                if ((ret=email_adr.Set(m_from)) != OpStatus::OK)
                {
                    len = 0;
                    return NULL;
                }

                if (email_adr.IsEmpty())
                {
                    if ((ret=m_smtp_backend->GetAccountPtr()->GetEmail(email_adr)) != OpStatus::OK)
                    {
                        len = 0;
                        return NULL;
                    }
                }
                senderaddr = email_adr.CStr();
            }

            if (!senderaddr)
            {
                senderaddr = SMTP_EmptySenderAddr;
            }
            len = op_strlen(senderaddr) + SMTP_MAIL_start_len + SMTP_MAIL_end_len;
            CheckRequestBuf(len+1);
            sprintf(m_request,"%s%s%s", SMTP_MAIL_start, senderaddr, SMTP_MAIL_end);
			m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::SENDING_MESSAGES);
            break;
        }

    case SMTP_SEND_RCPT:
        {
            OP_STATUS ret;
            OpString8 email_adr;
            const Header::From* temp_item = NULL;
            if (m_to_item)
            {
                temp_item = m_to_item;
                m_to_item = (Header::From*)m_to_item->Suc();
            }
            else if (m_cc_item)
            {
                temp_item = m_cc_item;
                m_cc_item = (Header::From*)m_cc_item->Suc();
            }
            else if (m_bcc_item)
            {
                temp_item = m_bcc_item;
                m_bcc_item = (Header::From*)m_bcc_item->Suc();
            }

            if (!temp_item)
            {
                len=0;
                return NULL;
            }

			BOOL imaa_failed = FALSE;
            if ((ret=temp_item->GetIMAAAddress(email_adr, &imaa_failed)) != OpStatus::OK || imaa_failed)
            {
                len=0;
                return NULL;
            }

            len = email_adr.Length() + SMTP_RCPT_start_len + SMTP_RCPT_end_len;
            CheckRequestBuf(len+1);
            sprintf(m_request, "%s%s%s", SMTP_RCPT_start, email_adr.CStr(), SMTP_RCPT_end);
			m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::SENDING_MESSAGES);
            break;
        }

        case SMTP_SEND_DATA:
            {
                len = SMTP_DATA_cmd_len;
                CheckRequestBuf(len+1);
                sprintf(m_request,"%s", SMTP_DATA_cmd);
				m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::SENDING_MESSAGES);
                break;
            }

        case SMTP_SEND_content:
            {
                OP_ASSERT(m_is_sending_mail);
                if (!m_is_sending_mail)
                {
                    len = 0;
                    return NULL;
                }

				CheckRequestBuf(m_network_buffer_size+1);

                len = min(m_message_size_remaining, m_request_buf_len-1);
                memcpy(m_request, m_rfc822_message_remaining, len);
                m_request[len]=0;
                if (len >= m_message_size_remaining)
                {
                    m_is_uploading = FALSE;
                    OP_DELETEA(m_rfc822_message_start);
                    m_rfc822_message_start = m_rfc822_message_remaining = NULL;
					m_message_size_remaining = 0;
                }
                else
                {
                    m_is_uploading = TRUE;
                    m_rfc822_message_remaining += len;
                    m_message_size_remaining -= len;
                }

                if (AddDotPrefix(&m_request) != OpStatus::OK)
                {
                    len = 0;
                    return NULL;
                }
                else
                {
                    len = op_strlen(m_request);
                }


                m_previous_buffer_end[0] = (len>1 && m_is_uploading) ? m_request[len-2] : (char)0;
                m_previous_buffer_end[1] = (len>0 && m_is_uploading) ? m_request[len-1] : (char)0;

				m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::SENDING_MESSAGES);
				m_smtp_backend->GetProgress().SetSubProgress(m_message_size-m_message_size_remaining, m_message_size);

                break;
            }

		case SMTP_SEND_RSET:
            {
                len = SMTP_RSET_cmd_len;
                CheckRequestBuf(len+1);
                sprintf(m_request,"%s", SMTP_RSET_cmd);
                break;
            }

        case SMTP_SEND_AUTH:
            {
                AccountTypes::AuthenticationType authmethod = m_smtp_backend->GetAuthenticationMethod();
                OP_ASSERT(authmethod!=AccountTypes::NONE); //Shouldn't get here if no authentication is requested

#ifdef AUTODETECT_SMTP_PLAINTEXT_AUTH
				if (authmethod==AccountTypes::PLAIN && m_smtp_backend->GetCurrentAuthMethod()!=AccountTypes::LOGIN) //PLAIN should first try LOGIN, then PLAIN (no reason to let users choose between them, it will only clutter UI)
                    authmethod = AccountTypes::LOGIN;
#endif

				if (authmethod == AccountTypes::AUTOSELECT)
				{
					authmethod = m_smtp_backend->GetCurrentAuthMethod();

					if (authmethod == AccountTypes::AUTOSELECT)
						authmethod = GetNextAuthenticationMethod(authmethod);
				}

				m_smtp_backend->SetCurrentAuthMethod(authmethod);

				const char* authtype = 0;
                switch(m_smtp_backend->GetCurrentAuthMethod())
                {
					case AccountTypes::PLAIN:
						authtype = "PLAIN";
						break;
					case AccountTypes::LOGIN:
						authtype = "LOGIN";
						break;
					case AccountTypes::CRAM_MD5:
						authtype = "CRAM-MD5";
						break;
					case AccountTypes::NONE: //Fallthrough
					default:
						authtype = "unsupported";
						OP_ASSERT(!"Should never try to send unsupported authentication methods");
						break;
                }

				len = op_strlen(authtype) + SMTP_AUTH_start_len + SMTP_AUTH_end_len + 3;
				CheckRequestBuf(len+1);
				sprintf(m_request,"%s%s%s", SMTP_AUTH_start, authtype, SMTP_AUTH_end);
				m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::AUTHENTICATING);
			}
			break;

		case SMTP_SEND_AUTH_PLAIN:
		case SMTP_SEND_AUTH_LOGIN_USER:
		case SMTP_SEND_AUTH_LOGIN_PASS:
		case SMTP_SEND_AUTH_CRAMMD5:
			{
				OpAuthenticate   login_info;
				const OpStringC8* response;
				len = 0;

				RETURN_VALUE_IF_ERROR(m_smtp_backend->GetLoginInfo(login_info, m_server_challenge), 0);

				if (m_what_to_send == SMTP_SEND_AUTH_LOGIN_USER)
					response = &login_info.GetUsername();
				else if (m_what_to_send == SMTP_SEND_AUTH_LOGIN_PASS)
					response = &login_info.GetPassword();
				else
					response = &login_info.GetResponse();

				len = response->Length() + SMTP_AUTH_end_len;
				CheckRequestBuf(len + 1);
				sprintf(m_request, "%s%s", response->CStr(), SMTP_AUTH_end);
				m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::AUTHENTICATING);
			}
			break;
/*
		case SMTP_SEND_AUTH_DIGESTMD5:
			{
			}
			break;
*/

        case SMTP_SEND_QUIT:
            {
                len = SMTP_QUIT_cmd_len;
                CheckRequestBuf(len+1);
                sprintf(m_request,"%s", SMTP_QUIT_cmd);
                break;
            }

        default:
            len = 0;
            break;
    }

    m_smtp_backend->Log("SMTP OUT : ", m_request);

	if (m_request)
    {
        len = op_strlen(m_request);
    }

    return m_request;

}

//********************************************************************

int SMTP::CheckReply()
{
    OP_NEW_DBG("SMTP::CheckReply", "m2");
    m_smtp_backend->Log("SMTP IN : ", m_reply_buf);

    if (m_reply_buf_loaded > 3)
    {
        char *tmp = m_reply_buf;
        while (tmp)
        {
            char* tmp2 = op_strchr(tmp, '\n');
            if (tmp2 && *tmp>='0' && *tmp<='9')
            {
                if (*(tmp+3) == '-')
                    tmp = tmp2+1;
                else
                {
                    m_reply_buf_loaded = 0;
                    return atoi(tmp);
                }
            }
            else
                tmp = 0;
        }
    }

    OP_DBG(("SMTP::Error:?? No reply!!\r\n"));

    return SMTP_NO_REPLY;
}

//********************************************************************

OP_STATUS SMTP::ProcessReceivedData()
{
	OP_NEW_DBG("SMTP::ProcessReceivedData", "m2");
	m_connection_has_failed = FALSE;

    int cnt_len;
    if (m_reply_buf_len-m_reply_buf_loaded <= 512)
    {
        int new_len = m_reply_buf_len + 1024;

        char *tmp = OP_NEWA(char, new_len);
		if (!tmp)
			return OpStatus::ERR_NO_MEMORY;

        memcpy(tmp, m_reply_buf, (int)m_reply_buf_loaded);
	    OP_DELETE(m_reply_buf);
        m_reply_buf = tmp;
		m_reply_buf_len = new_len;
    }

    cnt_len = ReadData(m_reply_buf+m_reply_buf_loaded, m_reply_buf_len-m_reply_buf_loaded-1);

    if (cnt_len <= 0)
    {
        return OpStatus::ERR;
    }

    *(m_reply_buf+m_reply_buf_loaded+cnt_len) = '\0';
    m_reply_buf_loaded += cnt_len;

	m_first_command_after_starttls = FALSE;

    OP_DBG((m_reply_buf));

    int reply = CheckReply();

    if (reply == SMTP_NO_REPLY)
        return OpStatus::ERR;

    switch (m_what_to_send)
    {
    case SMTP_SEND_NOTHING:															//first
        if (reply == SMTP_220_service_ready)
        {
            m_is_connected = TRUE;
            m_what_to_send = SMTP_SEND_EHLO;
        }
        else
        {
			switch(reply)
			{
			case SMTP_421_unavailable: m_error = ERR_SMTP_SERVICE_UNAVAILABLE; break;
			default:				   m_error = ERR_SMTP_INTERNAL_ERROR;
			}
        }
        break;

    case SMTP_SEND_STARTTLS:
        if (reply == SMTP_220_service_ready)
        {
            if(StartTLSConnection())	// Start TLS authentication
            {
                m_is_secure_connection = TRUE;
			    m_servercapabilities = 0; //A client must forget about old server capabilities after starting a TLS connection
                m_what_to_send = SMTP_SEND_EHLO;
				m_first_command_after_starttls = TRUE;
            }
            else
            {
                // Failed to start TLS authentication
                m_is_secure_connection = FALSE;
                m_error = ERR_SMTP_SERVICE_UNAVAILABLE;
                m_what_to_send = SMTP_SEND_QUIT;
            }
        }
        else if (reply == SMTP_454_TLS_not_available)
        {
            // TLS authentication not available right now
            m_is_secure_connection = FALSE;
            m_error = ERR_SMTP_SERVICE_UNAVAILABLE;
            m_what_to_send = SMTP_SEND_QUIT;
        }
        else
        {
            // Does not support TLS
            m_is_secure_connection = FALSE;
            m_error = ERR_SMTP_SERVICE_UNAVAILABLE;
            m_what_to_send = SMTP_SEND_QUIT;
        }
        break;

    case SMTP_SEND_HELO:
        if (reply == SMTP_250_mail_ok)
		{
			OpStatus::Ignore(Parse250Response());					//updates m_servercapabilities
            m_what_to_send = SMTP_SEND_MAIL;
			m_error = SMTP_NO_REPLY;
		}
        else
        {
            m_content_finished = FALSE;
			switch(reply)
			{
			case SMTP_421_unavailable: m_error = ERR_SMTP_SERVICE_UNAVAILABLE; break;
			default:				   m_error = ERR_SMTP_INTERNAL_ERROR;
			}
        }
        break;

    case SMTP_SEND_EHLO:
        if (reply == SMTP_250_mail_ok)
		{
			OpStatus::Ignore(Parse250Response());					//updates m_servercapabilities
			RETURN_IF_ERROR(DetermineNextCommand(SMTP_SEND_EHLO));
		}
        else	// try HELO which should be supported by all servers
        {
            m_content_finished = FALSE; // this is not an error, EHLO is optionally supported by the server
			m_what_to_send = SMTP_SEND_HELO;
			m_error = SMTP_NO_REPLY;
        }
        break;

    case SMTP_SEND_AUTH:
		{
			if(reply == SMTP_334_continue_req)
			{
				switch (m_smtp_backend->GetCurrentAuthMethod())
				{
                case AccountTypes::CRAM_MD5: m_what_to_send = SMTP_SEND_AUTH_CRAMMD5; m_server_challenge.Set(m_reply_buf+4); break;
				case AccountTypes::LOGIN: m_what_to_send = SMTP_SEND_AUTH_LOGIN_USER; break;
				case AccountTypes::PLAIN: m_what_to_send = SMTP_SEND_AUTH_PLAIN; break;
				default: m_what_to_send = SMTP_SEND_QUIT; m_error = ERR_SMTP_AUTHENTICATION_ERROR; break;
				}
			}
			else if(reply == SMTP_235_go_ahead)
			{
				OP_ASSERT(0); //Is it possible to get here?
				m_is_authenticated = TRUE;
				m_what_to_send = SMTP_SEND_MAIL;
			}
			else if(reply == SMTP_535_auth_failure)
			{
                AskForPassword(m_reply_buf);
			}
			else
			{
                AccountTypes::AuthenticationType selected_auth = m_smtp_backend->GetAuthenticationMethod();
                if (m_smtp_backend->GetCurrentAuthMethod() != AccountTypes::NONE &&
                    (selected_auth==AccountTypes::AUTOSELECT
#ifdef AUTODETECT_SMTP_PLAINTEXT_AUTH
                     || selected_auth==AccountTypes::PLAIN
#endif
                     ) )
                {
					m_smtp_backend->SetCurrentAuthMethod(GetNextAuthenticationMethod(m_smtp_backend->GetCurrentAuthMethod()));

                    if (m_smtp_backend->GetCurrentAuthMethod() == AccountTypes::NONE)
                        m_what_to_send = SMTP_SEND_MAIL;
                    else
                        m_what_to_send = SMTP_SEND_AUTH; //Try next authentication method
                }
                else
                {
				    switch(reply)
				    {
				    case SMTP_421_unavailable:
				    case SMTP_500_syntax_error: m_error = ERR_SMTP_SERVICE_UNAVAILABLE; break;
				    default:					m_error = ERR_SMTP_INTERNAL_ERROR;
				    }
                }
			}

		}
		break;


	case SMTP_SEND_AUTH_LOGIN_USER:
		{
			if(reply == SMTP_334_continue_req)
			{
				m_what_to_send = SMTP_SEND_AUTH_LOGIN_PASS;
			}
			else
			{
                AskForPassword(m_reply_buf);
			}

		}
		break;

	case SMTP_SEND_AUTH_LOGIN_PASS:
		{
			if(reply == SMTP_235_go_ahead)
			{
				m_smtp_backend->SetCurrentAuthMethod(AccountTypes::LOGIN);
				m_is_authenticated = TRUE;
				m_what_to_send = SMTP_SEND_MAIL;
			}
			else //if(SMTP_535_auth_failure)
			{
                AskForPassword(m_reply_buf);
			}
		}
		break;

	case SMTP_SEND_AUTH_PLAIN:
		{
			if(reply == SMTP_235_go_ahead)
			{
				m_smtp_backend->SetCurrentAuthMethod(AccountTypes::PLAIN);
				m_is_authenticated = TRUE;
				m_what_to_send = SMTP_SEND_MAIL;
			}
			else //if(SMTP_535_auth_failure)
			{
                AskForPassword(m_reply_buf);
			}

		}
		break;

	case SMTP_SEND_AUTH_CRAMMD5:
		{
			if(reply == SMTP_235_go_ahead)
			{
				m_smtp_backend->SetCurrentAuthMethod(AccountTypes::CRAM_MD5);
				m_is_authenticated = TRUE;
				m_what_to_send = SMTP_SEND_MAIL;
			}
			else //if(SMTP_535_auth_failure)
			{
                AskForPassword(m_reply_buf);
			}
		}
		break;

    case SMTP_SEND_MAIL:
        if (reply == SMTP_250_mail_ok)
		{
            m_what_to_send = SMTP_SEND_RCPT;
		}
        else
        {
			switch(reply)
			{
			case SMTP_421_unavailable:		   m_error = ERR_SMTP_SERVICE_UNAVAILABLE; break;

			case SMTP_451_local_error:
			case SMTP_452_insuff_sys_storage:  m_error = ERR_SMTP_SERVER_TMP_ERROR; break;

			case SMTP_552_storage_alloc_exeed: m_error = ERR_SMTP_SERVER_ERROR; break;

			case SMTP_535_auth_failure:		   AskForPassword(m_reply_buf); break;

			default:						   m_error = ERR_SMTP_INTERNAL_ERROR;
			}
        }
        break;

    case SMTP_SEND_RCPT:
        if (reply == SMTP_250_mail_ok || reply == SMTP_251_user_not_local)
        {
            m_successful_rcpt++;
			if (m_what_to_send == SMTP_SEND_RCPT && !m_to_item && !m_cc_item && !m_bcc_item)
			{
				m_what_to_send = SMTP_SEND_DATA;
			}
        }
        else
        {
			switch(reply)
			{
			case SMTP_421_unavailable:         m_error = ERR_SMTP_SERVICE_UNAVAILABLE; break;

			case SMTP_450_mailbox_unavailable:
			case SMTP_451_local_error:
			case SMTP_452_insuff_sys_storage:  m_error = ERR_SMTP_RCPT_ERROR; break;

			case SMTP_500_syntax_error:
			case SMTP_501_parm_syntax_error:
			case SMTP_553_mailbox_not_allowed: m_error = ERR_SMTP_RCPT_ERROR; break;

			case SMTP_550_mailbox_unavailable: m_error = ERR_SMTP_RCPT_ERROR; break;

			case SMTP_551_user_not_local:	   m_error = ERR_SMTP_RCPT_ERROR; break;

			case SMTP_552_storage_alloc_exeed: m_error = ERR_SMTP_SERVER_ERROR; break;

			case SMTP_571_relay_not_allowed:   m_error = ERR_SMTP_RCPT_ERROR; break;

			default:						   m_error = ERR_SMTP_INTERNAL_ERROR;
			}
        }
        break;

    case SMTP_SEND_DATA:
        if (reply == SMTP_354_start_input)
		{
            m_what_to_send = SMTP_SEND_content;
		}
        else
        {
			switch(reply)
			{
			case SMTP_421_unavailable: m_error = ERR_SMTP_SERVICE_UNAVAILABLE; break;
			case SMTP_451_local_error: m_error = ERR_SMTP_SERVER_TMP_ERROR; break;
			default:				   m_error = ERR_SMTP_INTERNAL_ERROR;
			}
        }
        break;

    case SMTP_SEND_content:
        if (reply == SMTP_250_mail_ok)
		{
			m_sent++;
			m_smtp_backend->GetProgress().UpdateCurrentAction();

			if (m_smtp_backend->Sent(m_current_message_info->m_id, OpStatus::OK) != OpStatus::OK)
			{
				OpString errorstring;

				OpStatus::Ignore(g_languageManager->GetString(Str::S_FAILED_MOVING_OUT_OF_OUTBOX, errorstring));

				m_smtp_backend->OnError(errorstring);
			}

			m_message_queue.Delete(m_current_message_info);
			m_current_message_info = NULL;

			if (m_message_queue.GetCount()==0)
			{
				m_what_to_send = SMTP_SEND_QUIT;
			}
			else
			{
				OP_ASSERT(m_is_connected);
				while (OpStatus::IsError(SendFirstQueuedMessage()))
				{
					if (m_message_queue.GetCount()==0)
					{
						m_what_to_send = SMTP_SEND_QUIT;
						break;
					}
				}
				//Bail out if SendFirstQueuedMessage has taken down the connection
				if (!m_is_connected)
					return OpStatus::OK;
			}
		}
        else
        {
			switch(reply)
			{
			case SMTP_421_unavailable:         m_error = ERR_SMTP_SERVICE_UNAVAILABLE; break;

			case SMTP_451_local_error:
			case SMTP_452_insuff_sys_storage:  m_error = ERR_SMTP_SERVER_TMP_ERROR; break;

			case SMTP_552_storage_alloc_exeed:
			case SMTP_554_transaction_failed:  m_error = ERR_SMTP_SERVER_ERROR; break;

			default:						   m_error = ERR_SMTP_INTERNAL_ERROR;
			}
        }
        break;

    case SMTP_SEND_RSET:
		{
			//We have failed to send one message. Remove it from progress-information
			m_smtp_backend->GetProgress().AddToCurrentAction(-1);
			m_smtp_backend->GetProgress().UpdateCurrentAction(-1);
			m_smtp_backend->GetProgress().SetSubProgress(0, 0);

			m_message_queue.Delete(m_current_message_info);
			m_current_message_info = NULL;

			if (m_message_queue.GetCount()==0)
			{
				m_what_to_send = SMTP_SEND_QUIT;
			}
			else
			{
				OP_ASSERT(m_is_connected);
				while (OpStatus::IsError(SendFirstQueuedMessage()))
				{
					if (m_message_queue.GetCount()==0)
					{
						m_what_to_send = SMTP_SEND_QUIT;
						break;
					}
				}
				//Bail out if SendFirstQueuedMessage has taken down the connection
				if (!m_is_connected)
					return OpStatus::OK;
			}
		}
        break;

    case SMTP_SEND_QUIT:
		{
			OP_DELETEA(m_rfc822_message_start);
			m_rfc822_message_start = m_rfc822_message_remaining = NULL;

			m_what_to_send = SMTP_SEND_NOTHING;

			OnClose(OpStatus::OK); //Drop connection, we're done
			return OpStatus::OK;
		}
        break;

    default:
		OP_ASSERT(0);
        m_error = ERR_SMTP_INTERNAL_ERROR;
    }

	//Skip this mail if something failed
	if (m_error!=SMTP_NO_REPLY && m_what_to_send!=SMTP_SEND_QUIT)
	{
		m_what_to_send = SMTP_SEND_RSET;
		OpString server_string;
		if (server_string.Set(m_reply_buf)==OpStatus::OK)
		{
			OpStatus::Ignore(ReportError(m_error, server_string));
			m_error = SMTP_NO_REPLY;
		}
	}

    int len;
    ComposeRequest(len);

	OP_DBG(("SMTP::m_request= %s\n", m_request));

    if(m_request)
    {
        SendData(m_request, len);
        m_request = NULL;   //the buffer is taken over by communication, we need a new one
    }

    return OpStatus::OK;
}

//********************************************************************

OP_STATUS SMTP::DetermineNextCommand(int currentcommand)
{
	switch(currentcommand)
	{
	case SMTP_SEND_EHLO:
		{
			OP_STATUS ret = OpStatus::OK;
            if (!m_is_secure_connection) //Check if we should send STARTTLS
            {
			    BOOL use_tls;
			    if ((ret=m_smtp_backend->GetUseSecureConnection(use_tls)) != OpStatus::OK)
			    {
				    return ret;
			    }

                if (use_tls)
                {
			        if(!(m_servercapabilities & (1 << SMTP_SUPPORT_STARTTLS_F)) )
			        {
                        m_error = ERR_SMTP_TLS_UNAVAILABLE;
                        m_what_to_send = SMTP_SEND_QUIT;
				        return OpStatus::OK;
			        }

                    m_what_to_send = SMTP_SEND_STARTTLS;
                    return OpStatus::OK;
                }
            }

            m_is_authenticated = m_smtp_backend->GetCurrentAuthMethod() == AccountTypes::NONE;
	
			if (m_is_authenticated)
				m_what_to_send = SMTP_SEND_MAIL;
			else
				m_what_to_send = SMTP_SEND_AUTH;
		}
		break;

	default:
		{
            OP_ASSERT(0);
			//should not happen
		}
	}

	return OpStatus::OK;
}

BOOL SMTP::MessageIsInQueue(message_gid_t id) const
{
	for (int i = m_message_queue.GetCount() - 1; i >= 0; i--)
	{
		MessageInfo* queue_item = m_message_queue.Get(i);

		if (queue_item && queue_item->m_id == id)
			return TRUE;
	}

	return FALSE;
}

//********************************************************************

void SMTP::OnClose(OP_STATUS rc)
{
	OP_ASSERT(!m_first_command_after_starttls);

	m_smtp_backend->Log("SMTP IN : ", "Disconnected");

	// callback to engine
	if(rc == OpStatus::ERR_NO_ACCESS)
	{
		m_error = ERR_SMTP_SERVER_UNAVAILABLE;
	}

    if (m_what_to_send==SMTP_SEND_MAIL || m_what_to_send==SMTP_SEND_AUTH) //Server dropped connection when we tried to send a mail. Probably because it didn't like our authentication state
    {
        if (m_error==SMTP_NO_REPLY)
            m_error = ERR_SMTP_CONNECTION_DROPPED;
    }

	m_smtp_backend->Disconnect();
    if (m_error != SMTP_NO_REPLY)
    {
		if (m_error == ERR_SMTP_SERVER_UNAVAILABLE)
		{
			if (m_connection_has_failed)
			{
				m_error = SMTP_NO_REPLY;
				return;
			}
			else
			{
				m_connection_has_failed = TRUE;
			}
		}
		ReportError(m_error, UNI_L(""));
		m_error = SMTP_NO_REPLY;
    }
}

//********************************************************************

void SMTP::OnRestartRequested()
{
	Finished();
	SendFirstQueuedMessage();
}

//********************************************************************

OP_STATUS SMTP::ReportError(int errorcode, const OpStringC& server_string)
{
    int title_id;
	switch(m_error)
	{
	case ERR_SMTP_SERVER_UNAVAILABLE:     title_id = Str::S_SMTP_ERR_SENDING_FAILED; break;
	case ERR_SMTP_SERVICE_UNAVAILABLE:    title_id = Str::S_SMTP_ERR_SERVICE_UNAVAILABLE; break;
	case ERR_SMTP_INTERNAL_ERROR:	      title_id = Str::S_SMTP_ERR_INTERNAL_ERROR; break;
	case ERR_SMTP_SERVER_TMP_ERROR:	      title_id = Str::S_SMTP_ERR_TEMPORARY_SERVER_ERROR; break;
	case ERR_SMTP_SERVER_ERROR:           title_id = Str::S_SMTP_ERR_SERVER_ERROR; break;
	case ERR_SMTP_RCPT_SYNTAX_ERROR:      title_id = Str::S_SMTP_ERR_RECIPIENT_SYNTAX_ERROR; break;
	case ERR_SMTP_RCPT_UNAVAILABLE_ERROR: title_id = Str::S_SMTP_ERR_RECIPIENT_UNAVAILABLE; break;
	case ERR_SMTP_RCPT_NOT_LOCAL_ERROR:	  title_id = Str::S_SMTP_ERR_RECIPIENT_NOT_LOCAL; break;
	case ERR_SMTP_NO_SERVER_SPEC:		  title_id = Str::S_SMTP_ERR_NO_SERVER_SPECIFIED; break;
	case ERR_SMTP_ERROR:				  title_id = Str::S_SMTP_ERROR; break;
	case ERR_SMTP_RCPT_ERROR:			  title_id = Str::S_SMTP_ERR_RECIPIENT_ERROR; break;
	case ERR_SMTP_AUTHENTICATION_ERROR:   title_id = Str::S_SMTP_ERR_AUTHENTICATION_ERROR; break;
	case ERR_SMTP_TLS_UNAVAILABLE:		  title_id = Str::S_SMTP_ERR_TLS_NOT_SUPPORTED; break;
	case ERR_SMTP_AUTH_UNAVAILABLE:		  title_id = Str::S_SMTP_ERR_SMTP_AUTH_NOT_SUPPORTED; break;
    case ERR_SMTP_CONNECTION_DROPPED:     title_id = Str::S_SMTP_ERR_SERVER_DROPPED_CONNECTION; break;
	default:							  title_id = Str::S_SMTP_ERR_UNSPECIFIED; break;
	}

	OP_STATUS ret;
    OpString status_message;

	RETURN_IF_ERROR(g_languageManager->GetString((Str::LocaleString)title_id, status_message));

	if (server_string.HasContent())
	{
		OpString error_message;
		if ((ret=error_message.Set(UNI_L(" [")))!=OpStatus::OK ||
			(ret=error_message.Append(server_string))!=OpStatus::OK ||
			(ret=error_message.Append(UNI_L("]")))!=OpStatus::OK)
		{
			return ret;
		}

		RemoveChars(error_message, UNI_L("\r\n\t"));		// FIX OOM

		if ((ret=status_message.Append(error_message))!=OpStatus::OK)
			return ret;
	}


	if (status_message.HasContent())
		m_smtp_backend->OnError(status_message);

	return OpStatus::OK;
}

//********************************************************************

int SMTP::Finished()
{

	StopLoading();

	m_is_sending_mail = FALSE;
	m_is_connected = FALSE;
    m_is_secure_connection = FALSE;
    m_is_authenticated = FALSE;
	m_smtp_backend->GetProgress().EndCurrentAction(TRUE);

	return 0;
}

int SMTP::GetUploadCount() const
{
	return m_sent;
}

//********************************************************************

OP_STATUS SMTP::SendMessage(Message& message, BOOL anonymous)
{
	if (!MessageIsInQueue(message.GetId()))
	{
		MessageInfo* info = OP_NEW(MessageInfo, ());
		if (!info)
			return OpStatus::ERR_NO_MEMORY;

		info->m_id = message.GetId();
		info->m_anonymous = anonymous;

		OP_STATUS ret = m_message_queue.Add(info);
		if (OpStatus::IsError(ret))
		{
			OP_DELETE(info);
			return ret;
		}
		m_smtp_backend->GetProgress().AddToCurrentAction(1);
	}

	if (!m_is_sending_mail)
		return SendFirstQueuedMessage();

	return OpStatus::OK;
}

OP_STATUS SMTP::SendFirstQueuedMessage()
{
    OP_STATUS ret;

	/// Find and prepare first message in queue
	m_current_message_info = m_message_queue.Get(0);
	if (!m_current_message_info)
		return OpStatus::ERR_NULL_POINTER;

	Account* account = m_smtp_backend->GetAccountPtr();
	Message message;
	if (account)
	{
		if ((ret=account->PrepareToSendMessage(m_current_message_info->m_id, m_current_message_info->m_anonymous, message)) != OpStatus::OK)
		{
			m_message_queue.Delete(m_current_message_info);
			m_current_message_info = NULL;
			m_smtp_backend->Log("SMTP OUT : ", "M2 failed to prepare message for sending");
			return ret;
		}
	}

	BOOL is_resent = message.IsFlagSet(Message::IS_RESENT);

	//Find sender
    m_from.Empty();
    Header* temp_header = message.GetHeader(is_resent ? Header::RESENTFROM : Header::FROM);
    const Header::From* from = temp_header?temp_header->GetFirstAddress():NULL;
	BOOL imaa_failed = FALSE;
    if (from && ((ret=from->GetIMAAAddress(m_from, &imaa_failed))!=OpStatus::OK || imaa_failed))
    {
		if (imaa_failed && ret==OpStatus::OK)
			ret=OpStatus::ERR_PARSING_FAILED;

        return ret;
    }

	//Find recipients
	m_to_item = m_cc_item = m_bcc_item = NULL; //Clear references to headers
    if (message.GetHeader(is_resent ? Header::RESENTTO : Header::TO))
    {
        m_to_header = *message.GetHeader(is_resent ? Header::RESENTTO : Header::TO);
        m_to_header.DetachFromMessage();
        m_to_item = m_to_header.GetFirstAddress();
    }

    if (message.GetHeader(is_resent ? Header::RESENTCC : Header::CC))
    {
        m_cc_header = *message.GetHeader(is_resent ? Header::RESENTCC : Header::CC);
        m_cc_header.DetachFromMessage();
        m_cc_item = m_cc_header.GetFirstAddress();
    }

    if (message.GetHeader(is_resent ? Header::RESENTBCC : Header::BCC))
    {
        m_bcc_header = *message.GetHeader(is_resent ? Header::RESENTBCC : Header::BCC);
        m_bcc_header.DetachFromMessage();
        m_bcc_item = m_bcc_header.GetFirstAddress();
    }

	//If we have noone to send message to, ignore it and send next message
	OP_ASSERT(m_to_item || m_cc_item || m_bcc_item);
	if (!m_to_item && !m_cc_item && !m_bcc_item)
	{
		m_message_queue.Delete(m_current_message_info);
		m_current_message_info = NULL;
		if (m_message_queue.GetCount() > 0)
		{
			return SendFirstQueuedMessage();
		}
		else //No more messages. End session
		{
			m_smtp_backend->StopSendingMessage();
			return OpStatus::OK;
		}
	}

#ifdef _DEBUG
	OpString8 debug_rawmessage;
	OpStatus::Ignore(message.GetRawMessage(debug_rawmessage, FALSE, FALSE, FALSE));
	message.CopyCurrentToOriginalHeaders();
#endif

	//Get raw message
    OpString8 tmp_rawmessage;
    if ((ret=message.GetRawMessage(tmp_rawmessage, FALSE, FALSE, FALSE)) != OpStatus::OK) //Bcc will be removed here
        return ret;

#ifdef _DEBUG
	int first_diff = debug_rawmessage.Compare(tmp_rawmessage);
	OP_ASSERT(first_diff==0);
#endif

    int tmp_rawmessagelen = tmp_rawmessage.Length();
	OP_DELETEA(m_rfc822_message_start);		// can be left over if previous message upload failed
	m_rfc822_message_start = OP_NEWA(char, tmp_rawmessagelen+1);
    if (!m_rfc822_message_start)
        return OpStatus::ERR_NO_MEMORY;
    memcpy(m_rfc822_message_start, tmp_rawmessage.CStr(), tmp_rawmessagelen);
    m_rfc822_message_start[tmp_rawmessagelen] = 0;
    m_rfc822_message_remaining = m_rfc822_message_start;
    m_message_size_remaining = tmp_rawmessagelen;

	m_message_size = tmp_rawmessagelen;

	//BCC (or, for redirected messages, RESENTBCC) header MUST not be among the headers sent
	OP_ASSERT(op_stristr(m_rfc822_message_start, is_resent ? "Resent-bcc: " : "Bcc: ")==NULL); //This might trigger too often, but searching for "\nbcc: " would trigger to seldom, and that is no good

	//Date must be present
	OP_ASSERT(op_stristr(m_rfc822_message_start, is_resent ? "Resent-date: " : "Date: ")!=NULL);

	/// Send message
	m_successful_rcpt = 0;
	m_is_sending_mail = TRUE;
	m_smtp_backend->GetProgress().UpdateCurrentAction();
	m_smtp_backend->GetProgress().SetSubProgress(0, 0);

    m_what_to_send = m_is_connected ? SMTP_SEND_MAIL : SMTP_SEND_NOTHING;
    m_error = SMTP_NO_REPLY;
    m_previous_buffer_end[0] = m_previous_buffer_end[1] = 0;
    m_is_uploading = FALSE;									//This is set to FALSE now and when GetNextBlock returns done=TRUE
    m_content_finished = FALSE;								//This is set to TRUE when comm is finished sending data

	if (!m_is_connected)
	{
		m_sent = 0;
		m_is_secure_connection = m_port==465;
		m_first_command_after_starttls = FALSE;

		if (m_servername.IsEmpty())
			return OpStatus::ERR_NULL_POINTER;

		m_smtp_backend->Log("SMTP OUT : ", "Connecting...");
		m_smtp_backend->GetProgress().SetCurrentAction(ProgressInfo::CONNECTING);
		return StartLoading(m_servername.CStr(), "smtp", m_port, m_is_secure_connection, m_is_secure_connection);
	}

	return OpStatus::OK;
}


OP_STATUS SMTP::AddDotPrefix(char** allocated_buffer)
{
	if (!allocated_buffer)
		return OpStatus::ERR_NULL_POINTER;

	if (!(*allocated_buffer)) //Empty buffer, nothing to do
		return OpStatus::OK;

	// Detect previous unfinished dot-stuffing
	BOOL  dot_stuff_first_dot =
			(m_previous_buffer_end[0] == '\r' && m_previous_buffer_end[1] == '\n' && (*allocated_buffer)[0] == '.') ||
			(m_previous_buffer_end[1] == '\r' && (*allocated_buffer)[0]   == '\n' && (*allocated_buffer)[1] == '.');

	int   grow_count = dot_stuff_first_dot > 0 ? 1 : 0;
	int   old_length = 0;
	char* temp       = *allocated_buffer;

	while (*temp)
	{
		if ((temp[0] == '\r' && temp[1] == '\n' && temp[2] == '.'))
			grow_count++;
		old_length++;
		temp++;
	}

	if (grow_count > 0)
	{
		char* new_allocated_buffer	= OP_NEWA(char, old_length + grow_count + 1);
		char* temp_dest				= new_allocated_buffer;
		char* temp_src				= *allocated_buffer;

		if (!new_allocated_buffer)
			return OpStatus::ERR_NO_MEMORY;

		while (*temp_src)
		{
			if (temp_src[0] == '\r' && temp_src[1] == '\n' && temp_src[2] == '.')
			{
				op_memcpy(temp_dest, "\r\n..", 4); // NULL-terminated after loop
				temp_dest += 4;
				temp_src  += 3;
			}
			else
			{
				if (dot_stuff_first_dot && temp_src[0] == '.')
				{
					*(temp_dest++) = '.';
					dot_stuff_first_dot = FALSE;
				}
				*(temp_dest++) = *(temp_src++);
			}
		}
		*temp_dest = '\0';

		OP_DELETEA(*allocated_buffer);
		*allocated_buffer = new_allocated_buffer;
	}

	return OpStatus::OK;
}

void SMTP::AskForPassword(const OpStringC8& server_message)
{
    OpString server_message16;
    OpStatus::Ignore(server_message16.Set(server_message));
    m_smtp_backend->OnAuthenticationRequired(server_message16);

	if (m_smtp_backend->GetProgress().GetCount() > 0)
		m_smtp_backend->GetProgress().UpdateCurrentAction(-1);
	if (m_smtp_backend->GetProgress().GetTotalCount() > 0)
		m_smtp_backend->GetProgress().AddToCurrentAction(-1);

    m_what_to_send = SMTP_SEND_QUIT;
}

OP_STATUS SMTP::Parse250Response()
{
	char* serverresponse = op_strstr(m_reply_buf, "\r\n");

	if(serverresponse)
	{
		OpString8 category;
		const char* authresponse = op_stristr(serverresponse, "250-AUTH");
		if(!authresponse)
			authresponse = op_stristr(serverresponse, "250 AUTH");
		if(authresponse)
		{
			m_servercapabilities |= (1 << SMTP_SUPPORT_AUTH_F);
			const char* endofcategory = op_stristr(authresponse+1, "\r\n");
			category.Set(authresponse+8, (endofcategory-(authresponse+8)));
			if(KNotFound != category.FindI("CRAM-MD5"))
			{
				m_servercapabilities |= (1 << SMTP_SUPPORT_AUTH_CRAMMD5_F);
			}
			if(KNotFound != category.FindI("PLAIN"))
			{
				m_servercapabilities |= (1 << SMTP_SUPPORT_AUTH_PLAIN_F);
			}
			if(KNotFound!=category.FindI("LOGIN") && KNotFound==category.FindI("LOGINDISABLED"))
			{
				m_servercapabilities |= (1 << SMTP_SUPPORT_AUTH_LOGIN_F);
			}
		}
		const char* tlsresponse = op_stristr(serverresponse, "250-STARTTLS");
		if(!tlsresponse)
			tlsresponse = op_stristr(serverresponse, "250 STARTTLS");
		if(tlsresponse)
		{
			m_servercapabilities |= (1 << SMTP_SUPPORT_STARTTLS_F);
		}
	}
	else
	{
		return OpStatus::ERR;
	}

	return OpStatus::OK;
}

AccountTypes::AuthenticationType SMTP::GetNextAuthenticationMethod(AccountTypes::AuthenticationType current_method) const
{
    UINT32 supported_authentication = 0;
    if(m_servercapabilities & (1<<SMTP_SUPPORT_AUTH_CRAMMD5_F)) supported_authentication |= 1<<AccountTypes::CRAM_MD5;
    if(m_servercapabilities & (1<<SMTP_SUPPORT_AUTH_LOGIN_F)) supported_authentication |= 1<<AccountTypes::LOGIN;
    if(m_servercapabilities & (1<<SMTP_SUPPORT_AUTH_PLAIN_F)) supported_authentication |= 1<<AccountTypes::PLAIN;

    if (supported_authentication==0) //If server doesn't tell us what it supports, try all we support
        supported_authentication = m_smtp_backend->GetAuthenticationSupported();

    return m_smtp_backend->GetNextAuthenticationMethod(current_method, supported_authentication);
}

#endif //M2_SUPPORT
