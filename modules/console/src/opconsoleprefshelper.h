/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined OPCONSOLEPREFSHELPER_H && defined OPERA_CONSOLE_LOGFILE
#define OPCONSOLEPREFSHELPER_H

#include "modules/prefs/prefsmanager/opprefslistener.h"
#include "modules/prefs/prefsmanager/collections/pc_files.h"
#include "modules/hardcore/mh/messobj.h"

/**
 * Preference helper for the console logger. This class takes care of all
 * log files supported by the console module, and reacts to changes in
 * the settings as needed.
 */
class OpConsolePrefsHelper :
	private OpPrefsListener, private PrefsCollectionFilesListener,
	private MessageObject
{
public:
	/** First-phase constructor. */
	OpConsolePrefsHelper()
		: m_pending_reconfigure(TRUE),
		  m_logger(NULL)
	{}

	/** Second-phase constructor. */
	void ConstructL();

	/** Destructor. Will deregister properly. */
	virtual ~OpConsolePrefsHelper();

	// Inherited interfaces
	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, int newvalue);
	virtual void PrefChanged(enum OpPrefsCollection::Collections id, int pref, const OpStringC &newvalue);
	virtual void FileChangedL(PrefsCollectionFiles::filepref pref, const OpFile *newvalue);
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

private:
	/** Set up console log file. */
	void SetupLoggerL();

	/** Signal reconfiguration. */
	void SignalReconfigure();

	/** Flag for pending reconfigure. */
	BOOL m_pending_reconfigure;

	/** The console log file. */
	class OpConsoleLogger *m_logger;
};

#endif
