/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2008
 *
 * Web server implementation -- overall server logic
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "modules/webserver/webserver_openid.h"
#include "modules/webserver/webserver_magic_url.h"
#include "modules/webserver/webserver_user_management.h"
#include "modules/webserver/webserver_request.h"
#include "modules/libcrypto/include/OpRandomGenerator.h"

#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/webserver/webserver-api.h"
#include "modules/util/opfile/opfile.h"
#include "modules/upload/upload.h"

#include "modules/xmlutils/xmlnames.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/webserver/src/resources/webserver_auth.h"

#define MAX_NUMBER_OF_USERS 500 

/* Webserver User Manager */

WebserverUserManager::~WebserverUserManager()
{
	OP_DELETE(m_users_iterator);
}

WebserverUserManager::WebserverUserManager()
	: m_allow_unknown_open_id_users(FALSE)
	, m_users_iterator(NULL)
{
}

/* FIXME: this function must load all users from disk */	
/* static */ OP_STATUS WebserverUserManager::Init(const OpStringC16 persistent_storage_file)
{
	OP_ASSERT(g_webserverUserManager == NULL);
	if (g_webserverUserManager != NULL)
		return OpStatus::ERR;
	
	g_webserverUserManager = OP_NEW(WebserverUserManager, ());
	RETURN_OOM_IF_NULL(g_webserverUserManager);

	WebserverUserManager *temp = g_webserverUserManager;

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (
			(temp->m_users_iterator =  temp->m_users.GetIterator()) == NULL ||
			OpStatus::IsError(status = temp->m_persistent_storage_file.Set(persistent_storage_file)) ||
			OpStatus::IsError(status = temp->LoadUserList()) /* This function needs g_webserverUserManager */
		)
	{
		OP_DELETE(g_webserverUserManager);
		g_webserverUserManager = NULL;
	}
	
	return status;
}

/* static */ void WebserverUserManager::Destroy()
{
	if (g_webserverUserManager)
	{
		OpStatus::Ignore(g_webserverUserManager->SaveUserList());		
	}
	
	OP_DELETE(g_webserverUserManager);
	g_webserverUserManager = NULL;	
}

/* static */ OP_STATUS WebserverUserManager::GenerateRandomTextString(OpString &random_hex, unsigned int number_of_bytes)
{
	/* Generate m_magic_url and session_id */
	UINT8 *random = OP_NEWA(UINT8, number_of_bytes);

	if (random == NULL || random_hex.Reserve(number_of_bytes * 2 + 1) == NULL)
	{
		OP_DELETEA(random);
		return OpStatus::ERR_NO_MEMORY;
	}
	
	uni_char *rand_string = random_hex.CStr();

	g_libcrypto_random_generator->GetRandom(random, number_of_bytes);
	unsigned int i;
	for (i = 0; i < number_of_bytes; i ++)
	{
		UINT8 a0 = random[i] & 15;
		UINT8 a1 = (random[i] >> 4) & 15;
		rand_string[i * 2] = a0 +  (a0 < 10 ? '0' : 'a' - 10);
		rand_string[i * 2 + 1] = a1 +  (a1 < 10 ? '0' : 'a' - 10);
	}
	
	rand_string[i] = '\0';
	
	OP_DELETEA(random);	
	return OpStatus::OK;
}
	

/* Persistent storage. */
OP_STATUS WebserverUserManager::LoadUserList()
{
	//	/* read xml- file */
	OpFile file;
	RETURN_IF_ERROR(file.Construct(m_persistent_storage_file.CStr(), OPFILE_ABSOLUTE_FOLDER));
	
	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (found == FALSE)
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	XMLFragment user_xml;
	OP_STATUS status;
	if (OpStatus::IsSuccess(status = user_xml.Parse(static_cast<OpFileDescriptor*>(&file))))
	{
		if (user_xml.EnterElement(UNI_L("userlist")))
		{
			DeleteAllUsers();
			while (user_xml.HasMoreElements())
			{
				if (user_xml.EnterElement(UNI_L("user")))
				{
					WebserverAuthUser *user;
					
					RETURN_IF_ERROR(WebserverAuthUser::MakeFromXML(user, user_xml));
					OP_STATUS status;
					
					if (OpStatus::IsError(status = AddUser(user)))
					{
						OP_DELETE(user);
						RETURN_IF_MEMORY_ERROR(status);
					}
					
				}
				else
				{
					user_xml.EnterAnyElement();
				}
				user_xml.LeaveElement();
			}		
		}
		else
		{
			user_xml.EnterAnyElement();
		}
		user_xml.LeaveElement();
	}
	else
	{
		RETURN_IF_MEMORY_ERROR(status);
	}
	
	RETURN_IF_ERROR(file.Close());	
	return OpStatus::OK;
}

