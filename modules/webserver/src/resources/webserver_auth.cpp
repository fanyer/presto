/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2012
 *
 * Web server implementation 
 */



#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver-api.h"

#include "modules/webserver/webserver_resources.h"
#include "modules/formats/base64_decode.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/upload/upload.h"
#include "modules/util/opautoptr.h"
#include "modules/auth/auth_digest.h"
#include "modules/formats/hdsplit.h" 
#include "modules/formats/argsplit.h"
#include "modules/formats/uri_escape.h"
#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"
#include "modules/webserver/webserver_user_management.h"		 
#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/webserver/webserver_openid.h"

WebResource_Auth::WebResource_Auth(WebServerConnection *service, WebserverRequest *requestObject, WebserverResourceDescriptor_AccessControl *authElement, WebSubServerAuthType auth_type)
	: 	WebResource(service, requestObject)
	, 	m_post_data_length(0)
	,  	m_auth_type(auth_type)
	,	m_authElement(authElement)
	, 	m_authentication_object(NULL)
{
	OP_ASSERT(authElement != NULL);
}

WebResource_Auth::~WebResource_Auth()
{
	if (m_authentication_object)
		m_authentication_object->AccessControlResourceKilled();
}
	
/*static*/ 
OP_BOOLEAN WebResource_Auth::Make(WebResource *&webResource, const OpStringC16 &subServerRequestString, WebServerConnection *service, WebserverRequest *requestObject, WebserverResourceDescriptor_AccessControl *authElement, WebServerFailure *result)
{
	webResource = NULL;
	OP_ASSERT(service != NULL);
	OP_ASSERT(requestObject != NULL);
	OP_ASSERT(authElement != NULL);
	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	
	*result = WEB_FAIL_NOT_AUTHENTICATED;
	
	OpString normalizedAuthPath;

	RETURN_IF_ERROR(WebserverFileName::SecureURLPathConcat(normalizedAuthPath, authElement->GetResourceVirtualPath()));
	
	const uni_char *normAuthPathPointer = normalizedAuthPath.CStr();
	
	
	unsigned int resourceVirtualPathLength = uni_strlen(normAuthPathPointer);
	
	while ( resourceVirtualPathLength > 1 && normAuthPathPointer[resourceVirtualPathLength-1] == '/' )
	{
		resourceVirtualPathLength--;
	}	
	
	while (resourceVirtualPathLength > 0 && normAuthPathPointer[0] == '/' )
	{
	 	normAuthPathPointer++;
		resourceVirtualPathLength--;
	}

	const uni_char *requestString = subServerRequestString.CStr();
	
	while (requestString[0] == '/')
	{
	 	requestString++;
	}	
	
	if (uni_strncmp(requestString, normAuthPathPointer, resourceVirtualPathLength) != 0)
	{
		*result = WEB_FAIL_NO_FAILURE;
		return OpBoolean::IS_TRUE; 
	}
	
	if (uni_strlen(normAuthPathPointer) != 0 && uni_strlen(requestString) > resourceVirtualPathLength && requestString[resourceVirtualPathLength] != '/' && requestString[resourceVirtualPathLength] != ' ' && requestString[resourceVirtualPathLength] != '?' && requestString[resourceVirtualPathLength] != '#')
	{
		*result = WEB_FAIL_NO_FAILURE;
		return OpBoolean::IS_TRUE; 
	}
	
	HeaderList *headerList = requestObject->GetClientHeaderList();

	
	if (headerList == NULL)
	{
		OP_ASSERT(!"headerList cannot be NULL");
		*result = WEB_FAIL_NOT_AUTHENTICATED;
		
		return OpStatus::ERR;
	}
	

	BOOL is_root_service = FALSE;
	WebSubServer *subserver;
	if (NULL!=(subserver = g_webserver->GetSubServer(requestObject->GetSubServerVirtualPath())) && subserver->IsRootService())
	{
		is_root_service = TRUE;
	}
	
	BOOL authenticated_but_no_access = FALSE;
	/* 1) First check authentication */	
	WebResource_Auth *authResource = NULL;
	WebSubServerAuthType auth_type; 
	RETURN_IF_ERROR(requestObject->DetectAuthenticationType(auth_type));	

	if (auth_type == WEB_AUTH_TYPE_HTTP_MAGIC_URL)
	{
		/* FIXME:  move this into g_webserverUserManager */
		/******* magic url authentication */	
		/* generate response with cookie, redirect and return */
		
		OpString auth_id;
		unsigned int index = 0;

		RETURN_IF_ERROR(requestObject->GetItemInQuery( UNI_L("uniteauth.id"), auth_id, index, TRUE));

		WebserverAuthUser *user_object = g_webserverUserManager->GetUserFromAuthId(auth_id, WEB_AUTH_TYPE_HTTP_MAGIC_URL);
		
		if (user_object != NULL)
		{
			service->PauseServingData();

			if ((authResource = OP_NEW(WebResource_Auth, (service, requestObject, authElement, auth_type))) == NULL)
				return OpStatus::ERR_NO_MEMORY;
			
	
			if (OpStatus::IsError(status = user_object->CheckAuthentication(requestObject, auth_id, authResource, WEB_AUTH_TYPE_HTTP_MAGIC_URL)))
			{
				OP_DELETE(authResource);
				return status;
			}

			*result = WEB_FAIL_NOT_AUTHENTICATED;	 /* not yet authenticated */
			webResource = authResource;
			return OpBoolean::IS_FALSE;	
		}
	}		
	else if (auth_type == WEB_AUTH_TYPE_HTTP_OPEN_ID )
	{
		WebServerMethod method = requestObject->GetHttpMethod();
		if (method == WEB_METH_POST)
		{
			HeaderEntry *contentTypeHeader = requestObject->GetClientHeaderList()->GetHeader("Content-Type");

			if (contentTypeHeader == NULL|| op_stristr(contentTypeHeader->Value(), "application/x-www-form-urlencoded") == NULL)
			{
				return OpStatus::ERR;
			}
	
			/* we have to wait for the post data. */
			service->PauseServingData();

			if ((authResource = OP_NEW(WebResource_Auth, (service, requestObject, authElement, auth_type))) == NULL)
				return OpStatus::ERR_NO_MEMORY;

				
			service->AcceptReceivingBody(TRUE);
			*result = WEB_FAIL_NOT_AUTHENTICATED;	 /* not yet authenticated */
			webResource = authResource;
			return OpBoolean::IS_FALSE;	
		}
		else
		{
			OpString auth_id;
			unsigned int index = 0;
		
			RETURN_IF_ERROR(requestObject->GetItemInQuery( UNI_L("openid.identity"), auth_id, index, TRUE));
			WebserverAuthUser *user_object = g_webserverUserManager->GetUserFromAuthId(auth_id, WEB_AUTH_TYPE_HTTP_OPEN_ID);
			/* FixMe: Create a dummy user, if user is not found */
			if (user_object != NULL)
			{
				service->PauseServingData();
	
				if ((authResource = OP_NEW(WebResource_Auth, (service, requestObject, authElement, auth_type))) == NULL)
					return OpStatus::ERR_NO_MEMORY;
				
		
				if (OpStatus::IsError(status = user_object->CheckAuthentication(requestObject, auth_id, authResource, WEB_AUTH_TYPE_HTTP_OPEN_ID)))
				{
					OP_DELETE(authResource);
					return status;
				}
	
				*result = WEB_FAIL_NOT_AUTHENTICATED; /* not yet authenticated */
				webResource = authResource;
				return OpBoolean::IS_FALSE;	
			}
		}
	}

/* Check session authentication and access control */	
	if (auth_type == WEB_AUTH_TYPE_NONE)
	{
		const uni_char *username = requestObject->GetAuthenticatedUsername(); /* will only return username if the request has a valid session key. */
		
		if (username)
		{
			if (authElement->UserHasAccess(username) || is_root_service == TRUE)
			{								
				*result = WEB_FAIL_NO_FAILURE;
				return OpBoolean::IS_TRUE;
			}
			else
			{
				authenticated_but_no_access = TRUE;
			}
		}
	}
	
	/* redirect to login page */
 	OpString unite_return_path;
	OpString reason;
	OpString path_item;
	if (auth_type  == WEB_AUTH_TYPE_NONE)
	{
		OpString escaped_request;
		RETURN_IF_ERROR(WebResource_Auth::RemoveUniteAuthItems(requestObject, escaped_request));
		
	#ifdef FORMATS_URI_ESCAPE_SUPPORT
		RETURN_IF_ERROR(UriEscape::AppendEscaped(unite_return_path, escaped_request.CStr(), UriEscape::QueryUnsafe));
	#else
		RETURN_IF_ERROR(WebResource::EscapeItemSplitters(escaped_request, UNI_L("=?&"), unite_return_path));
	#endif
		
	}
	else
	{
		unsigned int index;
		RETURN_IF_ERROR(requestObject->GetItemInQuery(UNI_L("uniteauth.path"), unite_return_path, index = 0, FALSE));
	}
	
	
	if (auth_type  == WEB_AUTH_TYPE_NONE)
	{
		if (authenticated_but_no_access)
		{
			// Had a valid session but did not have access.
			RETURN_IF_ERROR(reason.Set("no_access"));
		}
		else
		{
			// Did not have a valid session.
			RETURN_IF_ERROR(reason.Set("not_authenticated"));
		}
	} 
	else
	{
		// Tried to authenticate but failed.
		RETURN_IF_ERROR(reason.Set("auth_failed"));
	}

	 	
	authResource = OP_NEW(WebResource_Auth,(service, requestObject, authElement, auth_type));
	status = OpStatus::ERR_NO_MEMORY;
	if (
			authResource == NULL || 
			OpStatus::IsError(status = authResource->RedirectToLoginPage(unite_return_path, auth_type, reason))
		)
	{
		OP_DELETE(authResource);
		return status;
	}
	
	webResource = authResource;
	return OpBoolean::IS_FALSE;		
}

