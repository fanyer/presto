/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2012
 *
 * Web server implementation -- overall server logic
 */

#include "core/pch.h"
#ifdef WEBSERVER_SUPPORT

#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver_filename.h"
#include "modules/webserver/src/webservercore/webserver_private_global_data.h"
#include "modules/formats/uri_escape.h"
#include "modules/webserver/webserver_user_management.h"
#include "modules/formats/argsplit.h"

WebserverRequest::WebserverRequest()
	: m_httpMethod(WEB_METH_NONE_ERROR)
	, m_clientHeaderList(NULL)
	, m_forwardRequestState(FALSE)
	, m_subServerVirtualPathEdited(FALSE)
	, m_auth_status(FALSE)
	, m_isProxied(FALSE)
	, m_requstId(0)
	, m_responseCode(0)
	, m_had_session_cookie(FALSE)
	, m_temp_item_collection(NULL)
{
}

WebserverRequest::~WebserverRequest()
{
	if (m_clientHeaderList)
		OP_DELETE(m_clientHeaderList);
	m_clientHeaderList = 0;
	if (InList())
		Out();
}

OP_STATUS 
WebserverRequest::Construct(const char *resource, int requestStringLength, HeaderList *clientHeaderList, WebServerMethod aHttpMethod, BOOL isProxied)
{
	OP_ASSERT(m_clientHeaderList == 0);
	OP_ASSERT(m_originalRequestURI.Length() == 0 && m_clientHeaderList == 0);
	
	if (m_originalRequestURI.Length() != 0 || m_clientHeaderList != NULL )
	{
		return OpStatus::ERR;
	}
	
	RETURN_IF_ERROR(GetSubServerName(resource, requestStringLength, m_subServerVirtualPath));
	
	if (uni_strni_eq(m_subServerVirtualPath.CStr(), UNI_L("_root"), 5) && !uni_strni_eq(resource, UNI_L("/_root"), 6))
	{
		RETURN_IF_ERROR(m_originalRequestURI.Set("/_root"));
	}
	
	RETURN_IF_ERROR(m_originalRequestURI.Append(resource, requestStringLength));
	RETURN_IF_ERROR(m_uniOriginalRequestURI.SetFromUTF8(m_originalRequestURI.CStr()));
	RETURN_IF_ERROR(WebserverFileName::ConstructFromURL(m_originalRequestURI));
			
	m_requstId = g_webserverPrivateGlobalData->CountRequests();

	m_clientHeaderList = clientHeaderList;
	m_httpMethod = aHttpMethod;
	m_isProxied = isProxied;

	/* Must happen last */
	return SetupSessionId();
}

OP_STATUS 
WebserverRequest::Reconstruct(const char *resource, int requestStringLength, HeaderList *clientHeaderList)
{
	return ReconstructOverrideSecurity(resource, requestStringLength, clientHeaderList, FALSE);
}

OP_STATUS 
WebserverRequest::ReconstructOverrideSecurity(const char *resource, int requestStringLength, HeaderList *clientHeaderList, BOOL overrideSecurity)
{
	RETURN_IF_ERROR(m_originalRequestURI.Set(resource, requestStringLength));
	RETURN_IF_ERROR(m_uniOriginalRequestURI.SetFromUTF8(resource, requestStringLength));
	
	OpString newSubServerName;
	RETURN_IF_ERROR(GetSubServerName(resource, requestStringLength, newSubServerName));
	
	m_subServerVirtualPathEdited = uni_str_eq(newSubServerName.CStr(), m_subServerVirtualPath.CStr()) == TRUE || overrideSecurity == TRUE ? FALSE : TRUE; 
	
	RETURN_IF_ERROR(WebserverFileName::ConstructFromURL(resource));
	
	if (clientHeaderList)
	{
		if (m_clientHeaderList)
		{
			OP_DELETE(m_clientHeaderList);	
		}
		m_clientHeaderList = clientHeaderList;
	}
	
	if (m_subServerVirtualPathEdited == TRUE)
	{
		RETURN_IF_ERROR(m_subServerVirtualPath.Set(newSubServerName.CStr()));
	}

	/* Must happen last */
	return SetupSessionId();
}

