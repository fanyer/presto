/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2007 - 2008
 *
 */

#ifndef WEBSERVER_API_H
#define WEBSERVER_API_H

#include "modules/util/adt/opvector.h"
#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/webserver/webserver_user_management.h"
#include "modules/url/url_sn.h"
#include "modules/webserver/src/webservercore/webserver-references.h"

/* Interface to the web server.
*
*   There is only one web server in the system. The web server can run several sub servers (also called services).
* 	The subservers are available through http://webserverAdress/serviceName/{service/subpath}
*
* 	To put resources into a subserver, use the WebSubServer api.
*
*/

#ifdef HAVE_UINT64
	#define TRANSFERS_TYPE UINT64
#elif defined HAVE_INT64
	#define TRANSFERS_TYPE INT64
#else
	#warning "***** Warning: This platform does not support UINT64 / INT64. If you need the number of bytes uploaded, please ask to the webserver owner to provide a 32 bits function, that for example can return the KBs / MBs transferred. *****"
	#undef TRANSFERS_TYPE
#endif

enum WebserverStatus
{
	 WEBSERVER_OK = 0
	,WEBSERVER_ERROR									// unknown error.
	,WEBSERVER_ERROR_MEMORY								// out of memory.
	,WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN				// Some other service is already listening to this port.
	,WEBSERVER_ERROR_COULD_NOT_OPEN_PORT
	,PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION		// Serious error!. This version of opera has a serious security error. Server will not start.
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	,PROXY_CONNECTION_LOGGED_OUT						// Logged out from the proxy. T
	,PROXY_CONNECTION_ERROR_PROXY_DOWN					// Proxy is down. T
	,PROXY_CONNECTION_ERROR_DENIED						// Proxy denied connection (probably too many connections).
	,PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE			// Proxy is probably down.
	,PROXY_CONNECTION_ERROR_NETWORK						// Could not reach the proxy, network probably down.

	,PROXY_CONNECTION_LOGGED_OUT_BY_CALLER				// The caller of this api logged out from the proxy

	,PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN 		// The proxy address is unknown.
	,PROXY_CONNECTION_ERROR_AUTH_FAILURE				// Invalid username/password.
	,PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED	// This device has already been registered.
	,PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION		// The proxy has been updated, and webserver is still using the old protocol. User must download new version.
	,PROXY_CONNECTION_ERROR_PROTOCOL					// Serious protocol error.
	,PROXY_CONNECTION_DEVICE_NOT_CONFIGURED				// The device has not been configured properly (missing devicename, username or proxyname in the preferences)
#endif //WEBSERVER_RENDEZVOUS_SUPPORT
};

typedef unsigned int WebserverListeningMode;

/* WebserverListeningMode is defined with the following bits */
#define WEBSERVER_LISTEN_NONE 			0 		// Don't listen to requests.

#define WEBSERVER_LISTEN_DIRECT			1		// Listening on direct memory socket (bypasses the socket in OS level, url can connect to webserver directly in memory)

#define WEBSERVER_LISTEN_PROXY	   		2		// The webserver will listen on requests via proxy.

#define WEBSERVER_LISTEN_OPEN_PORT 		4		// The webserver will try to open the firewall.

#define WEBSERVER_LISTEN_UPNP_DISCOVERY	8		// The webserver will enable UPnP Services Discovery

#define WEBSERVER_LISTEN_ROBOTS_TXT		16		// Enable Robots.txt

#define WEBSERVER_LISTEN_PROXY_LOCAL 	(WEBSERVER_LISTEN_PROXY | WEBSERVER_LISTEN_LOCAL | WEBSERVER_LISTEN_DIRECT)

#define WEBSERVER_LISTEN_LOCAL 			(32 | WEBSERVER_LISTEN_DIRECT) // The webserver will listen on a localhost port and direct memory socket. (Port is given in preferences PrefsCollectionWebserver::WebserverPort).

#define WEBSERVER_LISTEN_DEFAULT		64		// Default mode according to web server settings


