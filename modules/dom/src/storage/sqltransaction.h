 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef DOMSQLTRANSACTION_H
#define DOMSQLTRANSACTION_H

#ifdef DATABASE_STORAGE_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/dom/src/storage/sqlresult.h"
#include "modules/ecmascript_utils/esasyncif.h"
#include "modules/doc/frm_doc.h"

class SqlValue;
class SqlTransaction;
class SqlStatementCallback;
class SqlStatementErrorCallback;
class SqlCommitCallback;
class DOM_ExecuteSqlFilterListener;
class DOM_Database;

class DOM_SQLTransaction : public DOM_BuiltInConstructor, public ES_AsyncCallback,
	public PS_ObjDelListener::ResourceShutdownCallback, public ListElement<DOM_SQLTransaction>
{
public:

	enum TransactionStatus {
		/**
		 * Object created
		 */
		TR_WAITING   = 0,
		/**
		 * Begin was issued
		 */
		TR_EXECUTING = 1,
		/**
		 * Everything completed OK
		 */
		TR_COMPLETED = 2,
		/**
		 * Transaction failed
		 */
		TR_FAILED    = 3
	};

private:

	AutoReleaseSqlTransactionPtr	m_sql_transaction;
	ES_Object*						m_transaction_callback;
	ES_Object*						m_error_callback;
	ES_Object*						m_void_callback;
	DOM_Database*					m_owner_db;
	List<SqlStatementCallback>		m_pending_statements_callbacks;
	SqlCommitCallback*				m_commit_callback;
	/**< List of running statements, used for gc:ing purposes and ability to cancel statements. */
	uni_char*						m_new_db_version;
	/**< The new version for the database, actually set if there are no errors.
	 *   See SetChangeDatabaseVersion() for a brief description of the version changing process. */
	TransactionStatus				m_status;
	/**< All statements and callbacks ran successfully, or an error occurred,
	 *   and this transaction shouldn't be used anymore since sql statements should only be
	 *   scheduled to run during the execution of a transaction callback, statement callback
	 *   or a statement error callback. */
	BOOL							m_bad_db_version;
	/**< Used to indicate that the current version of the db doesn't match when changing the
	 *   version of the database (the "preflight" operation).
	 *   See SetChangeDatabaseVersion() for a brief description of the version changing process. */
	unsigned m_waiting_for_asyncif_callback;
	/**< Used to count how many ES_Asyncif callbacks this transaction is waiting for, so
	 *   it only finishes the transaction when there are no ecmascript threads running. */

	DOM_SQLTransaction(DOM_Database *dom_db, BOOL read_only) :
		DOM_BuiltInConstructor(DOM_Runtime::SQLTRANSACTION_PROTOTYPE),
		m_sql_transaction(NULL),
		m_transaction_callback(NULL),
		m_error_callback(NULL),
		m_void_callback(NULL),
		m_owner_db(dom_db),
		m_commit_callback(NULL),
		m_new_db_version(NULL),
		m_status(TR_WAITING),
		m_bad_db_version(FALSE),
		m_waiting_for_asyncif_callback(0)
	{}

	friend class SqlStatementCallback;
	friend class SqlStatementErrorCallback;
	friend class SqlCommitCallback;
	friend class DOM_ExecuteSqlFilterListener;

public:

	OP_STATUS RunEsCallback(ES_Object *callback_object, int argc, ES_Value *argv, ES_AsyncCallback *callback);
	/**< Convenience function to call the transaction and statement ecmascript callbacks.
	 *
	 *   @param callback_object The callback object to run.
	 *   @param argc Number of arguments for the callback, or 0 if none.
	 *   @param argv Arguments for the callback, or NULL if none.
	 *   @param callback Asynchronous callback to run when the ES callback is done or NULL
	 *   				 if it's not necessary. If non-null SetWantExceptions() will be set to
	 *   				 be able to call the error callbacks with the proper storage errors.
	 */

	OP_STATUS RunEsCallback(ES_Object *callback_object) { return RunEsCallback(callback_object, 0, NULL, NULL); }
	/**< Like above, but don't pass arguments to the callback and don't run an async callback. */

	OP_STATUS CreateSqlValueList(ES_Runtime *runtime, SqlValueList *&value_list, ES_Object *array);
	/**< Assembles a SqlValueList from a corresponding ecmascript array to be used as
	 *   parameters for sql statements.
	 *
	 *   @param array          An ecmascript array object.
	 *   @param value_list     Out reference for the arguments list.
	 *   @returns OpStatus::OK if the list was created successfully.
	 *            OpStatus::ERR if the array is bogus.
	 *            OpStatus::ERR_OUT_OF_RANGE if the array is too big.
	 *            OpStatus::ERR_NO_MEMORY in case of OOM.
	 */

	BOOL HasFinished() const { return m_status > TR_EXECUTING; }
	/**< The transaction has completed all operations, either successfully or badly. */

	BOOL HasStarted() const { return m_status >= TR_EXECUTING; }
	/**< At least one statement has been issued. */

	BOOL HasFailed() const { return m_status == TR_FAILED; }

	BOOL HasSucceded() const { return m_status == TR_COMPLETED; }

	void SetState(TransactionStatus new_status);

	void Success();
	/**< Called when transaction was committed successfully. */

	OP_STATUS Error(DOM_SQLError::ErrorCode code, const uni_char *message);
	/**< Mark the transaction as done and call the error callback if provided.
	 *
	 *   @param code The error code.
	 *   @param message Optional. If not provided a default error message for the corresponding
	 *   				error code will be used.
	 */

	OP_STATUS ChangeDatabaseVersion();
	/**< Change the database version. This function should be run after the rest of the steps
	 *   in the database version change process ran successfully.
	 *   See SetChangeDatabaseVersion() for a brief description of the version changing process. */

	OP_STATUS StatementFinished();
	/**< Signal that a statement has finished. If all statements completed commit the transaction.
	 *
	 *   IMPORTANT: This method should be called for every statement after it completes whether
	 *   it completed successfully or not.
	 */

	virtual void HandleResourceShutdown();
	/**< From PS_ObjDelListener::ResourceShutdownCallback. This method will be called when
	 *   the transaction terminates so this dom object can be notified if the underlying
	 *   SqlTransaction objects gets invalidated and all pending statements can be cancelled.*/

	void SetDone(BOOL is_success);
	/**< Mark this transaction as done. No more sql statements will be run, even if the dom
	 *   transaction object is still live. If the transaction is marked as failed, any pending
	 *   statements are cancelled. */

	virtual ~DOM_SQLTransaction();

	static OP_STATUS	Make(DOM_SQLTransaction *&transaction, DOM_Database *db_object, BOOL read_only, const uni_char* expected_version);

	virtual BOOL		IsA(int type) { return type == DOM_TYPE_SQL_TRANSACTION || DOM_Object::IsA(type); }

	void	SetTransactionCb(ES_Object *object) { m_transaction_callback = object; }
	/**< The callback to be run with the transaction object.
	 *   See http://dev.w3.org/html5/webstorage/#transaction-steps */
	void	SetErrorCb(ES_Object *object) { m_error_callback = object; }
	/**< The callback to be run if there's an error while executing the transaction.
	 *   See http://dev.w3.org/html5/webstorage/#transaction-steps */
	void	SetVoidCb(ES_Object *object) { m_void_callback = object; }
	/**< The callback to be run after the transaction successfully completes.
	 *   See http://dev.w3.org/html5/webstorage/#transaction-steps */

	OP_STATUS SetChangeDatabaseVersion(const uni_char *old_version, const uni_char *new_version);
	/**< This is used for a transaction that is meant to be run when changing the database version.
	 *   The version will actually be changed if old_version matches the current version and if
	 *   the transaction callback completes successfully, otherwise the version won't be changed.
	 *
	 *   See http://dev.w3.org/html5/webstorage/#dom-database-changeversion for more details.
	 *
	 *   @param old_version The target version used for the change. The transaction callback will
	 *   					not run if this value doesn't match the current version of the database.
	 *   @param new_version The new version for the database.
	 *
	 */

	OP_STATUS Run();
	/**< After the object is created this method should be called to actually schedule the
	 *   transaction callbacks to run asynchronously. */

	OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result);

	OpFileLength GetPreferredSize() { return m_owner_db != NULL ? m_owner_db->GetPreferredSize() : 0; }

	const uni_char* GetDisplayName() { return m_owner_db != NULL ? m_owner_db->GetDisplayName() : NULL; }

	Window* GetWindow() { return GetFramesDocument() != NULL ? GetFramesDocument()->GetWindow() : NULL; }

	virtual void GCTrace();

	BOOL IsWaitingForAsyncifCallback() { return m_waiting_for_asyncif_callback > 0; }
	void OnAsyncifCallbackReply() { OP_ASSERT(m_waiting_for_asyncif_callback > 0); m_waiting_for_asyncif_callback--; }
	void OnAsyncifCallbackRegistered() { m_waiting_for_asyncif_callback++; }

	DOM_DECLARE_FUNCTION(executeSql);

	enum { FUNCTIONS_ARRAY_SIZE = 2 };
};

#endif // DATABASE_STORAGE_SUPPORT
#endif // DOMSQLTRANSACTION_H
