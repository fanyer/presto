/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#include <core/pch.h>

#ifdef DAPI_JIL_SUPPORT

#include "modules/device_api/jil/JILFeatures.h"

#include "modules/gadgets/OpGadget.h"
#include "modules/gadgets/OpGadgetClass.h"

#define JIL_FEATURE_URI_ACCELEROMETERINFO                    ("http://jil.org/jil/api/1.1/accelerometerinfo")
#define JIL_FEATURE_URI_ACCOUNT                              ("http://jil.org/jil/api/1.1/account")
#define JIL_FEATURE_URI_ACCOUNTINFO                          ("http://jil.org/jil/api/1.1/accountinfo")
#define JIL_FEATURE_URI_ADDRESSBOOKITEM                      ("http://jil.org/jil/api/1.1/addressbookitem")
#define JIL_FEATURE_URI_APPLICATIONTYPES                     ("http://jil.org/jil/api/1.1.5/applicationtypes")
#define JIL_FEATURE_URI_ATTACHMENT                           ("http://jil.org/jil/api/1.1/attachment")
#define JIL_FEATURE_URI_AUDIOPLAYER                          ("http://jil.org/jil/api/1.1/audioplayer")
#define JIL_FEATURE_URI_CALENDARITEM                         ("http://jil.org/jil/api/1.1/calendaritem")
#define JIL_FEATURE_URI_CALLRECORD                           ("http://jil.org/jil/api/1.1/callrecord")
#define JIL_FEATURE_URI_CALLRECORDTYPES                      ("http://jil.org/jil/api/1.1.1/callrecordtypes")
#define JIL_FEATURE_URI_CAMERA                               ("http://jil.org/jil/api/1.1.2/camera")
#define JIL_FEATURE_URI_CLIPBOARD                            ("http://jil.org/jil/api/1.1/clipboard")
#define JIL_FEATURE_URI_CONFIG                               ("http://jil.org/jil/api/1.1/config")
#define JIL_FEATURE_URI_DATANETWORKCONNECTIONTYPES           ("http://jil.org/jil/api/1.1.5/datanetworkconnectiontypes")
#define JIL_FEATURE_URI_DATANETWORKINFO                      ("http://jil.org/jil/api/1.1.1/datanetworkinfo")
#define JIL_FEATURE_URI_DEVICEINFO                           ("http://jil.org/jil/api/1.1/deviceinfo")
#define JIL_FEATURE_URI_DEVICESTATEINFO                      ("http://jil.org/jil/api/1.1/devicestateinfo")
#define JIL_FEATURE_URI_DEVICE_1_1                           ("http://jil.org/jil/api/1.1/device")
#define JIL_FEATURE_URI_DEVICE_1_2                           ("http://jil.org/jil/api/1.2/device")	// The 1.2 version is for compatibility only
#define JIL_FEATURE_URI_EVENTRECURRENCETYPES                 ("http://jil.org/jil/api/1.1/eventrecurrencetypes")
#define JIL_FEATURE_URI_EXCEPTION                            ("http://jil.org/jil/api/1.1.5/exception")
#define JIL_FEATURE_URI_EXCEPTIONTYPES                       ("http://jil.org/jil/api/1.1.5/exceptiontypes")
#define JIL_FEATURE_URI_FILE                                 ("http://jil.org/jil/api/1.1.1/file")
#define JIL_FEATURE_URI_MESSAGE                              ("http://jil.org/jil/api/1.1/message")
#define JIL_FEATURE_URI_MESSAGEFOLDERTYPES                   ("http://jil.org/jil/api/1.1.4/messagefoldertypes")
#define JIL_FEATURE_URI_MESSAGEQUANTITIES                    ("http://jil.org/jil/api/1.1/messagequantities")
#define JIL_FEATURE_URI_MESSAGETYPES                         ("http://jil.org/jil/api/1.1/messagetypes")
#define JIL_FEATURE_URI_MESSAGING                            ("http://jil.org/jil/api/1.1/messaging")
#define JIL_FEATURE_URI_MULTIMEDIA                           ("http://jil.org/jil/api/1.1/multimedia")
#define JIL_FEATURE_URI_PIM                                  ("http://jil.org/jil/api/1.1.1/pim")
#define JIL_FEATURE_URI_POSITIONINFO                         ("http://jil.org/jil/api/1.1/positioninfo")
#define JIL_FEATURE_URI_POWERINFO                            ("http://jil.org/jil/api/1.1/powerinfo")
#define JIL_FEATURE_URI_RADIOINFO                            ("http://jil.org/jil/api/1.1.1/radioinfo")
#define JIL_FEATURE_URI_RADIOSIGNALSOURCETYPES               ("http://jil.org/jil/api/1.1.5/radiosignalsourcetypes")
#define JIL_FEATURE_URI_TELEPHONY                            ("http://jil.org/jil/api/1.1.1/telephony")
#define JIL_FEATURE_URI_VIDEOPLAYER                          ("http://jil.org/jil/api/1.1.2/videoplayer")
#define JIL_FEATURE_URI_WIDGETMANAGER                        ("http://jil.org/jil/api/1.1.1/widgetmanager")
#define JIL_FEATURE_URI_WIDGET_1_1                           ("http://jil.org/jil/api/1.1/widget")
#define JIL_FEATURE_URI_WIDGET_1_2                           ("http://jil.org/jil/api/1.2/widget")	// The 1.2 version is for compatibility only
#define JIL_FEATURE_URI_XMLHTTPREQUEST                       ("http://jil.org/jil/api/1.1/xmlHttpRequest")

