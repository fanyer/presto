/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/pi/pi_module.h"
#include "modules/pi/ui/OpUiInfo.h"
#include "modules/pi/device_api/OpPowerStatus.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/pi/OpTimeInfo.h"
#include "modules/pi/OpScreenInfo.h"
#include "modules/pi/OpLocale.h"
#include "modules/pi/system/OpPlatformViewers.h"
#include "modules/pi/device_api/OpTelephonyNetworkInfo.h"
#include "modules/pi/device_api/OpSubscriberInfo.h"
#include "modules/pi/device_api/OpAccelerometer.h"
#include "modules/pi/device_api/OpCamera.h"
#include "modules/pi/device_api/OpAddressBook.h"
#include "modules/pi/device_api/OpCalendarService.h"
#include "modules/pi/device_api/OpCalendarListenable.h"
#include "modules/pi/device_api/OpDeviceStateInfo.h"

PiModule::PiModule() :
	ui_info(NULL)
	,system_info(NULL)
	,time_info(NULL)
	,screen_info(NULL)
#ifdef PI_POWER_STATUS
	,power_status_monitor(NULL)
#endif
#ifdef USE_PLATFORM_VIEWERS
	,platform_viewers(NULL)
#endif
	,locale(NULL)
#ifdef PI_TELEPHONYNETWORKINFO
	,telephony_network_info(NULL)
#endif //PI_TELEPHONYNETWORKINFO
#ifdef PI_SUBSCRIBERINFO
	,subscriber_info(NULL)
#endif //PI_SUBSCRIBERINFO
#ifdef PI_ACCELEROMETER
	,accelerometer(NULL)
#endif // PI_ACCELEROMETER
#ifdef PI_CAMERA
	,camera_manager(NULL)
#endif // PI_CAMERA
#ifdef PI_ADDRESSBOOK
	,address_book(NULL)
#endif // PI_ADDRESSBOOK
#ifdef PI_CALENDAR
	,calendar(NULL)
	,calendar_listeners_splitter(NULL)
#endif // PI_CALENDAR
#ifdef PI_DEVICESTATEINFO
	,device_state_info(NULL)
#endif // PI_DEVICESTATEINFO
{
}

void
PiModule::InitL(const OperaInitInfo& info)
{
	LEAVE_IF_ERROR(OpSystemInfo::Create(&system_info));
	LEAVE_IF_ERROR(OpTimeInfo::Create(&time_info));
	LEAVE_IF_ERROR(OpScreenInfo::Create(&screen_info));
	LEAVE_IF_ERROR(OpUiInfo::Create(&ui_info));
	LEAVE_IF_ERROR(OpLocale::Create(&locale));

#ifdef PI_POWER_STATUS
	LEAVE_IF_ERROR(OpPowerStatusMonitor::Create(&power_status_monitor));
#endif // PI_POWER_STATUS

#ifdef USE_PLATFORM_VIEWERS
	LEAVE_IF_ERROR(OpPlatformViewers::Create(&platform_viewers));
#endif

#ifdef PI_TELEPHONYNETWORKINFO
	LEAVE_IF_ERROR(OpTelephonyNetworkInfo::Create(&telephony_network_info));
#endif // PI_TELEPHONYNETWORKINFO

#ifdef PI_SUBSCRIBERINFO
	LEAVE_IF_ERROR(OpSubscriberInfo::Create(&subscriber_info));
#endif // PI_SUBSCRIBERINFO

#ifdef PI_ACCELEROMETER
	LEAVE_IF_ERROR(OpAccelerometer::Create(&accelerometer));
#endif // PI_ACCELEROMETER

#ifdef PI_CAMERA
	LEAVE_IF_ERROR(OpCameraManager::Create(&camera_manager));
#endif // PI_CAMERA

#ifdef PI_ADDRESSBOOK
	LEAVE_IF_ERROR(OpAddressBook::Create(&address_book, OpAddressBook::ADDRESSBOOK_DEVICE));
#endif // PI_ADDRESSBOOK

#ifdef PI_CALENDAR
	calendar_listeners_splitter = OP_NEW_L(OpCalendarListenable, ());
	LEAVE_IF_ERROR(OpCalendarService::Create(&calendar, calendar_listeners_splitter));
#endif // PI_CALENDAR

#ifdef PI_DEVICESTATEINFO
	LEAVE_IF_ERROR(OpDeviceStateInfo::Create(&device_state_info));
#endif // PI_DEVICESTATEINFO
}

void
PiModule::Destroy()
{
#ifdef PI_DEVICESTATEINFO
	OP_DELETE(device_state_info);
	device_state_info = NULL;
#endif // PI_DEVICESTATEINFO

#ifdef PI_CALENDAR
	OP_DELETE(calendar);
	calendar = NULL;
	OP_DELETE(calendar_listeners_splitter);
	calendar_listeners_splitter = NULL;
#endif // PI_CALENDAR

#ifdef PI_ADDRESSBOOK
	OP_DELETE(address_book);
	address_book = NULL;
#endif // PI_ADDRESSBOOK

#ifdef PI_CAMERA
	OP_DELETE(camera_manager);
	camera_manager = NULL;
#endif // PI_CAMERA

#ifdef PI_ACCELEROMETER
	OP_DELETE(accelerometer);
	accelerometer = NULL;
#endif // PI_ACCELEROMETER

#ifdef PI_SUBSCRIBERINFO
	OP_DELETE(subscriber_info);
	subscriber_info = NULL;
#endif // PI_SUBSCRIBERINFO

#ifdef PI_TELEPHONYNETWORKINFO
	OP_DELETE(telephony_network_info);
	telephony_network_info = NULL;
#endif // PI_TELEPHONYNETWORKINFO

#ifdef USE_PLATFORM_VIEWERS
	OP_DELETE(platform_viewers);
	platform_viewers = NULL;
#endif

	OP_DELETE(locale);
	locale = NULL;

#ifdef PI_POWER_STATUS
	OP_DELETE(power_status_monitor);
	power_status_monitor = NULL;
#endif // PI_POWER_STATUS

	OP_DELETE(ui_info);
	ui_info = NULL;

	OP_DELETE(screen_info);
	screen_info = NULL;

	OP_DELETE(system_info);
	system_info = NULL;

	OP_DELETE(time_info);
	time_info = NULL;
}
