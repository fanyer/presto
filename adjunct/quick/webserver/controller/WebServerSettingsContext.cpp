/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Manuela Hutter (manuelah)
 */

#include "core/pch.h"

#ifdef WEBSERVER_SUPPORT

#include "adjunct/quick/webserver/controller/WebServerSettingsContext.h"
#include "modules/locale/locale-enum.h"
#include "modules/locale/oplanguagemanager.h"



/***********************************************************************************
**  WebServerSettingsContext::WebServerSettingsContext
**
************************************************************************************/
WebServerSettingsContext::WebServerSettingsContext()
	: m_device_name()
	, m_device_name_suggestions(5) // create 5 elems at time, if necessary
#ifdef PREFS_CAP_UPNP_ENABLED
	, m_upnp_enabled(FALSE)
#endif // PREFS_CAP_UPNP_ENABLED
#ifdef PREFS_CAP_SERVICE_DISCOVERY_ENABLED
	, m_asd_enabled(FALSE)
#endif // PREFS_CAP_SERVICE_DISCOVERY_ENABLED
#ifdef PREFS_CAP_UPNP_DISCOVERY_ENABLED
	, m_upnp_discovery_enabled(FALSE)
#endif // PREFS_CAP_UPNP_DISCOVERY_ENABLED
#ifdef WEBSERVER_CAP_SET_VISIBILITY
	, m_robots_home_visible(FALSE)
#endif // WEBSERVER_CAP_SET_VISIBILITY
	, m_upload_speed(0)
	, m_port(0)
{
}


/***********************************************************************************
**  WebServerSettingsContext::GetFeatureType
**  @return
************************************************************************************/
FeatureType WebServerSettingsContext::GetFeatureType() const
{
	return FeatureTypeWebserver;
}


/***********************************************************************************
**  WebServerSettingsContext::GetFeatureStringID
**  @return
**
************************************************************************************/
Str::LocaleString WebServerSettingsContext::GetFeatureStringID() const
{
	return Str::D_WEBSERVER_NAME;
}


/***********************************************************************************
**  WebServerSettingsContext::GetFeatureLongStringID
**  @return
**
************************************************************************************/
Str::LocaleString WebServerSettingsContext::GetFeatureLongStringID() const
{
	return Str::D_WEBSERVER_NAME_LONG;
}


/***********************************************************************************
**  WebServerSettingsContext::GetDeviceName
**  @param context
**
************************************************************************************/
const OpStringC & WebServerSettingsContext::GetDeviceName() const
{
	return m_device_name;
}


/***********************************************************************************
**  WebServerSettingsContext::GetDeviceName
**  @return
**
************************************************************************************/
const OpAutoVector<OpString> * WebServerSettingsContext::GetDeviceNameSuggestions() const
{
	return &m_device_name_suggestions;
}

#ifdef PREFS_CAP_UPNP_ENABLED
/***********************************************************************************
**  WebServerSettingsContext::IsUPnPEnabled
**
************************************************************************************/
BOOL
WebServerSettingsContext::IsUPnPEnabled() const
{
	return m_upnp_enabled;
}
#endif // PREFS_CAP_UPNP_ENABLED

/***********************************************************************************
**  WebServerSettingsContext::SetDeviceName
**  @param device_name
**  @return
**
************************************************************************************/
OP_STATUS WebServerSettingsContext::SetDeviceName(const OpStringC& device_name)
{
	return m_device_name.Set(device_name);
}

/***********************************************************************************
**  WebServerSettingsContext::ResetDeviceName
**
************************************************************************************/
void
WebServerSettingsContext::ResetDeviceName()
{
	m_device_name.Empty();
}

/***********************************************************************************
**  WebServerSettingsContext::AddDeviceNameSuggestion
**  @param suggestion
**  @return
**
************************************************************************************/
OP_STATUS WebServerSettingsContext::AddDeviceNameSuggestion(OpString*  suggestion)
{
	return m_device_name_suggestions.Add(suggestion);
}

#ifdef PREFS_CAP_UPNP_ENABLED
/***********************************************************************************
**  WebServerSettingsContext::SetUPnPEnabled
**
************************************************************************************/
void
WebServerSettingsContext::SetUPnPEnabled(BOOL enabled)
{
	m_upnp_enabled = enabled;
}
#endif // PREFS_CAP_UPNP_ENABLED

#endif // WEBSERVER_SUPPORT
