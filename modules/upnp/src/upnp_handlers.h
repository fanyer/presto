/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifdef UPNP_SUPPORT

#ifndef UPNP_HANDLERS_H_
#define UPNP_HANDLERS_H_

#include "modules/upnp/src/upnp_soap.h"
#include "modules/upnp/src/upnp_util.h"

class UPnP;
class UPnPLogic;
class UPnPServicesProvider;
class UPnPService;

#define UPNP_UDP_BUFFER_SIZE 4096

#ifdef UPNP_ANTI_SPOOF
	class UPnPSpoofCheckOHRListener;
	
	// Listener used to check if an UPnP URL has been spoofed
	class SpoofListener
	{
	protected:
		// AntiSpoof check listener
		UPnPSpoofCheckOHRListener *upnp_spoof_lsn;
	public:
		SpoofListener() { upnp_spoof_lsn=NULL; }
		
		void SetUPnPSppofListener(UPnPSpoofCheckOHRListener *spoof_listener) { upnp_spoof_lsn=spoof_listener; }
		
		// Called when the address has been spoofed, so UPnP process has to stop
		virtual void OnAddressSpoofed()=0;
		// Called when the address is legit, so UPnP process can continue
		virtual void OnAddressLegit(OpSocketAddress *legit_address)=0;
		// Called when there is an error
		virtual void OnError(OP_STATUS error)=0;
		// Return the host resolver used by this listener
		virtual OpHostResolver *GetSpoofResolver()=0;
		
		virtual ~SpoofListener();
	};

	// Host Resolver Listener used to detect spoofing on UPnP
	class UPnPSpoofCheckOHRListener: public OpHostResolverListener
	{
	private:
		// Spoof listener (not to be deleted)
		SpoofListener *listener;
		// Address
		OpSocketAddress *address;
		
	public:
		UPnPSpoofCheckOHRListener() { ResetListener(); address=NULL; }
		virtual ~UPnPSpoofCheckOHRListener() { OP_DELETE(address); }
		
		// Set the listener and the address (the address is copied)
		OP_STATUS SetData(SpoofListener *spoof_listener, OpSocketAddress *addr);
		
		// Set the listener to NULL
		void ResetListener() { listener=NULL; }
		
		void OnHostResolved(OpHostResolver* host_resolver);
		void OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error);
	};
#endif

// Listener in charge of processing the answer to the UDP Discovery message broadcasted looking for IGD Devices
class UPnP_DiscoveryListener: public OpUdpSocketListener
{
private:
	friend class UPnP;
	friend class UPnPDevice;
	//OpAutoVector<XMLParser>xml_parsers;			///< XML Parser that need to be deleted on the destructor because "running"...
	ReferenceUserList<XMLParser>xml_parsers;			///< XML Parser that need to be deleted on the destructor because "running"...
	
#ifdef UPNP_ANTI_SPOOF
	// Listener used to continue the ssdp:alive process only if the URL is not spoofed
	// The listener is self destroying
	class AliveSpoofListener: public SpoofListener
	{
	private:
		OpString8 location;			// Location
		OpString8 target;			// Search Target (type of UPnP device monitored)
		OpString8 target2;			// Opera specific Search Target
		OpUdpSocket *socket;		// Socket that sent the data
		OpString8 usn;				// Advertisement UUID
		OpString addr;				// Address of the socket
		UPnP *upnp;					// UPnP
		ReferenceUserList<UPnPLogic> *logics; // Logics to call
		OpHostResolver *ohr_spoof;	///< Host resolver used for the anti spoofing
		UINT32 expire_period;		/// Expire period, in seconds
		
		UDPReason spoof_reason;     // Reason that triggered the sppof check, used to call the right method
		
	public:
		AliveSpoofListener() { logics=NULL; upnp=NULL; ohr_spoof=NULL; spoof_reason=UDP_REASON_UNDEFINED; expire_period=1800; }
		virtual ~AliveSpoofListener() { OP_DELETE(ohr_spoof); }
		
