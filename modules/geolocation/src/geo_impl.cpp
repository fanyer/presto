/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef GEOLOCATION_SUPPORT

#include "modules/geolocation/src/geo_impl.h"

#include "modules/geolocation/src/geo_tools.h"
#include "modules/geolocation/src/geo_network_request.h"
#include "modules/pi/OpGeolocation.h"
#include "modules/pi/OpSystemInfo.h"
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"

#ifdef SELFTEST
# include "modules/geolocation/selftest/geo_selftest_helpers.h"
#endif // SELFTEST

/* static */ OP_STATUS
GeolocationlistenerElm::Make(GeolocationlistenerElm *& new_obj, const OpGeolocation::Options options, OpGeolocationListener *listener)
{
	GeolocationlistenerElm *elm = OP_NEW(GeolocationlistenerElm, (options, listener));
	if (!elm)
		return OpStatus::ERR_NO_MEMORY;

	OP_STATUS err;
	if (OpStatus::IsError(err = elm->Construct()))
	{
		OP_DELETE(elm);
		return err;
	}

	new_obj = elm;

	return OpStatus::OK;
}

OP_STATUS
GeolocationlistenerElm::Construct()
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GEOLOCATION_TIMEOUT, Id()));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GEOLOCATION_ELM_RELEASE, Id()));

	return OpStatus::OK;
}

inline static BOOL
PositionIsDifferent(const OpGeolocation::Position &p1, const OpGeolocation::Position &p2)
{
	// longitude and/or longitude differs
	return !(p1.longitude == p2.longitude && p1.latitude == p2.latitude);
}

/* virtual */ void
GeolocationlistenerElm::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(par1 == Id());

	switch (msg)
	{
	case MSG_GEOLOCATION_TIMEOUT:
		{
			m_timeout_msg_posted = FALSE;
			double now =  g_op_time_info->GetTimeUTC();
			if (m_position.timestamp && now - m_position.timestamp < m_options.maximum_age)
				OnPositionAvailable(m_position, now);	// We have some geolocation position, use that.
			else
				OnPositionError(OpGeolocation::Error(OpGeolocation::Error::TIMEOUT_ERROR, OpGeolocation::ANY), now); // Timed out
		}
		break;
	case MSG_GEOLOCATION_ELM_RELEASE:
		OP_DELETE(this);
	default:
		break;
	}
}

GeolocationlistenerElm::GeolocationlistenerElm(const OpGeolocation::Options options, OpGeolocationListener *listener)
	: m_options(options)
	, m_listener(listener)
	, m_fired(0.0)
	, m_error(OpGeolocation::Error::ERROR_NONE, 0)
	, m_timeout_msg_posted(FALSE)
{
	if (m_options.timeout < 0)
		m_options.timeout = 0;

	if (m_options.maximum_age < 0)
		m_options.maximum_age = 0;

	if (m_options.high_accuracy)
		m_options.type |= OpGeolocation::GPS;

	op_memset(&m_position, 0, sizeof(m_position));
}

/* virtual */
GeolocationlistenerElm::~GeolocationlistenerElm()
{
	Out();
	CancelTimeout();
	g_main_message_handler->RemoveDelayedMessage(MSG_GEOLOCATION_ELM_RELEASE, Id(), 0);
	g_main_message_handler->UnsetCallBacks(this);

	m_listener = NULL;
}

void
GeolocationlistenerElm::StartTimeout(int delay)
{
	if (!m_timeout_msg_posted)
		m_timeout_msg_posted = g_main_message_handler->PostDelayedMessage(MSG_GEOLOCATION_TIMEOUT, Id(), 0, m_options.timeout + delay);
}

void
GeolocationlistenerElm::CancelTimeout()
{
	g_main_message_handler->RemoveDelayedMessage(MSG_GEOLOCATION_TIMEOUT, Id(), 0);
	m_timeout_msg_posted = FALSE;
}

