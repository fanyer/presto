/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4;  -*-
**
** Copyright (C) 2002-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UPNP_SUPPORT

#include "modules/upnp/upnp_port_opening.h"
#include "modules/formats/hdsplit.h"
#include "modules/url/url_sn.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/stdlib/util/opdate.h"


UPnPPortOpening::UPnPPortOpening(UPnP *upnp_father, UINT16 first_port, UINT16 last_port, BOOL try_ppp): UPnPLogic(upnp_father, UPNP_DISCOVERY_IGD_SEARCH_TYPE)
{
	OP_ASSERT(upnp && upnp==upnp_father);

	port_start=first_port;
	port_end=last_port;
	has_ip_conn=FALSE;
	has_ppp_conn=FALSE;
	try_ppp_conn=try_ppp;
	cards_policy=UPNP_CARDS_OPEN_FIRST;
}

UPnPPortOpening::~UPnPPortOpening()
{
}

OP_STATUS UPnPPortOpening::NotifyFailure(const char *mex)
{
	return HandlerFailed(NULL, mex);
}

OP_STATUS UPnPPortOpening::QueryExternalIP(UPnPDevice *device)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	UPnPXH_Auto*ptr=OP_NEW(UPnPXH_ExternalIP, ());
	
	return upnp->StartActionCall(device, ptr, this);
}

OP_STATUS UPnPPortOpening::QueryIPRouted(UPnPDevice *device)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	UPnPXH_Auto*ptr=OP_NEW(UPnPXH_IPRouted, ());
	
	return upnp->StartActionCall(device, ptr, this);
}

OP_STATUS UPnPPortOpening::QueryNATEnabled(UPnPDevice *device)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	UPnPXH_Auto*ptr=OP_NEW(UPnPXH_NATEnabled, ());
	
	return upnp->StartActionCall(device, ptr, this);
}

OP_STATUS UPnPPortOpening::QueryIDGPortStatus(UPnPDevice *device, UINT16 port)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	UPnPXH_Auto* ptr=OP_NEW(UPnPXH_PortStatus, (port, TRUE, UPnPXH_PortStatus::STOP));

	if(!ptr)
	  return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(static_cast<UPnPXH_PortStatus *>(ptr)->AddParameters(device->GetNetworkCardAddress()));

	return upnp->StartActionCall(device, ptr, this);
}

OP_STATUS UPnPPortOpening::QueryDisablePort(UPnPDevice *device, UINT16 port)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	UPnPXH_Auto* ptr=OP_NEW(UPnPXH_DisablePort, (port, TRUE, UPnPXH_PortStatus::STOP));

	if(!ptr)
	  return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(static_cast<UPnPXH_DisablePort *>(ptr)->AddParameters(device->GetNetworkCardAddress()));

	return upnp->StartActionCall(device, ptr, this);
}

OP_STATUS UPnPPortOpening::RollbackListeners(UPnPDevice *device, UINT16 port, int cur)
{
	OP_STATUS ops=OpStatus::OK;
	
	if(cur<0)
		cur=listeners.GetCount();
	
	for(int i=0; i<cur; i++)
	{
		OP_STATUS ops2=((UPnPPortListener *)listeners.Get(i))->PortsRefused(upnp, device, this, port, port);
		
		if(OpStatus::IsError(ops2) && OpStatus::IsSuccess(ops))
			ops=ops2;
	}
	
	return ops;
}

OP_STATUS UPnPPortOpening::NotifyListenersPortClosed(UPnPDevice *device, UINT16 port, BOOL success)
{
	OP_STATUS ops=OpStatus::OK;
	
	for(UINT i=0; i<listeners.GetCount(); i++)
	{
		UPnPPortListener *listener=(UPnPPortListener *)(listeners.Get(i));
			
		if(listener)
			LAST_ERROR(ops, listener->PortClosed(upnp, device, this, port, success));
	}
	
	return ops;
}
				
