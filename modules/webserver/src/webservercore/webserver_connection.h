/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2004 - 2010
 *
 * Web server implementation 
 */

#ifndef WEBSERVER_CONNECTION_H_
#define WEBSERVER_CONNECTION_H_

#ifdef WEBSERVER_SUPPORT

#include "modules/webserver/webresource_base.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webserver_request.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/upload/upload.h"
#include "modules/hardcore/mh/generated_messages.h"
#include "modules/util/simset.h"
#include "modules/webserver/src/webservercore/webserver-references.h"
#include "modules/webserver/webserver_direct_socket.h"

class WebServerConnectionListener;
class WebserverRequest;
class HeaderList;

#ifdef UPNP_SUPPORT
class UPnP;
#endif // UPNP_SUPPORT

struct ProtocolVersion
{
	int protocol_major;
	int protocol_minor;	
	
};

enum WebServerIncommingDataType
{
	WEB_DATA_TYPE_REQUEST,       	/*The current incomming data is a request line*/
	WEB_DATA_TYPE_BODY				/*THe current incmming data is a request body (as in post or put)*/
};


/** Null resource:
    Used for various simple things, and for sending error replies.
    */
class WebResource_Null : public WebResource
{
public:
	virtual ~WebResource_Null(){}
	static OP_STATUS Make(WebResource *&nullResource, WebServerConnection *service, WebserverRequest *requestObject, int responseCode, const OpStringC &body = UNI_L(""));

	OP_STATUS HandleIncommingData(const char *incommingData, unsigned length, BOOL lastData = FALSE);
	virtual OP_BOOLEAN FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);
	virtual OP_STATUS GetLastModified(time_t &date){ date = 0; return OpStatus::OK; }
	OP_STATUS OnConnectionClosed(){ return OpStatus::OK; }
protected:
	WebResource_Null(WebServerConnection *service,WebserverRequest *requestObject);
	OpString8 m_returnData;
};

