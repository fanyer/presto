/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006-2010
 *
 * Web server implementation -- overall server logic
 */
#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT
#include "modules/util/opfile/opfile.h"
#include "modules/util/htmlify.h"
#include "modules/util/datefun.h"
#include "modules/webserver/webserver_request.h"
#include "modules/webserver/webserver_resources.h"
#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/src/resources/webserver_file.h"
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/pi/OpLocale.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#include "modules/webserver/webserver_filename.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"
#include "modules/webserver/webresource_base.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/xmlutils/xmlnames.h"
#include "modules/webserver/webserver-api.h"
#include "modules/dochand/winman.h"

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#include "modules/webserver/src/rendezvous/webserver_rendezvous_util.h"
#endif

#include "modules/dochand/winman.h"

#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadget.h"
#endif



WebserverResourceDescriptor_Base::WebserverResourceDescriptor_Base()
	: m_active(TRUE)
{
}

WebResourceWrapper::WebResourceWrapper(WebResource *webResource)
	:	m_webResourcePtr(webResource)
{
	OP_ASSERT(m_webResourcePtr != NULL);
	m_webResourcePtr->Increment_Reference();
}

WebResourceWrapper::~WebResourceWrapper()
{
	if (m_webResourcePtr)
	{
		m_webResourcePtr->Decrement_Reference();
		if (m_webResourcePtr->Get_Reference_Count() == 0)
		{
			OP_DELETE(m_webResourcePtr);
		}
	}
}

WebResource*  WebResourceWrapper::Ptr()
{
	return m_webResourcePtr;
}

/*webserver authentication resource descriptor*/
WebserverResourceDescriptor_AccessControl::WebserverResourceDescriptor_AccessControl()
	: m_persistent(FALSE)
	, m_allow_all_known_users(FALSE)
{
 	
}

/* static */ OP_STATUS 
WebserverResourceDescriptor_AccessControl::Make(WebserverResourceDescriptor_AccessControl *&webserverAccessControlResource, const OpStringC &path, const OpStringC &realm, BOOL persistent, BOOL active, BOOL allow_all_known_users)
{
	OpString realm_string;
	if (realm.IsEmpty() == FALSE)
	{
		if (realm.FindFirstOf('\"') != KNotFound || realm.FindFirstOf('>') != KNotFound || realm.FindFirstOf('<') != KNotFound)
		{
			return OpStatus::ERR;
		}
		
		RETURN_IF_ERROR(realm_string.Set(realm));
	}
	else
	{
		RETURN_IF_ERROR(realm_string.Set("Authentication required")); /* FIXME: use translated string */	
	}
	
	webserverAccessControlResource = NULL; 
	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;

	WebserverResourceDescriptor_AccessControl *temp_authElement = OP_NEW(WebserverResourceDescriptor_AccessControl, ());

	if 	(	temp_authElement == NULL || 
			OpStatus::IsError( status = temp_authElement->m_resourceVirtualPath.Construct(path)) ||
			OpStatus::IsError( status = temp_authElement->m_realm.Set(realm_string.CStr()))
		)
	{
		OP_DELETE(temp_authElement);
		return status;
	}

	temp_authElement->m_active = active;
	temp_authElement->m_persistent = persistent;
	temp_authElement->m_allow_all_known_users = allow_all_known_users; 
	temp_authElement->m_realm.Strip(TRUE, TRUE);
	
	webserverAccessControlResource = temp_authElement; 
  
	return OpStatus::OK;
}

/* static */ OP_STATUS WebserverResourceDescriptor_AccessControl::MakeFromXML(WebserverResourceDescriptor_AccessControl *&webserverResourceDescriptor, XMLFragment &realm_element)
{
	webserverResourceDescriptor = NULL;

	/* Get path and text*/
	const uni_char *path = realm_element.GetAttribute(UNI_L("path"));

	const uni_char *name = realm_element.GetAttribute(UNI_L("name"));
	if (name == NULL) 
		return OpStatus::ERR;
	
	const uni_char *isactive = realm_element.GetAttribute(UNI_L("active"));
	
	BOOL active = FALSE;
	if (isactive && uni_stri_eq(isactive, "TRUE"))
	{
		active = TRUE;
	}
	
	const uni_char *all_known_users_have_access = realm_element.GetAttribute(UNI_L("allow-all-users"));
	BOOL allow_all_known_users = FALSE;
	if (all_known_users_have_access && uni_stri_eq(all_known_users_have_access, "TRUE"))
	{
		allow_all_known_users = TRUE;
	} 

	WebserverResourceDescriptor_AccessControl* temp;
	
	RETURN_IF_ERROR(WebserverResourceDescriptor_AccessControl::Make(temp, path, name, TRUE, active, allow_all_known_users));	
	
	OpAutoPtr<WebserverResourceDescriptor_AccessControl> temp_descriptor(temp);
	
	while (realm_element.HasMoreElements())
	{
		if (realm_element.EnterElement(UNI_L("user")))
		{
			const uni_char *name = realm_element.GetAttribute(UNI_L("name"));
			if (name)
			{
				RETURN_IF_ERROR(temp_descriptor->AddUser(name));
			}
		}
		else
		{
			realm_element.EnterAnyElement();
		}
		realm_element.LeaveElement();
	}
	
	webserverResourceDescriptor = temp_descriptor.release();
	return OpStatus::OK;
	
}

  /*Implementation of WebserverResourceDescriptor_Base*/
/*virtual*/
WebServerResourceType WebserverResourceDescriptor_AccessControl::GetResourceType() const
{
    return WEB_RESOURCE_TYPE_AUTH;
}

