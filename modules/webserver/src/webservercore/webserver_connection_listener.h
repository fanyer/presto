/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) Opera Software ASA  2006 - 2008
 *
 * Web server implementation
 */

#ifndef WEBSERVER_CONNECTION_LISTENER_H_
#define WEBSERVER_CONNECTION_LISTENER_H_

#include "modules/webserver/src/webservercore/webserver_connection.h"
#include "modules/webserver/src/resources/webserver_auth.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/formats/hdsplit.h"

#include "modules/hardcore/timer/optimer.h"

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"

#include "modules/util/simset.h"

#include "modules/dochand/winman.h"
#include "modules/dochand/win.h"
#include "modules/url/url_man.h"
#include "modules/hardcore/mh/generated_messages.h"

#ifdef UPNP_SUPPORT
#include "modules/upnp/upnp_upnp.h"
#include "modules/upnp/src/upnp_port_opening.h"
#endif

#include "modules/hardcore/mh/messobj.h"
#include "modules/ecmascript_utils/esasyncif.h"

#include "modules/webserver/webserver-api.h"
#include "modules/webserver/src/webservercore/webserver-references.h"

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
#include "modules/webserver/src/rendezvous/webserver_rendezvous.h"
#endif // WEBSERVER_RENDEZVOUS_SUPPORT

class WebserverEventListener;

#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
class WebserverDirectClientSocket;
#endif

#ifndef UPNP_PARAM
	#ifdef UPNP_SUPPORT
		#define UPNP_PARAM(DEF) DEF
		#define UPNP_PARAM_COMMA(DEF) , DEF
	#else
		#define UPNP_PARAM(DEF)
		#define UPNP_PARAM_COMMA(DEF)
	#endif
#endif

class WebServerConnectionListener
	: public WebServerConnectionListenerReference
	, public OpSocketListener
	, public MessageObject
	, public OpPrefsListener
#ifdef UPNP_SUPPORT
	#ifdef UPNP_SERVICE_DISCOVERY
		, public UPnPServicesProvider
	#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	, public UPnPPortListener
#endif	// UPNP_SUPPORT
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	, public RendezvousEventListener
#endif
{
public:
	static OP_STATUS Make(
	                   WebServerConnectionListener *&connection_listener,
	                   WebserverListeningMode listeningMode,
					   WebServer *eventListener,
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
								  const char *shared_secret,
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
					   unsigned int port,
					   unsigned int backlog,
					   int uploadRate
					   UPNP_PARAM_COMMA(UPnP *upnp),
					   const char *upnpDeviceType = NULL,
					   const uni_char *upnpDeviceIcon = NULL,
					   const uni_char *upnpPayload = NULL
					   );



	OP_STATUS SetUpListenerSocket();

	virtual ~WebServerConnectionListener();

	/* Read all preferences */

	OP_STATUS AddSubServer(WebSubServer *subserver, BOOL notify_upnp=TRUE);

	OP_STATUS SetRootSubServer(WebSubServer *root_service);

	WebSubServer *GetRootSubServer(){ return m_rootSubServer; }

	unsigned int GetLocalListeningPort() { return m_real_listening_port; }

	OP_STATUS RemoveSubServers(unsigned long windowId);

	OP_STATUS RemoveSubServer(WebSubServer *subserver, BOOL notify_upnp=TRUE);

	WebSubServer *WindowHasWebserverAssociation(unsigned long windowId);

	int NumberOfConnectionsToSubServer(WebSubServer *subserver);

	int NumberOfConnectionsInWindow(unsigned int windowId);

	int NumberOfConnectionsWithRequests();

	void windowClosed(unsigned long windowId);

	/* Interface inherited from OpPrefsListener */
	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);

	OpAutoVector<WebSubServer> *GetWebSubServers();

	WebSubServer *GetWebRootSubServer(){ return m_rootSubServer;}

	void SetUploadRate(UINT anUploadRate) { m_uploadRate =  anUploadRate; }

	WebserverListeningMode GetCurrentMode();

#if defined(WEBSERVER_DIRECT_SOCKET_SUPPORT)
	void OnSocketDirectConnectionRequest(WebserverDirectClientSocket* socket, BOOL is_owner = FALSE);
#endif

	void CloseAllConnectionsToSubServer(WebSubServer *subserver);
	
	void CloseAllConnections();
	
	// Return the webserver that created the connection
	WebServer *GetWebServer() { return m_webserverEventListener; }

	/* Check if the server is listening (so calling _root should work)
	 *
	 * @return TRUE if the server is listening.
	 */
	BOOL IsListening() { return m_listening_socket_local!=NULL 
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
		|| m_listening_socket_rendezvous!=NULL
#endif
		; }

