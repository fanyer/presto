/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
 *
 * Copyright (C) 2008-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef UPNP_SUPPORT

#include "modules/upnp/upnp_upnp.h"
#include "modules/upnp/upnp_port_opening.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/stdlib/util/opdate.h"
#include "modules/formats/hdsplit.h"
#include "modules/xmlutils/xmltoken.h"
#include "modules/url/url_sn.h"
#include "modules/url/url_socket_wrapper.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"
#ifdef WEBSERVER_SUPPORT
	#include "modules/webserver/webserver-api.h"
#endif

void UPnP_EventsListener::OnSocketDataReady(OpUdpSocket* socket)
{
	UINT len;
	OpSocketAddress *addr=NULL;  // Temporary socket address
	
	RETURN_VOID_IF_ERROR(OpSocketAddress::Create(&addr));
	
	if(OpStatus::IsError(socket->Receive(buf, UPNP_UDP_BUFFER_SIZE-1, addr, &len)))
	{
		if(logic)
			logic->HandlerFailed(NULL, "Cannot receive from UDP socket");
			
		OP_DELETE(addr);
		
		return;
	}

	// TODO: it could be useful to add support for UDP messages sending the data in more than one message
	if(len>0)
	{
		buf[len]=0;
		
		OP_NEW_DBG("UPnP_EventsListener::OnSocketDataReady", "upnp_trace_debug");
		OP_DBG(("UDP message received - bytes: %d\nMex: %s", len, buf));
		
		// Accept only NOTIFY actions and HTTP responses
		if(len<7 || (op_strnicmp(buf, "Notify", 6) && op_strnicmp(buf, "HTTP/", 5)))
			return;
	}
}

UPnP_DiscoveryListener::UPnP_DiscoveryListener(UPnP *ptr):
xml_parsers(FALSE, TRUE), // The XMLParser (pointer) will delete the UPnPXH_Auto, that will delete the reference
logics(FALSE, TRUE),
last_msearch_address(NULL),
last_msearch_time(0)
#ifdef UPNP_SERVICE_DISCOVERY
, services_providers(FALSE, FALSE)
#endif
{
	upnp=ptr;
}

OP_STATUS UPnP_DiscoveryListener::AddXMLParser(UPnPXH_Auto *xth, XMLParser *parser)
{
	OP_ASSERT(parser && xth && xth->GetReferenceObjectPtr() && !xth->GetReferenceObjectPtr()->GetPointer());
	
	if(!parser || !xth)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(xml_parsers.GetCount()<100);  // It is quite strange to have more than 100 parsers on flight...
	                                        // even if in somecrowded situations it could even happen, it is probably 
	                                        // better to stop here. Even because most likely is a different problem
	
	if(xml_parsers.GetCount()>=100)
		return OpStatus::ERR_NOT_SUPPORTED;
		
#ifdef _DEBUG
	OP_NEW_DBG("UPnP_DiscoveryListener::AddXMLParser", "upnp_trace_debug");
	OpString str;
	
	str.Set(xth->GetDescription());

	OP_DBG((UNI_L("*** ADD XML Parser: %p for %s"), parser, str.CStr()));
#endif

	//ReferenceObject<XMLParser>* ref=OP_NEW(ReferenceObject<XMLParser>, (parser));
	ReferenceObject<XMLParser>* ref=xth->GetReferenceObjectPtr();
	
	if(!ref)
		return OpStatus::ERR_NO_MEMORY;
		
	ref->SetPointer(parser);  // Parser deleted ==> Element removed from the list
	
	return xml_parsers.AddReference(ref);
}

OP_STATUS UPnP_DiscoveryListener::RemoveXMLParser(XMLParser *parser)
{
	ReferenceObject<XMLParser>* ref=xml_parsers.GetReferenceByPointer(parser);
	
	OP_ASSERT(ref);
	
	if(!ref)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
	
	BOOL b=xml_parsers.RemoveReference(ref);
	
	OP_ASSERT(b);
	
	if(!b)
		return OpStatus::ERR_NO_SUCH_RESOURCE;
		
	OP_NEW_DBG("UPnP_DiscoveryListener::RemoveXMLParser", "upnp_trace_debug");
	
	OP_DBG((UNI_L("*** Removed XML Parser: %p"), parser));
		
	return OpStatus::OK;
}

OP_STATUS UPnP_DiscoveryListener::AddLogic(UPnPLogic *logic, BOOL notify_devices, BOOL broadcast_search)
{
	OP_ASSERT(logic && logic->upnp && upnp && logic->upnp==upnp);
	
	if(!logic || !upnp)
		return OpStatus::ERR_NULL_POINTER;
		
	RemoveLogic(logic);
	
	OP_STATUS ops=logics.AddReference(logic->GetReferenceObjectPtr());
	
	if(broadcast_search)
		upnp->BroadcastUDPDiscoveryForSingleLogic(logic);
		
	OpStatus::Ignore(upnp->DeleteDeviceDuplicates(NULL));
	
	if(notify_devices && OpStatus::IsSuccess(ops))
	{
		for(int i=0, num=upnp->GetNumberOfDevices(); i<num; i++)
		{
			UPnPDevice *dev=upnp->GetDevice(i);
			SearchTargetMode mode;
			
			if(dev && dev->GetProvider() && dev->GetProvider()->AcceptSearch(logic->GetSearchTarget(), mode))
			{
				OP_ASSERT(mode==SearchDeviceType);

				logic->OnNewDevice(dev);
			}
		}
	}
	
	return ops;
}

void UPnP_DiscoveryListener::RemoveLogic(UPnPLogic *logic)
{
	if(!logic || !logics.GetCount())
		return;
		
	logics.RemoveReference(logic->GetReferenceObjectPtr());
}

#ifdef UPNP_SERVICE_DISCOVERY
UPnPServicesProvider *UPnP_DiscoveryListener::GetServicesProviderFromLocation(uni_char *location)
{
	for(UINT32 i=services_providers.GetCount(); i>0; i--)
	{
		UPnPServicesProvider *prov=services_providers.GetPointer(i-1);

		if(prov && (!prov->GetFullDescriptionURL().Compare(location) || !prov->GetDescriptionURL().Compare(location)))
			return prov;
	}
	return NULL;
}

UPnPServicesProvider *UPnP_DiscoveryListener::GetServicesProviderFromDeviceString(const char *devicestring)
{
	OP_ASSERT(devicestring);
	if(devicestring)
		return NULL;

	for(UINT32 i=services_providers.GetCount(); i>0; i--)
	{
		UPnPServicesProvider *prov=services_providers.GetPointer(i-1);

		if(prov && !op_strcmp(prov->GetDeviceTypeString(), devicestring))
			return prov;
	}
	return NULL;
}

#endif
	
OP_STATUS UPnP_DiscoveryListener::HandlerFailed(UPnPXH_Auto *child, const char *mex)
{
	OP_STATUS ops=OpStatus::OK;
	
	for(UINT32 i=0; i<logics.GetCount(); i++)
	{
		OP_STATUS ops2;
		
		UPnPLogic *logic=logics.GetPointer(i);
		
		if(child) // A Child is available: notify the interested devices
		{
			if(logic && logic->AcceptDevice(child->GetDevice())) // Error during some phase
			{
				if(OpStatus::IsError(ops2=logic->HandlerFailed(child, mex)))
					ops=ops2;
			}
		}
		else	// A Child is not available: notify every logic that does not have a device
		{
			if(logic)
			{
				int num=0;
				BOOL locked=logic->locked;
				
				logic->locked=FALSE;
				
				for(int j=0, len=upnp->GetNumberOfDevices(); j<len && !num; j++)
				{
					if(logic->AcceptDevice(upnp->GetDevice(j)))
						num++;
				}
				
				logic->locked=locked;
				
				// Notify the logic handlers
				if(!num && OpStatus::IsError(ops2=logic->HandlerFailed(child, mex)))
					ops=ops2;
			}
		}
	}
	
	if(upnp->GetSelfDelete())
	{
		OP_DELETE(upnp);
	}
	
	return ops;
}


