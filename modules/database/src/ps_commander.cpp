/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/database/ps_commander.h"

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

#include "modules/database/opdatabasemanager.h"
#include "modules/database/src/webstorage_data_abstraction_simple_impl.h"

#undef GetManager
#ifdef SELFTEST
/*virtual*/ void*
PersistentStorageCommander::GetManagerPtr()
{
	return g_database_manager;
}
# define GetManager() (static_cast<PS_Manager *>(GetManagerPtr()))
#else
# define GetManager() g_database_manager
#endif
URL_CONTEXT_ID PersistentStorageCommander::ALL_CONTEXT_IDS = DB_NULL_CONTEXT_ID;

#define TYPE_TO_FLAG(t) (1<<(PS_ObjectTypes::t))

static unsigned
GetWebStorageFlags()
{
	return
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
		TYPE_TO_FLAG(KSessionStorage) | TYPE_TO_FLAG(KLocalStorage) |
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
		TYPE_TO_FLAG(KWidgetPreferences) |
# endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
		TYPE_TO_FLAG(KUserJsStorage) |
# endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif // WEBSTORAGE_ENABLE_SIMPLE_BACKEND
		0;
}

static unsigned
GetWebDatabasesFlags()
{
	return
#ifdef DATABASE_STORAGE_SUPPORT
		TYPE_TO_FLAG(KWebDatabases) |
#endif // DATABASE_STORAGE_SUPPORT
		0;
}

/*static*/
PersistentStorageCommander*
PersistentStorageCommander::GetInstance()
{
	return g_database_module.m_ps_commander;
}

void PersistentStorageCommander::DeleteAllData(URL_CONTEXT_ID context_id, const uni_char *origin)
{
	GetManager()->PostDeleteDataMessage(context_id, GetWebDatabasesFlags() | GetWebStorageFlags(), origin);
}

void PersistentStorageCommander::DeleteWebDatabases(URL_CONTEXT_ID context_id, const uni_char *origin)
{
	GetManager()->PostDeleteDataMessage(context_id, GetWebDatabasesFlags(), origin);
}

void PersistentStorageCommander::DeleteWebStorage(URL_CONTEXT_ID context_id, const uni_char *origin)
{
	GetManager()->PostDeleteDataMessage(context_id, GetWebStorageFlags(), origin);
}

void PersistentStorageCommander::DeleteExtensionData(URL_CONTEXT_ID context_id, const uni_char *origin)
{
	GetManager()->PostDeleteDataMessage(context_id, GetWebDatabasesFlags() | GetWebStorageFlags(), origin, true);
}

OP_STATUS PersistentStorageCommander::EnumeratePersistentStorage(PS_Enumerator& enumerator, URL_CONTEXT_ID context_id, const uni_char *origin)
{
	class ThisIterator: public PS_MgrContentIterator
	{
	public:
		PS_Manager* m_mgr;
		PersistentStorageCommander::PS_Info m_info;
		PersistentStorageCommander::PS_Enumerator& m_enumerator;
		const uni_char *m_origin;
		BOOL m_skip;

		ThisIterator(PS_Manager* mgr, PersistentStorageCommander::PS_Enumerator& enumerator, const uni_char *origin)
			: m_mgr(mgr), m_enumerator(enumerator), m_origin(origin) {}

		virtual OP_STATUS HandleType(URL_CONTEXT_ID context_id, PS_ObjectType type)
		{
			if (m_origin)
				return HandleOrigin(context_id, type, m_origin);
			else
				return m_mgr->EnumerateOrigins(this, context_id, type);
		}
		virtual OP_STATUS HandleOrigin(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin)
		{
			OP_ASSERT(m_origin == NULL || uni_str_eq(m_origin, origin));

			// Check if it's being used and is persistent, in HandleObject
			m_info.m_in_use = FALSE;
			m_info.m_number_objects = 0;
			m_skip = TRUE;
			RETURN_IF_ERROR(m_mgr->EnumerateObjects(this, context_id, type, origin));
			if (m_skip)
				return OpStatus::OK;

			// Get used size
			RETURN_IF_ERROR(m_mgr->GetGlobalDataSize(&m_info.m_used_size,
				type, context_id, origin));

			// Context id
			m_info.m_context_id = context_id;

			// Origin
			m_info.m_origin = origin;

			// Readable type
			m_info.m_type = m_mgr->GetTypeDesc(type);

			m_enumerator.HandlePSInfo(m_info);
			return OpStatus::OK;
		}
		virtual OP_STATUS HandleObject(PS_IndexEntry* object_entry)
		{
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
			if (object_entry->GetType() == KSessionStorage)
				return OpStatus::OK;
#endif// WEBSTORAGE_ENABLE_SIMPLE_BACKEND
			if (object_entry->IsPersistent()) {
				m_skip = FALSE;
				m_info.m_number_objects++;
				if (0 < object_entry->GetRefCount())
					m_info.m_in_use = TRUE;
			}
			return OpStatus::OK;
		}
	} itr(GetManager(), enumerator, origin);
	return GetManager()->EnumerateTypes(&itr, context_id);
}

#undef GetManager

#endif // DATABASE_MODULE_MANAGER_SUPPORT
