/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifdef UPNP_SUPPORT

#ifndef UPNP_UPNP_H_
#define UPNP_UPNP_H_

#include "modules/pi/network/OpSocket.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/pi/network/OpHostResolver.h"
#include "modules/pi/network/OpUdpSocket.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/url/url2.h"
#include "modules/xmlutils/xmltokenhandler.h"
#include "modules/xmlutils/xmlparser.h"
#include "modules/xmlutils/xmlfragment.h"
#include "modules/upnp/src/upnp_util.h"
//#include "modules/upnp/src/upnp_soap.h"
#include "modules/upnp/src/upnp_handlers.h"
#ifdef PI_NETWORK_INTERFACE_MANAGER
	#include "modules/pi/network/OpNetworkInterface.h"
#endif


class UPnPXH_Auto;
class UPnPPortOpening;
class UPnPServicesProvider;
class UPnPPassiveControlPoint;

#ifdef _DEBUG
	#define UPNP_LOG
#endif

//#define ADD_PASSIVE_CONTROL_POINT

// Represent a socket to a network card, and its socket address (to reduce the changes on OpSocket)
class NetworkCard
{
	protected:
		OpUdpSocket *socket_uni;	// Unicast Socket associated with the network card (Broadcast send and Unicast receive))
		OpUdpSocket *socket_multi;	// Multicast Socket associated with the network card (Multicast receive))
		OpSocketAddress *address;	// SocketAddress of the network card
		
		// Protected constructor used for Mock classes
		NetworkCard() { socket_uni=socket_multi=NULL; address=NULL; }
		
	public:
		// Constructor. The class will take ownership of the pointers, and will deallocate them
		NetworkCard(OpUdpSocket *sk_uni, OpUdpSocket *sk_multi, OpSocketAddress *addr) { socket_uni=sk_uni; socket_multi=sk_multi; address=addr; }
		~NetworkCard() { OP_DELETE(socket_multi); OP_DELETE(socket_uni); OP_DELETE(address); socket_uni=NULL; socket_multi=NULL; address=NULL; }
		OpSocketAddress *GetAddress() { return address; }
		// Get the socket used for Multicast operations
		// WARNING: If there are really two sockets, it is an implementation detail of this class.
		OpUdpSocket *GetSocketMulticast() { return socket_multi; }
		// Get the socket used for Unicast operations
		// WARNING: If there are really two sockets, it is an implementation detail of this class.
		OpUdpSocket *GetSocketUnicast() { return socket_uni; }
		BOOL IsAddressEqual(OpSocketAddress *addr) { if(addr && address) return addr->IsEqual(address); return addr==address; }
		BOOL IsAddressEqual(OpString *ip);
};

// Used to check the network cards (par1==MSG_UPNP_CHECK_NETWORK_CARDS, it checks if the IPs changed) or to unconditionally retry UPnP (par1==MSG_UPNP_RECONNECT)
class UPnPRetryMessageHandler: 
public MessageObject, 
#ifdef PI_NETWORK_INTERFACE_MANAGER
	public OpNetworkInterfaceListener
#else
	public OpHostResolverListener
#endif // PI_NETWORK_INTERFACE_MANAGER
{
	private:
		OpAutoVector<OpString> network_cards_ips; // Array of the IPs available from the network cards
		UPnP *upnp;						// UPnP father object
	#ifdef PI_NETWORK_INTERFACE_MANAGER
		// Network cards manager
		OpNetworkInterfaceManager *nic_man;
	#else
		OpHostResolver *ohr;			// Used to resolve the IPs
	#endif // PI_NETWORK_INTERFACE_MANAGER
		MessageHandler mh;				// Used to postpone the check
		//BOOL first_check_done;			// True if the first check has been done (used to don't start the UPnP process 2 times without a reason)
		
		// Add a network Card to a list, checking for duplciates
		OP_STATUS AddAddressToList(OpSocketAddress *osa, OpAutoVector<OpString> *vector);
		
		// Destructor: remove messages
		void RemoveMessages();
		// Destructor: remove callbacks
		void RemoveCallBacks();
		// Destructor: delete ohr or nic_man
		void RemoveNic();
		
	public:
		UPnPRetryMessageHandler(UPnP *u);
		~UPnPRetryMessageHandler();
		
		// Manage all the changes in the interfaces list
		OP_STATUS ManageNICChanges();
	#ifdef PI_NETWORK_INTERFACE_MANAGER
		virtual void OnInterfaceAdded(OpNetworkInterface* iface) { OpStatus::Ignore(ManageNICChanges()); }
		virtual void OnInterfaceRemoved(OpNetworkInterface* iface) { OpStatus::Ignore(ManageNICChanges()); }
		virtual void OnInterfaceChanged(OpNetworkInterface* iface) { OpStatus::Ignore(ManageNICChanges()); }
		// THe the list of interfaces, from the Porting Interface
		OP_STATUS GetNetworkCardsListPI(OpAutoVector<OpString> *vector);
	#else
		// Make a copy of the IP of the network cards
		OP_STATUS GetNetworkCardsListHR(OpHostResolver* host_resolver, OpAutoVector<OpString> *vector);
		void OnHostResolved(OpHostResolver* host_resolver);
		void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error) { };
	#endif // PI_NETWORK_INTERFACE_MANAGER
		void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
		MessageHandler *GetMessageHandler() { return &mh; }
};


/// Listener that will receive notification of the event associated to the UPnP class
class UPnPListener
{
public:
	/// There was a failure during the process, on the specified device
	virtual OP_STATUS Failure(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic)=0;
	/// Called if it was possible to open the specified external port, and after all the listener agreed on that port
	virtual OP_STATUS Success(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port)=0;
	
	virtual ~UPnPListener() { }
};