OP_STATUS UPnP_DiscoveryListener::ManageDiscoveryAnswer(OpSocketAddress *legit_address, OpUdpSocket *socket, const char *location, const char *unique_service_url, UDPReason reason, const char *target, const char *target2, BOOL bypass_anti_flood)
{
	OP_NEW_DBG("UPn::ManageDiscoveryAnswer", "upnp_trace_debug");
	URL url=g_url_api->GetURL(location);

	OpString tt;
	const uni_char *path=NULL;

	if(OpStatus::IsSuccess(url.GetAttribute(URL::KUniPathAndQuery, 0, tt, URL::KFollowRedirect)))
		path=tt.CStr();

	ServerName *sn=(ServerName *)url.GetAttribute(URL::KServerName, NULL);;
	unsigned short port=url.GetServerPort();

	OP_DBG((UNI_L("\nServer: %s\nPort: %d\nPath: %s"), (sn)?sn->UniName():UNI_L(""), port, path));

	if(!port)
		port=80;

	if(path && sn && port)
	{
		OpString url;

		if(op_strncmp(location, "http://", 7))
		  url.Append("http://");

		url.Append(location);
		OpString ch;
		
		if(	(target && !op_strcmp(target, UPNP_DISCOVERY_OPERA_SEARCH_TYPE)) ||	//don't use the challenge for other devices
			(target2 && !op_strcmp(target2, UPNP_DISCOVERY_OPERA_SEARCH_TYPE))
			)
		{
			RETURN_IF_ERROR(ch.AppendFormat(UNI_L("%u%u"), (UINT32)op_rand(), (UINT32)g_op_time_info->GetWallClockMS()));
			
			// Add the challenge  (QueryDescription is a GET, not a POST)
			if(url.FindLastOf('?')!=KNotFound)
				RETURN_IF_ERROR(url.AppendFormat(UNI_L("&challenge=%s"), ch.CStr()));
			else
				RETURN_IF_ERROR(url.AppendFormat(UNI_L("?challenge=%s"), ch.CStr()));
		}

		//UPnPDevice *device=upnp->GetDeviceFromSocket(socket);
		// TODO: fix the logic: if nobody add the device, it will not work... device will not change from when the caller invoked the method
		UPnPDevice *device=upnp->GetDeviceFromUSN(unique_service_url, location);

		if(device)
		{
			// Save the last address (it will be used for byebye check); no error check, as this operation could be repeated and it is not that important... more or less...
			if(device)
			{
				if(device->known_address || OpStatus::IsSuccess(OpSocketAddress::Create(&device->known_address)))
					OpStatus::Ignore(device->known_address->Copy(legit_address));
			}
			
			// Look for a logic interested in this kind of devices
			BOOL accepted=FALSE;
			
			for(UINT32 i=0; !accepted && i<logics.GetCount(); i++)
			{
				UPnPLogic *logic=logics.GetPointer(i);
				
				if(logic)
				{ 
					if(target2)
					{
						if(logic->AcceptSearchTarget(target2))
							accepted=TRUE;
					}
					else if(logic->AcceptSearchTarget(target))
						accepted=TRUE;
				}
			}
			
			if(!accepted)  // Uninteresting device
				return OpStatus::OK;
			
			device->SetReason(reason);
			return QueryDescription(device, url, ch, bypass_anti_flood);
		}
		else
		{
			OP_DBG((UNI_L("\nSocket non found in the devices socket list")));
		}
	}

	return OpStatus::ERR;
}

OP_STATUS UPnP_DiscoveryListener::QueryDescription(UPnPDevice *device, OpString &device_string, OpString &ch, BOOL bypass_anti_flood)
{
	OP_NEW_DBG("UPnP::QueryDescription", "upnp_trace_debug");
	
	if(!device)
		return OpStatus::ERR_NULL_POINTER;

	if(bypass_anti_flood || device->GetCurrentDescriptionTime()<g_op_time_info->GetWallClockMS()-UPNP_NOTIFY_THRESHOLD_MS)
	{
		OP_DBG((UNI_L("*** Description URL: %s"), device_string.CStr()));

 		device->SetCurrentDescriptionURL(device_string, TRUE);
		upnp->StopDeviceSearch(FALSE);

		UPnPXH_Auto *ptr=OP_NEW(UPnPXH_DescriptionGeneric, ());
		
		if(!ptr)
			return OpStatus::ERR_NO_MEMORY;
			
		RETURN_IF_ERROR(static_cast<UPnPXH_DescriptionGeneric *>(ptr)->SetChallenge(ch));
		
		return upnp->StartXMLProcessing(device, &device_string, ptr, NULL, FALSE, NULL);
	}
	else
	{
		OP_DBG((UNI_L("*** Description on %s skipped because it was done only several seconds ago"), device_string.CStr()));

		return OpStatus::OK;
	}
}


void UPnP_DiscoveryListener::ManageAlive(OpSocketAddress *legit_address, OpUdpSocket *socket, const char *location, const char *unique_service_url, const char *target, const char *target2)
{
	// Same device, only try to reopen the port (but only if the last openenig attempt was at least UPNP_NOTIFY_THRESHOLD_MS ms old)
	//UPnPDevice *device=upnp->GetDeviceFromSocket(socket);
	UPnPDevice *device=upnp->GetDeviceFromUSN(unique_service_url, location);
	BOOL same_location=FALSE;
	
	if(device)
	{
		same_location=!device->GetCurrentDescriptionURL()->Compare(location);
		
		if(!same_location)  // Skip parameters in the URL (primary tought for challenge...)
		{
			 int len_desc=device->GetCurrentDescriptionURL()->Length();
			 int len_loc=op_strlen(location);
			 
			 if(len_desc>len_loc && device->GetCurrentDescriptionURL()->CStr()[len_loc]=='?')
				same_location=!device->GetCurrentDescriptionURL()->Compare(location, len_loc);
		}
	}
		

	if(!device || (!same_location))
	{
	  ManageDiscoveryAnswer(legit_address, socket, location, unique_service_url, UDP_REASON_SSDP_ALIVE_LOCATION, target, target2, TRUE);
	}
	else if(device->GetCurrentDescriptionTime()<g_op_time_info->GetWallClockMS()-UPNP_NOTIFY_THRESHOLD_MS)
	  ManageDiscoveryAnswer(legit_address, socket, location, unique_service_url, UDP_REASON_SSDP_ALIVE_THRESHOLD, target, target2, TRUE);
	else
	{
		OP_NEW_DBG("UPnPLogic::ManageAlive", "upnp_trace_debug");
		OP_DBG(("*** IGD Notify skipped: too young\n"));
	}
}

void UPnP_DiscoveryListener::ManageByeBye(const char *location, const char *unique_service_url)
{
	UPnPDevice *device=upnp->GetDeviceFromUSN(unique_service_url, location);
	
	if(device)
	{
		upnp->RemoveDevice(device);
		upnp->NotifyControlPointsOfDevice(device, FALSE);
		
		OP_DELETE(device);
	}
}


void UPnP_DiscoveryListener::dummy()
{
}

