/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef _MODULES_DOM_STORAGE_DOMDATABASE_H_
#define _MODULES_DOM_STORAGE_DOMDATABASE_H_

#ifdef DATABASE_STORAGE_SUPPORT

#include "modules/database/opdatabase.h"
#include "modules/dom/src/domobj.h"

class WSD_Database;
class DOM_DbManager;
class JS_Window;
class DOM_SQLTransaction;

PS_ObjectTypes::PS_ObjectType GetDatabaseTypeForWindow(Window* window);

class DOM_Database : public DOM_BuiltInConstructor, public ListElement<DOM_Database>
{
private:

	List<DOM_SQLTransaction>	m_transactions;
	AutoReleaseWSDDatabasePtr	m_db;
	const uni_char*				m_origin;
	const uni_char*				m_expected_version;
	const uni_char*				m_name;
	const uni_char*				m_display_name;
	DOM_DbManager*				m_db_mgr;
	OpFileLength				m_author_size; /**< Estimated/expected database size, provided by the author. */
	BOOL 						m_has_posted_err_msg; //tells if PostWebDatabaseErrorToConsole was called for IO problems on this database

	DOM_Database(WSD_Database* db, OpFileLength author_size);
	OP_STATUS EnsureDbIsInitialized();

public:
	/**
	 * Checks if object is a valid callback which can be used by the
	 * Database.transaction, Database.readTransaction, SqlTransition.executeSql
	 * and SqlTransition.changeVersion methods.
	 * A valid callback is either a function, or an object with a handleEvent
	 * property which is a function.
	 *
	 * @param callback_object  Object to check
	 * @param runtime          Runtime of the caller
	 * @return TRUE if this object is a good callback, which happens if:
	 *  - the object is callable
	 *  - the object has a handleEvent property which is callable.
	 */
	static BOOL IsValidCallbackObject(ES_Object *callback_object, DOM_Runtime *runtime);

	/**
	 * Utility to handle and return a callback object at the given position
	 * in the arguments array passed to Database.transaction, Database.readTransaction,
	 * SqlTransition.executeSql and SqlTransition.changeVersion.
	 *
	 * If the given position is outside the arguments array or if the value
	 * at the position is undefined or null, dest_callback will be assigned
	 * NULL meaning there was no callback object and the call will return TRUE.
	 *
	 * Else if the value at the position is a valid callback object as validated by
	 * IsValidCallbackObject(), dest_callback will be assigned that object and
	 * TRUE returned.
	 *
	 * Else FALSE will be returned meaning that the value at the given position is
	 * not null, undefined or a valid callback, hence a bad argument.
	 */
	static BOOL ReadCallbackArgument(ES_Value* argv, int argc, int position, DOM_Runtime *runtime, ES_Object **dest_callback);

	/**
	 * Frees used resources, and terminates pending transactions
	 * because their results will not be used.
	 */
	static void BeforeUnload(DOM_EnvironmentImpl*);

	virtual 			~DOM_Database();

	static OP_STATUS	Make(DOM_DbManager* m_db_mgr,
							 DOM_Database *&db_object,
							 DOM_Runtime *runtime,
							 WSD_Database *database,
							 const uni_char *expected_version,
							 const uni_char *display_name,
							 OpFileLength author_size);

	virtual ES_GetState GetName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);
	virtual ES_PutState PutName(OpAtom property_name, ES_Value *value, ES_Runtime *origining_runtime);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_DATABASE || DOM_Object::IsA(type); }

	BOOL HasOpenTransactions() const { return !m_transactions.Empty(); }

	virtual void 		GCTrace();

	WSD_Database *GetDb() { return m_db.operator->(); }
	BOOL HasPostedErrMsg() const { return m_has_posted_err_msg; }
	void SetHasPostedErrMsg(BOOL v) { m_has_posted_err_msg = v; }
	OpFileLength GetPreferredSize() { return m_author_size; }
	const uni_char* GetDisplayName() { return m_display_name; }
	const uni_char* GetRealName() { return m_name; }

	void ClearExpectedVersion();

	DOM_DECLARE_FUNCTION(changeVersion);
	enum { FUNCTIONS_ARRAY_SIZE = 2 };

	DOM_DECLARE_FUNCTION_WITH_DATA(CreateTransaction);
	enum { FUNCTIONS_WITH_DATA_ARRAY_SIZE = 3 };
};

class DOM_DbManager : public DOM_Object
{
private:

	List<DOM_Database>	m_db_list;

	DOM_Database*	FindDbObject(WSD_Database *db) const;

	OP_STATUS		InsertDbObject(DOM_Database *db_object);

public:
	virtual ~DOM_DbManager();

	static OP_STATUS	Make(DOM_DbManager *&manager, DOM_Runtime *runtime);

	/**  Get or create a dom database object.
	 *   Returns OpStatus::ERR if the expected version doesn't match the actual version of the db.
	 *
	 *   @param[out] database The database object.
	 *   @param db_name The database name.
	 *   @param version The expected database version.
	 *   @param display_name A user friendly display name for the database.
	 *   @param author_size Suggested/expected database size provided by the author.
	 */
	OP_STATUS	FindOrCreateDb(DOM_Database *&database,
							 const uni_char *db_name,
							 const uni_char *version,
							 const uni_char *display_name,
							 OpFileLength author_size);

	DOM_Database* FindDbByName(const uni_char* name) const;

	DOM_Database* GetFirstDb() const { return m_db_list.First(); }

	static DOM_DbManager* LookupManagerForWindow(DOM_Object* win_obj);

	void ClearExpectedVersions(const uni_char* db_name);

	virtual void 		GCTrace();
};

#endif // DATABASE_STORAGE_SUPPORT
#endif // _MODULES_DOM_STORAGE_DOMDATABASE_H_