OP_STATUS WebResource_Auth::StartServingData()
{
	return m_service->StartServingData();
}

OP_STATUS WebResource_Auth::Authenticated(WebserverAuthenticationId::AuthState success, WebserverAuthSession *session, WebSubServerAuthType auth_type)
{
	OpString path;
	unsigned int index;
	if (OpBoolean::IS_TRUE != m_requestObject->GetItemInQuery(UNI_L("uniteauth.path"), path, index = 0, FALSE))
	{
		RETURN_IF_ERROR(path.Set("/_root/authresource"));	
	}
	
	
	OP_STATUS status = OpStatus::OK;
	
	switch (success)
	{
		case WebserverAuthenticationId::SERVER_AUTH_STATE_SUCCESS:
		{
		#ifdef FORMATS_URI_ESCAPE_SUPPORT
			UriUnescape::ReplaceChars(path.CStr(), UriUnescape::LocalfileUrlUtf8);
		#else
			int length = path.Length();
			
			ReplaceEscapedCharsUTF8(path.CStr(), length, OPSTR_LOCALFILE_URL);
			path.CStr()[length] = '\0';
		#endif

			if (session && OpStatus::IsSuccess(status = GenerateCookieSessionKeyResponse(session, path)))
				return StartServingData();
			else
				status = OpStatus::ERR;
			
			break;
		}
		
		case WebserverAuthenticationId::SERVER_AUTH_STATE_FAILED:
		{
			return RedirectToLoginPage(path, auth_type, UNI_L("auth_failed"));
		}
		
		case WebserverAuthenticationId::SERVER_AUTH_STATE_ANOTHER_AGENT_IS_AUTHENTICATING:
		{
			return RedirectToLoginPage(path, auth_type, UNI_L("in_progress"));
		}
		
		case WebserverAuthenticationId::SERVER_AUTH_STATE_AUTH_SERVER_ERROR:
		{
			return RedirectToLoginPage(path, auth_type, UNI_L("server_error"));
		}

		default:
			OP_ASSERT(!"unhandled AuthState");
			status = OpStatus::ERR;
	}
	
	
	return status;
}

