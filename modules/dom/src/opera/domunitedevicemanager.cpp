/* -*- mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
**
** Copyright (C) 2010-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#include "core/pch.h"

#ifdef DOM_UNITEDEVMANAGER_SUPPORT

#include "modules/dom/src/opera/domunitedevicemanager.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/webserver/webserver-api.h"
#include "modules/prefs/prefsmanager/collections/pc_webserver.h"

// Values for range.
enum DeviceRangeState
{
	RANGE_INACTIVE,			///< Running only on the Opera Instance. No content is shared.
	RANGE_LOCAL_NETWORK,	///< Running on the local network, via ip:port combination.
	RANGE_PROXY_ATTEMPT,	///< Attempting to run via Opera's proxy.
	RANGE_UNITE_PROXY		///< Running via Opera's proxy, only possible with a valid account and deviceName.
};

// Values for status.
#define UNITE_STATUS_USER_IS_BANNED 10
#define UNITE_STATUS_COMMUNICATION_ABORTED 11
#define UNITE_STATUS_SUCCESS 20
#define UNITE_STATUS_BAD_REQUEST 40
#define UNITE_STATUS_PROXY_AUTHENTICATION_REQUIRED 41
#define UNITE_STATUS_REQUEST_TIMEOUT 42
#define UNITE_STATUS_PARSING_ERROR 43
#define UNITE_STATUS_OUT_OF_MEMORY 44
#define UNITE_STATUS_DEVICE_NAME_IS_INVALID 57
#define UNITE_STATUS_DEVICE_NAME_ALREADY_IN_USE 58

/* static */ void
DOM_UniteDeviceManager::MakeL(DOM_UniteDeviceManager *&new_obj, DOM_Runtime *origining_runtime)
{
	new_obj = OP_NEW_L(DOM_UniteDeviceManager, ());
	LEAVE_IF_ERROR(DOMSetObjectRuntime(new_obj, origining_runtime, origining_runtime->GetPrototype(DOM_Runtime::UNITEDEVICEMANAGER_PROTOTYPE), "UniteDeviceManager"));
	new_obj->ConstructL();
}

DOM_UniteDeviceManager::DOM_UniteDeviceManager()
	: m_onrangechange(NULL)
{
}

DOM_UniteDeviceManager::~DOM_UniteDeviceManager()
{
	if (g_opera_account_manager) // This may happen after OperaAuthModule::Destroy().
		g_opera_account_manager->RemoveListener(this);
}

#define PUT_RANGE_CODE_L(code) PutNumericConstantL(object, #code, code, runtime)
#define PUT_STATUS_CODE_L(code) PutNumericConstantL(object, #code, UNITE_STATUS_##code, runtime)

OP_STATUS
DOM_UniteDeviceManager::ConstructL()
{
	// Make error constants available.
	ES_Object *object = GetNativeObject();
	DOM_Runtime *runtime = GetRuntime();

	// Range constants.
	PUT_RANGE_CODE_L(RANGE_INACTIVE);
	PUT_RANGE_CODE_L(RANGE_LOCAL_NETWORK);
	PUT_RANGE_CODE_L(RANGE_PROXY_ATTEMPT);
	PUT_RANGE_CODE_L(RANGE_UNITE_PROXY);

	// Status constants.
	PUT_STATUS_CODE_L(USER_IS_BANNED);
	PUT_STATUS_CODE_L(COMMUNICATION_ABORTED);
	PUT_STATUS_CODE_L(SUCCESS);
	PUT_STATUS_CODE_L(BAD_REQUEST);
	PUT_STATUS_CODE_L(PROXY_AUTHENTICATION_REQUIRED);
	PUT_STATUS_CODE_L(REQUEST_TIMEOUT);
	PUT_STATUS_CODE_L(PARSING_ERROR);
	PUT_STATUS_CODE_L(OUT_OF_MEMORY);
	PUT_STATUS_CODE_L(DEVICE_NAME_IS_INVALID);
	PUT_STATUS_CODE_L(DEVICE_NAME_ALREADY_IN_USE);

	RETURN_IF_ERROR(g_opera_account_manager->AddListener(this));

	return OpStatus::OK;
}

#undef PUT_RANGE_CODE_L
#undef PUT_STATUS_CODE_L

/** Helper method to map an OpAtom to a data type and
  * preference constant. */
static void
OpAtom_to_webserverpref(OpAtom atom, ES_Value_Type *type, PrefsCollectionWebserver::integerpref *pref)
{
	switch (atom)
	{
	case OP_ATOM_maxUploadRate:
		*type = VALUE_NUMBER;
		*pref = PrefsCollectionWebserver::WebserverUploadRate;
		return;

	case OP_ATOM_port:
		*type = VALUE_NUMBER;
		*pref = PrefsCollectionWebserver::WebserverPort;
		return;

	case OP_ATOM_rendezvous:
		*type = VALUE_BOOLEAN;
		*pref = PrefsCollectionWebserver::ServiceDiscoveryEnabled;
		return;

	case OP_ATOM_upnpEnabled:
		*type = VALUE_BOOLEAN;
		*pref = PrefsCollectionWebserver::UPnPEnabled;
		return;

	case OP_ATOM_robotstxtEnabled:
		*type = VALUE_BOOLEAN;
		*pref = PrefsCollectionWebserver::RobotsTxtEnabled;
		return;
	}

	*type = VALUE_UNDEFINED;
	*pref = PrefsCollectionWebserver::DummyLastIntegerPref;
}

