/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Patryk Obara (pobara)
 */

#include "core/pch.h"

#ifdef PI_GEOLOCATION
#include "platforms/unix/base/common/unix_geolocation.h"
#include "platforms/quix/commandline/StartupSettings.h"

#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <ifaddrs.h>
#include <string.h>

#if defined(__linux__)
#include <linux/wireless.h>
#endif // __linux__

#if defined(__FreeBSD__) || defined(__NetBSD__)
#include <net/if.h>
#include <net80211/ieee80211_ioctl.h>
#endif // __FreeBSD__ or __NetBSD__


namespace
{
	// linux: IW_SCAN_MAX_DATA (defined as 4096) should be enough for everyone
	// but drivers seem to have problem with this (when there are many networks
	// around).
	// freebsd: ifconfig uses buffer of size 24*1024
	// see ifieee80211.c : list_scan(int)
	// If you want to make this size smaller, you need to test if it won't
	// affect geolocation accuracy.
	// others: we use freebsd limit, just in case
#if defined(__linux__)
	static const int MAX_IOCTL_SCAN_BUFFER_SIZE = 2 * IW_SCAN_MAX_DATA;
#else
	static const int MAX_IOCTL_SCAN_BUFFER_SIZE = 24 * 1024;
#endif
	static const int IOCTL_SCAN_BUFFER_STEP = 1024;

} // namespace


// Currently only wifi-scraping using ioctl on linux and freebsd is supported,
// all other implementations fall-back to ip-based location

/* static */ OP_STATUS
OpGeolocationWifiDataProvider::Create(
		OpGeolocationWifiDataProvider *& new_obj,
		OpGeolocationDataListener *listener)
{
#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__)
	UnixWifiDataProvider * new_provider =
		OP_NEW(UnixWifiDataProvider, (listener));
	RETURN_OOM_IF_NULL(new_provider);
	if (OpStatus::IsError(new_provider->Init()))
	{
		OP_DELETE(new_provider);
		return OpStatus::ERR_NOT_SUPPORTED;
	}
	new_obj = new_provider;
	return OpStatus::OK;
#else
	#error "not implemented, comment this line if you want to compile it anyway"
	return OpStatus::ERR_NOT_SUPPORTED;
#endif
}


// Empty pi implementations for unsupported stuff

/* static */ OP_STATUS
OpGeolocationRadioDataProvider::Create(
		OpGeolocationRadioDataProvider *& new_obj,
		OpGeolocationDataListener *)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}


/* static */ OP_STATUS
OpGeolocationGpsDataProvider::Create(
		OpGeolocationGpsDataProvider *& new_obj,
		OpGeolocationDataListener *)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}


UnixWifiDataProvider::UnixWifiDataProvider(OpGeolocationDataListener *listener)
	: m_listener(listener)
	, m_socket(-1)
	, m_buffer(NULL)
	, m_buffer_size(0)
{
	OP_ASSERT(m_listener);
#ifdef PLATFORM_COLLECT_WIFI_DATA
	m_timer.SetTimerListener(this);
#endif
}


UnixWifiDataProvider::~UnixWifiDataProvider()
{
#ifdef PLATFORM_COLLECT_WIFI_DATA
	m_timer.Stop();
#endif
	OP_DELETEA(m_buffer);
	if(-1 != m_socket)
		close(m_socket);
}


OP_STATUS UnixWifiDataProvider::Init()
{
	m_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (-1 == m_socket)
		return OpStatus::ERR;
#ifdef PLATFORM_COLLECT_WIFI_DATA
	m_buffer_size = IOCTL_SCAN_BUFFER_STEP;
	m_buffer = OP_NEWA(char, m_buffer_size);
	if (!m_buffer)
	{
		close(m_socket);
		m_socket = -1;
		return OpStatus::ERR_NO_MEMORY;
	}
	m_timer.Start(PLATFORM_WIFI_FIRST_POLL_TIME);
#endif
	return OpStatus::OK;
}


