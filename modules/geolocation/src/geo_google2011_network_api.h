/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GEOLOCATION_NEW_NETWORK_API_H
#define GEOLOCATION_NEW_NETWORK_API_H

#ifdef GEOLOCATION_SUPPORT

#include "modules/geolocation/src/geo_network_request.h"

/** Implements the Google's 2011 location service client. */
class OpGeolocationGoogle2011NetworkApi : public OpGeolocationNetworkApi
{
public:
	virtual OP_STATUS MakeRequestUrl(URL &request_url, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data);
	virtual OP_STATUS ProcessResponse(const uni_char *response_buffer, unsigned long response_length, OpGeolocation::Position &pos);

#ifndef SELFTEST
private:
#endif // SELFTEST

	static OP_STATUS AppendQueryString(OpString &string, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data, const uni_char *access_token);
	const uni_char *GetAccessToken();
};

#endif // GEOLOCATION_SUPPORT

#endif // !GEOLOCATION_NEW_NETWORK_API_H