OP_STATUS WebserverUserManager::SaveUserList()
{
   	WebserverAuthUser *user = NULL;
	OP_STATUS ret = m_users_iterator->First();
	XMLFragment user_xml;
	
	RETURN_IF_ERROR(user_xml.OpenElement(UNI_L("userlist")));
	OP_STATUS status;
	while (ret == OpStatus::OK)
	{
		if ((user = static_cast<WebserverAuthUser*>(m_users_iterator->GetData())) != NULL)
		{

			if (OpStatus::IsError(status = user->CreateXML(user_xml)))
			{
				return status;
			}
		}
		ret = m_users_iterator->Next();
	}
	user_xml.CloseElement();
	
	TempBuffer buffer;
	RETURN_IF_ERROR(user_xml.GetXML(buffer, TRUE, NULL, TRUE));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(m_persistent_storage_file.CStr(), OPFILE_ABSOLUTE_FOLDER));
    RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
    RETURN_IF_ERROR(file.WriteUTF8Line(buffer.GetStorage()));	
	RETURN_IF_ERROR(file.Close());
	
	return OpStatus::OK;
}

OP_STATUS WebserverUserManager::CreateUser(const OpStringC &username, const OpStringC &email)
{	
	if (m_allow_unknown_open_id_users && m_users.GetCount() > MAX_NUMBER_OF_USERS) // To avoid denial of service attack, when allowing unknown users.
		return OpStatus::ERR;
	
	WebserverAuthUser *user;
	RETURN_IF_ERROR(WebserverAuthUser::Make(user, username, email));
	
	OP_STATUS status;
	if (OpStatus::IsError(status = AddUser(user)))
	{
		OP_DELETE(user);
	}
	
	return status;
}

OP_STATUS WebserverUserManager::AddUser(WebserverAuthUser *user)
{	
	return m_users.Add(user->GetUsername(), user);
}

OP_STATUS WebserverUserManager::DeleteUser(const OpStringC &username)
{
	OP_STATUS status = OpStatus::ERR;	
	WebserverAuthUser *user = GetUserFromName(username);

	if (user)
	{
		if (OpStatus::IsSuccess(status = RemoveUser(user)))
			OP_DELETE(user);
	}

	return status;	
}

void WebserverUserManager::DeleteAllUsers()
{
   	WebserverAuthUser *user = NULL;	
	if (OpStatus::IsError(m_users_iterator->First()))
		return;
	
	OP_STATUS ret = OpStatus::OK;
	while (ret == OpStatus::OK)
	{
		if ((user = static_cast<WebserverAuthUser*>(m_users_iterator->GetData())) != NULL)
		{
			if (OpStatus::IsSuccess(RemoveUser(user)))
			{
				OP_DELETE(user);
			}
		}
		ret = m_users_iterator->Next();
	}	
}

OP_STATUS WebserverUserManager::RemoveUser(WebserverAuthUser *user)
{
	if (user && m_users.Contains(user->GetUsername()))
	{		
		UINT32 number_of_subservers = g_webserver->GetSubServerCount();
		
		WebServer* temp_server = g_webserver;
		for (unsigned i = 0; i < number_of_subservers; i++)
		{
			temp_server->GetSubServerAtIndex(i)->RemoveAuthenticationUserFromAllResources(user->GetUsername());
		}
		
		WebserverAuthUser *removed_user;
		
		RETURN_IF_ERROR(m_users.Remove(user->GetUsername(), &removed_user));
		OP_ASSERT(removed_user == user);
		return OpBoolean::OK;
	}

	return OpBoolean::ERR;	
}


