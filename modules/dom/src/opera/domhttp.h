/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef DOM_DOMHTTP_H
#define DOM_DOMHTTP_H

#ifdef DOM_HTTP_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/domevents/domeventtarget.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/util/opstring.h"
#include "modules/util/tempbuf.h"

#ifdef PROGRESS_EVENTS_SUPPORT
/* Progress events are to spaced out by at least 50ms. */
#define DOM_XHR_PROGRESS_EVENT_INTERVAL 50
#endif // PROGRESS_EVENTS_SUPPORT

/** Enable to comply with the specification and constrain
    synchronous XMLHttpRequest usage. It disallows open()
    uses with an explicit timeout, credentialled CORS requests
    or an explicit response type. Hurry slowly and disable it
    initially. */
/* #define DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE */

class DOM_Node;
class DOM_LSParser;
class DOM_HTTPInputStream;
class DOM_HTTPOutputStream;
class DOM_Blob;

class DOM_HTTPRequest
	: public DOM_Object
{
public:
	class Header
	{
	public:
		OpString8 name;
		OpString8 value;
		Header *next;
	};

protected:
	DOM_HTTPRequest();
	virtual ~DOM_HTTPRequest();

	uni_char *uri, *method;
	OpString8 forced_response_type;

	ES_Object *input, *output;
	DOM_HTTPInputStream *inputstream;
	DOM_HTTPOutputStream *outputstream;

	Header *headers;

	BOOL report_304_verbatim;
	/**< Set to TRUE if a 304 response to this request should be
	     reported as that (304), and not be cloaked as 200. If the network
	     layer inserts its own conditional headers (but scripts don't via
	     setRequestHeader()), then a 304 response must be reported as a
	     200 status instead. */

public:
	static OP_STATUS Make(DOM_HTTPRequest *&request, DOM_EnvironmentImpl *environment, const uni_char *uri, const uni_char *method);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTTPREQUEST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	OP_STATUS GetURL(URL &url, URL reference);

	const uni_char *GetURI() { return uri; }
	/**< Returns a string representation of the URL used in the request. */

	const uni_char *GetMethod() { return method; }

	Header *GetFirstHeader() { return headers; }

	void SetWasConditionallyIssued(BOOL flag) { report_304_verbatim |= !flag; }
	/**< Notify request if the network layer has inserted conditional
	     headers for this request, independently from what scripts have
	     added via setRequestHeader() on the XMLHttpRequest object. */

	BOOL ReportNotModifiedStatus() { return report_304_verbatim; }
	/**< Returns TRUE if Not-Modified(304) responses to this request must
	     be reported as having that status. An implementation of XMLHttpRequest
	     should hide a Not-Modified response if it is a result of the
	     implementation's own inserted conditional headers on the request.
	     (and report 200(OK) instead.) If scripts added conditional headers,
	     the Not-Modified status shouldn't be hidden regardless. */

	ES_Object *GetInput() { return input; }
	ES_Object *GetOutput() { return output; }

	DOM_HTTPInputStream *GetInputStream() { return inputstream; }
	DOM_HTTPOutputStream *GetOutputStream() { return outputstream; }

	OP_STATUS AddHeader(const uni_char *name, const uni_char *value);
	OP_STATUS ForceContentType(const uni_char *content_type);

	const char *GetForcedResponseType() { return forced_response_type.CStr(); };
	/**< Get overridden Content-Type string on response, if any. */

	static OP_STATUS ConvertRangeHeaderInURLAttributes(OpString8 &header_value, URL &url, BOOL &attributes_set);
	/**< Converts a Range header to KHTTPRangeStart and KHTTPRangeEnd attributes
		@param header_value Value of the header
		@param url destination URL
		@param attributes_set TRUE if the attributes are set
	*/
};

class DOM_HTTPInputStream
	: public DOM_Object
{
protected:
	DOM_HTTPInputStream(DOM_HTTPRequest *request);

	DOM_HTTPRequest *request;

public:
	static OP_STATUS Make(DOM_HTTPInputStream *&inputstream, DOM_HTTPRequest *request);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTTPINPUTSTREAM || DOM_Object::IsA(type); }
	virtual void GCTrace();

	DOM_HTTPRequest *GetRequest() { return request; }
};

