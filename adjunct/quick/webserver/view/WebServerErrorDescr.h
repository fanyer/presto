/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef WEBSERVERERROR_H
#define	WEBSERVERERROR_H

#ifdef WEBSERVER_SUPPORT

#include "modules/locale/locale-enum.h"
#include "modules/webserver/webserver-api.h"

/**
 * A descriptor structure for web server errors.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class WebServerErrorDescr
{
public:
	enum SuggestedAction
	{
		ActionNone,
		ActionRestart,
		ActionChangeUser
	};

	WebserverStatus		m_ws_status;
	UploadServiceStatus	m_us_status;
	const uni_char*		m_context;
	Str::LocaleString	m_msg_id;
	INT32				m_probable_cause;
	SuggestedAction		m_suggested_action;

	/**
	 * Looks up the web server error descriptor for a given status code.
	 *
	 * @param status the web server status code
	 * @return the web server error descriptor for @a status, or @c NULL if no
	 *		matching descriptor is found
	 */
	static const WebServerErrorDescr* FindByWebServerStatus(WebserverStatus status);
	static const WebServerErrorDescr* FindByUploadServiceStatus(UploadServiceStatus status);

private:
	WebServerErrorDescr(WebserverStatus status, const uni_char* context,
			Str::LocaleString msg_id, INT32 probable_cause, SuggestedAction suggested_action = ActionNone);

	WebServerErrorDescr(UploadServiceStatus status, const uni_char* context,
			Str::LocaleString msg_id, INT32 probable_cause,	SuggestedAction suggested_action = ActionNone);

	static const WebServerErrorDescr s_webserver_errors[];
	static const WebServerErrorDescr s_upload_service_errors[];
};

#endif // WEBSERVER_SUPPORT
#endif // WEBSERVERERROR_H