WebserverAuthUser *WebserverUserManager::GetUserFromName(const OpStringC &user_id)
{
	if (user_id.IsEmpty())
		return NULL;
	
	if (user_id.HasContent())
	{
		WebserverAuthUser *user;
		if (OpStatus::IsSuccess(m_users.GetData(user_id.CStr(), &user)))
		{
			return user;
		}
	}
	
	return NULL;
}

WebserverAuthUser *WebserverUserManager::GetUserFromSessionId(const OpStringC &session_id)
{
	if (session_id.IsEmpty())
		return NULL;
	
	WebserverAuthUser *user = NULL;

	OP_STATUS ret = m_users_iterator->First();
	while (ret == OpStatus::OK)
	{
		if ((user = static_cast<WebserverAuthUser*>(m_users_iterator->GetData())) != NULL)
		{
			if (user->GetAuthSessionFromSessionId(session_id.CStr()))
			{
				return user;
			}
		}
		ret = m_users_iterator->Next();
	}	
	return NULL;
}

WebserverAuthUser *WebserverUserManager::GetUserFromAuthId(const OpStringC &auth_id_string, WebSubServerAuthType type)
{
	if (auth_id_string.IsEmpty())
		return NULL;
	
	WebserverAuthUser *user = NULL;
	OP_STATUS ret = m_users_iterator->First();
	while (ret == OpStatus::OK)
	{
		if ((user = static_cast<WebserverAuthUser*>(m_users_iterator->GetData())) != NULL)
		{
			if (user->GetAuthenticationIdFromId(auth_id_string.CStr(), type))
			{
				return user;
			}
		}
		ret = m_users_iterator->Next();
	}	
	
	return NULL;
}

WebserverAuthSession *WebserverUserManager::GetAuthSessionFromSessionId(const OpStringC &session_id)
{
	if (session_id.IsEmpty())
		return NULL;	

	WebserverAuthUser *user = NULL;
	WebserverAuthSession *session = NULL;
	
	OP_STATUS ret = m_users_iterator->First();
	while (ret == OpStatus::OK)
	{
		if ((user = static_cast<WebserverAuthUser*>(m_users_iterator->GetData())) != NULL)
		{
			if ((session = user->GetAuthSessionFromSessionId(session_id.CStr())))
			{
				return session;
			}
		}
		ret = m_users_iterator->Next();
	}	
	return NULL;	
}

WebserverAuthenticationId *WebserverUserManager::GetAuthenticationIdFromId(const OpStringC &auth_id_string, WebSubServerAuthType type)
{
	if (auth_id_string.IsEmpty())
		return NULL;

	WebserverAuthUser *user = NULL;
	WebserverAuthenticationId *auth_id = NULL;

	OP_STATUS ret = m_users_iterator->First();
	while (ret == OpStatus::OK)
	{
		if ((user = static_cast<WebserverAuthUser*>(m_users_iterator->GetData())) != NULL)
		{
			if ((auth_id = user->GetAuthenticationIdFromId(auth_id_string.CStr(), type)))
			{
				return auth_id;
			}
		}
		ret = m_users_iterator->Next();
	}	
	return NULL;
}

