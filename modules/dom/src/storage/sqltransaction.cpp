/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#include "core/pch.h"

#ifdef DATABASE_STORAGE_SUPPORT

#include "modules/database/opdatabase.h"
#include "modules/doc/frm_doc.h"
#include "modules/dom/src/domenvironmentimpl.h"
#include "modules/dom/src/domglobaldata.h"
#include "modules/dom/src/storage/database.h"
#include "modules/dom/src/storage/sqlresult.h"
#include "modules/dom/src/storage/sqltransaction.h"
#include "modules/dom/src/storage/storageutils.h"

static DOM_SQLError::ErrorCode OpStatusToStorageError(OP_STATUS error)
{
	switch (OpStatus::GetIntValue(error))
	{
	case PS_Status::ERR_RSET_TOO_BIG:
		return DOM_SQLError::TOO_LARGE_ERR;

	case PS_Status::ERR_QUOTA_EXCEEDED:
		return DOM_SQLError::QUOTA_ERR;

	case PS_Status::ERR_READ_ONLY:
	case PS_Status::ERR_BAD_BIND_PARAMETERS:
	case PS_Status::ERR_AUTHORIZATION:
	case PS_Status::ERR_BAD_QUERY:
		return DOM_SQLError::SQL_SYNTAX_ERR;

	case PS_Status::ERR_CONSTRAINT_FAILED:
		return DOM_SQLError::CONSTRAINT_ERR;

	case PS_Status::ERR_VERSION_MISMATCH:
		return DOM_SQLError::VERSION_ERR;

	case PS_Status::ERR_TIMED_OUT:
		return DOM_SQLError::TIMEOUT_ERR;

	default:
		return DOM_SQLError::DATABASE_ERR;
	}
}

#define BREAK_IF_ERROR(expr) if (OpStatus::IsError(status = (expr))){ break; }

class SqlStatementCallback : public SqlStatement::Callback, public ListElement<SqlStatementCallback>
{
	SqlStatementCallback(DOM_SQLTransaction *transaction, BOOL is_begin,
			ES_Object *callback, ES_Object *error_callback) :
		m_transaction(transaction),
		m_callback(callback),
		m_error_callback(error_callback),
		m_is_begin(is_begin),
		m_has_called_handlecallback(TRUE)
	{
		OP_ASSERT(m_transaction);
	}

public:
	static SqlStatementCallback* Create(DOM_SQLTransaction *transaction, BOOL is_begin,
			ES_Object *callback, ES_Object *error_callback)
	{
		return OP_NEW(SqlStatementCallback, (transaction, is_begin, callback, error_callback));
	}

	virtual ~SqlStatementCallback()
	{
		Out();
		// If discard is called immediately, then it means that the database is maybe being deleted.
		if (!m_has_called_handlecallback)
		{
			m_has_called_handlecallback = TRUE;
			OP_ASSERT(m_transaction != NULL);
			if (m_transaction != NULL)
				OpStatus::Ignore(m_transaction->StatementFinished());
		}
	}

	virtual OP_STATUS HandleCallback(SqlResultSet *result_set)
	{
		m_has_called_handlecallback = TRUE;
		DOM_SQLResultSet *dom_result_set = NULL;
		ESCallback *es_callback = NULL;
		const uni_char *message = NULL;
		OP_STATUS status = OpStatus::OK;

		// If HasFinished() then transaction has been terminated before evaluating all statements.
		if (!m_transaction->HasFinished() && m_callback != NULL)
		{
			if (DOM_Database::IsValidCallbackObject(m_callback, m_transaction->GetRuntime()))
			{
				do {
					ES_Value arguments[2];
					DOM_Object::DOMSetObject(&arguments[0], m_transaction);

					if (result_set != NULL)
					{
						BREAK_IF_ERROR(DOM_SQLResultSet::Make(dom_result_set, result_set, m_transaction->GetRuntime()));
						DOM_Object::DOMSetObject(&arguments[1], dom_result_set);
					}

					if (m_is_begin)
					{
						BREAK_IF_ERROR(m_transaction->RunEsCallback(m_callback, 1, arguments, m_transaction));
					}
					else
					{
						es_callback = OP_NEW(ESCallback, (m_transaction));
						if (es_callback == NULL)
							BREAK_IF_ERROR(OpStatus::ERR_NO_MEMORY);

						BREAK_IF_ERROR(m_transaction->RunEsCallback(m_callback, 2, arguments, es_callback));
					}
					return OpStatus::OK;
				} while(0);
			}
#ifdef OPERA_CONSOLE
			else
			{
				DOM_PSUtils::PostExceptionToConsole(m_transaction->GetRuntime(), UNI_L("WebSQLDatabase delayed callback"), UNI_L("Invalid SQLTransactionCallback"));
			}
#endif // OPERA_CONSOLE
		}

		OP_DELETE(es_callback);
		if (dom_result_set)
			dom_result_set->ClearResultSet();
		OP_DELETE(result_set);

		m_has_called_handlecallback = FALSE;

		return OpStatus::IsError(status) ? m_transaction->Error(DOM_SQLError::UNKNOWN_ERR, message) : OpStatus::OK;
	};

