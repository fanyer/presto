/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"

#ifdef DATABASE_STORAGE_SUPPORT

#include "modules/dom/src/js/window.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/storage/database.h"
#include "modules/dom/src/storage/sqltransaction.h"
#include "modules/dom/src/storage/storageutils.h"
#include "modules/doc/frm_doc.h"

#include "modules/database/opdatabase.h"
#include "modules/database/opdatabasemanager.h"

DOM_DbManager::~DOM_DbManager()
{
	m_db_list.RemoveAll();
}

DOM_Database* DOM_DbManager::FindDbObject(WSD_Database *db) const
{
	for (DOM_Database *iter = GetFirstDb(); iter != NULL; iter = iter->Suc())
		if (iter->GetDb() == db)
			return iter;
	return NULL;
}

DOM_Database* DOM_DbManager::FindDbByName(const uni_char *name) const
{
	for (DOM_Database *iter = GetFirstDb(); iter != NULL; iter = iter->Suc())
	{
		const uni_char *odbn = iter->GetRealName();
		if (odbn == name || (odbn != NULL && name != NULL && uni_str_eq(odbn, name)))
			return iter;
	}
	return NULL;
}

/*static*/
DOM_DbManager *DOM_DbManager::LookupManagerForWindow(DOM_Object *win_obj)
{
	if (win_obj == NULL || !win_obj->IsA(DOM_TYPE_WINDOW))
		return NULL;

	JS_Window *window = static_cast<JS_Window *>(win_obj);

	ES_Value value;
	if (window->GetPrivate(DOM_PRIVATE_database, &value) == OpBoolean::IS_TRUE && value.type == VALUE_OBJECT)
		if (DOM_Object *object = DOM_GetHostObject(value.value.object))
			if (object->IsA(DOM_TYPE_OBJECT))
				return static_cast<DOM_DbManager *>(object);

	return NULL;
}

void
DOM_DbManager::ClearExpectedVersions(const uni_char *db_name)
{
	if (uni_str_eq(db_name, UNI_L("*")))
	{
		for (DOM_Database *dom_db = GetFirstDb(); dom_db != NULL; dom_db = dom_db->Suc())
			dom_db->ClearExpectedVersion();
	}
	else
	{
		DOM_Database *dom_db = FindDbByName(db_name);
		if (dom_db != NULL)
			dom_db->ClearExpectedVersion();
	}
}

void DOM_DbManager::GCTrace()
{
	for (DOM_Database *db = GetFirstDb(); db != NULL; db = db->Suc())
	{
		/* Databases that have open transactions need to be gcmarked
		   so they can gcmark their respective transactions, else the
		   database can be collected. */
		if (db->HasOpenTransactions())
			GCMark(db);
	}
}

OP_STATUS DOM_DbManager::InsertDbObject(DOM_Database *db_object)
{
	db_object->Into(&m_db_list);

	return OpStatus::OK;
}

/* static */ OP_STATUS DOM_DbManager::Make(DOM_DbManager *&manager, DOM_Runtime *runtime)
{
	RETURN_IF_ERROR(DOMSetObjectRuntime(manager = OP_NEW(DOM_DbManager, ()), runtime,
		runtime->GetObjectPrototype(), "Object"));

	return OpStatus::OK;
}

