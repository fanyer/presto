/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2006-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** @author Haavard Molland haavardm@opera.com
*/

#ifndef DOM_DOMUPNP_H
#define DOM_DOMUPNP_H

#if defined UPNP_SUPPORT && defined UPNP_SERVICE_DISCOVERY

#include "modules/dom/domenvironment.h"
#include "modules/dom/src/domwebserver/domwebserver.h"
#include "modules/upnp/upnp_upnp.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/util/adt/oplisteners.h"

class UPnPServicesProvider;
class UPnPService;
class UPnP;
class DOM_DeviceDescriptor;
class DOM_UPnPCollection;

/// Simple listener for changes in DOM_UPnPCollection::m_objects
class DOM_UPnPCollectionListener : public DOM_Object
{
private:
	ES_AsyncInterface *m_ai;
	ES_Object *m_callback;
	DOM_UPnPCollection *m_upnp_collection;
public:
	DOM_UPnPCollectionListener(ES_AsyncInterface *ai, ES_Object *callback, DOM_UPnPCollection *collection)
	    : m_ai(ai),
	      m_callback(callback),
	      m_upnp_collection(collection)
	{}

	~DOM_UPnPCollectionListener() { }

	OP_STATUS Listen();

	/**
	 * Called whenever the list of devices changes
	 *
	 */
	void DevicelistChanged();

	virtual void GCTrace() { GCMark(m_callback); }

};

/// Collection used for UPnP
class DOM_UPnPCollection: public DOM_WebServerObjectArray, public UPnPLogic
{
	OpListeners<DOM_UPnPCollectionListener> m_devicelistlistener;
public:
	DOM_UPnPCollection(const char* searchType);
	virtual ~DOM_UPnPCollection();

	/// Create the collection of all the Device descriptors available
	static OP_STATUS MakeItemCollectionDevices(DOM_UPnPCollection *&coll, DOM_Runtime *runtime, const char* searchType);

	virtual OP_STATUS HandlerFailed(UPnPXH_Auto *child, const char *mex) { return OpStatus::OK; }
	virtual void OnNewDevice(UPnPDevice *device);
	virtual void OnRemoveDevice(UPnPDevice *device);

	/// Getter() with casting
	DOM_DeviceDescriptor *Get(UINT32 idx);

	virtual void GCTrace();

	OP_STATUS AddDevicelistListener(DOM_UPnPCollectionListener *listener) { return m_devicelistlistener.Add(listener); }
	OP_STATUS RemoveDevicelistListener(DOM_UPnPCollectionListener* listener) { return m_devicelistlistener.Remove(listener); }
};

// Device / Service online
#define UPNP_DOM_ONLINE 1
// Device / Service offline
#define UPNP_DOM_OFFLINE 0

/// Generic Opera Unite Device
class DOM_DeviceDescriptor: public DOM_Object
{
private:
	// Readonly attributes
	OpString id;					///< ID
	OpString url;					///< Device URL
	OpString description_url;		///< Description URL
	OpString name;					///< Name
	OpString description;			///< Description
	BOOL isLocal;					///< TRUE if it is a local service provider
	OpString uniteUser;				///< Unite login User Name
	OpString uniteDeviceName;		///< Unite Device Name
	OpString payload;				///< Payload string
	OpString iconresource;			///< URI to image

	DOM_WebServerObjectArray *services;	///< List of services available

public:
	DOM_DeviceDescriptor(): services(NULL) { }
	/// Create a single Device Descriptor from a Services Provider
	static OP_STATUS Make(DOM_DeviceDescriptor *&dev_desc, DOM_Runtime *runtime, UPnPServicesProvider *prov);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_UPNP_DEVICE_DESCRIPTOR || DOM_Object::IsA(type); }
	/// Return the URL of the device
	uni_char *GetURL() { return url.CStr(); }
	virtual void GCTrace();

	/// Initialize constants
	static void ConstructDeviceDescriptorL(ES_Object *object, DOM_Runtime *runtime);

	/// Return the unique ID
	uni_char *GetID() { return id.CStr(); }
	/// Return the description URL
	uni_char *GetDescriptionURL() { return description_url.CStr(); }
};

/// Generic Opera Unite Service
class DOM_ServiceDescriptor: public DOM_Object
{
private:
	// Readonly attributes
	OpString id;					///< ID
	OpString name;					///< Name
	OpString url;					///< Device URL
	OpString alternateURL;			///< Alternative URL
	OpString description;			///< Description
	OpString uniteServiceName;		///< Unite Service Name

	DOM_DeviceDescriptor *device;	///< Device providing the services

	/// Default constructor
	DOM_ServiceDescriptor(): device(NULL) { }

public:
	/// Create a single Service Descriptor from a Service
	static OP_STATUS Make(DOM_ServiceDescriptor *&serv_desc, DOM_DeviceDescriptor *dev_desc, DOM_Runtime *runtime, UPnPService *serv);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual BOOL IsA(int type) { return type == DOM_TYPE_UPNP_SERVICE_DESCRIPTOR || DOM_Object::IsA(type); }

	/// Initialize constants
	static void ConstructServiceDescriptorL(ES_Object *object, DOM_Runtime *runtime);

	/// Return the unique ID
	uni_char *GetID() { return id.CStr(); }
};

#endif // UPNP_SUPPORT && UPNP_SERVICE_DISCOVERY
#endif // DOM_DOMUPNP_H