	virtual OP_STATUS HandleError(OP_STATUS error, const uni_char* message, BOOL is_fatal)
	{
		m_has_called_handlecallback = TRUE;
		DOM_SQLError *dom_error;
		ESErrorCallback *es_callback = NULL;
		OP_STATUS status = OpStatus::OK;

#ifdef OPERA_CONSOLE
		//this posts errors to the console related to FILE IO problems
		DOM_PSUtils::PostWebDatabaseErrorToConsole(m_transaction->GetRuntime(),
			UNI_L("WebSQLDatabase delayed callback"), m_transaction->m_owner_db, error);
#endif //OPERA_CONSOLE

		if (m_is_begin || is_fatal)
		{
			status = m_transaction->Error(OpStatusToStorageError(error), message);
			OP_DELETEA(const_cast<uni_char*>(message));
			return status;
		}

		// If HasFinished() then transaction has been terminated before evaluating all statements.
		if (!m_transaction->HasFinished())
		{
			if (m_error_callback != NULL &&
				DOM_Database::IsValidCallbackObject(m_error_callback, m_transaction->GetRuntime()))
			{
				do {
					ES_Value arguments[2];

					BREAK_IF_ERROR(DOM_SQLError::Make(dom_error, OpStatusToStorageError(error), message, m_transaction->GetRuntime()));

					DOM_Object::DOMSetObject(&arguments[0], m_transaction);
					DOM_Object::DOMSetObject(&arguments[1], dom_error);

					es_callback = OP_NEW(ESErrorCallback, (m_transaction, error, message));
					if (es_callback == NULL)
						BREAK_IF_ERROR(OpStatus::ERR_NO_MEMORY);

					//ESErrorCallback took ownership of the string
					message = NULL	;

					BREAK_IF_ERROR(m_transaction->RunEsCallback(m_error_callback, 2, arguments, es_callback));
					return OpStatus::OK;
				} while(0);
			}
#ifdef OPERA_CONSOLE
			else
			{
				if (m_error_callback != NULL)
					// Callback is invalid
					DOM_PSUtils::PostExceptionToConsole(m_transaction->GetRuntime(), UNI_L("WebSQLDatabase delayed callback"), UNI_L("Invalid SQLTransactionErrorCallback"));
				DOM_PSUtils::PostExceptionToConsole(m_transaction->GetRuntime(), UNI_L("WebSQLDatabase delayed callback"), message);
			}
#endif //OPERA_CONSOLE
		}

		// If we get here, then the error callback failed to schedule to run, so we need to consider the operation failed.
		OP_DELETE(es_callback);

		// Error internally calls Discard which calls StatementFinished
		// but we need to unset m_has_called_handlecallback.
		m_has_called_handlecallback = FALSE;
		status = m_transaction->Error(OpStatusToStorageError(error), message);
		OP_DELETEA(const_cast<uni_char*>(message));
		return status;
	};

	virtual Window*	GetWindow()
	{
		return m_transaction->GetFramesDocument() ? m_transaction->GetFramesDocument()->GetWindow() : NULL;
	}

	virtual void Discard()
	{
		OP_DELETE(this);
	}