class DOM_HTTPOutputStream
	: public DOM_Object
{
protected:
	DOM_HTTPOutputStream(DOM_HTTPRequest *request);
	~DOM_HTTPOutputStream();

	DOM_HTTPRequest *request;
	unsigned char *data;
	unsigned length;
	BOOL is_binary;

	DOM_Object *upload_object;
	/**< Non-NULL if the object is capable of generating
	     the request payload. Mutually exclusive with 'data'. */

	OpFileLength upload_length;
	/**< If non-zero, the known size of the uploaded data. */

	char *content_type;
	/**< If non-NULL, the Content-Type to issue this request with. */

public:
	static OP_STATUS Make(DOM_HTTPOutputStream *&outputstream, DOM_HTTPRequest *request);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_HTTPOUTPUTSTREAM || DOM_Object::IsA(type); }
	virtual void GCTrace();

	void RelinquishData();
	/**< Drop reference to the request data without releasing it. Can
	     only be done if ownership of the data has been transferred
	     elsewhere already, so calling context is responsible for
	     ensuring this. */

	void SetOutputLength(OpFileLength new_length)
	{
		OP_ASSERT(upload_object);
		upload_length = new_length;
	}
	/**< Record the length of the uploaded object's payload. */

	const unsigned char *GetData() { return data; }
	unsigned GetLength() { return length; }
	OpFileLength GetContentLength();
	BOOL GetIsBinary() { return is_binary; }
	DOM_Object *GetUploadObject() { return upload_object; }

	const char *GetContentType() { return content_type; }
	/**< Return the current content type for this request. */

	BOOL HasData() const { return (data != NULL || upload_object != NULL); }
	/**< Returns TRUE if this request will upload data. */

	OP_STATUS GetContentEncoding(OpString &result);
	/**< Determine the charset encoding from the request's content type.

	     @param [out]result The encoding found. If no explicit charset parameter,
	            the result is returned as "UTF-8."
	     @returns OpStatus::OK on successful update; OpStatus::ERR_NO_MEMORY on OOM. */

	void GetContentEncodingL(OpString &result);
	/**< Determine the charset encoding from the request's content type.

	     @param [out]result The encoding found. If no explicit charset parameter,
	            the result is returned as "UTF-8." */

	OP_STATUS SetData(const unsigned char *new_data, unsigned new_length, BOOL new_is_binary, DOM_Object *new_external);
	/**< Set this request's upload data, replacing any existing.

	     @param new_data The sequence of bytes to upload. Can be NULL.
	     @param new_length The length in bytes of 'new_data'.
	     @param new_binary If TRUE, the upload data is in binary form and should
	            not be encoded further before transmission.
	     @param new_external If non-NULL, the external object holding the data to
	            upload.

	     @returns OpStatus::OK on successful update; OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS SetContentType(const char *mime_type);
	/**< Set this request's content type, replacing the existing one, if any.

	     @param mime_type The media type to use for this request. Can include
	            parameters along with type, but cannot be NULL.
	     @returns OpStatus::OK on successful update; OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS UpdateContentType(const uni_char *mime_type, const char *encoding);
	/**< Update this request's content type with a preferred type and encoding
	     of the uploaded data. This is done when the request payload's type
	     and data has been determined, the resulting content type has to be
	     computed. If the request already has a MIME media type set for its
	     content type, it takes precedence over the preferred type 'mime_type'.

	     @param mime_type The media type to use for this request. Can be
	            NULL, but must not contain parameters along with the type.
	     @param encoding The encoding of this request's data. If NULL,
	            the content is transmitted in binary, raw format.
	     @returns OpStatus::OK on successful update; OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS TransformContentType(char *&content_type, const char *mime_type, const char *encoding);
	/**< Static helper for UpdateContentType() and a selftest
	     convenience.

	     @param content_type The existing content type to update.
	     @param mime_type The media type to use for this request. Can be
	            NULL, but may contain parameters along with the type.
	     @param encoding The encoding of this request's data. If NULL,
	            the content is transmitted in binary, raw format.
	     @returns OpStatus::OK on successful update; OpStatus::ERR_NO_MEMORY on OOM. */
};

class DOM_XMLHttpRequestUpload;