#ifdef UPNP_ANTI_SPOOF
void UPnP_DiscoveryListener::AliveSpoofListener::OnAddressSpoofed()
{ 
#ifdef UPNP_LOG
	if(upnp)
	{
		OpString temp;
		OpString location16;
		
		OpStatus::Ignore(location16.Set(location.CStr()));
		OpStatus::Ignore(temp.AppendFormat(UNI_L("Spoofing attempt detected (1st check): %s is not from %s"), location16.CStr(), addr.CStr()));
		
		upnp->Log(UNI_L("AliveSpoofListener::OnAddressSpoofed()"), temp.CStr());
	}
#endif
	
	OP_DELETE(this);
}

void UPnP_DiscoveryListener::AliveSpoofListener::OnAddressLegit(OpSocketAddress *legit_address)
{
	NetworkCard *nic=upnp->GetNetworkCardFromSocket(socket);
	
	if(spoof_reason==UDP_REASON_SSDP_BYEBYE)
		upnp->GetDiscoveryListener().ManageByeBye(location.CStr(), usn.CStr());
	else
	{
		OpString location16;
		OpString usn16;
		
		RETURN_VOID_IF_ERROR(location16.Set(location.CStr()));
		
	#ifdef UPNP_SERVICE_DISCOVERY
		// If it is an (internal) Service Provider, it should not appear on the list of devices, unlesss IsVisibleToOwner is set
		UPnPServicesProvider* prov = upnp->GetDiscoveryListener().GetServicesProviderFromDeviceString(usn.CStr());
		if(prov && !prov->IsVisibleToOwner())
		{
			OP_DELETE(this);
			
			return;
		}
	#endif
			
		RETURN_VOID_IF_ERROR(usn16.Set(usn.CStr()));
		RETURN_VOID_IF_ERROR(upnp->AddNewDevice(nic, location16.CStr(), usn16.CStr(), expire_period));
		
		if(spoof_reason==UDP_REASON_DISCOVERY)
			upnp->GetDiscoveryListener().ManageDiscoveryAnswer(legit_address, socket, location.CStr(), usn.CStr(), UDP_REASON_DISCOVERY, target.CStr(), target2.CStr(), FALSE);
		else if(spoof_reason==UDP_REASON_SSDP_ALIVE_LOCATION || spoof_reason==UDP_REASON_SSDP_ALIVE_THRESHOLD)
			upnp->GetDiscoveryListener().ManageAlive(legit_address, socket, location.CStr(), usn.CStr(), target.CStr(), target2.CStr());
	}
	
	OP_DELETE(this);
}

void UPnP_DiscoveryListener::AliveSpoofListener::OnError(OP_STATUS error)
{
#ifdef UPNP_LOG
	if(upnp)
	{
		OpString temp;
		
		temp.AppendFormat(UNI_L("Error during 1st spoofing check: %d"), error);
		
		upnp->Log(UNI_L("AliveSpoofListener::OnError()"), temp.CStr());
	}
#endif
	
	OP_DELETE(this);
}
#endif // UPNP_ANTI_SPOOF

void UPnP_DiscoveryListener::OnSocketDataReady(OpUdpSocket* socket)
{
	// No Control Points and no Service Providers, no fun...
	/*if(!logics.GetCount()
	#ifdef UPNP_SERVICE_DISCOVERY
		&& !services_providers.GetCount() 
	#endif
	)
		return;*/
		
	UINT len;
	OpSocketAddress *addr=NULL;  // Temporary socket address
	
	RETURN_VOID_IF_ERROR(OpSocketAddress::Create(&addr));
	
	if(OpStatus::IsError(socket->Receive((void *)buf, UPNP_UDP_BUFFER_SIZE-1, addr, &len)))
	{
		// FIX-ME: error management!
		//NotifyFailure("Cannot receive from UDP socket");
		OP_DELETE(addr);
		
		return;
	}

	// TODO: it could be useful to add support for UDP messages sending the data in more than one message
	if(len>7)
	{
		buf[len]=0;
		
		#ifdef _DEBUG
		  OpString8 str_mex;
		  
		  str_mex.Set(buf);
		#endif
		
		// Parse HTTP messages
		HeaderList hl;
		
		if(OpStatus::IsError(hl.SetValue(buf, NVS_HAS_RFC2231_VALUES | NVS_BORROW_CONTENT)))
		{
			// FIX-ME: error management!
			OP_DELETE(addr);
			
			return;
		}
		
	#ifdef _DEBUG
		OpString str_add;
		OpString8 str_add8;
		
		addr->ToString(&str_add);
		str_add8.Set(str_add.CStr());
		HeaderEntry *location=hl.GetHeader("LOCATION");
		OP_NEW_DBG("UPnP_DiscoveryListener::OnSocketDataReady", "upnp_trace_debug");
		OP_DBG(("UDP message received - bytes: %d\nMex: %s from %s claiming to be %s", len, (len>1000)?buf:str_mex.CStr(), str_add8.CStr(), location?location->Value():"?"));
	#endif
		
		if(!op_strnicmp(buf, "Notify", 6)) // Multicast advertisement
			ManageNotify(len, hl, socket, addr);
		else if(!op_strnicmp(buf, "HTTP/", 5)) // Unicast answer to M-SEARCH
			ManageHTTPAnswer(len, hl, socket, addr);
#ifdef UPNP_SERVICE_DISCOVERY
		else if(!op_strnicmp(buf, "M-SEARCH", 7)) // Multicast Search request
			ManageMSearch(len, hl, socket, addr);
#endif
	}
	
	OP_DELETE(addr);
}

#ifdef UPNP_SERVICE_DISCOVERY
OP_STATUS UPnP_DiscoveryListener::AddServicesProvider(UPnPServicesProvider *upnp_services_provider, BOOL advertise)
{
	if(!upnp_services_provider)
		return OpStatus::ERR_NULL_POINTER;
		
	int idx=services_providers.FindReference(&upnp_services_provider->GetReferenceObject());
	
	if(idx>=0) // Already Found: no activity
		return OpStatus::OK;
	
	RETURN_IF_ERROR(services_providers.AddReference(&upnp_services_provider->GetReferenceObject()));
	
	if(advertise)
		upnp->AdvertiseServicesProvider(upnp_services_provider, TRUE, TRUE);
		
	// Notify all the control points of the new Service Provider
	/*for(int i=0, num=logics.GetCount(); i<num; i++)
	{
		UPnPLogic *logic=logics.GetPointer(i);
		
		if(logic && upnp_services_provider->AcceptSearch(logic->GetSearchTarget()))
			logic->NotifyServicesProvider(upnp_services_provider);
	}*/
		
	return OpStatus::OK;
}

OP_STATUS UPnP_DiscoveryListener::RemoveServicesProvider(UPnPServicesProvider *upnp_services_provider)
{
	if(!upnp_services_provider)
		return OpStatus::OK;
		
	if(!services_providers.GetCount())
		return OpStatus::OK;
		
	OP_STATUS ops=services_providers.RemoveReference(&upnp_services_provider->GetReferenceObject());
	
	// Notify all the control points that the Service Provider has been removed
	/*for(int i=0, num=logics.GetCount(); i<num; i++)
	{
		UPnPLogic *logic=logics.GetPointer(i);
		
		if(logic && upnp_services_provider->AcceptSearch(logic->GetSearchTarget()))
			logic->NotifyServicesProviderRemoved(upnp_services_provider);
	}*/
	
	return ops;
}

