/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation -- script virtual resources
 */

#ifndef WEBSERVER_CUSTOM_RESOURCE_H_
#define WEBSERVER_CUSTOM_RESOURCE_H_

#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webresource_base.h"
#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver_body_objects.h"
#include "modules/util/adt/opvector.h"

class OpFile;
class FlushIsFinishedCallback;
class ObjectFlushedCallback;
class AddToCollectionCallback;
class WebServerConnection;
class WebserverUploadedFileWrapper;
class WebserverFolderCallback;
class Webserver_HttpConnectionListener;
class AbstractMultiPartParser;

class WebResource_Custom : public WebResource
{
public:
	static WebResource *Make(WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, unsigned long windowId, WebServerFailure *result);

	virtual ~WebResource_Custom();

	/* Finds the items of type item1=value1&item2=value2.. in url.
	 * 
	 * For each item that is found collection->AddToCollection("item","value") will be called. 
	 * 
	 * @param collection The callback for adding items.
	 * 
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetAllItemsInUri(AddToCollectionCallback *collection, BOOL unescape = TRUE, BOOL translatePlus = FALSE);

	/* Finds all the values for a specific "itemName" in the url.
	 * 
	 * @param itemName The name of the item to find.
	 * @param itemList The list of values that was found.
	 * 
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetItemInUri(const OpStringC16 &itemName, OpAutoVector<OpString> *itemList, BOOL unescape = TRUE);

	/* Finds the items of type item1=value1&item2=value2 etc in post body.
	 * 
	 * For each item that is found collection->AddToCollection("item","value") will be called. 
	 * 
	 * @param collection The callback for adding items.
	 * 
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */	
	OP_STATUS GetAllItemsInBody(AddToCollectionCallback *collection, BOOL unescape = TRUE);

	/* Finds all the values for a specific "itemName" in the post body.
	 * 
	 * @param itemName The name of the item to find.
	 * @param itemList The list of values that was found.
	 * 
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetItemInBody(const OpStringC16 &itemName, OpAutoVector<OpString> *itemList, BOOL unescape = TRUE);
	
	/* Appends a string to the data which is served on the request.
	 * 
	 * @param bodyData The string to be appended.
	 * 
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS AppendBodyData(const OpStringC16 &bodyData);
	OP_STATUS AppendBodyData(const OpStringC8 &bodyData);

	/* Appends a data object to the data which is served on the request.
	 * 
	 * The data object be images, strings, etc.
	 *
	 * 	@param bodyDataObject 	The data object that should be sent to client. 
	 *							The bodyDataObject object MUST NOT be deleted as long 
	 *							as callback->BodyObjectFlushed has not been called.
	 *	@param callback			When the data has been sent callback->BodyObjectFlushed will be called.
	 *	
	 *	@return OpStatus::ERR_NO_MEMORY if OOM.
	 *			OpStatus::ERR 			if bodyDataObject has been appended twice. 
	 *	
	 *	If success WebResource_Custom takes ownership over the object.
	 * 	If error the caller must delete the object. 
	 */	
	OP_STATUS AppendBodyData(WebserverBodyObject_Base *bodyDataObject, ObjectFlushedCallback *callback);

	/* Flush whatever is in the buffer.
	 *	
	 *	@param flushIsFinishedCallback 	flushIsFinishedCallback->FlushIsFinished() will be called
	 *									when all the body objects that had been appended before Flush() was called,
	 *									has been flushed.
	 *									
	 *									If Flush returns and error, the flushIsFinishedCallback->FlushIsFinished will not be called.
	 *	
	 *	@return OpStatus::ERR_NO_MEMORY if OOM.
	 *			OpStatus::ERR 			if connection has closed.
	 */
	OP_STATUS Flush(FlushIsFinishedCallback *flushIsFinishedCallback, unsigned int flushId); 

	/* Flush whatever is in the buffer and close the request.
	 *	
	 *	@param flushIsFinishedCallback 	flushIsFinishedCallback->FlushIsFinished() will be called
	 *									when all the body objects that had been appended before Close() was called,
	 *									has been flushed.  
	 *									
	 *									If Close returns and error, the flushIsFinishedCallback->FlushIsFinished will not be called. 
	 *	
	 *	@return OpStatus::ERR_NO_MEMORY if OOM.
	 *			OpStatus::ERR 			if request has closed.
	 */	
	OP_STATUS Close(FlushIsFinishedCallback *flushIsFinishedCallback, unsigned int flushId);
	
	/* Close the request, and put the request back in the request queue.
	 * 
	 * Next time the request is handled, it will be ignored by javascript,
	 *  and sent directly to disk or other resources that might handle the request.  
	 *	
	 *	@return		OpStatus::ERR_NO_MEMORY of OOM. 	
	 * 				OpBoolean::IS_FALSE		if subserver name has changed, which is a security breach. In this case, the request is not forwared to disk.
	 *				OpBoolean::IS_TRUE		if success.
	 */
	OP_BOOLEAN CloseAndForward();