OP_STATUS WebResource_Auth::RedirectToLoginPage(const OpStringC &auth_path, WebSubServerAuthType auth_type, const OpStringC &reason)
{
	/* redirect to login page */
	OpString location;
	
	RETURN_IF_ERROR(location.AppendFormat(UNI_L("/_root/login/?")));
	
	if (auth_path.HasContent())
	{
		RETURN_IF_ERROR(location.AppendFormat(UNI_L("uniteauth.path=%s&"), auth_path.CStr()));
	}
	
	if (reason.HasContent())
	{
		RETURN_IF_ERROR(location.AppendFormat(UNI_L("uniteauth.reason=%s&"), reason.CStr()));
	}
	
	RETURN_IF_ERROR(location.AppendFormat(UNI_L("uniteauth.type=%s"), WebserverAuthenticationId::AuthTypeToString(auth_type)));

	OpString url_encoded_path;
	RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(location, url_encoded_path));
	RETURN_IF_ERROR(AddResponseHeader(UNI_L("Location"), url_encoded_path));
	
	RETURN_IF_ERROR(SetResponseCode(302));
	RETURN_IF_ERROR(m_service->StartServingData());

	return OpBoolean::OK;
}

OP_STATUS WebResource_Auth::GenerateCookieSessionKeyResponse(WebserverAuthSession *session, OpString &location)
{
	OpString auth_cookie_value;
	
	RETURN_IF_ERROR(auth_cookie_value.Append("unite-session-auth="));
	RETURN_IF_ERROR(auth_cookie_value.Append(session->GetSessionAuth()));	
	RETURN_IF_ERROR(auth_cookie_value.Append("; path=/")); 
	
	RETURN_IF_ERROR(AddResponseHeader(UNI_L("Set-Cookie"), auth_cookie_value.CStr(), TRUE, FALSE, FALSE));
	
	OpString url_encoded_path;
	RETURN_IF_ERROR(WebserverRequest::UrlEncodeString(location, url_encoded_path));
	RETURN_IF_ERROR(AddResponseHeader(UNI_L("Location"), url_encoded_path));
	
	RETURN_IF_ERROR(SetResponseCode(302)); 
	RETURN_IF_ERROR(m_service->StartServingData());
	
	return OpStatus::OK;
}


