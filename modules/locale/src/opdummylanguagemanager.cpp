/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2005-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Peter Krefting
*/

#include "core/pch.h"

#ifdef USE_DUMMY_LANGUAGEMANAGER
#include "modules/locale/src/opdummylanguagemanager.h"
#include "modules/locale/locale-dbversion.h"

const OpStringC OpDummyLanguageManager::GetLanguage()
{
	// Not really true, but we must say something, or the UA string
	// will be messed up.
	return UNI_L("en");
}

unsigned int OpDummyLanguageManager::GetDatabaseVersionFromFileL()
{
	return LANGUAGE_DATABASE_VERSION;
}

OP_STATUS OpDummyLanguageManager::GetString(Str::LocaleString num, UniString &s)
{
	s.Clear();
	return OpStatus::OK;
}

#endif // USE_DUMMY_LANGUAGEMANAGER
