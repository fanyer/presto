/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2000-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * Yngve Pettersen
 */
#include "core/pch.h"

#ifdef _EMBEDDED_BYTEMOBILE
#include "modules/content_filter/content_filter.h"
#endif

#include "modules/hardcore/mem/mem_man.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"

#include "modules/locale/locale-enum.h"

#include "modules/util/timecache.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

#include "modules/util/handy.h"
#include "modules/util/cleanse.h"

#include "modules/url/url_man.h"
#include "modules/upload/upload.h"

#include "modules/url/protocols/scomm.h"
#include "modules/url/protocols/pcomm.h"
#include "modules/url/protocols/connman.h"
#include "modules/url/protocols/http1.h"
#include "modules/url/protocols/http_req2.h"
#include "modules/url/protocols/http_te.h"
#include "modules/auth/auth_elm.h"
#include "modules/dochand/win.h"
#include "modules/dochand/winman.h"

#include "modules/olddebug/timing.h"
#include "modules/olddebug/tstdump.h"

#include "modules/obml_comm/obml_config.h"
# include "modules/pi/OpSystemInfo.h"

#ifdef SCOPE_RESOURCE_MANAGER
# include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER

#ifdef WEB_TURBO_MODE
# include "modules/util/datefun.h"
# include "modules/obml_comm/obml_config.h"
# include "modules/obml_comm/obml_id_manager.h"
# include "modules/obml_comm/obml_config.h"
# ifdef URL_FILTER
#  include "modules/content_filter/content_filter.h"
# endif // URL_FILTER
#endif // WEB_TURBO_MODE

// http1.cpp

// HTTP 1.1 protocol

//extern MemoryManager* g_memory_manager;
//extern PrefsManager* prefsManager;
//extern URL_Manager* urlManager;

//#define YNP_WORK

#ifdef _DEBUG
#ifdef YNP_WORK
#define DEBUG_HTTP
#define DEBUG_HTTP_MSG
#define DEBUG_HTTP_FILE
#define DEBUG_HTTP_HEXDUMP
#define DEBUG_HTTP_REQDUMP
#define DEBUG_HTTP_REQDUMP1
#define DEBUG_UPLOAD
#endif
#endif

#ifdef _RELEASE_DEBUG_VERSION
#define DEBUG_HTTP
#define DEBUG_HTTP_MSG
#define DEBUG_HTTP_FILE
#define DEBUG_HTTP_HEXDUMP
#define DEBUG_HTTP_REQDUMP
#define DEBUG_HTTP_REQDUMP1
#endif

#ifdef DEBUG_HTTP_HEXDUMP
//#define DEBUG_HTTP_HEXDUMP_ONLY_INDATA

#include "modules/olddebug/tstdump.h"

#endif

#ifndef CHECK_IDLE_CONNECTION_DELAY
#define CHECK_IDLE_CONNECTION_DELAY 60
#endif
#ifndef CHECK_IDLE_CONNECTION_SEND_DELAY
#define CHECK_IDLE_CONNECTION_SEND_DELAY 25
#endif
#if defined(_EMBEDDED_BYTEMOBILE) || defined(_BYTEMOBILE) || defined(WEB_TURBO_MODE)
#define CHECK_IDLE_CONNECTION_SEND_DELAY_BYTEMOBILE 60
#endif

#if CHECK_IDLE_CONNECTION_DELAY < 1
#error "CHECK_IDLE_CONNECTION_DELAY must be 1 or more"
#endif

#ifdef _EMBEDDED_BYTEMOBILE
#define EBO_MAX_UNKNOWN_REQ_SIZE 204800
#endif

// 03/02/98 GI

static const OpMessage http_messages[] =
{
    MSG_COMM_DATA_READY
    //MSG_HTTP_HEADER_LOADED,
};

static const OpMessage http_messages2[] =
{
    MSG_COMM_LOADING_FAILED,
    MSG_COMM_LOADING_FINISHED
};

//***************************************************************************

void HTTP_Request_List::Out()
{
	static_cast<HTTP_Request_Head*>(GetList())->m_count--;
	Link::Out();
}

void HTTP_Request_List::Into(HTTP_Request_Head* list)
{
	Link::Into(static_cast<Head*>(list));
	list->m_count++;
}


void HTTP_Request_List::Precede(HTTP_Request_List* l)
{
	static_cast<HTTP_Request_Head*>(l->GetList())->m_count++;
	Link::Precede(static_cast<Link*>(l));
}


void HTTP_Request_List::Follow(HTTP_Request_List* l)
{
	static_cast<HTTP_Request_Head*>(l->GetList())->m_count++;
	Link::Follow(static_cast<Link*>(l));
}


void HTTP_Request_List::IntoStart(HTTP_Request_Head* list)
{
	Link::IntoStart(static_cast<Head*>(list));
	list->m_count++;
}

//***************************************************************************

HTTP_1_1::HTTP_1_1( MessageHandler* msg_handler, ServerName_Pointer _host, unsigned short _port, BOOL privacy_mode)
	: HttpRequestsHandler(_host, _port, privacy_mode, msg_handler)
{
	InternalInit(_host, _port);
}

void HTTP_1_1::InternalInit(ServerName* _host, unsigned short _port)
{
	manager = NULL;
	conn_elem = NULL;
	//HTTP_count ++;
	//PrintfTofile("httpcon.txt","Creating %u : %lu : %s:%u   : %p\n", Id(), HTTP_count, _host->Name(),_port,this);
	sending_request = request = NULL;
	prev_response = 0;

    //SetHeaderLoaded(FALSE);
    //SetProxyRequest(proxy);

    header_str = 0;
    //selfheader_info = header_info = NULL;
	content_length = 0;
	chunk_size = chunk_loaded = 0;

	protocol_def_port = 80;
	SetHandlingHeader(FALSE);
	info.is_uploading = FALSE;
	info.http_1_0_keep_alive = FALSE;
	info.continue_allowed = TRUE;
	info.waiting_for_chunk_header = FALSE;
	info.expect_trailer = FALSE;
	info.trailer_mode = FALSE;
	info.host_is_1_0 = FALSE;
	info.http_1_1_pipelined = FALSE;
	info.http_protocol_determined = FALSE;
	info.connection_active = FALSE;
	info.first_request = TRUE;
	info.connection_established = FALSE;
	info.connection_failed = FALSE;
	info.tested_http_1_1_pipelinablity = FALSE;
	info.read_source = FALSE;
	info.handling_end_of_connection = FALSE;
	info.activated_100_continue_fallback = FALSE;

#ifdef _SSL_USE_SMARTCARD_
	info.smart_card_authenticated = FALSE;
#endif
#ifdef _EMBEDDED_BYTEMOBILE
	info.ebo_authenticated = FALSE;
	info.predicted = FALSE;
	info.ebo_optimized = FALSE;
	info.pending_reqs_to_new_ebo_conn = FALSE;
#endif
#ifdef WEB_TURBO_MODE
	info.turbo_proxy_auth_retry = FALSE;
	info.turbo_unhandled_response = FALSE;
#endif
	info.restarting_requests = FALSE;
#ifdef URL_TURBO_MODE_HEADER_REDUCTION
	m_common_headers = NULL;
	m_common_response_headers = NULL;
#endif // URL_TURBO_MODE_HEADER_REDUCTION

	request_count = 0;
	http_10_ka_max = 0;

	last_active = 0;
#ifdef HTTP_BENCHMARK
	BenchMarkGetTime(last_finished);
	BenchMarkNewConn();
#endif
#ifdef OPERA_PERFORMANCE
	connection_number = urlManager->GetNextConnectionSeqNumber();
	http_Manager->OnConnectionCreated(connection_number, _host->Name());
#endif // OPERA_PERFORMANCE
	info.request_list_updated = FALSE;
}

void HTTP_1_1::ConstructL()
{
	HttpRequestsHandler::ConstructL();

#ifdef HTTP_CONNECTION_TIMERS
	m_response_timer_is_started = false;
	m_idle_timer_is_started = false;
	m_response_timer = OP_NEW_L(OpTimer, ());
	m_response_timer->SetTimerListener(this);
	m_idle_timer = OP_NEW_L(OpTimer, ());
	m_idle_timer->SetTimerListener(this);
#endif//HTTP_CONNECTION_TIMERS

	selfheader_info = header_info = OP_NEW_L(HeaderInfo, (HTTP_METHOD_GET));
	LEAVE_IF_ERROR(mh->SetCallBack(this, MSG_HTTP_CHECK_IDLE_TIMEOUT,Id()));

	unsigned long delay = CHECK_IDLE_CONNECTION_SEND_DELAY;
#ifdef _EMBEDDED_BYTEMOBILE
	if( urlManager->GetEmbeddedBmOpt() )
		delay = CHECK_IDLE_CONNECTION_SEND_DELAY_BYTEMOBILE;
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
	if( generic_info.turbo_enabled )
		delay = CHECK_IDLE_CONNECTION_SEND_DELAY_BYTEMOBILE;
#endif // WEB_TURBO_MODE

	mh->PostDelayedMessage(MSG_HTTP_CHECK_IDLE_TIMEOUT,Id(),0, delay * 1000UL);
}

HTTP_1_1::~HTTP_1_1()
{
	InternalDestruct();
}

void HTTP_1_1::InternalDestruct()
{
#ifdef OPERA_PERFORMANCE
	http_Manager->OnConnectionClosed(connection_number);
#endif // OPERA_PERFORMANCE

	if (mh)
		mh->RemoveDelayedMessage(MSG_HTTP_CHECK_IDLE_TIMEOUT, Id(), 0);

	//HTTP_count --;
	//PrintfTofile("httpcon.txt","Destroying %u : %lu\n", Id(),HTTP_count);
	SetNoMoreRequests();
	MoveRequestsToNewConnection(NULL, TRUE); // Should not be necessary but better safe than crashed

	//delete selfheader_info;
	OP_DELETEA(header_str);

#ifdef URL_TURBO_MODE_HEADER_REDUCTION
	if( m_common_headers )
	{
		OP_DELETE(m_common_headers);
	}
	if( m_common_response_headers )
	{
		OP_DELETE(m_common_response_headers);
	}
#endif // URL_TURBO_MODE_HEADER_REDUCTION

#ifdef DEBUG_HTTP_FILE
	//PrintfTofile("winsck.txt","[%d] HTTP:~HTTP() - call_count=%d\n", id, call_count);
#endif

	if(conn_elem)
		conn_elem->conn = NULL;

#ifdef HTTP_CONNECTION_TIMERS
	OP_DELETE(m_response_timer);
	OP_DELETE(m_idle_timer);
#endif//HTTP_CONNECTION_TIMERS

#ifdef DEBUG_HTTP_REQDUMP
	OpString8 textbuf1;

	textbuf1.AppendFormat("http%04d.txt",Id());
	PrintfTofile(textbuf1,"Destroying Connection %d Tick %lu\n",Id(), (unsigned long) g_op_time_info->GetWallClockMS());
	CloseDebugFile(textbuf1);
#endif
}

//***************************************************************************

void HTTP_1_1::Clear()
{
	header_info->headers.Clear();
	header_info->response_text.Empty();
	header_info->content_length = 0;
	header_info->response = HTTP_NO_RESPONSE;
	header_info->info.http1_1 = FALSE;
	header_info->info.has_trailer =
		header_info->info.received_close =
		header_info->info.received_content_length = FALSE;
	header_info->info.http_10_or_more = TRUE;
	OP_DELETEA(header_str);
	header_str = NULL;
	if(request)
		request->SetHeaderLoaded(FALSE);
	content_length = content_loaded = 0;
	pipelined_items.Clear();
}

//***************************************************************************


void HTTP_1_1::RequestMoreData()
{
	OP_ASSERT(info.connection_established);
	OP_ASSERT(is_connected);
	if(!info.connection_active)
		SetDoNotReconnect(TRUE);
	info.connection_active = TRUE;

	if (sending_request == NULL)
	{
		ComposeRequest();
		return;
	}

    if(sending_request->method == HTTP_METHOD_CONNECT)
    {
        // this is a tunnel;
        if(sending_request->GetHeaderLoaded())
            ProtocolComm::RequestMoreData();
        return;
    }

#ifdef HAS_SET_HTTP_DATA
	if (sending_request->GetSendingRequest() && sending_request->HasPayload())
	{
		if(sending_request->info.send_expect_100_continue && !sending_request->info.send_100c_body)
			return;

		SetRequestMsgMode(UPLOADING_FILES);

		int request_rest_len=0;

		int max = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize)*1024;
		char *request_buf = OP_NEWA(char, max);
		if(!request_buf)
		{
			Stop();
			mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			return;
		}

		BOOL done = FALSE;
		request_rest_len = sending_request->GetOutputData((unsigned char *) request_buf, max, done);
# ifdef _URL_EXTERNAL_LOAD_ENABLED_
		if (request_rest_len == 0 && !sending_request->upload_done && sending_request->info.ex_is_external)
		{
			/* No more upload data ready at the moment. Will call this method
			   again as soon as more data is available. We assume that
			   sending_request->upload_data is of type Upload_AsyncBuffer
			   here. */
			sending_request->info.upload_buffer_underrun = TRUE;
			return;
		}
# endif // _URL_EXTERNAL_LOAD_ENABLED_
#ifdef DEBUG_UPLOAD
		BinaryDumpTofile((unsigned char *) request_buf, request_rest_len, "upload.mim");
#endif

		ProtocolComm::SetProgressInformation(UPLOAD_COUNT, MAKELONG(sending_request->UploadedFileCount(),sending_request->FileCount()));

#ifdef DEBUG_HTTP_HEXDUMP
#ifndef DEBUG_HTTP_HEXDUMP_ONLY_INDATA
		{
			OpString8 textbuf;

			textbuf.AppendFormat("MoreRequest Sending data from %d (request ID %d)",Id(), sending_request->Id());
			DumpTofile((unsigned char *) request_buf,(unsigned long) request_rest_len,textbuf,"http.txt");
#ifdef DEBUG_HTTP_REQDUMP
			OpString8 textbuf1;

			textbuf1.AppendFormat("http%04d.txt",Id());
			DumpTofile((unsigned char *) request_buf,(unsigned long) request_rest_len,textbuf,textbuf1);
			OpString8 textbuf2;
			textbuf2.AppendFormat("htpr%04d.txt",sending_request->Id());
			DumpTofile((unsigned char *) request_buf,(unsigned long) request_rest_len,textbuf,textbuf2);
#endif
		}
#endif
#endif

		info.is_uploading = TRUE;

		if(done)
		{
#ifdef SCOPE_RESOURCE_MANAGER
			sending_request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_REQUEST_FINISHED, sending_request);
#endif //SCOPE_RESOURCE_MANAGER

			info.is_uploading = FALSE;
			sending_request->SetSentRequest(TRUE);
			sending_request =NULL;
		}

		last_active = g_timecache->CurrentTime();

		ProtocolComm::SendData(request_buf,request_rest_len);

		if(done)
			ProtocolComm::SetAllSentMsgMode(UPLOADING_FINISHED, LOAD_PROGRESS);

        return;
    }
	else
	{
		sending_request->SetSentRequest(TRUE);
		sending_request =NULL;
	}
#endif // HTTP_DATA
}

//***************************************************************************


void HTTP_1_1::ComposeRequest()
{
	HTTP_Request_List * OP_MEMORY_VAR item = request_list.First();
	int i = 0;

	if(item == NULL && !generic_info.disable_more_requests)
	{
		manager->ForciblyMoveRequest(this);
		return;
	}

	info.request_list_updated = FALSE;
	while(item)
	{
		last_active = g_timecache->CurrentTime();

		if(!item->request)
			break;

		if(!item->request->GetSentRequest())
		{
			// If we cannot trust the server to send correct content-lengths:
			// Do not send request before this is the current request
			if(request != item->request &&
#ifdef _BYTEMOBILE
			   !urlManager->GetByteMobileOpt() &&
#endif
				(!manager->GetHTTP_Trust_ContentLength() || !info.http_1_1_pipelined
//#ifndef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
				|| (/*!request->GetHeaderLoaded() && */!manager->GetUsedKeepAlive())
//#endif
				))
				break;

#ifdef HTTP_BENCHMARK
			BenchMarkLastActive();
			if(item->request == request && last_finished == 0)
				BenchMarkGetTime(last_finished);
#endif

#ifdef DEBUG_HTTP_FILE
			{
				int total = request_list.Cardinal(), unsent=0;
				HTTP_Request_List *item1 = item;

				while(item1)
				{
					unsent++;
					item1 = item1->Suc();
				}

				PrintfTofile("http.txt","Connection %u: %d unsent requests (of %d). Request %u %s\n",
					Id(), unsent, total, item->request->Id(),
					(item->request->GetWaiting() ? "Waiting" : "Not Active"));
			}
#endif

			if(
#ifdef _BYTEMOBILE
			   !urlManager->GetByteMobileOpt() &&
#endif
			   (item->request->GetWaiting() || item->request->GetSendingRequest()))
				break;

			SetRequestMsgMode(NO_STATE);

			info.activated_100_continue_fallback = FALSE;
			item->request->Clear();
			item->request->SetSendingRequest(TRUE);

			sending_request = item->request;
			unsigned len;
			sending_request->info.pipeline_used_by_previous = FALSE;
			if(item->Pred() && item->Pred()->request)
				item->Pred()->request->info.pipeline_used_by_previous = TRUE;

#ifdef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
			sending_request->info.sent_pipelined = (item->Pred() != NULL);
#endif

			char * OP_MEMORY_VAR reqst = NULL;
			OP_ASSERT(sending_request->http_conn != NULL);

			TRAPD(op_err, reqst = sending_request->ComposeRequestL(len));
			if(OpStatus::IsError(op_err))
			{
				NormalCallCount blocker(this);
				HandleEndOfConnection(URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				return;
			}

			if(sending_request == request)
			{
				request->sendcount++;
			}

			if(sending_request->method != HTTP_METHOD_CONNECT)
            {
                SecurityLevel sec_level = (SecurityLevel) GetSecurityLevel();
                if(sec_level != SECURITY_STATE_UNKNOWN)
				{
					UpdateSecurityInformation();
				    //sending_request->SetProgressInformation(SET_SECURITYLEVEL, sec_level ,(const uni_char *) GetSecurityText());
				}
            }

			if(sending_request->info.send_expect_100_continue)
			{
				if(OpStatus::IsError(mh->SetCallBack(this,MSG_HTTP_FORCE_CONTINUE,sending_request->Id())) ||
					!mh->PostDelayedMessage(MSG_HTTP_FORCE_CONTINUE,sending_request->Id(),0,1500))
					sending_request->info.send_expect_100_continue = FALSE; // Disable 100 continue in case of low memory
			}

#ifdef OPERA_PERFORMANCE
			http_Manager->OnRequestSent(item->request->http_conn->GetConnectionNumber(), item->request->GetRequestNumber(), sending_request->info.secure, GetHTTPMethodString(sending_request->GetMethod()), sending_request->request->origin_host->Name(), sending_request->request->path, item->request->time_request_created, item->request->GetPriority());
#endif // OPERA_PERFORMANCE

			if(sending_request->GetSentRequest())
				sending_request = NULL;

#ifdef SCOPE_RESOURCE_MANAGER
			HTTP_Request::LoadStatusData_Raw load_status_param(item->request, reqst, len);
			item->request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_REQUEST, &load_status_param);
#endif // SCOPE_RESOURCE_MANAGER
			ProtocolComm::SendData(reqst, len);
#ifdef HTTP_CONNECTION_TIMERS
			HTTP_Request *prev = item->Pred() ? item->Pred()->request : NULL;
			if( prev == NULL || (prev && prev->GetHeaderLoaded()) )
			{
				StopIdleTimer();
				StartResponseTimer();
/*				m_idle_timer->Stop();
				UINT http_response_timout = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HTTPResponseTimeout);
				if( http_response_timeout )
					m_response_timer->Start(http_response_timeout*1000);*/
			}
#endif//HTTP_CONNECTION_TIMERS
			if(sending_request)
				return;
		}
		else if(item->request->GetWaiting())
			break;

		if (info.request_list_updated)
			return; // we can no longer trust request_list as Stop() can have wiped it
		item = item->Suc();
		i++;
	}
}

void HTTP_1_1::SetHeaderInfoL()
{
	char *tmp = header_str;
	BOOL recvd_10_keepalive = FALSE;

#ifdef HTTP_BENCHMARK
	if(last_finished)
	{
		BenchMarkAddConnIdleTime(last_finished);
		last_finished = 0;
	}
#endif

#ifdef DEBUG_HTTP_FILE
	PrintfTofile("http.txt", " HTTP SetHeaderInfo: %d (Request %d):\n\n%s\n", Id(), request->Id(), tmp);
	PrintfTofile("headers.txt", " HTTP SetHeaderInfo: %d (Request %d):\n\n%s\n", Id(), request->Id(), tmp);
#endif

	SetHandlingHeader(TRUE); // 17/09/98 YNP

	BOOL version_is_1_0	= FALSE;

	if(op_strncmp(tmp,"HTTP/",5) == 0 || op_strnicmp(tmp,"HTTP/",5) == 0) // Compliant servers will be handled by the first efficient call, non-compliant by the second.
	{
		int major,minor;
		int scan_result = op_sscanf(tmp+5, "%d.%d", &major, &minor);

		if (scan_result == 2 && (major < 1 || (major == 1 && minor == 0)))
			version_is_1_0 = TRUE;

		if(!info.http_protocol_determined)
		{
			if(scan_result == 2)
			{
				if (!version_is_1_0)
				{
					header_info->info.http1_1 = TRUE;
					info.host_is_1_0 = FALSE;
				}
				manager->SetHTTP_ProtocolDetermined(TRUE);
				info.http_protocol_determined = TRUE;
			}
		}
		else
		{
			if(!version_is_1_0)
			{
				BOOL host_is_1_0 = info.host_is_1_0;
				header_info->info.received_close = host_is_1_0;
				header_info->info.http1_1 = !host_is_1_0;
			}
		}

	}
	else if((op_strncmp(tmp,"HTTP",4) == 0 || op_strnicmp(tmp,"HTTP",4) == 0) && (tmp[4] == ' ' || tmp[4] == '\t') )
	{
		version_is_1_0 = TRUE;
		manager->SetHTTP_ProtocolDetermined(TRUE);
		info.http_protocol_determined = TRUE;
	}

	if (version_is_1_0)
	{
		manager->SetHTTP_1_1(FALSE);
		header_info->info.received_close = TRUE;
		header_info->info.http1_1 = FALSE;
		info.host_is_1_0 = TRUE;
	}

	while (*tmp != '\0')
		if (*tmp++ == ' ' && op_isdigit((unsigned char) *tmp)) // 01/04/98 YNP
			break;
	if (*tmp != '\0')
	{
		header_info->response = op_atoi(tmp);

		tmp +=3;
		char tmpchar;
		while((tmpchar = *tmp) != '\0')
		{
			if(!op_isspace(tmpchar) || tmpchar == '\r' || tmpchar == '\n')
				break;
			tmp++;
		}

		if(tmpchar && tmpchar != '\r' && tmpchar != '\n')
		{
			char *start = tmp;
			char *stop = tmp+1;

			while((tmpchar = *tmp) != '\0')
			{
				if(tmpchar == '\r' || tmpchar == '\n')
					break;
				tmp++;
				if(!op_isspace(tmpchar))
					stop = tmp;
			}

			RAISE_IF_ERROR(header_info->response_text.Set(start, stop-start));
		}
		else
			header_info->response_text.Empty();

		while(*tmp == '\r' || *tmp == '\n')
			tmp++;
	}

	header_info->loading_started = request->loading_started;

	header_info->headers.SetKeywordList(KeywordIndex_HTTP_MIME);
	header_info->headers.SetValueL(tmp);
	content_length = 0;

#ifdef SCOPE_RESOURCE_MANAGER
	request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE, request);
#endif // SCOPE_RESOURCE_MANAGER

#ifdef URL_TURBO_MODE_HEADER_REDUCTION
	// Expand the response headers and update the header cache.
	if(GetTurboOptimized() && !GetLoadDirect() && !request->request->use_proxy_passthrough 
		&& request->request->IsTurboProxy())
		ExpandResponseHeadersL(header_info->headers);
#endif //URL_TURBO_MODE_HEADER_REDUCTION

	HeaderEntry *header;

#if defined(WEB_TURBO_MODE)
	// Handle out of order responses from Opera Web optimizing proxy
	header = header_info->headers.GetHeader("X-Opera-Info", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		urlManager->SetWebTurboAvailable(TRUE);
		ParameterList *req_parameters = header->GetParameters(PARAM_SEP_COMMA);
		if(req_parameters)
		{
			Parameters *key = req_parameters->GetParameter("ID", PARAMETER_ASSIGNED_ONLY);

			if(header_info->info.http1_1 && key && key->Value())
				info.turbo_unhandled_response = !HandleOutOfOrderResponse(op_atoi(key->Value()));
			
			if( header_info->response == HTTP_SERVICE_UNAVAILABLE && !info.turbo_unhandled_response ) // 503
			{
				key = req_parameters->GetParameter("e", PARAMETER_ASSIGNED_ONLY);
				if( key && key->Value() )
					HandleBypassResponse(op_atoi(key->Value()));
			}

		}
	}
	else if( generic_info.turbo_enabled
			 && request->request->use_turbo
			 && !request->info.bypass_turbo_proxy
#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
			 && !request->request->is_tunnel
#endif // URL_SIGNAL_PROTOCOL_TUNNEL
		)
	{
		urlManager->SetWebTurboAvailable(FALSE);
	}

	if( generic_info.turbo_enabled && request->info.bypass_turbo_proxy )
	{
		header = header_info->headers.GetHeader("Retry-After", NULL, HEADER_RESOLVE);
		if(header && header->Value())
		{
			OpStringC8 hdr_val(header->Value());
			time_t retry_time = 0;
			if( hdr_val.SpanOf("1234567890") == hdr_val.Length() )
				retry_time = g_timecache->CurrentTime() + op_atoi(hdr_val.CStr());
			else
				retry_time = ::GetDate(hdr_val.CStr());

			if( retry_time != 0 )
				g_pcnet->OverridePrefL(request->request->origin_host->GetUniName().CStr(), PrefsCollectionNetwork::UseWebTurbo, (int)retry_time, FALSE);
		}
	}

	// Handle Turbo Host Header
	header = header_info->headers.GetHeader("X-Opera-Host", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		manager->SetTurboHostL(header->Value());
	}

	// Handle Turbo Proxy authentication challenge
	header = header_info->headers.GetHeader("X-OA", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		// Re-send request with auth info
		g_obml_id_manager->SetTurboProxyAuthChallenge(header->Value());
		if( header_info->response == HTTP_UNAUTHORIZED )
			info.turbo_proxy_auth_retry = TRUE;
	}

	// Handle Turbo Proxy compression ratio indicator
	header = header_info->headers.GetHeader("X-OC", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		op_sscanf(header->Value(), "%d/%d", &header_info->turbo_transferred_bytes, &header_info->turbo_orig_transferred_bytes);
#ifdef URL_NETWORK_DATA_COUNTERS
		g_network_counters.turbo_savings += (INT64)header_info->turbo_orig_transferred_bytes - (INT64)header_info->turbo_transferred_bytes;
#endif
	}
#endif // WEB_TURBO_MODE


#if defined(_EMBEDDED_BYTEMOBILE)
	// Handle out of order responses from Bytemobile proxy
	header = header_info->headers.GetHeader("X-EBO-Resp", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		ParameterList *ebo_parameters = header->GetParameters(PARAM_SEP_COMMA);
		if(ebo_parameters)
		{
			Parameters *key = ebo_parameters->GetParameter("Key", PARAMETER_ASSIGNED_ONLY);
			if(key && key->Value())
				HandleOutOfOrderResponse(op_atoi(key->Value()));

			key = ebo_parameters->GetParameter("Flags", PARAMETER_ASSIGNED_ONLY);
			if(key && key->Value())
			{
				int flag_value = op_atoi(key->Value());
				if (request && flag_value & 0x01)
				{
					// This is a bypass request. 
					// Bit 1 is the only bit that is currently used in the Flags values.
					request->info.bypass_ebo_proxy = TRUE;
				}

			}

#ifdef URL_FILTER
			key = ebo_parameters->GetParameter("Bypass", PARAMETER_ASSIGNED_ONLY);
			if(key && key->Value())
			{
				//The value should be used in future checking of requests before submitted directly or through proxy.
				OpString value;
				value.Set("http://*");
				value.Append(key->Value());
				if(value[value.Length()-1] =='/')
					value.Append("*");
				else
					value.Append("/*");
				TRAPD(op_err, g_urlfilter->AddFilterL(value));// for instance "http://*.yahoo.com/"
			}
#endif
		}
	}

	header = header_info->headers.GetHeader("X-EBO-Opt",NULL, HEADER_RESOLVE);
	if (header && header->Value())
	{
		ParameterList *ebo_parameters = header->GetParameters(PARAM_SEP_COMMA);

		if(ebo_parameters)
		{
			Parameters *address = ebo_parameters->GetParameter("Address", PARAMETER_ASSIGNED_ONLY);
			Parameters *item = ebo_parameters->GetParameter("BCResp", PARAMETER_ASSIGNED_ONLY);
			
			bool bcrespCorrect = false;
			if (item && item->Value()) //check if BCResp is correct
			{
				urlManager->SetEBOBCResp(item->Value());
				bcrespCorrect = GetBMInformationProvider().checkBCRespL( 
									item->Value());
			}

			if(bcrespCorrect && address && address->Value())
			{
				//Only try to connect if both BCResp and Address have values.
				if (!urlManager->GetEmbeddedBmOpt())
				{
					header_info->info.received_close = TRUE;
					urlManager->SetEmbeddedBmOpt(TRUE);
					urlManager->SetEBOServer(address->Value(), 0);
				}
			}
			else
			{
				urlManager->SetEmbeddedBmOpt(FALSE);
				urlManager->SetEmbeddedBmDisabled( TRUE);
			}

			if(!bcrespCorrect)
				urlManager->SetEmbeddedBmOpt(FALSE);


			item = ebo_parameters->GetParameter("OCReq", PARAMETER_ASSIGNED_ONLY);
			if(item && item->Value())
			{
				SetEBOOCReq(item->Value());
			}
			item = ebo_parameters->GetParameter("Settings", PARAMETER_ASSIGNED_ONLY);
			if(item && item->Value())
			{
				urlManager->SetEBOOptions(item->Value());
			}

			if(header_info->response == HTTP_BAD_REQUEST)
			{
				MoveRequestsToNewConnection(NULL);
				Stop();
				mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
				return;
			}
		}
	}

	header = header_info->headers.GetHeader("X-EBO-Pred", NULL, HEADER_RESOLVE);
	do
	{
		if (header && header->Value() && request->request->predicted_depth<2)
		{
			ParameterList *ebo_parameters = header->GetParameters(PARAM_SEP_WHITESPACE | PARAM_ONLY_SEP | PARAM_NO_ASSIGN);
			if(ebo_parameters)
			{
				Parameters *item = ebo_parameters->First();

				while(item)
				{
					if (item->Value())
					{
						char * url_path;
						if (request->GetProxyRequest())
							url_path = (char *)request->GetPath();
						else
						{
							url_path = OP_NEWA_L(char, op_strlen((char *)(request->Origin_HostName())) + 15 + op_strlen(request->GetPath()));
							if (request->Origin_HostPort() != 80)
								op_sprintf(url_path, "HTTP://%s:%d%s", request->Origin_HostName(), request->Origin_HostPort(), request->GetPath());
							else
								op_sprintf(url_path, "HTTP://%s%s", request->Origin_HostName(), request->GetPath());
						}

						URL referer_url = urlManager->GetURL(url_path);

						URL tmp = urlManager->GetURL(item->Name());
						//set parameter so that request knows that this is a predicted object.
						if (tmp.Status(FALSE) == URL_UNLOADED)
						{
							tmp.SetPredicted(TRUE,request->request->predicted_depth+1);
							tmp.Load(g_main_message_handler, referer_url, FALSE, TRUE, FALSE);
						}

						if (!request->GetProxyRequest())
							OP_DELETEA(url_path);
					}
					item = item->Suc();
				}
			}
		}
	}
	while (header && (header = header_info->headers.GetHeader("X-EBO-Pred", header, HEADER_RESOLVE)));

#endif // _EMBEDDED_BYTEMOBILE
	
	header = header_info->headers.GetHeaderByID(HTTP_Header_Content_Length);
	if(header && header->Value())
	{
		header->StripQuotes();
		if(OpStatus::IsError(StrToOpFileLength(header->Value(), &header_info->content_length)))
			header_info->content_length = 0;
		else
			header_info->info.received_content_length = TRUE;
		content_length = header_info->content_length;

	}

	if(header_info->response == HTTP_PARTIAL_CONTENT)
	{
		if(header_info->info.received_content_length)
			header_info->content_length = (request->info.use_range_start ? request->range_start + content_length : content_length);
		header = header_info->headers.GetHeaderByID(HTTP_Header_Content_Range);
		if(header)
		{
			OpStringC8 header_val(header->Value());
			if (header_val.CompareI("bytes ", 6) == 0)
			{
				OpString8 length_string;
				ANCHOR(OpString8, length_string);
				OpString8 rangestart_string;
				ANCHOR(OpString8, rangestart_string);
				OpString8 range_end_string;
				ANCHOR(OpString8, range_end_string);

				int val_len = header_val.Length();
				if (length_string.Reserve(val_len) != NULL &&
					rangestart_string.Reserve(val_len) != NULL &&
					range_end_string.Reserve(val_len) != NULL &&
					op_sscanf(header_val.CStr(),"%*s %[0123456789]-%[0123456789]/%[0123456789*]", rangestart_string.DataPtr(), range_end_string.DataPtr(), length_string.DataPtr()) == 3)
				{
					OpFileLength range_start=0, range_end=0, recvd_content_length=0;
					if((length_string.Compare("*")== 0 ||
						(length_string.FindFirstOf('*') == KNotFound && OpStatus::IsSuccess(StrToOpFileLength(length_string.CStr(), &recvd_content_length)))) &&
						OpStatus::IsSuccess(StrToOpFileLength(rangestart_string.CStr(), &range_start)) &&
						OpStatus::IsSuccess(StrToOpFileLength(range_end_string.CStr(), &range_end))
						)
					{
						if(length_string.Compare("*")== 0)
							recvd_content_length = range_end+1;

						// A request like "Range: bytes=-X" would generate an answer with range start set (to end-X); in this case 
						// (request->range_start===FILE_LENGTH_NONE) it's acceptable if use_range_start==FALSE and range_start!=0
						if((request->info.use_range_start && range_start != request->range_start) ||
							(!request->info.use_range_start && range_start != 0 && (request->range_start!=FILE_LENGTH_NONE || !request->info.use_range_end)) || // "Range: bytes=-X"
							range_start >range_end ||
							range_end > header_info->content_length + HTTPRange::ConvertFileLengthNoneToZero(range_start)) // Content-Length is the length of the answer (the range), not of the file...
						{
							header_info->info.received_close =TRUE;
							request->info.use_range_start = FALSE;
							request->info.use_range_end = FALSE;
							request->Headers.ClearHeader("Range");
							request->Headers.ClearHeader("If-Range");
							// restart connection
							MoveRequestsToNewConnection(NULL);
							Stop();
							mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
							return;
						}
					}

					if(recvd_content_length)
					{
						header_info->info.received_content_length = TRUE;
						header_info->content_length = recvd_content_length;
					}
				}
			}
		}
	}

	header = NULL;
	// If proxy request and not HTTP 1.1 look for the proxy-connection header
	if(!header_info->info.http1_1 && request->info.proxy_request)
		header = header_info->headers.GetHeaderByID(HTTP_Header_Proxy_Connection);
	if(header == NULL)
		header = header_info->headers.GetHeaderByID(HTTP_Header_Connection);
	if(header && header->Value())
	{
		ParameterList *params = header->GetParametersL((PARAM_SEP_COMMA | PARAM_NO_ASSIGN | PARAM_ONLY_SEP), KeywordIndex_HTTP_General_Parameters);

		if(params)
		{
			if(header_info->info.http1_1)
			{
				if(params->GetParameterByID(HTTP_General_Tag_Close) != NULL)
					header_info->info.received_close = TRUE;
#ifndef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
				else if(!manager->GetUsedKeepAlive() && params->GetParameterByID(HTTP_General_Tag_Keep_Alive) == NULL /* && header_info->response != HTTP_NOT_MODIFIED*/)
					manager->SetUsedKeepAlive();
#else
				else
					manager->SetUsedKeepAlive();
#endif
			}
			else
			{
				Parameters *param;

				header->Out();
				param = params->First();
				if(param)
				{
					if(params->GetParameterByID(HTTP_General_Tag_Close) == 0)
					{
						Parameters *param1 = params->GetParameterByID(HTTP_General_Tag_Keep_Alive);
						if(param1 != NULL)
						{
							manager->SetHTTP_1_0_KeepAlive(TRUE);
							info.http_1_0_keep_alive = TRUE;
							recvd_10_keepalive = TRUE;
							header_info->info.received_close = FALSE;
							HeaderEntry *header1;

							header1 = header_info->headers.GetHeaderByID(HTTP_Header_Keep_Alive);
							if(header1)
							{
								ParameterList *params2 = header1->GetParametersL(0, KeywordIndex_HTTP_General_Parameters);

								if(params2)
								{
									Parameters *param2 = params2->GetParameterByID(HTTP_General_Tag_Max);

									if(param2 && param2->Value())
										http_10_ka_max = param2->GetUnsignedValue();
								}
							}
						}

						while(param)
						{
							HeaderEntry *header1;
							if(param != param1)
								while((header1 = header_info->headers.GetHeader(param->Name(), NULL, HEADER_RESOLVE)) != NULL)
								{
									header1->Out();
									OP_DELETE(header1);
								}
							param = param->Suc();
						}
					}
					else
					{
						manager->SetHTTP_1_0_UsedConnectionHeader(TRUE);
						header_info->info.received_close = TRUE;
						info.http_1_0_keep_alive = FALSE;
#ifdef WEB_TURBO_MODE
						// If the connection was closed in the same 401 where we got our turbo proxy
						// challenge (by a caching proxy) then we need to clear the request so it's 
						// moved to a new connection with the challenge response.
						if( info.turbo_proxy_auth_retry )
						{
							request->Clear();

							// Make sure POST data gets re-sent
							if( request->HasPayload() )
								request->ResetL();
						}
#endif //WEB_TURBO_MODE
					}
				}
				OP_DELETE(header);
			}
		}
	}
//#ifndef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
	else if(header_info->info.http1_1 && !manager->GetUsedKeepAlive() && !header_info->headers.Empty())
		manager->SetUsedKeepAlive();
//#endif

#ifdef _BYTEMOBILE
	if(urlManager->GetByteMobileOpt())
	{
		HeaderEntry *previous_header = NULL;

		header = header_info->headers.GetHeader("Via", previous_header,
												HEADER_RESOLVE);

		// Default to Off.
		urlManager->SetByteMobileOpt(FALSE);

		do
		{
			if (header && header->Value() &&
				strstr(header->Value(),"BM_MONACO"))
			{
				urlManager->SetByteMobileOpt(TRUE);
				break;
			}

			previous_header = header;
		}
		while (header &&
			   (header = header_info->headers.GetHeader("Via",
														previous_header,
														HEADER_RESOLVE)));
	}
#endif

	if(!info.tested_http_1_1_pipelinablity)
		TestAndSetPipeline();

#ifdef OPERA_PERFORMANCE
	http_Manager->OnPipelineDetermined(connection_number, info.http_1_1_pipelined);
#endif // OPERA_PERFORMANCE

	header = header_info->headers.GetHeaderByID(HTTP_Header_Transfer_Encoding);
	if(header && header->Value())
	{
		ParameterList *params = header->GetParametersL((PARAM_SEP_COMMA | PARAM_NO_ASSIGN | PARAM_ONLY_SEP));

		if(params)
		{
			Parameters *encoding = params->First();
			while(encoding)
			{
				ParameterList *current_encoding = encoding->GetParametersL(PARAM_SEP_SEMICOLON, KeywordIndex_HTTP_General_Parameters);
				Parameters *name = (current_encoding ? current_encoding->First() : NULL);

#ifdef _HTTP_COMPRESS
				if(name)
				{
					HTTP_compression mth;
					//extern HTTP_compression CheckTransferEncodeKeyword(const char *keyword);

					switch(mth = (HTTP_compression) name->GetNameID())
					{
					case HTTP_Identity:
					case HTTP_Unknown_encoding :
						break;
					case HTTP_Chunked:
						if(encoding->Suc() != NULL)
							break;
						header_info->info.received_content_length = FALSE;
						content_length = header_info->content_length = 0;

						info.use_chunking = TRUE;
						info.trailer_mode = FALSE;
						info.expect_trailer = FALSE;
						info.waiting_for_chunk_header = TRUE;
						chunk_size = chunk_loaded = 0;
						break;
					case HTTP_Deflate:
					case HTTP_Compress:
					case HTTP_Gzip:
						header_info->info.received_content_length = FALSE;
						content_length = header_info->content_length = 0;
						request->AddTransferDecodingL(mth);
					}
				}
				encoding = encoding->Suc();
#else
				//if(name && op_stricmp(name->Name(),"chunked") == 0 && encoding->Suc() == NULL)
				if(name && name->GetNameID() == HTTP_General_Tag_Chunked && encoding->Suc() == NULL)
				{
					header_info->info.received_content_length = FALSE;
					content_length = header_info->content_length = 0;

					info.use_chunking = TRUE;
					info.trailer_mode = FALSE;
					info.expect_trailer = FALSE;
					info.waiting_for_chunk_header = TRUE;
					chunk_size = chunk_loaded = 0;
				}
				encoding = NULL;
#endif
			}
			header->Out();
			OP_DELETE(header);
		}
	}

	if(header_info->response == HTTP_NOT_MODIFIED)
		info.use_chunking = FALSE; // Some servers apparently send chunking  when they should not send it.

	header = header_info->headers.GetHeaderByID(HTTP_Header_Trailer);
	if(header && info.use_chunking)
		info.expect_trailer = TRUE;

	if((!header_info->info.received_content_length && !info.use_chunking) && header_info->response != HTTP_NOT_MODIFIED )
		header_info->info.received_close = TRUE;

	if(request && request->method == HTTP_METHOD_CONNECT && header_info->response == HTTP_OK)
	{
		header_info->info.received_content_length = FALSE;
		header_info->info.received_close = TRUE;
	}

	if(manager->GetHTTP_No_Pipeline())
		header_info->info.received_close = TRUE;

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	request->info.do_ntlm_authentication = FALSE;
	BOOL SetUpNTLM_NegotiateL(BOOL proxy, HeaderList &headers, HTTP_Request *request, BOOL &set_waiting,
						  AuthElm *auth_element, BOOL first_request);

	if(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableNTLM) &&
		((request->info.proxy_request  && !request->info.secure && header_info->response == HTTP_PROXY_UNAUTHORIZED) ||
			((!request->info.proxy_request|| request->info.secure || request->server_negotiate_auth_element) && header_info->response == HTTP_UNAUTHORIZED) ))
	{
		BOOL set_waiting = FALSE;
		BOOL is_proxy = (request->info.proxy_request && !request->info.secure && header_info->response == HTTP_PROXY_UNAUTHORIZED);

		request->info.do_ntlm_authentication = SetUpNTLM_NegotiateL(
				is_proxy, header_info->headers,	request, set_waiting,
				(is_proxy ? request->proxy_ntlm_auth_element : request->server_negotiate_auth_element),
				info.first_request);

		if(set_waiting || !header_info->info.received_close)
		{
			HTTP_Request_List *item = request_list.First()->Suc();
			if(item && item->request)
			{
				request->SetWaiting(TRUE);
				item->request->SetWaiting(TRUE);
			}
			info.first_request= TRUE;
		}
	}
