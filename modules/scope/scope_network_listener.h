/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_SCOPE_NETWORK_LISTENER_H
#define OP_SCOPE_NETWORK_LISTENER_H

#ifdef SCOPE_RESOURCE_MANAGER

class Window;
class DocumentManager;
class HeaderList;
class Header_List;
class Upload_EncapsulatedElement;
class URL_Rep;
#include "modules/pi/network/OpHostResolver.h"
#include "modules/url/url_dns_man.h"

/**
 * A 'resource' is any item used by a Window to compose a page, HTML files,
 * CSS files, images, and so forth.
 *
 * Use this interface to notify the client of network activity, HTTP headers,
 * and other events related to resources.
 */
class OpScopeResourceListener
{
public:

	enum LoadStatus
	{
		LS_COMPOSE_REQUEST = 1,
		LS_REQUEST,
		LS_REQUEST_FINISHED,
		LS_RESPONSE,
		LS_RESPONSE_HEADER,
		LS_RESPONSE_FINISHED,
		LS_RESPONSE_FAILED,
		LS_DNS_LOOKUP_START,
		LS_DNS_LOOKUP_END
	}; // LoadStatus

	/**
	 * Check whether the HTTP listener is enabled. If it's not enabled,
	 * there is no point is calling the functions (they will be ignored).
	 * 
	 * @return TRUE if enabled, FALSE otherwise.
	 */
	static BOOL IsEnabled();

	/**
	 * Called from the URL layer when loading of a URL is started.
	 * A URL load always happens before an OnRequest (and related callbacks).
	 *
	 * @param url_rep The URL this load applies to.
	 * @param docman The originating DocumentManager (may be NULL).
	 * @param window The originating Window (may be NULL).
	 * @return OpStatus::OK
	 */
	static OP_STATUS OnUrlLoad(URL_Rep *url_rep, DocumentManager *docman, Window *window);

	/**
	 * Called from the URL layer when loading of a URL is finished or failed.
	 *
	 * @param url_rep The URL that was finished.
	 * @param result The result of the URL load, should be
	 *               URL_LOAD_FINISHED for success or
	 *               URL_LOAD_FAILED for failure or
	 *               URL_LOAD_STOPPED for manual stop
	 * @return OpStatus::OK
	 */
	static OP_STATUS OnUrlFinished(URL_Rep *url_rep, URLLoadResult result);

	/**
	 * Called from the URL layer when a URL redirection occurs.
	 *
	 * @param orig_url_rep The original URL that resulted in the redirect.
	 * @param new_url_rep The URL the redirection goes to.
	 * @return OpStatus::OK
	 */
	static OP_STATUS OnUrlRedirect(URL_Rep *orig_url_rep, URL_Rep *new_url_rep);

	/**
	 * Called from the URL layer when a URL is no longer in use.
	 * This will allow for cleaning up references to URL_Rep objects.
	 *
	 * @param url The URL_Rep that was destroyed.
	 * @return OpStatus::OK
	 */
	static OP_STATUS OnUrlUnload(URL_Rep *rep);

	/**
	 * Called from the URL layer when a request is about to be serialized into
	 * data to be sent over the network. Calling this will give the debugger
	 * a chance to add or remove headers from the request.
	 *
	 * @param url_rep The URL this request applies to.
	 * @param request_id Globally unique identifier for the request.
	 * @param upload The element from which we're about to compose a request.
	 * @param docman The originating DocumentManager (may be NULL).
	 * @param window The originating Window (may be NULL).
	 * @return OpStatus::OK, or OOM.
	 */
	static OP_STATUS OnComposeRequest(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload, DocumentManager *docman, Window *window);

	/**
	 * Called from the URL layer when a request has been sent.
	 *
	 * @param url_rep The URL this request applies to.
	 * @param request_id Globally unique identifier for the request.
	 * @param upload The element we're about to send.
	 * @param buf The request that was sent.
	 * @param len The length of the request buffer.
	 * @return OpStatus::OK
	 */
	static OP_STATUS OnRequest(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload, const char* buf, size_t len);

	/**
	 * Called after a POST request is made. This may be called many time for the same POST.
	 *
	 * @param url_rep The URL this request applies to.
 	 * @param request_id Globally unique identifier for the request.
	 * @param upload The element that was just sent.
	 */
	static OP_STATUS OnRequestFinished(URL_Rep *url_rep, int request_id, Upload_EncapsulatedElement *upload);