	/* Get request headers from the request.
	 * 
	 * @param headerName The header to search for.
	 * @param headerList The list of all headers named headerName.
	 * 
	 * @return OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS GetRequestHeader(const OpStringC16 &headerName, OpAutoVector<OpString> *headerList);
	
	/* Get the Host header from the request.
	 * 
	 * @param host Will be populated with the value of the Host header
	 * 
	 * @return OpStatus errors
	 */
	OP_STATUS GetHostHeader(OpString &host);
		
	/* Get the request uri for this resource. 
	 * 
	 * @return The normalized uri;
	 */
	const uni_char *GetRequestURI();

	/* Get the escaped request uri for this resource. 
	 * 
	 * @param request(out) the escaped and normalized request uri.
	 * 
	 * @param return 	OpStatus::ERR_NO_MEMORY for OOM.
	 * 				 	OpStatus::OK for success.
	 */
	OP_STATUS GetEscapedRequestURI(OpString &request);
	
	/* Get the http method for this resource. */
	WebServerMethod GetHttpMethod();
	
	/* Get the http method string for this resource. */
	const uni_char* GetHttpMethodString();

	/* Get the post body in the request for this resource.
	 * 
	 * @param postBody the body string.
	 * 
	 * @return 	OpStatus::OK or OpStatus::ERR_NO_MEMORY.
	 * 			OpStatus::ERR for generic errors.
	 */
	OP_STATUS GetRequestBody(TempBuffer *postBody);	
	
	/* 	Get the files posted to this resource.
	 * 
	 * 	@return the vector of uploaded files.
	 */
	OpVector<WebserverUploadedFileWrapper> *GetMultipartVector();

	/* Set chunked encoding on/off.
	 * Can only be done before any flush or close has happened.
	 * 
	 * @param  chunked If TRUE chunked encoding will be on.	
	 *	
	 * @return	OpStatus::OK if success. 
	 * 			OpStatus::ERR if this resource has flushed or closed.
	 */
	OP_STATUS setChunkedEncoding(BOOL chunked);
	
	/* Get chunked encoding state.
	 * 
	 * @return TRUE if this resource is using chunked encoding.
	 */
	BOOL GetChunkedEncoding();

	/* Get flush state.
	 * 
	 * @return TRUE if this resource is in flushing state.
	 */	
	BOOL IsFlushing();

	/* Get proxy state.
	 * 
	 * @return TRUE if this resource is serving going through the alien proxy.
	 */	
	BOOL IsProxied();

#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	/*
	 * Client is the owner of this server. Can be used to give extra privileges.
	 * 
	 * @return TRUE if the request comes from the owner of the server
	 */
	BOOL ClientIsOwner();
	
	/*
	 * The request comes from the same instance of opera ( (opera is speaking to itself)
	 * 
	 * @return TRUE if the client browser is the same as this webserver
	 */	
	BOOL ClientIsLocal();
#endif
	
	/* Implementation of WebResource */
	virtual OP_BOOLEAN FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);
			
	virtual OP_STATUS HandleIncommingData(const char *incommingData, UINT32 length, BOOL lastData = FALSE);
	
	virtual OP_STATUS GetLastModified(time_t &date) { date = 0; return OpStatus::OK; }
		 
private:
	WebResource_Custom(WebServerConnection *service, WebserverRequest *requestObject, OpSocketAddress *socketAddress);
	void SignalScriptFailed();
	OP_STATUS InitiateHandlingPostBodyData();
	
	OP_STATUS sendEvent(unsigned long id, const uni_char* event);
	
	OP_STATUS OnConnectionClosed();

	/* Any query string, or NULL.  Heap-allocated string owned by this WebResource_Custom instance. */
	const uni_char *m_query;

	/* The file where post and put data is saved. */		
	OpFile *m_incomingDataFile;

	/* We can also save the incoming data to memory if the content-type == application/x-www-form-urlencoded. */
	OpString m_incommingDataMem;
	
	/* The size of incommingDataMem. */	
	//unsigned int m_incommingDataMemSize; 
		
	BOOL m_saveIncommingBodyToFile;
	
	BOOL m_closeRequest;
	
	BOOL m_requestClosed;
	
	BOOL m_hasStartedFlushing;
	
	BOOL m_chunkedEncoding;
	
	/*When a chunk ends with a empty line("CRLF CRLF"), we signal the browser that this was the last chunk. */	
	OpString8 m_chunkData; 
	
	BOOL m_postBodyHasMultiparts;
	
	AbstractMultiPartParser *m_multiPartParser;
	
	OpAutoVector<WebserverUploadedFileWrapper> m_multiParts;
	
	OpAutoVector<WebserverBodyObject_Base> m_bodyObjects;
	
	
	WebserverUploadedFile *m_currentMultiPart;
	
	OpFile *m_currentMultiFile;
	
	OpString m_eventName;
	
	WebserverBodyObject_Base *m_currentFlushObject; /* The body object we currently are flushing */
	
	BOOL m_hasSentConnectionClosedSignal;
	
	unsigned int m_windowId;

	enum IncomingDataType
	{
		NONE = 0,
		FORMURLENCODED,
		TEXT,
		MULTIPART,
		BINARY
	} m_incomingDataType;
	
	
};
#endif /*WEBSERVER_CUSTOM_RESOURCE_H_*/
