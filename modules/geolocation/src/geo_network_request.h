/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef GEOLOCATION_NETWORK_PROVIDER_H
#define GEOLOCATION_NETWORK_PROVIDER_H

#ifdef GEOLOCATION_SUPPORT

#include "modules/hardcore/mh/messobj.h"
#include "modules/util/simset.h"
#include "modules/pi/OpGeolocation.h"
#include "modules/geolocation/geolocation.h"
#include "modules/geolocation/src/geo_base.h"

class OpGeolocationNetworkListener;

/** Provides the functionality to create a request and read the response.
 *
 * It may be used by multiple requests simultaneously (i.e. there is not per-request state).
 */
class OpGeolocationNetworkApi
{
public:
	virtual ~OpGeolocationNetworkApi() {}

	/** Constructs a request to obtain location.
	 *
	 * Not all provided data has to be used in constructing the request.
	 *
	 * Also, any and all of the wifi_data, radio_data and gps_data may be NULL.
	 * In case all of them are NULL, the location service is usually able to provide
	 * location based on the IP database.
	 *
	 * @param request_url Filled with the request. Only valid if the function returns OK.
	 * @param wifi_data WiFi data to send in the request. May be NULL.
	 * @param radio_data Radio (cell tower) data to send in the request. May be NULL.
	 * @param gps_data GPS data to send in the request. May be NULL.
	 * @returns
	 *  OK - the request has been constructed successfully.
	 */
	virtual OP_STATUS MakeRequestUrl(URL &request_url, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data) = 0;

	/** Read position from the response data.
	 *
	 * @param response_buffer Buffer with the response data.
	 * @param response_length Length of the data in response_buffer.
	 * @param position Set to the position read from the response. Valid only if the function returns OK.
	 * @return
	 *  - OK - response read successfully, there is a position in the position parameter,
	 *  - ERR_NO_MEMORY - out of memory,
	 *  - ERR - parsing failed (response is invalid).
	 */
	virtual OP_STATUS ProcessResponse(const uni_char *response_buffer, unsigned long response_length, OpGeolocation::Position &position) = 0;
};

class OpGeolocationNetworkRequest
	: public MessageObject
	, public Link
{
public:
	static OP_STATUS Create(OpGeolocationNetworkRequest *&new_obj, OpGeolocationNetworkApi *network_api, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data);

	void SetListener(OpGeolocationNetworkListener *listener) { m_listener = listener; }

/*	static OP_STATUS BuildJSONRequest(TempBuffer &request, const uni_char *access_token, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data); */
protected:
	virtual ~OpGeolocationNetworkRequest();

	void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	OP_STATUS SetCallbacks();
	void UnsetCallbacks();

	void HandleFinished();
	void HandleError(OpGeolocation::Error::ErrorCode error);
	OP_STATUS HandleResponse(OpGeolocation::Position &pos);

private:
	OpGeolocationNetworkRequest(OpGeolocationNetworkApi *network_api);
	OP_STATUS Construct(GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data);

	OpGeolocationNetworkListener *m_listener;
	OpGeolocationNetworkApi *m_network_api;
	URL m_request_url;
	MessageHandler *m_mh;
	BOOL m_finished;
};

//////////////////////////////////////////////////////////////////////////

class OpGeolocationNetworkListener
{
public:
	virtual void OnNetworkPositionAvailable(const OpGeolocation::Position &position, BOOL force_update = FALSE) = 0;
	virtual void OnNetworkPositionError(const OpGeolocation::Error &error) = 0;
};

#endif // GEOLOCATION_SUPPORT
#endif // GEOLOCATION_NETWORK_PROVIDER_H