/* static */ OP_STATUS WebResource_Auth::RemoveUniteAuthItems(WebserverRequest *requestObject, OpString &request_string)
{
	const uni_char *request = requestObject->GetOriginalRequestUni();
	while (*request == '/' && *(request + 1) == '/') 
		request++;
	
	const uni_char *query = uni_strchr(request, '?');
	
	if (query)
		request_string.Set(request, query  - request);
	else
		request_string.Set(request);

	OpAutoStringHashTable<OpAutoStringVector> item_collection;
	RETURN_IF_ERROR(requestObject->GetAllItemsInQuery(item_collection, FALSE));
	
	OpHashIterator*	iter = item_collection.GetIterator();

	OP_STATUS ret = iter->First();
	OpStringVector *items;
	uni_char div = '?';
	while(ret == OpStatus::OK)
	{
		if ((items = static_cast<OpStringVector*>(iter->GetData())) != NULL)
		{
			if (uni_strncmp(items->GetVectorName(), "openid.", 7) && uni_strncmp(items->GetVectorName(), "uniteauth.", 10))
			{
				for (unsigned i = 0; i < items->GetCount(); i++)
				{				
					if (OpStatus::IsError(ret = request_string.AppendFormat(UNI_L("%c%s=%s"), div, items->GetVectorName(), items->Get(i)->CStr())))
					{
						OP_DELETE(iter);
						return ret;
					}
					div = '&';
				}
			}
		}
		ret = iter->Next();
	}
	OP_DELETE(iter);	

	return OpStatus::OK;
}

