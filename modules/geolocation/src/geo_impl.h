/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GEOLOCATION_SRC_GEO_IMPL_H
#define GEOLOCATION_SRC_GEO_IMPL_H

#ifdef GEOLOCATION_SUPPORT

#include "modules/pi/OpGeolocation.h"
#include "modules/geolocation/geolocation.h"
#include "modules/geolocation/src/geo_network_request.h"
#include "modules/geolocation/src/geo_tools.h"

#include "modules/geolocation/src/geo_google2011_network_api.h"

class GeolocationlistenerElm
	: public Link
	, public MessageObject
{
public:
	static OP_STATUS Make(GeolocationlistenerElm *& new_obj, const OpGeolocation::Options options, OpGeolocationListener *listener);
	virtual ~GeolocationlistenerElm();

	void AcqusitionStarted(int time_to_start) { StartTimeout(time_to_start); }
	void OnPositionAvailable(const OpGeolocation::Position &position, double now_utc);
	void OnPositionError(const OpGeolocation::Error &error, double now_utc);

	BOOL RequestsHighAccuracy() { return m_options.high_accuracy; }
	long MaximumAge() { return m_fired != 0.0 ? MAX(m_options.maximum_age, GEOLOCATION_DEFAULT_POLL_INTERVAL) : m_options.maximum_age ; }
	BOOL IsOneShot() { return m_options.one_shot; }						// Only applicable for one_shot requests. Else FALSE.
	BOOL HasFired(double now_utc);

	OpGeolocationListener *GetListener() { return m_listener; }

protected:
	// From MessageObject
	/* virtual */ void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	GeolocationlistenerElm(const OpGeolocation::Options options, OpGeolocationListener *listener);
	OP_STATUS Construct();

	void StartTimeout(int delay);
	void CancelTimeout();
	void ReleaseIfNeeded();

	MH_PARAM_1 Id() { return reinterpret_cast<MH_PARAM_1>(this); }

	OpGeolocation::Options m_options;
	OpGeolocationListener *m_listener;
	double m_fired;
	struct OpGeolocation::Position m_position;
	struct OpGeolocation::Error m_error;
	BOOL m_timeout_msg_posted;
};

class GeolocationImplementation
	: public OpGeolocationDataListener
	, public OpGeolocationNetworkListener
	, public OpGeolocation
	, public MessageObject
	, public OpPrefsListener
{
	OpGeolocationWifiDataProvider *m_wifi_data_provider;
	OpGeolocationRadioDataProvider *m_radio_data_provider;
	OpGeolocationGpsDataProvider *m_gps_data_provider;
#ifdef SELFTEST
	OpGeolocationDataProviderBase *m_selftest_data_provider;
#endif // SELFTEST
	BOOL m_data_providers_initialized;

	Head m_listeners;
	Head m_network_requests;

	GeoWifiData m_wifi_data;
	GeoRadioData m_radio_data;
	GeoGpsData m_gps_data;

	int m_num_supported_providers;
	double m_next_acquisition;
	BOOL m_request_posted;
	BOOL m_geolocation_enabled;
	BOOL m_needs_wifi_data;
	BOOL m_needs_radio_data;
	int m_has_got_any_data;
	int m_pending_callbacks;

	struct OpGeolocation::Position m_cached_position;

	/** Network API to use. */
	OpGeolocationGoogle2011NetworkApi m_network_api;

	void PositionAcquired(const OpGeolocation::Position &position);
	BOOL RequestGPS();

	static int CompareMAC(const OpWifiData::AccessPointData **a, const OpWifiData::AccessPointData **b);
	static int CompareSignalStrength(const OpWifiData::AccessPointData **a, const OpWifiData::AccessPointData **b);
	static int CompareCellID(const OpRadioData::CellData **a, const OpRadioData::CellData **b);
	static int CompareSignalStrength(const OpRadioData::CellData **a, const OpRadioData::CellData **b);

	void InitDataProviders();
public:
	GeolocationImplementation();
	virtual ~GeolocationImplementation();

	OP_STATUS Construct();

	void AcquireIfNeeded(long max_age = LONG_MAX);
	void Acquire(long max_delayms);
	OP_STATUS SendNetworkRequest();
	void RequestNetworkRequest();
	void RequestNetworkRequestIfDataReady();
	BOOL HasDataProviders() { return m_num_supported_providers > 0; }
	OP_STATUS PollProvider(OpGeolocationDataProviderBase *provider);

	// From OpGeolocationDataListener
	/* virtual */ void OnNewDataAvailable(OpWifiData *data);
	/* virtual */ void OnNewDataAvailable(OpRadioData *data);
	/* virtual */ void OnNewDataAvailable(OpGpsData *data);

	// From OpGeolocation
	/* virtual */ OP_STATUS StartReception(const Options &options, OpGeolocationListener *listener);
	/* virtual */ void StopReception(OpGeolocationListener *listener);
	/* virtual */ BOOL IsReady() { return IsEnabled(); }
	/* virtual */ BOOL IsEnabled() { return m_geolocation_enabled; }
	/* virtual */ BOOL IsListenerInUse(OpGeolocationListener *listener);

	// From OpGeolocationNetworkListener
	/* virtual */ void OnNetworkPositionAvailable(const OpGeolocation::Position &position, BOOL force_update = FALSE);
	/* virtual */ void OnNetworkPositionError(const OpGeolocation::Error &error);

	// From MessageObject
	/* virtual */ void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	// From OpPrefsListener
	/* virtual */ void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	// From GeoCollectorListener
	/* virtual */ void OnGpsNeeded() { RequestGPS(); }

#ifdef SELFTEST
	OpGeolocationDataProviderBase *GetSelftestDataProvider() { return m_selftest_data_provider; }
#endif // SELFTEST
	
	static BOOL WithinRadius(double radius, struct OpGeolocation::Position p1, struct OpGeolocation::Position p2);
};

#endif // GEOLOCATION_SUPPORT
#endif // GEOLOCATION_SRC_GEO_IMPL_H
