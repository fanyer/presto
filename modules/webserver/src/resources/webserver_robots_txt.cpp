/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation 
 */

#include "core/pch.h"

#if defined(WEBSERVER_SUPPORT)

#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/src/resources/webserver_robots_txt.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/datefun.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"

/* static */ OP_STATUS
WebResource_Robots::Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Base *resource, WebServerFailure *result)
{
	webresource = NULL;
	*result=WEB_FAIL_GENERIC;
	
	const uni_char *request_resource_name = subServerRequestString.CStr();
	
	while (*request_resource_name == '/')
		request_resource_name++;
	
	const uni_char *resource_descriptor_name = resource->GetResourceVirtualPath();
	while (*resource_descriptor_name == '/')
		resource_descriptor_name++;
	
	WebResource_Robots *ptr=OP_NEW(WebResource_Robots, (service, requestObject));
	
	if(!ptr)
	{
		*result=WEB_FAIL_OOM;
		
		return OpStatus::ERR_NO_MEMORY;
	}
	
	OpAutoPtr<WebResource_Robots> robotsResource_anchor(ptr);
		
	int num_services=g_webserver->GetSubServerCount();
	
	ptr->buf.AppendString("User-agent: *\n");
	ptr->buf.AppendString("Crawl-delay: 3\n");
	
	// Allow the services that should be presented in robots.txt
	// Root service
	ManageSubServer(ptr->buf, g_webserver->GetRootSubServer());
	
	// All the other services
	for(int i=0; i<num_services; i++)
		RETURN_IF_ERROR(ManageSubServer(ptr->buf, g_webserver->GetSubServerAtIndex(i)));
	
	// Disallow should be after Allow, for compatibility reasons
	ptr->buf.AppendString("Disallow: /\n");
	
	OpString len;
	
	RETURN_IF_ERROR(len.AppendFormat(UNI_L("%d"), ptr->buf.Length()));
	RETURN_IF_ERROR(ptr->AddResponseHeader(UNI_L("Content-Type"), UNI_L("text/plain")));
	RETURN_IF_ERROR(ptr->AddResponseHeader(UNI_L("Content-Length"), len));
	
	RETURN_IF_ERROR(service->StartServingData());
	
	webresource = robotsResource_anchor.release();
	
	*result=WEB_FAIL_NO_FAILURE;
	
	return OpStatus::OK;
}

OP_STATUS WebResource_Robots::ManageSubServer(ByteBuffer &buf, WebSubServer *sub_server)
{
	if(sub_server && sub_server->IsVisibleRobots())
	{
		OpString8 tmp;
		RETURN_IF_ERROR(buf.AppendString("Allow: /"));
		
		RETURN_IF_ERROR(tmp.Set(sub_server->GetSubServerVirtualPath().CStr()));
		
		RETURN_IF_ERROR(buf.AppendString(tmp.CStr()));
		RETURN_IF_ERROR(buf.AppendString("\n"));
	}
	
	return OpStatus::OK;
}
#endif // defined(WEBSERVER_SUPPORT)