void UPnP_DiscoveryListener::ManageMSearch(UINT len, HeaderList &hl, OpUdpSocket *socket, OpSocketAddress *addr)
{
	HeaderEntry *host=hl.GetHeader("HOST");
	HeaderEntry *search_target=hl.GetHeader("ST");
	HeaderEntry *mx=hl.GetHeader("MX");
	
	if(!host || !search_target|| !mx)
	{
#ifdef UPNP_LOG
		upnp->Log(UNI_L("UPnP_DiscoveryListener::ManageMSearch()"), UNI_L("ERROR: Missing headers in UPnP response"));
#endif

		return;
	}
	
	for(UINT32 i=0; i<services_providers.GetCount(); i++)
	{
		UPnPServicesProvider *serv_prov=services_providers.GetPointer(i);
		OpString8 value;
		SearchTargetMode mode;
		
		OpStatus::Ignore(value.Set(search_target->Value()));
		
		if(serv_prov && !serv_prov->IsDiscovered() && serv_prov->AcceptSearch(value, mode))
		{
			serv_prov->OnSearchRequestAccepted(value, addr);
			
			OP_NEW_DBG("UPnP_DiscoveryListener::ManageMSearch", "upnp_trace");
			OP_DBG(("\n Host: %s\nSearch Target: %s\nMX: %s", host->Value(), value.CStr(), mx->Value()));
			//
			// FIXME: add MX support to wait some seconds
			//
			// int mx_int=3;
			// if(mx->Value() && op_strlen(mx->Value())<3)
			// 	 mx_int=op_atoi(mx->Value());

			OpStatus::Ignore(SendUnicastHTTPAnswerToMSearch(serv_prov, socket, addr, value.CStr(), mode));
			//serv_prov->ManageMSearch(socket, value);
		}
	}
}

OP_STATUS UPnP_DiscoveryListener::SendUnicastHTTPAnswerToMSearch(UPnPServicesProvider *serv_prov, OpUdpSocket *socket, OpSocketAddress *addr, const char *search_target, SearchTargetMode mode)
{
	OP_ASSERT(mode!=SearchNoMatch);
	
	// TODO: this algorythm should be improved, keeping a list of the last addresses and the last time of the M-SEARCH
	// But it is still better than nothing  :-)
	
	// Check if the Control Point has been notified just some seconds ago
	if(!last_msearch_address)
		OpStatus::Ignore(OpSocketAddress::Create(&last_msearch_address));
		
	if(last_msearch_address && last_msearch_address->IsEqual(addr) && last_msearch_time>=g_op_time_info->GetWallClockMS()-UPNP_MSEARCH_THRESHOLD_MS)
	{
	#ifdef _DEBUG
		OpString addr_str;
		
		addr->ToString(&addr_str);
		
		OP_NEW_DBG("UPnP_DiscoveryListener::SendUnicastHTTPAnswerToMSearch", "upnp_trace");
		OP_DBG(("\n Skipping M-Search request from %s:%d", addr_str.CStr(), addr->Port()));
	#endif
			
		last_msearch_time=g_op_time_info->GetWallClockMS();  // "chain" time, to skip them also on devices that wait a bit between each send
		
		return OpStatus::OK;
	}
	
	#ifdef _DEBUG
		OpString addr_str;
		
		addr->ToString(&addr_str);
		
		OP_NEW_DBG("UPnP_DiscoveryListener::SendUnicastHTTPAnswerToMSearch", "upnp_trace");
		OP_DBG(("\n Accepted M-Search request from %s:%d", addr_str.CStr(), addr->Port()));
	#endif
	
	if(last_msearch_address)
		OpStatus::Ignore(last_msearch_address->Copy(addr));
		
	last_msearch_time=g_op_time_info->GetWallClockMS();
	
	OpString str;
	OpString server;
	OpString src_trg;
	
	// Prepare the HTTP 200 OK answer
	NetworkCard *nc=upnp->GetNetworkCardFromSocket(socket);
	
	if(nc && nc->GetAddress())
		RETURN_IF_ERROR(serv_prov->UpdateDescriptionURL(nc->GetAddress()));
	
	RETURN_IF_ERROR(server.Set(g_op_system_info->GetPlatformStr()));
	RETURN_IF_ERROR(src_trg.Set(search_target));
	
	RETURN_IF_ERROR(str.Set("HTTP/1.1 200 OK\r\nCache-Control: max-age=900\r\nExt:\r\n"));
	RETURN_IF_ERROR(str.AppendFormat(UNI_L("Location: %s\r\nServer: %s\r\nST: %s\r\n"), serv_prov->GetFullDescriptionURL().CStr(), server.CStr(), src_trg.CStr()));
	
	NameValue *udn=serv_prov->GetAttribute(UPnPServicesProvider::UDN);
	NameValue *device_type=serv_prov->GetAttribute(UPnPServicesProvider::DEVICE_TYPE);
	
	const uni_char *udn_str=(udn && udn->GetValue().HasContent())?udn->GetValue().CStr():UNI_L("uuid:opera");
	const uni_char *device_type_str=(device_type && device_type->GetValue().HasContent())?device_type->GetValue().CStr():UNI_L("urn:unknown:device:unknown:0");
		
	if(mode==SearchRootDevice)
		RETURN_IF_ERROR(str.AppendFormat(UNI_L("USN: %s::upnp:rootdevice\r\n"), udn_str));
	else if(mode==SearchDeviceType)
		RETURN_IF_ERROR(str.AppendFormat(UNI_L("USN: %s::%s\r\n"), udn_str, device_type_str));
	else if(mode==SearchUUID)
		RETURN_IF_ERROR(str.AppendFormat(UNI_L("USN: %s\r\n"), udn_str, src_trg.CStr()));
		
	str.AppendFormat(UNI_L("Content-Length: 0\r\n\r\n"));
	
	OpString8 str8;
	
	// FIXME: wait for some seconds
	RETURN_IF_ERROR(str8.Set(str.CStr()));
	OP_STATUS ops1, ops2, ops3;
	
	// This is nice to bypass UDP firewall, but can create problems (Multicast socket that send a unicast message)
	// TODO: wait 500 ms between the 3 sends can be a good idea...
	ops1=socket->Send(str8.CStr(), str8.Length(), addr, OpUdpSocket::UNICAST);
	ops2=socket->Send(str8.CStr(), str8.Length(), addr, OpUdpSocket::UNICAST);
	ops3=socket->Send(str8.CStr(), str8.Length(), addr, OpUdpSocket::UNICAST);
	
	// This is from an Unicast socket. A possibility is to do it only if there is an error in the previous send, but it is probably
	// safer to just send them always (in case the platform is not able to detect the problem), even if this means more UDP traffic
	OpUdpSocket *sk_unicast=(nc)?nc->GetSocketUnicast():NULL;
		
	if(sk_unicast)
	{
		OpStatus::Ignore(sk_unicast->Send(str8.CStr(), str8.Length(), addr, OpUdpSocket::UNICAST));
		OpStatus::Ignore(sk_unicast->Send(str8.CStr(), str8.Length(), addr, OpUdpSocket::UNICAST));
		OpStatus::Ignore(sk_unicast->Send(str8.CStr(), str8.Length(), addr, OpUdpSocket::UNICAST));
	}
	
	if(OpStatus::IsError(ops1) || OpStatus::IsError(ops2) || OpStatus::IsError(ops3))
	{
		if( ops1==OpNetworkStatus::ERR_SOCKET_BLOCKING ||
			ops2==OpNetworkStatus::ERR_SOCKET_BLOCKING ||
			ops3==OpNetworkStatus::ERR_SOCKET_BLOCKING)
		{
			return OpNetworkStatus::ERR_SOCKET_BLOCKING;
		}
		
		return OpStatus::ERR;
	}
	
	return OpStatus::OK;
}
#endif // UPNP_SERVICE_DISCOVERY