OP_BOOLEAN WebserverUserManager::CheckSessionAuthentication(WebserverRequest *request_object, const OpStringC &session_id, WebserverAuthUser *&user_object, WebserverAuthSession *&session)
{
	if (session_id.IsEmpty())
		return OpBoolean::IS_FALSE;
	
	user_object = NULL;
	
	HeaderList *headerList = request_object->GetClientHeaderList();
	ParameterList *cookie_parameters = NULL;
	HeaderEntry *cookie_header = NULL;
	while ((cookie_header = headerList->GetHeader("Cookie", cookie_header)) != NULL)
	{
		cookie_parameters = cookie_header->GetParameters((PARAM_SEP_SEMICOLON | PARAM_SEP_COMMA | PARAM_SEP_WHITESPACE | PARAM_STRIP_ARG_QUOTES | PARAM_HAS_RFC2231_VALUES), KeywordIndex_HTTP_General_Parameters);
		if (cookie_parameters)
		{
			Parameters *auth_parameter = cookie_parameters->GetParameter("unite-session-auth", PARAMETER_ANY);
			if (auth_parameter)
			{
				OpString auth;
				RETURN_IF_ERROR(auth.SetFromUTF8(auth_parameter->Value()));

				WebserverAuthUser *temp_user_object = GetUserFromSessionId(session_id);
				
				/* FIXME: optimize this with hash tables */
				
				WebserverAuthSession *temp_session;
				if (temp_user_object != NULL && (temp_session = temp_user_object->GetAuthSessionFromSessionId(session_id)) != NULL)
				{
					if (auth.Compare(temp_session->GetSessionAuth()) == 0)
					{
						user_object = temp_user_object;
						session = temp_session;
						return OpBoolean::IS_TRUE;
					}
				}
			}
		}
	}
	
	return OpBoolean::IS_FALSE;	
	
}

#ifdef WEB_HTTP_DIGEST_SUPPORT
OP_STATUS WebserverUserManager::CreateAuthenctionNonceElement(OpString8 &nonce)
{	
	WebserverServerNonceElement *nonceElement = OP_NEW(WebserverServerNonceElement, ());
	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	
	if (nonceElement == NULL ||
		OpStatus::IsError(status = nonceElement->ContructNonce()) ||
		OpStatus::IsError(status = m_authenctionNonces.Add(nonceElement)))
	{
		OP_DELETE(nonceElement);
		nonceElement = NULL;
		return status;
	}
	
	RETURN_IF_ERROR(nonce.Set(nonceElement->GetNonce()));
	
	return OpStatus::OK;
}
	
WebserverServerNonceElement *WebserverUserManager::GetAuthenctionNonceElement(const OpStringC8 &nonce)
{
	double time = OpDate::GetCurrentUTCTime();
	int count = m_authenctionNonces.GetCount();

	WebserverServerNonceElement *elementFound = NULL;
	for (int idx = 0; idx < count; idx++)
	{		
		WebserverServerNonceElement *element = m_authenctionNonces.Get(idx);

		const OpStringC8 storedNonce = element->GetNonce();
		if 	( nonce == storedNonce )
		{
				OP_ASSERT(elementFound == NULL); /*we should not find the same nonce twice*/
				elementFound = element;
		}

		double nonceTimeStamp = element->GetTimeStamp();
		if ( time- nonceTimeStamp > WEB_PARM_NONCE_MAX_LIFETIME) /* checks and removes old nonces */
		{

			if (elementFound == element)
			{
				elementFound = NULL;				
			}
			
			/*removes old nonces*/
			m_authenctionNonces.Delete(element);

			idx--;
			count = m_authenctionNonces.GetCount();
			OP_ASSERT(count >= 0);
			OP_ASSERT(idx >= -1);
		}					
	}
	return elementFound;	
}
#endif // WEB_HTTP_DIGEST_SUPPORT

template<class T>
class OpVectorIterator : public OpHashIterator
{
public:
	
	OpVectorIterator(OpVector<T> *vector) 
		: m_index(0), 
		m_vector(vector) 
	{}
	
	virtual ~OpVectorIterator(){};
		
	virtual OP_STATUS First()
	{ 
		m_index = 0;
		return OpStatus::OK;
	}

	virtual OP_STATUS Next() 
	{
		
		if (m_index  >= m_vector->GetCount()) 
			return OpStatus::ERR;

		m_index++;
		return OpStatus::OK; 
	}
	
	virtual const void* GetKey() const 
	{ 
		return m_vector->Get(m_index)->GetAuthId();
	}
	
	virtual void* GetData() const
	{
		return m_vector->Get(m_index);	
	}
	
	unsigned int m_index;
	OpVector<T> *m_vector;
};

/* Webserver User */


WebserverAuthUser::WebserverAuthUser()
	: m_users_iterator(NULL)
	, m_temporary(FALSE)
{
}

