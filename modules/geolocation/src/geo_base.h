/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GEOLOCATION_BASE_H
#define GEOLOCATION_BASE_H

#ifdef GEOLOCATION_SUPPORT
#include "modules/geolocation/geolocation.h"
#include "modules/pi/OpSystemInfo.h"

template<class C> class OpGeoDeviceData
{
	double m_timestamp;
public:
	OpGeoDeviceData<C>() : m_timestamp(.0) {}
	C m_data;
	
	BOOL HasData() const { return m_timestamp != 0; }
	void SetTimestamp() { m_timestamp = g_op_time_info->GetTimeUTC(); }
	double Timestamp() const { return m_timestamp; }
};

typedef OpGeoDeviceData<OpGpsData> GeoGpsData;

class GeoWifiData : public OpGeoDeviceData<OpWifiData>
{
public:
	OpWifiData m_data_sorted_by_mac;
};

class GeoRadioData : public OpGeoDeviceData<OpRadioData>
{
public:
	OpRadioData m_data_sorted_by_cell_id;
};

#endif //GEOLOCATION_SUPPORT

#endif // GEOLOCATION_BASE_H
