/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Karlsson
*/

#if !defined PREFSLOADER_MODULE_H && defined PREFS_DOWNLOAD
#define PREFSLOADER_MODULE_H

/** We have global objects. */
#define PREFSLOADER_MODULE_REQUIRED

#include "modules/hardcore/opera/module.h"

class PrefsloaderModule : public OperaModule
{
public:
	PrefsloaderModule();

	// Inherited interfaces
	virtual void InitL(const OperaInitInfo &);
	virtual void Destroy();

	inline class PrefsLoadManager *PrefsLoadManager()
	{ return m_prefs_load_manager; }

private:
	class PrefsLoadManager *m_prefs_load_manager;
};

#endif