/*webserver authentication resource*/

OP_STATUS WebserverResourceDescriptor_AccessControl::GetUsers(OpVector<OpString> &users)
{
	OP_STATUS status = OpStatus::OK;
	
	OpHashIterator*	iterator = m_user_access_control_list.GetIterator();
	if (iterator != NULL && OpStatus::IsSuccess(iterator->First()))
	{
		do	
		{
			const uni_char *user = static_cast<const uni_char*>(iterator->GetKey());
			OpString *user_name = OP_NEW(OpString,());
			status = OpStatus::ERR_NO_MEMORY;
			if (user_name == NULL ||  OpStatus::IsError(status = user_name->Set(user)) || OpStatus::IsError(status = users.Add(user_name)))
			{
				OP_DELETE(user_name);
				break;
			}
		
		} while (OpStatus::IsSuccess(iterator->Next()));
	}

	OP_DELETE(iterator);
	
	return status;
}

OP_BOOLEAN
WebserverResourceDescriptor_AccessControl::AddUser(const OpStringC &username)
{
	if (g_webserverUserManager->GetUserFromName(username) == NULL)
		return OpStatus::ERR;
	
	OpString usernameStrip;
	
	RETURN_IF_ERROR(usernameStrip.Set(username));
	usernameStrip.Strip(TRUE, TRUE);
	
	if (usernameStrip.IsEmpty() == TRUE)
	{
		return OpStatus::ERR;
	}

	if (m_user_access_control_list.Contains(usernameStrip.CStr()) == TRUE)
	{
		return OpBoolean::IS_TRUE;
	}

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	OpString *user = OP_NEW(OpString, ());
	if (user == NULL || OpStatus::IsError(status = user->Set(usernameStrip)) ||  OpStatus::IsError(status = m_user_access_control_list.Add(user->CStr(), user)))
	{
		OP_DELETE(user);
		return status;
	}
	
	return OpBoolean::IS_FALSE;
}

/* virtual */ OP_BOOLEAN
WebserverResourceDescriptor_AccessControl::RemoveUser(const OpStringC &username)
{
	OpString usernameStrip;
	
	RETURN_IF_ERROR(usernameStrip.Set(username));
	usernameStrip.Strip(TRUE, TRUE);
	
	if (usernameStrip.IsEmpty() == TRUE)
	{
		return OpStatus::ERR;
	}
	
	OpString *user;
	OP_BOOLEAN status;
	if (OpStatus::IsSuccess(status = m_user_access_control_list.Remove(usernameStrip.CStr(), &user)))
	{
		OP_DELETE(user);
		return OpBoolean::IS_TRUE;
	}
	else
	{
		RETURN_IF_ERROR(status);
		return OpBoolean::IS_FALSE;
	}
}

BOOL 
WebserverResourceDescriptor_AccessControl::UserHasAccess(const OpStringC &username)
{ 
	if (m_allow_all_known_users) 
	{
		return g_webserverUserManager->GetUserFromName(username) ? TRUE : FALSE;
	}
	else
		return m_user_access_control_list.Contains(username.CStr());
}


OP_STATUS WebserverResourceDescriptor_AccessControl::CreateXML(XMLFragment &acl)
{
	if (m_persistent)
	{
		RETURN_IF_ERROR(acl.OpenElement(UNI_L("realm")));
		RETURN_IF_ERROR(acl.SetAttribute(UNI_L("allow-all-users"), m_allow_all_known_users ? UNI_L("TRUE") : UNI_L("FALSE")));
		RETURN_IF_ERROR(acl.SetAttribute(UNI_L("name"), m_realm.CStr()));
		RETURN_IF_ERROR(acl.SetAttribute(UNI_L("path"), GetResourceVirtualPath()));
		RETURN_IF_ERROR(acl.SetAttribute(UNI_L("active"), IsActive() ? UNI_L("TRUE") : UNI_L("FALSE")));
		
		OpAutoPtr<OpHashIterator> iter(m_user_access_control_list.GetIterator());
		
		const uni_char *username;
		OP_STATUS ret = iter->First();
		
		while (ret == OpStatus::OK)
		{
			RETURN_IF_ERROR(acl.OpenElement(UNI_L("user")));
	
			if ((username = static_cast<const uni_char*>(iter->GetKey())) != NULL)
			{
				RETURN_IF_ERROR(acl.SetAttribute(UNI_L("name"), username));
			}
			acl.CloseElement();
			ret = iter->Next();
		}	
	
		acl.CloseElement();
	}
	return OpStatus::OK;
}
WebSubServer::WebSubServer(WebServer* parent, unsigned long windowId, BOOL allowEcmaScriptSupport, BOOL persistent_access_control, BOOL m_allow_ecmascript_file_access)
	: m_windowId(windowId)
	, m_allowEcmaScriptSupport(allowEcmaScriptSupport)
	, m_serviceType(SERVICE_TYPE_SERVICE)
	, m_persistent_access_control(persistent_access_control)
	, m_allow_ecmascript_file_access(m_allow_ecmascript_file_access)
	, visible(TRUE)
	, visible_robots(FALSE)
	, visible_asd(TRUE)
	, visible_upnp(TRUE)
	, m_password_protected(0)
	, m_webserver(parent)
#ifdef GADGET_SUPPORT
	, m_is_gadget(FALSE)
#endif // GADGET_SUPPORT
{
}