/* virtual */ void
DOM_UniteDeviceManager::GCTrace()
{
	GCMark(m_onrangechange);
}

// Attributes declared on the UniteDeviceManager API.
/*
    readonly attribute unsigned short range;
    readonly attribute unsigned short status;
             attribute Function       onrangechange;
    readonly attribute DOMString      device;
             attribute int            maxUploadRate;
             attribute int            port;
             attribute boolean        rendezvous;
             attribute boolean        upnpEnabled;
             attribute boolean        robotstxtEnabled;
*/

/**
 * Implementation of reading attributes on the UniteDeviceManager API.
 *
 */
ES_GetState
DOM_UniteDeviceManager::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	case OP_ATOM_range:
		{
			WebserverListeningMode mode = g_webserver->GetCurrentMode();
			unsigned short rangeval = RANGE_INACTIVE;
			if (mode & WEBSERVER_LISTEN_PROXY)
			{
				if (g_webserver->GetRendezvousConnected())
					rangeval = RANGE_UNITE_PROXY;
				else
					rangeval = RANGE_PROXY_ATTEMPT;
			}
			else if (mode & WEBSERVER_LISTEN_LOCAL && g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks))
				rangeval = RANGE_LOCAL_NETWORK;
			DOMSetNumber(value, rangeval);
			return GET_SUCCESS;
		}

	case OP_ATOM_hostName:
		DOMSetString(value, g_webserver->GetWebserverUri());
		return GET_SUCCESS;

	case OP_ATOM_status:
		DOMSetNumber(value, g_opera_account_manager->LastError());
		return GET_SUCCESS;

	case OP_ATOM_onrangechange:
		DOMSetObject(value, m_onrangechange);
		return GET_SUCCESS;

	case OP_ATOM_device:
		DOMSetString(value, g_pcwebserver->GetStringPref(PrefsCollectionWebserver::WebserverDevice).CStr());
		return GET_SUCCESS;

	case OP_ATOM_maxUploadRate:
	case OP_ATOM_port:
	case OP_ATOM_rendezvous:
	case OP_ATOM_upnpEnabled:
	case OP_ATOM_robotstxtEnabled:
		{
			ES_Value_Type type;
			PrefsCollectionWebserver::integerpref pref;
			OpAtom_to_webserverpref(property_name, &type, &pref);
			if (type == VALUE_BOOLEAN)
				DOMSetBoolean(value, g_pcwebserver->GetIntegerPref(pref));
			else if (type == VALUE_NUMBER)
				DOMSetNumber(value, g_pcwebserver->GetIntegerPref(pref));
			return GET_SUCCESS;
		}
	}

	return DOM_Object::GetName(property_name, value, origining_runtime);
}

/**
 * Implementation of writing attributes on the UniteDeviceManager API.
 *
 */
ES_PutState
DOM_UniteDeviceManager::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	switch (property_name)
	{
	// Read-only attributes.
	case OP_ATOM_range:
	case OP_ATOM_hostName:
	case OP_ATOM_status:
	case OP_ATOM_device:
		return PUT_READ_ONLY;

	// Read-write attributes.
	case OP_ATOM_onrangechange:
		if (value->type != VALUE_OBJECT)
			return PUT_SUCCESS;
		m_onrangechange = value->value.object;
		return PUT_SUCCESS;

	case OP_ATOM_maxUploadRate:
	case OP_ATOM_port:
	case OP_ATOM_rendezvous:
	case OP_ATOM_upnpEnabled:
	case OP_ATOM_robotstxtEnabled:
		{
			ES_Value_Type typeneeded;
			PrefsCollectionWebserver::integerpref pref;

			OpAtom_to_webserverpref(property_name, &typeneeded, &pref);
			OP_ASSERT(typeneeded != VALUE_UNDEFINED);

			// Check that we have the type we need.
			if (value->type != typeneeded)
				return typeneeded == VALUE_BOOLEAN ? PUT_NEEDS_BOOLEAN : PUT_NEEDS_NUMBER;

			// Go on and write.
			TRAPD(rc, g_pcwebserver->WriteIntegerL(pref, typeneeded == VALUE_BOOLEAN ? value->value.boolean : static_cast<int>(value->value.number)));

			if (OpStatus::IsSuccess(rc) && property_name != OP_ATOM_maxUploadRate)
				OpStatus::Ignore(g_webserver->ChangeListeningMode(WEBSERVER_LISTEN_DEFAULT));

			return PUT_SUCCESS;
		}
	}

	// Unhandled, perhaps someone else knows what to do?
	return DOM_Object::PutName(property_name, value, origining_runtime);
}

