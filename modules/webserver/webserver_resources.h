/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation -- script virtual resources
 */

#ifndef WEBSERVER_RESOURCES_H_
#define WEBSERVER_RESOURCES_H_

#include "modules/webserver/webresource_base.h"
#include "modules/webserver/webserver_user_management.h"
#include "modules/webserver/webserver_filename.h"
#include "modules/dochand/win.h"
#ifdef GADGET_SUPPORT
#include "modules/gadgets/OpGadget.h"
#endif

#include "modules/prefsfile/prefsfile.h"

#define NO_WINDOW_ID		(~0U)
#define SUBSERVER_PERSISTENT_MOUNTPOINTS UNI_L("mounted_folders.xml")
#define DEFAULT_CONTENT_TYPE "text/html; charset=utf-8"

class WebserverFileName;
class WebServerConnectionListener;
class WebserverRequest;
class WebServerConnection;
class WebResource;
class XMLFragment;

enum WebServerFailure
{
	WEB_FAIL_NO_FAILURE,			/* no failure after all! */
	WEB_FAIL_REDIRECT_TO_DIRECTORY, /* The request has been redirected */
	WEB_FAIL_GENERIC,				/* unspecified error, generates the 503 response code */
	WEB_FAIL_OOM,					/* the server could not perform the operation due to an out-of-memory error */
	WEB_FAIL_PORT_BUSY,				/* the requested server port is busy */
	WEB_FAIL_FILE_NOT_AVAILABLE,	/* file does not exist, or is protected, or the request names something not a file */
	WEB_FAIL_NOT_AUTHENTICATED,		/* authentication missing or failed altogether */
	WEB_FAIL_METHOD_NOT_SUPPORTED,	/* method not supported for this resource */
	WEB_FAIL_REQUEST_SYNTAX_ERROR	/* if the request could no be understood, generates the 400 response code*/
	
};

enum WebServerResourceType
{
	 WEB_RESOURCE_TYPE_SCRIPT			/* Ecmascript resource */
	,WEB_RESOURCE_TYPE_AUTH				/* Authentication resource */
	,WEB_RESOURCE_TYPE_STATIC			/* Static file resource */
	,WEB_RESOURCE_TYPE_UPNP				/* UPnP Service Discovery */
	,WEB_RESOURCE_TYPE_ROBOTS			/* robots.txt */
	,WEB_RESOURCE_TYPE_ASD				/* ASD "replica" service */
	,WEB_RESOURCE_TYPE_CUSTOM			/* Future Custom resources */
};

class WebserverResourceDescriptor_Base 
{
public:
	WebserverResourceDescriptor_Base();
	virtual ~WebserverResourceDescriptor_Base(){}

	virtual const uni_char* GetResourceVirtualPath() const { return m_resourceVirtualPath.GetPathAndFileNamePointer(); }

	virtual WebServerResourceType GetResourceType() const = 0;
	
	virtual BOOL IsActive() const { return m_active; }
	
	virtual void SetActive(BOOL active) { m_active = active; }

protected:
	
	/* configuration data */
	WebserverFileName m_resourceVirtualPath;
	
	BOOL m_active;
};

/// Class for UPnP resource
class WebserverResourceDescriptor_UPnP: public WebserverResourceDescriptor_Base
{
public:
	static OP_STATUS Make(WebserverResourceDescriptor_UPnP *&ptr, const uni_char *virtual_path);
	
	virtual WebServerResourceType GetResourceType() const { return WEB_RESOURCE_TYPE_UPNP; }
};

/// Class for ASD resource
class WebserverResourceDescriptor_ASD: public WebserverResourceDescriptor_Base
{
public:
	static OP_STATUS Make(WebserverResourceDescriptor_ASD *&ptr, const uni_char *virtual_path);
	
	virtual WebServerResourceType GetResourceType() const { return WEB_RESOURCE_TYPE_ASD; }
};