/// Listener that will receive notification of the event associated to the UPnP class
class UPnPPortListener: public UPnPListener
{
public:
	/// If one of the listeners return FALSE, the port is closed, and another port is tried. This is basically a multiple handshake
	virtual OP_STATUS ArePortsAcceptable(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port, BOOL &ports_acceptable)=0;
	/// Called when there was a diasgreement on the port. Every listener must "roll back".
	virtual OP_STATUS PortsRefused(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 internal_port, UINT16 external_port)=0;
	/// Called when a port has been closed (if success is false, it means that it was not possible to close it). The WebServer can now be closed
	virtual OP_STATUS PortClosed(UPnP *upnp, UPnPDevice *device, UPnPLogic *logic, UINT16 port, BOOL success) = 0;
	
	virtual ~UPnPPortListener() { }
};

/// Class containing the data used to control a device.
class UPnPDevice
{
friend class UPnP;
friend class UPnP_DiscoveryListener;

#ifdef UPNP_ANTI_SPOOF
friend class UPnP_DiscoveryListener::AliveSpoofListener;
#endif // UPNP_ANTI_SPOOF

private:
	OpString ip_service_url;				///< IP Control URL of the service for WANIPConnection
	OpString ip_event_url;					///< IP Event URL, to subscribe to events
	OpString ppp_service_url;				///< PPP Control URL of the service for WANPPPConnection
	OpString ppp_event_url;					///< PPP Event URL, to subscribe to events
	BOOL ppp;								///< TRUE if we are dealing with WANPPPConnection, FALSE for WANIPConnection
	UDPReason reason;				        ///< Reason for the opening (used for debugging)
	UINT32 expire_period;					///< Validity period of the advertisemnt
	
	OpString device_url;					///< URL of the UPnP device
	OpString external_ip;					///< IGD External IP (real IP, but can become 0.0.0.0, during IGD startup or IP change)
	UINT16 external_port;					///< Port opened in the IGD device
	OpString notified_ip;					///< IGD External IP notified to the Opera server
	// OpSocket *socket_udp;				    ///< Socket used for UDP messages (to have only a single bind to port 1900)
	NetworkCard *network_card;			    ///< Network Card connected to the device
	OpString current_description_url;		///< URL of the current description device
	double current_description_time;		///< Last time where the description was set
	UPnPServicesProvider *provider;			///< Device exposed as "Service provider"
	OpString usn;							///< Unique Service Name, used to identify the device
	OpString device_ip;						///< device ip / name (ServerName part of the URL)
	UINT16 device_port;						///< port
	ReferenceObjectSingle<UPnPDevice> ref_obj_known;	///< Reference Object used to keep track of delete from UPnP::known_devices
	ReferenceObjectList<UPnPDevice> ref_obj_auto;	///< Reference Object used to keep track of delete from SEVERAL UPnPXH_Auto objects
	OpSocketAddress *known_address;			///< IP Address known to be valid (used for byebye anti spoof check)
	
	/// True if it is evaluating the IP connection
	BOOL IsIP() { return !ppp; }
	/// Trust level; Unite 1.0 is expected to have a level of TRUST_PRIVATE_KEY, older unsopported snapshots could have TRUST_UNTRUSTED
	TrustLevel trust;
	/// TRUE if the device has to be deleted (used for duplicates)
	BOOL to_be_deleted;

private	:
	/// Return the required attribute from the service provider; att is of type UPnPServicesProvider::ServicesProviderAttributes
	OP_STATUS SetDeviceAttribute(int att, OpString *str);
	
public:
	/// Constructor
	UPnPDevice(NetworkCard *card);
	
	/// Destructor
	~UPnPDevice();
	/// Second phase constructor
	OP_STATUS Construct(const char* devstring);
	/// Return the Service provider built from the UPnP discovery process
	UPnPServicesProvider *GetProvider() { return provider; }
	
