/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#ifndef PREFSFILE_MODULE_H
#define PREFSFILE_MODULE_H

#include "modules/hardcore/opera/module.h"

#if defined PREFSMAP_USE_HASH || !defined PREFSMAP_USE_CASE_SENSITIVE
/** PrefsfileModule must be created on startup */
# define PREFSFILE_MODULE_REQUIRED
#endif

/** Size of PrefsfileModule::m_temp_buffer */
#define PREFSFILE_MODULE_TMPBUF_LEN 2048

#ifdef PREFSFILE_MODULE_REQUIRED
#include "modules/prefsfile/impl/backend/prefshashfunctions.h"

class PrefsfileModule : public OperaModule
{
public:
	PrefsfileModule() {}

	// Inherited interfaces
	virtual void InitL(const OperaInitInfo &);
	virtual void Destroy();

	// Specials
#ifdef PREFSMAP_USE_HASH
	PrefsHashFunctions *GetHashFunctions() { return &m_prefs_hash_functions; }
#endif

	// Friend declare to access the variables
	friend class PrefsFile;
	friend class PrefsSection;
	friend class PrefsMap;

private:
#ifdef PREFSMAP_USE_HASH
	PrefsHashFunctions m_prefs_hash_functions;
#endif
#ifndef PREFSMAP_USE_CASE_SENSITIVE
	// Global variable m_temp_buffer is used for performance gains
	// because heap allocation is slow and the needed buffer is too
	// large for the stack. Using TempBufUni was not possible since we
	// can't guarantee that it's not already used at the relevant locations.
	uni_char m_temp_buffer[PREFSFILE_MODULE_TMPBUF_LEN]; /* ARRAY OK 2011-02-04 peter */
#endif
};
#endif // PREFSFILE_MODULE_REQUIRED

#endif