OP_STATUS DOM_DbManager::FindOrCreateDb(DOM_Database *&database,
									 const uni_char *db_name,
									 const uni_char *version,
									 const uni_char *display_name,
									 OpFileLength author_size)
{
	DOM_Runtime *runtime = GetRuntime();

	if (!runtime->GetFramesDocument())
		return OpStatus::ERR;

	DOM_PSUtils::PS_OriginInfo oi;
	RETURN_IF_ERROR(DOM_PSUtils::GetPersistentStorageOriginInfo(runtime, oi));

	WSD_Database *db = NULL;
	RETURN_IF_ERROR(WSD_Database::GetInstance(oi.m_origin, db_name, oi.m_is_persistent, oi.m_context_id, &db));
	AutoReleaseWSDDatabasePtr db_ptr(db); // Anchor pointer.

	if (version != NULL)
	{
		if (db->GetVersion() == NULL)
			// The database doesn't have a version yet, so set it
			RETURN_IF_ERROR(db->SetVersion(version));
		else if (*version != 0 && !uni_str_eq(version, db->GetVersion()))
			/* The database either has an empty version and it's trying to be opened with another
			   version or the versions differ, so it's an error.
			   If the database has a non-empty version but an empty version is provided then it's ok
			   and it means the author just wants the latest version available. */
			return OpStatus::ERR;
	}
	database = FindDbObject(db);
	if (database == NULL)
	{
		RETURN_IF_ERROR(DOM_Database::Make(this, database, runtime, db, version, display_name, author_size));
		db_ptr.Override(NULL);
		InsertDbObject(database);
	}

	return OpStatus::OK;
}

/*static*/ void
DOM_Database::BeforeUnload(DOM_EnvironmentImpl *e)
{
	DOM_DbManager *dbm = DOM_DbManager::LookupManagerForWindow(e->GetWindow());

	if (dbm == NULL)
		return;

	for (DOM_Database *db = dbm->GetFirstDb(); db != NULL; db = db->Suc())
	{
		for (DOM_SQLTransaction *t = db->m_transactions.First(); t != NULL; t = t->Suc())
		{
			// Finished transactions remove themselves from the list, so they can be gc'ed.
			OP_ASSERT(!t->HasFinished());
			t->SetDone(FALSE);
		}
		db->m_db = NULL;
	}
}

DOM_Database::DOM_Database(WSD_Database* db, OpFileLength author_size)
	: DOM_BuiltInConstructor(DOM_Runtime::DATABASE_PROTOTYPE)
	, m_db(db)
	, m_db_mgr(NULL)
	, m_author_size(author_size)
{
}

OP_STATUS
DOM_Database::EnsureDbIsInitialized()
{
	if (m_db == NULL)
	{
		if (!GetRuntime()->GetFramesDocument())
			return OpStatus::ERR;

		DOM_PSUtils::PS_OriginInfo oi;
		RETURN_IF_ERROR(DOM_PSUtils::GetPersistentStorageOriginInfo(GetRuntime(), oi));

		WSD_Database *db;
		RETURN_IF_ERROR(WSD_Database::GetInstance(oi.m_origin, m_name, oi.m_is_persistent, oi.m_context_id, &db));
		m_db = db;
	}

	return OpStatus::OK;
}

/* virtual */
DOM_Database::~DOM_Database()
{
	OP_DELETEA(const_cast<uni_char *>(m_origin));
	OP_DELETEA(const_cast<uni_char *>(m_name));
	OP_DELETEA(const_cast<uni_char *>(m_display_name));
	OP_DELETEA(const_cast<uni_char *>(m_expected_version));
	Out();

	// The transactions might still be attached during shutdown, when everything is forcefully gc'ed.
	m_transactions.RemoveAll();
}

/* static */ OP_STATUS DOM_Database::Make(DOM_DbManager *db_mgr,
										  DOM_Database *&db_object,
										  DOM_Runtime *runtime,
										  WSD_Database *database,
										  const uni_char *expected_version,
										  const uni_char *display_name,
										  OpFileLength author_size)
{
	OP_STATUS status = OpStatus::ERR_NO_MEMORY;
	const uni_char *origin = NULL, *name = NULL, *display_name_copy = NULL, *expected_version_copy = NULL;
	db_object = NULL;

	if (database->GetOrigin() && !(origin = UniSetNewStr(database->GetOrigin())) ||
		database->GetName() && !(name = UniSetNewStr(database->GetName())) ||
		display_name && !(display_name_copy = UniSetNewStr(display_name)) ||
		expected_version && !(expected_version_copy = UniSetNewStr(expected_version)))
		goto cleanup;

	status = DOMSetObjectRuntime((db_object = OP_NEW(DOM_Database, (database, author_size))),
			runtime, runtime->GetPrototype(DOM_Runtime::DATABASE_PROTOTYPE), "Database");

	if (OpStatus::IsError(status))
		goto cleanup;

	db_object->m_db_mgr = db_mgr;
	db_object->m_origin = origin;
	db_object->m_name = name;
	db_object->m_expected_version = expected_version_copy;
	db_object->m_display_name = display_name_copy;

	return OpStatus::OK;

cleanup:
	OP_DELETE(db_object);
	OP_DELETEA(const_cast<uni_char *>(origin));
	OP_DELETEA(const_cast<uni_char *>(name));
	OP_DELETEA(const_cast<uni_char *>(expected_version_copy));
	OP_DELETEA(const_cast<uni_char *>(display_name_copy));
	return status;
}

