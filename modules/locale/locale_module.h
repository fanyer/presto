/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2006 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef LOCALE_MODULE_H
#define LOCALE_MODULE_H

#include "modules/hardcore/opera/module.h"

/** LocaleModule must be created on startup */
#define LOCALE_MODULE_REQUIRED

#if defined LANGUAGE_FILE_SUPPORT || defined LOCALE_BINARY_LANGUAGE_FILE || defined USE_DUMMY_LANGUAGEMANAGER
/** LocaleModule is self-contained with regard to setup. */
# define LOCALE_SELFCONTAINED
#endif

class LocaleModule : public OperaModule
{
public:
	LocaleModule() : m_language_manager(NULL) {}

	// Inherited interfaces
	virtual void InitL(const OperaInitInfo &);
	virtual void Destroy();

	// Setter methods
#if !defined LOCALE_SELFCONTAINED || defined DOXYGEN_DOCUMENTATION
	/**
	 * For unknown OpLanguageManager implementation, the platform will need
	 * to tell us which object to use. The manager object is taken over by
	 * LocaleModule, and will be deallocated by Destroy().
	 *
	 * @param manager OpLanguageManager implementation to use.
	 */
	inline void SetLanguageManager(class OpLanguageManager *manager)
	{ m_language_manager = manager; }
#endif

	// Access methods
	inline class OpLanguageManager *OpLanguageManager()
	{ return m_language_manager; }

private:
	class OpLanguageManager *m_language_manager;
};

#endif // !LOCALE_MODULE_H