OP_STATUS UnixWifiDataProvider::Poll()
{
	OpWifiData wifi_data;
	OP_STATUS status;
#ifdef PLATFORM_COLLECT_WIFI_DATA
	status = GetCollectedScanResults(wifi_data);
#else
	status = GetScanResults(wifi_data);
#endif // !PLATFORM_COLLECT_WIFI_DATA
	if (OpStatus::IsMemoryError(status))
		return status;
	// Returning NULL here is ok as long as you are planning to send some real
	// data in very near future. 
	// Sending wifi_data with no results will cause fallback to ip.
	m_listener->OnNewDataAvailable(&wifi_data);
	return OpStatus::OK;
}


OP_STATUS UnixWifiDataProvider::GetScanResults(OpWifiData & data)
{
   	OP_STATUS status = OpStatus::ERR;

#ifndef PLATFORM_COLLECT_WIFI_DATA
	// When data collection is disabled, there is no need
	// to keep this buffer initialized all the time, it's
	// better to use it only for actual scan.
	m_buffer_size = MAX_IOCTL_SCAN_BUFFER_SIZE;
	m_buffer = OP_NEWA(char, m_buffer_size);
	RETURN_OOM_IF_NULL(m_buffer);
#endif

	UpdateInterfacesSet();
	OpHashIterator *it = m_ifs_set.GetIterator();
	if (!it)
		status = OpStatus::ERR_NO_MEMORY;
	else if (OpStatus::IsSuccess(it->First()))
		do
		{
			const char *dev = static_cast< OpString8 * >(it->GetData())->CStr();
			Log("Scanning %s\n", dev);
			OP_STATUS single_status = GetDeviceScanResults(dev, data);
			if (OpStatus::IsSuccess(single_status))
				status = OpStatus::OK;
		} while (OpStatus::IsSuccess(it->Next()));
	OP_DELETE(it);

#ifndef PLATFORM_COLLECT_WIFI_DATA
	OP_DELETEA(m_buffer);
	m_buffer = NULL;
	m_buffer_size = 0;
#endif

	return status;
}