enum UploadServiceStatus
{
	UPLOAD_SERVICE_OK = 0,					// Success.
	UPLOAD_SERVICE_WARNING_OLD_PROTOCOL,	// The protocol is out dated
	UPLOAD_SERVICE_ERROR,					// unknown error.
	UPLOAD_SERVICE_ERROR_PARSER,			// on missing argument or malformed XML
	UPLOAD_SERVICE_ERROR_MEMORY,			// out of memory.
	UPLOAD_SERVICE_ERROR_COMM_FAIL,			// communication error.
	UPLOAD_SERVICE_ERROR_COMM_ABORTED,		// communication aborted.
	UPLOAD_SERVICE_ERROR_COMM_TIMEOUT,		// timeout while accessing the server.
	UPLOAD_SERVICE_AUTH_FAILURE				// the upload service token not correct.
};


#include "modules/webserver/src/webservercore/webserver_connection_listener.h"
class WebSubServer;
class WebServerConnection;
class WebResource;
class UploadServiceListManager;
class UPnPServicesProvider;
class UPnP;
class UPnPDevice;

// Default header value
class DefaultHeader
{
private:
	OpString name;
	OpString value;
	
public:
	// Second phase constructor
	OP_STATUS Construct(const OpStringC &header_name, const OpStringC &header_value);
	// Return the name of the header
	OpStringC &GetName() { return name; }
	// Return the value of the header
	OpStringC &GetValue() { return value; }
};

class WebserverEventListener
{
public:
	/* The webserver has stopped.
	 *
	 * @param status The reason the webserver stopped.
	 */
	virtual void OnWebserverStopped(WebserverStatus status) = 0;
	
	/**
		Called when the UPnP ports hav been closed (so the WebServer cna be stoppped)
	*/
	virtual void OnWebserverUPnPPortsClosed(UINT16 port) = 0;

	/* A service has been added to the webserver.
	 *
	 * NB! Only service path is guaranteed to be unique.
	 *
	 * @param service_name The name of the service.
	 */
	virtual void OnWebserverServiceStarted(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE) = 0;

	/* A service has been removed from the webserver.
	 *
	 * NB! Only service path is guaranteed to be unique.
	 *
	 * @param service_name The name of the service.
	 */
	virtual void OnWebserverServiceStopped(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE) = 0;

	/* The webserver has successfully started listening on localhost.
	 *
	 * @param port 	The port the webserver is listening on. Note
	 * 				That this port might differ from the port set in preferences,
	 * 				if the preferences port is taken.
	 */
	virtual void OnWebserverListenLocalStarted(unsigned int port) = 0;

	/* The webserver has stopped listening on localhost.
	 *
	 * This will only happen if WebServer::ChangeListeningMode has been called.
	 */
	virtual void OnWebserverListenLocalStopped() = 0;

	/* The webserver was not able to listen to localhost.
	 * The port was probably taken. This will typically happen if two
	 * instances of Opera have been started simultaneously.
	 *
	 * @param status The reason
	 */
	virtual void OnWebserverListenLocalFailed(WebserverStatus status) = 0;

#ifdef WEB_UPLOAD_SERVICE_LIST
	/* The webserver has tried to upload a service list.
	 *
	 * @param The status of of service upload.
	 */
	virtual void OnWebserverUploadServiceStatus(UploadServiceStatus status) = 0;
#endif // WEB_UPLOAD_SERVICE_LIST

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT

	/* The webserver has successfully connected to the proxy.
	 */
	virtual void OnProxyConnected() = 0;

	/* The proxy refused login from webserver.
	 *
	 * @param status The reason.
	 * @param retry	 If TRUE, the server will try to reconnect
	 */
	virtual void OnProxyConnectionFailed(WebserverStatus status, BOOL retry) = 0;

	/* There are problems with the connection to the proxy.
	 *
	 * @param status The reason.
	 * @param retry	 If TRUE, the server will try to reconnect *
	 */
	virtual void OnProxyConnectionProblem(WebserverStatus status,  BOOL retry) = 0;

	/* The webserver was disconnected from the proxy.
	 *
	 * If reason was PROXY_CONNECTION_LOGGED_OUT, the server will automatically try to reconnect.
	 *
	 * @param status The reason.
	 * @param retry	 If TRUE, the server will try to reconnect
	 */
	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry, int code = 0) = 0;
