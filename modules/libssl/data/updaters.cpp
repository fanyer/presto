/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#if defined(_NATIVE_SSL_SUPPORT_)
#if defined LIBSSL_AUTO_UPDATE

#include "modules/libssl/sslbase.h" 
#include "modules/libssl/data/updaters.h" 
#include "modules/libssl/data/ev_oid_update.h" 
#include "modules/libssl/data/root_auto_update.h" 

#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
# include "modules/hardcore/base/periodic_task.h"
# include "modules/prefs/prefsmanager/collections/pc_network.h"
#endif // LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY

void SSL_AutoUpdaters::InitL()
{
#if defined _DEBUG && defined YNP_WORK
	g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0);
#endif
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SSL_START_AUTO_UPDATE, 0));
	LEAVE_IF_ERROR(g_main_message_handler->SetCallBack(this, MSG_SSL_FINISHED_AUTO_UPDATE_ACTION, 0));

	AutoFetch_Manager::InitL();

#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
	g_pcnet->RegisterListenerL(this);

	int time_of_last_check = g_pcnet->GetIntegerPref(PrefsCollectionNetwork::TimeOfLastCertUpdateCheck);
	time_t curr_time = g_timecache->CurrentTime();

	if (time_of_last_check + LIBSSL_AUTO_UPDATE_ROOTS_INTERVAL < curr_time)
	{
		m_checkUpdates.Run();
	}
	else
	{
		int interval = LIBSSL_AUTO_UPDATE_ROOTS_INTERVAL - (int)(curr_time - time_of_last_check);
		LEAVE_IF_ERROR(g_periodic_task_manager->RegisterTask(&m_checkUpdates, interval*1000));
	}
#endif // LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
}

SSL_AutoUpdaters::~SSL_AutoUpdaters()
{
#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
	g_periodic_task_manager->UnregisterTask(&m_checkUpdates);
#endif
}

#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
void SSL_AutoUpdaters::PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue)
{
	// If setting is reset (by user) trigger new update request
	if (id == OpPrefsCollection::Network &&
		pref == PrefsCollectionNetwork::TimeOfLastCertUpdateCheck &&
		newvalue == 0)
	{
		m_checkUpdates.Run();
	}
}
#endif // LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY

void SSL_AutoUpdaters::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	switch(msg)
	{
	case MSG_SSL_START_AUTO_UPDATE:
		if(Active())
			break;
#ifdef LIBSSL_AUTO_UPDATE_ROOTS
		{
			SSL_Auto_Root_Updater *auto_updater = OP_NEW(SSL_Auto_Root_Updater, ());
			
			if(auto_updater)
			{
				auto_updater->Construct(MSG_SSL_FINISHED_AUTO_UPDATE_ACTION);
				AddUpdater(auto_updater);
			}
		}
#endif
#if defined SSL_CHECK_EXT_VALIDATION_POLICY
		{
			SSL_EV_OID_updater *ev_updater = OP_NEW(SSL_EV_OID_updater, ());
			
			if(ev_updater)
			{
				ev_updater->Construct(MSG_SSL_FINISHED_AUTO_UPDATE_ACTION);
				AddUpdater(ev_updater);
			}
		}
#endif
#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
		// Failing following operations is not fatal. At least one update has been attempted during this browser session.
		g_periodic_task_manager->UnregisterTask(&m_checkUpdates);	// In case previous period was shorter than LIBSSL_AUTO_UPDATE_ROOTS_INTERVAL
		g_periodic_task_manager->RegisterTask(&m_checkUpdates, LIBSSL_AUTO_UPDATE_ROOTS_INTERVAL*1000);

		TRAPD(err, g_pcnet->WriteIntegerL(PrefsCollectionNetwork::TimeOfLastCertUpdateCheck, (int)g_timecache->CurrentTime()));
#endif
		g_ssl_tried_auto_updaters = TRUE;
		break;
	default:
		AutoFetch_Manager::HandleCallback(msg,par1,par2);
	};
}

#endif
#endif