#endif

	if(header_info->response != HTTP_NOT_MODIFIED &&
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		!request->info.do_ntlm_authentication &&
#endif
		((!header_info->info.http1_1 && (!recvd_10_keepalive || header_info->info.received_close)) ||
		 (header_info->info.http1_1 && !recvd_10_keepalive && header_info->info.received_close)) &&
		header_info->info.received_content_length && header_info->content_length == 0)
	{
		header_info->info.received_content_length = FALSE;
		header_info->info.received_close = TRUE;
		mh->PostDelayedMessage(MSG_HTTP_ZERO_LENGTH_TIMEOUT, Id(), 0, 1000);
		OpStatus::Ignore(mh->SetCallBack(this,MSG_HTTP_ZERO_LENGTH_TIMEOUT, Id())); // It's not fatal if this fails
	}

	if(header_info->info.received_close)
	{
		HTTP_Request_List *item;

		info.first_request = FALSE;
		generic_info.disable_more_requests = TRUE;
		item = request_list.First();
#ifdef DEBUG_HTTP_REQDUMP
		OpString8 textbuf1;

		textbuf1.AppendFormat("http%04d.txt",Id());
		PrintfTofile(textbuf1,"HTTP connection %d : Received Close.\n", Id());
		{
			HTTP_Request_List *item1;
			item1 = request_list.First();
			while(item1)
			{
				if(item1->request)
					PrintfTofile(textbuf1,"   HTTP connection %d : Request %d.\n", Id(),item1->request->Id());
				else
					PrintfTofile(textbuf1,"   HTTP connection %d : Empty Request.\n", Id());
				item1 = item1->Suc();
			}
		}
#endif
		if(item && item->request == request)
			item = item->Suc();
		if(item && item->request)
			this->MoveRequestsToNewConnection(item);
	}
	else if (info.first_request
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		&& !request->info.do_ntlm_authentication
#endif
#ifdef WEB_TURBO_MODE
		&& !info.turbo_proxy_auth_retry
#endif // WEB_TURBO_MODE
		)
	{
		info.first_request = FALSE;
		HTTP_Request_List *item = request_list.First()->Suc();
		if(item && item->request &&
			!request->GetForceWaiting()/* && (info.host_is_1_0 || info.http_1_1_pipelined)*/)
		{
			if(request->GetSentRequest() && item->request->GetWaiting())
			{
				item->request->SetWaiting(FALSE);
				RequestMoreData();
			}
		}
	}

	content_loaded = 0;

	if(request && request->method != HTTP_METHOD_CONNECT)
	{
		SecurityLevel sec_level = (SecurityLevel) GetSecurityLevel();
		if(sec_level != SECURITY_STATE_UNKNOWN)
		{
			UpdateSecurityInformation();
			//sending_request->SetProgressInformation(SET_SECURITYLEVEL, sec_level ,(const uni_char *) GetSecurityText());
		}
	}

	SetHandlingHeader(FALSE); // 17/09/98 YNP
}

#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
BOOL HTTP_1_1::HandleOutOfOrderResponse(int seq_no)
{
	OP_NEW_DBG("HandleResponse","request_handling");
	HTTP_Request_List *item = request_list.First();

	OP_DBG(("Connection %d handling request (seq %d)",Id(), seq_no));

	if (item && item->request && item->request->GetRequestNumber() != seq_no)
	{
		OP_DBG(("    Request out of order"));
		while(item)
		{
			OP_DBG(("    Testing request %d (seq %d): %s",(item->request ? item->request->Id() : 0),(item->request ? item->request->GetRequestNumber() : 0),(item->request ? item->request->request->request.CStr() : "None")));
			if(item->request && item->request->GetRequestNumber() == seq_no)
			{
				item->Out();
				item->IntoStart(&request_list);
				request->header_info = item->request->header_info;
				item->request->header_info = header_info;
				request->SetHeaderLoaded(FALSE);
				BOOL temp = request->info.sent_pipelined;
				request->info.sent_pipelined = item->request->info.sent_pipelined;
				request = item->request;
				request->info.sent_pipelined = temp;
				request->SetHeaderLoaded(TRUE);
				OP_ASSERT(request);
				ChangeParent(request);
				break;
			}
			item =  item->Suc();
		}
	}
	OP_DBG(("    Request %shandled!",(item ? "" : "NOT ")));

	return item != NULL;
}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

#ifdef WEB_TURBO_MODE

BOOL HTTP_request_st::IsTurboProxy()
{
	return connect_host->GetUniName().Compare(g_obml_config->GetTurboProxyName(), connect_host->GetUniName().Length()) == 0;
}

void HTTP_1_1::HandleBypassResponse(int error_code)
{
	OP_NEW_DBG("BypassResponse","request_handling");
	OP_DBG(("Connection %d handling bypass response %d",Id(),error_code));
	if( request == NULL )
		return;

	switch( error_code )
	{
	case WEB_TURBO_BYPASS_RESPONSE:
		{
#ifdef URL_FILTER
			OpString site_url;
			// Add server to filter with wildcard
			OpStringC hostname = request->request->origin_host->GetUniName();
			if( site_url.Reserve(hostname.Length()+9) )
			{
				site_url.Set(UNI_L("http://"));
				site_url.Append(hostname);
#ifdef _DEBUG
				BOOL found = FALSE;
				g_urlfilter->CheckBypassURL(site_url.CStr(),found);
				OP_ASSERT(!found);
#endif // _DEBUG
				site_url.Append(UNI_L("/*"));
				TRAPD(err, g_urlfilter->AddFilterL(site_url.CStr()));
				OpStatus::Ignore(err);
				OP_DBG(("    Adding filter for %s",request->request->origin_host->GetName().CStr()));
			}
#endif // URL_FILTER
		}
		// Do not break here.
	case WEB_TURBO_HOSTNAME_LOOKUP_FAULURE:
		request->info.bypass_turbo_proxy = TRUE;
		break;
	default:
		OP_ASSERT(!"Unhandled Turbo Proxy error code");
	}
}
#endif // WEB_TURBO_MODE

void HTTP_1_1::TestAndSetPipeline()
{
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	BOOL opt = FALSE;
#ifdef _EMBEDDED_BYTEMOBILE
	opt = urlManager->GetEmbeddedBmOpt();
#endif
#ifdef WEB_TURBO_MODE
	opt = opt || generic_info.turbo_enabled;
#endif
	if(opt && !generic_info.load_direct)
	{
		manager->SetTestedHTTP_1_1_Pipeline(TRUE);
		manager->SetHTTP_Trust_ContentLength(TRUE);
		info.http_1_1_pipelined = TRUE;
		manager->SetHTTP_1_1_Pipelined(TRUE);
		return;
	}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

	if (!g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnablePipelining, manager->HostName()))
	{
		manager->SetTestedHTTP_1_1_Pipeline(TRUE);
		manager->SetHTTP_Trust_ContentLength(TRUE);
		info.http_1_1_pipelined = FALSE;
		manager->SetHTTP_1_1_Pipelined(FALSE);
		return;
	}

#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
	BOOL secure = manager->IsSecure();
#else
	BOOL secure = FALSE;
#endif

#ifdef _BYTEMOBILE
	if( urlManager->GetByteMobileOpt() )
	{
		manager->SetTestedHTTP_1_1_Pipeline(TRUE);
		manager->SetHTTP_Trust_ContentLength(TRUE);
		info.http_1_1_pipelined = TRUE;
		manager->SetHTTP_1_1_Pipelined(TRUE);
		return;
	}
#endif

	if(request && request->info.proxy_request && !secure)
	{
		info.http_1_1_pipelined = TRUE;
		manager->SetHTTP_1_1_Pipelined(TRUE);
		return; // Do not test pipeline on proxy requests, cannot trust Server header
	}

	HeaderEntry *header = header_info->headers.GetHeaderByID(HTTP_Header_Server);
	BOOL support_pipeline_fully = TRUE;

	if(header && header->Value())
	{
		manager->SetTestedHTTP_1_1_Pipeline(TRUE);
		info.tested_http_1_1_pipelinablity = TRUE;
		// Check for servers with known pipeline problems
		// IIS/4.0
		OpStringC8 server_type(header->Value());
		int match;

		if(server_type.CompareI("Netscape", STRINGLENGTH("Netscape")) == 0)
		{
			manager->SetHTTP_No_Pipeline(TRUE);
			generic_info.disable_more_requests = TRUE;
			support_pipeline_fully = FALSE;

			HTTP_Request_List *item= request_list.First();
			if(item)
				item = item->Suc();
			if(item)
				MoveRequestsToNewConnection(item,TRUE);
		}

		if((match = server_type.FindI("IIS/")) != KNotFound)
		{
			int ver = op_atoi(server_type.CStr() + (match + STRINGLENGTH("IIS/")));

			if(ver>0 && ver <=5)
			{
				support_pipeline_fully = FALSE;
			}
#if (defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
			if(manager->IsSecure())
			{
				// Some IIS 5 servers do not handle renegotiation and no certificate properly
				manager->HostName()->SetSSLSessionIIS4(manager->Port(), (ver >= 5 ? TRUE : FALSE));
			}
#endif
		}
		else if(server_type.FindI("xitami") != KNotFound ||
				server_type.FindI("Monkey/") != KNotFound ||
				server_type.FindI("EFAServer/") != KNotFound ||
				server_type.FindI("EFAController/") != KNotFound ||
				server_type.FindI("WebLogic") != KNotFound)
		{
			support_pipeline_fully = FALSE;
		}

	}
	else if(header_info->info.http1_1)
	{
		support_pipeline_fully = FALSE;
	}
	else
	{
		manager->SetHTTP_No_Pipeline(TRUE);
		support_pipeline_fully = FALSE;
		generic_info.disable_more_requests = TRUE;

		HTTP_Request_List *item= request_list.First();
		if(item)
			item = item->Suc();
		if(item)
			MoveRequestsToNewConnection(item,TRUE);
	}

	info.http_1_1_pipelined = support_pipeline_fully;
	manager->SetHTTP_1_1_Pipelined(support_pipeline_fully);
}

//***************************************************************************

//FIXME:OOM Needs OOM handling
unsigned int HTTP_1_1::ReadData(char *buf, unsigned bytes_to_read)
{
	OpFileLength bytes_read=0;
	OpFileLength blen = bytes_to_read;

#ifdef DEBUG_HTTP_FILE
	PrintfTofile("http.txt","Entering ReadData [%d]\n", Id());
#endif

	if(info.use_chunking)
	{
		bytes_read =ReadChunkedData(buf,blen);
		content_loaded += bytes_read;

#ifdef DEBUG_HTTP_FILE
	    PrintfTofile("http.txt","Leaving ReadData [%d] #1\n", Id());
#endif

#ifdef _EMBEDDED_BYTEMOBILE
		if( info.ebo_optimized && !info.pending_reqs_to_new_ebo_conn
			&& content_loaded >= EBO_MAX_UNKNOWN_REQ_SIZE )
		{
			info.pending_reqs_to_new_ebo_conn = TRUE;
			HTTP_Request_List* fitem = request_list.First();
			HTTP_Request_List* item = (fitem && fitem->request == request) ? fitem->Suc() : NULL;
			MoveRequestsToNewConnection(item);
		}
#endif // _EMBEDDED_BYTEMOBILE

		return (unsigned) bytes_read;
	}
	if(content_length  && content_length - content_loaded < blen)
		blen = content_length - content_loaded;
	if (!content_buf.IsEmpty())
	{
		bytes_read = content_buf.Length();
		if (bytes_read > blen)
			bytes_read = blen;
#ifdef DEBUG_HTTP_FILE
			/*
			FILE *tfp = fopen("c:\\klient\\winsck.txt","a");
			fprintf(tfp,"   HTTP::GetContent() read %d bytes of %d, id: %d\n", bytes_read, (int) content_buf.Length(), Id());
			fclose(tfp);
		*/
#endif
		content_buf.CopyInto(buf, (size_t)bytes_read);
		content_buf.Consume((size_t)bytes_read);
		content_loaded += bytes_read;
		if((!content_length || content_loaded < content_length) && !info.read_source)
		{
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Sending MSG_COMM_DATA_READY [%d] #1:%d\n", Id(),info.read_source);
#endif
			mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0);
		}
	}
	else if(!info.read_source)
	{
#ifdef DEBUG_HTTP_FILE
		PrintfTofile("http.txt","Reading for connection [%d] #1:\n", Id());
#endif
		bytes_read = ProtocolComm::ReadData(buf, (unsigned long) blen);
		info.read_source = TRUE;

#ifdef _EMBEDDED_BYTEMOBILE
		if( info.ebo_optimized && !header_info->info.received_content_length &&
			!info.pending_reqs_to_new_ebo_conn && content_buf.Length() >= EBO_MAX_UNKNOWN_REQ_SIZE )
		{
			info.pending_reqs_to_new_ebo_conn = TRUE;
			HTTP_Request_List* fitem = request_list.First();
			HTTP_Request_List* item = (fitem && fitem->request == request) ? fitem->Suc() : NULL;
			MoveRequestsToNewConnection(item);
		}
#endif // _EMBEDDED_BYTEMOBILE

#ifdef DEBUG_HTTP_HEXDUMP
		{
			OpString8 textbuf;

			textbuf.AppendFormat("Getcontent Reading data from %d Tick %lu",Id(), (unsigned long) g_op_time_info->GetWallClockMS());
			DumpTofile((unsigned char *) buf,(unsigned long) bytes_read,textbuf,"http.txt");
#ifdef DEBUG_HTTP_REQDUMP
			OpString8 textbuf1;

			textbuf1.AppendFormat("http%04d.txt",Id());
			DumpTofile((unsigned char *) buf,(unsigned long) bytes_read,textbuf,textbuf1);
			OpString8 textbuf2;
			textbuf2.AppendFormat("htpr%04d.txt",request->Id());
			DumpTofile((unsigned char *) buf,(unsigned long) bytes_read,textbuf,textbuf2);
#endif
		}
#endif
		content_loaded += bytes_read;
	}
	if(ProtocolComm::Closed())
	{
		HandleEndOfConnection();
	}
	else if(content_length && content_loaded == content_length)
	{
		if (header_info->info.received_close || (info.host_is_1_0 && !info.http_1_0_keep_alive))
			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);

		MoveToNextRequest();
	}

#ifdef DEBUG_HTTP_FILE
    PrintfTofile("http.txt","Leaving ReadData [%d] #2\n", Id());
#endif

	return (unsigned) bytes_read;
}

//FIXME:OOM Needs OOM handling
OpFileLength HTTP_1_1::ReadChunkedData(char *buf, OpFileLength blen)
{
	OpFileLength bytes_read = 0;
	OpFileLength bytes_added;
	BOOL more= TRUE;
	BOOL request_finished = FALSE;
	BOOL incorrect_format = FALSE;

	while(more)
	{
		bytes_added = ProcessChunkedData(buf,blen, more, request_finished, incorrect_format);
		bytes_read += bytes_added;
		buf+= bytes_added;
		blen -= bytes_added;
	}

	if(incorrect_format)
		goto handle_incorrect_format;

	if(blen == 0 || request_finished)
	{
#ifdef DEBUG_HTTP_FILE
		PrintfTofile("http.txt","Sending MSG_COMM_DATA_READY [%d] #2:%d\n", Id(),info.read_source);
#endif
		if(!info.read_source || (request_finished && !content_buf.IsEmpty()))
			mh->PostMessage((request_finished && !content_buf.IsEmpty() ? MSG_COMM_NEW_REQUEST : MSG_COMM_DATA_READY), Id(), 0);
		return bytes_read;
	}

	if(info.read_source)
		return bytes_read;

	{
	OpFileLength max_buf_len = (unsigned long)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
	OpFileLength buf_len = (max_buf_len < (OpFileLength)HTTP_TmpBufSize) ? max_buf_len : (OpFileLength)HTTP_TmpBufSize;

	OP_ASSERT(content_buf.Length() < (size_t)buf_len);
	buf_len -= content_buf.Length();

	char* tmp_buf = HTTP_TmpBuf;
	OpFileLength cnt_len;

#ifdef DEBUG_HTTP_FILE
		PrintfTofile("http.txt","Reading for connection [%d] #2:\n", Id());
#endif
	cnt_len = ProtocolComm::ReadData(tmp_buf, (unsigned long) buf_len-1);
	info.read_source = TRUE;

#ifdef DEBUG_HTTP_HEXDUMP
	{
		OpString8 textbuf;

		textbuf.AppendFormat("Read Chunked content data from %d Tick %lu",Id(), (unsigned long) g_op_time_info->GetWallClockMS());
		DumpTofile((unsigned char *) tmp_buf,(unsigned long) cnt_len,textbuf,"http.txt");
#ifdef DEBUG_HTTP_REQDUMP
		OpString8 textbuf1;

		textbuf1.AppendFormat("http%04d.txt",Id());
		DumpTofile((unsigned char *) tmp_buf,(unsigned long) cnt_len,textbuf,textbuf1);
		OpString8 textbuf2;
		textbuf2.AppendFormat("htpr%04d.txt",request->Id());
		DumpTofile((unsigned char *) tmp_buf,(unsigned long) cnt_len,textbuf,textbuf2);
#endif
	}
#endif

	if(cnt_len && OpStatus::IsError(content_buf.AppendCopyData(tmp_buf, (size_t)cnt_len)))
	{
		mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		EndLoading();
		Stop();
		SetNewSink(NULL);
		return bytes_read;
	}

	more= TRUE;

	while(more)
	{
		bytes_added = ProcessChunkedData(buf,blen, more, request_finished, incorrect_format);
		bytes_read += bytes_added;
		buf+= bytes_added;
		blen -= bytes_added;

		if(incorrect_format)
			goto handle_incorrect_format;
	}

	if(content_buf.IsEmpty() && ProtocolComm::Closed())
	{
		HandleEndOfConnection();
	}
    }
	return bytes_read;

handle_incorrect_format:;
	// Illegal format // may be related to pipelining, but we don't know
	content_buf.Clear();

	if(!request || !request->info.pipeline_used_by_previous || request->info.is_formsrequest)
		MoveToNextRequest();
	else
	{
		//if(request)
		//	request->sendcount++;
		bytes_read = 0;
	}

	manager->IncrementPipelineProblem();
	manager->SetHTTP_1_1_Pipelined(FALSE);
	SignalPipelineReload();
	MoveRequestsToNewConnection(NULL, TRUE);
	HandleEndOfConnection(0); // handle as normal premature close
	Stop();

	info.use_chunking = FALSE;
	info.expect_trailer = info.trailer_mode = FALSE;
	return bytes_read; // Ignore data due to cache
}

size_t HTTP_1_1::FindChunkTerminator(OpData &data, unsigned terminatorFlags, size_t &longestTerminatorLength)
{
	longestTerminatorLength = 0;
	size_t pos = OpDataNotFound;
	if ((terminatorFlags & CTF_CRLFCRLF) && (pos = data.FindFirst("\r\n\r\n")) != OpDataNotFound)
		longestTerminatorLength = 4;
	else if ((terminatorFlags & CTF_LFCRLF) && (pos = data.FindFirst("\n\r\n")) != OpDataNotFound)
		longestTerminatorLength = 3;
	else if ((terminatorFlags & CTF_LFLF) && (pos = data.FindFirst("\n\n")) != OpDataNotFound)
		longestTerminatorLength = 2;
	else if ((terminatorFlags & CTF_CRLF) && (pos = data.FindFirst("\r\n")) != OpDataNotFound)
		longestTerminatorLength = 2;
	else if ((terminatorFlags & CTF_LFCR) && (pos = data.FindFirst("\n\r")) != OpDataNotFound)
		longestTerminatorLength = 2;
	else if ((terminatorFlags & CTF_CR) && (pos = data.FindFirst("\r")) != OpDataNotFound)
		longestTerminatorLength = 1;
	else if ((terminatorFlags & CTF_LF) && (pos = data.FindFirst("\n")) != OpDataNotFound)
		longestTerminatorLength = 1;
	return pos;
}