#if defined(__linux__)
OP_STATUS UnixWifiDataProvider::GetDeviceScanResults(const char* iw_name,
											  OpWifiData & data)
{
	iwreq iw_request;

	char *buffer = m_buffer;
	iw_request.u.data.pointer = buffer;
	iw_request.u.data.length = m_buffer_size; // sizeof (buffer);

	strncpy(iw_request.ifr_name, iw_name, IFNAMSIZ);
	iw_request.u.data.flags = IW_SCAN_ALL_ESSID;
	if (ioctl(m_socket, SIOCGIWSCAN, &iw_request) != 0)
	{
		OP_STATUS st = OpStatus::ERR;
		// "Operation not supported" - this interface doesn't support
		// wireless extensions
		Log("SIOCGIWSCAN ioctl: %s\n", strerror(errno));
		if (E2BIG == errno) 
		{
			// "Argument list too long" error
			// results data is too big to fit into buffer
			st = EnlargeBuffer();
			if (OpStatus::IsSuccess(st))
				return GetDeviceScanResults(iw_name, data);
		}
		return st;
	}
	int length = iw_request.u.data.length;
	if (!length)
	{
		Log("[%s] No wireless networks detected.\n", iw_name);
		return OpStatus::ERR;
	}
	char *current_event_ptr = buffer;
	char *events_end = buffer + iw_request.u.data.length;
	iw_event event;
	bool found_mac_address, found_ssid, found_signal_strength, found_channel;
	bool skip_this_result;
	found_mac_address = found_ssid = found_signal_strength =
		found_channel = skip_this_result = FALSE;
	OP_STATUS st, st1, st2;
	OpString mac_address;
	OpString ssid;
	INT32 signal_strength;
	BYTE channel;

	// We're doing a lot of memcpy'ing in this function instead of casting.
	// There is reason for this: some of iw_* structures contain pointers,
	// therefore scanning may fail when ioctl'ing 64bit kernel from 32bit
	// browser.
	// We don't support such case yet, but using memcpy enables us to fix it
	// when required.
	for(; (current_event_ptr + IW_EV_LCP_PK_LEN) <= events_end ;
		  current_event_ptr += event.len )
	{
		memcpy(&event, current_event_ptr, IW_EV_LCP_PK_LEN); // get event header
		OP_ASSERT(event.len > 0);
		if (event.len <= 0)
			return OpStatus::ERR_NOT_SUPPORTED;

		switch (event.cmd)
		{
			case SIOCGIWAP:		// get access point MAC addresses
			{
				st = GetMACFromEvent(current_event_ptr, event.len, mac_address);
				if (OpStatus::IsError(st))
					skip_this_result = TRUE;
				found_mac_address = TRUE;
				LogU("%s\t", mac_address.CStr());
			}
			break;
			case IWEVQUAL:		// Quality part of statistics (scan)
			{
				st = GetQualityFromEvent(current_event_ptr, event.len,
						signal_strength);
				if (OpStatus::IsError(st))
					skip_this_result = TRUE;
				found_signal_strength = TRUE;
				Log("signal: %d dBm\t", signal_strength);
			}
			break;
			case SIOCGIWESSID: 	// get ESSID
			{
				st = GetSSIDFromEvent(current_event_ptr, event.len, ssid);
				if (OpStatus::IsError(st))
					skip_this_result = TRUE;
				found_ssid = TRUE;
				LogU("\"%s\"\t", ssid.CStr());
			}
			break;
			case SIOCGIWFREQ:	// get channel/frequency (Hz)
			{
				st = GetChannelFromEvent(current_event_ptr, event.len, channel);
				if (OpStatus::IsError(st))
					break;
				found_channel = TRUE;
				Log("channel: %d\t", channel);
			}
			break;
			case IWEVCUSTOM:	// driver specific ascii string
			case IWEVGENIE:		// generic IE (WPA, RSN, WMM, ..)
			case SIOCGIWRATE:	// get default bit rate (bps)
			case SIOCGIWMODE:	// get operation mode
			case SIOCGIWENCODE: // get encoding token & mode
				break;
			default:
				OP_ASSERT(!"unknown wireless event");
				Log("\nwarning: unknown wireless event\n");
				break;
		}

		if (found_mac_address && found_ssid && found_signal_strength)
		{
			// all info about single accespoint found
			if (!skip_this_result)
			{
				OpWifiData::AccessPointData *op_ap_data = OP_NEW(
						OpWifiData::AccessPointData, ());
				RETURN_OOM_IF_NULL(op_ap_data);
				st1 = op_ap_data->mac_address.Set(mac_address);
				st2 = op_ap_data->ssid.Set(ssid);
				op_ap_data->signal_strength = signal_strength;
				if (found_channel)
					op_ap_data->channel = channel;
				if (OpStatus::IsSuccess(st1) && OpStatus::IsSuccess(st2))
					if (OpStatus::IsError( data.wifi_towers.Add(op_ap_data) ))
						OP_DELETE(op_ap_data);
			}
			else Log(" (skipped)\n");
			Log("\n");

			mac_address.Empty();
			ssid.Empty();
			found_mac_address = found_ssid = found_signal_strength = 
				found_channel = skip_this_result = FALSE;

			Log("\n");
		}
	}
	return OpStatus::OK;
}


OP_STATUS UnixWifiDataProvider::GetMACFromEvent(const char* events_buffer,
		int event_len, OpString & mac)
{
	iw_event full_event;
	memcpy( &full_event, events_buffer, event_len );
	sockaddr ap_addr = full_event.u.ap_addr;
	// before trimming to 0xff values were waaay too big
	mac.Empty();
	return mac.AppendFormat(
				UNI_L("%02X-%02X-%02X-%02X-%02X-%02X"),
				ap_addr.sa_data[0] & 0xff,
				ap_addr.sa_data[1] & 0xff,
				ap_addr.sa_data[2] & 0xff,
				ap_addr.sa_data[3] & 0xff,
				ap_addr.sa_data[4] & 0xff,
				ap_addr.sa_data[5] & 0xff);
}


OP_STATUS UnixWifiDataProvider::GetSSIDFromEvent(const char* events_buffer,
		int event_len, OpString & ssid)
{
	// this is pointer event
	// see comment in wireless.h around IW_EV_POINT_OFF macro
	// otherwise following code won't make any sense ;)
	const char *essid_ptr = events_buffer + IW_EV_LCP_LEN + IW_EV_POINT_OFF;
	int essid_len = event_len - IW_EV_LCP_LEN - IW_EV_POINT_OFF;
	if (essid_len < 0 || essid_len > IW_ESSID_MAX_SIZE)
		return OpStatus::ERR;
	return ssid.Set(essid_ptr, essid_len);
}


