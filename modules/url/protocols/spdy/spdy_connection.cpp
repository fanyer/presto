/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2012 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Kajetan Switalski
**
*/

#include "core/pch.h"

#ifdef USE_SPDY

#include "modules/url/protocols/spdy/spdy_connection.h"
#include "modules/url/protocols/spdy/spdy_httprequesthandler.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/obml_comm/obml_id_manager.h"
#include "modules/url/url_module.h"
#include "modules/url/url_man.h"
#ifdef SCOPE_RESOURCE_MANAGER
# include "modules/scope/scope_network_listener.h"
#endif // SCOPE_RESOURCE_MANAGER


SpdyConnection::SpdyConnection(NextProtocol protocol, MessageHandler* msg_handler, ServerName_Pointer hostname, unsigned short portnumber, BOOL privacy_mode, BOOL secureMode): 
	HttpRequestsHandler(hostname, portnumber, privacy_mode, msg_handler),
	spdyHandler(NULL),
	sendDataMsgSent(FALSE),
	connected(FALSE),
	aborted(FALSE),
	protocol(protocol),
	isSendingData(FALSE),
	pendingDataToSend(FALSE),
	secureMode(secureMode)
{
#ifdef WEB_TURBO_MODE
	turboAuthorizationStatus = TAS_START;
#endif // WEB_TURBO_MODE;
	OP_ASSERT(protocol == NP_SPDY2 || protocol == NP_SPDY3);
}

SpdyConnection::~SpdyConnection()
{
	requests.Clear();
	OP_DELETE(spdyHandler);
	mh->UnsetCallBack(this, MSG_SPDY_SENDDATA);
}

void SpdyConnection::ConstructL()
{
	HttpRequestsHandler::ConstructL();
	spdyHandler = OP_NEW_L(SpdyFramesHandler, (protocol == NP_SPDY2 ? SPDY_V2 : SPDY_V3, server_name, port, generic_info.open_in_privacy_mode, secureMode, this));
	spdyHandler->ConstructL();
	LEAVE_IF_ERROR(mh->SetCallBack(this, MSG_SPDY_SENDDATA, reinterpret_cast<MH_PARAM_1>(this)));
}

BOOL SpdyConnection::SafeToDelete() const
{
	return ProtocolComm::SafeToDelete();
}

BOOL SpdyConnection::IsNeededConnection() const
{
	return GetRequestCount() > 0;
}

BOOL SpdyConnection::AddRequest(HTTP_Request *req, BOOL force_first /*= FALSE*/)
{
	OP_ASSERT(!generic_info.disable_more_requests);
	req->IncSendCount();
#ifdef WEB_TURBO_MODE
	if (GetTurboOptimized())
	{
		req->SetDisableHeadersReduction(TRUE);
		switch (turboAuthorizationStatus)
		{
		case TAS_START:
			req->SetIsFirstTurboRequest(TRUE);
			turboAuthorizationStatus = TAS_FIRST_REQ_SENT;
			break;
		case TAS_FIRST_REQ_SENT:
		case TAS_SECOND_REQ_SENT:
			req->SetNewSink(this);
			return OpStatus::IsSuccess(deferedRequests.Add(req));
		case TAS_XOA_RECEIVED:
			req->SetIsTurboProxyAuthRetry(TRUE);
			req->SetTurboAuthId(Id());
			turboAuthorizationStatus = TAS_SECOND_REQ_SENT;
			break;
		case TAS_DONE:
			break;
		}
	}
#endif // WEB_TURBO_MODE

	SpdyHTTPRequestHandler *reqhandler = OP_NEW(SpdyHTTPRequestHandler, (req, mh, this));
	if (reqhandler)
	{
		INT32 streamId;
		TRAPD(ret, streamId = spdyHandler->CreateStreamL(reqhandler));
		if (OpStatus::IsError(ret))
		{
			OP_DELETE(reqhandler);
			return FALSE;
		}
		reqhandler->Into(&requests);
		reqhandler->SetStreamId(streamId);

		if (connected)
			SendData();

		generic_info.requests_sent_on_connection = TRUE;
		return TRUE;
	}
	return FALSE;
}

BOOL SpdyConnection::Idle()
{
	return requests.Empty() && AcceptNewRequests();
}

