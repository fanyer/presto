/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICEAPI_JILFEATURES_H
#define DEVICEAPI_JILFEATURES_H

class OpGadget;
class OpGadgetClass;

class JILFeatures
{
public:

	enum JIL_Api
	{
		JIL_API_FIRST = 0,
		/* 'features' related to JIL Objects */
		JIL_API_ACCELEROMETERINFO = JIL_API_FIRST,
		JIL_API_ACCOUNT,
		JIL_API_ACCOUNTINFO,
		JIL_API_ADDRESSBOOKITEM,
		JIL_API_APPLICATIONTYPES,
		JIL_API_ATTACHMENT,
		JIL_API_AUDIOPLAYER,
		JIL_API_CALENDARITEM,
		JIL_API_CALLRECORD,
		JIL_API_CALLRECORDTYPES,
		JIL_API_CAMERA,
		JIL_API_CLIPBOARD,
		JIL_API_CONFIG,
		JIL_API_DATANETWORKCONNECTIONTYPES,
		JIL_API_DATANETWORKINFO,
		JIL_API_DEVICE,
		JIL_API_DEVICEINFO,
		JIL_API_DEVICESTATEINFO,
		JIL_API_EVENTRECURRENCETYPES,
		JIL_API_EXCEPTION,
		JIL_API_EXCEPTIONTYPES,
		JIL_API_FILE,
		JIL_API_MESSAGE,
		JIL_API_MESSAGEFOLDERTYPES,
		JIL_API_MESSAGEQUANTITIES,
		JIL_API_MESSAGETYPES,
		JIL_API_MESSAGING,
		JIL_API_MULTIMEDIA,
		JIL_API_PIM,
		JIL_API_POSITIONINFO,
		JIL_API_POWERINFO,
		JIL_API_RADIOINFO,
		JIL_API_RADIOSIGNALSOURCETYPES,
		JIL_API_TELEPHONY,
		JIL_API_VIDEOPLAYER,
		JIL_API_WIDGET,
		JIL_API_WIDGETMANAGER,

		// Old JIL features:
		JIL_API_ACCOUNTINFO_1_1,
		JIL_API_ADDRESSBOOK_1_1,
		JIL_API_APPLICATION_1_1,
		JIL_API_CALENDAR_1_1,
		JIL_API_CAMERA_1_1,
		JIL_API_CLIPBOARD_1_1,
		JIL_API_CONFIG_1_1,
		JIL_API_DEVICEINFO_1_1,
		JIL_API_FILESYSTEM_1_1,
		JIL_API_LOCATION_1_1,
		JIL_API_MESSAGING_1_1,
		JIL_API_TELEPHONY_1_1,
		JIL_API_URL_1_1,
		JIL_API_WIDGETMANAGER_1_1,
		JIL_API_XMLHTTPREQUEST_1_1,

		/* 'features' related to JIL functionalities */
		JIL_API_REVERSE_SET_PREFERENCE_FOR_KEY_ARGS, ///< in JIL specs prior to 1.2 Widget.setPreferenceForKey has reverse args order to w3c widget.

		JIL_API_LAST // ...
	};

	/**
	 * Check whether given api_name is present in configuration file of the gadget.
	 *
	 * @param api_name Identifier of the API to check.
	 * @param gadget Gadget for which API declaration should be checked.
	 *
	 * @return TRUE if the given API/feature is enabled.
	 */
	static BOOL IsJILApiRequested(JIL_Api api_name, OpGadget* gadget);

	static const char* GetFeatureURI(JIL_Api api_name);

	static BOOL IsJILFeatureSupported(const uni_char* feature_uri, const OpGadgetClass* gadget_class);
};

#endif // DEVICEAPI_JILFEATURES_H
