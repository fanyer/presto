/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006-2012
 */
#include "core/pch.h"

#if defined(WEBSERVER_SUPPORT) && defined(NATIVE_ROOT_SERVICE_SUPPORT)
#include "modules/formats/uri_escape.h"
#include "modules/webserver/webserver_request.h"
#include "modules/webserver/src/resources/webserver_index.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/webserver-api.h"

WebResource_Index::WebResource_Index(WebServerConnection *service, WebserverRequest *requestObject)
	: WebResource(service, requestObject)
	, m_printState(PRINTSTATE_HEAD)
	, m_currentSubServer(0)

{

	m_headFormat = 		
						"<!DOCTYPE HTML>\n"
						"<HTML>\n"
						"<HEAD>\n"
						"  <TITLE>Opera home page</TITLE>\n"
						"</HEAD>\n";
			
	m_initFormat = 
						"<BODY> \n" 
						"  <P>No root service installed. Using native root service.\n</P>"
						"  <H1>Resources</H1>\n"
						"  <UL>";

	m_subServerHtmlFormat =						
						"    <LI>\n"
						"      <STRONG><A href='%s/'>%s</A></STRONG>   <BR />\n"
						"      %s <BR />\n"
						"      Made by <A href='%s'>%s</A>.\n"
						"    </LI>\n";
	m_EOFformat =					
						"  </UL>\n"
						"  <BR>\n"
						"  <A href='http://unite.opera.com/applications/'>Get more unite applications</A>\n"
						"</BODY>\n"
						"</HTML>\n";
}

WebResource_Index::~WebResource_Index()
{
}

/* static */ 
WebResource_Index *WebResource_Index::Make(WebServerConnection *service, WebserverRequest *requestObject)
{	
	WebResource_Index *indexResource = OP_NEW(WebResource_Index, (service, requestObject));
	
	if (indexResource == NULL || OpStatus::IsError(service->StartServingData()))
	{
  		OP_DELETE(indexResource);
		return NULL;
	}
	
	if (OpStatus::IsError(indexResource->AddResponseHeader(UNI_L("Connection"), UNI_L("Close"), FALSE)))
	{
  		OP_DELETE(indexResource);
		return NULL;		
	}
	
	return indexResource;
}
	
OP_STATUS WebResource_Index::PrintSubServers(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize, PrintState &printState)
{
	unsigned int strLength;
	int n = g_webserver->GetSubServerCount();
	
	/* FIXME: if a subserver is closed while we are serving this, a subserver might be printed twice*/
	FilledDataSize = 0;
	WebSubServer *subServer;
	while (m_currentSubServer < n)
	{
		subServer = g_webserver->GetSubServerAtIndex(m_currentSubServer);
		
		const OpStringC16 subserverName = subServer->GetSubServerName();
		const OpStringC16 subServerVirtualPath = subServer->GetSubServerVirtualPath();
		
		if (subServerVirtualPath == UNI_L("") || subServerVirtualPath == UNI_L("/"))
		{
			m_currentSubServer++;	
			continue;
		}
		
		OpString8 utf8SubServerName;
		OpString8 utf8SubServerVirtualPath;
		OpString8 utf8SubServerAuthor;
		OpString8 utf8SubServerDescription;
		
		RETURN_IF_ERROR(utf8SubServerName.SetUTF8FromUTF16(subServer->GetSubServerName().CStr()));
				
		RETURN_IF_ERROR(utf8SubServerVirtualPath.SetUTF8FromUTF16(subServer->GetSubServerVirtualPath().CStr()));
		int nbEscapes = 0;
		
	#ifdef FORMATS_URI_ESCAPE_SUPPORT
		if (( nbEscapes = UriEscape::CountEscapes(utf8SubServerVirtualPath.CStr(), UriEscape::StandardUnsafe | UriEscape::Percent)) > 0)
	#else
		if (( nbEscapes = CountEscapes(utf8SubServerVirtualPath, utf8SubServerVirtualPath.Length())) > 0)
	#endif
		
		{
			char *escapedUrl = OP_NEWA(char, utf8SubServerVirtualPath.Length() + nbEscapes*5 + 1);
			if (escapedUrl == NULL)
			{
				return OpStatus::ERR_NO_MEMORY;
			}
			
		#ifdef FORMATS_URI_ESCAPE_SUPPORT
			UriEscape::Escape(escapedUrl, utf8SubServerVirtualPath.CStr(), UriEscape::StandardUnsafe | UriEscape::Percent);
		#else
			 EscapeURL(escapedUrl, utf8SubServerVirtualPath);
		#endif
	
			
			OP_STATUS status = utf8SubServerVirtualPath.Set(escapedUrl);
			OP_DELETEA(escapedUrl);			
			RETURN_IF_ERROR(status);
		}

		RETURN_IF_ERROR(utf8SubServerAuthor.SetUTF8FromUTF16(subServer->GetSubServerAuthor().CStr()));
		RETURN_IF_ERROR(utf8SubServerDescription.SetUTF8FromUTF16(subServer->GetSubServerDescription().CStr()));

		strLength = op_snprintf(buffer+FilledDataSize, bufferSize-FilledDataSize, m_subServerHtmlFormat, utf8SubServerVirtualPath.CStr(), utf8SubServerName.CStr(),utf8SubServerDescription.CStr(),  "", utf8SubServerAuthor.CStr());
		if ( strLength >= (bufferSize - FilledDataSize) )
		{	
			break;	
		}
		else
		{
			FilledDataSize += strLength;
			m_currentSubServer++;
		}		
	}

	if (m_currentSubServer == n)
	{
		printState = PRINTSTATE_EOF;
		return OpStatus::OK;
	}
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS WebResource_Index::HandleIncommingData(const char *incommingData, UINT32 length, BOOL lastData)
{
	OP_ASSERT(!"NOT IMPLEMENTED IN THIS RESOURCE");
	return OpStatus::ERR;
}

/*virtual*/ 
OP_BOOLEAN WebResource_Index::FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{
  	switch (m_printState) {
		case PRINTSTATE_HEAD:
			FilledDataSize = op_snprintf(buffer, bufferSize, m_headFormat);
			m_printState = PRINTSTATE_INIT;
			break;
		case PRINTSTATE_INIT:
			FilledDataSize = op_snprintf(buffer, bufferSize, m_initFormat);
			m_printState = PRINTSTATE_STARTSUBSERVERS;
			break;
		case PRINTSTATE_STARTSUBSERVERS:
		case PRINTSTATE_SUBSERVES:
			RETURN_IF_ERROR(PrintSubServers(buffer, bufferSize, FilledDataSize, m_printState));
			break;
		case PRINTSTATE_EOF:
			FilledDataSize = op_snprintf(buffer, bufferSize, m_EOFformat);	
			m_printState = PRINTSTATE_STOP;
			break;
		default:
		case PRINTSTATE_STOP:
			return OpBoolean::IS_FALSE;
			break;
	}
  	return OpBoolean::IS_TRUE;
}

#endif //WEBSERVER_SUPPORT && NATIVE_ROOT_SERVICE_SUPPORT
