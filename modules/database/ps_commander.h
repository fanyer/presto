	/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#ifndef _MODULES_DATABASE_PS_COMMANDER_H_
#define _MODULES_DATABASE_PS_COMMANDER_H_

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

/**
 * Class that acts as interface between core's html5 persistent
 * storage data structures and logic and the outside world.
 *
 * This class currently only supports the URL_CONTEXT_ID 0, which is
 * the main browsing context.
 */
class PersistentStorageCommander
{
public:
	/**
	 * Default value which represents all context ids
	 */
	static URL_CONTEXT_ID ALL_CONTEXT_IDS;

	/**
	 * Returns single instance of this class
	 */
	static PersistentStorageCommander* GetInstance();

	/**
	 * Deletes all data held by the database module
	 * which requires storage on disk. Includes all
	 * web databases and web storage objects
	 * Volatile data kept only in memory is not affected.
	 * The deletion is performed asynchronously.
	 *
	 * @param context_id    Profile's context id the data belongs to.
	 * @param origin        Optional. Origin of data to delete.
	 */
	void DeleteAllData(URL_CONTEXT_ID context_id, const uni_char *origin = NULL);

	/**
	 * Deletes all persistent web databases.
	 * Volatile data kept only in memory is not affected.
	 * The deletion is performed asynchronously.
	 *
	 * @param context_id    Profile's context id the data belongs to.
	 * @param origin        Optional. Origin of data to delete.
	 */
	void DeleteWebDatabases(URL_CONTEXT_ID context_id, const uni_char *origin = NULL);

	/**
	 * Deletes all persistent web storage objects, like
	 * localStorage, widget.preferences if any and user script
	 * storage if any.
	 * Volatile data kept only in memory is not affected.
	 * The deletion is performed asynchronously.
	 *
	 * @param context_id    Profile's context id the data belongs to.
	 * @param origin        Optional. Origin of data to delete.
	 */
	void DeleteWebStorage(URL_CONTEXT_ID context_id, const uni_char *origin = NULL);

	/**
	 * Deletes data for a given extension, which includes localStorage,
	 * web sql dbs, and idb.
	 *
	 * @param context_id    Profile's context id the data belongs to.
	 * @param origin        Optional. Origin of data to delete.
	 */
	void DeleteExtensionData(URL_CONTEXT_ID context_id, const uni_char *origin = NULL);

	/**
	 * Helper class for PS_Enumerator::HandlePSInfo
	 */
	struct PS_Info {
		URL_CONTEXT_ID m_context_id;     ///< url context id
		OpFileLength   m_used_size;      ///< used size for origin in bytes
		unsigned       m_number_objects; ///< Number of objects. e.g.: localStorage will always be one, but web dbs can be many for a given origin.
		BOOL           m_in_use;         ///< TRUE if the storage area is being used by a webpage in this given moment.
		OpStringC      m_type;           ///< Type. One of localstorage, sessionstorage, webdatabase, widgetpreferences, userscript
		OpStringC      m_origin;         ///< Origin!
	};

	/**
	 * Helper class for EnumeratePersistentStorage.
	 */
	class PS_Enumerator {
	public:
		virtual void HandlePSInfo(const PS_Info& info) = 0;
	};

	/**
	 * Enumerates all persistent storage areas per url context id,
	 * origin and type, in no particular order, synchronously.
	 * Persistent storage includes web sql databases, web storage,
	 * widget.preferences, etc.
	 * The data is passed to PS_Enumerator::HandlePSInfo but the callback
	 * does not own the data.
	 *
	 * @param enumerator   Callback object which the data is passed to.
	 * @param context_id   Context id of user profile that contains the data.
	 * @param origin       Optional. Origin of storage areas to enumerate.
	 */
	OP_STATUS EnumeratePersistentStorage(PS_Enumerator& enumerator, URL_CONTEXT_ID context_id, const uni_char *origin = NULL);

#ifdef SELFTEST
private:
	virtual void* GetManagerPtr();
public:
	virtual ~PersistentStorageCommander(){} // Silence clang warning.
#endif // SELFTEST
};

#endif // DATABASE_MODULE_MANAGER_SUPPORT

#endif // _MODULES_DATABASE_PS_COMMANDER_H_