class DOM_XMLHttpRequest
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:
	/** Ready states, each followed by its synonym from http://www.w3.org/TR/XMLHttpRequest: */
	enum ReadyState
	{
		READYSTATE_UNINITIALIZED = 0,
		READYSTATE_UNSENT = READYSTATE_UNINITIALIZED,

		READYSTATE_OPEN = 1,
		READYSTATE_OPENED = READYSTATE_OPEN,

		READYSTATE_SENT = 2,
		READYSTATE_HEADERS_RECEIVED = READYSTATE_SENT,

		READYSTATE_RECEIVING = 3,
		READYSTATE_LOADING = READYSTATE_RECEIVING,

		READYSTATE_LOADED = 4,
		READYSTATE_DONE = READYSTATE_LOADED
	};

	/**
	 * This is basically a TempBuffer that can handle embedded NUL chars. Couldn't find any
	 * such class already. Replace if one such appear.
	 */
	class ResponseDataBuffer
	{
		uni_char* m_buffer;
		unsigned char *m_byte_buffer;

		unsigned m_used;
		unsigned m_buffer_size;
		BOOL m_is_byte_buffer;
	public:
		ResponseDataBuffer()
			: m_buffer(NULL),
			  m_byte_buffer(NULL),
			  m_used(0),
			  m_buffer_size(0),
			  m_is_byte_buffer(FALSE)
		{
		}

		~ResponseDataBuffer();

		const uni_char* GetStorage() const { return m_buffer; }
		uni_char* GetStorage() { return m_buffer; }
		unsigned char* GetByteStorage() { return m_byte_buffer; }
		unsigned Length() const { return (m_buffer || m_byte_buffer) ? m_used : 0; }
		BOOL IsByteBuffer() { return m_is_byte_buffer; }

		void SetIsByteBuffer(BOOL f);
		void Clear();
		void ClearByteBuffer(BOOL release);

		OP_STATUS GrowBuffer(unsigned extra_bytes);

		OP_STATUS Append(const uni_char* data, unsigned length);
		OP_STATUS AppendBytes(const unsigned char* data, unsigned length);
	};

	const char *GetForcedResponseType() { return request ? request->GetForcedResponseType() : NULL; }
	/**< Get overridden Content-Type on the response, if any. */

protected:
	DOM_XMLHttpRequest(BOOL is_anon);

	int status;
	OpString statusText;

	ReadyState readyState;
	/**< The current readyState, as reported by the 'readyState'
	     property. Normally it will climb up the ladder to READYSTATE_DONE,
	     starting from READYSTATE_UNSENT. One or more increments to
	     this state may be recorded before the value makes the
	     actual transition (see queued_readyState.) */

	ReadyState queued_readyState;
	/**< The pending ready state to progress 'readyState' to. If
	     none, set to READYSTATE_UNINITIALIZED. As the ready state
	     is updated following network traffic (or enforced state
	     switches by 'open()', 'send() or 'abort()'), the updates
	     are queued up so as to allow separate delivery of
	     'readystatechange' events. The 'readystatechange'
	     event handlers should only observe one at the time
	     (and not the final/latest state.) */

	DOM_HTTPRequest *request;
	DOM_LSParser *parser;

	ResponseDataBuffer responseData;
	DOM_Node *responseXML;
	ES_Object *responseBuffer;
	DOM_Blob *responseBlob;
	uni_char *responseType;
	uni_char *responseHeaders;
	unsigned readystatechanges_in_progress;
	OpVector<ES_Thread> threads_waiting_for_headers;

	BOOL async;

	BOOL send_flag;
	BOOL error_flag; ///< Can only change during calling send() or abort() (per specification).

	/** Synchronous request and security checks may cause XMLHttpRequest operations
	    to throw exceptions. These are the kinds supported. */
	enum RequestError {
		NoError,
		NetworkError,
		TimeoutError,
		AbortError
	};

	RequestError request_error_status;

	BOOL is_anonymous;
	/**< TRUE if this will be perform anonymous request; created
	     using AnonXMLHttpRequest(). */