WebSubServer::~WebSubServer()
{
#ifdef GADGET_SUPPORT	
	if (!m_is_gadget)		
		OpStatus::Ignore(SaveSubserverMountPoints());
#else
	OpStatus::Ignore(SaveSubserverMountPoints());
#endif	// GADGET_SUPPORT
	
	OpStatus::Ignore(SaveSubserverAccessControlResources(m_autoSaveAccessControlResourcesPath));
	
	m_webserver->CloseAllConnectionsToSubServer(this);
	
	for(UINT32 i=0; i<m_eventListeners.GetCount(); i++)
		m_eventListeners.Get(i)->OnWebSubServerDelete();
}

/*static */
OP_STATUS WebSubServer::Make(WebSubServer *&subServer, WebServer* server, unsigned long windowId, const OpStringC16 &subServerStoragePath, const OpStringC16 &subServerName, const OpStringC16 &subServerVirtualPath,  const OpStringC16 &subServerAuthor, const OpStringC16 &subServerDescription, const OpStringC16 &subServerOriginUrl, BOOL allowEcmaScriptSupport, BOOL persistent_access_control, BOOL allow_ecmascript_file_access)
{	
	if (server == NULL || subServerVirtualPath.IsEmpty())
	{
		return OpStatus::ERR;
	}

	subServer = NULL;
	
	if (windowId == NO_WINDOW_ID && allowEcmaScriptSupport == TRUE)
	{
		OP_ASSERT(!"Cannot add ecmascript support to subserver without a windowId");
		return OpStatus::ERR;
	}

	const uni_char *firstNonSlash = subServerVirtualPath.CStr();
	
	while (*firstNonSlash == '/' || *firstNonSlash == '\\')
	{
		firstNonSlash++;
	}
	
	if (uni_strpbrk(firstNonSlash, UNI_L(":/?#[]@!$&'()*+,;=\\<>")) != NULL)
	{
		return OpStatus::ERR;
	}
	
	
	WebSubServer *temp_subServer = OP_NEW(WebSubServer, (server, windowId, allowEcmaScriptSupport, persistent_access_control, allow_ecmascript_file_access));
	if (temp_subServer == NULL ) 
		return OpStatus::ERR_NO_MEMORY;
		
 	OpStackAutoPtr<WebSubServer> subServerAnchor(temp_subServer);
	
	RETURN_IF_ERROR(WebserverFileName::SecurePathConcat(temp_subServer->m_subServerStoragePath, subServerStoragePath));

	if (temp_subServer->m_subServerStoragePath[temp_subServer->m_subServerStoragePath.Length() - 1] != '/')
	{
		RETURN_IF_ERROR(temp_subServer->m_subServerStoragePath.Append("/"));
	}

	RETURN_IF_ERROR(temp_subServer->m_autoSaveAccessControlResourcesPath.Set(subServerStoragePath));
	RETURN_IF_ERROR(temp_subServer->m_autoSaveAccessControlResourcesPath.Append(WEB_PARM_SUBSERVER_ACCESS_FILE));
	if (persistent_access_control)
		RETURN_IF_ERROR(temp_subServer->LoadSubserverAccessControlResources());

	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	
	if (subServerName.IsEmpty())
	{
		/* Return error subServerName is not set */ 
		return OpStatus::ERR;
	}
	else if 	
	(
		OpStatus::IsError(status = WebserverFileName::SecurePathConcat(temp_subServer->m_subServerName, subServerName.CStr()))
	)
	{
		return status;	
	}

	/* htmlify Author */
	status = OpStatus::ERR_NO_MEMORY;
	
	if (subServerAuthor.IsEmpty())
	{
		RETURN_IF_ERROR(temp_subServer->m_subServerAuthor.Set(UNI_L("")));
	}
	else if 	
	(
		OpStatus::IsError(status =  temp_subServer->m_subServerAuthor.Set(subServerAuthor.CStr()))
	)
	{
		return status;
	}
	
	/* htmlify Description */	
	status = OpStatus::ERR_NO_MEMORY;
	
	if (subServerDescription.IsEmpty())
	{
		RETURN_IF_ERROR(temp_subServer->m_subServerDescription.Set(UNI_L("")));
	}
	else if 	
	(
		OpStatus::IsError(status =  temp_subServer->m_subServerDescription.Set(subServerDescription.CStr()))
	)
	{
		return status;
	}

	status = OpStatus::ERR_NO_MEMORY;
	OpString temp_subServerVirtualPath;
	if 	
	(
		OpStatus::IsError(status = temp_subServerVirtualPath.Set(subServerVirtualPath.CStr()))
	)
	{
		return status;
	}

	RETURN_IF_ERROR(temp_subServer->m_subServerVirtualPath.Set(temp_subServerVirtualPath));
	
#ifdef SELFTEST 
	/* When selftests are running, a subserver is installed in every window. To be able to have different virtual paths we add version numbers on it */
	if (g_selftest.running )
	{	
		int version = 1;
		while (server->GetSubServer(temp_subServer->m_subServerVirtualPath))
		{
			temp_subServer->m_subServerVirtualPath.Wipe();
			RETURN_IF_ERROR(temp_subServer->m_subServerVirtualPath.AppendFormat(UNI_L("%s_%d"), temp_subServerVirtualPath.CStr(), version));
			version++;
		}
	}
#endif // SELFTEST
		
	if (subServerOriginUrl.IsEmpty())
	{
		RETURN_IF_ERROR(temp_subServer->m_subServerOriginUrl.Set(UNI_L("")));
	}
	else 	
	{
		RETURN_IF_ERROR(temp_subServer->m_subServerOriginUrl.Set(subServerOriginUrl));
	}
	
#ifdef GADGET_SUPPORT	
	Window* window = g_windowManager->GetWindow(windowId);	
	
	if (window && window->GetGadget())
	{		
		temp_subServer->m_is_gadget = TRUE;
	}
	else 
	{
		RETURN_IF_ERROR(temp_subServer->LoadSubserverMountPoints());
	}
	
#else	
	RETURN_IF_ERROR(temp_subServer->LoadSubserverMountPoints());
#endif	// GADGET_SUPPORT
	
	subServer = subServerAnchor.release();

	return OpStatus::OK;
}