OP_STATUS UPnPPortOpening::QueryEnablePort(UPnPDevice *device, UINT16 port)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	BOOL acceptable=FALSE;

	// Find an acceptable port number
	while(!acceptable)
	{
		int cur=0;
		int len=(int)listeners.GetCount();

		acceptable=TRUE;

		// Test if the port is acceptable for the listeners;
		while(cur<len && acceptable)
		{
			BOOL b=TRUE;

			UPnPListener *listener=listeners.Get(cur);
			
			if(listener)
			{
				if(OpStatus::IsError(((UPnPPortListener *)listener)->ArePortsAcceptable(upnp, device, this, port, port, b)))
					acceptable=FALSE;

				if(!b)
					acceptable=FALSE;
			}

			if(acceptable)
				cur++;
		}

		// cur now is equal to listeners.GetCount() or point to the listener that refused the connection
		OP_ASSERT(((UINT32)cur==listeners.GetCount() && acceptable) || !acceptable);

		// Rollback and try another port
		if(!acceptable)
		{
			int i;

			// Rollback
			RETURN_IF_ERROR(RollbackListeners(device, port, cur));

			if(port<GetLastPort())
				port++;
			else
			{
				// No more ports available: notify failure
				for(i=0, len=listeners.GetCount(); i<len; i++)
					RETURN_IF_ERROR(listeners.Get(i)->Failure(upnp, device, this));

				return OpStatus::ERR_NO_SUCH_RESOURCE;
			}
		}
	}

	// The port is acceptable: proceed
	UPnPXH_Auto* ptr=OP_NEW(UPnPXH_EnablePort, (port, TRUE, UPnPXH_PortStatus::STATUS));
	OP_STATUS ops;

	if(!ptr)
	  return OpStatus::ERR_NO_MEMORY;

	// upnp object used in the AddParameters(), before being set by StartXMLProcessing()
	static_cast<UPnPXH_EnablePort *>(ptr)->SetUPnP(upnp, NULL, this); // No device, to avoid an ASSERT during the second call to SetUPnP()
	
	if(OpStatus::IsError(ops=static_cast<UPnPXH_EnablePort *>(ptr)->AddParameters(device->GetNetworkCardAddress())) ||
	   OpStatus::IsError(ops=upnp->StartActionCall(device, ptr, this)))
	{
		OP_DELETE(ptr);

		// An error here probably means that it does not make sense to continue
		for(int i=0, len=listeners.GetCount(); i<len; i++)
			RETURN_IF_ERROR(listeners.Get(i)->Failure(upnp, device, this));


		return ops;
	}


	return OpStatus::OK;
}

OP_STATUS UPnPPortOpening::HandlerFailed(UPnPXH_Auto *child, const char *mex)
{
	locked=FALSE;
	
	OP_NEW_DBG("UPnP::HandlerFailed", "upnp_trace");

	#ifdef _DEBUG

		OpString str;
		//char buf[16];  /* ARRAY OK 2008-08-12 lucav */

		if(child && child->GetDevice())
		{
			str.Set("*** Handler Failed on Device URL ");
			str.Append(*child->GetDevice()->GetCurrentDescriptionURL());
		}
		else
			str.Set("*** Handler Failed on start up phase");

		str.Append(" with message: ");
		str.Append(mex);
		OP_DBG((str.CStr()));
	#endif

	// Try to switch to PPP
	if(!upnp->GetDeviceSearch() && TryPPPConnection() && child && child->GetDevice()->IsPPP())
	{
		child->GetDevice()->SetPPP(TRUE);
		RETURN_IF_ERROR(QueryEnablePort(child->GetDevice(), GetFirstPort()));				// Try the port

		return OpStatus::OK;
	}

	/*if(!upnp->GetDeviceSearch() && TryNextDevice())
	{
		OpString device_string;

		if(OpStatus::IsError(ComputeURLString(child->GetDevice(), device_string, &(child->GetDevice()->device_url))))
			return OpStatus::ERR;

		return QueryDescription(child->GetDevice(), device_string);
	}*/

	/*OpString device_string;

	if(OpStatus::IsError(ComputeURLString(child->GetDevice(), device_string, &(child->GetDevice()->device_url))))
		return OpStatus::ERR;

	if(OpStatus::IsSuccess(QueryDescription(child->GetDevice(), device_string)))
		return OpStatus::OK;*/

	// TODO: Alien incoming request disabled
#ifdef UPNP_LOG
	OP_DBG(("### Impossible to open the UPnP device"));
	last_error.Set(mex);

	OpString temp_string;

	temp_string.Set(mex);

	Log(UNI_L("UPnP::HandlerFailed()"), temp_string.CStr());
#endif

	// Notify the listeners
	for(int i=0, len=listeners.GetCount(); i<len; i++)
	{
		RETURN_IF_ERROR(listeners.Get(i)->Failure(upnp, (child)?child->GetDevice():NULL, this));
	}

	//port_openening_result(0, FALSE, this, (child)?child->GetDevice():NULL, TRUE);

	// TODO: verify
	return OpStatus::OK;
}