#ifdef PROGRESS_EVENTS_SUPPORT
	DOM_XMLHttpRequestUpload *uploader;

	BOOL progress_events_flag;
	/**< TRUE if progress events are currently being delivered. */

	BOOL upload_complete_flag;
	/**< TRUE if upload of request has completed; upload
	     progress events are only delivered when it is
	     FALSE. */

	unsigned timeout_value;
	/**< The currently set timeout value. Default is 0,
	     meaning no timeout constraint on request completion. */

	BOOL delayed_progress_event_pending;
	/**< TRUE if progress event message has been posted, but
	     not received. No further progress ticks will be posted
	     while TRUE. */

	double last_progress_event_time;
	/**< Time of last progress event delivery. Used to moderate
	     the delivery of progress events to at least not more
	     often than once per 50 ms. */

	OpFileLength downloaded_bytes;
	/**< The number of bytes to be reported in the next progress event. */

	OpFileLength last_downloaded_bytes;
	/**< The number of bytes that was reported in the last progress event.
	     Only report on increase, hence the need to track. */

	BOOL delivered_onloadstart;
	/**< TRUE once request object has fired onloadstart events. */

#endif // PROGRESS_EVENTS_SUPPORT

	BOOL blocked_on_prepare_upload;
	/**< TRUE if send() is suspended waiting for its argument to serialize into
	     something that can be transmitted. */

	uni_char *username, *password;
	OpString forced_content_type;

#ifdef CORS_SUPPORT
	BOOL with_credentials;
	/**< TRUE if user credentials are to be included in the cross-origin
	     request. */

	BOOL want_cors_request;
	/**< TRUE if this request is potentially cross-origin. */

	BOOL is_cors_request;
	/**< TRUE if this request will be cross-origin. */

	OpCrossOrigin_Request *cross_origin_request;
	/**< The final, and allowed, cross-origin request. */

#endif // CORS_SUPPORT

	friend class DOM_XHR_ReadyStateChangeListener;

	ReadyState GetActualReadyState();

	OP_STATUS AdvanceReadyStateTowardsActualReadyState(ES_Thread *interrupt_thread);

	void SetListenerToCurrentThread(DOM_Runtime *origining_runtime);

#ifdef CORS_SUPPORT

	void ResetCrossOrigin();
	/**< Reset all information handling cross-origin access on the
	     object. Needed if the object is attempted reused in
		 subsequent open()s. */

	OP_BOOLEAN CheckIfCrossOriginCandidate(const URL &target_url);
	/**< Check if access of 'target_url' from 'url' would be a valid
	     CORS access to attempt and update internal state to reflect
	     if the access should be attempted using CORS. It will be
	     not attempted using CORS if it is to the same-origin or
	     the URL is unsupported by CORS. */

	OP_STATUS UpdateCrossOriginStatus(URL &url, BOOL allowed, BOOL &is_security_error);
	/**< Given the outcome of a security check from an attempted send(),
	     update this request's cross-origin status. If TRUE, the actual
		 send() request will go ahead as a cross-origin request if CORS
		 was what allowed the security check.

	     If FALSE, then this request must clear and reset all information
	     kept related to it being a potential cross-origin request.
	     The send() operation will proceed to fail either due to a security
	     or network error; the implementation will opt for NETWORK_ERR if
	     that leads to clearer error reporting.

	     @return OpStatus::OK on status updated successfully;
	             OpStatus::ERR_NO_MEMORY on OOM. */

	static OP_STATUS ProcessCrossOriginResponseHeaders(OpCrossOrigin_Request *request, TempBuffer *buffer, const uni_char *headers);
	/**< A CORS resource may constrain the response headers that must only
	     be exposed to the origin context. [CORS: 7.1.1]

	     @param request The cross-origin request.
	     @param [out] buffer The resulting header name/value string
	            from filtering the response headers against the list
	            of headers that the resource allows exposed (along
	            with ones considered simple.)
	     @param headers the newline-separated string of header name/value
	            fields in the response.
	     @return OpStatus::OK or OpStatus::ERR_NO_MEMORY. */

#endif // CORS_SUPPORT

#ifdef PROGRESS_EVENTS_SUPPORT
	BOOL HaveUploadEventListeners();
	/**< Returns TRUE if one or more upload progress event listeners
	     have been registered. */

	OP_STATUS FinishedUploading();
	/**< Called to signal the completion of the uploading of data
	     from the request. The required progress events (load,loadend)
	     will be sent along with disabling further progress event
	     updates.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

#endif // PROGRESS_EVENTS_SUPPORT

#ifdef DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE
	BOOL HasWindowDocument();
	/**< Returns TRUE if the XMLHttpRequest object exists in a context
	     which has a global Window that has an associated document.

	     The global object of Web Worker execution contexts is the sole
	     example of something that doesn't have such a document, hence
	     will return FALSE.

	     Separated out as a predicate as the specification now has side
	     conditions that use this property. */