#endif

	/* The direct access status of the webserver has changed
	 *
	 * @param direct_access 		If true, it is possible to bypass the proxy and access the webserver directly.
	 * @param direct_access_address The webserver address. Will be on the form address:port, where address can be an IP-address or a servername.
	 */
	virtual void OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address) = 0;

	/**
		A new event listener has been added to the DOM obejct (opera.io.webserver).

		@param service_name Service name
		@param evt Event listened to
		@param virtual_path Virtual path of the service
	*/
	virtual void OnNewDOMEventListener(const uni_char *service_name, const uni_char *evt, const uni_char *virtual_path = NULL) = 0;
	
	/**
		Called when a router has been found and a port has been opened.
		@param device UPnP router that was able to forward the port
		@param internal_port local port the webserver is listening to
		@param external_port port opened and accessible from the outside. For now it is the same as internal_port
	*/
	virtual void OnPortOpened(UPnPDevice *device, UINT16 internal_port, UINT16 external_port) = 0;

	virtual ~WebserverEventListener(){}
};

#ifndef UPNP_PARAM
	#ifdef UPNP_SUPPORT
		#define UPNP_PARAM(DEF) DEF
		#define UPNP_PARAM_COMMA(DEF) , DEF
	#else
		#define UPNP_PARAM(DEF)
		#define UPNP_PARAM_COMMA(DEF)
	#endif
#endif

// Class that logs the access to the WebServer
class WebLogAccess
{
private:
	OpString user;		// User name or User IP
	BOOL authenticated; // TRUE if the user is authenticated (user is a user name) or not (user is an IP)
	time_t last_access; // Time of the last access
	int num_accesses;	// Number of request (proibably it can be misleading...)

public:
	WebLogAccess(): authenticated(FALSE), last_access(0), num_accesses(0) {}
	virtual ~WebLogAccess() { }
	// Second phase contructor; it can also be called to change the user (for example to switch from IP to user_name)
	OP_STATUS Construct(const OpString &unique_user, BOOL is_authenticaed) { RETURN_IF_ERROR(user.Set(unique_user)); authenticated=is_authenticaed; UpdateTime(); return OpStatus::OK; }

	// Update the access time
	void UpdateTime() { last_access=GetSystemTime(); }
	// Return the user; if auth is FALSE, it is an IP, else it is a user name
	uni_char *GetUser(BOOL &auth) { auth=authenticated; return user.CStr(); }
	// Get the last access time
	time_t GetAccessTime() { return last_access; }
	// Get the time, in second
	time_t static GetSystemTime() { return (time_t) (g_op_time_info->GetTimeUTC()/1000.0); }
	// Increment the number of accesses
	void IncAccesses() { num_accesses++; }
};