	/// Return the required attribute from the service provider; att is of type UPnPServicesProvider::ServicesProviderAttributes
	OpStringC *GetDeviceAttribute(int att);
	/// Return the manufacturer of the device (can be empty but not null)
	OpStringC *GetManufacturer();
	/// Return the model description of the device (can be empty but not null)
	OpStringC *GetModelDescription();
	/// Return the model name of the device (can be empty but not null)
	OpStringC *GetModelName();
	/// Return the model number of the device (can be empty but not null)
	OpStringC *GetModelNumber();
	/// Return the serial number of the device (can be empty but not null)
	OpStringC *GetSerialNumber();
	/// Return the external IP of the device
	OpStringC *GetExternalIP() { return &external_ip; }
	/// Return the IP notified to the Opera Server
	OpStringC *GetNotifiedExternalIPPtr() { return &notified_ip; }
	/// Return the IP notified to the Opera Server
	OP_STATUS SetNotifiedExternalIPPtr(OpStringC *str) { OP_ASSERT(str); return notified_ip.Set(str->CStr()); }
	/// Return the external port opened on the device
	UINT16 GetExternalPort() { return external_port; }
	/// Return the address of the Network Card associated to the device
	OpSocketAddress *GetNetworkCardAddress() { return (network_card)?network_card->GetAddress():NULL; }
	/// Return the address of the Network Card associated to the device, used for Multicast operations
	OpUdpSocket *GetNetworkCardSocketMulticast() { return (network_card)?network_card->GetSocketMulticast():NULL; }
	/// Return the address of the Network Card associated to the device, used for Unicast operations
	OpUdpSocket *GetNetworkCardSocketUnicast() { return (network_card)?network_card->GetSocketUnicast():NULL; }
	/// Return Network Card associated to the device
	NetworkCard *GetNetworkCard() { return network_card; }
	/// Set the USN of the device
	OP_STATUS SetUSN(const uni_char *dev_usn) { return usn.Set(dev_usn); }
	/// Return the USN of the device
	OpStringC *GetUSN() { return &usn; }
	/// Return TRUE if the USN is the same as the supplied one
	BOOL IsSameDevice(const char *dev_usn, const char *dev_loc);
	/// Return TRUE if the USN is the same as the supplied one
	BOOL IsSameDevice(const uni_char *dev_usn, const uni_char *dev_loc);
	// Return true if the two devices are basically the same (no check done on attributes, apart the location)
	BOOL IsSameDevice(UPnPDevice *dev);
	/// Get the current Service (could be for IP or PPP WAN Connection)
	const uni_char *GetService() { return (IsPPP())?UPNP_WANPPPCONNECTION:UPNP_WANIPCONNECTION; }
	/// Get the URL of the current service (could be for IP or PPP WAN Connection)
	const OpString *GetServiceURL() { return (IsPPP())?&ppp_service_url:&ip_service_url; }
	/// Get the event URL of the current service (could be for IP or PPP WAN Connection)
	const OpString *GetEventURL() { return (IsPPP())?&ppp_event_url:&ip_event_url; }
	/// Set the service URL for the IP Connection
	OP_STATUS SetIPServiceURL(const uni_char *svc_url) { return ip_service_url.Set(svc_url); }
	/// Set the events URL for the IP Connection
	OP_STATUS SetIPEventURL(const uni_char *evt_url) { return ip_event_url.Set(evt_url); }
	/// Set the service URL for the PPP Connection
	OP_STATUS SetPPPServiceURL(const uni_char *svc_url) { return ppp_service_url.Set(svc_url); }
	/// Set the events URL for the PPP Connection
	OP_STATUS SetPPPEventURL(const uni_char *evt_url) { return ppp_event_url.Set(evt_url); }
	/// True if it is evaluating the PPP connection
	BOOL IsPPP() { return ppp; }
	/// Set the PPP flag, to choose between PPP and IP connection
	void SetPPP(BOOL b) { ppp=b; }
	/// Set tha data about the device manufacturer and model
	OP_STATUS SetDeviceBrandAndModel(OpString *brand, OpString *device_name, OpString *device_description, OpString *device_number, OpString *device_serial);
	/// Get the last description URL used (used to stop the "advertisement flooding")
    OpString *GetCurrentDescriptionURL() { return &current_description_url; }
    /// Get the last description time (used to stop the "advertisement flloding")
    double GetCurrentDescriptionTime() { return current_description_time; }
    /// Set the URL of the description, and update the time, if required
    OP_STATUS SetCurrentDescriptionURL(const OpString &str, BOOL update_time);
    /// Set the reason of the opening (used for debugging)
    void SetReason(UDPReason reason_opening) { reason=reason_opening; }
    /// Get the reason of the opening
    const char * GetReason();
    /// Return the reference object used for UPnP::known_devices
    ReferenceObject<UPnPDevice> *GetReferenceObjectPtrKnownDevices() { return &ref_obj_known; }
    /// Return the reference object used for UPnPXH_Auto
    ReferenceObject<UPnPDevice> *GetReferenceObjectPtrAuto() { return &ref_obj_auto; }
    /// Return the trust level
    TrustLevel GetTrust() { return trust; }
    /// Set the trust level
    void SetTrust(TrustLevel tl) { trust=tl; }
    /// TRUE if the device needs to be deleted (for duplication)
    BOOL IsToBeDeleted() { return to_be_deleted; }
    /// Set if the device needs to be deleted (for duplication)
    void SetToBeDeleted(BOOL b) { to_be_deleted=b; }
    /// Set the period before the advertisement expires (only if seconds > 0)
    void SetExpirePeriod(UINT32 seconds) { if(seconds>0) expire_period=seconds; }
    /// Get the moment (time in MS, according to g_op_time_info->GetWallClockMS())
    double GetExpireMoment() { OP_ASSERT(expire_period>0); return current_description_time+expire_period*1000; } 
};

/**
  UPnP support, used for Port Opening and Services Discovery.<br />
  This class try to open a port in every Internet Gateway Device connected to the network, to enable incoming TCP
  connections and to allow an Opera Unite Server to work with the client in a true P2P way, without having to use the proxy.<br />
  For the details on of the algorithm, see documentation/index.html.<br />
  <br />
  Minimal Example:<br />
  <code>
	UPnP *upnp=new UPnP(16640, 16670, TRUE, TRUE);

	if(!upnp)
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(upnp->StartUPnPDiscovery());

	return OpStatus::OK;
  </code>
  Usually the UPnP object must live for all the time that the Web Server is listening, because network devices can be added at runtime.<br />
  <br />
  In a real case scenario, you probably want to receive notifications about ports opening; to do so, you neeed to supply a UPnPListener,
  calling the AddListener() function.<br /> Remember to call RemoveListener() before destroying the listener.
  <br />
  You should need to use the other methods only to change the UPnP process implementation, not to use the object "as is".
  */ 
class UPnP: public XMLParser::Listener
{
private:
    UINT16 debug_mode;							///< Used to simulate problematic devices
	BOOL stop_search;							///< FALSE to try all the UPnP devices detected
	//OpAutoVector<UPnPDevice> known_devices;	///< Devices discovered
	ReferenceUserList<UPnPDevice> known_devices;	///< Devices discovered
	BOOL test_completed;						///< Used to signal only once that the SelfTest has been completed
	OpAutoVector<NetworkCard> network_cards;	///< Network Cards found
	UPnPRetryMessageHandler scheduler;			///< Object use as a scheduler, to check for Network Cards number change
	OpSocketAddress *saUDPDiscovery;			///< UDP Discovery Socket address (UPNP_DISCOVERY_ADDRESS)
	UPnP_DiscoveryListener dev_lsn;				///< UDP Device Listener
	BOOL self_delete;							///< The object must delete itself on failure
	BOOL discovery_started;						///< TRUE if the discovery process is already started (it checks also the NIC)
	BOOL port_bound;							///< TRUE if the discovery port has been bound
    
public:
    /*!
		Constructor
		@param try_all_devices TRUE to try to open every device
    */
    UPnP(BOOL try_all_devices);
    // Destructor
    ~UPnP();
    
    /// XMLParser::Listener Interface
    virtual void Continue(XMLParser *parser) { }
	virtual void Stopped(XMLParser *parser) { }
	virtual BOOL Redirected(XMLParser *parser) { return FALSE; }  // Disable redirect
	