void UPnP_DiscoveryListener::ManageHTTPAnswer(UINT len, HeaderList &hl, OpUdpSocket *socket, OpSocketAddress *addr)
{
	HeaderEntry *location=hl.GetHeader("LOCATION");
	HeaderEntry *search_target=hl.GetHeader("ST");
	HeaderEntry *unique_service_url=hl.GetHeader("USN");
	HeaderEntry *expire=hl.GetHeader("CACHE-CONTROL");
	
	if(!location || !search_target|| !unique_service_url)
	{
#ifdef UPNP_LOG
		upnp->Log(UNI_L("UPnP_DiscoveryListener::ManageHTTPAnswer()"), UNI_L("ERROR: Missing headers in UPnP response"));
#endif

		return;
	}
	
	OpString8 loc_val;
	OpString8 us_val;
	UINT32 expire_val=1800;
	
	OpStatus::Ignore(loc_val.Set(location->Value()));
	OpStatus::Ignore(us_val.Set(unique_service_url->Value()));
	
	if(expire)
		expire_val=UPnP_DiscoveryListener::ReadExpire(expire->Value());
		
	// First defense: Anti Spoof
	OpString8 st_val;
	OpString8 nt_val2; // empty
	
	OpStatus::Ignore(st_val.Set(search_target->Value()));
	
#ifdef UPNP_LOG
	OpString8 log_mex8;
	OpString log_mex;
	
	OpStatus::Ignore(log_mex8.AppendFormat("HTTP 200 from %s - %s [%s]", unique_service_url->Value(), location->Value(), search_target->Value()));
	OpStatus::Ignore(log_mex.Set(log_mex8.CStr()));
	
	upnp->Log(UNI_L("UPnP_DiscoveryListener::ManageHTTPAnswer()"), log_mex.CStr());
#endif



#ifdef UPNP_ANTI_SPOOF
	ManageSpoofing(socket, addr, loc_val, us_val, st_val, nt_val2, UDP_REASON_DISCOVERY, expire_val);
#else
	NetworkCard *nic=upnp->GetNetworkCardFromSocket(socket);
	OpString location16;
	OpString usn16;
		
	RETURN_VOID_IF_ERROR(location16.Set(loc_val.CStr()));
	
#ifdef UPNP_SERVICE_DISCOVERY
	// If it is an (internal) Service Provider, it should not appear on the list of devices
	if(GetServicesProviderFromLocation(location16.CStr()))
		return;
#endif
			
	RETURN_VOID_IF_ERROR(usn16.Set(us_val.CStr()));
	RETURN_VOID_IF_ERROR(upnp->AddNewDevice(nic, location16.CStr(), usn16.CStr(), expire_val));
	
	ManageDiscoveryAnswer(addr, socket, loc_val.CStr(), us_val.CStr(), UDP_REASON_DISCOVERY, st_val.CStr(), nt_val2.CStr(), FALSE);
	
	/*for(UINT32 i=0; i<logics.GetCount(); i++)
	{
		UPnPLogic *logic=logics.Get(i);
			
		if(logic && logic->AcceptSearchTarget(st_val))
		{
			OP_NEW_DBG("UPnP_DiscoveryListener::ManageHTTPAnswer", "upnp_trace");
			OP_DBG(("\n Location: %s\nSearch Target: %s\nService URL: %s", location->Value(), st_val, unique_service_url->Value()));
			logic->ManageDiscoveryAnswer(addr, socket, loc_val, us_val, TRUE, UDP_REASON_DISCOVERY, FALSE);
		}
	}*/
#endif
}

/// Read a Cache-Control header and retrieve the expire
UINT32 UPnP_DiscoveryListener::ReadExpire(const char *cache_control)
{
	// Cache-Control: max-age=180 ==> 180
	OP_ASSERT(cache_control);

	if(!cache_control)
		return 0;

	const char *start=op_strstr(cache_control, "=");

	if(start)
		return op_atoi(start+1);
		
	return op_atoi(cache_control);
}