// Old JIL version.
#define JIL_FEATURE_URI_ACCOUNTINFO_1_1                      ("http://jil.org/jil/api/1.1/accountinfo")
#define JIL_FEATURE_URI_ADDRESSBOOK_1_1                      ("http://jil.org/jil/api/1.1/addressbook")
#define JIL_FEATURE_URI_APPLICATION_1_1                      ("http://jil.org/jil/api/1.1/application")
#define JIL_FEATURE_URI_CALENDAR_1_1                         ("http://jil.org/jil/api/1.1/calendar")
#define JIL_FEATURE_URI_CAMERA_1_1                           ("http://jil.org/jil/api/1.1/camera")
#define JIL_FEATURE_URI_CLIPBOARD_1_1                        ("http://jil.org/jil/api/1.1/clipboard")
#define JIL_FEATURE_URI_CONFIG_1_1                           ("http://jil.org/jil/api/1.1/config")
#define JIL_FEATURE_URI_DEVICEINFO_1_1                       ("http://jil.org/jil/api/1.1/deviceinfo")
#define JIL_FEATURE_URI_FILESYSTEM_1_1                       ("http://jil.org/jil/api/1.1/filesystem")
#define JIL_FEATURE_URI_LOCATION_1_1                         ("http://jil.org/jil/api/1.1/location")
#define JIL_FEATURE_URI_MESSAGING_1_1                        ("http://jil.org/jil/api/1.1/messaging")
#define JIL_FEATURE_URI_TELEPHONY_1_1                        ("http://jil.org/jil/api/1.1/telephony")
#define JIL_FEATURE_URI_URL_1_1                              ("http://jil.org/jil/api/1.1/url")
#define JIL_FEATURE_URI_WIDGETMANAGER_1_1                    ("http://jil.org/jil/api/1.1/widgetmanager")
#define JIL_FEATURE_URI_XMLHTTPREQUEST_1_1                   ("http://jil.org/jil/api/1.1/xmlhttprequest")


/* static */ BOOL
JILFeatures::IsJILApiRequested(JILFeatures::JIL_Api api_name, OpGadget* gadget)
{
	OP_ASSERT(gadget->GetClass());
	OpGadgetClass* gadget_class = gadget->GetClass();
	if (gadget_class->SupportsNamespace(GADGETNS_JIL_1_0) && !gadget_class->SupportsNamespace(GADGETNS_W3C_1_0))
	{
		// in old packaging all object are available - feature check is beeing done only in SecurityCheck
		return TRUE;
	}
	if (gadget->GetClass()->SupportsNamespace(GADGETNS_W3C_1_0))
	{
		switch (api_name)
		{
		case JIL_API_ACCELEROMETERINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_ACCELEROMETERINFO);
		case JIL_API_ACCOUNT:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_ACCOUNT);
		case JIL_API_ACCOUNTINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_ACCOUNTINFO);
		case JIL_API_ADDRESSBOOKITEM:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_ADDRESSBOOKITEM);
		case JIL_API_APPLICATIONTYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_APPLICATIONTYPES);
		case JIL_API_ATTACHMENT:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_ATTACHMENT);
		case JIL_API_AUDIOPLAYER:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_AUDIOPLAYER);
		case JIL_API_CALENDARITEM:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_CALENDARITEM)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_EVENTRECURRENCETYPES, gadget)
#endif
		;
		case JIL_API_CALLRECORD:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_CALLRECORD);
		case JIL_API_CALLRECORDTYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_CALLRECORDTYPES);
		case JIL_API_CAMERA:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_CAMERA);
		case JIL_API_CONFIG:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_CONFIG);
		case JIL_API_DATANETWORKCONNECTIONTYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_DATANETWORKCONNECTIONTYPES);
		case JIL_API_DATANETWORKINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_DATANETWORKINFO)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_DATANETWORKCONNECTIONTYPES, gadget)
