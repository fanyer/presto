/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/
#ifndef CORS_REQUEST_H
#define CORS_REQUEST_H

#ifdef CORS_SUPPORT
class OpCrossOrigin_Loader;

/** The CORS view of a cross-origin request. */
class OpCrossOrigin_Request
	: public ListElement<OpCrossOrigin_Request>
{
public:
	/** A CORS request moves through a small set of possible
	    states during the processing of the request, progressing
	    towards either 'Successful' or an error state. */
	enum Status
	{
		Initial,

		PreflightActive,
		/**< Awaiting preflight response + redirects. */

		PreflightComplete,
		/**< Preflight response received, but no security check
		     performed. */

		Active,
		/**< Successful preflight step (or none required);
		     cross-origin request is underway. */

		Successful,
		/**< Response to cross-origin request has passed the
		     resource sharing check. */

		ErrorAborted,
		/**< Processing of request was aborted externally. */

		ErrorNetwork
		/**< Network error processing the request, or CORS
		     signalled a network error. */
	};

	static OP_STATUS Make(OpCrossOrigin_Request *&result, const URL &origin_url, const URL &request_url, const uni_char *method, BOOL with_credentials, BOOL is_anonymous);
	/**< Construct a CORS request. If successful, the arguments will
	     have been copied and ownersip resides with the new instance.

	     @param [out] result Set to the constructed request object if successful,
	            undefined otherwise.
	     @param origin_url The origin URL of the cross-origin request.
	     @param request_url The given initial target of the request; a request's
	            target will be updated on redirects seen.
	     @param method The request method. No restrictions, but CORS does require
	            a separate preflight step for methods considered non-simple. Method
	            string is copied.
	     @param with_credentials TRUE if this request will be performed with credential
	            information included.
	     @param is_anonymous a flag used to control if the CORS request is
	            coming from a "privacy sensitive" context, and Origin: information
	            should not be provided (== "null").
	     @return OpStatus::OK upon successful creation; OpStatus::ERR_NO_MEMORY on OOM. */

	virtual ~OpCrossOrigin_Request();

	void SetStatus(Status new_status) { OP_ASSERT(status <= new_status); status = new_status; }

	Status GetStatus() const { return status; }

	BOOL IsActive() const { return status > Initial; }
	/**< Returns TRUE if the request has been started. */

	BOOL HasCompleted(BOOL &allowed) const
	{
		if (status >= Successful)
		{
			allowed = status == Successful;
			return TRUE;
		}
		else
			return FALSE;
	}

	void SetIsSimple(BOOL flg) { is_simple_request = flg; }
	/**< The request tracks if it is to be considered simple (with respect
	     to requiring a preflight step.) Operations that update
	     a request in ways that alter this state, must call
	     SetIsSimple(). */

	BOOL IsSimple() const { return is_simple_request; }
	/**< Is the request simple and does not require a preflight step? */

	BOOL IsPreflightRequired() const { return requires_preflight; }
	/**< Is the preflight step required for this request? If TRUE,
	     this overrides the "simple"ness of a request and the
	     cross-origin access must first perform a preflight step. */

	void SetPreflightRequired() { requires_preflight = TRUE; }
	/**< Force this request to perform a preflight step. */

	BOOL WithCredentials() const { return with_credentials; }
	/**< TRUE if this a credential'ed request. */

	void ClearWithCredentials() { with_credentials = FALSE; }
	/**< Clear the request's credentials flag. Needed if
	     the request is used in a context where credentialled
	     request do not make sense or expressly not permitted. */

	const uni_char *GetMethod() const { return method.CStr(); }
	/**< The method for which cross-origin access is requested. */

	OP_STATUS SetMethod(const uni_char *method);
	/**< Change the request method to 'method'. */

	const uni_char *GetOrigin() const { return origin.CStr(); }
	/**< Return the origining origin. If from a "privacy sensitive" contexts,
	     this will be represented as "null". */

	URL GetURL() const { return url; }
	/**< The URL of the request (not constant; updated on redirects.) */

	URL GetOriginURL() const { return origin_url; }
	/**< The origin URL of the request. */

	BOOL IsNetworkError() { return (status == ErrorNetwork); }
	/**< TRUE if request ended up in the network-error state. */

	void SetNetworkError();
	/**< Set the request's status as having failed on a network error. */

	void SetInRedirect(BOOL flag = TRUE) { in_redirect = flag; }
	/**< To implicitly signal to the cross-origin security checks that a
	     redirect is being processed, the request can be set as being
	     redirected. A security check will reset the flag upon having
	     processed the redirect request. */

	BOOL IsInRedirect() { return in_redirect; }

	void SetHasNonSimpleContentType(BOOL flg) { has_non_simple_content_type = flg; }
	/**< Does this request contain a Content-Type with a non-simple type? If TRUE,
	     then a request must not classify a Content-Type header as "simple" when
	     determining if a preflight step is required. */

	BOOL HasNonSimpleContentType() const { return has_non_simple_content_type; }
	/**< TRUE if request has a Content-Type header with a non-simple MIME type. */

	OpVector<uni_char> &GetHeaders() { return headers; }
	/**< The headers included with the cross-origin request. */

	const OpVector<uni_char> &GetHeaders() const { return headers; }
	/**< The headers included with the cross-origin request. */

	OP_STATUS AddHeader(const char *name, const char *value = NULL);
	/**< Add the given header 'name' to the set of headers that are part of
	     the cross-origin request. Also check if 'name' is a simple request
	     header or not, and update the request's "is-simple" flag accordingly.
	     For some header names, determining simplicity is dependent on the
	     header value, provided as 'value'.

	     @param name The header name; assumed to be a valid request-header [HTTP].
	     @param value The header value.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

	OP_BOOLEAN AddRedirect(const URL &moved_to_url);
	/**< Add a redirect to this request. */

	BOOL CheckRedirect(const URL &moved_to_url);
	/**< Add a redirect to this request. */

	void SetAllowRedirect(BOOL f) { allow_redirect = f; }
	/**< Alter the request's handling of redirects.  */

	BOOL AllowsRedirect() const { return allow_redirect; }
	/**< TRUE if this request may be redirected. Default is TRUE. */

	void SetIsAnonymous(BOOL f) { is_anonymous = f; }
	/**< Alter the request's 'anonymous' status. */

	BOOL IsAnonymous() const { return is_anonymous; }
	/**< TRUE if this is sent from an anonymous context.
	     This has bearing on both origin and referer information
	     used. */

	OP_STATUS ToOriginString(OpString &result);
	/**< Convert request origin(s) into a string representation. */

	BOOL CanExposeResponseHeader(const uni_char *name, unsigned length = UINT_MAX);
	/**< Returns TRUE if CORS allows a reponse header with
	     name 'name' to be exposed as a response header to the
	     CORS user (e.g., XMLHttpRequest.getResponseHeader().)
	     [CORS: 7.1.1 + XHR2: 4.7.3] */

	OP_STATUS AddResponseHeader(const uni_char *name, BOOL copy = TRUE);
	/**< Add the given header 'name' to the set of headers that the cross-origin
	     resource allows the CORS API user to see. Called during final resoure

	     @param name The header name; assumed to be a valid request-header [HTTP].
	     @param copy If FALSE then transfer ownership of 'name' to the request;
	            TRUE makes a copy of the header name.
	     @return OpStatus::OK on success; OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS PrepareRequestURL(URL *request_url = NULL);
	/**< Prepare the given request URL for cross-origin use. This includes
	     decoration of the request with an Origin: header. The URL is assumed
	     to be an otherwise valid CORS request URL in terms of headers
	     included.

	     @param request_url The URL to update. If NULL, then update the request's
	            URL.
	     @return OpStatus::OK on successful update; OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef SELFTEST
	void ClearHeaders();
#endif // SELFTEST

	void SetLoader(OpCrossOrigin_Loader *l) { loader = l; }
	/**< Set the preflight loader that's performing the preflight request on
	     behalf of it. */

	static OP_STATUS ToOriginString(OpString &result, const URL &origin_url);
	/**< Convert URL to its Origin: string representation. */

	static BOOL IsSimpleMethod(const uni_char *method);
	/**< Returns TRUE if method is a "CORS simple" method. */

	static BOOL IsSimpleHeader(const char *name, const char *value);
	/**< Returns TRUE if name=value is a "CORS simple" request header. */

protected:

	OpString method;
	/**< The method for which cross-origin access is requested. */

	OpString origin;
	/**< The origining origin. If from a "privacy sensitive" contexts,
	     this will be represented as "null". */

	OpAutoVector<URL> redirects;
	/**< The redirects seen for this request. */

	URL url;
	/**< The URL of the request (not constant; updated on redirects.) */

	URL origin_url;
	/**< The origin URL of the request. */

	OpVector<uni_char> headers;
	/**< The headers included with the cross-origin request. */

	BOOL with_credentials;
	/**< TRUE if this a credential'ed request. */

	BOOL is_anonymous;
	/**< TRUE if the request is from a context with no origin. */

	BOOL is_simple_request;
	/**< TRUE if the request is considered simple. Maintained as
	     the CORS request is updated. */

	BOOL requires_preflight;
	/**< TRUE if this request must perform a preflight step regardless
	     of whether or not it is a simple request. */

	BOOL allow_redirect;
	/**< TRUE if this request will allow redirects. */

	BOOL in_redirect;
	/**< TRUE if this request is currently being redirected. */

	BOOL has_non_simple_content_type;
	/**< TRUE if this request has a non-simple Content-Type MIME type. A special
	     case, avoiding the storage of request header values. (Content-Type is
	     the only header that CORS needs to look at its value in order to
	     classify it as simple or not.) */

	OpVector<uni_char> exposed_headers;
	/**< The response headers that the resource allows exposure of. */

	Status status;
	/**< The current state. The Status type tracks the possible
	     states a request might have in the CORS specification.
	     A request starts off in the Initial state, progressing
	     through to either Successful or an Error state. If the
	     request doesn't require a preflight step, it will jump
	     over the PreflightActive and PreflightActive states
	     to Active. If not, it linearly progress through to eventual
	     success or failure. */

	OpCrossOrigin_Loader *loader;
	/**< The preflight loader that's currently active on behalf of this request.
	     Reference kept on the request to cleanly detach the loader if the request
	     is aborted early. */

	OpCrossOrigin_Request(const URL &url, BOOL with_credentials)
		: url(url)
		, with_credentials(with_credentials)
		, is_anonymous(FALSE)
		, is_simple_request(FALSE)
		, requires_preflight(FALSE)
		, allow_redirect(TRUE)
		, in_redirect(FALSE)
		, has_non_simple_content_type(FALSE)
		, status(Initial)
		, loader(NULL)
	{
	}
};

#endif // CORS_SUPPORT
#endif // CORS_REQUEST_H