/*static */
OP_STATUS WebSubServer::MakeWithWindow(WebSubServer *&subServer, WebServer* server, const OpStringC16 &service_path, const OpStringC16 &subServerStoragePath, const OpStringC16 &subServerName, const OpStringC16 &subServerVirtualPath,  const OpStringC16 &subServerAuthor, const OpStringC16 &subServerDescription, const OpStringC16 &subServerOriginUrl, BOOL allowEcmaScriptSupport, BOOL persistent_access_control, BOOL allow_ecmascript_file_access)
{	
	/* FIXME :CREATE WINDOW */
	Window *opener_window = NULL;
	
	Window *window = g_windowManager->SignalNewWindow(opener_window);
	
	if (!window)
		return OpStatus::ERR;
	
	OpString service_path_index;
	RETURN_IF_ERROR(service_path_index.Set(service_path.CStr()));

	RETURN_IF_ERROR(service_path_index.Append(PATHSEP "index.html"));
//	window->SetVisibleOnScreen(FALSE);
	RETURN_IF_ERROR(window->OpenURL(service_path_index.CStr(), TRUE));
//	window->SetVisibleOnScreen(FALSE);
	
	RETURN_IF_ERROR(Make(subServer, server, window->Id(), subServerStoragePath, subServerName, subServerVirtualPath,  subServerAuthor, subServerDescription, subServerOriginUrl, allowEcmaScriptSupport, persistent_access_control, allow_ecmascript_file_access));
	RETURN_IF_ERROR(subServer->m_subServerEcmascriptResource.Set(service_path));
	return subServer->m_subServerEcmascriptResource.Append(PATHSEP);
}

void WebSubServer::SetPasswordProtected(int password_protected)
{
	m_password_protected = password_protected;
#ifdef WEB_UPLOAD_SERVICE_LIST
	m_webserver->UploadServiceList();
#endif // WEB_UPLOAD_SERVICE_LIST
}

const OpStringC16 &WebSubServer::GetSubServerOriginUrl()
{ 
#ifdef GADGET_SUPPORT
	if (m_subServerOriginUrl.IsEmpty())
	{
		OpGadget* gadget;
		Window* window;
		if ((window = g_windowManager->GetWindow(m_windowId)) && (gadget = window->GetGadget()))
		{
			OpStatus::Ignore(gadget->GetGadgetDownloadUrl(m_subServerOriginUrl));
		}
	}
#endif
	
	if (m_subServerOriginUrl.CStr() == NULL)
	{
		OpStatus::Ignore(m_subServerOriginUrl.Set(""));
	}
	
	return m_subServerOriginUrl;
}

const OpStringC16 &WebSubServer::GetVersion()
{ 
#ifdef GADGET_SUPPORT
	if (m_subServerVersion.IsEmpty())
	{
		OpGadget* gadget;
		Window* window;
		if ((window = g_windowManager->GetWindow(m_windowId)) && (gadget = window->GetGadget()) && gadget->GetClass())
		{
			OpStatus::Ignore(gadget->GetClass()->GetGadgetVersion(m_subServerVersion));
		}
	}
#endif
	
	if (m_subServerVersion.CStr() == NULL)
	{
		OpStatus::Ignore(m_subServerVersion.Set(""));
	}
	
	return m_subServerVersion;
}

#ifdef GADGET_SUPPORT
OpGadget *WebSubServer::GetGadget()
{
	Window* window = g_windowManager->GetWindow(m_windowId);
	if (window)
		return window->GetGadget();
	
	return NULL;
}

UINT32 WebSubServer::GetGadgetIconCount()
{
	OpGadget *gadget=GetGadget();
	
	if(!gadget)
		return 0;
		
	return gadget->GetGadgetIconCount();
}

OP_STATUS WebSubServer::GetGadgetIcon(UINT32 index, OpString& icon_path, INT32& specified_width, INT32& specified_height)
{
	OpGadget *gadget=GetGadget();
	
	if(!gadget)
		return 0;
		
	return gadget->GetGadgetIcon(index, icon_path, specified_width, specified_height, FALSE);
}
#endif

OP_STATUS WebSubServer::SetSubServerOriginUrl(const OpStringC16 &origin_url)
{
	RETURN_IF_ERROR(m_subServerOriginUrl.Set(origin_url.CStr()));
	return OpStatus::OK;
}

OP_STATUS WebSubServer::AddListener(WebSubserverEventListener *listener)
{
	if(m_eventListeners.Find(listener) < 0)
	{
		return m_eventListeners.Add(listener);
	}

	return OpStatus::OK;	
}

void WebSubServer::RemoveListener(WebSubserverEventListener *listener)
{
	if(m_eventListeners.Find(listener) > -1)
	{
		m_eventListeners.RemoveByItem(listener);
	}
}

void WebSubServer::SendNewRequestEvent(WebserverRequest *request)
{
	unsigned int i;
	unsigned int n = m_eventListeners.GetCount();

	for(i = 0; i < n; i++)
	{
		m_eventListeners.Get(i)->OnNewRequest(request);
	}
}

