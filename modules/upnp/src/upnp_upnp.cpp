/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2011 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UPNP_SUPPORT

#include "modules/upnp/upnp_upnp.h"
#include "modules/upnp/upnp_port_opening.h"
#include "modules/pi/network/OpSocket.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/pi/network/OpSocketAddress.h"
#include "modules/pi/network/OpHostResolver.h"
#include "modules/url/url_module.h"
#include "modules/url/url2.h"
#include "modules/url/tools/url_util.h"
#include "modules/url/url_rep.h"
#include "modules/url/protocols/comm.h"
#include "modules/util/opfile/opfile.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/util/tempbuf.h"
#include "modules/util/adt/bytebuffer.h"
#ifdef SELFTEST
	#include "modules/selftest/src/testutils.h"
#endif
#include "modules/pi/OpSystemInfo.h"
#include "modules/xmlutils/xmlfragment.h"

#include "modules/libcrypto/include/CryptoHash.h"


#ifdef PI_NETWORK_INTERFACE_MANAGER
	#include "modules/pi/network/OpNetworkInterface.h"
#endif

//#define LOCAL_UDP_PORT 1900 // reserved UDP local port, open even by the Windows firewall
#define LOCAL_UDP_PORT 0 // reserved UDP local port, open even by the Windows firewall


// Enable a "mock" network interface that simulates an OpNetworkInterfaceManager
//#define UPNP_PI_NETWORK_INT_MOCK

#if defined(UPNP_PI_NETWORK_INT_MOCK) && defined(PI_NETWORK_INTERFACE_MANAGER)
	// Put your IP here
	#define MOCK_IP UNI_L("192.168.1.101")

	// Mock class that simulate the interface of this PC
	class OpNUPnPMockInterface: public OpNetworkInterface
	{
	public:
		virtual OP_STATUS GetAddress(OpSocketAddress* address) { return address->FromString(MOCK_IP); };
		virtual OpNetworkInterfaceStatus GetStatus() { return NETWORK_LINK_UP; }
	};
	
	// Mock class that simulate a PC with two Network Card with the same address (used to check the duplciation code)
	class OpUPnPMockInterfaceManager: public OpNetworkInterfaceManager
	{
	private:
		UINT pos;
		OpNUPnPMockInterface nic;
	
	public:
		OpUPnPMockInterfaceManager() { pos=1000; }
		virtual void SetListener(OpNetworkInterfaceListener* listener) { }
		virtual OP_STATUS BeginEnumeration() { pos=0; return OpStatus::OK; }
		virtual void EndEnumeration() { }

		virtual OpNetworkInterface* GetNextInterface()
		{
			OP_ASSERT(pos<=2);
			
			pos++;
			
			return (pos<=2)?&nic:NULL;
		}
	};
		
	OP_STATUS OpNetworkInterfaceManager::Create(OpNetworkInterfaceManager** new_object, class OpNetworkInterfaceListener* listener)
	{
		*new_object=OP_NEW(OpUPnPMockInterfaceManager, ());
		
		if(!new_object)
			return OpStatus::ERR_NO_MEMORY;
	}

#endif

OP_STATUS UPnP::BroadcastUDPDiscoveryForSingleLogic(UPnPLogic *logic)
{
	OP_ASSERT(logic);
	
	if(!logic)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_STATUS st=OpStatus::OK;
	
	// M-SEARCH in every network card
	for(UINT32 i=0; i<network_cards.GetCount(); i++)
	{
		OP_ASSERT(network_cards.Get(i));
		
		if(network_cards.Get(i))
		{
			OpUdpSocket *sk_udp_discovery=network_cards.Get(i)->GetSocketUnicast();
			OpSocketAddress *addr=network_cards.Get(i)->GetAddress();
	
			OP_ASSERT(addr && sk_udp_discovery && saUDPDiscovery && saUDPDiscovery->IsValid());
	
			if(!addr || !saUDPDiscovery || !saUDPDiscovery->IsValid())
				return UPnPStatus::ERROR_UDP_SOCKET_ADDRESS_ERROR;

			// Advertise 3 times, as UDP standard...
			// FIXME: it should be better to wait a bit between every message repeat... like 500 ms
			for(int j=0; j<3; j++)
			{
				OpString8 dscv_mex;
				
				if( OpStatus::IsError(logic->CreateDiscoveryessage(dscv_mex)) || 
					OpStatus::IsError(sk_udp_discovery->Send(dscv_mex.CStr(), dscv_mex.Length(), saUDPDiscovery, OpUdpSocket::MULTICAST)))
					st=UPnPStatus::ERROR_UDP_SEND_FAILED;
			}
		}
	}
	
	return st;
}

OP_STATUS UPnP::BroadcastUDPDiscovery(NetworkCard *nic)
{
	OP_ASSERT(nic);

	if(!nic)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS st=OpStatus::OK;
	OpUdpSocket *sk_udp_discovery=nic->GetSocketUnicast();
	OpSocketAddress *addr=nic->GetAddress();
	
	OP_ASSERT(addr && sk_udp_discovery && saUDPDiscovery && saUDPDiscovery->IsValid());
	
    if(!addr || !saUDPDiscovery || !saUDPDiscovery->IsValid())
			st=UPnPStatus::ERROR_UDP_SOCKET_ADDRESS_ERROR;
    else
	{
		//if(LOCAL_UDP_PORT)
		//	addr->SetPort(LOCAL_UDP_PORT);
		
		//RETURN_IF_ERROR(sk_udp_discovery->SelectNetworkCard(addr));
		//RETURN_IF_ERROR(sk_udp_discovery->Bind(addr, BIND_BROADCAST));
		
		// FIXME: Attention to listeners that listen to the same messages and send duplicate messages
		// Looks for interesting devices
		// Send multiple broadcasts because UDP is unreliable (suggested by UPnP specifications too)
		for(int j=0; j<3; j++)
		{
			for(UINT32 i=0; i<dev_lsn.logics.GetCount(); i++)
			{
				OpString8 dscv_mex;
				UPnPLogic *cp=dev_lsn.logics.GetPointer(i);
				
				if(cp)
				{
					if( OpStatus::IsError(cp->CreateDiscoveryessage(dscv_mex)) || 
						OpStatus::IsError(sk_udp_discovery->Send(dscv_mex.CStr(), dscv_mex.Length(), saUDPDiscovery, OpUdpSocket::MULTICAST)))
						st=UPnPStatus::ERROR_UDP_SEND_FAILED;
				}
			}
			
			// FIXME: it should be better to wait a bit between every message repeat... like 500 ms
		}
		
#ifdef UPNP_LOG
		OpString local_ip, remote_ip;

		addr->ToString(&local_ip);
		saUDPDiscovery->ToString(&remote_ip);
		
		Log(UNI_L("UPnP::BroadcastUDPDiscovery()"), UNI_L("Local IP: %s - Remote IP: %s"), local_ip.CStr(), remote_ip.CStr());
#endif
	}
	
    //OP_DELETE(saUDPDiscovery);
	
	return st;
}

OP_STATUS UPnP::SubscribeUDPDiscovery(NetworkCard *nic)
{
	OP_STATUS st=OpStatus::OK;
	OpUdpSocket *sk_udp_multicast=nic->GetSocketMulticast();
	OpUdpSocket *sk_udp_unicast=nic->GetSocketUnicast();
	OpSocketAddress *addr=nic->GetAddress();
	
	OP_ASSERT(addr && sk_udp_multicast && sk_udp_unicast && saUDPDiscovery && saUDPDiscovery->IsValid());
	
    if(!addr || !saUDPDiscovery || !saUDPDiscovery->IsValid())
			st=UPnPStatus::ERROR_UDP_SOCKET_ADDRESS_ERROR;
    else
	{
		//st=sk_udp_subscriber->SetLocalUDPPort(LOCAL_UDP_PORT);
		// PI_UPDATED
		//if(LOCAL_UDP_PORT)
			addr->SetPort(0);
		//st=sk_udp_subscriber->Bind(addr, BIND_UNICAST);
		//sk_udp_subscriber->SetLocalUDPPortRange(LOCAL_UDP_PORT, LOCAL_UDP_PORT+10);
		
		//if(OpStatus::IsSuccess(st))
		{
			//saUDPDiscovery->SetPort(UPNP_DISCOVERY_PORT);
		
			//if(OpStatus::IsError(sk_udp_subscriber->Connect(saUDPDiscovery)))
			//	st=UPnPStatus::ERROR_UDP_CONNECTION_FAILED;
			
			//RETURN_IF_ERROR(sk_udp_subscriber->SelectNetworkCard(addr));
			
			st=sk_udp_multicast->Bind(saUDPDiscovery, OpUdpSocket::MULTICAST);
			
			if(OpStatus::IsSuccess(st) && sk_udp_multicast!=sk_udp_unicast)
				st=sk_udp_unicast->Bind(addr, OpUdpSocket::UNICAST);

			port_bound=TRUE;
			//st=sk_udp_subscriber->SubscribeMulticast(saUDPDiscovery);
		}
	}
	
	
    //OP_DELETE(saUDPDiscovery);
	
	return st;
}

OP_STATUS UPnP::ComputeURLString(UPnPDevice *device, OpString &deviceString, const OpString *url)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(url && url->Length()>0 && device->device_port>0);
	
	if(device->device_ip.Length()<=0 || device->device_url.Length()<=0 || 0==device->device_port || NULL==url || url->Length()<=0)
	  return OpStatus::ERR_OUT_OF_RANGE;
	  
	if(HTTPMessageWriter::URLStartWithHTTP(url->CStr()))
	{
		return deviceString.Set(url->CStr());
	}
	  
	char bufPort[16];	/* ARRAY OK 2008-08-12 lucav */
	
	// Compute the full URL
	deviceString.Empty();
	RETURN_IF_ERROR(deviceString.Append("http://"));
	RETURN_IF_ERROR(deviceString.Append(device->device_ip));
	RETURN_IF_ERROR(deviceString.Append(":"));
	RETURN_IF_ERROR(deviceString.Append(op_itoa(device->device_port, bufPort, 10)));
	if(url->CStr()[0]!='/')
	  RETURN_IF_ERROR(deviceString.Insert(0, "/"));
	RETURN_IF_ERROR(deviceString.Append(*url));
	
	return OpStatus::OK;
}

