/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#ifndef NO_FTP_SUPPORT

#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mem/mem_man.h"

#include "modules/pi/OpSystemInfo.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/handy.h"
#include "modules/formats/uri_escape.h"

#include "modules/url/url_man.h"

#include "modules/olddebug/timing.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/comm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/ftp.h"

#include "modules/formats/argsplit.h"


#ifdef YNP_WORK
#define DEBUG_FTP
#define DEBUG_FTP_HEXDUMP
#endif

#ifdef _RELEASE_DEBUG_VERSION
#define DEBUG_FTP
#define DEBUG_FTP_HEXDUMP
#endif

#ifdef DEBUG_FTP_HEXDUMP

#include "modules/olddebug/tstdump.h"

#endif

BOOL Is_Restricted_Port(ServerName *server, unsigned short port, URLType type);


#define FTP_REQ_BUF_MIN_LEN 512

#define FTP_NO_REPLY        0 // no reply yet
#define FTP_110_restart_marker    110 // restart marker reply
#define FTP_120_wait_for_service  120 // service ready in nnn minutes
#define FTP_125_data_conn_open    125 // data connection already open; transfer starting
#define FTP_150_file_status_ok    150 // file status okay; about to open data connection
#define FTP_200_command_ok      200 // command okay
#define FTP_202_cmd_not_implemented 202 // command not implemented, superflous at this site
#define FTP_213_file_size 213 // return size of file
#define FTP_220_service_ready   220 // service ready ready for new user
#define FTP_221_service_closing   221 // service closing control connection
#define FTP_226_closing_data_conn 226 // closing data connection. requested file action successful
#define FTP_227_passive_mode    227 // entering passive mode
#define FTP_229_passive_mode	229 // entering extended passive mode
#define FTP_230_user_logged_in    230 // user logged in, proceed
#define FTP_250_file_action_ok    250 // requested file action okay, completed
#define FTP_257_directory_create  257 // Directory created, also used for home directory CWD OK and PWD
#define FTP_331_user_ok_need_pass 331 // user name okay, need password
#define FTP_332_need_account    332 // need account for login
#define FTP_350_Requested_file_action_pending_information 350 // waiting form more information
#define FTP_421_unavailable     421 // service not available, closing control connection
#define FTP_425_cant_open_data_conn 425 // cant open data connection
#define FTP_426_connection_closed 426 // connection closed; transfer aborted
#define FTP_450_file_unavailable  450 // requested file action not taken: file unavailable
#define FTP_451_req_action_aborted  451 // requested action aborted: local error in processing
#define FTP_452_out_of_storage  452 // requested action aborted: Out of space
#define FTP_500_syntax_error    500 // syntax error, command unrecognized
#define FTP_501_parm_syntax_error 501 // syntax error in parameters or arguments
#define FTP_502_cmd_not_implemented 502 // command not implemented
#define FTP_503_bad_cmd_sequence  503 // bad sequence of commands
#define FTP_504_cmd_not_implemented 504 // command not implemented for that parameter
#define FTP_530_not_logged_in   530 // not logged in
#define FTP_531_permission_denied 531 // not logged in
#define FTP_532_not_logged_in   532 // not logged in
#define FTP_550_file_unavailable  550 // requested action not taken: file unavailable
#define FTP_552_exceeded_storage_allocation  552 // requested action not taken: exceeded_storage_allocation
#define FTP_553_file_unavailable  553 // requested action not taken: file unavailable

#define FTP_SEND_NOTHING  0
#define FTP_SEND_USER   1
#define FTP_SEND_PASS   2
#define FTP_SEND_CWD    3
#define FTP_SEND_RETR   4
#define FTP_SEND_TYPE   5
#define FTP_SEND_PASV   6
#define FTP_SEND_LIST   7
#define FTP_SEND_QUIT   9
#define FTP_SEND_STOR   10
#define FTP_SEND_CWD_ROOT    11
#define FTP_SEND_SIZE 12
#define FTP_SEND_SIZE1 13
#define FTP_SEND_MODE	14
#define FTP_SEND_RESTART	15
#define FTP_SEND_CWDUP	16
#define FTP_SEND_CWD_FULL	17
#define FTP_SEND_RETR_FULL  18
#define FTP_SEND_SIZE_FULL  19
#define FTP_SEND_MDTM_FULL  20
#define FTP_SEND_MDTM		21
#define FTP_SEND_CWD_HOME	23
#define FTP_SEND_RESTART_TEST 24
#define FTP_SEND_TYPE_INIT   25
#define FTP_SEND_EPSV		26
#define FTP_SEND_EPSV_ALL   27
#define FTP_SEND_PWD	28

#define FTP_PATH_STATUS_NULL  0
#define FTP_PATH_STATUS_DIR   1
#define FTP_PATH_STATUS_FILE  2
#define FTP_PATH_STATUS_END   3

#define FTP_DATA_CONN_NULL    0
#define FTP_DATA_CONN_OPEN    1
#define FTP_DATA_CONN_CLOSED  2
#define FTP_DATA_CONN_FAILED  3
#define FTP_DATA_CONN_CONNECTING	4

static const char FTP_AnonymousUser[] = "anonymous";

#define FTP_USER_cmd "USER"
#define FTP_PASS_cmd "PASS"
#define FTP_CWD_cmd  "CWD"
#define FTP_PWD_cmd  "PWD"
#define FTP_CDUP_cmd  "CDUP"
#define FTP_TYPE_cmd "TYPE"
#define FTP_RETR_cmd "RETR"
#define FTP_LIST_cmd "LIST"
#define FTP_PASV_cmd "PASV"
#define FTP_QUIT_cmd "QUIT"
#define FTP_SIZE_cmd "SIZE"
#define FTP_MODE_cmd "MODE"
#define FTP_STOR_cmd "STOR"
#define FTP_REST_cmd "REST"
#define FTP_MDTM_cmd "MDTM"
#define FTP_EPSV_cmd "EPSV"

#define FTP_CWD_cmd_len 3
#define FTP_SIZE_cmd_len 4

static const OpMessage ftp_messages[] =
{
    MSG_COMM_DATA_READY,
    MSG_COMM_CONNECTED,
    MSG_COMM_LOADING_FINISHED,
    MSG_COMM_LOADING_FAILED
};

FTP::FTP(MessageHandler* msg_handler)
  : ProtocolComm(msg_handler, NULL, NULL),
	has_start_path(false),
	pathname(NULL), next_dir(NULL)
{
	InternalInit();
}

void FTP::InternalInit()
{
	pathname = NULL;
	next_dir = NULL;
	ftp_request = NULL;
	manager = NULL;

	request = NULL;   // 02/04/97 YNP
	request_buf_len = 0;
	what_to_send = FTP_SEND_NOTHING;
	error = FTP_NO_REPLY;
	pathname = 0;
	next_dir = 0;
	path_status = FTP_PATH_STATUS_NULL;

	data_conn = 0;
	data_conn_status = FTP_DATA_CONN_NULL;

	header_loaded_sent = FALSE;
	current_typecode = FTP_TYPECODE_NULL;

	reply_buf = 0;
	reply_buf_len = 0;
	reply_buf_loaded = 0;

	// 05/10/98 YNP
	disable_size = FALSE;
	disable_mode = FALSE;
	tried_full_path = FALSE;
	disable_mdtm = FALSE;
	disable_full_path = FALSE;
	disable_cwd_when_size = FALSE;
	datamode = FTP_MODE_STREAM;

	need_transfer_finished = FALSE;
	received_transfer_finished = FALSE;
	connection_active = FALSE;
	accept_requests = TRUE;

	ftp_path = NULL;

	last_active = 0;
	OpStatus::Ignore(mh->SetCallBack(this, MSG_HTTP_CHECK_IDLE_TIMEOUT,Id())); // not really critical
	mh->PostDelayedMessage(MSG_HTTP_CHECK_IDLE_TIMEOUT,Id(),0,60000UL);


#ifdef DEBUG_FTP
	/*
	{
    FILE *ffp = fopen("c:\\klient\\ftp.txt","a");
    //fprintf(ffp,"\n[%d] FTP::FTP() - server %s, path %s\n", Id(), hostname, path);
    fclose(ffp);
	}
	*/
#endif
}

FTP::~FTP()
{
	InternalDestruct();
}

void FTP::InternalDestruct()
{
	if (mh)
		mh->RemoveDelayedMessage(MSG_HTTP_CHECK_IDLE_TIMEOUT, Id(), 0);

	if(ftp_request)
	{
		ChangeParent(NULL);
		ftp_request->ftp_conn = NULL;
		ftp_request->used = TRUE;
		ftp_request->mh->PostMessage(MSG_COMM_LOADING_FINISHED, ftp_request->Id(), 0);
		ftp_request = NULL;
	}

	OP_DELETEA(ftp_path);
	DeleteDataConn(FALSE);
	OP_DELETEA(reply_buf);


	/* 02/04/97  YNP */
	if(request != NULL)
		OP_DELETEA(request);
	/*****************/
}

void FTP::SetManager(FTP_Server_Manager *mgr)
{
	manager = mgr;
	if(manager)
	{
		disable_size = manager->GetDisableSize();
		disable_mode= manager->GetDisableSize();
		disable_full_path= manager->GetDisableSize();
		disable_mdtm = manager->GetDisableSize();
		disable_cwd_when_size = manager->GetDisableCwdWhenSize();
	}
}


const char* FTP::GetUser(BOOL not_default)
{
  if (!ftp_request || (not_default && ftp_request->username.CStr() == FTP_AnonymousUser))
    return 0;
  else
    return ftp_request->username.CStr();
}

void FTP::SetUser(const char* val)
{
	if(ftp_request)
		RAISE_IF_ERROR(ftp_request->username.Set(val));
}

void FTP::SetPassword(const char* val)
{
	if(ftp_request)
		RAISE_IF_ERROR(ftp_request->password.Set(val));
}

void FTP::Stop()
{
	if (data_conn)
	{
		data_conn->Stop();
		SafeDestruction(data_conn);
		data_conn = NULL;
	}
	ChangeParent(NULL);
	if(ftp_request)
		ftp_request->ftp_conn = NULL;
	ftp_request = NULL;
	ProtocolComm::Stop();
}

void FTP::CheckRequestBufL(int min_len)
{
  if (!request || request_buf_len < min_len)
  {
    OP_DELETEA(request);
    request_buf_len = (min_len < FTP_REQ_BUF_MIN_LEN) ? FTP_REQ_BUF_MIN_LEN : min_len;
    request = OP_NEWA_L(char, request_buf_len);
  }
}

/* Unref YNP
BOOL FTP::MoreDirs()
{
  return ftp_path && path_status != FTP_PATH_STATUS_END;
}
*/

const char* FTP::GetNextDir(int& len)
{
  if (pathname && *pathname != '\0')
    len = next_dir - pathname;
  else
    len = 0;

  if (len && pathname[len-1] == '/')
    len--;

  return pathname;
}

void FTP::ResetPathParser()
{
  pathname = 0;
  next_dir = 0;
  path_status = FTP_PATH_STATUS_NULL;
}

BOOL FTP::ParsePath()
{
  if (!next_dir)
  {
    next_dir = ftp_path;
    path_status = FTP_PATH_STATUS_DIR;
  }

  if (next_dir && *next_dir != '\0')
  {
    pathname = next_dir;
    next_dir = op_strchr(pathname, '/');
    if (next_dir)
      next_dir++;
    else
    {
      next_dir = pathname + op_strlen(pathname);
      path_status = FTP_PATH_STATUS_FILE;
	  if(ftp_request->typecode == FTP_TYPECODE_A ||
		  ftp_request->typecode == FTP_TYPECODE_I)
		  return FALSE;
    }
  }
  else
  {
    if (next_dir) // *next_dir == '\0' !!!
      pathname = next_dir;
    path_status = FTP_PATH_STATUS_END;
  }

  return path_status != FTP_PATH_STATUS_END;
}

void FTP::RequestMoreData()
{
	/*
	int len;
	char *request = ComposeRequest(len);

	if(request && len)
		ProtocolComm::SendData(request, len);
		*/
}

char* FTP::ComposeRequestL(int& len)
{
	SetDoNotReconnect(TRUE);

	len = 0;
	switch (what_to_send)
	{
    case FTP_SEND_USER:
		{
			BuildCommandL(len, FTP_USER_cmd, (ftp_request->username.HasContent() ? ftp_request->username.CStr() : FTP_AnonymousUser), TRUE);
			break;
		}
    case FTP_SEND_PASS:
		{
			BuildCommandL(len, FTP_PASS_cmd, (ftp_request->password.HasContent() || ftp_request->password.CStr() != NULL ? ftp_request->password.CStr() : "opera@"), TRUE);
			break;
		}
    case FTP_SEND_CWD:
		{
			int dir_len;
			const char* dir = GetNextDir(dir_len);

			if(dir_len == 0)
			{
				dir = "/";
				dir_len = 1;
			}

			len = FTP_CWD_cmd_len + (dir && dir_len ? dir_len +3 : 2);
			CheckRequestBufL(len+1);
			op_snprintf(request, len+1, (dir && dir_len? "%s %.*s\r\n" : "%s\r\n"), FTP_CWD_cmd, dir_len, dir);
			len = UriUnescape::ReplaceChars(request, UriUnescape::NonCtrlAndEsc);

			//OpStringF8 temp_cwd("%s%s%.*s", (cwd.CStr() ? cwd.CStr() : ""), (!cwd.CStr() || !*cwd.CStr() || (cwd.CStr())[1]  ? "/" : ""),  dir_len, dir);
			//ANCHOR(OpStringF8,temp_cwd);
			//new_cwd.SetL(temp_cwd);

            new_cwd.SetL(cwd);
            if (cwd.Length())
                new_cwd.AppendL("/");
            new_cwd.AppendL(dir,dir_len);
			break;
		}
    case FTP_SEND_CWD_ROOT:
		{
			int dir_len;
			GetNextDir(dir_len); // Advance parser, result always fixed
			BuildCommandL(len, FTP_CWD_cmd, "/" , FALSE);

			new_cwd.SetL("/");
			break;
		}
	case FTP_SEND_PWD:
		{
			BuildCommandL(len, FTP_PWD_cmd, "", FALSE);
			break;
		}
	case FTP_SEND_CWD_HOME:
		{
			const char *path = NULL;

			if (has_start_path) {
				path = m_start_path.CStr();
			} else {
				OP_ASSERT(0); // We shouldn't really be here - it means that start path is too long or FTP server answers PWD in an unknown format. Please notify jhoff
				if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) ||
					ftp_request->password.CStr() == NULL ||
					ftp_request->username.Compare(FTP_AnonymousUser) == 0)
				{
					path = "/";
				}
				else if(manager->GetUseTildeWhenCwdHome())
					path = "~";
			}

			BuildCommandL(len, FTP_CWD_cmd, path , FALSE);

			new_cwd.Empty();
			break;
		}
	case FTP_SEND_CWDUP:
		{
			OpString8 tempcwd;
			ANCHOR(OpString8,tempcwd);
			new_cwd.SetL(cwd);
			{
				int i = new_cwd.FindLastOf('/');
				if(i == KNotFound)
					i = 0;
				new_cwd.Delete(i);
			}
			LEAVE_IF_ERROR(tempcwd.SetConcat("/", new_cwd));

			//BuildCommandL(len, FTP_CWD_cmd, tempcwd , FALSE);
			BuildCommandL(len, FTP_CDUP_cmd, NULL , FALSE);

			break;
		}
		// ********************
    case FTP_SEND_RETR:
		{
			BuildCommandL(len, FTP_RETR_cmd, pathname, TRUE);
			SetProgressInformation(START_REQUEST,0, ftp_request->master->HostName()->UniName());
			break;
		}
    case FTP_SEND_LIST:
		{
			BuildCommandL(len, FTP_LIST_cmd, NULL, FALSE);
			SetProgressInformation(START_REQUEST,0, ftp_request->master->HostName()->UniName());
			break;
		}
    case FTP_SEND_TYPE:
		{
			current_typecode = (ftp_request->typecode != FTP_TYPECODE_NULL ? ftp_request->typecode : FTP_TYPECODE_I);
			BuildCommandL(len, FTP_TYPE_cmd, (ftp_request->typecode == FTP_TYPECODE_A ? "A" : "I"), FALSE);
			break;
		}
    case FTP_SEND_MODE:
		{
			BuildCommandL(len, FTP_MODE_cmd, (datamode == FTP_MODE_BLOCK ? "B" : "S"), FALSE);
			break;
		}

	case FTP_SEND_PASV:
		{
			BuildCommandL(len, FTP_PASV_cmd, NULL, FALSE);
			break;
		}
	case FTP_SEND_SIZE1:
	case FTP_SEND_SIZE:
		{
			int dir_len=0;
			const char* dir = (what_to_send == FTP_SEND_SIZE ? GetNextDir(dir_len) : pathname);

			if(what_to_send == FTP_SEND_SIZE1)
			{
				dir_len = (pathname ? op_strlen(pathname) : 0);
			}
			len = FTP_SIZE_cmd_len + (dir && dir_len ? dir_len +3 : 2);
			CheckRequestBufL(len+1);
			op_snprintf(request, len+1, (dir && dir_len? "%s %.*s\r\n" : "%s\r\n"), FTP_SIZE_cmd, dir_len, dir);
			len = UriUnescape::ReplaceChars(request, UriUnescape::NonCtrlAndEsc);
			break;
		}
#ifdef FTP_RESUME
    case FTP_SEND_RESTART:
		{
			// 24/09/98 YNP
			OpString8 temp_start;
			ANCHOR(OpString8, temp_start);
			LEAVE_IF_ERROR(g_op_system_info->OpFileLengthToString(ftp_request->resume_pos, &temp_start));
			BuildCommandL(len, FTP_REST_cmd, temp_start.CStr(), FALSE);
			break;
		}
    case FTP_SEND_RESTART_TEST:
		{
			BuildCommandL(len, FTP_REST_cmd, "0", FALSE);
			break;
		}
#endif
	case FTP_SEND_TYPE_INIT:
		{
			current_typecode = FTP_TYPECODE_I;
			BuildCommandL(len, FTP_TYPE_cmd, "I", FALSE);
			break;
		}

	case FTP_SEND_MDTM:
		{
			BuildCommandL(len, FTP_MDTM_cmd, pathname, TRUE, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath));
			break;
		}
	case FTP_SEND_CWD_FULL:
		{
            new_cwd.SetL(ftp_request->path);
			BuildCommandL(len, FTP_CWD_cmd, ftp_request->path.CStr(), TRUE, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) );
			break;
		}
	case FTP_SEND_RETR_FULL:
		{
			BuildCommandL(len, FTP_RETR_cmd, ftp_request->path.CStr(), TRUE, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) );
			break;
		}
	case FTP_SEND_SIZE_FULL:
		{
			BuildCommandL(len, FTP_SIZE_cmd, ftp_request->path.CStr(), TRUE, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) );
			break;
		}
	case FTP_SEND_MDTM_FULL:
		{
			BuildCommandL(len, FTP_MDTM_cmd, ftp_request->path.CStr(),TRUE, g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) );
			break;
		}
	case FTP_SEND_EPSV:
		{
			BuildCommandL(len, FTP_EPSV_cmd, NULL, FALSE  );
			break;
		}
	case FTP_SEND_EPSV_ALL:
		{
			BuildCommandL(len, FTP_EPSV_cmd, "ALL", FALSE  );
			break;
		}
    case FTP_SEND_QUIT:
		{
			// 24/09/98 YNP
			BuildCommandL(len, FTP_QUIT_cmd, NULL, FALSE);
			break;
		}

    default:
		len = 0;
		break;
	}

