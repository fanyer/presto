/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2004 - 2012
 *
 * Web server implementation -- script virtual resources
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "modules/webserver/webresource_base.h"
#include "modules/util/opstring.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/hardcore/timer/optimer.h"
#include "modules/formats/hdsplit.h"
#include "modules/upload/upload.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/util/datefun.h"
#include "modules/util/opfile/opfile.h"
#include "modules/formats/argsplit.h"
#include "modules/formats/uri_escape.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/webserver/webserver_callbacks.h"
#include "modules/webserver/webserver-api.h"

WebResource::WebResource(WebServerConnection *service, WebserverRequest *requestObject,OpSocketAddress *socketAddress)
	: m_service(service)
	, m_headers_sent(FALSE)
	, m_sendBody(TRUE)
	, m_requestObject(requestObject)
	, m_socketAddress(socketAddress)
	, m_totalResourceLength(~(OpFileLength)0)
	, m_responseCode(0)	
	, m_auto_cookies(TRUE)
	, m_allowPersistentConnection(TRUE)
	, m_noMoreDataToSend(FALSE)	
	, m_allOutDataIsSent(FALSE)
	, m_headerLength(0)
	, m_lastDateChanged(0)
{
	/*we take over ownership from webserverConnection here*/
	if (requestObject && requestObject->InList())
	{
		requestObject->Out();
	}
	
	OP_ASSERT(m_service != NULL);
}

/* virtual */
WebResource::~WebResource()
{
	OP_DELETE(m_requestObject);	
	OP_DELETE(m_socketAddress);
	
	OP_ASSERT(Get_Reference_Count()==0);/* If this kicks in, someone has called delete directly on this object, which is not legal */
}

OP_STATUS 
WebResource::OnConnectionClosedBase()
{
	OP_ASSERT(m_service != NULL);
	m_service = NULL;
	OP_STATUS status = OpStatus::OK;
	OP_STATUS status2;
	OP_STATUS temp_status;
	if (m_HttpConnectionListeners.Cardinal() > 0)	
	{		
		for (Webserver_HttpConnectionListener * listener = (Webserver_HttpConnectionListener *) m_HttpConnectionListeners.First(); listener; listener = (Webserver_HttpConnectionListener *) listener->Suc()) 
		{
			   /*We signal the ecmascript listeners (mainly  DOM_WebserverEcmaSendObject) that the connection has been closed.
			 * This will cause any ongoing blocking function (like flush and write) to return with and exception)
			 */ 

			if (OpStatus::IsError(temp_status = listener->WebServerConnectionKilled()))
			{
				status = temp_status;
			}
		}
	}
	
	RETURN_IF_MEMORY_ERROR(status2 = OnConnectionClosed());
	RETURN_IF_MEMORY_ERROR(status);
	return status2;
}

OP_BOOLEAN 
WebResource::ReIssueRequest()
{
	if (m_requestObject->GetSubServerNameEditedState() == TRUE)
	{
		return OpBoolean::IS_FALSE;
	}
	
	m_requestObject->SetForwardRequestState(TRUE);
	RETURN_IF_ERROR(m_service->ReIssueRequest(m_userResponseHeaders, m_responseCode));
	return OpBoolean::IS_TRUE;
}	
                    
BOOL 
WebResource::GetFowardState()
{
	if (m_requestObject)
		return m_requestObject->GetForwardRequestState();
	return TRUE;

}

OP_STATUS 
WebResource::SetResponseCode(int responseCode, const OpStringC16 &responseString)
{
	m_responseCode = responseCode;
	
	RETURN_IF_ERROR(m_responseCodeString.SetUTF8FromUTF16(responseString.CStr()));
	m_responseCodeString.Strip();
	return OpStatus::OK;
}
	
OP_STATUS 
WebResource::GetClientIp(OpString &ipAddress)
{
	if (m_socketAddress)
	{
		m_socketAddress->ToString(&ipAddress);
		return OpStatus::OK;
	}
	else
		return OpStatus::ERR;
}

OP_STATUS WebResource::SetProtocolString(const OpStringC16 &protocolString)
{
	RETURN_IF_ERROR(m_protocolString.SetUTF8FromUTF16(protocolString.CStr()));
	m_protocolString.Strip();
	return OpStatus::OK;
}