OpFileLength HTTP_1_1::ProcessChunkedData(char *buf, OpFileLength blen, BOOL &more, BOOL &request_finished, BOOL &incorrect_format)
{
#ifdef DEBUG_HTTP_FILE
	PrintfTofile("http.txt","Entered ProcessChunkedData [%d]:\n", Id());
#endif
	more = FALSE;
	request_finished = FALSE;
	incorrect_format = FALSE;

	if(chunk_size == chunk_loaded && !info.trailer_mode)
	{
		if(!info.waiting_for_chunk_header)
		{
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Exiting ProcessChunkedData [%d] #1:\n", Id());
#endif // DEBUG_HTTP_FILE
			info.waiting_for_chunk_header = TRUE;
			if(content_buf.IsEmpty())
			{
				mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0);
				return 0;
			}
		}
	}

	if(info.waiting_for_chunk_header && !content_buf.IsEmpty())
	{
		size_t term_length;
		size_t term_pos = FindChunkTerminator(content_buf, CTF_CRLF | CTF_LFCR | CTF_CR | CTF_LF, term_length);
		BOOL success = term_pos != OpDataNotFound && (term_length == 2 || Closed());

		if(success)
		{
			long ret;
			OpData chunk_size_str(content_buf, 0, term_pos);
			if (OpStatus::IsError(chunk_size_str.ToLong(&ret, NULL, 16)))
			{
				incorrect_format = TRUE;
				request_finished = TRUE;
				return 0;
			}
			chunk_size = ret;
			content_buf.Consume(term_pos + term_length);
			chunk_loaded = 0;

			info.waiting_for_chunk_header = FALSE;

			if(chunk_size == 0)
			{
				info.trailer_mode = TRUE;
#ifdef DEBUG_HTTP_FILE
				PrintfTofile("http.txt","Last Chunk : looking for headers [%d]\n", Id());
#endif // DEBUG_HTTP_FILE
			}
		}
		else if (term_pos == OpDataNotFound && content_buf.FindEndOf("0123456789abcdefABCDEF") != OpDataNotFound)
		{
			incorrect_format = TRUE;
			request_finished = TRUE;
			return 0;
		}
	}

	if(info.trailer_mode && !content_buf.IsEmpty())
	{
		BOOL finished = FALSE;

#ifdef DEBUG_HTTP_FILE
		PrintfTofile("http.txt","Entering trailer mode for HTTP connection [%d] (Content available %d)\n", Id(),
			content_buf.Length());
#endif // DEBUG_HTTP_FILE

		size_t term_length;
		if(FindChunkTerminator(content_buf, CTF_CRLF | CTF_LFCR | CTF_CR | CTF_LF, term_length) == 0)
		{
			content_buf.Consume(term_length);
			finished = TRUE;
		}

		OpData headers;
		if(!finished)
		{
			size_t term_pos = FindChunkTerminator(content_buf, CTF_CRLFCRLF | CTF_LFCRLF | CTF_LFLF, term_length);
			if (term_pos != OpDataNotFound)
			{
				finished  = TRUE;
				headers = OpData(content_buf, 0, term_pos);
				if (OpStatus::IsError(headers.AppendConstData("\r\n\0", 3)))
					headers.Clear();
				content_buf.Consume(term_pos + term_length);
			}
		}

		if(finished)
		{
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Entering trailer mode finished for HTTP connection [%d] (Content available %d)\n", Id(),
				content_buf.Length());
#endif // DEBUG_HTTP_FILE
			if(!headers.IsEmpty())
			{
				header_info->trailingheaders.SetKeywordList(KeywordIndex_HTTP_MIME);
				if(OpStatus::IsSuccess(header_info->trailingheaders.SetValue(headers.Data())))//FIXME:OOM Allocation?
				{
					header_info->info.has_trailer = TRUE;
#ifdef WEB_TURBO_MODE
					// Handle Turbo Proxy compression ratio indicator
					HeaderEntry* header = header_info->trailingheaders.GetHeader("X-OC", NULL, HEADER_RESOLVE);
					if(header && header->Value())
					{
						op_sscanf(header->Value(), "%d/%d", &header_info->turbo_transferred_bytes, &header_info->turbo_orig_transferred_bytes);
#ifdef URL_NETWORK_DATA_COUNTERS
						g_network_counters.turbo_savings += (INT64)header_info->turbo_orig_transferred_bytes - (INT64)header_info->turbo_transferred_bytes;
#endif // URL_NETWORK_DATA_COUNTERS
					}
#endif // WEB_TURBO_MODE
					ProtocolComm::SetProgressInformation(HEADER_LOADED);
				}
				else
					g_memory_manager->RaiseCondition(OpStatus::ERR_NO_MEMORY);
			}

			info.use_chunking = FALSE;
			info.expect_trailer = info.trailer_mode = FALSE;

			MoveToNextRequest();
			request_finished = TRUE;
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Exiting ProcessChunkedData [%d] #2:\n", Id());
#endif // DEBUG_HTTP_FILE
			return 0;
		}
	}

	OpFileLength bytes_read = 0;
	if(!content_buf.IsEmpty() && !info.waiting_for_chunk_header && !info.trailer_mode)
	{
		OpFileLength read_len = blen;
		OpFileLength content_avail = content_buf.Length();
		if(read_len >= chunk_size-chunk_loaded)
		{
			read_len = chunk_size-chunk_loaded;

			if(read_len && read_len <= content_avail)
			{
				if(content_avail <= 2 || read_len > content_avail - 2)
					read_len --;
			}
		}
		if(read_len > content_avail)
			read_len = content_avail;
		if(read_len == 0)
		{
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Exiting ProcessChunkedData [%d] #3:\n", Id());
#endif // DEBUG_HTTP_FILE
			return bytes_read;
		}

		content_buf.CopyInto(buf, (size_t)read_len);
		chunk_loaded += read_len;
		content_buf.Consume((size_t)read_len);
		bytes_read += read_len;

		if(chunk_loaded == chunk_size)
		{
			size_t term_length;
			if(FindChunkTerminator(content_buf, CTF_CRLF | CTF_LFCR | CTF_CR | CTF_LF, term_length) == 0)
				content_buf.Consume(term_length);
			else
			{
				incorrect_format = TRUE;
				request_finished = TRUE;
				more = FALSE;
				return bytes_read;
			}
			info.waiting_for_chunk_header = TRUE;
		}

		if(!content_buf.IsEmpty())
			more = TRUE;
	}

#ifdef DEBUG_HTTP_FILE
	PrintfTofile("http.txt","Exiting ProcessChunkedData [%d] #4:\n", Id());
#endif // DEBUG_HTTP_FILE
	return bytes_read;
}

//***************************************************************************

void HTTP_1_1::MoveToNextRequest()
{
#ifdef WEB_TURBO_MODE
	if( generic_info.turbo_enabled && info.turbo_unhandled_response && request )
	{
		request->SetHeaderLoaded(FALSE);
		OP_ASSERT(header_info == request->header_info);
		header_info->Clear();
		info.turbo_unhandled_response = FALSE;
		return;
	}
#endif // WEB_TURBO_MODE

	HTTP_Request_List *item;

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
	if(request && request->info.do_ntlm_authentication && !ProtocolComm::Closed())
	{
        OP_STATUS op_err;

		mh->UnsetCallBacks(request);
		//request->info.do_ntlm_authentication = FALSE;
		header_info = request->header_info;
		request->Clear();
		TRAP(op_err, request->ResetL());
		OpStatus::Ignore(op_err);
		sending_request = NULL;
		Clear();

        op_err = mh->SetCallBackList(request, Id(), http_messages, ARRAY_SIZE(http_messages));
		if(request->method == HTTP_METHOD_CONNECT && OpStatus::IsSuccess(op_err))
		{
			if(request->GetSink() != this)
			request->SetNewSink(this);
	        op_err = mh->SetCallBackList(request, Id(), http_messages2, ARRAY_SIZE(http_messages2));
		}
		if(OpStatus::IsError(op_err) || generic_info.disable_more_requests)
		{
			MoveRequestsToNewConnection(request_list.First(), TRUE);
			Stop();
			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
			return;
		}

		if(call_count == 0)
			ComposeRequest();
		return;
	}
	//info.do_ntlm_authentication = FALSE;
#endif

	if(request)
	{
#ifdef DEBUG_HTTP_FILE
		PrintfTofile("httpreq.txt","Removing Request %d from HTTP connection %d\n", request->Id(), Id());
		PrintfTofile("http.txt","Removing Request %d from HTTP connection %d\n", request->Id(), Id());
#endif
		mh->UnsetCallBacks(request);
		if(request->InList())
		{
			if(request->info.pipeline_used_by_previous || request->info.sent_pipelined)
				pipelined_items.Add(request->GetMsgId());
			request->LoadingFinished();
			request->PopSink();
			request->Out();
		}

		ChangeParent(NULL);

		if(sending_request == request)
		{
			generic_info.disable_more_requests = TRUE;
			request = NULL;
			if(request_list.First()->Suc())
				MoveRequestsToNewConnection(request_list.First()->Suc());
			EndLoading();
			Stop();
			mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(), 0);
		}
		request = NULL;

		item = request_list.First();
		OP_DELETE(item);
		manager->RequestHandled();
	}

	item = request_list.First();
	if(item == NULL)
		pipelined_items.Clear(); // Completed list of requests
	request = (item != NULL ? item->request : NULL);
	header_info = selfheader_info;
#ifdef _DEBUG_CONN
	if(!request)
	{
		BOOL check = urlManager->TooManyOpenConnections(manager->HostName());
		PrintfTofile("http1.txt","Empty queue in HTTP connection %d %s [%lu]\n", Id(),
			(check ? "Too many connections" : "enough free"), prefsManager->CurrentTime());
	}
#endif
	if(request)
	{
		if(ProtocolComm::Closed())
		{
			HandleEndOfConnection();

			return;
		}
#ifdef DEBUG_HTTP_FILE
		PrintfTofile("httpreq.txt","Starting to use Request %d in HTTP connection %d\n", request->Id(), Id());
		PrintfTofile("http.txt","Starting to use Request %d in HTTP connection %d\n", request->Id(), Id());
#endif

#ifdef HTTP_BENCHMARK
		BenchMarkGetTime(last_finished);
#endif
		if(request->GetSentRequest())
			request->sendcount++;

		ChangeParent(request);
		header_info = request->header_info;
		Clear();

        OP_STATUS op_err;
        op_err = mh->SetCallBackList(request, Id(), http_messages, ARRAY_SIZE(http_messages));
		if(request->method == HTTP_METHOD_CONNECT && OpStatus::IsSuccess(op_err))
		{
			request->SetNewSink(this);
	        op_err = mh->SetCallBackList(request, Id(), http_messages2, ARRAY_SIZE(http_messages2));
		}
		if(OpStatus::IsError(op_err))
		{
			MoveRequestsToNewConnection(request_list.First(), TRUE);
			Stop();

			mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
			return;
		}

		if(request->GetWaiting() || !request->GetSentRequest())
		{
			SetProgressInformation(START_REQUEST,0, manager->HostName()->UniName());
			request->SetWaiting(FALSE);
			ComposeRequest();
		}
		else if(request->method != HTTP_METHOD_CONNECT)
		{
			SetProgressInformation(START_REQUEST,0, manager->HostName()->UniName());
		}
		//else
		//	RequestMoreData();
	}
	else if(item || (info.host_is_1_0 && !info.http_1_0_keep_alive) ||
		ProtocolComm::PendingClose() || urlManager->TooManyOpenConnections(manager->HostName()) ||
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NoConnectionKeepAlive) ||
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OfflineMode)
#ifdef _EMBEDDED_BYTEMOBILE
		|| (info.ebo_optimized && generic_info.disable_more_requests)
#endif // _EMBEDDED_BYTEMOBILE
		)
	{
#ifdef _DEBUG_CONN
	if(!request)
	{
#ifdef HTTP_CONNECTION_TIMERS
		StopIdleTimer();
#endif//HTTP_CONNECTION_TIMERS
		PrintfTofile("http1.txt","Stopping idle HTTP connection %d [%lu]\n", Id(), prefsManager->CurrentTime());
	}
#endif
#ifdef DEBUG_HTTP_REQDUMP
			PrintfTofile("winsck.txt","Terminating stopped HTTP connection %d\n", Id());
			OpString8 textbuf1;

			textbuf1.AppendFormat("http%04d.txt",Id());
			PrintfTofile(textbuf1,"Terminating stopped HTTP connection %d\n", Id());
#endif

		EndLoading();
		Stop();
		SetNewSink(NULL);
//		info.disable_more_requests = TRUE; // Set to TRUE by call to Stop() above

		mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(), 0);
	}
	else if(!generic_info.disable_more_requests
#ifdef _BYTEMOBILE
			&& urlManager->GetByteMobileOpt()
#endif
			)
	{
		// Try to get a new request
		manager->ForciblyMoveRequest(this);
	}
	else
	{
		pipelined_items.Clear(); // Completed list of requests
#ifdef HTTP_BENCHMARK
		last_finished = 0;
#endif
	}

}

unsigned int HTTP_1_1::GetUnsentRequestCount() const
{
	HTTP_Request_List *item;
	unsigned int unsent_count = 0;

	item = request_list.First();
	while(item)
	{
		HTTP_Request *req= item->request;

		if(req&& !req->GetSendingRequest() &&
			!req->GetSentRequest())
			unsent_count++;
		item =  item->Suc();
	}

	return unsent_count;
}

unsigned int HTTP_1_1::GetRequestCount() const
{
	HTTP_Request_List *item;
	unsigned int count = request_list.Cardinal();

	// Request list items with request==NULL will be at the end of the list
	// (However, I believe there can be more than one)
	for (item = request_list.Last(); item && !item->request; item = item->Suc())
		count--;

	return count;
}

BOOL HTTP_1_1::IsLegalHeader(const char *a_tmp_buf,OpFileLength a_cnt_len) // TRUE if legal, FALSE if not, handled internally if FALSE
{
	const char * OP_MEMORY_VAR tmp_buf = a_tmp_buf;
	OP_MEMORY_VAR OpFileLength cnt_len = a_cnt_len;

	if(tmp_buf == NULL)
		return TRUE; // Should never happen, lets assume it is OK

	while(cnt_len > 0 && (*tmp_buf == '\r' || *tmp_buf == '\n'))
	{
		tmp_buf++;
		cnt_len--;
	}

	if(cnt_len<5 || // If we get in here with content length less than 5 the connection is closing or aborting. No legal HTTP response can take less than 5 bytes
		(op_strncmp(tmp_buf, "HTTP", 4) != 0 && op_strnicmp(tmp_buf, "HTTP", 4) != 0) ||
			(tmp_buf[4] != ' ' && tmp_buf[4] == '\t') )
	{
		if((!info.first_request || (manager->GetHTTP_ProtocolDetermined() && manager->GetHTTP_1_1())) /* && info.received_legal_header // This is an invariant */ )
		{
			//OP_ASSERT(0); // The server has done something it should not have done
			// Server is sending illegal content. Abort and reschedule all requests
			generic_info.disable_more_requests = TRUE;
			HTTP_Request_List *start_req = request_list.First();
			BOOL continue_load = TRUE;

			if(request && request->sendcount < 5 &&
				!request->info.is_formsrequest &&
				GetMethodIsSafe(request->method))
			{
				continue_load = FALSE;
			}
			else
				start_req = (start_req ? start_req->Suc() : NULL);

			if(request && (request->info.pipeline_used_by_previous || request->info.sent_pipelined))
			{
				manager->IncrementPipelineProblem();
				SignalPipelineReload();
			}
			if(start_req)
				MoveRequestsToNewConnection( start_req , TRUE);
			// Closing connection handled by caller (request == NULL => request != current_req
			if(!continue_load || !request)
				return FALSE;
		}

		if (OpStatus::IsError(content_buf.AppendCopyData(tmp_buf, (size_t)cnt_len)))
			return FALSE;
		OP_ASSERT(cnt_len <= (OpFileLength)HTTP_TmpBufSize);
		request->SetHeaderLoaded(TRUE);
		header_info->response = HTTP_OK;

		header_info->info.received_close = TRUE;

		if (tmp_buf[4] != ' ' || tmp_buf[4] != '\t')
			header_info->info.http_10_or_more = FALSE;


		HTTP_Request_List *item;

		generic_info.disable_more_requests = TRUE;
		item = request_list.First();
#ifdef DEBUG_HTTP_REQDUMP
		OpString8 textbuf1;

		textbuf1.AppendFormat("http%04d.txt",Id());
		PrintfTofile(textbuf1,"HTTP connection %d : Received Close.\n", Id());
		{
			HTTP_Request_List *item1;
			item1 = request_list.First();
			while(item1)
			{
				if(item1->request)
					PrintfTofile(textbuf1,"   HTTP connection %d : Request %d.\n", Id(),item1->request->Id());
				else
					PrintfTofile(textbuf1,"   HTTP connection %d : Empty Request.\n", Id());
				item1 = item1->Suc();
			}
		}
#endif
		if(item && item->request == request)
			item = item->Suc();
		if(item && item->request)
			this->MoveRequestsToNewConnection(item);

		//mh->PostMessage(MSG_HTTP_HEADER_LOADED, Id(), 0);
		ProtocolComm::SetProgressInformation(HEADER_LOADED);
		return FALSE;
	}

	return TRUE;
}

BOOL HTTP_1_1::LoadHeaderL()
{

#ifdef DEBUG_HTTP_FILE
/*
FILE *tfp = fopen("c:\\klient\\winsck.txt","a");
fprintf(tfp,"   HTTP::LoadHeader() started, id: %d\n", Id());
fclose(tfp);
	*/
#endif

	if(GetHandlingHeader())
		return FALSE;

	//int max_buf_len = prefsManager->GetNetworkBufferSize() * 1024;
	unsigned long max_buf_len = (unsigned long)g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize) * 1024;
	unsigned long buf_len = (max_buf_len < HTTP_TmpBufSize) ? (unsigned long)max_buf_len : HTTP_TmpBufSize;

	char* tmp_buf = HTTP_TmpBuf;
	OpFileLength cnt_len;

	if(!content_buf.IsEmpty())
	{
		cnt_len = content_buf.Length();
		OP_ASSERT((OpFileLength)buf_len >= cnt_len);
		content_buf.CopyInto(tmp_buf, (size_t)cnt_len);
		content_buf.Clear();
	}
	else
	{
		if(info.read_source)
			return FALSE;

#ifdef DEBUG_HTTP_FILE
		PrintfTofile("http.txt","Reading for connection [%d] #3:\n", Id());
#endif
		cnt_len = ProtocolComm::ReadData(tmp_buf, buf_len);
		info.read_source = TRUE;
	}

#ifdef DEBUG_HTTP_HEXDUMP
	{
		OpString8 textbuf;

		textbuf.AppendFormat("Loadheader Reading data from %d (Request %d) Tick %lu",Id(), request->Id(), (unsigned long) g_op_time_info->GetWallClockMS());
		DumpTofile((unsigned char *) tmp_buf,(unsigned long) cnt_len,textbuf,"http.txt");

#ifdef DEBUG_HTTP_REQDUMP
		OpString8 textbuf1;

		textbuf1.AppendFormat("http%04d.txt",Id());
		DumpTofile((unsigned char *) tmp_buf,(unsigned long) cnt_len,textbuf,textbuf1);
		OpString8 textbuf2;

		textbuf2.AppendFormat("htpr%04d.txt",request->Id());
		DumpTofile((unsigned char *) tmp_buf,(unsigned long) cnt_len,textbuf,textbuf2);
#endif
	}
#endif

	tmp_buf[cnt_len] = '\0';

	OpFileLength hdr_len = 0;

	if (!header_str && cnt_len >= 5 && !IsLegalHeader(tmp_buf, cnt_len))
		return TRUE;

	unsigned k = 0;
	if (header_str)
	{
		unsigned len = op_strlen(header_str);
		//    if (len>3 && (header_str[len-1] == '\r' || header_str[len-1] == '\n'))
		if (len>6 && (header_str[len-1] == '\r' || header_str[len-1] == '\n'))
		{
			char *endhead = (char *) g_memory_manager->GetTempBuf2k();
			op_strcpy(endhead, &(header_str[len-3])); // Max length 3
			op_strncat(endhead, tmp_buf, (cnt_len>=3) ? 3 : (size_t) cnt_len); // Max length 3
			endhead[(cnt_len>=3) ? 6 : cnt_len+3] = '\0';
			char* tmp = op_strstr(endhead, "\n\n");
			if (tmp)
			{
				k = tmp-endhead - 3 + 2;
			}
			else
			{
				tmp = op_strstr(endhead, "\r\n\r\n");
				if (tmp)
					k = tmp-endhead - 3 + 4;
				else
				{
					tmp = op_strstr(endhead, "\n\r\n");
					if (tmp)
						k = tmp-endhead - 3 + 3;
				}
			}

			if (tmp)
				request->SetHeaderLoaded(TRUE);
		}
	}

	if (!request->GetHeaderLoaded())
	{
		if(!header_str && k == 0)
		{
			if(tmp_buf[0] == '\r' || tmp_buf[0] == '\n')
			{
				k++;
				while(k < cnt_len && (tmp_buf[k] == '\r' || tmp_buf[k] == '\n'))
				{
					k++;
				}

				// bypassing leading CR's and LF's sent by badly configured servers.
				tmp_buf += k;
				k = 0;
			}
		}
		while (k < cnt_len && !request->GetHeaderLoaded())
		{
			if (tmp_buf[k] == '\r')
			{
				if (k+3 < cnt_len && tmp_buf[k+1] == '\n' && tmp_buf[k+2] == '\r' && tmp_buf[k+3] == '\n') // '\r\n\r\n'
				{
					request->SetHeaderLoaded(TRUE);
					k +=3;
				}
			}
			else if (tmp_buf[k] == '\n')
			{
				if (k+1 < cnt_len && tmp_buf[k+1] == '\n') // '\n\n'
				{
					request->SetHeaderLoaded(TRUE);
					k +=1;
				}
				else if (k+2 < cnt_len && tmp_buf[k+1] == '\r' && tmp_buf[k+2] == '\n') // '\n\r\n'
				{
					request->SetHeaderLoaded(TRUE);
					k +=2;
				}
			}
			k++;
		}
		if(!request->GetHeaderLoaded() && k>0 && ProtocolComm::Closed())
			request->SetHeaderLoaded(TRUE);
	}

	if (request->GetHeaderLoaded() && k < cnt_len)
	{
		LEAVE_IF_ERROR(content_buf.AppendCopyData(tmp_buf+k, (size_t)cnt_len - k));
		hdr_len = k;
#ifdef DEBUG_HTTP_HEXDUMP
#if 0
		{
			OpString8 textbuf;

			textbuf.AppendFormat("Loadheader Saving Content from %d (Request %d) ",Id(), request->Id());
			DumpTofile((unsigned char *) content_buf.Data(),(unsigned long) content_buf.Length(),textbuf,"http.txt");

#ifdef DEBUG_HTTP_REQDUMP
			OpString8 textbuf1;

			textbuf1.AppendFormat("http%04d.txt",Id());
			DumpTofile((unsigned char *) content_buf.Data(),(unsigned long) content_buf.Length(),textbuf,textbuf1);
			OpString8 textbuf2;

			textbuf2.AppendFormat("htpr%04d.txt",request->Id());
			DumpTofile((unsigned char *) content_buf.Data(),(unsigned long) content_buf.Length(),textbuf,textbuf2);
#endif
		}
#endif
#endif
	}
	else
		hdr_len = cnt_len;

	if (hdr_len)
	{
		if (header_str)
		{
			OpFileLength len = op_strlen(header_str) + hdr_len;
			char *old_header = header_str;
			header_str = OP_NEWA_L(char, (size_t) len+1);
			op_strcpy(header_str, old_header);
			op_strncat(header_str, tmp_buf, (size_t) hdr_len);
			header_str[len] = '\0';
			OP_DELETEA(old_header);
		}
		else
		{
			//header_str = new char[hdr_len+1];
			//strncpy(header_str, tmp_buf, hdr_len);
			//header_str[hdr_len] = '\0';
			LEAVE_IF_ERROR(SetStr(header_str, tmp_buf, (size_t) hdr_len)); // 11/10/97 YNP
		}
	}


	if (request->GetHeaderLoaded())
	{
		SetHeaderInfoL();
		OP_DELETEA(header_str);
		header_str = 0;

#ifdef _DEBUG
		//this simulates an inline ebo proxy.
/*		if (!urlManager->GetEmbeddedBmOpt())
		{
			header_info->info.received_close = TRUE;
			urlManager->SetEmbeddedBmOpt(TRUE);
			urlManager->SetEmbeddedBmCompressed(TRUE);
			urlManager->SetEBOServer("125.206.200.59:9009", 0);
		}*/
#endif
		if(request == NULL)
			return FALSE;
		// *** 27/09/97 YNP ***
#ifdef SCOPE_RESOURCE_MANAGER
		HTTP_Request::LoadStatusData_Raw load_status_param(request, tmp_buf, (size_t)hdr_len);
		request->SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE_HEADER, &load_status_param);