void UPnP_DiscoveryListener::ManageNotify(UINT len, HeaderList &hl, OpUdpSocket *socket, OpSocketAddress *addr)
{
	HeaderEntry *location=hl.GetHeader("LOCATION");
	HeaderEntry *unique_service_url=hl.GetHeader("USN");
	HeaderEntry *notification_sub_type=hl.GetHeader("NTS");
	HeaderEntry *notification=hl.GetHeader("NT");
	HeaderEntry *notification2=hl.GetHeader("NT2");  // Opera specific
	HeaderEntry *expire=hl.GetHeader("CACHE-CONTROL");
	OpString8 loc_val;
	OpString8 us_val;
	OpString8 nt_val;
	OpString8 ntst_val;
	OpString8 nt_val2;
	UINT32 expire_val=1800;
	
	if(location)
		OpStatus::Ignore(loc_val.Set(location->Value()));
	if(unique_service_url)
		OpStatus::Ignore(us_val.Set(unique_service_url->Value()));
	if(notification)
		OpStatus::Ignore(nt_val.Set(notification->Value()));
	if(notification2)
		OpStatus::Ignore(nt_val2.Set(notification2->Value()));
	if(notification_sub_type)
		OpStatus::Ignore(ntst_val.Set(notification_sub_type->Value()));
	if(expire)
		expire_val=UPnP_DiscoveryListener::ReadExpire(expire->Value());
	
	if(!notification || !notification_sub_type || !unique_service_url)
	{
#ifdef UPNP_LOG
		upnp->Log(UNI_L("UPnP_DiscoveryListener::ManageNotify()"), UNI_L("ERROR: Missing headers in UPnP response"));
#endif

		return;
	}

#ifdef UPNP_LOG
		OpString8 log_mex8;
		OpString log_mex;
		
		OpStatus::Ignore(log_mex8.AppendFormat("Mex: %s from %s - %s", notification_sub_type->Value(), unique_service_url->Value(), notification->Value()));
		OpStatus::Ignore(log_mex.Set(log_mex8.CStr()));
		
		upnp->Log(UNI_L("UPnP_DiscoveryListener::ManageNotify()"), log_mex.CStr());
#endif

	// First defense: Anti Spoof
#ifdef UPNP_ANTI_SPOOF
	if(!op_stricmp(notification_sub_type->Value(), "ssdp:alive"))
		ManageSpoofing(socket, addr, loc_val, us_val, nt_val, nt_val2, UDP_REASON_SSDP_ALIVE_LOCATION, expire_val);
	else if(!op_stricmp(notification_sub_type->Value(), "ssdp:byebye"))
		ManageSpoofing(socket, addr, loc_val, us_val, nt_val, nt_val2, UDP_REASON_SSDP_BYEBYE, expire_val);
#else
	NetworkCard *nic=upnp->GetNetworkCardFromSocket(socket);
	
	if(!op_stricmp(notification_sub_type->Value(), "ssdp:byebye"))
		RETURN_VOID_IF_ERROR(upnp->RemoveDevice(unique_service_url->Value(), loc_val.CStr()));
	else if(!op_stricmp(notification_sub_type->Value(), "ssdp:alive"))
	{
		OpString location16;
		OpString usn16;
		
		RETURN_VOID_IF_ERROR(location16.Set(loc_val.CStr()));
		
#ifdef UPNP_SERVICE_DISCOVERY
		// If it is an (internal) Service Provider, it should not appear on the list of devices
		if(GetServicesProviderFromLocation(location16.CStr())
			return;
#endif
			
		RETURN_VOID_IF_ERROR(usn16.Set(us_val.CStr()));
		RETURN_VOID_IF_ERROR(upnp->AddNewDevice(nic, location16.CStr(), usn16.CStr(), expire_val));
	}
		//RETURN_VOID_IF_ERROR(upnp->AddNewDevice(nic, path, unique_service_url->Value(), expire_val));
	
	/*for(UINT32 i=0; i<logics.GetCount(); i++)
	{
		UPnPLogic *logic=logics.Get(i);
			
		if(logic && logic->AcceptSearchTarget(nt_val) && (!notification2 || logic->AcceptSearchTarget(nt_val2)))
		{
			if(notification_sub_type && !op_stricmp(notification_sub_type->Value(), "ssdp:alive"))
				logic->ManageAlive(socket, loc_val, us_val);
			else if(notification_sub_type && !op_stricmp(notification_sub_type->Value(), "ssdp:byebye"))
				logic->ManageByeBye(loc_val.CStr(), us_val.CStr());
		}
	}*/
#endif
}

#ifdef UPNP_ANTI_SPOOF
SpoofListener::~SpoofListener()
{
	OP_DELETE(upnp_spoof_lsn);
}

OP_STATUS UPnP_DiscoveryListener::AliveSpoofListener::Construct(UPnP *current_upnp, ReferenceUserList<UPnPLogic> *current_logics, OpUdpSocket *udp_socket, OpSocketAddress *address, const OpStringC8 &loc, const OpStringC8 &dev_usn, const OpStringC8 &search_target, const OpStringC8 &search_target2, UDPReason reason, UINT32 expire)
{
	OP_ASSERT(current_logics && current_upnp && udp_socket && address);
	OP_ASSERT(reason==UDP_REASON_DISCOVERY || reason==UDP_REASON_SSDP_ALIVE_LOCATION || reason==UDP_REASON_SSDP_BYEBYE);

	if(!current_logics || !current_upnp || !udp_socket || !address)
		return OpStatus::ERR_NULL_POINTER;

	if(!upnp_spoof_lsn)
	{
		UPnPSpoofCheckOHRListener *spoof_listener=OP_NEW(UPnPSpoofCheckOHRListener, ());
			
		if(!spoof_listener)
			return OpStatus::ERR_NO_MEMORY;
				
		spoof_listener->SetData(this, address);
		upnp_spoof_lsn=spoof_listener;
	}
	spoof_reason=reason;
	upnp=current_upnp;
	logics=current_logics;
	socket=udp_socket;
	expire_period=expire;
	RETURN_IF_ERROR(target.Set(search_target));
	RETURN_IF_ERROR(target2.Set(search_target2));

	RETURN_IF_ERROR(location.Set(loc));
	RETURN_IF_ERROR(address->ToString(&addr));
	RETURN_IF_ERROR(usn.Set(dev_usn));
	RETURN_IF_ERROR(SocketWrapper::CreateHostResolver(&ohr_spoof, upnp_spoof_lsn));
	
	// FIXME: This is a vulnerability: everyone can send an ssdp:byebye, claiming to be Opera
	// The network card should be checked
	if(spoof_reason==UDP_REASON_SSDP_BYEBYE)
	{
		UPnPDevice *dev=upnp->GetDeviceFromUSN(usn.CStr(), NULL);
		
		if(dev && dev->known_address && dev->known_address->IsEqual(address))
		{
			OnAddressLegit(address);
		
			return OpStatus::OK;
		}
		
		return OpStatus::ERR_OUT_OF_RANGE;
	}

	return upnp->CheckIfAddressIsSpoofed(location.CStr(), address, this);
}


OP_STATUS UPnP_DiscoveryListener::ManageSpoofing(OpUdpSocket *socket, OpSocketAddress *addr, OpStringC8 location, OpStringC8 unique_service_url, OpStringC8 search_target, OpStringC8 search_target2, UDPReason reason, UINT32 expire_val)
{
	/*if(!logics.GetCount()) // No logic, no fun...
		return OpStatus::OK;*/
		
	OP_STATUS ops;

	AliveSpoofListener *spoof=OP_NEW(AliveSpoofListener, ());

	if(!spoof)
		return OpStatus::ERR_NO_MEMORY;
		
	// It will lunch the process and delete the listener (if successfull)
	if(OpStatus::IsError(ops=spoof->Construct(upnp, &logics, socket, addr, location, unique_service_url, search_target, search_target2, reason, expire_val)))
	{
		OP_DELETE(spoof);

		return ops;
	}

	return OpStatus::OK;
}

OP_STATUS UPnPSpoofCheckOHRListener::SetData(SpoofListener *spoof_listener, OpSocketAddress *addr)
{
	OP_ASSERT(spoof_listener && addr);
	listener=spoof_listener;
	
	if(!addr)
		return OpStatus::ERR_NULL_POINTER;
		
	if(!address)
		RETURN_IF_ERROR(OpSocketAddress::Create(&address));

	if(!address)
		return OpStatus::ERR_NO_MEMORY;
		
	return address->Copy(addr);
 }

void UPnPSpoofCheckOHRListener::OnHostResolved(OpHostResolver* host_resolver)
{
	OP_ASSERT(listener && host_resolver);
	
	if(listener && host_resolver)
	{
		OpSocketAddress *osa;
		BOOL spoofed=TRUE;
		OP_STATUS ops=OpStatus::OK;
		
		if(OpStatus::IsError(OpSocketAddress::Create(&osa)))
		{
			listener->OnError(OpStatus::ERR_NO_MEMORY);
			
			return;
		}
			
		for(int i=0, len=host_resolver->GetAddressCount(); spoofed && OpStatus::IsSuccess(ops) && i<len; i++)
		{
			ops=host_resolver->GetAddress(osa, i);
			if(OpStatus::IsSuccess(ops))
			{
#ifdef DEBUG
				OpString resolvedaddress;
				OpString thisaddress;
				osa->ToString(&resolvedaddress);
				address->ToString(&thisaddress);
				OP_NEW_DBG("UPnPSpoofCheckOHRListener::OnHostResolved", "upnp_trace_debug");
				OP_DBG(("host resolved - address %S socketaddress %S", resolvedaddress.CStr(), thisaddress.CStr()));
#endif //DEBUG
				if( osa->IsHostEqual(address) )
				{
					spoofed=FALSE;
				}
			}
		}
		
		OP_DELETE(osa);
		
		if(spoofed)
			listener->OnAddressSpoofed();
		else
			listener->OnAddressLegit(address);
	}
}

void UPnPSpoofCheckOHRListener::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error)
{
	OP_ASSERT(listener);
	
	if(listener)
	{
		if(error==OpHostResolver::OUT_OF_MEMORY)
			listener->OnError(OpStatus::ERR_NO_MEMORY);
		else
			listener->OnError(OpStatus::ERR);
	}
}
#endif // UPNP_ANTI_SPOOF

void EventSubscribeHandler::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_NEW_DBG("ExternalIPHandler::HandleCallback", "upnp_trace");
	OP_DBG(("Message: %s", (int)msg));
}

