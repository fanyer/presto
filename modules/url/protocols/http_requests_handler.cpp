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
#include "modules/url/protocols/http_requests_handler.h"

HttpRequestsHandler::HttpRequestsHandler(ServerName_Pointer server_name, unsigned short port, BOOL privacy_mode, MessageHandler* msg_handler):
	ProtocolComm(msg_handler, NULL, NULL), 
	server_name(server_name),
	port(port)
{
	generic_info.disable_more_requests = FALSE;
	generic_info.requests_sent_on_connection = FALSE;
#ifdef WEBSERVER_SUPPORT
	generic_info.unite_connection = FALSE;
	generic_info.unite_admin_connection = FALSE;
#endif // WEBSERVER_SUPPORT
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	generic_info.load_direct = FALSE;
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef WEB_TURBO_MODE
	generic_info.turbo_enabled = FALSE;
#endif // WEB_TURBO_MODE
	generic_info.open_in_privacy_mode = privacy_mode;
}

HttpRequestsHandler::~HttpRequestsHandler()
{
	mh->UnsetCallBack(this, MSG_COMM_LOADING_FINISHED);
	mh->UnsetCallBack(this, MSG_COMM_LOADING_FAILED);
}

void HttpRequestsHandler::ConstructL()
{
	if(HTTP_TmpBuf == NULL || HTTP_TmpBufSize != ((unsigned long) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize))*1024)
	{
		unsigned long tmp_len = ((unsigned long) g_pcnet->GetIntegerPref(PrefsCollectionNetwork::NetworkBufferSize))*1024;
		char *tmp_buf = OP_NEWA_L(char, tmp_len+1);
		OP_DELETEA(HTTP_TmpBuf);
		HTTP_TmpBuf = tmp_buf;
		HTTP_TmpBufSize = tmp_len;
	}
}

OP_STATUS HttpRequestsHandler::SetCallbacks(MessageObject *master, MessageObject *parent)
{
	static const OpMessage messages[] =
	{
		MSG_COMM_LOADING_FINISHED,
		MSG_COMM_LOADING_FAILED
	};

	RETURN_IF_ERROR(mh->SetCallBackList((parent ? parent : master), Id(), messages, ARRAY_SIZE(messages)));
	RETURN_IF_ERROR(mh->SetCallBack(this, MSG_COMM_NEW_REQUEST, Id()));

	return ProtocolComm::SetCallbacks(master,this);
}
