/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef GEOLOCATION_SUPPORT

#include "modules/geolocation/src/geo_network_request.h"
#include "modules/geolocation/geolocation.h"
#include "modules/geolocation/src/geo_tools.h"

#include "modules/pi/OpGeolocation.h"
#include "modules/pi/OpSystemInfo.h"

// The responses are usually several hundred characters long.
#define RESPONSE_CHARS_LIMIT 32768

/* static */ OP_STATUS
OpGeolocationNetworkRequest::Create(OpGeolocationNetworkRequest *&new_obj, OpGeolocationNetworkApi *network_api, GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data)
{
	OpGeolocationNetworkRequest *obj = OP_NEW(OpGeolocationNetworkRequest, (network_api));
	if (!obj)
		return OpStatus::ERR_NO_MEMORY;

	if (OpStatus::IsError(obj->Construct(wifi_data, radio_data, gps_data)))
	{
		OP_DELETE(obj);
		return OpStatus::ERR_NO_MEMORY;
	}

	new_obj = obj;
	return OpStatus::OK;
}

OP_STATUS
OpGeolocationNetworkRequest::Construct(GeoWifiData *wifi_data, GeoRadioData *radio_data, GeoGpsData *gps_data)
{
	m_mh = g_main_message_handler;

	URL request_url;
	RETURN_IF_ERROR(m_network_api->MakeRequestUrl(request_url, wifi_data, radio_data, gps_data));
	m_request_url = request_url;
	CommState state = m_request_url.Load(m_mh, URL(), TRUE, TRUE, FALSE, TRUE);
	if (state != COMM_LOADING)
		return OpStatus::ERR;

	RETURN_IF_ERROR(SetCallbacks());

	return OpStatus::OK;
}

OpGeolocationNetworkRequest::OpGeolocationNetworkRequest(OpGeolocationNetworkApi *network_api)
: m_listener(NULL)
, m_network_api(network_api)
, m_mh(NULL)
, m_finished(FALSE)
{
}

OpGeolocationNetworkRequest::~OpGeolocationNetworkRequest()
{
	Out();
	UnsetCallbacks();
}

void
OpGeolocationNetworkRequest::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch (msg)
	{
	case MSG_URL_MOVED:
		{
			UnsetCallbacks();
			m_request_url = m_request_url.GetAttribute(URL::KMovedToURL); // TODO? Update m_url_str with new target?
			SetCallbacks();
			return;
		}
	case MSG_URL_LOADING_FAILED:
		{
			HandleError(OpGeolocation::Error::PROVIDER_ERROR);
			OP_DELETE(this);
			return;
		}
	case MSG_URL_DATA_LOADED:
		{
			if (m_request_url.Status(TRUE) == URL_LOADED)
			{
				HandleFinished();
				OP_DELETE(this);
			}
			return;
		}
	}

	OP_ASSERT(!"You shouldn't be here, are we sending messages to ourselves that we do not handle?");
	HandleError(OpGeolocation::Error::PROVIDER_ERROR);
}

OP_STATUS
OpGeolocationNetworkRequest::SetCallbacks()
{
	URL_ID id = m_request_url.Id();

	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_DATA_LOADED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_LOADING_FAILED, id));
	RETURN_IF_ERROR(m_mh->SetCallBack(this, MSG_URL_MOVED, id));

	return OpStatus::OK;
}

void
OpGeolocationNetworkRequest::UnsetCallbacks()
{
	if (m_mh)
		m_mh->UnsetCallBacks(this);
}

void
OpGeolocationNetworkRequest::HandleFinished()
{
	m_finished = TRUE;
	UnsetCallbacks();

	int response = m_request_url.GetAttribute(URL::KHTTP_Response_Code);
	if (response == HTTP_OK || m_request_url.Type() == URL_DATA)
	{
		OpGeolocation::Position position;
		op_memset(&position, 0, sizeof(position));
		position.type = OpGeolocation::WIFI|OpGeolocation::RADIO;

		OP_STATUS err = HandleResponse(position);
		if (m_listener)
		{
			if (OpStatus::IsSuccess(err))
				m_listener->OnNetworkPositionAvailable(position);
			else
				m_listener->OnNetworkPositionError(OpGeolocation::Error(OpGeolocation::Error::POSITION_NOT_FOUND, OpGeolocation::WIFI|OpGeolocation::RADIO));
		}
	}
	else
	{
#ifdef OPERA_CONSOLE
		GeoTools::PostConsoleMessage(OpConsoleEngine::Error, UNI_L("Location provider responded with error code."), GeoTools::CustomProviderNote());
#endif // OPERA_CONSOLE

		if (m_listener)
			m_listener->OnNetworkPositionError(OpGeolocation::Error(OpGeolocation::Error::PROVIDER_ERROR, OpGeolocation::WIFI|OpGeolocation::RADIO));
	}
}

void
OpGeolocationNetworkRequest::HandleError(OpGeolocation::Error::ErrorCode error)
{
	m_finished = TRUE;
	UnsetCallbacks();

	if (m_listener)
		m_listener->OnNetworkPositionError(OpGeolocation::Error(error, OpGeolocation::WIFI|OpGeolocation::RADIO));
}

OP_STATUS
OpGeolocationNetworkRequest::HandleResponse(OpGeolocation::Position &pos)
{
	OpAutoPtr<URL_DataDescriptor> url_dd(m_request_url.GetDescriptor(m_mh, TRUE, FALSE, TRUE, NULL, URL_TEXT_CONTENT));
	if (!url_dd.get())
		return OpStatus::ERR;

	BOOL more;
	unsigned long bytes_read = url_dd->RetrieveData(more);
	unsigned int chars_read = bytes_read / 2;
	unsigned int total_chars_read = 0;

	uni_char *buffer;
	TempBuffer temp_buffer;
	if (more)
	{
		do
		{
			temp_buffer.Append((uni_char*) url_dd->GetBuffer(), chars_read);
			total_chars_read += chars_read;
			url_dd->ConsumeData(chars_read * 2);

			bytes_read = url_dd->RetrieveData(more);
			chars_read = bytes_read / 2;
		} while(chars_read > 0 && total_chars_read < RESPONSE_CHARS_LIMIT);
		buffer = temp_buffer.GetStorage();
	}
	else
	{
		buffer = (uni_char*) url_dd->GetBuffer();
		total_chars_read = chars_read;
	}

	return m_network_api->ProcessResponse(buffer, total_chars_read, pos);
}

#endif // GEOLOCATION_SUPPORT
