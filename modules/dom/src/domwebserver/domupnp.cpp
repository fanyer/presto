/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 1996-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Haavard Molland haavardm@opera.com
*/
#include "core/pch.h"

#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY

#include "modules/dom/src/domwebserver/domupnp.h"
#include "modules/dom/src/opera/dombytearray.h"
#include "modules/upnp/upnp_upnp.h"
#include "modules/formats/hdsplit.h"
#include "modules/formats/argsplit.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domcore/node.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/encodings/decoders/inputconverter.h"
#include "modules/util/datefun.h"
#include "modules/dom/src/opera/domio.h"

#ifdef DOM_GADGET_FILE_API_SUPPORT
# include "modules/dom/src/opera/domgadgetfile.h"
#endif // DOM_GADGET_FILE_API_SUPPORT

#include "modules/util/gen_str.h"

/* virtual */ void
DOM_DeviceDescriptor::GCTrace()
{
	GCMark(services);
}

/* static */ void
DOM_DeviceDescriptor::ConstructDeviceDescriptorL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "OFFLINE", 0, runtime);
	PutNumericConstantL(object, "ONLINE", 1, runtime);
}

// Create a Device Descriptor from a Services Provider
OP_STATUS DOM_DeviceDescriptor::Make(DOM_DeviceDescriptor *&dev_desc, DOM_Runtime *runtime, UPnPServicesProvider *prov)
{
	OpString unite_user;
	OpString unite_device;

	RETURN_IF_ERROR(prov->CopyAttributeValue(UPnPServicesProvider::UNITE_USER, unite_user));
	RETURN_IF_ERROR(prov->CopyAttributeValue(UPnPServicesProvider::UNITE_DEVICE, unite_device));

	// Get the URL of the Unite device
	URL url = g_url_api->GetURL(prov->GetDescriptionURL().CStr());

	if (!url.IsValid())
		return OpStatus::ERR_OUT_OF_RANGE;

	ServerName *sn = url.GetServerName();

	if (!sn)
		return OpStatus::ERR_OUT_OF_RANGE;

	unsigned short port = static_cast<unsigned short>(url.GetAttribute(URL::KServerPort));
	URLType url_type = url.Type();

	if (port == 0)
	{
		if (url_type == URL_HTTP)
			port = 80;
		else if (url_type == URL_HTTPS)
			port = 443;
		else
			return OpStatus::ERR_OUT_OF_RANGE;
	}

	// Create the device
	DOM_DeviceDescriptor *dd = OP_NEW(DOM_DeviceDescriptor, ());

	RETURN_IF_ERROR(DOMSetObjectRuntime(dd, runtime, runtime->GetPrototype(DOM_Runtime::DEVICEDESCRIPTOR_PROTOTYPE), "DeviceDescriptor"));

	RETURN_IF_ERROR(dd->url.AppendFormat(UNI_L("http%s://%s"), url_type == URL_HTTPS ? UNI_L("s") : UNI_L(""), sn->UniName()));

	if (url_type == URL_HTTPS && port != 443 ||
		url_type != URL_HTTPS && port != 80)
		RETURN_IF_ERROR(dd->url.AppendFormat(UNI_L(":%d"), port));

	RETURN_IF_ERROR(dd->id.AppendFormat(UNI_L("%s-%s"), unite_user.CStr(), unite_device.CStr()));

	RETURN_IF_ERROR(dd->description_url.Set(prov->GetDescriptionURL().CStr()));
	RETURN_IF_ERROR(prov->CopyAttributeValue(UPnPServicesProvider::MODEL_NAME, dd->name));
	RETURN_IF_ERROR(prov->CopyAttributeValue(UPnPServicesProvider::MODEL_DESCRIPTION, dd->description));
	RETURN_IF_ERROR(dd->uniteUser.Set(unite_user));
	RETURN_IF_ERROR(dd->uniteDeviceName.Set(unite_device));
	RETURN_IF_ERROR(prov->CopyAttributeValue(UPnPServicesProvider::DEVICE_ICON_RESOURCE, dd->iconresource));
	RETURN_IF_ERROR(prov->CopyAttributeValue(UPnPServicesProvider::PAYLOAD, dd->payload));
	dd->isLocal = TRUE;

	DOM_WebServerObjectArray *temp_array = OP_NEW(DOM_WebServerObjectArray, ());
	RETURN_IF_ERROR(DOMSetObjectRuntime(temp_array, runtime, runtime->GetPrototype(DOM_Runtime::UPNPCOLLECTION_PROTOTYPE), "ServiceListCollection"));
	dd->services = temp_array;

	// Initialize the services
	for (UINT i = 0; i < prov->GetNumberOfServices(); i++)
	{
		UPnPService *serv;
		DOM_ServiceDescriptor *desc;

		RETURN_IF_ERROR(prov->RetrieveService(i, serv));
		if (serv->IsVisible())
		{
			RETURN_IF_ERROR(DOM_ServiceDescriptor::Make(desc, dd, runtime, serv));
			RETURN_IF_ERROR(dd->services->Add(desc));
		}
	}

	dev_desc = dd;

	return OpStatus::OK;
}