OP_BOOLEAN 
WebResource::AddResponseHeader(const OpStringC16 &header_name, const OpStringC16 &header_value, BOOL append, BOOL cookie_check_path, BOOL cookie_check_domain)
{
	if (header_name.IsEmpty())
		return OpBoolean::IS_TRUE;
	
	if (m_requestObject->GetForwardRequestState())
	{
		if (m_userResponseHeaders.CalculateLength() == 0 && m_requestObject->HasResponseHeaders())
		{
			RETURN_IF_LEAVE(m_userResponseHeaders.InitL());
			m_requestObject->TakeOverResponseHeaders(m_userResponseHeaders);
		}
	}
	
	OpString8 name;
	RETURN_IF_ERROR(name.SetUTF8FromUTF16(header_name.CStr()));
	OpString8 value; 
	RETURN_IF_ERROR(value.SetUTF8FromUTF16(header_value.CStr()));
#ifndef TURN_OFF_SERVER_COOKIE_CHECK

	/* Check security for cookie headers. */

	if (uni_stri_eq(header_name.CStr(), UNI_L("set-cookie")) || uni_stri_eq(header_name.CStr(), UNI_L("set-cookie2")))
	{
		Sequence_Splitter aList;
		HeaderEntry *cookieHeader = OP_NEW(HeaderEntry, ());
		RETURN_OOM_IF_NULL(cookieHeader);
		
		TRAPD(status, cookieHeader->ConstructFromNameAndValueL(aList, name.CStr(), value.CStr(), value.Length()));/* If OpStatus::OK, aList takes over cookieHeader. */
		if (OpStatus::IsError(status))
		{
			OP_DELETE(cookieHeader);
			return status;
		}
		
		ParameterList *parameters = NULL;
		
		RETURN_IF_LEAVE(parameters = cookieHeader->GetParametersL(PARAM_SEP_SEMICOLON | PARAM_STRIP_ARG_QUOTES | PARAM_SEP_WHITESPACE | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters));
	
		if (parameters)
		{
			Parameters *parameter = NULL;
			WebSubServer *subserver = m_service->GetCurrentSubServer();
			
			OP_ASSERT((cookie_check_path == TRUE && subserver) || cookie_check_path == FALSE);

			if (cookie_check_path && subserver == NULL) 
				return OpStatus::ERR;
			
			if (cookie_check_path && subserver->IsRootService() == FALSE ) /* The root server can set any path. */
			{
				/* check the cookie path. This must match the subserver virtual path. This prevents one subserver from setting headers for another subserver. */	
				if (NULL!=(parameter = parameters->GetParameter("path", PARAMETER_ANY)))
				{
					if (parameters->GetParameter("path", PARAMETER_ANY, parameter) != NULL)
						return OpBoolean::IS_FALSE;  /* return security if path is set several times.*/
					
					/* Check the cookie path */
					const char *cookie_path = parameter->GetValue().CStr();
					
					if (cookie_path == NULL)
						return OpBoolean::IS_FALSE;
					
					while (*cookie_path == '/')
						cookie_path++;
			
					unsigned int cookie_subserver_length = 0;
					while (cookie_path[cookie_subserver_length] != '/' && cookie_path[cookie_subserver_length] != '\0')
						cookie_subserver_length++;
					
					OpString url_encoded_path;
					RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(subserver->GetSubServerVirtualPath(), url_encoded_path));
					
					OpString8  subserver_virtual_path;
					RETURN_IF_ERROR(subserver_virtual_path.SetUTF8FromUTF16(url_encoded_path.CStr()));
					
					char *virtual_path = subserver_virtual_path.CStr();
					
					while (*virtual_path == '/')
						virtual_path++;
			
					unsigned int virtual_path_length = 0;
					while (virtual_path[cookie_subserver_length] != '/' && virtual_path[virtual_path_length] != '\0')
						virtual_path_length++;
					
					if (op_strncmp(cookie_path, virtual_path, cookie_subserver_length) != 0 || cookie_subserver_length != virtual_path_length)
					{
						return OpBoolean::IS_FALSE;
					}
				}
				else
				{
					/* Set default path parameter */
					OpString url_encoded_path;
					RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(subserver->GetSubServerVirtualPath(), url_encoded_path));
					OpString8 url_encoded_path_utf8;
					RETURN_IF_ERROR(url_encoded_path_utf8.SetUTF8FromUTF16(url_encoded_path.CStr()));
		
					RETURN_IF_ERROR(value.AppendFormat("; path=/%s/", url_encoded_path_utf8.CStr()));
				}
			}
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT			
			if (cookie_check_domain)
			{
				/* FIXME: Do not check domain if the request is a localhost request */
				
				/* also check the domain name. This must match this server. */

				OpString8 hostname;
				OpString8 server_domain;
				HeaderEntry *host_header =  m_requestObject->GetClientHeaderList()->GetHeader("host");
				int port = 0;
				if (host_header)
				{
					RETURN_IF_ERROR(hostname.Set(host_header->Value()));
					char *port_str;
					if (NULL!=(port_str = op_strchr(hostname.CStr(), ':')))
					{
						port = op_atoi(port_str + 1); 
						*port_str = '\0';/* remove port part */
					}
										
					if (port == 0)
						port = 80;	
				}
				
				if (m_requestObject->IsProxied()) // 	Request had to come through proxy, and domain must match address through proxy: device.user.proxyname.com  
				{				// 	Fixme. Check from which listening socket the request actualy came from (localhost, proxy, nat etc..)
/*					OpString prefs_domain_utf16;
					RETURN_IF_ERROR(prefs_domain_utf16.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice)));
					RETURN_IF_ERROR(prefs_domain_utf16.Append("."));
					RETURN_IF_ERROR(prefs_domain_utf16.Append(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser)));
					RETURN_IF_ERROR(prefs_domain_utf16.Append("."));
					RETURN_IF_ERROR(prefs_domain_utf16.Append(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost)));
					RETURN_IF_ERROR(server_domain.SetUTF8FromUTF16(prefs_domain_utf16.CStr()));*/
					server_domain.Set(g_webserver->GetServerName()->Name());
				}
				else
				{
					RETURN_IF_ERROR(server_domain.Set(hostname));
				}
					
				if (NULL!=(parameter = parameters->GetParameter("domain", PARAMETER_ANY)))
				{
					if (parameters->GetParameter("domain", PARAMETER_ANY, parameter) != NULL)
						return OpBoolean::IS_FALSE;  /* return security error if domain is set several times.*/
					
					const char *cookie_domain = parameter->GetValue().CStr();
					int cookie_port = 0;
					const char *cookie_port_str = op_strchr(cookie_domain, ':');
					unsigned int cookie_domain_strlen;
					if (cookie_port_str)
					{
						cookie_port = op_atoi(cookie_port_str + 1);
						cookie_domain_strlen  = cookie_port_str - cookie_domain;
					}
					else
					{
						 cookie_domain_strlen = op_strlen(cookie_domain);
					}

					if (cookie_port == 0)
						cookie_port = 80;
					
					if (op_strnicmp(cookie_domain, server_domain.CStr(), cookie_domain_strlen) != 0  || op_strlen(server_domain.CStr()) != cookie_domain_strlen)
					{
						return OpBoolean::IS_FALSE;  /* return security error if domain set but is incorrect.*/
					}
				}
				else if (m_requestObject->IsProxied())
				{
					/* Set default domain parameter */
					RETURN_IF_ERROR(value.AppendFormat("; domain=%s", server_domain.CStr()));
				}
			}
#endif // WEBSERVER_RENDEZVOUS_SUPPORT			
		}
	}