/*static*/
BOOL DOM_Database::IsValidCallbackObject(ES_Object *callback_object, DOM_Runtime *runtime)
{
	OP_ASSERT(callback_object != NULL);
	if (ES_Runtime::IsCallable(callback_object))
		return TRUE;
	else
	{
		ES_Value value;
		// ES_Runtime::GetName() doesn't handle getters, but it's acceptable for now.
		if (runtime->GetName(callback_object, UNI_L("handleEvent"), &value) == OpBoolean::IS_TRUE &&
			value.type == VALUE_OBJECT &&
			ES_Runtime::IsCallable(value.value.object))
		return TRUE;
	}

	return FALSE;
}

/*static*/
BOOL DOM_Database::ReadCallbackArgument(ES_Value* argv, int argc, int position, DOM_Runtime *runtime, ES_Object **dest_callback)
{
	if (argc <= position || argv[position].type == VALUE_UNDEFINED || argv[position].type == VALUE_NULL)
	{
		*dest_callback = NULL;
		return TRUE;
	}
	if (IsValidCallbackObject(argv[position].value.object, runtime))
	{
		*dest_callback = argv[position].value.object;
		return TRUE;
	}
	return FALSE;
}

/* virtual */ ES_GetState
DOM_Database::GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_version)
	{
		if (value)
		{
			GET_FAILED_IF_ERROR(EnsureDbIsInitialized());
			DOMSetString(value, m_db->GetVersion());
		}
		return GET_SUCCESS;
	}

	return GET_FAILED;
}

/* virtual */ ES_PutState
DOM_Database::PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime)
{
	if (property_name == OP_ATOM_version)
		// read only
		return PUT_SUCCESS;

	return PUT_FAILED;
}

void
DOM_Database::ClearExpectedVersion()
{
	OP_DELETEA(const_cast<uni_char *>(m_expected_version));
	m_expected_version = NULL;
}

/* virtual */ void DOM_Database::GCTrace()
{
	GCMark(m_db_mgr);

	DOM_SQLTransaction *current = static_cast<DOM_SQLTransaction *>(m_transactions.First());
	for (; current != NULL; current = static_cast<DOM_SQLTransaction *>(current->Suc()))
		GCMark(current);
}

/* static */ int
DOM_Database::CreateTransaction(DOM_Object *this_object, ES_Value *argv, int argc,
								ES_Value *return_value, DOM_Runtime *origining_runtime, int data)
{
	DOM_THIS_OBJECT(database, DOM_TYPE_DATABASE, DOM_Database);
	DOM_CHECK_ARGUMENTS("o|OO");

	ES_Object *transaction_cb, *error_cb, *void_cb;
	if (!ReadCallbackArgument(argv, argc, 0, origining_runtime, &transaction_cb) ||
		!ReadCallbackArgument(argv, argc, 1, origining_runtime, &error_cb) ||
		!ReadCallbackArgument(argv, argc, 2, origining_runtime, &void_cb))
		return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

	CALL_FAILED_IF_ERROR(database->EnsureDbIsInitialized());

	OP_ASSERT(data == 0 || data == 1);
	BOOL read_only = data == 1;

	DOM_SQLTransaction *trans;
	CALL_FAILED_IF_ERROR(DOM_SQLTransaction::Make(trans, database, read_only, database->m_expected_version));

	trans->Into(&database->m_transactions);
	trans->SetTransactionCb(transaction_cb);
	trans->SetErrorCb      (error_cb);
	trans->SetVoidCb       (void_cb);

	CALL_FAILED_IF_ERROR(trans->Run());

	return ES_FAILED;
}