void SpdyConnection::RemoveRequest(HTTP_Request *req)
{
	for (SpdyHTTPRequestHandler *reqHandler = requests.First(); reqHandler; reqHandler = reqHandler->Suc())
	{
		if (req == reqHandler->GetRequest())
		{
			spdyHandler->RemoveStream(reqHandler->GetStreamId());
			reqHandler->Out();
			SafeDestruction(reqHandler);
			if (connected)
				SendData();
			return;
		}
	}
#ifdef WEB_TURBO_MODE
	if (req->GetSink() == this)
		req->PopSink();
	deferedRequests.RemoveByItem(req);
#endif // WEB_TURBO_MODE
}

void SpdyConnection::RemoveHandler(SpdyHTTPRequestHandler *handler)
{
	handler->Out();
	SafeDestruction(handler);
	if (generic_info.disable_more_requests && requests.Empty() && deferedRequests.GetCount() == 0)
		mh->PostMessage(MSG_COMM_LOADING_FINISHED,Id(), 0);
}

BOOL SpdyConnection::AcceptNewRequests()
{
	return spdyHandler->CanCreateNewStream() && !generic_info.disable_more_requests && !ProtocolComm::Closed() && !ProtocolComm::PendingClose();
}

BOOL SpdyConnection::HasId(unsigned int sid)
{
	if (sid == Id())
		return TRUE;
	for (SpdyHTTPRequestHandler *reqHandler = requests.First(); reqHandler; reqHandler = reqHandler->Suc())
		if (reqHandler->Id() == sid)
			return TRUE;
	return FALSE;
}

void SpdyConnection::SendData()
{
	if (aborted)
		return;

	pendingDataToSend = FALSE;
	isSendingData = TRUE;
	TRAPD(res, spdyHandler->SendDataL(GetSink()));
	isSendingData = FALSE;

	if (OpStatus::IsError(res))
		AbortConnection();
}

void SpdyConnection::RequestMoreData()
{
	if (!isSendingData && !sendDataMsgSent)
		SendData();
}

void SpdyConnection::ProcessReceivedData()
{
	if (aborted)
		return;

	TRAPD(res, spdyHandler->ProcessReceivedDataL(GetSink()));

	if (OpStatus::IsError(res))
		AbortConnection();
}

void SpdyConnection::AbortConnection()
{
	NormalCallCount blocker(this);
	aborted = TRUE;
	MoveUnfinishedRequestsToNewConnection();
	ProtocolComm::CloseConnection();
	ProtocolComm::Stop();
}

unsigned int SpdyConnection::ReadData(char* buf, unsigned blen)
{
	return 0;
}

void SpdyConnection::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_SPDY_SENDDATA:
		sendDataMsgSent = FALSE;
		SendData();
		break;
	case MSG_COMM_LOADING_FAILED:
	case MSG_COMM_LOADING_FINISHED:
		MoveUnfinishedRequestsToNewConnection();
	}
}

void SpdyConnection::SetProgressInformation(ProgressState progress_level, unsigned long progress_info1, const void *progress_info2)
{
	if (progress_level == SET_NEXTPROTOCOL)
	{
		OP_ASSERT(!connected);
		if (static_cast<NextProtocol>(progress_info1) == NP_SPDY2 || static_cast<NextProtocol>(progress_info1) == NP_SPDY3)
			return;

		NextProtocol protocol = static_cast<NextProtocol>(progress_info1);
		generic_info.disable_more_requests = TRUE;
		manager->NextProtocolNegotiated(this, protocol);
		return;
	}
	HttpRequestsHandler::SetProgressInformation(progress_level, progress_info1, progress_info2);
}


void SpdyConnection::OnHasDataToSend()
{
	pendingDataToSend = TRUE;
	if (!sendDataMsgSent && connected)
	{
		mh->PostMessage(MSG_SPDY_SENDDATA, reinterpret_cast<MH_PARAM_1>(this), 0);
		sendDataMsgSent = TRUE;
	}
}