OP_STATUS UPnPPortOpening::SignalExternalPort(UPnPDevice *device, UINT16 port)
{
	locked=FALSE;
	
	// Notify the listeners
	for(int i=0, len=listeners.GetCount(); i<len; i++)
		RETURN_IF_ERROR(listeners.Get(i)->Success(upnp, device, this, port, port));

	return OpStatus::OK;
}


OP_STATUS UPnPPortOpening::SubscribeExternalIP(UPnPDevice *device)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;
		
	OpString url_string;

	RETURN_IF_ERROR(ComputeURLString(device, url_string, device->GetEventURL()));

	if(url_string.IsEmpty())
		return OpStatus::ERR;

	OP_NEW_DBG("UPnP::SubscribeExternalIP", "upnp_trace");
	OP_DBG((UNI_L("Subscription URL: %s"), url_string.CStr()));

	//url_subscribe=g_url_api->GetURL("http://10.20.20.47:2869/upnp/events/", NULL, TRUE);
	//if(url_subscribe.Load(&mh_event, empty_ref, FALSE, FALSE)!=COMM_LOADING)
	//	return OpStatus::ERR;

	//return OpStatus::OK;

	url_subscribe=g_url_api->GetURL(url_string.CStr(), NULL, TRUE);

	if(url_subscribe.IsEmpty())
		return OpStatus::ERR;

	//OpString out_http_message;
	//HTTPMessageWriter mex;

	//RETURN_IF_ERROR(mex.setURL(device->device_ip.CStr(), device->GetEventURL()->CStr(), device->device_port));
	OpString addr;
	char bufInt[16];	/* ARRAY OK 2008-08-12 lucav */
	URL empty_ref;

	op_itoa(LOCAL_TCP_PORT_EVENTS, bufInt, 10);
	//OP_ASSERT(!current_ip.IsEmpty());
	RETURN_IF_ERROR(addr.Set("<http://"));
	//RETURN_IF_ERROR(addr.Append(current_ip));
	OpSocketAddress *osa;
	OpString ip;

	osa=device->GetNetworkCardAddress();
	if(!osa)
		return OpStatus::ERR_NULL_POINTER;

	RETURN_IF_ERROR(osa->ToString(&ip));
	RETURN_IF_ERROR(addr.Append(ip.CStr()));
	RETURN_IF_ERROR(addr.Append(":"));
	RETURN_IF_ERROR(addr.Append(bufInt));
	RETURN_IF_ERROR(addr.Append("/upnp/events/>"));

	URL_Custom_Header header_item;
	OP_STATUS op_err = OpStatus::OK;

	// Address the the router will call back
	RETURN_IF_ERROR(header_item.name.Set("CALLBACK"));
	RETURN_IF_ERROR(header_item.value.Set(addr.CStr()));
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, url_subscribe.SetAttribute(URL::KAddHTTPHeader, &header_item), op_err);

	// Notification
	RETURN_IF_ERROR(header_item.name.Set("NT"));
	RETURN_IF_ERROR(header_item.value.Set("upnp:event"));
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, url_subscribe.SetAttribute(URL::KAddHTTPHeader, &header_item), op_err);

	// Timeout
	RETURN_IF_ERROR(header_item.name.Set("TIMEOUT"));
	RETURN_IF_ERROR(header_item.value.Set("Second-infinite"));
	TRAP_AND_RETURN_VALUE_IF_ERROR(op_err, url_subscribe.SetAttribute(URL::KAddHTTPHeader, &header_item), op_err);


	//RETURN_IF_ERROR(mex.setNotificationType(UNI_L("upnp:event")));
	//mex.setTimeout(3600);
	//RETURN_IF_ERROR(mex.GetMessage(&out_http_message, "SUBSCRIBE", NULL));

	//http_mex8.Set(out_http_message.CStr());

	//OP_DBG(("Subscription SOAP Message: %s", http_mex8.CStr()));

	//RETURN_IF_ERROR(url_subscribe.SetHTTP_Data(http_mex8.CStr(), TRUE, TRUE));

	//g_main_message_handler->SetCallBack(&event_handler, MSG_COMM_LOADING_FINISHED, url_subscribe.Id());
	ReEnable();
	//mh_event.SetCallBack(&event_handler, MSG_COMM_LOADING_FINISHED, 0);

	if(url_subscribe.Load(g_main_message_handler, empty_ref)!=COMM_LOADING)
		return OpStatus::ERR;

	return OpStatus::OK;
}

OP_STATUS UPnPPortOpening::ReEnable()
{
	RETURN_IF_ERROR(UPnPLogic::ReEnable());
	
	return g_main_message_handler->SetCallBack(&event_handler, MSG_COMM_LOADING_FINISHED, url_subscribe.Id());
}

