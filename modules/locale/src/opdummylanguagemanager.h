/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#if defined USE_DUMMY_LANGUAGEMANAGER && !defined OPDUMMYLANGUAGEMANAGER_H
#define OPDUMMYLANGUAGEMANAGER_H

#include "modules/locale/oplanguagemanager.h"

/**
 * Dummy implementation of the OpLanguageManager interface.
 *
 * This implementation will return empty strings for all requested
 * strings, and is meant to be used to get new ports up and running
 * as quickly as possible and for versions of Opera where we do not
 * actually want language strings.
 *
 * @author Peter Krefting
 */
class OpDummyLanguageManager : public OpLanguageManager
{
public:
	// Inherited interfaces; see OpLanguageManager for descriptions
	virtual const OpStringC GetLanguage();
	virtual unsigned int GetDatabaseVersionFromFileL();
	virtual OP_STATUS GetString(Str::LocaleString num, UniString &s);
	virtual WritingDirection GetWritingDirection() { return LTR; }
};

#endif // USE_DUMMY_LANGUAGEMANAGER && !OPDUMMYLANGUAGEMANAGER_H
