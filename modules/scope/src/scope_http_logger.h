/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_HTTP_LOGGER_H
#define SCOPE_HTTP_LOGGER_H

#ifdef SCOPE_HTTP_LOGGER

#include "modules/scope/src/scope_service.h"
#include "modules/scope/src/generated/g_scope_http_logger_interface.h"

class Window;
class OpScopeWindowManager;

class OpScopeHttpLogger
	: public OpScopeHttpLogger_SI
{
public:
	OpScopeHttpLogger();
	virtual ~OpScopeHttpLogger();

	virtual OP_STATUS OnPostInitialization();

	/**
	 * Called right before a request is compised.
	 *
	 * @param id   Identifies the request, though perhaps not uniquely across
	 *             time -- it is the address of the request object.
	 * @param window The Window this request originates from.
	 */
	OP_STATUS RequestComposed(void *id, Window *window);

	/**
	 * Called from the URL layer when a request has been sent.
	 *
	 * @param id   Identifies the request, though perhaps not uniquely across
	 *             time -- it is the address of the request object.
	 * @param ctx  A string identifying the context, if the context is known;
	 *             it may be eg "page-load", "xmlhttprequest".  It may be NULL.
	 * @param buf  The request that was sent
	 * @param len  The length of the request
	 */
	OP_STATUS RequestSent(void* id, const char* ctx, const char* buf, size_t len);

	/**
	 * Called from the URL layer when a header has been completely loaded
	 * from a server response.
	 *
	 * @param id   Identifies the request that caused this response to be
	 *             generated, though perhaps not uniquely across time -- it
	 *             is the address of the request object.
	 * @param ctx  A string identifying the context, if the context is known;
	 *             it may be eg "page-load", "xmlhttprequest".  It may be NULL.
	 * @param buf  The header that was received
	 * @param len  The length of the header
	 */
	OP_STATUS HeaderLoaded(void* id, const char* ctx, const char* buf, size_t len);

	/**
	 * Called from the URL layer when a reponse has failed to load.
	 *
	 * @param id   Identifies the request that caused this response to be
	 *             generated, though perhaps not uniquely across time -- it
	 *             is the address of the request object.
	 * @param ctx  A string identifying the context, if the context is known;
	 *             it may be eg "page-load", "xmlhttprequest".  It may be NULL.
	 */
	OP_STATUS ResponseFailed(void* id, const char* ctx);

	/**
	 * Called from the URL layer when the reponse has been completely received.
	 *
	 * @param id   Identifies the request that caused this response to be
	 *             generated, though perhaps not uniquely across time -- it
	 *             is the address of the request object.
	 * @param ctx  A string identifying the context, if the context is known;
	 *             it may be eg "page-load", "xmlhttprequest".  It may be NULL.
	 */
	OP_STATUS ResponseReceived(void* id, const char* ctx);

	/**
	 * Disables the service and removes all open requests.
	 */
	virtual OP_STATUS OnServiceDisabled();

	// From OpScopeService
	virtual void WindowClosed(Window *window);

private:
	BOOL AcceptWindow(Window* window);
	UINT32 next_available_id;

private:
    OP_STATUS UpdateHeader(Header &msg, void* ptr, UINT32 id, const char* ctx, const char* buf, size_t buf_len, Window* window, double time);
	UINT32    GetNextId();
	OP_STATUS Send(const uni_char* tag, void* ptr, UINT32 id, const char* ctx, const char* buf, size_t buf_len, Window* window, double time);
	void SendL(const uni_char* tag, void* ptr, UINT32 id, const char* ctx, const char* buf, size_t buf_len, Window* window, double time);

	Head requests; ///< Contains information on open requests of class OpScopeHttpInfo
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	OpScopeWindowManager *window_manager;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
};

#endif // SCOPE_HTTP_LOGGER

#endif // SCOPE_HTTP_LOGGER_H
