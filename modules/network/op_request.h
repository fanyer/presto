/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef __OPREQUEST_H__
#define __OPREQUEST_H__

class OpRequest;
class OpResource;
class OpResponse;

#include "modules/network/op_url.h"
#include "modules/windowcommander/OpWindowCommander.h"
#include "modules/windowcommander/OpCookieManager.h"

/** @class OpMultipartCallback
 *
 * OpMultipartCallback lets a user advance through the different parts in multipart/mixed and multipart/replace resources.
 *
 * If OnResponseMultipartBodyLoaded is not overload the default implementation makes only the first part of the multipart visible.
 *
 */
class OpMultipartCallback
{
public:
	virtual ~OpMultipartCallback() {}
	/** LoadNextMultipart can be called when you are ready to retrieve the next multipart element.
	 */
	virtual void LoadNextMultipart() = 0;

	/** StopLoading can be called when you no longer wants to receive more multipart elements. This is only required if you want to abort loading of any expected remaining multipart objects.
	 */
	virtual void StopLoading() = 0;

	/** HandleAsSingleResponse is the default way we handle multipart request results.
	 *  Only the first element of the multipart is returned to the client code.
	 */
	virtual void HandleAsSingleResponse() = 0;
};

/** @class OpRequestListener
 *
 * OpRequestListener is used to report the progress when you are requesting a URL.
 *
 *  To retrieve a URL resource we do the following:
 *  1. Create an OpURL from the URL.
 *  2. Use the OpURL to create an OpRequest.
 *  3. After having called OpRequest::SendRequest, progress is reported through calls to OpRequestListener methods.
 *
 *  A basic sequence of callbacks for an HTTP GET request will result in the following method calls:
 *  1. OnResponseAvailable
 *  2. OnResponseDataLoaded 0..n times
 *  3. OnResponseFinished
 *
 *  A sequence of callbacks for an HTTP GET request with redirect(s):
 *  1. OnRequestRedirected 1..n times
 *  2. OnResponseAvailable
 *  3. OnResponseDataLoaded 0..m times
 *  4. OnResponseFinished
 *
 *  A sequence of callbacks for an HTTP POST request:
 *  1. OnRequestDataSent 1..n times
 *  2. OnResponseAvailable
 *  4. OnResponseFinished
 *
 *  A sequence of callbacks for an HTTPS GET request with a self signed certificate:
 *  1. OnCertificateBrowsingRequired
 *  2. OnResponseAvailable
 *  3. OnResponseDataLoaded 0..n times
 *  4. OnResponseFinished
 *
 *  A sequence of callbacks for an HTTP GET request with proxy authentication required:
 *  1. OnAuthenticationRequired
 *  2. OnResponseAvailable
 *  3. OnResponseDataLoaded 0..n times
 *  4. OnResponseFinished
 *
 *  If the request fails for any reason, OnRequestFailed is called with an error code describing what went wrong.
 *
 *  At OnResponseAvailable it is possible to retrieve all data available in the header.
 *  After OnResponseDataLoaded has been called you can create an OpDataDescriptor object
 *  and retrieve all the contents of the resource being retrieved.
 *
 * At any point you can cancel a request by deleting the OpRequest object. If a request
 * is deleted there will be no callbacks.
 */

class OpRequestListener
{
public:
	OpRequestListener():m_active_requests(0) { }
	virtual ~OpRequestListener() { OP_ASSERT(m_active_requests == 0); }//Make sure that the listener does not have outstanding requests when deleted.

	/** OnRequestRedirected is called for each redirected request until a final response has been downloaded.
	 *  The default implementation of this method does nothing. To cancel a redirect delete the request in this call-back.
	 *
	 * @param req	The request for which this redirect was returned.
	 * @param res	The response which contains the redirect header. The response is specific to this redirect.
	 *              The lifetime of the redirect response object is until the OpRequest is deleted.
	 * @param from	The URL we were redirected from.
	 * @param from	The URL we were redirected to.
	 */
	virtual void OnRequestRedirected(OpRequest *req, OpResponse *res, OpURL from, OpURL to) {}