#ifdef DEBUG_FTP
	PrintfTofile("ftp.txt", "[%i] Request: (%d) ", Id(), len);
	if (len)
		PrintfTofile("ftp.txt", "%s", request);
	PrintfTofile("ftp.txt", "\n");
#endif

	if(len)
	{
		last_active = g_timecache->CurrentTime();
	}

	char *temp = request;
	request = NULL;
	return temp;
}

void FTP::Clear()
{
	reply_buf_loaded = 0;
	header_loaded_sent = FALSE;
	what_to_send = FTP_SEND_NOTHING;
	error = 0;
}

void FTP::MoveToNextReply()
{
	if(reply_buf == NULL || reply_buf_next == NULL || reply_buf_loaded == 0)
		return;

	int reply_rest_len = reply_buf_loaded - (reply_buf_next -reply_buf);

	if(reply_rest_len == 0)
	{
		reply_buf_loaded = 0;
		return;
	}

	op_memmove(reply_buf, reply_buf_next, reply_rest_len);
	reply_buf_loaded = reply_rest_len;
	reply_buf[reply_buf_loaded] = '\0';
}

int FTP::CheckReply()
{
	if (reply_buf_loaded > 3)
	{
		int response = 0;
		int multi_response = 0;
		char cont;
		char *line = reply_buf;
		while (line)
		{
			char* endline = op_strchr(line, '\n');
			if (endline)
			{
				if (!response)
				{
					response = op_atoi(line);
					if (!response || !op_isdigit((unsigned char) *line))
					{
						reply_buf_next = endline+1;
						return FTP_NO_REPLY;
					}
				}
				else
				{
					if (op_isdigit((unsigned char) *line))
					{
						op_sscanf(line, "%d%c", &multi_response, &cont);
						if (multi_response == response && cont == ' ')
						{
							reply_buf_next = endline+1;
							return response;
						}
					}
					line = endline+1;
				}
			}
			else
				line = 0;
		}
	}
	return FTP_NO_REPLY;
}

