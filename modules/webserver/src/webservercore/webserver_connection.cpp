/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2004 - 2011
 *
 * Web server implementation -- overall server logic
 */
#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/webserver_custom_resource.h"
#include "modules/webserver/webserver_request.h"
#include "modules/webserver/src/resources/webserver_file.h"
#include "modules/webserver/src/resources/webserver_upnp_discovery.h"
#include "modules/webserver/src/resources/webserver_robots_txt.h"
#include "modules/webserver/src/resources/webserver_asd.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/webserver_direct_socket.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"
#include "modules/doc/frm_doc.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/prefsfile/prefsfile.h"
#include "modules/prefsfile/prefssection.h"
#include "modules/util/opfile/opfolder.h"
#include "modules/util/gen_math.h"
#include "modules/formats/hdsplit.h"
#include "modules/webserver/src/rendezvous/control_channel.h"

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#include "modules/webserver/src/rendezvous/webserver_rendezvous_util.h"
#endif /* WEBSERVER_RENDEZVOUS_SUPPORT */

#include "modules/pi/OpLocale.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/ecmascript_utils/essched.h"

#include "modules/ecmascript/ecmascript.h"
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/webserver/src/resources/webserver_index.h"

WebServerConnection::WebServerConnection()
	: m_state(ST_INDETERMINATE)
	, m_socketTimer(NULL)
	, m_socket_ptr(NULL)
	, m_direct_socket_srv_ref(FALSE, FALSE)
	, m_resourceWrapper(NULL)
	, m_connectionListener(FALSE, FALSE)
	, m_isHandlingRequest(FALSE)
	, m_waitFor100Continue(FALSE)
	, m_currentRequest(NULL)
	, m_currentSubserver(NULL)
	, m_incommingDataType(WEB_DATA_TYPE_REQUEST)
	, m_bytesInBuffer(0)
	, m_incommingBodyLength(0)
	, m_lengthOfBodyReceived(0)
	, m_buf(NULL)
	, m_resourceBuffer(NULL)
	, m_buflim(0)
	, m_dying(FALSE)
	, m_acceptIncommingData(FALSE)
	, m_resourceBufPtr(0)
	, m_resourceBufLim(0)
	, m_moreData(FALSE)
	, m_delayMS(2)
	, m_transfer_rate(0)
	, m_hasSentFinishedEvent(FALSE)
	, m_requestQueueLengthLimitReached(FALSE)
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	, m_proxied_connection(FALSE)
#endif
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
	, m_client_is_owner(FALSE)
	, m_client_is_local(FALSE)
#endif
{
	m_protocolVersion.protocol_major = 0;
	m_protocolVersion.protocol_minor = 0;
	m_100ContinueString = "HTTP/1.1 100 Continue\n\n";
}

OP_STATUS WebServerConnection::Construct(WebserverDirectServerSocket *socket, WebServerConnectionListener *aConnectionListener 
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT                                         
                                         , BOOL client_is_owner
                                         , BOOL client_is_local
#endif
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
                                         , BOOL proxied_connection, const OpStringC8 &rendezvous_client_ip
#endif
)
{
	OP_STATUS ops=Construct((OpSocket *)socket, aConnectionListener
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
                                         , client_is_owner
                                         , client_is_local
#endif
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
                                         , proxied_connection, rendezvous_client_ip
#endif
	);
	
	RETURN_IF_ERROR(ops);
	
	if(m_socket_ptr)
	{
		OP_ASSERT(!m_direct_socket_srv_ref.HasPointerEverBeenSet());
		OP_ASSERT(GetSocketPointer()==m_socket_ptr);
		OP_ASSERT(GetSocketPointer()==socket);
		m_direct_socket_srv_ref.AddReference(socket->GetReferenceObjectPtrConn());
		OP_ASSERT(m_direct_socket_srv_ref.HasPointerEverBeenSet());
		OP_ASSERT(GetSocketPointer()==m_socket_ptr);
		OP_ASSERT(GetSocketPointer()==socket);
	}
	
	return OpStatus::OK;
}

OP_STATUS WebServerConnection::Construct(OpSocket *socket, WebServerConnectionListener *aConnectionListener
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
                                         , BOOL client_is_owner
                                         , BOOL client_is_local
#endif

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
                                         , BOOL proxied_connection, const OpStringC8 &rendezvous_client_ip
#endif
)
{
	//DEBUG_VERIFY_SOCKET(socket);

	//m_connectionListener = aConnectionListener;
	if(aConnectionListener)
		m_connectionListener.AddReference(aConnectionListener->GetReferenceObjectPtr());

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	m_proxied_connection = proxied_connection;
#endif

#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
	OP_ASSERT(client_is_owner == FALSE || (client_is_owner && client_is_local)); // If owner == TRUE, then local also must be TRUE.

	m_client_is_owner = client_is_local && client_is_owner; // Force is_owner to FALSE if is_local is FALSE
	m_client_is_local = client_is_local;
#endif

	m_socketTimer = OP_NEW(OpTimer, ());
	if (m_socketTimer)
	{
		m_socketTimer->SetTimerListener(this);
		m_socketTimer->Start(WEB_PARM_CONNECTION_TIMEOUT);
	}
	else
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	/*End: Initalize the timer*/

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	if (!rendezvous_client_ip.IsEmpty())
	{
		RETURN_IF_ERROR(m_rendezvous_client_address.Set(rendezvous_client_ip));
	}

#endif
	/* We register this object to listen to the MSG_WEBSERVER_DATAREADY message
	 * This is used for the servedata method */

	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_WEBSERVER_DATAREADY, Id()));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_WEBSERVER_FORCE_READING_DATA, Id()));

	// only take ownership of the socket after successful construction
	m_socket_ptr = socket;

	//DEBUG_VERIFY_SOCKET(GetSocketPointer());

	return OpStatus::OK;
}

WebServer *WebServerConnection::GetWebServer()
{
	return GetConnectionLister()?GetConnectionLister()->GetWebServer():NULL;
}

#ifdef UPNP_SUPPORT
UPnP *WebServerConnection::GetUPnP()
{
	return GetConnectionLister()?GetConnectionLister()->m_upnp:NULL;
}
#endif // UPNP_SUPPORT

INTPTR WebServerConnection::Id()
{
	return (INTPTR)this;
}

OP_STATUS WebServerConnection::GetProtocol(OpString &protocol)
{
	if(!GetSocketPointer())
		return OpStatus::ERR_NULL_POINTER;

//#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
	//if(m_client_is_owner && m_client_is_local)
	//	return protocol.Set("unite");
	//else
//#endif // WEBSERVER_DIRECT_SOCKET_SUPPORT
		return protocol.Set("http");

	/*OpSocketAddress *addr;
	OP_STATUS ops;

	RETURN_IF_ERROR(OpSocketAddress::Create(&addr));

	if(OpStatus::IsSuccess(ops=GetSocketPointer()->GetSocketAddress(addr)))
	{
		if(addr->Port()==0)
		{
		#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
			if(m_client_is_owner && m_client_is_local)
				ops=protocol.Set("unite");
			else
		#endif // WEBSERVER_DIRECT_SOCKET_SUPPORT
				ops=OpStatus::ERR_OUT_OF_RANGE;
		}
		else
			ops=protocol.Set("http");
	}

	OP_DELETE(addr);

	return ops;*/
}

void
WebServerConnection::OnTimeOut(OpTimer* timer)
{
	OP_ASSERT(m_requestQueueLengthLimitReached == FALSE); /* socket timeout should not happen when pausing socket reading */

	OP_ASSERT(timer == m_socketTimer);

	KillService();
}

WebServerConnection::~WebServerConnection()
{

	g_main_message_handler->RemoveCallBacks(this, Id());
	g_main_message_handler->UnsetCallBacks(this);

	if (InList())
	{
		Out();
	}

	OP_DELETE(GetSocketPointer());
	m_socket_ptr=NULL;

	OP_DELETE(m_socketTimer);
	m_socketTimer=NULL;

	OP_DELETEA(m_buf);

	OP_DELETEA(m_resourceBuffer);

	if (m_resourceWrapper && OpStatus::IsMemoryError(m_resourceWrapper->Ptr()->OnConnectionClosedBase()))
	{
		g_webserverPrivateGlobalData->SignalOOM();
	}

	OP_DELETE(m_resourceWrapper);
}