	/** OnRequestFailed is called when a request failed to return a valid response. A valid response has a return code below 400.
	 *  SetIgnoreResponseCode() can be used to change this behavior so that also responses with HTTP code greater than 400 are valid.
	 *
	 * @param req	The request which failed.
	 * @param res	The response header. If not NULL it will contain the server error response.
	 * @param error	The error description.
	 */
	virtual void OnRequestFailed(OpRequest *req, OpResponse *res, Str::LocaleString error) = 0;

	/** OnRequestDataSent is called whenever POST/PUT data has been sent.
	 *  The default implementation of this method does nothing.
	 *
	 *  @param req	The request for which data was sent.
	 */
	virtual void OnRequestDataSent(OpRequest *req) {}

	/** @short Authentication is needed.
	 *
	 * The user's data needs to be returned to the caller by calling OpAuthenticationCallback::Authenticate() or
	 * OpAuthenticationCallback::CancelAuthentication() on the specified callback instance.
	 * The default implementation of this method calls CancelAuthentication().
	 *
	 * @param req		The request that needs authentication.
	 * @param callback	Contains the OpAuthenticationCallback instance for the authentication request. This
	 * 					callback needs to be notified about the user's input.
	 *
	 * @see OpAuthenticationListener::OnAuthenticationRequired()
	 */
	virtual void OnAuthenticationRequired(OpRequest *req, OpAuthenticationCallback *callback)
		{ callback->CancelAuthentication(); }

	/** This method is called when a url authentication request is cancelled.
	 *
	 * A possible use-case is that an authentication is requested multiple times
	 * for the same url in different contexts. E.g. in addition to the
	 * OnAuthenticationRequired() the same url might be loaded in a window (see
	 * OpWindowCommander's OpLoadingListener::OnAuthenticationRequired()) or in
	 * another windowless context. If the user entered or cancelled the other
	 * authentication request before handling this authentication, then this
	 * method is called to cancel this authentication request.
	 *
	 * The implementation of this method should e.g. close the associated
	 * authentication dialog.
	 *
	 * @param req		The request that no longer needs authentication.
	 * @param callback	The authentication callback that was cancelled. This
	 *  callback instance shall no longer be used.
	 *
	 * @see OnAuthenticationRequired()
	 * @see OpLoadingListener::OnAuthenticationRequired()
	 * @see OpLoadingListener::OnAuthenticationCancelled()
	 */
	virtual void OnAuthenticationCancelled(OpRequest *req, OpAuthenticationCallback *callback) { }

#ifdef _SSL_SUPPORT_
	/** @short Called when the request needs user interaction regarding a certificate.
	 *
	 * SSLCertificateContext::OnCertificateBrowsingDone() must be called as a response to
	 * this. It can be done synchronously or asynchronously.
	 * The default implementation of this method calls OnCertificateBrowsingDone(FALSE, SSL_CERT_OPTION_NONE).
	 *
	 * @param context 	Provider of information about the certificate(s) and the
	 *					callback method to call when finished
	 * @param reason	An enum with the reason why user interaction is needed
	 * @param options	A bitfield with possible options for what to do when calling
	 *					OnCertificateBrowsingDone.
	 */
	virtual void OnCertificateBrowsingRequired(OpRequest *req, OpSSLListener::SSLCertificateContext *context, OpSSLListener::SSLCertificateReason reason, OpSSLListener::SSLCertificateOption options)
		{ context->OnCertificateBrowsingDone(FALSE, OpSSLListener::SSL_CERT_OPTION_NONE); }

	/** @short Called when a request needs the master security password from the user.
	 *
	 * callback->OnSecurityPasswordDone() must always be called in response to
	 * this method when finished, when the password request was either
	 * fulfilled or rejected. This can be done either synchronously or
	 * asynchronously.
	 *
	 * @param req The request for which data was sent.
	 * @param callback A call-back used to report when finished.
	 */
	virtual void OnSecurityPasswordRequired(OpRequest *req, OpSSLListener::SSLSecurityPasswordCallback *callback) { callback->OnSecurityPasswordDone(FALSE, "", ""); }
#endif // _SSL_SUPPORT_

#ifdef _ASK_COOKIE
	/** OnCookieConfirmationRequired is called when a new cookie is about to be set.
	 *  The default implementation of this method rejects the cookie.
	 *
	 *  @param req	The request for which data was sent.
	 *  @param res	The callback that needs to be acted upon.
	 */