class WebServerConnection : public 	Link,
								 	public OpSocketListener ,
								 	public MessageObject, 
								 	public OpTimerListener								 
{
friend class WebResource;	
	
public:
/** Special second phase constructor for WebserverDirectServerSocket. It tries to fix CORE-21620 */

OP_STATUS Construct(WebserverDirectServerSocket *socket, WebServerConnectionListener *aConnectionListener 
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT                                         
                                         , BOOL client_is_owner = FALSE
                                         , BOOL client_is_local = FALSE                                          
#endif

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
                                         , BOOL proxied_connection = FALSE, const OpStringC8 &rendezvous_client_ip = ""
#endif
);

/** Common second phase constructor */
OP_STATUS Construct(OpSocket *socket, WebServerConnectionListener *aConnectionListener 
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT                                         
                                         , BOOL client_is_owner = FALSE
                                         , BOOL client_is_local = FALSE                                          
#endif

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
                                         , BOOL proxied_connection = FALSE, const OpStringC8 &rendezvous_client_ip = ""
#endif
);
	
	INTPTR Id();							
	ProtocolVersion getProtocolVersion();

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	BOOL IsProxied() { return m_proxied_connection; }
#endif
	OP_STATUS GetSocketAddress(OpSocketAddress *socketAddress);	
	
#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	/**
	 * Client is the owner of this server.
	 * 
	 * @return TRUE if the client browser is the same as this webserver (opera is speaking to itself) 
	 */
	BOOL ClientIsOwner(){ return m_client_is_owner; }
	
	BOOL ClientIsLocal(){ return m_client_is_local; }
#endif	
	
	WebResource *GetWebresource();

	WebSubServer *GetCurrentSubServer() { return m_currentSubserver; }
	
	WebserverRequest *GetCurrentRequest() { return m_currentRequest; }
	
	void MakeIgnoreIncommingData();	 

    OP_STATUS handleIncomingBodyData(BOOL redirected);
			 
	void AcceptReceivingBody(BOOL accept=TRUE);

	OP_STATUS StartServingData();
	void PauseServingData();

	OP_STATUS ContinueServingData();

	void OnSocketListenError(OpSocket* socket, OpSocket::Error error){}


	~WebServerConnection();
	
	WebServerConnection();

	void OnTimeOut(OpTimer* timer);	
	
	void OnSocketDataReady(OpSocket* socket);

	
	void KillService(OP_STATUS status = OpStatus::OK);
	
	double GetTransferRate() { return m_transfer_rate; }
	
	/* Get the protocol of the request: "http", "https" or "unite"
	 * 
	 * @return OpStatus error
	 */
	OP_STATUS GetProtocol(OpString &protocol);
	
	/// Return the WebServer object	
	WebServer *GetWebServer();
	/// Return the Connection lintener
	WebServerConnectionListener *GetConnectionLister() { return reinterpret_cast<WebServerConnectionListener *>(m_connectionListener.GetPointer()); }
	/// Return TRUE if a given response header is present
	BOOL IsResponseHeaderPresent(const char *name);
	/// Return the value of the response header
	OP_STATUS GetResponseHeader(const char *name, OpString8 &value);
	
#ifdef UPNP_SUPPORT
	/// Return the UPnP object
	UPnP *GetUPnP();
#endif // UPNP_SUPPORT
	
private:

	enum ConnectionState
	{
		ST_INDETERMINATE,			/* Not yet set up */
		ST_PARSING_REQUEST,			/* Service: parsing the request */
		ST_SETTING_UP_SERVICE,		/* Service: getting ready to serve the request */
		ST_WAITING_FOR_RESOURCE,	/* Service: waiting for resource to be ready to */
		ST_SERVING_DATA	,			/* Service: replying */
		ST_WATING_FOR_PUTPOST_BODY,  /* service: waiting for the body after a Expect: 100-continue header from client*/
		ST_CLOSING
	};
	
	struct TransferSample
	{
		TransferSample()	
				: m_bytes(0),
				  m_time(0)
			
		{	
		}
		
		long m_bytes;
		double m_time;
	};
	
	OP_STATUS ReIssueRequest(Header_List &responseHeaders, int responseCode);
	
	unsigned int ComputeNextSendDelay(UINT bytesSent);
	
	void InitTransferRate();
	
	double CalculateTransferRate(UINT bytesTransfered, double min_interval, BOOL outgoing, BOOL requestFinished = FALSE);
	
	WebserverRequest *FindRequestById(INTPTR id);

	// implementation of MessageObject 
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
	
	void StartReceivingBodyData();

	OP_STATUS Send100Continue();

	/*Reads and handles the incomming request lines*/
    OP_STATUS ReadIncomingData();
    
    // Process the headers. If redirected is TRUE, a permanent redirect has been sent to the client, so the current operation should stop
    OP_STATUS handleIncomingRequstData(BOOL &redirected);
    

    /*incommingRequest is a char of requests separated by "CRLF CRLF"
     * This method parses, separates and puts the requests in a queue
     * 
     * @param  incommingRequest The incomming pipeline of requests
     * @return incommingRequest This will now point to the data after the last request
     */     
    OP_STATUS ParseAndPutRequestsInQueue(char *&incommingRequest);
    
    /*Find one request, and parses it into requestObject*/
	OP_STATUS ParseRequest(WebserverRequest *&requestObject,char *incommingRequest,unsigned int requestLength);
	
    /* Checks the queue, and setup a service for the first request in line*/
	OP_STATUS CheckRequestQueue();
	

	/*Not implemented yet*/
	OP_BOOLEAN SetupError(WebserverRequest *requestObject);

	/*set up the service*/
		
	OP_BOOLEAN SetupService();
	OP_BOOLEAN FindResourceInSubServer(WebResource *&webResource,  WebSubServer *subServer, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebServerFailure *result);
	OP_STATUS ServeData(UINT bytes_sent);
	OP_STATUS FinishRequestAndLookForNextRequest();

	
	void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);
	void OnSocketClosed(OpSocket* socket);
	void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	void OnSocketSendError(OpSocket* socket, OpSocket::Error error);

	void OnSocketConnectionRequest(OpSocket* socket);
	void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent);
	void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	
	void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);
	void OnSocketConnected(OpSocket* socket);
#ifdef _EXTERNAL_SSL_SUPPORT_	
	virtual void OnSecureSocketError(OpSocket* socket, OpSocket::Error error){ OP_ASSERT(!"DOES NOT MAKE SENSE IN AN INCOMMING CONNECTION"); }
