/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef _SSL_UPDATERS_H_
#define _SSL_UPDATERS_H_

#if defined(_NATIVE_SSL_SUPPORT_) && defined(LIBSSL_AUTO_UPDATE)
#include "modules/updaters/update_man.h"

#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
# include "modules/hardcore/base/periodic_task.h"
# include "modules/prefs/prefsmanager/opprefslistener.h"
#endif // LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY

typedef class AutoFetch_Element SSL_AutoUpdaterElement;

class SSL_AutoUpdaters :
	public AutoFetch_Manager
#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
	, public OpPrefsListener
#endif
{

#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
private:
	class PeriodicCheckUpdates: public PeriodicTask
	{
	public:
		virtual void Run() { g_main_message_handler->PostMessage(MSG_SSL_START_AUTO_UPDATE, 0, 0); }
	} m_checkUpdates;
#endif // LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY

public:

	SSL_AutoUpdaters()
		: AutoFetch_Manager(MSG_SSL_FINISHED_AUTOUPDATE_BATCH)
		{};
	virtual ~SSL_AutoUpdaters();

	void InitL();

	virtual void	HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

#ifdef LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
	virtual void	PrefChanged(OpPrefsCollection::Collections id , int pref, int newvalue);
#endif // LIBSSL_AUTO_UPDATE_ROOTS_PERIODICALLY
};

#endif

#endif //_SSL_UPDATERS_H_