#endif // !TURN_OFF_SERVER_COOKIE_CHECK
	
	if (append)
	{		
		Header_Item *header = OP_NEW(Header_Item, ());
		if (header == NULL) 
			return OpStatus::ERR_NO_MEMORY;
		
		TRAPD(err, header->InitL(name, value));
		
		if (OpStatus::IsError(err))
		{
			OP_DELETE(header);
			return OpStatus::ERR_NO_MEMORY;
		}
		
		m_userResponseHeaders.InsertHeader(header);
	}
	else
	{
		RETURN_IF_LEAVE(m_userResponseHeaders.ClearAndAddParameterL(name, value));
	}
	
	return OpBoolean::IS_TRUE;
}

OP_STATUS
WebResource::HasHeader(const OpStringC header_name, BOOL &found)
{
	OpString8 name;
	found=FALSE;
	
	RETURN_IF_ERROR(name.SetUTF8FromUTF16(header_name.CStr()));
	
	if (m_userResponseHeaders.FindHeader(name, FALSE))
		found=TRUE;
	
	return OpStatus::OK;
}

OP_BOOLEAN
WebResource::BaseCheckModified()
{		
	OP_BOOLEAN hasChanged = OpBoolean::IS_TRUE;
	
	HeaderList *clientHeaderList = m_requestObject->GetClientHeaderList();
	
	if (clientHeaderList)
	{
		/*Check the cache here
	 	* If the resource has changed, we reply with 304 and we do not send the body
	 	*/			
		HeaderEntry *clientHeader = clientHeaderList->GetHeader("If-Modified-Since");
		if (clientHeader != NULL)
		{
			OpString headerValue;
			RETURN_IF_ERROR(headerValue.SetFromUTF8(clientHeader->Value()));
			
			time_t resource_modified;
			RETURN_IF_ERROR(GetLastModified(resource_modified));

			if (resource_modified == 0) /* If uknown set current date */
			{
				resource_modified = (time_t)(OpDate::GetCurrentUTCTime()/1000);	
			}
			
			double client_version_modified = op_floor(OpDate::ParseDate(headerValue.CStr())/1000);
			
			if (client_version_modified >= resource_modified)
			{
				hasChanged = OpBoolean::IS_FALSE;
			}
		}
	}

	if (hasChanged == OpBoolean::IS_TRUE)
	{
		/*If method is HEAD we do not send the body*/
		if (m_requestObject->GetHttpMethod() != WEB_METH_HEAD)
		{
			m_sendBody = TRUE;
		}
		else
		{
			m_sendBody = FALSE;
		}
	}
	else
	{
		m_sendBody = FALSE;			
	}

	return hasChanged;
}

OP_STATUS 
WebResource::SetResponseString(OpString8 &string, int responseCode)
{
	switch (responseCode)
	{
	case 200:
		return string.Set("OK");
	
	case 201:
		return string.Set("Created"); 

	case 202:
		return string.Set("Accepted"); 

	case 203:
		return string.Set("Non-Authoritative Information"); 

	case 204:
		return string.Set("No Content"); 

	case 205:
		return string.Set("Reset Content"); 
		
	case 206:
		return string.Set("Partial Content");
		
	case 300:
		return string.Set("Multiple Choices"); 
	
	case 301:
		return string.Set("Moved Permanently"); 
	
	case 302:
		return string.Set("Found"); 
	
	case 303:
		return string.Set("See Other"); 

	case 304:
		return string.Set("Not Modified"); 

	case 305:
		return string.Set("Use Proxy"); 

	case 307:
		return string.Set("Temporary Redirect"); 

	case 400:
		return string.Set("Bad Request"); 
		
	case 401:
		return string.Set("Unauthorized");

	case 402:
		return string.Set("Payment Required");

	case 403:
		return string.Set("Forbidden");
		
	case 404:
		return string.Set("Resource not found");
		
	case 405:
		return string.Set("Method not allowed");

	case 406:
		return string.Set("Not Acceptable");

	case 407:
		return string.Set("Proxy Authentication Required");

	case 408:
		return string.Set("Request Timeout");

	case 409:
		return string.Set("Conflict");

	case 410:
		return string.Set("Gone");

	case 411:
		return string.Set("Length Required");
		
	case 412:
		return string.Set("Precondition Failed");

	case 413:
		return string.Set("Request Entity Too Large");

	case 414:
		return string.Set("Request-URI Too Long");

	case 415:
		return string.Set("Unsupported Media Type");

	case 416:
		return string.Set("Requested Range Not Satisfiable");

	case 417:
		return string.Set("Expectation Failed");

	case 500:	
		return string.Set("Internal Server Error");

	case 502:	
		return string.Set("Bad Gateway");

	case 503:	
		return string.Set("Service Unavailable");

	case 504:	
		return string.Set("Gateway Timeout");

	case 505:	
		return string.Set("HTTP Version Not Supported");
		
	default:
		/* unknown response code */
		string.Wipe();
		return string.AppendFormat("%d", responseCode);
	}
}

