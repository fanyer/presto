/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DOM_GEOLOCATION_SUPPORT
#include "platforms/mac/pi/MacOpGeolocation.h"
#include "platforms/mac/util/macutils.h"
#include "modules/pi/OpThreadTools.h"

#define BOOL NSBOOL
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSTask.h>
#import <Foundation/NSFileHandle.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSAutoreleasePool.h>
#undef BOOL
#include <CoreFoundation/CFPropertyList.h>

#ifdef _DEBUG
//# define GEOLOC_DUMP
#endif
//# define GEOLOC_DUMP


OP_STATUS OpGeolocationWifiDataProvider::Create(OpGeolocationWifiDataProvider *& new_obj, class OpGeolocationDataListener *listener)
{
	new_obj = new MacOpGeolocationWifiDataProvider(listener);
	return new_obj ? OpStatus::OK : OpStatus::ERR_NO_MEMORY;
}


MacOpGeolocationWifiDataProvider::MacOpGeolocationWifiDataProvider(class OpGeolocationDataListener *listener)
	: m_listener(listener)
{
}

MacOpGeolocationWifiDataProvider::~MacOpGeolocationWifiDataProvider()
{
}

OP_STATUS MacOpGeolocationWifiDataProvider::Poll()
{
	g_main_message_handler->SetCallBack(this, MSG_MAC_WIFI_SCAN_DONE, 0);
	pthread_t thread = 0;
	pthread_create(&thread, NULL, worker, NULL);
	return OpStatus::OK;
}

OP_STATUS SetMACAddress(OpString& out_str, CFStringRef in_str)
{
	unsigned int p1,p2,p3,p4,p5,p6;
	OpString temp_str;
	SetOpString(temp_str, in_str);
	out_str.Set("");
	if (temp_str.CStr() && 6 == uni_sscanf(temp_str.CStr(), UNI_L("%x:%x:%x:%x:%x:%x"), &p1, &p2, &p3, &p4, &p5, &p6))
	{
		return out_str.AppendFormat(UNI_L("%02X-%02X-%02X-%02X-%02X-%02X"), p1, p2, p3, p4, p5, p6);
	}
	return OpStatus::ERR;
}

void* MacOpGeolocationWifiDataProvider::worker(void* arg)
{
	NSAutoreleasePool * pool = [[NSAutoreleasePool alloc] init];
	NSString* airport = @"/System/Library/PrivateFrameworks/Apple80211.framework/Resources/airport";
	NSArray* args = [NSArray arrayWithObjects:@"--scan", @"--xml", nil];
	NSTask *task = [[NSTask alloc] init];
	[task setLaunchPath:airport];
	[task setArguments:args];
	[task setStandardOutput:[NSPipe pipe]];
	[task setStandardError:[NSPipe pipe]];
#ifdef GEOLOC_DUMP
	printf("Scanning...\n");
#endif
	[task launch];
	NSData* result = [[[task standardOutput] fileHandleForReading] readDataToEndOfFile];
	[task waitUntilExit];
	[task release];
#ifdef GEOLOC_DUMP
	printf("Result: %p, len: %u\n", result, [result length]);
#endif
	CFArrayRef data = (CFArrayRef)[NSPropertyListSerialization propertyListFromData:result mutabilityOption:NSPropertyListImmutable format:nil errorDescription:nil];
	OpWifiData *wifi_data = OP_NEW(OpWifiData, ());
	if (data)
	{
#ifdef GEOLOC_DUMP
		printf("Got data\n");
#endif
		if (CFPropertyListIsValid((CFPropertyListRef)data, kCFPropertyListXMLFormat_v1_0)) {
#ifdef GEOLOC_DUMP
			printf("Valid data\n");
#endif
			if ((CFArrayGetTypeID() == CFGetTypeID(data))) {
#ifdef GEOLOC_DUMP
				printf("Data of type array\n");
#endif
				for (int i=0; i<CFArrayGetCount(data); i++) {
					CFDictionaryRef dict = (CFDictionaryRef)CFArrayGetValueAtIndex(data, i);
#ifdef GEOLOC_DUMP
					printf("Getting item %i: %p\n", i, dict);
#endif
					if (dict && (CFDictionaryGetTypeID() == CFGetTypeID(dict))) {
						OpWifiData::AccessPointData* ap_data = OP_NEW(OpWifiData::AccessPointData, ());
#ifdef GEOLOC_DUMP
						printf("Dict of type dictionary\n");
#endif
						CFStringRef ssid = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("SSID_STR"));
						if (ssid && (CFStringGetTypeID() == CFGetTypeID(ssid))) {
							SetOpString(ap_data->ssid, ssid);
							CFStringRef mac_address = (CFStringRef)CFDictionaryGetValue(dict, CFSTR("BSSID"));
							if (mac_address && (CFStringGetTypeID() == CFGetTypeID(mac_address))) {
								if (OpStatus::IsSuccess(SetMACAddress(ap_data->mac_address, mac_address)))
								{
									CFNumberRef signal = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("RSSI"));
									if (signal && CFNumberGetTypeID() == CFGetTypeID(signal)) {
										CFNumberGetValue(signal, kCFNumberIntType, &ap_data->signal_strength);
										int noise_val = 0;
										CFNumberRef noise = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("NOISE"));
										if (noise && CFNumberGetTypeID() == CFGetTypeID(noise)) {
											CFNumberGetValue(noise, kCFNumberIntType, &noise_val);
											ap_data->snr = ap_data->signal_strength - noise_val;
										} else
											ap_data->snr = 0;

										CFNumberRef channel = (CFNumberRef)CFDictionaryGetValue(dict, CFSTR("CHANNEL"));
										if (channel && CFNumberGetTypeID() == CFGetTypeID(channel))
											CFNumberGetValue(channel, kCFNumberCharType, &ap_data->channel);
										else
											ap_data->channel = 0;

										wifi_data->wifi_towers.Add(ap_data);
									}
								}
							}
						}
					}
				}
//				m_listener->OnNewDataAvailable(&wifi_data);
			}
		}
	}
	g_thread_tools->PostMessageToMainThread(MSG_MAC_WIFI_SCAN_DONE, (MH_PARAM_1)wifi_data, (MH_PARAM_2)NULL);
	[pool release];
	return 0;
}

void MacOpGeolocationWifiDataProvider::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_MAC_WIFI_SCAN_DONE)
	{
		OpWifiData *wifi_data = (OpWifiData *)par1;

#ifdef GEOLOC_DUMP
		if (wifi_data) {
			printf("OpWifiData {\n");
			for (unsigned int i=0; i<wifi_data->wifi_towers.GetCount(); i++) {
				OpWifiData::AccessPointData* ap_data = wifi_data->wifi_towers.Get(i);
				OpString8 name8, mac8;
				name8.SetUTF8FromUTF16(ap_data->ssid.CStr());
				mac8.SetUTF8FromUTF16(ap_data->mac_address.CStr());
				printf("{%s, %s, %d, %hhd, %d}\n", name8.CStr(), mac8.CStr(), ap_data->signal_strength, ap_data->channel, ap_data->snr);
			}
			printf("};\n");
		}
#endif

		m_listener->OnNewDataAvailable(wifi_data);
		OP_DELETE(wifi_data);
	}
}

OP_STATUS OpGeolocationRadioDataProvider::Create(OpGeolocationRadioDataProvider *& new_obj, class OpGeolocationDataListener *listener)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

OP_STATUS OpGeolocationGpsDataProvider::Create(OpGeolocationGpsDataProvider *& new_obj, class OpGeolocationDataListener *listener)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

#endif // DOM_GEOLOCATION_SUPPORT