void //new version of method
WebServerConnection::OnSocketConnectionRequest(OpSocket* aSocket)
{
	OP_ASSERT(!"WebServerConnection:We should not get new connection requests on connections");
}

void
WebServerConnection::OnSocketDataReady(OpSocket* aSocket)
{
	if (m_state == ST_CLOSING || m_dying || m_requestQueueLengthLimitReached)
	{
		return;
	}
	/* SERVICE: Incoming request data on service GetSocketPointer() */

	OP_ASSERT(aSocket == GetSocketPointer() );

	/*We may ignore this signal if we are not ready, or if we have allready have osted a message to read more data*/
	if (m_socketTimer)
	{
		m_socketTimer->Stop();
		m_socketTimer->Start(WEB_PARM_CONNECTION_TIMEOUT);
	}
	OP_STATUS status;
	if (OpStatus::IsError(status = ReadIncomingData()))
	{
		KillService(status);
	}
}

void WebServerConnection::InitTransferRate()
{
	m_prev_sample.m_bytes = 0;
	m_prev_sample.m_time = 0;

	m_sample.m_bytes = 0;
	m_sample.m_time =  0;

	m_hasSentFinishedEvent = FALSE;
}

double
WebServerConnection::CalculateTransferRate(UINT bytesTransfered, double min_interval, BOOL outgoing, BOOL requestFinished)
{
	m_sample.m_bytes += bytesTransfered;
	m_sample.m_time = g_op_time_info->GetRuntimeMS();

	if (m_prev_sample.m_time != 0)
	{
		double interval = (m_sample.m_time - m_prev_sample.m_time ) / 1000;

		if (interval > min_interval)
		{
			m_transfer_rate = ((m_sample.m_bytes - m_prev_sample.m_bytes) / interval) / 1024;

			if (m_currentRequest)
			{
				WebSubServer *currentSubserver = GetWebServer()->GetSubServer(m_currentRequest->GetSubServerVirtualPath());

				if (currentSubserver != NULL && m_hasSentFinishedEvent == FALSE )
				{
					currentSubserver->SendProgressEvent(m_transfer_rate, m_sample.m_bytes - m_prev_sample.m_bytes, m_sample.m_bytes, outgoing, m_currentRequest, requestFinished);
					m_hasSentFinishedEvent = requestFinished;
				}
			}

			m_prev_sample.m_bytes = m_sample.m_bytes;
			m_prev_sample.m_time = m_sample.m_time;
		}
	}
	else
	{
		m_prev_sample.m_bytes =  m_sample.m_bytes;
		m_prev_sample.m_time =  m_sample.m_time;
	}

	return m_transfer_rate;
}

/* FIXME: move this to WebServerConnectionListener */
unsigned int
WebServerConnection::ComputeNextSendDelay(UINT bytesSent)
{
	OP_ASSERT(GetConnectionLister());
	if(!GetConnectionLister())
		return 0;

	unsigned int result = 0;
	static const int sampleSize = 50; // milliseconds
	static const unsigned int windowSize = 20;
	double now = g_op_time_info->GetRuntimeMS();
	if (GetConnectionLister()->m_prevClockCheck == 0) /* initialization */
	{
		GetConnectionLister()->m_prevClockCheck = now;
	}
	else
	{
		GetConnectionLister()->m_bytesSentThisSample += bytesSent;
		double actualSampleSize = now - GetConnectionLister()->m_prevClockCheck;
		if (actualSampleSize >= sampleSize)
		{
			if (GetConnectionLister()->m_currentWindowSize < windowSize)
			{
				GetConnectionLister()->m_currentWindowSize++;
			}

			double kBpsThisSample = (double)GetConnectionLister()->m_bytesSentThisSample / actualSampleSize;
			GetConnectionLister()->m_kBpsUp = (GetConnectionLister()->m_kBpsUp * (GetConnectionLister()->m_currentWindowSize - 1) + kBpsThisSample) /(double)windowSize;
			GetConnectionLister()->m_prevClockCheck = now;
			GetConnectionLister()->m_bytesSentThisSample = 0;

			if (!m_client_is_owner && OpRound(GetConnectionLister()->m_kBpsUp) > GetConnectionLister()->m_uploadRate)
			{
				GetConnectionLister()->m_nowThrottling = TRUE;
			}
			else
			{
				GetConnectionLister()->m_nowThrottling = FALSE;
			}
		}
		if (GetConnectionLister()->m_nowThrottling)
		{
			result = (unsigned int)(((double)WEB_PARM_DATABUFFER_SIZE / (double)GetConnectionLister()->m_uploadRate)*(double) GetConnectionLister()->NumberOfConnectionsWithRequests());
		}
	}
	return result;
}

void
WebServerConnection::OnSocketDataSent(OpSocket* aSocket, UINT bytesSent)
{
	// Notify the bytes sent
	OP_ASSERT(GetWebServer());
	GetWebServer()->AddBytesTransfered(bytesSent, TRUE);

	if (m_state == ST_CLOSING || m_state == ST_INDETERMINATE)
		return;
	/* SERVICE: Some data written to the socket were sent; consume them from
	   the output buffer and either send some more or finish up. */

	if (m_socketTimer)
	{
		m_socketTimer->Stop();
		m_socketTimer->Start(WEB_PARM_CONNECTION_TIMEOUT);
	}
	if ( m_waitFor100Continue == TRUE )
	{
		m_waitFor100Continue = FALSE;
		StartReceivingBodyData();
	}
	else
	{

		OP_ASSERT( (m_state == ST_SERVING_DATA || m_state == ST_WAITING_FOR_RESOURCE));
		m_resourceBufPtr += bytesSent;

		OP_ASSERT(m_resourceBufPtr <= m_resourceBufLim);
		/*This will only happen if the send(..) was blocked and we had to wait for the data to be sent to check to queue
		 */

		if (m_resourceBufPtr >= m_resourceBufLim)
		{
			m_resourceWrapper->Ptr()->m_allOutDataIsSent = TRUE;
			m_resourceBufPtr = m_resourceBufLim; // In case the assert above can ever fail
		}


		if (m_resourceWrapper->Ptr()->m_noMoreDataToSend &&  m_resourceWrapper->Ptr()->m_allOutDataIsSent )
		{
			CalculateTransferRate(bytesSent, 0, TRUE, TRUE);
			OP_STATUS status;
			if (OpStatus::IsError(status = FinishRequestAndLookForNextRequest()))
			{
				KillService(status);
				return;
			}

		}
		else
		{
			CalculateTransferRate(bytesSent, 1, TRUE);
		}
		/* first, compute delay required to meet upload rate cap */


		if (!m_client_is_owner && GetConnectionLister() && GetConnectionLister()->m_uploadRate > 0) {
			m_delayMS = ComputeNextSendDelay(bytesSent);
		}
		else
		{
			m_delayMS = 0;
		}

		if (m_resourceBufPtr == m_resourceBufLim)
		{
			if (m_delayMS > 0)
				g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_DATAREADY, Id(), bytesSent, m_delayMS);
			else
				g_main_message_handler->PostMessage(MSG_WEBSERVER_DATAREADY, Id(), bytesSent);
		}
	}
	/* end of delay computation */
}

void
WebServerConnection::OnSocketClosed(OpSocket* aSocket)
{
	KillService();
}


void
WebServerConnection::OnSocketReceiveError(OpSocket* aSocket, OpSocket::Error aSocketErr)
{
	if (m_state == ST_CLOSING)
		return;

	if (aSocketErr != OpSocket::SOCKET_BLOCKING)
	{
		KillService();
	}
}

void
WebServerConnection::OnSocketSendError(OpSocket* aSocket, OpSocket::Error error)
{
	if (m_state == ST_CLOSING)
		return;

	OP_ASSERT( m_state == ST_SERVING_DATA && aSocket == GetSocketPointer() );

	if (error !=  OpSocket::SOCKET_BLOCKING)
	 {
		 KillService(OpStatus::ERR);
		return;
	 }
}

void
WebServerConnection::OnSocketCloseError(OpSocket* aSocket, OpSocket::Error aSocketErr)
{


}

void
WebServerConnection::OnSocketConnectError(OpSocket* aSocket, OpSocket::Error aSocketErr)
{


}

void
WebServerConnection::OnSocketDataSendProgressUpdate(OpSocket* aSocket, UINT bytesSent)
{

}