void SpdyConnection::MoveUnfinishedRequestsToNewConnection()
{
	NormalCallCount blocker(this);
	generic_info.disable_more_requests = TRUE;
	SpdyHTTPRequestHandler *suc;
	for (SpdyHTTPRequestHandler *handler = requests.First(); handler; handler = suc)
	{
		suc = handler->Suc();
		handler->IfUnfinishedMoveToNewConnection();
	}

	while (deferedRequests.GetCount())
	{
		HTTP_Request *req = deferedRequests.Remove(0);
		req->SetProgressInformation(RESTART_LOADING,0,0);
		req->PopSink();
		req->Out();
		if(req->GetSendCount() >=5 || manager->AddRequest(req) == COMM_REQUEST_FAILED)
		{
			req->EndLoading();
#ifdef SCOPE_RESOURCE_MANAGER
			SetProgressInformation(REPORT_LOAD_STATUS, OpScopeResourceListener::LS_RESPONSE_FAILED, req);
#endif // SCOPE_RESOURCE_MANAGER
			mh->PostMessage(MSG_COMM_LOADING_FAILED, req->Id(), URL_ERRSTR(SI, ERR_COMM_INTERNAL_ERROR));
		}
	}
}

CommState SpdyConnection::ConnectionEstablished()
{
	connected = TRUE;
	return ProtocolComm::ConnectionEstablished();
}

void SpdyConnection::OnPingTimeout()
{
	AbortConnection();
}

void SpdyConnection::OnProtocolError()
{
	AbortConnection();
}

void SpdyConnection::OnGoAway()
{
	generic_info.disable_more_requests = TRUE;
}

unsigned int SpdyConnection::GetRequestCount() const
{
	return requests.Cardinal() + deferedRequests.GetCount();
}

unsigned int SpdyConnection::GetUnsentRequestCount() const
{
	return connected ? 0 : requests.Cardinal();
}

BOOL SpdyConnection::HasPriorityRequest(UINT priority)
{
	for (SpdyHTTPRequestHandler *handler = requests.First(); handler; handler = handler->Suc())
	{
		if (handler->GetRequest() && handler->GetRequest()->GetPriority() == (UINT)priority)
			return TRUE;
	}
	return FALSE;
}

BOOL SpdyConnection::HasRequests()
{
	return !requests.Empty() || deferedRequests.GetCount();
}

HTTP_Request *SpdyConnection::MoveLastRequestToANewConnection()
{
	// SPDY connections are not eager to share their requests with others
	return NULL;
}

void SpdyConnection::RestartRequests()
{
	AbortConnection();
}

#ifdef WEB_TURBO_MODE
void SpdyConnection::HandleTurboHeadersL(HeaderInfo *headerInfo, BOOL &shouldClearRequest)
{
	// Handle Turbo Host Header
	HeaderEntry *header = headerInfo->headers.GetHeader("X-Opera-Host", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		urlManager->SetWebTurboAvailable(TRUE);
		manager->SetTurboHostL(header->Value());
	}

	// Handle Turbo Proxy authentication challenge
	header = headerInfo->headers.GetHeader("X-OA", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		// Re-send request with auth info
		g_obml_id_manager->SetTurboProxyAuthChallenge(header->Value());
		if(headerInfo->response == HTTP_UNAUTHORIZED)
		{
			turboAuthorizationStatus = TAS_XOA_RECEIVED;
			shouldClearRequest  = TRUE;
		}
	}
	else if (headerInfo->response != HTTP_UNAUTHORIZED)
		turboAuthorizationStatus = TAS_DONE;

	// Handle Turbo Proxy compression ratio indicator
	header = headerInfo->headers.GetHeader("X-OC", NULL, HEADER_RESOLVE);
	if(header && header->Value())
	{
		op_sscanf(header->Value(), "%d/%d", &headerInfo->turbo_transferred_bytes, &headerInfo->turbo_orig_transferred_bytes);
#ifdef URL_NETWORK_DATA_COUNTERS
		g_network_counters.turbo_savings += (INT64)headerInfo->turbo_orig_transferred_bytes - (INT64)headerInfo->turbo_transferred_bytes;
#endif
	}
	
	while (turboAuthorizationStatus != TAS_FIRST_REQ_SENT && turboAuthorizationStatus != TAS_SECOND_REQ_SENT && deferedRequests.GetCount())
	{
		HTTP_Request *req = deferedRequests.Remove(0);
		req->PopSink();
		if (!AddRequest(req))
			LEAVE(OpStatus::ERR_NO_MEMORY);
	}
}
#endif // WEB_TURBO_MODE

#endif // USE_SPDY
