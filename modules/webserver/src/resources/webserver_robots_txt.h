/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation
 */
 
#ifndef WEBSERVER_ROBOTS_H_
#define WEBSERVER_ROBOTS_H_
#if defined(WEBSERVER_SUPPORT)

#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webresource_base.h"

class WebServerConnection;
class WebServer;

/**
  This class provide a robots.txt file for blocking Search Engines
*/
class WebResource_Robots : public WebResource_Administrative_Standard
{
private:
	static OP_STATUS ManageSubServer(ByteBuffer &buf, WebSubServer *sub_server);
	
public:
	/* Create the resource */
	static OP_STATUS Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Base *resource, WebServerFailure *result);
	
	WebResource_Robots(WebServerConnection *service, WebserverRequest *requestObject): WebResource_Administrative_Standard(service, requestObject) { }
};

#endif // defined(WEBSERVER_SUPPORT)
#endif /*WEBSERVER_FILE_H_*/