/* static */ int
DOM_Database::changeVersion(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value *return_value, DOM_Runtime *origining_runtime)
{
	DOM_THIS_OBJECT(database, DOM_TYPE_DATABASE, DOM_Database);
	DOM_CHECK_ARGUMENTS("ss|OOO");

	CALL_FAILED_IF_ERROR(database->EnsureDbIsInitialized());

	ES_Object *transaction_cb, *error_cb, *void_cb;
	if (!ReadCallbackArgument(argv, argc, 2, origining_runtime, &transaction_cb) ||
		!ReadCallbackArgument(argv, argc, 3, origining_runtime, &error_cb) ||
		!ReadCallbackArgument(argv, argc, 4, origining_runtime, &void_cb))
		return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

	if (transaction_cb == NULL)
	{
		/**
		 * Optimize for the case of no transaction callback
		 * Just update the version directly
		 */
		if (database->m_db != NULL)
		{
			if (database->m_db->GetIndexEntry()->CompareVersion(argv[0].value.string))
			{
				CALL_FAILED_IF_ERROR(database->m_db->GetIndexEntry()->SetVersion(argv[1].value.string));

				OP_DELETEA(const_cast<uni_char *>(database->m_expected_version));
				database->m_expected_version = UniSetNewStr(argv[1].value.string);
				if (database->m_expected_version == NULL)
					CALL_FAILED_IF_ERROR(OpStatus::ERR_NO_MEMORY);

			}
#ifdef OPERA_CONSOLE
			else
			{
				TempBuffer message;
				CALL_FAILED_IF_ERROR(message.AppendFormat(UNI_L("Version '%s' did not match current version '%s' of database '%s'"),
					argv[0].value.string,
					database->m_db->GetVersion() ? database->m_db->GetVersion() : UNI_L(""),
					database->m_name ? database->m_name : UNI_L("")));
				DOM_PSUtils::PostExceptionToConsole(
					origining_runtime,
					GetCurrentThread(origining_runtime) != NULL ?
						GetCurrentThread(origining_runtime)->GetInfoString() : UNI_L("") ,
					message.GetStorage());
			}
#endif //OPERA_CONSOLE
		}
		return ES_FAILED;
	}

	DOM_SQLTransaction *trans;
	CALL_FAILED_IF_ERROR(DOM_SQLTransaction::Make(trans, database, FALSE, database->m_expected_version));

	CALL_FAILED_IF_ERROR(trans->SetChangeDatabaseVersion(argv[0].value.string, argv[1].value.string));
	trans->SetTransactionCb(transaction_cb);
	trans->SetErrorCb(error_cb);
	trans->SetVoidCb(void_cb);
	CALL_FAILED_IF_ERROR(trans->Run());

	return ES_FAILED;
}

#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_Database)
	DOM_FUNCTIONS_FUNCTION(DOM_Database, DOM_Database::changeVersion, "changeVersion", "ss|OOO-")
DOM_FUNCTIONS_END(DOM_Database)

DOM_FUNCTIONS_WITH_DATA_START(DOM_Database)
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Database, DOM_Database::CreateTransaction, 0, "transaction", "o|OO-")
	DOM_FUNCTIONS_WITH_DATA_FUNCTION(DOM_Database, DOM_Database::CreateTransaction, 1, "readTransaction", "o|OO-")
DOM_FUNCTIONS_WITH_DATA_END(DOM_Database)

#endif // DATABASE_STORAGE_SUPPORT