/*virtual*/ 
OP_STATUS WebResource_Auth::HandleIncommingData(const char *incommingData, unsigned length, BOOL lastData)
{
	if (m_auth_type != WEB_AUTH_TYPE_HTTP_OPEN_ID)
		return OpStatus::ERR;
	
	m_post_data_length += length;
	if (m_post_data_length > 100)
	{
		Authenticated(WebserverAuthenticationId::SERVER_AUTH_STATE_FAILED , NULL, m_auth_type);		
		return OpStatus::ERR;
	}
	
	OpString data;
	RETURN_IF_ERROR(data.SetFromUTF8(incommingData, length));
	
	RETURN_IF_ERROR(m_auth_post_data.Append(data));
	WebserverUserManager *user_manager = g_webserverUserManager;
	if (lastData)
	{
		unsigned int index;
		BOOL found;
		OpString openid_url;
		
	#ifdef FORMATS_URI_ESCAPE_SUPPORT
		UriUnescape::ReplaceChars(m_auth_post_data.CStr(), UriUnescape::NonCtrlAndEsc);
	#else
		int l = m_auth_post_data.Length();
		uni_char *str =  m_auth_post_data.CStr();
		
		ReplaceEscapedChars(m_auth_post_data, l);
		str[l] = '\0';
	#endif
		
		/* openid_url is deprecated, use uniteauth.id instead */
		RETURN_IF_ERROR(WebResource::FindItemsInString(m_auth_post_data.CStr(), UNI_L("openid_url"), openid_url, found, index = 0));
		if (found == FALSE)
		{
			RETURN_IF_ERROR(WebResource::FindItemsInString(m_auth_post_data.CStr(), UNI_L("uniteauth.id"), openid_url, found, index = 0));
		}
		WebserverAuthUser *user_object = NULL;
		if (found)
		{
			user_object = user_manager->GetUserFromAuthId(openid_url.CStr(), m_auth_type);
		
			if (user_object == NULL)
			{
				/* If a user is not found, create a dummy user.
				 * 
				 * Set a "temporary" property on the temporary dummy user, to ensure that the user is not saved on disk.
				 * 
				 * Then the user can be authenticated at the third party server, 
				 * without being registered in the user management system of this webserver.
				 **/
				
				if (user_manager->AllowUnknownOpenIdUsers())
				{
					OpString openid_username;
					RETURN_IF_ERROR(openid_username.Set(openid_url));
					
					int tries = 1;
					OP_STATUS status=OpStatus::OK;
					
					while (tries < 10 && (status = user_manager->CreateUser(openid_url.CStr())) == OpStatus::ERR)
					{
						openid_username.Wipe();
						openid_username.AppendFormat(UNI_L("%s_%d"), openid_url.CStr(), tries++);
					}
					RETURN_IF_ERROR(status);
					
					user_object = user_manager->GetUserFromName(openid_username.CStr());
					user_object->SetTemporary(TRUE);
					
					WebserverAuthenticationId_OpenId *open_id_object;
					
					RETURN_IF_ERROR(WebserverAuthenticationId_OpenId::Make(open_id_object, openid_url));
					if (OpStatus::IsError(status = user_object->AddAuthenticationId(open_id_object)))
					{
						OP_DELETE(open_id_object);				
						return status;
					}
					
				}
			}
		}
				
		if (user_object != NULL)
		{
			RETURN_IF_ERROR(user_object->CheckAuthentication(m_requestObject, openid_url.CStr(), this, m_auth_type));	
		}
		else
		{
			Authenticated(WebserverAuthenticationId::SERVER_AUTH_STATE_FAILED, NULL, m_auth_type);
		}
	}
	
	return OpStatus::OK;
}
/*virtual*/ 
OP_BOOLEAN WebResource_Auth::FillBuffer(char *buffer, unsigned int bufferSize, unsigned int &FilledDataSize)
{
	/* This resource does not send a body, only headers */
	
	return OpBoolean::IS_FALSE;
}

#endif // WEBSERVER_SUPPORT