OP_STATUS UPnPPortOpening::NotifyExternalIP(UPnPDevice *device)
{
	if(!device)
		return OpStatus::ERR_NULL_POINTER;

	OpStringC *cur_ip=device->GetExternalIP();
	OpStringC *notif_ip=device->GetNotifiedExternalIPPtr();

	if(!cur_ip || !notif_ip)
		return OpStatus::ERR_OUT_OF_RANGE;

	if(cur_ip->Compare("0.0.0.0") && cur_ip->Compare(*notif_ip))  // Signal to Opera server
	{
		OP_NEW_DBG("UPnP::NotifyExternalIP", "upnp_trace");
		OP_DBG((UNI_L("*** External IP change: new=%s - old=%s"), cur_ip->CStr(), notif_ip->CStr(), "upnp_trace"));

		return device->SetNotifiedExternalIPPtr(cur_ip);
	}

	return OpStatus::OK;
}


/////////////////// Handlers of the Port Opening
OP_STATUS UPnPXH_IPRouted::HandleTokenAuto()
{
	OpString* conn_type=tags_parser.getTagValue(0);
	
	if(!GetDevice())
		return OpStatus::ERR_NULL_POINTER;
	
	if(GetLogic() && !upnp->IsDebugging(UPNP_DBG_NO_ROUTED) && !conn_type->Compare(UNI_L("IP_Routed")))
		return ((UPnPPortOpening *)GetLogic())->QueryNATEnabled(GetDevice());
	
	if(GetDevice()->IsPPP())
		NotifyFailure("No PPP IP_Routed");
	else
		NotifyFailure("No IP IP_Routed");
	
	return OpStatus::OK;
}

OP_STATUS UPnPXH_NATEnabled::HandleTokenAuto()
{
	OpString* nat_enabled=tags_parser.getTagValue(0);
	
	if(!GetDevice())
		return OpStatus::ERR_NULL_POINTER;
	
	if(GetLogic() && !upnp->IsDebugging(UPNP_DBG_NO_ROUTED) && !nat_enabled->Compare(UNI_L("1")))
	{
		return ((UPnPPortOpening *)GetLogic())->QueryExternalIP(GetDevice());
		//((UPnPPortOpening *)GetLogic())->QueryDisablePort();
		//return ((UPnPPortOpening *)GetLogic())->QueryIDGPortStatus();
		//return ((UPnPPortOpening *)GetLogic())->QueryDisablePort();
		//return ((UPnPPortOpening *)GetLogic())->QueryIDGPortStatus();
	}
	
	if(GetDevice()->IsPPP())
		NotifyFailure("No PPP NAT Enabled");
	else
		NotifyFailure("No IP NAT Enabled");
		
	return OpStatus::OK;
}

OP_STATUS UPnPXH_PortStatus::HandleTokenAuto()
{
	OP_NEW_DBG("UPnPXH_PortStatus::HandleTokenAuto", "upnp_trace");
	OP_DBG((UNI_L("*** Internal Port: %s\n"), tags_parser.getTagValue(0)->CStr()));
	OP_DBG((UNI_L("*** Enable: %s\n"), tags_parser.getTagValue(1)->CStr()));
	OP_DBG((UNI_L("*** Port Mapping Description: %s\n"), tags_parser.getTagValue(2)->CStr()));
	OP_DBG((UNI_L("*** Lease Duration: %s\n"), tags_parser.getTagValue(3)->CStr()));
	
	OP_ASSERT(GetLogic());
	
	if(!GetLogic())
		return OpStatus::ERR_NULL_POINTER;
	
	if(next_action==DISABLE)
	  return ((UPnPPortOpening *)GetLogic())->QueryDisablePort(GetDevice(), external_port);
	if(next_action==ENABLE)
	  return ((UPnPPortOpening *)GetLogic())->QueryEnablePort(GetDevice(), external_port);
	if(next_action==STATUS)
	  return ((UPnPPortOpening *)GetLogic())->QueryIDGPortStatus(GetDevice(), external_port);
	  
	return OpStatus::OK;
}

