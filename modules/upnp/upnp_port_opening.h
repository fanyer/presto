/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifdef UPNP_SUPPORT

#ifndef UPNP_PORT_OPENING_H_
#define UPNP_PORT_OPENING_H_

#include "modules/url/tools/url_util.h"
#include "modules/upnp/upnp_upnp.h"
#include "modules/upnp/src/upnp_handlers.h"

#define LOCAL_TCP_PORT_EVENTS 2869 // Local port for events (in Windows, this is reserved for UPnP by the firewall)

// Class that implements the logic for opening a port in the router
class UPnPPortOpening: public UPnPLogic
{
public:
	/** Policies available to manage situations where more than one IGD device is available */
	enum CardsPolicy
	{
		/** Open only the first network card */
		UPNP_CARDS_OPEN_FIRST,
		/** Open every network card (it can be problematic for the Web Server) */
		UPNP_CARDS_OPEN_ALL
	};
	
private:
	// TODO: write a generic "Discovery listener" and put it on UPnPLogic
    UINT16 port_start;							///< Beginning of the port range
    UINT16 port_end;							///< End of the port range
    BOOL has_ip_conn;							///< TRUE if the device is able to handle a WANIPConnection action
	BOOL has_ppp_conn;							///< TRUE if the device is able to handle a WANPPPConnection action
	BOOL try_ppp_conn;							///< TRUE tu try PPP connection too
	CardsPolicy cards_policy;					///< Policy used for port opening
	
	// Signal an error
	OP_STATUS NotifyFailure(const char *mex);

public:
	/// Constructor
	UPnPPortOpening(UPnP *upnp, UINT16 first_port, UINT16 last_port, BOOL try_ppp);
	// Destructor
	virtual ~UPnPPortOpening();
	virtual OP_STATUS ReEnable();
    OP_STATUS HandlerFailed(UPnPXH_Auto *child, const char *mex);
    
    /// Set the policy used with more than one Network Card is available
    void SetPolicy(CardsPolicy new_cards_policy) { cards_policy=new_cards_policy; }
    
	// Start the UPnP process
	OP_STATUS StartPortOpening() { return StartUPnPDiscovery(); }
    
	/// Send a TCP UPnP request for the externalIP of the router
	OP_STATUS QueryExternalIP(UPnPDevice *device);
	/// Send a TCP UPnP request to know if the device is IP Routed
	OP_STATUS QueryIPRouted(UPnPDevice *device);
	/// Send a TCP UPnP request to know if the device is Nat Enabled
	OP_STATUS QueryNATEnabled(UPnPDevice *device);
	/// Send a TCP UPnP request to query the status of the IGD port
	OP_STATUS QueryIDGPortStatus(UPnPDevice *device, UINT16 port);
	/// Send a TCP UPnP request to disable the IGD port
	OP_STATUS QueryDisablePort(UPnPDevice *device, UINT16 port);
	/// Send a TCP UPnP request to enable the IGD port
	OP_STATUS QueryEnablePort(UPnPDevice *device, UINT16 port);
	/// Subscribe to External IP change (not working)
	OP_STATUS SubscribeExternalIP(UPnPDevice *device);
	
    /// Notify the current external IP to the Opera server, but only if it is valid and changed (not working)
    OP_STATUS NotifyExternalIP(UPnPDevice *device);

    /// Get the first port in the range
    UINT16 GetFirstPort() { return port_start; }
    /// Get the last port in the range
    UINT16 GetLastPort() { return port_end; }
    /// TRUE if the device enables IP connections (these are the normal connections)
    BOOL HasIPConnection() { return has_ip_conn; }
    /// TRUE if the device enables PPP connections (so far this case could not be tested)
    BOOL HasPPPConnection() { return has_ppp_conn; }
    /// Set the first port in the range
    void SetPortStart(UINT16 n) { port_start=n; }
    /// Set the last port in the range
    void SetPortEnd(UINT16 n) { port_end=n; }
    /// Set if the enables IP connections
    void SetIPConnection(BOOL b) { has_ip_conn=b; }
    /// Set if the enables PPP connections
    void SetPPPConnection(BOOL b) { has_ppp_conn=b; }
    
    /// Signal the port that has been opened in the current device
    OP_STATUS SignalExternalPort(UPnPDevice *device, UINT16 port);
    
    /// TRUE if there is a PPP connection and it is required to try it
	BOOL TryPPPConnection() { return has_ppp_conn && try_ppp_conn; }
	
	/// Ask the listeners to do a RollBack
	OP_STATUS RollbackListeners(UPnPDevice *device, UINT16 port, int cur=-1);
	/// Notify the listeners that a port has been closed
	OP_STATUS NotifyListenersPortClosed(UPnPDevice *device, UINT16 port, BOOL success);
	