class WebServer
	: public OpPrefsListener
	, public WebserverEventListener
{
public:
	/* Starting the server.
	 *
	 * The username, devicename, proxy address, proxy port and localhost port is set in g_pcwebserver (the preference system):
	 * Get them with  g_pcwebserver->GetStringPref(preference), where preference is one of
	 * 	PrefsCollectionWebserver::WebserverUser
	 * 	PrefsCollectionWebserver::WebserverDevice
	 * 	PrefsCollectionWebserver::WebserverProxyHost
	 *
	 * and g_pcwebserver->GetIntegerPref(preference) where preference is one of
	 * 	PrefsCollectionWebserver::WebserverPort // local listening port
 	 *  PrefsCollectionWebserver::WebserverProxyPort // the proxy port
	 *
	 * When local listening socket is set up, there will be a callback on WebserverEventListener::OnWebserverListenLocalStarted()
	 * When webserver is connected to proxy there will be a callback to WebserverEventListener::OnProxyConnected()
	 *
	 * @param mode 			The listening mode. Listen on localhost, proxy or both. If listening on proxy, shared_secret and proxy_address cannot be NULL.
	 * @param shared_secret A secret shared between webserver and proxy in utf8 format.
	 *
	 * @param upnpDeviceType	String identifying the device type broadcasted. I.e. "urn:opera-com:device:OperaUnite:1"
	 * @param upnpPayload		String used for data transport.
	 * @param upnpDeviceIcon	String with icon resource for the announced device.
	 *
	 * @return ERR if user, device or proxy host is not set.
	 * 		   OK for success, or ERR_NO_MEMORY.
	 *
	 */
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OP_STATUS Start(WebserverListeningMode mode, unsigned int port, const char *shared_secret = NULL UPNP_PARAM_COMMA(UPnP *upnp=NULL));
#else
	OP_STATUS Start(WebserverListeningMode mode UPNP_PARAM_COMMA(UPnP *upnp=NULL));
#endif

	OP_STATUS StartUPnPServer(WebserverListeningMode mode, unsigned int port, const char* upnpDeviceType, const uni_char* upnpDeviceIcon, const uni_char* upnpPayload UPNP_PARAM_COMMA(UPnP *upnp=NULL));

	/* Change the mode of the webserver without stopping the services running.
	 *
	 * This might be useful to change ports, shared secret,
	 * or listening mode without having to close the services.
	 *
	 * @param mode 			The listening mode. Listen on localhost, proxy or both. If listening on proxy, shared_secret and proxy_address cannot be NULL.
	 * @param shared_secret A secret shared between webserver and proxy in utf8 format.
	 */
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OP_STATUS ChangeListeningMode(WebserverListeningMode mode, const char *shared_secret = NULL);
#else
	OP_STATUS ChangeListeningMode(WebserverListeningMode mode);
#endif

	/* Stopping the server.
	 *
	 * This will broadcast a OnWebserverStopped(status) signal to all listeners.
	 * All subservers (services) added to this webserver will also be stopped.
	 *
	 * @param status Give the webserver a reason for stopping.
	 *
	 * @return OK or ERR_NO_MEMORY.
	 */
	OP_STATUS Stop(WebserverStatus status = WEBSERVER_OK);

	/* Get the port the webserver is listening on.
	 *
	 * Note that this port might might differ from the port
	 * set in PrefsCollectionWebserver::WebserverPort,
	 * if the preferred port was taken by another application.
	 *
	 * @return The local listening port
	 */
	unsigned int GetLocalListeningPort();

	/* Add an event listener to the server.
	 *
	 * The caller of this function must manage and delete the listener.
	 *
	 * @param listener 	The event listener to be added.
	 *
	 * @return			ERR_NO_MEMORY or OK.
	 *
	 */
	OP_STATUS AddWebserverListener(WebserverEventListener *listener);

	/* Remove an event listener from the server.
	 *
	 * The caller of this function must manage and delete the listener.
	 *
	 * @param listener The event listener to be removed.
	 *
	 */
	void RemoveWebserverListener(WebserverEventListener *listener);

	/* Check if window_id is running a webserver service.
	 *
	 * @param  window_id The window that will be checked.
	 *
	 * @return The service that is running in the window, NULL otherwise.
	 */
	WebSubServer *WindowHasWebserverAssociation(unsigned long window_id);

	/* Get the number of tcp connections to a service.
	 *
	 * @param 	subServer The service.
	 *
	 * @return 	The number of connections.
	 *
	 */
	int NumberOfConnectionsToSubServer(WebSubServer *service);

	/* Close all on ingoing connections connected to a service
	 *
	 * @param subserver The subserver.
	 *
	 */
	void CloseAllConnectionsToSubServer(WebSubServer *service);
	
	/* Close all on ingoing connections connected to the server
	 */
	void CloseAllConnections();

	/********************************** Adding services to the webserver **********************************/

	/* Add a service to the webserver.
	 *
	 * Note! The webserver takes ownership over the service pointer. Do not delete the pointer.
	 *
	 * @param service The service to be added.
	 *
	 * @return OK, ERR_NO_MEMORY, or ERR if the service name already exists
	 */
	OP_STATUS AddSubServer(WebSubServer *service, BOOL notify_upnp=TRUE);

	/* Get a service from the webserver.
	 *
	 * NB! 	Do NOT store the service pointer for later use,
	 *  	since the service may be deleted at any time!
	 *
	 * @param service_virtual_path The uri path on which the service was installed.
	 *
	 * @return 					The service, or NULL if not found.
	 */
	WebSubServer* GetSubServer(const OpStringC &service_virtual_path);

	/* Get a service from the webserver.
	 *
	 * Use this function to browse through all services.
	 *
	 * @param idx 	The index of the service.
	 *
	 * @return 		The service.
	 */
	DEPRECATED(WebSubServer* GetSubServer(UINT32 idx)); /* Use GetSubServerAtIndex when looping through all webservers. */
	WebSubServer* GetSubServerAtIndex(UINT32 idx);

	/* Get number of services installed.
	 *
	 * @return number of services.
	 */
	UINT32 GetSubServerCount();

	/* Remove a service from the server.
	 *
	 * The service object will be deleted.
	 *
	 * @param service_virtual_path The virtual path to a service.
	 *
	 * @return 	OpStatus::ERR if not found, or if server is not running.
	 * 			OpStatus::OK
	 */
	OP_STATUS RemoveSubServer(const OpStringC &service_virtual_path);

	/* Remove a service from the server.
	 *
	 * The service object will be deleted.
	 *
	 * @param service The service to remove.
	 *
	 * @return 	ERR If not found, or if server is not running.
	 * 			OK 	If success.
	 */
	OP_STATUS RemoveSubServer(WebSubServer *service);

	/* Remove all services in window.
	 *
	 * Remove all subservers in the window. All pointers to any of the services must be released.
	 * The subserver objects will be deleted.
	 *
	 * @param window_id The id of the window.
	 *
	 * @return 	OK or ERR if server is not running
	 */
	OP_STATUS RemoveSubServers(unsigned long window_id);


	/* Set the root service
	 *
	 * If a root service already has been set, the previous service object will be deleted.
	 *
	 * @param root_service The service that will act as root service. NB! webserver takes owner ship. Do not delete !
	 *
	 * @return OK or ERR if server is not running, or ERR_NO_MEMORY
	 */
	OP_STATUS SetRootSubServer(WebSubServer *root_service);

	/* Get the root service
	 *
	 * @return The root service, NULL of not set.
	 */
	WebSubServer *GetRootSubServer();

	/* Check if the server is running.
	 *
	 * @return TRUE if the server is running.
	 */
	BOOL IsRunning() { return GetConnectionListener() != NULL; }
	
	/* Check if the server is listening (so calling _root should work)
	 *
	 * @return TRUE if the server is listening.
	 */
	BOOL IsListening();
	
	/**
		Check if the server name requested is resolved by the webserver, in the right port.
		This function will also take care to check the admin url (e.g. admin.host.username.operaunite.com).
		See also KIsUniteServiceAdminURL.
	*/
	BOOL MatchServer(const ServerName *host, unsigned short port);
	/**
		Check if the server name requested is resolved by the webserver as admin.
		It will NOT match if it is not admin
	*/
	BOOL MatchServerAdmin(const ServerName *host, unsigned short port);
	/**
		Check if the server name requested is resolved by the webserver as admin.
		It will NOT match if it is not admin
	*/
	BOOL MatchServerAdmin(const char *host);
	

	/* Get the current listening mode
	 *
	 * @return The current listening mode.
	 */
	WebserverListeningMode GetCurrentMode();

#ifdef WEB_UPLOAD_SERVICE_LIST
	/* Service discovery.
	 *
	 * Every time a service is stared, the service list will be posted to
	 * a central server that provides simple service discovery.
	 *
	 */

	/* Set the central service discovery url.
	 *
	 * @param server_uri The uri to the central server for service discovery.
	 * @param session_token The token for this session.
	 *
	 * @return OK, or ERR_NO_MEMORY.
	 */
	OP_STATUS StartServiceDiscovery(const OpStringC &central_server_uri, const OpStringC8 &session_token);

	void StopServiceDiscovery();

#endif //WEB_UPLOAD_SERVICE_LIST

	/* returns the idna encoded webserver uri. */
	const uni_char *GetWebserverUriIdna();
	const uni_char *GetWebserverUri();
	const uni_char *GetUserName();
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	ServerName_Pointer GetAdminServerName() { return sn_admin; }
	ServerName_Pointer GetServerName() { return sn_main; }
	/* is Webserver connected to the proxy. */
	BOOL GetRendezvousConnected() { return m_rendezvousConnected; }

#endif
	// Get the public IP address of the server (if direct connection is available)
	const uni_char *GetPublicIP();
	// Get the public port of the server (if direct connection is available)
	UINT16 GetPublicPort();

#ifdef TRANSFERS_TYPE
	// Return the number of bytes uploaded
	TRANSFERS_TYPE GetBytesUploaded() { return m_bytes_uploaded; }
	// Return the number of bytes downloaded
	TRANSFERS_TYPE GetBytesDownloaded() { return m_bytes_downloaded; }
#endif
	// Get the number of users connected to the webserver in the last requested seconds
	UINT32 GetLastUsersCount(UINT32 seconds);
	
	/* Add a default response header, that will be put in any resource, EXCEPT the ones that override it (via addResponseHeader)
	 * 
	 * @param header_name	The header name
	 * @param header_value	The header value
	 * @param append		For now not used. //If TRUE, the header will we appended. If FALSE, 
	 * 						//any header that exists with same name will be overwritten. 
	 * 
	 * @return 	Error
	 */
	OP_STATUS SetDefaultHeader(const OpStringC16 &header_name, const OpStringC16 &header_value, BOOL append = FALSE);
	
	/* Add the default headers to the resource
	 * 
	 * @param res	Resource
	 * 
	 * @return 	Error
	 */
	OP_STATUS SetDefaultResourceHeaders(WebResource *res);
	
	/**
		Return the last time that the service list has changed
		
		@return The Time of the change
	*/
	time_t GetLastServicesChangeTime() { return last_service_change; }
	
	/// Return the user and the device name
	//OP_STATUS GetCredentials(OpString &user, OpString &device);
	
	#ifdef UPNP_SUPPORT
		// Close the UPnP Port and disable UPnP port forward
		OP_STATUS CloseUPnPPort();
		UPnP *GetUPnP();
	#endif
	
	#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
		// The the UPnP Service Discovery provider created for the SubServer installed
		UPnPServicesProvider *GetUPnPServicesProvider();
		// Enable UPnP Services Discovery (server side)
		OP_STATUS EnableUPnPServicesDiscovery();
	#endif
	
	/// Return the Connection lintener
	WebServerConnectionListener *GetConnectionListener() { return reinterpret_cast<WebServerConnectionListener *>(m_connectionListener.GetPointer()); }
	
	// Utility function used to extract the extension from a file name, accepting even only extentions
	static const uni_char* StrFileExt(const uni_char* fName);
	
	/// Return the UUID of the WebServer (used by UPnP Services Discovery)
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	const char *GetUUID() { OP_ASSERT(uuid.HasContent()); return uuid.CStr(); }
#endif

#ifdef WEB_UPLOAD_SERVICE_LIST
	/// Upload the service list to ASD
	void UploadServiceList();
#endif // WEB_UPLOAD_SERVICE_LIST

	void SetAlwaysListenToAllNetworks(BOOL always) { m_always_listen_to_all_networks = always;}
	BOOL GetAlwaysListenToAllNetworks() { return m_always_listen_to_all_networks; } 
private:

	 /* Tell the webserver that a window has closed.
	 *
	 * ! Should only be called by window management when a window has been closed !
	 *
	 * @param windowId The id of the window that was closed.
	 *
	 */
	void windowClosed(unsigned long windowId);


		/* From OpPrefsListener */
	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);
	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue);

	OP_STATUS SetServerName();

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual void OnProxyConnected();

	virtual void OnProxyConnectionFailed(WebserverStatus status, BOOL retry);
	virtual void OnProxyConnectionProblem(WebserverStatus status, BOOL retry);
	virtual void OnProxyDisconnected(WebserverStatus status, BOOL retry, int code = 0);
