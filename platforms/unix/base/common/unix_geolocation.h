/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#ifndef UNIX_OPGEOLOCATION_H
#define UNIX_OPGEOLOCATION_H

#ifdef PI_GEOLOCATION

#include "modules/pi/OpGeolocation.h"

#if defined(__linux__)
# define UNIX_PROCFS_INTERFACES "/proc/net/dev"
# define PLATFORM_COLLECT_WIFI_DATA
#endif // __linux__

#ifdef PLATFORM_COLLECT_WIFI_DATA
# include "modules/hardcore/timer/optimer.h"
# include "modules/util/OpHashTable.h"
# define PLATFORM_WIFI_POLL_TIME 1000 // ms
# define PLATFORM_WIFI_FIRST_POLL_TIME 2000 // ms
# define UNIX_APINFO_TIMEOUT 240 // s
#endif


/** Timestamped AccessPointData */
struct UnixWifiAccessPointInfo
{
	time_t m_timestamp;

	OpAutoPtr< OpWifiData::AccessPointData > m_ap_data;
};

/**
 * Pi implementation for wifi data provider.
 *
 * In *nix environments, root privileges are required to trigger scan
 * (obviously). Userspace applications (like opera) are expected to ask drivers
 * for results of last periodical scan.
 *
 * Behaviour depends on following defines:
 *
 * PLATFORM_COLLECT_WIFI_DATA
 *
 * When NOT defined, provider will ask drivers for results of last scan each
 * time Poll() is called; underlying code is expected to return immedietaly
 * (it does on linux and *bsd).
 *
 * When defined, platform code aggregates scan results from few seconds back.
 * Aggregation is performed by polling drivers for results of last scan.
 * On every Poll(), list of aggregated access points information is copied
 * to OpWifiData, and passed to m_listener. 
 *
 * This tweak is intended to use by platforms, where wireless scan is not
 * cached by kernel - currently only linux is known to behave in such way
 * so it's important to know how linux drivers perform wireless scan:
 *
 * On linux, full scan can be triggered only by root - but once every ~100s
 * periodical scan is performed. Full scan takes few seconds, and ioctl
 * returns "Resource temporarily unavailable" when it's being performed.
 * During first few seconds after full scan, all networks around are
 * returned. On subsequent ioctl's, only few are returned (usually one,
 * with best signal, zero when using older kernels). Chance to get next
 * accurate list is after next periodic scan. This behaviour was reported in
 * all wireless drivers tested so far.
 *
 * Behaviour can be tweaked by additional defines:
 *
 * PLATFORM_WIFI_POLL_TIME (ms) - polling time
 *
 * PLATFORM_WIFI_FIRST_POLL_TIME (ms) - additional before first poll
 *
 * UNIX_APINFO_TIMEOUT (s) - scan results older than that will be discarded
 *
 */
class UnixWifiDataProvider
	: public OpGeolocationWifiDataProvider
#ifdef PLATFORM_COLLECT_WIFI_DATA
	, private OpTimerListener
#endif
{
public:
	/**
	 * Constructor.
	 *
	 * Data provider needs a list of network interfaces to perform
	 * queries on.
	 *
	 * @param listener
	 */
	UnixWifiDataProvider(OpGeolocationDataListener *listener);

	~UnixWifiDataProvider();

	/**
	 * Initialize data provider, if initialization fails, 
	 * object has to be destroyed to avoid memory leak.
	 */
	OP_STATUS Init();

	/**
	 * Part of OpGeolocationWifiDataProvider interface.
	 * Notifies m_listener about new wifi data around.
	 *
	 * When PLATFORM_COLLECT_WIFI_DATA is defined, returns data
	 * collected during last UNIX_APINFO_TIMEOUT seconds,
	 * otherwise it queries network interfaces for data on
	 * each call.
	 */
	OP_STATUS Poll();

private:
	/**
	 * Performs query about wifi access points around on all
	 * interfaces from m_iw_list.
	 *
	 * @param data Object to store results in.
	 */
	OP_STATUS GetScanResults(OpWifiData & data);

	/**
	 * Queries device for access point data.
	 *
	 * @param iw_name Name of interface to perform query on.
	 * @param data Object to store results in.
	 */
	OP_STATUS GetDeviceScanResults(const char* iw_name, OpWifiData & data);

	/* Makes m_buffer larger.
	 * @retval OK				ok
	 * @retval ERR				m_buffer has already maximum possible size
	 * @retval ERR_NO_MEMORY	no memory to initialize bigger buffer
	 */
	OP_STATUS EnlargeBuffer();

	/* Helper functions for logging. Prints message to stderr, only
	 * when logging is enabled through g_startup_settings.debug_iwscan
	 */
	void LogU(const char *format, uni_char *u_arg);
	void Log(const char *format, ...);

#if defined(__linux__)

	/**
	 * Linux-specific.
	 * Extracts one MAC address from events pointed by events_buffer.
	 */
	OP_STATUS GetMACFromEvent(const char* events_buffer,
							  int event_len,
							  OpString & mac);

	/**
	 * Linux-specific.
	 * Extracts one ESSID from events pointed by events_buffer.
	 */
	OP_STATUS GetSSIDFromEvent(const char* events_buffer,
							  int event_len,
							  OpString & ssid);

	/**
	 * Linux-specific.
	 * Extracts info about signal quality from events pointed by events_buffer.
	 */
	OP_STATUS GetQualityFromEvent(const char* events_buffer,
							  int event_len,
							  INT32 & signal_strength);

	/**
	 * Linux-specific.
	 * Extracts info about channel from events pointed by events_buffer.
	 */
	OP_STATUS GetChannelFromEvent(const char* events_buffer,
							  int event_len,
							  BYTE & channel);

#endif // __linux__

	/**
	 * Fills m_ifs_set with all available interfaces.
	 *
	 * Implementation note: Depending on host configuration, certain
	 * interfaces may appear many times (e.g. sometimes as AF_LINK).
	 * We are interested only in their unique names.
	 */
	void UpdateInterfacesSet();

	/* Listener to be notified about all new access point data.
	 */
	OpGeolocationDataListener * m_listener;

	/* Set of all wireless interfaces.
	 */
	OpAutoString8HashTable<OpString8> m_ifs_set;

	/* Socket for ioctl.
	 */
	int m_socket;

	/* Buffer for ioctl.
	 */
	char * m_buffer;

	/* Actual buffer size.
	 */
	int m_buffer_size;

#ifdef PLATFORM_COLLECT_WIFI_DATA

private:
	/**
	 * Part of OpTimerListener interface. On each timeout, data
	 * m_collected_data is updated. 
	 */
	void OnTimeOut(OpTimer *timer);

	/**
	 * Copies collected access point data to OpWifiData object.
	 *
	 * @param data
	 */
	OP_STATUS GetCollectedScanResults(OpWifiData & data);

	/**
	 * Filters m_collected data, removing access_point data, that is
	 * older than UNIX_APINFO_TIMEOUT.
	 */
	void PurgeCollectedData();

	/* Collected data about access points around.
	 */
	OpAutoStringHashTable< UnixWifiAccessPointInfo > m_collected_data;

	/**
	 * Helper function for removing key/data pairs from m_collected_data.
	 */
	void RemoveByKey(const uni_char *key);

	/* Internal timer.
	 */
	OpTimer m_timer;


#endif // PLATFORM_COLLECT_WIFI_DATA

};

#endif // PI_GEOLOCATION
#endif // UNIX_OPGEOLOCATION_H

