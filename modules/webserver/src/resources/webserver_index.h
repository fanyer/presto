/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006
 *
 */

#ifndef WEBSERVER_INDEX_H_
#define WEBSERVER_INDEX_H_

#ifdef NATIVE_ROOT_SERVICE_SUPPORT
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webresource_base.h"

class WebServerConnection;

class WebResource_Index : public WebResource
{
public:
	static WebResource_Index *Make(WebServerConnection *service, WebserverRequest *requestObject);


	virtual ~WebResource_Index();
		/**< Close the file */

	virtual OP_BOOLEAN FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize);
		/**< Serve data from the open file, until EOF */

	virtual OP_STATUS HandleIncommingData(const char *incommingData,UINT32 length,BOOL lastData=FALSE);

	virtual OP_BOOLEAN GetLastModified(time_t &date) { date = 0; return OpStatus::OK; }
	
	WebResource_Index(WebServerConnection *service, WebserverRequest *requestObject);
	
	virtual OP_STATUS OnConnectionClosed(){ return OpStatus::OK; }	
private:
		
	enum PrintState
	{
		PRINTSTATE_HEAD,
		PRINTSTATE_INIT,
		PRINTSTATE_STARTSUBSERVERS,
		PRINTSTATE_SUBSERVES,
		PRINTSTATE_EOF,
		PRINTSTATE_STOP
	} m_printState;



	OP_STATUS PrintSubServers(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize, PrintState &printState);
	
	int m_currentSubServer;
	
	const char *m_headFormat;
	const char *m_initFormat;
	const char *m_subServerHtmlFormat;
	const char *m_EOFformat;
};

#endif // NATIVE_ROOT_SERVICE_SUPPORT

#endif // WEBSERVER_INDEX_H_