	virtual void OnNewDevice(UPnPDevice *device);
	virtual void OnRemoveDevice(UPnPDevice *device) { }
};

////////////////////// Port Opening Handlers

// Parsing of the SOAP answer for the description phase. Logic handlers can inherit from this class to save some lines of code
/*class UPnPXH_DescriptionPortOpening: public UPnPXH_DescriptionGeneric
{
public:
    virtual void ParsingSuccess(XMLParser *parser);
    virtual OP_STATUS NotifyService(UPnPService *service);
};*/


// Parsing of the SOAP answer for the External IP Address
class UPnPXH_ExternalIP: public UPnPXH_Auto
{
public:
    OP_STATUS Construct();
    const uni_char *GetActionName() { return UNI_L("GetExternalIPAddress"); }
    OP_STATUS HandleTokenAuto();
    const char *GetDescription() { return "External IP"; }
};

// Parsing of the SOAP answer for the NewConnectionType value, to know if the device is IP_Routed
class UPnPXH_IPRouted: public UPnPXH_Auto
{
public:
    OP_STATUS Construct();
    const uni_char *GetActionName() { return UNI_L("GetConnectionTypeInfo"); }
    OP_STATUS HandleTokenAuto();
    const char *GetDescription() { return "IP Routed check"; }
};
 
// Parsing of the SOAP answer the NAT Enabled value
class UPnPXH_NATEnabled: public UPnPXH_Auto
{
public:
    OP_STATUS Construct();
    const uni_char *GetActionName() { return UNI_L("GetNATRSIPStatus"); }
    OP_STATUS HandleTokenAuto();
    const char *GetDescription() { return "NAT Enabled check"; }
};

// Parsing of the SOAP answer to find the Static NAT status of the required port
class UPnPXH_PortStatus: public UPnPXH_Auto
{
protected:
	UINT16 external_port;	// Port Number
	BOOL use_tcp;
	int next_action;
	
public:
	enum Actions { ENABLE, DISABLE, STATUS, STOP };
	UPnPXH_PortStatus(UINT16 port, BOOL tcp, int action) { external_port=port; use_tcp=tcp; next_action=action; notify_failure=TRUE; }
    
    OP_STATUS Construct();
    const uni_char *GetActionName() { return UNI_L("GetSpecificPortMappingEntry"); }
    OP_STATUS HandleTokenAuto();
    void ParsingError(XMLParser *parser, UINT32 http_status, const char *err_mex)
    {
		OP_NEW_DBG("UPnPXH_PortStatus::ParsingError", "upnp_trace");	OP_DBG((UNI_L("*** Error on status for port: %d\n"), external_port));
		NotifyFailure(err_mex);
	}
    
    // Add the parameters required for the SOAP call
	OP_STATUS AddParameters(OpSocketAddress *address);
	const char *GetDescription() { return "Port status"; }
};

// Parsing of the SOAP answer to Delete a Port mapping
class UPnPXH_DisablePort: public UPnPXH_PortStatus
{
public:
	// TODO: security problem? Notify the user?
	UPnPXH_DisablePort(UINT16 port, BOOL tcp, int action):UPnPXH_PortStatus(port, tcp, action) { notify_failure=FALSE; trust_http_on_fault=TRUE; }
    
    const uni_char *GetActionName() { return UNI_L("DeletePortMapping"); }
    OP_STATUS Construct() { return tags_parser.Construct(NULL, 1, 0, -1, "dummy", NULL); }
    OP_STATUS Finished();
    const char *GetDescription() { return "Port disable"; }
    virtual OP_STATUS NotifyFailure(const char *mex);
};

// Parsing of the SOAP answer to Delete a Port mapping
class UPnPXH_EnablePort: public UPnPXH_PortStatus
{
protected:
	// Function called to signal the opening (or not) of the port; if the port was not opened, the return value is BOOL to try the next port/device/protocol(IP/PPP), FALSE if you don't want to try opening other ports
	//BOOL (* port_openening_result)(int port, BOOL opened, UPnP *upnp, UPnPDevice *device, BOOL finished); 
public:
	UPnPXH_EnablePort(UINT16 port, BOOL tcp, int action/*, int (* ptr_result)(int port, BOOL opened, UPnP *upnp, UPnPDevice *device, BOOL finished)*/):UPnPXH_PortStatus(port, tcp, action) {  /*port_openening_result=ptr_result;*/ notify_failure=FALSE; notify_missing_variables=FALSE; trust_http_on_fault=TRUE; }
    
    const uni_char *GetActionName() { return UNI_L("AddPortMapping"); }
    // Add the parameters required for the SOAP call
	OP_STATUS AddParameters(OpSocketAddress *address);
	void ParsingSuccess(XMLParser *parser);
	void ParsingError(XMLParser *parser, UINT32 http_status, const char *err_mex);
	const char *GetDescription() { return "Port enable"; }
};

#endif
#endif