OP_STATUS UPnPXH_PortStatus::AddParameters(OpSocketAddress *address)
{
	NamedValue *nv1=OP_NEW(NamedValue, ());
	NamedValue *nv2=OP_NEW(NamedValue, ());
	NamedValue *nv3=OP_NEW(NamedValue, ());
	
	if(!nv1 || !nv2 || !nv3)
	{
		OP_DELETE(nv1);
		OP_DELETE(nv2);
		OP_DELETE(nv3);
		
		return OpStatus::ERR_NO_MEMORY;
	}
	
	uni_char buf[16];   /* ARRAY OK 2008-08-12 lucav */
	
	OP_STATUS ops=OpStatus::OK;
	
	LAST_ERROR(ops, nv1->SetName(UNI_L("NewRemoteHost")));
	LAST_ERROR(ops, nv1->SetValue(UNI_L("")));
	LAST_ERROR(ops, nv2->SetName(UNI_L("NewExternalPort")));
	LAST_ERROR(ops, nv2->SetValue(uni_itoa(external_port, buf, 10)));
	LAST_ERROR(ops, nv3->SetName(UNI_L("NewProtocol")));
	LAST_ERROR(ops, nv3->SetValue((use_tcp)?UNI_L("TCP"):UNI_L("UDP")));
	LAST_ERROR(ops, params.Add(nv1));
	LAST_ERROR(ops, params.Add(nv2));
	LAST_ERROR(ops, params.Add(nv3));
	
	if(OpStatus::IsError(ops))
	{
		OP_DELETE(nv1);
		OP_DELETE(nv2);
		OP_DELETE(nv3);
	}
	
	return ops;
}

OP_STATUS UPnPXH_PortStatus::Construct()
{
	if(!upnp)
		return OpStatus::ERR_NULL_POINTER;
		
	return tags_parser.Construct(NULL, 4, 0, -1, UNI_L("NewInternalPort"), NULL, UNI_L("NewEnabled"), NULL, UNI_L("NewPortMappingDescription"), NULL, UNI_L("NewLeaseDuration"), NULL); 
}
   
OP_STATUS UPnPXH_DisablePort::Finished()
{
	if(GetLogic())
	{
		((UPnPPortOpening *)GetLogic())->NotifyListenersPortClosed(GetDevice(), external_port, TRUE);
		
		if(next_action==STATUS)
			return ((UPnPPortOpening *)GetLogic())->QueryIDGPortStatus(GetDevice(), external_port);
	}
		
	return OpStatus::OK;
}

OP_STATUS UPnPXH_DisablePort::NotifyFailure(const char *mex)
{
	if(GetLogic())
	{
		((UPnPPortOpening *)GetLogic())->NotifyListenersPortClosed(GetDevice(), external_port, FALSE);
	}
	
	return UPnPXH_PortStatus::NotifyFailure(mex);
}

OP_STATUS UPnPXH_EnablePort::AddParameters(OpSocketAddress *address)
{
	OP_ASSERT(address);
	
	if(!address)
		return OpStatus::ERR_NULL_POINTER;
	
	if(!g_local_host_addr)
		return OpStatus::ERR;
	
	NamedValue *nv1=OP_NEW(NamedValue, ());
	NamedValue *nv2=OP_NEW(NamedValue, ());
	NamedValue *nv3=OP_NEW(NamedValue, ());
	NamedValue *nv4=OP_NEW(NamedValue, ());
	NamedValue *nv5=OP_NEW(NamedValue, ());
	NamedValue *nv6=OP_NEW(NamedValue, ());
	NamedValue *nv7=OP_NEW(NamedValue, ());
	NamedValue *nv8=OP_NEW(NamedValue, ());
	
	
	if(!nv1 || !nv2 || !nv3 || !nv4 || !nv5 || !nv6 || !nv7 || !nv8)
	{
		OP_DELETE(nv1);
		OP_DELETE(nv2);
		OP_DELETE(nv3);
		OP_DELETE(nv4);
		OP_DELETE(nv5);
		OP_DELETE(nv6);
		OP_DELETE(nv7);
		OP_DELETE(nv8);
		return OpStatus::ERR_NO_MEMORY;
	}
	  
	uni_char buf[16];	/* ARRAY OK 2008-08-12 lucav */
	OpString addr;
	OP_STATUS ops=OpStatus::OK;
	
	LAST_ERROR(ops, nv1->SetName(UNI_L("NewRemoteHost")));
	LAST_ERROR(ops, nv1->SetValue(UNI_L("")));
	LAST_ERROR(ops, nv2->SetName(UNI_L("NewExternalPort")));
	LAST_ERROR(ops, nv2->SetValue(uni_itoa(external_port, buf, 10)));
	LAST_ERROR(ops, nv3->SetName(UNI_L("NewProtocol")));
	LAST_ERROR(ops, nv3->SetValue((use_tcp)?UNI_L("TCP"):UNI_L("UDP")));
	LAST_ERROR(ops, nv4->SetName(UNI_L("NewInternalPort")));
	LAST_ERROR(ops, nv4->SetValue(uni_itoa(external_port, buf, 10)));
	
	LAST_ERROR(ops, nv5->SetName(UNI_L("NewInternalClient")));

	OpString ip;
	
	LAST_ERROR(ops, address->ToString(&ip));
	OP_ASSERT(!ip.IsEmpty() && ip.Compare("0.0.0.0"));
	LAST_ERROR(ops, nv5->SetValue(ip.CStr()));
	
	LAST_ERROR(ops, nv6->SetName(UNI_L("NewEnabled")));
	LAST_ERROR(ops, nv6->SetValue(UNI_L("1")));
	LAST_ERROR(ops, nv7->SetName(UNI_L("NewPortMappingDescription")));
	
	#ifdef _DEBUG
		double date=OpDate::GetCurrentUTCTime();
		OpString8 descr;
		
		descr.Set("Opera Unite - ");
		descr.AppendFormat("%s %d:%d:%d %d-%d-%d", (GetDevice())?GetDevice()->GetReason():" NUL DEV - no reasons", OpDate::HourFromTime(date), OpDate::MinFromTime(date), OpDate::SecFromTime(date), OpDate::YearFromTime(date), OpDate::MonthFromTime(date)+1, OpDate::DateFromTime(date));

		LAST_ERROR(ops, nv7->SetValue(descr.CStr()));
	#else
		LAST_ERROR(ops, nv7->SetValue(UNI_L("Opera Unite")));
	#endif
	LAST_ERROR(ops, nv8->SetName(UNI_L("NewLeaseDuration")));
	LAST_ERROR(ops, nv8->SetValue(uni_itoa(UPNP_PORT_MAPPING_LEASE, buf, 10)));
	
	
	LAST_ERROR(ops, params.Add(nv1));
	LAST_ERROR(ops, params.Add(nv2));
	LAST_ERROR(ops, params.Add(nv3));
	LAST_ERROR(ops, params.Add(nv4));
	LAST_ERROR(ops, params.Add(nv5));
	LAST_ERROR(ops, params.Add(nv6));
	LAST_ERROR(ops, params.Add(nv7));
	LAST_ERROR(ops, params.Add(nv8));
	
	if(OpStatus::IsError(ops))
	{
		OP_DELETE(nv1);
		OP_DELETE(nv2);
		OP_DELETE(nv3);
		OP_DELETE(nv4);
		OP_DELETE(nv5);
		OP_DELETE(nv6);
		OP_DELETE(nv7);
		OP_DELETE(nv8);
	}
	
	return OpStatus::OK;
}

void UPnPXH_EnablePort::ParsingSuccess(XMLParser *parser)
{
	OP_ASSERT(GetDevice() && upnp);
	
	if(!upnp || !GetDevice())
		return;
	
#ifdef UPNP_LOG
	if(upnp->IsDebugging(UPNP_DBG_NO_PORT))
	{
		ParsingError(parser, 0xFFFFFFFF, NULL);
		return;
	}

	OP_NEW_DBG("UPnPXH_EnablePort::ParsingSuccess", "upnp_trace");
	OP_DBG((UNI_L("*** Port %d opened for %s!\n"), external_port, GetDevice()->IsPPP()?UNI_L("PPP"):UNI_L("IP")));
	
	if(upnp->IsNetworkCardPresent(GetDevice()->GetNetworkCard()))
	{
		OpSocketAddress *osa=GetDevice()->GetNetworkCardAddress();
		OpString routerIP;
		
		if(osa && osa->IsValid())
			osa->ToString(&routerIP);
			
		upnp->Log(UNI_L("UPnPXH_EnablePort::ParsingSuccess()"),UNI_L("*** Port %d opened for %s on router %s!\n"), external_port, GetDevice()->IsPPP()?UNI_L("PPP"):UNI_L("IP"), routerIP.CStr());
	}
	else
		upnp->Log(UNI_L("UPnPXH_EnablePort::ParsingSuccess()"),UNI_L("*** Port %d opened for %s on unknown router...\n"), external_port, GetDevice()->IsPPP()?UNI_L("PPP"):UNI_L("IP"));
#endif

	OP_ASSERT(GetLogic()); // It can be deleted...
	
	if(!GetLogic())
		return;
		
	// Alien services enabled
	((UPnPPortOpening *)GetLogic())->SignalExternalPort(GetDevice(), external_port);
	
	//if(port_openening_result)
		//port_openening_result(external_port, TRUE, upnp, GetDevice(), TRUE);
	  
	// Subscribe to IGD IP change; errors are not notified to the father because not so much "important" to stop the UPnP process
	// TODO: add notification of non blocking problem
	((UPnPPortOpening *)GetLogic())->SubscribeExternalIP(GetDevice());
}