void WebSubServer::SendProgressEvent(double transfer_rate, UINT bytes_transfered, UINT total_bytes_transfered, BOOL out, WebserverRequest *request, BOOL request_finished)
{
	unsigned int i;
	unsigned int n = m_eventListeners.GetCount();
	
	UINT numberOfTCPConnections = m_webserver->NumberOfConnectionsToSubServer(this);
	
	for(i = 0; i < n; i++)
	{
		if (out == TRUE)
		{	
			m_eventListeners.Get(i)->OnTransferProgressOut(transfer_rate, total_bytes_transfered, request, numberOfTCPConnections, request_finished);
		}
		else
		{
			m_eventListeners.Get(i)->OnTransferProgressIn(transfer_rate, total_bytes_transfered, request, numberOfTCPConnections);
		}
	}
}


OP_BOOLEAN WebSubServer::AddSubserverResource( WebserverResourceDescriptor_Base *resource)
{
	if(!m_webserver|| !m_webserver->IsRunning())
		return OpStatus::ERR_NOT_SUPPORTED;
		
	if (resource->GetResourceType() == WEB_RESOURCE_TYPE_AUTH)
	{
		OP_ASSERT(!"Don't add authentication resource here, use AddSubServerAccessControlResource instead ");
		return OpStatus::ERR;
	}
	
	const uni_char *virtualPath = resource->GetResourceVirtualPath();
	
	for (unsigned int i = 0; i < m_subServerResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		WebserverResourceDescriptor_Base *aResource =  m_subServerResources.Get(i);

		if (aResource && uni_strcmp(aResource->GetResourceVirtualPath(), virtualPath) == 0)
			return OpBoolean::IS_TRUE;
	}
	
	RETURN_IF_ERROR(m_subServerResources.Add(resource));

	return OpStatus::OK;
}

WebserverResourceDescriptor_Base *WebSubServer::RemoveSubserverResource(const OpStringC16 &resourceVirtualPath)
{
	for (unsigned int i = 0; i < m_subServerResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		WebserverResourceDescriptor_Base *aResource =  m_subServerResources.Get(i);

		if (aResource && uni_strcmp(aResource->GetResourceVirtualPath(), resourceVirtualPath.CStr()) == 0)
		{
			m_subServerResources.Remove(i);
			return aResource;
		}
	}
	return NULL;
}

WebserverResourceDescriptor_Base *WebSubServer::RemoveSubserverResource(WebserverResourceDescriptor_Base *resource)
{
	if (resource && OpStatus::IsSuccess(m_subServerResources.RemoveByItem(resource)))
	{
		return resource;
	}
	else
	{
		return NULL;
	}
}

WebserverResourceDescriptor_Base * WebSubServer::GetSubserverResource(const OpStringC16 &resourceVirtualPath)
{
	for (unsigned int i = 0; i < m_subServerResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		WebserverResourceDescriptor_Base *aResource =  m_subServerResources.Get(i);

		if (aResource && uni_strcmp(aResource->GetResourceVirtualPath(), resourceVirtualPath.CStr()) == 0)
			return aResource;
	}

	return NULL;	
}

BOOL WebSubServer::HasActiveAccessControl()
{
	for (unsigned int i = 0; i < m_subServerAccessControlResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		if (m_subServerAccessControlResources.Get(i)->IsActive() == TRUE)
		{
			return TRUE;
		}
	}
	return FALSE;
}

void WebSubServer::RemoveAuthenticationUserFromAllResources(const OpStringC16 &username)
{
 	for (unsigned i = 0; i< m_subServerAccessControlResources.GetCount(); i++)
 	{
 		OpStatus::Ignore(m_subServerAccessControlResources.Get(i)->RemoveUser(username));
 	}
}

OP_STATUS WebSubServer::LoadSubserverMountPoints()
{
	OpString path;
	
	RETURN_IF_ERROR(path.Set(m_subServerStoragePath));
	RETURN_IF_ERROR(path.Append(SUBSERVER_PERSISTENT_MOUNTPOINTS));

	
	OpFile file;
	RETURN_IF_ERROR(file.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
	
	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (found == FALSE)
	{
		return OpStatus::OK;
	}	

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	XMLFragment mountpoints;
	
	OP_STATUS status;
	if (OpStatus::IsSuccess(status = mountpoints.Parse(static_cast<OpFileDescriptor*>(&file))))
	{
		if (mountpoints.EnterElement(UNI_L("MountPoints")))
		{
			m_persistent_mountpoints.DeleteAll();	
			while (mountpoints.HasMoreElements())
			{
				if (mountpoints.EnterElement(UNI_L("MountPoint")))
				{
					OP_STATUS status;
					MountPoint *mountpoint = OP_NEW(MountPoint, ());					
					RETURN_OOM_IF_NULL(mountpoint);
					if (
							OpStatus::IsError(status = mountpoint->m_key.Set( mountpoints.GetAttribute(UNI_L("key")))) ||							
							OpStatus::IsError(status = mountpoint->m_data.Set( mountpoints.GetAttribute(UNI_L("data")))) ||
							OpStatus::IsError(status = m_persistent_mountpoints.Add(mountpoint->m_key.CStr(), mountpoint))
						)
					{
						OP_DELETE(mountpoint);
						return status;
					}
				}
				else
				{
					mountpoints.EnterAnyElement();
				}
				mountpoints.LeaveElement();
			}
		}
	}
	
	
	return OpStatus::OK;
}

OP_STATUS WebSubServer::SaveSubserverMountPoints()
{
	XMLFragment mountpoints;

	RETURN_IF_ERROR(mountpoints.OpenElement(UNI_L("MountPoints")));
	
	OpStackAutoPtr<OpHashIterator> iterator(m_persistent_mountpoints.GetIterator());
	
	MountPoint *mountPoint;
	
	OP_STATUS ret = iterator->First();

	while (ret == OpStatus::OK)
	{
		if ((mountPoint = static_cast<MountPoint*>(iterator->GetData())) != NULL)
		{
			RETURN_IF_ERROR(mountpoints.OpenElement(UNI_L("MountPoint")));
			
			RETURN_IF_ERROR(mountpoints.SetAttribute(UNI_L("key"), mountPoint->m_key.CStr()));
			RETURN_IF_ERROR(mountpoints.SetAttribute(UNI_L("data"), mountPoint->m_data.CStr()));
			mountpoints.CloseElement();
		}
		ret = iterator->Next();
	}
	mountpoints.CloseElement();
	OP_DELETE(iterator.get());
	iterator.release();


	TempBuffer buffer;
	RETURN_IF_ERROR(mountpoints.GetXML(buffer, TRUE, NULL, TRUE));

	OpString path;
	
	RETURN_IF_ERROR(path.Set(m_subServerStoragePath));
	RETURN_IF_ERROR(path.Append(SUBSERVER_PERSISTENT_MOUNTPOINTS));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(path.CStr(), OPFILE_ABSOLUTE_FOLDER));
    RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
    RETURN_IF_ERROR(file.WriteUTF8Line(buffer.GetStorage()));	
	RETURN_IF_ERROR(file.Close());
	
	return OpStatus::OK;
}