/// Class for robots.txt
class WebserverResourceDescriptor_Robots: public WebserverResourceDescriptor_Base
{
public:
	static OP_STATUS Make(WebserverResourceDescriptor_Robots *&ptr, const uni_char *virtual_path);
	
	virtual WebServerResourceType GetResourceType() const { return WEB_RESOURCE_TYPE_ROBOTS; }
};

class WebserverResourceDescriptor_AccessControl : public WebserverResourceDescriptor_Base
{
public :
	
	/** Create an access controlled realm.
	 * 
	 * @param webserverResourceDescriptor(out) 	The object that is created.
	 * @param path 								The path of the realm. This path is also used as a unique id for the realm. Path MUST have been url decoded.
	 * @param realm_text 						The text that pops up in the authentication dialog. 
	 * @param persistent						If TRUE, The access control list for this service will be saved to disk.
	 * @param active							If TRUE, The access control system is active.
	 * @param allow_all_known_users				If TRUE, the access control list will be ignored and all known authenticated users will be accepted.
	 * 
	 * @return 	OpStatus::OK 			If success.
	 * 			OpStatus::ERR 			If realm name is contains illegal characters.
	 * 			OpStatus::ERR_NO_MEMORY If OOM.
	 */
	static OP_STATUS Make(WebserverResourceDescriptor_AccessControl *&webserverResourceDescriptor, const OpStringC &path, const OpStringC &realm_text, BOOL persistent, BOOL active = TRUE, BOOL allow_all_known_users = FALSE);

	static OP_STATUS MakeFromXML(WebserverResourceDescriptor_AccessControl *&webserverResourceDescriptor, XMLFragment &realm_element);
	
	virtual ~WebserverResourceDescriptor_AccessControl(){}

	/** Check if this realm is active.
	 * 
	 * Default value is TRUE.
	 * 
	 * @return TRUE if access control is on.
	 */
	virtual BOOL IsActive() const { return m_active; }
	
	/** Set authentication on/off.
	 * 
	 * Default value is TRUE.
	 * 
	 * @param active If TRUE, authentication will be turned on.
	 */	
	virtual void SetActive(BOOL active) { m_active = active; }
	
	/** Give a user access to this realm.
	 * 
	 * @param	username		The username, the user must exist in the global user list controlled by g_webserverUserManager
	 *
	 * 			OpStatus::ERR_NO_MEMORY If OOM.
	 *			OpBoolean::ERR 			If user does not exist in the global user list.
	 *			OpBoolean::IS_TRUE 		If user is already  added, and the user is NOT added.
	 * 			OpBoolean::IS_FALSE		If success.
	 */ 
	OP_BOOLEAN AddUser(const OpStringC &username); 
	
	/** Remove access for user
	 * 
	 * @param	username		The username, the user must exist in the global user list controlled by g_webserverUserManager
	 *
	 * 			OpStatus::ERR_NO_MEMORY If OOM.
	 *			OpBoolean::IS_TRUE 		If user existed and the user was removed.
	 * 			OpBoolean::IS_FALSE		If the user did not exist.
	 */ 
	OP_BOOLEAN RemoveUser(const OpStringC &username);
	
	/** Check if a user has access to this services. 
	 * 
	 * @param username 	The username to check.
	 * 
	 * @return TRUE if user exists, FALSE otherwise.
	 */
	BOOL UserHasAccess(const OpStringC &username);

	/** Check if all users that are authenticated have access */
	BOOL AllKnownUsersHaveAccess() { return m_allow_all_known_users; }
	
	/*** Give access to all users that are authenticated
	 * 
	 * @param access If true all users that have authenticated have access to this service 
	 */
	void  SetAllKnownUsersHaveAccess(BOOL access) { m_allow_all_known_users = access; }
	/** Get all users in this realm.
	 * 
	 * @return A vector of users added on this realm. 
	 */ 
	OP_STATUS GetUsers(OpVector<OpString>  &users);
	