#endif // DOM_XMLHTTPREQUEST_CONSTRAIN_SYNC_USAGE

	OP_STATUS ToJSON(ES_Value *value, ES_Runtime *runtime);
	/**< Parse the response text to a JSON value, in the context of
	     'runtime'.

	     @return OpStatus::OK on either successful translation or invalid
	             JSON input. If not well-formed input, 'value' is set to
	             the null value. OpStatus::ERR_NO_MEMORY on OOM. */

	int PrepareUploadData(ES_Value* argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime);
	/**< Add upload data in preparation for send().

	     @returns ES_FAILED on successful completion, ES_NEEDS_CONVERSION if
	              further argument conversions are needed from the engine.
	              ES_NO_MEMORY on OOM. */

#ifdef URL_FILTER
	static OP_STATUS CheckContentFilter(DOM_XMLHttpRequest *xmlhttprequest, DOM_Runtime* origining_runtime, BOOL &allowed);
	/**< Calls content_filter to verify if the URL is allowed or not
		@param xmlhttprequest XHR request used to retrieve the URL to check
		@param origining_runtime DOM runtime
		@param allowed will be set to TRUE if the URL is allowed.
		       Please note that the original value of this parameter will be
			   used as a default value if no rules match the URL, so it should
			   have a meaningful value (typically FALSE)
	*/
#endif // URL_FILTER

public:
	static OP_STATUS Make(DOM_XMLHttpRequest *&xmlhttprequest, DOM_EnvironmentImpl *environment, BOOL is_anonymous = FALSE);

	virtual ~DOM_XMLHttpRequest();

	/** Adding the constants for prototypes and constructors. */
	static void ConstructXMLHttpRequestObjectL(ES_Object* object, DOM_Runtime* runtime);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XMLHTTPREQUEST || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	// From the DOM_EventTargetOwner interface.
	virtual DOM_Object *GetOwnerObject() { return this; }

	OP_STATUS SetCredentials(URL &url);
	OP_STATUS SetReadyState(ReadyState new_readyState, ES_Thread *interrupt_thread, BOOL force_immediate_switch = FALSE);
	/**< Update the current readyState to 'new_readyState', firing the
	     required events as a result. If an event is fired,
	     'interrupt_thread' is blocked waiting for the completion
	     of event delivery.

	     If the readyState is attempted updated while delivering
	     events, the update is delayed until its completion. This
	     delaying behaviour is overridden if 'force_immediate_switch'
	     is set to TRUE. Required for methods open(), send(), and abort(),
	     as they switch readyState instantly. */

	OP_STATUS AddResponseData(const unsigned char *text, unsigned byte_length);
	BOOL IsExpectingBinaryResponse() { return responseData.IsByteBuffer(); }
	OP_STATUS AdvanceReadyStateToLoading(ES_Thread *interrupt_thread);
	void SetResponseXML(DOM_Node *responseXML);
	OP_STATUS SetResponseHeaders(const uni_char *headers);
	OP_STATUS SetResponseHeaders(URL url);

	const uni_char *GetMethod() { return request->GetMethod(); }
	DOM_HTTPRequest *GetRequest() { return request; }

	BOOL HasEventHandlerAndUnsentEvents();

	BOOL IsAnonymous() const { return is_anonymous; }

	BOOL HasTimeoutError() const { return request_error_status == TimeoutError; }

#ifdef CORS_SUPPORT
	BOOL WantCrossOriginRequest() { return want_cors_request; }
	/**< Returns TRUE if this XHR request is potentially performed cross-origin.
	     If the request target fails the same-origin check, it will be initially
	     set as "wanting cross-origin". The security manager decides if the request
	     will be allowed to go ahead at all (and IsCrossOriginRequest() is set
	     if so.) */

	BOOL IsCredentialled() { return with_credentials; }

	OpCrossOrigin_Request *GetCrossOriginRequest() { return cross_origin_request; }

	OP_STATUS CreateCrossOriginRequest(const URL &origin_url, const URL &target_url);
	/**< Create a cross-origin access request from the XMLHttpRequest request. */

	OP_STATUS SetCrossOriginRequest(OpCrossOrigin_Request *request);
#endif // CORS_SUPPORT