OP_STATUS WebSubServer::SetPersistentData(const uni_char* section, const uni_char* key, const uni_char* value)
{
	MountPoint *mountpoint = NULL;
	OpStatus::Ignore(m_persistent_mountpoints.Remove(key, &mountpoint));
	OP_DELETE(mountpoint);

	OP_STATUS status;
	mountpoint = OP_NEW(MountPoint, ());					
	RETURN_OOM_IF_NULL(mountpoint);
	if (
			OpStatus::IsError(status = mountpoint->m_key.Set(key)) ||							
			OpStatus::IsError(status = mountpoint->m_data.Set(value)) ||
			OpStatus::IsError(status = m_persistent_mountpoints.Add(mountpoint->m_key.CStr(), mountpoint))
		)
	{
		OP_DELETE(mountpoint);
		return status;
	}	

	return OpStatus::OK;
}

const uni_char* WebSubServer::GetPersistentData(const uni_char* section, const uni_char* key)
{
	MountPoint *mountpoint;
	if (OpStatus::IsSuccess(m_persistent_mountpoints.GetData(key, &mountpoint)))
	{
		return mountpoint->m_data.CStr();
	}

	return NULL;
}

OP_STATUS WebSubServer::DeletePersistentData(const uni_char* section, const uni_char* key)
{
	MountPoint *mountpoint = NULL;
	RETURN_IF_ERROR(m_persistent_mountpoints.Remove(key, &mountpoint));
	OP_DELETE(mountpoint);
	return OpStatus::OK;
}
OP_STATUS WebSubServer::GetPersistentDataItem(UINT32 idx, OpString& key, OpString& data)
{
	OpStackAutoPtr<OpHashIterator> iterator(m_persistent_mountpoints.GetIterator());
	
	MountPoint *mountPoint;
	
	OP_STATUS ret = iterator->First();
	UINT32 i = 0;
	while (ret == OpStatus::OK)
	{
		
		if (i == idx && (mountPoint = static_cast<MountPoint*>(iterator->GetData())) != NULL)
		{
			RETURN_IF_ERROR(key.Set(mountPoint->m_key.CStr()));
			return data.Set(mountPoint->m_data.CStr());
			
		}
		ret = iterator->Next();
		i++;
	}	
	return OpStatus::ERR;
}


OP_BOOLEAN WebSubServer::AddSubServerAccessControlResource( WebserverResourceDescriptor_AccessControl *access_control_descriptor)
{
	const uni_char *virtualPath = access_control_descriptor->GetResourceVirtualPath();
	
	for (unsigned int i = 0; i < m_subServerAccessControlResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		WebserverResourceDescriptor_Base *aResource =  m_subServerAccessControlResources.Get(i);

		if (aResource && uni_strcmp(aResource->GetResourceVirtualPath(), virtualPath) == 0)
			return OpBoolean::IS_TRUE;
	}
	
	RETURN_IF_ERROR(m_subServerAccessControlResources.Add(access_control_descriptor));

	return OpBoolean::IS_FALSE;
}

		  
OP_BOOLEAN WebSubServer::RemoveSubserverAccessControlResource(const OpStringC16 &resourceVirtualPath, WebserverResourceDescriptor_AccessControl *&access_control_descriptor)
{
	access_control_descriptor =  NULL;

	OpString normalizedAuthPath;
	
	RETURN_IF_ERROR(WebserverFileName::SecurePathConcat(normalizedAuthPath, resourceVirtualPath));
	
	for (unsigned int i = 0; i < m_subServerAccessControlResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		WebserverResourceDescriptor_AccessControl *aResource =  m_subServerAccessControlResources.Get(i);

		if (aResource && uni_strcmp(aResource->GetResourceVirtualPath(), normalizedAuthPath.CStr()) == 0)
		{
			m_subServerAccessControlResources.Remove(i);
			access_control_descriptor = aResource;
			return OpBoolean::IS_TRUE;
		}
	}
	
	return OpBoolean::IS_FALSE;
}
		