	/** Get the name of this realm. */
	const OpStringC &GetRealm() const { return m_realm; } 
	
	/**Implementation of WebserverResourceDescriptor_Base. */
	WebServerResourceType GetResourceType() const;		
	
	/** Create xml for this realm */
	OP_STATUS CreateXML(XMLFragment &user_xml);

private:
	WebserverResourceDescriptor_AccessControl();
	
	/**data*/
	OpString m_realm;

	OpAutoVector<OpString> m_userNames;
	
	OpAutoStringHashTable< OpString > m_user_access_control_list; /* Objects are owned by WebserverUserManager */

	/** Realms with m_needsAccesRight == TRUE, 
	 * Requires that the caller has the access right to add and remove users. 
	 **/
	BOOL m_persistent;
	BOOL m_allow_all_known_users;
	
	friend class WebResource_Auth;
	friend class WebSubServer; 
};

/* Descriptor for file resource. */
class WebserverResourceDescriptor_Static : public WebserverResourceDescriptor_Base
{
public:

	static WebserverResourceDescriptor_Static *Make(const OpStringC &realPath, const OpStringC &virtualPath = UNI_L(""));

	WebServerResourceType GetResourceType() const;
	
	const uni_char* GetResourceRealPath() const;

	BOOL IsFile() { return m_isFile; }
private:
	WebserverResourceDescriptor_Static(BOOL isFile);
	
	WebserverFileName m_resourceRealPath;
	
	BOOL m_isFile;
};

class WebSubserverEventListener
{
public:
	/** The service has received a new http request  
	 * 
	 * @param request	 		The request. This request is owned by the webserver. Do not store the pointer for later use. 
	 * 							Use request->Copy() method for storing the request.
	 */
	virtual void OnNewRequest(WebserverRequest *request) = 0;

	/** Progress when a client is uploading data to service  
	 * 
	 * @param transfer_rate 	The transfer rate in Bytes/s.
	 * @param bytes_transfered	Total bytes transfered on this request.
	 * @param request 			The request. This request is owned by the webserver. Do not store the pointer for later use. 
	 * 							Use request->Copy() method for storing the request.
	 * @param tcp_connections 	Total number of tcp connections to this service.
	 */
	virtual void OnTransferProgressIn(double transfer_rate, UINT bytes_transfered,  WebserverRequest *request, UINT tcp_connections) = 0;

	/** Progress when a client is uploading data to service  
	 * 
	 * @param transfer_rate 	The transfer rate in Bytes/s.
	 * @param bytes_transfered	Total bytes transfered on this request.
	 * @param request 			The request. This request is owned by the webserver. Do not store the pointer for later use. 
	 * 							Use request->Copy() method for storing the request.
	 * @param tcp_connections 	Total number of tcp connections to this service .
	 * @param request_finished  TRUE if this was the last progress event on this request, 
	 * 							FALSE otherwise.
	 */
	virtual void OnTransferProgressOut(double transfer_rate, UINT bytes_transfered, WebserverRequest *request, UINT tcp_connections, BOOL request_finished) = 0;
	
	/** The WebSubServer is being deleted
	 */
	virtual void OnWebSubServerDelete(){}

	virtual ~WebSubserverEventListener(){}
};

class WebSubServer 
#ifdef GADGET_SUPPORT
	: public OpPersistentStorageListener/* Same as "service" in the javascript api */