#endif
			;
		case JIL_API_DEVICE:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_DEVICE_1_1)
				|| gadget->IsFeatureRequested(JIL_FEATURE_URI_DEVICE_1_2)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_ACCOUNTINFO, gadget)
				|| IsJILApiRequested(JIL_API_APPLICATIONTYPES, gadget)
				|| IsJILApiRequested(JIL_API_DATANETWORKINFO, gadget)
				|| IsJILApiRequested(JIL_API_DEVICEINFO, gadget)
				|| IsJILApiRequested(JIL_API_DEVICESTATEINFO, gadget)
				|| IsJILApiRequested(JIL_API_FILE, gadget)
				|| IsJILApiRequested(JIL_API_POWERINFO, gadget)
				|| IsJILApiRequested(JIL_API_RADIOINFO, gadget)
#endif
			;
		case JIL_API_DEVICEINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_DEVICEINFO);
		case JIL_API_DEVICESTATEINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_DEVICESTATEINFO)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_CONFIG, gadget)
			    || IsJILApiRequested(JIL_API_POSITIONINFO, gadget)
				|| IsJILApiRequested(JIL_API_ACCELEROMETERINFO, gadget)
#endif
			;
		case JIL_API_EVENTRECURRENCETYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_EVENTRECURRENCETYPES);
		case JIL_API_EXCEPTION:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_EXCEPTION);
		case JIL_API_EXCEPTIONTYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_EXCEPTIONTYPES);
		case JIL_API_FILE:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_FILE);
		case JIL_API_MESSAGE:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_MESSAGE);
		case JIL_API_MESSAGEFOLDERTYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_MESSAGEFOLDERTYPES);
		case JIL_API_MESSAGEQUANTITIES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_MESSAGEQUANTITIES);
		case JIL_API_MESSAGETYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_MESSAGETYPES);
		case JIL_API_MESSAGING:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_MESSAGING)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_MESSAGE, gadget)
				|| IsJILApiRequested(JIL_API_MESSAGEFOLDERTYPES, gadget)
				|| IsJILApiRequested(JIL_API_MESSAGEQUANTITIES, gadget)
				|| IsJILApiRequested(JIL_API_MESSAGETYPES, gadget)
				|| IsJILApiRequested(JIL_API_ATTACHMENT, gadget)
#endif
			;
		case JIL_API_MULTIMEDIA:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_MULTIMEDIA)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_AUDIOPLAYER, gadget)
				|| IsJILApiRequested(JIL_API_CAMERA, gadget)
				|| IsJILApiRequested(JIL_API_VIDEOPLAYER, gadget)
#endif
			;
		case JIL_API_PIM:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_PIM)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_ADDRESSBOOKITEM, gadget)
				|| IsJILApiRequested(JIL_API_CALENDARITEM, gadget)
#endif
			;
		case JIL_API_POSITIONINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_POSITIONINFO);
		case JIL_API_POWERINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_POWERINFO);
		case JIL_API_RADIOINFO:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_RADIOINFO)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_RADIOSIGNALSOURCETYPES, gadget)
#endif
			;
		case JIL_API_RADIOSIGNALSOURCETYPES:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_RADIOSIGNALSOURCETYPES);
		case JIL_API_TELEPHONY:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_TELEPHONY)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_CALLRECORD, gadget)
				|| IsJILApiRequested(JIL_API_CALLRECORDTYPES, gadget)
#endif
			;
		case JIL_API_VIDEOPLAYER:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_VIDEOPLAYER);
		case JIL_API_WIDGET:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_WIDGET_1_1)
				|| gadget->IsFeatureRequested(JIL_FEATURE_URI_WIDGET_1_2)
#ifdef JIL_FEATURES_IMPLY_PARENTS
			    || IsJILApiRequested(JIL_API_DEVICE, gadget)
				|| IsJILApiRequested(JIL_API_PIM, gadget)
				|| IsJILApiRequested(JIL_API_MESSAGING, gadget)
				|| IsJILApiRequested(JIL_API_EXCEPTION, gadget)
				|| IsJILApiRequested(JIL_API_EXCEPTIONTYPES, gadget)
				|| IsJILApiRequested(JIL_API_MULTIMEDIA, gadget)
				|| IsJILApiRequested(JIL_API_TELEPHONY, gadget)