	virtual void OnCookieConfirmationRequired(OpRequest *req, OpCookieListener::AskCookieContext *callback) { callback->OnAskCookieCancel(); }
#endif //_ASK_COOKIE

	/** OnResponseAvailable is called when all headers have been received.
	 *  The default implementation of this method does nothing (i.e. the client might wait to
	 *  handle the response until OnResponseDataLoaded or OnResponseFinished is received).
	 *
	 *  @param req	The request for which data was sent.
	 *  @param res	The response that is now available. The lifetime of the response is until the OpRequest is deleted.
	 */

	virtual void OnResponseAvailable(OpRequest *req, OpResponse *res) {}

	/** OnResponseDataLoaded is called whenever more data is available.
	 *  The default implementation of this method does nothing (i.e. the client might wait to
	 *  handle the response until OnResponseFinished is received). To cancel a download delete
	 *  the request in this call-back.
	 *
	 *  @param req	The request.
	 *  @param res	The response for which data is now available.
	 */
	virtual void OnResponseDataLoaded(OpRequest *req, OpResponse *res) {}

	/** When the final part of the response has been received OnResponseFinished will be called.
	 *
	 *  @param req	The request.
	 *  @param res	The completed response.
	 */
	virtual void OnResponseFinished(OpRequest *req, OpResponse *res) = 0;

	/** As different parts of a multipart response is received OnResponseMultipartBodyLoaded will be called.
	 *  It is important that a user of the API retrieve the available data using the data descriptor when OnResponseMultipartBodyLoaded is called,
	 *  otherwise the remaining parts will not be generated and made available.
	 *  The default implementation of this method stops downloading (as if the OpRequest was deleted), so that only the first body part will be seen.
	 *  The lifetime of the callback object is until the request has either finished, failed or StopLoading is called.
	 */
	virtual void OnResponseMultipartBodyLoaded(OpRequest *req, OpResponse *res, OpMultipartCallback *callback) { callback->HandleAsSingleResponse(); }

private:
	void IncActiveRequests() { m_active_requests++; }
	void DecActiveRequests() { OP_ASSERT(m_active_requests); m_active_requests--; }
	int m_active_requests;
	friend class OpRequest;
};


/** @class OpRequest
 *
 *  OpRequest is used to send a URL request and fetch a OpResponse result (Typically a HTTP request and response).
 *
 *  A request is created through calling OpRequest::Make(). Whoever holds the pointer to OpRequest after the Make call owns the OpRequest.
 *  After you are done with OpRequest you can delete it whenever you like. Any associated OpRequestListener should be deleted after the OpRequest
 *  is deleted. If a request is in progress when deleted it will be aborted. The OpRequest object owns the OpResponse and the response will be
 *  deleted along with the OpRequest object.
 *
 *  Usage:
 *  @code
 *  OpURL url = OpURL::Make("http://t/core/networking/http/cache/data/blue.jpg");
 *  OP_STATUS result = OpRequest::Make(request, requestListener, url, parent_resource->GetContextId());
 *
 *  if (result == OpStatus::OK)
 *  {
 *    ...
 *    request->SendRequest();
 *  }
 *  @endcode
 *
 */

class OpRequest
{
public:
	virtual ~OpRequest() {}

	/** Create a request.
	 *
	 *	@param request			The request output.
	 *	@param listener			Listener for feedback about progress.
	 *	@param url				The url where this request is sent.
	 *	@param referrer			The HTTP referrer used when sending this request.
	 *	@param context_id		Context ID for the request.
	 *	@return OP_STATUS		Returns OpStatus::ERR_NO_MEMORY if not enough memory, otherwise OpStatus::OK.
	 */