WebserverAuthUser::~WebserverAuthUser()
{
	OP_DELETE(m_users_iterator);
}

/* static */ OP_STATUS WebserverAuthUser::Make(WebserverAuthUser *&user, const OpStringC &username, const OpStringC &email)
{
	user = NULL;
	OpAutoPtr<WebserverAuthUser> temp_user(OP_NEW(WebserverAuthUser, ()));
	RETURN_OOM_IF_NULL(temp_user.get());
	
	RETURN_IF_ERROR(temp_user->m_username.Set(username));

	temp_user->m_users_iterator = OP_NEW(OpVectorIterator<WebserverAuthenticationId>, (&temp_user->m_auth_ids));

	if (temp_user->m_users_iterator == NULL)
		return OpStatus::ERR_NO_MEMORY;
		
	
	user = temp_user.release();
	return OpBoolean::OK;
}

/* static */ OP_STATUS WebserverAuthUser::MakeFromXML(WebserverAuthUser *&user, XMLFragment &user_xml)
{
	user = NULL;

	const uni_char *username = user_xml.GetAttribute(UNI_L("username"));

	/* create user */
	if (username)
	{
		const uni_char *user_email = user_xml.GetAttribute(UNI_L("email"));	
		WebserverAuthUser *temp;
		
		OP_STATUS exists;
		RETURN_IF_ERROR(WebserverAuthUser::Make(temp, username, user_email));
		OpAutoPtr<WebserverAuthUser> temp_user(temp);
		while (user_xml.HasMoreElements())
		{
			if (user_xml.EnterElement(UNI_L("authentication")))
			{
				OP_STATUS status;
				WebserverAuthenticationId *authentication_id;
				RETURN_IF_MEMORY_ERROR(status = WebserverAuthenticationId::MakeFromXML(authentication_id, user_xml));
				
				if (OpStatus::IsSuccess(status) && OpStatus::IsError(exists = temp_user->AddAuthenticationId(authentication_id)))
				{
					OP_DELETE(authentication_id);
					RETURN_IF_MEMORY_ERROR(exists);
				}
			}
			else
			{
				user_xml.EnterAnyElement();
			}
			user_xml.LeaveElement();	
		}
		
		user = temp_user.release();
		return OpBoolean::OK;
	}
	
	return OpBoolean::OK;
	
}

OP_STATUS WebserverAuthUser::AddAuthenticationId(WebserverAuthenticationId *auth_id)
{
	/* Check that auth id is globally unique */
	if (g_webserverUserManager->GetUserFromAuthId(auth_id->GetAuthId(), auth_id->GetAuthType()))
		return OpBoolean::ERR;
	
	OP_ASSERT(auth_id->GetUser() == NULL && "auth_id is already added to a user.");
	if (auth_id->GetUser() != NULL)
		return OpStatus::ERR;

	RETURN_IF_ERROR(m_auth_ids.Add(auth_id));

	auth_id->SetUser(this);
	return OpBoolean::OK;
}

OP_STATUS WebserverAuthUser::RemoveAuthenticationId(const OpStringC &auth_id, WebSubServerAuthType type)
{
	return m_auth_ids.Delete(GetAuthenticationIdFromId(auth_id, type));
}

WebserverAuthSession *WebserverAuthUser::GetAuthSessionFromSessionId(const OpStringC &session_id) const
{
	if (session_id.IsEmpty())
		return NULL;
	
	WebserverAuthSession *session;
	if (OpStatus::IsSuccess(m_auth_sessions.GetData(session_id.CStr(), &session)))
	{
		return session;
	}
	
	return NULL;
}

BOOL WebserverAuthUser::RemoveSession(const OpStringC &session_id)
{
	WebserverAuthSession *session;
	if (OpStatus::IsSuccess(m_auth_sessions.Remove(session_id.CStr(), &session)))
	{
		OP_DELETE(session);
		return TRUE;
	}
	
	return FALSE;
}

void WebserverAuthUser::RemoveAllSessions()
{
	m_auth_sessions.DeleteAll();
}