BOOL FTP::GetPasvHostAndPort(int& h1, int& h2, int &h3, int& h4, int& p)
{
	char *tmp = reply_buf;
	if (tmp && op_strchr(tmp, '\n'))
	{
		//tmp = op_strrchr(tmp, '(');
		char* scan_tmp = tmp + op_strlen(tmp) - 1;
		while (scan_tmp != tmp && !op_isdigit((unsigned char) *scan_tmp)) // 01/04/98 YNP
			scan_tmp--;
		while (scan_tmp != tmp && (op_isdigit((unsigned char) *scan_tmp) || *scan_tmp == ',')) // 01/04/98 YNP
			scan_tmp--;
		if (scan_tmp != tmp)
		{
			int p1, p2;
			//if (*scan_tmp && !op_isdigit(*scan_tmp))
			if (*scan_tmp && !op_isdigit((unsigned char) *scan_tmp)) // 01/04/98 YNP
				scan_tmp++;
			if (op_sscanf(scan_tmp, "%d,%d,%d,%d,%d,%d",
				&h1, &h2, &h3, &h4, &p1, &p2) < 6)
				return FALSE;

			p = (p1*256) + p2;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL FTP::PendingClose() const
{
	return FALSE;
}

/*
int FTP::PendingError()
{
	return error;
}
*/

void FTP::SecondaryProcessReceivedData()
{
	last_active = g_timecache->CurrentTime();

	if(!header_loaded_sent)
	{
        header_loaded_sent = TRUE;
		SetProgressInformation(HEADER_LOADED, what_to_send == FTP_SEND_LIST);
        //mh->PostMessage(MSG_FTP_HEADER_LOADED, ftp_request->Id(), what_to_send == FTP_SEND_LIST);
	}
	ProtocolComm::ProcessReceivedData();
}

void FTP::ProcessReceivedData()
{
	int cnt_len;

	last_active = g_timecache->CurrentTime();

	if (reply_buf_len-reply_buf_loaded < 513)
	{
		reply_buf_len += 1024;
		char *tmp = OP_NEWA(char, reply_buf_len);
		if(!tmp)
			goto memory_failure;

		if (reply_buf)
		{
			op_memcpy(tmp, reply_buf, (int)reply_buf_loaded);
			OP_DELETEA(reply_buf);
		}
		reply_buf = tmp;
		reply_buf_next = reply_buf;
	}

	cnt_len = ProtocolComm::ReadData(reply_buf+reply_buf_loaded, reply_buf_len-reply_buf_loaded-1);
	reply_buf[reply_buf_loaded+cnt_len] = '\0';
	if (cnt_len + reply_buf_loaded == 0)
		return;


#ifdef DEBUG_FTP
	PrintfTofile("ftp.txt","[%d] HandleDataReady() - read %d bytes: %s\n", Id(), cnt_len, reply_buf+reply_buf_loaded);
#endif

	reply_buf_loaded += cnt_len;

	int reply;


	while ((reply = CheckReply())!= FTP_NO_REPLY)
	{
#ifdef DEBUG_FTP
	PrintfTofile("ftp.txt", "[%d] HandleDataReady(): CheckReply returned %d\n", Id(), reply);
#endif
		int what_to_send_next = FTP_SEND_NOTHING;

		if (reply == FTP_221_service_closing)
		{
			if (ftp_request)
				mh->PostMessage(MSG_COMM_LOADING_FINISHED, ftp_request->Id(), 0);

				Stop();
				mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
				break;
		}

		switch (what_to_send)
		{
		case FTP_SEND_NOTHING:
			{
				if (reply == FTP_220_service_ready)
				{
					connection_active = TRUE;

					char *match = op_strstr(reply_buf, "WarFTPd");

					if(match && match < reply_buf_next)
					{
						disable_cwd_when_size = TRUE;
						manager->DisableCwdWhenSize();
					}

					// Add code to disable the SIZE sommand for Bulletproof servers  (bug #153796)?

					what_to_send_next = FTP_SEND_USER;
				}
				else if (reply == FTP_226_closing_data_conn || reply == FTP_250_file_action_ok)
				{
					received_transfer_finished = TRUE;
					if(data_conn && datamode == FTP_MODE_STREAM)
					{
						what_to_send_next = FTP_SEND_NOTHING;
						// wait for data_conn to close;
					}
					else
					{
						StartNextRequest();
						return;
					}
				}
				else if (reply != FTP_120_wait_for_service)
				{
					what_to_send_next = FTP_SEND_QUIT;
					if (reply == FTP_421_unavailable)
						error = URL_ERRSTR(SI, ERR_FTP_SERVICE_UNAVAILABLE);
					else
					{
						MakeDirMsg();
						what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
					}

				}
			}

			break;

		case FTP_SEND_USER:
			if (reply == FTP_230_user_logged_in)
			{
				what_to_send_next = (manager->GetDisableEPSV() ? StartDirActions() : FTP_SEND_EPSV_ALL);
			}
			else if (reply == FTP_331_user_ok_need_pass && ftp_request &&
				(!(ftp_request->password.IsEmpty() && ftp_request->password.CStr() == NULL) ||
				ftp_request->username.IsEmpty() ||
				ftp_request->username.Compare(FTP_AnonymousUser) == 0))
				what_to_send_next = FTP_SEND_PASS;
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;

		case FTP_SEND_PASS:
			if (reply == FTP_230_user_logged_in || reply == FTP_202_cmd_not_implemented)
			{
				what_to_send_next = FTP_SEND_PWD;
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;

		case FTP_SEND_CWDUP:
			if (reply == FTP_250_file_action_ok || reply == FTP_200_command_ok || reply == FTP_257_directory_create)
			{
				if(OpStatus::IsError(cwd.Set(new_cwd)))
					goto memory_failure;

				MakeDirMsg();
				what_to_send_next = DecideToSendTypeOrMode(FTP_SEND_RETR);
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;

		case FTP_SEND_PWD:
			{
				if (reply == FTP_257_directory_create)
				{
					ParameterList response_line;

					if(OpStatus::IsSuccess(response_line.SetValue(reply_buf, PARAM_NO_ASSIGN | PARAM_ONLY_SEP | PARAM_SEP_WHITESPACE | PARAM_STRIP_ARG_QUOTES |  PARAM_BORROW_CONTENT)))
					{
						Parameters *item = response_line.First();

						if(item)
							item = item->Suc();

						// Assign <quoted-string> (w/o quotes) to m_start_path
						if(item && item->Name() && OpStatus::IsSuccess(m_start_path.Set(item->GetName())))
							has_start_path = TRUE;
					}
				}

				what_to_send_next = (manager->GetDisableEPSV() ? StartDirActions() : FTP_SEND_EPSV_ALL) ;
				break;
			}
		case FTP_SEND_CWD:
		case FTP_SEND_CWD_ROOT:
			if (reply == FTP_250_file_action_ok || reply == FTP_200_command_ok || reply == FTP_257_directory_create)
			{
				if(OpStatus::IsError(cwd.Set(new_cwd)))
					goto memory_failure;

				new_cwd.Empty();

				if (path_status == FTP_PATH_STATUS_FILE)
				{
					// path not ending with "/" was a directory
					MakeDirMsg();
					path_status = FTP_PATH_STATUS_DIR;
					what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
					//what_to_send_next = (ftp_request->typecode == FTP_TYPECODE_I ? FTP_SEND_TYPE : FTP_SEND_PASV);
				}
				else if (ParsePath())
				{
					what_to_send_next = (disable_size ? FTP_SEND_CWD : FTP_SEND_SIZE); // 05/10/98 YNP
				}
				else
				{
					MakeDirMsg();
					what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
					//what_to_send_next = (ftp_request->typecode == FTP_TYPECODE_I ? FTP_SEND_TYPE : FTP_SEND_PASV);
				}
			}
			// 14/10/97 YNP        Some servers return a 501 instead of 550
			// 16/02/98 YNP And yet another sends a 553 instead
			else if ((reply == FTP_550_file_unavailable || reply == FTP_553_file_unavailable || reply == FTP_501_parm_syntax_error || reply == FTP_530_not_logged_in)
				&& (what_to_send == FTP_SEND_CWD_ROOT || path_status == FTP_PATH_STATUS_FILE))
			{
				if(what_to_send != FTP_SEND_CWD_ROOT)
				{
					// path not ending with "/" was **NOT** a directory
					what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
				}
				else
					what_to_send_next = (disable_size ? FTP_SEND_CWD : FTP_SEND_SIZE); // 05/10/98 YNP
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;

			// ********************

		case FTP_SEND_PASV:

			if (reply == FTP_227_passive_mode)
			{
				int h1, h2, h3, h4, data_port;
				if (ftp_request && GetPasvHostAndPort(h1, h2, h3, h4, data_port) && h1 < 256 && h2 < 256 && h3 < 256 && h4 < 256)
				{
					char *data_host = (char *) g_memory_manager->GetTempBuf();
					op_snprintf(data_host, g_memory_manager->GetTempBufLen(), "%d.%d.%d.%d", h1, h2, h3, h4);
					data_host[g_memory_manager->GetTempBufLen()-1] = 0;

					OpString ipaddress;
					BOOL is_ok = TRUE;
					OpSocketAddress *sockaddr = manager->HostName()->SocketAddress();
					if(sockaddr && OpStatus::IsSuccess(sockaddr->ToString(&ipaddress)))
					{
						if(ipaddress.Compare(data_host) != 0)
						{
							is_ok = FALSE;
						}
					}

					if(is_ok && (data_port ==  21 || Is_Restricted_Port(manager->HostName(), data_port, URL_FTP)))
					{
						is_ok = FALSE;
					}

					if(!is_ok)
					{
						OpString reply_buf1;
						reply_buf1.Set(reply_buf);

						SetProgressInformation(SET_INTERNAL_ERROR_MESSAGE,0, reply_buf1.CStr());

						what_to_send_next = FTP_SEND_NOTHING;
						error = URL_ERRSTR(SI, ERR_FTP_CANNOT_OPEN_PASV_DATA_CONN);
						break;
					}
					/*
					ServerName *servername = urlManager->GetServerName(data_host,TRUE);
					if (!servername)
						goto memory_failure;
					*/
					what_to_send_next = ActivatePassiveConnection(manager->HostName(), data_port, error);
#ifdef SOCKET_SUPPORTS_TIMER
					((Comm*)GetSink())->SetSocketTimeOutInterval(0); // Turn off the timer for the command connection
#endif
				}
				else
				{
					what_to_send_next = FTP_SEND_NOTHING;
					error = URL_ERRSTR(SI, ERR_FTP_CANNOT_OPEN_PASV_DATA_CONN);
				}
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;
		case FTP_SEND_EPSV:
		case FTP_SEND_EPSV_ALL:

			if (what_to_send == FTP_SEND_EPSV_ALL && (reply == FTP_200_command_ok || reply == FTP_220_service_ready))
			{
				what_to_send_next =  StartDirActions();
			}
			else if (reply == FTP_229_passive_mode)
			{
				char d1=0,d2=0,d3=0,d4=0, p1;
				unsigned int data_port;
				int args = op_sscanf(reply_buf, "%*d %*[^(](%c%c%c%u%c%c", &d1,&d2,&d3,&data_port, &d4, &p1);

				if(((args>=2 && args< 6) || (args == 6 && p1 != ')')) && d1 && d1==d2)
				{
					args = 0;

					char *tempstring = (char *) g_memory_manager->GetTempBuf2k();

					// Create format string for scanning a string with a known format // NOTE: WATCH the "%%"s and special characters in the "[]" segments
					op_snprintf(tempstring, g_memory_manager->GetTempBuf2Len(), "%%*d %%*[^(](%c%c%%*[^\\%c]%c%%u%c%%c", d1,d1,d1,d1, d1);
					args = op_sscanf(reply_buf, tempstring, &data_port, &p1);
					if(args ==2)
					{
						args = 6; // to get by the test below
						d2=d3=d4=d1; // same delimiter to get by the test below
					}
				}

				if(args < 6 || !d1 || d1 != d2 || d1 != d3 || d1 != d4 ||
					(data_port ==  21 || Is_Restricted_Port(manager->HostName(), data_port, URL_FTP)))
				{
					error = URL_ERRSTR(SI, ERR_FTP_CANNOT_OPEN_PASV_DATA_CONN);
					what_to_send_next = FTP_SEND_QUIT;

					OpString reply_buf1;
					reply_buf1.Set(reply_buf);

					SetProgressInformation(SET_INTERNAL_ERROR_MESSAGE,0, reply_buf1.CStr());
					break;
				}

				what_to_send_next = ActivatePassiveConnection(manager->HostName(), data_port, error);
				if(data_conn)
					mh->PostDelayedMessage(MSG_COMM_LOADING_FAILED, data_conn->Id(), 1, 10*1000);

#ifdef SOCKET_SUPPORTS_TIMER
				((Comm*)GetSink())->SetSocketTimeOutInterval(0); // Turn off the timer for the command connection
#endif

				if(what_to_send == FTP_SEND_EPSV_ALL)
					what_to_send_next = StartDirActions();
			}
			else
			{
				manager->DisableEPSV();
				manager->SetTestedEPSV();
				what_to_send_next = (what_to_send == FTP_SEND_EPSV_ALL?  StartDirActions() : MapStatusToAction(reply, error));
			}


			break;


		case FTP_SEND_TYPE:

			if (reply == FTP_200_command_ok || reply == FTP_504_cmd_not_implemented)
			{
				if(/*prefsManager->FtpDataConnectionBlockMode() && */
					!disable_mode && !data_conn && datamode != FTP_MODE_BLOCK)
				{
					datamode = FTP_MODE_BLOCK;
					what_to_send_next = FTP_SEND_MODE;
				}
				else if(!tried_full_path && ! disable_full_path)
					what_to_send_next = (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV) ;
				else
					what_to_send_next = (!disable_size && path_status == FTP_PATH_STATUS_FILE ? FTP_SEND_SIZE1 : FTP_SEND_PASV);
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;
		case FTP_SEND_TYPE_INIT:

			what_to_send_next = StartDirActions();
			break;

		case FTP_SEND_MODE:
			if (reply == FTP_200_command_ok ||
				reply == FTP_250_file_action_ok)
			{
				if(!tried_full_path && ! disable_full_path)
					what_to_send_next = (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV);
				else
					what_to_send_next = (!disable_size && path_status == FTP_PATH_STATUS_FILE ? FTP_SEND_SIZE1 : (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
			}
			else if(reply == FTP_504_cmd_not_implemented ||
				reply == FTP_202_cmd_not_implemented ||
				reply == FTP_421_unavailable || // Not really proper, but apparently some servers use it instead of 504 :(
				reply == FTP_500_syntax_error ||
				reply == FTP_501_parm_syntax_error ||
				reply == FTP_502_cmd_not_implemented ||
				reply == FTP_503_bad_cmd_sequence ||
				reply == FTP_550_file_unavailable )
			{
				disable_mode = TRUE;
				manager->DisableMode();
				datamode = FTP_MODE_STREAM;
				if(!tried_full_path && ! disable_full_path)
					what_to_send_next = (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV);
				else
					what_to_send_next = (!disable_size && path_status == FTP_PATH_STATUS_FILE ? FTP_SEND_SIZE1 : (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;

			// *** 05/10/98 YNP ***
		case FTP_SEND_SIZE1: // 04/11/98 YNP
		case FTP_SEND_SIZE:

			// Some servers return 250 instead of 213
			if (ftp_request && (reply == FTP_213_file_size || reply == FTP_250_file_action_ok))
			{
				// It's a file, download it!
				char *size_pos = op_strpbrk(reply_buf, " \t\r\n");
				if(!size_pos || OpStatus::IsError(StrToOpFileLength(size_pos, &ftp_request->file_size)))
					ftp_request->file_size = 0;
				what_to_send_next = (what_to_send == FTP_SEND_SIZE1 || disable_cwd_when_size ? (disable_mdtm ? (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV) : FTP_SEND_MDTM) :  FTP_SEND_CWD);
			}
			else if (reply == FTP_500_syntax_error || reply == FTP_501_parm_syntax_error ||
				reply == FTP_502_cmd_not_implemented || reply == FTP_503_bad_cmd_sequence ||
				reply == FTP_504_cmd_not_implemented)
			{
				disable_size = TRUE;
				manager->DisableSize();
				what_to_send_next = FTP_SEND_CWD;
			}
			else if(what_to_send == FTP_SEND_SIZE && (reply == FTP_550_file_unavailable || reply == FTP_553_file_unavailable))
			{
				what_to_send_next = FTP_SEND_CWD; // Maybe a directory, try a CD
			}
			else if(what_to_send == FTP_SEND_SIZE1 && reply == FTP_550_file_unavailable)
			{
				what_to_send_next = (disable_mdtm ? (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV) : FTP_SEND_MDTM);
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;
			// **********************
#ifdef FTP_RESUME
		case FTP_SEND_RESTART:
			if(ftp_request && reply == FTP_350_Requested_file_action_pending_information ||
				reply == FTP_250_file_action_ok ||
				reply == FTP_200_command_ok
				)
			{
				ftp_request->using_resume = TRUE;
				what_to_send_next = (disable_full_path || tried_full_path ? FTP_SEND_RETR : FTP_SEND_RETR_FULL);
			}
			else if(reply == FTP_202_cmd_not_implemented ||
				reply == FTP_502_cmd_not_implemented ||
				reply == FTP_504_cmd_not_implemented)
			{
				what_to_send_next = (disable_full_path || tried_full_path ? FTP_SEND_RETR : FTP_SEND_RETR_FULL);
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;
		case FTP_SEND_RESTART_TEST:
			if(reply == FTP_350_Requested_file_action_pending_information ||
				reply == FTP_250_file_action_ok ||
				reply == FTP_200_command_ok
				)
			{
				manager->SetRestartTested(TRUE);
			}
			else
			{
				manager->SetRestartTested(FALSE);
				if(ftp_request && ftp_request->use_resume)
					ftp_request->use_resume = FALSE;
			}
			what_to_send_next = FTP_SEND_TYPE_INIT;

			break;
#endif

		case FTP_SEND_RETR:

			if (reply == FTP_125_data_conn_open || reply == FTP_150_file_status_ok)
			{
				if(tried_full_path)
				{
					disable_full_path = TRUE;
					manager->DisableFullPath();
				}
				break; // do nothing, wait for data_conn to close;
			}
			else if (reply == FTP_226_closing_data_conn || reply == FTP_250_file_action_ok)
			{
				received_transfer_finished = TRUE;
				if(!header_loaded_sent)
				{
					header_loaded_sent = TRUE;
					SetProgressInformation(HEADER_LOADED, FALSE);
				}
				// wait for data_conn to close;
				if(data_conn && datamode == FTP_MODE_STREAM)
				{
					what_to_send = what_to_send_next = FTP_SEND_NOTHING;
				}
				else
				{
					reply_buf_loaded = 0;
					StartNextRequest();
					return;
				}
				break; // do nothing, wait for data_conn to close;
			}
			else
			{
				what_to_send_next = MapStatusToAction(reply, error);
			}

			break;

		case FTP_SEND_LIST:

			if (ftp_request) {
				if (HasStartPath()) ftp_request->SetStartPath(m_start_path);
			}

			if (reply == FTP_125_data_conn_open || reply == FTP_150_file_status_ok)
			{
				break; // do nothing, wait for data_conn to close;
			}
			else if (reply == FTP_550_file_unavailable && cwd.HasContent() && cwd.Compare("/") != 0)
			{
				// publicfile from Bernstein http://cr.yp.to allow CWD to a file
				// try a retrieve instead
				what_to_send_next = FTP_SEND_CWDUP;
			}
			else if (reply == FTP_226_closing_data_conn || reply == FTP_250_file_action_ok)
			{
 				if(tried_full_path)
 				{
 					disable_full_path = TRUE;
 					manager->DisableFullPath();
 				}
 				
				received_transfer_finished = TRUE;
				if(!header_loaded_sent)
				{
					header_loaded_sent = TRUE;
					SetProgressInformation(HEADER_LOADED, TRUE);
				}
			
				what_to_send_next =  FTP_SEND_QUIT;
				break; // do nothing, wait for data_conn to close;
			}
			else
			{
				if(ftp_request && !tried_full_path && !disable_full_path)
				{
					if(OpStatus::IsError(SetStr(ftp_path, ftp_request->path.CStr())))
						goto memory_failure;

					ResetPathParser();
					tried_full_path = TRUE;
					what_to_send_next = FTP_SEND_CWD_ROOT;
				}
				else
					what_to_send_next = MapStatusToAction(reply, error);
			}

			break;

		case FTP_SEND_CWD_HOME:
			{
				if(reply == FTP_501_parm_syntax_error && !manager->GetUseTildeWhenCwdHome())
				{
					manager->SetUseTildeWhenCwdHome();
					what_to_send_next = FTP_SEND_CWD_HOME;
				}
				else if (reply == FTP_250_file_action_ok || reply == FTP_200_command_ok || reply == FTP_257_directory_create ||
					reply == FTP_550_file_unavailable || reply == FTP_553_file_unavailable ||
					reply == FTP_501_parm_syntax_error || reply == FTP_530_not_logged_in)
				{
					if(OpStatus::IsError(cwd.Set(new_cwd)))
						goto memory_failure;

					new_cwd.Empty();

					MakeDirMsg();
					what_to_send_next = StartDirActions();
				}
				else
				{
					what_to_send_next = MapStatusToAction(reply, error);
				}
				break;
			}
		case FTP_SEND_CWD_FULL:
			{
				if (reply == FTP_250_file_action_ok || reply == FTP_200_command_ok || reply == FTP_257_directory_create)
				{
					if(OpStatus::IsError(cwd.Set(new_cwd)))
						goto memory_failure;

					path_status = FTP_PATH_STATUS_DIR;
					new_cwd.Empty();

					MakeDirMsg();
					what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
				}
				else if (ftp_request&& (reply == FTP_550_file_unavailable || reply == FTP_553_file_unavailable ||
					reply == FTP_501_parm_syntax_error || reply == FTP_530_not_logged_in))
				{
					if(OpStatus::IsError(SetStr(ftp_path, ftp_request->path.CStr())))
						goto memory_failure;

					if(data_conn)
					{
						mh->RemoveCallBacks(this, data_conn->Id());
						data_conn->ChangeParent(NULL);
						DeleteDataConn(FALSE);
						data_conn_status = FTP_DATA_CONN_CLOSED;
					}
					ResetPathParser();
					tried_full_path = TRUE;
					what_to_send_next = FTP_SEND_CWD_HOME;
				}
				else
				{
					what_to_send_next = MapStatusToAction(reply, error);
				}
				break;
			}
		case FTP_SEND_RETR_FULL:
			{
				if (reply == FTP_125_data_conn_open || reply == FTP_150_file_status_ok)
				{
					break; // do nothing, wait for data_conn to close;
				}
				else if (reply == FTP_226_closing_data_conn || reply == FTP_250_file_action_ok)
				{
					received_transfer_finished = TRUE;
					if(!header_loaded_sent)
					{
						header_loaded_sent = TRUE;
						SetProgressInformation(HEADER_LOADED, FALSE);
					}
					// wait for data_conn to close;
					if(data_conn && datamode == FTP_MODE_STREAM)
					{
						what_to_send = what_to_send_next = FTP_SEND_NOTHING;
					}
					else
					{
						what_to_send = what_to_send_next = FTP_SEND_NOTHING;
						reply_buf_loaded = 0;
						StartNextRequest();
						return;
					}
					break; // do nothing, wait for data_conn to close;
				}
				else if(ftp_request)
				{
					if(OpStatus::IsError(SetStr(ftp_path, ftp_request->path.CStr())))
						goto memory_failure;

					if(data_conn)
					{
						mh->RemoveCallBacks(this, data_conn->Id());
						data_conn->ChangeParent(NULL);
						DeleteDataConn(FALSE);
						data_conn_status = FTP_DATA_CONN_CLOSED;
					}
					ResetPathParser();
					tried_full_path = TRUE;
					what_to_send_next = FTP_SEND_CWD_HOME;
				}
				else
				{
					what_to_send_next = MapStatusToAction(reply, error);
				}
				break;
			}
		case FTP_SEND_SIZE_FULL:
			{
				// Some servers return 250 instead of 213
				if (reply == FTP_213_file_size || reply == FTP_250_file_action_ok)
				{
					// It's a file, download it!
					char *size_pos = op_strpbrk(reply_buf, " \t\r\n");
					if(!size_pos || OpStatus::IsError(StrToOpFileLength(size_pos, &ftp_request->file_size)))
						ftp_request->file_size = 0;
					path_status = FTP_PATH_STATUS_FILE;
					what_to_send_next = (disable_mdtm ? DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV)) : FTP_SEND_MDTM_FULL);
				}
				else if (ftp_request && (reply == FTP_500_syntax_error || reply == FTP_501_parm_syntax_error ||
					reply == FTP_502_cmd_not_implemented || reply == FTP_503_bad_cmd_sequence ||
					reply == FTP_504_cmd_not_implemented))
				{
					disable_size = TRUE;
					manager->DisableSize();
					disable_full_path = TRUE;
					manager->DisableFullPath();
					if(OpStatus::IsError(SetStr(ftp_path, ftp_request->path.CStr())))
						goto memory_failure;

					ResetPathParser();
					tried_full_path = TRUE;
					what_to_send_next = FTP_SEND_CWD_HOME;
				}
				else if(reply == FTP_550_file_unavailable || reply == FTP_553_file_unavailable)
				{
					what_to_send_next = FTP_SEND_CWD_FULL;
				}
				else
				{
					what_to_send_next = MapStatusToAction(reply, error);
				}
				break;
			}
		case FTP_SEND_MDTM:
		case FTP_SEND_MDTM_FULL:
			{
				// Some servers return 250 instead of 213
				if (reply == FTP_213_file_size || reply == FTP_250_file_action_ok ||
					reply == FTP_550_file_unavailable || reply == FTP_553_file_unavailable)
				{
					// It's really a file, download it!
#ifdef FTP_RESUME
					if (reply == FTP_213_file_size || reply == FTP_250_file_action_ok)
					{
						int dummy;
						char *buf = (char *) g_memory_manager->GetTempBuf();
						if(op_sscanf(reply_buf, "%d %64s",&dummy, buf) == 2)
						{
							if(ftp_request->use_resume && ftp_request->mdtm_date.HasContent())
							{
								if(ftp_request->mdtm_date.Compare(buf) == 0)
								{
									buf = NULL; // No need to update;
								}
								else
									ftp_request->use_resume = FALSE; // assume date has changed
							}
							if(buf)
								OpStatus::Ignore(ftp_request->mdtm_date.Set(buf)); // it is not critical that this succeeds
						}
						else if(ftp_request->use_resume && ftp_request->mdtm_date.HasContent())
							ftp_request->use_resume = FALSE; // there was an error when reading the MDTM date, assume it is non-resumable)
					}
#endif
					what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));;
				}
				else if (reply == FTP_500_syntax_error || reply == FTP_501_parm_syntax_error ||
					reply == FTP_502_cmd_not_implemented || reply == FTP_503_bad_cmd_sequence ||
					reply == FTP_504_cmd_not_implemented)
				{
					disable_mdtm = TRUE;
					manager->DisableMDTM();
					what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));;
				}
				else
				{
					what_to_send_next = MapStatusToAction(reply, error);
				}
				break;
			}
		case FTP_SEND_QUIT:
			if (reply == FTP_221_service_closing)
			{
				if (ftp_request)
					mh->PostMessage(MSG_COMM_LOADING_FINISHED, ftp_request->Id(), 0);

			}
			// do nothing;
			reply_buf_loaded = 0;
			break;

		default:
			error = URL_ERRSTR(SI, ERR_FTP_INTERNAL_ERROR);
			what_to_send_next = FTP_SEND_QUIT;
			reply_buf_loaded = 0;
		}

		if(what_to_send_next != FTP_SEND_NOTHING)
		{
			if((what_to_send_next == FTP_SEND_PASV || what_to_send_next == FTP_SEND_EPSV) && data_conn && !data_conn->Closed())
			{
				what_to_send_next = (ftp_request->typecode == FTP_TYPECODE_D ||
					path_status != FTP_PATH_STATUS_FILE ?
FTP_SEND_LIST :
#ifdef FTP_RESUME
				(ftp_request->use_resume ? FTP_SEND_RESTART : (disable_full_path || tried_full_path ? FTP_SEND_RETR : FTP_SEND_RETR_FULL))
#else
					(disable_full_path || tried_full_path ? FTP_SEND_RETR : FTP_SEND_RETR_FULL)
#endif
					);
			}

			if(error)
			{

				mh->PostMessage(MSG_COMM_LOADING_FAILED, (ftp_request ? ftp_request->Id() : Id()), error);
				error = 0;
				if(ftp_request)
				{
					ChangeParent(NULL);
					ftp_request->ftp_conn = NULL;
					ftp_request->used = TRUE;
					ftp_request = NULL;

					if(what_to_send_next != FTP_SEND_QUIT && !Closed())
					{
						if(!need_transfer_finished || received_transfer_finished)
							StartNextRequest();
					}
				}
			}

			what_to_send = what_to_send_next;
			SendCommand();
		}
		else if(error)
		{

			mh->PostMessage(MSG_COMM_LOADING_FAILED, (ftp_request ? ftp_request->Id() : Id()), error);
			error = 0;
			if(ftp_request)
			{
				ChangeParent(NULL);
				ftp_request->ftp_conn = NULL;
				ftp_request->used = TRUE;
				ftp_request = NULL;

				if(what_to_send == FTP_SEND_NOTHING && !Closed())
				{
					if(!need_transfer_finished || received_transfer_finished)
						StartNextRequest();
				}
				else
				{
					what_to_send = FTP_SEND_QUIT;
					SendCommand();
				}
			}
		}

		MoveToNextReply();
	} // Check for next reply code

	if(ProtocolComm::Closed())
	{
		if(ftp_request)
			mh->PostMessage(MSG_COMM_LOADING_FAILED, ftp_request->Id(), URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED));
		mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
	}
#ifdef DEBUG_FTP
	PrintfTofile("ftp.txt", "[%d] HandleDataReady(): CheckReply returned %d\n", Id(), reply);
#endif

	return;
 memory_failure:
	Stop();
	g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
}

void FTP::SendCommand()
{
	int len;
	char * OP_MEMORY_VAR req = NULL;

	if(ftp_request == NULL && what_to_send != FTP_SEND_QUIT)
		what_to_send = FTP_SEND_NOTHING;

	TRAPD(op_err, req = ComposeRequestL(len));

	if(OpStatus::IsError(op_err))
	{
		Stop();
		g_memory_manager->RaiseCondition(op_err);
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		return;
	}

	if(req && len)
		ProtocolComm::SendData(req, len);

}

int FTP::ActivatePassiveConnection(ServerName *data_server, unsigned short data_port, int &error)
{
	int what_to_send_next = FTP_SEND_NOTHING;
	error = 0;

	if (ftp_request && data_server)
	{
		SComm *comm = Comm::Create(mh, data_server, data_port, TRUE);

		if(comm == NULL)
		{
			goto memory_failure;
		}

		data_conn = OP_NEW(FTP_Data, (mh));
		if(data_conn == NULL)
		{
			OP_DELETE(comm);
			if(ftp_request)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, ftp_request->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			goto memory_failure;
		}

		int comm_id = data_conn->Id();

		data_conn->SetNewSink(comm);

		if(
			OpStatus::IsError(data_conn->SetCallbacks(this,this)) ||
			OpStatus::IsError(mh->SetCallBack(data_conn, MSG_COMM_CONNECTED, comm->Id())) ||
			OpStatus::IsError(mh->SetCallBackList(this, comm_id, ftp_messages, ARRAY_SIZE(ftp_messages)))
			)
		{
			Stop();
			if(ftp_request)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, ftp_request->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			return FTP_SEND_NOTHING;
		}

		if(datamode == FTP_MODE_BLOCK)
			data_conn->SetRecordMode();

		data_conn->ChangeParent(this);
		data_conn->Load();

		need_transfer_finished = TRUE;
		data_conn_status = FTP_DATA_CONN_CONNECTING;

		what_to_send_next = (ftp_request->typecode == FTP_TYPECODE_D ||
			path_status != FTP_PATH_STATUS_FILE ?
FTP_SEND_LIST :
#ifdef FTP_RESUME
		(ftp_request->use_resume ? FTP_SEND_RESTART : (disable_full_path || tried_full_path ? FTP_SEND_RETR : FTP_SEND_RETR_FULL))
#else
			(disable_full_path || tried_full_path ? FTP_SEND_RETR : FTP_SEND_RETR_FULL)
#endif
			);
	}
	else
	{
		what_to_send_next = FTP_SEND_NOTHING;
		error = URL_ERRSTR(SI, ERR_FTP_CANNOT_OPEN_PASV_DATA_CONN);
	}

	return what_to_send_next;

memory_failure:;
	Stop();
	g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
	mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
	return FTP_SEND_NOTHING;
}

void FTP::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	NormalCallCount blocker(this);
									__DO_START_TIMING(TIMING_COMM_LOAD);
	switch (msg)
	{
		/*
    case MSG_COMM_DATA_READY:
		if (!header_loaded_sent)
		{
			header_loaded_sent = TRUE;
			mh->PostMessage(MSG_FTP_HEADER_LOADED, ftp_request->Id(), what_to_send == FTP_SEND_LIST);
		}
		mh->PostMessage(MSG_COMM_DATA_READY, ftp_request->Id(), 0);
		break;
		*/
    case MSG_COMM_CONNECTED:
		data_conn_status = FTP_DATA_CONN_OPEN;
		if(!manager->GetDisableEPSV())
			manager->SetTestedEPSV();
		if(data_conn)
			mh->RemoveDelayedMessage(MSG_COMM_LOADING_FAILED, data_conn->Id(), 1);
		//SendCommand();
		break;

    case MSG_COMM_LOADING_FINISHED:
    case MSG_COMM_LOADING_FAILED:
		{
			BOOL was_data_conn = FALSE;
			if (data_conn && (unsigned int) par1 == data_conn->Id())
			{
				if (msg == MSG_COMM_LOADING_FAILED &&  par2 == 1 && data_conn_status == FTP_DATA_CONN_OPEN)
					break; // Ignore, if it is special EPSV timeout

				int old_data_conn_status = data_conn_status ;
				mh->RemoveCallBacks(this, data_conn->Id());
				data_conn->ChangeParent(NULL);
				DeleteDataConn(FALSE);
				data_conn_status = FTP_DATA_CONN_CLOSED;
				was_data_conn = TRUE;

				if (msg == MSG_COMM_LOADING_FAILED)
				{
					if((par2 == 1 || (!manager->GetDisableEPSV() && !manager->GetTestedEPSV())) && old_data_conn_status == FTP_DATA_CONN_CONNECTING)
					{
						// EPSV servers may fail to connect, and it is not detectable until the timeout
						manager->DisableEPSV();
						manager->SetTestedEPSV();
						if(ftp_request)
						{
							accept_requests = FALSE;
							ChangeParent(NULL);
							FTP_Request *req = ftp_request;
							ftp_request->ftp_conn = NULL;
							ftp_request = NULL;

							NormalCallCount blocker(this);
							manager->AddRequest(req);
						}
					}
					data_conn_status = FTP_DATA_CONN_FAILED;
					if(ftp_request && !header_loaded_sent && what_to_send != FTP_SEND_RETR_FULL && what_to_send != FTP_SEND_RETR && what_to_send != FTP_SEND_LIST)
					{
						ChangeParent(NULL);
						FTP_Request *req = ftp_request;
						ftp_request->ftp_conn = NULL;
						ftp_request = NULL;

						req->mh->PostMessage(msg, req->Id(), par2);

					}
					__DO_STOP_TIMING(TIMING_COMM_LOAD);
					if(ftp_request)
						return;
				}
				else if(what_to_send == FTP_SEND_LIST && !header_loaded_sent)
					SetProgressInformation(HEADER_LOADED, TRUE);
			}

			if(!was_data_conn)
			{
				if(what_to_send == FTP_SEND_USER || what_to_send == FTP_SEND_PASS)
				{
					msg = MSG_COMM_LOADING_FAILED;
					par2 = URL_ERRSTR(SI, ERR_FTP_USER_ERROR);
				}
				else if(ftp_request && !connection_active && what_to_send == FTP_SEND_NOTHING &&
					!(msg == MSG_COMM_LOADING_FAILED && par2 == URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND)))
				{
					// Some servers do not send a proper error code if they do not allow more conenctions, treating it as a code 530
					msg = MSG_COMM_LOADING_FAILED;
					int error_code = 0;
					MapStatusToAction(FTP_530_not_logged_in, error_code);
					par2 = error_code;
				}
				accept_requests = FALSE;
			}

			if(ftp_request)
				ftp_request->mh->PostMessage(msg, ftp_request->Id(), par2);
			if(!Connected() || Closed())
				mh->PostMessage(msg, Id(), par2);

			error = 0;
			//what_to_send = FTP_SEND_NOTHING;
			if(ftp_request)
			{
				ChangeParent(NULL);
				ftp_request->ftp_conn = NULL;
				ftp_request->used = TRUE;
				ftp_request = NULL;

				if(what_to_send == FTP_SEND_NOTHING && Connected() && !Closed() && was_data_conn)
				{
					if(!need_transfer_finished || received_transfer_finished)
						StartNextRequest();
				}
				__DO_STOP_TIMING(TIMING_COMM_LOAD);
				return;
			}
		}
		break;
	case MSG_HTTP_CHECK_IDLE_TIMEOUT:
		{
			time_t lst_active = LastActive();

			if(lst_active && lst_active + 60 < g_timecache->CurrentTime() )
			{
				Stop();
				mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
				mh->UnsetCallBacks(this);
			}
			else
				mh->PostDelayedMessage(MSG_HTTP_CHECK_IDLE_TIMEOUT,Id(),0,60000UL);
		}
		break;
    default:
		break;
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD);
}

OP_STATUS  FTP::SetCallbacks(MessageObject* master, MessageObject* parent)
{
	static const OpMessage messages[] =
    {
        MSG_COMM_LOADING_FINISHED,
        MSG_COMM_LOADING_FAILED
    };

    RETURN_IF_ERROR(mh->SetCallBackList((parent ? parent : master), Id(), messages, ARRAY_SIZE(messages)));

	return ProtocolComm::SetCallbacks(master,this);
}

OP_STATUS FTP_Request::SetStartPath(const OpString8& start_path) {
	OP_STATUS res = m_start_path.Set(start_path);
	if (OpStatus::IsError(res))
		m_start_path.Set(""); // If path couldn't be set, leave it blank (this operation shouldn't fail)
	return res;
}

const char* FTP_Request::GetParentPath()
{
	char *buf = (char *) g_memory_manager->GetTempBuf();
	if (m_start_path.Length() > 0 && path.Length() == 0 && username.Compare(FTP_AnonymousUser) != 0) // Startpath defined && we're browsing in a relative path (not starting with //)
	{
		if (m_start_path.Compare("/")==0)
			buf[0] = 0; // If m_start_path == "/", then "/" and "//" points to the same location, hence there is no parent path
		else
			op_snprintf(buf, g_memory_manager->GetTempBufLen(), "ftp://%s@%s/%s/..", username.CStr(), master->HostName()->Name(), m_start_path.CStr());
	}
	/*else if (path[0] == '/' && path[1] == 0) { // This block had to be disabled, because on some servers "CWD /" goes to home folder (at least on BPFTP)
		// There is no parent path
		buf[0] = 0;
	}*/
	else
	{
		// Default: Returning ".." as parent path
		buf[0] = '.';
		buf[1] = '.';
		buf[2] = 0;
	}
	return buf;
}

void FTP::DeleteDataConn(BOOL force)
{
	data_conn_status = FTP_DATA_CONN_NULL;
	if (data_conn)
	{
		data_conn->ChangeParent(NULL);
		SafeDestruction(data_conn);
		data_conn = NULL;
#ifdef SOCKET_SUPPORTS_TIMER
		((Comm*)GetSink())->SetSocketTimeOutInterval(0,TRUE); // Turn on socket timer for control connection
#endif
	}
}

void FTP::MakeDirMsg()
{
	if(!ftp_request)
		return;

	ftp_request->dir_msg.Empty();

	if (!reply_buf_loaded)
		return;

	OP_STATUS op_err;
	TRAP_AND_RETURN_VOID_IF_ERROR(op_err, ftp_request->dir_msg.ReserveL(reply_buf_loaded));

	ftp_request->dir_msg[0] = '\0';

	int response = 0;
	int multi_response = 0;
	char cont;
	char *line = reply_buf;
	while (line)
	{
		char* endline = op_strchr(line, '\n');
		if (endline)
		{
			if (!response)
			{
				response = op_atoi(line);
				if (!response || !op_isdigit((unsigned char) *line))
					return;
			}
			else
			{
				int i = 0;
				if (op_isdigit((unsigned char) *line))
				{
					i = op_sscanf(line, "%d%c", &multi_response, &cont);
					if (multi_response == response && cont == ' ')
					{
						return;
					}
				}

				int offset = 0;
				if (i == 2 && multi_response == response)
					offset = 4;
				int len = endline-line-offset+1;
				if(OpStatus::IsSuccess(ftp_request->dir_msg.Append(line+offset, len)))
					line = endline+1;
				else
					line = 0;
			}
		}
		else
			line = 0;
	}
}

unsigned int FTP::ReadData(char* buf, unsigned int blen)
{
	if (data_conn)
		return data_conn->ReadData(buf, blen);
	else
	{
		return 0;
	}
}

void FTP::BuildCommandL(int & len, const char *command, const char *parameter, BOOL replace_escaped, BOOL add_intial_slash)
{
	len = op_strlen(command) + (parameter && *parameter ? op_strlen(parameter) +3 : 2)+(add_intial_slash ? 1 : 0);
	CheckRequestBufL(len+1);
	op_snprintf(request, len+1, (parameter && *parameter ? "%s %s%s\r\n" : "%s\r\n"), command, (add_intial_slash ? "/" : ""), parameter);
	if(replace_escaped)
	{
		len = UriUnescape::ReplaceChars(request, UriUnescape::NonCtrlAndEsc);
	}
}
// ********************

BOOL FTP::SetNewRequest(FTP_Request *req, BOOL autostart)
{
	if(ftp_request != NULL || req == NULL)
		return FALSE;

	need_transfer_finished = FALSE;
	received_transfer_finished = FALSE;
	ftp_request = req;
	req->ftp_conn = this;

	if(data_conn)
		data_conn->StartNext();

	ChangeParent(ftp_request);
	//mh->SetCallBackList(ftp_request, ftp_request->Id(), 0, MSG_COMM_DATA_READY, MSG_COMM_CONNECTED, 0);

	tried_full_path = FALSE;
	if(!disable_full_path && !disable_size)
	{
		if(autostart)
		{
			header_loaded_sent = FALSE;

			if(current_typecode != FTP_TYPECODE_I)
				what_to_send = FTP_SEND_TYPE_INIT;
			else if(cwd.Length() && !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) )
				what_to_send = FTP_SEND_CWD_HOME;
			else if(cwd.Length() && !ftp_request->path.Length())
			{
				path_status = FTP_PATH_STATUS_DIR;
				what_to_send = (g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) ? FTP_SEND_CWD_ROOT : FTP_SEND_CWD_HOME);
			}
			else if(!cwd.Length() && !ftp_request->path.Length())
			{
				path_status = FTP_PATH_STATUS_DIR;
				what_to_send = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
			}
			else if(ftp_request->path.Length() && ftp_request->path.CStr()[ftp_request->path.Length()-1] == '/')
			{
				path_status = FTP_PATH_STATUS_DIR;
				what_to_send = FTP_SEND_CWD_FULL;
			}
			else
			{

				path_status = FTP_PATH_STATUS_FILE;
				what_to_send = FTP_SEND_SIZE_FULL;
			}
			SendCommand();
		}
		return TRUE;
	}

	RETURN_VALUE_IF_ERROR(SetStr(ftp_path, ftp_request->path.CStr()), FALSE);
	ResetPathParser();

	header_loaded_sent = FALSE;
	//ftp_request->started = !autostart; // In case the control connection times out;
	if(!autostart)
		return TRUE; // Loading is started elsewhere

	int cwd_len = 0;
	const char *cwd2 = cwd.CStr();

	if(cwd2)
	{
		cwd_len = cwd.Length();
		if(cwd_len && cwd[0] == '/')
		{
			cwd_len--;
			cwd2++;
		}
	}


	if(cwd_len && ftp_path && *ftp_path)
	{
		if(op_strncmp(ftp_path, cwd2, cwd_len) == 0)
		{
			if(!ftp_path[cwd_len] || (ftp_path[cwd_len] == '/' && !ftp_path[cwd_len+1]))
			{
				what_to_send = (!data_conn || data_conn->Closed() ? (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV):  FTP_SEND_LIST);
				path_status = FTP_PATH_STATUS_DIR;
			}
			else if(ftp_path[cwd_len] == '/')
			{
				pathname = ftp_path + cwd_len+1;
				next_dir = op_strchr(pathname,'/');
				path_status = FTP_PATH_STATUS_DIR;
				if (next_dir)
				  next_dir++;
				else
				{
				  next_dir = pathname + op_strlen(pathname);
				  path_status = FTP_PATH_STATUS_FILE;
				}
				if(path_status == FTP_PATH_STATUS_FILE && (
					ftp_request->typecode == FTP_TYPECODE_A ||
					ftp_request->typecode == FTP_TYPECODE_I))
					what_to_send = (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV);
				else
					what_to_send = (disable_size ? FTP_SEND_CWD : FTP_SEND_SIZE);
			}
			else
				what_to_send = FTP_SEND_CWD_ROOT;

		}
		else
			what_to_send = FTP_SEND_CWD_ROOT;
	}
	else if(cwd_len)
	{
		what_to_send = FTP_SEND_CWD_ROOT;
	}
	else
	{
		if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath))
			what_to_send = FTP_SEND_CWD_ROOT;
		else
		{
			if (ParsePath())
			{
				what_to_send = (disable_size ? FTP_SEND_CWD : FTP_SEND_SIZE); // 05/10/98 YNP
			}
			else
			{
				MakeDirMsg();
				what_to_send = (manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV);
			}
		}
	}

	if(what_to_send != FTP_SEND_NOTHING)
		SendCommand();

	return TRUE;
}

BOOL FTP::MatchCWD(const OpStringC8 &path)
{
	int cwd_len = cwd.Length();

	if(cwd_len)
	{
		if(cwd.Compare(path.CStr(), cwd_len) == 0 && (path.Length() == cwd_len || path[cwd_len] == '/'))
			return TRUE;
	}
	return FALSE;
}


BOOL FTP::Idle() const
{
	return (ftp_request == NULL && !ProtocolComm::PendingClose() && what_to_send != FTP_SEND_QUIT && (!need_transfer_finished || received_transfer_finished));
}

int FTP::DecideToSendTypeOrMode(int action)
{
	if(ftp_request == NULL)
		return FTP_SEND_QUIT; // Bug # 69414; crash when receiving unexpected data between requests. Quitting to avoid problems

	if((current_typecode == FTP_TYPECODE_NULL || (ftp_request->typecode != current_typecode &&
		(current_typecode != FTP_TYPECODE_I || ftp_request->typecode != FTP_TYPECODE_NULL)) ) && ftp_request->typecode != FTP_TYPECODE_D)
		return FTP_SEND_TYPE;

#ifdef FTP_RESUME
	if(!disable_mode && ftp_request->use_resume && datamode == FTP_MODE_BLOCK)
	{
		if(data_conn)
			DeleteDataConn(TRUE);
		datamode = FTP_MODE_STREAM;
		return FTP_SEND_MODE;
	}
	else
#endif
	if(!disable_mode && !data_conn && /*prefsManager->FtpDataConnectionBlockMode() && */
				datamode != FTP_MODE_BLOCK)
	{
		datamode = FTP_MODE_BLOCK;
		return FTP_SEND_MODE;
	}

	return action;
}


int FTP::MapStatusToAction(int reply, int &error)
{
	int what_to_send_next = FTP_SEND_NOTHING;
	switch(reply)
	{
	case FTP_226_closing_data_conn:
		if(data_conn)
		{
			mh->RemoveCallBacks(this, data_conn->Id());
			data_conn->ChangeParent(NULL);
			DeleteDataConn(FALSE);
			data_conn_status = FTP_DATA_CONN_CLOSED;
		}
		break;
	case FTP_421_unavailable:
		error = URL_ERRSTR(SI, ERR_FTP_SERVICE_UNAVAILABLE);
		what_to_send_next = FTP_SEND_QUIT;
		break;
	case FTP_425_cant_open_data_conn:
		error = URL_ERRSTR(SI, ERR_FTP_CANNOT_OPEN_PASV_DATA_CONN);
		break;
	case FTP_426_connection_closed:
	case FTP_451_req_action_aborted:
		error = URL_ERRSTR(SI, ERR_FTP_FILE_TRANSFER_ABORTED);
		break;
	case FTP_332_need_account:
	case FTP_530_not_logged_in:
	case FTP_531_permission_denied:
		if(/*(what_to_send == FTP_SEND_USER && reply == FTP_530_not_logged_in) || */
			(what_to_send != FTP_SEND_USER && what_to_send != FTP_SEND_PASS && reply != FTP_531_permission_denied))
		{
			error = URL_ERRSTR(SI, ERR_FTP_NOT_LOGGED_IN);
			what_to_send_next = FTP_SEND_QUIT;
		}
		else
		{
			error = URL_ERRSTR(SI, ERR_FTP_USER_ERROR);
			what_to_send_next = FTP_SEND_QUIT;
		}
		break;
	case FTP_450_file_unavailable:
	case FTP_550_file_unavailable:
	case FTP_553_file_unavailable:
	case FTP_501_parm_syntax_error:
		if(what_to_send == FTP_SEND_SIZE1 || what_to_send == FTP_SEND_SIZE ||
			what_to_send == FTP_SEND_RETR || what_to_send == FTP_SEND_LIST ||
			what_to_send == FTP_SEND_CWD ||
			what_to_send == FTP_SEND_STOR)
			error = URL_ERRSTR(SI, ERR_FTP_FILE_UNAVAILABLE);
		else
			error = URL_ERRSTR(SI, ERR_FTP_USER_ERROR);
		break;
	case FTP_331_user_ok_need_pass:
		error = URL_ERRSTR(SI, ERR_FTP_NEED_PASSWORD);
		what_to_send_next = FTP_SEND_QUIT;
		break;
	default:
		if(reply >= FTP_500_syntax_error && what_to_send == FTP_SEND_PASS && ftp_request && ftp_request->password.IsEmpty())
		{
			error = URL_ERRSTR(SI, ERR_FTP_USER_ERROR);
			what_to_send_next = FTP_SEND_QUIT;
		}
		else
		{
			error = URL_ERRSTR(SI, ERR_FTP_INTERNAL_ERROR);
		}
		break;
	}

	if(error != URL_ERRSTR(SI, ERR_FTP_NEED_PASSWORD))
	{
		OpString reply_buf1;
		if(OpStatus::IsError(reply_buf1.Set(reply_buf)))
		{
			Stop();
			g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			error = URL_ERRSTR(SI, ERR_FTP_INTERNAL_ERROR);
			return what_to_send_next;
		}

		reply_buf1.Strip(FALSE, TRUE); // The received error message is most likely CRLF terminated
		SetProgressInformation(SET_INTERNAL_ERROR_MESSAGE,0, reply_buf1.CStr());
	}

	return what_to_send_next;
}

int FTP::StartDirActions()
{
#ifdef FTP_RESUME
	if(!manager->GetRestartTested())
		return FTP_SEND_RESTART_TEST;
#endif

	if(current_typecode != FTP_TYPECODE_I)
		return FTP_SEND_TYPE_INIT;

	int what_to_send_next = FTP_SEND_NOTHING;

	if(!disable_full_path && !disable_size && !tried_full_path)
	{
		if(cwd.Length() && !g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath) )
			return FTP_SEND_CWD_HOME;
		if(!cwd.Length() && !ftp_request->path.Length())
		{
			path_status = FTP_PATH_STATUS_DIR;
			return DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
		}
		if(!ftp_request->path.Length() || ftp_request->path.CStr()[ftp_request->path.Length()-1] == '/')
		{
			path_status = FTP_PATH_STATUS_DIR;
			return FTP_SEND_CWD_FULL;
		}

		path_status = FTP_PATH_STATUS_FILE;
		return FTP_SEND_SIZE_FULL;
	}

	ResetPathParser();
	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseAbsoluteFTPPath))
		what_to_send_next = FTP_SEND_CWD_ROOT;
	else
	{
		if (ParsePath())
		{
			what_to_send_next = (disable_size ? FTP_SEND_CWD : FTP_SEND_SIZE);
		}
		else
		{
			MakeDirMsg();
			what_to_send_next = DecideToSendTypeOrMode((manager->GetDisableEPSV() ? FTP_SEND_PASV : FTP_SEND_EPSV));
		}
	}
	return what_to_send_next;
}