	static OP_STATUS Make(OpRequest *&request, OpRequestListener *listener, OpURL url, URL_CONTEXT_ID context_id, OpURL referrer = OpURL());

	/** Returns the load policy object of this OpRequest through which the load policy for the request can be modified.
	 *  The object reference will become invalid if the OpRequest is deleted.
	 */
	virtual URL_LoadPolicy &GetLoadPolicy() = 0;

	/** Set HTTP method.
	 *
	 *  When a non-HEAD/GET method is used the request is not cached and sent on a new connection.
	 *  If sending does not succeed we will not make any automatic attempts to send the request again.
	 *  For HEAD/GET we make up to 5 automatic attempts before giving up. The request is sent with
	 *  Content-Length: 0 if no HTTP POST form data has been set.
	 *
	 *	@param method			The HTTP request method.
	 */
	virtual OP_STATUS SetHTTPMethod(HTTP_Request_Method method) = 0;

	/** @return the HTTP method used for this request. */
	virtual HTTP_Request_Method GetHTTPMethod() const = 0;

	/** Set custom HTTP method.
	 *
	 *  When a non-HEAD/GET method is used the request is not cached and sent on a new connection (Unique).
	 *  If sending does not succeed we will not make any automatic attempts to send the request again.
	 *  For HEAD/GET we make up to 5 automatic attempts before giving up. The request is sent with
	 *  Content-Length: 0 if no HTTP POST form data has been set.
	 *
	 *	@param special_method	If the string is a HTTP_Request_Method, that method value will be used, if it is any other valid
	 *							value the string will be used in the request.
	 *
	 *	@return OP_STATUS		Invalid methods (e.g. non-tokens, non-allowed values, or containing whitespace, see
	 *							http://dev.w3.org/2006/webapi/XMLHttpRequest) will result in
	 *							OpStatus::ERR_PARSING_FAILED being returned. Otherwise it returns OpStatus::OK.
	 *
	 */
	virtual OP_STATUS SetCustomHTTPMethod(const char *special_method) = 0;

	/** @return the custom HTTP method string used for this request. */
	virtual const char *GetCustomHTTPMethod() const = 0;

	/** What is the context ID of the request. */
	virtual URL_CONTEXT_ID GetContextId() const = 0;

	/** Adds a custom HTTP header that is sent with this request. When a request is processed it will
	 *  first look in cache to see if a resource exists that matches the request. To get a match the
	 *  HTTP headers used must be the same. */
	virtual OP_STATUS AddHTTPHeader(const char *name, const char *value) = 0;

	/** Disable sending and receipt of cookies. */
	virtual OP_STATUS SetCookiesProcessingDisabled(BOOL value) = 0;

	/** If set to other than NETTYPE_UNDETERMINED (the default) this limits the network type this URL can
	 *  load from to the broader network category (e.g. localhost can access public, public cannot access local).
	 *  If the limit check fails the request is blocked and OnRequestFailed is called.
	 */
	virtual OP_STATUS SetNetTypeLimit(OpSocketAddressNetType value) = 0;

	/** Set the maximum time we allow a request to wait from sending out a request until we start getting a response.
	 *  If max time is exceeded we call OnRequestFailed().
	 */
	virtual OP_STATUS SetMaxRequestTime(UINT32 value) = 0;

	/** Set the max idle time we allow in between receiving data once we start getting a response.
	 *  If max time is exceeded we call OnRequestFailed().
	 */
	virtual OP_STATUS SetMaxResponseIdleTime(UINT32 value) = 0;

	/** Priority of http request. Higher number means higher priority. */
	virtual OP_STATUS SetHTTPPriority(UINT32 value) = 0;

	/** Is this connection externally managed? If set, normal max connections per server and
	 *  max connections total rules for HTTP connections will be ignored.
	 *  NOTE: MUST be used with *extreme* caution as abusing this can cause connection problems
	 *  for normal HTTP connections.
	 */
	virtual OP_STATUS SetExternallyManagedConnection(BOOL value) = 0;