#endif // SCOPE_RESOURCE_MANAGER
#ifdef OPERA_PERFORMANCE
		http_Manager->OnHeaderLoaded(request->GetRequestNumber(), header_info->response);
#endif // OPERA_PERFORMANCE
#if defined(_SSL_SUPPORT_) && !defined(_EXTERNAL_SSL_SUPPORT_)
		if (request->method == HTTP_METHOD_CONNECT && header_info->response == HTTP_OK)
		{
			request->info.proxy_connect_open = TRUE;
			ProtocolComm::ConnectionEstablished();

			if(request == NULL)
				return FALSE; // Connection closed

			ProtocolComm::SetProgressInformation(CONNECT_FINISHED);
			if(OpStatus::IsError(request->SetExternalHeaderInfo(NULL)))
			{
				Stop();
				mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				return FALSE;
			};
			ProtocolComm::RequestMoreData();
		}
		else
#elif defined(_SSL_SUPPORT_) && defined(_EXTERNAL_SSL_SUPPORT_)  && defined(_USE_HTTPS_PROXY) && defined(URL_ENABLE_INSERT_TLS)
		if (request->method == HTTP_METHOD_CONNECT && header_info->response == HTTP_OK)
		{
			request->info.proxy_connect_open = TRUE;
			ProtocolComm::InsertTLSConnection(request->request->origin_host, request->request->origin_port);
			ProtocolComm::SetProgressInformation(CONNECT_FINISHED);
			if(OpStatus::IsError(request->SetExternalHeaderInfo(NULL)))
			{
				Stop();
				mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				mh->PostMessage(MSG_COMM_LOADING_FAILED, Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
				return FALSE;
			};
		}
		else
#endif
			// ********************
		{
			//mh->PostMessage(MSG_HTTP_HEADER_LOADED, Id(), 0);
			//header_info->info.content_waiting = (content_length > 0 || header_info->info.received_close);
			if(sending_request && sending_request->info.send_expect_100_continue)
			{
				mh->UnsetCallBack(this,MSG_HTTP_FORCE_CONTINUE);
				mh->RemoveDelayedMessage(MSG_HTTP_FORCE_CONTINUE,request->Id(),0);
			}
			if(header_info->response >= HTTP_OK || header_info->response <100)
			{
				if(info.activated_100_continue_fallback)
				{
					mh->UnsetCallBack(this,MSG_HTTP_CONTINUE_FALLBACK);
					mh->RemoveDelayedMessage(MSG_HTTP_CONTINUE_FALLBACK,request->Id(),0);
					info.activated_100_continue_fallback = FALSE;
				}

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
				//if(!info.do_ntlm_authentication)
#endif
				{
#ifdef _SSL_USE_SMARTCARD_
					if(info.smart_card_authenticated)
					{
						ProtocolComm::SetProgressInformation(SMARTCARD_AUTHENTICATED);
					}
#endif

					ProtocolComm::SetProgressInformation(HEADER_LOADED);



				}
			}
			else
			{
				if(header_info->response == HTTP_CONTINUE)
				{
					if(GetMethodHasRequestBody(request->method) &&
						!request->info.send_expect_100_continue && !info.activated_100_continue_fallback)
					{
						// YNP: OOM: Failure here will only cause a timeout warning to be lost
						// This warning message is not all that important, and will not be displayed that often
						OpStatus::Ignore(mh->SetCallBack(this,MSG_HTTP_CONTINUE_FALLBACK,request->Id()));
						mh->PostDelayedMessage(MSG_HTTP_CONTINUE_FALLBACK,request->Id(),0,40000);
						info.activated_100_continue_fallback = TRUE;
						ProtocolComm::SetProgressInformation(STARTED_40SECOND_TIMEOUT,0,NULL);
					}
					if(sending_request && sending_request->info.send_expect_100_continue)
						sending_request->info.send_100c_body = TRUE;
				}

				Clear();
				RequestMoreData();
			}
		}

			//header_info->info.content_waiting = FALSE;

#ifdef DEBUG_HTTP_FILE
			/*
			FILE *tfp = fopen("c:\\klient\\winsck.txt","a");
			fprintf(tfp,"   HTTP::LoadHeader() - header loaded, id: %d\n", Id());
			fclose(tfp);
		*/
#endif
#ifdef DEBUG_HTTP_HEXDUMP

		PrintfTofile("http.txt","   HTTP::LoadHeader() - header loaded, id: %d (Request %d) \n", Id(), request ? request->Id() : 0);
#endif
	}
#ifdef DEBUG_HTTP_FILE
	/*
	else
	{
    FILE *tfp = fopen("c:\\klient\\winsck.txt","a");
    fprintf(tfp,"   HTTP::LoadHeader() - header NOT finished, id: %d\n", Id());
    fclose(tfp);
	}
	*/
#endif

	return request && request->GetHeaderLoaded() && (!content_buf.IsEmpty() ||
		(header_info->info.received_content_length && header_info->content_length == 0) ||
		header_info->response == HTTP_NOT_MODIFIED);
}

//***************************************************************************

void HTTP_1_1::ProcessReceivedData()
{
	OP_MEMORY_VAR OP_STATUS op_err = OpStatus::OK;
#ifdef DEBUG_HTTP_FILE
	PrintfTofile("winsck.txt","HTTP_1_1::ProcessReceivedData() HTTP 1.1 Receive Data entry: %d\n", Id());
#endif
	info.read_source = FALSE;
#ifdef DEBUG_HTTP_FILE
	PrintfTofile("http.txt","Entering ProcessRD for HTTP connection [%d] :\n", Id());
#endif
	while(1)
	{
		if(request == NULL)
		{
			Stop();
			mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);
			info.read_source = FALSE;
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Exiting ProcessRD for HTTP connection [%d] #1:\n", Id());
#endif
			return;
		}

		last_active = g_timecache->CurrentTime();
#ifdef HTTP_BENCHMARK
		BenchMarkLastActive();
#endif

#ifdef HTTP_CONNECTION_TIMERS
		StopResponseTimer();
		StartIdleTimer();
/*		m_response_timer->Stop();
		UINT http_idle_timeout = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HTTPIdleTimeout);
		if( http_idle_timeout )
			m_idle_timer->Start(http_idle_timeout*1000);*/
#endif//HTTP_CONNECTION_TIMERS

		HTTP_Request * OP_MEMORY_VAR current_req = request;
		OP_MEMORY_VAR BOOL hdr_loaded = FALSE;
		if (!(hdr_loaded = request->GetHeaderLoaded()))
			TRAP(op_err, hdr_loaded = LoadHeaderL());
#ifdef _EMBEDDED_BYTEMOBILE
		if(hdr_loaded && request && request->info.bypass_ebo_proxy)
		{
			current_req = request;
			mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), ERR_COMM_RELOAD_DIRECT);
		}
		else if (hdr_loaded && request && urlManager->GetEmbeddedBmOpt())
			current_req = request;
#endif //_EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
		if( generic_info.turbo_enabled && hdr_loaded && request )
		{
			if( info.turbo_unhandled_response )
			{
				/**
				 * Response received does not belong to request handled by this connection.
				 * Request may have been removed due to call to URL::StopLoading().
				 * If we can expect to receive large amounts of content with the unhandled
				 * request we should abort by closing the connection.
				 */
				UINT32 max_size = g_memory_manager->GetTempBuf2kLen();
				if( !header_info->info.received_content_length ||
					header_info->content_length > max_size ||
					header_info->turbo_transferred_bytes > max_size	)
				{
					current_req = NULL;
				}
			}
			else
			{
				if( request->info.bypass_turbo_proxy )
				{
					current_req = request;
					mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), ERR_COMM_RELOAD_DIRECT);
				}
				else if( request->request->use_turbo && urlManager->GetWebTurboAvailable() )
				{
					current_req = request;
				}

				if( info.turbo_proxy_auth_retry )
				{
					// Re-send the first request (hopefully with the correct auth)
					if( request->sendcount < 5 )
					{
						request->Clear();
						// Make sure POST data gets re-sent
						if( request->HasPayload() )
						{
							TRAPD(err,request->ResetL());
							OpStatus::Ignore(err);
						}
					}
				}
			}
		}
#endif // WEB_TURBO_MODE
		if(OpStatus::IsError(op_err))
		{
			g_memory_manager->RaiseCondition(op_err);
			Stop();
			if(request) mh->PostMessage(MSG_COMM_LOADING_FAILED,request->Id(),URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			mh->PostMessage(MSG_COMM_LOADING_FAILED,Id(),URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
			info.read_source = FALSE;
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Exiting ProcessRD for HTTP connection [%d] #abort:\n", Id());
#endif
			return;
		}
		if (hdr_loaded)
		{
			if(request != current_req)
			{
#ifdef DEBUG_HTTP_REQDUMP
			PrintfTofile("winsck.txt","Terminating out of sync HTTP connection %d\n", Id());
			OpString8 textbuf1;

			textbuf1.AppendFormat("http%04d.txt",Id());
			PrintfTofile(textbuf1,"Terminating out of sync HTTP connection %d\n", Id());
#endif
#ifdef WEB_TURBO_MODE
				if(generic_info.turbo_enabled)
					MoveRequestsToNewConnection(NULL);
#endif // WEB_TURBO_MODE
				EndLoading();
				Stop();
				SetNewSink(NULL);

				mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);
				info.read_source = FALSE;
#ifdef DEBUG_HTTP_FILE
				PrintfTofile("http.txt","Exiting ProcessRD for HTTP connection [%d] #2:\n", Id());
#endif
				return;
			}

			prev_response = header_info->response;
			if(request->GetMethod() == HTTP_METHOD_HEAD || header_info->response == HTTP_NOT_MODIFIED ||
				(header_info->info.received_content_length && header_info->content_length == 0))
			{
#ifdef WEB_TURBO_MODE
				if( generic_info.turbo_enabled && info.turbo_unhandled_response )
					current_req = NULL;
				else
#endif // WEB_TURBO_MODE
				ProtocolComm::SetProgressInformation(REQUEST_FINISHED,0,0);
				//mh->PostMessage(MSG_COMM_LOADING_FINISHED,request->Id(),0);
				MoveToNextRequest();
				if(request && !content_buf.IsEmpty() && request != current_req)
					continue;
				if(!request && !content_buf.IsEmpty())
				{
					// Error. Content after a 304 is not permitted (in this case there is no current request
					EndLoading();
					Stop();
					SetNewSink(NULL);

					mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(),0);

				}
				info.read_source = FALSE;
#ifdef DEBUG_HTTP_FILE
				PrintfTofile("http.txt","Exiting ProcessRD for HTTP connection [%d] #3:\n", Id());
#endif
				return;
			}

			if(request->GetMethod() == HTTP_METHOD_CONNECT)
			{
				if(request->info.proxy_connect_open)
				{
					ProtocolComm::ProcessReceivedData();
				}
				else
					ProtocolComm::SetProgressInformation(HANDLE_SECONDARY_DATA,0,0);

				info.read_source = FALSE;
#ifdef DEBUG_HTTP_FILE
				PrintfTofile("http.txt","Exiting ProcessRD for HTTP connection [%d] #4:\n", Id());
#endif
				return;
			}


			HTTP_Request_List *item = request_list.First()->Suc();
			if(item && item->request)
			{
				if(header_info->info.received_close)
					MoveRequestsToNewConnection(item);
				else
					if(/*(info.host_is_1_0 || info.http_1_1_pipelined) && */
						request->GetSentRequest() && item->request->GetWaiting() &&
						!request->GetForceWaiting())
					{
						item->request->SetWaiting(FALSE);
						RequestMoreData();
					}
			}

#ifdef WEB_TURBO_MODE
			if( generic_info.turbo_enabled && info.turbo_unhandled_response )
			{
				// Discard data
				char *tmpBuf = (char *) g_memory_manager->GetTempBuf2k();
				ReadData(tmpBuf,g_memory_manager->GetTempBuf2kLen());
			}
			else
#endif // WEB_TURBO_MODE
			ProtocolComm::ProcessReceivedData();
#ifdef DEBUG_HTTP_FILE
			/*
			FILE *tfp = fopen("c:\\klient\\winsck.txt","a");
			fprintf(tfp,"   HTTP::HandleDataReady() - PostMessage: MSG_COMM_DATA_READY for id: %d\n", Id());
			fclose(tfp);
			*/
#endif
		}

#ifdef DEBUG_HTTP_FILE
    	PrintfTofile("http.txt","End of request loop for HTTP connection [%d] (Content rest %p: Request %p : Current request %p : Header loaded %s)\n", Id(),
            content_buf.Data(), request, current_req, (request && request->GetHeaderLoaded()? "True" : "False") );
#endif
		if(!content_buf.IsEmpty() && (request != current_req || (request && !request->GetHeaderLoaded())))
			continue;
		break;
	}

	if(ProtocolComm::Closed() && content_buf.IsEmpty() && !request_list.Empty())
	{
		HandleEndOfConnection();
	}
#ifdef DEBUG_HTTP_FILE
	PrintfTofile("http.txt","Exiting ProcessRD for HTTP connection [%d] #6:\n", Id());
#endif

	info.read_source = FALSE;

	if(request && !(request->GetSentRequest() || request->GetSendingRequest()))
		RequestMoreData();
}

//***************************************************************************

BOOL HTTP_1_1::MoveRequestsToNewConnection(HTTP_Request_List *item, BOOL forced)
{
	BOOL ret = FALSE;
	generic_info.disable_more_requests = TRUE;
	/*
	if(item == NULL || item->request == NULL)
		return ret;
		*/

	if(item == NULL)
		item = request_list.First();

#ifdef DEBUG_HTTP_REQDUMP
	if(item && item->request)
	{
	PrintfTofile("httpq.txt","Removing Requests from HTTP connection %d :"
		" Starting with %d\n", Id(), item->request->Id());
	OpString8 textbuf1;

	textbuf1.AppendFormat("http%04d.txt",Id());
	PrintfTofile(textbuf1,"Removing Requests from HTTP connection %d :"
		" Starting with %d\n", Id(), item->request->Id());
	}
#endif
#ifdef DEBUG_HTTP_REQDUMP1
	if(item && item->request)
	{
	PrintfTofile("httpreq.txt","Removing Requests from HTTP connection %d :"
		" Starting with %d\n", Id(), item->request->Id());
	}
#endif


	HTTP_Request_List *current_item = item;

	while(current_item)
	{
		BOOL already_sent = FALSE;

		if(current_item->request != NULL)
		{
			if(current_item->request == request)
			{
				mh->RemoveCallBacks(request, Id());
				ChangeParent(NULL);
				request->PopSink();
				request = NULL;
			}
			if(current_item->request == sending_request)
			{
				sending_request = NULL;
			}

#ifdef DEBUG_HTTP_REQDUMP
			PrintfTofile("httpq.txt","Removing Request %d from HTTP connection %d\n",
				current_item->request->Id() , Id());
			OpString8 textbuf1;

			textbuf1.AppendFormat("http%04d.txt",Id());
			PrintfTofile(textbuf1,"Removing Request %d from HTTP connection %d\n",
				current_item->request->Id() , Id());
			OpString8 textbuf2;

			textbuf2.AppendFormat("htpr%04d.txt",current_item->request->Id());
			PrintfTofile(textbuf2,"Removing Request %d from HTTP connection %d\n",
				current_item->request->Id() , Id());
#endif
			if(current_item->request->info.sending_request ||
				current_item->request->info.sent_request)
				already_sent = TRUE;

			if(already_sent && !current_item->request->info.header_loaded &&
					!GetMethodIsSafe(current_item->request->method))
			{
				mh->PostMessage(MSG_COMM_LOADING_FAILED, current_item->request->Id(), URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED));
#if 0
				Log("HTTP1: Move to new ");
				Log(Id());
				Log(" : ");
				Log(current_item->request->request->origin_host->Name());
				Log(" : ");
				LogNl(current_item->request->request->request);
#endif
			}
			else
			{
				if(forced && already_sent && !current_item->request->info.header_loaded)
					current_item->request->SetForceWaiting(TRUE);
				HTTP_Request *req1 = current_item->request;
				current_item->request = NULL;
				NormalCallCount blocker1(req1);
				NormalCallCount blocker2(this);
				req1->PopSink();
				if(info.restarting_requests
#ifdef _EMBEDDED_BYTEMOBILE
						|| (info.ebo_optimized && !info.pending_reqs_to_new_ebo_conn)
#endif //_EMBEDDED_BYTEMOBILE
					)
				{
					if(request == req1)
					{
						HTTP_Request_List *item1 = current_item->Suc();
						request = (item1 != NULL ? item1->request : NULL);
						header_info = (request ? request->header_info : selfheader_info);
						ChangeParent(request);
					}
					req1->Clear();
					req1->SetWaiting(FALSE);
					if (GetParent() == req1)
					{
						ChangeParent(NULL);
					}
					req1->http_conn = NULL;
					if(info.restarting_requests)
						mh->PostMessage(MSG_COMM_LOADING_FAILED, req1->Id(), URL_ERRSTR(SI, ERR_AUTO_PROXY_CONFIG_FAILED));
#ifdef _EMBEDDED_BYTEMOBILE
					else
						mh->PostMessage(MSG_COMM_LOADING_FAILED, req1->Id(), ERR_COMM_RELOAD_DIRECT);
#endif //_EMBEDDED_BYTEMOBILE
				}
				else
				{
					req1->mh->RemoveCallBacks(req1, Id());
					if(manager->AddRequest(req1) == COMM_REQUEST_FAILED)
					{
						req1->EndLoading();
						mh->PostMessage(MSG_COMM_LOADING_FAILED, req1->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
					}
				}
				//current_item->request->requeued = (current_item->request->requeued << 2) | 0x01;
			}
			ret = TRUE;
		}

		HTTP_Request_List *next_item = current_item->Suc();

		current_item->request= NULL;

		if( current_item != item || !already_sent)
		{
			OP_DELETE(current_item);
		}

		current_item = next_item;
	}

	return ret;
}