OP_STATUS UPnP::GetSoapMessage(UPnPDevice *device, OpString *out_http_message, const uni_char *soap_service_type, const OpString *soap_control_url, const OpString *soap_mex, const uni_char* soap_action, BOOL mpost)
{
	OP_ASSERT(out_http_message && soap_mex && soap_action && soap_service_type && soap_control_url && device);
	
	if(!device || !out_http_message || !soap_mex || !soap_action || !soap_service_type || !soap_control_url)
		return OpStatus::ERR_NO_MEMORY;
	
	HTTPMessageWriter mex;
	
	RETURN_IF_ERROR(mex.setURL(device->device_ip.CStr(), soap_control_url->CStr(), device->device_port));
	RETURN_IF_ERROR(mex.setContentType(UNI_L("text/xml"), UNI_L("utf-8")));
	
	if(mpost)
	{
		RETURN_IF_ERROR(mex.setSoapAction(soap_service_type, soap_action, UNI_L("01")));
		RETURN_IF_ERROR(mex.setMandatory(UNI_L(SOAP_ENVELOPE_SCHEMA), UNI_L("01")));
		RETURN_IF_ERROR(mex.GetMessage(out_http_message, "M-POST", soap_mex));
	}
	else
	{
		RETURN_IF_ERROR(mex.setSoapAction(soap_service_type, soap_action, NULL));
		RETURN_IF_ERROR(mex.GetMessage(out_http_message, "POST", soap_mex));
	}
	
	return OpStatus::OK;
}
        
OP_STATUS UPnP::StartActionCall(UPnPDevice *device, UPnPXH_Auto *&uxh, UPnPLogic *upnp_logic)
{
	OP_ASSERT(uxh && device);
	
	if(!uxh)
		return OpStatus::ERR_NO_MEMORY;
		
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	const uni_char *soap_action=uxh->GetActionName();
	OP_ASSERT(soap_action && device->GetService() && device->GetServiceURL());
	
	if(!device->GetService() || !device->GetServiceURL() || !soap_action)
	{
		OP_DELETE(uxh);
		uxh=NULL;
		
		return OpStatus::ERR_NULL_POINTER;
	}
	  
	OpString http_mex;
	OpString soap_xml;
	OpString8 http_mex8;
	OpString url_string;
	OP_STATUS ops;
	
	//RETURN_IF_ERROR(soap_mex.GetActionCallXML(soap_xml, soap_action, service.CStr(), NULL, 0));
	ops=SOAPMessage::CostructMethod(soap_xml, soap_action, device->GetService(), NULL, uxh->getParameters());
	if(OpStatus::IsSuccess(ops))
	{
		ops=GetSoapMessage(device, &http_mex, device->GetService(), device->GetServiceURL(), &soap_xml, soap_action, FALSE);

		if(OpStatus::IsSuccess(ops))
		{
			OP_NEW_DBG("UPnP::StartActionCall", "upnp_trace_debug");
			OP_DBG((UNI_L("*** SOAP Mex for %s: %s\n"), soap_action, soap_xml.CStr()));
			//OP_DBG((UNI_L("*** HTTP Mex for %s: %s\n"), soap_action, http_mex.CStr()));
			
			//LogOutPacket(UNI_L("HTTP Mex"), soap_action, http_mex8.CStr());
			//Log(UNI_L("OutgoingPacket"), UNI_L("HTTP Mex for %s: %s"), UNI_L("Test 1"), UNI_L("Test 2"));
			
			ops=http_mex8.Set(http_mex.CStr());
			if(OpStatus::IsSuccess(ops))
			  ops=ComputeURLString(device, url_string, device->GetServiceURL());
		}
	}
	
	if(OpStatus::IsError(ops))
	{
		OP_DELETE(uxh);
		uxh=NULL;
		
		return ops;
	}
	
 	return StartXMLProcessing(device, &url_string, uxh, http_mex8.CStr(), TRUE, upnp_logic);
}


OP_STATUS UPnP::StartXMLProcessing(UPnPDevice *device, OpString *url_string, UPnPXH_Auto *&uxh, const char *postMessage, BOOL with_headers, UPnPLogic *upnp_logic)
{
	if(!uxh)
	  return OpStatus::ERR_NULL_POINTER;
	  
	uxh->SetUPnP(this, device, upnp_logic);
	  
	RETURN_IF_ERROR(uxh->Construct());
		
	return StartXMLProcessing(url_string, uxh, uxh->GetMessageHandler(), postMessage, with_headers);
};

OP_STATUS UPnP::StartXMLProcessing(OpString *url_string, UPnPXH_Auto *&xth, MessageHandler *mh, const char *postMessage, BOOL with_headers)
{
	OP_ASSERT(xth!=NULL && url_string && !url_string->IsEmpty());
	
	if(!url_string || url_string->IsEmpty() || !xth)
	{
		OP_DELETE(xth);
		xth=NULL;
		
		if(!url_string)
			return OpStatus::ERR_NO_MEMORY;
		
		return OpStatus::ERR_OUT_OF_RANGE;
	}
	
#ifdef UPNP_LOG
	LogOutPacket(UNI_L("SOAP_Call"), url_string->CStr(), postMessage);
#endif

    XMLParser *parser;
    URL url=g_url_api->GetURL(url_string->CStr(), NULL, TRUE);
    OP_STATUS ops=OpStatus::OK;
    
    // Avoid sending authentication and cookies
	url.SetAttribute(URL::KBlockUserInteraction, TRUE);
	url.SetAttribute(URL::KDisableProcessCookies, TRUE);
    
    if(postMessage)
	{
		url.SetHTTP_Method(HTTP_METHOD_POST);
		
		ops=url.SetHTTP_Data(postMessage, TRUE, with_headers);
	}
	
	xth->EmptyPartianXML();
	  
	if(OpStatus::IsSuccess(ops) && url.IsEmpty())
		ops=UPnPStatus::ERROR_TCP_WRONGDEVICE;
	else if(OpStatus::IsSuccess(ops) && OpStatus::IsSuccess(ops=XMLParser::Make(parser, this, mh, xth, url)))
	{
		XMLParser::Configuration configuration;
		
		ops=dev_lsn.AddXMLParser(xth, parser); // The Control Point will delete the parser on exit
		
		if(OpStatus::IsError(ops)) // OOM or even too many parsers in flight
		{
			OP_DELETE(parser);
			
			return ops;
		}
		
		xth->upnp=this;	// The handler need to clear the parser
		xth->SetParser(parser);
		
		configuration.parse_mode = XMLParser::PARSEMODE_FRAGMENT;
		configuration.load_external_entities = XMLParser::LOADEXTERNALENTITIES_YES;
		#ifdef XML_ERRORS
			configuration.generate_error_report=TRUE;
		#endif
		parser->SetConfiguration(configuration);
		parser->SetOwnsTokenHandler();
	    
		#ifdef _DEBUG
			//unsigned int consumed=0;
			//const uni_char *data=UNI_L("<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body><s:Fault><faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail><UPnPError xmlns=\"urn:schemas-upnp-org:control-1-0\"><errorCode>402</errorCode><errorDescription>Invalid Args</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>");
			
			//parser->Parse(data, uni_strlen(data), FALSE, &consumed);
		#endif
		OP_NEW_DBG("UPnP::StartXMLProcessing", "upnp_trace_debug");
		OP_DBG((UNI_L("*** The XML Parser is supposed to load %s\n"), url_string->CStr()));

		// Load the URL specified in the Make() and at the end delete the parser
		URL ref;
		ops=parser->Load(ref, TRUE, 5000); // 5 seconds seems a sensible timeout; this is also done to avoid to have 
		                                   // too many parsers on flight if a device is unresponsive

		if(OpStatus::IsError(ops))
			OP_DELETE(parser); // xth is owned
	}
	else
	{
		OP_DELETE(xth);
		xth=NULL;
	}
    
    return ops;
}

/// Default constructor
UPnPDevice::UPnPDevice(NetworkCard *card): ref_obj_known(this), ref_obj_auto(this)
{
	OP_ASSERT(card && card->GetSocketUnicast() && card->GetSocketMulticast() && card->GetAddress());
	
	network_card=card;
	ppp=FALSE;
	device_port=0;
	external_port=0;
	//socket_udp=NULL;
	current_description_time=0;
	reason=UDP_REASON_UNDEFINED;
	provider=NULL;
	trust=TRUST_UNKNOWN;
	to_be_deleted=FALSE;
	known_address=NULL;
	expire_period=1800;  // Default expire time
}

UPnPDevice::~UPnPDevice()
{
	OP_DELETE(provider);
	OP_DELETE(known_address);
}

OP_STATUS UPnPDevice::Construct(const char* devstring)
{
	provider=OP_NEW(DiscoveredUPnPServicesProvider, ());
	
	if(!provider)
		return OpStatus::ERR_NO_MEMORY;
		
	return static_cast<DiscoveredUPnPServicesProvider*>(provider)->Construct(devstring);
}

OpStringC *UPnPDevice::GetDeviceAttribute(int att)
{
	NameValue *nv=provider->GetAttribute(att);
	
	if(!nv)
		return NULL;
	
	return &(nv->GetValue());
}

OP_STATUS UPnPDevice::SetDeviceAttribute(int att, OpString *str)
{
	if(!str)
		return OpStatus::ERR_NULL_POINTER;
		
	NameValue *nv=provider->GetAttribute(att);
	
	if(!nv)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	return nv->SetValue(str->CStr());
}

OpStringC *UPnPDevice::GetManufacturer()
{
	return GetDeviceAttribute(UPnPServicesProvider::MANUFACTURER);
}
OpStringC *UPnPDevice::GetModelDescription()
{
	return GetDeviceAttribute(UPnPServicesProvider::MODEL_DESCRIPTION);
}

OpStringC *UPnPDevice::GetModelName()
{
	return GetDeviceAttribute(UPnPServicesProvider::MODEL_NAME);
}

OpStringC *UPnPDevice::GetModelNumber()
{
	return GetDeviceAttribute(UPnPServicesProvider::MODEL_NUMBER);
}

OpStringC *UPnPDevice::GetSerialNumber()
{
	return GetDeviceAttribute(UPnPServicesProvider::SERIAL_NUMBER);
}

OP_STATUS UPnPDevice::SetDeviceBrandAndModel(OpString *brand, OpString *device_name, OpString *device_description, OpString *device_number, OpString *device_serial)
{
	if(brand && !brand->IsEmpty())
		SetDeviceAttribute(UPnPServicesProvider::MANUFACTURER, brand);
	if(device_name && !device_name->IsEmpty())
		SetDeviceAttribute(UPnPServicesProvider::MODEL_NAME, device_name);
	if(device_description && !device_description->IsEmpty())
		SetDeviceAttribute(UPnPServicesProvider::MODEL_DESCRIPTION, device_description);
	if(device_number && !device_number->IsEmpty())
		SetDeviceAttribute(UPnPServicesProvider::MODEL_NUMBER, device_number);
	if(device_serial && !device_serial->IsEmpty())
		SetDeviceAttribute(UPnPServicesProvider::SERIAL_NUMBER, device_serial);
	
	OP_NEW_DBG("UPnPDevice::SetDeviceBrandAndModel", "upnp_trace");
	OP_DBG((UNI_L("*** Device informations - Manufacturer: %s - Description: %s - Name: %s, Number: %s, Serial: %s\n"), brand->CStr(), device_description->CStr(), device_name->CStr(), device_number->CStr(), device_serial->CStr()));
	
	return OpStatus::OK;
}