void
WebServerConnection::OnSocketConnected(OpSocket* aSocket)
{
	if (m_state == ST_CLOSING)
		return;
}

void
WebServerConnection::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT( par1 == Id( ));

	if (m_dying == TRUE || m_state == ST_CLOSING)
	{
		KillService();
		return;
	}

	OP_STATUS status;
	switch (msg)
	{
	case MSG_WEBSERVER_DATAREADY:
		g_main_message_handler->RemoveDelayedMessage(MSG_WEBSERVER_FORCE_READING_DATA, Id(), 0);
		if (OpStatus::IsError(status = ServeData((UINT) par2)))
		{
			KillService(status);
		}
		break;

	case MSG_WEBSERVER_FORCE_READING_DATA:
		if (OpStatus::IsError(status = ReadIncomingData()))
		{
			KillService(status);
		}
		break;

	default:
		OP_ASSERT(!"We do not listen to this");
		break;
	}
}


OP_STATUS
WebServerConnection::ReadIncomingData()
{
	BOOL readMore = FALSE;

	unsigned int bytes_received;
	unsigned int maximumAmountOfData;


	if (m_requestQueue.Cardinal() >= WEB_PARM_REQUEST_QUEUE_LENGTH_LIMIT)
	{
		/* We parse this request, but wait until all requests
		 * are handled before reading more from socket.
		 *
		 * IMPORTANT! This is NOT done for POST and PUT request, since POST and put is not piped anyway, and connection
		 * will be closed after the POST or PUT request has been handled.
		 *
		 * */
		m_requestQueueLengthLimitReached = TRUE;
		return OpStatus::OK;
	}

	if (m_socketTimer)
	{
		m_socketTimer->Stop();
		m_socketTimer->Start(WEB_PARM_CONNECTION_TIMEOUT);
	}

	if (m_buf == NULL)
	{
		m_buf = OP_NEWA(char, WEB_PARM_DATABUFFER_SIZE /*!*/ + 1);	/*We add one byte to assure that the buffer ends with 0  !IMPORTANT!*/
		if (m_buf == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		m_buf[WEB_PARM_DATABUFFER_SIZE] = '\0';			//assures that the buffer ends with 0	 !IMPORTANT!
		m_buflim = WEB_PARM_DATABUFFER_SIZE;
		m_bytesInBuffer = 0;
	}

	OP_ASSERT(m_bytesInBuffer <= m_buflim);


	if (m_incommingDataType == WEB_DATA_TYPE_REQUEST && m_bytesInBuffer >= WEB_PARM_REQUEST_LENGTH_LIMIT)
	{
		/* If number of bytes in the buffer is bigger than WEB_PARM_REQUEST_LENGTH_LIMIT
		 * And still no full request (full request means that the data ends with CRLF CRLF)
		 */
		 return OpStatus::ERR;
	}



	if (m_bytesInBuffer == m_buflim && m_incommingDataType == WEB_DATA_TYPE_REQUEST)
	{
		/* This will only happen when reading requests:
		 * If the buffer is full, and a full request line is not received, expand the buffer
		 */

		char *newbuf = OP_NEWA(char, m_buflim * 2 /*!*/ + 1);/*We add one byte to assure that the buffer ends with 0  !IMPORTANT!*/
		if (newbuf == NULL)
		{
			return OpStatus::ERR_NO_MEMORY;
		}

		op_memcpy( newbuf, m_buf, m_buflim );
		OP_DELETEA(m_buf);
		m_buf = newbuf;
		m_buflim *= 2;
		m_buf[m_buflim] = '\0';
	}

	OP_ASSERT( m_buf != NULL);
	/*Read incomming data */
	maximumAmountOfData = m_buflim - m_bytesInBuffer;

	RETURN_IF_ERROR(GetSocketPointer()->Recv(m_buf + m_bytesInBuffer, maximumAmountOfData, &bytes_received));

	// Notify the bytes received
	GetWebServer()->AddBytesTransfered(bytes_received, FALSE);

	if (m_dying == TRUE || m_buf == NULL)
	{
		return OpStatus::ERR;
	}

	OP_ASSERT(m_buflim >= m_bytesInBuffer);

	// FIXME: this logic is wrong
	if (bytes_received > maximumAmountOfData)  //If bytes_received == -1 (error)
	{
		bytes_received = 0;
		readMore = FALSE;

		if (m_incommingDataType == WEB_DATA_TYPE_BODY && m_acceptIncommingData == TRUE)
		{
			RETURN_IF_ERROR(m_resourceWrapper->Ptr()->HandleIncommingData(m_buf, 0, TRUE)); //We tell the resource that this was the last data (no data)
			OP_ASSERT( m_buf != NULL);
		}
	}
	else
	{
		readMore = TRUE;

	}

	m_bytesInBuffer += bytes_received;

	*(m_buf + m_bytesInBuffer) = '\0';  /* Make sure the buffer ends with 0 */

	OP_ASSERT(m_bytesInBuffer <= m_buflim);

	/*We handle the data here according to the data type */
	if (m_incommingDataType == WEB_DATA_TYPE_BODY)
	{
		CalculateTransferRate(bytes_received, 1, FALSE);
		m_lengthOfBodyReceived += bytes_received;
	}

	BOOL redirected=FALSE;

	if (m_incommingDataType == WEB_DATA_TYPE_REQUEST && m_bytesInBuffer > 0) /*if the incomming data is (part of) a request and not body:	*/
	{
		/*If the first request in the data is a PUT or POST m_incommingDataType will change to WEB_DATA_TYPE_BODY*/
		RETURN_IF_ERROR(handleIncomingRequstData(redirected));
		m_lengthOfBodyReceived = m_bytesInBuffer;

		if(redirected)
			return OpStatus::OK;
	}

	if (m_incommingDataType == WEB_DATA_TYPE_BODY)// && m_bytesInBuffer > 0)
	{
		RETURN_IF_ERROR(handleIncomingBodyData(redirected));
	}

	// FIXME: not called bacause readMore is never FALSE...
	/*Reset the size of buffer */
	if (m_bytesInBuffer == 0 && readMore == FALSE && m_buflim > (WEB_PARM_DATABUFFER_SIZE + 1))
	{
		OP_DELETEA(m_buf);
		m_buf = NULL;
	}

	return  OpStatus::OK;
}

OP_STATUS
WebServerConnection::handleIncomingRequstData(BOOL &redirected)
{
	char *nextIncommingData;
	OP_ASSERT(m_incommingDataType == WEB_DATA_TYPE_REQUEST);

	redirected=FALSE;

	/*Check if we have got at least one full request,that is a request line that ends with CRLF CRLF (or "\r\n\r\n")
		 * If so, put requests in queue
		 */
	if (op_strstr(m_buf, CRLF CRLF))
	{
		nextIncommingData = m_buf;

		RETURN_IF_ERROR(ParseAndPutRequestsInQueue(nextIncommingData));

		// Check if it is an admin request from another User Agent, and redirect it
		if(!m_client_is_owner)
		{
			HeaderList *headers=NULL;
			WebserverRequest *req = static_cast<WebserverRequest*>(m_requestQueue.First());

			if(req && (headers=req->GetClientHeaderList()) != NULL)
			{
				HeaderEntry *header=headers->GetHeader("Host");
				const char *host=(header)?header->Value():NULL;
				WebServer *ws=GetWebServer();

				OP_ASSERT(ws);
				if(host && ws && ws->MatchServerAdmin(host) && op_stricmp(host, "admin.") == 0)
				{
					// Create the "Moved permanently" request
					redirected=TRUE;

					KillService(OpStatus::OK);

					OpString8 moving;

					if (op_stricmp(host, "admin.") == 0)
						host += 6;

					RETURN_IF_ERROR(moving.AppendFormat("HTTP/1.1 301 Moved Permanently\r\n"));

					if(req->GetOriginalRequest() && req->GetOriginalRequest()[0]=='/') // Just to be on the safe side
						RETURN_IF_ERROR(moving.AppendFormat("Location: http://%s%s\r\n\r\n", host, req->GetOriginalRequest())); // remove Admin
					else
						RETURN_IF_ERROR(moving.AppendFormat("Location: http://%s/%s\r\n\r\n", host, req->GetOriginalRequest())); // remove Admin and add "/"

					RETURN_IF_ERROR(GetSocketPointer()->Send(moving.CStr(), moving.Length()));

					return OpStatus::OK;
				}
			}
		}


		OP_ASSERT(nextIncommingData != m_buf); /*nextIncommingData must now point to the end+1 of the last requets in the pipe*/
			/* If this connection is not handling any request and there are requests in the queue, start handling the queue*/

		RETURN_IF_ERROR(CheckRequestQueue());  /*If the request in the queue is a PUT or POST m_incommingDataType will change to WEB_DATA_TYPE_BODY*/

		/*If there was one or more full request(s) in the line,
		 *the char *incommingRequest will now point to the next uncomplete request or body in line
		 */
		m_bytesInBuffer = RemoveHandledData(m_buf, nextIncommingData, m_bytesInBuffer);
	}
	else
	{
		m_socketTimer->Start(10000);
	}
	return  OpStatus::OK;
}

OP_STATUS
WebServerConnection::handleIncomingBodyData(BOOL redirected)
{
	OP_ASSERT(m_incommingDataType == WEB_DATA_TYPE_BODY);
	if (m_incommingDataType != WEB_DATA_TYPE_BODY)
		return  OpStatus::OK;
	OP_ASSERT(m_resourceWrapper != NULL);

	if (m_acceptIncommingData == TRUE)
	{
		if (m_lengthOfBodyReceived > m_incommingBodyLength )
		{
			RETURN_IF_ERROR(m_resourceWrapper->Ptr()->HandleIncommingData(m_buf, m_bytesInBuffer - (m_lengthOfBodyReceived - m_incommingBodyLength), TRUE));
		}
		else if (m_lengthOfBodyReceived == m_incommingBodyLength)
		  {
			RETURN_IF_ERROR(m_resourceWrapper->Ptr()->HandleIncommingData(m_buf, m_bytesInBuffer,TRUE));
		  }
		else
		{
			RETURN_IF_ERROR(m_resourceWrapper->Ptr()->HandleIncommingData(m_buf, m_bytesInBuffer&(~1)));
		}
	}

	/*When all data is handled*/
	if (m_lengthOfBodyReceived >= m_incommingBodyLength)
	{
		m_incommingDataType = WEB_DATA_TYPE_REQUEST;

		/*If we get more data than  m_incommingBodyLength, the data after m_incommingBodyLength should be a new request*/
		if (m_lengthOfBodyReceived > m_incommingBodyLength)
		{
			UINT firstByteAfterBody = m_bytesInBuffer - (m_lengthOfBodyReceived - m_incommingBodyLength);
			m_bytesInBuffer = RemoveHandledData(m_buf, m_buf + firstByteAfterBody, m_bytesInBuffer);
		}
		else
		{
			/*All data in the buffer were handled*/
			m_bytesInBuffer = 0;
		}
	}
	else
	{
		/*If we expect more data for this post or put, all data should have been handled there*/
		m_bytesInBuffer = RemoveHandledData(m_buf,m_buf+(int)( m_bytesInBuffer&(~1)),m_bytesInBuffer);
	}

	return  OpStatus::OK;
}



OP_STATUS
WebServerConnection::ParseAndPutRequestsInQueue(char *&incommingRequest)
{
	OP_ASSERT(m_incommingDataType == WEB_DATA_TYPE_REQUEST);
	/*We save the current webserver m_state for later*/
	ConnectionState state_temp = m_state;

	char *requestEnd;
	int numberOfrequests = 0;
	WebServerMethod method;

	/*Here we check the incoming buffer for requests, and put them in a request queue*/
	m_state = ST_PARSING_REQUEST;
	requestEnd = op_strstr(incommingRequest, CRLF CRLF);
	BOOL continuePiping = TRUE;

	while (incommingRequest && op_strstr(incommingRequest, CRLF CRLF) && continuePiping == TRUE)
	{
		int i = 0;
		while ( op_isspace((unsigned char) incommingRequest[0]) && incommingRequest < requestEnd)
		{
			i++;
			incommingRequest++;
		}

		UINT requestLength = requestEnd - incommingRequest;
		numberOfrequests++;

		WebserverRequest *requestObject = 0;


		if (requestLength > 0) /* If the request is only CRLF's, then ignore*/
		{
			RETURN_IF_ERROR(ParseRequest(requestObject, incommingRequest, requestLength));

			if (requestObject)
			{
				requestObject->Into(&m_requestQueue);

				/*We stop piping if the method is not get or head (options?)*/
				method = requestObject->GetHttpMethod();
				if  (method  == WEB_METH_POST || method  == WEB_METH_PUT )
				{
					continuePiping = FALSE;
				}
			}
		}

		/*We make incommingRequest point to the next request*/
		char *end = op_strstr(incommingRequest, CRLF CRLF);
		if (end)
		{
			requestEnd = end;
			incommingRequest = end + 4;

		}
	}

	//Set the server back to the m_state it was before parsing
	m_state = state_temp;

	return OpStatus::OK;
}


/*This method checks the queue. If there are any requests in the queue, we set up the service for the first in the queue*/
OP_STATUS
WebServerConnection::CheckRequestQueue()
{
	if (m_isHandlingRequest == TRUE)
	{
		return OpStatus::OK;
	}

	InitTransferRate();
	OP_ASSERT(m_resourceWrapper == NULL && m_incommingDataType == WEB_DATA_TYPE_REQUEST);


	if (m_requestQueue.Cardinal() > 0)
	{
		m_state = ST_SETTING_UP_SERVICE;
		m_lengthOfBodyReceived = 0;
		RETURN_IF_ERROR(SetupService());
		m_isHandlingRequest = TRUE;
	}
	else if (m_requestQueueLengthLimitReached == TRUE)
	{
		g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_FORCE_READING_DATA, Id(), 0, 5);
		m_requestQueueLengthLimitReached = FALSE;
	}

	return OpStatus::OK;
}