OP_BOOLEAN WebSubServer::GetSubserverAccessControlResource(const OpStringC16 &resourceVirtualPath, WebserverResourceDescriptor_AccessControl *&access_control_descriptor)
{
	access_control_descriptor = NULL;
	
	OpString normalizedAuthPath;
	
	RETURN_IF_ERROR(WebserverFileName::SecurePathConcat(normalizedAuthPath, resourceVirtualPath));

	const uni_char *resourcePath;
	for (unsigned int i = 0; i < m_subServerAccessControlResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		WebserverResourceDescriptor_AccessControl *resource =  m_subServerAccessControlResources.Get(i);
		if (resource && (resourcePath = resource->GetResourceVirtualPath()) && uni_strncmp(resourcePath, normalizedAuthPath.CStr(), uni_strlen(resourcePath)) == 0)
		{
			access_control_descriptor = resource;
			return OpBoolean::IS_TRUE;
		}
	}
	
	return OpBoolean::IS_FALSE;	
}

OP_STATUS WebSubServer::SaveSubserverAccessControlResources(const OpStringC16 &path)
{
	if (m_persistent_access_control == FALSE)
		return OpStatus::OK;
	
	OpString filename;
	if (path.HasContent())
	{
		RETURN_IF_ERROR(filename.Set(path));
	}
	else
	{
		RETURN_IF_ERROR(filename.Set(m_autoSaveAccessControlResourcesPath));		
	}
	
	if (m_subServerAccessControlResources.GetCount() == 0)
		return OpStatus::OK;
	
	XMLFragment access_control_list;
	
	RETURN_IF_ERROR(access_control_list.OpenElement(UNI_L("acl")));
	
	for (unsigned int i = 0; i < m_subServerAccessControlResources.GetCount(); i++)
	{
		WebserverResourceDescriptor_AccessControl *auth_element = m_subServerAccessControlResources.Get(i);
		RETURN_IF_ERROR(auth_element->CreateXML(access_control_list));
	}

	access_control_list.CloseElement();
	
	TempBuffer buffer;
	RETURN_IF_ERROR(access_control_list.GetXML(buffer, TRUE, NULL, TRUE));

	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr(), OPFILE_ABSOLUTE_FOLDER));
    RETURN_IF_ERROR(file.Open(OPFILE_WRITE));
    RETURN_IF_ERROR(file.WriteUTF8Line(buffer.GetStorage()));	
	RETURN_IF_ERROR(file.Close());
	
	return OpStatus::OK;
}