    /** Create the object that can be used to open a  port on a router; the caller is responsible for deleting both of the objects
      @param upnp UPnP object (usually the caller just need to delete it)
      @param logic Object that can open the port (the caller can delete it after calling RemoveLogic, or just let the upnp object delete it))
      @param start_port Beginning of the port range
      @param end_port End of the port range
      @param open_every_card True to try to 
    **/
    static OP_STATUS CreatePortOpeningObject(UPnP *upnp, UPnPPortOpening *&logic, int start_port, int end_port);
    
	/// Main function, used to start the full UPnP discovery process;
    OP_STATUS StartUPnPDiscovery();
#ifdef UPNP_SERVICE_DISCOVERY
	/// Get the object used for the DOM API
	static UPnP *GetDOMUPnpObject() { return g_upnp_dom; }
	/// Get the object used for the DOM API (it has to be set using SetDOMUPnpObject()
	//static void SetDOMUPnpObject(UPnP *obj) { g_upnp_dom=obj; }
	/** Create a "dummy" object that subscribes to Unite request (this is usefull for the DOM API)
	*/
	static OP_STATUS CreateUniteDiscoveryObject(UPnP *upnp, UPnPServicesProvider *prov);
	
    /// Function used to advertise all the service providers and their services.
    OP_STATUS AdvertiseAllServicesProviders(BOOL force_update, BOOL delay, UINT num_times=3);
    /// Function used to advertise a single service provider
    OP_STATUS AdvertiseServicesProvider(UPnPServicesProvider *serv_prov, BOOL force_update, BOOL delay, UINT num_times=3);
    // Send an ssdp:byebye message
    OP_STATUS SendByeBye(UPnPServicesProvider *serv_prov, UINT num_times=3);
#endif
    
    /** Add a (control point) logic; it will be deleted automatically
		@param logic Control Point to add
		@param notify_devices if TRUE, OnNewDevice() will be called for every device
		@param broadcast_search	if TRUE, a search for the devices the logic is interested in will be performed
    */
	OP_STATUS AddLogic(UPnPLogic *logic, BOOL notify_devices=FALSE, BOOL broadcast_search=TRUE) { return dev_lsn.AddLogic(logic, notify_devices, broadcast_search); }
	/// Remove a logic (control point)
	void RemoveLogic(UPnPLogic *logic) { dev_lsn.RemoveLogic(logic); }
#ifdef UPNP_SERVICE_DISCOVERY
	/// Add a service provider
    OP_STATUS AddServicesProvider(UPnPServicesProvider *upnp_services_provider, BOOL advertise) { return dev_lsn.AddServicesProvider(upnp_services_provider, advertise); }
    /// Remove a service provider
    OP_STATUS RemoveServicesProvider(UPnPServicesProvider *upnp_services_provider) { return dev_lsn.RemoveServicesProvider(upnp_services_provider); }
    /// Return the number of services providers
    int GetNumberOfServicesProviders() { return dev_lsn.GetNumberOfServicesProviders(); }
    // Return a Services Provider
    UPnPServicesProvider *GetServicesProvider(UINT index) { return dev_lsn.GetServicesProvider(index); }
#endif // UPNP_SERVICE_DISCOVERY
    
    /// Stop the search for the next device
	void StopDeviceSearch(BOOL b) { stop_search=b; }
	/// Stop the search for the next device
	BOOL GetDeviceSearch() { return stop_search; }
	// Return the number of UPnP Devices available
	UINT32 GetNumberOfDevices() { return known_devices.GetCount(); }
	// Return the number of UPnP Devices available
	UPnPDevice *GetDevice(UINT32 index) { OP_ASSERT(index<GetNumberOfDevices()); return known_devices.GetPointer(index);  }
	/// Get the device attached to the specified socket
	//UPnPDevice *GetDeviceFromSocket(OpUdpSocket *socket);
	/// Get a device from the device ID
	UPnPDevice *GetDeviceFromUSN(const char *usn, const char *location);
	/// Get a device from the device ID
	UPnPDevice *GetDeviceFromUSN(const uni_char *usn, const uni_char *location);
	/// Teturn the NetworkCard associated with the socket
	NetworkCard *GetNetworkCardFromSocket(OpUdpSocket *socket);
	/// Used to test the cases that are difficult to simulate using real hardware
	BOOL IsDebugging(UINT16 flag) { return (debug_mode&flag)?TRUE:FALSE; }
	/// Return the network cards
	BOOL IsNetworkCardPresent(NetworkCard *card);
	/// Log a message (used mainly to help supporting new routers)
#ifdef UPNP_LOG
	void Log(const uni_char *position, const uni_char *msg, ...);
#endif
	/// Remove a device using the serial name
	OP_STATUS RemoveDevice(const char *usn, const char *location);
	/// Remove a device
	OP_STATUS RemoveDevice(UPnPDevice *device);
	/// create a new device and add it to the list of the known devices.
    OP_STATUS AddNewDevice(NetworkCard *network_card, const uni_char *controlURL, const uni_char *usn, UINT32 expire_period);
    /// Remove expired devices
	OP_STATUS CheckExpires();
    /// Set if the UPnP object must delete itself
    void SetSelfDelete(BOOL del) { self_delete=del; }
    /// TRUE if the object is supposed ot be deleted on failure
    BOOL GetSelfDelete() { return self_delete; }
    // Return the discovery listener
	UPnP_DiscoveryListener &GetDiscoveryListener() { return dev_lsn; }
	/// Start a UPnP Action call and make the answer processeed by the handler
	OP_STATUS StartActionCall(UPnPDevice *device, UPnPXH_Auto *&uxh, UPnPLogic *upnp_logic);
	/// Starts the XML Processing of the specified URL. The Simple_XML_Handler will be deleted at the end of the XML Processing, or, in case of errors, at the end of the function call
	OP_STATUS StartXMLProcessing(OpString *url_string, UPnPXH_Auto *&xth, MessageHandler *mh, const char *postMessage, BOOL with_headers);
	/// Construct the handler and starts the XML Processing of the specified URL. The Simple_XML_Handler handler will be deleted at the end of the XML Processing, or, in case of errors, at the end of the function call
	OP_STATUS StartXMLProcessing(UPnPDevice *device, OpString *url_string, UPnPXH_Auto *&uxh, const char *postMessage, BOOL with_headers, UPnPLogic *upnp_logic);
	/// Notify all the interested control points that the description of a device is now available
	void NotifyControlPointsOfDevice(UPnPDevice *device, BOOL new_device);
	/// TRUE if there are network cards available
	BOOL NetworkCardsAvailable() { return network_cards.GetCount()>0; }
	/// Return TRUE if the IP is suitable for Unite, so no IPv6 and no localhost
	static BOOL IsIPSuitable(OpString *card_ip);
	/// MD5 Encrypt the string privatekey+nonce, and put the hexadecimal result in output
	static OP_STATUS Encrypt(const OpStringC8 &nonce, const OpStringC8 &private_key, OpString8 &output);
#ifdef UPNP_ANTI_SPOOF
    /** 
		Check if an address has been spoofed; the listener will be notified.
		
		@param url URL provided by the router
		@param address socket address that answered toi the UDP broadcast message
		@param listener Listener that will be notified of the result; it could be called even before the function return
    */
    OP_STATUS CheckIfAddressIsSpoofed(const char *url, OpSocketAddress *address, SpoofListener *listener);
#endif

#ifdef _DEBUG
	/// Debug function used to duplicates devices, for the selftests
	OP_STATUS DebugDuplicateDevice(UINT dev, UINT num_duplicates, BOOL add_itself);
#endif
	/// Fix the "duplicated devices" problem; keep it's the device to keep (in case of duplicates), because it is the current one, NULL means auto keep (last)
	OP_STATUS DeleteDeviceDuplicates(UPnPDevice *keep);