	/** Set the MIME type of the POST data for this document. */
	virtual OP_STATUS SetHTTPDataContentType(const OpStringC8 &value) = 0;

	/** Get the MIME type of the POST data for this document. */
	virtual const char *GetHTTPDataContentType() const = 0;

	/** HTTP authentication username. */
	virtual OP_STATUS SetHTTPUsername(const OpStringC8 &value) = 0;

	/** HTTP authentication password. */
	virtual OP_STATUS SetHTTPPassword(const OpStringC8 &value) = 0;

	/** Use generic authentication handling. WindowCommander will ask for authentication if required, ie. OnAuthenticationRequired will not be called. */
	virtual OP_STATUS UseGenericAuthenticationHandling() = 0;

	/** Override redirection preference. */
	virtual OP_STATUS SetOverrideRedirectDisabled(BOOL value) = 0;

	/** Limit how a request is processed. Typically used for favicon and OCSP requests.
	 * Authentication is disabled.
	 * Redirects are not allowed.
	 * Cached items have a minimum expiry time of 24 hours.
	 */
	virtual OP_STATUS SetLimitedRequestProcessing(BOOL value) = 0;

	/** Only allow one connection at a time to server. */
	virtual void SetParallelConnectionsDisabled() = 0;

	/** Get the original URL used when making this request. On redirect, use GetURL on the OpResponse objects to get the urls redirected to. */
	virtual OpURL GetURL() = 0;

	/** Get the referrer URL used for this request. */
	virtual OpURL GetReferrerURL() = 0;

	/** Let URL responses with HTTP 4xx or greater status code whose content does not really represent the requested resource
	 *  get the same sequence of callbacks as for a normal successful load instead of one where the final callback is OnRequestFailed. */
	virtual void SetIgnoreResponseCode(BOOL value) = 0;

	/** Position in the request that we are requesting that the server starts the response at (if not specified or 0, at beginning, and not sent). Zero-based and valid for HTTP and FTP protocol. */
	virtual OP_STATUS SetRangeStart(OpFileLength position) = 0;

	/** Position in the request that we are requesting that the server stops the response at (if not specified or 0, at end, and not sent) inclusive. Zero-based and valid for HTTP protocol. */
	virtual OP_STATUS SetRangeEnd(OpFileLength position) = 0;

	/** Get the next request in an OpBatchRequest list. */
	virtual OpRequest *Next() = 0;

	/** Set HTTP POST form data (string).
	 *
	 *  @param	data				NULL terminated string that will be copied by the implementation.
	 *  @param	includes_headers	Does the data string include valid HTTP headers to be used in the request?
	 *  @return OP_STATUS
	 */
	virtual OP_STATUS SetHTTPData(const char* data, BOOL includes_headers = FALSE) = 0;

	/** Set HTTP POST form data (upload).
	 *
	 *  @param	data			Upload element, PrepareUploadL must have been called
	 *  @param	new_data		Is this new data?
	 *  @return OP_STATUS
	 */
	virtual void SetHTTPData(Upload_Base *data, BOOL new_data) = 0;

#ifdef HTTP_CONTENT_USAGE_INDICATION
	/** Indication of current usage for this URL. */
	virtual OP_STATUS SetHTTPContentUsageIndication(HTTP_ContentUsageIndication value) = 0;
	virtual HTTP_ContentUsageIndication GetHTTPContentUsageIndication() const = 0;
#endif // HTTP_CONTENT_USAGE_INDICATION

#ifdef CORS_SUPPORT
	/** Should this request follow CORS rules in redirects? (default FALSE) */
	virtual OP_STATUS SetFollowCORSRedirectRules(BOOL value) = 0;
#endif // CORS_SUPPORT

#ifdef ABSTRACT_CERTIFICATES
	/** Requests the certificate details of the current URL. The URL must of course be a HTTPS url to get a valid Cert.
	 *  It is only possible to request the certificate before the URL is loaded, therefore this method must be called before SendRequest().
	 *  Afterwards, the certificate will be available from the OpResponse by calling GetRequestedCertificate().
	 *  @param value Set to TRUE to request certificate details.
	 *  @return OK or ERR_NO_MEMORY
	 */
	virtual OP_STATUS SetCertificateRequested(BOOL value) = 0;
#endif

#ifdef URL_ALLOW_DISABLE_COMPRESS
	/** Prevent setting the HTTP Accept-Encoding header with value "gzip, deflate". Used to avoid getting compressed results
	 *  when fetching for instance video resources.
	 */
	virtual OP_STATUS SetDisableCompress(BOOL value) = 0;
#endif

