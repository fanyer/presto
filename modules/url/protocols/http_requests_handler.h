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

#ifndef _HTTP_REQUESTS_HANDLER_H_
#define _HTTP_REQUESTS_HANDLER_H_

#include "modules/url/protocols/pcomm.h"
#include "modules/prefs/prefsmanager/collections/pc_network.h"

class HTTP_Request;
class HTTP_Connection;
class HTTP_Server_Manager;

/* 
 * Abstract base for classes that are able to handle HTTP requests (e.g. HTTP_1_1, SpdyConnection).
 */
class HttpRequestsHandler: public ProtocolComm
{
	friend class HTTP_Server_Manager;
protected:
	struct http_requests_handler_info_st
	{
		unsigned requests_sent_on_connection:1; // True if connection has been used for requests. False if connection have been idle up to now.
		unsigned disable_more_requests:1;
#ifdef WEBSERVER_SUPPORT
		unsigned unite_connection:1;
		unsigned unite_admin_connection:1;
#endif // WEBSERVER_SUPPORT
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
		unsigned load_direct:1;
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef WEB_TURBO_MODE
		unsigned turbo_enabled:1;
#endif // WEB_TURBO_MODE
		unsigned open_in_privacy_mode:1;
	} generic_info;

	HTTP_Server_Manager *manager;
	HTTP_Connection *conn_elem;
	ServerName_Pointer server_name;
	unsigned short port;

public:
	HttpRequestsHandler(ServerName_Pointer server_name, unsigned short port, BOOL privacy_mode, MessageHandler* msg_handler);
	virtual ~HttpRequestsHandler();

	void SetManager(HTTP_Server_Manager *mgr) { manager = mgr; }
	HTTP_Server_Manager * GetManager(){ return manager; }
	void SetConnElement(HTTP_Connection *elm) { conn_elem = elm; }
	void SetNoMoreRequests() { generic_info.disable_more_requests = TRUE; }
	BOOL WasUsed() { return generic_info.requests_sent_on_connection; }
#ifdef WEBSERVER_SUPPORT
	BOOL IsUniteConnection() { return generic_info.unite_connection; }
#endif // WEBSERVER_SUPPORT
#if defined(_EMBEDDED_BYTEMOBILE) || defined(WEB_TURBO_MODE)
	void SetLoadDirect(){generic_info.load_direct = TRUE;}
	BOOL GetLoadDirect(){return generic_info.load_direct;}
#endif // _EMBEDDED_BYTEMOBILE || WEB_TURBO_MODE
#ifdef WEB_TURBO_MODE
	void SetTurboOptimized() { generic_info.turbo_enabled = TRUE; }
	BOOL GetTurboOptimized() { return generic_info.turbo_enabled; }
#endif // WEB_TURBO_MODE
	BOOL GetOpenInPrivacyMode() { return generic_info.open_in_privacy_mode; }

	virtual void ConstructL();
	virtual OP_STATUS SetCallbacks(MessageObject* master, MessageObject* parent);

	virtual void RemoveRequest(HTTP_Request *) = 0;
	virtual BOOL AddRequest(HTTP_Request *, BOOL force_first = FALSE) = 0;
	virtual unsigned int GetRequestCount() const = 0;
	virtual unsigned int GetUnsentRequestCount() const = 0;
	virtual BOOL HasPriorityRequest(UINT priority) = 0;
	virtual BOOL HasRequests() = 0;
	virtual HTTP_Request *MoveLastRequestToANewConnection() = 0;
	virtual BOOL Idle() = 0;
	virtual BOOL AcceptNewRequests() = 0;
	virtual void RestartRequests() = 0;
};

#endif // _HTTP_REQUESTS_HANDLER_H_