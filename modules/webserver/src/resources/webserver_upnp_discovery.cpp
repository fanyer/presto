/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation 
 */

#include "core/pch.h"

#if defined(WEBSERVER_SUPPORT) && defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)

#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/webserver/src/resources/webserver_upnp_discovery.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/datefun.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"

/* static */ OP_STATUS
WebResource_UPnP_Service_Discovery::Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Base *resource, WebServerFailure *result, OpString8 challenge)
{
	webresource = NULL;
	*result=WEB_FAIL_GENERIC;
	
	const uni_char *request_resource_name = subServerRequestString.CStr();
	
	while (*request_resource_name == '/')
		request_resource_name++;
	
	//const uni_char *resource_descriptor_name = resource->GetResourceVirtualPath();
	//while (*resource_descriptor_name == '/')
	//	resource_descriptor_name++;
	
	WebResource_UPnP_Service_Discovery *ptr;
	BOOL is_desc=FALSE;
	
	if (!uni_strncmp(request_resource_name, UNI_L("desc"), 4) ) // Description
	{
		ptr=OP_NEW(WebResource_UPnP_Service_Discovery, (service, requestObject));
		is_desc=TRUE;
	}
	else if (!uni_strncmp(request_resource_name, UNI_L("scpd"), 4) ) // SCPDURL
		ptr=OP_NEW(WebResource_UPnP_Service_Discovery, (service, requestObject));
	else
	{
		*result=WEB_FAIL_FILE_NOT_AVAILABLE;
		
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	}
		
	if(!ptr)
	{
		*result=WEB_FAIL_OOM;
		
		return OpStatus::ERR_NO_MEMORY;
	}
	
	OpAutoPtr<WebResource_UPnP_Service_Discovery> upnpResource_anchor(ptr);
	
	// FOr now on, changes have to be notified
	service->GetConnectionLister()->SetEnableUPnPUpdates(TRUE);
		
	if(is_desc)
		RETURN_IF_ERROR(service->GetConnectionLister()->CreateDescriptionXML(ptr->buf, challenge));
	else
		RETURN_IF_ERROR(service->GetConnectionLister()->CreateSCPD(ptr->buf));
	
	OpString len;
	
	RETURN_IF_ERROR(len.AppendFormat(UNI_L("%d"), ptr->buf.Length()));
	RETURN_IF_ERROR(ptr->AddResponseHeader(UNI_L("Content-Type"), UNI_L("text/xml")));
	RETURN_IF_ERROR(ptr->AddResponseHeader(UNI_L("Content-Length"), len));
	
	RETURN_IF_ERROR(service->StartServingData());
	
	webresource = upnpResource_anchor.release();
	
	*result=WEB_FAIL_NO_FAILURE;
	
	return OpStatus::OK;
}

#endif // defined(WEBSERVER_SUPPORT) && defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