	/**
	 *  Send the request.
	 *  This operation may initiate network action or automatic generation of a document, depending on the request type, but may also
	 *  decide to use an existing cached response as-is without network action.
	 *
	 *  Specification of cache policy, inline loading, user interaction, etc. used to determine how to load the document is set through
	 *  the URL_LoadPolicy available through GetLoadPolicy().
	 *
	 *  The referrer set through OpRequest construction is used for the request.
	 *
	 *  The response is communicated through the OpRequestListener callbacks.
	 *
	 *  Before calling SendRequest you can set a number of load policy parameters through GetLoadPolicy(). (Methods are shown with defaults below):
	 *
	 *  SendRequest() can only be called once.
	 *
	 *  - SetHistoryWalk(BOOL hist_walk = FALSE)
	 *  - SetThirdpartyDetermined(BOOL thirdpart_det = FALSE)
	 *  - SetEnteredByUser(EnteredByUser entered = NotEnteredByUser)
	 *  - SetBypassProxy(BOOL bypass_proxy = FALSE)
	 *  - SetUserInitiated(BOOL user = FALSE)
	 *  - SetIsInlineElement(BOOL inline_elm = FALSE)
	 *  - SetReloadPolicy(URL_Reload_Policy reload_pol = URL_Reload_If_Expired)
	 *  - To resume a download use SetReloadPolicy(URL_Resume)
	 */
	virtual OP_STATUS SendRequest() = 0;

	/** Request result. GetResponse return NULL initially and will have a value once OnResponseAvailable
	 *  or OnRequestRedirected has been called. On redirect, the last redirected-to response will be returned. */
	virtual OpResponse *GetResponse() = 0;

	/** @return The first response returned by this request. This might be a redirect response.
	 *  It will be NULL until OnResponseAvailable or OnRequestRedirected has been called. */
	virtual OpResponse *GetFirstResponse() = 0;

	/** Use GenerateResponse() to create a custom OpResponse. No OpRequestListener callbacks will be
	 *  called through this method. SendRequest() should not be used since this generates the response
	 *  manually instead.
	 *  @param response Generated response
	 *  @return OK or ERR_NO_MEMORY
	 *  */
	virtual OP_STATUS GenerateResponse(class OpGeneratedResponse *&response) = 0;

#ifdef TCP_PAUSE_DOWNLOAD_EXTENSION
	/** Pause the request. Does not actively close an existing TCP connection. */
	virtual OP_STATUS Pause() = 0;

	/** Resume a previously paused request.
	 *
	 *  When Continue() is called, if the socket has already been closed, a new request is made if and only if the
	 *  server supports byte range requests, and the only way to reliably know this is to have previously made a byte
	 *  range request (initially using Range: 0-). Otherwise it returns OpStatus::ERR, along with normal listener callbacks.
	 * */
	virtual OP_STATUS Continue() = 0;
#endif

	/** Add another listener to this OpRequest.
	 *  @param listener Listener to add
	 *  @return OK, ERR (if listener==NULL) or ERR_NO_MEMORY
	 */
	virtual OP_STATUS AddListener(OpRequestListener *listener) = 0;

	/** Remove a listener from this OpRequest. The listener will stop receiving callbacks.
	 *  @param listener Listener to remove
	 *  @return OK or ERR if the listener was not found
	 */
	virtual OP_STATUS RemoveListener(OpRequestListener *listener) = 0;

protected:
	static void IncActiveRequests(OpRequestListener *listener) { listener->IncActiveRequests(); }
	static void DecActiveRequests(OpRequestListener *listener) { listener->DecActiveRequests(); }
};

#endif
