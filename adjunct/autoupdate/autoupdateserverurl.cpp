/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Michal Zajaczkowski
 */

#include "core/pch.h"

#ifdef AUTO_UPDATE_SUPPORT
#include "adjunct/autoupdate/autoupdateserverurl.h"
#include "modules/prefs/prefsmanager/collections/pc_ui.h"

AutoUpdateServerURL::AutoUpdateServerURL():
	m_current_url_no(-1)
{
}

AutoUpdateServerURL::~AutoUpdateServerURL()
{
	g_pcui->UnregisterListener(this);

	m_url_list.DeleteAll();
}

OP_STATUS AutoUpdateServerURL::Init()
{
	TRAP_AND_RETURN(err, g_pcui->RegisterListenerL(this));

	return ReadServers();
}

OP_STATUS AutoUpdateServerURL::ReadServers()
{
	OpStringC autoupdate_server_pref = g_pcui->GetStringPref(PrefsCollectionUI::AutoUpdateServer);
	OpString autoupdate_server;

	// Copy and strip any whitespace
	RETURN_IF_ERROR(autoupdate_server.Set(autoupdate_server_pref.CStr()));
	autoupdate_server.Strip(); 

	// Clear out the list
	m_url_list.Clear();
	
	// Add items by parsing the preference
	uni_char *p = autoupdate_server.CStr();
	uni_char *start = p;

	while (1)
	{
		// Look for the end marker
		if (*p == '\0' || *p == ' ')
		{
			OpString *server_url = OP_NEW(OpString, ());
			RETURN_OOM_IF_NULL(server_url);

			OpAutoPtr<OpString> server_url_ap(server_url);
			RETURN_IF_ERROR(server_url->Set(start, p - start));

			// Add to the internal list
			RETURN_IF_ERROR(m_url_list.Add(server_url));
			server_url_ap.release();
		
			// End of the string
		    if (*p == '\0')
				break;
				
			// Move up the start
			start = p + 1;
		}
		p++;
	}

	if (m_url_list.GetCount() <= 0)
		return OpStatus::ERR;

	m_current_url_no = 0;
	return OpStatus::OK;
}

OP_STATUS AutoUpdateServerURL::GetCurrentURL(OpString& url)
{
	if (m_url_list.GetCount() <= 0)
		return OpStatus::ERR;

	if (m_current_url_no < 0)
		return OpStatus::ERR;

	OP_ASSERT(m_current_url_no < (int)m_url_list.GetCount());
	return url.Set(m_url_list.Get(m_current_url_no)->CStr());
}

UINT32 AutoUpdateServerURL::GetURLCount()
{
	return m_url_list.GetCount();
}

OP_STATUS AutoUpdateServerURL::IncrementURLNo(WrapType wrap)
{
	if (m_url_list.GetCount() <= 0)
		return OpStatus::ERR;

	m_current_url_no++;
	if (m_current_url_no >= (int)m_url_list.GetCount())
		if (wrap == NoWrap)
			return OpStatus::ERR;
		else
			m_current_url_no = 0;

	return OpStatus::OK;
}

OP_STATUS AutoUpdateServerURL::SetURLNo(int url_number)
{
	if (m_url_list.GetCount() <= 0)
		return OpStatus::ERR;

	if (url_number >= (int)m_url_list.GetCount())
		return OpStatus::ERR;

	if (url_number < 0)
		return OpStatus::ERR;

	m_current_url_no = url_number;
	return OpStatus::OK;
}

OP_STATUS AutoUpdateServerURL::ResetURLNo()
{
	if (m_url_list.GetCount() <= 0)
		return OpStatus::ERR;

	m_current_url_no = 0;
	return OpStatus::OK;
}

void AutoUpdateServerURL::PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC & newvalue)
{
	if(OpPrefsCollection::UI != id)
		return;

	switch(PrefsCollectionUI::integerpref(pref))
	{
		case (PrefsCollectionUI::AutoUpdateServer):
			ReadServers();
			break;
		default:
			break;
	}
}

#endif // AUTO_UPDATE_SUPPORT
