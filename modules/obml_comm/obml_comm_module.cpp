/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Yngve Pettersen
**
*/


#include "core/pch.h"

#ifdef WEB_TURBO_MODE

#include "modules/obml_comm/obml_id_manager.h"
#include "modules/obml_comm/obml_config.h"

#if defined(WEB_TURBO_MODE) && defined(URL_FILTER)
# include "modules/content_filter/content_filter.h"
# include "modules/prefs/prefsmanager/prefsmanager.h"
# include "modules/prefs/prefsmanager/collections/pc_network.h"
# include "modules/util/opstrlst.h"
#endif // WEB_TURBO_MODE && URL_FILTER

void ObmlCommModule::InitL(const OperaInitInfo& info)
{
	OP_ASSERT(config);
	id_manager = OBML_Id_Manager::CreateL(config);
	CreateBypassFiltersL();
}

void ObmlCommModule::InitConfigL()
{
	config = OP_NEW_L(OBML_Config, ());
	config->ConstructL();
}

void ObmlCommModule::Destroy()
{
	OP_DELETE(config);
	config = NULL;
	OP_DELETE(id_manager);
	id_manager = NULL;
}

void ObmlCommModule::CreateBypassFiltersL()
{
#if defined(URL_FILTER) && defined(FILTER_BYPASS_RULES) && defined(PREFS_HOSTOVERRIDE) && defined(DEFAULT_WEB_TURBO_MODE)
	OpStackAutoPtr<OpString_list> hosts(g_prefsManager->GetOverriddenHostsL());
	OpString_list *hlptr = hosts.get();
	if( !hlptr )
		return;

	for( unsigned int i=0; i < hlptr->Count(); i++ )
	{
		const OpStringC host(hlptr->Item(i).CStr());
		if( g_pcnet->IsPreferenceOverridden(PrefsCollectionNetwork::UseWebTurbo, host.CStr()) )
		{
			int setting = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::UseWebTurbo, host.CStr());
			if( setting > 1 ) // Expiry time set
			{
				if( (time_t)setting > g_timecache->CurrentTime() ) // Still valid
					setting = 0;
				else
				{
					// Setting value to TRUE means it is not overridden (hack until there is a posibility to remove single overrides)
					g_pcnet->OverridePrefL(host.CStr(), PrefsCollectionNetwork::UseWebTurbo, TRUE, FALSE);
				}
			}

			if( !setting ) // Setting is overridden
			{
				OpString site_url;
				// Add server to filter with wildcard
				site_url.ReserveL(host.Length()+10);
				site_url.Set(UNI_L("http://"));
				site_url.Append(host);
#ifdef _DEBUG
				BOOL found = FALSE;
				g_urlfilter->CheckBypassURL(site_url.CStr(),found);
				OP_ASSERT(!found);
#endif // _DEBUG
				site_url.Append(UNI_L("/*"));
				g_urlfilter->AddFilterL(site_url.CStr());
			}
		}
	}
#endif // URL_FILTER && FILTER_BYPASS_RULES && PREFS_HOSTOVERRIDE && DEFAULT_WEB_TURBO_MODE
}

#endif // WEB_TURBO_MODE