OP_STATUS
WebServerConnection::ParseRequest(WebserverRequest *&requestObject, char *incommingRequest, unsigned int requestLength)
{
	OP_ASSERT( m_state == ST_PARSING_REQUEST );

	requestObject = NULL;
	if ( incommingRequest == 0 || requestLength == 0 || incommingRequest[0] == '\0' )
	{
		return OpStatus::ERR;
	}

	OP_STATUS status;

	WebserverRequest *theRequestObject = NULL;

	HeaderList *clientHeaderList = NULL;

	int unsigned j = 0, req_start = 0, req_end;
	WebServerMethod method;

	m_protocolVersion.protocol_major = 0;
	m_protocolVersion.protocol_minor = 0;

	if (MatchMethod(&method, &j, incommingRequest) &&
		(MatchWS(&j,incommingRequest, requestLength), TRUE) &&
		MatchNonWS(&req_start, &j, incommingRequest, requestLength))
	{
		req_end = j;

		MatchWS(&j,incommingRequest, requestLength);
		if (!(Match("HTTP", &j, incommingRequest, requestLength) &&
			  (MatchWS(&j, incommingRequest, requestLength), TRUE) &&
			  Match("/", &j, incommingRequest, requestLength) &&
			  (MatchWS(&j, incommingRequest, requestLength), TRUE) &&
			  MatchDecimal(&m_protocolVersion.protocol_major, &j, incommingRequest, requestLength) &&
			  (MatchWS(&j,incommingRequest, requestLength), TRUE) &&
			  Match(".", &j,incommingRequest, requestLength) &&
			  (MatchWS(&j, incommingRequest, requestLength), TRUE) &&
			  MatchDecimal(&m_protocolVersion.protocol_minor, &j, incommingRequest, requestLength)))
		{
			m_protocolVersion.protocol_major = 0;
			m_protocolVersion.protocol_minor = 9;
		}

		if (m_protocolVersion.protocol_major == 0 && m_protocolVersion.protocol_minor == 0)
			return OpStatus::ERR;

		char *headerStart = op_strstr(incommingRequest, CRLF);// + 2; // Goes to the next line after CRLF.

		if (headerStart == NULL || headerStart[2] == '\x0D' || headerStart[2] == '\x0A' || (unsigned int)(headerStart - incommingRequest) > requestLength )
		{
			return OpStatus::ERR;
		}

		status = OpStatus::ERR_NO_MEMORY;

		char *headerEnd = op_strstr(headerStart, CRLF CRLF);

		if (!headerEnd)
		{
			return OpStatus::ERR_PARSING_FAILED;
		}
		char temp = *headerEnd;
		*headerEnd = '\0';

		if ((clientHeaderList = OP_NEW(HeaderList, ())) == NULL || OpStatus::IsError(status = clientHeaderList->SetValue(headerStart + 2 )))
		{
			OP_DELETE(clientHeaderList);
			return status;
		}

		*headerEnd = temp;

		char *request = incommingRequest + req_start;

		int req_length = req_end - req_start;

		status = OpStatus::ERR_NO_MEMORY;
		if ((theRequestObject = OP_NEW(WebserverRequest, ())) == NULL || OpStatus::IsError(status = theRequestObject->Construct(request, req_length, clientHeaderList, method, IsProxied())))
		{
			OP_DELETE(theRequestObject);
			if (theRequestObject == NULL)
			{
				OP_DELETE(clientHeaderList);
			}
			return status;
		}

		WebSubServer *currentSubserver = GetWebServer()->GetSubServer(theRequestObject->GetSubServerVirtualPath());
		if (currentSubserver)
		{
			currentSubserver->SendNewRequestEvent(theRequestObject);
		}
	}

	if (theRequestObject == NULL)
	{
		KillService(OpStatus::ERR);
		return OpStatus::ERR;
	}

	requestObject = theRequestObject;

	return OpStatus::OK;
}