	/**
	 * Indicate that a request is about to be retried, due to some HTTP connection error. After this
	 * event has been observed, we do not expect to get more events from the request tracked by
	 * 'orig_request_id'.
	 *
	 * @param url_rep The URL this request applies to.
	 * @param orig_request_id The original request ID for the request.
	 * @param new_request_id The new request ID for the (same) request.
	 * @return OpStatus::OK
	 */
	static OP_STATUS OnRequestRetry(URL_Rep *url_rep, int orig_request_id, int new_request_id);

	/**
	 * Called at 'first contact' from the server, before headers are parsed.
	 *
	 * @param url The URL this response applies to.
 	 * @param request_id Globally unique identifier for the request.
	 * @param response_code The HTTP response code for the request, e.g. 200, 304, etc.
	 */
	static OP_STATUS OnResponse(URL_Rep *url_rep, int request_id, int response_code);

	/**
	 * Called from the URL layer when a header has been completely loaded
	 * from a server response.
	 *
	 * @param url_rep The URL this request applies to.
	 * @param request_id Globally unique identifier for the request.
	 * @param headerList A list of parsed headers.
	 * @param buf The raw response header.
	 * @param len The length of the response buffer.
	 */
	static OP_STATUS OnResponseHeader(URL_Rep *url_rep, int request_id, const HeaderList *headerList, const char* buf, size_t len);

	/**
	 * Called from the URL layer when a response has finished
	 *
	 * @param url_rep The URL this event applies to.
	 * @param request_id Globally unique identifier for the request.
	 */
	static OP_STATUS OnResponseFinished(URL_Rep *url_rep, int request_id);

	/**
	 * Called from the URL layer when a response has failed to load.
	 *
	 * @param url_rep The URL this request applies to.
	 * @param request_id Globally unique identifier for the request.
	 */
	static OP_STATUS OnResponseFailed(URL_Rep *url_rep, int request_id);

	/**
	 * Called when the URL layer needs to know whether a resource should be retrived over
	 * the network, even if it's available in cache.
	 *
	 * @param url_rep The URL this request applies to.
	 * @param docman The originating DocumentManager (may be NULL).
	 * @param window The originating Window (may be NULL).
	 * @return TRUE to force a full reload. FALSE does not guarantee that the resource
	 *         is *not* retrieved over the network -- it simply means "do not change load
	 *         policy".
	 */
	static BOOL ForceFullReload(URL_Rep *url_rep, DocumentManager *docman, Window *window);

	/**
	 * Called when a XHR is about to be loaded, either from cache or from the network.
	 * This is necessary because DOM_LSLoader implements its own caching rules, and
	 * can in some cases not involve the network layer at all.
	 *
	 * When this function is called, and a cached load is indicated, Scope may send
	 * events to the client indicating that a cached XHR load took place.
	 *
	 * @param url_rep The URL this load applies to.
	 * @param cached TRUE if the network layer is not involved in the load, FALSE otherwise.
	 * @param docman The originating DocumentManager (may be NULL).
	 * @param window The originating Window (may be NULL).
	 */
	static OP_STATUS OnXHRLoad(URL_Rep *url_rep, BOOL cached, DocumentManager *docman, Window *window);

	/**
	 * Return the http request-id registered in OnRequest. -1 is returned if not found.
	 *
	 * @param url_rep The URL we want the http request from.
	 */
	static int GetHTTPRequestID(URL_Rep *url_rep);

	/**
	 * Called when DNS resolving is started.
	 *
	 * @param url_rep The URL resolving is connected to.
	 */
	static void OnDNSResolvingStarted(URL_Rep *url_rep);

	/**
	 * Called when a DNS resolve action has completed, also called when a resolve action fails.
	 *
	 * @param url_rep The URL DNS resolving ended.
	 * @param errorcode Status of the resolving.
	 */
	static void OnDNSResolvingEnded(URL_Rep *url_rep, OpHostResolver::Error errorcode);

}; // OpScopeResourceListener


#endif // SCOPE_RESOURCE_MANAGER

#endif // OP_SCOPE_NETWORK_LISTENER_H
