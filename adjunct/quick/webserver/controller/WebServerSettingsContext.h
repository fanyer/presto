/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#ifndef WEBSERVER_SETTINGS_CONTEXT_H
#define WEBSERVER_SETTINGS_CONTEXT_H

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/controller/FeatureSettingsContext.h"

/***********************************************************************************
**  @class	WebServerSettingsContext
**	@brief	Settings that can be specified for webserver.
************************************************************************************/
class WebServerSettingsContext : public FeatureSettingsContext
{
public:
	WebServerSettingsContext();

	// getters
	virtual FeatureType				GetFeatureType() const;
	virtual Str::LocaleString		GetFeatureStringID() const;
	virtual Str::LocaleString		GetFeatureLongStringID() const;

	const OpStringC &				GetDeviceName() const;
	const OpAutoVector<OpString>*	GetDeviceNameSuggestions() const;

#ifdef PREFS_CAP_UPNP_ENABLED
	BOOL	IsUPnPEnabled() const;
#endif // PREFS_CAP_UPNP_ENABLED

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	BOOL	IsASDEnabled() const	{ return m_asd_enabled; }
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
	BOOL	IsUPnPServiceDiscoveryEnabled()	const { return m_upnp_discovery_enabled; }
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	BOOL	IsVisibleRobotsOnHome() const { return m_robots_home_visible; }
#endif // WEBSERVER_CAP_SET_VISIBILITY

	INT32	GetUploadSpeed() const	{ return m_upload_speed; }
	INT32	GetPort() const			{ return m_port; }

	// setters
	OP_STATUS		SetDeviceName(const OpStringC& device_name);
	void			ResetDeviceName();
	OP_STATUS		AddDeviceNameSuggestion(OpString* suggestion);
#ifdef PREFS_CAP_UPNP_ENABLED
	void			SetUPnPEnabled(BOOL enabled);
#endif // PREFS_CAP_UPNP_ENABLED
#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	void			SetASDEnabled(BOOL asd_enabled) { m_asd_enabled = asd_enabled; }
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
	void			SetUPnPServiceDiscoveryEnabled(BOOL upnp_discovery_enabled)	{ m_upnp_discovery_enabled = upnp_discovery_enabled; }
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	void			SetVisibleRobotsOnHome(BOOL robots_home_visible) { m_robots_home_visible = robots_home_visible; }
#endif // WEBSERVER_CAP_SET_VISIBILITY

	void			SetUploadSpeed(INT32 upload_speed) { m_upload_speed = upload_speed; }
	void			SetPort(INT32 port) { m_port = port; }

private:
	WebServerSettingsContext(const WebServerSettingsContext &);
	WebServerSettingsContext & operator=(const WebServerSettingsContext&);

	OpString					m_device_name;
	OpAutoVector<OpString>		m_device_name_suggestions;
#ifdef PREFS_CAP_UPNP_ENABLED
	BOOL	m_upnp_enabled;
#endif // PREFS_CAP_UPNP_ENABLED

#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	BOOL	m_asd_enabled;
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED

#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
	BOOL	m_upnp_discovery_enabled;
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED

#ifdef WEBSERVER_CAP_SET_VISIBILITY
	BOOL	m_robots_home_visible;
#endif // WEBSERVER_CAP_SET_VISIBILITY

	INT32	m_upload_speed;
	INT32	m_port;
};

#endif // WEBSERVER_SUPPORT

#endif // WEBSERVER_SETTINGS_CONTEXT_H