WebserverAuthenticationId *WebserverAuthUser::GetAuthenticationIdFromId(const OpStringC &auth_id_string, WebSubServerAuthType type) const
{
	if (auth_id_string.IsEmpty())
		return NULL;
	
	WebserverAuthenticationId *auth_id;
	for (unsigned i = 0; i < m_auth_ids.GetCount(); i++)
	{		
		if ((auth_id = m_auth_ids.Get(i)) != NULL && type == auth_id->GetAuthType() && auth_id_string.Compare(auth_id->GetAuthId()) == 0)
		{
			return auth_id;
		}
	}
	return NULL;
}

OP_STATUS WebserverAuthUser::CheckAuthentication(WebserverRequest *request_object, const OpStringC &auth_id_string, WebResource_Auth *auth_resource, WebSubServerAuthType authentication_type)
{
	if (request_object == NULL)
	{ 
		return OpStatus::ERR;
	}

	WebserverAuthenticationId *auth_id = GetAuthenticationIdFromId(auth_id_string, authentication_type);
	
	if (auth_id)
		return auth_id->CheckAuthenticationBase(request_object, auth_resource);
	else
		return auth_resource->Authenticated(WebserverAuthenticationId::SERVER_AUTH_STATE_FAILED, NULL, authentication_type);
}

OP_STATUS WebserverAuthUser::CreateXML(XMLFragment &user_xml)
{
	if (m_temporary)
		return OpStatus::OK; // Don't save if the user is temporary only
	
	RETURN_IF_ERROR(user_xml.OpenElement(UNI_L("user")));
	
	OP_ASSERT(m_username.HasContent());
	RETURN_IF_ERROR(user_xml.SetAttribute(UNI_L("username"), m_username.CStr()));
	
	if (m_user_email.HasContent())
		RETURN_IF_ERROR(user_xml.SetAttribute(UNI_L("email"), m_user_email.CStr()));
	
	for (unsigned i = 0; i < m_auth_ids.GetCount(); i++)
	{
		RETURN_IF_ERROR(m_auth_ids.Get(i)->CreateXML(user_xml));
	}
	
	user_xml.CloseElement();

	return OpStatus::OK;
}

WebserverAuthenticationId::WebserverAuthenticationId()
	: m_auth_resource(NULL),
	  m_user(NULL)
{
	
}

/* Webserver Authentication objects */

/* static */ OP_STATUS WebserverAuthenticationId::MakeFromXML(WebserverAuthenticationId *&authentication_id, XMLFragment &user_xml)
{
	authentication_id = NULL;
	OpAutoPtr<WebserverAuthenticationId> temp;
	
	WebSubServerAuthType type = AuthStringToType(user_xml.GetAttribute(UNI_L("type")));
	const uni_char *id = user_xml.GetAttribute(UNI_L("id"));
	
	switch (type) {
		case WEB_AUTH_TYPE_HTTP_MAGIC_URL:
		{
			WebserverAuthenticationId_MagicUrl *auth_id;
			RETURN_IF_ERROR(WebserverAuthenticationId_MagicUrl::MakeFromXML(auth_id, user_xml));
			temp = auth_id;
			break;
		}

		case WEB_AUTH_TYPE_HTTP_OPEN_ID:
		{
			WebserverAuthenticationId_OpenId *auth_id;
			RETURN_IF_ERROR(WebserverAuthenticationId_OpenId::MakeFromXML(auth_id, user_xml));
			temp = auth_id;
			break;
		}
	
		default:
			return OpStatus::ERR;
	}

	RETURN_IF_ERROR(temp->m_auth_id.Set(id));
	
	authentication_id = temp.release();
	return OpStatus::OK;
}

/* static */ const uni_char *WebserverAuthenticationId::AuthTypeToString(WebSubServerAuthType type)
{
	OP_ASSERT(type >= 0 && type <= WEB_AUTH_TYPE_NONE);
	
	switch (type) {
		case WEB_AUTH_TYPE_HTTP_MAGIC_URL:
			return UNI_L("magic-url");

		case WEB_AUTH_TYPE_HTTP_OPEN_ID:
			return UNI_L("openid");

		case WEB_AUTH_TYPE_HTTP_DIGEST:
			return UNI_L("digest");
			
		default:
			return UNI_L("none");
	}
}