/*OP_STATUS AddressesListener::StartUDPDiscovery()
{
	OpString localName;
	OpHostResolver::Error err;
	
	OP_DELETE(ohr); 
    RETURN_IF_ERROR(SocketWrapper::CreateHostResolver(&ohr, this));
	RETURN_IF_ERROR(ohr->GetLocalHostName(&localName, &err));
	
	if(!uni_stricmp(localName.CStr(), UNI_L("localhost")))
	{
		upnp->HandlerFailed(NULL, "Local Host Name not resolved");
		return OpStatus::ERR;
	}
	
	RETURN_IF_ERROR(ohr->Resolve(localName.CStr()));
	//RETURN_IF_ERROR(ohr->Resolve(UNI_L("localhost")));
	
	return OpStatus::OK;
}

void AddressesListener::OnHostResolved(OpHostResolver* host_resolver)
{
	SendUDPDiscoveryToEveryNetworkCard(host_resolver);
}

void AddressesListener::OnHostResolverError(OpHostResolver* host_resolver, OpHostResolver::Error error)
{
	OP_NEW_DBG("AddressesListener::OnHostResolverError", "upnp_trace");
	OP_DBG(("*** Resolved with error!\n"));
}
	
void AddressesListener::SendUDPDiscoveryToEveryNetworkCard(OpHostResolver* host_resolver)
{
	OpString str;
	
	// TODO: if required, delete the current network card list
	upnp->Log(UNI_L("AddressesListener::SendUDPDiscoveryToEveryNetworkCard()"), UNI_L("Network cards available: %d"), host_resolver->GetAddressCount());
	
	for(int i=0, len=host_resolver->GetAddressCount(); i<len; i++)
	{
		OpSocketAddress *osa;
		
		if(OpStatus::IsError(OpSocketAddress::Create(&osa)))
		{
			upnp->HandlerFailed(NULL, "Socket Address creation error: Out of Memory?");
			
			return;
		}
		
		if(OpStatus::IsError(host_resolver->GetAddress(osa, i) || OpStatus::IsError(osa->ToString(&str))))
		{
			upnp->HandlerFailed(NULL, "Get Address error: Out of Memory?");
			
			OP_DELETE(osa);
			return;
		}
		
		//if(!str.Compare("192.168.1.102"))
		//	continue;
		
		OP_NEW_DBG("AddressesListener::SendUDPDiscoveryToEveryNetworkCard", "upnp_trace");
		OP_DBG((UNI_L("*** Resolved host: %s!\n"), str.CStr()));
		
		NetworkCard *card=upnp->AddNewNetworkCard(&str);
		OpSocket *socket=(card)?card->GetSocket():NULL;
		
		if(!socket)
		{
			upnp->HandlerFailed(NULL, "Error during socket creation!");
			OP_DELETE(osa);
			
			return;			
		}
		
		//if(OpStatus::IsError(upnp->SetCurrentIP(&str)))
	    //{
		//	// TODO: signal error?
		//	upnp->HandlerFailed(NULL, "Impossible to set the current IP");
		//}
		
		if(OpStatus::IsError(upnp->SubscribeUDPDiscovery(socket, osa)))
		{
			upnp->Log(UNI_L("AddressesListener::SendUDPDiscoveryToEveryNetworkCard()"), UNI_L("Error on subscribing to UDP discovery advertisments on network card %s"), str.CStr());
		}
		
		OP_STATUS broadcastErr=upnp->BroadcastUDPDiscovery(socket, osa);
		
		if(OpStatus::IsError(broadcastErr))
		{
			upnp->Log(UNI_L("AddressesListener::SendUDPDiscoveryToEveryNetworkCard()"), UNI_L("Error on sending UDP discovery request to network card %s"), str.CStr());
			// TODO: signal error?
			//OP_DELETE(upnp);
			//return OpStatus::ERR;
			if(broadcastErr==UPnPStatus::ERROR_UDP_SEND_FAILED)
				upnp->HandlerFailed(NULL, "Broadcast failed: bind problem?");
			else
				upnp->HandlerFailed(NULL, "Broadcast failed");
		}
		
		OP_DELETE(osa);
	}
}*/

/*OP_STATUS UPnPXH_DescriptionGeneric::HandleTokenAuto()
{
	OpString* service_id=tags_parser.getTagValue(0);
	OpString* service_type=tags_parser.getTagValue(1);
	OpString* control_url=tags_parser.getTagValue(2);
	OpString* event_url=tags_parser.getTagValue(3);
	OpString* manufacturer=tags_parser.getTagValue(4);
	OpString* model_name=tags_parser.getTagValue(5);
	OpString* model_description=tags_parser.getTagValue(6);
	OpString* model_number=tags_parser.getTagValue(7);
	OpString* serial_number=tags_parser.getTagValue(8);
	
	// An error here is not enough to block che UPnP process, let's continue
	xh_device->SetDeviceBrandAndModel(manufacturer, model_name, model_description, model_number, serial_number);
	
	OP_ASSERT(service_type && control_url && event_url && !service_type->IsEmpty() && !control_url->IsEmpty() && !event_url->IsEmpty());
	
	return NotifyService(service_id, service_type, control_url, event_url);
}*/

OP_STATUS UPnPXH_DescriptionGeneric::Construct()
{
	//enable_tags_subset=TRUE;
	notify_missing_variables=FALSE;
	notify_failure=TRUE;
	
	return tags_parser.Construct(NULL, 6, 3, 4, UNI_L("serviceId"), NULL, UNI_L("serviceType"), NULL, UNI_L("controlURL"), NULL, UNI_L("eventSubURL"), NULL, UNI_L("manufacturer"), NULL, UNI_L("modelName"), NULL, UNI_L("modelDescription"), NULL, UNI_L("modelNumber"), NULL, UNI_L("serialNumber"), NULL);
	//return tags_parser.Construct(NULL/*UNI_L("service")*/, 8, UNI_L("serviceType"), NULL, UNI_L("controlURL"), NULL, UNI_L("eventSubURL"), NULL, UNI_L("manufacturer"), NULL, UNI_L("modelName"), NULL, UNI_L("modelDescription"), NULL, UNI_L("modelNumber"), NULL, UNI_L("serialNumber"), NULL);
}

#define RET_IF_ERROR(expr) \
	do { \
		XMLTokenHandler::Result RETURN_IF_ERROR_TMP = expr; \
		if (RETURN_IF_ERROR_TMP!=XMLTokenHandler::RESULT_OK) \
			return RETURN_IF_ERROR_TMP; \
	} while (0)
	
XMLTokenHandler::Result UPnPXH_DescriptionGeneric::ManageChallenge(UPnPServicesProvider *prov)
{
	GetDevice()->SetTrust(TRUST_UNTRUSTED);
	
	if(challenge.Length())
	{
		OpStringC *res=prov->GetAttributeValue(UPnPServicesProvider::CHALLENGE_RESPONSE);
		
		if(res && res->Length()>0)
		{
			// Compute MD5: PrivateKey+challenge
			OpString8 expected;
			OpString8 pk;
			
			RETURN_VALUE_IF_ERROR(pk.Set(UPNP_OPERA_PRIVATE_KEY), XMLTokenHandler::RESULT_ERROR);
			RETURN_VALUE_IF_ERROR(UPnP::Encrypt(challenge, pk, expected), XMLTokenHandler::RESULT_ERROR);
			
			if(!res->Compare(expected.CStr()))
				GetDevice()->SetTrust(TRUST_PRIVATE_KEY);
			else
				GetDevice()->SetTrust(TRUST_ERROR);
		}
	}
	
	return XMLTokenHandler::RESULT_OK;
}

XMLTokenHandler::Result UPnPXH_DescriptionGeneric::ManageServiceStart(UPnPServicesProvider *prov)
{
	// Add a service to the Service Provider
	if(!GetCurrentService())
	{
		UPnPService *service=OP_NEW(UPnPService, ());
		
		if(!service|| OpStatus::IsError(service->Construct()) || OpStatus::IsError(prov->AddService(service)))
		{
			OP_DELETE(service);
			
			return XMLTokenHandler::RESULT_ERROR;
		}
		
		OpStatus::Ignore(cur_service_ref.AddReference(service->GetReferenceObjectPtrXML())); // It cannot really fail
	}
		
	return XMLTokenHandler::RESULT_OK;
}

