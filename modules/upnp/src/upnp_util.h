/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/
#ifdef UPNP_SUPPORT

#ifndef UPNP_UTIL_H_
#define UPNP_UTIL_H_

#include "modules/upnp/upnp_references.h"
// Keeps track of the last error; REMEMBER to initialize ops to OpStatus::OK!
#define LAST_ERROR(ops, call) { OP_STATUS ops_new=call; if(OpStatus::IsError(ops_new)) ops=ops_new; }


// Search type used for discovering IGD devices, like routers
#define UPNP_DISCOVERY_IGD_SEARCH_TYPE "urn:schemas-upnp-org:device:InternetGatewayDevice:1"
// Search type used for discovering every root device, like routers
#define UPNP_DISCOVERY_ROOT_SEARCH_TYPE "upnp:rootdevice"
// Search type used for discovering an Opera Unite device
#define UPNP_DISCOVERY_OPERA_SEARCH_TYPE "urn:opera-com:device:OperaUnite:1"
// Search type used for discovering an Opera Dragonfly instance
#define UPNP_DISCOVERY_DRAGONFLY_SEARCH_TYPE "urn:opera-com:device:OperaDragonfly:1"
// IGD Discovery message (always the same): port 1900, UDP address 239.255.255.250
//#define UPNP_DISCOVERY_IGD_MESSAGE "M-SEARCH * HTTP/1.1\r\nHOST:239.255.255.250:1900\r\nST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nMAN:\"ssdp:discover\"\r\nMX:3\r\nUser-Agent:Opera Unite\r\n\r\n"
// IGD Discovery message (always the same): port 1900, UDP address 239.255.255.250
//#define UPNP_DISCOVERY_UNITE_MESSAGE "M-SEARCH * HTTP/1.1\r\nHOST:239.255.255.250:1900\r\nST:urn:opera-com:device:OperaUnite:1\r\nMAN:\"ssdp:discover\"\r\nMX:3\r\nUser-Agent:Opera Unite\r\n\r\n"
// Root Device Discovery message (always the same): port 1900, UDP address 239.255.255.250
//#define UPNP_DISCOVERY_ROOT_MESSAGE "M-SEARCH * HTTP/1.1\r\nHOST:239.255.255.250:1900\r\nST:upnp:rootdevice\r\nMAN:\"ssdp:discover\"\r\nMX:3\r\nUser-Agent:Opera Unite\r\n\r\n"
// IGD Discovery Address
#define UPNP_DISCOVERY_ADDRESS UNI_L("239.255.255.250")
// IGD Discovery Port
#define UPNP_DISCOVERY_PORT 1900
// IGD Service urn of WANIPConnection
#define UPNP_WANIPCONNECTION UNI_L("urn:schemas-upnp-org:service:WANIPConnection:1")
// IGD Service urn of WANPPPConnection
#define UPNP_WANPPPCONNECTION UNI_L("urn:schemas-upnp-org:service:WANPPPConnection:1")
// Lease duration of the port mapping entries (0 means forever)
#define UPNP_PORT_MAPPING_LEASE 0/*(7*86400)*/
// Time (ms) that needs to be passed after a succesfull descrption phase before trying another one (UPnP sends a lot of NOTIFY messages at the same time, so we don't want to try subsequent ones)
#define UPNP_NOTIFY_THRESHOLD_MS 10000
// Time (ms) that needs to be passed after an M-SEARCH before answering again
#define UPNP_MSEARCH_THRESHOLD_MS 1500
// Time (ms) to wait before checking the number of the network cards (0== disabled)
#define DELAY_NETWORK_CARD_CHECK_MS 60000
// Time (ms) to wait before retry to open the port (0== disabled)
#define DELAY_REPROCESS_UPNP 0 /*1800000*/
// Time (ms) to wait before signaling that no UPnP devices are present (UPnP specifications talks about 3 seconds)  (0== disabled)
#define DELAY_UPNP_TIMEOUT 3000
// Time between periodic advertises (12 minutes, 30 minutes is the advertised period)  (0== disabled)
#ifdef _DEBUG
#define DELAY_UPNP_ADVERTISE 30000 /*1800000*/
#else
#define DELAY_UPNP_ADVERTISE 720000
#endif
// Delay between every attempt of the first advertise
#define DELAY_FIRST_ADVERTISE 3000


#ifdef _DEBUG
	// Time between checks to remove expired devices
	#define UPNP_DEVICE_EXPIRE_CHECK_MS 30000
	// Postpone the first check (UPnP ask for at least 1800 seconds, so we can wait a bit before start)
	// 0 turn off the check
	#define DELAY_UPNP_FIRST_EXPIRE_CHECK_MS	15000