void
WebServerConnection::AcceptReceivingBody(BOOL accept)
{
	m_acceptIncommingData = accept;
	HeaderEntry *header = m_currentRequest->GetClientHeaderList()->GetHeader("Expect");
	m_waitFor100Continue = FALSE;
	if (header)
	{
		if (op_stristr(header->Value(), "100-continue"))
		{
			/*If we get a 100-continue we ignore whats left in the buffer, and wait for more data*/
			Send100Continue();
		}
	}

	if (m_waitFor100Continue == FALSE)
	{
		StartReceivingBodyData();
	}
}

void WebServerConnection::StartReceivingBodyData()
{
	if (m_resourceWrapper)
		m_resourceWrapper->Ptr()->m_noMoreDataToSend = FALSE;
//	m_state = ST_READING_DATA;
	m_lengthOfBodyReceived = 0;
}



OP_STATUS
WebServerConnection::Send100Continue()
{
	int length = op_strlen(m_100ContinueString);
	m_waitFor100Continue = TRUE;

	/* FIXME : this should be sent using the buffer system send ServeData() instead
	*/

	RETURN_IF_MEMORY_ERROR(GetSocketPointer()->Send(m_100ContinueString, length));
	return OpStatus::OK;
}

OP_BOOLEAN
WebServerConnection::SetupError(WebserverRequest *requestObject)
{
	OP_ASSERT(!"Not implemented yet");
	/* Here we set up more advanced error messages, that is we provide a body in addition to
	 * the response code to the client*/
	return OpStatus::OK;
}

