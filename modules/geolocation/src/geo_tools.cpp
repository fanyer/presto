/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef GEOLOCATION_SUPPORT

#include "modules/geolocation/src/geo_tools.h"

#include "modules/prefs/prefsmanager/collections/pc_geoloc.h"

static inline double
Haversine(double x)
{
	double sin_x = op_sin(x/2);
	return sin_x * sin_x;
}

/* static */ double
GeoTools::Distance(double lat1, double long1, double lat2, double long2)
{
	const double R = 6371000; // Earth's mean radius in meters
	double to_rad = 3.14159265358979323846 / 180;
	double lat_diff = (lat1 - lat2) * to_rad;
	double long_diff = (long1 - long2) * to_rad;
	return 2 * R * op_asin(op_sqrt(Haversine(lat_diff) + op_cos(lat1 * to_rad) * op_cos(lat2 * to_rad) * Haversine(long_diff)));
}

/* static */ double
GeoTools::Distance(const GeoCoordinate &point1, const GeoCoordinate &point2)
{
	return Distance(point1.latitude, point1.longitude, point2.latitude, point2.longitude);
}

/* static */ BOOL
GeoTools::SignificantWifiChange(OpWifiData *data_a, OpWifiData *data_b)
{
	// Pre-condition: old_data and new_data are sorted by MAC address
	int equal = 0;
	int count_a = data_a->wifi_towers.GetCount();
	int count_b = data_b->wifi_towers.GetCount();

	for (int a=0, b=0; a < count_a && b < count_b;)
	{
		const OpString &mac_a(data_a->wifi_towers.Get(a)->mac_address);
		const OpString &mac_b(data_b->wifi_towers.Get(b)->mac_address);
		int ordering = mac_a.Compare(mac_b);
		if (ordering == 0)
		{
			equal++;
			a++; b++;
		}
		else if (ordering < 0)
			a++;
		else
			b++;
	}
	int changed = MAX(count_a, count_b) - equal;
	return changed > MIN(5, (count_a+1) / 2);
}

/* static */ BOOL
GeoTools::SignificantCellChange(OpRadioData *data_a, OpRadioData *data_b)
{
	return data_a->primary_cell_id != data_b->primary_cell_id;
}

#ifdef OPERA_CONSOLE
/* static */ void
GeoTools::PostConsoleMessage(OpConsoleEngine::Severity severity, const uni_char *msg, const uni_char *msg_part2 /* = NULL */)
{
	if (g_console->IsLogging())
	{
		OpConsoleEngine::Message message(OpConsoleEngine::Geolocation, OpConsoleEngine::Error);
		if (OpStatus::IsSuccess(message.message.SetConcat(msg, msg_part2)))
		{
			TRAPD(result, g_console->PostMessageL(&message));
			OpStatus::Ignore(result);
		}
	}
}

/* static */ const uni_char *
GeoTools::CustomProviderNote()
{
	return g_pcgeolocation->GetStringPref(PrefsCollectionGeolocation::LocationProviderUrl).IsEmpty() ? UNI_L("") : UNI_L(" (note: opera:config#Geolocation|LocationProviderUrl is set to a custom value)");
}
#endif // OPERA_CONSOLE

#endif // GEOLOCATION_SUPPORT