OP_STATUS UnixWifiDataProvider::GetQualityFromEvent(const char* events_buffer,
		int event_len, INT32 & signal_strength)
{
	iw_event full_event;
	memcpy( &full_event, events_buffer, event_len );
	int updated = full_event.u.qual.updated;

	// we set fallback signal strength to very weak
	// but not extremely weak ;)
	int fallback_db_level = -32;

	if (updated & IW_QUAL_RCPI)
	{
		// Received Channel Power Indicator
		int rcpi_level = full_event.u.qual.level;
		int db_level = (rcpi_level/2) - 110;
		if ( -192 <= db_level && db_level < 64 )
		{
			signal_strength = db_level;
			return OpStatus::OK;
		}
		else
		{
			Log("warning: rcpi signal data detected.\n");
			signal_strength = fallback_db_level;
		}
		return OpStatus::OK;
	}
	else if(updated & IW_QUAL_DBM)
	{
		int db_level = full_event.u.qual.level;
		if (64 <= db_level)
		{
			db_level -= 256;
		}
		signal_strength = db_level;
		return OpStatus::OK;
	}
	else
	{
		Log("warning: no DBM flag, signal strength may be inaccurate\n");
		// drivers didn't report DBM data, I am not sure how to
		// interpret signal level
		//
		// Newer drivers should indicate through setting a flag, that reported
		// value is in dBm, which means we have to convert value to
		// range [-192,63]. However, all drivers in old kernels AND older
		// drivers (sometimes distributed in new kernels) behave as if they've
		// set DBM flag, (but without setting it) or sometimes, they report
		// quality relative to "max value" supported by network interface
		// (another ioctl call is required to obtain it).
		//
		// As test hack: we try to report signal strength anyway.
		// WT usually tries just this:
		int db_level = full_event.u.qual.level;
		if (64 <= db_level)
		{
			db_level -= 256;
		}
		if (db_level > 63 || db_level < -192)
		{
			db_level = fallback_db_level;
		}
		signal_strength = db_level;
		return OpStatus::OK;
	}
}


OP_STATUS UnixWifiDataProvider::GetChannelFromEvent(const char* events_buffer,
		int event_len, BYTE & channel)
{
	// driver sends two such events per access point
	// first one contains value scaled to 0-1000 (representing channel)
	// second has value > 1000, which is frequency in Hz
	// we are interested only in channel
	iw_event full_event;
	memcpy( &full_event, events_buffer, event_len );
	iw_freq f = full_event.u.freq;
	double freq = double(f.m)*op_pow(10,f.e);
	if (freq >= 0 && freq <= 1000)
	{
		int chan = int(op_floor(freq));
		OP_ASSERT( chan == freq );
		// as of 802.11a/h/j/n valid channels are: 1-192
		if(chan < 1 || 192 < chan)
			  Log("\nchannel is out of 802.11a/h/j/n spec\n");
		if (1 <= chan && chan <= 192)
		{
			channel = static_cast<BYTE>(chan);
			return OpStatus::OK;
		}
	}
	return OpStatus::ERR;
}

#elif defined(__FreeBSD__) || defined(__NetBSD__)