void UPnPXH_EnablePort::ParsingError(XMLParser *parser, UINT32 http_status, const char *err_mex)
{
	OP_NEW_DBG("UPnPXH_EnablePort::ParsingError", "upnp_trace_debug");
	OP_DBG(("*** Port %d could not be opened for %s - HTTP status: %d - Error mex: %s\n", external_port, GetDevice()->IsPPP()?"PPP":"IP", (int)http_status, (err_mex)?err_mex:""));
	
	//BOOL try_next=true; //(port_openening_result)?port_openening_result(external_port, FALSE, upnp, GetDevice(), FALSE):TRUE;
	
	int n=0;
	
	OP_ASSERT(GetLogic());
	
	if(!GetLogic())
		return;
	
	//if(try_next)
	//{
		if(external_port<((UPnPPortOpening *)GetLogic())->GetLastPort())	// Try all the ports
			n=external_port+1;
	    // NOTE: PPP managed in the UPnP Object
	    //else if(upnp->TryPPPConnection() && !device->IsPPP())			// Then switch to PPP
	    //{
		//	upnp->SetPPPDevice(TRUE);
		//	n=upnp->GetFirstPort();
		//}
	//}
	//else
	//	upnp->StopDeviceSearch(TRUE);			// No more ports
	
	///// If we have this problem, chances are that we will have it in every port...
	//if(n>0)
	//	((UPnPPortOpening *)GetLogic())->QueryEnablePort(GetDevice(), n);				// Try the port
	//else
	    //((UPnPPortOpening *)GetLogic())->HandlerFailed(this, "No more ports");
	    
	
	// The listeners must now rollback
	((UPnPPortOpening *)GetLogic())->RollbackListeners(GetDevice(), external_port);

	// UPnP... if the port is taken, a response 500 (Internal Server Error) is sent...
	if(http_status==500 && n>0)
		((UPnPPortOpening *)GetLogic())->QueryEnablePort(GetDevice(), n);				// Try the port
	else
	{
	    ((UPnPPortOpening *)GetLogic())->HandlerFailed(this, "No more ports");
	    NotifyFailure(err_mex);
	}
}

OP_STATUS UPnPXH_ExternalIP::Construct()
{
	if(!upnp || !GetDevice())
		return OpStatus::ERR_NULL_POINTER;
		
	notify_failure=TRUE;
	
	return tags_parser.Construct(NULL, 1, 0, -1, UNI_L("NewExternalIPAddress"), GetDevice()->GetExternalIP()); 
}

OP_STATUS UPnPXH_ExternalIP::HandleTokenAuto()
{
	OP_ASSERT(GetLogic());
	
	if(!GetLogic())
		return OpStatus::ERR_NULL_POINTER;
		
	((UPnPPortOpening *)GetLogic())->NotifyExternalIP(GetDevice());
	
	return ((UPnPPortOpening *)GetLogic())->QueryEnablePort(GetDevice(), ((UPnPPortOpening *)GetLogic())->GetFirstPort());
}

OP_STATUS UPnPXH_IPRouted::Construct()
{
	if(!upnp)
		return OpStatus::ERR_NULL_POINTER;
		
	notify_failure=TRUE;
	
	return tags_parser.Construct(NULL, 1, 0, -1, upnp->IsDebugging(UPNP_DBG_NO_ROUTED2)?UNI_L("FAKE_FOR_DEBUG"):UNI_L("NewConnectionType"), NULL);
}

OP_STATUS UPnPXH_NATEnabled::Construct()
{
	if(!upnp)
		return OpStatus::ERR_NULL_POINTER;
		
	notify_failure=TRUE;
	
	return tags_parser.Construct(NULL, 1, 0, -1, upnp->IsDebugging(UPNP_DBG_NO_NAT2)?UNI_L("FAKE_FOR_DEBUG"):UNI_L("NewNATEnabled"), NULL);
}