#endif // GADGET_SUPPORT
{
	friend class WebServer;
	friend class WebServerConnection;
	friend class WebServerConnectionListener;
public:
	~WebSubServer();
	
	enum ServiceType 
	{
		SERVICE_TYPE_BUILT_IN_ROOT_SERVICE,
		SERVICE_TYPE_CUSTOM_ROOT_SERVICE,
		SERVICE_TYPE_SERVICE
	};
	
	/** Various simple getters */
	unsigned long  GetWindowId() const { return m_windowId; }
	
	void  SetWindowId(unsigned long window_id) { m_windowId = window_id;}
	
	BOOL IsRootService() { return (m_serviceType == SERVICE_TYPE_BUILT_IN_ROOT_SERVICE || m_serviceType == SERVICE_TYPE_CUSTOM_ROOT_SERVICE) ? TRUE : FALSE; }
	
	/** 0 == no password needed 
	*   1 == requires a password 
	*   More will be added in the future.
	*/
	int GetPasswordProtected() { return m_password_protected; }

	void SetPasswordProtected(int password_protected);

	ServiceType GetServiceType() const { return m_serviceType; }
	
	const OpStringC16 &GetSubServerVirtualPath() const { return m_subServerVirtualPath; }
	
	OP_STATUS ChangeSubServerVirtualPath(const OpStringC &virualPath) { return m_subServerVirtualPath.Set(virualPath); }
	
	const OpStringC16 &GetSubServerName() const { return m_subServerName; }
	
	const OpStringC16 &GetSubServerAuthor() const { return m_subServerAuthor; }

	const OpStringC16 &GetSubServerDescription() const { return m_subServerDescription; }
	
	const OpStringC16 &GetSubServerOriginUrl();
	
	const OpStringC16 &GetVersion();

	OP_STATUS SetSubServerOriginUrl(const OpStringC16 &origin_url);
	
	const OpStringC16 &GetSubServerStoragePath() const  { return m_subServerStoragePath; }

	const OpStringC16 &GetSubServerAuthFilename() const { return m_autoSaveAccessControlResourcesPath; }
	
	BOOL AllowEcmaScriptSupport() { return m_allowEcmaScriptSupport; }
	
	BOOL AllowEcmaScriptFileAccess() { return m_allow_ecmascript_file_access; }
	
	/** Create a new subserver and install it in a window.
	 * A subserver is a collection of server resources, and behave almost
	 * like a server. 
	 * 
	 * After calling make, the subserver must be added to the server using
	 * g_webserver->AddSubServer(subserver);
	 * 
	 * The uri for the subserver is http://<device>.<username>.<proxy.com>/<sub_server_uri_name>/
	 *
	 * @param sub_server (out) 				The new subserver.
	 * @param server						The parent server
	 * @param window_id  	 				The window where the subserver will be installed.
	 * @param sub_server_storage_path		File path to an "sandbox" where sub server can keep state data for this service.
	 * @param sub_server_uri_name 			The uri path that will trigger resources in this subserver. NB! subServerUriName must not contain slashes !
	 * @param sub_server_name 				The name that will displayed for this subserver.
	 * @param sub_server_description		A short description of the subserver, that may be displayed.
	 * @param sub_server_origin_url		The url from where this subserver (service) was downloaded.
	 * @param allow_ecma_script_support		If TRUE, this subserver can run server-side ecmascript,
	 *										using the webserver object.
	 *
	 * @return						OpStatus::ERR 			If windowId == NO_WINDOW_ID && allowEcmaScriptSupport == TRUE, or if subServerUriName contains any '/'.
	 * 								OpStatus::ERR_NO_MEMORY If OOM.
	 * 								OpStatus::OK 			If success. 
	 */ 	
	static OP_STATUS Make(WebSubServer *&sub_server,
	                      WebServer* server,
	                      unsigned long window_id,
	                      const OpStringC16 &sub_server_storage_path,
	                      const OpStringC16 &sub_server_display_name,
	                      const OpStringC16 &sub_server_uri_name,
	                      const OpStringC16 &sub_server_author,
	                      const OpStringC16 &sub_server_description,
	                      const OpStringC16 &sub_server_origin_url,
	                      BOOL allow_ecma_script_support = FALSE,
	                      BOOL persistent_access_control = TRUE,
	                      BOOL allow_ecmascript_file_access = TRUE
	                      );

	/** Same as Make above, but will also create a window.
	 * 
	 * A subserver is a collection of server resources, and behave almost
	 * like a server. 
	 * 
	 * The uri for the subserver is http://device.username.proxy.com/subServerUriName/
	 *
	 * @param sub_server (out) 				The new subserver.
	 * @param server						The server.
	 * @param service_path					The path to a directory containing the service. The directory 
	 * 										MUST contain index.html
	 * @param sub_server_storage_path		File path to a "sandbox" where sub server can keep state data for this service.
	 * @param sub_server_uri_name 			The uri path that will trigger resources in this subserver. NB! subServerUriName must not contain slashes !
	 * @param sub_server_name 				The name that will displayed for this subserver.
	 * @param sub_server_description		A short description of the subserver, that may be displayed.
	 * @param sub_server_download_url		The url from where this subserver (service) was downloaded.
	 * @param allow_ecma_script_support		If TRUE, this subserver can run server-side ecmascript,
	 *										using the webserver object.
	 * @param allow_ecmascript_file_access	Give ecmascript file access.
	 *
	 * @return						OpStatus::ERR 			If subServerUriName contains any '/'.
	 * 								OpStatus::ERR_NO_MEMORY If OOM.
	 * 								OpStatus::OK 			If success. 
	 */	
	static OP_STATUS MakeWithWindow(WebSubServer *&sub_server,
	                      WebServer* server,
	                      const OpStringC16 &service_path,
	                      const OpStringC16 &sub_server_storage_path,
	                      const OpStringC16 &sub_server_display_name,
	                      const OpStringC16 &sub_server_uri_name,
	                      const OpStringC16 &sub_server_author,
	                      const OpStringC16 &sub_server_description,
	                      const OpStringC16 &sub_server_origin_url,
	                      BOOL allow_ecma_script_support = FALSE,
	                      BOOL persistent_access_control = TRUE,
	                      BOOL allow_ecmascript_file_access = FALSE
	                      );
	
	/** Add an event listener to the subserver.
	 * 
	 * The caller of this function must manage and delete the listener.
	 *
	 * @param listener 	The event listener to be added.
	 * 
	 * @return			OpStatus::ERR_NO_MEMORY or OpStatus::OK.
	 */
	OP_STATUS AddListener(WebSubserverEventListener *listener);
	
	/** Remove an event listener from the subserver.
	 * 
	 * The caller of this function must manage and delete the listener.
	 *
	 * @param listener The event listener to be removed.
	 */	
	void RemoveListener(WebSubserverEventListener *listener);
		
	/** Adds a resource to the sub server (for example a javascript resource, or a static html resource)
	 *	 
	 * WebSubServer takes ownership over resource_descriptor when added. 
	 * 
	 * @param resource_descriptor The resource_descriptor to add
	 * 
	 * @return 			OpBoolean::IS_FALSE 	If a resource did not exist on the same path, and the resource was added.
	 * 					OpBoolean::IS_TRUE 		If a resource with the same virtual path already is added. In this case, 
	 * 											the resource will not be added and the caller MUST delete the resource_descriptor.
	 * 					
	 * 					OpStatus::ERR_NO_MEMORY If out of memory.
	 */ 	
	OP_BOOLEAN AddSubserverResource( WebserverResourceDescriptor_Base *resource_descriptor);

	/** Remove the WebserverResourceDescriptor_Base resource with resourceVirtualPath as virtual path
	 * 
	 * WebSubServer looses the ownership over resource_descriptor when removed.	 	
	 * 
	 * @param resource_virtual_path (in) the virtual path of the resource to be removed
	 *
	 * @return the pointer to the resource_descriptor that will be removed. The caller of this function must now delete the resource.
	 */
	WebserverResourceDescriptor_Base *RemoveSubserverResource(const OpStringC16 &resource_virtual_path);
	
	/** Removes a WebserverResourceDescriptor_Base resource
	 * 
	 * WebSubServer looses the ownership over resource_descriptor when removed.	 	
	 * 
	 * @param resource The pointer to the resource to be removed
	 *
	 * @return the pointer to the resource_descriptor that will be removed. The caller of this function must now delete the resource.
	 */
	WebserverResourceDescriptor_Base *RemoveSubserverResource(WebserverResourceDescriptor_Base *resource);	

	/** Get the WebserverResourceDescriptor_Base resource with resourceVirtualPath as virtual path
	 * 
	 * @param resource_virtual_path (in) the virtual path of the resource to be removed
	 *  	
 	 * @return the pointer to the resource_descriptor
	 */
	WebserverResourceDescriptor_Base *GetSubserverResource(const OpStringC16 &resource_virtual_path);

	/** Get all resources in this subserver 
	 * 
	 * @return The vector of resources. 
	 */
	const OpAutoVector<WebserverResourceDescriptor_Base> *GetSubserverResources() const { return &m_subServerResources ;}	 	
	
	/** Checks if this subserver requires authentication for any paths
	 * 
	 * @param return TRUE if any of the authentication realms are active.
	 */
	BOOL HasActiveAccessControl();
	
	/** Save the auth resources to file 
	 * 
	 * @param file If filename == "" the filename set by SetAutoSaveAccessControlResources() is used.
	 * 
	 * @return OpStatus::ERR 			If filename is a directory path.
	 * @return OpStatus::ERR_NO_MEMORY 	If OOM.
	 * @return OpStatus::OK 			If success.
	 */
	OP_STATUS SaveSubserverAccessControlResources(const OpStringC16 &filename = UNI_L(""));

	/** Load the auth resources from file into this subserver. 
	 * 
	 * @param file If filename == "" the filename set by SetAutoSaveAccessControlResources() is used.
	 * 
	 * @return OpBoolean::IS_FALSE 			If filename did not exist.
	 * @return OpStatus::ERR_NO_MEMORY 		If OOM.
	 * @return OpStatus::ERR				If the xml parsing failed.
	 * @return OpBoolean::IS_TRUE 			if success.
	 */	
	OP_BOOLEAN LoadSubserverAccessControlResources(const OpStringC16 &filename = UNI_L(""));	
	
	/** Get all authentication resources in this subserver 
	 * 
	 * @return The vector of resources. 
	 */	
	const OpAutoVector<WebserverResourceDescriptor_AccessControl> *GetSubserverAccessControlResources() const { return &m_subServerAccessControlResources ;}
	
	/** Adds a authentication resource  to the sub server
	 * Use this to have authentication on uri's
	 *	 
	 * WebSubServer takes ownership over access_control_descriptor.
	 * 
	 * @param access_control_descriptor The auth-element to add.
	 * 
	 * @return  IS_FALSE 		If the resource did not already exist, and the resource has been added successfully.
	 * 			IS_TRUE 		If this already exists. The resource is not added. 	
	 *  		ERR_NO_MEMORY 	if out of memory
	 */
	OP_BOOLEAN AddSubServerAccessControlResource( WebserverResourceDescriptor_AccessControl *access_control_descriptor);

	/** Removes the WebserverResourceDescriptor_AccessControl resource with resourceVirtualPath as virtual path
	 *	
	 * WebSubServer looses the ownership over resource_descriptor when removed.
	 * 	 	
	 * @param resourceVirtualPath (in) 		The virtual path of the resource to be removed
	 * @param access_control_descriptor (out)	
	 *
	 * @return 	IS_TRUE 		If the resource was found and removed.
	 * 			IS_FALSE 		If the resource was not found.
	 * 			ERR_NO_MEMORY 	if out of memory.
	 */
	OP_BOOLEAN RemoveSubserverAccessControlResource(const OpStringC16 &resourceVirtualPath, WebserverResourceDescriptor_AccessControl *&access_control_descriptor);

	/** Get the WebserverResourceDescriptor_AccessControl resource with resourceVirtualPath as virtual path
	 *
	 * @param resourceVirtualPath The virtual path of the resource to be removed
	 * @param 
	 * 
	 * @return 	IS_TRUE 		If the resource already has been added.
	 * 			IS_FALSE 		If the resource was not found
	 * 			ERR_NO_MEMORY 	if out of memory.
 	 */	
	OP_BOOLEAN GetSubserverAccessControlResource(const OpStringC16 &resourceVirtualPath, WebserverResourceDescriptor_AccessControl *&access_control_descriptor);
	
	
	void RemoveAuthenticationUserFromAllResources(const OpStringC16 &userName);
	
	/// Return TRUE if the SubServer is visible (and so it has to be uploaded to ASD)
	BOOL IsVisible() { return visible; }
	/// Return if the service can be indexed by Search Engines
	BOOL IsVisibleRobots() { return visible && visible_robots; }
	/// Return if the service can be sent to ASD
	BOOL IsVisibleASD() { return visible && visible_asd; }
	/// Return if the service can be published via UPnP Service Discovery
	BOOL IsVisibleUPNP() { return visible && visible_upnp; }
	
	/// Set if the service can be indexed by Search Engines
	void SetVisibleRobots(BOOL b) { visible_robots=b; }
	/// Set if the service can be sent to ASD
	void SetVisibleASD(BOOL b) { visible_asd=b; }
	/// Set if the service can be published via UPnP Service Discovery
	void SetVisibleUPNP(BOOL b) { visible_upnp=b; }
	
#ifdef GADGET_SUPPORT
	/** From OpPersistentStorageListener */
	OP_STATUS LoadSubserverMountPoints();
	OP_STATUS SaveSubserverMountPoints();
	
	// Get the Icon count
	UINT32 GetGadgetIconCount();
	// Get the required Icon
	OP_STATUS GetGadgetIcon(UINT32 index, OpString& icon_path, INT32& specified_width, INT32& specified_height);
	
	virtual OP_STATUS		SetPersistentData(const uni_char* section, const uni_char* key, const uni_char* value);
	virtual const uni_char*	GetPersistentData(const uni_char* section, const uni_char* key);
	virtual OP_STATUS		DeletePersistentData(const uni_char* section, const uni_char* key);
	virtual OP_STATUS		GetPersistentDataItem(UINT32 idx, OpString& key, OpString& data);
	virtual OP_STATUS		GetApplicationPath(OpString& path){ if (m_subServerEcmascriptResource.HasContent()) return path.Set(m_subServerEcmascriptResource.CStr()); else { OP_ASSERT(!"Should not be called");  return OpStatus::ERR;} }
	virtual OP_STATUS		GetStoragePath(OpString& storage_path){ return storage_path.Set(m_subServerStoragePath.CStr());}	
#endif //GADGET_SUPPORT
private:

#ifdef GADGET_SUPPORT
	// Return the gadget associated to the webserver
	OpGadget *GetGadget();
#endif
	WebSubServer(WebServer* parent, unsigned long windowId, BOOL allowEcmaScriptSupport, BOOL persistent_access_control, BOOL allow_ecmascript_file_access = TRUE);

	static OP_BOOLEAN AddSubServerAccessControlResource(OpVector<WebserverResourceDescriptor_AccessControl> &subServerAccessControlResources, WebserverResourceDescriptor_AccessControl *access_control_descriptor);
	
	void SendNewRequestEvent(WebserverRequest *request);
	void SendProgressEvent(double transfer_rate, UINT bytes_transfered, UINT total_bytes_transfered, BOOL out, WebserverRequest *request, BOOL request_finished);
	
	/** Virtual path (url-path used to access this resource) */	
	OpString m_subServerVirtualPath;	

	/** Subserver name, as displayed to user*/
	OpString m_subServerName;
		
	OpString m_subServerDescription;
	
	OpString m_subServerOriginUrl;

	OpString m_subServerVersion;

	OpString m_subServerAuthor;

	/** posted files and private file storage will end up here */	
	OpString m_subServerStoragePath;
	
	OpString m_subServerEcmascriptResource;	

	unsigned long m_windowId;

	/** List of Authentication elements */
	OpAutoVector<WebserverResourceDescriptor_AccessControl> m_subServerAccessControlResources;
		
	/** List of resources in this subserver */
	OpAutoVector<WebserverResourceDescriptor_Base> m_subServerResources;
				
	/** Event listeners */
	OpVector<WebSubserverEventListener> m_eventListeners;

	BOOL m_allowEcmaScriptSupport;
	
	ServiceType m_serviceType;
	
	OpString m_autoSaveAccessControlResourcesPath;
	
	BOOL m_persistent_access_control;
	
	BOOL m_allow_ecmascript_file_access;
	
	// True if the service is visible
	BOOL visible;
	// True if the service can be indexed (via ribots.txt)
	BOOL visible_robots;
	// True if the service can posted to ASD
	BOOL visible_asd;
	// True if the service can presented via UPnP Services Discovery
	BOOL visible_upnp;

	int m_password_protected;

	class MountPoint
	{
	public:
		OpString m_key;
		OpString m_data;
	};
	
	OpAutoStringHashTable< MountPoint > m_persistent_mountpoints;

	WebServer* m_webserver;

#ifdef GADGET_SUPPORT	
	BOOL m_is_gadget;
#endif // GADGET_SUPPORT
};


									/* Low lever classes */
