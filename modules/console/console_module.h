/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined CONSOLE_MODULE_H && defined OPERA_CONSOLE
#define CONSOLE_MODULE_H

#include "modules/hardcore/opera/module.h"

/** ConsoleModule must be created on startup */
#define CONSOLE_MODULE_REQUIRED

#if defined OPERA_CONSOLE_LOGFILE && defined PREFS_READ && defined PREFS_WRITE
/** Enable console serialization */
# define CON_ENABLE_SERIALIZATION
#endif

/** Meta-class for the console module. */
class ConsoleModule : public OperaModule
{
public:
	// Construction
	ConsoleModule()
		: m_engine(NULL)
#ifdef OPERA_CONSOLE_LOGFILE
		, m_prefshelper(NULL)
#endif
	{}

	// Inherited interfaces
	virtual void InitL(const OperaInitInfo &);
	virtual void Destroy();
	virtual BOOL FreeCachedData(BOOL);

	// Access methods
	inline class OpConsoleEngine *OpConsoleEngine()
	{ return m_engine; }

private:
	class OpConsoleEngine *m_engine;

#ifdef OPERA_CONSOLE_LOGFILE
	class OpConsolePrefsHelper *m_prefshelper;
#endif
};

#endif
