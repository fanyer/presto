/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef CORS_MANAGER_H
#define CORS_MANAGER_H

#ifdef CORS_SUPPORT
class OpCrossOrigin_PreflightCache;
class OpCrossOrigin_Request;
class OpCrossOrigin_Loader;
class OpSecurityCheckCancel;
class OpSecurityCheckCallback;

/** The CORS manager class, controlling access to a set of request
    URLs with respect to a set of origins. */
class OpCrossOrigin_Manager
{
public:
	static OP_STATUS Make(OpCrossOrigin_Manager *&instance, unsigned default_max_age = SECMAN_DEFAULT_PREFLIGHT_MAX_AGE);
	/**< Create a new instance, using 'default_max_age' for preflight
	     cache max-age entries if responses lack them.

	     @param [out]instance If successful, the result.
	     @param default_max_age The number of seconds the result of
	            a preflight request can be reused before invalidated.
	            The default is given to a cache entry if the server
	            doesn't supply an explicit max-age value or the
	            one given is invalid.
	     @return OpStatus::OK on successful creation, OpStatus::ERR_NO_MEMORY
	             on OOM. */

	~OpCrossOrigin_Manager();

	/** CORS failure modes; used to provide information back to the
	    user why a CORS check failed. */
	enum FailureReason
	{
		IncorrectResponseCode,
		MissingAllowOrigin,
		MultipleAllowOrigin,
		NoMatchAllowOrigin,
		InvalidAllowCredentials,
		UnsupportedScheme,
		RedirectLoop,
		UserinfoRedirect
	};

	OP_STATUS VerifyCrossOriginAccess(OpCrossOrigin_Request *request, const URL &response_url, BOOL &allowed);
	/**< Verify the response to the actual cross-origin request,
	     checking if the target resource permits CORS access.
	     This is a synchronous check over the response headers,
	     with the outcome recorded via 'allowed'.

	     @param request The cross-origin accessed that was requested.
	     @param response_url The URL holding the response headers.
	     @param [out] allowed If this method returns OpStatus::OK,
	            the outcome of the security check.
	     @return OpStatus::OK if check was performed successfully;
	             OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS HandleRequest(OpCrossOrigin_Request *request, const URL &origin_url, const URL &target_url, OpSecurityCheckCallback *security_callback, OpSecurityCheckCancel **security_cancel);
	/**< Stage 1 check of cross-origin access request. If the request
	     is either simple (in CORS terms) or the request is known
	     to be allowed by consulting the preflight cache, signal
	     it via the callback that it is allowed to go ahead. If a
	     preflight request is required, it will be issued and the
	     calling context must block waiting for the callback to
	     return the outcome of that preflight.

	     @param request The cross-origin access requested.
	     @param origin_url The source/origin URL of this request;
	            used as referer on the preflight request.
	     @param target_url The target of the request.
	     @param security_callback Callback for notifying of outcome.
	     @param [out]security_cancel If non-NULL, the cancellation
	            interface to this security check.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

	OP_BOOLEAN HandleRedirect(OpCrossOrigin_Request *request, URL &moved_to_url);
	/**< Perform the redirect steps for a request. [CORS: 7.1.7]

	     @param request The cross-origin access requested.
	     @return OpBoolean::IS_TRUE if the redirect steps were successful.
	             OpBoolean::IS_FALSE if they failed; consult the status
	             of the request to determine resulting error state.
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_BOOLEAN ResourceSharingCheck(const OpCrossOrigin_Request &request, const URL &response_url, FailureReason &reason);
	/**< Perform the CORS resource sharing check, determining if
	     the response URL satisifies CORS constraints. [CORS: Section 7.2].

	     @param request The cross-origin access requested.
	     @param response_url The response URL.
	     @param [out]reason if outcome is OpBoolean::IS_FALSE,
	            details on why CORS failed.

	     @return OpBoolean::IS_TRUE if the resource sharing check passed.
	             OpBoolean::IS_FALSE if failed.
	             OpStatus::ERR_NO_MEMORY on OOM. */

	BOOL RequiresPreflight(const OpCrossOrigin_Request &request);
	/**< Returns TRUE if 'request' requires a preflight step to determine
	     if allowed. FALSE implies that no further checks are required
	     before issuing the (cross-origin) request.

	     [CORS spec: 7.1.1, 7.1.2, 7.1.3 -> 7.1.5.1] */

