/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/database/src/opdatabase_base.h"

#if defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT

#include "modules/database/database_module.h"
#include "modules/url/url_lop_api.h"

#ifdef DATABASE_MODULE_MANAGER_SUPPORT
# include "modules/database/opdatabasemanager.h"
# include "modules/database/sec_policy.h"
# include "modules/database/ps_commander.h"
# include "modules/about/operawebstorage.h"
#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef OPSTORAGE_SUPPORT
# include "modules/database/opstorage.h"
#endif //OPSTORAGE_SUPPORT

#ifdef HAS_OPERA_WEBSTORAGE_PAGE
class OperaWebStorageAdminURLGenerator
	: public OperaURL_Generator
{
	OperaWebStorage::PageType m_type;
public:
	OperaWebStorageAdminURLGenerator(OperaWebStorage::PageType type) : m_type(type) {}
	virtual ~OperaWebStorageAdminURLGenerator() {}
	virtual GeneratorMode GetMode() const { return KQuickGenerate; }
	virtual OP_STATUS QuickGenerate(URL &url, OpWindowCommander*)
	{
		g_url_api->MakeUnique(url);
		OperaWebStorage page(url, m_type);
		return page.GenerateData();
	}
	static OP_STATUS Create(OperaWebStorage::PageType type, const OpStringC8 &p_name, BOOL prefix, OperaWebStorageAdminURLGenerator ** result)
	{
		OP_ASSERT(result != NULL);
		OperaWebStorageAdminURLGenerator* o =  OP_NEW(OperaWebStorageAdminURLGenerator, (type));
		RETURN_OOM_IF_NULL(o);
		OP_STATUS status = o->Construct(p_name, prefix);
		if (OpStatus::IsError(status))
			OP_DELETE(o);
		else
			*result = o;
		return status;
	}
};
#endif // HAS_OPERA_WEBSTORAGE_PAGE

DatabaseModule::DatabaseModule() : m_being_destroyed(FALSE)
{
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	m_ps_manager_instance = NULL;
	m_ps_commander = NULL;
	m_policy_globals = NULL;
# ifdef DATABASE_ABOUT_WEBDATABASES_URL
	m_webdatabase_admin_generator = NULL;
# endif //DATABASE_ABOUT_WEBDATABASES_URL
#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef OPSTORAGE_SUPPORT
	m_web_storage_mgr_instance = NULL;
	m_opstorage_globals = NULL;
# ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	m_webstorage_admin_generator = NULL;
# endif // DATABASE_ABOUT_WEBSTORAGE_URL
#endif //OPSTORAGE_SUPPORT
}

void DatabaseModule::InitL(const OperaInitInfo& info)
{
	OP_STATUS status = Init(info);
	if (OpStatus::IsError(status))
	{
		Destroy();
		LEAVE(status);
	}
}