/*void UPnPXH_DescriptionPortOpening::ParsingSuccess(XMLParser *parser)
{
	OP_ASSERT(GetLogic());
	
	if(!GetLogic())
		return;
		
	// Useless (for Opera Unite) network device
	if( (upnp->IsDebugging(UPNP_DBG_NO_IP_CONN)  || !((UPnPPortOpening *)GetLogic())->HasIPConnection()) && 
		(upnp->IsDebugging(UPNP_DBG_NO_PPP_CONN) || !((UPnPPortOpening *)GetLogic())->HasPPPConnection()))
	{
		NotifyFailure("No IP/PPP Connection");
	}
	else
	{
		if(OpStatus::IsError(((UPnPPortOpening *)GetLogic())->QueryIPRouted(GetDevice())))
			NotifyFailure("Error querying for IP_Routed device");
	}
}

OP_STATUS UPnPXH_DescriptionPortOpening::NotifyService(UPnPService *service)
{
	if(!service)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_ASSERT(GetLogic());
	
	if(!GetLogic())
		return OpStatus::ERR_NULL_POINTER;
		
	OpStringC *service_id=service->GetAttributeValue(UPnPService::SERVICE_ID);
	OpStringC *service_type=service->GetAttributeValue(UPnPService::SERVICE_TYPE);
	OpStringC *control_url=service->GetAttributeValue(UPnPService::CONTROL_URL);
	OpStringC *event_url=service->GetAttributeValue(UPnPService::EVENT_URL);
	
	if(!service_id || !service_type || !control_url || !event_url)
		return OpStatus::ERR_NULL_POINTER;
		
	OP_NEW_DBG("UPnPXH_DescriptionPortOpening::NotifyService", "upnp_trace_debug");
	
	if(!upnp->IsDebugging(UPNP_DBG_NO_IP_CONN) && !service_type->Compare(UPNP_WANIPCONNECTION))
	{
		OP_DBG((UNI_L("*** WanIPConnection (%s) Service URL: %s\nEvent URL: %s"), service_type->CStr(), control_url->CStr(), event_url->CStr()));
		//RETURN_IF_ERROR(upnp->SetIPService(service_type->CStr()));
		RETURN_IF_ERROR(GetDevice()->SetIPServiceURL(control_url->CStr()));
		RETURN_IF_ERROR(GetDevice()->SetIPEventURL(event_url->CStr()));
		((UPnPPortOpening *)GetLogic())->SetIPConnection(TRUE);
	}
	
	if(!upnp->IsDebugging(UPNP_DBG_NO_PPP_CONN) && !service_type->Compare(UPNP_WANPPPCONNECTION))
	{
		OP_DBG((UNI_L("*** WanPPPConnection (%s) Service UR): %s\nEvent URL: %s"), service_type->CStr(), control_url->CStr(), event_url->CStr()));
		//RETURN_IF_ERROR(upnp->SetPPPService(service_type->CStr()));
		RETURN_IF_ERROR(GetDevice()->SetPPPServiceURL(control_url->CStr()));
		RETURN_IF_ERROR(GetDevice()->SetPPPEventURL(event_url->CStr()));
		((UPnPPortOpening *)GetLogic())->SetPPPConnection(TRUE);
	}
		
	return OpStatus::OK;
}*/

void UPnPPortOpening::OnNewDevice(UPnPDevice *device)
{
	if(!device || !device->GetProvider())
		return;
		
	// CHeck the availability of IP and PP
	UPnPServicesProvider *prov=device->GetProvider();
	UPnPService *service;
	
	if(OpStatus::IsSuccess(prov->RetrieveServiceByName(UPNP_WANIPCONNECTION, service)))
	{
		RETURN_VOID_IF_ERROR(device->SetIPServiceURL(service->GetAttributeValueUni(UPnPService::CONTROL_URL)));
		RETURN_VOID_IF_ERROR(device->SetIPEventURL(service->GetAttributeValueUni(UPnPService::CONTROL_URL)));
		SetIPConnection(TRUE);
	}
	
	if(OpStatus::IsSuccess(prov->RetrieveServiceByName(UPNP_WANPPPCONNECTION, service)))
	{
		RETURN_VOID_IF_ERROR(device->SetPPPServiceURL(service->GetAttributeValueUni(UPnPService::CONTROL_URL)));
		RETURN_VOID_IF_ERROR(device->SetPPPEventURL(service->GetAttributeValueUni(UPnPService::CONTROL_URL)));
		SetPPPConnection(TRUE);
	}
	
	// Proceed with the GetLogic()
		
	// Useless (for Opera Unite) network device
	if( (upnp->IsDebugging(UPNP_DBG_NO_IP_CONN)  || !HasIPConnection()) && 
		(upnp->IsDebugging(UPNP_DBG_NO_PPP_CONN) || !HasPPPConnection()))
	{
		NotifyFailure("No IP/PPP Connection");
	}
	else
	{
		locked=TRUE;
		
		if(OpStatus::IsError(QueryIPRouted(device)))
			NotifyFailure("Error querying for IP_Routed device");
	}
}
#endif