HTTP_Request *HTTP_1_1::MoveLastRequestToANewConnection()
{
	HTTP_Request_List *item = request_list.First();
	HTTP_Request *req = NULL;

	while(item)
	{
		req = item->request;

		HTTP_Request_List *item1 = item->Suc();

		if(req&& 
			GetMethodIsSafe(req->method) &&
			/*
			req->method != HTTP_METHOD_CONNECT &&
			req->method != HTTP_METHOD_POST &&
			req->method != HTTP_METHOD_PUT &&
			req->method != HTTP_METHOD_DELETE &&
			*/
			!req->GetSendingRequest() &&
			!req->GetSentRequest()&&
            (item->Pred() != NULL || (item1 && item1->request) || this->info.connection_active) // DO NOT reschedule the first request of a connection,
                // unless there are more requests in the pipeline, because doing so might cause the connection to hang on the serverside
            )
		{
			if(req->GetWaiting() && item1 && item1->request)
				item1->request->SetWaiting(TRUE);
			item->request = NULL;
			OP_DELETE(item);
			req->PopSink();

			if(request == req)
			{
				request = (item1 != NULL ? item1->request : NULL);
				header_info = (request ? request->header_info : selfheader_info);
				ChangeParent(request);
				if(request)
					request->SetWaiting(FALSE);
			}

#ifdef DEBUG_HTTP_REQDUMP1
			PrintfTofile("httpreq.txt","Forcing unsent Request %d from Connection %d Tick %lu\n",
				req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
			PrintfTofile("http.txt","Forcing unsent Request %d from Connection %d Tick %lu\n",
				req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
#ifdef DEBUG_HTTP_REQDUMP
            OpString8 textbuf1;

			textbuf1.AppendFormat("htpr%04d.txt",req->Id());
            PrintfTofile(textbuf1,"MoveLastRequestToANewConnection: Forcing unsent Request %d from Connection %d Tick %lu\n",
                req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
            OpString8 textbuf2;

			textbuf2.AppendFormat("http%04d.txt",Id());
            PrintfTofile(textbuf2,"MoveLastRequestToANewConnection: Forcing unsent Request %d from Connection %d Tick %lu\n",
                req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
#endif //DEBUG_HTTP_REQDUMP
#endif //DEBUG_HTTP_REQDUMP1
			req->mh->RemoveCallBacks(req, Id());
			req->Clear();
			req->SetWaiting(FALSE);
			req->http_conn = NULL;

			return req;
		}

		item =  item1;
	}

#ifdef WEB_TURBO_MODE
	// Do not force sent Turbo Mode requests to new connections
	if( generic_info.turbo_enabled )
		return NULL;
#endif // WEB_TURBO_MODE

	item = request_list.Last();

	if(item && item->request == NULL)
		item = item->Pred();

	if (item == NULL || item->request == NULL || item->Pred() == NULL || item->request == sending_request)
		return NULL;

	req = item->request;

	// DO not reassign a posted form or POST request
	if(req->GetIsFormsRequest() ||
		!GetMethodIsSafe(req->method))
		return NULL;

	item->request = NULL;
	req->PopSink();

	if(item->Suc() != NULL || !req->GetSentRequest())
	{
		OP_DELETE(item);
	}

#ifdef DEBUG_HTTP_REQDUMP1
	PrintfTofile("httpreq.txt","Forcing sent Request %d from Connection %d Tick %lu\n",
		req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
	PrintfTofile("http.txt","Forcing sent Request %d from Connection %d Tick %lu\n",
		req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
#endif
	req->Clear();
	req->SetWaiting(FALSE);
	req->http_conn = NULL;

	return req;
}

void HTTP_1_1::EndLoading()
{

#ifdef DEBUG
	//OP_ASSERT(request_list.Cardinal() == 0);
#endif

	generic_info.disable_more_requests = TRUE;
#ifdef DEBUG_HTTP_FILE
		//PrintfTofile("winsck.txt","   HTTP::EndLoading() id: %d\n", Id());
#endif
	if (request && !request->GetHeaderLoaded() && header_str)
	{
		if(IsLegalHeader(header_str, op_strlen(header_str)))
		{
			request->SetHeaderLoaded(TRUE);
			TRAPD(op_err, SetHeaderInfoL());
		}

#ifdef DEBUG_HTTP_FILE
		/*
		FILE *tfp = fopen("c:\\klient\\winsck.txt","a");
		fprintf(tfp,"   HTTP::EndLoading() - header loaded, id: %d\n", Id());
		fclose(tfp);
		*/
#endif
	}
	RemoveRequest(request);
}

void HTTP_1_1::Stop()
{
	NormalCallCount blocker(this);
#ifdef HTTP_CONNECTION_TIMERS
	StopIdleTimer();
#endif//HTTP_CONNECTION_TIMERS
	EndLoading();
	ProtocolComm::CloseConnection();
	ProtocolComm::Stop();
}

void HTTP_1_1::RestartRequests()
{
	info.restarting_requests = TRUE;

	if(request)
	{
		ProtocolComm::SetProgressInformation(RESTART_LOADING,0,NULL);
		request = NULL;
		sending_request = NULL;
	}

	MoveRequestsToNewConnection(NULL, TRUE);
	Stop();

	info.restarting_requests = FALSE;
}

BOOL HTTP_1_1::IsActiveConnection() const
{
	HTTP_Request_List *req = request_list.First();
	return (!request_list.Empty() && req->request &&
		(req->request->GetSentRequest() || req->request->GetSendingRequest()));
}

BOOL HTTP_1_1::IsNeededConnection() const
{
	return !request_list.Empty() && request;
}

BOOL HTTP_1_1::Idle()
{
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	BOOL opt = FALSE;
#ifdef _EMBEDDED_BYTEMOBILE
	opt = urlManager->GetEmbeddedBmOpt();
#endif // _EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
	opt = opt || generic_info.turbo_enabled;
#endif // WEB_TURBO_MODE

	if (opt && !generic_info.load_direct && AcceptNewRequests())
	{
		time_t lst_active = LastActive();
		if(lst_active && lst_active + CHECK_IDLE_CONNECTION_DELAY*2 > g_timecache->CurrentTime() )
			return FALSE;
	}
#endif //_EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE

	return (info.connection_established && (request_list.First() == NULL && !ProtocolComm::PendingClose()/* ||
		request_list.First()->request == NULL*/)
		&& AcceptNewRequests());
}

BOOL HTTP_1_1::AcceptNewRequests()
{
	if( generic_info.disable_more_requests || ProtocolComm::Closed() || ProtocolComm::PendingClose() ||
		(info.host_is_1_0 && !info.http_1_0_keep_alive && !manager->GetHTTP_1_0_UsedConnectionHeader()) ||
		(http_10_ka_max && http_10_ka_max>= request_count))
		return FALSE;

	return !request_list.Last() || request_list.Last()->request;
}

BOOL HTTP_1_1::HasRequests()
{
	HTTP_Request_List *req_item = request_list.First();

	while(req_item)
	{
		if (req_item->request)
			return TRUE;
		req_item = req_item->Suc();
	}
	return FALSE;
}

BOOL HTTP_1_1::HasPriorityRequest(UINT priority)
{
	HTTP_Request_List *req_item = request_list.First();

	while(req_item)
	{
		if (req_item->request && req_item->request->GetPriority() == (UINT)priority)
			return TRUE;
		req_item = req_item->Suc();
	}
	return FALSE;
}

void HTTP_1_1::SetHTTP_1_1_flags(BOOL http_1_1, BOOL http_prot_det, BOOL http_10ka,
								 BOOL http_continue, BOOL http_chunking, BOOL http_11p, BOOL http_11tp)
{
	info.host_is_1_0 = !http_1_1;
	info.http_protocol_determined = http_prot_det;
	info.continue_allowed = http_continue;
	info.use_chunking = http_chunking;
	info.http_1_0_keep_alive= http_10ka;
	info.http_1_1_pipelined = http_11p;
	info.tested_http_1_1_pipelinablity = http_11tp;
}

void HTTP_1_1::RemoveRequest(HTTP_Request *req)
{
	if (req == NULL)
		return;

	info.request_list_updated = TRUE;

	HTTP_Request_List *req_item = request_list.First();

	while(req_item)
	{
		if(req_item->request == req)
		{
#ifdef DEBUG_HTTP_REQDUMP
			OpString8 textbuf1;

			textbuf1.AppendFormat("http%04d.txt",Id());
			PrintfTofile(textbuf1,"HTTP connection %d : Removing request %d.\n", Id(),req->Id());
#endif
			if(req->info.sending_request ||
				req->info.sent_request)
			{

				generic_info.disable_more_requests = TRUE;
				sending_request = NULL;
				HTTP_Request_List *req_item2 = req_item->Suc();
				HTTP_Request *req2;
				while(req_item2)
				{
					req2 = req_item2->request;
					HTTP_Request_List *temp_req_item = req_item2;
					req_item2 = req_item2->Suc();
					OP_DELETE(temp_req_item);
					if(req2)
					{
						req2->mh->RemoveCallBacks(req2, Id());
						if(GetParent() == req2)
							ChangeParent(NULL);
						req2->http_conn = NULL;
					}
					//req2->requeued = (req2->requeued << 2) | 0x02;
					manager->AddRequest(req2);//FIXME:OOM-yngve AddRequest might fail due to OOM, how should it be handled?
				}
			}
#ifdef _DEBUG
			else
			{
				OP_ASSERT(req != sending_request);
			}
#endif
			req_item->request = NULL;

			if(request == req/* && info.disable_more_requests*/)
			{
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
# ifdef _EMBEDDED_BYTEMOBILE
				if (!generic_info.load_direct && (urlManager->GetEmbeddedBmOpt()
#  ifdef WEB_TURBO_MODE
					|| generic_info.turbo_enabled
#  endif
					))
# else
				if (!generic_info.load_direct && generic_info.turbo_enabled)
# endif

				{
					request->LoadingFinished(FALSE);
					mh->RemoveCallBacks(request, Id());
					request->PopSink();

					OP_DELETE(req_item);
					BOOL request_sent = request->info.sending_request || request->info.sent_request;
					request = (!request_sent && request_list.Cardinal()) ? request_list.First()->request : NULL;
					ChangeParent(request);
				}
				else
#endif //_EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
				{
					request->LoadingFinished(FALSE);
					ChangeParent(NULL);
					mh->RemoveCallBacks(request, Id());
					request->PopSink();
					mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);

					request = NULL;
					header_info = NULL;
					EndLoading();
					HTTP_Request_List *item = req_item->Suc();
					if (item)
						MoveRequestsToNewConnection(item);
					Stop();
					SetNewSink(NULL);
				}
			}
			else if(!generic_info.disable_more_requests)
			{
				OP_DELETE(req_item);
			}

			OP_ASSERT(GetParent() != req);

			break;
		}

		req_item = req_item->Suc();
	}
}

CommState HTTP_1_1::Load()
{
	if(request)
		request->connectcount++;
	info.connection_failed = FALSE;

	CommState load_state = ProtocolComm::Load();
	if (load_state == COMM_REQUEST_FAILED)
		info.connection_failed = TRUE;

	if (request && !info.connection_failed)
		ProtocolComm::SetMaxTCPConnectionEstablishedTimeout(request->m_tcp_connection_established_timout);

	return load_state;
}

//OOM Can return FALSE due to OOM, should it be signalled?
BOOL HTTP_1_1::AddRequest(HTTP_Request *req, BOOL force_first /*= FALSE*/)
{
	if (req == NULL)
		return FALSE;
#ifdef DEBUG_HTTP_REQDUMP1
	PrintfTofile("httpreq.txt","Adding Request %d to Connection %d Tick %lu\n",
		req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
	PrintfTofile("http.txt","Adding Request %d to Connection %d Tick %lu\n",
		req->Id(), Id(), (unsigned long) g_op_time_info->GetWallClockMS());
#endif

	if (sending_request && force_first)
	{
		OP_ASSERT(!"Only the OptimizeRequestOrder code should use force_first.");
		return FALSE;
	}

	HTTP_Request_List *req_item = request_list.Last();
	if (req_item && !req_item->request)
		return FALSE;

	req_item = OP_NEW(HTTP_Request_List, ());

	if(req_item == NULL)
		return FALSE;

	req_item->request = req;

	if (force_first)
		req_item->IntoStart(&request_list);
	else
		req_item->Into(&request_list);


	HTTP_Request_List *pref_req;
	pref_req = req_item->Pred();

	if(pref_req ==  NULL)
	{

		OP_STATUS op_err;
        op_err = mh->SetCallBackList(req, Id(), http_messages, ARRAY_SIZE(http_messages));
		if(req->method == HTTP_METHOD_CONNECT && OpStatus::IsSuccess(op_err))
		{
	        op_err = mh->SetCallBackList(req, Id(), http_messages2, ARRAY_SIZE(http_messages2));
		}
		if(OpStatus::IsError(op_err))
		{
			mh->RemoveCallBacks(req, Id());
			OP_DELETE(req_item);
			return FALSE;
		}

		request = req;
		header_info = req->header_info;
		req->SetNewSink(this);

		if (!force_first)
			Clear();

#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		if(info.first_request &&
			(req->server_negotiate_auth_element ||
				req->proxy_ntlm_auth_element))
		{
			req->info.do_ntlm_authentication = TRUE;

			if(req->server_negotiate_auth_element && !req->info.ntlm_updated_server)
			{
				req->AccessHeaders().ClearHeader("Authorization");

				OpString8 auth_str;
				ANCHOR(OpStringS8, auth_str);

				AuthElm *auth= req->server_negotiate_auth_element;

				auth->ClearAuth();

				if(OpStatus::IsSuccess(auth->GetAuthString(auth_str, NULL, req)))
				{
					req->SetAuthorization(auth_str);
					req->SetAuthorizationId(auth->GetId());
				}
			}

			if(req->proxy_ntlm_auth_element && !req->info.ntlm_updated_proxy)
			{
				req->AccessHeaders().ClearHeader("Proxy-Authorization");

				OpString8 auth_str;
				ANCHOR(OpStringS8, auth_str);

				AuthElm *auth= req->proxy_ntlm_auth_element;

				auth->ClearAuth();

				if(OpStatus::IsSuccess(auth->GetAuthString(auth_str, NULL, req)))
				{
					req->SetProxyAuthorization(auth_str);
					req->SetProxyAuthorizationId(auth->GetId());
				}
			}

		}
		req->info.ntlm_updated_proxy = FALSE;
		req->info.ntlm_updated_server= FALSE;
#endif
		ChangeParent(req);
	}

	request_count++;

	req->http_conn = this;

	if(req->info.send_close)
		generic_info.disable_more_requests = TRUE;

	// Non-idempotent methods (currently only POST) do not pipeline except
	// when the previous request has received its header

	if(pref_req && pref_req->request &&
		(!GetMethodIsSafe(req->GetMethod()) ||
		/*(req->GetMethod() == HTTP_METHOD_POST ||
		req->GetMethod() == HTTP_METHOD_PUT ||
		req->GetMethod() == HTTP_METHOD_DELETE ||*/
		(
#ifdef _EMBEDDED_BYTEMOBILE
		(!req->info.proxy_request || !urlManager->GetEmbeddedBmOpt()) && 
#endif //_EMBEDDED_BYTEMOBILE
		info.http_1_0_keep_alive) ||
		(info.first_request && (!info.http_protocol_determined || info.host_is_1_0) && pref_req->Pred() == NULL) ||
		(info.http_protocol_determined && !info.host_is_1_0 && !info.http_1_1_pipelined) ||
		pref_req->request->GetForceWaiting() ||
		(req->request->request.HasContent() && req->GetIsFormsRequest() /*strchr(req->request->request,'?') != NULL*/) ||
		// also force wait for next request
		!GetMethodIsSafe(pref_req->request->GetMethod()) || 
		/*pref_req->request->GetMethod() == HTTP_METHOD_POST ||
		pref_req->request->GetMethod() == HTTP_METHOD_PUT ||
		pref_req->request->GetMethod() == HTTP_METHOD_DELETE ||*/
		(pref_req->request->request->request.HasContent() && req->GetIsFormsRequest()/*strchr(pref_req->request->request->request,'?') != NULL*/)
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		|| pref_req->request->info.do_ntlm_authentication
#endif
		))
	{
		if(!pref_req->request->GetHeaderLoaded())
			req->SetWaiting(TRUE);
	}

	if(req->info.proxy_request && !(g_pcnet->GetIntegerPref(PrefsCollectionNetwork::EnableHTTP11ForProxy)
#ifdef _EMBEDDED_BYTEMOBILE
		 || urlManager->GetEmbeddedBmOpt()
#endif
#ifdef WEB_TURBO_MODE
		 || (req->request->use_turbo && !req->request->use_proxy_passthrough)
#endif
		)
#if defined(_SSL_SUPPORT_) || defined(_NATIVE_SSL_SUPPORT_) || defined(_CERTICOM_SSL_SUPPORT_)
		&& !req->info.secure
#endif
#ifdef _SUPPORT_PROXY_NTLM_AUTH_
		&& !req->proxy_ntlm_auth_element
#endif
		)
		SetNoMoreRequests();

	if (!force_first)
		OptimizeRequestOrder();

#ifdef WEB_TURBO_MODE
	if( req->request->use_turbo && req->request->use_proxy_passthrough )
		req->SetProxyNoCache(TRUE);
#endif //WEB_TURBO_MODE

	if(!Closed() && info.connection_active &&
		(!req->GetWaiting() || !pref_req || !pref_req->request || !pref_req->request->GetSentRequest()))
		RequestMoreData();

	generic_info.requests_sent_on_connection = TRUE;
	return TRUE;
}

void HTTP_1_1::OptimizeRequestOrder()
{
	HTTP_Request_List *req_item = request_list.Last();
	HTTP_Request_List *move_after = NULL;

	HTTP_Request_List *current_item = req_item->Pred();
	while(current_item && current_item->request && 
		!current_item->request->GetSendingRequest() &&
		!current_item->request->GetForceWaiting() &&
		!current_item->request->GetSentRequest() && 
		current_item->request->method != HTTP_METHOD_CONNECT &&
		!Closed() 
		)
	{
		if (current_item->Pred())
		{
			int curr_prio = current_item->request->GetPriority();
			int new_prio = req_item->request->GetPriority();
			if (curr_prio <= new_prio)
			{
				move_after = current_item;
				break;
			}
		}
		else
		{
			//Move before and handle all config of first request.
			if (current_item->request->GetPriority() < req_item->request->GetPriority())
			{
				req_item->Out();
				request_count--;

				current_item->request->mh->RemoveCallBacks(current_item->request, Id());
				current_item->request->PopSink();
				req_item->request->info.waiting = 0;
				AddRequest(req_item->request, TRUE);
				OP_DELETE(req_item);
				return;
			}
		}
		current_item = current_item->Pred();
	}

	if (move_after)
	{
		req_item->Out();
		req_item->Follow(move_after);
	}
}

#ifdef URL_TURBO_MODE_HEADER_REDUCTION
void HTTP_1_1::ReduceHeadersL(Header_List &hdrList)
{
	OP_NEW_DBG("ReduceHeaders","header_reduction");
	Header_Item* keep_item;
	Header_Item *ask_item = hdrList.First();
	if( !ask_item  )
		return;

	OP_DBG(("HTTP_1_1 connection %d on %s",Id(),(manager ? manager->HostName()->Name() : "None")));

	uint32 ask_item_len = 0;
	BOOL ask_item_enabled = FALSE;
	OpStackAutoPtr<Header_List> temp_list(OP_NEW_L(Header_List, ()));

	if( !m_common_headers )
		m_common_headers = OP_NEW_L(Header_List, ());

	while( (ask_item = ask_item->Suc()) != NULL ) // Yes, we should ignore the very first item. It is not really a header
	{
		if ( op_stricmp(ask_item->GetName().CStr(), "host") == 0)
			continue; // some transparent proxy servers do not appreciate requests without the host header (violating rfc2616, see CORE-43918)
		ask_item_len = ask_item->CalculateLength();
		ask_item_enabled = ask_item->IsEnabled() && ask_item_len > (unsigned int)(ask_item->GetName().Length()+2);

		OP_DBG(("Checking header %s: enabled=%d, len=%d",ask_item->GetName().CStr(),ask_item->IsEnabled(),ask_item_len));

		keep_item = m_common_headers->FindHeader(ask_item->GetName(),FALSE);
		if( !keep_item )
		{
			OpStackAutoPtr<Header_Item> tmp_item(OP_NEW_L(Header_Item,(SEPARATOR_COMMA)));
			tmp_item->InitL(ask_item->GetName());

			if( ask_item_enabled &&	(ask_item_len+1 < g_memory_manager->GetTempBuf2kLen()) )
			{
				// Copy header contents
				char* ask_ptr = (char *) g_memory_manager->GetTempBuf2k();
				ask_item->OutputHeader(ask_ptr);
				ask_ptr += ask_item->GetName().Length() + 2; // Skip header name, colon and initial white space
				tmp_item->AddParameterL(ask_ptr);
			}
			else
				tmp_item->SetEnabled(FALSE);

			OP_DBG(("Header created (enabled=%d): \"%s\"",tmp_item->IsEnabled(),(tmp_item->IsEnabled() ? g_memory_manager->GetTempBuf2k() : "")));

			temp_list->InsertHeader(tmp_item.release()); // Item automatically removed from old list
			continue;
		}
		temp_list->InsertHeader(keep_item); // Item automatically removed from old list

		if( ask_item_enabled == keep_item->IsEnabled() )
		{
			if( ask_item_enabled )
			{
				BOOL equal = FALSE;
				char *ask_ptr = 0;

				if( ask_item_len == keep_item->CalculateLength() ) // The headers are not empty and there is a chance they are the same
				{
					if( 2*ask_item_len+2 > g_memory_manager->GetTempBuf2kLen() )
					{
						OP_DBG(("  OBS! Header too long for buffer!"));
						continue; // Don't bother with this insanely long header
					}

					ask_ptr = (char *) g_memory_manager->GetTempBuf2k();
					char *keep_ptr = ask_item->OutputHeader(ask_ptr);
					ask_ptr[ask_item_len-2] = '\0'; // Avoid CR/LF
					keep_item->OutputHeader(keep_ptr);
					uint32 skiplen = ask_item->GetName().Length()+2; // Skip header name, colon and initial white space
					ask_ptr += skiplen;
					keep_ptr += skiplen;
					equal = op_strncmp(ask_ptr,keep_ptr,ask_item_len-(skiplen+2)) == 0;
				}

				if( equal )
				{
					OP_DBG(("    Header \"%s\" matched! Removing header from request.",ask_item->GetName().CStr()));
					OP_DBG(("    Header content: %s",ask_ptr));
					ask_item->SetTemporaryDisable(Header_Item::TEMP_DISABLED_YES);
				}
				else
				{
					// copy ask_item to keep_item
					if( !ask_ptr )
					{
						if( ask_item_len+1 > g_memory_manager->GetTempBuf2kLen() )
						{
							OP_DBG(("  OBS! Header too long for buffer!"));
							continue; // Don't bother with this insanely long header
						}
						ask_ptr = (char *) g_memory_manager->GetTempBuf2k();
						ask_item->OutputHeader(ask_ptr);
						ask_ptr += ask_item->GetName().Length() + 2; // Skip header name, colon and initial white space
					}
					keep_item->ClearParameters();
					keep_item->AddParameterL(ask_ptr);
					keep_item->SetEnabled(TRUE);
					OP_DBG(("Setting new common header contents: %s",ask_ptr));
				}
			}
		}
		else
		{
			if( ask_item_enabled )
			{
				// copy ask_item to keep_item
				if( ask_item_len + 1 < g_memory_manager->GetTempBuf2kLen() )
				{
					char* ask_ptr = (char *) g_memory_manager->GetTempBuf2k();
					ask_item->OutputHeader(ask_ptr);
					ask_ptr += ask_item->GetName().Length() + 2; // Skip header name, colon and initial white space

					keep_item->ClearParameters();
					keep_item->AddParameterL(ask_ptr);
					keep_item->SetEnabled(TRUE);
					OP_DBG(("    Setting new common header contents: %s",ask_ptr));
				}
			}
			else
			{
				ask_item->SetTemporaryDisable(Header_Item::TEMP_DISABLED_SIGNAL_REMOVE);
				keep_item->SetEnabled(FALSE);
				OP_DBG(("    Signaling header disabled to server"));
			}
		}
	}

	// Reset any remaining headers
	Header_Item* temp_item;
	keep_item = m_common_headers->First();
	while( keep_item )
	{
		temp_item = keep_item;
		keep_item = keep_item->Suc();
		OP_DBG(("Remaining header! %s (enabled=%d)",temp_item->GetName().CStr(),temp_item->IsEnabled()));
		if( temp_item->IsEnabled() )
		{
			// Add non-intrusive header to request header list
			Header_Item* new_item = OP_NEW_L(Header_Item,(SEPARATOR_COMMA));
			new_item->InitL(temp_item->GetName());
			new_item->SetEnabled(FALSE);
			new_item->SetTemporaryDisable(Header_Item::TEMP_DISABLED_SIGNAL_REMOVE);
			hdrList.InsertHeader(new_item);
		}
		temp_item->SetEnabled(FALSE);
		temp_list->InsertHeader(temp_item); // Item automatically removed from old list
	}
	OP_DELETE(m_common_headers);
	m_common_headers = temp_list.release();
}

void HTTP_1_1::ExpandResponseHeadersL(HeaderList& hdrList)
{
	OP_NEW_DBG("ExpandResponseHeaders","header_reduction");

	// We're inevitably going to need the cache so we might as well create it up front.
	if( !m_common_response_headers )
		m_common_response_headers = OP_NEW_L(HeaderList, ());

	// Nothing to do so just drop out.
	if( hdrList.Empty() )
		return;

	// Make a pass over the incoming list. For each negative entry remove it from
	// both the incoming list and the cache.
	for( NameValue_Splitter *negHeader = hdrList.First(); negHeader != NULL; )
	{
		NameValue_Splitter *currNeg = negHeader;
		negHeader = negHeader->Suc();

		if( op_strcmp("-", currNeg->Value()) == 0 )
		{
			for( NameValue_Splitter *cachedHeader = m_common_response_headers->First(); cachedHeader != NULL;  )
			{
				NameValue_Splitter *currCached = cachedHeader;
				cachedHeader = cachedHeader->Suc();
				if( op_strcmp(currNeg->Name(), currCached->Name()) == 0 )
				{
					currCached->Out();
					OP_DELETE(currCached);
				}
			}
			currNeg->Out();
			OP_DELETE(currNeg);
		}
	}

	NameValue_Splitter *header = NULL;

	// Handle cookies separately
	HeaderList cookie_headers;
	ANCHOR(HeaderList, cookie_headers);
	cookie_headers.SetKeywordList(hdrList);
	
	hdrList.DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie);
	while ((header = hdrList.GetHeaderByID(HTTP_Header_Set_Cookie)) != NULL)
	{
		header->Out();
		OP_DELETE(header);
	}

	hdrList.DuplicateHeadersL(&cookie_headers, HTTP_Header_Set_Cookie2);
	while ((header = hdrList.GetHeaderByID(HTTP_Header_Set_Cookie2)) != NULL)
	{
		header->Out();
		OP_DELETE(header);
	}

	// Make a pass over the incoming list and add any new ones to the cache.
	if (!hdrList.Empty())
	{
		for( NameValue_Splitter *newHeader = hdrList.First(); newHeader != NULL; newHeader = newHeader->Suc() )
		{
			// Unfortunately there's no good way to just reset the value of a cache entry
			// so we'll just have to remove it and reinsert a new entry.
			for( NameValue_Splitter *cachedHeader = m_common_response_headers->First(); cachedHeader != NULL; cachedHeader = cachedHeader->Suc() )
				if (op_strcmp(newHeader->Name(), cachedHeader->Name()) == 0)
				{
					cachedHeader->Out();
					OP_DELETE(cachedHeader);
					break;
				}

			// Allocate and insert into the cache.
			newHeader->DuplicateIntoL(m_common_response_headers);
		}
	}

	// Copy the cache to the passed in list as that's what we're after.
	hdrList.Clear();
	for (header = m_common_response_headers->First(); header != NULL; header = header->Suc())
		header->DuplicateIntoL(&hdrList);

	for (header = cookie_headers.First(); header != NULL; header = header->Suc())
		header->DuplicateIntoL(&hdrList);
}
#endif //URL_TURBO_MODE_HEADER_REDUCTION

void HTTP_1_1::OnLoadingFinished()
{
	 // If the connection is going to close, attempt open a new connection if pref is set.
	if (request->info.sent_request && request->info.open_extra_idle_connections && (header_info->info.received_close || !header_info->info.http1_1) && g_pcnet->GetIntegerPref(PrefsCollectionNetwork::OpenIdleConnectionOnClose))
		manager->AttemptCreateExtraIdleConnection(request, 0);
}

BOOL HTTP_1_1::SafeToDelete() const
{
	if(!request_list.Empty())
	{
		HTTP_Request_List *req_item = request_list.First();

		while(req_item)
		{
			if (req_item->request && (req_item->request->GetMethod() != HTTP_METHOD_CONNECT || GetParent() && GetParent()->IsNeededConnection()))
				return FALSE;

			req_item = req_item->Suc();
		}
	}

	return ProtocolComm::SafeToDelete();
}

CommState HTTP_1_1::ConnectionEstablished()
{
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	BOOL already_connected =  info.connection_established;
#endif
	info.connection_established = TRUE;
	info.connection_failed = FALSE;

#ifdef OPERA_PERFORMANCE
	http_Manager->OnConnectionConnected(connection_number);
#endif // OPERA_PERFORMANCE
#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(request)
		request->secondary_request = NULL;
#endif

	if(!request)
	{
		manager->ForciblyMoveRequest(this);
		if(!request)
		{
			last_active = g_timecache->CurrentTime();
#ifdef HTTP_BENCHMARK
			BenchMarkLastActive();
#endif
			return COMM_LOADING;
		}
	}


#if defined(_SSL_SUPPORT_) && (!defined(_EXTERNAL_SSL_SUPPORT_)  || defined(_USE_HTTPS_PROXY))
	if(!already_connected || request->method != HTTP_METHOD_CONNECT)
#endif
	{
#ifdef HTTP_BENCHMARK
		BenchMarkAddConnWaitTime(last_finished);
#endif
		if(request->method == HTTP_METHOD_CONNECT)
			return COMM_LOADING;
	}
	return ProtocolComm::ConnectionEstablished();
}

//FIXME:OOM Cleanup, return status
void HTTP_1_1::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2_arg)
{
	OP_MEMORY_VAR MH_PARAM_2 par2 = par2_arg; // may be trampled by TRAP
									__DO_START_TIMING(TIMING_COMM_LOAD_HT);
	switch(msg)
	{
		case MSG_HTTP_FORCE_CONTINUE:
			if(sending_request && sending_request->Id() == (unsigned) par1)
			{
				mh->UnsetCallBack(this,MSG_HTTP_FORCE_CONTINUE);
				manager->SetHTTP_Continue(FALSE);
				sending_request->info.send_100c_body = TRUE;
				RequestMoreData();
			}
			break;
		case MSG_HTTP_CONTINUE_FALLBACK:
			if(request && request->Id() == (unsigned) par1)
			{
				mh->UnsetCallBack(this,MSG_HTTP_CONTINUE_FALLBACK);
				manager->SetAlwaysUseHTTP_Continue(TRUE);
				info.activated_100_continue_fallback = FALSE;

				// Str::SI_ERR_HTTP_100CONTINUE_ERROR is Non - fatal error (Hardcoded)
				mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), URL_ERRSTR(SI, ERR_HTTP_100CONTINUE_ERROR));
			}
			break;

		case MSG_HTTP_CHECK_IDLE_TIMEOUT:
			{
				time_t lst_active = LastActive();

				if(!generic_info.requests_sent_on_connection || // if connection was never used, simply close
						(!request && lst_active && lst_active + (
#ifdef _BYTEMOBILE
			       urlManager->GetByteMobileOpt() ?
				   CHECK_IDLE_CONNECTION_SEND_DELAY_BYTEMOBILE :
#endif
				   CHECK_IDLE_CONNECTION_DELAY)*2 < g_timecache->CurrentTime() )
				   )
				{
					OP_ASSERT(!request);
//					info.disable_more_requests = TRUE;	// Set to TRUE by call to Stop() below
					Stop();
					mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);
					mh->UnsetCallBacks(this);
					break;
				}
#ifdef HTTP_AGGRESSIVE_PIPELINE_TIMEOUT
				HTTP_Request_List *nextreq = (request_list.First() ? request_list.First()->Suc() : NULL);
#ifdef DEBUG_HTTP_REQDUMP
				{
					OpString8 textbuf1;

					textbuf1.AppendFormat("http%04d.txt",Id());
					PrintfTofile(textbuf1,"Testing idle Connection %d Tick %lu : %lu %lu : %p",Id(),
						(unsigned long) g_op_time_info->GetWallClockMS(), lst_active,
						g_timecache->CurrentTime()
						, request);
					if(request)
						PrintfTofile(textbuf1,"(%d) : %d %d %d ",request->Id(),
							request->info.sent_pipelined, request->info.is_formsrequest, request->GetHeaderLoaded());
					if(nextreq && nextreq->request)
						PrintfTofile(textbuf1,": %p (%d) : %d %d",nextreq->request, nextreq->request->Id(),
							nextreq->request->info.sent_pipelined, nextreq->request->info.is_formsrequest);

					PrintfTofile(textbuf1,"\n");
				}
#endif

				unsigned int delay = CHECK_IDLE_CONNECTION_SEND_DELAY;
#ifdef _BYTEMOBILE
				if( urlManager->GetByteMobileOpt() )
					delay = CHECK_IDLE_CONNECTION_SEND_DELAY_BYTEMOBILE;
#endif
#ifdef _EMBEDDED_BYTEMOBILE
				if( urlManager->GetEmbeddedBmOpt() )
					delay = CHECK_IDLE_CONNECTION_SEND_DELAY_BYTEMOBILE;
#endif
#ifdef WEB_TURBO_MODE
				if( generic_info.turbo_enabled )
					delay = CHECK_IDLE_CONNECTION_SEND_DELAY_BYTEMOBILE;
#endif

				if(request && lst_active && (lst_active +  (time_t) (delay * 2)) < g_timecache->CurrentTime()
					&& /*  !info.disable_more_requests && */!request->info.is_formsrequest  &&
						(request->info.sent_pipelined || ( nextreq && nextreq->request && nextreq->request->info.sent_pipelined) ||
						 ((!nextreq || !nextreq->request) && request->info.pipeline_used_by_previous)) /* &&
						(!request->GetHeaderLoaded() ||
						 (request_list.First() && request_list.First()->Suc() && request_list.First()->Suc()->request &&
						   request_list.First()->Suc()->request->info.sent_request))*/
						 )
				{
#ifdef DEBUG_HTTP_REQDUMP
					OpString8 textbuf1;

					textbuf1.AppendFormat("http%04d.txt",Id());
					PrintfTofile(textbuf1,"Forcibly shutting down (due to suspected pipeline problem) Connection %d Tick %lu\n",Id(), (unsigned long) g_op_time_info->GetWallClockMS());
					PrintfTofile("http.txt","Forcibly shutting down (due to suspected pipeline problem) Connection %d Tick %lu\n",Id(), (unsigned long) g_op_time_info->GetWallClockMS());
#endif
					// Possibly a failed pipeline
					manager->IncrementPipelineProblem();
					SignalPipelineReload();

#ifdef WEB_TURBO_MODE
					if (!request->request->use_turbo || manager->GetHTTP_No_Pipeline())
#endif // WEB_TURBO_MODE
					manager->SetHTTP_1_1_Pipelined(FALSE);

					//request->sendcount++;
					ProtocolComm::SetProgressInformation(RESTART_LOADING,0,0);
					MoveRequestsToNewConnection(NULL, TRUE);
					HandleEndOfConnection(URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED));
				}
#endif
				else
					mh->PostDelayedMessage(MSG_HTTP_CHECK_IDLE_TIMEOUT,Id(),0,delay * 1000UL);
			}
			break;

		case MSG_HTTP_ZERO_LENGTH_TIMEOUT:
			mh->UnsetCallBack(this,MSG_HTTP_ZERO_LENGTH_TIMEOUT, Id());
			if(request && content_loaded)
			{
				break; // This connection seems to be under control
			}
			// Looks like the server forgot about us, close the connection by force
			HandleEndOfConnection(0);
			break;


		case MSG_COMM_LOADING_FAILED :
			info.connection_failed = TRUE;
			// fall through
		case MSG_COMM_LOADING_FINISHED :
			{
				//HTTP_Request_List *item;

#ifdef DEBUG_HTTP_REQDUMP
				OpString8 textbuf1;

				textbuf1.AppendFormat("http%04d.txt",Id());
				PrintfTofile(textbuf1,"HTTP connection %d : Received %s.\n", Id(), (msg == MSG_COMM_LOADING_FAILED ? "Failed" : "Finished") );
				if(request)
				{
					OpString8 textbuf2;

					textbuf2.AppendFormat("htpr%04d.txt",request->Id());
					PrintfTofile(textbuf2,"HTTP connection %d : Received %s.\n", Id(), (msg == MSG_COMM_LOADING_FAILED ? "Failed" : "Finished") );
				}
#endif

				generic_info.disable_more_requests = TRUE;

				if(request)
				{
					if(header_str)
					{
						if(IsLegalHeader(header_str, op_strlen(header_str)))
						{
							request->SetHeaderLoaded(TRUE);
							TRAPD(op_err, SetHeaderInfoL());//FIXME:OOM Error should be signalled.
							ProtocolComm::SetProgressInformation(HEADER_LOADED);
						}
					}

				}
				HandleEndOfConnection(msg == MSG_COMM_LOADING_FAILED ? par2 : 0);
				__DO_STOP_TIMING(TIMING_COMM_LOAD);
			}
			break; // message posted by Handleend of connection

		case MSG_COMM_DATA_READY :
#ifdef DEBUG_HTTP_FILE
			PrintfTofile("http.txt","Sending MSG_COMM_DATA_READY [%d] #3:%d\n", Id(),info.read_source);
#endif
			mh->PostMessage(msg, Id(), par2);
			break;

		case MSG_COMM_NEW_REQUEST:
			if(!content_buf.IsEmpty())
			{
				ProcessReceivedData();
			}
			break;
	}
									__DO_STOP_TIMING(TIMING_COMM_LOAD_HT);
}