	/// Returns TRUE if the discovery process is already started (it checks also the NIC)
	BOOL IsDiscoveryStarted() { return discovery_started; }
	/// Returns TRUE if the discovery port has been bound
	BOOL IsPortBound() { return port_bound; }

    
private:
	friend class UPnPXH_Auto;
	friend class UPnP_DiscoveryListener;
	friend class UPnPLogic;

    /// Broadcast an UDP UPnP Discovery Message, to find interesting devices that the logic is interested in
    OP_STATUS BroadcastUDPDiscoveryForSingleLogic(UPnPLogic *logic);
    /// Broadcast an UDP UPnP Discovery Message, to find interesting devices
	OP_STATUS BroadcastUDPDiscovery(NetworkCard *nic);
	/// Subscribe to UDP Discovery Advertisement Messages (tobe informed about new IGD devices)
	OP_STATUS SubscribeUDPDiscovery(NetworkCard *nic);
	/// Broadcast a packet to the UDP Multicast Address
	OP_STATUS SendBroadcast(NetworkCard *nic, char *mex);

    /// Create a new NetworkCard and a new Socket bound to the address specified, and add it to network_cards; if required, the OpSocketAddress will be copied
	NetworkCard *AddNewNetworkCard(OpString *card_ip);
	/// Return TRUE if the logging is enabled
	BOOL IsLogging() { 
		#ifdef _DEBUG
			return TRUE;
		#else
			return FALSE;
		#endif
	}
	/// TRUE if the test has been completed
	//BOOL GetTestCompleted() { return test_completed; } 
	/// Set if the test has been completed
	//void SetTestCompleted(BOOL b) { test_completed=b; } 
#ifdef UPNP_LOG
	/// Log an incoming network packet
	void LogInPacket(const uni_char *description, const char *httpPath, const uni_char *action, const char *message);
	/// Log an outgoing network packet
	void LogOutPacket(const uni_char *description, const uni_char *action, const char *message);
#endif
	/// Return the network cards
	OpAutoVector<NetworkCard> *GetNetworkCards() { return &network_cards; }
	/// Delete all the network cards
	void DeleteNetworkCards() { network_cards.DeleteAll(); }
	// Called when there is an error
	OP_STATUS HandlerFailed(UPnPXH_Auto *child, const char *mex) { return dev_lsn.HandlerFailed(child, mex); }

#if defined(UPNP_SERVICE_DISCOVERY) && defined(ADD_PASSIVE_CONTROL_POINT)
	// Control Point used to ask notifications about Unite devices
	UPnPPassiveControlPoint *cp_unite;
#endif
	
private:
	friend class UPnPRetryMessageHandler;
	// Function that really starts the UPnP process on the network cards supplied (cards!=NULL) or in the current network_cards (canrds==NULL)
	OP_STATUS StartUPNPDiscoveryProcess(OpAutoVector<OpString> *cards);
	// TRUE if the network cards list has changed (at the moment, it the position changed, the list is considered changed
	BOOL IsNetworkCardsListChanged(OpAutoVector<OpString> *new_cards);
	// Update the Network Cards list with the new one (at the momen, all the card will be deleted, and new sockets created)
	OP_STATUS UpdateNetworkCardsList(OpAutoVector<OpString> *new_cards);
	
private:
	/// Compute the URL string
	OP_STATUS ComputeURLString(UPnPDevice *device, OpString &deviceString, const OpString *url);
	/// Retrieve an HTTP SOAP Action call message in out_http_message; soap_mex contains the SOAP Action XML call
	OP_STATUS GetSoapMessage(UPnPDevice *device, OpString *out_http_message, const uni_char *soap_service_type, const OpString *soap_control_url, const OpString *soap_mex, const uni_char* soap_action, BOOL mpost);
#ifdef UPNP_LOG
	// Log a message (used mainly to help supporting new routers)
	void LogMessage(const uni_char* msg, va_list arglist);
#endif
};