BOOL UPnPXH_DescriptionGeneric::ManageSpoofing(UPnPServicesProvider *prov)
{
	const uni_char *srv_url=(GetCurrentService())?GetCurrentService()->GetAttributeValueUni(UPnPService::CONTROL_URL):NULL;
			
	// Second antispoof check: service server name must be the same as the description (validate) url, or it must be relative
	if(GetCurrentService() && srv_url && srv_url[0]!='/')
	{
		if(uni_strstr(srv_url, "://")) // Relative paths are not spoofed...
		{
			OpStringC prov_url=prov->GetFullDescriptionURL();
			
			URL url_prov=g_url_api->GetURL(prov_url);
			URL url_serv=g_url_api->GetURL(srv_url);
			
			ServerName *sn_prov=(ServerName *)url_prov.GetAttribute(URL::KServerName, NULL);
			ServerName *sn_serv=(ServerName *)url_serv.GetAttribute(URL::KServerName, NULL);
			
			if( !url_prov.IsValid() || !url_serv.IsValid() ||
				!sn_prov || !sn_serv ||
				!sn_prov->UniName() || !sn_serv->UniName() ||
				uni_strcmp(sn_prov->UniName(), sn_serv->UniName()))
			{
				// Possible spoofing: the service does not match the description server
				prov->RemoveService(GetCurrentService());
				
	#ifdef UPNP_LOG
				OpString temp;
				
				temp.AppendFormat(UNI_L("Spoofing attempt detected (2nd check): %s is not from %s"), srv_url, prov_url.CStr());
				upnp->Log(UNI_L("UPnPXH_DescriptionGeneric::HandleToken()"), temp.CStr());
	#endif

				OP_DELETE(GetCurrentService());
				OP_ASSERT(GetCurrentService()==NULL);

				return FALSE; // spoofing
			}
		}
		else
		{
			// FIXME: it should probably check BaseURL...
			// Add the "/" to the services..
			OpString str;
			
			str.AppendFormat(UNI_L("/%s"), srv_url);
			GetCurrentService()->SetAttributeValue(UPnPService::CONTROL_URL, str.CStr());
		}
	}
	
	return TRUE; // OK
}

XMLTokenHandler::Result UPnPXH_DescriptionGeneric::ManageServiceEnd(UPnPServicesProvider *prov)
{
	// Remove the service if already present
	if(GetCurrentService())
	{
		const uni_char *srv_id=GetCurrentService()->GetAttributeValueUni(UPnPService::SERVICE_ID);
		
		OP_ASSERT(srv_id);
		UPnPService *srv;
		
		// Duplicates removal attempt
		if(srv_id && OpStatus::IsSuccess(prov->RetrieveServiceByName(srv_id, srv)))
		{
			if(srv!=GetCurrentService())
			{
				prov->RemoveService(srv);
				OP_DELETE(srv);
			}
		}
		
		//RETURN_VALUE_IF_ERROR(NotifyService(GetCurrentService()), XMLTokenHandler::RESULT_ERROR);
	}
	cur_service_ref.CleanReference();
	OP_ASSERT(GetCurrentService()==NULL);
	
	return XMLTokenHandler::RESULT_OK;
}

XMLTokenHandler::Result UPnPXH_DescriptionGeneric::ManageText(XMLToken &token, UPnPServicesProvider *prov, const uni_char *tag_name, int tag_len)
{
	NameValue *nv;
			
	if(GetCurrentService()) // Property of the service (e.g. serviceType)
		nv=GetCurrentService()->GetAttributeByNameLen(tag_name, tag_len);
	else			// Property of the UPnP device (e.g. deviceType)
		nv=prov->GetAttributeByNameLen(tag_name, tag_len);
	
	if(nv)
	{
		// Reconstruction of the text
		OP_STATUS ops;
		OpString temp;

		if(!nv->GetValue().Length())
		{
			XMLToken::Literal text;
			unsigned int lp;
		
			if(OpStatus::IsError(ops=token.GetLiteral(text)))
				return XMLTokenHandler::RESULT_ERROR;
	
			for(lp=0; lp<text.GetPartsCount(); lp++)
				RETURN_VALUE_IF_ERROR(temp.Append(text.GetPart(lp), text.GetPartLength(lp)), XMLTokenHandler::RESULT_ERROR);
		
			RETURN_VALUE_IF_ERROR(nv->SetValue(temp.CStr()), XMLTokenHandler::RESULT_ERROR);
		}
	}
	
	return XMLTokenHandler::RESULT_OK;
}

XMLTokenHandler::Result UPnPXH_DescriptionGeneric::HandleToken(XMLToken &token)
{
	OP_ASSERT(GetDevice() && upnp);
	
	if(!GetDevice() || !upnp || GetDevice()->GetTrust()==TRUST_ERROR)
		return XMLTokenHandler::RESULT_ERROR;
		
	const uni_char *tag_name=token.GetName().GetLocalPart();
	int tag_len=token.GetName().GetLocalPartLength();
	UPnPServicesProvider *prov=GetDevice()->GetProvider();
			
	//OP_ASSERT(prov);
	
	if(!prov)
		return XMLTokenHandler::RESULT_ERROR;
		
	if(tag_name && tag_len>0 && !uni_strncmp(tag_name, UNI_L("service"), tag_len))
	{
		// Trust check
		if(GetDevice()->GetTrust()==TRUST_UNKNOWN)
			RET_IF_ERROR(ManageChallenge(prov));
		
		OP_ASSERT(GetDevice()->GetTrust()!=TRUST_UNKNOWN);
		
		if(GetDevice()->GetTrust()==TRUST_ERROR)
		{
			upnp->RemoveDevice(GetDevice());
			
			return XMLTokenHandler::RESULT_ERROR;
		}
		
		if(token.GetType()==XMLToken::TYPE_STag)
			RET_IF_ERROR(ManageServiceStart(prov));
		else if(token.GetType()==XMLToken::TYPE_ETag)
		{
			if(!ManageSpoofing(prov))
				return XMLTokenHandler::RESULT_OK;
			
			RET_IF_ERROR(ManageServiceEnd(prov));
		}
	}
	else if(token.GetType()==XMLToken::TYPE_STag && tag_name && tag_len>0 && !uni_strncmp(tag_name, UNI_L("root"), tag_len))
	{
		prov->ResetAttributesValue();
	}
	else
	{
		if(token.GetType()==XMLToken::TYPE_Text)
			RET_IF_ERROR(ManageText(token, prov, tag_name, tag_len));
	}
	return XMLTokenHandler::RESULT_OK;
	
}

void UPnPXH_DescriptionGeneric::ParsingSuccess(XMLParser *parser)
{
	if(upnp)
	{
		UPnPDevice *dev=GetDevice();

		if(dev)
		{
			xh_device_ref.RemoveReference(dev->GetReferenceObjectPtrAuto());

			OP_ASSERT(GetDevice()==NULL);

		#ifdef WEBSERVER_SUPPORT
			if(g_webserver && g_webserver->IsRunning()) // If the webserver is not running, then it is a device that took over the same name
			{
				// Do not notify about self
				UPnPServicesProvider * prov = dev->GetProvider();
				if (prov && !prov->IsVisibleToOwner())
				{
					const uni_char * dev_type = prov->GetAttributeValueUni(UPnPServicesProvider::DEVICE_TYPE);
					if (dev_type && uni_strcmp(UNI_L(UPNP_DISCOVERY_OPERA_SEARCH_TYPE), dev_type) == 0)
					{
						OpString user_name, device_name;
						user_name.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverUser));
						device_name.Set(g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice));

						if (!user_name.Compare(prov->GetAttributeValueUni(UPnPServicesProvider::UNITE_USER)) &&
							!device_name.Compare(prov->GetAttributeValueUni(UPnPServicesProvider::UNITE_DEVICE)))
						{
							g_upnp->RemoveDevice(dev);
							return;
						}
					}
				}
			}
		#endif
			upnp->NotifyControlPointsOfDevice(dev, TRUE);
		}
	}
}
#endif