#endif
			;
		case JIL_API_WIDGETMANAGER:
			return gadget->IsFeatureRequested(JIL_FEATURE_URI_WIDGETMANAGER);
		case JIL_API_REVERSE_SET_PREFERENCE_FOR_KEY_ARGS:
			              // JIL ppl say that namespace should decide about it not feature :/
			return FALSE; // ==> gadget_ns == GADGETNS_JIL_1_0
		default:
			OP_ASSERT(FALSE); //
			return FALSE;
		}
	}
	// no other namespaces supported by jil
	return FALSE;
}


/* static */ const char*
JILFeatures::GetFeatureURI(JILFeatures::JIL_Api api_name)
{
	switch (api_name)
	{
		case JIL_API_ACCELEROMETERINFO:              return JIL_FEATURE_URI_ACCELEROMETERINFO;
		case JIL_API_ACCOUNT:                        return JIL_FEATURE_URI_ACCOUNT;
		case JIL_API_ACCOUNTINFO:                    return JIL_FEATURE_URI_ACCOUNTINFO;
		case JIL_API_ADDRESSBOOKITEM:                return JIL_FEATURE_URI_ADDRESSBOOKITEM;
		case JIL_API_APPLICATIONTYPES:               return JIL_FEATURE_URI_APPLICATIONTYPES;
		case JIL_API_ATTACHMENT:                     return JIL_FEATURE_URI_ATTACHMENT;
		case JIL_API_AUDIOPLAYER:                    return JIL_FEATURE_URI_AUDIOPLAYER;
		case JIL_API_CALENDARITEM:                   return JIL_FEATURE_URI_CALENDARITEM;
		case JIL_API_CALLRECORD:                     return JIL_FEATURE_URI_CALLRECORD;
		case JIL_API_CAMERA:                         return JIL_FEATURE_URI_CAMERA;
		case JIL_API_CLIPBOARD:                      return JIL_FEATURE_URI_CLIPBOARD;
		case JIL_API_DATANETWORKCONNECTIONTYPES:     return JIL_FEATURE_URI_DATANETWORKCONNECTIONTYPES;
		case JIL_API_DATANETWORKINFO:                return JIL_FEATURE_URI_DATANETWORKINFO;
		case JIL_API_DEVICEINFO:                     return JIL_FEATURE_URI_DEVICEINFO;
		case JIL_API_DEVICESTATEINFO:                return JIL_FEATURE_URI_DEVICESTATEINFO;
		case JIL_API_EXCEPTION:                      return JIL_FEATURE_URI_EXCEPTION;
		case JIL_API_EXCEPTIONTYPES:                 return JIL_FEATURE_URI_EXCEPTIONTYPES;
		case JIL_API_FILE:                           return JIL_FEATURE_URI_FILE;
		case JIL_API_MULTIMEDIA:                     return JIL_FEATURE_URI_MULTIMEDIA;
		case JIL_API_PIM:                            return JIL_FEATURE_URI_PIM;
		case JIL_API_POSITIONINFO:                   return JIL_FEATURE_URI_POSITIONINFO;
		case JIL_API_CALLRECORDTYPES:                return JIL_FEATURE_URI_CALLRECORDTYPES;
		case JIL_API_CONFIG:                         return JIL_FEATURE_URI_CONFIG;
		case JIL_API_EVENTRECURRENCETYPES:           return JIL_FEATURE_URI_EVENTRECURRENCETYPES;
		case JIL_API_MESSAGE:                        return JIL_FEATURE_URI_MESSAGE;
		case JIL_API_MESSAGEFOLDERTYPES:             return JIL_FEATURE_URI_MESSAGEFOLDERTYPES;
		case JIL_API_MESSAGEQUANTITIES:              return JIL_FEATURE_URI_MESSAGEQUANTITIES;
		case JIL_API_MESSAGETYPES:                   return JIL_FEATURE_URI_MESSAGETYPES;
		case JIL_API_MESSAGING:                      return JIL_FEATURE_URI_MESSAGING;
		case JIL_API_POWERINFO:                      return JIL_FEATURE_URI_POWERINFO;
		case JIL_API_RADIOSIGNALSOURCETYPES:         return JIL_FEATURE_URI_RADIOSIGNALSOURCETYPES;
		case JIL_API_TELEPHONY:                      return JIL_FEATURE_URI_TELEPHONY;
		case JIL_API_VIDEOPLAYER:                    return JIL_FEATURE_URI_VIDEOPLAYER;
		case JIL_API_WIDGETMANAGER:                  return JIL_FEATURE_URI_WIDGETMANAGER;
		case JIL_API_RADIOINFO:                      return JIL_FEATURE_URI_RADIOINFO;
		case JIL_API_DEVICE:                         return JIL_FEATURE_URI_DEVICE_1_1;	// JIL_FEATURE_URI_DEVICE_1_2 is not in spec
		case JIL_API_WIDGET:                         return JIL_FEATURE_URI_WIDGET_1_1;	// JIL_FEATURE_URI_WIDGET_1_2 is not in spec

		case JIL_API_ACCOUNTINFO_1_1:                return JIL_FEATURE_URI_ACCOUNTINFO_1_1;
		case JIL_API_ADDRESSBOOK_1_1:                return JIL_FEATURE_URI_ADDRESSBOOK_1_1;
		case JIL_API_APPLICATION_1_1:                return JIL_FEATURE_URI_APPLICATION_1_1;
		case JIL_API_CALENDAR_1_1:                   return JIL_FEATURE_URI_CALENDAR_1_1;
		case JIL_API_CAMERA_1_1:                     return JIL_FEATURE_URI_CAMERA_1_1;
		case JIL_API_CLIPBOARD_1_1:                  return JIL_FEATURE_URI_CLIPBOARD_1_1;
		case JIL_API_CONFIG_1_1:                     return JIL_FEATURE_URI_CONFIG_1_1;
		case JIL_API_DEVICEINFO_1_1:                 return JIL_FEATURE_URI_DEVICEINFO_1_1;
		case JIL_API_FILESYSTEM_1_1:                 return JIL_FEATURE_URI_FILESYSTEM_1_1;
		case JIL_API_LOCATION_1_1:                   return JIL_FEATURE_URI_LOCATION_1_1;
		case JIL_API_MESSAGING_1_1:                  return JIL_FEATURE_URI_MESSAGING_1_1;
		case JIL_API_TELEPHONY_1_1:                  return JIL_FEATURE_URI_TELEPHONY_1_1;
		case JIL_API_URL_1_1:                        return JIL_FEATURE_URI_URL_1_1;
		case JIL_API_WIDGETMANAGER_1_1:              return JIL_FEATURE_URI_WIDGETMANAGER_1_1;
		case JIL_API_XMLHTTPREQUEST_1_1:             return JIL_FEATURE_URI_XMLHTTPREQUEST_1_1;
		default: return NULL;
	}
}