	FramesDocument* GetFramesDocument() { return m_transaction != NULL ? m_transaction->GetFramesDocument() : NULL; }
	virtual OpFileLength GetPreferredSize() { return m_transaction != NULL ? m_transaction->GetPreferredSize() : 0; }
	virtual const uni_char* GetDisplayName() { return m_transaction != NULL ? m_transaction->GetDisplayName() : NULL; }
	virtual BOOL RequiresCaching() const{ return TRUE; }

private:
	class ESCallback : public ES_AsyncCallback
	{
	public:
		ESCallback(DOM_SQLTransaction *transaction) : m_transaction(transaction) {}

		OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
		{
			m_transaction->OnAsyncifCallbackReply();

			if (status != ES_ASYNC_SUCCESS)
				m_transaction->Error(DOM_SQLError::UNKNOWN_ERR, UNI_L("SQLTransactionCallback returned error"));

			OP_STATUS op_status = m_transaction->StatementFinished();

			OP_DELETE(this);

			return op_status;
		};

	private:
		DOM_SQLTransaction *m_transaction;
	};

	class ESErrorCallback : public ES_AsyncCallback
	{
	public:
		ESErrorCallback(DOM_SQLTransaction *transaction, OP_STATUS last_error, const uni_char* error_message) :
			m_transaction(transaction),
			m_last_error(last_error),
			m_last_error_msg(error_message){}
		~ESErrorCallback(){ OP_DELETEA(const_cast<uni_char*>(m_last_error_msg)); }

		OP_STATUS HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
		{
			m_transaction->OnAsyncifCallbackReply();

			if (status == ES_ASYNC_SUCCESS)
			{
				// If the callback returns false ignore the error and carry on
				if (result.type != VALUE_BOOLEAN || result.value.boolean)
					m_transaction->Error(OpStatusToStorageError(m_last_error), m_last_error_msg);
			}
			else
				m_transaction->Error(DOM_SQLError::UNKNOWN_ERR, UNI_L("SQLTransactionErrorCallback returned error"));

			OP_STATUS op_status = m_transaction->StatementFinished();

			OP_DELETE(this);

			return op_status;
		};

	private:
		DOM_SQLTransaction *m_transaction;
		OP_STATUS m_last_error;
		const uni_char* m_last_error_msg;
	};

	DOM_SQLTransaction * m_transaction;
public:
	ES_Object *m_callback;
	ES_Object *m_error_callback;
	BOOL m_is_begin;
	BOOL m_has_called_handlecallback;
};

class SqlCommitCallback : public SqlStatement::Callback
{
public:
	SqlCommitCallback() : m_transaction(NULL) {}
	virtual ~SqlCommitCallback()
	{
		if (DOM_SQLTransaction *transaction = m_transaction)
		{
			ClearTransaction();
			if (!transaction->HasFinished())
				OpStatus::Ignore(transaction->Error(OpStatusToStorageError(OpStatus::ERR), NULL));
		}
	}
	virtual OP_STATUS HandleCallback(SqlResultSet *result_set)
	{
		OP_ASSERT(m_transaction != NULL);

		OP_DELETEA(result_set);

		DOM_SQLTransaction *transaction = m_transaction;
		ClearTransaction();
		transaction->Success();

		return OpStatus::OK;
	}

	virtual OP_STATUS HandleError(OP_STATUS error, const uni_char* error_message, BOOL is_fatal)
	{
		OP_ASSERT(m_transaction != NULL);

		// DOM_SqlTransaction::Error() cancels all pending statements, and that includes deleting the
		// commit callback so we set to NULL here to prevent a double deletion.
		DOM_SQLTransaction *transaction = m_transaction;
		ClearTransaction();

		error = transaction->Error(OpStatusToStorageError(error), error_message);
		OP_DELETEA(error_message);

		return error;
	}

	void ClearTransaction()
	{
		m_transaction->m_commit_callback = NULL;
		m_transaction = NULL;
	}
	virtual Window* GetWindow()
	{
		return GetFramesDocument() ? GetFramesDocument()->GetWindow() : NULL;
	}

	virtual void Discard()
	{
		OP_DELETE(this);
	}
	FramesDocument* GetFramesDocument()
	{
		return m_transaction != NULL ? m_transaction->GetFramesDocument() : NULL;
	}
	virtual OpFileLength GetPreferredSize()
	{
		return m_transaction != NULL ? m_transaction->GetPreferredSize() : 0;
	}
	virtual const uni_char* GetDisplayName()
	{
		return m_transaction != NULL ? m_transaction->GetDisplayName() : NULL;
	}

