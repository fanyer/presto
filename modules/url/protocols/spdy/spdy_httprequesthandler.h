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

#ifndef _SPDY_HTTPREQUESTHANDLER_H_
#define _SPDY_HTTPREQUESTHANDLER_H_

#include "modules/url/protocols/spdy/spdy_protocol.h"
#include "modules/url/protocols/spdy/spdy_internal/spdy_headers.h"
#include "modules/url/protocols/http_req2.h"

class SpdyConnection;

class SpdyHTTPRequestHandler: public SpdyStreamController, public ListElement<SpdyHTTPRequestHandler>, public ProtocolComm
{
	HTTP_Request *request;
	OpData data;
	OpData headersBuffer;
	UINT32 streamId;
	BOOL headersLoaded;
	SpdyConnection *master;
	BOOL postedUploadInfoMsg;
	size_t uploadProgressDelta;

	int AddHeadersL(SpdyVersion version, Header_List& headers, SpdyHeaders::HeaderItem *headersArray) const;
	size_t ComposeHeadersString(const SpdyHeaders &headers, SpdyVersion version, char *target, BOOL measureOnly);
	void HandleTrailingHeadersL(const SpdyHeaders &headers, SpdyVersion version);
#ifdef WEB_TURBO_MODE
	void HandleBypassIfNeeded();
	void HandleBypassResponse(int errorCode);
#endif // WEB_TURBO_MODE
public:
	SpdyHTTPRequestHandler(HTTP_Request *req, MessageHandler *mh, SpdyConnection *master);
	virtual ~SpdyHTTPRequestHandler();
	
	void SetStreamId(UINT32 id) { streamId = id; }
	UINT32 GetStreamId() const { return streamId; }
	const HTTP_Request *GetRequest() const { return request; }
	void IfUnfinishedMoveToNewConnection();

	// methods inherited from SpdyStreamControler
	virtual SpdyHeaders *GetHeadersL(SpdyVersion version) const;
	virtual void OnHeadersLoadedL(const SpdyHeaders &headers, SpdyVersion version);
	virtual void OnDataLoadedL(const OpData &data);
	virtual void OnDataLoadingFinished();
	virtual UINT8 GetPriority() const;
	virtual BOOL HasDataToSend() const;
	virtual size_t GetNextDataChunkL(char *buffer, size_t bufferLength, BOOL &more);
	virtual void OnResetStream();
	virtual void OnGoAway();

	// methods inherited from ProtocolComm
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	virtual unsigned int ReadData(char* buf, unsigned blen);
};


#endif // _SPDY_HTTPREQUESTHANDLER_H_