void
GeolocationlistenerElm::ReleaseIfNeeded()
{
	if (m_fired && m_options.one_shot)
	{
		Out();
		g_main_message_handler->PostMessage(MSG_GEOLOCATION_ELM_RELEASE, Id(), 0);
	}
}

BOOL
GeolocationlistenerElm::HasFired(double now_utc)
{
	if (m_options.one_shot)
		return m_fired != 0.0;
	else if (m_fired)
	{
		// If we're watchPosition'ing and we've been triggered before, consider
		// ourselves as triggered as long as we're younger than maximum_age
		return now_utc - m_fired < MaximumAge();
	}
	else
		return FALSE;
}

void
GeolocationlistenerElm::OnPositionAvailable(const OpGeolocation::Position &position, double now_utc)
{
	BOOL new_pos = PositionIsDifferent(position, m_position);

	// Only trigger for requested type of provider
	// Positions coming in here are expected to be young enough for us
	if (position.type & m_options.type && new_pos && !HasFired(now_utc))
	{
		OP_NEW_DBG("GeolocationlistenerElm", "geolocation");
		OP_DBG((UNI_L("OnPositionAvailable(0x%lx)"), this));

		m_position = position;	// Cache position for later (eg. in case of high acc. and gps times out)
		m_error = OpGeolocation::Error(OpGeolocation::Error::ERROR_NONE, 0);

		m_fired = now_utc;
		CancelTimeout();
		ReleaseIfNeeded(); // issue delayed destruction
		m_listener->OnGeolocationUpdated(position);	// might call StopReception and thus delete us
	}
}

void
GeolocationlistenerElm::OnPositionError(const OpGeolocation::Error &error, double now_utc)
{
	BOOL new_error = error.error != m_error.error || error.type != m_error.type;

	// Only trigger for requested type of provider
	if (error.type & m_options.type && new_error && !HasFired(now_utc))
	{
		OP_NEW_DBG("GeolocationlistenerElm", "geolocation");
		OP_DBG((UNI_L("OnPositionError(0x%lx)"), this));

		m_error = error;
		op_memset(&m_position, 0, sizeof(m_position));

		m_fired = now_utc;
		CancelTimeout();
		ReleaseIfNeeded(); // issue delayed destruction
		m_listener->OnGeolocationError(error);	// might call StopReception and thus delete us
	}
}

//////////////////////////////////////////////////////////////////////////

GeolocationImplementation::GeolocationImplementation()
	: m_wifi_data_provider(NULL)
	, m_radio_data_provider(NULL)
	, m_gps_data_provider(NULL)
#ifdef SELFTEST
	, m_selftest_data_provider(NULL)
#endif // SELFTEST
	, m_data_providers_initialized(FALSE)
	, m_num_supported_providers(0)
	, m_next_acquisition(g_stdlib_infinity)
	, m_request_posted(FALSE)
	, m_geolocation_enabled(TRUE)
	, m_needs_wifi_data(FALSE)
	, m_needs_radio_data(FALSE)
	, m_has_got_any_data(0)
	, m_pending_callbacks(0)
{
	op_memset(&m_cached_position, 0, sizeof(m_cached_position));
}

/* virtual */
GeolocationImplementation::~GeolocationImplementation()
{
	g_main_message_handler->RemoveDelayedMessage(MSG_GEOLOCATION_POLL, 0, 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_GEOLOCATION_REQUEST_NETWORK_LOOKUP, 0, 0);
	g_main_message_handler->RemoveDelayedMessage(MSG_GEOLOCATION_DEVICE_DATA_TIMEOUT, 0, 0);
	g_main_message_handler->UnsetCallBacks(this);

	OP_DELETE(m_wifi_data_provider);
	OP_DELETE(m_radio_data_provider);
	OP_DELETE(m_gps_data_provider);
#ifdef SELFTEST
	OP_DELETE(m_selftest_data_provider);
#endif // SELFTEST

	m_listeners.Clear();
	m_network_requests.Clear();
}

OP_STATUS
GeolocationImplementation::Construct()
{
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GEOLOCATION_POLL, 0));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GEOLOCATION_REQUEST_NETWORK_LOOKUP, 0));
	RETURN_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_GEOLOCATION_DEVICE_DATA_TIMEOUT, 0));

	m_geolocation_enabled = g_pcgeolocation->GetIntegerPref(PrefsCollectionGeolocation::EnableGeolocation);

	return g_pcgeolocation->RegisterListener(this);
}

void GeolocationImplementation::InitDataProviders()
{
	if (OpStatus::IsSuccess(OpGeolocationWifiDataProvider::Create(m_wifi_data_provider, this)))
		m_num_supported_providers++;

	if (OpStatus::IsSuccess(OpGeolocationRadioDataProvider::Create(m_radio_data_provider, this)))
		m_num_supported_providers++;

	if (OpStatus::IsSuccess(OpGeolocationGpsDataProvider::Create(m_gps_data_provider, this)))
		m_num_supported_providers++;

#ifdef SELFTEST
	OpStatus::Ignore(OpGeolocationSelftestDataProvider::Create(m_selftest_data_provider, this));
#endif // SELFTEST

	m_data_providers_initialized = TRUE;
}

void
GeolocationImplementation::AcquireIfNeeded(long max_age /* = LONG_MAX */)
{
	BOOL acquire = FALSE;

	GeolocationlistenerElm *elm = static_cast<GeolocationlistenerElm*>(m_listeners.First());
	while (elm)
	{
		acquire = TRUE;
		max_age = MIN(elm->MaximumAge(), max_age);

		elm = static_cast<GeolocationlistenerElm*>(elm->Suc());
	}

	// No acqusition needed if thare are no listeners
	if (!acquire)
	{
		g_main_message_handler->RemoveDelayedMessage(MSG_GEOLOCATION_POLL, 0, 0);
		m_next_acquisition = g_stdlib_infinity;
		return;
	}

	if (max_age < LONG_MAX)
		Acquire(max_age);
}

void
GeolocationImplementation::Acquire(long max_delayms)
{
	BOOL post = FALSE;
	double now = g_op_time_info->GetRuntimeMS();
	if (now + max_delayms < m_next_acquisition)
	{
		// reschedule
		m_next_acquisition = now + max_delayms;
		g_main_message_handler->RemoveDelayedMessage(MSG_GEOLOCATION_POLL, 0, 0);
		post = TRUE;
	}
	else if (m_next_acquisition == g_stdlib_infinity)
	{
		m_next_acquisition = now + max_delayms;
		post = TRUE;
	}

	if (post)
	{
		g_main_message_handler->PostDelayedMessage(MSG_GEOLOCATION_POLL, 0, 0, max_delayms);

		GeolocationlistenerElm *elm = static_cast<GeolocationlistenerElm*>(m_listeners.First());
		while (elm)
		{
			GeolocationlistenerElm *suc = static_cast<GeolocationlistenerElm*>(elm->Suc());
			elm->AcqusitionStarted(max_delayms);
			elm = suc;
		}
	}
}

void
GeolocationImplementation::RequestNetworkRequest()
{
	if (!m_request_posted)
		m_request_posted = g_main_message_handler->PostMessage(MSG_GEOLOCATION_REQUEST_NETWORK_LOOKUP, 0, 0);
}

void
GeolocationImplementation::RequestNetworkRequestIfDataReady()
{
	// If we have the data we need, send request.
	if (!(m_needs_wifi_data && m_needs_radio_data))
		RequestNetworkRequest();
}

OP_STATUS
GeolocationImplementation::SendNetworkRequest()
{
	OpGeolocationNetworkRequest *obj;
	RETURN_IF_ERROR(
		OpGeolocationNetworkRequest::Create(obj,
			&m_network_api,
			m_wifi_data.HasData()  ? &m_wifi_data : NULL,
			m_radio_data.HasData() ? &m_radio_data : NULL,
			m_gps_data.HasData()   ? &m_gps_data : NULL)
			);
	obj->Into(&m_network_requests);
	obj->SetListener(this);

	return OpStatus::OK;
}

/* static */ int
GeolocationImplementation::CompareMAC(const OpWifiData::AccessPointData **a, const OpWifiData::AccessPointData **b)
{
	return (*a)->mac_address.Compare((*b)->mac_address);
}

/* static */ int
GeolocationImplementation::CompareSignalStrength(const OpWifiData::AccessPointData **a, const OpWifiData::AccessPointData **b)
{
	return (*b)->signal_strength - (*a)->signal_strength;
}

// From OpGeolocationDataListener
/* virtual */ void
GeolocationImplementation::OnNewDataAvailable(OpWifiData *new_data)
{
	--m_pending_callbacks;
	OP_ASSERT(m_pending_callbacks >= 0);

	if (!new_data)
	{
		if (!m_pending_callbacks && !m_has_got_any_data)
			OnNetworkPositionError(OpGeolocation::Error(OpGeolocation::Error::PROVIDER_ERROR, OpGeolocation::WIFI));
		return;
	}

	OpWifiData new_data_sorted_by_signal_strength;
	RAISE_AND_RETURN_VOID_IF_ERROR(new_data->CopyTo(new_data_sorted_by_signal_strength));
	new_data_sorted_by_signal_strength.wifi_towers.QSort(CompareSignalStrength);

	int towers = new_data_sorted_by_signal_strength.wifi_towers.GetCount();
	if (towers > 25)
	{
		new_data_sorted_by_signal_strength.wifi_towers.Delete(25, towers - 25);
		OP_ASSERT(new_data_sorted_by_signal_strength.wifi_towers.GetCount() == 25);
	}

	OpWifiData new_data_sorted_by_mac;
	RAISE_AND_RETURN_VOID_IF_ERROR(new_data_sorted_by_signal_strength.CopyTo(new_data_sorted_by_mac));
	new_data_sorted_by_mac.wifi_towers.QSort(CompareMAC);

	OP_ASSERT(new_data_sorted_by_mac.wifi_towers.GetCount() <= 25);

	BOOL significant_change = TRUE;
	if (g_pcgeolocation->GetIntegerPref(PrefsCollectionGeolocation::SendLocationRequestOnlyOnChange))
	{
		significant_change = GeoTools::SignificantWifiChange(&m_wifi_data.m_data_sorted_by_mac, &new_data_sorted_by_mac);
	}

	RAISE_AND_RETURN_VOID_IF_ERROR(new_data_sorted_by_mac.MoveTo(m_wifi_data.m_data_sorted_by_mac));
	RAISE_AND_RETURN_VOID_IF_ERROR(new_data_sorted_by_signal_strength.MoveTo(m_wifi_data.m_data));

	m_wifi_data.SetTimestamp();

	m_needs_wifi_data = FALSE;
	m_has_got_any_data++;

	if (significant_change || m_cached_position.timestamp == 0)
		RequestNetworkRequestIfDataReady();
	else
		OnNetworkPositionAvailable(m_cached_position);
}

/* static */ int
GeolocationImplementation::CompareCellID(const OpRadioData::CellData **a, const OpRadioData::CellData **b)
{
	return (*a)->cell_id - (*b)->cell_id;
}

/* static */ int
GeolocationImplementation::CompareSignalStrength(const OpRadioData::CellData **a, const OpRadioData::CellData **b)
{
	return (*b)->signal_strength - (*a)->signal_strength;
}

/* virtual */ void
GeolocationImplementation::OnNewDataAvailable(OpRadioData *data)
{
	--m_pending_callbacks;
	OP_ASSERT(m_pending_callbacks >= 0);

	if (!data)
	{
		if (!m_pending_callbacks && !m_has_got_any_data)
			OnNetworkPositionError(OpGeolocation::Error(OpGeolocation::Error::PROVIDER_ERROR, OpGeolocation::RADIO));
		return;
	}

	OpRadioData new_data_sorted_by_signal_strength;
	RAISE_AND_RETURN_VOID_IF_ERROR(data->CopyTo(new_data_sorted_by_signal_strength));
	new_data_sorted_by_signal_strength.cell_towers.QSort(CompareSignalStrength);

	int towers = new_data_sorted_by_signal_strength.cell_towers.GetCount();
	if (towers > 25)
	{
		new_data_sorted_by_signal_strength.cell_towers.Delete(25, towers - 25);
		OP_ASSERT(new_data_sorted_by_signal_strength.cell_towers.GetCount() == 25);
	}

	OP_ASSERT(new_data_sorted_by_signal_strength.cell_towers.GetCount() <= 25);

	OpRadioData new_data_sorted_by_cell_id;
	RAISE_AND_RETURN_VOID_IF_ERROR(new_data_sorted_by_signal_strength.CopyTo(new_data_sorted_by_cell_id));
	new_data_sorted_by_cell_id.cell_towers.QSort(CompareCellID);

	BOOL significant_change = TRUE;
	if (g_pcgeolocation->GetIntegerPref(PrefsCollectionGeolocation::SendLocationRequestOnlyOnChange))
	{
		significant_change = GeoTools::SignificantCellChange(&m_radio_data.m_data_sorted_by_cell_id, &new_data_sorted_by_cell_id);
	}

	RAISE_AND_RETURN_VOID_IF_ERROR(new_data_sorted_by_signal_strength.MoveTo(m_radio_data.m_data));
	RAISE_AND_RETURN_VOID_IF_ERROR(new_data_sorted_by_cell_id.MoveTo(m_radio_data.m_data_sorted_by_cell_id));

	m_radio_data.SetTimestamp();

	m_needs_radio_data = FALSE;
	m_has_got_any_data++;

	if (significant_change || m_cached_position.timestamp == 0)
		RequestNetworkRequestIfDataReady();
	else
		OnNetworkPositionAvailable(m_cached_position);
}

/* virtual */ void
GeolocationImplementation::OnNewDataAvailable(OpGpsData *data)
{
	--m_pending_callbacks;
	OP_ASSERT(m_pending_callbacks >= 0);

	if (data && OpStatus::IsSuccess(data->CopyTo(m_gps_data.m_data)))
	{
		m_gps_data.SetTimestamp();
		m_has_got_any_data++;

		OpGeolocation::Position position;
		op_memset(&position, 0, sizeof position);
		if (OpStatus::IsSuccess(position.CopyFrom(*data, m_gps_data.Timestamp())))
		{
			OnNetworkPositionAvailable(position, TRUE); // Force update on gps
			return;	// Success!
		}
	}

	if (!m_pending_callbacks && !m_has_got_any_data)
	{
		// Failure! If we got here, something went wrong.
		OnNetworkPositionError(OpGeolocation::Error(OpGeolocation::Error::PROVIDER_ERROR, OpGeolocation::GPS));
	}
}

// From OpGeolocation
OP_STATUS
GeolocationImplementation::StartReception(const Options &options, OpGeolocationListener *listener)
{
	if (!m_geolocation_enabled)
	{
		listener->OnGeolocationError(OpGeolocation::Error(OpGeolocation::Error::PROVIDER_ERROR, OpGeolocation::ANY));
		return OpStatus::OK;
	}

	GeolocationlistenerElm *elm = static_cast<GeolocationlistenerElm*>(m_listeners.First());
	while (elm)
	{
		if (elm->GetListener() == listener)
			break;

		elm = static_cast<GeolocationlistenerElm*>(elm->Suc());
	}

	if (!elm)
	{
		if (OpStatus::IsError(GeolocationlistenerElm::Make(elm, options, listener)))
		{
			listener->OnGeolocationError(OpGeolocation::Error(OpGeolocation::Error::GENERIC_ERROR, OpGeolocation::ANY));
			return OpStatus::ERR_NO_MEMORY;
		}
		elm->Into(&m_listeners);
	}

	double now =  g_op_time_info->GetTimeUTC();
	if (m_cached_position.timestamp && options.maximum_age >= 0 && now - m_cached_position.timestamp < options.maximum_age)
	{
		// if we have a cached position that is valid for this request, feed that.
		elm->OnPositionAvailable(m_cached_position, now);
		AcquireIfNeeded();
	}
	else if (options.timeout == 0)
	{
		// if the cached position is not valid for this request AND timeout is 0, timeout immediately.
		elm->OnPositionError(OpGeolocation::Error(OpGeolocation::Error::TIMEOUT_ERROR, OpGeolocation::ANY), now);
		AcquireIfNeeded();
	}
	else
		// else acquire a new position immediately
		Acquire(0);

	return OpStatus::OK;
}

void
GeolocationImplementation::StopReception(OpGeolocationListener *listener)
{
	GeolocationlistenerElm *elm = static_cast<GeolocationlistenerElm*>(m_listeners.First());
	while (elm)
	{
		if (elm->GetListener() == listener)
		{
			OP_DELETE(elm);
			break;
		}

		elm = static_cast<GeolocationlistenerElm*>(elm->Suc());
	}

	// Reschedule
	AcquireIfNeeded();
}

/* virtual */ BOOL
GeolocationImplementation::IsListenerInUse(OpGeolocationListener *listener)
{
	GeolocationlistenerElm *elm = static_cast<GeolocationlistenerElm*>(m_listeners.First());
	while (elm)
	{
		if (elm->GetListener() == listener)
			return TRUE;

		elm = static_cast<GeolocationlistenerElm*>(elm->Suc());
	}

	return FALSE;
}

// From OpGeolocationNetworkListener
/* virtual */ void
GeolocationImplementation::OnNetworkPositionAvailable(const OpGeolocation::Position &position, BOOL force_update)
{
	struct OpGeolocation::Position* usedPosition = const_cast<struct OpGeolocation::Position*>(&position);
	
	/* Filter out what gets sent to the listener:
	 *   Positions that are suspected of being extreme values
	 *   should be filtered.
	 */
	if (!force_update && m_cached_position.timestamp)
	{
		// If the new position is within the old positions accuracy radius
		if (GeolocationImplementation::WithinRadius(m_cached_position.horizontal_accuracy,
				position, m_cached_position))
		{
			/* It looks like our provider has a lower limit of the accuracy
			 * for wifi + radio based positions, and the limit is quite high ~150 meters,
			 * even though the positions ofter turn out to be very correct.
			 * 
			 * Therefore, we let the old positions accuracy deteriorate exponentially
			 * in order to acquire new positions
			 */
			
			double delta = position.timestamp - m_cached_position.timestamp;
			
			// exponential growth that doubles after 10 seconds
			double multiplier = op_pow(2, delta / 10000);
			
			if (position.horizontal_accuracy >= (m_cached_position.horizontal_accuracy * multiplier))
				usedPosition = &m_cached_position;
		}
		else
		{
			/* If the new point was outside of the old positions accuracy radius,
			 * check to see if the old point is within the new positions accuracy
			 * radius. If so, give back the old position. 
			 */
			if (GeolocationImplementation::WithinRadius(position.horizontal_accuracy, position, m_cached_position))
				usedPosition = &m_cached_position;
		}
	}
	
	PositionAcquired(*usedPosition);	// feed position cache
}

/* virtual */ void
GeolocationImplementation::OnNetworkPositionError(const OpGeolocation::Error &error)
{
	GeolocationlistenerElm *elm = static_cast<GeolocationlistenerElm*>(m_listeners.First());
	while (elm)
	{
		elm->OnPositionError(error, g_op_time_info->GetTimeUTC());
		elm = static_cast<GeolocationlistenerElm*>(elm->Suc());
	}

	AcquireIfNeeded();
}

/* static */
BOOL GeolocationImplementation::WithinRadius(double radius, struct OpGeolocation::Position p1, struct OpGeolocation::Position p2)
{
	double delta = GeoTools::Distance(p1.latitude, p1.longitude, p2.latitude, p2.longitude);
	
	return delta < radius;
}

OP_STATUS
GeolocationImplementation::PollProvider(OpGeolocationDataProviderBase *provider)
{
	if (!provider)
		return OpStatus::ERR_NULL_POINTER;

	OP_STATUS err = provider->Poll();
	if (OpStatus::IsError(err))
		--m_pending_callbacks;

	return err;
}

/* virtual */ void
GeolocationImplementation::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	if (msg == MSG_GEOLOCATION_POLL)
	{
		if (!m_data_providers_initialized)
			InitDataProviders();
		m_next_acquisition = g_stdlib_infinity;
		m_has_got_any_data = 0;

		if (m_num_supported_providers == 0
#ifdef SELFTEST
			&& m_selftest_data_provider && !g_selftest.running
#endif // SELFTEST
			)
		{
			RequestNetworkRequest();
		}
		else
		{
			m_needs_wifi_data = m_needs_radio_data = FALSE;
			m_pending_callbacks = m_num_supported_providers;

#ifdef SELFTEST
			if (g_selftest.running)
			{
				++m_pending_callbacks;
				OpStatus::Ignore(PollProvider(m_selftest_data_provider));
			}
			else
#endif // SELFTEST
			{
				if (OpStatus::IsSuccess(PollProvider(m_wifi_data_provider)))
					m_needs_wifi_data = TRUE;
				if (OpStatus::IsSuccess(PollProvider(m_radio_data_provider)))
					m_needs_radio_data = TRUE;
				OpStatus::Ignore(PollProvider(m_gps_data_provider));
			}

			// If we have no pending callbacks from the platform, triggered by calls to Poll:
			// send off an "empty" network request, in the hopes of getting ip location.
			if (!m_pending_callbacks && !m_has_got_any_data)
				RequestNetworkRequest();
			else if (m_needs_wifi_data || m_needs_radio_data)
				g_main_message_handler->PostDelayedMessage(MSG_GEOLOCATION_DEVICE_DATA_TIMEOUT, 0, 0, GEOLOCATION_DEFAULT_DEVICE_DATA_TIMEOUT);
		}
	}
	else if (msg == MSG_GEOLOCATION_REQUEST_NETWORK_LOOKUP)
	{
		m_request_posted = FALSE;
		if (OpStatus::IsError(SendNetworkRequest()))
			OnNetworkPositionError(OpGeolocation::Error(OpGeolocation::Error::PROVIDER_ERROR, OpGeolocation::ANY));
	}
	else if (msg == MSG_GEOLOCATION_DEVICE_DATA_TIMEOUT)
	{
		RequestNetworkRequest();
	}
	else
		OP_ASSERT(!"I shouldn't have gotten that message");
}

/* virtual */ void
GeolocationImplementation::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	if (id == OpPrefsCollection::Geolocation && pref == PrefsCollectionGeolocation::EnableGeolocation)
		m_geolocation_enabled = newvalue ? TRUE : FALSE;
}

void
GeolocationImplementation::PositionAcquired(const OpGeolocation::Position &position)
{
	// Only cache younger positions.
	if (position.timestamp > m_cached_position.timestamp)
		m_cached_position = position;

	GeolocationlistenerElm *elm = static_cast<GeolocationlistenerElm*>(m_listeners.First());
	while (elm)
	{
		elm->OnPositionAvailable(m_cached_position, m_cached_position.timestamp);
		elm = static_cast<GeolocationlistenerElm*>(elm->Suc());
	}

	AcquireIfNeeded();
}

BOOL
GeolocationImplementation::RequestGPS()
{
	if (!m_data_providers_initialized)
		InitDataProviders();
	if (m_gps_data_provider)
	{
		RETURN_VALUE_IF_ERROR(m_gps_data_provider->Poll(), FALSE);
		return TRUE;
	}

	return FALSE;
}

#endif // GEOLOCATION_SUPPORT
