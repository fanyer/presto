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

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

PS_Policy::PS_Policy(PS_Policy* parent)
	: m_parent_policy(parent)
{}

PS_Policy::~PS_Policy()
{}

/*virtual*/
OpFileLength
PS_Policy::GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	return m_parent_policy != NULL ? m_parent_policy->GetAttribute(attr, context_id, domain, window) : UINT64_ATTR_INVALID;
}

/*virtual*/
unsigned
PS_Policy::GetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	return m_parent_policy != NULL ? m_parent_policy->GetAttribute(attr, context_id, domain, window) : UINT_ATTR_INVALID;
}

/*virtual*/
const uni_char*
PS_Policy::GetAttribute(SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	return m_parent_policy != NULL ? m_parent_policy->GetAttribute(attr, context_id, domain, window) : NULL;
}

/*virtual*/ OP_STATUS
PS_Policy::SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	if (m_parent_policy)
		return m_parent_policy->SetAttribute(attr, context_id, new_value, domain, window);
	return OpStatus::OK;
}

/*virtual*/ OP_STATUS
PS_Policy::SetAttribute(SecAttrUint attr, URL_CONTEXT_ID context_id, unsigned new_value, const uni_char* domain, const Window* window/*=NULL*/)
{
	if (m_parent_policy)
		return m_parent_policy->SetAttribute(attr, context_id, new_value, domain, window);
	return OpStatus::OK;
}

BOOL
PS_Policy::IsConfigurable(SecAttrUint64 attr) const
{
	return m_parent_policy != NULL && m_parent_policy->IsConfigurable(attr);
}
BOOL
PS_Policy::IsConfigurable(SecAttrUint   attr) const
{
	return m_parent_policy != NULL && m_parent_policy->IsConfigurable(attr);
}
BOOL
PS_Policy::IsConfigurable(SecAttrUniStr attr) const
{
	return m_parent_policy != NULL && m_parent_policy->IsConfigurable(attr);
}

#ifdef SUPPORT_DATABASE_INTERNAL
#ifdef DEBUG_ENABLE_OPASSERT
void PS_PolicyFactory::EnsureSqlActionsConstruction()
{
	for(int k=0; k<MAX_SQL_ACTIONS; k++)
	{
		OP_ASSERT(m_sql_action_properties[k].statement_type == k);
	}
}
#else
#define EnsureSqlActionsConstruction() ((void)0)
#endif // DEBUG_ENABLE_OPASSERT
#endif // SUPPORT_DATABASE_INTERNAL

PS_PolicyFactory::PS_PolicyFactory()
	: m_default_global_policy(NULL)
#ifdef DATABASE_STORAGE_SUPPORT
	, m_database_global_policy(&m_default_global_policy)
#endif //DATABASE_STORAGE_SUPPORT
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	, m_local_storage_policy(KLocalStorage, &m_default_global_policy)
	, m_session_storage_policy(KSessionStorage, &m_default_global_policy)
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	, m_widget_prefs_policy(&m_default_global_policy)
#endif
# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	, m_user_js_storage_policy(&m_default_global_policy)
# endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND
#ifdef SELFTEST
	, m_override_policy(NULL)
#endif
{
#ifdef SUPPORT_DATABASE_INTERNAL
	CONST_ARRAY_INIT(m_sql_action_properties);
	EnsureSqlActionsConstruction();
#endif //SUPPORT_DATABASE_INTERNAL
}

PS_PolicyFactory::~PS_PolicyFactory() {}

const PS_Policy*
PS_PolicyFactory::GetPolicy(PS_ObjectType type) const
{
#ifdef SELFTEST
	if (m_override_policy != NULL)
		return m_override_policy;
#endif
	switch(type)
	{
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	case KLocalStorage:
		return &m_local_storage_policy;
	case KSessionStorage:
		return &m_session_storage_policy;
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	case KWidgetPreferences:
		return &m_widget_prefs_policy;
# endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	case KUserJsStorage:
		return &m_user_js_storage_policy;
# endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#ifdef DATABASE_STORAGE_SUPPORT
	case KWebDatabases:
		return &m_database_global_policy;
#endif
	}
	return &m_default_global_policy;
}

PS_Policy*
PS_PolicyFactory::GetPolicy(PS_ObjectType type)
{
	return const_cast<PS_Policy*>(const_cast<const PS_PolicyFactory*>(this)->GetPolicy(type));
}