#endif 	//WEBSERVER_RENDEZVOUS_SUPPORT

	virtual void OnNewDOMEventListener(const uni_char *service_name, const uni_char *evt, const uni_char *virtual_path=NULL);

	virtual void OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address);

	virtual void OnWebserverListenLocalStarted(unsigned int port);

	virtual void OnWebserverListenLocalStopped();

	virtual void OnWebserverListenLocalFailed(WebserverStatus status);

	virtual void OnWebserverStopped(WebserverStatus status);
	
	virtual void OnWebserverUPnPPortsClosed(UINT16 port);

	virtual void OnWebserverServiceStarted(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE);

	virtual void OnWebserverServiceStopped(const uni_char *service_name, const uni_char *service_path, BOOL is_root_service = FALSE);
#ifdef WEB_UPLOAD_SERVICE_LIST
	virtual void OnWebserverUploadServiceStatus(UploadServiceStatus status);
#endif // WEB_UPLOAD_SERVICE_LIST

	virtual void OnPortOpened(UPnPDevice *device, UINT16 internal_port, UINT16 external_port) { } 

	// Add the specified bytes to the number of bytes uploaded to the clients
	void AddBytesTransfered(UINT bytes_transferred, BOOL outgoing)
	{
	#ifdef TRANSFERS_TYPE
		if(outgoing)
			m_bytes_uploaded += (TRANSFERS_TYPE)bytes_transferred;
		else
			m_bytes_downloaded += (TRANSFERS_TYPE)bytes_transferred;
	#endif
	}
	// Log that a user has requested access to a resource
	OP_STATUS LogResourceAccess(WebServerConnection *conn, WebserverRequest *request, WebSubServer *sub_server, WebResource *resource);
	// Internal log with the user already available
	OP_STATUS LogResourceAccessByUser(const OpString &user, const OpString &user_ip, BOOL auth, WebSubServer *sub_server, WebResource *resource);
	
	static OP_STATUS Init();