UPnPDevice *UPnP::GetDeviceFromUSN(const char *usn, const char *location)
{
	OP_ASSERT(usn);
	
	if(!usn)
		return NULL;
	
	// Check if the same device is already present
	for(UINT32 i=0; i<known_devices.GetCount(); i++)
	{
		UPnPDevice *device=known_devices.GetPointer(i);
		OP_ASSERT(device && device->GetNetworkCard());

		// Same network card
		if(device && device->IsSameDevice(usn, location))
			return device;
	}

	return NULL;
}

UPnPDevice *UPnP::GetDeviceFromUSN(const uni_char *usn, const uni_char *location)
{
	OP_ASSERT(usn);
	
	if(!usn)
		return NULL;
	
	// Check if the same device is already present
	for(UINT32 i=0; i<known_devices.GetCount(); i++)
	{
		UPnPDevice *device=known_devices.GetPointer(i);
		OP_ASSERT(device && device->GetNetworkCard());

		// Same network card
		if(device && device->IsSameDevice(usn, location))
			return device;
	}
	
	return NULL;
}

OP_STATUS UPnP::RemoveDevice(const char *usn, const char *location)
{
	UPnPDevice *prev_dev=GetDeviceFromUSN(usn, location);

	if(prev_dev)
		return known_devices.RemoveReference(prev_dev->GetReferenceObjectPtrKnownDevices());

	return OpStatus::OK;
}

OP_STATUS UPnP::RemoveDevice(UPnPDevice *device)
{
	if(device)
		return known_devices.RemoveReference(device->GetReferenceObjectPtrKnownDevices());

	return OpStatus::OK;
}

OP_STATUS UPnP::AddNewDevice(NetworkCard *network_card, const uni_char *controlURL, const uni_char *usn, UINT32 expire_period)
{
	UPnPDevice *prev_dev=GetDeviceFromUSN(usn, controlURL);

	if(prev_dev)
	{
	#ifdef UPNP_LOG
		Log(UNI_L("UPnP::AddNewDevice()"), UNI_L("Device already present: - %s in %s"), usn, controlURL);
	#endif
		prev_dev->SetExpirePeriod(expire_period);
		
		return prev_dev->device_url.Set(controlURL);
	}
	
#ifdef UPNP_LOG
	Log(UNI_L("UPnP::AddNewDevice()"), UNI_L("New Device detected: - %s in %s"), usn, controlURL);
#endif

	//OpSocketAddress *sa_device=(network_card)?network_card->GetAddress():NULL;
	//if(cards_policy==UPNP_CARDS_OPEN_FIRST && known_devices.GetCount()>0)
	//	return OpStatus::ERR_NOT_SUPPORTED;

	UPnPDevice *dev=OP_NEW(UPnPDevice, (network_card));

	if(!dev)
		return OpStatus::ERR;
	OpString8 devstr;
	RETURN_IF_ERROR(devstr.Set(usn));
	OP_STATUS ops=dev->Construct(devstr.CStr());
	
	if(OpStatus::IsError(ops) || OpStatus::IsError(ops=dev->SetUSN(usn)))
	{
		OP_DELETE(dev);
		
		return ops;
	}
	
	OP_ASSERT(dev->GetProvider());
	
	// Set IP and port
	URL url=g_url_api->GetURL(controlURL);
	ServerName *sn=(ServerName *)url.GetAttribute(URL::KServerName, NULL);
	
	dev->device_port = url.GetServerPort() ? url.GetServerPort() : 80;
	
	OP_ASSERT(sn);
	
	if(!sn)
	{
		OP_DELETE(dev);
		return OpStatus::ERR_NULL_POINTER;
	}
		
	RETURN_IF_ERROR(dev->device_ip.Set(sn->UniName()));
	dev->SetExpirePeriod(expire_period);

	if(OpStatus::IsSuccess(dev->device_ip.Set(sn->UniName())) && OpStatus::IsSuccess(dev->usn.Set(usn)) && OpStatus::IsSuccess(dev->device_url.Set(controlURL)))
	{
		if(OpStatus::IsSuccess(known_devices.AddReference(dev->GetReferenceObjectPtrKnownDevices())))
		{
#ifdef UPNP_LOG
			Log(UNI_L("UPnP::AddNewDevice()"), UNI_L("Device %s in %s added at position %d"), usn, controlURL, known_devices.GetCount()-1);
#endif			

			return OpStatus::OK;
		}
	}

	OP_DELETE(dev);		
	return OpStatus::ERR_NO_MEMORY;
}

UPnP::UPnP(BOOL try_all_devices):known_devices(FALSE, TRUE), scheduler(this), dev_lsn(this)/*, mh(NULL), mh_event(NULL), addresses_listener(this)*/
{
	debug_mode=0;
	self_delete=FALSE;
	stop_search=!(try_all_devices);

	test_completed=FALSE;
	//Log(UNI_L("UPnP::UPnP()"), UNI_L("UPnP object construction"));
	saUDPDiscovery=NULL;
	discovery_started=FALSE;
	port_bound=FALSE;

	#if defined(UPNP_SERVICE_DISCOVERY) && defined(ADD_PASSIVE_CONTROL_POINT)
		cp_unite=NULL;
	#endif
}

OP_STATUS UPnP::StartUPnPDiscovery()
{
	if(dev_lsn.logics.GetCount()==0) // With no logic to listen, it's useless to start the discovery process
		return OpStatus::OK;

	if(!discovery_started) // Message loopes (started only once)
	{
		// Put UDP broadcasting address on saUDPDiscovery
		if(!saUDPDiscovery)
		{
			RETURN_IF_ERROR(OpSocketAddress::Create(&saUDPDiscovery));
			if(!saUDPDiscovery)
				return OpStatus::ERR_NO_MEMORY;

			if(OpStatus::IsError(saUDPDiscovery->FromString(UPNP_DISCOVERY_ADDRESS)) || !saUDPDiscovery->IsValid())
			{
				OP_DELETE(saUDPDiscovery);
				
				saUDPDiscovery=NULL;
				return OpStatus::ERR;
			}

			discovery_started=TRUE;
			
			saUDPDiscovery->SetPort(UPNP_DISCOVERY_PORT);
			
		#ifdef UPNP_SERVICE_DISCOVERY
			if(DELAY_UPNP_ADVERTISE>0)
				scheduler.GetMessageHandler()->PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_SERVICES_ADVERTISE, 0, DELAY_UPNP_ADVERTISE);
		#endif
		}
		else
			discovery_started=TRUE;

		if(DELAY_NETWORK_CARD_CHECK_MS>0)
			scheduler.GetMessageHandler()->PostMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_CHECK_NETWORK_CARDS, 0);
		
		if(DELAY_REPROCESS_UPNP>0)
			scheduler.GetMessageHandler()->PostMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_RECONNECT, 0);
			
		if(DELAY_UPNP_TIMEOUT>0)
			scheduler.GetMessageHandler()->PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_TIMEOUT, 0, DELAY_UPNP_TIMEOUT);
			
		if(DELAY_UPNP_FIRST_EXPIRE_CHECK_MS>0)
			scheduler.GetMessageHandler()->PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_CHECK_EXPIRE, 0, DELAY_UPNP_FIRST_EXPIRE_CHECK_MS);
	}
	else
	{
		if(DELAY_UPNP_TIMEOUT>0)
		{
			scheduler.GetMessageHandler()->RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_TIMEOUT, 0);
			scheduler.GetMessageHandler()->PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_TIMEOUT, 0, DELAY_UPNP_TIMEOUT);
		}	
		StartUPNPDiscoveryProcess(NULL); // Force the discovery process with the current cards
	}

	// "Auto expiring" loop
	if(DELAY_FIRST_ADVERTISE>0)
		scheduler.GetMessageHandler()->PostMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_FIRST_SERVICES_ADVERTISE, 0);
		
	return OpStatus::OK;
}

#ifdef UPNP_LOG
void UPnP::LogInPacket(const uni_char *description, const char *httpPath, const uni_char *action, const char *message)
{
	// No error management is done in this function, to try to log everything that is possible
	
	if(!IsLogging())
		return;
		
	OpString str;
	
	str.Set(description);
	if(action)
	{
		str.Append(" | ");
		if(httpPath && httpPath[0])
		  str.Append(httpPath);
		str.Append(" ^ ");
		str.Append(action);
	}
	str.Append("\n#####\n");
	str.Append("%s");
	str.Append("\n#####\n");
	
	OpString temp;
	
	temp.Set(message);
	Log(UNI_L("IncomingPacket"), str.CStr(), temp.CStr());
}

void UPnP::LogOutPacket(const uni_char *description, const uni_char *action, const char *message)
{
	// No error management is done in this function, to try to log everything that is possible
	
	if(!IsLogging())
		return;
		
	OpString str;
	
	str.Set(description);
	if(action)
	{
		str.Append(" ^ ");
		str.Append(action);
	}
	str.Append("\n#####\n");
	str.Append("%s");
	str.Append("\n#####\n");
	
	OpString temp;
	
	temp.Set(message);
	Log(UNI_L("OutgoingPacket"), str.CStr(), temp.CStr());
}


void UPnP::Log(const uni_char *position, const uni_char *msg, ...)
{
	// No error management is done in this function, to try to log everything that is possible
	
	if(!IsLogging())
		return;
		
	va_list arglist;
	va_start(arglist, msg);

	OpString str;
	
	str.Set(position);
	str.Append(" @ ");
	str.Append(msg);
	
	LogMessage(str.CStr(), arglist);

	va_end(arglist);
}
#endif

BOOL UPnP::IsIPSuitable(OpString *card_ip)
{
	OP_ASSERT(card_ip);
	
	// No IPv6
	if(card_ip->FindFirstOf(':')!=KNotFound)
		return FALSE;
	
	if(!card_ip->Compare("127.", 4) || // IPv4 reserves A LOT of addresses for loopback...
	  //!card_ip->Compare("::1") || // Loopbakc IPv6, short notation
	  //!card_ip->Compare("0:0:0:0:0:0:0:1") || // Loopbakc IPv6 long notation
	  !card_ip->Compare("0.", 2))  // Not sure about what these addresses are, but they are quite harmful...
		return FALSE;
		
	return TRUE;
}