#define CRLF "\x0D\x0A"
#define UNI_CRLF "\x000D\x000A"

/** General web resource: something being served to the client. 
    Subclass this for particular types of resources.
	*/
	
class WebResourceWrapper
{
public:
	WebResourceWrapper(WebResource *webResource);	
	~WebResourceWrapper();
	WebResource *Ptr();
private:
	WebResource *m_webResourcePtr;
};

class WebserverUploadedFile;

class WebserverUploadedFileWrapper
{
public:
	WebserverUploadedFileWrapper(WebserverUploadedFile *webserverUploadedFile);	
	~WebserverUploadedFileWrapper();
	WebserverUploadedFile *Ptr();
private:
	WebserverUploadedFile *m_webserverUploadedFilePtr;
};

class WebserverUploadedFile : public OpReferenceCounter
{
public:
	enum MultiPartFileType				
	{
		WEB_FILETYPE_NOT_SET,
		WEB_FILETYPE_UNKNOWN,
		WEB_FILETYPE_FORM_TEXT,	
		WEB_FILETYPE_FILE
	};
	
	~WebserverUploadedFile();
	
	/*NOTE: WebserverUploadedFile takes over ownership over headerList*/
	static	OP_STATUS Make(WebserverUploadedFile *&webserverUploadedFile, HeaderList *headerList, const OpStringC &temporaryFilePath, MultiPartFileType fileType);
	HeaderList* GetHeaderList() const { return m_multiPartHeaderList; }
	OP_STATUS GetRequestHeader(const OpStringC16 &headerName, OpAutoVector<OpString> *headerList);
	const uni_char*   GetTemporaryFilePath() const { return m_temporaryFilePath.CStr(); }
	MultiPartFileType GetMultiPartFileType() const { return m_fileType; }
	OP_STATUS GetFileName(OpString &fileName);
	
private:
	WebserverUploadedFile(HeaderList *headerList, MultiPartFileType fileType);

	/*The path and filename, where the file is temporary saved*/
	HeaderList* m_multiPartHeaderList;
	OpString m_temporaryFilePath;
	MultiPartFileType m_fileType;
	friend class WebserverUploadedFileWrapper;
};


#endif /*WEBSERVER_RESOURCES_H_*/