OP_BOOLEAN
WebServerConnection::SetupService()
{
	/*If resource is not NULL, we have forgot to delete the resource for the previous connection */
	OP_ASSERT(m_resourceWrapper == NULL);
	OP_ASSERT(m_requestQueue.Cardinal() > 0);
	OP_ASSERT(GetConnectionLister());

	if(!GetConnectionLister())
		return OpStatus::ERR;  // FIXME ??? OP_BOOLEAN?

	WebServerFailure result = WEB_FAIL_NO_FAILURE;
	WebResource *webresource = NULL;

	m_currentRequest = static_cast<WebserverRequest*>(m_requestQueue.First());

	m_acceptIncommingData = FALSE;
	if (m_currentRequest->GetHttpMethod() == WEB_METH_POST)
	{
		m_incommingDataType = WEB_DATA_TYPE_BODY;
		HeaderEntry *header = m_currentRequest->GetClientHeaderList()->GetHeader("content-length");
		if (header)
		{
			m_incommingBodyLength = op_atoi(header->Value());
		}
		else
		{
			//FIXME: ERROR	CODE if not chunked. We must implement chunked encoding for incoming data
			m_incommingBodyLength = 0;
		}
	}

	/* First check the root service */
	WebSubServer *rootSubServer = GetConnectionLister()->GetWebRootSubServer();
#ifdef SELFTEST
	OP_ASSERT(g_selftest.running || (rootSubServer && "No root service has been set!"));
#else
	OP_ASSERT((rootSubServer && "No root service has been set!"));
#endif

	OP_BOOLEAN foundResource = OpBoolean::IS_FALSE;

	if (uni_stri_eq(m_currentRequest->GetRequest(), UNI_L("/_check_direct_access")))
	{
		RETURN_IF_ERROR(WebResource_Null::Make(webresource, this, m_currentRequest, 200));
		RETURN_IF_ERROR(webresource->AddResponseHeader(UNI_L("Connection"), UNI_L("Close")));
		foundResource = OpBoolean::IS_TRUE;
	}

	m_currentSubserver = GetWebServer()->GetSubServer(m_currentRequest->GetSubServerVirtualPath());

	if (m_currentSubserver && m_currentRequest->GetSubServerRequest()[0] != '/')
	{
		result = WEB_FAIL_REDIRECT_TO_DIRECTORY;
	}
	else
	{
		/* Check root service */
		if (foundResource == OpBoolean::IS_FALSE && rootSubServer && uni_str_eq(m_currentRequest->GetSubServerVirtualPath(), "_root"))
		{
			RETURN_IF_ERROR(foundResource = FindResourceInSubServer(webresource, rootSubServer, m_currentRequest->GetSubServerRequest(), m_currentRequest, &result));
			if (foundResource == OpBoolean::IS_FALSE)
			{
				OP_ASSERT(webresource == NULL);
				OP_DELETE(webresource);
				webresource = NULL;

			}
			else if (foundResource == OpBoolean::IS_TRUE)
			{
				m_currentSubserver = rootSubServer;
			}
		}

		/* Check all the services services */
		//if (foundResource == OpBoolean::IS_FALSE && m_currentRequest->GetSubServerVirtualPath()[0] != '_')
		if (foundResource == OpBoolean::IS_FALSE && m_currentSubserver)
		{
			RETURN_IF_ERROR(foundResource = FindResourceInSubServer(webresource, m_currentSubserver, m_currentRequest->GetSubServerRequest(), m_currentRequest, &result));
			if (foundResource == OpBoolean::IS_FALSE)
			{
				m_currentSubserver = NULL;
				OP_ASSERT(webresource == NULL);
				OP_DELETE(webresource);
				webresource = NULL;
			}
		}
	}

	if(foundResource == OpBoolean::IS_TRUE)
	{
		OP_ASSERT(webresource);
		//OP_ASSERT(m_currentSubserver);

		if(webresource && m_currentSubserver)
		{
			OpStatus::Ignore(GetWebServer()->LogResourceAccess(this, m_currentRequest, m_currentSubserver, webresource));
		}
	}

	if (webresource == NULL && result == WEB_FAIL_NO_FAILURE)
	{
		result = WEB_FAIL_FILE_NOT_AVAILABLE;
	}
	/* No matching resource or error */

	if (result != WEB_FAIL_NO_FAILURE && result != WEB_FAIL_NOT_AUTHENTICATED)
	{
		OP_ASSERT( m_state == ST_SETTING_UP_SERVICE );

		m_state = ST_SERVING_DATA;

		OP_ASSERT(webresource == NULL);

		OP_DELETE(webresource); /* Just in case */

		m_resourceBufLim = 0;
		m_resourceBufPtr = 0;

		switch (result)
		{
			case WEB_FAIL_REDIRECT_TO_DIRECTORY :
				RETURN_IF_ERROR(WebResource_Null::Make(webresource, this, m_currentRequest, 301));
				break;
			case WEB_FAIL_REQUEST_SYNTAX_ERROR :
				RETURN_IF_ERROR(WebResource_Null::Make(webresource, this, m_currentRequest, 400));
				break;
			case WEB_FAIL_FILE_NOT_AVAILABLE :
				RETURN_IF_ERROR(WebResource_Null::Make(webresource, this, m_currentRequest, 404));
				break;
			case WEB_FAIL_METHOD_NOT_SUPPORTED:
				RETURN_IF_ERROR(WebResource_Null::Make(webresource, this, m_currentRequest, 405));
				break;
			case WEB_FAIL_OOM :
				RETURN_IF_ERROR(WebResource_Null::Make(webresource, this, m_currentRequest, 500));
				break;
			default:
				RETURN_IF_ERROR(WebResource_Null::Make(webresource, this, m_currentRequest, 503));
				break;
		}

		if ( webresource == NULL )
		{
			OP_ASSERT(!"WebResource_Null::Make should have caught this ");
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	OP_ASSERT(webresource != NULL);
	if (webresource)
	{
		OP_ASSERT(m_state != ST_SETTING_UP_SERVICE);
		OP_ASSERT(m_resourceWrapper == NULL);
		m_resourceWrapper = OP_NEW(WebResourceWrapper, (webresource));
		if (m_resourceWrapper == NULL)
		{
			OP_DELETE(webresource);
			webresource = NULL;
			return OpStatus::ERR_NO_MEMORY;
		}
	}

	if (webresource)
		return OpStatus::OK; /* resource created */
	else
		return OpStatus::ERR; /* resource not created */
}

OP_BOOLEAN WebServerConnection::FindResourceInSubServer(WebResource *&webresource, WebSubServer *subServer, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebServerFailure *result)
{
	webresource = NULL;

	const OpAutoVector<WebserverResourceDescriptor_AccessControl> *subServerAccessControlResources = subServer->GetSubserverAccessControlResources();
	int numberOfSubServerAccessControlResources = subServerAccessControlResources->GetCount();
	WebserverResourceDescriptor_AccessControl *resourceAuthDescriptor;

#ifdef DEBUG_ENABLE_OPASSERT
	BOOL needs_auth = FALSE;
#endif

	// Root Service
	if (requestObject->HasAuthenticated() == FALSE && (subServer->GetServiceType() != WebSubServer::SERVICE_TYPE_CUSTOM_ROOT_SERVICE || uni_strncmp(subServerRequestString.CStr(), "login?", 6) != 0)) /* the login page in the root service will not require authentication */
	{
		for (int j = 0; j < numberOfSubServerAccessControlResources; j++)
		{
			resourceAuthDescriptor = subServerAccessControlResources->Get(j);

			if (resourceAuthDescriptor->IsActive() == FALSE)
			{
				continue;
			}

#ifdef DEBUG_ENABLE_OPASSERT
			needs_auth = TRUE;
#endif

			/* Ok, this url has a authentication resource we need to check for authentication.
			 * WebResource_Auth::Make will return a webresource if the authentication was not successful. */
			OP_BOOLEAN authenticated;
			RETURN_IF_ERROR(authenticated = WebResource_Auth::Make(webresource, subServerRequestString, this, requestObject, resourceAuthDescriptor, result));


			if (authenticated == OpBoolean::IS_FALSE && webresource == NULL) /*This is extremely important to prevent FALSE successes*/
			{
				OP_ASSERT(!"Security error: authenticated is false but no auth resource was produced. Should not happen!");
				return OpBoolean::IS_FALSE;
			}

			OP_ASSERT( (*result == WEB_FAIL_NO_FAILURE && webresource == NULL) || (*result == WEB_FAIL_NOT_AUTHENTICATED && webresource != NULL));

			OP_ASSERT((authenticated == OpBoolean::IS_FALSE && webresource != NULL) || (authenticated == OpBoolean::IS_TRUE && webresource == NULL));

			if (authenticated == OpBoolean::IS_TRUE)
			{
				requestObject->SetAuthStatus(TRUE);
			}
			else
			{
				requestObject->SetAuthStatus(FALSE);
				if (webresource != NULL)
				{
					return OpBoolean::IS_TRUE;
				}
				else
				{
					return OpBoolean::IS_FALSE;
				}
			}
		}
	}

	OP_ASSERT(needs_auth == FALSE || (needs_auth == TRUE && requestObject->HasAuthenticated() == TRUE && "AUTHENTICATION ERROR: user has not been authenticated!"));

	const OpAutoVector<WebserverResourceDescriptor_Base> *subServerResources = subServer->GetSubserverResources();
	int numberOfSubServerResources = subServerResources->GetCount();

	// Ecmascript resources
	if (webresource == NULL && subServer->AllowEcmaScriptSupport() == TRUE)
	{
		unsigned long windowId = subServer->GetWindowId();
		if (windowId != NO_WINDOW_ID && (webresource = WebResource_Custom::Make(this, subServerRequestString, requestObject, windowId, result)))
		{
			return OpBoolean::IS_TRUE;
		}

		if (*result == WEB_FAIL_OOM)
		{
			OP_ASSERT(webresource == NULL);
			return OpStatus::ERR_NO_MEMORY;
		}
		else if (*result == WEB_FAIL_GENERIC)
		{
			OP_ASSERT(webresource == NULL);
			return OpStatus::ERR;
		}
	}

	WebserverResourceDescriptor_Base *resourcesDescriptor = NULL;

	// Custom Resource (currently not supported
	/*for (int j = 0; j < numberOfSubServerResources; j++)
	{
		resourcesDescriptor = subServerResources->Get(j);
		if (resourcesDescriptor->GetResourceType() == WEB_RESOURCE_TYPE_CUSTOM )
		{
			//FIXME: Here we put in other custom resources

		}
	}*/

	// Other resources
	if (webresource == NULL && (*result == WEB_FAIL_NO_FAILURE || *result == WEB_FAIL_METHOD_NOT_SUPPORTED || *result == WEB_FAIL_FILE_NOT_AVAILABLE ))
	{
		for (int j = 0; j < numberOfSubServerResources; j++)
		{
			resourcesDescriptor = subServerResources->Get(j);

			// Static files
			if (resourcesDescriptor->GetResourceType() == WEB_RESOURCE_TYPE_STATIC && resourcesDescriptor->IsActive() == TRUE )
			{
				RETURN_IF_ERROR(WebResource_File::Make(webresource, this, subServerRequestString, requestObject, static_cast<WebserverResourceDescriptor_Static*>(resourcesDescriptor), result));

				if (*result == WEB_FAIL_NO_FAILURE)
				{
					OP_ASSERT(webresource != NULL);
					return OpBoolean::IS_TRUE;
				}
				else if (*result ==  WEB_FAIL_REDIRECT_TO_DIRECTORY)
				{
					OP_ASSERT(webresource == NULL);
					return OpBoolean::IS_TRUE;
				}
				else if (*result == WEB_FAIL_METHOD_NOT_SUPPORTED)
				{
					OP_ASSERT(webresource == NULL);
					return OpBoolean::IS_FALSE;
				}
			}
		#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
			else if(resourcesDescriptor->GetResourceType() == WEB_RESOURCE_TYPE_UPNP && resourcesDescriptor->IsActive() == TRUE )
			{
				OpString challenge16;
				OpString8 challenge8;
				unsigned int index=0;

				requestObject->GetItemInQuery(UNI_L("challenge"), challenge16, index, FALSE);

				if(challenge16.Length()>0)
					challenge8.Set(challenge16.CStr());

				RETURN_IF_ERROR(WebResource_UPnP_Service_Discovery::Make(webresource, this, subServerRequestString, requestObject, resourcesDescriptor, result, challenge8));

				if (*result == WEB_FAIL_NO_FAILURE)
				{
					OP_ASSERT(webresource != NULL);

					return OpBoolean::IS_TRUE;
				}

				OP_ASSERT(webresource == NULL);
				return OpBoolean::IS_FALSE;
			}
		#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
			else if(resourcesDescriptor->GetResourceType() == WEB_RESOURCE_TYPE_ROBOTS && resourcesDescriptor->IsActive() == TRUE )
			{
				RETURN_IF_ERROR(WebResource_Robots::Make(webresource, this, subServerRequestString, requestObject, resourcesDescriptor, result));

				if (*result == WEB_FAIL_NO_FAILURE)
				{
					OP_ASSERT(webresource != NULL);

					return OpBoolean::IS_TRUE;
				}

				OP_ASSERT(webresource == NULL);
				return OpBoolean::IS_FALSE;
			}
			else if(resourcesDescriptor->GetResourceType() == WEB_RESOURCE_TYPE_ASD && resourcesDescriptor->IsActive() == TRUE )
			{
				RETURN_IF_ERROR(WebResource_ASD::Make(webresource, this, subServerRequestString, requestObject, resourcesDescriptor, result));

				if (*result == WEB_FAIL_NO_FAILURE)
				{
					OP_ASSERT(webresource != NULL);

					return OpBoolean::IS_TRUE;
				}

				OP_ASSERT(webresource == NULL);
				return OpBoolean::IS_FALSE;
			}
		}
	}

#ifdef NATIVE_ROOT_SERVICE_SUPPORT
	if ( webresource == NULL && (*result == WEB_FAIL_NO_FAILURE || *result == WEB_FAIL_METHOD_NOT_SUPPORTED || *result == WEB_FAIL_FILE_NOT_AVAILABLE )  )
	{
		/* FIXME: Put this functionality into request object {*/
		const uni_char *requestStr = requestObject->GetRequest();
		while (*requestStr == '/' )
			requestStr++;

		OpString tempRequestStr;

		RETURN_IF_ERROR(tempRequestStr.Set(requestStr));
		int id1 = KNotFound;
		int id2 = KNotFound;

		if ((id1 = tempRequestStr.FindFirstOf('?')) != KNotFound)
		{
			tempRequestStr[id1] = '\0';
		}

		if ((id2 = tempRequestStr.FindFirstOf('#')) != KNotFound)
		{
			tempRequestStr[id2] = '\0';
		}
		/*} end  of FIXME: Put this functionality into request object */


		if (uni_str_eq(tempRequestStr.CStr(), UNI_L("_root/")))
		{
			webresource = WebResource_Index::Make(this, requestObject);

			if (webresource == NULL)
			{
				*result = WEB_FAIL_OOM;
				OP_ASSERT(webresource == NULL);
				return OpStatus::ERR_NO_MEMORY;
			}
			else
			{
				m_state = ST_SERVING_DATA;
				*result = WEB_FAIL_NO_FAILURE;
				return OpBoolean::IS_TRUE;
			}
		}
		else if (uni_str_eq(tempRequestStr.CStr(), UNI_L("_root")))
		{
			*result = WEB_FAIL_REDIRECT_TO_DIRECTORY;
			OP_ASSERT(webresource == NULL);
			return OpBoolean::IS_TRUE;
		}
	}

#endif //NATIVE_ROOT_SERVICE_SUPPORT2


	OP_ASSERT(webresource == NULL);
	return OpBoolean::IS_FALSE;
}

BOOL WebServerConnection::IsResponseHeaderPresent(const char *name)
{
	if(!m_currentRequest)
		return FALSE;

	OpString8 header;

  	RETURN_VALUE_IF_ERROR(header.Set(name), FALSE);

  	Header_Item *item=m_currentRequest->m_responseHeaderList.FindHeader(header.CStr(), FALSE);

  	if(!item)
  		return FALSE;

  	return TRUE;
}

OP_STATUS WebServerConnection::GetResponseHeader(const char *name, OpString8 &value)
{
	if(!m_currentRequest)
		return FALSE;

	OpString8 header;

  	RETURN_IF_ERROR(header.Set(name));

  	Header_Item *item=m_currentRequest->m_responseHeaderList.FindHeader(header.CStr(), FALSE);

  	if(item)
  		RETURN_IF_ERROR(WebResource::GetHeaderValue(item, value));
  	else if(value.CStr())
  		value.CStr()[0]=0;

  	return OpStatus::OK;
}

ProtocolVersion WebServerConnection::getProtocolVersion()
{
	return m_protocolVersion;
}

WebResource *WebServerConnection::GetWebresource()
{
	return m_resourceWrapper->Ptr();
}

OP_STATUS WebServerConnection::GetSocketAddress(OpSocketAddress *socketAddress)
{
	if (socketAddress == NULL || m_dying || m_state == ST_CLOSING)
		return OpStatus::ERR;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	if (!m_rendezvous_client_address.IsEmpty())
	{
		RETURN_IF_ERROR(socketAddress->FromString(m_rendezvous_client_address.CStr()));
		socketAddress->SetPort(80);
		return OpStatus::OK;
	}
	else
#endif
	if (GetSocketPointer())
	{
		return GetSocketPointer()->GetSocketAddress(socketAddress);
	}
	else
	{
		socketAddress = NULL;
		return OpStatus::ERR;
	}
}

OP_STATUS WebServerConnection::StartServingData()
{
	if (m_socketTimer)
	{
		m_socketTimer->Stop();
		m_socketTimer->Start(WEB_PARM_CONNECTION_TIMEOUT);
	}
	if (m_resourceWrapper)
		m_resourceWrapper->Ptr()->m_noMoreDataToSend = FALSE;

	m_resourceBufLim = 0;
	m_resourceBufPtr = 0;

	m_state = ST_SERVING_DATA;

	m_moreData = OpBoolean::IS_TRUE;

	InitTransferRate();
	g_main_message_handler->PostDelayedMessage(MSG_WEBSERVER_DATAREADY, Id(), 0, 10);
	return OpStatus::OK;
}

void
WebServerConnection::PauseServingData()
{
	if (m_socketTimer)
	{
		m_socketTimer->Stop();
	}

	g_main_message_handler->RemoveDelayedMessage(MSG_WEBSERVER_DATAREADY, Id(), 0);

	InitTransferRate();
	m_state = ST_WAITING_FOR_RESOURCE;
}

OP_STATUS WebServerConnection::ContinueServingData()
{
	if (m_socketTimer)
	{
		m_socketTimer->Stop();
		m_socketTimer->Start(WEB_PARM_CONNECTION_TIMEOUT);
	}

	if (m_resourceWrapper)
		m_resourceWrapper->Ptr()->m_noMoreDataToSend = FALSE;

	m_state = ST_SERVING_DATA;
	InitTransferRate();

	g_main_message_handler->PostMessage(MSG_WEBSERVER_DATAREADY, Id(), 0); /* FIXME: Is return false an memory error?*/

	return OpStatus::OK;
}

OP_STATUS
WebServerConnection::ServeData(UINT bytes_sent)
{
	OP_ASSERT(GetSocketPointer() != NULL);
	OP_ASSERT(m_waitFor100Continue == FALSE);
	OP_ASSERT(m_resourceBufPtr <= m_resourceBufLim);

	if (m_resourceBuffer == NULL)
	{
		if ((m_resourceBuffer = OP_NEWA(char, WEB_PARM_DATABUFFER_SIZE)) == NULL)
			return OpStatus::ERR_NO_MEMORY;
	}

	if (GetSocketPointer() == NULL || m_waitFor100Continue)
		return OpStatus::ERR;

	if (m_state == ST_SERVING_DATA)
	{
		OP_ASSERT(m_resourceWrapper != NULL );

		if (m_resourceBufPtr >= m_resourceBufLim && m_moreData == OpBoolean::IS_TRUE)
		{
			m_resourceBufLim = 0;

			m_resourceBufPtr = 0;

			if (OpStatus::IsError(m_moreData = m_resourceWrapper->Ptr()->GetData(m_resourceBuffer, WEB_PARM_DATABUFFER_SIZE, m_resourceBufLim)))
			{
				m_resourceBufLim = 0;
				if (OpStatus::IsMemoryError(m_moreData))
				{
					return OpStatus::ERR_NO_MEMORY;
				}
				else
				{
					return OpStatus::ERR;
				}

			}
		}

		/* The GetSocketPointer()->Send() code will send a callback to OnSocketDataSent. In OnSocketDataSent method we
		 * post a message to the message loop. The message loop will then call handleCallback,
		 * which then again calls this method.
		 */

		UINT bytesToSend = (UINT)(m_resourceBufLim - m_resourceBufPtr);

		if ( m_resourceBufLim > m_resourceBufPtr )
		{
			OP_ASSERT(m_state == ST_SERVING_DATA );
			OP_STATUS status;
			RETURN_IF_MEMORY_ERROR(status = GetSocketPointer()->Send(m_resourceBuffer + m_resourceBufPtr, bytesToSend)); /* this might change m_resourceBufPtr */
		}
		else
		{
			g_main_message_handler->PostMessage(MSG_WEBSERVER_DATAREADY, Id(), bytes_sent);
		}

		if (m_resourceBufPtr >= m_resourceBufLim && m_moreData == OpBoolean::IS_FALSE)
		{
			m_resourceWrapper->Ptr()->m_noMoreDataToSend = TRUE;

			if (m_resourceWrapper->Ptr()->m_noMoreDataToSend &&  m_resourceWrapper->Ptr()->m_allOutDataIsSent )
			{
				CalculateTransferRate(bytesToSend, 0, TRUE, TRUE);

				/*Check the queue for more requests*/
				RETURN_IF_ERROR(FinishRequestAndLookForNextRequest());
			}
			else  if (!m_resourceWrapper->Ptr()->m_noMoreDataToSend)
			{
				OP_ASSERT(!"Should not be here");
			}
		}

	}

	return OpStatus::OK;
}

OP_STATUS
WebServerConnection::FinishRequestAndLookForNextRequest()
{
	OP_ASSERT(m_resourceWrapper != NULL);
	OP_ASSERT(m_waitFor100Continue == FALSE);

	m_currentSubserver = NULL;

	WebserverRequest *currentRequest = m_resourceWrapper ? m_resourceWrapper->Ptr()->GetRequestObject() : NULL;

	OP_ASSERT(currentRequest != NULL);

	if (!currentRequest)
	{
		return OpStatus::ERR;
	}

	BOOL closeConnection = FALSE;
	if (m_currentRequest->GetForwardRequestState() == FALSE) /* FIXME : this logic should be moved in to webresource_base */
	{
		HeaderList *headerList = currentRequest->GetClientHeaderList();

		if (m_resourceWrapper->Ptr()->m_allowPersistentConnection == FALSE)
		{
			closeConnection = TRUE;
		}
		else if (headerList)
		{
			HeaderEntry *header = NULL;
			const char* headerValue = NULL;
			if (
					(header = headerList->GetHeader("Connection")) != NULL &&
					(headerValue = header->Value()) != NULL &&
					(op_stristr(headerValue,"close")) != NULL
				)
			{
				closeConnection = TRUE;
			}
		}
	}

	if (closeConnection == TRUE)
	{
		KillService();
		return OpStatus::OK;
	}
	else
	{
		m_state = ST_INDETERMINATE;
		OP_STATUS err = m_resourceWrapper->Ptr()->OnConnectionClosedBase();
		OP_DELETE(m_resourceWrapper);
		m_resourceWrapper = NULL;
		m_currentRequest = NULL;
		if (OpStatus::IsError(err))
		{
			return err;
		}

		m_isHandlingRequest = FALSE;
		m_incommingDataType = WEB_DATA_TYPE_REQUEST;

		if  (m_bytesInBuffer > 0)
		{
			BOOL redirected=FALSE;

			/*If the first request in the data is a PUT or POST m_incommingDataType will change to WEB_DATA_TYPE_BODY*/
			RETURN_IF_ERROR(handleIncomingRequstData(redirected));
			m_lengthOfBodyReceived = m_bytesInBuffer;

			if(redirected)
			{
				OP_ASSERT(FALSE && "Does it make sense here? Bug in the widget code?");

				return OpStatus::OK;
			}

			if (m_incommingDataType == WEB_DATA_TYPE_BODY)
			{
				RETURN_IF_ERROR(handleIncomingBodyData(redirected));
			}
		}
		else
		{
			return CheckRequestQueue();
		}


	}
	return OpStatus::OK;
}


void
WebServerConnection::KillService(OP_STATUS status)
{

	if (OpStatus::IsMemoryError(status))
	{
		g_webserverPrivateGlobalData->SignalOOM();
	}

	if (m_dying) {
		return;
	}

	CalculateTransferRate(0, 0, TRUE, TRUE);

	m_state = ST_CLOSING;

	OP_DELETEA(m_buf);
	m_buf = NULL;

	if (m_incommingDataType == WEB_DATA_TYPE_BODY && m_acceptIncommingData == TRUE && m_buf)
	{
		m_resourceWrapper->Ptr()->HandleIncommingData(m_buf, 0, TRUE);
	}

	if (m_resourceWrapper && OpStatus::IsMemoryError(m_resourceWrapper->Ptr()->OnConnectionClosedBase()))
	{
		g_webserverPrivateGlobalData->SignalOOM();
	}

	OP_DELETE(m_resourceWrapper);
	m_resourceWrapper = NULL;
	m_currentRequest = NULL;


	/*This sends a message to the connectionListener that it should delete this connection object*/
	if(GetConnectionLister())
		g_main_message_handler->PostMessage(MSG_WEBSERVER_DELETE_CONNECTION, (UINTPTR)GetConnectionLister(), Id());

	g_main_message_handler->RemoveCallBacks(this, Id());
	g_main_message_handler->UnsetCallBacks(this);
	m_dying = TRUE;
}

OP_STATUS WebServerConnection::ReIssueRequest(Header_List &responseHeaders, int responseCode)
{
	if (m_currentRequest && m_currentRequest->InList() == FALSE)
	{
		WebserverRequest *requestCopy = OP_NEW(WebserverRequest, ());

		OP_STATUS status = OpStatus::ERR_NO_MEMORY;
		if (requestCopy == NULL || OpStatus::IsError(status = requestCopy->Copy(m_currentRequest)))
		{
			OP_DELETE(requestCopy);
			KillService(status);
			return status;
		}

		RETURN_IF_ERROR(requestCopy->SetResponseHeaders(responseHeaders));
		requestCopy->SetResponseCode(responseCode);
		requestCopy->IntoStart(&m_requestQueue);
		if (OpStatus::IsError(status = FinishRequestAndLookForNextRequest()))
		{
			KillService(status);
			return status;
		}
		return OpStatus::OK;
	}

	OP_ASSERT(!"This should not happen");

	return OpStatus::ERR;
}

BOOL
WebServerConnection::MatchMethod(WebServerMethod *method, UINT *state,char *str)
{
	// In same order as enum WebServerMethod in modules/webserver/webserver_request.h
	const char *methods[11];

	// Have to do it like this because of Brew
	methods[0] = "GET";
	methods[1] = "HEAD";
	methods[2] = "POST";
	methods[3] = "PUT";
	methods[4] = "DELETE";
	methods[5] = "LINK";
	methods[6] = "UNLINK";
	methods[7] = "TRACE";
	methods[8] = "CONNECT";
	methods[9] = "OPTIONS";
	methods[10] = NULL;

	const char * const *meth = &methods[0];
	while (*meth != NULL && op_strnicmp(str+*state, *meth, op_strlen(*meth)) != 0)
		meth++;
	if (*meth == NULL)
	{
		*method = WEB_METH_NONE_ERROR;
		return FALSE;
	}
	*method = (WebServerMethod)(meth-methods);
	*state += op_strlen(*meth);
	return TRUE;
}

BOOL
WebServerConnection::Match(const char *s, unsigned int *state, char *str,unsigned int length)
{
	int l = op_strlen(s);
	if (*state + l < length && op_strncmp(s, str+*state, l) == 0)
	{
		*state += l;
		return TRUE;
	}
	else
		return FALSE;
}

int
WebServerConnection::MatchWS(unsigned int *state, char *str, unsigned int length)
{
	int k=0;
	while (*state < length && op_isspace(str[*state]))
	{
		(*state)++;
		++k;
	}
	return k;
}

BOOL
WebServerConnection::MatchNonWS(unsigned int *start, unsigned int *state, char *str, unsigned int length)
{
	UINT k=*state;
	while (*state < length && !op_isspace(str[*state]))
		(*state)++;
	if (*state > k)
	{
		*start = k;
		return TRUE;
	}
	else
		return FALSE;
}

BOOL
WebServerConnection::MatchDecimal(int *value, unsigned int *state, char *str, unsigned  int length)
{
	int k = 0;
	UINT initial = *state;
	while (*state < length && op_isdigit(str[*state]))
	{
		k = k*10 + (str[*state] - '0');
		(*state)++;
	}
	if (initial < *state)
	{
		*value = k;
		OP_ASSERT( k >= 0);
		return TRUE;
	}
	else
		return FALSE;
}

/*removes the data between char* theBuffer and Char* notHandledData */
UINT WebServerConnection::RemoveHandledData(char *theBuffer,char* notHandledData,UINT theBytesInBuffer)
{
	OP_ASSERT(notHandledData >= theBuffer);
	if (notHandledData < theBuffer) return 0;

	if (notHandledData > theBuffer)
	{
		theBytesInBuffer -= (INTPTR)(notHandledData - theBuffer);

		op_memmove(theBuffer, notHandledData, theBytesInBuffer);
		theBuffer[theBytesInBuffer] = '\0';
	}

	return theBytesInBuffer;
}

#endif //WEBSERVER_SUPPORT