OP_STATUS UnixWifiDataProvider::GetDeviceScanResults(const char* iw_name,
											  OpWifiData & data)
{
	ieee80211req iw_request;

	char *buffer = m_buffer;

	iw_request.i_data = buffer;
	iw_request.i_len = m_buffer_size;
	strncpy(iw_request.i_name, iw_name, sizeof(iw_request.i_name));
	iw_request.i_type = IEEE80211_IOC_SCAN_RESULTS;

	if (ioctl(m_socket, SIOCG80211, &iw_request) != 0)
	{
		Log("SIOCG80211 ioctl: %s\n", strerror(errno));
		// "Invalid argument" - no read privileges
		// "Device not configured" - interface is not wireless or doesn't exist
		return OpStatus::ERR;
	}
	size_t length = iw_request.i_len;
	if (!length)
	{
		// Buffer is too small to report _any_ results OR
		// wireless is off / there are no networks around.
		// Freebsd will try to cram as much results as
		// possible into buffer of given size, so we have to
		// make it as big as reasonably possible in the first place
		// to get better accuracy.
		Log("[%s] No wireless networks detected.\n", iw_name);
		return OpStatus::ERR;
	}
	char *current_result_ptr = buffer;
	do
	{
		const ieee80211req_scan_result *scan_result = reinterpret_cast
			< const ieee80211req_scan_result * >(current_result_ptr);

		OpWifiData::AccessPointData *op_ap_data = OP_NEW(
				OpWifiData::AccessPointData, ());
		RETURN_OOM_IF_NULL(op_ap_data);

		OP_STATUS st1 = OpStatus::ERR;
		OP_STATUS st2 = OpStatus::ERR;
		if (scan_result->isr_ssid_len <= IEEE80211_NWID_LEN)
			st1 = op_ap_data->ssid.Set(
				(current_result_ptr+scan_result->isr_ie_off),
				scan_result->isr_ssid_len);

		OP_ASSERT(IEEE80211_ADDR_LEN == 6);
		op_ap_data->mac_address.Empty();
		st2 = op_ap_data->mac_address.AppendFormat(
				UNI_L("%02X-%02X-%02X-%02X-%02X-%02X"),
				scan_result->isr_bssid[0],
				scan_result->isr_bssid[1],
				scan_result->isr_bssid[2],
				scan_result->isr_bssid[3],
				scan_result->isr_bssid[4],
				scan_result->isr_bssid[5]);

		int noise = scan_result->isr_noise;
		int signal = scan_result->isr_rssi/2 + noise;
		op_ap_data->signal_strength = signal;
		op_ap_data->snr = signal-noise;
		if (OpStatus::IsSuccess(st1) && OpStatus::IsSuccess(st2))
			if (OpStatus::IsError( data.wifi_towers.Add(op_ap_data) ))
				OP_DELETE(op_ap_data);
		LogU("%s  ", op_ap_data->mac_address.CStr());
		Log( "signal: %d dBm  noise: %d dBm ", signal, noise);
		LogU("essid: \"%s\"\n", op_ap_data->ssid.CStr());
		current_result_ptr += scan_result->isr_len;
		length -= scan_result->isr_len;
	} while (length >= sizeof(ieee80211req_scan_result));

	Log("\n");
	return OpStatus::OK;
}

#else


OP_STATUS UnixWifiDataProvider::GetDeviceScanResults(const char* iw_name,
											  OpWifiData & data)
{
	return OpStatus::ERR_NOT_SUPPORTED;
}

#endif // __linux__ or __FreeBSD__ or __NetBSD__

#ifdef PLATFORM_COLLECT_WIFI_DATA

OP_STATUS UnixWifiDataProvider::GetCollectedScanResults(OpWifiData & data)
{
	int n = m_collected_data.GetCount();
	OpHashIterator *it = m_collected_data.GetIterator();
	if (!it)
		return OpStatus::ERR_NO_MEMORY;
	OpStatus::Ignore(it->First());
	for(int i=0; i<n; i++, it->Next())
	{
		UnixWifiAccessPointInfo *ap_info =
			static_cast< UnixWifiAccessPointInfo * >(it->GetData());

		char timebuf[10];
		tm *timestamp;
		timestamp = localtime(&(ap_info->m_timestamp));
		strftime(timebuf, sizeof(timebuf), "%H:%M:%S", timestamp);
		char *mac = uni_down_strdup(ap_info->m_ap_data->mac_address.CStr());
		if (mac)
		{
			Log("[%s] %s\n", timebuf, mac);
			op_free(mac);
		}

		OpWifiData::AccessPointData *op_ap_data = OP_NEW(
				OpWifiData::AccessPointData, ());
		if (op_ap_data)
		{
			OP_STATUS st1, st2;
			st1 = op_ap_data->mac_address.Set(ap_info->m_ap_data->mac_address);
			st2 = op_ap_data->ssid.Set(ap_info->m_ap_data->ssid);
			op_ap_data->signal_strength = ap_info->m_ap_data->signal_strength;
			op_ap_data->channel = ap_info->m_ap_data->channel;

			if (OpStatus::IsSuccess(st1) && OpStatus::IsSuccess(st2))
				if (OpStatus::IsError( data.wifi_towers.Add(op_ap_data) ))
					OP_DELETE(op_ap_data);
		}
	}
	OP_DELETE(it);
	return OpStatus::OK;
}


void UnixWifiDataProvider::RemoveByKey(const uni_char *key)
{
	UnixWifiAccessPointInfo * ap_info = NULL;
	OP_STATUS removed = m_collected_data.Remove(key, &ap_info);
	if (OpStatus::IsSuccess(removed))
	{
		OP_DELETE(ap_info);
	}
}


void UnixWifiDataProvider::PurgeCollectedData()
{
	int n = m_collected_data.GetCount();
	OpHashIterator *it = m_collected_data.GetIterator();
	if (!it)
		return;
	OpStatus::Ignore(it->First());
	for(int i=0; i<n; i++, it->Next())
	{
		const uni_char *key =
			static_cast< const uni_char * >(it->GetKey());
		UnixWifiAccessPointInfo *ap_info =
			static_cast< UnixWifiAccessPointInfo * >(it->GetData());

		// if too old
		if ((time(NULL) - ap_info->m_timestamp) > UNIX_APINFO_TIMEOUT)
		{
			RemoveByKey(key);
		}
	}
	OP_DELETE(it);
}


void UnixWifiDataProvider::OnTimeOut(OpTimer *timer)
{
	OP_ASSERT(&m_timer == timer); // sabotage! :)
	if(&m_timer != timer)
		return;

	OpWifiData wifi_data;
   	OpStatus::Ignore( GetScanResults(wifi_data) );
	int oom_on_index = -1;
	int size = wifi_data.wifi_towers.GetCount();
	for(int i=0; i<size; i++)
	{
		UnixWifiAccessPointInfo * unix_ap_info =
				OP_NEW(UnixWifiAccessPointInfo, ());
		if (!unix_ap_info)
		{
			oom_on_index = i;
			break;
		}
		unix_ap_info->m_timestamp = time(NULL);
		unix_ap_info->m_ap_data = wifi_data.wifi_towers.Get(i);
		const uni_char *mac = unix_ap_info->m_ap_data->mac_address.CStr();

		// now update key/data pair
		RemoveByKey(mac);
		m_collected_data.Add(mac, unix_ap_info);
	}
	if (oom_on_index > -1)
		wifi_data.wifi_towers.Delete(oom_on_index, size-oom_on_index);
	wifi_data.wifi_towers.Clear();

	PurgeCollectedData();
	Log("collected %d results\n", m_collected_data.GetCount());
	m_timer.Start(PLATFORM_WIFI_POLL_TIME);
}
#endif // PLATFORM_COLLECT_WIFI_DATA


OP_STATUS UnixWifiDataProvider::EnlargeBuffer()
{
	int new_size = m_buffer_size + IOCTL_SCAN_BUFFER_STEP;
	if (new_size > MAX_IOCTL_SCAN_BUFFER_SIZE)
		return OpStatus::ERR;
	char * new_buffer = OP_NEWA(char, new_size);
	if (!new_buffer)
		return OpStatus::ERR_NO_MEMORY;
	OP_DELETEA(m_buffer);
	m_buffer = new_buffer;
	m_buffer_size = new_size;
	return OpStatus::OK;
}


void UnixWifiDataProvider::UpdateInterfacesSet()
{
	m_ifs_set.DeleteAll();
	ifaddrs *ifaddrs_head = NULL; // head of linked list
	if (-1 != getifaddrs(&ifaddrs_head))
	{
		ifaddrs *ifptr = ifaddrs_head;
		for (; ifptr != NULL ; ifptr = ifptr->ifa_next)
		{
			OpString8 *device = OP_NEW(OpString8, ());
			OP_STATUS st;
			if (device)
				st = device->Set(ifptr->ifa_name);
			if (OpStatus::IsSuccess(st))
				if (OpStatus::IsError(m_ifs_set.Add(ifptr->ifa_name, device)))
					OP_DELETE(device);
		}
		freeifaddrs(ifaddrs_head);
	}
}


inline void UnixWifiDataProvider::LogU(const char *format, uni_char *u_arg)
{
	if (!g_startup_settings.debug_iwscan)
		return;
	char *arg = uni_down_strdup(u_arg);
	if (arg)
	{
		fprintf(stderr, format, arg);
		op_free(arg);
	}
}


inline void UnixWifiDataProvider::Log(const char *format, ...)
{
	if (!g_startup_settings.debug_iwscan)
		return;
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}


#endif // PI_GEOLOCATION