#ifdef SCOPE_SUPPORT
	static OP_STATUS InitDragonflyUPnPServer();
#endif // SCOPE_SUPPORT

	WebServer();
	virtual ~WebServer();
	
	// Reference to the connection listener; it is of type WebServerConnectionListener, but to solve bug CORE-21620, it is defined as a MessageObject
	ReferenceUserSingle<WebServerConnectionListenerReference> m_connectionListener;
	/*< This is the class that listens to connections from socket or external proxy (rendezvous) */

#ifdef WEB_UPLOAD_SERVICE_LIST
	/* utf8 encoded token */
	OpString8 m_upload_service_token;
	OpString m_upload_service_uri;
	UploadServiceListManager *m_upload_service_list_manager;
#endif
#ifdef TRANSFERS_TYPE
	TRANSFERS_TYPE m_bytes_uploaded;
	TRANSFERS_TYPE m_bytes_downloaded;
#endif
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	BOOL m_rendezvousConnected;
#endif

	OpVector<WebserverEventListener> m_webserverEventListeners;
	/// ServerName of the webserver (the name it is supposed to resolve, e.g. device.user.operaunite.com)
	OpString m_webserver_server_name;
	/// ServerName_Pointer version of the ServerName (e.g. device.user.operaunite.com)
	ServerName_Pointer sn_main;
	/// ServerName_Pointer of the "admin" version of ServerName (e.g. admin.device.user.operaunite.com)
	ServerName_Pointer sn_admin;
	/// ServerName_Pointer of "localhost"
	ServerName_Pointer sn_localhost_1;
	/// ServerName_Pointer of "127.0.0.1"
	ServerName_Pointer sn_localhost_2;
	OpString m_webserver_server_name_idna;
	/// Log the last access time of the users
	OpStringHashTable<WebLogAccess> log_users;
	// Default headers assigned to every resource (that has not already defined them)
	OpAutoVector<DefaultHeader> m_default_headers;
	// Last time a SubServer has been added or removed
	time_t last_service_change;
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	/// uuid of this Instance. It is not persistent across reboots...
	OpString8 uuid;
#endif

	/// set this to override PrefsCollectionWebserver::WebserverListenToAllNetworks
	BOOL m_always_listen_to_all_networks;

	friend class DOM_WebServer;
	friend class WebServerConnection;
	friend class WebserverModule;
	friend class WindowManager;
	friend class WebServerConnectionListener;
#ifdef WEBSERVER_DIRECT_SOCKET_SUPPORT
	friend class WebserverDirectClientSocket;
#endif // WEBSERVER_DIRECT_SOCKET_SUPPORT
};

#endif // WEBSERVER_API_H