/* static */ BOOL
JILFeatures::IsJILFeatureSupported(const uni_char* feature_uri, const OpGadgetClass* gadget_class)
{
	if (gadget_class->SupportsNamespace(GADGETNS_JIL_1_0) &&
		    (uni_str_eq(feature_uri, JIL_FEATURE_URI_ACCOUNTINFO_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_ADDRESSBOOK_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CALENDAR_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_APPLICATION_1_1)
#ifdef JIL_CAMERA_SUPPORT
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CAMERA_1_1)
#endif // JIL_CAMERA_SUPPORT
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_FILESYSTEM_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_LOCATION_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_MESSAGING_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_TELEPHONY_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CONFIG_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_URL_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_XMLHTTPREQUEST_1_1)
		  ))
	{
		return TRUE;
	}
	else if (!gadget_class->SupportsNamespace(GADGETNS_JIL_1_0) &&
		    (uni_str_eq(feature_uri, JIL_FEATURE_URI_ACCELEROMETERINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_ACCOUNTINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_ACCOUNT)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_ADDRESSBOOKITEM)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_APPLICATIONTYPES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_ATTACHMENT)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CALLRECORD)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CALLRECORDTYPES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CALENDARITEM)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_EVENTRECURRENCETYPES)
#ifdef JIL_CAMERA_SUPPORT
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CAMERA)
#endif // JIL_CAMERA_SUPPORT
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_AUDIOPLAYER)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_DEVICE_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_DEVICE_1_2)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_DATANETWORKINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_DATANETWORKCONNECTIONTYPES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_DEVICEINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_DEVICESTATEINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_POWERINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_EXCEPTION)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_EXCEPTIONTYPES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_FILE)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_MESSAGE)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_MESSAGETYPES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_MESSAGEFOLDERTYPES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_MESSAGEQUANTITIES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_MESSAGING)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_MULTIMEDIA)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_PIM)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_POSITIONINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_RADIOINFO)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_RADIOSIGNALSOURCETYPES)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_TELEPHONY)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_CONFIG)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_WIDGET_1_1)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_WIDGET_1_2)
#ifdef MEDIA_JIL_PLAYER_SUPPORT
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_AUDIOPLAYER)
		  || uni_str_eq(feature_uri, JIL_FEATURE_URI_VIDEOPLAYER)
#endif // MEDIA_JIL_PLAYER_SUPPORT
		  ))
	{
	  return TRUE;
	}

	return FALSE;
}

#endif // DAPI_JIL_SUPPORT