NetworkCard *UPnP::AddNewNetworkCard(OpString *card_ip)
{
	OP_ASSERT(card_ip);
	
	if(!card_ip)
		return NULL;
		
	// Look for an already existing socket
	for(UINT32 i=0; i<network_cards.GetCount(); i++)
	{
		if(network_cards.Get(i)->IsAddressEqual(card_ip))
			return network_cards.Get(i);
	}
	
	// Look for "strange addresses"
	if(!IsIPSuitable(card_ip))
		return NULL;
	
	// Add socket and address to the array
	OpUdpSocket *socket_uni=NULL;  // Unicast socket
	OpUdpSocket *socket_multi=NULL;// Multicast socket
	OpSocketAddress *addr=NULL;
	
	RETURN_VALUE_IF_ERROR(OpSocketAddress::Create(&addr), NULL);
	
	if(OpStatus::IsSuccess(addr->FromString(card_ip->CStr())))
	{
		// PI_UPDATED
		//if(OpStatus::IsSuccess(addr->FromString(card_ip->CStr())) && OpStatus::IsSuccess(socket->ChooseNetworkCard(addr)))
		//if(LOCAL_UDP_PORT)
			//addr->SetPort(LOCAL_UDP_PORT);
		addr->SetPort(UPNP_DISCOVERY_PORT);
		
		//if(OpStatus::IsSuccess(socket->SelectNetworkCard(addr)) && OpStatus::IsSuccess(socket->Bind(addr, BIND_BROADCAST)))
		// NBP: check for broadcast...
		if( OpStatus::IsSuccess(SocketWrapper::CreateUDPSocket(&socket_uni, &dev_lsn, SocketWrapper::ALLOW_CONNECTION_WRAPPER, addr)) &&
			OpStatus::IsSuccess(SocketWrapper::CreateUDPSocket(&socket_multi, &dev_lsn, SocketWrapper::ALLOW_CONNECTION_WRAPPER, addr))
		 /*&& OpStatus::IsSuccess(socket->Bind(addr, BIND_BROADCAST))*/)
		{
			NetworkCard *nc=OP_NEW(NetworkCard, (socket_uni, socket_multi, addr));  // Takes ownership of socket and addr
		
			if(nc && OpStatus::IsSuccess(network_cards.Add(nc)))
				return nc;
			
			OP_DELETE(nc);
			
			return NULL;
		}
	}
	
	OP_DELETE(addr);
	OP_DELETE(socket_uni);
	OP_DELETE(socket_multi);
	
	return NULL;	
}

#ifdef UPNP_LOG
void UPnP::LogMessage(const uni_char* msg, va_list arglist)
{
		if(!g_folder_manager)
			return;
			
		// No error management is done in this function, to try to log everything that is possible
		
		if(!IsLogging())
			return;
			
		uni_vsnprintf(g_dbg_mybuff, DEBUG_DEBUGBUFFERSIZE- 2, msg, arglist);
		uni_strcat(g_dbg_mybuff, UNI_L("\n"));

		OpString8 str;
		OpFile file;
		OpString8 prefix;
		double date=(g_op_system_info)?OpDate::GetCurrentUTCTime():0.0;
		
		prefix.Set("\r\n\r\n{@@@@@ ");
		if(date!=0.0)
		{
			prefix.AppendFormat("%d/%d/%d ", OpDate::YearFromTime(date), OpDate::MonthFromTime(date)+1, OpDate::DateFromTime(date));
			prefix.AppendFormat("%d:%d:%d @", OpDate::HourFromTime(date), OpDate::MinFromTime(date), OpDate::SecFromTime(date));
		}
		else
			prefix.Append(" @");
		
		str.Set(g_dbg_mybuff);
		
		file.Construct(UNI_L("UPnP.log"), OPFILE_HOME_FOLDER);
		if(OpStatus::IsSuccess(file.Open(OPFILE_APPEND)))
		{
			file.Write(prefix.CStr(), op_strlen(prefix.CStr()));
			if(str.CStr() && str.Length())
				file.Write(str.CStr(), str.Length());
			file.Write("@@@@@}", 6);
			file.Close();
		}
		
		//dbg_systemoutput(g_dbg_mybuff);
}
#endif

#ifdef UPNP_ANTI_SPOOF
	OP_STATUS UPnP::CheckIfAddressIsSpoofed(const char *url, OpSocketAddress *address, SpoofListener *listener)
	{
		if(!address || !url || !listener)
			return OpStatus::ERR_NULL_POINTER;
			
		if(!listener->GetSpoofResolver())
			return OpStatus::ERR_NULL_POINTER;
			
		URL test=g_url_api->GetURL(url);
		OpStringC16 host=test.GetAttribute(URL::KUniHostName, TRUE);

		if(!host.CStr())
			return OpStatus::ERR_NULL_POINTER;

		OpString addr;
		
		RETURN_IF_ERROR(address->ToString(&addr));
		if(!addr.Compare(host))
			listener->OnAddressLegit(address);
		else
			return listener->GetSpoofResolver()->Resolve(host.CStr());
		
		return OpStatus::OK;
	}
#endif

NetworkCard *UPnP::GetNetworkCardFromSocket(OpUdpSocket *socket)
{
	for(UINT32 i=0; i<network_cards.GetCount(); i++)
	{
		if(network_cards.Get(i)->GetSocketUnicast()==socket || network_cards.Get(i)->GetSocketMulticast() == socket)
			return network_cards.Get(i);
	}
	
	return NULL;
}

OP_STATUS UPnPRetryMessageHandler::AddAddressToList(OpSocketAddress *osa, OpAutoVector<OpString> *vector)
{
	if(!osa)
		return OpStatus::ERR_NULL_POINTER;
		
	// Double check on localhost UPnP::IsIPSuitable() is also used
	if(osa->GetNetType()==NETTYPE_LOCALHOST)
		return OpStatus::OK;
		
	OpString *str=OP_NEW(OpString, ());
		
	if(!str)
		return OpStatus::ERR_NO_MEMORY;
		
	OP_STATUS ops=osa->ToString(str);
	
	if(OpStatus::IsError(ops))
	{
		OP_DELETE(str);
		
		return ops;
	}
	
	if(UPnP::IsIPSuitable(str))  // If the IP is "strange" just skip it without giving errors
	{
		// Check for duplicates
		for(UINT32 i=0; i<vector->GetCount(); i++)
		{
			if(vector->Get(i) && !vector->Get(i)->Compare(str->CStr())) // duplicated
			{
				OP_DELETE(str);
				
				return OpStatus::OK;
			}
		}
		
		// Add it to the list
		if(OpStatus::IsError(ops=vector->Add(str)))
			OP_DELETE(str);
	}
	else
	{
		OP_DELETE(str);
	}
		
	return ops;
}

#ifdef PI_NETWORK_INTERFACE_MANAGER
OP_STATUS UPnPRetryMessageHandler::GetNetworkCardsListPI(OpAutoVector<OpString> *vector)
{
	if(!nic_man || !vector)
		return OpStatus::ERR_NULL_POINTER;
		
	vector->DeleteAll();
	
	OpSocketAddress *osa;
		
	if(OpStatus::IsError(OpSocketAddress::Create(&osa)))
		return OpStatus::ERR_NO_MEMORY;
		
	OP_STATUS ops;
	
	if(OpStatus::IsSuccess(ops=nic_man->BeginEnumeration()))
	{
		OpNetworkInterface *nic;
		
		while((nic=nic_man->GetNextInterface()) != NULL)
		{
			if(OpStatus::IsError(ops=nic->GetAddress(osa)) ||
				OpStatus::IsError(ops=AddAddressToList(osa, vector)))
				break;
		}
	}
	
	nic_man->EndEnumeration();
	
	OP_DELETE(osa);
	
	return ops;
}

#endif // PI_NETWORK_INTERFACE_MANAGER

OP_STATUS UPnPRetryMessageHandler::ManageNICChanges()
{
	OpAutoVector<OpString>current_cards;
	
	#ifdef PI_NETWORK_INTERFACE_MANAGER
		GetNetworkCardsListPI(&current_cards);
	#else
		GetNetworkCardsListHR(ohr, &current_cards);
	#endif
	
	if(upnp && upnp->IsNetworkCardsListChanged(&current_cards))
	{
#ifdef UPNP_LOG
		upnp->Log(UNI_L("UPnPRetryMessageHandler::OnHostResolved()"), UNI_L("Change detected on network cards list: %d cards"), current_cards.GetCount());
#endif
		
		OP_STATUS ops=upnp->StartUPNPDiscoveryProcess(&current_cards);
		OP_STATUS ops2=OpStatus::OK;
		
	#ifdef UPNP_SERVICE_DISCOVERY
		ops2=upnp->AdvertiseAllServicesProviders(FALSE, FALSE);
	#endif
		
		return OpStatus::IsError(ops)?ops:ops2;
	}
	
	return OpStatus::OK;
}


#ifndef PI_NETWORK_INTERFACE_MANAGER
OP_STATUS UPnPRetryMessageHandler::GetNetworkCardsListHR(OpHostResolver* host_resolver, OpAutoVector<OpString> *vector)
{
	OP_ASSERT(host_resolver && vector && vector->GetCount()==0);
	
	if(!host_resolver || !vector)
		return OpStatus::ERR_NULL_POINTER;
		
	vector->DeleteAll();
	
	int len=host_resolver->GetAddressCount();
	
	if(!len)
		return OpStatus::OK;
	
	OpSocketAddress *osa;
		
	if(OpStatus::IsError(OpSocketAddress::Create(&osa)))
		return OpStatus::ERR_NO_MEMORY;
	 
	OP_STATUS ops=OpStatus::OK;;
	
	for(int i=0; i<len; i++)
	{
		if(OpStatus::IsError(ops=host_resolver->GetAddress(osa, i)) ||
			OpStatus::IsError(ops=AddAddressToList(osa, vector)))
			break;
	}
	
	OP_DELETE(osa);
	
	return ops;
}

void UPnPRetryMessageHandler::OnHostResolved(OpHostResolver* host_resolver)
{
	OP_ASSERT(host_resolver==ohr);
	
	OpStatus::Ignore(ManageNICChanges());
}

#endif // !PI_NETWORK_INTERFACE_MANAGER
	
void UPnPRetryMessageHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if(par1==MSG_UPNP_CHECK_NETWORK_CARDS)
	{
		mh.PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_CHECK_NETWORK_CARDS, 0, DELAY_NETWORK_CARD_CHECK_MS);
		
	#ifdef PI_NETWORK_INTERFACE_MANAGER
		if(!nic_man)
			RETURN_VOID_IF_ERROR(OpNetworkInterfaceManager::Create(&nic_man, this));

		ManageNICChanges();
	#else
		// Resolve the current IPs to check if they changes since last time
		OpHostResolver::Error err;
		OP_DELETE(ohr);
		ohr=NULL;

		OpString local_host_name;

		RETURN_VOID_IF_ERROR(SocketWrapper::CreateHostResolver(&ohr, this));
		RETURN_VOID_IF_ERROR(ohr->GetLocalHostName(&local_host_name, &err));
		RETURN_VOID_IF_ERROR(ohr->Resolve(local_host_name.CStr()));
	#endif
	}
	else if(par1==MSG_UPNP_RECONNECT)
	{
		upnp->StartUPnPDiscovery();
		mh.PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_RECONNECT, 0, DELAY_REPROCESS_UPNP);
	}
	
#ifdef UPNP_SERVICE_DISCOVERY
	else if(par1==MSG_UPNP_SERVICES_ADVERTISE)
	{
		upnp->AdvertiseAllServicesProviders(FALSE, FALSE);
		mh.PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_SERVICES_ADVERTISE, 0, DELAY_UPNP_ADVERTISE);
	}
	else if(par1==MSG_UPNP_FIRST_SERVICES_ADVERTISE)
	{
		if(upnp->NetworkCardsAvailable())
			upnp->AdvertiseAllServicesProviders(FALSE, FALSE);
		else
			mh.PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_FIRST_SERVICES_ADVERTISE, 0, DELAY_FIRST_ADVERTISE);
	}
	else if(par1==MSG_UPNP_BROADCAST_NOTIFY) // Broadcast the notify message (delayed to give the webserver time to wake up properly)
	{
		for(UINT repeat=0; repeat<3; repeat++)
		{
			OpString8 mex;

			for(UINT j=0; j<upnp->network_cards.GetCount(); j++)
			{
				NetworkCard *nic=upnp->network_cards.Get(j);

				upnp->SendBroadcast(nic, ((OpString8 *)par2)->CStr());
			}
		}

		OP_DELETE((OpString8 *)par2);
	}
#endif
	else if(par1==MSG_UPNP_TIMEOUT)
	{
		BOOL self_del=upnp->GetSelfDelete();
		
		upnp->HandlerFailed(NULL, "No UPnP devices detected!");
			
		if(self_del)
		{
			OP_DELETE(upnp);
			upnp=NULL;
		}
	}
	else if(par1==MSG_UPNP_CHECK_EXPIRE)
	{
		upnp->CheckExpires();
		mh.PostDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_CHECK_EXPIRE, 0, UPNP_DEVICE_EXPIRE_CHECK_MS);
	}
}

UPnPRetryMessageHandler::UPnPRetryMessageHandler(UPnP *u):mh(NULL)
{
	OP_ASSERT(u);
	
	upnp=u;
#ifdef PI_NETWORK_INTERFACE_MANAGER
	nic_man=NULL;
#else
	ohr=NULL;
#endif // PI_NETWORK_INTERFACE_MANAGER
	//first_check_done=FALSE;
	
	// FIXME: second phase constructor required
	mh.SetCallBack(this, MSG_COMM_RETRY_CONNECT, 0);
	
	//// Done in the UPnP::StartUPNPDiscovery() object
	//if(DELAY_NETWORK_CARD_CHECK_MS>0)
	//	mh.PostMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_CHECK_NETWORK_CARDS, 0, DELAY_NETWORK_CARD_CHECK_MS);
	//if(DELAY_REPROCESS_UPNP>0)
	//	mh.PostMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_RECONNECT, 0, DELAY_REPROCESS_UPNP);
}

// Destructor: remove messages
void UPnPRetryMessageHandler::RemoveMessages()
{
	mh.RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_CHECK_NETWORK_CARDS, 0);
	mh.RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_RECONNECT, 0);
	mh.RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_TIMEOUT, 0);
	mh.RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_CHECK_EXPIRE, 0);
	mh.RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_SERVICES_ADVERTISE, 0);
	mh.RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_FIRST_SERVICES_ADVERTISE, 0);
	mh.RemoveDelayedMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_BROADCAST_NOTIFY, 0);
}

// Destructor: remove callbacks
void UPnPRetryMessageHandler::RemoveCallBacks()
{
	mh.RemoveCallBacks(this, 0);
}

// Destructor: delete ohr or nic_man
void UPnPRetryMessageHandler::RemoveNic()
{
#ifdef PI_NETWORK_INTERFACE_MANAGER
	OP_DELETE(nic_man);
	nic_man=NULL;
#else
	OP_DELETE(ohr);
	ohr=NULL;
#endif // PI_NETWORK_INTERFACE_MANAGER

}

UPnPRetryMessageHandler::~UPnPRetryMessageHandler()
{
	RemoveMessages();
	RemoveCallBacks();
	RemoveNic();
}

// TRUE if the network cards list has changed (at the moment, it the position changed, the list is considered changed
BOOL UPnP::IsNetworkCardsListChanged(OpAutoVector<OpString> *new_cards)
{
	OP_ASSERT(new_cards);
 
	if(!new_cards)
		return FALSE;
	
	if(new_cards->GetCount()!=network_cards.GetCount())
		return TRUE;
		
	for(UINT32 i=0; i<new_cards->GetCount(); i++)
	{
		OpString *ip_card1=new_cards->Get(i);
		NetworkCard *card2=network_cards.Get(i);
		
		OP_ASSERT(ip_card1 && card2);
		
		if(!card2 || !ip_card1)
			return TRUE;
			
		if(!card2->IsAddressEqual(ip_card1))
			return TRUE;
	}
		
	return FALSE;
}

// Update the Network Cards list with the new one (at the momen, all the card will be deleted, and new sockets created)
OP_STATUS UPnP::UpdateNetworkCardsList(OpAutoVector<OpString> *new_cards)
{
	OP_ASSERT(new_cards);
	
	/*if (known_devices.GetCount() != 0)
	{
		// TODO: Bailing out for now. The second time this function is called,
		// dev_lsn may have an XMLParser pointing to a UPnPDevice we are about
		// to remove.
		return OpStatus::OK;
	}*/
	
	known_devices.RemoveAll(FALSE, TRUE);
	network_cards.DeleteAll();
	
#ifdef UPNP_LOG
	Log(UNI_L("UPnP::UpdateNetworkCardsList()"), UNI_L("UPnPDevices and NetworkCards deleted"));
#endif
	
	for(UINT32 i=0; i<new_cards->GetCount(); i++)
	{
		AddNewNetworkCard(new_cards->Get(i));
	}
	
	return OpStatus::OK;
}

BOOL NetworkCard::IsAddressEqual(OpString *ip)
{
	OP_ASSERT(ip && address);
	
	if(!ip && !address)
		return TRUE;
		
	if(address && ip)
	{
		OpString str;
		
		RETURN_VALUE_IF_ERROR(address->ToString(&str), FALSE);
		
		return !ip->CompareI(str);
	}
	
	return FALSE;
}

OP_STATUS UPnP::StartUPNPDiscoveryProcess(OpAutoVector<OpString> *cards)
{
	if(cards)
		RETURN_IF_ERROR(UpdateNetworkCardsList(cards));
		
	OP_STATUS st=OpStatus::OK;
	
	for(UINT32 i=0; i<network_cards.GetCount(); i++)
	{
		OP_ASSERT(network_cards.Get(i));
		
		if(network_cards.Get(i))
		{
#ifdef UPNP_LOG
			OpSocketAddress *address=network_cards.Get(i)->GetAddress();
			OpString str;
			
			address->ToString(&str);
#endif
			
			if(OpStatus::IsError(SubscribeUDPDiscovery(network_cards.Get(i))))
			{
#ifdef UPNP_LOG
				Log(UNI_L("UPnP::StartUPNPDiscoveryProcess()"), UNI_L("Error on subscribing to UDP discovery advertisements on network card %s"), str.CStr());
#endif
			}
			
			OP_STATUS broadcastStatus=BroadcastUDPDiscovery(network_cards.Get(i));
			
			if(OpStatus::IsError(broadcastStatus))
			{
				st=broadcastStatus;
#ifdef UPNP_LOG
				Log(UNI_L("UPnP::StartUPNPDiscoveryProcess()"), UNI_L("Error on sending UDP discovery request to network card %s"), str.CStr());
#endif
				// TODO: signal error?
				//OP_DELETE(upnp);
				//return OpStatus::ERR;
				if(broadcastStatus==UPnPStatus::ERROR_UDP_SEND_FAILED)
					HandlerFailed(NULL, "Broadcast failed: bind problem?");
				else
					HandlerFailed(NULL, "Broadcast failed");
			}
		}
	}
	
	return st;
}

// Return the network cards
BOOL UPnP::IsNetworkCardPresent(NetworkCard *card)
{
	for(UINT32 i=0; i<network_cards.GetCount(); i++)
	{
		if(network_cards.Get(i)==card)
			return TRUE;
	}
	
	return FALSE;
}
	
// Get the device attached to the specified socket
/*UPnPDevice *UPnP::GetDeviceFromSocket(OpUdpSocket *socket)
{
	OP_ASSERT(socket);
	
	if(!socket)
		return NULL;
		
	for(UINT i=0; i<known_devices.GetCount(); i++)
	{
		UPnPDevice *device=known_devices.Get(i);
		OpUdpSocket *device_socket_multicast=(!device)?NULL:device->GetNetworkCardSocketMulticast();
		OpUdpSocket *device_socket_unicast=(!device)?NULL:device->GetNetworkCardSocketUnicast();
		
		OP_ASSERT(device && device_socket_multicast && device_socket_unicast);
		
		if(socket==device_socket_multicast || socket==device_socket_unicast)
			return device;
	}
	
	return NULL;
}*/

BOOL UPnPDevice::IsSameDevice(const char *dev_usn, const char *dev_loc)
{
	return (usn.HasContent() && !usn.Compare(dev_usn)) || (dev_loc && (device_url.HasContent() && !device_url.Compare(dev_loc))) || (current_description_url.HasContent() && !current_description_url.Compare(dev_loc));
}

BOOL UPnPDevice::IsSameDevice(const uni_char *dev_usn, const uni_char *dev_loc)
{
	return (usn.HasContent() && !usn.Compare(dev_usn)) || (dev_loc && (device_url.HasContent() && !device_url.Compare(dev_loc))) || (current_description_url.HasContent() && !current_description_url.Compare(dev_loc));
}

BOOL UPnPDevice::IsSameDevice(UPnPDevice *dev)
{
	OP_ASSERT(dev);
	
	if(!dev)
		return FALSE;
	
	return (usn.HasContent() && !usn.Compare(dev->usn.CStr())) || 
		(device_url.HasContent() && !device_url.Compare(dev->device_url.CStr()))
		|| (current_description_url.HasContent() && !current_description_url.Compare(dev->current_description_url.CStr()));
}

OP_STATUS UPnPDevice::SetCurrentDescriptionURL(const OpString &str, BOOL update_time)
{
	RETURN_IF_ERROR(current_description_url.Set(str));
	
	if(provider)
		RETURN_IF_ERROR(provider->SetDescriptionURL(str));
	
	if(update_time || !current_description_time)
		current_description_time=g_op_time_info->GetWallClockMS();
		
	return OpStatus::OK;
}

const char * UPnPDevice::GetReason()
{
	if(reason==UDP_REASON_DISCOVERY)
		return "UPnP Discovery";
	else if(reason==UDP_REASON_SSDP_ALIVE_LOCATION)
		return "ssdp:alive Loc";
	else if(reason==UDP_REASON_SSDP_ALIVE_THRESHOLD)
		return "ssdp:alive Thr";
	else
		return "???";
}

UPnPLogic::~UPnPLogic()
{
	Disable();
}

void UPnPLogic::Disable()
{
	upnp->RemoveLogic(this);
	g_main_message_handler->RemoveCallBacks(&event_handler, url_subscribe.Id());
}

OP_STATUS UPnPLogic::ReEnable()
{
	OpStatus::Ignore(g_main_message_handler->SetCallBack(&event_handler, MSG_COMM_LOADING_FINISHED, url_subscribe.Id()));
	
	return upnp->AddLogic(this, FALSE, FALSE);
}

OP_STATUS UPnPLogic::AddListener(UPnPListener *upnp_listener)
{
	if(!upnp_listener)
		return OpStatus::ERR_NULL_POINTER;
		
	return listeners.Add(upnp_listener);
}