#if defined(WEBSERVER_RENDEZVOUS_SUPPORT) && defined(UPNP_SUPPORT)
	// Get the UPnP object
	UPnP *GetUPnP() { return m_upnp; }
#endif // defined(WEBSERVER_RENDEZVOUS_SUPPORT) && defined(UPNP_SUPPORT)

		// Device string for search matching
	OpString8 m_upnpSearchType;
	const char* GetDeviceSearchString() { return m_upnpSearchType.CStr(); }

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	// Enable UPnP Services Discovery
	OP_STATUS EnableUPnPServicesDiscovery();

	// Methods for UPnPServicesProvider
	virtual UINT32 GetNumberOfServices();
	virtual OP_STATUS RetrieveService(int index, UPnPService *&service);
	virtual OP_STATUS UpdateDescriptionURL(OpSocketAddress *addr);
	virtual OP_STATUS UpdateValues(OpString8 &challenge);
	virtual BOOL IsLocal() { return TRUE; }
	/// Enable or disable UPnP updates; this has to be used carefully, with the aim to avoid flooding the network with UDP messages
	void SetEnableUPnPUpdates(BOOL b) { advertisement_enabled=b; }
	virtual BOOL IsDisabled() { return disabled || !upnp_advertised; } // Critical to avoid problems during the startup
	virtual const char* GetDeviceTypeString() { return GetDeviceSearchString(); }

//	virtual const char* GetDeviceTypeString() { return UPNP_DISCOVERY_DRAGONFLY_SEARCH_TYPE; }

#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)


private:
	WebServerConnectionListener(UPNP_PARAM(UPnP *upnp));
	// Start listening to the specified port
	OP_STATUS StartListening();
#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OP_STATUS StartListeningRendezvous();
#endif
	enum ServerState
	{
		ST_INDETERMINATE,			/* Not yet set up */
		ST_LISTENING				/* Server: listening for a request */
	};

	#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY
		void AdvertiseUPnP(BOOL delay);
	#else
		void AdvertiseUPnP(BOOL delay) { }
	#endif

#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	// Temporary object used to cycle through the services
	UPnPService temp_service;
#endif // defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)


#ifdef NATIVE_ROOT_SERVICE_SUPPORT
	OP_STATUS InstallNativeRootService(WebServer *server);
#endif //NATIVE_ROOT_SERVICE_SUPPORT

	WebServerConnection *FindConnectionById(INTPTR connectionId);

	void CloseAllConnectionsToWindow(unsigned int windowId);

	// Implementation of MessageObject
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/*Creates a new connection and put in the the connectionList */
	OP_STATUS CreateNewService(OpSocket *the_socket);

	/*Socket code from OpSocketListener */
	void OnSocketDataReady(OpSocket* socket);
	void OnSocketListenError(OpSocket* socket, OpSocket::Error error);
	void OnSocketDataSent(OpSocket* socket, UINT bytes_sent);
	void OnSocketClosed(OpSocket* socket);
	void OnSocketReceiveError(OpSocket* socket, OpSocket::Error error);
	void OnSocketSendError(OpSocket* socket, OpSocket::Error error);

	void OnSocketConnectionRequest(OpSocket* socket);

	void OnSocketCloseError(OpSocket* socket, OpSocket::Error error);
	void OnSocketDataSendProgressUpdate(OpSocket* socket, UINT bytes_sent);
	void OnSocketConnectError(OpSocket* socket, OpSocket::Error error);
	void OnSocketConnected(OpSocket* socket);
#ifdef _EXTERNAL_SSL_SUPPORT_
	virtual void OnSecureSocketError(OpSocket* socket, OpSocket::Error error){ OP_ASSERT(!"DOES NOT MAKE SENSE IN LISTENING SOCKET"); }
#endif // _EXTERNAL_SSL_SUPPORT_

