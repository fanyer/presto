/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

#include "modules/database/prefscollectiondatabase.h"
#include "modules/prefs/prefsmanager/collections/prefs_macros.h"
#include "modules/database/sec_policy.h" // for default values

#include "modules/database/src/prefscollectiondatabase_c.inl"

PrefsCollectionDatabase *PrefsCollectionDatabase::CreateL(PrefsFile *reader)
{
	if (g_opera->prefs_module.m_pcdatabase)
		LEAVE(OpStatus::ERR);
	g_opera->prefs_module.m_pcdatabase = OP_NEW_L(PrefsCollectionDatabase, (reader));
	return g_opera->prefs_module.m_pcdatabase;
}

PrefsCollectionDatabase::~PrefsCollectionDatabase()
{
#ifdef PREFS_COVERAGE
	CoverageReport(
		m_stringprefdefault, PCDATABASE_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCDATABASE_NUMBEROFINTEGERPREFS);
#endif // PREFS_COVERAGE

	g_opera->prefs_module.m_pcdatabase = NULL;
}

void PrefsCollectionDatabase::ReadAllPrefsL(PrefsModule::PrefsInitInfo *)
{
	// Read everything
	OpPrefsCollection::ReadAllPrefsL(
		m_stringprefdefault, PCDATABASE_NUMBEROFSTRINGPREFS,
		m_integerprefdefault, PCDATABASE_NUMBEROFINTEGERPREFS);
}

#ifdef PREFS_HOSTOVERRIDE
void PrefsCollectionDatabase::ReadOverridesL(const uni_char *host, PrefsSection *section, BOOL active, BOOL from_user)
{
	ReadOverridesInternalL(host, section, active, from_user,
	                       m_integerprefdefault, m_stringprefdefault);
}
#endif // PREFS_HOSTOVERRIDE

#ifdef PREFS_VALIDATE
void PrefsCollectionDatabase::CheckConditionsL(int which, int * value, const uni_char *)
{
	// Check any post-read/pre-write conditions and adjust value accordingly
	switch (static_cast<integerpref>(which))
	{
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	case LocalStorageQuota:
	case LocalStorageGlobalQuota:
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case WidgetPrefsQuota:
	case WidgetPrefsGlobalQuota:
# endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	case UserJSScriptStorageQuota:
# endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND
#ifdef SUPPORT_DATABASE_INTERNAL
	case DatabaseStorageQuota:
	case DatabaseStorageGlobalQuota:
	case DatabaseStorageQueryExecutionTimeout:
#endif // SUPPORT_DATABASE_INTERNAL
		if (*value < 0)
			*value = 0;
		break;

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	case LocalStorageQuotaExceededHandling:
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case WidgetPrefsQuotaExceededHandling:
# endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
#endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND
#ifdef SUPPORT_DATABASE_INTERNAL
	case DatabaseStorageQuotaExceededHandling:
#endif // SUPPORT_DATABASE_INTERNAL
		if (*value < PS_Policy::KQuotaDeny)
			*value = PS_Policy::KQuotaDeny;
		else if (*value > PS_Policy::KQuotaAllow)
			*value = PS_Policy::KQuotaAllow;
		break;

#ifdef SUPPORT_DATABASE_INTERNAL
	case DatabasesAccessHandling:
		if (*value < PS_Policy::KAccessDeny)
			*value = PS_Policy::KAccessDeny;
		else if (*value > PS_Policy::KAccessAllow)
			*value = PS_Policy::KAccessAllow;
		break;
#endif // SUPPORT_DATABASE_INTERNAL

	default:
		// Unhandled preference!
		OP_ASSERT(!"Unhandled preference");
	}
}

BOOL PrefsCollectionDatabase::CheckConditionsL(int which, const OpStringC &invalue, OpString **outvalue, const uni_char *)
{
	// When FALSE is returned, no OpString is created for outvalue
	return FALSE;
}
#endif // PREFS_VALIDATE

#endif //DATABASE_MODULE_MANAGER_SUPPORT