OP_STATUS WebserverRequest::SetResponseHeaders(Header_List &headers)
{
	if (m_responseHeaderList.CalculateLength() == 0)
		RETURN_IF_LEAVE(m_responseHeaderList.InitL());
	
	m_responseHeaderList.InsertHeaders(headers);
	return OpStatus::OK;
}

OP_STATUS WebserverRequest::SetupSessionId()
{

	/* Check if user have a session */
	ParameterList *cookie_parameters = NULL;
	HeaderEntry *cookie_header = NULL;
	
	m_session_id.Wipe();
	m_session_auth.Wipe();
	
	while ((cookie_header = m_clientHeaderList->GetHeader("Cookie", cookie_header)) != NULL)
	{
		cookie_parameters = cookie_header->GetParameters((PARAM_SEP_SEMICOLON | PARAM_SEP_COMMA | PARAM_SEP_WHITESPACE | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES), KeywordIndex_HTTP_General_Parameters);
		if (cookie_parameters)
		{
			Parameters *auth_parameter = cookie_parameters->GetParameter("unite-session-auth", PARAMETER_ANY);
			Parameters *session_id_parameter = cookie_parameters->GetParameter("unite-session-id", PARAMETER_ANY);
			
			if (session_id_parameter)
			{
				m_had_session_cookie = TRUE;
				RETURN_IF_ERROR(m_session_id.Set(session_id_parameter->Value()));
			}
				
			if (auth_parameter)
			{
				RETURN_IF_ERROR(m_session_auth.Set(auth_parameter->Value()));
			}
			
			if (session_id_parameter && auth_parameter)
			{
				break;
			}
		}
	}

	if (!uni_str_eq(GetSubServerRequest(), UNI_L("/favicon.ico")))
	{
		/* If the user did not have a session, generate a session id*/	
		if (m_session_id.IsEmpty())
		{
			m_had_session_cookie = FALSE;
			m_session_auth.Wipe();
			RETURN_IF_ERROR(WebserverUserManager::GenerateRandomTextString(m_session_id, 24));
		}
		else
		{
			OP_BOOLEAN authenticated;
			WebserverAuthSession *session;	
			WebserverAuthUser *user_object = NULL;
			if (
					OpStatus::IsSuccess(authenticated = g_webserverUserManager->CheckSessionAuthentication(this, m_session_id, user_object, session)) && 
					user_object && 
					session &&
					authenticated == OpBoolean::IS_TRUE
				)
			{
				RETURN_IF_ERROR(m_authenticated_user.Set(user_object->GetUsername()));
			}
		}
	}
	return OpStatus::OK;
}

BOOL WebserverRequest::HasResponseHeaders()
{
	if (m_responseHeaderList.CalculateLength() > 0)
		return TRUE;
	else 
		return FALSE;
}

void WebserverRequest::TakeOverResponseHeaders(Header_List &headers)
{
	if (m_responseHeaderList.CalculateLength() != 0)
		headers.InsertHeaders(m_responseHeaderList);
}

/* static */ OP_STATUS 
WebserverRequest::UrlEncodeString(const OpStringC &uri, OpString &encoded_uri)
{
 	if (!uri.CStr())
	{
		RETURN_IF_ERROR(encoded_uri.Set(""));
		return OpStatus::OK;
	}
		
#ifdef FORMATS_URI_ESCAPE_SUPPORT
	if (uri.CStr() && UriEscape::CountEscapes(uri.CStr(), UriEscape::StandardUnsafe) == 0)	
#else
	if (uri.CStr() && CountEscapes(uri.CStr()) == 0)
#endif		
	{
		return encoded_uri.Set(uri);
	}
	
	OpString8 unescaped;	
	RETURN_IF_ERROR(unescaped.SetUTF8FromUTF16(uri.CStr()));

	OpString8 escaped;
	if (escaped.Reserve(unescaped.Length() * 3 + 1) == NULL)
		return OpStatus::ERR_NO_MEMORY;
	
#ifdef FORMATS_URI_ESCAPE_SUPPORT
	UriEscape::Escape(escaped.CStr(), unescaped.CStr(), UriEscape::StandardUnsafe);
#else
	EscapeURL(escaped.CStr(), unescaped);
#endif

	RETURN_IF_ERROR(encoded_uri.SetFromUTF8(escaped.CStr()));
	
	return OpStatus::OK;
}