#ifdef UPNP_SUPPORT
	/// There was a failure during the process, on the specified device
	virtual OP_STATUS Failure(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic);
	/// If one of the listeners return FALSE, the port is closed, and another port is tried. This is basically a multiple handshake
	virtual OP_STATUS ArePortsAcceptable(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port, BOOL &ports_acceptable);
	/// Called when there was a diasgreement on the port. Every listener must "roll back".
	virtual OP_STATUS PortsRefused(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port);
	/// Called if it was possible to open the specified external port, and after all the listener agreed on that port
	virtual OP_STATUS Success(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port);
	/// Called when a port has been closed (if success is false, it means that it was not possible to close it). The WebServer can now be closed
	virtual OP_STATUS PortClosed(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 port, BOOL success);
	// Disable the UPnP port forwarding mechanism
	void DisableUPnPPortForwarding();
	// Used to enable agait Port Forwarding; this function should probably be only called by the WebServerConnectionListener object itself
	OP_STATUS ReEnableUPnPPortForwarding();
	// Close the port opened and disable UPnP port forward.
	// If the call is successfull, PortClosed() will later be called (and the WebServer could finally be closed)
	// If it returns an error, the WebServer can be closed immediately
	OP_STATUS CloseUPnPPort();
#endif // UPNP_SUPPORT

	AutoDeleteHead m_connectionList;

	unsigned int m_backlog;

	OpAutoVector<WebSubServer> m_subServers;

	WebSubServer *m_rootSubServer;

	/* used for bandwidth capping */
	int m_uploadRate;
	unsigned int m_currentWindowSize;
	unsigned long m_bytesSentThisSample;
	double m_prevClockCheck; // milliseconds
	bool m_nowThrottling;
	double m_kBpsUp;

	OpSocket *m_listening_socket_local;

	/* The listening mode this listener try to keep.
	 * Not the actual listening mode. To get the actual
	 * listening mode call GetCurrentMode(). */
	WebserverListeningMode m_listeningMode;

	unsigned int m_prefered_listening_port;

	/* The actual listening port. Is initially set to m_prefered_listening_port.
	 * If m_prefered_listening_port is taken, we try ++m_prefered_listening_port. */
	unsigned int m_real_listening_port;
	// Public IP address (empty if direct connection is not available)
	OpString public_ip;
	// Public port (0 if direct connection is not available)
	UINT16 public_port;

	BOOL m_hasPostedSetupListeningsocketMessage;

	WebServer *m_webserverEventListener;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	OP_STATUS ReconnectProxy(const char *shared_secret);
	void StopListeningProxy();
#endif //WEBSERVER_RENDEZVOUS_SUPPORT

	/** Reconnect the local listener
		@param reconnect_immediately TRUE if we want a safe immediate reconnection, FALSE to delay it a few seconds
		@param port 0 to reconnect to the same port, another value to change it
	*/
	OP_STATUS ReconnectLocal(unsigned int port, BOOL reconnect_immediately);
	/// Unsafe method that reconnect immediately; it should be called carefully; usually calling ReconnectLocal() is better
	OP_STATUS ReconnectLocalImmediately();
	void StopListeningLocal();
	void StartListeningDirectMemorySocket();
	void StopListeningDirectMemorySocket();

	friend class WebServerConnection;
	friend class WebServer;

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	virtual void OnRendezvousConnected(WebserverRendezvous *socket);

	virtual void OnRendezvousConnectError(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry);
	virtual void OnRendezvousConnectionProblem(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry);
	virtual void OnRendezvousConnectionRequest(WebserverRendezvous *socket);

	virtual void OnRendezvousClosed(WebserverRendezvous *socket, RendezvousStatus status, BOOL retry, int code = 0);
	virtual void OnDirectAccessStateChanged(BOOL direct_access, const char* direct_access_address);

	WebserverRendezvous *m_listening_socket_rendezvous;
	OpString8 m_shared_secret;

	friend class WebserverRendezvous;
	friend class ControlChannel;

#endif // WEBSERVER_RENDEZVOUS_SUPPORT

#ifdef UPNP_SUPPORT
	UPnPPortOpening *m_upnp_port_opener; /* Not owned by this object, owned by m_upnp. */
	// Main UPnP object
	UPnP *m_upnp;
	// Port opened with UPnP
	UINT16 m_opened_port;
	// Device opened with UPnP
	UPnPDevice *m_opened_device;
	// TRUE if the portchecking is enabled
	BOOL m_port_checking_enabled;
#endif //UPNP_SUPPORT
	// TRUE if UPnP has been advertised
	BOOL upnp_advertised;
#if defined(UPNP_SUPPORT) && defined (UPNP_SERVICE_DISCOVERY)
	// TRUE if UPnP advertisement is enabled
	BOOL advertisement_enabled;
#endif //UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY

};

#endif /*WEBSERVER_CONNECTION_LISTENER_H_*/