/**
 * Implementation of the UniteDeviceManager::setRange DOM interface.
 *
 * void setRange (in Unsigned Short newRange, in optional DOMString
 * deviceName, in optional Boolean forceDeviceName);
 */
/* static */ int
DOM_UniteDeviceManager::setRange(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_UNITEDEVICEMANAGER);

	DOM_CHECK_ARGUMENTS("n|sb");

	// Change device range.
	int new_range = (int)argv[0].value.number;
	WebserverListeningMode mode = g_webserver->GetCurrentMode();

	unsigned short current_range = RANGE_INACTIVE;
	if (mode & WEBSERVER_LISTEN_PROXY)
		current_range = RANGE_UNITE_PROXY;
	else if (mode & WEBSERVER_LISTEN_LOCAL)
	{
		if (g_pcwebserver->GetIntegerPref(PrefsCollectionWebserver::WebserverListenToAllNetworks))
			current_range = RANGE_LOCAL_NETWORK;
		else
			current_range = RANGE_INACTIVE;
	}

	if (current_range != new_range)
	{
		TRAPD(err,
			if (new_range == RANGE_INACTIVE)
			{
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::UseOperaAccount, 0);
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverListenToAllNetworks, 0);
			}
			else if (new_range == RANGE_LOCAL_NETWORK)
			{
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::UseOperaAccount, 0);
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverListenToAllNetworks, 1);
			}
			else if (new_range == RANGE_UNITE_PROXY && g_opera_account_manager->IsLoggedIn())
			{
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::UseOperaAccount, 1);
				g_pcwebserver->WriteIntegerL(PrefsCollectionWebserver::WebserverListenToAllNetworks, 1);
			}
		);
	}

	if (argc > 1)
	{
		//Change device name and range.
		BOOL force_device_name = FALSE;
		if (argc > 2)
			force_device_name = argv[2].value.boolean;
		if (*argv[1].value.string)
		{
			g_opera_account_manager->RegisterDevice(argv[1].value.string, force_device_name);
			return ES_FAILED;
		}
	}

	CALL_FAILED_IF_ERROR(g_webserver->ChangeListeningMode(WEBSERVER_LISTEN_DEFAULT));

	return ES_FAILED;
}

/**
 * Implementation of the UniteDeviceManager::resetProperty DOM interface.
 *
 * void resetProperty (in DOMString propertyName);
 */
/* static */ int
DOM_UniteDeviceManager::resetProperty(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT_UNUSED(DOM_TYPE_UNITEDEVICEMANAGER);
	DOM_CHECK_ARGUMENTS("s");

	OpAtom atom = DOM_StringToAtom(argv[0].value.string);

	ES_Value_Type type = VALUE_UNDEFINED;
	PrefsCollectionWebserver::integerpref pref;
	OpAtom_to_webserverpref(atom, &type, &pref);
	if (type != VALUE_UNDEFINED)
	{
		TRAPD(rc, g_pcwebserver->ResetIntegerL(pref));
		if (OpStatus::IsMemoryError(rc))
			return ES_NO_MEMORY;
	}

	return ES_FAILED;
}

// Functions declared in the UniteDeviceManager API.
DOM_FUNCTIONS_START(DOM_UniteDeviceManager)
	DOM_FUNCTIONS_FUNCTION(DOM_UniteDeviceManager, DOM_UniteDeviceManager::setRange, "setRange", "nsb")
	DOM_FUNCTIONS_FUNCTION(DOM_UniteDeviceManager, DOM_UniteDeviceManager::resetProperty, "resetProperty", "s")
DOM_FUNCTIONS_END(DOM_UniteDeviceManager)

/* virtual */ void
DOM_UniteDeviceManager::OnAccountDeviceRegisteredChange()
{
	DOM_Runtime *runtime = GetRuntime();
	ES_AsyncInterface *asyncif = runtime->GetEnvironment()->GetAsyncInterface();

	if (!m_onrangechange)
		return;

	if (op_strcmp(ES_Runtime::GetClass(m_onrangechange), "Function") == 0)
	{
		asyncif->CallFunction(m_onrangechange, NULL, 0, NULL, NULL);
		return;
	}

	// Make a dummy Event argument.
	ES_Value arguments[1];
	DOM_Event *event = OP_NEW(DOM_Event, ());
	RETURN_VOID_IF_ERROR(DOMSetObjectRuntime(event, runtime, runtime->GetPrototype(DOM_Runtime::EVENT_PROTOTYPE), "DeviceChangeEvent"));
	event->InitEvent(DOM_EVENT_CUSTOM, NULL);
	RETURN_VOID_IF_ERROR(event->SetType(UNI_L("DeviceChangeEvent")));
	DOM_Object::DOMSetObject(&arguments[0], event);

	asyncif->CallMethod(m_onrangechange, UNI_L("handleEvent"), 1, arguments, NULL);
}

#endif // DOM_UNITEDEVMANAGER_SUPPORT