	DOM_SQLTransaction* m_transaction;
	BOOL m_has_called_callback;
};

DOM_SQLTransaction::~DOM_SQLTransaction()
{
	// The object might be gc'ed prematurely, like after a page reload,
	// so this needs to make sure everything is properly cleaned up.
	if (!HasFinished())
		SetDone(FALSE);

	OP_ASSERT(HasFinished());
	OP_ASSERT(m_sql_transaction == NULL);
	OP_ASSERT(m_pending_statements_callbacks.Empty());

	Out();
}

/*static*/ OP_STATUS DOM_SQLTransaction::Make(DOM_SQLTransaction *&transaction, DOM_Database *db_object, BOOL read_only, const uni_char* expected_version)
{
	DOM_Runtime *runtime = db_object->GetRuntime();
	RETURN_OOM_IF_NULL(transaction = OP_NEW(DOM_SQLTransaction, (db_object, read_only)));

	transaction->m_sql_transaction = db_object->GetDb()->CreateTransactionAsync(read_only, expected_version);
	if (transaction->m_sql_transaction == NULL)
	{
		OP_DELETE(transaction);
		transaction = NULL;
		return OpStatus::ERR_NO_MEMORY;
	}

	RETURN_IF_ERROR(DOMSetObjectRuntime(transaction, runtime,
			runtime->GetPrototype(DOM_Runtime::SQLTRANSACTION_PROTOTYPE), "SQLTransaction"));

	return OpStatus::OK;
}

/*virtual*/
void DOM_SQLTransaction::HandleResourceShutdown()
{
	if (!HasFinished())
	{
		if (g_opera->running) // avoid executing stuff during shutdown
			OpStatus::Ignore(Error(DOM_SQLError::UNKNOWN_ERR, UNI_L("Database was shut down")));
		else
			SetDone(FALSE);
	}
}

OP_STATUS DOM_SQLTransaction::Run()
{
	if (m_bad_db_version)
		return Error(DOM_SQLError::VERSION_ERR, NULL);

	if (m_transaction_callback == NULL)
		return Error(DOM_SQLError::UNKNOWN_ERR, UNI_L("SQLTransactionCallback required"));

	OP_ASSERT(!HasStarted());

	SqlStatementCallback *sql_callback = SqlStatementCallback::Create(this,
			TRUE, m_transaction_callback, m_error_callback);
	if (sql_callback == NULL)
		return ES_NO_MEMORY;
	OpAutoPtr<SqlStatementCallback> sql_callback_ptr(sql_callback);

	OP_STATUS status = m_sql_transaction->Begin(sql_callback);
	if (OpStatus::IsError(status))
	{
		OpStatus::Ignore(Error(OpStatusToStorageError(status), UNI_L("Error during transaction creation")));
		return status;
	}
	SetState(TR_EXECUTING);

	sql_callback->Into(&m_pending_statements_callbacks);
	sql_callback->m_has_called_handlecallback = FALSE;
	sql_callback_ptr.release();

	return OpStatus::OK;
}

OP_STATUS DOM_SQLTransaction::SetChangeDatabaseVersion(const uni_char *old_version, const uni_char *new_version)
{
	if (!m_sql_transaction->GetDatabase()->GetIndexEntry()->CompareVersion(old_version))
	{
		m_bad_db_version = TRUE;
		return OpStatus::OK;
	}

	OP_ASSERT(m_new_db_version == NULL);
	m_new_db_version = UniSetNewStr(new_version);
	RETURN_OOM_IF_NULL(m_new_db_version);

	return OpStatus::OK;
}

OP_STATUS DOM_SQLTransaction::ChangeDatabaseVersion()
{
	OP_STATUS status = OpStatus::OK;
	if (m_new_db_version != NULL)
	{
		if (m_sql_transaction != NULL)
		{
			status = m_sql_transaction->GetDatabase()->SetVersion(m_new_db_version);

			if (OpStatus::IsError(status))
				Error(DOM_SQLError::UNKNOWN_ERR, UNI_L("Internal error while changing version"));
		}
		OP_DELETEA(m_new_db_version);
		m_new_db_version = NULL;
	}
	return status;
}

/* static */ int
DOM_SQLTransaction::executeSql(DOM_Object *this_object, ES_Value *argv, int argc, ES_Value* return_value, DOM_Runtime* origining_runtime)
{
	DOM_THIS_OBJECT(transaction, DOM_TYPE_SQL_TRANSACTION, DOM_SQLTransaction);
	DOM_CHECK_ARGUMENTS("s|OOO");

	if (transaction->m_sql_transaction == NULL || transaction->HasFinished())
		// Ignore everything after the transaction is done or an error occurred.
		return DOM_CALL_DOMEXCEPTION(INVALID_STATE_ERR);

	ES_Object *query_arguments  = 1 < argc && argv[1].type == VALUE_OBJECT ? argv[1].value.object : NULL;
	ES_Object *query_success_cb, *query_error_cb = NULL;

	if (!DOM_Database::ReadCallbackArgument(argv, argc, 2, origining_runtime, &query_success_cb) ||
		!DOM_Database::ReadCallbackArgument(argv, argc, 3, origining_runtime, &query_error_cb))
		return DOM_CALL_INTERNALEXCEPTION(WRONG_ARGUMENTS_ERR);

	SqlStatementCallback *sql_callback = SqlStatementCallback::Create(transaction,
			FALSE, query_success_cb, query_error_cb);
	if (sql_callback == NULL)
		return ES_NO_MEMORY;
	OpAutoPtr<SqlStatementCallback> sql_callback_ptr(sql_callback);

	unsigned sql_length = argv[0].GetStringLength();
	uni_char *sql = OP_NEWA(uni_char, sql_length);
	if (sql == NULL)
		return ES_NO_MEMORY;
	op_memcpy(sql, argv[0].value.string, UNICODE_SIZE(sql_length));
	OpHeapArrayAnchor<uni_char> sql_ptr(sql);

	SqlValueList *value_list = NULL;

	if (query_arguments != NULL)
	{
		OP_STATUS status = transaction->CreateSqlValueList(origining_runtime, value_list, query_arguments);
		if (status == OpStatus::ERR)
			status = origining_runtime->CreateNativeErrorObject(&return_value->value.object, ES_Native_TypeError, UNI_L("object argument expected"));
		else if (status == OpStatus::ERR_OUT_OF_RANGE)
		{
			OpString message;
			if (OpStatus::IsSuccess(status = message.AppendFormat("Too many query arguments, maximum allowed is %u", DATABASE_INTERNAL_MAX_QUERY_BIND_VARIABLES)))
				status = origining_runtime->CreateNativeErrorObject(&return_value->value.object, ES_Native_RangeError, message.CStr());
		}
		else
			goto execute_sql_call;

		if (OpStatus::IsError(status))
			return ES_NO_MEMORY;

		return_value->type = VALUE_OBJECT;
		return ES_EXCEPTION;
	}
execute_sql_call:

	transaction->m_sql_transaction->SetSqlValidator(SqlValidator::UNTRUSTED);

	sql_ptr.release();

	SqlText sql_text(SqlText::HEAP_16BIT, sql, sql_length);
	CALL_FAILED_IF_ERROR(transaction->m_sql_transaction->ExecuteSql(sql_text, value_list, sql_callback));

	sql_callback->Into(&transaction->m_pending_statements_callbacks);
	sql_callback->m_has_called_handlecallback = FALSE;
	sql_callback_ptr.release();

	return ES_FAILED;
}

OP_STATUS DOM_SQLTransaction::RunEsCallback(ES_Object *callback_object, int argc, ES_Value *argv, ES_AsyncCallback *callback)
{
	/* During shutdown, the Discard() method might be called in the
	 * query callbacks which in turn might call this function, so
	 * skip this part*/
	if (!g_opera->running)
		return OpStatus::ERR;

	OP_ASSERT(callback_object);
	OP_ASSERT(DOM_Database::IsValidCallbackObject(callback_object, GetRuntime()));

	ES_AsyncInterface *asyncif = GetRuntime()->GetEnvironment()->GetAsyncInterface();

	OP_STATUS status;
	if (ES_Runtime::IsCallable(callback_object))
		status = asyncif->CallFunction(callback_object, NULL, argc, argv, callback);
	else
		status = asyncif->CallMethod(callback_object, UNI_L("handleEvent"), argc, argv, callback);

	if (OpStatus::IsSuccess(status))
		OnAsyncifCallbackRegistered();

	return status;
}

OP_STATUS DOM_SQLTransaction::HandleCallback(ES_AsyncOperation operation, ES_AsyncStatus status, const ES_Value &result)
{
	OnAsyncifCallbackReply();

	// The transaction object itself is the listener for the main transaction callback, the 1st one.
	if (status == ES_ASYNC_SUCCESS)
		return StatementFinished();
	else
		return Error(DOM_SQLError::UNKNOWN_ERR, NULL);
}

void DOM_SQLTransaction::SetState(TransactionStatus new_status)
{
	OP_ASSERT(!HasFinished());
	OP_ASSERT(m_status < new_status);

	m_status = new_status;
}

void DOM_SQLTransaction::Success()
{
	OP_ASSERT(!HasFinished());
	OP_ASSERT(!IsWaitingForAsyncifCallback());
	OP_ASSERT(m_pending_statements_callbacks.Empty());

	OP_STATUS status = OpStatus::OK;

	if (m_new_db_version != NULL)
		status = ChangeDatabaseVersion();

	if (m_void_callback != NULL && !DOM_Database::IsValidCallbackObject(m_void_callback, GetRuntime()))
	{
#ifdef OPERA_CONSOLE
		DOM_PSUtils::PostExceptionToConsole(GetRuntime(), UNI_L("WebSQLDatabase delayed callback"), UNI_L("Invalid SQLVoidCallback"));
#endif //OPERA_CONSOLE
	}
	else if (m_void_callback != NULL && OpStatus::IsSuccess(status))
		RunEsCallback(m_void_callback);

	SetDone(TRUE);
}

OP_STATUS DOM_SQLTransaction::Error(DOM_SQLError::ErrorCode code, const uni_char *message)
{
	if (m_new_db_version != NULL)
	{
		OP_DELETEA(m_new_db_version);
		m_new_db_version = NULL;
	}

	if (HasFinished())
	{
		OP_ASSERT(m_sql_transaction == NULL);
		OP_ASSERT(m_pending_statements_callbacks.Empty());
		return OpStatus::OK;
	}

	SetDone(FALSE);

	// don't call ecmascript during shutdown
	if (!g_opera->running)
		return OpStatus::OK;

	if (m_error_callback == NULL || !DOM_Database::IsValidCallbackObject(m_error_callback, GetRuntime()))
	{
#ifdef OPERA_CONSOLE
		DOM_PSUtils::PostExceptionToConsole(GetRuntime(),
			UNI_L("WebSQLDatabase delayed callback"), message ? message : DOM_SQLError::GetErrorDescription(code));
#endif //OPERA_CONSOLE
		return OpStatus::OK;
	}

	DOM_SQLError *error;

	RETURN_IF_ERROR(DOM_SQLError::Make(error, code, message, GetRuntime()));

	ES_Value arguments[1];
	DOMSetObject(&arguments[0], error);

	OP_STATUS status = RunEsCallback(m_error_callback, 1, arguments, NULL);
#ifdef OPERA_CONSOLE
	if (OpStatus::IsError(status))
		DOM_PSUtils::PostExceptionToConsole(GetRuntime(),
			UNI_L("WebSQLDatabase delayed callback"), message ? message : DOM_SQLError::GetErrorDescription(code));
#endif //OPERA_CONSOLE
	return status;
}

void DOM_SQLTransaction::SetDone(BOOL was_success)
{
	OP_ASSERT(!HasFinished());
	OP_ASSERT(m_sql_transaction != NULL);

	SetState(was_success ? TR_COMPLETED : TR_FAILED);
	if (!was_success)
		m_sql_transaction->CancelAllStatements();

	// Must have completed all statements.
	OP_ASSERT(m_pending_statements_callbacks.Empty());
	OP_ASSERT(m_commit_callback == NULL);

	m_sql_transaction->RemoveShutdownCallback(this);
	m_sql_transaction = NULL;

	// The transaction is removed from the parent DOM_Database, so it can be collected.
	m_owner_db = NULL;
	Out();
}

OP_STATUS DOM_SQLTransaction::StatementFinished()
{
	if (m_pending_statements_callbacks.Empty() && !IsWaitingForAsyncifCallback() && !HasFinished())
	{
		if (m_sql_transaction != NULL && HasStarted())
		{
			if (!g_opera->running)
			{
				return Error(DOM_SQLError::DATABASE_ERR, UNI_L("Database was shut down"));
			}

			// If this assert triggers, then StatementFinished() has been called too much.
			OP_ASSERT(m_commit_callback == NULL);

			SqlCommitCallback *sql_callback = OP_NEW(SqlCommitCallback, ());
			if (!sql_callback || OpStatus::IsError(m_sql_transaction->Commit(sql_callback)))
			{
				OP_DELETE(sql_callback);
				return Error(DOM_SQLError::DATABASE_ERR, UNI_L("Error while committing transaction"));
			}
			sql_callback->m_transaction = this;
			m_commit_callback = sql_callback;

			return OpStatus::OK;
		}

		Success();
	}
	return OpStatus::OK;
}

/* static */ OP_STATUS DOM_SQLTransaction::CreateSqlValueList(ES_Runtime *runtime, SqlValueList *&value_list, ES_Object *array)
{
	if (runtime->IsStringObject(array) || runtime->IsFunctionObject(array))
		return OpStatus::ERR;

	ES_Value value;
	unsigned array_length;
	if (!DOMGetArrayLength(array, array_length))
		return OpStatus::ERR;

	if (array_length == 0)
	{
		value_list = NULL;
		return OpStatus::OK;
	}
	else if (DATABASE_INTERNAL_MAX_QUERY_BIND_VARIABLES < array_length)
		return OpStatus::ERR_OUT_OF_RANGE;

	OpAutoPtr<SqlValueList> value_list_ptr(SqlValueList::Make(array_length));
	RETURN_OOM_IF_NULL(value_list_ptr.get());

	for (unsigned i = 0; i < array_length; i++)
	{
		if (runtime->GetIndex(array, i, &value) == OpBoolean::IS_TRUE)
		{
			switch(value.type)
			{
			case VALUE_BOOLEAN:
				value_list_ptr->values[i].SetBoolean(value.value.boolean);
				break;
			case VALUE_NUMBER:
				value_list_ptr->values[i].SetDouble(value.value.number);
				break;
			case VALUE_STRING:
				RETURN_IF_ERROR(value_list_ptr->values[i].SetString(value.value.string, value.GetStringLength()));
				break;
			case VALUE_STRING_WITH_LENGTH:
				RETURN_IF_ERROR(value_list_ptr->values[i].SetString(value.value.string_with_length->string, value.value.string_with_length->length));
				break;
#ifdef DEBUG_ENABLE_OPASSERT
			case VALUE_UNDEFINED:
			case VALUE_NULL:
				break;

			case VALUE_OBJECT:
				OP_ASSERT(!"Object should have been converted to a native type");
				break;

			default:
				OP_ASSERT(!"Missed an ES type");
				break;
#endif // DEBUG_ENABLE_OPASSERT
			}
		}
	}

	value_list = value_list_ptr.release();

	return OpStatus::OK;
}

/* virtual */ void DOM_SQLTransaction::GCTrace()
{
	if (m_transaction_callback)
		GCMark(m_transaction_callback);

	if (m_error_callback)
		GCMark(m_error_callback);

	if (m_void_callback)
		GCMark(m_void_callback);

	GCMark(m_owner_db);

	for (SqlStatementCallback *current = m_pending_statements_callbacks.First(); current ; current = current->Suc())
	{
		GCMark(current->m_callback);
		GCMark(current->m_error_callback);
	}
}


#include "modules/dom/src/domglobaldata.h"

DOM_FUNCTIONS_START(DOM_SQLTransaction)
	DOM_FUNCTIONS_FUNCTION(DOM_SQLTransaction, DOM_SQLTransaction::executeSql, "executeSql", "s?(#Function|#String|[p])OO-")
DOM_FUNCTIONS_END(DOM_SQLTransaction)

#endif // DATABASE_STORAGE_SUPPORT