#ifdef PROGRESS_EVENTS_SUPPORT
	OP_BOOLEAN HandleTimeout();
	/**< Notification that the request took longer than the timeout
	     value set upon invoking send(). If the request is not
	     in the DONE state, the request error steps will be invoked
	     along with issuing a 'timeout' progress event. If the
	     request is DONE, the timeout notification is ignored,
	     return OpBoolean::IS_FALSE to signal this. */

	unsigned GetTimeoutMS();
	/**< Return the currently set timeout value for this request.
	     Zero if not explicitly set by the user via the 'timeout'
	     property to impose an upper bound on delivery of the
	     request. */

	OP_STATUS HandleProgressTick(BOOL also_upload_progress = TRUE);
	/**< Process a periodic progress event tick, issuing an upload
	     progress event and/or a download progress event. Or neither,
	     if it has been less than a set minimum (50ms) since the
	     previous progress event, then delivery is postponed.

	     The progress tick is issued by the request loader, which
	     handles incoming messages.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS HandleUploadProgress(OpFileLength bytes);
	/**< Progress notification for uploads.

	     @param bytes The delta (in bytes) that has been uploaded successfully.
	            If zero, it signals the completion of the upload.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	OP_STATUS StartProgressEvents(ES_Runtime *origining_runtime);
	/**< Set up delivery of progress events, including the delivery of
	     progress ticks.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

#endif // PROGRESS_EVENTS_SUPPORT

	// Also: dispatchEvent
	DOM_DECLARE_FUNCTION(open);
	DOM_DECLARE_FUNCTION(send);
	DOM_DECLARE_FUNCTION(abort);
	DOM_DECLARE_FUNCTION(getAllResponseHeaders);
	DOM_DECLARE_FUNCTION(getResponseHeader);
	DOM_DECLARE_FUNCTION(setRequestHeader);
	DOM_DECLARE_FUNCTION(overrideMimeType);
	enum { FUNCTIONS_ARRAY_SIZE = 9 };

	// Also: {add,remove}EventListener
	enum {
		FUNCTIONS_addEventListener = 1,
		FUNCTIONS_removeEventListener,

		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

private:
	friend class DOM_LSLoader;
	friend class DOM_LSParser;
	friend class DOM_LSParser_LoadHandler;

	OP_STATUS SetStatus(const URL &url);

	OP_STATUS SetNotModifiedStatus(const URL &url);
	/**< Appropriately update this request's status based
	     on the response being a 304/MSG_NOT_MODIFIED. */

	OP_STATUS SetNetworkError(ES_Thread *interrupt_thread);
	/**< Set internal flag that a network error has occurred. If in
	     a LOADED state and interrupt_thread is non-NULL, fire an
	     onerror event at any registered error handler. */

	void SetNetworkErrorFlag() { OP_ASSERT(request_error_status <= NetworkError); request_error_status = NetworkError; }
	/**< Set the network error flag, but do not fire an 'error' event. */

	int ThrowRequestError(ES_Value *return_value);
	/**< If the object is in an error state, propagate this by
	     throwing the correspoding DOM exception. */

	OP_STATUS SendRequestErrorEvent(ES_Thread *interrupt_thread);
	/**< If the object has encountered an request error, fire an
	     onerror event at any registered error handler. */

	OP_STATUS SendEvent(DOM_EventType event_type, ES_Thread *interrupt_thread, ES_Thread** created_thread = NULL);
	/**< Fire an event of the given type.

	     @param event_type The progress event type.
	     @param interrupt_thread The thread being interrupted on the
	            the event delivery.
	     @param [out]created_thread On success, the event thread.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

#ifdef PROGRESS_EVENTS_SUPPORT
	OP_STATUS SendProgressEvent(DOM_EventType event_type, BOOL also_as_upload, BOOL download_last, ES_Thread *interrupt_thread);
	/**< Fire a progress event of the given type. If 'also_as_upload' is FALSE,
	     only a download progress event will be attempted sent.

	     @param event_type The progress event type.
	     @param also_as_upload TRUE to attempt delivery of an upload
	            progress event also.
	     @param download_last If TRUE along with also_as_upload, then
	            the download event will be issued after the one for
	            upload.
	     @param interrupt_thread The thread being interrupted on the
	            the event delivery.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	void CancelProgressEvents();
	/**< Stop further progress from being reported  to the listeners.
	     Called upon abort or on the request reaching the DONE state. */