/* static */ WebSubServerAuthType WebserverAuthenticationId::AuthStringToType(const uni_char *authentication_type)
{
	if (authentication_type == NULL)
		return WEB_AUTH_TYPE_NONE;
	
	WebSubServerAuthType type = WEB_AUTH_TYPE_HTTP_MAGIC_URL;

	while (type < WEB_AUTH_TYPE_NONE && uni_stricmp(authentication_type, WebserverAuthenticationId::AuthTypeToString(type)) != 0)
		type = WebSubServerAuthType(type + 1);

	return type;
}

WebserverAuthenticationId::~WebserverAuthenticationId()
{
	if (m_auth_resource)
	{
		m_auth_resource->SetAuthenticationMethod(NULL);
	}
}

OP_STATUS WebserverAuthenticationId::CreateXML(XMLFragment &user_xml)
{
	RETURN_IF_ERROR(user_xml.OpenElement(UNI_L("authentication")));
	RETURN_IF_ERROR(user_xml.SetAttribute(UNI_L("type"), AuthTypeToString(GetAuthType())));
	
	if (m_auth_id.HasContent())
	{
		RETURN_IF_ERROR(user_xml.SetAttribute(UNI_L("id"), m_auth_id.CStr()));
	}
	
		
	RETURN_IF_ERROR(CreateAuthXML(user_xml));
	
	user_xml.CloseElement();

	return OpStatus::OK;
}

/* virtual */ OP_STATUS WebserverAuthenticationId::CheckAuthenticationBase(WebserverRequest *request_object, WebResource_Auth *auth_resource)
{
	if (m_auth_resource != NULL)
	{
		return CreateSessionAndSendCallback(SERVER_AUTH_STATE_ANOTHER_AGENT_IS_AUTHENTICATING);
	}
	OP_ASSERT(auth_resource != NULL);

	m_auth_resource = auth_resource;
	m_auth_resource->SetAuthenticationMethod(this);
	return CheckAuthentication(request_object);
}

OP_STATUS WebserverAuthenticationId::CreateSessionAndSendCallback(AuthState success)
{
	OP_ASSERT(m_auth_resource != NULL);
	
	if (m_auth_resource == NULL || m_auth_resource->GetRequestObject() == NULL)
		return OpStatus::ERR;

	WebResource_Auth *auth_resource = m_auth_resource;
	m_auth_resource = NULL;	
	auth_resource->SetAuthenticationMethod(NULL);
	WebserverAuthSession *session = NULL;
	if (success == SERVER_AUTH_STATE_SUCCESS)
	{
		OP_ASSERT(m_user != NULL);
		m_user->RemoveSession(auth_resource->GetRequestObject()->GetSessionId());
		
				
		RETURN_IF_ERROR(WebserverAuthSession::Make(session, auth_resource->GetRequestObject()->GetSessionId(), GetAuthType()));
		OP_STATUS status;
		if (OpStatus::IsError(status = m_user->m_auth_sessions.Add(session->GetSessionId(), session)))
		{
			OP_DELETE(session);
			session = NULL;
			return status;	
		}
	}

	return auth_resource->Authenticated(success, session, GetAuthType());
}

/* Webserver Authentication sessions */

WebserverAuthSession::WebserverAuthSession(WebSubServerAuthType type)
	: m_type(type)
	{
}

/* static */ OP_STATUS WebserverAuthSession::Make(WebserverAuthSession *&session, const OpStringC &session_id, WebSubServerAuthType type)
{
	session = NULL;
	OpAutoPtr<WebserverAuthSession> temp_session(OP_NEW(WebserverAuthSession,(type)));
	RETURN_OOM_IF_NULL(temp_session.get());
	
	RETURN_IF_ERROR(WebserverUserManager::GenerateRandomTextString(temp_session->m_session_auth, 24));
	
	RETURN_IF_ERROR(temp_session->m_session_id.Set(session_id));
	
	session = temp_session.release();
	return OpStatus::OK;
}

#endif // WEBSERVER_SUPPORT