#endif // _EXTERNAL_SSL_SUPPORT_

	void SocketCloseErrorL(OpSocket* aSocket,OpSocket::Error aSocketErr);	
	void SocketDataSendProgressUpdateL(OpSocket* aSocket, UINT aBytesSent);	// Ignored
	void SocketConnectedL(OpSocket* aSocket);										// Not used
	void SocketConnectErrorL(OpSocket* aSocket,OpSocket::Error aSocketErr);	// Not used

	
	BOOL MatchMethod(WebServerMethod *method, unsigned int *state, char *str);
	BOOL Match(const char *s, unsigned int *state,char *str,unsigned int length);
	int  MatchWS(unsigned int *state,char *str,unsigned  int length);
	BOOL MatchNonWS(UINT *start, unsigned int *state,char *str,unsigned  int length);
	BOOL MatchDecimal(int *value, unsigned int *state,char *str,unsigned  int length);
	
	// Return the real socket pointer
	OpSocket *GetSocketPointer() { return m_direct_socket_srv_ref.HasPointerEverBeenSet() ? (OpSocket *)(m_direct_socket_srv_ref.GetPointer()) : m_socket_ptr; }

	/* 
	 * removes the data between char* theBuffer and Char* notHandledData 
	 * @param theBuffer The from buffer pointer
	 * @param notHandledData the to buffer pointer
	 * @param theBytesInBuffer The current amount of bytes in the buffer
	 * @return the new number of bytes in the buffer after remove
	 * */
	UINT RemoveHandledData(char *theBuffer,char* notHandledData, UINT theBytesInBuffer);
	
	/*removes the dangerous /../ and /./ from the path
	 * 
	 * @param filePath The path that should be resolved
	 * @param length  OUT-parameter:the length of the filePath, returns with the new length
	 * @return OpStatus:OK if the path was legal, error if not
	 */		
	OP_STATUS ResolveFilePath(char *filePath,int &length);
	
	/* SERVER/SERVICE: The current state of this instance. */
	ConnectionState m_state;								

	OpTimer* m_socketTimer;
	
	/** Pointer set for the socket; in the scope of CORE-21620, this pointer cannot really be trusted.
	it should NEVER be used directly, but GetSocketPointer() should instead be called */
	OpSocket *m_socket_ptr;
	/** (when available), this pointer is the real one */
	ReferenceUserSingle<WebserverDirectServerSocket> m_direct_socket_srv_ref;

	WebResourceWrapper *m_resourceWrapper;								/**< SERVICE: the resource being served */

	// Reference to the connection listener
	ReferenceUserSingle<WebServerConnectionListenerReference> m_connectionListener;
	
	
	/* The queue of incomming requests to this server*/
	AutoDeleteHead m_requestQueue;  

	/*Is the connection handling a request?*/
	BOOL m_isHandlingRequest;
	
	
	BOOL m_waitFor100Continue;
	
	WebserverRequest *m_currentRequest;
		 
	WebSubServer *m_currentSubserver;
	
	WebServerIncommingDataType m_incommingDataType;
		 
	unsigned int m_bytesInBuffer;		 
		 
		 
	/*incomming body stuff(POST and PUT), we will move this to a class for incomming data*/ 

		 
	unsigned int m_incommingBodyLength;		 
		 
	unsigned int m_lengthOfBodyReceived;
		 
	/*End of body stuff*/
	

	/* SERVICE: Set to the HTTP protocol number seen in the request,
	   or to 0 and 9 if no protocol number was found. */
	ProtocolVersion m_protocolVersion;
	
	/* Input data buffer: used to parse the request and body.  Heap-allocated, owned
	   by this WebServerConnectionListener instance. */
	char *m_buf;

	/* Data buffer. */	
	char *m_resourceBuffer;
	
	/* Size of the buffer */
	unsigned int m_buflim;

	BOOL m_dying;
	
	BOOL m_acceptIncommingData;

	/* Index of next free char in buf */	
	unsigned int m_resourceBufPtr;

	/* Index past last used char in buf*/
	unsigned int m_resourceBufLim;
		
	OP_BOOLEAN m_moreData;
	
	unsigned int m_delayMS;
	
	const char* m_100ContinueString;
	
	TransferSample m_sample;
	
	TransferSample m_prev_sample;
	
	double m_transfer_rate;
	
	BOOL m_hasSentFinishedEvent;
	
	BOOL m_requestQueueLengthLimitReached;
	
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT	
	BOOL m_proxied_connection;
	OpString m_rendezvous_client_address;
#endif	
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT	
	BOOL m_client_is_owner;
	BOOL m_client_is_local;
#endif	
};

#endif // WEBSERVER_SUPPORT

#endif /*WEBSERVER_CONNECTION_H_*/