/// Class that contains a name and a value
class NameValue
{
private:
	/// Name
	OpString name;
	/// Value
	OpString value;
	/// TRUE if the attribute need to be written even if empty
	BOOL required;
	
public:
	/// Set the name
	OP_STATUS SetName(const char *param_name, BOOL is_required=FALSE) { required=is_required; return name.Set(param_name); }
	/// Set the value
	OP_STATUS SetValue(const char *param_value) { return value.Set(param_value); }
	/// Set the value
	OP_STATUS SetValue(const uni_char *param_value) { return value.Set(param_value); }
	/// Set the value to an empty string
	void ResetValue() { if(value.CStr()) value.CStr()[0]=0; }
	/// TRUE if the values is not an empty string
	BOOL HasValue() { return required || (value.CStr() && value.CStr()[0]); }
	
	/// Get the name
	OpStringC &GetName() { return name; }
	/// Get the value
	OpStringC &GetValue() { return value; }
};

/// Class that has a vector of attrbitues (NameValue objects)
class AttributesVector
{
private:
	/// Attributes
	OpAutoVector <NameValue> attributes;
	
public:
	virtual ~AttributesVector() { }
	/// Return the number of attributes
	int GetAttributesCount() { return attributes.GetCount(); }
	
	/** Create num attributes; it means that the vector will be expanded and the NameValue objects will be allocated; in case of error, less attributes could be created
		@param num Number of attributes to create
		@param delete_if_errorr In case of error, the attributes created will be deleted
	*/
	OP_STATUS CreateAttributes(UINT num, BOOL delete_if_errorr);
	
	/// Get an attribute
	NameValue *GetAttribute(UINT index) { OP_ASSERT(index<attributes.GetCount()); return attributes.Get(index); }
	/// Get the value of an attribute
	OpStringC *GetAttributeValue(UINT index) { OP_ASSERT(index<attributes.GetCount()); return attributes.Get(index)?&attributes.Get(index)->GetValue():NULL; }
	/// Get the value of an attribute as an uni_char pointer
	const uni_char *GetAttributeValueUni(UINT index) { OpStringC *t=GetAttributeValue(index); return t?t->CStr():NULL; }
	/// Get an attribute using the name as index
	NameValue *GetAttributeByName(const uni_char *name);
	/// Get an attribute using the name as index
	NameValue *GetAttributeByNameLen(const uni_char *name, int tag_len);
	/// Set an attribute name
	OP_STATUS SetAttributeName(UINT index, const char *name, BOOL required=FALSE);
	/// Set an attribute value
	OP_STATUS SetAttributeValue(UINT index, const uni_char *value);
	/// Set an attribute value
	OP_STATUS SetAttributeValue(UINT index, const char *value);
	/// Set an attribute value using the name as index
	OP_STATUS SetAttributeValueByName(const uni_char *name, const uni_char *value);
	/// Set an attribute value using the name as index
	OP_STATUS SetAttributeValueByNameLen(const uni_char *name, int tag_len, const uni_char *value);
	/// Copy the value of the attrbiute to an OpString
	OP_STATUS CopyAttributeValue(UINT index, OpString &dest);
	
	/**
		Add the XML fragment of the attributes to the buffer; it will generate something like:
		<parent_tag><attribute1>..</attribute1>...<attributeN>...</attributeN></parent_tag>
	*/
	OP_STATUS AppendAttributesXML(XMLFragment *xf, const uni_char *parent_tag);
	
	/// Reset the value of all the attributes
	virtual void ResetAttributesValue();
};

/// Class that describe a single UPnP service.
/// The standard UPnP attributes are already defined, others could be added
class UPnPService: public AttributesVector
{
private:
	/// True if the service is visible
	BOOL visible;
	/// Reference object used for the list
	ReferenceObjectSingle<UPnPService> ref_obj_list;
	/// Reference object used for the XML parsing
	ReferenceObjectSingle<UPnPService> ref_obj_xml;
public:
	UPnPService(): visible(TRUE), ref_obj_list(this), ref_obj_xml(this) { }
	/// All the attributes required for a service. See the UPnP specifications for more details
	enum ServiceAttribute
	{
		/// Service type (for example: urn:opera-com:service:OperaUniteTest1:1)
		SERVICE_TYPE=0,
		/// Unique service ID (for example: urn:opera-com:serviceId:OperaUniteTest1)
		SERVICE_ID=1,
		/// Service description URL
		SCPD_URL=2,
		/// URL used for control
		CONTROL_URL=3,
		/// URL used for eventing
		EVENT_URL=4,
		/// Name of the service (Unite custom attribute)
		NAME=5,
		/// Description of the service (Unite custom attribute)
		DESCRIPTION=6,
		/// This will be the first number of the additional attributes that can be registerd
		ADDITIONAL_SERVICE_ATTRIBUTES=7
	};
	
	/**
		Second phase constructor, that allocates and set the name to all the attributes
	*/
	virtual OP_STATUS Construct();

	virtual ~UPnPService() {}
	
	/// TRUE if the service is visible
	BOOL IsVisible() { return visible; }
	/// Set the visibility value
	void SetVisible(BOOL b) { visible=b; }
	
	/// Reset also "visible" attribute
	virtual void ResetAttributesValue() { visible=TRUE; AttributesVector::ResetAttributesValue(); }
	
	// Return the reference object used for lists
	ReferenceObject<UPnPService> *GetReferenceObjectPtrList() { return &ref_obj_list; }
	// Return the reference object used for the XML aprsing
	ReferenceObject<UPnPService> *GetReferenceObjectPtrXML() { return &ref_obj_xml; }
};

/**
	Class that define a service provider; it will be used to publish services (UPnP service discovery)
*/
class UPnPServicesProvider: public AttributesVector
{
protected:
	friend class UPnPXH_DescriptionGeneric;