#ifdef SUPPORT_DATABASE_INTERNAL
const SqlValidator*
PS_PolicyFactory::GetSqlValidator(SqlValidator::Type type) const
{
	switch(type)
	{
	case SqlValidator::TRUSTED:
		return &m_trusted_sql_validator;
	case SqlValidator::UNTRUSTED:
		return &m_untrusted_sql_validator;
	}
	OP_ASSERT(!"Bad SqlValidator::Type");
	return NULL;
}
#endif //SUPPORT_DATABASE_INTERNAL

OpFileLength
PS_PolicyFactory::GetPolicyAttribute(PS_ObjectType type, SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	const PS_Policy* policy = GetPolicy(type);
	return policy != NULL ? policy->GetAttribute(attr, context_id, domain, window) : UINT64_ATTR_INVALID;
}

unsigned
PS_PolicyFactory::GetPolicyAttribute(PS_ObjectType type, SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	const PS_Policy* policy = GetPolicy(type);
	return policy != NULL ? policy->GetAttribute(attr, context_id, domain, window) : UINT_ATTR_INVALID;
}

const uni_char*
PS_PolicyFactory::GetPolicyAttribute(PS_ObjectType type, SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain, const Window* window) const
{
	const PS_Policy* policy = GetPolicy(type);
	return policy != NULL ? policy->GetAttribute(attr, context_id, domain, window) : NULL;
}

#ifdef SUPPORT_DATABASE_INTERNAL

#ifdef HAS_COMPLEX_GLOBALS
# define CONST_STRUCT_ARRAY_DEFN(klass,name,type,size) const type klass::name[size] = {
# define CONST_STRUCT_ENTRY_2(a, b)    {a,b},
#else
# define CONST_STRUCT_ARRAY_DEFN(klass,name,type,size) void klass::init_##name () { type *local = name; int i=0;
# define CONST_STRUCT_ENTRY_2(a, b)    local[i].statement_type=a;local[i].flags=b;i++;
#endif // HAS_COMPLEX_GLOBALS

CONST_STRUCT_ARRAY_DEFN(PS_PolicyFactory, m_sql_action_properties, PS_PolicyFactory::SqlActionProperties, PS_PolicyFactory::MAX_SQL_ACTIONS)
	CONST_STRUCT_ENTRY_2(SQLITE_COPY,                0)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_INDEX,        SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_TABLE,        SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_TEMP_INDEX,   SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_TEMP_TABLE,   SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_TEMP_TRIGGER, SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_TEMP_VIEW,    SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_TRIGGER,      SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_VIEW,         SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DELETE,              SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_INDEX,          SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_TABLE,          SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_TEMP_INDEX,     SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_TEMP_TABLE,     SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_TEMP_TRIGGER,   SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_TEMP_VIEW,      SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_TRIGGER,        SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_VIEW,           SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_INSERT,              SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_PRAGMA,              0)
	CONST_STRUCT_ENTRY_2(SQLITE_READ,                SQL_ACTION_ALLOWED_UNTRUSTED)
	CONST_STRUCT_ENTRY_2(SQLITE_SELECT,              SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_TRANSACTION,         SQL_ACTION_AFFECTS_TRANSACTION)
	CONST_STRUCT_ENTRY_2(SQLITE_UPDATE,              SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_ATTACH,              0)
	CONST_STRUCT_ENTRY_2(SQLITE_DETACH,              0)
	CONST_STRUCT_ENTRY_2(SQLITE_ALTER_TABLE,         SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_REINDEX,             SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_ANALYZE,             SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_CREATE_VTABLE,       SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_DROP_VTABLE,         SQL_ACTION_ALLOWED_UNTRUSTED | SQL_ACTION_IS_STATEMENT)
	CONST_STRUCT_ENTRY_2(SQLITE_FUNCTION,            SQL_ACTION_ALLOWED_UNTRUSTED)
	CONST_STRUCT_ENTRY_2(SQLITE_SAVEPOINT,           SQL_ACTION_ALLOWED_UNTRUSTED)
CONST_END(m_sql_action_properties)

#endif //SUPPORT_DATABASE_INTERNAL

#endif //DATABASE_MODULE_MANAGER_SUPPORT
