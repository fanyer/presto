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


#ifndef _SPDY_CONNECTION_H_
#define _SPDY_CONNECTION_H_

#include "modules/url/protocols/spdy/spdy_protocol.h"
#include "modules/url/protocols/http1.h"
#include "modules/hardcore/timer/optimer.h"

class SpdyHTTPRequestHandler;

class SpdyConnection: private SpdyFramesHandlerListener, public HttpRequestsHandler
{
	List<SpdyHTTPRequestHandler> requests;
	SpdyFramesHandler *spdyHandler;
	BOOL sendDataMsgSent;
	BOOL connected;
	BOOL aborted;
	NextProtocol protocol;
	BOOL isSendingData;
	BOOL pendingDataToSend;
	BOOL secureMode;

private:
#ifdef WEB_TURBO_MODE
	enum TurboAuthorizationStatus
	{
		TAS_START,
		TAS_FIRST_REQ_SENT,
		TAS_XOA_RECEIVED,
		TAS_SECOND_REQ_SENT,
		TAS_DONE,
	} turboAuthorizationStatus;
	OpVector<HTTP_Request> deferedRequests;
#endif // WEB_TURBO_MODE

	virtual void OnPingTimeout();
	virtual void OnHasDataToSend();
	virtual void OnProtocolError();
	virtual void OnGoAway();
	void SendData();
	void AbortConnection();
public:
	SpdyConnection(NextProtocol protocol, MessageHandler* msg_handler, ServerName_Pointer hostname, unsigned short portnumber, BOOL privacy_mode, BOOL secureMode);
	~SpdyConnection();
	
	virtual void ConstructL();
	virtual void RequestMoreData();
	virtual void RemoveRequest(HTTP_Request *req);
	virtual unsigned int ReadData(char* buf, unsigned blen);
	virtual void ProcessReceivedData();
	virtual BOOL AddRequest(HTTP_Request *, BOOL force_first = FALSE);
	virtual BOOL SafeToDelete() const;
	virtual BOOL IsNeededConnection() const;
	virtual BOOL Idle();
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual BOOL HasId(unsigned int sid);
	virtual BOOL AcceptNewRequests();
	virtual void SetProgressInformation(ProgressState progress_level, unsigned long progress_info1, const void *progress_info2);
	virtual CommState ConnectionEstablished();
	virtual unsigned int GetRequestCount() const;
	virtual unsigned int GetUnsentRequestCount() const;
	virtual BOOL HasPriorityRequest(UINT priority);
	virtual BOOL HasRequests();
	virtual HTTP_Request *MoveLastRequestToANewConnection();
	virtual void RestartRequests();

	void MoveUnfinishedRequestsToNewConnection();
	void RemoveHandler(SpdyHTTPRequestHandler *handler);
#ifdef WEB_TURBO_MODE
	void HandleTurboHeadersL(HeaderInfo *headerInfo, BOOL &shouldClearRequest);
#endif // WEB_TURBO_MODE
};

#endif // _SPDY_CONNECTION_H_