	/// Seconds to wait between each new advertisement (0 means no advertisement)
	int advertise_time;
	/// URL used by control point to get the description, complete with the challenge
	OpString full_description_url;
	/// URL used by control point to get the description, without the challenge
	OpString short_description_url;
	// TRUE if the service provider is disabled, for example after a Bye Bye
	BOOL disabled;
	// Object that keeps track of the referencies and remove them
	ReferenceObjectSingle<UPnPServicesProvider> ref_obj_prov;

protected:
	// The device has been discovered, so it is not an internal Service Provider
	BOOL discovered;
	
	/** Constructor
		@param ad_time
	*/
	UPnPServicesProvider(int ad_time=900): ref_obj_prov(this) { advertise_time=ad_time; disabled=FALSE; discovered=FALSE; }
	
	/// Used to remove all services; usually subclasses don't need to implement it
	virtual void RemoveAllServices() { }
	/// Used to remove a service; usually subclasses don't need to implement it
	virtual void RemoveService(UPnPService *srv) { }
	
	/// Add a new service; usually subclasses don't need to implement it
	virtual OP_STATUS AddService(UPnPService *service) { return OpStatus::ERR_NOT_SUPPORTED; }

public:
	/// All the attributes available for a device. See the UPnP specifications for more details
	enum ServicesProviderAttributes
	{
		/// Device type [required] (for example: urn:opera-com:device:OperaUnite:1)
		DEVICE_TYPE=0,
		/// Friendly name [required]
		FRIENDLY_NAME=1,
		/// Manufacturer name [required]
		MANUFACTURER=2,
		/// Manufacturer URL [optional]
		MANUFACTURER_URL=3,
		/// Model Description [recommended]
		MODEL_DESCRIPTION=4,
		/// Model Name [required]
		MODEL_NAME=5,
		/// Model Number [recommended]
		MODEL_NUMBER=6,
		/// Model URL [optional]
		MODEL_URL=7,
		/// Serial Number [recommended]
		SERIAL_NUMBER=8,
		/// Unique Device Name [required]
		UDN=9,
		/// Universal Product Code (12-digit numeric code) [optional]
		UPC=10,
		/// Presentation URL [optional]
		PRESENTATION_URL=11,
		// Unite Username
		UNITE_USER=12,
		// Device Name
		UNITE_DEVICE=13,
		// Response (without authentication) to the Challenge sent by the client
		CHALLENGE_RESPONSE=14,
		// Response (with authentication) to the Challenge sent by the client
		CHALLENGE_RESPONSE_AUTH=15,
		// Second Nonce to use for the authentication
		NONCE_AUTH=16,
		// URI to icon
		DEVICE_ICON_RESOURCE=17,
		// Generic payload
		PAYLOAD=18,
		/// This will be the first number of the additional attributes that can be registerd
		ADDITIONAL_DEVICE_ATTRIBUTES=19
	};
	
	virtual ~UPnPServicesProvider() { }
	
	/// Second phase constructor.
	OP_STATUS Construct();
	
	/// Unique string identifying the device type (for example: urn:opera-com:device:OperaUnite:1)
	virtual const char* GetDeviceTypeString() = 0;

	/// Return true if this provider accept the search request
	virtual BOOL AcceptSearch(const OpStringC &device_type, SearchTargetMode &mode);
	/// Return true if this provider accept the search request
	virtual BOOL AcceptSearch(const OpStringC8 &device_type, SearchTargetMode &mode);
	/// Return true if this provider accept the search request
	virtual BOOL AcceptSearch(const char *device_type, SearchTargetMode &mode);
	
	/// Return the advertisement time
	int GetAdvertisementTime() { return advertise_time; }
	
	/// Return the number of services currently available
	virtual UINT32 GetNumberOfServices() = 0;
	
	ReferenceObject<UPnPServicesProvider> &GetReferenceObject() { return ref_obj_prov; }
	
	/// Short-cut for the Device Type
	const uni_char *GetDeviceType() { return GetAttributeValueUni(DEVICE_TYPE); }
	
	/// TRUE if the device has been discovered (so he does not qualify for advertising & Co.)
	BOOL IsDiscovered() { return discovered; }
	/// Return true if the Services Provider is disabled
	virtual BOOL IsDisabled() { return disabled; }
	/// Return true if the Services Provider is disabled
	virtual void SetDisabled(BOOL b) { OP_ASSERT(b==TRUE || b==FALSE); disabled=b; }
	/// Return true if the Services Provider is visible on the machine running it
	virtual BOOL IsVisibleToOwner() { return FALSE; }
	
	/**
		Get the required service.
		
		@param index: index of the service (must be between 0 and GetNumServices()
		@param service output parameter that will contain a pointer to the service required; 
			   the pointer is owned by the UPnPServicesProvider object, so the caller cannot delete it
			   or assume that it will be available after calling another method of this object (this is 
			   done to recycle the object and reduce memory fragmentation).
	*/
	virtual OP_STATUS RetrieveService(int index, UPnPService *&service) = 0;
	
	/// Retrieve a service from its name
	virtual OP_STATUS RetrieveServiceByName(const uni_char *service_name, UPnPService *&service);
	
	// Event send when a request has been accepted
	virtual void OnSearchRequestAccepted(OpStringC8 search_target, OpSocketAddress *addr) { }
	
	/// Create the XML used for the description phase. Of course the attributes need to be set in advance
	virtual OP_STATUS CreateDescriptionXML(ByteBuffer &buf, OpString8 &challenge);
	/// Create the XML used for SCPD
	virtual OP_STATUS CreateSCPD(ByteBuffer &buf);
	/// Create the packet used for ssdp:alive. Remember to update the address
	virtual OP_STATUS CreateSSDPAlive(OpString8 &str, BOOL force_update);
	/// Create the packet used for ssdp:byebye. Remember to update the address
	virtual OP_STATUS CreateSSDPByeBye(OpString8 &str);
	
	// Set the description URL (both long and short version)
	OP_STATUS SetDescriptionURL(const OpString &description);
	// Get the description URL, without the challenge
	const OpStringC &GetDescriptionURL() { return short_description_url; }
	// Get the description URL, with the challenge
	const OpStringC &GetFullDescriptionURL() { return full_description_url; }
	// Retrieve the presentation URL (used for accessing the root service of Unite)
	const uni_char *GetPresentationURL() { return GetAttributeValueUni(PRESENTATION_URL); }
	// Updated the description URL, with the IP address that needs to be used
	virtual OP_STATUS UpdateDescriptionURL(OpSocketAddress *addr) { return OpStatus::OK; }
	// Update the values and the custom attributes
	virtual OP_STATUS UpdateValues(OpString8 &challenge) { return OpStatus::OK;  }
	// TRUE if the service device is local (meaning generated by this instance of Opera)
	virtual BOOL IsLocal() { return FALSE; }
};

/**
	Service Provider discovered using UPnP
*/
class DiscoveredUPnPServicesProvider: public UPnPServicesProvider
{
private:
	//OpAutoVector<UPnPService> services;
	ReferenceUserList<UPnPService> services;

	OpString8 m_deviceString;

	/// Used to remove all services
	//virtual void RemoveAllServices() { services.RemoveAll(FALSE, TRUE); }
	/// Used to remove a service
	virtual void RemoveService(UPnPService *srv) { if(srv) services.RemoveReference(srv->GetReferenceObjectPtrList()); }
	
	virtual OP_STATUS AddService(UPnPService *service);
	virtual const char* GetDeviceTypeString() { return m_deviceString.CStr(); }

public:
	virtual UINT32 GetNumberOfServices() { return services.GetCount(); }
	
	OP_STATUS Construct(const char* devstring) { RETURN_IF_ERROR(UPnPServicesProvider::Construct()); return m_deviceString.Set(devstring); }

	virtual OP_STATUS RetrieveService(int index, UPnPService *&service) { service=services.GetPointer(index); return (service)?OpStatus::OK:OpStatus::ERR_NO_SUCH_RESOURCE; }
	DiscoveredUPnPServicesProvider(): services(FALSE, TRUE) { discovered=TRUE; }
};


/**
  Base class used for implementing the logic required by a UPnP process (for example, port opening).
  Basically it enables a Control Point.
*/
class UPnPLogic
{
friend class UPnP_DiscoveryListener;
friend class UPnP;
friend class Simple_XML_Handler;

protected:
	UPnP *upnp;		// UPnP object
	OpString8 last_error;						///< Last error recorded
	OpAutoVector <UPnPListener> listeners;		///< List of the listeners attached to the object
	URL url_subscribe;							///< URL used to subscribe to the events notification
	EventSubscribeHandler event_handler;		///< Handler for the event subscribe
	OpString8 target;							///< Search Target managed by the logic
	BOOL locked;								///< When the control point id locked, it does not accept search targets
	/// References Object
	ReferenceObjectList<UPnPLogic> ref_obj;

protected:
#ifdef UPNP_LOG
	/// Log a message
	void Log(const uni_char *position, const uni_char *msg) { upnp->Log(position, msg); }
#endif
	/// Compute the URL string
	OP_STATUS ComputeURLString(UPnPDevice *device, OpString &deviceString, const OpString *url) { return upnp->ComputeURLString(device, deviceString, url); }
	// FIXME: this function needs to be more generic
	/// Main function, used to start the full UPnP discovery process;
    OP_STATUS StartUPnPDiscovery() { return upnp->StartUPnPDiscovery(); }
    
public:
	/// Constructor
	UPnPLogic(UPnP *upnp_father, const char *search_target): upnp(upnp_father), locked(FALSE), ref_obj(this) { target.Set(search_target); }
	// Return the search target
	char *GetSearchTarget() { return target.CStr(); }
	/// Virtual destructor
	virtual ~UPnPLogic();
	/// Disable the Control Point
	virtual void Disable();
	/// Enable again the control point (it does not mean that the control point will do something, but this call is a requirement)
	virtual OP_STATUS ReEnable();
	/// Called when an important handler has failed
	virtual OP_STATUS HandlerFailed(UPnPXH_Auto *child, const char *mex) = 0;
	/// Return the reference object
	ReferenceObject<UPnPLogic> *GetReferenceObjectPtr() { return &ref_obj; }
	
	/// Get the last error notified
	const char *GetLastError() { return last_error.CStr(); }
	/// Add a listener that will be notified of every UPnP event. If the operation is successful, the UPnPLogic object will take care of deleting the object when appropriate
    OP_STATUS AddListener(UPnPListener *upnp_listener);
    /// Remove a listener
    OP_STATUS RemoveListener(UPnPListener *upnp_listener);
	// TRUE if the target is accepted
	virtual BOOL AcceptSearchTarget(const OpStringC8 &search_target);
	// TRUE if the target is accepted
	virtual BOOL AcceptSearchTarget(const char *search_target);
	// TRUE if the target is accepted
	virtual BOOL AcceptDevice(UPnPDevice *dev);
	// Create the discovery message to send via UDP
	OP_STATUS CreateDiscoveryessage(OpString8 &dscv_mex);
	// Called when a new device is avalilable, or an old one as been updated
	virtual void OnNewDevice(UPnPDevice *device) = 0;
	// Called when a device is removed (byebye)
	virtual void OnRemoveDevice(UPnPDevice *device) = 0;
};

#ifdef ADD_PASSIVE_CONTROL_POINT
/// Class designed to provide a Control Point with the only reason to have the interested devices collected and shown in the devices list
class UPnPPassiveControlPoint: public UPnPLogic
{
public:
	UPnPPassiveControlPoint(UPnP *upnp_father, const char *search_target): UPnPLogic(upnp_father, search_target) { }
	
	virtual OP_STATUS HandlerFailed(UPnPXH_Auto *child, const char *mex) { return OpStatus::OK; }
	virtual void OnNewDevice(UPnPDevice *device) { }
	virtual void OnRemoveDevice(UPnPDevice *device) { }
};
#endif
#endif
#endif