DOM_UPnPCollection::DOM_UPnPCollection(const char* searchType)
	: UPnPLogic(UPnP::GetDOMUPnpObject(), searchType)
{
	OP_ASSERT(g_upnp);
	g_upnp->AddLogic(this, TRUE, TRUE);
	StartUPnPDiscovery();
}

DOM_UPnPCollection::~DOM_UPnPCollection()
{
	g_upnp->RemoveLogic(this);
}

void DOM_UPnPCollection::OnNewDevice(UPnPDevice *dev)
{
	OP_ASSERT(dev && dev->GetProvider());

	// If the device is already present, remove the old versions and create a new one (hopefully updated)
	for (UINT32 i = GetCount(); i > 0; i--)
	{
		DOM_DeviceDescriptor *item = Get(i-1);

		if (item && item->GetDescriptionURL() && *item->GetDescriptionURL() && !uni_strcmp(item->GetDescriptionURL(), dev->GetProvider()->GetDescriptionURL().CStr()))
			Remove(i-1);
	}

	DOM_DeviceDescriptor *dd;
	RETURN_VOID_IF_ERROR(DOM_DeviceDescriptor::Make(dd, GetRuntime(), dev->GetProvider()));
	Add(dd);
	for (OpListenersIterator iterator(m_devicelistlistener); m_devicelistlistener.HasNext(iterator);)
		m_devicelistlistener.GetNext(iterator)->DevicelistChanged();
}

void DOM_UPnPCollection::OnRemoveDevice(UPnPDevice *device)
{
	// Remove all the devices with the same URL (there should be one or zero...)
	if (device && device->GetProvider())
	{
		const uni_char *desc_dev = device->GetProvider()->GetDescriptionURL().CStr();

		if (!desc_dev)
			return;

		for (UINT32 i = GetCount(); i > 0; i--)
		{
			DOM_DeviceDescriptor *dd = Get(i-1);

			if (dd)
			{
				const uni_char *desc_cur = dd->GetDescriptionURL();

				if (desc_cur && !uni_strcmp(desc_dev, desc_cur))
				{
					Remove(i-1);
					for (OpListenersIterator iterator(m_devicelistlistener); m_devicelistlistener.HasNext(iterator);)
						m_devicelistlistener.GetNext(iterator)->DevicelistChanged();
				}
			}
		}
	}
}

DOM_DeviceDescriptor *DOM_UPnPCollection::Get(UINT32 idx)
{
	return static_cast<DOM_DeviceDescriptor *>(DOM_WebServerObjectArray::Get(idx));
}

OP_STATUS DOM_UPnPCollection::MakeItemCollectionDevices(DOM_UPnPCollection *&collection, DOM_Runtime *runtime, const char* searchType)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(collection = OP_NEW(DOM_UPnPCollection, (searchType)), runtime, runtime->GetPrototype(DOM_Runtime::UPNPCOLLECTION_PROTOTYPE), "DeviceListCollection"));

	// The webserver developers think that upnp, though it might not be NULL right now,
	// may some time in the future be NULL so we check it to be extra extra secure.
	if (!collection->upnp)
		return OpStatus::ERR_NULL_POINTER;

	return collection->upnp->AddLogic(collection, TRUE);
}

void DOM_UPnPCollection::GCTrace()
{
	DOM_WebServerObjectArray::GCTrace();
	for (OpListenersIterator iterator(m_devicelistlistener); m_devicelistlistener.HasNext(iterator);)
	{
		DOM_UPnPCollectionListener *listener = m_devicelistlistener.GetNext(iterator);
		GCMark(listener);
	}
}

ES_GetState DOM_DeviceDescriptor::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_id:
		DOMSetString(value, id.CStr());
		break;

	case OP_ATOM_url:
		DOMSetString(value, url.CStr());
		break;

	case OP_ATOM_name:
		DOMSetString(value, name.CStr());
		break;

	case OP_ATOM_description:
		DOMSetString(value, description.CStr());
		break;

	case OP_ATOM_isLocal:
		DOMSetBoolean(value, isLocal);
		break;

	case OP_ATOM_uniteUser:
		DOMSetString(value, uniteUser.CStr());
		break;

	case OP_ATOM_uniteDeviceName:
		DOMSetString(value, uniteDeviceName.CStr());
		break;

	case OP_ATOM_services:
		DOMSetObject(value, services);
		break;

	case OP_ATOM_status:
		DOMSetNumber(value, UPNP_DOM_ONLINE);
		break;

	case OP_ATOM_upnpDevicePayload:
		DOMSetString(value, payload.CStr());
		break;

	case OP_ATOM_upnpDeviceIcon:
		DOMSetString(value, iconresource.CStr());
		break;

	default:
		return GET_FAILED;
	}

	return GET_SUCCESS;
}

