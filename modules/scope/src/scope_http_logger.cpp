/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2006-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined SCOPE_HTTP_LOGGER

#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_http_logger.h"
#include "modules/scope/src/scope_window_manager.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/dochand/win.h"
#include "modules/pi/OpSystemInfo.h"

/**
 * Stores information about an HTTP request/response identified by a given id.
 * This object will be updated when callbacks from the url module occur, for
 * instance to tell that the response headers have been loaded.
 * Once the requests is complete (response has been fully received) or something
 * fails the object will be deleted.
 * Also all open http objects will be deleted when the http-logger service is deleted.
 */
class OpScopeHttpInfo : public Link
{
public:
	OpScopeHttpInfo(void *ptr, UINT32 id, Window *window, const char *context)
		: ptr(ptr)
		, id(id)
		, window(window)
		, context(context)
		, time_request_sent(0.0)
		, time_response_header(0.0)
		, time_response_sent(0.0)
	{}

	void       *ptr;
	UINT32      id;
	Window     *window;
	const char *context;
	OpString8   response_header;

	double time_request_sent; ///< Time (in ms) when the request was sent.
	double time_response_header; ///< Time (in ms) when the headers for the response has been received.
	double time_response_sent; ///< Time (in ms) when the response has been full received.
};

OpScopeHttpLogger::OpScopeHttpLogger()
	: OpScopeHttpLogger_SI()
	, next_available_id(1)
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	, window_manager(NULL)
#endif // SCOPE_WINDOW_MANAGER_SUPPORT
{
}

/* virtual */
OpScopeHttpLogger::~OpScopeHttpLogger()
{
	requests.Clear();
}

/* virtual */
OP_STATUS
OpScopeHttpLogger::OnPostInitialization()
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	if (OpScopeServiceManager *manager = GetManager())
		window_manager = OpScopeFindService<OpScopeWindowManager>(manager, "window-manager");
	if (!window_manager)
		return OpStatus::ERR_NULL_POINTER;
#endif // SCOPE_WINDOW_MANAGER_SUPPORT

	return OpStatus::OK;
}

UINT32
OpScopeHttpLogger::GetNextId()
{
	return next_available_id++;
}

OP_STATUS
OpScopeHttpLogger::RequestComposed(void *id, Window *window)
{
	if (IsEnabled())
	{
		OpScopeHttpInfo *info = OP_NEW(OpScopeHttpInfo, (id, GetNextId(), window, NULL));
		RETURN_OOM_IF_NULL(info);
		info->Into(&requests);
	}

	return OpStatus::OK;
}

OP_STATUS 
OpScopeHttpLogger::RequestSent(void* ptr, const char* ctx, const char* buf, size_t buf_len)
{
	if (IsEnabled())
	{
		double time = g_op_time_info->GetTimeUTC();

		OpScopeHttpInfo *info = reinterpret_cast<OpScopeHttpInfo *>(requests.First());
		for (; info; info = reinterpret_cast<OpScopeHttpInfo *>(info->Suc()))
		{
			if (info->ptr == ptr)
				break;
		}

		if (!info || !AcceptWindow(info->window))
			return OpStatus::OK;

		info->time_request_sent = time;
		Header msg;
		RETURN_IF_ERROR(UpdateHeader(msg, ptr, info->id, ctx, buf, buf_len, info->window, time));
		return SendOnRequest(msg);
	}
	else
		return OpStatus::OK;
}

OP_STATUS 
OpScopeHttpLogger::HeaderLoaded(void* ptr, const char* ctx, const char* buf, size_t buf_len)
{
	if (IsEnabled())
	{
		double time = g_op_time_info->GetTimeUTC();
		OpScopeHttpInfo *info = reinterpret_cast<OpScopeHttpInfo *>(requests.First());
		for (; info; info = reinterpret_cast<OpScopeHttpInfo *>(info->Suc()))
		{
			if (info->ptr == ptr)
				break;
		}
		if (info)
		{
			RETURN_IF_ERROR(info->response_header.Set(buf, buf_len));
			info->time_response_header = time;
			return OpStatus::OK;
		}
		else
			return OpStatus::ERR;
	}
	else
		return OpStatus::OK;
}

OP_STATUS
OpScopeHttpLogger::ResponseFailed(void* ptr, const char* ctx)
{
	return ResponseReceived(ptr, ctx);
}

OP_STATUS
OpScopeHttpLogger::ResponseReceived(void* ptr, const char* ctx)
{
	if (IsEnabled())
	{
		double time = g_op_time_info->GetTimeUTC();
		OpScopeHttpInfo *info = reinterpret_cast<OpScopeHttpInfo *>(requests.First());
		for (; info; info = reinterpret_cast<OpScopeHttpInfo *>(info->Suc()))
		{
			if (info->ptr == ptr)
				break;
		}
		if (info)
		{
			info->time_response_sent = time;
			OP_STATUS status = OpStatus::OK;
			if (AcceptWindow(info->window))
			{
				Header msg;
				RETURN_IF_ERROR(UpdateHeader(msg, ptr, info->id, ctx, info->response_header.CStr(), info->response_header.Length(), info->window, info->time_response_sent));
				status = SendOnResponse(msg);
			}
			info->Out();
			OP_DELETE(info);
			return status;
		}
		else
			return OpStatus::ERR;
	}
	else
		return OpStatus::OK;
}

/*virtual*/
OP_STATUS
OpScopeHttpLogger::OnServiceDisabled()
{
	requests.Clear();

	return OpStatus::OK;
}

/* virtual */ void
OpScopeHttpLogger::WindowClosed(Window *window)
{
	// Clear OpScopeHttpInfo items associated with the window that was just closed.
	// (Responses can occur after the Window has closed, see CORE-35300.

	OpScopeHttpInfo *info = reinterpret_cast<OpScopeHttpInfo *>(requests.First());

	while (info)
	{
		OpScopeHttpInfo *next = reinterpret_cast<OpScopeHttpInfo *>(info->Suc());

		if (info->window == window)
		{
			info->Out();
			OP_DELETE(info);
		}

		info = next;
	}
}

BOOL
OpScopeHttpLogger::AcceptWindow(Window* window)
{
#ifdef SCOPE_WINDOW_MANAGER_SUPPORT
	return window_manager && window_manager->AcceptWindow(window);
#else
	return TRUE;
#endif
}

OP_STATUS
OpScopeHttpLogger::UpdateHeader(Header &msg, void* ptr, UINT32 id, const char* ctx, const char* buf, size_t buf_len, Window* window, double time)
{
	uni_char tempbuffer[44]; // ARRAY OK 2008-08-18 jhoff

	msg.SetRequestID(id);
	msg.SetWindowID(window ? window->Id() : 0);
	uni_snprintf(tempbuffer, sizeof(tempbuffer), UNI_L("%f"), time);
	RETURN_IF_ERROR(msg.SetTime(tempbuffer));
	RETURN_IF_ERROR(msg.SetHeader(buf, buf_len));

	return OpStatus::OK;
}

#endif // SCOPE_HTTP_LOGGER
