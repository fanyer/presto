/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 */

#ifndef WEBRESOURCE_BASE_H_
#define WEBRESOURCE_BASE_H_

#include "modules/util/smartptr.h"
#include "modules/upload/upload.h"
#include "modules/util/opfile/opfile.h"
#include "modules/util/adt/bytebuffer.h"

class OpStringC16;
class OpStringC8;
class OpString8;
class OpString16;
class WebServerConnection;
class WebserverRequest;
class AddToCollectionCallback;
class Webserver_HttpConnectionListener;
class WebServer;

class WebResource : public OpReferenceCounter
{
	friend class WebServerConnection;
	friend class WebResourceWrapper;
		
public:
	virtual ~WebResource();
	
	/* Handle post data from a client. 
	 * 
	 * Data posted to this resource is fed to this function.
	 * 	
	 * @param 	incomming_data	The data.
	 * @param 	length			Length of the data.
	 * @param 	last_data		TRUE if this was the last data from the client. 
	 * 
	 * @return 	OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */  
	virtual OP_STATUS HandleIncommingData(const char *incomming_data, unsigned length, BOOL last_data = FALSE) = 0;

	/* Produce response data to client.
	 * 
	 * The resource fills the buffer, when there as available data.
	 * 
	 * @param buffer(out) 	The buffer that is filled with data.
	 * @param bufferSize	Size of the buffer.
	 * @param length(out)	The actual length that is served from resource.
	 * 
	 * 
	 * @return 	OpStatus::ERR_NO_MEMORY For OOM.
	 * 			OpStatus::IS_TRUE		If there are more data.
	 * 			OpStatus::IS_FALSE		If this was the last data from this resource. 
	 * 
	 */
	virtual OP_BOOLEAN FillBuffer(char *buffer, unsigned int buffer_size, unsigned int &length) = 0;

	/* Last time modified
	 * 
	 * @param date (out) the last time the resource was modified. If not nown, return 0 to signal current time;
	 * 
	 * @return OpStatus::ERR or OpStatus::ERR_NO_MEMORY; 
	 */
	
	/* Get Last modified. 	 
	 * 
	 * Used by cache handling. Mostly relevant for static files.
	 * 
	 * If unknown, return 0 (typically for dynamically produced data).
	 * 
	 * @param date(out) The last time this resource was modified
	 * 
	 * return OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	virtual OP_STATUS GetLastModified(time_t &date) = 0;
		
	/* Get ETag.
	 * 
	 * Used by cache handling. Mostly relevant for static files.
	 * 
	 * If unknown, return empty string (typically for dynamically produced data).
	 * 
	 * @param etag(out) The etag as produced by specific resource
	 * 
	 * return OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	virtual OP_STATUS GetETag(OpString8 &etag) { return OpStatus::OK; }
		
	/* Set the HTTP response code for the response
	 * 
	 * @param response_code 	The http response code (http://en.wikipedia.org/wiki/List_of_HTTP_status_codes)
	 * @param response_string 	The corresponding response string. If response_string=="",
	 * 							the default string corresponding to response_code will be used.
	 * 
	 * @return OpStatus::ERR_NO_MEMORY or OpStatus::OK
	 */
	OP_STATUS SetResponseCode(int response_code, const OpStringC16 &response_string = UNI_L("")); 

	/* Add a response header 
	 * 
	 * @param header_name	The header name
	 * @param header_value	The header value
	 * @param append		If TRUE, the header will we appended. If FALSE, 
	 * 						any header that exists with same name will be overwritten. 
	 * 
	 * @return 	OpStatus::ERR_NO_MEMORY For OOM.
	 * 			OpBoolean::IS_FALSE 	Security error (for example setting illegal cookie path).
	 * 			OpBoolean::IS_TRUE		success.
	 */
	OP_BOOLEAN AddResponseHeader(const OpStringC16 &header_name, const OpStringC16 &header_value, BOOL append = FALSE, BOOL cookie_check_path = TRUE, BOOL cookie_check_domain = TRUE);
	
	/** Found if a header is already present at least one time
	 * @param header_name	The header name
	 * @param found	Set to TRUE if the header is present, else set to FALSE
	 *
	 * return Error
	*/
	OP_STATUS HasHeader(const OpStringC header_name, BOOL &found);
	
	
	WebserverRequest *GetRequestObject() const { return m_requestObject; }
	
	virtual OP_STATUS GetClientIp(OpString16 &ip_address);
	
	OP_STATUS SetProtocolString(const OpStringC16 &protocol_string);

	BOOL GetFowardState();

	static OP_STATUS FindItemsInString(const uni_char *string, const uni_char *itemName, OpVector<OpString> *itemList, BOOL unescape);

	static OP_STATUS FindItemsInString(const uni_char *string, const uni_char *itemName, OpString &itemValue, BOOL &found, unsigned int &index, BOOL unescape = TRUE);
	