	OP_BOOLEAN HandlePreflightResponse(OpCrossOrigin_Request &request, const URL &response_url);
	/**< Given the response to a preflight request, perform the required
	     checks [CORS: Section 7.1.5 (Otherwise/200)] of that response,
	     refreshing/updating the preflight cache.

	     Depending on outcome, the state of the CORS request is adjusted. It must
	     be set to PreflightActive upon entry.

	     @param request The cross-origin request.
	     @param response_url The preflight response URL.
	     @return OpBoolean::IS_TRUE if the preflight response is syntactically valid
	             and the request is permitted with respect to that response.
	             OpBoolean::IS_FALSE if the preflight step failed.
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static BOOL IsSimpleCrossOriginRequest(const OpCrossOrigin_Request &request);
	/**< Classify the request as being "simple" or not, not requiring a preflight step
	     if so.

	     @param request The cross-origin request.
	     @return TRUE if can cross-origin request can be performed; FALSE if
	             it needs to be handled via a preflight step. */

	void InvalidatePreflightCache(BOOL expire_all = FALSE);
	/**< Invalidate expired preflight cache entries. If 'expire_all' is
	     TRUE, invalidate unconditionally of expiration time. */

	static OP_STATUS LogFailure(const uni_char *context, const URL &url, FailureReason reason);

	OpCrossOrigin_Request *FindNetworkRedirect(const URL &url);
	/**< Check if URL is the origin of a CORS redirect that the network layer
	     initiated. */

protected:
	friend class OpSecurityManager_CrossOrigin;

	OP_BOOLEAN CacheNetworkFailure(OpCrossOrigin_Request &request, BOOL update_cache);
	/**< Perform the cache-and-network-steps [CORS: 7.1.7],
	     clearing the cache of entries associated with the request.

	     @param request The cross-origin request.
	     @param update_cache TRUE if all entries from the request's
		        origin must be removed from the preflight cache. No
		        updates if FALSE is supplied.
	     @return OpStatus::IS_FALSE upon completion. OpStatus::ERR_NO_MEMORY
	             on OOM. */

	void RecordActiveRequest(OpCrossOrigin_Request *r);
	/**< Add the given request to the list of currently active CORS
	     requests. Needed so that redirect URLs can be mapped back to
	     requests and verified if they're OK to go ahead. */

	void DeregisterRequest(OpCrossOrigin_Request *r);
	/**< Request has failed or completed, remove it from the list of
	     currently active requests. */

	void RecordNetworkRedirect(OpCrossOrigin_Request *r);
	/**< Add the given request to the list of inflight redirects. Kept
	     track of internally so that the network layer can initiate a
	     CORS redirect request before notification of the redirect has
	     propagated to CORS handling code. */

	OP_BOOLEAN CheckRedirect(URL &url, URL &moved_to, BOOL &matched_url);
	/**< Check if a redirect from 'url' to 'moved_to' is invalid with
	     respect to the currently active CORS requests. If 'url' is
	     not associated with any current request, it is considered valid. */

private:
	OpCrossOrigin_Manager()
		: pre_cache(NULL)
		, force_preflight(FALSE)
	{
	}

	OpCrossOrigin_PreflightCache *pre_cache;

	List<OpCrossOrigin_Request> active_requests;
	/**< List of currently registered and ongoing CORS requests. */

	List<OpCrossOrigin_Request> network_initiated_redirects;
	/**< List of requests checked and initiated by the network layer during
	     its eager treatment of redirects. The network code handling redirects
	     does not currently call upon the code that initiated the request when
	     deciding whether or not to follow a redirect -- it is only later
	     notified of the redirect having gone ahead (or not) via messages.

	     If that redirect is across origins, the URL may have been marked as
	     either allowing or denying such. 'initiated_redirects' record the
	     outcome of checking (and possibly allowing) such CORS redirects,
	     keeping track of CORS redirects that the application code would
	     have initiated if only it had been involved.

	     Code that mark same-origin to cross-origin access allowable for
	     a URL request is required to check initiated_redirects for CORS
	     requests and take ownership of the object -- using FindNetworkRedirect().
	     Either when it is eventually notified of a redirect
	     (MSG_URL_MOVED) or upon failure (MSG_URL_LOADING_FAILED, possibly
	     due to the redirect not being allowed.) */

	BOOL force_preflight;
	/**< Always perform the preflight request step, irrespective of request.
	     Not clear why you would want to do this (testing excepted.) */
};

#endif // CORS_SUPPORT
#endif // CORS_MANAGER_H
