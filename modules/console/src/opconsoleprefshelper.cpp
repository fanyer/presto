/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef OPERA_CONSOLE_LOGFILE

#include "modules/console/opconsolelogger.h"
#include "modules/console/opconsolefilter.h"
#include "modules/console/src/opconsoleprefshelper.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/prefs/prefsmanager/collections/pc_core.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/hardcore/mem/mem_man.h"

void OpConsolePrefsHelper::ConstructL()
{
	// Register as listener to relevant preferences
	g_pccore->RegisterListenerL(this);
	g_pcfiles->RegisterFilesListenerL(this);

	// Register with the message handler
	MessageHandler *mmh = g_main_message_handler;
	mmh->SetCallBack(this, MSG_CONSOLE_RECONFIGURE, 0);

	// Set up the logger later
	mmh->PostMessage(MSG_CONSOLE_RECONFIGURE, 0, 0);
}

OpConsolePrefsHelper::~OpConsolePrefsHelper()
{
	// Unregister everything
	g_pccore->UnregisterListener(this);
	g_pcfiles->UnregisterFilesListener(this);
	g_main_message_handler->UnsetCallBacks(this);

	// Kill the logger
	OP_DELETE(m_logger);
}

void OpConsolePrefsHelper::SignalReconfigure()
{
	if (!m_pending_reconfigure)
	{
		// Remind ourselves that we need to reconfigure. Since we are likely
		// to get both settings changed at once, it's not necessary to do
		// the reconfigure call twice in a row.
		g_main_message_handler->PostMessage(MSG_CONSOLE_RECONFIGURE, 0, 0);
		m_pending_reconfigure = TRUE;
	}
}

void OpConsolePrefsHelper::PrefChanged(enum OpPrefsCollection::Collections id, int pref, int)
{
	if (id == OpPrefsCollection::Core &&
		static_cast<PrefsCollectionCore::integerpref>(pref) == PrefsCollectionCore::UseErrorLog)
	{
		SignalReconfigure();
	}
}

void OpConsolePrefsHelper::PrefChanged(enum OpPrefsCollection::Collections id, int pref, const OpStringC &)
{
	if (id == OpPrefsCollection::Core &&
		static_cast<PrefsCollectionCore::stringpref>(pref) == PrefsCollectionCore::ErrorLogFilter)
	{
		SignalReconfigure();
	}
}

void OpConsolePrefsHelper::FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *)
{
	if (pref == PrefsCollectionFiles::ConsoleErrorLogName)
	{
		SignalReconfigure();
	}
}

void OpConsolePrefsHelper::HandleCallback(OpMessage msg, MH_PARAM_1 obj, MH_PARAM_2 data)
{
	if (MSG_CONSOLE_RECONFIGURE == msg)
	{
		TRAPD(rc, SetupLoggerL());
		if (OpStatus::IsError(rc))
		{
			// Report the problem and try again in a while
			if (OpStatus::IsRaisable(rc))
				g_memory_manager->RaiseCondition(rc);
#if CON_OOM_RETRY_DELAY > 0
			g_main_message_handler->PostDelayedMessage(msg, obj, data, CON_OOM_RETRY_DELAY);
#else
			// Configured not to try again, so just wait for the next time
			// we are reconfigured.
			m_pending_reconfigure = FALSE;
#endif
		}
	}
	else
	{
		OP_ASSERT(!"Should not have seen that message.");
		return;
	}
}

void OpConsolePrefsHelper::SetupLoggerL()
{
	PrefsCollectionCore *pccore = g_pccore;
	if (pccore->GetIntegerPref(PrefsCollectionCore::UseErrorLog))
	{
		const OpFile *logfile =
			g_pcfiles->GetFile(PrefsCollectionFiles::ConsoleErrorLogName);

		// Parse the filter string
		OpStackAutoPtr<OpConsoleFilter>
			new_filter(OP_NEW_L(OpConsoleFilter, (OpConsoleEngine::DoNotShow)));
#ifdef CON_ENABLE_SERIALIZATION
		LEAVE_IF_ERROR(new_filter->SetFilter(
			pccore->GetStringPref(PrefsCollectionCore::ErrorLogFilter),
			OpConsoleEngine::Information));
#else
		// Just enable for JavaScript if we lack serialization support.
		LEAVE_IF_ERROR(new_filter->SetOrReplace(OpConsoleEngine::EcmaScript,
			                                    OpConsoleEngine::Information));
#endif

		// Set up new logger
		OpStackAutoPtr<OpConsoleLogger> new_logger(OP_NEW_L(OpConsoleLogger, ()));
		new_logger->ConstructL(logfile, new_filter.get());

		// Replace old logger with the new one
		OP_DELETE(m_logger);
		m_logger = new_logger.release();
	}
	else
	{
		// Disable logging
		OP_DELETE(m_logger);
		m_logger = NULL;
	}

	// We're done. Clear the pending flag so that further reconfigurations
	// will trigger. If we didn't make it, our caller will make sure we
	// are called again in a while, when the OOM situation hopefully has
	// been resolved.
	m_pending_reconfigure = FALSE;
}

#endif // OPERA_CONSOLE_LOGFILE
