/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/view/WebServerErrorDescr.h"

#include "adjunct/quick/managers/OperaAccountManager.h"


const WebServerErrorDescr WebServerErrorDescr::s_webserver_errors[] = {

	WebServerErrorDescr(
			WEBSERVER_ERROR,
			UNI_L("WEBSERVER_ERROR"),
			Str::S_WEBSERVER_ERROR,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			WEBSERVER_ERROR_MEMORY,
			UNI_L("WEBSERVER_ERROR_MEMORY"),
			Str::S_WEBSERVER_ERROR_MEMORY,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN,
			UNI_L("WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN"),
			Str::S_WEBSERVER_ERROR_LISTENING_SOCKET_TAKEN,
			OAM_ERROR_TYPE_OTHER,
			ActionRestart),

	WebServerErrorDescr(
			WEBSERVER_ERROR_COULD_NOT_OPEN_PORT,
			UNI_L("WEBSERVER_ERROR_COULD_NOT_OPEN_PORT"),
			Str::S_WEBSERVER_ERROR_COULD_NOT_OPEN_PORT,
			OAM_ERROR_TYPE_OTHER,
			ActionRestart),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION,
			UNI_L("PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_UNSECURE_SERVER_VERSION,
			OAM_ERROR_TYPE_OTHER),

#ifdef WEBSERVER_RENDEZVOUS_SUPPORT
	WebServerErrorDescr(
			PROXY_CONNECTION_LOGGED_OUT,
			UNI_L("PROXY_CONNECTION_LOGGED_OUT"),
			Str::S_WEBSERVER_PROXY_CONNECTION_LOGGED_OUT,
			OAM_ERROR_TYPE_AUTH,
			ActionRestart),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_PROXY_DOWN,
			UNI_L("PROXY_CONNECTION_ERROR_PROXY_DOWN"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_PROXY_DOWN,
			OAM_ERROR_TYPE_AUTH,
			ActionRestart),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_DENIED,
			UNI_L("PROXY_CONNECTION_ERROR_DENIED"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_DENIED,
			OAM_ERROR_TYPE_AUTH),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE,
			UNI_L("PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_PROXY_NOT_REACHABLE,
			OAM_ERROR_TYPE_AUTH,
			ActionRestart),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_NETWORK,
			UNI_L("PROXY_CONNECTION_ERROR_NETWORK"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_NETWORK,
			OAM_ERROR_TYPE_NETWORK,
			ActionRestart),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN,
			UNI_L("PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_PROXY_ADDRESS_UNKOWN,
			OAM_ERROR_TYPE_OTHER,
			ActionRestart),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_AUTH_FAILURE,
			UNI_L("PROXY_CONNECTION_ERROR_AUTH_FAILURE"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_AUTH_FAILURE,
			OAM_ERROR_TYPE_AUTH,
			ActionChangeUser),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED,
			UNI_L("PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_DEVICE_ALREADY_REGISTERED,
			OAM_ERROR_TYPE_OTHER,
			ActionRestart),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION,
			UNI_L("PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_PROTOCOL_WRONG_VERSION,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			PROXY_CONNECTION_ERROR_PROTOCOL,
			UNI_L("PROXY_CONNECTION_ERROR_PROTOCOL"),
			Str::S_WEBSERVER_PROXY_CONNECTION_ERROR_PROTOCOL,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			PROXY_CONNECTION_DEVICE_NOT_CONFIGURED,
			UNI_L("PROXY_CONNECTION_DEVICE_NOT_CONFIGURED"),
			Str::S_WEBSERVER_PROXY_CONNECTION_DEVICE_NOT_CONFIGURED,
			OAM_ERROR_TYPE_OTHER),
#endif // WEBSERVER_RENDEZVOUS_SUPPORT
};

const WebServerErrorDescr WebServerErrorDescr::s_upload_service_errors[] = {
	WebServerErrorDescr(
			UPLOAD_SERVICE_WARNING_OLD_PROTOCOL,
			UNI_L("UPLOAD_SERVICE_WARNING_OLD_PROTOCOL"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_WARNING_OLD_PROTOCOL,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			UPLOAD_SERVICE_ERROR,
			UNI_L("UPLOAD_SERVICE_ERROR"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_ERROR,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			UPLOAD_SERVICE_ERROR_PARSER,
			UNI_L("UPLOAD_SERVICE_ERROR_PARSER"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_ERROR_PARSER,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			UPLOAD_SERVICE_ERROR_MEMORY,
			UNI_L("UPLOAD_SERVICE_ERROR_MEMORY"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_ERROR_MEMORY,
			OAM_ERROR_TYPE_OTHER),

	WebServerErrorDescr(
			UPLOAD_SERVICE_ERROR_COMM_FAIL,
			UNI_L("UPLOAD_SERVICE_ERROR_COMM_FAIL"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_ERROR_COMM_FAIL,
			OAM_ERROR_TYPE_NETWORK),

	WebServerErrorDescr(
			UPLOAD_SERVICE_ERROR_COMM_ABORTED,
			UNI_L("UPLOAD_SERVICE_ERROR_COMM_ABORTED"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_ERROR_COMM_ABORTED,
			OAM_ERROR_TYPE_NETWORK),

	WebServerErrorDescr(
			UPLOAD_SERVICE_ERROR_COMM_TIMEOUT,
			UNI_L("UPLOAD_SERVICE_ERROR_COMM_TIMEOUT"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_ERROR_COMM_TIMEOUT,
			OAM_ERROR_TYPE_NETWORK),

	WebServerErrorDescr(
			UPLOAD_SERVICE_AUTH_FAILURE,
			UNI_L("UPLOAD_SERVICE_AUTH_FAILURE"),
			Str::S_WEBSERVER_UPLOAD_SERVICE_AUTH_FAILURE,
			OAM_ERROR_TYPE_AUTH),
};

WebServerErrorDescr::WebServerErrorDescr(WebserverStatus status,
		const uni_char* context, Str::LocaleString msg_id, INT32 probable_cause, SuggestedAction suggested_action)
	: m_ws_status(status),
	  m_us_status(UPLOAD_SERVICE_OK),
	  m_context(context),
	  m_msg_id(msg_id),
	  m_probable_cause(probable_cause),
	  m_suggested_action(suggested_action)
{
}

WebServerErrorDescr::WebServerErrorDescr(UploadServiceStatus status,
		const uni_char* context, Str::LocaleString msg_id, INT32 probable_cause, SuggestedAction suggested_action)
	: m_ws_status(WEBSERVER_OK),
	  m_us_status(status),
	  m_context(context),
	  m_msg_id(msg_id),
	  m_probable_cause(probable_cause),
	  m_suggested_action(suggested_action)
{
}

const WebServerErrorDescr* WebServerErrorDescr::FindByWebServerStatus(
		WebserverStatus status)
{
	const WebServerErrorDescr* error = NULL;

	for (size_t i = 0; i < ARRAY_SIZE(s_webserver_errors); ++i)
	{
		if (status == s_webserver_errors[i].m_ws_status)
		{
			error = &s_webserver_errors[i];
			break;
		}
	}

	return error;
}

const WebServerErrorDescr* WebServerErrorDescr::FindByUploadServiceStatus(
	UploadServiceStatus status)	
{
	const WebServerErrorDescr* error = NULL;

	for (size_t i = 0; i < ARRAY_SIZE(s_upload_service_errors); ++i)
	{
		if (status == s_upload_service_errors[i].m_us_status)
		{
			error = &s_upload_service_errors[i];
			break;
		}
	}

	return error;
}
#endif // WEBSERVER_SUPPORT