	static OP_STATUS FindIAlltemsInString(const uni_char *string, AddToCollectionCallback *collection, BOOL unescape, BOOL translatePlus = FALSE);
	
	// Return the connection used by the resource
	WebServerConnection *GetConnection() { return m_service; }
	
#ifndef FORMATS_URI_ESCAPE_SUPPORT
	static OP_STATUS EscapeItemSplitters(const uni_char *string, const uni_char *escape_chars, OpString &escaped_string);
#endif

	/* Add a http connection listener for this resource.
	 * 
	 * listener is NOT owned by this object, and must be removed using RemoveHttpConnectionListener(listener) if listener is deleted.
	 * 
	 * @param listener 	The listener. The callback listener->WebServerConnectionKilled() 
	 * 					is called if the connection for this resource goes down.
	 */ 	
	void AddHttpConnectionListener(Webserver_HttpConnectionListener *listener);

	/* Remove a http connection listener for this resource.
	 * 
	 * @param listener The listener to be removed. This MUST be done if listener is deleted. 
	 */
	void RemoveHttpConnectionListener(Webserver_HttpConnectionListener *listener);
	
	/** Get the value of an header */
	static OP_STATUS GetHeaderValue(Header_Item *header, OpString8 &value);
	
protected:
	
	/* The request is closed and put back in the request queue.
	 * 
	 * @return		OpStatus::ERR_NO_MEMORY of OOM 	.
	 * 				OpBoolean::IS_FALSE		if subserver name has changed, which is a security breach.
	 *				OpBoolean::IS_TRUE		if success.
	 */
	OP_BOOLEAN ReIssueRequest();

	/*We need to know if the WebServerConnection object has been deleted, so that we do not call the WebServerConnection object*/
	
	virtual OP_STATUS OnConnectionClosedBase();
	
	virtual OP_STATUS OnConnectionClosed() = 0;

	/*Check if the clients cache is updated*/	
	OP_BOOLEAN ResourceModifiedSince(const char *clientsDate, const char *resourceLastDateChanged);

	WebResource(WebServerConnection *service, WebserverRequest *requestObject = NULL, OpSocketAddress *socketAddress = NULL);
	
	/* The headers that we build up and send in response */
	Header_List m_userResponseHeaders;
	
	
	WebServerConnection *m_service;

	BOOL m_headers_sent;
	
	/*Should we send body data? For example, if method is HEAD we do not send body data*/
	BOOL m_sendBody;
	
	WebserverRequest *m_requestObject;
	
	OpSocketAddress *m_socketAddress;
	
	OpString8 m_protocolString;

	/* length of the resource. This is initialized to ~0, which means that no length has been set*/
	OpFileLength m_totalResourceLength;

	OpString8 m_responseCodeString;
	
	int m_responseCode;

	
	/// Automatically set the session cookie
	BOOL m_auto_cookies;

	BOOL m_allowPersistentConnection;
	
private:

	OP_BOOLEAN BaseCheckModified();
	
	OP_STATUS SetResponseString(OpString8 &string, int responseCode);
	
 	OP_STATUS BuildResponseHeaders(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);

	OP_BOOLEAN GetData(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);
		
	/* Both m_noMoreDataToSend and m_allOutDataIsSent must be TRUE,
	 * before we can finish a request and start handling the next
	 * Note: we do not know in which order they are set to TRUE
	 */
	
	OP_STATUS SetupResponseCode(int &responseCode);
	 	
	/*If this TRUE, there is no more data to send for the current request*/
	BOOL m_noMoreDataToSend;
	
	/* If this is TRUE, all data for the current request has been sent out on the socket
	 * We set this to TRUE when we get the callback from opera, saying all has been sent*/
	BOOL m_allOutDataIsSent;	
	
	unsigned int m_headerLength;
	
	time_t m_lastDateChanged;
	
	Head m_HttpConnectionListeners;
};

/** Class that implements a standard behaviour for Administrative C++ services*/
class WebResource_Administrative_Standard: public WebResource
{
protected:
	WebResource_Administrative_Standard(WebServerConnection *service, WebserverRequest *requestObject);

	// WebServer (it provides the service list)
	WebServer *server;
	// XML with the answer
	ByteBuffer buf;
	// Number of bytes already read
	UINT32 read_bytes;

public:
	/* Destructor */
	virtual ~WebResource_Administrative_Standard() { }
	
	/* Serve the XML used for UPnP Service Discovery */
	virtual OP_BOOLEAN FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);
	
	virtual OP_STATUS GetLastModified(time_t &date);

	OP_STATUS OnConnectionClosed(){ return OpStatus::OK; }	
	
	virtual OP_STATUS HandleIncommingData(const char *incommingData, UINT32 length, BOOL lastData=FALSE);
};

#endif /*WEBRESOURCE_BASE_H_*/