#else
	// Time between checks to remove expired devices
	#define UPNP_DEVICE_EXPIRE_CHECK_MS 300000
	// Postpone the first check (UPnP ask for at least 1800 seconds, so we can wait a bit before start)
	// 0 turn off the check
	#define DELAY_UPNP_FIRST_EXPIRE_CHECK_MS	1200000
#endif

// UPnP Namespace
#define _XML_ATT_XMLNS_ UNI_L("urn:schemas-upnp-org:device-1-0"), NULL, 

// Message sent to verify the NetworkCards
#define MSG_UPNP_CHECK_NETWORK_CARDS 1
// Message sent to redo the UPnP process
#define MSG_UPNP_RECONNECT 2
// Timeout: check if there are UPnP devices
#define MSG_UPNP_TIMEOUT 3
// Periodic advertise
#define MSG_UPNP_SERVICES_ADVERTISE 4
// First advertise
#define MSG_UPNP_FIRST_SERVICES_ADVERTISE 5
// Used to delay the notification of a Services provider
#define MSG_UPNP_BROADCAST_NOTIFY 6
// Check if some devices are expired
#define MSG_UPNP_CHECK_EXPIRE 7

/// Private key common to every instance of Opera, used to "authenticate" devices.
/// This is a temporary solution to the Authentication problem
// FIXME: generate an SSL certificate for every widget and add HTTPS
#define UPNP_OPERA_PRIVATE_KEY "_OUPK_*&^%!@9#$s4'"

/** Identify the search target requested on a M-SEARCH message */
enum SearchTargetMode
{
	/** Search for upnp:rootdevice*/
	SearchRootDevice,
	/** Search for the DeviceType (class of devices) */
	SearchDeviceType,
	/** Search for the exact device */
	SearchUUID,
	/** No match for this device */
	SearchNoMatch
};

/** Identify how much we can trust the identity of an Opera Instance discovered with UPnP */
enum TrustLevel
{
	/** The Unite Instance tried to authenticate but was unable. It's probably an hack attempt, so it will be removed */
	TRUST_ERROR,
	/** The level of trust is unknown (this is a temporary state) */
	TRUST_UNKNOWN,
	/** This program did not answer to challenge: old snapshot of Unite? */
	TRUST_UNTRUSTED,
	/** This is the normale level of trust for Unite 1.0 (the private key is UPNP_OPERA_PRIVATE_KEY) */
	TRUST_PRIVATE_KEY,
	/** This level is not available now. Among other things, it requires Unite to have a different certificate for each installation */
	TRUST_CERTIFICATE
};

// Debug modes, used to simulate missing capabilities
enum UPNPDebugModes
{
	UPNP_DBG_NO_ROUTED=1,
	UPNP_DBG_NO_NAT=2,
	UPNP_DBG_NO_IP_CONN=4,
	UPNP_DBG_NO_PPP_CONN=8,
	UPNP_DBG_NO_PORT=16,
	UPNP_DBG_NO_ROUTED2=32,
	UPNP_DBG_NO_NAT2=64
};

// Error codes for UPnP
class UPnPStatus: public OpStatus
{
 public:
	enum
	{
		ERROR_UDP_SOCKET_ADDRESS_NOT_CREATED = USER_ERROR + 0,
		ERROR_UDP_SOCKET_NOT_CREATED = USER_ERROR + 1,
		ERROR_UDP_SOCKET_ADDRESS_ERROR = USER_ERROR + 2,
		ERROR_UDP_CONNECTION_FAILED = USER_ERROR + 3,
		ERROR_UDP_BIND_FAILED = USER_ERROR + 4,
		ERROR_UDP_SEND_FAILED = USER_ERROR + 5,
		ERROR_TCP_WRONGDEVICE = USER_ERROR + 100,
		ERROR_TCP_LOAD_DESCRIPTION_FAILED = USER_ERROR + 101
	};
};

// Reason to start the port opening process (mainly for debugging purposes)
enum UDPReason
{
	// Unknown reason
	UDP_REASON_UNDEFINED=0,
	// A new IGD device has been discovered
	UDP_REASON_DISCOVERY=1,
	// The location has changed
	UDP_REASON_SSDP_ALIVE_LOCATION=2,
	// A new ssdp:alive message, after that the threshold time is expired
	UDP_REASON_SSDP_ALIVE_THRESHOLD=3,
	// The device is no longer available
	UDP_REASON_SSDP_BYEBYE=4,
};

#endif
#endif
