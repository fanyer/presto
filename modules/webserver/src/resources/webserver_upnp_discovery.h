/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation
 */
 
#ifndef WEBSERVER_UPNP_DISCOVERY_H_
#define WEBSERVER_UPNP_DISCOVERY_H_
#if defined(WEBSERVER_SUPPORT) && defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/webresource_base.h"
#include "modules/upnp/upnp_upnp.h"
#include "modules/util/adt/bytebuffer.h"

class WebServerConnection;
class WebServer;

/**
  Class that provide the XML used in the UPnP discovery phase, effectively providing the list of UPnP services (== Opera Unite Services, in this context) available
*/
class WebResource_UPnP_Service_Discovery : public WebResource_Administrative_Standard
{
public:
 
	/* Create the resource */
	static OP_STATUS Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Base *resource, WebServerFailure *result, OpString8 challenge);
	
	WebResource_UPnP_Service_Discovery(WebServerConnection *service, WebserverRequest *requestObject): WebResource_Administrative_Standard(service, requestObject) { }
};

#endif // defined(WEBSERVER_SUPPORT) && defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
#endif /*WEBSERVER_FILE_H_*/
