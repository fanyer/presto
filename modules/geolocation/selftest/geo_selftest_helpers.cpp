/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
**
*/

#include "core/pch.h"

#if defined(SELFTEST) && defined(GEOLOCATION_SUPPORT)

#include "modules/geolocation/selftest/geo_selftest_helpers.h"
#include "modules/geolocation/geolocation.h"


/* static */ OP_STATUS
OpGeolocationSelftestDataProvider::Create(OpGeolocationDataProviderBase *& new_obj, OpGeolocationDataListener *listener)
{
	OpGeolocationSelftestDataProvider *obj = OP_NEW(OpGeolocationSelftestDataProvider, ());
	if (!obj)
		return OpStatus::ERR_NO_MEMORY;
	
	obj->m_listener = listener;
	new_obj = obj;
	
	return OpStatus::OK;
}

OP_STATUS
OpGeolocationSelftestDataProvider::Poll()
{
	OpGpsData data;
	op_memset(&data, 0, sizeof(data));
	data.capabilities = OpGeolocation::Position::STANDARD |
						OpGeolocation::Position::HAS_ALTITUDE |
						OpGeolocation::Position::HAS_ALTITUDEACCURACY |
						OpGeolocation::Position::HAS_HEADING |
						OpGeolocation::Position::HAS_VELOCITY;
	data.latitude				= 10.0;
	data.longitude				= 60.0;
	data.horizontal_accuracy	= 10.0;
	data.altitude				= 50.0;
	data.vertical_accuracy		= 15.0;
	data.heading				= 270.0;
	data.velocity				= 0.7;
	
	m_listener->OnNewDataAvailable(&data);
	
	return OpStatus::OK;
}

#endif // SELFTEST && GEOLOCATION_SUPPORT