ES_PutState DOM_DeviceDescriptor::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	// All the attributes are read only
	switch (property_name)
	{
	case OP_ATOM_id:
	case OP_ATOM_url:
	case OP_ATOM_name:
	case OP_ATOM_description:
	case OP_ATOM_isLocal:
	case OP_ATOM_uniteUser:
	case OP_ATOM_services:
	case OP_ATOM_uniteDeviceName:
	case OP_ATOM_status:
	case OP_ATOM_upnpDevicePayload:
	case OP_ATOM_upnpDeviceIcon:
		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}

/* static */ void
DOM_ServiceDescriptor::ConstructServiceDescriptorL(ES_Object *object, DOM_Runtime *runtime)
{
	PutNumericConstantL(object, "OFFLINE", 0, runtime);
	PutNumericConstantL(object, "ONLINE", 1, runtime);
}


ES_GetState DOM_ServiceDescriptor::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_id:
		DOMSetString(value, id.CStr());
		break;

	case OP_ATOM_name:
		DOMSetString(value, name.CStr());
		break;

	case OP_ATOM_url:
		DOMSetString(value, url.CStr());
		break;

	case OP_ATOM_description:
		DOMSetString(value, description.CStr());
		break;

	case OP_ATOM_alternateURL:
		DOMSetString(value, alternateURL.CStr());
		break;

	case OP_ATOM_uniteServiceName:
		DOMSetString(value, uniteServiceName.CStr());
		break;

	case OP_ATOM_device:
		DOMSetObject(value, device);
		break;

	case OP_ATOM_status:
		DOMSetNumber(value, UPNP_DOM_ONLINE);
		break;

	default:
		return GET_FAILED;
	}

	return GET_SUCCESS;
}

ES_PutState DOM_ServiceDescriptor::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	// All the attributes are read only
	switch (property_name)
	{
	case OP_ATOM_id:
	case OP_ATOM_name:
	case OP_ATOM_url:
	case OP_ATOM_description:
	case OP_ATOM_alternateURL:
	case OP_ATOM_uniteServiceName:
	case OP_ATOM_device:
	case OP_ATOM_status:
		return PUT_READ_ONLY;
	}

	return PUT_FAILED;
}

OP_STATUS DOM_ServiceDescriptor::Make(DOM_ServiceDescriptor *&serv_desc, DOM_DeviceDescriptor *dev_desc, DOM_Runtime *runtime, UPnPService *serv)
{
	OP_ASSERT(dev_desc);

	DOM_ServiceDescriptor *sd = OP_NEW(DOM_ServiceDescriptor, ());

	RETURN_IF_ERROR(DOMSetObjectRuntime(sd, runtime, runtime->GetPrototype(DOM_Runtime::SERVICEDESCRIPTOR_PROTOTYPE), "ServiceDescriptor"));

	sd->device = dev_desc;
	RETURN_IF_ERROR(sd->url.AppendFormat(UNI_L("%s%s"), dev_desc->GetURL(), serv->GetAttributeValue(UPnPService::CONTROL_URL)->CStr()));

	RETURN_IF_ERROR(serv->CopyAttributeValue(UPnPService::SERVICE_ID, sd->id));
	RETURN_IF_ERROR(serv->CopyAttributeValue(UPnPService::NAME, sd->name));

	RETURN_IF_ERROR(serv->CopyAttributeValue(UPnPService::DESCRIPTION, sd->description));
	RETURN_IF_ERROR(serv->CopyAttributeValue(UPnPService::CONTROL_URL, sd->alternateURL));
	RETURN_IF_ERROR(serv->CopyAttributeValue(UPnPService::NAME, sd->uniteServiceName));

	serv_desc = sd;

	return OpStatus::OK;
}

OP_STATUS DOM_UPnPCollectionListener::Listen()
{
	return m_upnp_collection->AddDevicelistListener(this);
}

void DOM_UPnPCollectionListener::DevicelistChanged()
{
	ES_Value argv[1]; // ARRAY OK 2012-02-29 andre
	argv[0].value.object =  DOM_Utils::GetES_Object(m_upnp_collection);
	argv[0].type = VALUE_OBJECT;
	OpStatus::Ignore(m_ai->CallFunction(m_callback, NULL, 1, argv));
}
#endif // UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY
