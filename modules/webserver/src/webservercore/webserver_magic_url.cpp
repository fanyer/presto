/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2008-2012
 *
 * Web server implementation 
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/webserver_magic_url.h"
#include "modules/webserver/webserver-api.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/url/url2.h"
#include "modules/forms/form.h"
#include "modules/windowcommander/src/TransferManager.h"
#include "modules/webserver/webserver_request.h"
#include "modules/util/str.h"
#include "modules/formats/uri_escape.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"

WebserverAuthenticationId_MagicUrl::WebserverAuthenticationId_MagicUrl()
	: WebserverAuthenticationId()
{
	
}

/* static */ OP_STATUS WebserverAuthenticationId_MagicUrl::Make(WebserverAuthenticationId_MagicUrl *&auhtentication_object, const OpStringC &auth_id)
{	
	auhtentication_object = NULL;
	OpAutoPtr<WebserverAuthenticationId_MagicUrl> temp(OP_NEW(WebserverAuthenticationId_MagicUrl, ()));
	RETURN_OOM_IF_NULL(temp.get());

	RETURN_IF_ERROR(WebserverUserManager::GenerateRandomTextString(temp->m_auth_data, 24));

	RETURN_IF_ERROR(temp->SetAuthId(auth_id.CStr()));
	
	auhtentication_object = temp.release();
	return OpStatus::OK;
}

/* static */ OP_STATUS WebserverAuthenticationId_MagicUrl::MakeFromXML(WebserverAuthenticationId_MagicUrl *&auhtentication_object, XMLFragment &auth_element)
{
	auhtentication_object = NULL;
	OpAutoPtr<WebserverAuthenticationId_MagicUrl> temp(OP_NEW(WebserverAuthenticationId_MagicUrl, ()));
	RETURN_OOM_IF_NULL(temp.get());

	RETURN_IF_ERROR(temp->m_auth_data.Set(auth_element.GetAttribute(UNI_L("data"))));
	
	auhtentication_object = temp.release();
	return OpStatus::OK;
}


OP_STATUS WebserverAuthenticationId_MagicUrl::GetMagicUrlString(OpString &magic_url, const WebSubServer *subserver, const OpStringC &service_sub_path_and_args)
{
	OP_ASSERT(GetUser() != NULL && GetAuthId() != NULL);
	
	if (GetUser() == NULL || GetAuthId() == NULL)
		return OpStatus::ERR;
	
	magic_url.Wipe();
	OpString encoded_virtual_path;
	const uni_char *virtual_path = subserver->GetSubServerVirtualPath().CStr();
	
	RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(virtual_path, encoded_virtual_path));

	OpString encoded_subpath;
	RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(service_sub_path_and_args, encoded_subpath));
	
	OpString escape_spliters_encoded_subpath;
	
#ifdef FORMATS_URI_ESCAPE_SUPPORT
	RETURN_IF_ERROR(UriEscape::AppendEscaped(escape_spliters_encoded_subpath, encoded_subpath.CStr(), UriEscape::QueryUnsafe));
#else
	RETURN_IF_ERROR(WebResource::EscapeItemSplitters(encoded_subpath, UNI_L("=?&"), escape_spliters_encoded_subpath));
#endif
	
	
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT	
	return magic_url.AppendFormat(UNI_L("http://%s.%s.%s/_root/authresource/?uniteauth.id=%s&uniteauth.auth=%s&uniteauth.type=%s&uniteauth.path=/%s/%s"),
	                       		g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice).CStr(),
	                       		g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser).CStr(),
	                       		g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverProxyHost).CStr(),
	                       		GetAuthId(),
	                       		m_auth_data.CStr(),
	                       		AuthTypeToString(GetAuthType()),
	                       		encoded_virtual_path.CStr(),
	                       		escape_spliters_encoded_subpath.CStr()
	                       	);
#else
	return magic_url.AppendFormat(UNI_L("http://localhost:%d/_root/authresource/?uniteauth.id=%s&uniteauth.auth=%s&uniteauth.type=%s&uniteauth.path=/%s/%s"),
	                            g_webserver->GetLocalListeningPort(),
	                       		GetAuthId(),
	                       		m_auth_data.CStr(),
	                       		AuthTypeToString(GetAuthType()),
	                       		encoded_virtual_path.CStr(),
	                       		escape_spliters_encoded_subpath.CStr()
	                       	);
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
}

OP_STATUS WebserverAuthenticationId_MagicUrl::CheckAuthentication(WebserverRequest *request_object)
{
	OpString auth;
	OP_BOOLEAN found;
	unsigned int i = 0;
	RETURN_IF_ERROR(found = request_object->GetItemInQuery(UNI_L("uniteauth.auth"), auth, i, FALSE));
	if (found == OpBoolean::IS_TRUE)
		RETURN_IF_ERROR(CreateSessionAndSendCallback(auth == m_auth_data ? SERVER_AUTH_STATE_SUCCESS : SERVER_AUTH_STATE_FAILED));
	else
		RETURN_IF_ERROR(CreateSessionAndSendCallback(SERVER_AUTH_STATE_FAILED));
	
	return OpBoolean::OK;	
}

/* virtual */ OP_STATUS WebserverAuthenticationId_MagicUrl::CreateAuthXML(XMLFragment &auth_element)
{
	RETURN_IF_ERROR(auth_element.SetAttribute(UNI_L("data"), m_auth_data.CStr()));
	return OpStatus::OK;
} 

#endif // WEBSERVER_SUPPORT