OP_STATUS DatabaseModule::Init(const OperaInitInfo& info)
{
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	OP_ASSERT(m_policy_globals == NULL);
	m_policy_globals = OP_NEW(PS_PolicyFactory,());
	RETURN_OOM_IF_NULL(m_policy_globals);

	OP_ASSERT(m_ps_manager_instance == NULL);
	m_ps_manager_instance = OP_NEW(PS_Manager,());
	RETURN_OOM_IF_NULL(m_ps_manager_instance);

	OP_ASSERT(m_ps_commander == NULL);
	m_ps_commander = OP_NEW(PersistentStorageCommander,());
	RETURN_OOM_IF_NULL(m_ps_commander);

# if defined DATABASE_ABOUT_WEBDATABASES_URL && defined HAS_OPERA_WEBSTORAGE_PAGE
	OP_ASSERT(m_webdatabase_admin_generator == NULL);
	RETURN_IF_ERROR(OperaWebStorageAdminURLGenerator::Create(OperaWebStorage::WEB_DATABASES, DATABASE_ABOUT_WEBDATABASES_URL, FALSE, &m_webdatabase_admin_generator));
	OP_ASSERT(m_webdatabase_admin_generator != NULL);
	g_url_api->RegisterOperaURL(m_webdatabase_admin_generator);
# endif // defined DATABASE_ABOUT_WEBDATABASES_URL && defined HAS_OPERA_WEBSTORAGE_PAGE

#endif //DATABASE_MODULE_MANAGER_SUPPORT

#ifdef OPSTORAGE_SUPPORT
	OP_ASSERT(m_web_storage_mgr_instance == NULL);
	m_web_storage_mgr_instance = OpStorageManager::Create();
	RETURN_OOM_IF_NULL(m_web_storage_mgr_instance);

	OP_ASSERT(m_opstorage_globals == NULL);
	m_opstorage_globals = OP_NEW(OpStorageGlobals,());
	RETURN_OOM_IF_NULL(m_opstorage_globals);

# if defined DATABASE_ABOUT_WEBSTORAGE_URL && defined HAS_OPERA_WEBSTORAGE_PAGE
	OP_ASSERT(m_webstorage_admin_generator == NULL);
	RETURN_IF_ERROR(OperaWebStorageAdminURLGenerator::Create(OperaWebStorage::WEB_STORAGE, DATABASE_ABOUT_WEBSTORAGE_URL, FALSE, &m_webstorage_admin_generator));
	OP_ASSERT(m_webstorage_admin_generator != NULL);
	g_url_api->RegisterOperaURL(m_webstorage_admin_generator);
# endif // defined DATABASE_ABOUT_WEBSTORAGE_URL && defined HAS_OPERA_WEBSTORAGE_PAGE

#endif //OPSTORAGE_SUPPORT

	return OpStatus::OK;
}

#ifdef DEBUG_ENABLE_OPASSERT
DatabaseModule::~DatabaseModule()
{
#ifdef DATABASE_MODULE_MANAGER_SUPPORT
	OP_ASSERT(m_policy_globals == NULL);
	OP_ASSERT(m_ps_manager_instance == NULL);
	OP_ASSERT(m_ps_commander == NULL);
# ifdef DATABASE_ABOUT_WEBDATABASES_URL
	OP_ASSERT(m_webdatabase_admin_generator == NULL);
# endif // DATABASE_ABOUT_WEBDATABASES_URL
#endif // DATABASE_MODULE_MANAGER_SUPPORT

#ifdef OPSTORAGE_SUPPORT
	OP_ASSERT(m_web_storage_mgr_instance == NULL);
	OP_ASSERT(m_opstorage_globals == NULL);
# ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	OP_ASSERT(m_webstorage_admin_generator == NULL);
# endif // DATABASE_ABOUT_WEBSTORAGE_URL
#endif // OPSTORAGE_SUPPORT
}
#endif // DEBUG_ENABLE_OPASSERT

void DatabaseModule::Destroy()
{
	m_being_destroyed = TRUE;

#ifdef OPSTORAGE_SUPPORT

# ifdef DATABASE_ABOUT_WEBSTORAGE_URL
	OP_DELETE(m_webstorage_admin_generator);
	m_webstorage_admin_generator = NULL;
# endif // defined DATABASE_ABOUT_WEBSTORAGE_URL && defined DATABASE_MODULE_MANAGER_SUPPORT

	OP_ASSERT(m_web_storage_mgr_instance->GetRefCount() == 1);
	m_web_storage_mgr_instance->Release();
	m_web_storage_mgr_instance = NULL;

	OP_DELETE(m_opstorage_globals);
	m_opstorage_globals = NULL;

#endif //OPSTORAGE_SUPPORT

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

# ifdef DATABASE_ABOUT_WEBDATABASES_URL
	OP_DELETE(m_webdatabase_admin_generator);
	m_webdatabase_admin_generator = NULL;
# endif // DATABASE_ABOUT_WEBDATABASES_URL

	OP_DELETE(m_ps_commander);
	m_ps_commander = NULL;

	if (m_ps_manager_instance != NULL)
	{
		OP_DELETE(m_ps_manager_instance);
		m_ps_manager_instance = NULL;
	}

	OP_DELETE(m_policy_globals);
	m_policy_globals = NULL;

#endif //DATABASE_MODULE_MANAGER_SUPPORT
}

#endif //defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT
