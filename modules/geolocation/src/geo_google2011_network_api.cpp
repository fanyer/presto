/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include <core/pch.h>

#ifdef GEOLOCATION_SUPPORT

#include "modules/geolocation/src/geo_google2011_network_api.h"

#include "modules/ecmascript/json_parser.h"
#include "modules/formats/uri_escape.h"
#include "modules/geolocation/src/geo_google2011_network_api_response_parser.h"
#include "modules/geolocation/src/geo_tools.h"
#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"

OP_STATUS
OpGeolocationGoogle2011NetworkApi::MakeRequestUrl(URL &request_url, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data)
{
	OpString request_url_string;
	OpStringC provider_url(g_pcgeolocation->GetStringPref(PrefsCollectionGeolocation::LocationProviderUrl));
	if (provider_url.IsEmpty())
		RETURN_IF_ERROR(request_url_string.Set(UNI_L("https://maps.googleapis.com/maps/api/browserlocation/json")));
	else
		RETURN_IF_ERROR(request_url_string.Set(provider_url));

	RETURN_IF_ERROR(AppendQueryString(request_url_string, wifi_data, radio_data, gps_data, GetAccessToken()));

	request_url = g_url_api->GetURL(request_url_string);
	if (request_url.IsEmpty())
		return OpStatus::ERR;

	RETURN_IF_ERROR(request_url.SetAttribute(URL::KDisableProcessCookies, TRUE));

	return OpStatus::OK;
}

OP_STATUS
OpGeolocationGoogle2011NetworkApi::ProcessResponse(const uni_char *response_buffer, unsigned long response_length, OpGeolocation::Position &pos)
{
	Google2011NetworkApiResponseParser parse_data;
	JSONParser parser(&parse_data);

	OP_STATUS parse_status = parser.Parse(response_buffer, response_length);
#ifdef OPERA_CONSOLE
	if (OpStatus::IsError(parse_status) && !OpStatus::IsMemoryError(parse_status))
		GeoTools::PostConsoleMessage(OpConsoleEngine::Error, UNI_L("Location provider responded with invalid data."), GeoTools::CustomProviderNote());
#endif // OPERA_CONSOLE
	RETURN_IF_ERROR(parse_status);

	if (parse_data.GetStatus() != Google2011NetworkApiResponseParser::OK)
	{
#ifdef OPERA_CONSOLE
		GeoTools::PostConsoleMessage(OpConsoleEngine::Error, UNI_L("Location provider responded with an error."), GeoTools::CustomProviderNote());
#endif // OPERA_CONSOLE
		return OpStatus::ERR;
	}

	if (op_isnan(parse_data.GetAccuracy()) || parse_data.GetAccuracy() < 0 || op_isnan(parse_data.GetLongitude()) || op_isnan(parse_data.GetLatitude()))
	{
#ifdef OPERA_CONSOLE
		GeoTools::PostConsoleMessage(OpConsoleEngine::Error, UNI_L("Location provider responded with insufficient data."), GeoTools::CustomProviderNote());
#endif // OPERA_CONSOLE
		return OpStatus::ERR;
	}

	pos.timestamp = g_op_time_info->GetTimeUTC();
	pos.capabilities = OpGeolocation::Position::STANDARD;
	pos.latitude = parse_data.GetLatitude();
	pos.longitude = parse_data.GetLongitude();
	pos.horizontal_accuracy = parse_data.GetAccuracy();

	if (parse_data.GetAccessToken() != NULL)
	{
		TRAPD(error, g_pcgeolocation->WriteStringL(g_pcgeolocation->Google2011LocationProviderAccessToken, parse_data.GetAccessToken()));
		// If we can't store the access token, we still can use the result.
		// OTOH, in practice we're most likely to get OOM in which case something else will fail anyway.
		if (OpStatus::IsMemoryError(error))
			g_memory_manager->RaiseCondition(error);
	}

	return OpStatus::OK;
}

static OP_STATUS
AppendEscapedValue(OpString &target, const uni_char *value)
{
	OpString escaped;
	RETURN_IF_ERROR(escaped.Set(value));
	RETURN_IF_ERROR(escaped.ReplaceAll(UNI_L("\\"), UNI_L("\\\\")));
	RETURN_IF_ERROR(escaped.ReplaceAll(UNI_L("|"), UNI_L("\\|")));
	return target.Append(escaped);
}

static int
RadioTypeCode(OpRadioData::RadioType radio_type)
{
	switch(radio_type)
	{
	case OpRadioData::RADIO_TYPE_GSM:   return 1;
	case OpRadioData::RADIO_TYPE_CDMA:  return 2;
	case OpRadioData::RADIO_TYPE_WCDMA: return 3;
	default:
		OP_ASSERT(!"Unexpected radio network type.");
		/* fallthrough */
	case OpRadioData::RADIO_TYPE_UNKNOWN: return 0;
	}
}

/** Compute how much space should be left in the request URI
 *  for radio data.
 *
 * The computed value is the upper bound for 5 cell tower
 * entries.
 */
inline static int
ComputeRadioDataReserve(GeoRadioData *radio_data)
{
	const int MAX_DEVICE_SEGMENT_LENGTH = 54;
	const int MAX_CELL_SEGMENT_LENGTH = 82;
	return MAX_DEVICE_SEGMENT_LENGTH + MAX_CELL_SEGMENT_LENGTH * MIN(radio_data->m_data.cell_towers.GetCount(), 5);
}

/* static */ OP_STATUS
OpGeolocationGoogle2011NetworkApi::AppendQueryString(OpString &url_string, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data, const uni_char *access_token)
{
	const int MAX_URI_LENGTH = 2048; // As per Google's spec.

	RETURN_IF_ERROR(url_string.AppendFormat(UNI_L("?browser=opera&sensor=true")));
	if (access_token)
	{
		RETURN_IF_ERROR(url_string.Append(UNI_L("&token=")));
		RETURN_IF_ERROR(UriEscape::AppendEscaped(url_string, access_token, UriEscape::FormUnsafe));
	}

	int url_string_length = url_string.Length();
	if (wifi_data)
	{
		int max_length;
		if (radio_data)
			max_length = MAX_URI_LENGTH - ComputeRadioDataReserve(radio_data);
		else
			max_length = MAX_URI_LENGTH;

		// Precondition: wifi_data->m_data.wifi_towers are sorted by signal strength, strongest first.
		for (UINT32 i = 0; i < wifi_data->m_data.wifi_towers.GetCount(); ++i)
		{
			OpWifiData::AccessPointData *ap = wifi_data->m_data.wifi_towers.Get(i);

			OpString wifi_string;
			// Mandatory values
			RETURN_IF_ERROR(wifi_string.AppendFormat(UNI_L("mac:%s|ss:%d|ssid:"), ap->mac_address.CStr(), ap->signal_strength));
			RETURN_IF_ERROR(AppendEscapedValue(wifi_string, ap->ssid));

			// Optional values
			if (wifi_data->Timestamp() > 0)
				RETURN_IF_ERROR(wifi_string.AppendFormat(UNI_L("|age:%u"), static_cast<unsigned>(g_op_time_info->GetTimeUTC() - wifi_data->Timestamp())));
			if (ap->channel > 0)
				RETURN_IF_ERROR(wifi_string.AppendFormat(UNI_L("|chan:%u"), ap->channel));
			if (ap->snr > 0)
				RETURN_IF_ERROR(wifi_string.AppendFormat(UNI_L("|snr:%d"), ap->snr));

			int escaped_length = UriEscape::GetEscapedLength(wifi_string.CStr(), wifi_string.Length(), UriEscape::FormUnsafe);
			if (escaped_length + url_string_length + 6 > max_length)
				break;

			RETURN_IF_ERROR(url_string.Append("&wifi="));
			RETURN_IF_ERROR(UriEscape::AppendEscaped(url_string, wifi_string, UriEscape::FormUnsafe));
			url_string_length += 6 + escaped_length;
		}
	}

	if (radio_data)
	{
		OpString device_string;

		RETURN_IF_ERROR(device_string.AppendFormat(UNI_L("mcc:%d|mnc:%d|rt:%d"),
				radio_data->m_data.home_mobile_country_code,
				radio_data->m_data.home_mobile_network_code,
				RadioTypeCode(radio_data->m_data.radio_type)));

		RETURN_IF_ERROR(url_string.Append("&device="));
		RETURN_IF_ERROR(UriEscape::AppendEscaped(url_string, device_string, UriEscape::FormUnsafe));

		url_string_length = url_string.Length();

		// Precondition: radio_data->m_data.cell_towers are sorted by signal strength, strongest first.
		for (UINT32 i = 0; i < radio_data->m_data.cell_towers.GetCount(); ++i)
		{
			OpRadioData::CellData *cell = radio_data->m_data.cell_towers.Get(i);

			OpString cell_string;

			RETURN_IF_ERROR(cell_string.AppendFormat(UNI_L("id:%d|lac:%u|mcc:%d|mnc:%d|ss:%d|ta:%d"),
					cell->cell_id,
					cell->location_area_code,
					cell->mobile_country_code,
					cell->mobile_network_code,
					cell->signal_strength,
					cell->timing_advance));

			int escaped_length = UriEscape::GetEscapedLength(cell_string.CStr(), cell_string.Length(), UriEscape::FormUnsafe);
			if (url_string_length + escaped_length + 6 > MAX_URI_LENGTH)
				break;

			RETURN_IF_ERROR(url_string.Append("&cell="));
			RETURN_IF_ERROR(UriEscape::AppendEscaped(url_string, cell_string, UriEscape::FormUnsafe));
			url_string_length += 6 + escaped_length;
		}
	}

	if (gps_data)
	{
		RETURN_IF_ERROR(url_string.AppendFormat(UNI_L("&location=lat%%3A%f%%7Clng%%3A%f"), gps_data->m_data.latitude, gps_data->m_data.longitude));
		if (url_string.Length() > MAX_URI_LENGTH)   // Too long? Revert
			url_string.Delete(url_string_length);
	}

	return OpStatus::OK;
}

const uni_char *
OpGeolocationGoogle2011NetworkApi::GetAccessToken()
{
	OpStringC access_token = g_pcgeolocation->GetStringPref(PrefsCollectionGeolocation::Google2011LocationProviderAccessToken);
	return access_token.IsEmpty() ? NULL : access_token.CStr();
}

#endif // GEOLOCATION_SUPPORT