void FTP::StartNextRequest()
{
	need_transfer_finished = FALSE;
	received_transfer_finished = FALSE;
	FTP_Request *nftp = manager->GetNewRequest(this);
	if(nftp == NULL &&
			(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NoConnectionKeepAlive) ||
			g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode) ||
			urlManager->TooManyOpenConnections(manager->HostName())))
	{
		what_to_send = FTP_SEND_QUIT;
		SendCommand();
	}
	else
	{
		if(nftp)
		{
			SetNewRequest(nftp);
			SetProgressInformation(ACTIVITY_CONNECTION , 0, NULL);
		}
	}
}
#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
void FTP::PauseLoading()
{
	ProtocolComm::PauseLoading(); // Pauses the control connection
	if (data_conn)
	{
		data_conn->PauseLoading(); // Pauspes data connection
		if( data_conn_status == FTP_DATA_CONN_CONNECTING )
		{
			mh->RemoveDelayedMessage(MSG_COMM_LOADING_FAILED, data_conn->Id(), 1);
		}
	}
}

OP_STATUS FTP::ContinueLoading()
{
	ProtocolComm::ContinueLoading(); // Resumes the control connection
	if (data_conn)
	{
		data_conn->ContinueLoading(); // Resumes data connection
		if( data_conn_status == FTP_DATA_CONN_CONNECTING )
		{
			mh->PostDelayedMessage(MSG_COMM_LOADING_FAILED, data_conn->Id(), 1, 10*1000);
			return OpStatus::ERR;
		}
		return OpStatus::OK;
	}
	return OpStatus::ERR;
}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#endif // NO_FTP_SUPPORT