OP_STATUS UPnPLogic::RemoveListener(UPnPListener *upnp_listener)
{
	if(!upnp_listener)
		return OpStatus::ERR_NULL_POINTER;
		
	return listeners.RemoveByItem(upnp_listener);
}

BOOL UPnPLogic::AcceptSearchTarget(const OpStringC8 &search_target)
{
	if(locked)
		return FALSE;
		
	return !search_target.Compare(target) || !search_target.Compare(UPNP_DISCOVERY_ROOT_SEARCH_TYPE);
}

BOOL UPnPLogic::AcceptSearchTarget(const char *search_target)
{
	if(locked)
		return FALSE;
		
	return !target.Compare(search_target) || !op_strcmp(search_target, UPNP_DISCOVERY_ROOT_SEARCH_TYPE);
}

BOOL UPnPLogic::AcceptDevice(UPnPDevice *dev)
{
	if(!dev)
		return FALSE;
	
	UPnPServicesProvider *prov=dev->GetProvider();
	
	if(!prov)
		return FALSE;
	
	OpString8 temp;
	const uni_char *type=prov->GetAttributeValueUni(UPnPServicesProvider::DEVICE_TYPE);
	
	if(!type)
		return FALSE;
	
	RETURN_VALUE_IF_ERROR(temp.Set(type), FALSE);
	
	return AcceptSearchTarget(temp);
}

OP_STATUS UPnPLogic::CreateDiscoveryessage(OpString8 &dscv_mex)
{
	RETURN_IF_ERROR(dscv_mex.Set("M-SEARCH * HTTP/1.1\r\n"));
	RETURN_IF_ERROR(dscv_mex.Append("HOST: 239.255.255.250:1900\r\n"));
	RETURN_IF_ERROR(dscv_mex.AppendFormat("ST: %s\r\n", target.CStr()));
	RETURN_IF_ERROR(dscv_mex.Append("MAN: \"ssdp:discover\"\r\n"));
	RETURN_IF_ERROR(dscv_mex.Append("MX: 3\r\n"));
	RETURN_IF_ERROR(dscv_mex.Append("User-Agent: Opera Unite\r\n\r\n"));
	
	return OpStatus::OK;
}

/// Broadcast a packet to the UDP Multicast Address
OP_STATUS UPnP::SendBroadcast(NetworkCard *nic, char *mex)
{
	OP_ASSERT(nic && mex);
	
	if(!nic || !mex || !nic->GetSocketUnicast())
		return OpStatus::ERR_NULL_POINTER;
		
	return nic->GetSocketUnicast()->Send(mex, op_strlen(mex), saUDPDiscovery, OpUdpSocket::MULTICAST);
}
	
#ifdef UPNP_SERVICE_DISCOVERY
OP_STATUS UPnP::SendByeBye(UPnPServicesProvider *serv_prov, UINT num_times)
{
	OP_ASSERT(num_times>0);
	OP_ASSERT(serv_prov);

	// FIXME: only discovered devices?
	if(serv_prov && !serv_prov->IsDisabled())
	{
		for(UINT repeat=0; repeat<num_times; repeat++)
		{
			OpString8 mex;
				
			for(UINT j=0; j<network_cards.GetCount(); j++)
			{
				NetworkCard *nic=network_cards.Get(j);
				
				if(nic)
				{
					RETURN_IF_ERROR(serv_prov->CreateSSDPByeBye(mex));
					
					OpStatus::Ignore(SendBroadcast(nic, mex.CStr()));
				}
			}
		}
		
		serv_prov->SetDisabled(TRUE);
	}
	
	return OpStatus::OK;
}


OP_STATUS UPnP::AdvertiseAllServicesProviders(BOOL force_update, BOOL delay, UINT num_times)
{
	for(UINT i=0; i<dev_lsn.services_providers.GetCount(); i++)
	{
		RETURN_IF_ERROR(UPnP::AdvertiseServicesProvider(dev_lsn.services_providers.GetPointer(i), delay, force_update, num_times));
	}
	
	return OpStatus::OK;
}

OP_STATUS UPnP::AdvertiseServicesProvider(UPnPServicesProvider *serv_prov, BOOL force_update, BOOL delay, UINT num_times)
{
	OP_ASSERT(num_times>0);
	OP_ASSERT(serv_prov);
	
	if(serv_prov && !serv_prov->IsDisabled() && !serv_prov->IsDiscovered() )
	{
		for(UINT repeat=0; repeat<num_times; repeat++)
		{
			OpString8 mex;
				
			for(UINT j=0; j<network_cards.GetCount(); j++)
			{
				NetworkCard *nic=network_cards.Get(j);
				
				if(nic && nic->GetAddress())
				{
					RETURN_IF_ERROR(serv_prov->UpdateDescriptionURL(nic->GetAddress()));
					RETURN_IF_ERROR(serv_prov->CreateSSDPAlive(mex, force_update));
					
					if(delay)
					{
						OpString8 *new_mex=OP_NEW(OpString8, ());

						if(!new_mex)
							return OpStatus::ERR_NO_MEMORY;

						OP_STATUS ops=new_mex->Set(mex);

						if(OpStatus::IsError(ops))
						{
							OP_DELETE(new_mex);

							return ops;
						}

						scheduler.GetMessageHandler()->PostMessage(MSG_COMM_RETRY_CONNECT, MSG_UPNP_BROADCAST_NOTIFY, (MH_PARAM_2)new_mex);
					}
					else
						SendBroadcast(nic, mex.CStr());
				}
			}
		}
	}
	
	return OpStatus::OK;
}
#endif // UPNP_SERVICE_DISCOVERY

OP_STATUS AttributesVector::CreateAttributes(UINT num, BOOL delete_if_errorr)
{
	UINT i;
	OP_STATUS ops=OpStatus::OK;
	UINT cnt=attributes.GetCount();
	
	for(i=0; i<num; i++)
	{
		NameValue *nv=OP_NEW(NameValue, ());
		
		if(!nv)
		{
			ops=OpStatus::ERR_NO_MEMORY;
			break;
		}
		else if(OpStatus::IsError(ops=attributes.Add(nv)))
		{
			OP_DELETE(nv);
			break;
		}
	}
	
	if(OpStatus::IsError(ops) && delete_if_errorr && i)
		attributes.Delete(cnt, i);			
	
	return ops;
}

OP_STATUS AttributesVector::SetAttributeName(UINT index, const char *name, BOOL required)
{
	NameValue *nv=GetAttribute(index);
	
	if(!nv)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	
	return nv->SetName(name, required);
}

OP_STATUS AttributesVector::SetAttributeValue(UINT index, const uni_char *value)
{
	NameValue *nv=GetAttribute(index);
	
	if(!nv)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	
	OP_ASSERT(nv->GetName().HasContent());
	
	return nv->SetValue(value);
}

OP_STATUS AttributesVector::SetAttributeValue(UINT index, const char *value)
{
	NameValue *nv=GetAttribute(index);
	
	if(!nv)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	
	OP_ASSERT(nv->GetName().HasContent());
	
	return nv->SetValue(value);
}

NameValue *AttributesVector::GetAttributeByName(const uni_char *name)
{
	if(!name)
		return NULL;
		
	for(int i=0, len=attributes.GetCount(); i<len; i++)
	{
		NameValue *nv=attributes.Get(i);
		
		if(nv && !nv->GetName().Compare(name))
			return nv;
	}
	
	return NULL;
}

NameValue *AttributesVector::GetAttributeByNameLen(const uni_char *name, int tag_len)
{
	if(!name || tag_len<=0)
		return NULL;
		
	for(int i=0, len=attributes.GetCount(); i<len; i++)
	{
		NameValue *nv=attributes.Get(i);
		
		if(nv && !nv->GetName().Compare(name, tag_len) && nv->GetName().Length()==tag_len)
			return nv;
	}
	
	return NULL;
}

OP_STATUS AttributesVector::SetAttributeValueByName(const uni_char *name, const uni_char *value)
{
	NameValue *nv=GetAttributeByName(name);
	
	if(!nv)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	
	OP_ASSERT(nv->GetName().HasContent());
	
	return nv->SetValue(value);
}

OP_STATUS AttributesVector::SetAttributeValueByNameLen(const uni_char *name, int tag_len, const uni_char *value)
{
	NameValue *nv=GetAttributeByNameLen(name, tag_len);
	
	if(!nv)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	
	OP_ASSERT(nv->GetName().HasContent());
	
	return nv->SetValue(value);
}

OP_STATUS AttributesVector::CopyAttributeValue(UINT index, OpString &dest)
{
	OpStringC *str=GetAttributeValue(index);
	
	if(!str)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	return dest.Set(str->CStr());
}


OP_STATUS AttributesVector::AppendAttributesXML(XMLFragment *xf, const uni_char *parent_tag)
{
	if(parent_tag && *parent_tag)
	{
		XMLCompleteName xcn_parent(_XML_ATT_XMLNS_ parent_tag);
		
		RETURN_IF_ERROR(xf->OpenElement(xcn_parent));
	}
	
	for(UINT i=0, len=attributes.GetCount(); i<len; i++)
	{
		NameValue *nv=GetAttribute(i);
	
		if(!nv)
			return OpStatus::ERR_NO_SUCH_RESOURCE;
		
		if(nv->HasValue())
		{		
			XMLCompleteName xcn_att(_XML_ATT_XMLNS_ nv->GetName().CStr());
			
			RETURN_IF_ERROR(xf->OpenElement(xcn_att));
			RETURN_IF_ERROR(xf->AddText(nv->GetValue().CStr()));
			
			xf->CloseElement();
		}
	}
	
	if(parent_tag && *parent_tag)
		xf->CloseElement(); // parent_tag
	
	return OpStatus::OK;
}

OP_STATUS UPnPService::Construct()
{
	RETURN_IF_ERROR(CreateAttributes(ADDITIONAL_SERVICE_ATTRIBUTES, TRUE));
	
	// UPnP Attributes
	RETURN_IF_ERROR(SetAttributeName(SERVICE_TYPE, "serviceType", TRUE));
	RETURN_IF_ERROR(SetAttributeName(SERVICE_ID, "serviceId", TRUE));
	RETURN_IF_ERROR(SetAttributeName(SCPD_URL, "SCPDURL", TRUE));
	RETURN_IF_ERROR(SetAttributeName(CONTROL_URL, "controlURL", TRUE));
	RETURN_IF_ERROR(SetAttributeName(EVENT_URL, "eventSubURL", TRUE));
	
	// Custom Unite attributes
	RETURN_IF_ERROR(SetAttributeName(NAME, "name"));
	RETURN_IF_ERROR(SetAttributeName(DESCRIPTION, "description"));
	
	
	OP_ASSERT(GetAttributesCount()==ADDITIONAL_SERVICE_ATTRIBUTES);
	
	return OpStatus::OK;
}