void HTTP_1_1::SetProgressInformation(ProgressState progress_level,
											 unsigned long progress_info1,
											 const void *progress_info2)
{
#ifdef _SSL_USE_SMARTCARD_
	if(progress_level == SMARTCARD_AUTHENTICATED)
		info.smart_card_authenticated = TRUE;
#endif

	if(progress_level == STOP_FURTHER_REQUESTS)
	{

		generic_info.disable_more_requests = TRUE;
		HTTP_Request_List *item = request_list.First();
		if(progress_info1==STOP_REQUESTS_TC_NOT_FIRST_REQUEST && item)
			item = item->Suc(); // Ignore first request on the connection

		while(item)
		{
			if(item->request && !item->request->GetSendingRequest() && ! item->request->GetSentRequest())
				break;

			item = item->Suc();
		}

		if(item)
		{
			MoveRequestsToNewConnection(item,TRUE);
		}

		if(progress_info1 == STOP_REQUESTS_ALL_CONNECTIONS)
		{
			NormalCallCount blocker(this);
			manager->ForceOtherConnectionsToClose(this);
		}
		return;
	}
	else if(progress_level == RESTART_LOADING)
	{
		if(request && (!info.connection_established ||
			(GetMethodIsRestartable(request->GetMethod()) &&
			/*request->method != HTTP_METHOD_POST &&
			request->method != HTTP_METHOD_PUT &&
			request->method != HTTP_METHOD_DELETE &&*/
			!request->info.header_loaded &&
			progress_info2 != NULL)))
		{
			if(progress_info2)
				*((BOOL *) progress_info2) = TRUE;
			request->Clear();
			request->info.header_loaded = FALSE;
			request->SetForceWaiting(TRUE);
			sending_request = NULL;
		}
		else
			EndLoading();

		return;
	}
	else if (progress_level == REQUEST_CONNECTION_ESTABLISHED_TIMOUT)
	{
		HTTP_Request_List* request_item = request_list.First();

		while (request_item)
		{
			request_item->request->SetProgressInformation(progress_level, progress_info1, progress_info2);

			request_item =  request_item->Suc();;
		}
	}

	if (progress_level == SET_NEXTPROTOCOL)
	{
		OP_ASSERT(!info.connection_established);
		NextProtocol protocol = static_cast<NextProtocol>(progress_info1);
		if (protocol != NP_HTTP)
		{
			generic_info.disable_more_requests = TRUE;
			manager->NextProtocolNegotiated(this, protocol);
			return;
		}
	}

	ProtocolComm::SetProgressInformation(progress_level, progress_info1, progress_info2);

	if(progress_level == HANDLE_SECONDARY_DATA)
		info.read_source = FALSE;

}

void HTTP_1_1::HandleEndOfConnection(MH_PARAM_2 par2)
{
	// Status at call: Connection closed
	// Data may still be buffered

#if 0
	{
		Log("HTTP1: End Connection 0 ");
		Log(Id());
		Log(" : error msg ");
		Log(par2);
		if(par2 == URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED))
			Log(" : Connection Closed by remote server");
		Log(" : ");
		if(request)
			Log(request->request->origin_host->Name());
		Log(" : ");
		if(request)
			LogNl(request->request->request);
		Log("HTTP1: Status : Content-Length");
		Log(content_loaded);
		Log("/");
		Log(content_length);
		if(info.use_chunking)
		{
			Log("Chunking : ");
			Log(chunk_loaded);
			Log("/");
			Log(chunk_size);
			Log(info.waiting_for_chunk_header ? "Waiting" : "");
		}
		Log(" Previous  ");
		if(request)
			Log(request->previous_load_length);
		Log("/");
		if(request)
			Log(request->previous_load_difference);
		Log(" Tick : ");
		Log(g_op_time_info->GetWallClockMS());
		LogNl();
	}
