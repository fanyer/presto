/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_PI_PI_MODULE_H
#define MODULES_PI_PI_MODULE_H

#include "modules/hardcore/opera/module.h"

class PiModule : public OperaModule
{
public:
	PiModule();

	void InitL(const OperaInitInfo& info);
	void Destroy();
	class OpUiInfo* ui_info;
	class OpSystemInfo* system_info;
	class OpTimeInfo* time_info;
	class OpScreenInfo* screen_info;
#ifdef PI_POWER_STATUS
	class OpPowerStatusMonitor* power_status_monitor;
#endif // PI_POWER_STATUS
#ifdef USE_PLATFORM_VIEWERS
	class OpPlatformViewers* platform_viewers;
#endif
	class OpLocale* locale;
#ifdef PI_TELEPHONYNETWORKINFO
	class OpTelephonyNetworkInfo* telephony_network_info;
#endif // PI_TELEPHONYNETWORKINFO
#ifdef PI_SUBSCRIBERINFO
	class OpSubscriberInfo* subscriber_info;
#endif // PI_SUBSCRIBERINFO
#ifdef PI_ACCELEROMETER
	class OpAccelerometer* accelerometer;
#endif // PI_ACCELEROMETER
#ifdef PI_CAMERA
	class OpCameraManager* camera_manager;
#endif // PI_CAMERA
#ifdef PI_ADDRESSBOOK
	class OpAddressBook* address_book;
#endif // PI_ADDRESSBOOK
#ifdef PI_CALENDAR
	class OpCalendarService* calendar;
	class OpCalendarListenable* calendar_listeners_splitter;
#endif // PI_CALENDAR
#ifdef PI_DEVICESTATEINFO
	class OpDeviceStateInfo* device_state_info;
#endif // PI_DEVICESTATEINFO
};

#define g_op_ui_info g_opera->pi_module.ui_info
#define g_op_system_info g_opera->pi_module.system_info
#define g_op_time_info g_opera->pi_module.time_info
#define g_op_power_status_monitor g_opera->pi_module.power_status_monitor
#define g_op_screen_info g_opera->pi_module.screen_info
#ifdef USE_PLATFORM_VIEWERS
#define g_op_platform_viewers g_opera->pi_module.platform_viewers
#endif
#define g_oplocale g_opera->pi_module.locale
#ifdef PI_TELEPHONYNETWORKINFO
#define g_op_telephony_network_info g_opera->pi_module.telephony_network_info
#endif // PI_TELEPHONYNETWORKINFO
#ifdef PI_SUBSCRIBERINFO
#define g_op_subscriber_info g_opera->pi_module.subscriber_info
#endif // PI_SUBSCRIBERINFO
#ifdef PI_ACCELEROMETER
#define g_op_accelerometer g_opera->pi_module.accelerometer
#endif // PI_ACCELEROMETER
#ifdef PI_CAMERA
#define g_op_camera_manager g_opera->pi_module.camera_manager
#endif // PI_CAMERA
#ifdef PI_ADDRESSBOOK
#define g_op_address_book g_opera->pi_module.address_book
#endif // PI_ADDRESSBOOK
#ifdef PI_CALENDAR
#define g_op_calendar g_opera->pi_module.calendar
#define g_op_calendar_listeners g_opera->pi_module.calendar_listeners_splitter
#endif // PI_CALENDAR
#ifdef PI_DEVICESTATEINFO
#define g_op_device_state_info g_opera->pi_module.device_state_info
#endif // PI_DEVICESTATEINFO
#define PI_MODULE_REQUIRED

#endif // !MODULES_PI_PI_MODULE_H