OP_STATUS UPnPServicesProvider::Construct()
{
	RETURN_IF_ERROR(CreateAttributes(ADDITIONAL_DEVICE_ATTRIBUTES, TRUE));
	
	// UPnP Attributes
	RETURN_IF_ERROR(SetAttributeName(DEVICE_TYPE, "deviceType", TRUE));
	RETURN_IF_ERROR(SetAttributeName(FRIENDLY_NAME, "friendlyName", TRUE));
	RETURN_IF_ERROR(SetAttributeName(MANUFACTURER, "manufacturer", TRUE));
	RETURN_IF_ERROR(SetAttributeName(MANUFACTURER_URL, "manufacturerURL"));
	RETURN_IF_ERROR(SetAttributeName(MODEL_DESCRIPTION, "modelDescription"));
	RETURN_IF_ERROR(SetAttributeName(MODEL_NAME, "modelName", TRUE));
	RETURN_IF_ERROR(SetAttributeName(MODEL_NUMBER, "modelNumber"));
	RETURN_IF_ERROR(SetAttributeName(MODEL_URL, "modelURL"));
	RETURN_IF_ERROR(SetAttributeName(SERIAL_NUMBER, "serialNumber"));
	RETURN_IF_ERROR(SetAttributeName(UDN, "UDN", TRUE));
	RETURN_IF_ERROR(SetAttributeName(UPC, "UPC"));
	RETURN_IF_ERROR(SetAttributeName(PRESENTATION_URL, "presentationURL"));
	
	// Custom Unite attributes
	RETURN_IF_ERROR(SetAttributeName(UNITE_USER, "uniteUser", FALSE));
	RETURN_IF_ERROR(SetAttributeName(UNITE_DEVICE, "uniteDeviceName", FALSE));
	RETURN_IF_ERROR(SetAttributeName(CHALLENGE_RESPONSE, "challengeResponse", FALSE));
	RETURN_IF_ERROR(SetAttributeName(CHALLENGE_RESPONSE_AUTH, "challengeResponseAuth", FALSE));
	RETURN_IF_ERROR(SetAttributeName(NONCE_AUTH, "nonceAuth", FALSE));
	
	// Custom Opera attributes
	RETURN_IF_ERROR(SetAttributeName(DEVICE_ICON_RESOURCE, "deviceicon", FALSE));
	RETURN_IF_ERROR(SetAttributeName(PAYLOAD, "payload", FALSE));

	OP_ASSERT(GetAttributesCount()==ADDITIONAL_DEVICE_ATTRIBUTES);
	
	return OpStatus::OK;
}

BOOL UPnPServicesProvider::AcceptSearch(const OpStringC &device_type, SearchTargetMode &mode)
{
	if(IsDisabled())
		return FALSE;
		
	OP_ASSERT(GetAttribute(DEVICE_TYPE) && GetAttribute(DEVICE_TYPE)->GetValue().HasContent());
	
	mode=SearchNoMatch;
	
	if(!device_type.CompareI(UPNP_DISCOVERY_ROOT_SEARCH_TYPE))
		mode=SearchRootDevice;
	else if(GetAttribute(DEVICE_TYPE) && !GetAttribute(DEVICE_TYPE)->GetValue().CompareI(device_type))
		mode=SearchDeviceType;
	else if(GetAttribute(UDN) && !GetAttribute(UDN)->GetValue().CompareI(device_type))
		mode=SearchUUID;
		
	return (mode!=SearchNoMatch);
}

BOOL UPnPServicesProvider::AcceptSearch(const char *dev_type, SearchTargetMode &mode)
{
	if(IsDisabled())
		return FALSE;
		
	OpString8 device_type;
	
	RETURN_VALUE_IF_ERROR(device_type.Set(dev_type), FALSE);
	
	return AcceptSearch(device_type, mode);
}

BOOL UPnPServicesProvider::AcceptSearch(const OpStringC8 &device_type, SearchTargetMode &mode)
{
	if(IsDisabled())
		return FALSE;
		
	const char* devstr = GetDeviceTypeString();

	OP_ASSERT(devstr);

	mode=SearchNoMatch;
	
	if(!device_type.CompareI(UPNP_DISCOVERY_ROOT_SEARCH_TYPE))
		mode=SearchRootDevice;
	else if(!device_type.Compare(devstr))
		mode=SearchDeviceType;
	else if(GetAttribute(UDN) && !GetAttribute(UDN)->GetValue().CompareI(device_type))
		mode=SearchUUID;
		
	return (mode!=SearchNoMatch);
}

/// Create the packet used for ssdp:alive
OP_STATUS UPnPServicesProvider::CreateSSDPAlive(OpString8 &str8, BOOL force_update)
{
	OP_ASSERT(!discovered);
	
	if(IsDisabled() || discovered)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	OpString str;
	OpString server;
	
	RETURN_IF_ERROR(server.Set(g_op_system_info->GetPlatformStr()));
	const uni_char *udn_str;
	const uni_char *dev_str;
	NameValue *udn=GetAttribute(UDN);
	if(udn && udn->GetValue().HasContent())
		udn_str=udn->GetValue().CStr();
	else
		udn_str=UNI_L("uuid:opera");
		
	NameValue *dev_type=GetAttribute(DEVICE_TYPE);
	if(dev_type && dev_type->GetValue().HasContent())
		dev_str=dev_type->GetValue().CStr();
	else
		dev_str=UNI_L(UPNP_DISCOVERY_ROOT_SEARCH_TYPE);
		
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("NOTIFY * HTTP/1.1\r\n")));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("Host: %s:%d\r\n"), UPNP_DISCOVERY_ADDRESS, UPNP_DISCOVERY_PORT));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("Cache-Control: max-age=1800\r\n")));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("Location: %s\r\n"), GetFullDescriptionURL().CStr()));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("Server: %s\r\n"), server.CStr()));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("NT: %s\r\n"), UNI_L(UPNP_DISCOVERY_ROOT_SEARCH_TYPE)));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("NT2: %s\r\n"), dev_str));  // Opera specific... to skip some messages...
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("NTS: ssdp:alive\r\n")));
	if(force_update)
		RETURN_IF_ERROR(str.AppendFormat(UNI_L("ForceUpdate: TRUE\r\n")));
	OpString deviceunistr;
	RETURN_IF_ERROR(deviceunistr.Set(GetDeviceTypeString()));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("USN: %s::%s\r\n\r\n"), udn_str, deviceunistr.CStr()));
	
	return str8.Set(str.CStr());
}

OP_STATUS UPnPServicesProvider::CreateSSDPByeBye(OpString8 &str8)
{
	OP_ASSERT(!discovered);
	
	if(IsDisabled() || discovered)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	OpString str;
	const uni_char *udn_str;
	NameValue *udn=GetAttribute(UPnPServicesProvider::UDN);
	if(udn && udn->GetValue().HasContent())
		udn_str=udn->GetValue().CStr();
	else
		udn_str=UNI_L("uuid:opera");
		
	RETURN_IF_ERROR(str.Append(UNI_L("NOTIFY * HTTP/1.1\r\n")));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("HOST: %s:%d\r\n"), UPNP_DISCOVERY_ADDRESS, UPNP_DISCOVERY_PORT));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("NT: %s\r\n"), UNI_L(UPNP_DISCOVERY_ROOT_SEARCH_TYPE)));
	RETURN_IF_ERROR(str.Append(UNI_L("NTS: ssdp:byebye\r\n")));
	//RETURN_IF_ERROR(str.AppendFormat(UNI_L("USN: %s\r\n\r\n"), udn_str));
	OpString deviceunistr;
	RETURN_IF_ERROR(deviceunistr.Set(GetDeviceTypeString()));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("USN: %s::%s\r\n\r\n"), udn_str, deviceunistr.CStr()));
	
	return str8.Set(str.CStr());
}

OP_STATUS UPnPServicesProvider::CreateSCPD(ByteBuffer &buf)
{
	OP_ASSERT(!discovered);
	
	if(IsDisabled() || discovered)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	OpString8 str;
	
	//RETURN_IF_ERROR(str.Set("<?xml version=\"1.0\"?><scpd xmlns=\"urn:schemas-upnp-org:service-1-0\"><specVersion><major>1</major><minor>0</minor></specVersion><serviceStateTable><stateVariable sendEvents=\"no\"><name>dummy</name><dataType>ui4</dataType><defaultValue>0</defaultValue></stateVariable></serviceStateTable></scpd>"));
	RETURN_IF_ERROR(str.Set("<?xml version=\"1.0\"?><scpd xmlns=\"urn:schemas-upnp-org:service-1-0\"><specVersion><major>1</major><minor>0</minor></specVersion></scpd>"));
	
	return buf.AppendString(str.CStr());
}

OP_STATUS UPnPServicesProvider::CreateDescriptionXML(ByteBuffer &buf, OpString8 &challenge)
{
	OP_ASSERT(!discovered);
	
	if(IsDisabled() || discovered)
		return OpStatus::ERR_NOT_SUPPORTED;
		
	RETURN_IF_ERROR(UpdateValues(challenge));
		
	XMLFragment xf;
	XMLCompleteName xcn_root(_XML_ATT_XMLNS_ UNI_L("root"));
	XMLCompleteName xcn_spec(_XML_ATT_XMLNS_ UNI_L("specVersion"));
	XMLCompleteName xcn_major(_XML_ATT_XMLNS_ UNI_L("major"));
	XMLCompleteName xcn_minor(_XML_ATT_XMLNS_ UNI_L("minor"));
	XMLCompleteName xcn_device(_XML_ATT_XMLNS_ UNI_L("device"));
	XMLCompleteName xcn_service_list(_XML_ATT_XMLNS_ UNI_L("serviceList"));
	
	// Write the device attributes
//#ifdef XMLUTILS_XMLFRAGMENT_EXTENDED_CONTENT
//	xf.SetStoreCommentsAndPIs(TRUE);
//#endif
	RETURN_IF_ERROR(xf.OpenElement(xcn_root));
	//RETURN_IF_ERROR(xf.SetAttribute(xcn_xmlns, UNI_L("urn:schemas-upnp-org:device-1-0")));
	RETURN_IF_ERROR(xf.OpenElement(xcn_spec));
	RETURN_IF_ERROR(xf.OpenElement(xcn_major));
	RETURN_IF_ERROR(xf.AddText(UNI_L("1")));
	xf.CloseElement();  // Major
	RETURN_IF_ERROR(xf.OpenElement(xcn_minor));
	RETURN_IF_ERROR(xf.AddText(UNI_L("0")));
	xf.CloseElement();  // Minor
	xf.CloseElement();  // Spec Version
	
	RETURN_IF_ERROR(xf.OpenElement(xcn_device));
	
	RETURN_IF_ERROR(AppendAttributesXML(&xf, NULL)); // Device attributes
	
	int num_services=GetNumberOfServices();
	
	if(num_services)
	{
		RETURN_IF_ERROR(xf.OpenElement(xcn_service_list));
		
		// Write the real services
		UPnPService *service;
		
		for(int i=0; i<num_services; i++)
		{
			RETURN_IF_ERROR(RetrieveService(i, service));
			if(service && service->IsVisible())
				service->AppendAttributesXML(&xf, UNI_L("service"));
		}
		
		// Close the XML
		xf.CloseElement();  // Service List
	}
	
	
	xf.CloseElement();  // Device
	xf.CloseElement();  // Root
	
	XMLFragment::GetXMLOptions xml_opt(TRUE);
	
	xml_opt.encoding="UTF-8";
	//xml_opt.canonicalize=XMLFragment::GetXMLOptions::CANONICALIZE_WITH_COMMENTS;
	
	return xf.GetEncodedXML(buf, xml_opt);
}