#endif

	HTTP_Request_List *item;

	if(info.handling_end_of_connection)
		return;

	info.handling_end_of_connection = TRUE;

	if(request && !content_buf.IsEmpty())
	{
		// Continue with next request while there are data
		if(content_length && content_loaded == content_length)
			MoveToNextRequest();

		if(request) // in case there are no more requests
		{
			mh->PostMessage(MSG_COMM_DATA_READY, Id(), 0);
			info.handling_end_of_connection = FALSE;
			return; // Still more data to handle
		}
	}

	if (request)
	{
#ifdef WEB_TURBO_MODE
		// If connection to Turbo proxy is closed without any response it is probably overloaded
		if( info.connection_established && generic_info.turbo_enabled && urlManager->GetWebTurboAvailable()
			&& !request->request->use_proxy_passthrough && info.first_request && !request->GetHeaderLoaded() )
		{
			urlManager->SetWebTurboAvailable(FALSE);
		}
		// If we couldn't establish a connection to the turbo server, restart all requests to it
		// as direct loads.
		else if (!info.connection_established && generic_info.turbo_enabled)
		{
			urlManager->SetWebTurboAvailable(FALSE);

			request->Clear();
			request->info.header_loaded = FALSE;

			ProtocolComm::SetProgressInformation(RESTART_LOADING,0,NULL);

			for (HTTP_Request_List *next = request_list.First(); next != NULL; next = next->Suc())
				mh->PostMessage(MSG_COMM_LOADING_FAILED, next->request->Id(), ERR_COMM_RELOAD_DIRECT);

			request = NULL;
			sending_request = NULL;

			MoveRequestsToNewConnection(NULL, TRUE);

			info.handling_end_of_connection = FALSE;
			return;
		}
#endif // WEB_TURBO_MODE

		BOOL terminate_request = FALSE;
		OpMessage msg = MSG_NO_MESSAGE;

		if(!info.connection_established &&
			((par2 && par2 != ERR_SSL_ERROR_HANDLED && par2 != Str::S_SSL_FATAL_ERROR_MESSAGE)
#ifdef _SSL_SUPPORT_
			 || (!par2 && request->secondary_request)
#endif // _SSL_SUPPORT_
			 ))
		{
#ifdef SUPPORT_AUTO_PROXY_CONFIGURATION
			if(request->request->current_connect_host &&
				((par2 == URL_ERRSTR(SI, ERR_COMM_CONNECT_FAILED) ||
					par2 == URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND) ||
					par2 == URL_ERRSTR(SI, ERR_COMM_HOST_UNAVAILABLE) ||
					par2 == URL_ERRSTR(SI, ERR_COMM_CONNECTION_REFUSED)) &&
					request->MoveToNextProxyCandidate())
					)
			{
				item = request_list.First();
				item->request = NULL;
				OP_DELETE(item);

				mh->RemoveCallBacks(this, GetSink()->Id());
				request->PopSink();
				ChangeParent(NULL);
				request->Load();
				request = NULL;
			}
			else
#endif
			if(!par2)
			{
				terminate_request = TRUE;
				msg = MSG_COMM_LOADING_FINISHED;
			}
			else
			{
				terminate_request = TRUE;
				msg = MSG_COMM_LOADING_FAILED;

				if(request->GetProxyRequest())
				{
#ifdef WEB_TURBO_MODE
					BOOL disable_turbo_mode =	request->request->use_turbo
												&& !request->info.bypass_turbo_proxy
#ifdef URL_SIGNAL_PROTOCOL_TUNNEL
												&& !request->request->is_tunnel
#endif // URL_SIGNAL_PROTOCOL_TUNNEL
												&& !request->request->use_proxy_passthrough
												&& urlManager->GetWebTurboAvailable();
#endif // WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
					BOOL disable_bytemobile = TRUE;
#endif //_EMBEDDED_BYTEMOBILE

					switch (par2)
					{
					case URL_ERRSTR(SI, ERR_COMM_CONNECT_FAILED): par2 = URL_ERRSTR(SI, ERR_COMM_PROXY_CONNECT_FAILED); break;
					case URL_ERRSTR(SI, ERR_COMM_HOST_NOT_FOUND): par2 = URL_ERRSTR(SI, ERR_COMM_PROXY_HOST_NOT_FOUND); break;
					case URL_ERRSTR(SI, ERR_COMM_HOST_UNAVAILABLE): par2 = URL_ERRSTR(SI, ERR_COMM_PROXY_HOST_UNAVAILABLE); break;
					case URL_ERRSTR(SI, ERR_COMM_CONNECTION_REFUSED): par2 = URL_ERRSTR(SI, ERR_COMM_PROXY_CONNECTION_REFUSED); break;
					default:
#ifdef _EMBEDDED_BYTEMOBILE
						disable_bytemobile = FALSE;
#endif //_EMBEDDED_BYTEMOBILE
#ifdef WEB_TURBO_MODE
						disable_turbo_mode = FALSE;
#endif // WEB_TURBO_MODE
						break;
					}

#ifdef WEB_TURBO_MODE
					if( disable_turbo_mode )
						urlManager->SetWebTurboAvailable(FALSE);
#endif // WEB_TURBO_MODE
#ifdef _EMBEDDED_BYTEMOBILE
					if(disable_bytemobile)
					{
						urlManager->SetEmbeddedBmOpt(FALSE);
						urlManager->SetEmbeddedBmDisabled( TRUE);
					}
#endif //_EMBEDDED_BYTEMOBILE
				}
			}
		}
		else if(par2 && !info.connection_established &&
			(par2 == ERR_SSL_ERROR_HANDLED || par2 == Str::S_SSL_FATAL_ERROR_MESSAGE))
		{
			terminate_request = TRUE;
			msg = MSG_COMM_LOADING_FAILED;
		}
		else if(!info.connection_established &&
			request->connectcount >=5)
		{
			// Too many attempts
			terminate_request = TRUE;
			msg = MSG_COMM_LOADING_FAILED;
			par2 = URL_ERRSTR(SI, ERR_COMM_CONNECTION_REFUSED);
		}
		else if(request->GetHeaderLoaded() &&
			!request->info.proxy_request &&
			((content_length && content_loaded != content_length) ||
			(!content_length && info.use_chunking && (chunk_size != 0 || info.waiting_for_chunk_header))))
		{

#ifdef HTTP_CONNECTION_TIMERS
			if( par2 == URL_ERRSTR(SI, ERR_HTTP_TIMEOUT ))//ERR_HTTP_TIMER_EXPIRED )
			{
				terminate_request = TRUE;
				msg = MSG_COMM_LOADING_FAILED;
				par2 = ERR_SSL_ERROR_HANDLED; // No error dialog necessary. That is handled by timeout code.
			}
#endif //HTTP_CONNECTION_TIMERS
			// Redirects and forms with content-lenght (definite or chunked)
			// are not retried

			if(IsRedirectResponse(header_info->response) ||
				request->info.proxy_request ||
				(content_length && !manager->GetHTTP_Trust_ContentLength()))
			{
				terminate_request = TRUE;
				msg = MSG_COMM_LOADING_FINISHED;
				par2 = 0;
			}
			else if(request->GetIsFormsRequest() ||
				!GetMethodIsRestartable(request->method)
				/*request->method == HTTP_METHOD_POST ||
				request->method == HTTP_METHOD_PUT ||
				request->method == HTTP_METHOD_DELETE*/
				)
			{
				terminate_request = TRUE;
				msg = MSG_COMM_LOADING_FAILED;
				par2 = URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED);
#if 0
				Log("HTTP1: End Connection 1 ");
				Log(Id());
				Log(" : ");
				Log(request->request->origin_host->Name());
				Log(" : ");
				LogNl(request->request->request);
				Log("HTTP1: Status : Content-Length");
				Log(content_loaded);
				Log("/");
				Log(content_length);
				if(info.use_chunking)
				{
					Log("Chunking : ");
					Log(chunk_loaded);
					Log("/");
					Log(chunk_size);
					Log(info.waiting_for_chunk_header ? "Waiting" : "");
				}
				Log(" Previous  ");
				Log(request->previous_load_length);
				Log("/");
				Log(request->previous_load_difference);
				LogNl();
#endif
			}
			// else normal end of connection
		}
		else if(request->GetHeaderLoaded() && (!content_length || content_length == content_loaded || request->info.proxy_request))
		{
			// request without content-length are not retried, except if they are terminated with an error
			// in addition, proxy requests, forms and redirects are not retried
			if( par2 == 0 ||
				par2 == ERR_SSL_ERROR_HANDLED || par2 == Str::S_SSL_FATAL_ERROR_MESSAGE ||
				(!content_length && !info.use_chunking && par2 == URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED)) ||
				(content_length && (content_length == content_loaded || !manager->GetHTTP_Trust_ContentLength())) ||
				request->info.proxy_request ||
				IsRedirectResponse(header_info->response))
			{
				terminate_request = TRUE;
				if (par2 == URL_ERRSTR(SI, ERR_NETWORK_PROBLEM) || par2 == ERR_SSL_ERROR_HANDLED)
				{
					/* We definitely want to treat this condition as an error.
					   I don't know if we should check for other values here as
					   well. */
					msg = MSG_COMM_LOADING_FAILED;
				}
				else
				{
			    	msg = MSG_COMM_LOADING_FINISHED;
			    	par2 = 0;
                }
			}
			else if(request->GetIsFormsRequest() ||
				!GetMethodIsRestartable(request->method)
				/*request->method == HTTP_METHOD_POST ||
				request->method == HTTP_METHOD_PUT ||
				request->method == HTTP_METHOD_DELETE*/
				)
			{
				terminate_request = TRUE;
				msg = MSG_COMM_LOADING_FAILED;
				par2 = URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED);
#if 0
				Log("HTTP1: End Connection 2 ");
				Log(Id());
				Log(" : ");
				Log(request->request->origin_host->Name());
				Log(" : ");
				LogNl(request->request->request);
				Log("HTTP1: Status : Content-Length");
				Log(content_loaded);
				Log("/");
				Log(content_length);
				if(info.use_chunking)
				{
					Log("Chunking : ");
					Log(chunk_loaded);
					Log("/");
					Log(chunk_size);
					Log(info.waiting_for_chunk_header ? "Waiting" : "");
				}
				Log(" Previous  ");
				Log(request->previous_load_length);
				Log("/");
				Log(request->previous_load_difference);
				LogNl();
#endif
			}

			if(request->info.proxy_request && content_length && content_length != content_loaded)
			{
				if(request->info.pipeline_used_by_previous
#ifdef _EMBEDDED_BYTEMOBILE
					&& !urlManager->GetEmbeddedBmOpt()
#endif
#ifdef WEB_TURBO_MODE
					&& !generic_info.turbo_enabled
#endif
				)
				{
					if(info.http_1_1_pipelined)
						manager->SetHTTP_1_1_Pipelined(FALSE);
					else
						manager->SetHTTP_No_Pipeline(TRUE);
				}
			}
		}
		else if((request->GetSendingRequest() || request->GetSentRequest()) &&
			(request->GetIsFormsRequest() ||
			!GetMethodIsRestartable(request->method)
			/*request->method == HTTP_METHOD_POST ||
			request->method == HTTP_METHOD_PUT ||
			request->method == HTTP_METHOD_DELETE*/
			))
		{
			terminate_request = TRUE;
			// Forms are never retried
			msg = MSG_COMM_LOADING_FAILED;
			par2 = URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED);
#if 0
				Log("HTTP1: End Connection 3 ");
				Log(Id());
				Log(" : ");
				Log(request->request->origin_host->Name());
				Log(" : ");
				LogNl(request->request->request);
				Log("HTTP1: Status : Content-Length");
				Log(content_loaded);
				Log("/");
				Log(content_length);
				if(info.use_chunking)
				{
					Log("Chunking : ");
					Log(chunk_loaded);
					Log("/");
					Log(chunk_size);
					Log(info.waiting_for_chunk_header ? "Waiting" : "");
				}
				Log(" Previous  ");
				Log(request->previous_load_length);
				Log("/");
				Log(request->previous_load_difference);
				LogNl();
#endif
		}
		else if(par2 == ERR_SSL_ERROR_HANDLED || par2 == Str::S_SSL_FATAL_ERROR_MESSAGE)
		{
			terminate_request = TRUE;
			msg = MSG_COMM_LOADING_FAILED;
		}
#ifdef _SSL_SUPPORT_
		else if(!par2 && request->secondary_request && request->header_info->response == HTTP_PROXY_UNAUTHORIZED)
		{
			terminate_request = TRUE;
			msg = MSG_COMM_LOADING_FINISHED;
			par2 = 0;
		}
#endif // _SSL_SUPPORT_

		if(terminate_request)
		{
			item = request_list.First();
			item->request = NULL;
			OP_DELETE(item);

			if(sending_request == request)
				sending_request = NULL;

			request->LoadingFinished(FALSE);
			OP_ASSERT(GetParent() == request);
			request->PopSink();
			ChangeParent(NULL);
			mh->PostMessage(msg, request->Id(), par2);
			request = NULL;
		}
	}

	if(request)
	{
		if((!info.host_is_1_0 || info.http_1_0_keep_alive) &&
			request->sendcount < 5 &&
			request->info.header_loaded &&
			!request->info.proxy_request &&
			((content_length && content_loaded != content_length) ||
			(!content_length && info.use_chunking && (chunk_size != 0 || info.waiting_for_chunk_header)))
			)
		{
			if(!request->info.retried_loading ||
				(request->previous_load_length && request->previous_load_length != content_loaded &&
					request->previous_load_difference != content_length - content_loaded))
			{
				item = request_list.First();
				item = item->Suc();

				if(item && (!item->request || item->request->GetSendingRequest() || item->request->GetSentRequest()))
				{
					manager->IncrementPipelineProblem();
					manager->SetHTTP_1_1_Pipelined(FALSE);
					if(request->sendcount >2)
						manager->SetHTTP_No_Pipeline(TRUE);
				}

				if(request->sendcount < 5 && !request->info.disable_automatic_refetch_on_error &&
					header_info->response == HTTP_OK)
				{
					request->Clear();
					request->info.header_loaded = FALSE;
					request->SetForceWaiting(TRUE);
					request->info.retried_loading = TRUE;
					if(content_loaded && (!request->previous_load_length || request->previous_load_length < content_loaded))
						request->previous_load_length = content_loaded;
					if(content_loaded && (!request->previous_load_difference || request->previous_load_difference < content_length - content_loaded))
						request->previous_load_difference = content_length - content_loaded;
					ProtocolComm::SetProgressInformation(RESTART_LOADING,0,0);
					request = NULL; // restart is handled by MoveRequestToNewConnection
				}
			}

		}

		if(request /* has not been restarted */)
		{
			if((request->info.proxy_request && request->GetHeaderLoaded()) || //request->sendcount >=5 ||
				(request->sendcount >=5 && !request->GetHeaderLoaded()) ||
				(request->info.retried_loading && request->GetHeaderLoaded() &&
				(request->previous_load_length == content_loaded ||
				request->previous_load_difference == content_length - content_loaded)))
			{
				item = request_list.First();
				item->request = NULL;
				OP_DELETE(item);

				if(request->sendcount >=5 && !request->GetHeaderLoaded())
				{
					mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), 
						(request->method == HTTP_METHOD_CONNECT ? ERR_COMM_PROXY_CONNECT_FAILED : ERR_COMM_CONNECTION_CLOSED));
					request->LoadingFinished(FALSE);
				}
				else
					request->LoadingFinished(TRUE);
				request->PopSink();
			}
			else if(request->info.disable_automatic_refetch_on_error ||
				(request->info.retried_loading && request->GetHeaderLoaded() &&
				(request->previous_load_length && request->previous_load_length != content_loaded &&
					request->previous_load_difference != content_length - content_loaded)) )
			{
				item = request_list.First();
				item->request = NULL;
				OP_DELETE(item);

				request->PopSink();
				request->LoadingFinished(FALSE);
				if(request->sendcount >1)
					manager->SetHTTP_Trust_ContentLength(FALSE);
				mh->PostMessage(MSG_COMM_LOADING_FAILED, request->Id(), (request->sendcount > 1 ? URL_ERRSTR(SI, ERR_COMM_REPEATED_FAILED ): URL_ERRSTR(SI, ERR_COMM_CONNECTION_CLOSED)));
#if 0
				Log("HTTP1: End Connection 4 ");
				Log(Id());
				Log(" : ");
				Log(request->request->origin_host->Name());
				Log(" : ");
				LogNl(request->request->request);
				Log("HTTP1: Status : Content-Length");
				Log(content_loaded);
				Log("/");
				Log(content_length);
				if(info.use_chunking)
				{
					Log("Chunking : ");
					Log(chunk_loaded);
					Log("/");
					Log(chunk_size);
					Log(info.waiting_for_chunk_header ? "Waiting" : "");
				}
				Log(" Previous  ");
				Log(request->previous_load_length);
				Log("/");
				Log(request->previous_load_difference);
				LogNl();
#endif
			}
			else
			{
				request->Clear();
				request->info.header_loaded = FALSE;
				request->SetForceWaiting(TRUE);
				request->info.retried_loading = TRUE;
				if(content_loaded && (!request->previous_load_difference || request->previous_load_difference < content_length - content_loaded))
					request->previous_load_difference = content_length - content_loaded;
				if(content_loaded && (!request->previous_load_difference || request->previous_load_difference < content_length - content_loaded))
					request->previous_load_difference = content_length - content_loaded;
				ProtocolComm::SetProgressInformation(RESTART_LOADING,0,NULL);
			}
			request = NULL;
		}
		ChangeParent(NULL);
	}

	sending_request = NULL;

	MoveRequestsToNewConnection(NULL, TRUE);
	mh->PostMessage(MSG_COMM_LOADING_FINISHED, Id(), 0);

	info.handling_end_of_connection = FALSE;
}

#ifdef HTTP_CONNECTION_TIMERS
void HTTP_1_1::OnTimeOut(OpTimer* timer)
{
#ifdef NEED_URL_ABORTIVE_CLOSE
	if(GetSink())
		GetSink()->SetAbortiveClose();
#endif
	HandleEndOfConnection(URL_ERRSTR(SI, ERR_HTTP_TIMEOUT)); //ERR_HTTP_TIMER_EXPIRED);
#ifdef SYNERGY
# warning "HTTP connection time-out handled silently."
#else
	if( timer == m_idle_timer )
	{
		mess->Error(NULL,Str::D_HTTP_IDLE_TIMEOUT,(uni_char*)NULL); // "Connection has been idle for a period longer than setting time"
	}
	else if( timer == m_response_timer )
	{
		mess->Error(NULL,Str::D_HTTP_RESPONSE_TIMEOUT,(uni_char*)NULL); // "Server failed to respond within setting time"
	}
#endif // SYNERGY
}

/**
 * Start time-out while waiting for response from the server.
 */
void HTTP_1_1::StartResponseTimer()
{
	UINT http_response_timeout =
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HTTPResponseTimeout);
	if( http_response_timeout ){
		m_response_timer->Start(http_response_timeout*1000);
		m_response_timer_is_started = TRUE;
	}
}

/**
 * Start time-out while waiting for data from the server.
 */
void HTTP_1_1::StartIdleTimer()
{
	UINT http_idle_timeout =
		g_pcnet->GetIntegerPref(PrefsCollectionNetwork::HTTPIdleTimeout);
	if( http_idle_timeout ){
		m_idle_timer->Start(http_idle_timeout*1000);
		m_idle_timer_is_started = TRUE;
	}
}

/**
 * Stop time-out waiting for response from the server.
 */
void HTTP_1_1::StopResponseTimer()
{
	m_response_timer->Stop();
	m_response_timer_is_started = FALSE;
}

/**
 * Stop time-out waiting for data from the server.
 */
void HTTP_1_1::StopIdleTimer()
{
	m_idle_timer->Stop();
	m_idle_timer_is_started = FALSE;
}

/*
 * If we want to use both HTTP_CONNECTION_TIMERS and
 * TCP_PAUSE_DOWNLOAD_EXTENSION without the timer triggering
 * in the paused state, we need to pause the timers along with
 * the connection. (kentda)
 */

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
/**
 * Pause connection using pause download extension.
 * To avoid a time-out while the download is in a paused state
 * we stop the timers. Note that we do not reset the flags,
 * in order to know which timer(s) to restart when the
 * download is resumed.
 */
void HTTP_1_1::PauseLoading()
{
	ProtocolComm::PauseLoading(); // super
	// We could store the return values here and use them when restarting
	// in ContinueLoading, instead of resetting the timeout on pausing.
	m_response_timer->Stop();
	m_idle_timer->Stop();
	// Don't call method versions, as we want to keep the flag status.
}
/**
 * Resume connection that was previously paused.
 * The timers are restarted if they were running when PauseLoading was called.
 * We rely on others calling the methods when starting/stopping the timers,
 * to ensure that the flags tell us the correct state.
 */
OP_STATUS HTTP_1_1::ContinueLoading()
{
	OP_STATUS result = OP_ProtocolComm::ContinueLoading(); // super
	// Restart timers if they were running when we paused.
	if(m_response_timer_is_started) StartResponseTimer();
	if(m_idle_timer_is_started) StartIdleTimer();
	return result;
}
#endif // TCP_PAUSE_DOWNLOAD_EXTENSION

#endif//HTTP_CONNECTION_TIMERS

void HTTP_1_1::SignalPipelineReload()
{
	UINT32 i,n = pipelined_items.GetCount();

	for(i=0;i<n; i++)
	{
		MH_PARAM_1 msg_id = pipelined_items.Get(i);
		if(msg_id)
			g_main_message_handler->PostMessage(MSG_URL_PIPELINE_PROB_DO_RELOAD_INTERNAL,msg_id,0);
	}
	pipelined_items.Clear();
}

OP_STATUS HTTPRange::Parse(const char *range, BOOL assume_bytes, OpFileLength max_length)
{
	BOOL starts_with_bytes = FALSE;

	// Range is in the form "bytes=X-Y" with X and Y optional, but at least one should be present
	if (!range)
		return OpStatus::ERR_NULL_POINTER;

	SkipWhiteSpaces(range);

	if (op_strncmp(range, "bytes", 5))
	{
		if (!assume_bytes)
			return OpStatus::ERR_OUT_OF_RANGE;
	}
	else
		starts_with_bytes = TRUE;

	const char *where_dash = op_strchr(range, '-');
	const char *where_equal = op_strchr(range, '=');

	if (!where_dash)
		return OpStatus::ERR_OUT_OF_RANGE;

	if (starts_with_bytes && (!where_equal || where_dash < where_equal))
		return OpStatus::ERR_OUT_OF_RANGE;

	// This is just some input validation
	if (starts_with_bytes)
	{
		const char *equal_ptr = range + 5; // Skip "bytes"

		SkipWhiteSpaces(equal_ptr);

		if (*equal_ptr != '=')
			return OpStatus::ERR_OUT_OF_RANGE;
	}

	char *start_number_end;
	char *end_number_end;
	const char *where_start = (starts_with_bytes) ? where_equal + 1 : range;	// Start is after "bytes=", or at the beginning of the string
	const char *where_end = where_dash + 1;										// End is after "-"
	OpFileLength start = static_cast<OpFileLength>(op_strtol(where_start, &start_number_end, 10)); 
	OpFileLength end = static_cast<OpFileLength>(op_strtol(where_end, &end_number_end, 10));

	if (!start_number_end || !end_number_end)
		return OpStatus::ERR_OUT_OF_RANGE;

	// Skip trailing spaces in both "start" and "end"
	SkipWhiteSpaces(start_number_end);
	SkipWhiteSpaces(end_number_end);

	// Check if no end has been specified
	if (!end)
	{
		SkipWhiteSpaces(where_end);

		if (*where_end && (*where_end<'0' || *where_end>'9'))
			return OpStatus::ERR_OUT_OF_RANGE;

		if (*where_end != '0' || where_end==end_number_end)
			end = FILE_LENGTH_NONE;
	}

	if (start_number_end == end_number_end) // We are in case "bytes=-X"   ==> Last bytes
	{
		OP_ASSERT(start<0);   // Of course in this case start is "-X" and end is "X"

		start = FILE_LENGTH_NONE;
	}
	else if (start_number_end != where_dash)  // The next character should be '-'
		return OpStatus::ERR_OUT_OF_RANGE;

	if (*end_number_end)										// No characters expected after the end
		return OpStatus::ERR_OUT_OF_RANGE;

	if (end == 0 && max_length != FILE_LENGTH_NONE)  // Look if we are in case "bytes=X-"  ==> till the end
	{
		const char *pos = where_dash + 1;  // Everything after "-"

		while (*pos && op_isspace(*pos))
			pos++;

		if (!*pos)
			end = max_length -1;
	}

	if (max_length != FILE_LENGTH_NONE)
	{
		OP_ASSERT(max_length != FILE_LENGTH_NONE);

		range_end = (end >= max_length ||			// Not over the file length
			         end==FILE_LENGTH_NONE			// Request till the end of the file
					 || start==FILE_LENGTH_NONE)    // start==FILE_LENGTH_NONE means "last end bytes"
					 ? max_length - 1 : end;
		range_start = (start == FILE_LENGTH_NONE) ? max_length-end : start;
	}
	else
	{
		range_end = end;
		range_start = start;
	}

	OP_ASSERT(range_start <= range_end || range_start==FILE_LENGTH_NONE || range_end==FILE_LENGTH_NONE);

	return OpStatus::OK;
}