OP_STATUS 
WebResource::GetHeaderValue(Header_Item *header, OpString8 &headerValue)
{
	headerValue.Wipe();
	
	int length = header->CalculateLength() + 1;

	char *header_string = headerValue.Reserve(length);
	
	if (header_string == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	
	header->OutputHeader(header_string);
	
	header_string[length - 1 ] = '\0';
	
	char *value = op_strstr(header_string, ":");

	if (value)
	{
		value++;
		while (op_isspace(*value))
		{
			value++;
		}
	}
	else
	{
		return OpStatus::OK;
	}
	
	length = op_strlen(value);
	
	op_strcpy(header_string, value);

	while (*(header_string + length - 1) == '\n' || *(header_string + length - 1) == '\r')
	{
		length--;
	}
	
	header_string[length] = '\0';
	
	return OpStatus::OK;
}

OP_STATUS
WebResource::SetupResponseCode(int &responseCode)
{
	/* if no responsecode has been set we will calculate the correct one here, from request header and data from local resource */
	
	if (responseCode != 0)
	{
		return OpStatus::OK;
	}

	OP_BOOLEAN modified;

	RETURN_IF_ERROR(modified = BaseCheckModified());

	if (modified == OpBoolean::IS_FALSE)
	{
		responseCode = 304;
	}
	else
	{
		responseCode = 200;
	}
	
	return OpStatus::OK;
}

OP_STATUS
WebResource::BuildResponseHeaders(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{
	OP_ASSERT(m_service != NULL);
	
	g_webserver->SetDefaultResourceHeaders(this);
	
	FilledDataSize = 0;

	if (m_requestObject->GetForwardRequestState())
	{
		if (m_responseCode == 0 && m_requestObject->GetResponseCode() != 0)
			m_responseCode = m_requestObject->GetResponseCode();
		
		if (m_userResponseHeaders.CalculateLength() == 0 && m_requestObject->HasResponseHeaders())
		{
			RETURN_IF_LEAVE(m_userResponseHeaders.InitL());
			m_requestObject->TakeOverResponseHeaders(m_userResponseHeaders);
		}
	}
	
	int protocol_major = m_service->getProtocolVersion().protocol_major;
	int protocol_minor = m_service->getProtocolVersion().protocol_minor;
	
	OP_ASSERT(protocol_major >= 0);
	OP_ASSERT(protocol_minor >= 0);
	
	if (m_protocolString.IsEmpty())
	{
		if (protocol_major < 1 || (protocol_major == 1 && protocol_minor < 1))
		{
			RETURN_IF_ERROR(m_protocolString.Set("HTTP/1.0"));
		}
		else
		{
			RETURN_IF_ERROR(m_protocolString.Set("HTTP/1.1"));
		}
	}
	else if (m_protocolString.Length() > 50)
	{
		return OpStatus::ERR;
	}	
		
	if (!m_responseCode)
	{
		RETURN_IF_ERROR(SetupResponseCode(m_responseCode));
	}
	
	if (m_responseCodeString.IsEmpty())
	{
		RETURN_IF_ERROR(SetResponseString(m_responseCodeString, m_responseCode));
	}
	else if (m_responseCodeString.Length() > 500)
	{
		return OpStatus::ERR;
	}	
	
	/* Set the session cookie, if the user did not send a cookie in the request. */
	if (m_requestObject->RequestHadSessionCookie() == FALSE && m_requestObject->GetSessionId() && m_auto_cookies)
	{
		OpString session_cookie_value;	
		RETURN_IF_ERROR(session_cookie_value.Set("unite-session-id="));
		RETURN_IF_ERROR(session_cookie_value.Append(m_requestObject->GetSessionId()));
		RETURN_IF_ERROR(session_cookie_value.Append("; Max-Age=2073600"));
		RETURN_IF_ERROR(session_cookie_value.Append("; path=/"));   			/* fixme: for magic url, the path should be the service */
	
		RETURN_IF_ERROR(AddResponseHeader(UNI_L("Set-Cookie"), session_cookie_value.CStr(), TRUE, FALSE, FALSE));
	}
	
	OpString8 http_string;	
	RETURN_IF_ERROR(http_string.AppendFormat("%s %d %s" CRLF, m_protocolString.CStr(), m_responseCode, m_responseCodeString.CStr()));
	
	Header_List responseHeaders; 
	RETURN_IF_LEAVE(responseHeaders.InitL());	
	RETURN_IF_LEAVE(responseHeaders.AddParameterL(NULL, http_string.CStr()));
	
	if (m_userResponseHeaders.FindHeader("Server", FALSE) == NULL)
	{
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT		
		OpString8 device;
		RETURN_IF_ERROR(device.SetUTF8FromUTF16(g_webserver->GetWebserverUriIdna()));
		OpString8 server;
		RETURN_IF_ERROR(server.AppendFormat("Opera/%s", device.CStr()));
		RETURN_IF_LEAVE(responseHeaders.AddParameterL("Server", server.CStr()));
#else
		RETURN_IF_LEAVE(responseHeaders.AddParameterL("Server","Opera"));
#endif
	}
	
	if (m_responseCode >= 200 && m_responseCode != 204 && m_responseCode != 304) /* header length for 206 is set by the file resource */
	{
		Header_Item *header = NULL;
		if ((header = m_userResponseHeaders.FindHeader("Content-Length", FALSE)) != NULL)
		{
			if(m_totalResourceLength == ~(OpFileLength)0)
			{
				OpString8 header_value;
				RETURN_IF_ERROR(GetHeaderValue(header, header_value));
				m_totalResourceLength = op_atoi(header_value.CStr());
			}
		}
		else if (m_totalResourceLength != ~(OpFileLength)0)
		{
			OpString header_value;
			header_value.AppendFormat(UNI_L("%lu"), m_totalResourceLength);
			RETURN_IF_ERROR(AddResponseHeader(UNI_L("Content-Length"), header_value.CStr()));						
		}
	}
	else 
	{
		Header_Item *header = m_userResponseHeaders.FindHeader("Content-Length", FALSE);
		if (header)
		{
			header->Out();
			OP_DELETE(header);
			m_totalResourceLength = 0;
		}
	}

	if (m_userResponseHeaders.FindHeader("Content-Type", FALSE) == NULL)
	{
		RETURN_IF_LEAVE(responseHeaders.AddParameterL("Content-Type","text/html"));
	}
	
	if(m_service->ClientIsOwner() || m_service->ClientIsLocal())
		RETURN_IF_LEAVE(responseHeaders.AddParameterL("Cache-Control","no-cache"));

	if (protocol_major < 1 || (protocol_major == 1 && protocol_minor < 1))
	{
		m_allowPersistentConnection = FALSE;
	}
	else
	{
		Header_Item *header = NULL;
	
		if (m_userResponseHeaders.FindHeader("Content-Length", FALSE) == NULL)
		{
			if ((header = m_userResponseHeaders.FindHeader("Transfer-Encoding", FALSE)) == NULL || header->CalculateLength() == 0)
			{					
				m_allowPersistentConnection = FALSE;	
			}
			else
			{
				int length = header->CalculateLength() + 1;
				char *value = OP_NEWA(char, length); /* ARRAY OK 2008-06-06 haavardm */ 
				
				if (value == NULL)
					return OpStatus::ERR_NO_MEMORY;
				
				header->OutputHeader(value);
				
				value[length - 1 ] = '\0';
				      
				if (op_stristr(value, "chunked") == NULL)
				{
					m_allowPersistentConnection = FALSE;			
				}
				
				OP_DELETEA(value);
			}
		}
	}	
	time_t timeStamp = (time_t)(OpDate::GetCurrentUTCTime()/1000);
	uni_char dateString[70]; /* ARRAY OK 2008-06-10 haavardm */

	struct tm* gt = op_gmtime(&timeStamp);
	FMakeDate(*gt,"\247w, \247D \247n \247Y \247h:\247m:\247s UTC", dateString, 68);

	OpString8 currentDate;
	RETURN_IF_ERROR(currentDate.SetUTF8FromUTF16(dateString));
	
	RETURN_IF_LEAVE(responseHeaders.AddParameterL("Date", currentDate));
		
	if (m_responseCode >= 200 && m_responseCode < 300 && m_userResponseHeaders.FindHeader("Last-Modified", FALSE) == NULL)
	{
		time_t last_modified;
		RETURN_IF_ERROR(GetLastModified(last_modified));
		
		if (last_modified)
		{
			struct tm* file_time = op_gmtime(&last_modified);
			FMakeDate(*file_time,"\247w, \247D \247n \247Y \247h:\247m:\247s UTC", dateString, 68);

			OpString8 modifiedDate;
			RETURN_IF_ERROR(modifiedDate.SetUTF8FromUTF16(dateString));

			RETURN_IF_LEAVE(responseHeaders.AddParameterL("Last-Modified", modifiedDate.CStr()));
		}
	}
			
	if (m_responseCode >= 200 && m_responseCode < 300 && m_userResponseHeaders.FindHeader("ETag", FALSE) == NULL)
	{
		OpString8 etag;
		RETURN_IF_ERROR(GetETag(etag));
		
		if (etag.Length())
		{
			RETURN_IF_LEAVE(responseHeaders.AddParameterL("ETag", etag.CStr()));
		}
	}

	/*Check that the buffer is big enough too contain all the headers*/
	if (bufferSize < responseHeaders.CalculateLength() + m_userResponseHeaders.CalculateLength() + 3) 
	{
		return OpStatus::ERR;
	}
	
	responseHeaders.OutputHeaders(buffer);

	int userResponseHeadersLength = m_userResponseHeaders.CalculateLength();
	
	m_userResponseHeaders.OutputHeaders(buffer + responseHeaders.CalculateLength());

	op_sprintf(buffer+responseHeaders.CalculateLength() + userResponseHeadersLength, CRLF);	
	
	*(buffer+responseHeaders.CalculateLength() + userResponseHeadersLength + 2) = '\0';

	m_headerLength = op_strlen(buffer);
	
	OP_ASSERT(m_headerLength < WEB_PARM_DATABUFFER_SIZE - 1);
	
	FilledDataSize = m_headerLength;
	m_headers_sent = TRUE;
	
	return OpStatus::OK;
}

OP_BOOLEAN
WebResource::GetData(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{
	OP_ASSERT(m_service != NULL);

	if (!m_headers_sent)
	{
		RETURN_IF_ERROR(BuildResponseHeaders(buffer, bufferSize, FilledDataSize));
		
		if (!m_sendBody)
		{
			m_totalResourceLength = 0;				
		}	
		return OpBoolean::IS_TRUE;	
	}
	else if (!m_sendBody || m_responseCode == 204 || m_responseCode == 304)
	{
		FilledDataSize = 0;
		return OpBoolean::IS_FALSE;
	}
	
	OP_ASSERT(m_responseCode != 304);
	unsigned int DataSize = 0;
	OP_BOOLEAN result;
	RETURN_IF_ERROR( result = FillBuffer(buffer + FilledDataSize, bufferSize - FilledDataSize, DataSize));
	
	FilledDataSize += DataSize;
	return result;
}

OP_BOOLEAN WebResource::ResourceModifiedSince(const char *clientsDate, const char *resourceLastDateChanged)
{
	OpString uniClientsDate;
	RETURN_IF_ERROR(uniClientsDate.Set(clientsDate));
	
	OpString uniResourceLastDateChanged;	
	RETURN_IF_ERROR(uniResourceLastDateChanged.Set(resourceLastDateChanged));
	
	if (OpDate::ParseDate(uniResourceLastDateChanged.CStr()) > OpDate::ParseDate(uniClientsDate.CStr()))
	{
		return OpBoolean::IS_TRUE;
	}
	else
	{
		return OpBoolean::IS_FALSE;
	}
}

OP_STATUS WebResource::FindItemsInString(const uni_char *string, const uni_char *itemName, OpVector<OpString> *itemList, BOOL unescape)
{	
	unsigned int index = 0;
	while (index < uni_strlen(string))
	{
		OpString *itemValue = OP_NEW(OpString,());
		RETURN_OOM_IF_NULL(itemValue);
		BOOL found;
		OP_STATUS err = FindItemsInString(string, itemName, *itemValue, found, index, unescape);

		if (err || found == FALSE )
		{
			OP_DELETE(itemValue);
			RETURN_IF_ERROR(err);
		}
		else
		{
			err = itemList->Add(itemValue);
			if (err)
			{
				itemList->RemoveByItem(itemValue);
				OP_DELETE(itemValue);
				return err;					
			}
		}
	}

	return OpStatus::OK;
}

OP_STATUS WebResource::FindIAlltemsInString(const uni_char *itemStart, AddToCollectionCallback *collection, BOOL unescape, BOOL translatePlus)
{
	while (itemStart != NULL && itemStart[0] != '\0')
	{		
		const uni_char *itemEnd = uni_strpbrk(itemStart, UNI_L("=&\r\n"));
		if (itemEnd == itemStart)
		{
			itemStart++;
			continue;
		}
		
		
		unsigned int itemNameLength;
		
		if (itemEnd == NULL)
		{
			itemNameLength = uni_strlen(itemStart);
			itemEnd = itemStart + itemNameLength;

			OpString itemName;
			RETURN_IF_ERROR(itemName.Set(itemStart, itemNameLength));
			
			RETURN_IF_ERROR(collection->AddToCollection(itemName.CStr(), UNI_L("")));
			break;
		}
		else
		{
			itemNameLength = (unsigned int)(itemEnd - itemStart);			
		}

		if (*itemEnd == '=') // if this was an assigment
		{			
			const uni_char *valueStart = itemEnd + 1;
			const uni_char *valueEnd = uni_strstr(itemStart, "&");
	
			if (valueEnd == 0)
				valueEnd= uni_strstr(valueStart, UNI_CRLF);         /*FIXME : Maybe we also should accept only CR or LF*/
			if (valueEnd == 0)
				valueEnd = &valueStart[uni_strlen(valueStart)];
				
			if (valueEnd < valueStart) 
			{
				valueStart = valueEnd;
				itemEnd = valueStart;
			}
							
			if (itemNameLength > 0 && valueEnd != NULL && valueEnd >= valueStart )
			{
				OpString itemName;
				RETURN_IF_ERROR(itemName.Set(itemStart, itemNameLength));			
				OpString value;
				if (valueEnd > valueStart)
				{
					int l = valueEnd - valueStart;
					RETURN_IF_ERROR(value.Set(valueStart, l));
					if (unescape)
					{
					#ifdef FORMATS_URI_ESCAPE_SUPPORT
						UriUnescape::ReplaceChars(value.CStr(), UriUnescape::NonCtrlAndEsc | UriUnescape::TranslatePlusToSpace | UriUnescape::ConvertUtf8);
					#else
						ReplaceEscapedChars(value, l);
						value[l] = '\0';
					#endif
					}
					if (translatePlus)
						StrReplaceChars(value.CStr(), '+', ' ');
				}
				
				if (value.IsEmpty())
					RETURN_IF_ERROR(value.Set(""));

				RETURN_IF_ERROR(collection->AddToCollection(itemName.CStr(), value.CStr()));
			}
			else
			{
				itemStart = itemEnd + 1; 
			}

			if (itemStart[1] != '\0')
			{
				itemStart = uni_strstr(valueEnd, UNI_L("&")); /*finds the next item */
				if (itemStart)
				{			
					itemStart = itemStart + 1;
				}
			}
			else
			{
				break;
			}			
		}
		else // No assignment
		{
			if (itemNameLength > 0)
			{
				OpString itemName;
				RETURN_IF_ERROR(itemName.Set(itemStart, itemNameLength));
				
				RETURN_IF_ERROR(collection->AddToCollection(itemName.CStr(), UNI_L("")));
			}
			itemStart = itemEnd + 1;
			while (*itemStart == '\r' || *itemStart == '\n')
				itemStart++;
		}
			
		
	}
	return OpStatus::OK;
}

OP_STATUS WebResource::FindItemsInString(const uni_char *string, const uni_char *itemName, OpString &itemValue, BOOL &found, unsigned int &index, BOOL unescape)
{
	found = FALSE;

	const uni_char *itemStart = &string[index];

	int itemNameLength = uni_strlen(itemName);

	while (itemStart != NULL && *itemStart != '\0')
	{
		if (
				uni_strncmp(itemStart, itemName, itemNameLength) == 0   /*means that it matched*/
				&& 
					( &itemStart[itemNameLength] == uni_strpbrk(itemStart, UNI_L("=&\r\n")) /*check that the item name ends with =*/
							|| itemStart[itemNameLength] == '\0' /*Check that the string has not ended*/
					)
			)
		{
			if (itemStart[itemNameLength] == '=' && itemStart[itemNameLength + 1] != '\0')
			{
				/*item was found*/				
				const uni_char *valueStart = &itemStart[itemNameLength + 1];
				const uni_char *valueEnd = uni_strpbrk(valueStart, UNI_L("&\r\n"));
				
				if (valueEnd == 0)
					valueEnd = &valueStart[uni_strlen(valueStart)];

				unsigned int valueLength = valueEnd - valueStart;
								
				if (valueLength > 0 )
				{
					found = TRUE;
					index = valueEnd - string + 1;
					int l = valueEnd - valueStart;
					RETURN_IF_ERROR(itemValue.Set(valueStart, l));
					if (unescape)
					{
					#ifdef FORMATS_URI_ESCAPE_SUPPORT
						UriUnescape::ReplaceChars(itemValue.CStr(), UriUnescape::NonCtrlAndEsc | UriUnescape::TranslatePlusToSpace | UriUnescape::ConvertUtf8);
					#else
						ReplaceEscapedChars(itemValue, l);
						itemValue[l] = '\0';
					#endif
					}
					return OpStatus::OK;
				}
			}

			found = TRUE;
			index = itemStart + itemNameLength - string;
			while (string[index] == '&' || string[index] == '=') 
				index++;  
			
			return itemValue.Set("");
		}

		
		if (itemStart[1] != '\0')
		{
			itemStart = uni_strstr(itemStart + 1, UNI_L("&")); /*finds the next item */
			while (itemStart && (*itemStart == '&' || *itemStart == '='))
			{			
				itemStart++;				
			}
		}
		else
		{
			itemStart = NULL;
		}		
	}
	index = uni_strlen(string);
	return OpStatus::OK;
}

#ifndef FORMATS_URI_ESCAPE_SUPPORT
/* static */ OP_STATUS WebResource::EscapeItemSplitters(const uni_char *string, const uni_char *escape_chars, OpString &escaped_string)
{
    if (escaped_string.Reserve(uni_strlen(string) * 3 + 1) == NULL)
        return OpStatus::ERR_NO_MEMORY;
    
    const uni_char *escape_character;
    while ((escape_character = uni_strpbrk(string, escape_chars)))
    {
        RETURN_IF_ERROR(escaped_string.Append(string, escape_character - string));
        RETURN_IF_ERROR(escaped_string.AppendFormat(UNI_L("%%%2X"), *escape_character));
        string = escape_character + 1;
    }
    return escaped_string.Append(string);
}
#endif // FORMATS_URI_ESCAPE_SUPPORT

void WebResource::AddHttpConnectionListener(Webserver_HttpConnectionListener *listener)
{
	listener->Into(&m_HttpConnectionListeners);
}

void WebResource::RemoveHttpConnectionListener(Webserver_HttpConnectionListener *listener)
{
	listener->Out();
}

WebResource_Null::WebResource_Null(WebServerConnection *service, WebserverRequest *requestObject)
	: WebResource(service, requestObject)
{
}

/* static */ OP_STATUS
WebResource_Null::Make(WebResource *&nullResource, WebServerConnection *service,WebserverRequest *requestObject, int responseCode, const OpStringC &body)
{
	nullResource = NULL;
	WebResource_Null *temp_nullResource = OP_NEW(WebResource_Null, (service, requestObject));

	if (temp_nullResource == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}

	OpAutoPtr<WebResource_Null> temp_nullResourceAnchor(temp_nullResource);
	
	
	temp_nullResource->m_responseCode = responseCode;
	OpString newLocation;
	
	if (body.IsEmpty())
	{	
		switch (responseCode) {
			case 200:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Set(""));
				break;
				
			case 301:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>301 Moved Permanently</h1> The url has moved permanently</body></html>\r\n"));
	
			
				RETURN_IF_ERROR(newLocation.Set(requestObject->GetRequest()));
				
				if (newLocation.Length() > 0 && newLocation[newLocation.Length()-1] != '/')
				{
					RETURN_IF_ERROR(newLocation.Append("/"));
				}
				
				RETURN_IF_ERROR(temp_nullResource->AddResponseHeader(UNI_L("Location"), newLocation, FALSE));
				
				break;
	
			case 400:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>400 Bad Request</h1>Your browser sent a bad request</body></html>\r\n"));
				break;
				
			case 403:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>403 Forbidden</h1>Your browser sent a request to forbidden source</body></html>\r\n"));
				break;
	
			case 404:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<!DOCTYPE html><html><head><title>Address not found</title><style type=\"text/css\">*{margin:0;padding:0;color:#58595b;font-family:Arial,sans-serif;}body{margin:0 auto;font-size:1em;}div{background:#262626;padding:10px;border-bottom:4px solid #DD2000;}h1{font-size:1.2em;color:#fff;max-width:960px;margin: auto;}p{margin:10px auto;max-width:960px;}a{color:#28afe2;text-decoration:none;}a:hover{text-decoration:underline;}</style></head><body><div><h1>The address was not found (Error 404)</h1></div><p>The address could not be found on this Opera Unite server. <a href=\"/\">Visit the Opera Unite Home page</a> to see available applications.</p></body></html>\r\n"));
				break;
	
			case 405:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>405 method not allowed</h1> No resource is handling this method</body></html>\r\n"));			
				break;
			
			case 500:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>500 Internal Error</h1> An internal error happened in the server</body></html>\r\n"));			
				break;
	
			case 501:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>501 method not allowed</h1> No resource is handling this method</body></html>\r\n"));			
				break;
			
			case 503:
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>503 Service Unavailable</h1>This service is unavailable</body></html>\r\n"));			
				break;
			
			default:	
				RETURN_IF_ERROR(temp_nullResource->m_returnData.Append("<html><body><h1>Unkown Response Code</h1></body></html>\r\n"));
				OP_ASSERT(!" WebResource_Null does not know this response code. */");
				break;
		}
	}
	else
	{
		RETURN_IF_ERROR(temp_nullResource->m_returnData.Set(body.CStr()));
	}
	temp_nullResource->m_totalResourceLength = temp_nullResource-> m_returnData.Length();
	
	RETURN_IF_ERROR(service->StartServingData());

	nullResource = temp_nullResourceAnchor.release();
	return OpStatus::OK;
}


