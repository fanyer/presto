/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SCOPE_RESOURCE_MANAGER

#include "modules/scope/scope_network_listener.h"
#include "modules/scope/src/scope_manager.h"
#include "modules/scope/src/scope_resource_manager.h"
#include "modules/scope/src/scope_http_logger.h"
#include "modules/dochand/docman.h"

/* static */ BOOL
OpScopeResourceListener::IsEnabled()
{
	return g_scope_manager && (
		g_scope_manager->resource_manager && g_scope_manager->resource_manager->IsEnabled()
#ifdef SCOPE_HTTP_LOGGER
		|| g_scope_manager->http_logger && g_scope_manager->http_logger->IsEnabled()
#endif // SCOPE_HTTP_LOGGER
		);
}

/*static*/ OP_STATUS
OpScopeResourceListener::OnUrlLoad(URL_Rep *url_rep, DocumentManager *docman, Window *window)
{
#ifdef SCOPE_HTTP_LOGGER
	if (g_scope_manager && g_scope_manager->http_logger)
	{
		URLType url_type = (URLType)url_rep->GetAttribute(URL::KType);
		if (url_type == URL_HTTP || url_type == URL_HTTPS)
		{
			void *url_id_ptr = reinterpret_cast<void *>(static_cast<UINTPTR>(url_rep->GetID()));
			Window *w = window;
			if (!w && docman)
				w = docman->GetWindow();
			RETURN_IF_MEMORY_ERROR(g_scope_manager->http_logger->RequestComposed(url_id_ptr, w));
		}
	}
#endif // SCOPE_HTTP_LOGGER
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnUrlLoad(url_rep, docman, window);
	return OpStatus::ERR;
}

/*static*/ OP_STATUS
OpScopeResourceListener::OnUrlFinished(URL_Rep *url_rep, URLLoadResult result)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnUrlFinished(url_rep, result);
	return OpStatus::ERR;
}

/*static*/ OP_STATUS
OpScopeResourceListener::OnUrlRedirect(URL_Rep *orig_url_rep, URL_Rep *new_url_rep)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnUrlRedirect(orig_url_rep, new_url_rep);
	return OpStatus::ERR;
}

/*static*/ OP_STATUS
OpScopeResourceListener::OnUrlUnload(URL_Rep *rep)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnUrlUnload(rep);
	return OpStatus::ERR;
}


/* static */ OP_STATUS
OpScopeResourceListener::OnComposeRequest(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload, DocumentManager *docman, Window *window)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnComposeRequest(url_rep, request_id, upload, docman, window);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnRequest(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload, const char* buf, size_t len)
{
#ifdef SCOPE_HTTP_LOGGER
	if (g_scope_manager && g_scope_manager->http_logger)
	{
		void *url_id_ptr = reinterpret_cast<void *>(static_cast<UINTPTR>(url_rep->GetID()));
		RETURN_IF_MEMORY_ERROR(g_scope_manager->http_logger->RequestSent(url_id_ptr, NULL, buf, len));
	}
#endif // SCOPE_HTTP_LOGGER

	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnRequest(url_rep, request_id, upload, buf, len);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnRequestFinished(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnRequestFinished(url_rep, request_id, upload);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnRequestRetry(URL_Rep *url_rep, int orig_request_id, int new_request_id)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnRequestRetry(url_rep, orig_request_id, new_request_id);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnResponse(URL_Rep *url_rep, int request_id, int response_code)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnResponse(url_rep, request_id, response_code);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnResponseHeader(URL_Rep *url_rep, int request_id, const HeaderList *headerList, const char* buf, size_t len)
{
#ifdef SCOPE_HTTP_LOGGER
	if (g_scope_manager && g_scope_manager->http_logger)
	{
		void *url_id_ptr = reinterpret_cast<void *>(static_cast<UINTPTR>(url_rep->GetID()));
		RETURN_IF_MEMORY_ERROR(g_scope_manager->http_logger->HeaderLoaded(url_id_ptr, NULL, buf, len));
	}
#endif // SCOPE_HTTP_LOGGER

	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnResponseHeader(url_rep, request_id, headerList, buf, len);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnResponseFinished(URL_Rep *url_rep, int request_id)
{
#ifdef SCOPE_HTTP_LOGGER
	if (g_scope_manager && g_scope_manager->http_logger)
	{
		void *url_id_ptr = reinterpret_cast<void *>(static_cast<UINTPTR>(url_rep->GetID()));
		RETURN_IF_MEMORY_ERROR(g_scope_manager->http_logger->ResponseReceived(url_id_ptr, NULL));
	}
#endif // SCOPE_HTTP_LOGGER

	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnResponseFinished(url_rep, request_id);
	return OpStatus::ERR;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnResponseFailed(URL_Rep *url_rep, int request_id)
{
#ifdef SCOPE_HTTP_LOGGER
	if (g_scope_manager && g_scope_manager->http_logger)
	{
		void *url_id_ptr = reinterpret_cast<void *>(static_cast<UINTPTR>(url_rep->GetID()));
		RETURN_IF_MEMORY_ERROR(g_scope_manager->http_logger->ResponseFailed(url_id_ptr, NULL));
	}
#endif // SCOPE_HTTP_LOGGER

	return OpStatus::ERR;
}

/* static */ BOOL
OpScopeResourceListener::ForceFullReload(URL_Rep *url_rep, DocumentManager *docman, Window *window)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->ForceFullReload(url_rep, docman, window);
	return FALSE;
}

/* static */ OP_STATUS
OpScopeResourceListener::OnXHRLoad(URL_Rep *url_rep, BOOL cached, DocumentManager *docman, Window *window)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->OnXHRLoad(url_rep, cached, docman, window);
	return OpStatus::ERR;
}

/* static */ int
OpScopeResourceListener::GetHTTPRequestID(URL_Rep *url_rep)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		return g_scope_manager->resource_manager->GetHTTPRequestID(url_rep);
	return -1;
}

/* static */ void
OpScopeResourceListener::OnDNSResolvingStarted(URL_Rep *url_rep)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
	{
		const ServerName *server = static_cast<const ServerName *>(url_rep->GetAttribute(URL::KServerName, NULL));
		g_scope_manager->resource_manager->OnDNSStarted(url_rep, g_url_dns_manager->GetPrefetchTimeSpent(server));
	}
}

/* static */ void
OpScopeResourceListener::OnDNSResolvingEnded(URL_Rep *url_rep, OpHostResolver::Error errorcode /* OpHostResolver::NETWORK_NO_ERROR */)
{
	if (g_scope_manager && g_scope_manager->resource_manager)
		g_scope_manager->resource_manager->OnDNSEnded(url_rep, errorcode);
}

#endif // SCOPE_RESOURCE_MANAGER
