/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef GEOLOCATION_SUPPORT

#include "modules/pi/OpGeolocation.h"
#include "modules/geolocation/geolocation.h"

OP_STATUS
OpGpsData::CopyTo(OpGpsData &destination)
{
	destination = *this; // This one is flat
	return OpStatus::OK;
}

OP_STATUS
OpWifiData::CopyTo(OpWifiData &destination)
{
	destination.wifi_towers.DeleteAll();

	for (UINT32 i = 0; i < wifi_towers.GetCount(); i++)
	{
		AccessPointData *apdata = wifi_towers.Get(i);
		if (apdata)
		{
			OpAutoPtr<OpWifiData::AccessPointData> apdata_copy;
			apdata_copy = OP_NEW(OpWifiData::AccessPointData, ());
			if (!apdata_copy.get())
				return OpStatus::ERR_NO_MEMORY;

			apdata_copy->channel = apdata->channel;
			RETURN_IF_ERROR(apdata_copy->mac_address.Set(apdata->mac_address));
			apdata_copy->signal_strength = apdata->signal_strength;
			apdata_copy->snr = apdata->snr;
			RETURN_IF_ERROR(apdata_copy->ssid.Set(apdata->ssid));

			RETURN_IF_ERROR(destination.wifi_towers.Add(apdata_copy.get()));
			apdata_copy.release();
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpWifiData::MoveTo(OpWifiData &destination)
{
	destination.wifi_towers.DeleteAll();

	for (UINT32 i = 0; i < wifi_towers.GetCount(); i++)
	{
		AccessPointData *apdata = wifi_towers.Get(i);
		OP_STATUS add_status = destination.wifi_towers.Add(apdata);
		if (OpStatus::IsError(add_status))
		{
			destination.wifi_towers.Clear();
			return add_status;
		}
	}

	wifi_towers.Clear();
	return OpStatus::OK;
}

OP_STATUS
OpRadioData::CopyTo(OpRadioData &destination)
{
	destination.cell_towers.DeleteAll();

	destination.radio_type = radio_type;
	RETURN_IF_ERROR(destination.carrier.Set(carrier));
	destination.home_mobile_network_code = home_mobile_network_code;
	destination.home_mobile_country_code = home_mobile_country_code;
	destination.primary_cell_id = primary_cell_id;

	for (UINT32 i = 0; i < cell_towers.GetCount(); i++)
	{
		CellData *cell_data = cell_towers.Get(i);
		if (cell_data)
		{
			OpAutoPtr<OpRadioData::CellData> cell_data_copy;
			cell_data_copy = OP_NEW(OpRadioData::CellData, ());
			if (!cell_data_copy.get())
				return OpStatus::ERR_NO_MEMORY;

			cell_data_copy->cell_id = cell_data->cell_id;
			cell_data_copy->location_area_code = cell_data->location_area_code;
			cell_data_copy->mobile_network_code = cell_data->mobile_network_code;
			cell_data_copy->mobile_country_code = cell_data->mobile_country_code;
			cell_data_copy->signal_strength = cell_data->signal_strength;
			cell_data_copy->timing_advance = cell_data->timing_advance;

			RETURN_IF_ERROR(destination.cell_towers.Add(cell_data_copy.get()));
			cell_data_copy.release();
		}
	}

	return OpStatus::OK;
}

OP_STATUS
OpRadioData::MoveTo(OpRadioData &destination)
{
	destination.cell_towers.DeleteAll();

	for (UINT32 i = 0; i < cell_towers.GetCount(); i++)
	{
		CellData *cell_data = cell_towers.Get(i);
		if (cell_data)
		{
			OP_STATUS add_status = destination.cell_towers.Add(cell_data);
			if (OpStatus::IsError(add_status))
			{
				destination.cell_towers.Clear();
				return add_status;
			}
		}
	}

	destination.radio_type = radio_type;
	destination.carrier.TakeOver(carrier);
	destination.home_mobile_network_code = home_mobile_network_code;
	destination.home_mobile_country_code = home_mobile_country_code;
	destination.primary_cell_id = primary_cell_id;

	cell_towers.Clear();
	return OpStatus::OK;

}

OP_STATUS
OpGeolocation::Position::CopyFrom(struct OpGpsData &source, double source_timestamp)
{
	capabilities = source.capabilities;
	timestamp = source_timestamp;
	latitude = source.latitude;
	longitude = source.longitude;
	horizontal_accuracy = source.horizontal_accuracy;
	altitude = source.altitude;
	vertical_accuracy = source.vertical_accuracy;
	heading = source.heading;
	velocity = source.velocity;

	type |= OpGeolocation::GPS;


	return OpStatus::OK;
}

#endif // GEOLOCATION_SUPPORT