OP_STATUS 
WebResource_Null::HandleIncommingData(const char *incommingData, unsigned int length,BOOL lastData)
{
	OP_ASSERT(!"NOT IMPLEMENTED");	
	return OpStatus::ERR;
}

/* virtual */ OP_BOOLEAN
WebResource_Null::FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{
	OP_ASSERT(bufferSize >= (unsigned int)m_returnData.Length());

	if (bufferSize < (unsigned int)m_returnData.Length())
	{
		FilledDataSize = 0;
		return OpBoolean::IS_FALSE;		
	}
	
	if (!m_returnData.IsEmpty())
	{
		op_strcpy(buffer, m_returnData.CStr());
	}
	
	FilledDataSize = m_returnData.Length();
	return OpBoolean::IS_FALSE;
}

WebResource_Administrative_Standard::WebResource_Administrative_Standard(WebServerConnection *service, WebserverRequest *requestObject)
: WebResource(service, requestObject)
,read_bytes(0)
{
	OP_ASSERT(service && service->GetWebServer());
	
	server=(service)?service->GetWebServer():NULL;
	m_auto_cookies=FALSE;	
}

/* virtual */ OP_STATUS
WebResource_Administrative_Standard::GetLastModified(time_t &date)
{
	OP_ASSERT(server);
	
	if (!server)
		return OpStatus::ERR;

	// TODO: this behaviour is not the best...
	date=(server)?server->GetLastServicesChangeTime():0;

	return OpStatus::OK;
}

/* virtual */ OP_STATUS
WebResource_Administrative_Standard::HandleIncommingData(const char *incommingData, UINT32 length, BOOL lastData)
{
	return OpStatus::OK;
}

/* virtual */ OP_BOOLEAN
WebResource_Administrative_Standard::FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{
	UINT32 to_read=buf.Length()-read_bytes;
	OP_ASSERT(FilledDataSize<=bufferSize);
	
	if(to_read>bufferSize-FilledDataSize)
	  to_read=bufferSize-FilledDataSize;
	
	OP_ASSERT(to_read<=bufferSize && to_read<=bufferSize-FilledDataSize && to_read<=buf.Length());
	
	if(to_read)
	{
		buf.Extract(read_bytes, to_read, buffer+FilledDataSize);
		//op_memcpy(buffer+FilledDataSize, xml.CStr(), to_read);
		FilledDataSize+=to_read;
		read_bytes+=to_read;
	}
	
	
	if(read_bytes<buf.Length())
		return OpBoolean::IS_TRUE;
		
	return OpBoolean::IS_FALSE;
}

#endif /* WEBSERVER_SUPPORT */
