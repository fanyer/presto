/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/database/sec_policy.h"
#include "modules/database/sec_policy_defs.h"
#include "modules/database/opdatabasemanager.h"
#include "modules/util/util_module.h"
#include "modules/url/url_man.h"
#include "modules/database/prefscollectiondatabase.h"
#include "modules/pi/OpSystemInfo.h"

#include "modules/dochand/win.h"

#define IS_DEV_TOOLS_WINDOW(w) ((w) != NULL && ((w)->GetType() == WIN_TYPE_DEVTOOLS))

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

static unsigned OpFileLengthToUnsigned(OpFileLength value)
{
#ifndef HAVE_INT64
	return value;
#else
	if (value > 0xffffffff)
		return 0xffffffff;
	else
		return static_cast<unsigned>(value);
#endif //HAVE_INT64
}

static OP_STATUS
PS_Policy_SetPref(const uni_char *domain, PrefsCollectionDatabase::integerpref which, unsigned value, BOOL from_user)
{
#ifdef PREFS_WRITE
#ifdef PREFS_HOSTOVERRIDE
		OP_ASSERT(domain != NULL && *domain);
		if (domain != NULL && *domain)
			TRAP_AND_RETURN(_, g_pcdatabase->OverridePrefL(domain, which, value, from_user));
#endif //PREFS_HOSTOVERRIDE
#endif //PREFS_WRITE
	return OpStatus::OK;
}


/*virtual*/
unsigned
OpDefaultGlobalPolicy::GetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	switch(attr) {
	case KOriginExceededHandling:
		return KQuotaAllow;
	case KAccessToObject:
		return KAccessAllow;
	}
	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
OpFileLength
OpDefaultGlobalPolicy::GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
const uni_char*
OpDefaultGlobalPolicy::GetAttribute(SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	switch(attr) {
	case KMainFolderPath:
	{
		OpFileFolder folder_id = OPFILE_HOME_FOLDER;

		// The context must have been registered !
		// If this happens, then someone forgot to call g_database_manager->AddContext()
		OP_ASSERT(g_database_manager->GetContextRootFolder(context_id, &folder_id));

		if (!g_database_manager->GetContextRootFolder(context_id, &folder_id))
			return UNI_L("");

		if (folder_id == OPFILE_ABSOLUTE_FOLDER)
			return UNI_L("");

		OP_ASSERT(folder_id != OPFILE_CACHE_FOLDER);

		RETURN_VALUE_IF_ERROR(g_folder_manager->GetFolderPath(folder_id, m_getattr_returnbuffer), NULL);
		return m_getattr_returnbuffer.IsEmpty() ? UNI_L("") : m_getattr_returnbuffer.CStr();
	}
	case KSubFolder:
		return DATABASE_INTERNAL_PROFILE_FOLDER;
	}
	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

#ifdef DATABASE_STORAGE_SUPPORT
/*virtual*/
unsigned
WSD_DatabaseGlobalPolicy::GetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	switch(attr) {
	case KQueryExecutionTimeout:
		return static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::DatabaseStorageQueryExecutionTimeout, domain));
	case KMaxResultSetSize:
		return DATABASE_INTERNAL_MAX_RESULT_SET_SIZE;
	case KOriginExceededHandling:
		if (IS_DEV_TOOLS_WINDOW(window))
			return PS_Policy::KQuotaAllow;
		return static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::DatabaseStorageQuotaExceededHandling, domain));
	case KMaxObjectsPerOrigin:
		return DATABASE_INTERNAL_MAX_WEBPAGE_DBS_PER_ORIGIN;
	case KAccessToObject:
		return static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::DatabasesAccessHandling, domain));
	}

	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
OpFileLength
WSD_DatabaseGlobalPolicy::GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	if (IS_DEV_TOOLS_WINDOW(window))
		return UINT_ATTR_INVALID;

	OpFileLength value;
	switch(attr) {
	case KGlobalQuota:
		value = static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::DatabaseStorageGlobalQuota, domain));
		value *= 1024;
		return value;
	case KOriginQuota:
		value = static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::DatabaseStorageQuota, domain));
		value *= 1024;
		return value;
	}

	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
const uni_char*
WSD_DatabaseGlobalPolicy::GetAttribute(SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
OP_STATUS
WSD_DatabaseGlobalPolicy::SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	if (attr == KOriginQuota)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::DatabaseStorageQuota, OpFileLengthToUnsigned(new_value / 1024), TRUE);
	else if (attr == KGlobalQuota)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::DatabaseStorageGlobalQuota, OpFileLengthToUnsigned(new_value / 1024), TRUE);
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
WSD_DatabaseGlobalPolicy::SetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, unsigned new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	switch(attr) {
	case KQueryExecutionTimeout:
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::DatabaseStorageQueryExecutionTimeout, new_value, TRUE);
	case KOriginExceededHandling:
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::DatabaseStorageQuotaExceededHandling, new_value, TRUE);
	case KAccessToObject:
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::DatabasesAccessHandling, new_value, TRUE);
	}
	return OpStatus::OK;
}

BOOL
WSD_DatabaseGlobalPolicy::IsConfigurable(SecAttrUint64 attr) const
{
	switch(attr){
	case KGlobalQuota:
	case KOriginQuota:
		return TRUE;
	case KMaxResultSetSize:
		return FALSE;
	}
	return PS_Policy::IsConfigurable(attr);
}

BOOL
WSD_DatabaseGlobalPolicy::IsConfigurable(SecAttrUint   attr) const
{
	switch(attr){
	case KQueryExecutionTimeout:
	case KOriginExceededHandling:
	case KAccessToObject:
		return TRUE;
	case KMaxResultSetSize:
	case KMaxObjectsPerOrigin:
		return FALSE;
	}
	return PS_Policy::IsConfigurable(attr);
}
#endif //DATABASE_STORAGE_SUPPORT

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND

/*virtual*/
OpFileLength
WebStoragePolicy::GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	if (IS_DEV_TOOLS_WINDOW(window))
		return UINT_ATTR_INVALID;

	OpFileLength value;
	switch(attr) {
	case KGlobalQuota:
		if (m_type == KSessionStorage)
			return UINT64_ATTR_INVALID;

		value = static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::LocalStorageGlobalQuota, domain));
		value *= 1024;
		return value;
	case KOriginQuota:
		if (m_type == KSessionStorage)
			value = OPSTORAGE_SESSION_MEMORY_QUOTA;
		else
			value = static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::LocalStorageQuota, domain));
		value *= 1024;
		return value;
	}

	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
unsigned
WebStoragePolicy::GetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	switch(attr) {
	case KOriginExceededHandling:
		if (m_type == KSessionStorage)
			return PS_Policy::KQuotaDeny;
		else
			return static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::LocalStorageQuotaExceededHandling, domain));
	case KMaxObjectsPerOrigin:
		return 1;
	case KMaxResultSetSize:
		return UINT_ATTR_INVALID;
	}
	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
OP_STATUS
WebStoragePolicy::SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	if (m_type == KSessionStorage)
		return OpStatus::OK;

	if (attr == KOriginQuota)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::LocalStorageQuota, OpFileLengthToUnsigned(new_value / 1024), TRUE);
	else if (attr == KGlobalQuota)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::LocalStorageGlobalQuota, OpFileLengthToUnsigned(new_value / 1024), TRUE);
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
WebStoragePolicy::SetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, unsigned new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	if (m_type == KSessionStorage)
		return OpStatus::OK;

	if (attr == KOriginExceededHandling)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::LocalStorageQuotaExceededHandling, new_value, TRUE);
	return OpStatus::OK;
}

BOOL
WebStoragePolicy::IsConfigurable(SecAttrUint64 attr) const
{
	switch(attr){
	case KGlobalQuota:
	case KOriginQuota:
		return m_type != KSessionStorage;
	case KMaxResultSetSize:
		return FALSE;
	}
	return PS_Policy::IsConfigurable(attr);
}

BOOL
WebStoragePolicy::IsConfigurable(SecAttrUint   attr) const
{
	switch(attr){
	case KOriginExceededHandling:
		return m_type != KSessionStorage;
	case KMaxObjectsPerOrigin:
	case KMaxResultSetSize:
		return FALSE;
	}
	return PS_Policy::IsConfigurable(attr);
}

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT

/*virtual*/
OpFileLength
WidgetPreferencesPolicy::GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	if (IS_DEV_TOOLS_WINDOW(window))
		return UINT_ATTR_INVALID;

	OpFileLength value;
	switch(attr) {
	case KGlobalQuota:
		value = static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::WidgetPrefsGlobalQuota, domain));
		value *= 1024;
		return value;
	case KOriginQuota:
		value = static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::WidgetPrefsQuota, domain));
		value *= 1024;
		return value;
	case KMaxResultSetSize:
		return UINT_ATTR_INVALID;
	}

	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
unsigned
WidgetPreferencesPolicy::GetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	switch(attr) {
	case KOriginExceededHandling:
		return static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::WidgetPrefsQuotaExceededHandling, domain));
	case KMaxObjectsPerOrigin:
		return 1;
	}
	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
OP_STATUS
WidgetPreferencesPolicy::SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	if (attr == KOriginQuota)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::WidgetPrefsQuota, OpFileLengthToUnsigned(new_value / 1024), TRUE);
	else if (attr == KGlobalQuota)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::WidgetPrefsGlobalQuota, OpFileLengthToUnsigned(new_value / 1024), TRUE);
	return OpStatus::OK;
}

/*virtual*/
OP_STATUS
WidgetPreferencesPolicy::SetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, unsigned new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	if (attr == KOriginExceededHandling)
		return PS_Policy_SetPref(domain, PrefsCollectionDatabase::WidgetPrefsQuotaExceededHandling, new_value, TRUE);
	return OpStatus::OK;
}

BOOL
WidgetPreferencesPolicy::IsConfigurable(SecAttrUint64 attr) const
{
	switch(attr){
	case KGlobalQuota:
	case KOriginQuota:
		return TRUE;
	}
	return PS_Policy::IsConfigurable(attr);
}

BOOL
WidgetPreferencesPolicy::IsConfigurable(SecAttrUint   attr) const
{
	switch(attr){
	case KOriginExceededHandling:
		return TRUE;
	case KMaxObjectsPerOrigin:
		return FALSE;
	}
	return PS_Policy::IsConfigurable(attr);
}

#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

/*virtual*/
OpFileLength
WebStorageUserScriptPolicy::GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	OpFileLength value;
	switch(attr){
	case KGlobalQuota:
		value = UINT64_ATTR_INVALID;
		return value;
	case KOriginQuota:
		value = static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::UserJSScriptStorageQuota, domain));
		value *= 1024;
		return value;
	}

	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

/*virtual*/
unsigned
WebStorageUserScriptPolicy::GetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	switch(attr){
	case KOriginExceededHandling:
		return PS_Policy::KQuotaDeny;
	case KMaxObjectsPerOrigin:
		return UINT_ATTR_INVALID;
	case KAccessToObject:
		return static_cast<unsigned>(g_pcdatabase->GetIntegerPref(PrefsCollectionDatabase::UserJSScriptStorageQuota, domain) == 0 ? KAccessDeny : KAccessAllow);
	}
	return PS_Policy::GetAttribute(attr, context_id, domain, window);
}

BOOL
WebStorageUserScriptPolicy::IsConfigurable(SecAttrUint64 attr) const
{
	switch(attr){
	case KGlobalQuota:
	case KOriginQuota:
	case KMaxResultSetSize:
		return FALSE;
	}
	return PS_Policy::IsConfigurable(attr);
}

BOOL
WebStorageUserScriptPolicy::IsConfigurable(SecAttrUint   attr) const
{
	switch(attr){
	case KOriginExceededHandling:
	case KMaxObjectsPerOrigin:
		return FALSE;
	}
	return PS_Policy::IsConfigurable(attr);
}

#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#ifdef SUPPORT_DATABASE_INTERNAL

#include "modules/sqlite/sqlite3.h"

int
TrustedSqlValidator::Validate(int sqlite_action, const char* d1, const char* d2, const char* d3, const char* d4) const
{
	return SQLITE_OK;
}

int
UntrustedSqlValidator::Validate(int sqlite_action, const char* d1, const char* d2, const char* d3, const char* d4) const
{
	return g_database_policies->GetSqlActionFlag(sqlite_action, SQL_ACTION_ALLOWED_UNTRUSTED) ? SQLITE_OK : SQLITE_DENY;
}
#endif //SUPPORT_DATABASE_INTERNAL

#endif //DATABASE_MODULE_MANAGER_SUPPORT
