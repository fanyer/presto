
/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULE_DATABASE_SEC_POLICY_H_
#define _MODULE_DATABASE_SEC_POLICY_H_

#include "modules/database/src/opdatabase_base.h"

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

#include "modules/sqlite/sqlite3.h"

class PS_PolicyFactory;

class PS_Policy : public PS_ObjectTypes
{
public:

	enum QuotaHandling
	{
		KQuotaDeny = 0,
		KQuotaAskUser = 1,
		KQuotaAllow = 2
	};

	enum AccessValues
	{
		KAccessDeny  = 0,
		KAccessAllow = 1
	};

	enum SecAttrUint
	{
		KUintStart,
			/**
			 * Timeout for query execution
			 * 0 to disable, anything else to enable
			 */
			KQueryExecutionTimeout,

			/**
			 * When the quota exceeds, this attribute
			 * tells how it should be handled:
			 *  - KQuotaDeny deny
			 *  - KQuotaAskUser ask user through WindowCommander API.
			 *    Only done if called asynchronously, else it's denied
			 *  - KQuotaAllow ignore
			 *  Unrecognized value is handled as KQuotaDeny.
			 *  When value is denied, ERR_QUOTA_EXCEEDED
			 *  will be returned
			 */
			KOriginExceededHandling,

			/**
			 * Maximum size in bytes that a result set with caching
			 * enabled can get to.
			 * After passing this limit, any call to SqlResultSet::StepL
			 * will return PS_Status::ERR_RSET_TOO_BIG
			 */
			KMaxResultSetSize,

			/**
			 * Maximum number of databases per type and origin:
			 *  0 - unlimited
			 *  anything else - limit
			 */
			KMaxObjectsPerOrigin,

			/**
			 * Tells if access to the database should b allowed or not:
			 * 0 - deny
			 * 1 - allows but returns PS_Status::SHOULD_ASK_FOR_ACCESS
			 *     so the callee can decide whether to ask the user or not
			 * 2 - allows
			 */
			KAccessToObject,
		KUintEnd
	};

	enum SecAttrUint64
	{
		KUint64Start,
			/**
			 * Global browser quota for ALL databases.
			 * Value in bytes
			 * 0 to disable, anything else to enable
			 */

			KGlobalQuota,
			/**
			 * Database quota for the given domain
			 * Value in bytes
			 * 0 to disable, anything else to enable
			 */

			KOriginQuota,
		KUint64End
	};
	enum SecAttrUniStr
	{
		KUniStrStart,
			/**
			 * Path to the main folder where the data files should be placed
			 * This will typically point to the profile folder, widget folder, or extension folder
			 * NOTE: the index file will also be placed here !
			 * If NULL is returned assume OOM.
			 * If an empty string is returned assume that the information is
			 * not available so the call should fail gracefully.
			 */
			KMainFolderPath,

			/**
			 * Named of folder inside KMainFolderPath where the database datafiles should be placed
			 * For instance, if we're on a widget KMainFolderPath will point to the widget data folder
			 * and this will be the name of the folder inside KMainFolderPath that will store the data files
			 */
			KSubFolder,
		KUniStrEnd
	};

	static const unsigned     UINT_ATTR_INVALID   = (unsigned)-1;
	static const OpFileLength UINT64_ATTR_INVALID = FILE_LENGTH_NONE;

	virtual OpFileLength    GetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	virtual unsigned        GetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

	/**
	 * The returned pointer is only valid until the next call to
	 * GetAttribute().  This method does not indicate whether it
	 * succeeds, but unless stated otherwise in the documentation of
	 * the requested attribute, it is reasonable to assume that it
	 * will return NULL on errors.  However, unless stated otherwise in
	 * the documentation of the requested attribute, NULL is also a valid
	 * non-error return value.
	 */
	virtual const uni_char* GetAttribute(SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

	virtual OP_STATUS		SetAttribute(SecAttrUint64 attr, URL_CONTEXT_ID context_id, OpFileLength new_value, const uni_char* domain = NULL, const Window* window = NULL);
	virtual OP_STATUS		SetAttribute(SecAttrUint   attr, URL_CONTEXT_ID context_id, unsigned     new_value, const uni_char* domain = NULL, const Window* window = NULL);

	virtual BOOL IsConfigurable(SecAttrUint64 attr) const;
	virtual BOOL IsConfigurable(SecAttrUint   attr) const;
	virtual BOOL IsConfigurable(SecAttrUniStr attr) const;

protected:

	friend class PS_PolicyFactory;

	PS_Policy(PS_Policy* parent = NULL);
	virtual ~PS_Policy();

	//GetDefaultPolicyL

	PS_Policy* m_parent_policy;
};


#ifdef SUPPORT_DATABASE_INTERNAL
/*************
 * Classes to filter sql statements
 *************/
class SqlValidator
{
public:
	enum Type
	{
		INVALID = 0,
		TRUSTED = 1,
		UNTRUSTED = 2
	};

