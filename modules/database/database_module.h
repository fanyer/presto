/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#ifndef _MODULES_DATABASE_DATABASE_MODULE_H_
#define _MODULES_DATABASE_DATABASE_MODULE_H_

#if defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT

#include "modules/hardcore/opera/module.h"

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	class PS_Manager;
	class PersistentStorageCommander;
	class PS_PolicyFactory;
#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef OPSTORAGE_SUPPORT
	class OpStorageManager;
	class OpStorageGlobals;
#endif //OPSTORAGE_SUPPORT

#if defined DATABASE_ABOUT_WEBSTORAGE_URL || defined DATABASE_ABOUT_WEBDATABASES_URL
	class OperaWebStorageAdminURLGenerator;
#endif // defined DATABASE_ABOUT_WEBSTORAGE_URL || defined DATABASE_ABOUT_WEBDATABASES_URL

/**
 * DatabaseModule is the entry class for the database module.
 * This class does not provide much functionality but to store the main instance
 * of the PS_Manager class which will store the index of all database that
 * the code has in use. It also keeps an instance of OpStorageManager which holds
 * LocalStorage objects, because there can be shared to spare memory and the events
 * queue. This class also stores the main OpStorageGlobals instance
 * when client side storage events are enabled, because events handling need to
 * keep of a global status of all events being queued to prevent DoS
 */
class DatabaseModule: public OperaModule
{
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	/**
	 * Main instance PS_Manager which contains the index of all
	 * data files used by the database module. See class documentation for details
	 * */
	PS_Manager* m_ps_manager_instance;

	/**
	 * Single instance of PersistentStorageCommander
	 */
	PersistentStorageCommander* m_ps_commander;
	friend class PersistentStorageCommander;

	/**
	 * This object stores the main instance of PS_PolicyFactory
	 * which is responsible for returning values which control quotas,
	 * other security seting and resource setting for databases
	 */
	PS_PolicyFactory* m_policy_globals;

# ifdef DATABASE_ABOUT_WEBDATABASES_URL
	/**
	 *  Opera url generator for the webdatabases admin page
	 */
	OperaWebStorageAdminURLGenerator* m_webdatabase_admin_generator;
# endif // DATABASE_ABOUT_WEBDATABASES_URL

#endif // DATABASE_MODULE_MANAGER_SUPPORT

#ifdef OPSTORAGE_SUPPORT
	/**
	 * Instance OpStorageManager which contains all LocalStorage objects.
	 * See class documentation for details
	 * */
	OpStorageManager* m_web_storage_mgr_instance;


	/**
	 * Main instance OpStorageGlobals which contains globals needed for
	 * the client side storage events feature. See class documentation for details
	 * */
	OpStorageGlobals* m_opstorage_globals;

# ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	/**
	 *  Opera url generator for the webstorage admin page
	 */
	OperaWebStorageAdminURLGenerator* m_webstorage_admin_generator;
# endif // DATABASE_ABOUT_WEBSTORAGE_URL

#endif // OPSTORAGE_SUPPORT

	/**
	 * Common InitL method to all OperaModule decendants.
	 * This method allocates the main instance of the database manager
	 * and of the OpStorageGlobals class internally
	 **/
	OP_STATUS Init(const OperaInitInfo& info);

public:

	/**
	 * Tells if Destroy has been called
	 */
	BOOL m_being_destroyed;

	/**
	 * Main constructor - does nothing
	 **/
	DatabaseModule();

#ifdef DEBUG_ENABLE_OPASSERT
	virtual ~DatabaseModule();
#endif // DEBUG_ENABLE_OPASSERT

	/**
	 * Common InitL method to all OperaModule decendants.
	 * This method allocates the main instance of the database manager
	 * and of the OpStorageGlobals class internally.
	 * This is just a wrapper around Init()
	 **/
	virtual void InitL(const OperaInitInfo& info);

	/**
	 * Cleanup method to dispose this module object
	 **/
	virtual void Destroy();

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

	/**
	 * Simply returns the main instance of PS_Manager
	 * */
	inline PS_Manager* GetPSManager() const { return m_ps_manager_instance; }

	/**
	 * Returns main instance of PS_PolicyFactory
	 */
	PS_PolicyFactory* GetPolicyFactory() const { return m_policy_globals; }
#define g_database_policies g_database_module.GetPolicyFactory()

#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef OPSTORAGE_SUPPORT
	/**
	 * Simply returns the localStorage instance of OpStorageManager
	 * */
	inline OpStorageManager* GetWebStorageManager() const { return m_web_storage_mgr_instance; }
#define g_webstorage_manager g_database_module.GetWebStorageManager()


	/**
	 * Simply returns the main instance of OpStorageGlobals
	 * */
	inline OpStorageGlobals* GetOpStorageGlobals() const { return m_opstorage_globals; }
#define g_opstorage_globals g_database_module.GetOpStorageGlobals()


#endif //OPSTORAGE_SUPPORT
};

/**
 * Macros to allow easy access to the database module globals
 * */
#define g_database_module g_opera->database_module
#define g_database_manager g_database_module.GetPSManager()

#define DATABASE_MODULE_REQUIRED

#endif //defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT


#ifdef DATABASE_MODULE_MANAGER_SUPPORT

# define DATABASE_MODULE_ADD_CONTEXT(id, root_folder) (g_database_manager != NULL ? g_database_manager->AddContext(id, root_folder, false) : OpStatus::OK)
# define DATABASE_MODULE_ADD_EXTENSION_CONTEXT(id, root_folder) (g_database_manager != NULL ? g_database_manager->AddContext(id, root_folder, true) : OpStatus::OK)
# define DATABASE_MODULE_REMOVE_CONTEXT(id) do { if (g_database_manager != NULL) g_database_manager->RemoveContext(id); } while(0)

#else // DATABASE_MODULE_MANAGER_SUPPORT

# define DATABASE_MODULE_ADD_CONTEXT(id, root_folder) (OpStatus::OK)
# define DATABASE_MODULE_ADD_EXTENSION_CONTEXT(id, root_folder) (OpStatus::OK)
# define DATABASE_MODULE_REMOVE_CONTEXT(id) ((void)0)

#endif // DATABASE_MODULE_MANAGER_SUPPORT

#endif /* _MODULES_DATABASE_DATABASE_MODULE_H_ */