OP_STATUS 
WebserverRequest::GetSubServerName(const char *request, int requestStringLength, OpString &subServerName)
{
	if (requestStringLength > 1 && *request == '/' && *(request + 1) == '/')
	{
		requestStringLength--;
		request++;
	}
	
	if ((requestStringLength == 12 && uni_strni_eq(request, UNI_L("/favicon.ico"), 12)) || ( requestStringLength == 1 && uni_strni_eq(request, UNI_L("/"), 1)) || ( requestStringLength > 1 && uni_strni_eq(request, UNI_L("/?"), 2)))
	{
		return subServerName.Set("_root");
	}
	
	OpString requestCopy;
	
	RETURN_IF_ERROR(requestCopy.SetFromUTF8(request, requestStringLength));
	
#ifdef FORMATS_URI_ESCAPE_SUPPORT
	UriUnescape::ReplaceChars(requestCopy.CStr(), UriUnescape::LocalfileUrlUtf8);
#else
	int length = requestCopy.Length();
	
	ReplaceEscapedCharsUTF8(requestCopy.CStr(), length, OPSTR_LOCALFILE_URL);
	*(requestCopy.CStr() + length) = '\0'; 
#endif
	
	const uni_char *start = uni_strchr(requestCopy.CStr(), '/');
	
	if (start == NULL)
	{
		start = requestCopy.CStr();
	}
	else  
	{
		while (*start == '/')
			start++;
	}	
	
	if (start && *start != '\0')
	{
		const uni_char *end = uni_strpbrk(start, UNI_L("/?#\\"));
		RETURN_IF_ERROR(subServerName.Set(start, end > start ? end - start : int(KAll) ));
	}
	else
	{
		RETURN_IF_ERROR(subServerName.Set(""));
	}
	return OpStatus::OK;
}