OP_BOOLEAN WebSubServer::LoadSubserverAccessControlResources(const OpStringC16 &path)
{
	if (m_persistent_access_control == FALSE)
		return OpStatus::OK;

	OpString filename;
	if (path.HasContent())
	{
		RETURN_IF_ERROR(filename.Set(path));
	}
	else
	{
		RETURN_IF_ERROR(filename.Set(m_autoSaveAccessControlResourcesPath));		
	}
			
	OpFile file;
	RETURN_IF_ERROR(file.Construct(filename.CStr(), OPFILE_ABSOLUTE_FOLDER));
	
	BOOL found;
	RETURN_IF_ERROR(file.Exists(found));
	if (found == FALSE)
	{
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(file.Open(OPFILE_READ));

	XMLFragment access_control_list;
	
	OP_STATUS status;
	if (OpStatus::IsSuccess(status = access_control_list.Parse(static_cast<OpFileDescriptor*>(&file))))
	{
		if (access_control_list.EnterElement(UNI_L("acl")))
		{
			m_subServerAccessControlResources.DeleteAll();	
			while (access_control_list.HasMoreElements())
			{
				if (access_control_list.EnterElement(UNI_L("realm")))
				{
					WebserverResourceDescriptor_AccessControl *webserverResourceDescriptor;
					OP_BOOLEAN exists;
					RETURN_IF_MEMORY_ERROR(WebserverResourceDescriptor_AccessControl::MakeFromXML(webserverResourceDescriptor, access_control_list));
					if (webserverResourceDescriptor && OpStatus::IsError(exists = AddSubServerAccessControlResource(webserverResourceDescriptor)) && exists != OpBoolean::IS_FALSE)
					{
						OP_DELETE(webserverResourceDescriptor);
						RETURN_IF_ERROR(exists);
					}
				}
				else
				{
					access_control_list.EnterAnyElement();
				}
	
				access_control_list.LeaveElement();
			}
		}
		else
		{
			access_control_list.LeaveElement();	
		}		
		access_control_list.LeaveElement();	
	}
	else
	{
		RETURN_IF_MEMORY_ERROR(status);
	}
	
	RETURN_IF_ERROR(file.Close());
	return OpStatus::OK;
}

/*static*/ OP_BOOLEAN WebSubServer::AddSubServerAccessControlResource(OpVector<WebserverResourceDescriptor_AccessControl> &subServerAccessControlResources, WebserverResourceDescriptor_AccessControl *access_control_descriptor)
{
	const uni_char *virtualPath = access_control_descriptor->GetResourceVirtualPath();
	
	for (unsigned int i = 0; i < subServerAccessControlResources.GetCount(); i++)
	{
		/* checks if this virtual path has been added before */		
		WebserverResourceDescriptor_Base *aResource =  subServerAccessControlResources.Get(i);

		if (aResource && uni_strcmp(aResource->GetResourceVirtualPath(), virtualPath) == 0)
			return OpBoolean::IS_TRUE;
	}
	
	RETURN_IF_ERROR(subServerAccessControlResources.Add(access_control_descriptor));

	return OpBoolean::IS_FALSE;	
}

WebserverUploadedFileWrapper::WebserverUploadedFileWrapper(WebserverUploadedFile *webserverUploadedFile)
	:	m_webserverUploadedFilePtr(webserverUploadedFile)
{
	OP_ASSERT(m_webserverUploadedFilePtr != NULL);
	m_webserverUploadedFilePtr->Increment_Reference();
}

WebserverUploadedFileWrapper::~WebserverUploadedFileWrapper()
{
	m_webserverUploadedFilePtr->Decrement_Reference();
	if (m_webserverUploadedFilePtr->Get_Reference_Count()==0)
	{
		OP_DELETE(m_webserverUploadedFilePtr);
	}
}

WebserverUploadedFile*  WebserverUploadedFileWrapper::Ptr()
{
	return m_webserverUploadedFilePtr;
}


WebserverUploadedFile::WebserverUploadedFile(HeaderList *headerList, MultiPartFileType fileType)
 :	 m_multiPartHeaderList(headerList)
 	,m_fileType(fileType) 	
{
}

	
WebserverUploadedFile::~WebserverUploadedFile()
{
	OpFile deleteFile;

	if (OpStatus::IsSuccess(deleteFile.Construct(m_temporaryFilePath.CStr(), OPFILE_ABSOLUTE_FOLDER)))
	{
		
#ifdef USE_ASYNC_FILEMAN
		OpStatus::Ignore(deleteFile.DeleteAsync());
#endif // USE_ASYNC_FILEMAN		
		OpStatus::Ignore(deleteFile.Delete());
	}
	
	OP_DELETE(m_multiPartHeaderList);
}



OP_STATUS WebserverUploadedFile::GetFileName(OpString &fileName)
{
	fileName.Wipe();
	if (HeaderEntry *header = m_multiPartHeaderList->GetHeader("Content-Disposition"))
	{
		ParameterList *parameters = NULL;
		TRAPD(status, parameters = header->GetParametersL(PARAM_SEP_SEMICOLON | PARAM_SEP_WHITESPACE | PARAM_HAS_RFC2231_VALUES, KeywordIndex_HTTP_General_Parameters));
		RETURN_IF_ERROR(status);

		if (parameters)
			if (Parameters *filename_parameter = parameters->GetParameter("filename", PARAMETER_ANY))
			{
				const char *unsecureFilename = filename_parameter->Value();

				if (unsecureFilename)
				{
					WebserverFileName::SecureURLPathConcat(fileName, unsecureFilename);
				}
				else
				{
					RETURN_IF_ERROR(fileName.Set(""));
				}
			}
	}
	return OpStatus::OK;
}

OP_STATUS WebserverUploadedFile::GetRequestHeader(const OpStringC16 &headerName, OpAutoVector<OpString> *headerList)
{
	OpString8 headerNameUtf8;
	RETURN_IF_ERROR(headerNameUtf8.SetUTF8FromUTF16(headerName.CStr()));

	HeaderEntry *lastHeader = NULL;

	if (HeaderEntry *header = m_multiPartHeaderList->GetHeader(headerNameUtf8.CStr(), lastHeader))
	{
		OpString *headerValue = OP_NEW(OpString, ());
		OP_STATUS status = OpStatus::OK;
		if (headerValue == NULL 
			|| OpStatus::IsError(status = headerValue->Set(header->Value()))
			|| OpStatus::IsError(status = headerList->Add(headerValue)))
		{
			OP_DELETE(headerValue);
			return status;			
		}
		lastHeader = header;
	}
	return  OpStatus::OK;
}

/*static*/ 
OP_STATUS WebserverUploadedFile::Make(WebserverUploadedFile *&webserverUploadedFile, HeaderList *headerList, const OpStringC &temporaryFilePath, MultiPartFileType fileType)
{
	webserverUploadedFile = NULL;


	WebserverUploadedFile *temp_webserverUploadedFile = OP_NEW(WebserverUploadedFile, (headerList, fileType));
	
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	if (temp_webserverUploadedFile == NULL || OpStatus::IsError(status = temp_webserverUploadedFile->m_temporaryFilePath.Set(temporaryFilePath)))
	{
		return status;
	}
	
	webserverUploadedFile = temp_webserverUploadedFile;
	return OpStatus::OK; 
}

OP_STATUS WebserverResourceDescriptor_UPnP::Make(WebserverResourceDescriptor_UPnP *&ptr, const uni_char *virtual_path)
{
	ptr=OP_NEW(WebserverResourceDescriptor_UPnP, ());
	
	if(!ptr)
		return OpStatus::ERR_NO_MEMORY;
		
	return ptr->m_resourceVirtualPath.ConstructFromURL(virtual_path);
}

OP_STATUS WebserverResourceDescriptor_ASD::Make(WebserverResourceDescriptor_ASD *&ptr, const uni_char *virtual_path)
{
	ptr=OP_NEW(WebserverResourceDescriptor_ASD, ());
	
	if(!ptr)
		return OpStatus::ERR_NO_MEMORY;
		
	return ptr->m_resourceVirtualPath.ConstructFromURL(virtual_path);
}


OP_STATUS WebserverResourceDescriptor_Robots::Make(WebserverResourceDescriptor_Robots *&ptr, const uni_char *virtual_path)
{
	ptr=OP_NEW(WebserverResourceDescriptor_Robots, ());
	
	if(!ptr)
		return OpStatus::ERR_NO_MEMORY;
		
	return ptr->m_resourceVirtualPath.ConstructFromURL(virtual_path);
}

#endif //WEBSERVER_SUPPORT