	SqlValidator() {}
	virtual ~SqlValidator() {}
	virtual int Validate(int sqlite_action, const char* d1, const char* d2, const char* d3, const char* d4) const = 0;
	virtual Type GetType() const { return INVALID; }
};

enum SqlActionFlags
{
	SQL_ACTION_ALLOWED_UNTRUSTED = 0x01,
	SQL_ACTION_AFFECTS_TRANSACTION = 0x02,
	SQL_ACTION_IS_STATEMENT = 0x04
};
#endif //SUPPORT_DATABASE_INTERNAL

//expose the default definitions as well
#include "modules/database/sec_policy_defs.h"

#ifdef HAS_COMPLEX_GLOBALS
# define CONST_STRUCT_ARRAY_DECL(name,type,size) static const type name[size]
#else
# define CONST_STRUCT_ARRAY_DECL(name,type,size) type name[size];void init_##name()
#endif // HAS_COMPLEX_GLOBALS

class PS_PolicyFactory : private PS_Policy
{
public:

	PS_PolicyFactory();
	~PS_PolicyFactory();

	PS_Policy* GetPolicy(PS_ObjectType type);
	const PS_Policy* GetPolicy(PS_ObjectType type) const;

#ifdef SUPPORT_DATABASE_INTERNAL
	const SqlValidator* GetSqlValidator(SqlValidator::Type) const;

	struct SqlActionProperties
	{
		int statement_type;
		unsigned flags;
	};
	enum{ MAX_SQL_ACTIONS = SQLITE_SAVEPOINT+1 };

	BOOL GetSqlActionFlag(unsigned action, unsigned flag) const
	{ OP_ASSERT(flag<MAX_SQL_ACTIONS);return (m_sql_action_properties[action].flags & flag)!=0; }
#endif //SUPPORT_DATABASE_INTERNAL


	/**
	 * Shorthand method to access the policy object directly
	 */
	OpFileLength    GetPolicyAttribute(PS_ObjectType type, SecAttrUint64 attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	unsigned        GetPolicyAttribute(PS_ObjectType type, SecAttrUint   attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;
	const uni_char* GetPolicyAttribute(PS_ObjectType type, SecAttrUniStr attr, URL_CONTEXT_ID context_id, const uni_char* domain = NULL, const Window* window = NULL) const;

private:

#ifdef SUPPORT_DATABASE_INTERNAL
	CONST_STRUCT_ARRAY_DECL(m_sql_action_properties, SqlActionProperties, MAX_SQL_ACTIONS);
#ifdef DEBUG_ENABLE_OPASSERT
	void EnsureSqlActionsConstruction();
#endif // DEBUG_ENABLE_OPASSERT
#endif // SUPPORT_DATABASE_INTERNAL

	OpDefaultGlobalPolicy m_default_global_policy;
#ifdef DATABASE_STORAGE_SUPPORT
	WSD_DatabaseGlobalPolicy m_database_global_policy;
#endif // DATABASE_STORAGE_SUPPORT
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
	WebStoragePolicy m_local_storage_policy;
	WebStoragePolicy m_session_storage_policy;
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	WidgetPreferencesPolicy m_widget_prefs_policy;
#endif
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	WebStorageUserScriptPolicy m_user_js_storage_policy;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#ifdef SUPPORT_DATABASE_INTERNAL
	//sql validators
	TrustedSqlValidator m_trusted_sql_validator;
	UntrustedSqlValidator m_untrusted_sql_validator;
#endif //SUPPORT_DATABASE_INTERNAL

#ifdef SELFTEST
public:
	void SetOverridePolicy(PS_Policy* p) { m_override_policy = p; }
	PS_Policy* GetOverridePolicy() { return m_override_policy; }
	PS_Policy* m_override_policy;
#endif
};

#endif //DATABASE_MODULE_MANAGER_SUPPORT

#endif//_MODULE_DATABASE_SEC_POLICY_H_