		// The address has been spoofed
		virtual void OnAddressSpoofed();
		// The address is fine
		virtual void OnAddressLegit(OpSocketAddress *legit_address);
		// An error has been detected
		virtual void OnError(OP_STATUS error);		
		// Second phase constructor
		OP_STATUS Construct(UPnP *current_upnp, ReferenceUserList<UPnPLogic> *current_logics, OpUdpSocket *udp_socket, OpSocketAddress *address, const OpStringC8 &loc, const OpStringC8 &dev_usn, const OpStringC8 &search_target, const OpStringC8 &search_target2, UDPReason reason, UINT32 expire);
		virtual OpHostResolver *GetSpoofResolver() { return ohr_spoof; }
	};
#endif

private:
	// Temporary buffer used to read UDP
	char buf[UPNP_UDP_BUFFER_SIZE];		/* ARRAY OK 2008-08-12 lucav */
	// Main UPnP object
	UPnP *upnp;
	// Array with the logic / Control Points available (there could be more than one... e.g. one for port-opening and one for Unite...)
	ReferenceUserList<UPnPLogic> logics;
	// Address of the last M-Search (used to reduce a bit the number of packets sent)
	OpSocketAddress *last_msearch_address;
	// Time of the last M-Search (used to reduce a bit the number of packets sent)
	double last_msearch_time;
	// Target searched during last M-Search
	OpString8 last_msearch_target;
	//OpAutoVector<UPnPLogic> logics;
#ifdef UPNP_SERVICE_DISCOVERY
	// List of the service providers available (published by this instance of Opera)
	ReferenceUserList<UPnPServicesProvider> services_providers;
#endif // UPNP_SERVICE_DISCOVERY
	
	void OnSocketDataReady(OpUdpSocket* socket);
#ifdef UPNP_SERVICE_DISCOVERY
	// Manage M-Search requests. Len is the number of bytes read from the socket.
	void ManageMSearch(UINT len, HeaderList &hl, OpUdpSocket *socket, OpSocketAddress *addr);
	// Send the answer to an M-Search request
	OP_STATUS SendUnicastHTTPAnswerToMSearch(UPnPServicesProvider *serv_prov, OpUdpSocket *socket, OpSocketAddress *addr, const char *search_target, SearchTargetMode mode);
#endif
	// Manage Notify answer (advertisement / discovery phase). Len is the number of bytes read from the socket.
	void ManageNotify(UINT len, HeaderList &hl, OpUdpSocket *socket, OpSocketAddress *addr);
	// Manage the HTTP Unicast answer sent in response to an M-SEARCH packet. Len is the number of bytes read from the socket.
	void ManageHTTPAnswer(UINT len, HeaderList &hl, OpUdpSocket *socket, OpSocketAddress *addr);
	
#ifdef UPNP_ANTI_SPOOF	
	OP_STATUS ManageSpoofing(OpUdpSocket *socket, OpSocketAddress *addr, OpStringC8 location, OpStringC8 unique_service_url, OpStringC8 saarch_target, OpStringC8 saarch_target2, UDPReason reason, UINT32 expire_val);
#endif

	// "Fake" function used only to please the linker...
	virtual void dummy();
	
public:
	// Constructor
	UPnP_DiscoveryListener(UPnP *ptr);
	// Destructor
	~UPnP_DiscoveryListener() { OP_DELETE(last_msearch_address); }
	
	// Signal the error to all the logic
	OP_STATUS HandlerFailed(UPnPXH_Auto *child, const char *mex);
	/** Add a (control point) logic; it will be deleted by the ReferenceUserList
		@param logic Control Point to add
		@param notify_devices if TRUE, OnNewDevice() will be called for every device
		@param broadcast_search	if TRUE, a search for the devices the logic is interested in will be performed
    */
	OP_STATUS AddLogic(UPnPLogic *logic, BOOL notify_devices, BOOL broadcast_search);
	// Remove a logic object
	void RemoveLogic(UPnPLogic *logic);
#ifdef UPNP_SERVICE_DISCOVERY
	/// Add a service provider
    OP_STATUS AddServicesProvider(UPnPServicesProvider *upnp_services_provider, BOOL advertise);
    /// Remove a service provider
    OP_STATUS RemoveServicesProvider(UPnPServicesProvider *upnp_services_provider);
    /// Return the number of services providers
    int GetNumberOfServicesProviders() { return services_providers.GetCount(); }
    // Return a Services Provider
    UPnPServicesProvider *GetServicesProvider(UINT index) { OP_ASSERT(index<services_providers.GetCount()); return services_providers.GetPointer(index); }
    // Return the Services Provider requested
    UPnPServicesProvider *GetServicesProviderFromLocation(uni_char *location);
    // Return the Services Provider requested via a device-string
    UPnPServicesProvider *GetServicesProviderFromDeviceString(const char *devicestring);

#endif // UPNP_SERVICE_DISCOVERY

	// Manage the answer to the discovery message; it starts the Description phase, calling QueryDescription()
	OP_STATUS ManageDiscoveryAnswer(OpSocketAddress *legit_address, OpUdpSocket *socket, const char *location, const char *unique_service_url, UDPReason reason, const char *target, const char *target2, BOOL bypass_anti_flood);
	// Download the description of the device. 
	OP_STATUS QueryDescription(UPnPDevice *device, OpString &device_string, OpString &ch, BOOL bypass_anti_flood);
	// Manage the "ssdp:alive" message notification in the advertisment phase
	void ManageAlive(OpSocketAddress *legit_address, OpUdpSocket *socket, const char *location, const char *unique_service_url, const char *target, const char *target2);
	// Manage the "ssdp:byebye" message notification in the advertisment phase
	void ManageByeBye(const char *location, const char *unique_service_url);
	/// Add an XML parser to the list
    OP_STATUS AddXMLParser(UPnPXH_Auto *xth, XMLParser *parser);
    /// Add an XML parser to the list
    OP_STATUS RemoveXMLParser(XMLParser *parser);
    /// Read a Cache-Control header and retrieve the expire
    static UINT32 ReadExpire(const char *cache_control);
};

// Listener in charge of processing the answer to the UDP Discovery message broadcasted lloking for IGD Devices
class UPnP_EventsListener: public OpUdpSocketListener
{
	// Temporary buffer used to read UDP
	char buf[UPNP_UDP_BUFFER_SIZE];		/* ARRAY OK 2008-08-12 lucav */
	// Main UPnP object
	UPnP *upnp;
	// Logic of the process
	UPnPLogic *logic;
	
	void OnSocketDataReady(OpUdpSocket* socket);
	
public:
	UPnP_EventsListener(UPnP *ptr, UPnPLogic *upnp_logic) { upnp=ptr; logic=upnp_logic; }
};

// Massage Handler for the Event Subscribe UPnP request
class EventSubscribeHandler: public MessageObject
{
	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);
};


// Parsing of the SOAP answer for the description phase. Control Points handlers can inherit from this class to save some lines of code
class UPnPXH_DescriptionGeneric: public UPnPXH_Auto
{
private:
	/// Reference to the service currently used for XML parsing
	ReferenceUserSingle<UPnPService> cur_service_ref;
	// Challenge (if required)
	OpString8 challenge;
	
	// Manage the Challenge/Response part
	XMLTokenHandler::Result ManageChallenge(UPnPServicesProvider *prov);
	// Manage the start of the "service" tag
	XMLTokenHandler::Result ManageServiceStart(UPnPServicesProvider  *prov);
	// Manage the anti-spoofing part
	BOOL ManageSpoofing(UPnPServicesProvider *prov);
	// Manage the start of the end "service" tag
	XMLTokenHandler::Result ManageServiceEnd(UPnPServicesProvider *prov);
	// Manage the context of a tag
	XMLTokenHandler::Result ManageText(XMLToken &token, UPnPServicesProvider *prov, const uni_char *tag_name, int tag_len);

public:
	UPnPXH_DescriptionGeneric(): cur_service_ref(FALSE, FALSE) {}
	// Return the current service
	UPnPService *GetCurrentService() { return cur_service_ref.GetPointer(); }
    OP_STATUS Construct();
    const uni_char *GetActionName() { return (const uni_char *) NULL; }
    // Parsing success. Time to call the listeners...
    virtual void ParsingSuccess(XMLParser *parser);
    const char *GetDescription() { return "Description"; }
    XMLTokenHandler::Result HandleToken(XMLToken &token);
    // Set the current challenge and also compute the expected response
    OP_STATUS SetChallenge(OpString &ch) { return challenge.Set(ch.CStr()); }
};

#endif // !UPNP_HANDLERS_H_
#endif // UPNP_SUPPORT