OP_STATUS UPnPServicesProvider::RetrieveServiceByName(const uni_char *service_name, UPnPService *&service)
{
	UPnPService *srv;
	NameValue *nv_type=NULL;
	NameValue *nv_id=NULL;
	
	service=NULL;
	
	for(UINT i=0, len=GetNumberOfServices(); i<len; i++)
	{
		RETURN_IF_ERROR(RetrieveService(i, srv));
		
		if(srv)
		{
			nv_type=srv->GetAttribute(UPnPService::SERVICE_TYPE);
			nv_id=srv->GetAttribute(UPnPService::SERVICE_ID);
		}
			
		if(nv_type && (!nv_type->GetValue().Compare(service_name) || !nv_id->GetValue().Compare(service_name)))
		{
			service=srv;
			
			return OpStatus::OK;
		}
	}
	
	return OpStatus::ERR_NO_SUCH_RESOURCE;
}

OP_STATUS UPnPServicesProvider::SetDescriptionURL(const OpString &description)
{
	RETURN_IF_ERROR(full_description_url.Set(description));

	int challenge_index=description.Find("challenge=");

	if(challenge_index>0)
	{
		if(description[challenge_index-1]=='?' || description[challenge_index-1]=='&')
			return short_description_url.Set(description, challenge_index-1);
		else
			return short_description_url.Set(description, challenge_index);
	}
	else
		return short_description_url.Set(description);
}

void AttributesVector::ResetAttributesValue()
{
	for(UINT i=0, len=attributes.GetCount(); i<len; i++)
	{
		NameValue *nv=GetAttribute(i);
	
		if(nv)
			nv->ResetValue();
	}
};

#ifdef UPNP_SERVICE_DISCOVERY
OP_STATUS UPnP::CreateUniteDiscoveryObject(UPnP *upnp, UPnPServicesProvider *prov)
{
	OP_ASSERT(upnp);
	
	if(!upnp)
		return OpStatus::ERR_NULL_POINTER;
		
	RETURN_IF_ERROR(upnp->StartUPnPDiscovery());
	if(prov)
		RETURN_IF_ERROR(upnp->AddServicesProvider(prov, FALSE));
	
#ifdef ADD_PASSIVE_CONTROL_POINT
	if(!upnp->cp_unite)
		upnp->cp_unite=OP_NEW(UPnPPassiveControlPoint, (upnp, prov->GetDeviceTypeString()));
		
	if(!upnp->cp_unite)
		return OpStatus::ERR_NO_MEMORY;
		
	return upnp->AddLogic(upnp->cp_unite);  // No memory leak even if it fails
#else
	return OpStatus::OK;
#endif //ADD_PASSIVE_CONTROL_POINT
}
#endif //UPNP_SERVICE_DISCOVERY

OP_STATUS UPnP::CreatePortOpeningObject(UPnP *upnp, UPnPPortOpening *&logic, int start_port, int end_port)
{
	OP_ASSERT(upnp);
	
	if(!upnp)
		return OpStatus::ERR_NULL_POINTER;
		
	logic=OP_NEW(UPnPPortOpening, (upnp, start_port, end_port, TRUE));
	
	if(!logic)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS ops=upnp->AddLogic(logic, FALSE, FALSE);
	
	if(OpStatus::IsError(ops))
	{
		OP_DELETE(logic);
		
		logic=NULL;
	}
	
	return ops;
}

UPnP::~UPnP()
{
	OP_DELETE(saUDPDiscovery);
	#if defined(UPNP_SERVICE_DISCOVERY) && defined(ADD_PASSIVE_CONTROL_POINT)
		OP_DELETE(cp_unite);
	#endif
}

#ifdef _DEBUG
OP_STATUS UPnP::DebugDuplicateDevice(UINT dev_index, UINT num_duplicates, BOOL add_itself)
{
	OP_ASSERT(dev_index<known_devices.GetCount());
	
	if(dev_index>=known_devices.GetCount())
		return OpStatus::ERR_OUT_OF_RANGE;
		
	UPnPDevice *curDev=known_devices.GetPointer(dev_index);
	
	OP_ASSERT(curDev);
	
	if(!curDev)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	if(!curDev->GetUSN())
		return OpStatus::ERR_NULL_POINTER;
		
	// Duplciate the device
	for(UINT i=0; i<num_duplicates; i++)
	{
		UPnPDevice *dev=OP_NEW(UPnPDevice, (curDev->GetNetworkCard()));
		
		if(!dev)
			return OpStatus::ERR_NO_MEMORY;
			
		dev->SetUSN(curDev->GetUSN()->CStr());
		
		OP_STATUS ops=known_devices.AddReference(dev->GetReferenceObjectPtrKnownDevices());
		
		if(OpStatus::IsError(ops))
		{
			OP_DELETE(dev);
			
			return ops;
		}
	}
	
	if(add_itself)
		return known_devices.AddReference(curDev->GetReferenceObjectPtrKnownDevices());
		
	return OpStatus::OK;
}
#endif


OP_STATUS UPnP::DeleteDeviceDuplicates(UPnPDevice *keep)
{
	if(known_devices.GetCount()<2) // Avoid infinite loops... and does not waste CPU
		return OpStatus::OK;
		
	UINT index=0;
		
	/// Set the delete status
	for(index=known_devices.GetCount(); index>0; index--)
	{
		UPnPDevice *dev=known_devices.GetPointer(index-1);
		
		OP_ASSERT(!dev->IsToBeDeleted());
		
		dev->SetToBeDeleted(FALSE);
	}
		
	// Mark the devices that need to be deleted
	for(UINT len=known_devices.GetCount(), old_index=0; old_index<len-1; old_index++)
	{
		UPnPDevice *oldDev=known_devices.GetPointer(old_index);
		//UPnPDevice *newDev=NULL;
		
		if(oldDev && !oldDev->IsToBeDeleted())
		{
			for(UINT cur_index=old_index+1; cur_index<len; cur_index++)
			{
				UPnPDevice *curDev=known_devices.GetPointer(cur_index);
				
				// Preserve the new (last one), delete the old, if not to keep
				if(oldDev && curDev && curDev->IsSameDevice(oldDev))
				{
					if(keep==oldDev)
						curDev->SetToBeDeleted(TRUE);
					else
					{
						oldDev->SetToBeDeleted(TRUE);
						break;
					}
				}
			}
		}
	}

	// Delete the marked devices
	for(index=known_devices.GetCount(); index>0; index--)
	{
		UPnPDevice *dev=known_devices.GetPointer(index-1);
		if(dev && dev->IsToBeDeleted())
			OP_DELETE(dev);
	}

	return OpStatus::OK;
}

void UPnP::NotifyControlPointsOfDevice(UPnPDevice *device, BOOL new_device)
{
	//OP_ASSERT(device && device->GetProvider());
	
	if(!device || !device->GetProvider())
		return;
		
	OpStatus::Ignore(DeleteDeviceDuplicates(device));
	
	NameValue *nv=device->GetProvider()->GetAttribute(UPnPServicesProvider::DEVICE_TYPE);

	OP_ASSERT(nv);
	
	if(!nv || !nv->GetValue().HasContent())
		return;
		
	OpString8 search_target;
	
	RETURN_VOID_IF_ERROR(search_target.Set(nv->GetValue().CStr()));
		
	for(UINT32 i=0; i<dev_lsn.logics.GetCount(); i++)
	{
		UPnPLogic *logic=dev_lsn.logics.GetPointer(i);
		
		if(logic && logic->AcceptSearchTarget(search_target))
		{	
			if(new_device)
				logic->OnNewDevice(device);
			else
				logic->OnRemoveDevice(device);
		}
	}
}

OP_STATUS UPnP::Encrypt(const OpStringC8 &nonce, const OpStringC8 &private_key, OpString8 &output)
{
	OpAutoPtr<CryptoHash> hash_A1(CryptoHash::CreateMD5());
	if (!hash_A1.get())
		return OpStatus::ERR_NO_MEMORY;

	hash_A1->InitHash();
	OP_ASSERT(private_key.HasContent());

	hash_A1->CalculateHash(private_key.CStr());
	hash_A1->CalculateHash(nonce.CStr());

	const int len = hash_A1->Size() * 2 + 1;
	char *buffer = output.Reserve(len);

	if (buffer == NULL)
	{
		return OpStatus::ERR_NO_MEMORY;
	}
	
	const char hexchars[] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	
	byte digest[16]; /* ARRAY OK 2008-12-12 haavardm */
	OP_ASSERT(hash_A1->Size() == 16);
	
	hash_A1->ExtractHash(digest);
	
	int length = hash_A1->Size(); 
	char *pos = buffer;
	int i;
	for(i = 0; i < length; i++)
	{
		unsigned char val = digest[i];
		*(pos++) = hexchars[(val>>4) & 0x0f];
		*(pos++) = hexchars[val & 0x0f];
	}
	
	*(pos++) = '\0';

	return OpStatus::OK;
}

OP_STATUS UPnP::CheckExpires()
{
	double now=g_op_time_info->GetWallClockMS();
	
	for(UINT32 i=known_devices.GetCount(); i>0; i--)
	{
		UPnPDevice *dev=known_devices.GetPointer(i-1);
		
		OP_ASSERT(dev);
		
		if(dev)
		{
			double expire=dev->GetExpireMoment();
			
			if(expire<=now)
			{
				RETURN_IF_ERROR(RemoveDevice(dev));
				NotifyControlPointsOfDevice(dev, FALSE);
				
				OP_DELETE(dev);
			}
		}
	}
	return OpStatus::OK;
}

OP_STATUS DiscoveredUPnPServicesProvider::AddService(UPnPService *service)
{
	if(!service)
		return OpStatus::ERR_NULL_POINTER;

	const uni_char *new_id=service->GetAttributeValueUni(UPnPService::SERVICE_ID);
			
	// Delete old duplicates
	for(UINT32 i=services.GetCount(); i>0; i--)
	{
		UPnPService *srv=services.GetPointer(i-1);
		
		if(srv)
		{
			OpStringC *id=srv->GetAttributeValue(UPnPService::SERVICE_ID);
			
			if(id && !id->Compare(new_id))
			{
				services.RemoveReference(srv->GetReferenceObjectPtrList());
				
				OP_DELETE(srv);
			}
		}
	}
	
	return services.AddReference(service->GetReferenceObjectPtrList());
}

#endif // UPNP_SUPPORT