OP_STATUS 
WebserverRequest::Copy(WebserverRequest *requestObjectOriginal)
{
	OP_ASSERT(requestObjectOriginal != NULL);
	if (requestObjectOriginal == NULL)
		return OpStatus::ERR;
	
	OP_DELETE(m_clientHeaderList);
	m_clientHeaderList = NULL;
	
	TRAPD(status, m_clientHeaderList = requestObjectOriginal->GetClientHeaderList()->DuplicateL());
	if (!m_clientHeaderList || OpStatus::IsError(status))
	{
		OP_DELETE(m_clientHeaderList);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	RETURN_IF_ERROR(WebserverFileName::Construct(requestObjectOriginal->GetRequest()));
	
	m_httpMethod = requestObjectOriginal->m_httpMethod;
	m_forwardRequestState = requestObjectOriginal->m_forwardRequestState;
	m_subServerVirtualPathEdited = requestObjectOriginal->m_subServerVirtualPathEdited;
	
	RETURN_IF_ERROR(m_originalRequestURI.Set(requestObjectOriginal->m_originalRequestURI));
	RETURN_IF_ERROR(m_uniOriginalRequestURI.Set(requestObjectOriginal->m_uniOriginalRequestURI));
	RETURN_IF_ERROR(m_subServerVirtualPath.Set(requestObjectOriginal->m_subServerVirtualPath));
	RETURN_IF_ERROR(m_session_id.Set(requestObjectOriginal->m_session_id));
	RETURN_IF_ERROR(m_session_auth.Set(requestObjectOriginal->m_session_auth));
	RETURN_IF_ERROR(m_authenticated_user.Set(requestObjectOriginal->m_authenticated_user));
	
	m_requstId = requestObjectOriginal->m_requstId;
	
	if (m_subServerVirtualPathEdited == FALSE) /* only allow this if the subserver name has not changed */
		m_auth_status  = requestObjectOriginal->HasAuthenticated();
	
	return OpStatus::OK;
}

const char *WebserverRequest::GetOriginalRequest()
{
	return m_originalRequestURI.CStr();
}	

const uni_char *WebserverRequest::GetOriginalRequestUni()
{
	return m_uniOriginalRequestURI.CStr();
}	


const uni_char *WebserverRequest::GetRequest()
{
	return GetPathAndFileNamePointer();
}

const uni_char *WebserverRequest::GetSubServerRequest()
{
	const uni_char *path = GetOriginalRequestUni();
	while (path && *path == '/')
		path++;
	return path + uni_strlen(GetSubServerVirtualPath());
}


OP_STATUS WebserverRequest::GetEscapedRequest(OpString &request)
{
	return request.Set(m_uniOriginalRequestURI);
}

HeaderList *WebserverRequest::GetClientHeaderList()
{
	return m_clientHeaderList;		
}
	
WebServerMethod WebserverRequest::GetHttpMethod()
{
	return  m_httpMethod;	
}

OP_BOOLEAN WebserverRequest::GetItemInQuery(const uni_char* item, OpString &value, unsigned int &index, BOOL unescape)
{
	const uni_char *request_string = unescape ? GetSubServerRequest() : m_uniOriginalRequestURI.CStr();		
	const uni_char *string = NULL;
	if (index)
	{	
		if (index >= uni_strlen(request_string))
		{
			OP_ASSERT(!"mixing unescaped and escaped");
			return OpStatus::ERR;
			
		}
		string = request_string + index;
	}
	else 
	{
		string = uni_strchr(request_string, '?');
		if (string)
		{
			string ++;
		}	
	}

	if (string == NULL)
		return OpBoolean::IS_FALSE;

	BOOL found = FALSE;
	RETURN_IF_ERROR(WebResource::FindItemsInString(string, item, value, found, index, unescape));

	if (found)
		return OpBoolean::IS_TRUE;
	
	return OpBoolean::IS_FALSE;
}

OP_STATUS WebserverRequest::GetAllItemsInQuery(OpStringHashTable<OpAutoStringVector> &item_collection, BOOL unescape)
{
	m_temp_item_collection =  &item_collection;
	const uni_char *string = uni_strchr(m_uniOriginalRequestURI.CStr(), '?');
	
	if (string)
	{
		string ++;
		return WebResource::FindIAlltemsInString(string, this, unescape);	
	}	
	
	return OpStatus::OK;
}

OP_STATUS WebserverRequest::DetectAuthenticationType(WebSubServerAuthType &type)
{
	OpString type_string;
	unsigned int index = 0;
	
	RETURN_IF_ERROR(GetItemInQuery( UNI_L("uniteauth.type"), type_string, index, FALSE));
	type = WebserverAuthenticationId::AuthStringToType(type_string.CStr());
	return OpStatus::OK;

}

OP_STATUS WebserverRequest::AddToCollection(const uni_char* key, const uni_char* data_string)
{
	OpAutoStringVector *item_vector = NULL;

	OP_STATUS status;
		
	if (OpStatus::IsError(m_temp_item_collection->GetData(key, &item_vector)))
	{
		OP_ASSERT(item_vector == NULL);
		

		status = OpStatus::ERR_NO_MEMORY;
		item_vector = OP_NEW(OpAutoStringVector, ());
		
		if (item_vector == NULL || OpStatus::IsError(status = item_vector->SetVectorName(key)) || OpStatus::IsError(status = m_temp_item_collection->Add(item_vector->GetVectorName(), item_vector)))
		{
			OP_DELETE(item_vector);
			return status;
		}
	}
	status = OpStatus::ERR_NO_MEMORY;
	
	OpString *data = OP_NEW(OpString, ());
	
	if (data == NULL || OpStatus::IsError(status = data->Set(data_string)) || OpStatus::IsError(status = item_vector->Add(data)))
	{
		OP_DELETE(data);
	}
	
	return status;
}




#endif //WEBSERVER_SUPPORT