#endif // PROGRESS_EVENTS_SUPPORT

	class WaitingForHeadersThreadListener
		: public ES_ThreadListener
	{
	public:
		WaitingForHeadersThreadListener(DOM_XMLHttpRequest* xmlhttprequest)
			: xmlhttprequest(xmlhttprequest)
		{
		}
		virtual ~WaitingForHeadersThreadListener();
		virtual OP_STATUS Signal(ES_Thread *thread, ES_ThreadSignal signal);

	private:
		DOM_XMLHttpRequest* xmlhttprequest;
	};
	WaitingForHeadersThreadListener waiting_for_headers_thread_listener;

	friend class WaitingForHeadersThreadListener;

	void UnblockThreadsWaitingForHeaders();
	BOOL headers_received;
};

#ifdef PROGRESS_EVENTS_SUPPORT
/** Upload progress can be listened to through the XMLHttpRequest.upload
    event target. */
class DOM_XMLHttpRequestUpload
	: public DOM_Object
	, public DOM_EventTargetOwner
{
public:
	static OP_STATUS Make(DOM_XMLHttpRequestUpload *&upload, DOM_Runtime *runtime);

	virtual ~DOM_XMLHttpRequestUpload()
	{
	}

	virtual ES_GetState GetName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(const uni_char *property_name, int property_code, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_XMLHTTPREQUEST_UPLOAD || DOM_Object::IsA(type); }
	virtual void GCTrace();

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	// From the DOM_EventTargetOwner interface.
	virtual DOM_Object *GetOwnerObject() { return this; }

	BOOL HaveEventListeners();
	/**< Returns TRUE if one or more upload event listeners have been
	     registered. */

	OP_STATUS SendEvent(DOM_EventType event_type, OpFileLength bytes_length, OpFileLength bytes_total, ES_Thread *interrupt_thread = NULL);
	/**< Fire an upload progress event of the given type.

	     @param event_type The progress event type.
	     @param bytes_length The number of bytes having been uploaded so far.
	            Left as 0 if not known.
	     @param bytes_total The number of bytes that will be uploaded, assuming
	            no errors. Left as 0 if not known.
	     @param interrupt_thread The thread being interrupted on the
	            the event delivery.

	     @return OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on OOM. */

	// Also: dispatchEvent
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

	// Also: {add,remove}EventListener
	enum {
		FUNCTIONS_addEventListener = 1,
		FUNCTIONS_removeEventListener,

		FUNCTIONS_WITH_DATA_ARRAY_SIZE
	};

	OpFileLength GetUploadedBytes();
	/**< Return the upload totals, including any bytes  that have been
	     delayed reported. */

	OpFileLength GetPreviousUploadedBytes() const { return previous_uploaded_bytes; }
	/**< Return the upload totals sent in the previous progress event, or zero
	     if none sent already. */

	void SetPreviousUploadedBytes(OpFileLength new_watermark) { previous_uploaded_bytes = new_watermark; }

	void AddUploadedBytes(unsigned length, BOOL queued);
	/**< Update the uploaded totals; if the 'queued' flag is TRUE, the bytes
	     will be added to the delayed count. */

private:
	DOM_XMLHttpRequestUpload()
		: uploaded_bytes(0)
		, queued_uploaded_bytes(0)
		, previous_uploaded_bytes(0)
	{
	}

	OpFileLength uploaded_bytes;
	/**< The number of uploaded bytes that was reported in the last progress
	     event. */

	OpFileLength queued_uploaded_bytes;
	/**< The number of outstanding bytes that haven't yet been reported in a
	     progress event to listeners. Such events are staggered at 50ms ticks. */

	OpFileLength previous_uploaded_bytes;
	/**< The number of uploaded bytes previously reported. */
};
#endif // PROGRESS_EVENTS_SUPPORT

class DOM_XMLHttpRequest_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_XMLHttpRequest_Constructor();

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

class DOM_AnonXMLHttpRequest_Constructor
	: public DOM_BuiltInConstructor
{
public:
	DOM_AnonXMLHttpRequest_Constructor();

	virtual int Construct(ES_Value* argv, int argc, ES_Value* return_value, ES_Runtime *origining_runtime);
};

#endif // DOM_HTTP_SUPPORT
#endif // DOM_DOMHTTP_H
