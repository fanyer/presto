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
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/webserver/src/resources/webserver_asd.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpLocale.h"
#include "modules/util/datefun.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"
#include "modules/xmlutils/xmlnames.h"


/* static */ OP_STATUS
WebResource_ASD::Make(WebResource *&webresource, WebServerConnection *service, const OpStringC16 &subServerRequestString, WebserverRequest *requestObject, WebserverResourceDescriptor_Base *resource, WebServerFailure *result)
{
	webresource = NULL;
	*result=WEB_FAIL_GENERIC;
	
	const uni_char *request_resource_name = subServerRequestString.CStr();
	
	while (*request_resource_name == '/')
		request_resource_name++;


	// Only possible path: "services"
	if (uni_strncmp(request_resource_name, UNI_L("services"), 8) )
		return OpStatus::ERR_NO_SUCH_RESOURCE;

	OpAutoPtr<WebResource_ASD> ptr(OP_NEW(WebResource_ASD, (service, requestObject)));
	
	if(!ptr.get())
	{
		*result=WEB_FAIL_OOM;
		
		return OpStatus::ERR_NO_MEMORY;
	}
	
	OP_STATUS ops=ptr->CreateServicesXML();
	
	if(OpStatus::IsSuccess(ops))
	{
		
		OpString len;
	
		RETURN_IF_ERROR(len.AppendFormat(UNI_L("%d"), ptr->buf.Length()));
		RETURN_IF_ERROR(ptr->AddResponseHeader(UNI_L("Content-Type"), UNI_L("text/xml")));
		RETURN_IF_ERROR(ptr->AddResponseHeader(UNI_L("Content-Length"), len));
		
		RETURN_IF_ERROR(service->StartServingData());

		webresource = ptr.release();
		*result=WEB_FAIL_NO_FAILURE;
	}

	
	return ops;
}

OP_STATUS WebResource_ASD::CreateServicesXML()
{
	int num_services=g_webserver->GetSubServerCount();
	int num_visible_services=0;
	OpString user;
	OpString dev;
	OpString proxy;
	OpString full_name;
	
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	RETURN_IF_ERROR(user.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser)));
	RETURN_IF_ERROR(dev.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice)));
	RETURN_IF_ERROR(proxy.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost)));
	RETURN_IF_ERROR(full_name.AppendFormat(UNI_L("%s.%s.%s"), dev.CStr(), user.CStr(), proxy.CStr()));
#else
	RETURN_IF_ERROR(full_name.Set("UnknownName"));
#endif

	XMLFragment xf;

	// Count the visible services
	for(int i=0; i<num_services; i++)
	{
		WebSubServer *srv=g_webserver->GetSubServerAtIndex(i);
		
		if(srv && srv->IsVisibleASD())
			num_visible_services++;
	}

	RETURN_IF_ERROR(xf.OpenElement(UNI_L("services")));
	RETURN_IF_ERROR(xf.SetAttributeFormat(UNI_L("length"), UNI_L("%d"), num_visible_services));
	RETURN_IF_ERROR(xf.SetAttributeFormat(UNI_L("total"), UNI_L("%d"), num_visible_services));

	// Write all the services
	for(int i=0; i<num_services; i++)
	{
		WebSubServer *srv=g_webserver->GetSubServerAtIndex(i);

		OP_ASSERT(srv);

		if(srv && srv->IsVisibleASD())
		{
			RETURN_IF_ERROR(xf.OpenElement(UNI_L("service")));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("access"), UNI_L("public")));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("author"), srv->GetSubServerAuthor().CStr()));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("description"), srv->GetSubServerDescription().CStr()));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("device"), dev.CStr()));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("name"), srv->GetSubServerName().CStr()));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("onlinefor"), UNI_L("")));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("originurl"), srv->GetSubServerOriginUrl().CStr()));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("path"), srv->GetSubServerVirtualPath().CStr()));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("server"), full_name.CStr()));
			RETURN_IF_ERROR(xf.SetAttribute(UNI_L("username"), user.CStr()));

			xf.CloseElement();  // Service
		}
	}

	xf.CloseElement();  // Services

	XMLFragment::GetXMLOptions xml_opt(TRUE);

	xml_opt.encoding="UTF-8";
	//xml_opt.canonicalize=XMLFragment::GetXMLOptions::CANONICALIZE_WITH_COMMENTS;

	return xf.GetEncodedXML(buf, xml_opt);
}

#endif // defined(WEBSERVER_SUPPORT)
