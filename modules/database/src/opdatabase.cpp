/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef SUPPORT_DATABASE_INTERNAL

#include "modules/database/opdatabasemanager.h"
#include "modules/database/opdatabase.h"
#include "modules/dochand/win.h"
#include "modules/hardcore/mh/messages.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/sqlite/sqlite3.h"
#include "modules/windowcommander/src/WindowCommander.h"

#define SQL_STR_PAIR(s) s,(sizeof(s)-1)

#define IS_SQLITE_STATUS_OK(s) ((s) == SQLITE_OK || (s) == SQLITE_ROW || (s) == SQLITE_DONE)

/**********************
 * SqlStatement
***********************/
/* static */
SqlStatement *
SqlStatement::Create(SqlTransaction *transaction, const SqlText &sql_text,
		SqlValueList *parameters, BOOL should_not_validate, BOOL no_version_check, Callback *callback)
{
	return OP_NEW(SqlStatement, (transaction, sql_text,
			parameters, should_not_validate, no_version_check, callback));
}

SqlStatement::~SqlStatement()
{
	//must be deleted in order
	OP_ASSERT(Pred() == NULL);

	DB_DBG_2(("%p: SqlTransaction::SqlStatement::~SqlStatement(%p): %s\n", m_transaction, this, DB_DBG_QUERY(m_sql)))

	DiscardCallback();
	TerminateStatementAndResultSet();
	Out();
	if (m_parameters != NULL)
		m_parameters->Delete();

	if (m_sql.m_sql_type == SqlText::HEAP_8BIT)
		OP_DELETEA(m_sql.m_sql8);
	else if (m_sql.m_sql_type == SqlText::HEAP_16BIT)
		OP_DELETEA(m_sql.m_sql16);
}

/**
 * Discards the callback used in this statement
 */
void
SqlStatement::DiscardCallback()
{
	if (m_callback != NULL) {
		Callback *callback = m_callback;
		m_callback = NULL;
		callback->Discard();
	}
}

BOOL
SqlStatement::MustRestartTransaction() const
{
	return m_transaction->m_sqlite_aborted && IsErrorRecoverable();
}

void
SqlStatement::Reset()
{
	TerminateStatementAndResultSet();
	m_sw.Reset();
	//SetState triggers assert
	m_state = BEFORE_EXECUTION;
	m_number_of_yields = 0;
	m_has_called_callbacks = FALSE;
	m_is_result_success = TRUE;
	m_result_set = NULL;
}

BOOL
SqlStatement::IsStatementCompleted() const
{
	if (m_state == SqlStatement::CANCELLED)
		return TRUE;

	if (m_state != SqlStatement::AFTER_EXECUTION)
		return FALSE;

	if (!m_is_result_success || //error callback, nothing to wait for
		m_result_set == NULL || //no result set, so there is no pending sqlite3_stmt
		!m_result_set->IsIterable() || //not iterable, so the sqlite3_stmt object has been finished
		m_result_set->IsCachingEnabled() || //cacheable, so the sqlite3_stmt object has been flushed to memory and destroyed
		m_result_set->IsClosed())//nor iterable, but closed so the sqlite3_stmt object has been deleted
	{
		return TRUE;
	}
	return FALSE;
}

void
SqlStatement::TerminateStatementAndResultSet()
{
	if (m_state == SqlStatement::AFTER_EXECUTION)
	{
		if (m_is_result_success)
		{
			if (!m_has_called_callbacks)
			{
				OP_DELETE(m_result_set);
				m_result_set = NULL;
			}
			else if (m_result_set != NULL)
			{
				OP_ASSERT(m_result_set->IsClosed());
				m_result_set->m_sql_statement = NULL;
				m_result_set = NULL;
			}
		}
		else
		{
			OP_DELETEA(m_result_error_data.m_result_error_message);
			m_result_error_data.m_result_error_message = NULL;
		}
	}
	else
	{
		if (m_is_result_success)
		{
			//while this object owns the sqlite3_stmt object, there is no resultset
			//else the m_stmt object has been passed to the result set and the later
			//became the owner
			OP_ASSERT((m_stmt != NULL && m_result_set == NULL) || m_stmt == NULL);
			if (m_stmt != NULL)
			{
				sqlite3_finalize(m_stmt);
				if (m_transaction != NULL &&
					m_transaction->HasRollbackSegment() &&
					sqlite3_get_autocommit(m_transaction->GetDatabaseHandle()))
				{
					DB_DBG(("  - %p sqlite aborted\n", this))
					m_transaction->m_sqlite_aborted = TRUE;
				}
				m_stmt = NULL;
			}

			if (m_result_set != NULL)
			{
				OP_DELETE(m_result_set);
				m_result_set = NULL;
			}
		}
		else
		{
			OP_DELETEA(m_result_error_data.m_result_error_message);
			m_result_error_data.m_result_error_message = NULL;
		}
	}
}

void
SqlStatement::RaiseError()
{
	if (!IsRaisable())
		return;

	MessageHandler* mh = GetWindow() != NULL ? GetWindow()->GetMessageHandler() : NULL;

	if (mh == NULL)
		mh = m_transaction->GetDatabase()->GetMessageHandler();

	if (mh == NULL)
		return;

	if (m_result_error_data.m_result_error == OpStatus::ERR_NO_MEMORY)
		mh->PostOOMCondition(TRUE);
	else if (m_result_error_data.m_result_error == OpStatus::ERR_SOFT_NO_MEMORY)
		mh->PostOOMCondition(FALSE);
	else if (m_result_error_data.m_result_error == OpStatus::ERR_NO_DISK)
		mh->PostOODCondition();
}

void SqlStatement::OnResultSetClosed(BOOL sqlite_aborted)
{
	m_transaction->m_sqlite_aborted = sqlite_aborted;
	m_transaction->GetDatabase()->ScheduleTransactionExecute(0);
}

/*static*/
BOOL
SqlStatement::IsStatement(const uni_char* sql, unsigned sql_length, const uni_char* statement_type, unsigned statement_type_length)
{
	OP_ASSERT(sql != NULL);
	OP_ASSERT(statement_type != NULL);
	if (sql_length == (unsigned)-1)
		sql_length = uni_strlen(sql);
	while(*sql && !op_isalnum(*sql))
	{
		sql++;
		sql_length--;
	}
	return *sql && sql_length >= statement_type_length &&
		!op_isalnum(sql[statement_type_length]) && (uni_strnicmp(sql, statement_type, statement_type_length) == 0);
}

/*static*/
BOOL
SqlStatement::IsStatement(const char* sql, unsigned sql_length, const char* statement_type, unsigned statement_type_length)
{
	OP_ASSERT(sql != NULL);
	OP_ASSERT(statement_type != NULL);
	if (sql_length == (unsigned)-1)
		sql_length = op_strlen(sql);
	while(*sql && !op_isalnum(*sql))
	{
		sql++;
		sql_length--;
	}
	return *sql && sql_length >= statement_type_length &&
		!op_isalnum(sql[statement_type_length]) && (op_strnicmp(sql, statement_type, statement_type_length) == 0);
}

/**********************
 * SqlTransaction_QuotaCallback
***********************/
class SqlTransaction_QuotaCallback : public OpDocumentListener::QuotaCallback
{
public:
	SqlTransaction_QuotaCallback(SqlTransaction *tr) : m_tr(tr){}

	virtual void OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size)
	{
		if (m_tr != NULL)
		{
			m_tr->OnQuotaReply(allow_increase, new_quota_size);
		}
		else
		{
			// Object orphaned
			OP_DELETE(this);
		}
	}
	virtual void OnCancel()
	{
		if (m_tr != NULL)
		{
			m_tr->OnQuotaCancel();
		}
		else
		{
			// Object orphaned
			OP_DELETE(this);
		}
	}
	SqlTransaction *m_tr;
};

/**********************
 * SqlTransaction
***********************/
SqlTransaction::SqlTransaction(WSD_Database *db, BOOL read_only, BOOL synchronous)
	: m_saved_statement_callback(this)
	, m_database(db)
	, m_used_db_version(NULL)
	, m_sql_validator(NULL)
	, m_used_datafile_name(NULL)
	, m_sqlite_db(NULL)
	, m_quota_callback(NULL)
	, m_cached_page_size(0)
	, m_sqlite_aborted(FALSE)
{
	if (read_only)
		SetFlag(READ_ONLY_TRANSACTION);
	if (synchronous)
		SetFlag(SYNCHRONOUS_TRANSACTION);
	DB_DBG(("%p: SqlTransaction::SqlTransaction(), WSD_Database(%p)\n", this, db))
}

SqlTransaction::~SqlTransaction()
{
	if (m_quota_callback != NULL)
	{
		if (GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER)
			m_quota_callback->m_tr = NULL;
		else
			OP_DELETE(m_quota_callback);
	}

	DB_DBG(("%p: SqlTransaction::~SqlTransaction(), WSD_Database(%p), file(%p, %d)\n", this, GetDatabase(), m_used_datafile_name, m_used_datafile_name ? m_used_datafile_name->GetRefCount() : -1))
	INTEGRITY_CHECK();
	OP_ASSERT(!GetFlag(BEING_DELETED));
	Out();

	SetFlag(BEING_DELETED);
	FireShutdownCallbacks();
	//If this transaction has not been released, then the
	//shudown callback should take care of that
	OP_ASSERT(m_used_datafile_name == NULL);
	OP_ASSERT(m_sqlite_db == NULL);
	GetDatabase()->OnTransactionTermination(this);
	DiscardPendingStatements();
	DiscardSavedStatements();
	OP_DELETEA(m_used_db_version);
	m_used_db_version = NULL;
}

/*static*/
SqlTransaction*
SqlTransaction::Create(WSD_Database *db, BOOL read_only, BOOL synchronous, const uni_char* expected_version)
{
	SqlTransaction* transaction = OP_NEW(SqlTransaction, (db, read_only, synchronous));

	if (expected_version == NULL || *expected_version == 0)
		expected_version = db->GetVersion();

	if (transaction != NULL && expected_version != NULL && *expected_version != 0)
	{
		transaction->m_used_db_version = UniSetNewStr(expected_version);
		if (transaction->m_used_db_version == NULL)
		{
			OP_DELETE(transaction);
			transaction = NULL;
		}
	}
	return transaction;
}

OP_STATUS
SqlTransaction::Release()
{
	INTEGRITY_CHECK();

	SetFlag(CLOSE_SHOULD_DELETE | HAS_BEEN_RELEASED);

	if (GetFlag(SHUTTING_DOWN | BUSY))
		return OpStatus::OK;

	return SafeClose();
}

OP_STATUS
SqlTransaction::Close()
{
	INTEGRITY_CHECK();
	BOOL synchronous = IsSynchronous() || GetDatabase()->IsBeingDeleted() || !GetDatabase()->IsOperaRunning();

	DB_DBG(("%p: SqlTransaction::Close(), WSD_Database(%p), file(%p,%d)\n", this, GetDatabase(), m_used_datafile_name, m_used_datafile_name?m_used_datafile_name->GetRefCount():-1))

	//this assert warns that the object might leak
	OP_ASSERT(!"a SqlTransaction might leak!" || GetFlag(CLOSE_SHOULD_DELETE) || synchronous);

	if (GetFlag(SHUTTING_DOWN | BEEN_POSTED_CLOSE_MESSAGE) && !synchronous)
		return OpStatus::OK;

	if (!GetFlag(HAS_BEEN_RELEASED) && !GetDatabase()->IsBeingDeleted())
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);

	//still has data to process
	if (GetCurrentStatement() != NULL)
	{
		if (!synchronous)
			return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);

		//if the core is being shutted down then we need to clear everything
		//else trigger the assert to warn that something is wrong
		OP_ASSERT(!"Trying to close transaction with pending data - ignore if the browser is shutting down! " || GetDatabase()->IsBeingDeleted() || !GetDatabase()->IsOperaRunning());

		SetFlag(BUSY | SHUTTING_DOWN);
		DiscardPendingStatements();
		UnsetFlag(BUSY | SHUTTING_DOWN);
	}

	OP_STATUS status = OpStatus::OK;
	SQLiteErrorCode sqliteerr = SQLITE_OK;
	if (m_sqlite_db != NULL)
	{
		SetFlag(BUSY | SHUTTING_DOWN);
		//kill all pending statements
		if (synchronous)
		{
			sqlite3_stmt *stmt = NULL;
			while( (stmt = sqlite3_next_stmt(m_sqlite_db, 0)) != NULL )
			{
				sqlite3_finalize(stmt);
			}
		}

		if (IsSingleTransaction())
		{
			//if the data file is empty, then optimize by deleting it
			//but only do it when this is the last transaction being
			//closed on the database
			//if there is a rollback segment, we need to eliminate it because
			//the object count in the open rollback segment might be different
			//than the object count on the data file
			if (!HasRollbackSegment() ||
				OpStatus::IsSuccess(ExecQuickQuery(SQL_STR_PAIR("ROLLBACK"), NULL, NULL)))
				OpStatus::Ignore(FetchObjectCount());
		}

		sqliteerr = sqlite3_close(m_sqlite_db);
		switch (sqliteerr)
		{
			case SQLITE_OK:
				break;
			case SQLITE_BUSY:
				if (!synchronous)
				{
					status = OpDbUtils::GetOpStatusError(ScheduleClose(), status);
					if (OpStatus::IsSuccess(status))
					{
						UnsetFlag(BUSY | SHUTTING_DOWN);
						return SIGNAL_OP_STATUS_ERROR(status);
					}
					// If not successful then fallback to error handling.
					// Allow fallthrough.
				}
				// if it fails to close, or is synchronous, then kill it
				// Allow fallthrough
			default:
				OP_ASSERT(!"Panic ! All statements should have been terminated but SQLite refused to close" || GetDatabase()->IsBeingDeleted() || !GetDatabase()->IsOperaRunning());

				//forcefull termination
				sqlite3_interrupt(m_sqlite_db);

				//and try to close one more time
				sqliteerr = sqlite3_close(m_sqlite_db);
				OP_ASSERT(sqliteerr == SQLITE_OK);
				status = OpStatus::ERR;
		}

		DB_DBG(("%p: SqlTransaction::Close() completed, file(%p)\n", this, m_used_datafile_name))

		OP_ASSERT(m_used_datafile_name != NULL);
		PS_DataFile_ReleaseOwner(m_used_datafile_name, this)
		m_used_datafile_name->Release();
		m_used_datafile_name = NULL;

		m_sqlite_db = NULL;
		m_cached_page_size = 0;
		UnsetFlag(BUSY | HAS_ROLLBACK_SEGMENT | SHUTTING_DOWN);
	}

	if (m_used_datafile_name != NULL)
	{
		OP_ASSERT(m_used_datafile_name->IsBogus());
		//this reference should only be kept when the file is unaccessible so it'll prevent superfluous processing
		PS_DataFile_ReleaseOwner(m_used_datafile_name, this)
		m_used_datafile_name->Release();
		m_used_datafile_name = NULL;
	}

	//we schedule a execute because there might be other transactions with pending statements
	//and the database might be waiting for this transaction's rollback segment to be disposed
	if (!IsSingleTransaction())
		status = OpDbUtils::GetOpStatusError(GetDatabase()->ScheduleTransactionExecute(0), status);

	if (GetFlag(CLOSE_SHOULD_DELETE) && !GetFlag(BEING_DELETED))
	{
		OP_DELETE(this);
	}

	return SIGNAL_OP_STATUS_ERROR(status);
}

OP_STATUS
SqlTransaction::Begin(SqlStatement::Callback *callback)
{
	INTEGRITY_CHECK();
	OP_ASSERT(!IsSynchronous() || !"Executing BEGIN is not allowed in a synchronous transaction else sqlite might deadlock");
	if (IsSynchronous())
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NOT_SUPPORTED);
	//if this assert triggers, then you've issued BGIN; twice
	OP_ASSERT(!HasRollbackSegment());
	return SIGNAL_OP_STATUS_ERROR(ExecuteSql(SqlText(SqlText::CONST_8BIT, SQL_STR_PAIR("BEGIN")), NULL, TRUE, FALSE, callback));
}

OP_STATUS
SqlTransaction::Commit(SqlStatement::Callback *callback)
{
	INTEGRITY_CHECK();
	return SIGNAL_OP_STATUS_ERROR(ExecuteSql(SqlText(SqlText::CONST_8BIT, SQL_STR_PAIR("COMMIT")), NULL, TRUE, TRUE, callback));
}

OP_STATUS
SqlTransaction::Rollback(SqlStatement::Callback *callback)
{
	INTEGRITY_CHECK();
	return SIGNAL_OP_STATUS_ERROR(ExecuteSql(SqlText(SqlText::CONST_8BIT, SQL_STR_PAIR("ROLLBACK")), NULL, TRUE, TRUE, callback));
}

OP_STATUS
SqlTransaction::ExecuteSql(
		const SqlText &sql_text,
		SqlValueList *parameters,
		SqlStatement::Callback *callback)
{
	return SIGNAL_OP_STATUS_ERROR(ExecuteSql(sql_text, parameters, FALSE, FALSE, callback));
}

OP_STATUS
SqlTransaction::ExecuteSql(
		const SqlText &sql_text,
		SqlValueList *parameters,
		BOOL should_not_validate,
		BOOL no_version_check,
		SqlStatement::Callback *callback)
{
	INTEGRITY_CHECK();

	SqlStatement *stmt;
	RETURN_OOM_IF_NULL(stmt = SqlStatement::Create(this, sql_text, parameters, should_not_validate, no_version_check, NULL));
	OpAutoPtr<SqlStatement> stmt_anchor(stmt);

	if (GetFlag(BEING_DELETED | SHUTTING_DOWN))
		return SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_CANCELLED);

#ifdef SELFTEST
	if (IsSynchronous())
	{
		//doing the Run is dangerous !
		//Later on, synchronous transaction will disappear and everything will be async
		//but meanwhile, this is what we have to do
		if (GetFlag(BUSY))
			return SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_YIELD);

		stmt_anchor.release();
		stmt->Into(&m_statement_queue);
		stmt->SetCallback(callback);

		if (GetCurrentStatement()->m_callback == &m_saved_statement_callback &&
			m_saved_statement_callback.m_original_statement == NULL)
			m_saved_statement_callback.Set(stmt);

		OP_STATUS status = OpStatus::OK;
		while (stmt == GetCurrentStatement() && OpStatus::IsSuccess(status))
		{
			status = ProcessNextStatement();
#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
			if (status == PS_Status::ERR_YIELD)
			{
				//let it run for a while
				unsigned number_of_runs = MIN(stmt->m_number_of_yields*5,40);
				while (number_of_runs-- > 0)
					g_opera->RequestRunSlice();
				stmt->m_number_of_yields++;
				status = OpStatus::OK;
			}
#endif //MESSAGELOOP_RUNSLICE_SUPPORT
		}
		return SIGNAL_OP_STATUS_ERROR(status);
	}
	else
#endif //SELFTEST
	{
		RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(GetDatabase()->InitMessageQueue()));
		RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(GetDatabase()->ScheduleTransactionExecute(0)));

		stmt_anchor.release();
		stmt->Into(&m_statement_queue);
		stmt->SetCallback(callback);

		if (GetCurrentStatement()->m_callback == &m_saved_statement_callback &&
			m_saved_statement_callback.m_original_statement == NULL)
			m_saved_statement_callback.Set(stmt);

		return OpStatus::OK;
	}
}

OP_STATUS
SqlTransaction::Flush()
{
	OP_STATUS status = OpStatus::OK;
	SqlStatement* stmt;
	while ((stmt = GetCurrentStatement()) != NULL && OpStatus::IsSuccess(status))
	{
		status = ProcessNextStatement();
#ifdef MESSAGELOOP_RUNSLICE_SUPPORT
		if (status == PS_Status::ERR_YIELD)
		{
			//let it run for a while
			unsigned number_of_runs = MIN(stmt->m_number_of_yields*5,40);
			while (number_of_runs-- > 0)
				g_opera->RequestRunSlice();
			stmt->m_number_of_yields++;
		}
#endif //MESSAGELOOP_RUNSLICE_SUPPORT
	}
	return status;
}

BOOL
SqlTransaction::IsMemoryOnly() const
{
	return GetDatabase()->IsMemoryOnly();
}

void
SqlTransaction::CancelAllStatements()
{
	DB_DBG(("%p: SqlTransaction::CancelAllStatements()\n", this))

	DiscardSavedStatements();

	SqlStatement *current = m_statement_queue.First(), *next;
	while (current != NULL)
	{
		next = current->Suc();
		current->Cancel();
		current = next;
	}
}

/**
 * Cancels the statement linked to the given callback
 */
void
SqlTransaction::CancelStatement(SqlStatement::Callback *callback)
{
	OP_ASSERT(callback);
	if (callback == NULL)
		return;

	DB_DBG(("%p: SqlTransaction::CancelStatement(%p)\n", this, callback))

	SqlStatement *current = m_statement_queue.First(), *next;
	while (current != NULL)
	{
		next = current->Suc();
		if (current->GetCallback() == callback)
			current->Cancel();
		current = next;
	}
}


OP_STATUS
SqlTransaction::EnsureInitialization(SQLiteErrorCode *error_code)
{
	INTEGRITY_CHECK();

	SQLiteErrorCode db_status;
	if (error_code == NULL)
		error_code = &db_status;
	*error_code = SQLITE_OK;

	if (IsInitialized())
	{
		*error_code = SQLITE_OK;
		return OpStatus::OK;
	}
	if (GetFlag(OBJ_INITIALIZED_ERROR))
	{
		*error_code = SQLITE_ABORT;
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);
	}

	OP_ASSERT(m_used_datafile_name == NULL);

	//optimization 1: if there is no datafile, and transaction is read-only, don't create it
	//optimization 2: if db has been marked for deletion, and we don't have a data file, also don't create it
	if ( (GetDatabase()->GetFileNameObj() == NULL && IsReadOnly()) ||
		  GetDatabase()->GetIndexEntry()->WillBeDeleted())
	{
		OP_ASSERT(m_used_datafile_name == NULL);

		if (GetDatabase()->GetIndexEntry()->WillBeDeleted())
		{
			if (GetDatabase()->GetFileNameObj() != NULL)
			{
				m_used_datafile_name = GetDatabase()->GetFileNameObj();
			}
			else if (!IsSingleTransaction())
			{
				//if the db is going to be deleted, reuse the datafile from a previous transaction
				SqlTransaction *t = Pred();
				while(m_used_datafile_name == NULL && t != NULL)
				{
					m_used_datafile_name = t->m_used_datafile_name;
					t = t->Pred();
				}
				t = Suc();
				while(m_used_datafile_name == NULL && t != NULL)
				{
					m_used_datafile_name = t->m_used_datafile_name;
					t = t->Suc();
				}
			}
		}
		if (m_used_datafile_name == NULL)
			m_used_datafile_name = GetDatabase()->GetPSManager()->GetNonPersistentFileName();
	}
	else
	{
		OP_ASSERT(m_sqlite_db == NULL);

		if (GetDatabase()->GetFileNameObj() != NULL)
			RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(GetDatabase()->GetFileNameObj()->CheckBogus()));

		if (GetDatabase()->GetFileAbsPath() == NULL)
		{
			RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(GetDatabase()->MakeAbsFilePath()));
			RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(GetDatabase()->EnsureDataFileFolder()));
		}
		m_used_datafile_name = GetDatabase()->GetFileNameObj();
	}

	OP_ASSERT(m_used_datafile_name != NULL);
	OP_ASSERT(m_used_datafile_name->m_file_abs_path != NULL);
	if (m_used_datafile_name->IsBogus())
	{
		m_used_datafile_name = NULL;
		return OpStatus::ERR_NO_ACCESS;
	}

	PS_DataFile_AddOwner(m_used_datafile_name, this)
	m_used_datafile_name->IncRefCount();

	DB_DBG(("%p: SqlTransaction::EnsureInitialization(): %d, %S\n", this, m_used_datafile_name->GetRefCount(),  m_used_datafile_name->m_file_abs_path))

	OP_STATUS op_status;
	if (IsReadOnly())
	{
		//*error_code = sqlite3_open16_v2(data_file_path, &m_sqlite_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL);
		OpString8 filename_utf8;
		if (OpStatus::IsSuccess(op_status = filename_utf8.SetUTF8FromUTF16(m_used_datafile_name->m_file_abs_path)))
			*error_code = sqlite3_open_v2(filename_utf8.CStr(), &m_sqlite_db, SQLITE_OPEN_READONLY | SQLITE_OPEN_NOMUTEX, NULL);
	}
	else
	{
		*error_code = sqlite3_open16(m_used_datafile_name->m_file_abs_path, &m_sqlite_db);
	}

	op_status = g_database_manager->SqliteErrorCodeToOpStatus(*error_code);
	op_status = HandleCorruptFile(op_status);
	if (OpStatus::IsError(op_status))
	{
		SetFlag(OBJ_INITIALIZED_ERROR);
		return op_status;
	}
	else if (!GetDatabase()->IsMemoryOnly())
		// We have file name and therefore should flush the index to make sure it keeps updated information.
		op_status = GetDatabase()->GetPSManager()->FlushIndexToFileAsync(GetDatabase()->GetUrlContextId());

	OP_ASSERT(m_sqlite_db != NULL);
	OP_ASSERT(m_used_datafile_name != NULL && !m_used_datafile_name->IsBogus());

	if (m_sqlite_db != NULL)
	{
		sqlite3_extended_result_codes(m_sqlite_db, 1);
		sqlite3_limit(m_sqlite_db, SQLITE_LIMIT_VARIABLE_NUMBER, DATABASE_INTERNAL_MAX_QUERY_BIND_VARIABLES);
	}
	SetFlag(OBJ_INITIALIZED);
	return SIGNAL_OP_STATUS_ERROR(op_status);
}

OP_STATUS
SqlTransaction::HandleCorruptFile(OP_STATUS status)
{
	if (status != PS_Status::ERR_CORRUPTED_FILE && status != OpStatus::ERR_NO_ACCESS)
		return OpStatus::OK;

	if (m_used_datafile_name != NULL)
		RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(m_used_datafile_name->CheckBogus()));

	const uni_char* data_file_path;
	unsigned number_of_tries = 0;
	while (number_of_tries++ < 3)
	{
		//1. cleanup
		//close db
		if (m_sqlite_db != NULL)
		{
			sqlite3_stmt *stmt = NULL;
			while( (stmt = sqlite3_next_stmt(m_sqlite_db, 0)) != NULL ) {
				sqlite3_finalize(stmt);
			}
			if (sqlite3_close(m_sqlite_db) != SQLITE_OK)
			{
				//forcefull termination
				sqlite3_interrupt(m_sqlite_db);
				//and try to close one more time
				sqlite3_close(m_sqlite_db);
			}
			m_sqlite_db = NULL;
			m_cached_page_size = 0;
		}

		if (status == PS_Status::ERR_CORRUPTED_FILE) {
			//delete the file to start all over
			data_file_path = GetDatabase()->GetFileAbsPath();
			if (data_file_path != NULL && !GetDatabase()->IsMemoryOnly())
			{
				OpFile file;
				if (OpStatus::IsSuccess(file.Construct(data_file_path)))
					OpStatus::Ignore(file.Delete());
			}
		}
		else if (status == OpStatus::ERR_NO_ACCESS)
		{
			//or if we're not allowed to write to the file jump to the next one
			GetDatabase()->GetIndexEntry()->DeleteDataFile();
		}
		if (m_used_datafile_name != NULL)
		{
			PS_DataFile_ReleaseOwner(m_used_datafile_name, this)
			m_used_datafile_name->Release();
			m_used_datafile_name = NULL;
		}

		//2. start all over
		RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(GetDatabase()->MakeAbsFilePath()));
		RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(GetDatabase()->EnsureDataFileFolder()));
		data_file_path = GetDatabase()->GetFileAbsPath();

		OP_ASSERT(data_file_path != NULL);
		OP_ASSERT(m_used_datafile_name == NULL);

		m_used_datafile_name = GetDatabase()->GetFileNameObj();
		OP_ASSERT(m_used_datafile_name != NULL);

		PS_DataFile_AddOwner(m_used_datafile_name, this)
		m_used_datafile_name->IncRefCount();

		DB_DBG(("%p: SqlTransaction::HandleCorruptFile(%d): %S\n", this, status, data_file_path))

		OP_ASSERT(m_sqlite_db == NULL);
		status = g_database_manager->SqliteErrorCodeToOpStatus(sqlite3_open16(data_file_path, &m_sqlite_db));
		if (OpStatus::IsSuccess(status))
		{
			SetFlag(OBJ_INITIALIZED);
			UnsetFlag(OBJ_INITIALIZED_ERROR);
			return OpStatus::OK;
		}
		else if (status == PS_Status::ERR_CORRUPTED_FILE || status == OpStatus::ERR_NO_ACCESS)
		{
			//again ?!? probably the file is not writable... try a new one then
			OP_ASSERT(m_used_datafile_name->GetRefCount() >= 2);
			PS_DataFile_ReleaseOwner(m_used_datafile_name, this)
			m_used_datafile_name->Release();
			m_used_datafile_name = NULL;
			GetDatabase()->GetIndexEntry()->DeleteDataFile();
			//continue the cycle
		}
		else
		{
			//nothing to do... bail out
			break;
		}
	}

	OP_ASSERT(m_used_datafile_name != NULL);
	m_used_datafile_name->SetBogus(TRUE);
	SetFlag(OBJ_INITIALIZED | OBJ_INITIALIZED_ERROR);

	return SIGNAL_OP_STATUS_ERROR(status);
}

OP_STATUS
SqlTransaction::ScheduleClose()
{
	INTEGRITY_CHECK();
	OP_ASSERT(GetDatabase() != NULL);

	if (GetDatabase()->GetMessageHandler() == NULL || !IsOperaRunning())
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);

	if (GetFlag(BEEN_POSTED_CLOSE_MESSAGE))
		return OpStatus::OK;

	RETURN_IF_ERROR(GetDatabase()->InitMessageQueue());
	OP_STATUS status =
		GetDatabase()->GetMessageHandler()->PostMessage(MSG_DATABASE_TRANSACTION_CLOSE,
				GetDatabase()->GetMessageQueueId(),reinterpret_cast<MH_PARAM_2>(this),0) ?
				OpStatus::OK : SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);

	if (OpStatus::IsSuccess(status))
		SetFlag(BEEN_POSTED_CLOSE_MESSAGE);

	return SIGNAL_OP_STATUS_ERROR(status);
}

#define BREAK_IF_ERROR(value) \
	if (OpStatus::IsError(op_status = SIGNAL_OP_STATUS_ERROR(value))) { \
		break; \
	}

#define SQLITE_EXEC_N_OPS 200 //how many ops until the progress handler is called ?
#define SQLITE_EXEC_N_STEPS 50 //how many consecutive steps


OP_STATUS
SqlTransaction::ProcessNextStatement()
{
	INTEGRITY_CHECK();
	SqlStatement *stmt = GetCurrentStatement();

	if (stmt == NULL)
		return OpStatus::OK;

	if (GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER)
		return SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_SUSPENDED);

	SQLiteErrorCode db_status = SQLITE_OK;
	OP_STATUS op_status = OpStatus::OK;

	if (stmt->m_state != SqlStatement::CANCELLED)
	{
		if (GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_USER_REPLIED)
		{
			op_status = HandleQuotaReply(stmt);

			OP_ASSERT(GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_DEFAULT);

			RETURN_IF_ERROR(op_status);
		}

		if (m_sqlite_aborted && !stmt->IsStatementCompleted())
		{
			RestoreSavedStatements();
			stmt->Reset();

			m_sqlite_aborted = FALSE;

			return OpStatus::OK;
		}
	}

	//If we're restoring previous statements, wait until the user schedules a new
	//statement. This has 2 advantages:
	// - gives a new callback if errors happen. Note that this callback
	//   obviously cannot receive result sets as signal of success of the
	//   backed up queries
	// - ensures that we only execute the backed up statement if indeed the
	//   user schedules more statements. Else if the transaction is dropped
	//   all pending statements are killed with it
	//Final note: if the transaction is released, then don't wait for a new callback object

	if (stmt->m_callback == &m_saved_statement_callback &&
		m_saved_statement_callback.m_original_statement == NULL &&
		stmt->m_state != SqlStatement::CANCELLED &&
		!GetFlag(HAS_BEEN_RELEASED))
		return SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_SUSPENDED);

	switch(stmt->m_state)
	{
	case SqlStatement::BEFORE_EXECUTION:
	{
		OP_ASSERT(stmt->m_stmt == NULL);
		stmt->m_is_result_success = TRUE;

		if (stmt->m_sql.m_sql_length == 0)
		{
			//sqlite does not handle empty string properly
			//it returns OK from sqlite3_prepare-* but all
			//subsequent call to other API functions break
			db_status = SQLITE_ERROR;
			BREAK_IF_ERROR(g_database_manager->SqliteErrorCodeToOpStatus(db_status));
		}

		DB_DBG(("------------------\n%p: SqlTransaction::ProcessNextStatement(%d, %p): (rb segment: %s/%s), (cb: %s, %p), %s\n",
				this, stmt->m_state, stmt, DB_DBG_BOOL(HasRollbackSegment()),
				DB_DBG_BOOL(m_sqlite_db != NULL ? !sqlite3_get_autocommit(m_sqlite_db) : 0),
				DB_DBG_BOOL(stmt->m_callback == &m_saved_statement_callback), stmt->m_callback,
				DB_DBG_QUERY(stmt->m_sql)))

		if (!stmt->m_no_version_check)
		{
			DB_DBG((" - version check: \"%S\" == \"%S\"\n", m_used_db_version, GetDatabase()->GetVersion()))
			BREAK_IF_ERROR(CheckDbVersion());
		}
		BREAK_IF_ERROR(EnsureInitialization(&db_status));

		if (!GetDatabase()->IsMemoryOnly() &&
			!HasRollbackSegment() &&
			GetDatabase()->GetIndexEntry()->GetFlag(PS_IndexEntry::HAS_CACHED_DATA_SIZE))
		{
			BREAK_IF_ERROR(GetDatabase()->GetDataFileSize(&stmt->m_data_file_size));
		}
		else
		{
			BREAK_IF_ERROR(GetSize(&stmt->m_data_file_size));

			if (!GetDatabase()->IsMemoryOnly() &&
				!GetDatabase()->GetIndexEntry()->GetFlag(PS_IndexEntry::HAS_CACHED_DATA_SIZE))
			{
				GetDatabase()->SetDataFileSize(stmt->m_data_file_size);
			}
		}

		BREAK_IF_ERROR(CheckQuotaPolicies());

		stmt->m_avail_exec_time_ms = GetDatabase()->GetPolicyAttribute(
				PS_Policy::KQueryExecutionTimeout, stmt->GetWindow());
		if (stmt->m_avail_exec_time_ms == 0)
			stmt->m_avail_exec_time_ms = 0xffffffff;

		stmt->m_statement_auth_data.m_validator = GetSqlValidator();
		stmt->m_statement_auth_data.m_statement_type = 0;
		stmt->m_statement_auth_data.m_flags = stmt->m_should_not_validate ? AUTH_DONT_VALIDATE : 0;

		SetFlag(BUSY);
		sqlite3_set_authorizer(m_sqlite_db, SqliteAuthorizerCallback, &stmt->m_statement_auth_data);
		sqlite3_progress_handler(m_sqlite_db, SQLITE_EXEC_N_OPS, SqliteProgressCallback, stmt);

		OP_ASSERT(!stmt->m_sw.IsRunning());
		OP_ASSERT(stmt->m_sw.GetEllapsedMS() == 0);

		stmt->m_sw.Start();
		if (stmt->m_sql.Is8Bit())
			db_status = sqlite3_prepare_v2(m_sqlite_db, stmt->m_sql.m_sql8, stmt->m_sql.m_sql_length, &stmt->m_stmt, NULL);
		else
			db_status = sqlite3_prepare16_v2(m_sqlite_db, stmt->m_sql.m_sql16, UNICODE_SIZE(stmt->m_sql.m_sql_length), &stmt->m_stmt, NULL);
		stmt->m_sw.Stop();

		op_status = g_database_manager->SqliteErrorCodeToOpStatus(db_status);
		if (op_status == PS_Status::ERR_CORRUPTED_FILE)
		{
			op_status = HandleCorruptFile(op_status);
			if (OpStatus::IsSuccess(op_status))
			{
				sqlite3_set_authorizer(m_sqlite_db, SqliteAuthorizerCallback, &stmt->m_statement_auth_data);
				sqlite3_progress_handler(m_sqlite_db, SQLITE_EXEC_N_OPS, SqliteProgressCallback, stmt);

				stmt->m_sw.Start();
				if (stmt->m_sql.Is8Bit())
					db_status = sqlite3_prepare_v2(m_sqlite_db, stmt->m_sql.m_sql8, stmt->m_sql.m_sql_length, &stmt->m_stmt, NULL);
				else
					db_status = sqlite3_prepare16_v2(m_sqlite_db, stmt->m_sql.m_sql16, UNICODE_SIZE(stmt->m_sql.m_sql_length), &stmt->m_stmt, NULL);
				stmt->m_sw.Stop();

				op_status = g_database_manager->SqliteErrorCodeToOpStatus(db_status);
			}
		}
		BREAK_IF_ERROR(op_status);

		db_status = BindParametersToStatement(stmt->m_stmt, stmt->m_parameters);
		op_status = g_database_manager->SqliteErrorCodeToOpStatus(db_status);
		BREAK_IF_ERROR(op_status);

		stmt->SetState(SqlStatement::DURING_EXECUTION);
		stmt->m_number_of_yields = 0;

		return OpStatus::OK;
	}
	case SqlStatement::DURING_EXECUTION:
	{
		OP_ASSERT(!stmt->m_sw.IsRunning());
		stmt->m_sw.Continue();
		db_status = sqlite3_step(stmt->m_stmt);
		stmt->m_sw.Stop();
		op_status = g_database_manager->SqliteErrorCodeToOpStatus(db_status);

		DB_DBG_2(("%p: SqlTransaction::ProcessNextStatement(%d, %p): (rb segment: %s/%s), %d, %d\n", this, stmt->m_state, stmt,
				DB_DBG_BOOL(HasRollbackSegment()), DB_DBG_BOOL(m_sqlite_db != NULL ? !sqlite3_get_autocommit(m_sqlite_db) : 0), db_status, op_status))

		if (stmt->HasTimedOut())
		{
			//too much time, goodbye
			DB_DBG_2((" - timed out: %u vs %u\n", stmt->m_avail_exec_time_ms, stmt->m_sw.GetEllapsedMS()))

			db_status = SQLITE_INTERRUPT;
			BREAK_IF_ERROR(PS_Status::ERR_TIMED_OUT);
		}

		if (op_status == PS_Status::ERR_TIMED_OUT)
		{
			//this is actually the error returned by our progress handler
			return OpStatus::OK;
		}
		if (op_status == PS_Status::ERR_NO_MEMORY)
		{
			//result set grew too much
			BREAK_IF_ERROR(PS_Status::ERR_RSET_TOO_BIG);
		}

		if (db_status == SQLITE_BUSY || db_status == SQLITE_IOERR_BLOCKED)
		{
			//yield this ! the datafile is locked
			return op_status;
		}
		BREAK_IF_ERROR(op_status);

		//create result set IF exists callback to receive it and statement is not a commit/begin/rollback
		if (stmt->m_callback != NULL && !stmt->m_statement_auth_data.GetFlag(AUTH_AFFECTS_ROLLBACK))
		{
			if ((stmt->m_result_set = OP_NEW(SqlResultSet,())) == NULL)
				BREAK_IF_ERROR(OpStatus::ERR_NO_MEMORY);
		}

		OP_ASSERT(db_status == SQLITE_DONE || db_status == SQLITE_ROW);

		if (db_status == SQLITE_ROW || stmt->IsSelect())
		{
			if (stmt->m_result_set != NULL)
			{
				stmt->m_result_set->SetStatement(stmt, stmt->m_stmt, db_status == SQLITE_ROW);
				stmt->m_stmt = NULL;

				OP_ASSERT(stmt->m_callback != NULL);
				if (stmt->m_callback->RequiresCaching())
				{
					unsigned max_size_bytes = GetDatabase()->GetPolicyAttribute
						(PS_Policy::KMaxResultSetSize, stmt->GetWindow());

					stmt->m_result_set->EnableCaching(max_size_bytes);
					stmt->SetState(SqlStatement::DURING_STEPPING);
				}
				else
					stmt->SetState(SqlStatement::AFTER_EXECUTION);
			}
			else
			{
				//SELECT without callback... hum.... just jump to the end to terminate everything
				stmt->SetState(SqlStatement::AFTER_EXECUTION);
			}

			if (db_status == SQLITE_DONE)
			{
				//no rows returned for a select, so we'll just do this extra step
				//to leave the result set in the proper state
				if (stmt->m_result_set != NULL)
				{
					OP_ASSERT(stmt->m_callback != NULL);
					BOOL test = FALSE;
					BREAK_IF_ERROR(stmt->m_result_set->Step(&test));
					OP_ASSERT(!test);
					OP_ASSERT(stmt->m_result_set->IsClosed());
				}
				stmt->SetState(SqlStatement::AFTER_EXECUTION);
			}
		}
		else// if (db_status == SQLITE_DONE)
		{
			//non-SELECT - therefore transactional. DDL and DML are transactional in sqlite
			if (stmt->m_result_set != NULL)
			{
				stmt->m_result_set->SetDMLData(
						stmt,
						sqlite3_changes(m_sqlite_db),
						sqlite3_last_insert_rowid(m_sqlite_db));
			}

			if (stmt->m_stmt != NULL)
			{
				sqlite3_finalize(stmt->m_stmt);
				stmt->m_stmt = NULL;
			}
			UnsetFlag(BUSY);

			if (stmt->m_statement_auth_data.GetFlag(AUTH_AFFECTS_ROLLBACK))
			{
				if (stmt->m_statement_auth_data.GetFlag(AUTH_IS_COMMIT | AUTH_IS_ROLLBACK))
					UnsetFlag(HAS_ROLLBACK_SEGMENT);
				else //if (m_last_statement_auth_data.GetFlag(AUTH_IS_BEGIN))
					SetFlag(HAS_ROLLBACK_SEGMENT);
			}

			InvalidateCachedDataSize();

			if (!GetDatabase()->IsMemoryOnly() &&
					(stmt->m_statement_auth_data.GetFlag(AUTH_IS_COMMIT) || !HasRollbackSegment()))
			{
				OpFileLength new_size;
				op_status = OpDbUtils::GetOpStatusError(GetSize(&new_size),op_status);
				if (OpStatus::IsSuccess(op_status))
				{
					GetDatabase()->SetDataFileSize(new_size);
					//need to invalidate the global value
					GetDatabase()->GetPSManager()->InvalidateCachedDataSize(GetDatabase()->GetType(),
						GetDatabase()->GetUrlContextId(), GetDatabase()->GetOrigin());
				}
			}
			stmt->SetState(SqlStatement::AFTER_EXECUTION);

			if (stmt->m_callback == NULL)
			{
				//nothing else to do, we're not going to return anything, so just bail out
				UnsetFlag(BUSY);
				TerminateCurrentStatement(stmt);
				return SIGNAL_OP_STATUS_ERROR(op_status);
			}
		}

		//if this triggers, then something was missing before
		OP_ASSERT(stmt->m_state != SqlStatement::DURING_EXECUTION);

		return OpStatus::OK;
	}
	case SqlStatement::DURING_STEPPING:
	{
		OP_ASSERT(stmt->m_result_set != NULL);

		BOOL has_data = FALSE;
		int row_count = SQLITE_EXEC_N_STEPS;

		//Step internally uses the stopwatch
		TRAP(op_status,{
			do{
				has_data = stmt->m_result_set->StepL();
			} while (has_data && row_count-- > 0);
		});

		if (has_data && (stmt->HasTimedOut()))
		{
			//too much time, goodbye
			db_status = SQLITE_INTERRUPT;
			BREAK_IF_ERROR(PS_Status::ERR_TIMED_OUT);
		}
		if (op_status == PS_Status::ERR_TIMED_OUT)
		{
			//this is actually the error returned by our progress handler
			return OpStatus::OK;
		}
		if (op_status == PS_Status::ERR_NO_MEMORY)
		{
			//result set grew too much
			BREAK_IF_ERROR(PS_Status::ERR_RSET_TOO_BIG);
		}

		BREAK_IF_ERROR(op_status);

		//finished
		if (stmt->m_result_set->IsClosed())
		{
			stmt->SetState(SqlStatement::AFTER_EXECUTION);
			UnsetFlag(BUSY);
		}
		return OpStatus::OK;
	}
	case SqlStatement::AFTER_EXECUTION:
	{
		if (!stmt->m_has_called_callbacks)
		{
			stmt->m_has_called_callbacks = TRUE;
			if (stmt->m_is_result_success)
			{
				if (stmt->m_callback != NULL)
				{
					DB_DBG(("%p: SqlTransaction::ProcessNextStatement(%d, %p) : HandleCallback(%p)\n", this, stmt->m_state, stmt, stmt->m_callback))
					op_status = stmt->m_callback->HandleCallback(stmt->m_result_set);
				}
				else
				{
					//callback might have been deleted meanwhile
					OP_DELETE(stmt->m_result_set);
					stmt->m_result_set = NULL;
				}
			}
			else
			{
				if (stmt->m_callback != NULL)
				{
					DB_DBG(("%p: SqlTransaction::ProcessNextStatement(%d, %p) : HandleError(%p,\"%S\")\n", this, stmt->m_state, stmt, stmt->m_callback, stmt->m_result_error_data.m_result_error_message))

					const uni_char* error_message = stmt->m_result_error_data.m_result_error_message;
					stmt->m_result_error_data.m_result_error_message = NULL;

					op_status = OpDbUtils::GetOpStatusError(
							SIGNAL_OP_STATUS_ERROR(stmt->m_callback->HandleError(stmt->m_result_error_data.m_result_error, error_message, !stmt->IsErrorRecoverable())),
							stmt->m_result_error_data.m_result_error);
				}
				else
				{
					//callback might have been deleted meanwhile
					OP_DELETEA(stmt->m_result_error_data.m_result_error_message);
					stmt->m_result_error_data.m_result_error_message = NULL;
				}
				OP_ASSERT(stmt->IsStatementCompleted());
			}
		}

		if (!stmt->IsStatementCompleted())
			// Iterable result set without caching means we need to wait until
			// the API user has depleted it and closed it.
			return SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_SUSPENDED);

		//query complete. Finish ad libitum
		UnsetFlag(BUSY);
		TerminateCurrentStatement(stmt);
		return SIGNAL_OP_STATUS_ERROR(op_status);
	}
	case SqlStatement::CANCELLED:
	{
		UnsetFlag(BUSY);
		TerminateCurrentStatement(stmt);
		return SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_CANCELLED);
	}
	default:
		OP_ASSERT(!"Invalid state hit!");
	}

	//the following section can only run once
	OP_ASSERT(OpStatus::IsError(op_status));
	OP_ASSERT(stmt->m_state != SqlStatement::AFTER_EXECUTION);
	OP_ASSERT(stmt->m_is_result_success);

	stmt->TerminateStatementAndResultSet();

	DB_DBG(("%p: SqlTransaction::ProcessNextStatement(%d, %p) : ERROR(%d)\n", this, stmt->m_state, stmt, op_status))

	UnsetFlag(BUSY);

	//if the file is going to be deleted, don't ask for a quota increase because it'll be useless
	if (op_status == PS_Status::ERR_QUOTA_EXCEEDED && !WillFileBeDeleted())
	{
		unsigned handling = GetDatabase()->GetPolicyAttribute(PS_Policy::KOriginExceededHandling, stmt->GetWindow());

		OP_ASSERT(GetDatabase()->GetQuotaHandlingStatus() != PS_IndexEntry::QS_WAITING_FOR_USER);

		if (handling == PS_Policy::KQuotaAskUser)
		{
			if (stmt->GetWindow() != NULL && GetDatabase()->GetDomain() != NULL)
			{
				if (m_quota_callback == NULL)
					m_quota_callback = OP_NEW(SqlTransaction_QuotaCallback, (this));

				if (m_quota_callback == NULL)
				{
					db_status = SQLITE_NOMEM;
					op_status = SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);
				}
				else
				{
					WindowCommander* wc = stmt->GetWindow()->GetWindowCommander();
					OpDocumentListener *listener = wc->GetDocumentListener();

					GetDatabase()->SetQuotaHandlingStatus(PS_IndexEntry::QS_WAITING_FOR_USER);

					OpFileLength current_size;
					if (OpStatus::IsError(GetSize(&current_size)))
						current_size = stmt->m_data_file_size;

					const uni_char* display_name = stmt->GetDisplayName();
					if (display_name == NULL || *display_name == 0)
						display_name = GetDatabase()->GetName();

					listener->OnIncreaseQuota(wc, display_name,
							GetDatabase()->GetDomain(), GetDatabase()->GetTypeDesc(),
							GetDatabase()->GetPolicyAttribute(PS_Policy::KOriginQuota), stmt->GetPreferredSize(), m_quota_callback);

					//statement needs to be restarted, because it's in a invalid state
					stmt->Reset();

					//check again in case the listener replied synchronously
					if (GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER)
					{
						return SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_SUSPENDED);
					}
					else if (GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_USER_REPLIED)
					{
						//jump back up
						return OpStatus::OK;
					}
				}
			}
			else
			{
				/**
				 * Some things happened:
				 *  - there is no callback, so don't do fancy quota handling
				 *  - there is no Window. That can be bad API usage, but can
				 *    happen too if a storage operation is is queued to be
				 *    executed and the Window is destroyed meanwhile.
				 *  - there's no domain, which means preferences can't be saved and
				 *    hence the operation should fail.
				 */
			}
		}
		else if (handling == PS_Policy::KQuotaAllow)
		{
			// CheckQuotaPolicies sets infinite quota if KQuotaAllow is set, so a SQLITE_FULL error
			// will mean out of disk then. Sqlite as of the time of this writing, does not provide
			// a reliable way to test if SQLITE_FULL was returned when hitting PRAGMA max_page_count
			// or filling in the disk.
			// There's also an edge case when the quota reaches the limits that sqlite supports
			// for the file size, so we need to check the page count.
			unsigned curr_page_count;
			if (OpStatus::IsSuccess(GetPageCount(&curr_page_count)) && curr_page_count == SQLITE_MAX_PAGE_COUNT)
			{
				// Reached end of file, proceed with quota error.
			}
			else
			{
				//out of disk !
				db_status = SQLITE_OK;
				op_status = SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_DISK);
			}
		}
	}

	stmt->SetState(SqlStatement::AFTER_EXECUTION);
	stmt->m_is_result_success = FALSE;
	stmt->m_result_error_data.m_result_error = SIGNAL_OP_STATUS_ERROR(op_status);
	stmt->m_result_error_data.m_result_error_sqlite = db_status;
	stmt->m_result_error_data.m_result_error_message = NULL;
	stmt->RaiseError();

	if (stmt->m_callback != NULL)
	{
		if (op_status != OpStatus::ERR_NO_MEMORY)
		{
			//just some code to make good messages
			if (op_status == OpStatus::ERR_NO_DISK)
				stmt->m_result_error_data.m_result_error_message = UniSetNewStr(UNI_L("Out of disk"));
			else if (db_status == SQLITE_ERROR && stmt->m_sql.m_sql_length == 0)
				stmt->m_result_error_data.m_result_error_message = UniSetNewStr(UNI_L("syntax error: no query"));
			else if (db_status == SQLITE_RANGE)
				// in case there are less arguments supplied than the ones passed to the query
				// the error is caused by BindParametersToStatement directly, so there is not error message.
				stmt->m_result_error_data.m_result_error_message = UniSetNewStr(UNI_L("bind or column index out of range"));
			else if (!IS_SQLITE_STATUS_OK(db_status) && db_status != SQLITE_INTERRUPT)
				stmt->m_result_error_data.m_result_error_message = UniSetNewStr(reinterpret_cast<const uni_char*>(sqlite3_errmsg16(m_sqlite_db)));
		}
	}
	else
	{
		TerminateCurrentStatement(stmt);
		return SIGNAL_OP_STATUS_ERROR(op_status);
	}

	return OpStatus::OK;
}
#undef BREAK_IF_ERROR
#undef SQLITE_EXEC_N_OPS
#undef SQLITE_EXEC_N_STEPS

void
SqlTransaction::TerminateCurrentStatement(SqlStatement*& stmt)
{
	OP_ASSERT(stmt != NULL);
	OP_ASSERT(stmt == GetCurrentStatement());
	OP_ASSERT(stmt->IsStatementCompleted());

	stmt->TerminateStatementAndResultSet();

	BOOL must_restart = stmt->MustRestartTransaction();

	DB_DBG_2(("%p: SqlTransaction::TerminateCurrentStatement(%d, %p) : restart - %s\n", this, stmt->m_state, stmt, DB_DBG_BOOL(must_restart)))

	//No rollback segment means there's nothing to reuse later
	//An OOM/OOD error is also fatal, so kill transaction also
	if (!must_restart && !HasRollbackSegment())
	{
		DiscardSavedStatements();
	}

	//We save statement if there is a rollback statement, else
	//  they are flushed to disk, so there's no saving to do
	//If the transaction is going to be closed (safely) then
	//  ignore the statement, because it's going to be ignored,
	//  and it would prevent the transaction from closing anyway
	//Select statements have no effect on the transaction, so we ignore those
	//Statements cancelled by the API users are also obviously ignored
	//We also save statements that either succeeded only, if
	//  they failed and the fail was not triggered by sqlite itself
	//  but by us, like timeouts
	if (HasRollbackSegment() &&
		!CanCloseSafely() &&
		stmt->m_state != SqlStatement::CANCELLED &&
		!stmt->IsSelect() &&
		(stmt->m_is_result_success || IS_SQLITE_STATUS_OK(stmt->m_result_error_data.m_result_error_sqlite)))
	{
		stmt->DiscardCallback();
		stmt->Out();
		stmt->Into(&m_saved_statements);
		stmt->Reset();
#ifdef _DEBUG
		stmt->m_has_executed_once = TRUE;
#endif
	}
	else
	{
		OP_DELETE(stmt);
	}

	stmt = NULL;
	if (must_restart)
		RestoreSavedStatements();

	//switch to the next transaction so it'll have also it's chance to execute
	if (!HasRollbackSegment() &&
		m_database->m_current_transaction == this)
	{
		m_database->m_current_transaction = Suc();
	}
	OpStatus::Ignore(SafeClose());
}

void
SqlTransaction::RestoreSavedStatements()
{
	SqlStatement* last_saved = m_saved_statements.Last(), *prev_saved;

	if (last_saved == NULL)
		return;

	DB_DBG(("%p: SqlTransaction::RestoreSavedStatements(%d,%p)\n", this, GetCurrentStatement() ? GetCurrentStatement()->m_state : -1, GetCurrentStatement()))

	if (m_saved_statement_callback.m_original_statement == NULL)
		m_saved_statement_callback.Set(GetCurrentStatement());

	while (last_saved != NULL)
	{
		prev_saved = last_saved->Pred();

		OP_ASSERT(last_saved->m_state == SqlStatement::BEFORE_EXECUTION);

		DB_DBG_2((" - %p: %s\n", last_saved, DB_DBG_QUERY(last_saved->m_sql)))

		last_saved->Out();
		last_saved->IntoStart(&m_statement_queue);
		last_saved->m_callback = &m_saved_statement_callback;
		last_saved = prev_saved;
	}
}

// From the OpDocumentListener::QuotaCallback API
void
SqlTransaction::OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size)
{
	OP_ASSERT(GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER);

	GetDatabase()->SetQuotaHandlingStatus(PS_IndexEntry::QS_USER_REPLIED);
	m_reply_cancelled = FALSE;
	m_reply_allow_increase = allow_increase;
	m_reply_new_quota_size = new_quota_size;

	ReportCondition(GetDatabase()->ScheduleTransactionExecute(5));
}

void
SqlTransaction::OnQuotaCancel()
{
	OP_ASSERT(GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_WAITING_FOR_USER);

	GetDatabase()->SetQuotaHandlingStatus(PS_IndexEntry::QS_USER_REPLIED);
	m_reply_cancelled = TRUE;

	ReportCondition(GetDatabase()->ScheduleTransactionExecute(5));
}

OP_STATUS
SqlTransaction::HandleQuotaReply(SqlStatement* stmt)
{
	DB_DBG_2(("%p: SqlTransaction::OnQuotaReply(%s,%u)\n", this, DB_DBG_BOOL(m_reply_allow_increase), (unsigned)m_reply_new_quota_size))

	OpFileLength current_size;
	if (OpStatus::IsError(GetSize(&current_size )))
		current_size = stmt->m_data_file_size;

	OP_ASSERT(GetDatabase()->GetQuotaHandlingStatus() == PS_IndexEntry::QS_USER_REPLIED);
	GetDatabase()->SetQuotaHandlingStatus(PS_IndexEntry::QS_DEFAULT);

	OP_STATUS status = OpStatus::OK;

	if (m_reply_cancelled)
	{
		if (!stmt->m_is_result_success)
			OP_DELETE(stmt->m_result_error_data.m_result_error_message);
		stmt->m_result_error_data.m_result_error_message = NULL;
		stmt->m_is_result_success = FALSE;
		stmt->m_result_error_data.m_result_error = PS_Status::ERR_QUOTA_EXCEEDED;
		stmt->SetState(SqlStatement::AFTER_EXECUTION);
	}
	else if (m_reply_allow_increase && (m_reply_new_quota_size > current_size || m_reply_new_quota_size == 0))
	{
		if (m_reply_new_quota_size == 0) // don't ask again, always increase
		{
			status = SetMaxSize(PS_Policy::UINT64_ATTR_INVALID);
			status = OpDbUtils::GetOpStatusError(GetDatabase()->SetPolicyAttribute(PS_Policy::KOriginExceededHandling,
					PS_Policy::KQuotaAllow, stmt->GetWindow()), status);
		}
		else
		{
			status = SetMaxSize(m_reply_new_quota_size);
			status = OpDbUtils::GetOpStatusError(GetDatabase()->SetPolicyAttribute(PS_Policy::KOriginQuota,
					m_reply_new_quota_size, stmt->GetWindow()), status);

			if (m_reply_new_quota_size > GetDatabase()->GetPolicyAttribute(PS_Policy::KGlobalQuota, stmt->GetWindow()))
				status = OpDbUtils::GetOpStatusError(GetDatabase()->SetPolicyAttribute(PS_Policy::KGlobalQuota,
						m_reply_new_quota_size, stmt->GetWindow()), status);
		}
	}
	else if (!m_reply_allow_increase)
	{
		status = GetDatabase()->SetPolicyAttribute(PS_Policy::KOriginExceededHandling, PS_Policy::KQuotaDeny, stmt->GetWindow());
		if (m_reply_new_quota_size != 0)
		{
			status = OpDbUtils::GetOpStatusError(GetDatabase()->SetPolicyAttribute(PS_Policy::KOriginQuota,
					m_reply_new_quota_size, stmt->GetWindow()), status);
			if (m_reply_new_quota_size > GetDatabase()->GetPolicyAttribute(PS_Policy::KGlobalQuota, stmt->GetWindow()))
				status = OpDbUtils::GetOpStatusError(GetDatabase()->SetPolicyAttribute(PS_Policy::KGlobalQuota,
						m_reply_new_quota_size, stmt->GetWindow()), status);
		}
		if (!stmt->m_is_result_success)
			OP_DELETEA(stmt->m_result_error_data.m_result_error_message);
		stmt->m_result_error_data.m_result_error_message = NULL;
		stmt->m_is_result_success = FALSE;
		stmt->m_result_error_data.m_result_error = PS_Status::ERR_QUOTA_EXCEEDED;
		stmt->SetState(SqlStatement::AFTER_EXECUTION);
	}

	return status;
}

int
SqlTransaction::SqliteAuthorizerCallback(void* user_data,int sqlite_action,
		const char* d1,const char*d2,const char*d3,const char*d4)
{
	SqlStatement::SqliteAuthorizerData *auth_data = reinterpret_cast<SqlStatement::SqliteAuthorizerData*>(user_data);
	OP_ASSERT(auth_data != NULL);

	if (g_database_policies->GetSqlActionFlag(sqlite_action, SQL_ACTION_IS_STATEMENT) &&
			auth_data->m_statement_type == 0)
		auth_data->m_statement_type = sqlite_action;

	if (g_database_policies->GetSqlActionFlag(sqlite_action, SQL_ACTION_AFFECTS_TRANSACTION))
	{
		auth_data->SetFlag(AUTH_AFFECTS_ROLLBACK);
		if (SqlStatement::IsBeginStatement(d1))
			auth_data->SetFlag(AUTH_IS_BEGIN);
		else if (SqlStatement::IsRollbackStatement(d1))
			auth_data->SetFlag(AUTH_IS_ROLLBACK);
		else if (SqlStatement::IsCommitStatement(d1))
			auth_data->SetFlag(AUTH_IS_COMMIT);
		else{ OP_ASSERT(!"Unknown statement changed transaction state!");}
	}

	if (auth_data->GetFlag(AUTH_DONT_VALIDATE))
		return SQLITE_OK;

	int should_run = auth_data->m_validator != NULL ?
			auth_data->m_validator->Validate(sqlite_action, d1, d2, d3, d4) : SQLITE_OK;

	return should_run;
}

int
SqlTransaction::SqliteProgressCallback(void *user_data)
{
	//Although it might seem odd, this is exactly the only thin that
	//this function needs to do. If the sqlite progress handler
	//returns SQLITE_ERROR, sqlite will interrupt the current statement
	//but thanks to our outstanding patch, the statement is nt invalidated
	//and can be resumed.
	return SQLITE_ERROR;
}

/*static*/
SQLiteErrorCode
SqlTransaction::BindParametersToStatement(sqlite3_stmt *stmt, SqlValueList* parameters)
{
	unsigned i = 0;
	SQLiteErrorCode db_status;

	if (parameters != NULL)
	{
		for (; i < parameters->length; i++)
		{
			// Note: parameter indexes start at 1.
			db_status = parameters->values[i].BindToStatement(stmt, i + 1);
			if (db_status != SQLITE_OK)
				return db_status;
			if (i > DATABASE_INTERNAL_MAX_QUERY_BIND_VARIABLES)
				return SQLITE_RANGE;
		}
	}

	i++;

	//last try - bind a NULL at position i, which is one extra argument after the ones passed
	//if the number of args fits the number of placeholders, this will return SQLITE_RANGE
	//if the number of args is less than the number of placeholders, it'll return SQLITE_OK
	//in the later case, we return error, because the number of placeholders must
	//match the number of args forcefully
	db_status = sqlite3_bind_int(stmt, i, 0);
	sqlite3_bind_null(stmt, i);

	if (db_status == SQLITE_OK)
		return SQLITE_RANGE;

	return SQLITE_OK;
}

OP_STATUS
SqlTransaction::CheckQuotaPolicies()
{
	INTEGRITY_CHECK();

	if (!IsReadOnly())
	{
		OpFileLength allowed_size;
		RETURN_IF_ERROR(CalculateAvailableDataSize(&allowed_size));
		RETURN_IF_ERROR(SetMaxSize(allowed_size));
	}
	return OpStatus::OK;
}

OP_STATUS
SqlTransaction::CheckDbVersion() const
{
	return m_used_db_version == NULL || *m_used_db_version == 0 ||
		OpDbUtils::StringsEqual(m_used_db_version, GetDatabase()->GetVersion()) ?
			(OP_STATUS)OpStatus::OK : (OP_STATUS)PS_Status::ERR_VERSION_MISMATCH;
}

BOOL
SqlTransaction::WillFileBeDeleted() const
{
	return m_used_datafile_name != NULL && m_used_datafile_name->WillBeDeleted();
}

#define CHECK_DBSTATUS(s, block) \
	do{ \
		SQLiteErrorCode code_tmp = (s); \
		if (code_tmp != SQLITE_OK && \
			code_tmp != SQLITE_ROW && \
			code_tmp != SQLITE_DONE) \
		{ \
			if (error_code != NULL) \
				*error_code = code_tmp; \
			OP_STATUS status = g_database_manager->SqliteErrorCodeToOpStatus(code_tmp); \
			if (OpStatus::IsError(status)) \
			{ \
				do{ \
					block; \
				}while(0); \
				return SIGNAL_OP_STATUS_ERROR(status); \
			} \
			return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);\
		} \
	}while(0)

//this is synchronous
OP_STATUS
SqlTransaction::ExecQuickQuery(const char *sql, unsigned sql_length, SqlValue* result, SQLiteErrorCode *error_code)
{
	INTEGRITY_CHECK();

	RETURN_IF_ERROR(EnsureInitialization(error_code));

	sqlite3_stmt *stmt;
	if (error_code != NULL)
		*error_code = SQLITE_OK;

	sqlite3_set_authorizer(m_sqlite_db, NULL, NULL);
	sqlite3_progress_handler(m_sqlite_db, 0, NULL, NULL);
	SQLiteErrorCode db_status = sqlite3_prepare_v2(m_sqlite_db, sql, sql_length, &stmt, NULL);
	if (db_status != SQLITE_OK && OpStatus::IsSuccess(HandleCorruptFile(g_database_manager->SqliteErrorCodeToOpStatus(db_status))))
	{
		sqlite3_set_authorizer(m_sqlite_db, NULL, NULL);
		sqlite3_progress_handler(m_sqlite_db, 0, NULL, NULL);
		db_status = sqlite3_prepare_v2(m_sqlite_db, sql, sql_length, &stmt, NULL);
	}
	CHECK_DBSTATUS(db_status, sqlite3_finalize(stmt));
	OP_ASSERT(stmt != NULL);

	db_status = sqlite3_step(stmt);
	CHECK_DBSTATUS(db_status, sqlite3_finalize(stmt));
	if (result != NULL)
	{
		if (db_status == SQLITE_ROW && sqlite3_data_count(stmt)>=1)
			result->Set(sqlite3_column_value(stmt, 0));
		else
			result->Clear();
	}

#ifdef DATABASE_MODULE_DEBUG
	SqlValue test;
	if (db_status == SQLITE_ROW && sqlite3_data_count(stmt)>=1)
		test.Set(sqlite3_column_value(stmt, 0));
	if (test.Type() == SqlValue::TYPE_INTEGER)
		DB_DBG(("%p: SqlTransaction::ExecQuickQuery(%d) : %s\n", this, test.IntegerValue(), sql))
	else
		DB_DBG(("%p: SqlTransaction::ExecQuickQuery() : %s\n", this, sql))
#endif //DATABASE_MODULE_DEBUG

	sqlite3_finalize(stmt);

	return OpStatus::OK;
}

OP_STATUS
SqlTransaction::FetchObjectCount()
{
	if (!IsMemoryOnly() && !WillFileBeDeleted())
		RETURN_IF_ERROR(GetObjectCount(&GetDatabase()->m_cached_object_count));

	return OpStatus::OK;
}

#define SQL_GET_OBJECT_COUNT "SELECT count(0) FROM sqlite_master WHERE type != 'table' OR name != 'sqlite_sequence'"

OP_STATUS
SqlTransaction::GetObjectCount(unsigned *object_count)
{
	OP_ASSERT(object_count != NULL);

	SqlValue object_count_sv;
	RETURN_IF_ERROR(ExecQuickQuery(SQL_STR_PAIR(SQL_GET_OBJECT_COUNT), &object_count_sv, NULL));
	*object_count = static_cast<unsigned>(object_count_sv.IntegerValue());

	return OpStatus::OK;
}

OP_STATUS SqlTransaction::CalculateAvailableDataSize(OpFileLength* available_size)
{
	OP_ASSERT(available_size != NULL);

	OpFileLength global_used_size = 0;
	OpFileLength global_policy_size = 0;

	OpFileLength origin_used_size = 0;
	OpFileLength origin_policy_size = 0;

	OpFileLength this_used_size = 0;

	PS_IndexEntry* key = GetDatabase()->GetIndexEntry();
	Window* win = GetCurrentStatement() != NULL ? GetCurrentStatement()->GetWindow() : NULL;

	if (PS_Policy::KQuotaAllow == key->GetPolicyAttribute(PS_Policy::KOriginExceededHandling, win))
	{
		*available_size = FILE_LENGTH_NONE;
		return OpStatus::OK;
	}

	origin_policy_size = key->GetPolicyAttribute(PS_Policy::KOriginQuota, win);
	if (origin_policy_size == 0 || origin_policy_size == FILE_LENGTH_NONE)
	{
		*available_size = origin_policy_size;
		return OpStatus::OK;
	}

	global_policy_size = key->GetPolicyAttribute(PS_Policy::KGlobalQuota, win);
	if (global_policy_size == 0)
	{
		*available_size = global_policy_size;
		return OpStatus::OK;
	}

	RETURN_IF_ERROR(key->GetPSManager()->GetGlobalDataSize(&global_used_size, key->GetType(), key->GetUrlContextId(), NULL));
	RETURN_IF_ERROR(key->GetPSManager()->GetGlobalDataSize(&origin_used_size, key->GetType(), key->GetUrlContextId(), key->GetOrigin()));

	RETURN_IF_ERROR(GetSize(&this_used_size));

	//1st get size used by all the other databases, excluding the size of this one
	if (global_used_size >= this_used_size)
		global_used_size = global_used_size - this_used_size;
	else
		global_used_size = 0;

	if (origin_used_size >= this_used_size)
		origin_used_size = origin_used_size - this_used_size;
	else
		origin_used_size = 0;

	//2nd subtract global used size from global quota to see if there is some global space left
	//and subtract origin used size from origin quota to see if there is some origin space left
	if (global_used_size < global_policy_size)
		global_used_size = (global_policy_size - global_used_size);
	else
		global_used_size = 0;

	if (origin_used_size < origin_policy_size)
		origin_used_size = (origin_policy_size - origin_used_size);
	else
		origin_used_size = 0;

	//3rd choose what's smaller - available global space, or available origin space
	if (origin_used_size < global_used_size)
		*available_size = origin_used_size;
	else
		*available_size = global_used_size;

	DB_DBG_2(("%p: SqlTransaction::CalculateAvailableDataSize() : %u vs %u\n", this, (unsigned)*available_size, (unsigned)this_used_size))

	return OpStatus::OK;
}



#undef CHECK_DBSTATUS

#define STR_PRAGMA_GET_PAGE_COUNT "PRAGMA page_count"
#define STR_PRAGMA_GET_PAGE_SIZE "PRAGMA page_size"
#define STR_PRAGMA_GET_MAX_PAGE_COUNT "PRAGMA max_page_count"
#define STR_PRAGMA_SET_MAX_PAGE_COUNT "PRAGMA max_page_count = "

OP_STATUS
SqlTransaction::GetPageSize(unsigned *page_size)
{
	INTEGRITY_CHECK();
	OP_ASSERT(page_size != NULL);

	if (m_cached_page_size == 0)
	{
		SqlValue page_size_value;
		SQLiteErrorCode error_code = SQLITE_OK;

		RETURN_IF_ERROR(SIGNAL_OP_STATUS_ERROR(ExecQuickQuery(SQL_STR_PAIR(STR_PRAGMA_GET_PAGE_SIZE), &page_size_value, &error_code)));
		OP_ASSERT(error_code == SQLITE_OK);
		OP_ASSERT(page_size_value.Type() == SqlValue::TYPE_INTEGER);
		m_cached_page_size = static_cast<unsigned>(page_size_value.IntegerValue());
	}
	*page_size = m_cached_page_size;
	return OpStatus::OK;
}

OP_STATUS
SqlTransaction::GetSize(OpFileLength *db_size)
{
	INTEGRITY_CHECK();
	OP_ASSERT(db_size != NULL);

	if (GetFlag(HAS_CACHED_DATA_SIZE))
	{
		*db_size = m_cached_data_size;
		return OpStatus::OK;
	}

	unsigned page_count, page_size;
	OP_STATUS status = GetPageCount(&page_count);
	if (OpStatus::IsSuccess(status))
	{
		if (page_count > 0)
		{
			status = GetPageSize(&page_size);
			if (OpStatus::IsSuccess(status))
			{
				m_cached_data_size = page_size;
				m_cached_data_size *= static_cast<OpFileLength>(page_count);
				SetFlag(HAS_CACHED_DATA_SIZE);
				*db_size = m_cached_data_size;
			}
		}
		else
		{
			SetFlag(HAS_CACHED_DATA_SIZE);
			m_cached_data_size = 0;
			*db_size = m_cached_data_size;
		}
	}
	return SIGNAL_OP_STATUS_ERROR(status);
}

OP_STATUS
SqlTransaction::SetMaxSize(OpFileLength db_size)
{
	INTEGRITY_CHECK();
	if (db_size == 0)
	{
		return SetMaxPageCount(0);
	}
	else if (db_size != FILE_LENGTH_NONE)
	{
		unsigned page_size;
		RETURN_IF_ERROR(GetPageSize(&page_size));
		OpFileLength page_size_64 = page_size;
		unsigned calc_page_count = static_cast<unsigned>((db_size + page_size_64 - 1) / page_size_64);

		return SetMaxPageCount(calc_page_count);
	}
	else
		return SetMaxPageCount(SQLITE_MAX_PAGE_COUNT);
}

OP_STATUS
SqlTransaction::GetPageCount(unsigned *page_count)
{
	INTEGRITY_CHECK();
	OP_ASSERT(page_count != NULL);
	SqlValue page_count_value;
	SQLiteErrorCode error_code = SQLITE_OK;
	OP_STATUS status = ExecQuickQuery(SQL_STR_PAIR(STR_PRAGMA_GET_PAGE_COUNT), &page_count_value, &error_code);
	OP_ASSERT(error_code == SQLITE_OK);
	OP_ASSERT(page_count_value.Type() == SqlValue::TYPE_INTEGER);
	*page_count = static_cast<unsigned>(page_count_value.IntegerValue());
	return SIGNAL_OP_STATUS_ERROR(status);
}

OP_STATUS
SqlTransaction::SetMaxPageCount(unsigned page_count)
{
	INTEGRITY_CHECK();

	//sqlite bug.. 0 is disrespected
	if (page_count == 0)
		page_count = 1;

	char query[sizeof(STR_PRAGMA_SET_MAX_PAGE_COUNT) + 12]; /* ARRAY OK 2012-03-22 joaoe */
	int length = op_snprintf(query, ARRAY_SIZE(query), "%s%u", STR_PRAGMA_SET_MAX_PAGE_COUNT, page_count);

	SQLiteErrorCode error_code = SQLITE_OK;
	OP_STATUS status = ExecQuickQuery(query, length, NULL, &error_code);
	OP_ASSERT(error_code == SQLITE_OK);

	return SIGNAL_OP_STATUS_ERROR(status);
}

/**********************
 * SqlTransaction::SavedStmtOverrideCallback
***********************/
SqlTransaction::SavedStmtOverrideCallback::~SavedStmtOverrideCallback()
{
	if (m_original_statement != NULL)
		DiscardCallback();
	OP_ASSERT(m_original_statement == NULL);
}

void
SqlTransaction::SavedStmtOverrideCallback::Set(SqlStatement* original_statement)
{
	if (original_statement != NULL && original_statement->GetCallback() == this)
	{
		//this happens if the statement to back up is already using
		//this callback and the former has not been called and then cleared
		//because meanwhile the statement got interrupted due to a
		//quota error and sqlite abort
		return;
	}

	OP_ASSERT((original_statement == NULL) || (original_statement != NULL && m_original_statement == NULL));

	DB_DBG_2(("%p: SavedStmtOverrideCallback::Set(%p -> %p)\n", m_transaction, m_original_statement, original_statement))

	m_original_statement = original_statement;
	if (original_statement != NULL)
	{
#ifdef _DEBUG
		OP_ASSERT(!original_statement->HasExecutedOnce());
#endif
		if (original_statement->GetCallback() == NULL)
			DiscardCallback();
	}
}

OP_STATUS
SqlTransaction::SavedStmtOverrideCallback::HandleCallback(SqlResultSet *result_set)
{
	m_called_handle = TRUE;
	OP_DELETE(result_set);
	return OpStatus::OK;
}

OP_STATUS
SqlTransaction::SavedStmtOverrideCallback::HandleError(OP_STATUS error, const uni_char* error_message, BOOL is_fatal)
{
	m_called_handle = TRUE;
	OP_STATUS status = OpStatus::OK;

	if (GetOverrideCallback() != NULL)
		status = GetOverrideCallback()->HandleError(error, error_message, is_fatal);
	else
		OP_DELETEA(error_message);

	DiscardCallback();
	return status;
}

Window*
SqlTransaction::SavedStmtOverrideCallback::GetWindow()
{
	return GetOverrideCallback() != NULL ? GetOverrideCallback()->GetWindow() : NULL;
}

OpFileLength
SqlTransaction::SavedStmtOverrideCallback::GetPreferredSize()
{
	return GetOverrideCallback() != NULL ? GetOverrideCallback()->GetPreferredSize() : 0;
}

const uni_char*
SqlTransaction::SavedStmtOverrideCallback::GetDisplayName()
{
	return GetOverrideCallback() ? GetOverrideCallback()->GetDisplayName() : NULL;
}

void
SqlTransaction::SavedStmtOverrideCallback::Discard()
{
	if (!m_called_handle)
	{
		// A call to Discard() without a HandleError() or HandleCallback means core shutdown.
		DiscardCallback();
	}

	m_called_handle = FALSE;

	OP_ASSERT(m_transaction->GetCurrentStatement() != NULL);
	// Need to find out if the current statement is the last one that uses this
	// callback. If so, eliminate references to the original callback.
	SqlStatement* next_stmt = m_transaction->GetCurrentStatement()->Suc();
	if (next_stmt == NULL || next_stmt->GetCallback() != this)
		Set(NULL);
}

void
SqlTransaction::SavedStmtOverrideCallback::DiscardCallback()
{
	if (m_original_statement != NULL)
		m_original_statement->DiscardCallback();
	Set(NULL);
}

/**********************
 * WSD_Database
***********************/
/*static*/OP_STATUS
WSD_Database::GetInstance(const uni_char* origin, const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id, WSD_Database** result)
{
	OP_ASSERT(result != NULL);

	PS_Object* ps_obj;
	RETURN_IF_ERROR(PS_Object::GetInstance(KWebDatabases, origin, name, is_persistent, context_id, &ps_obj));

	INTEGRITY_CHECK_P(static_cast<WSD_Database*>(ps_obj));
	*result = static_cast<WSD_Database*>(ps_obj);

	return OpStatus::OK;
}

/*static*/OP_STATUS
WSD_Database::DeleteInstance(const uni_char* origin, const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id)
{
	return PS_Object::DeleteInstance(KWebDatabases, origin, name, is_persistent, context_id);
}

/*static*/OP_STATUS
WSD_Database::DeleteInstances(URL_CONTEXT_ID context_id, const uni_char *origin, BOOL only_persistent)
{
	return PS_Object::DeleteInstances(KWebDatabases, context_id, origin, only_persistent);
}

/*static*/OP_STATUS
WSD_Database::DeleteInstances(URL_CONTEXT_ID context_id, BOOL only_persistent)
{
	return PS_Object::DeleteInstances(KWebDatabases, context_id, only_persistent);
}

WSD_Database::WSD_Database(PS_IndexEntry* entry)
	: PS_Object(entry)
	, m_cached_object_count(UINT_MAX)
	, m_current_transaction(NULL)
{
	SetFlag(OBJ_INITIALIZED);
}

WSD_Database::~WSD_Database()
{
	OP_ASSERT(!GetFlag(BEING_DELETED));
	SetFlag(BEING_DELETED);
	FireShutdownCallbacks();
	OpStatus::Ignore(Close(TRUE));
	OP_ASSERT(!IsOpened());
	if (GetIndexEntry() != NULL)
	{
		OP_ASSERT(GetRefCount() == 0);
		if (GetFlag(INITED_MESSAGE_QUEUE) && GetMessageHandler() != NULL)
		{
			GetMessageHandler()->UnsetCallBacks(this);
		}
	}
	HandleObjectShutdown(CanBeDropped());
}

class QuickShutdownNotifier: public PS_ObjDelListener::ResourceShutdownCallback
{
public:
	BOOL was_deleted;
	QuickShutdownNotifier() : was_deleted(FALSE) {}
	virtual void HandleResourceShutdown() {was_deleted = TRUE;}
};

void
WSD_Database::HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2)
{
	OP_ASSERT(reinterpret_cast<WSD_Database *>(par1) == this);

	//after the following function calls, this database might not be accessible anymore
	//because the callbacks might Release() it which will cause it to be deleted if ref counter gets to 0
	//However, if the functions ask for this operation to yield, then it's unlikely it'll be deleted
	QuickShutdownNotifier sh_notifier;

	OP_STATUS status = OpStatus::OK;
	switch(msg)
	{
	case MSG_DATABASE_EXECUTE_TRANSACTION:
	{
		UnsetFlag(HAS_POSTED_EXECUTION_MSG);

		if (!IsOperaRunning())
			return;

		if (GetQuotaHandlingStatus()  == PS_IndexEntry::QS_WAITING_FOR_USER)
			return;

		//if there is more stuff to process
		m_current_transaction = GetNextTransactionToExecute();
		if (m_current_transaction != NULL && m_current_transaction->HasPendingStatements())
		{
			//if the transaction does not have pending statements then
			//wait until a new ExecuteSql is done, hence HasPendingStatements()

			unsigned timeout_for_next_execution = 0;

			AddShutdownCallback(&sh_notifier);

			status = m_current_transaction->ProcessNextStatement();
			if (status == PS_Status::ERR_SUSPENDED)
			{
				return;
			}

			if (status == PS_Status::ERR_YIELD)
			{
#ifdef DEBUG
				m_current_transaction->VerifyIntegrity();
#endif
				//if yielding is requested, then messages are posted with higher intervals,
				//so the rest of the core has enough time to process stuff
				SqlStatement *stmt = m_current_transaction->GetCurrentStatement();
				OP_ASSERT(stmt != NULL);

				unsigned m_number_of_yields = stmt->m_number_of_yields++;
				timeout_for_next_execution = 1;
				while (m_number_of_yields-- > 0)
					timeout_for_next_execution *= 2;
				timeout_for_next_execution *= 10;
			}
			if (!sh_notifier.was_deleted && GetNextTransactionToExecute() != NULL)
				status = OpDbUtils::GetOpStatusError(status, ScheduleTransactionExecute(timeout_for_next_execution));

		}
		break;
	}
	case MSG_DATABASE_TRANSACTION_CLOSE:
	{
		SqlTransaction *transaction = reinterpret_cast<SqlTransaction*>(par2);
		OP_ASSERT(transaction != NULL);
		INTEGRITY_CHECK_P(transaction);

		AddShutdownCallback(&sh_notifier);

		transaction->UnsetFlag(SqlTransaction::BEEN_POSTED_CLOSE_MESSAGE);
		status = transaction->Close();

		break;
	}
	default:
		OP_ASSERT(!"Warning: unhandled message received");
	}
	if (!sh_notifier.was_deleted)
		ReportCondition(status);
}

OP_STATUS WSD_Database::InitMessageQueue()
{
	if (GetFlag(INITED_MESSAGE_QUEUE))
		return OpStatus::OK;

	if (GetMessageHandler() == NULL)
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);

	OpMessage messages[2] = {MSG_DATABASE_EXECUTE_TRANSACTION, MSG_DATABASE_TRANSACTION_CLOSE};

	OP_STATUS status = GetMessageHandler()->SetCallBackList(this, GetMessageQueueId(), messages, 2);

	if (OpStatus::IsSuccess(status))
		SetFlag(INITED_MESSAGE_QUEUE);
	else
		GetMessageHandler()->UnsetCallBacks(this);

	return SIGNAL_OP_STATUS_ERROR(status);
}

SqlTransaction*
WSD_Database::GetNextTransactionToExecute() const
{
	SqlTransaction *previous_current = m_current_transaction, *new_current = m_current_transaction;
	if (new_current == NULL)
		previous_current = new_current = m_transactions.First();

	if (new_current == NULL)
		return NULL;

	//this first cycle look for transactions with results or a rollback segment
	//sync transactions don't matter
	do
	{
#ifdef DEBUG
		new_current->VerifyIntegrity();
#endif
		if (!new_current->IsSynchronous())
		{
			if (new_current->GetFlag(SqlTransaction::HAS_ROLLBACK_SEGMENT) ||
				new_current->IsCurrentStatementStarted())
				return new_current;
		}
		new_current = new_current->Suc();
		if (new_current == NULL)
			new_current = m_transactions.First();

	}while(previous_current != new_current);

	OP_ASSERT(new_current != NULL);

	//then find one transaction
	do{
#ifdef DEBUG
		new_current->VerifyIntegrity();
#endif
		if (!new_current->IsSynchronous())
		{
			if (new_current->HasPendingStatements())
				return new_current;
		}
		new_current = new_current->Suc();
		if (new_current == NULL)
			new_current = m_transactions.First();

	}while(previous_current != new_current);

	//no interesting transactions, quit
	return NULL;
}

void
WSD_Database::OnTransactionTermination(SqlTransaction* transaction)
{
	if (m_current_transaction == transaction)
		m_current_transaction = transaction->Pred();

	SafeDelete();
}

bool
WSD_Database::SafeDelete()
{
	if (!IsOpened() &&
		GetRefCount() == 0 &&
		!IsBeingDeleted() &&
		!IsIndexBeingDeleted())
	{
		OP_DELETE(this);
		return true;
	}
	return false;
}

/** Deletes this object is it's not opened and not being referenced
 *  nor already deleted.
 *  @retval true if the object was deleted
 *  @retval false if the object was not deleted. */
bool SafeDelete();

OP_STATUS
WSD_Database::ScheduleTransactionExecute(unsigned timeout)
{
	if (!IsOperaRunning() || GetMessageHandler() == NULL)
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);

	if (GetFlag(HAS_POSTED_EXECUTION_MSG))
		return OpStatus::OK;

	RETURN_IF_ERROR(InitMessageQueue());
	timeout = MIN(timeout,2000);
	OP_STATUS status = GetMessageHandler()->PostMessage(MSG_DATABASE_EXECUTE_TRANSACTION,
			GetMessageQueueId(),0,timeout) ? OpStatus::OK : SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);
	if (OpStatus::IsSuccess(status))
		SetFlag(HAS_POSTED_EXECUTION_MSG);
	return status;
}

/*virtual*/ BOOL
WSD_Database::PreClearData()
{
	//this function is called by PS_IndexEntry::Delete
	//which already calls DeleteDataFile() which is good enough
	return FALSE;
}

/* virtual */ BOOL
WSD_Database::HasPendingActions() const
{
	return IsOpened();
}

OP_STATUS
WSD_Database::Release()
{
	OP_ASSERT(GetIndexEntry() != NULL);
	OP_ASSERT(GetIndexEntry()->GetRefCount() > 0);

	GetIndexEntry()->DecRefCount();
	if (GetIndexEntry()->GetRefCount() == 0)
		return Close(FALSE);

	return OpStatus::OK;
}

OP_STATUS
WSD_Database::Close(BOOL synchronous)
{
	if (!IsInitialized())
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);

	if (SafeDelete())
		return OpStatus::OK;

	if (!IsOpened())
		return OpStatus::OK;

	if (GetFlag(SHUTTING_DOWN) && !synchronous)
		return OpStatus::OK;

	//this assert warns that the object might leak
	OP_ASSERT(!"an WSD_Database might leak!" || GetRefCount() == 0  || synchronous);

	SetFlag(SHUTTING_DOWN);

	OP_STATUS status = OpStatus::OK;
	if (synchronous)
	{
		SqlTransaction* elem = m_transactions.First(), *next_elem;
		OP_ASSERT(elem != NULL); // Guaranteed by IsOpened().
		while(elem != NULL)
		{
			next_elem = elem->Suc();
			elem->SetFlag(SqlTransaction::CLOSE_SHOULD_DELETE | SqlTransaction::SYNCHRONOUS_TRANSACTION);
			status = OpDbUtils::GetOpStatusError(elem->Close(),status);
			elem = next_elem;
		}
		OP_ASSERT(!IsOpened());

		UnsetFlag(SHUTTING_DOWN);
		SafeDelete();
	}
	else
	{
		SqlTransaction* elem = m_transactions.First(), *next_elem;
		OP_ASSERT(elem != NULL);//garanteed by IsOpened
		while(elem != NULL)
		{
			next_elem = elem->Suc();
			elem->SetFlag(SqlTransaction::CLOSE_SHOULD_DELETE);
			status = OpDbUtils::GetOpStatusError(elem->Close(),status);
			elem = next_elem;
		}
	}
	return SIGNAL_OP_STATUS_ERROR(status);
}

#ifdef SELFTEST
SqlTransaction*
WSD_Database::CreateTransactionSync(BOOL read_only, const uni_char* expected_version)
{
	SqlTransaction* transaction = SqlTransaction::Create(this, read_only, TRUE, expected_version);
	if (transaction != NULL)
		transaction->Into(&m_transactions);
	return transaction;
}
#endif //SELFTEST

SqlTransaction*
WSD_Database::CreateTransactionAsync(BOOL read_only, const uni_char* expected_version)
{
	SqlTransaction* transaction = SqlTransaction::Create(this, read_only, FALSE, expected_version);
	if (transaction != NULL)
		transaction->Into(&m_transactions);
	return transaction;
}

OP_STATUS
WSD_Database::EvalDataSizeSync(OpFileLength *result)
{
	BOOL file_exists = FALSE;
	RETURN_IF_ERROR(FileExists(&file_exists));
	if (file_exists)
	{
		SqlTransaction* transaction = CreateTransactionAsync(TRUE);
		RETURN_OOM_IF_NULL(transaction);
		OP_STATUS status = transaction->GetSize(result);
		OpStatus::Ignore(transaction->Release());
		transaction = NULL;
		return SIGNAL_OP_STATUS_ERROR(status);
	}
	*result = 0;
	return OpStatus::OK;
}

/***************************
* SqlValue
****************************/
OP_STATUS
SqlValue::Set(const SqlValue& value)
{
	switch(value.m_type)
	{
	case TYPE_NULL: Clear(); return OpStatus::OK;
	case TYPE_STRING: return SetString(value.m_packed.m_string_value,value.m_packed.m_value_length);
	case TYPE_INTEGER: SetInteger(value.m_integer_value); return OpStatus::OK;
	case TYPE_DOUBLE: SetDouble(value.m_double_value); return OpStatus::OK;
	case TYPE_BLOB: return SetBlob(value.m_packed.m_blob_value,value.m_packed.m_value_length);
	}
	OP_ASSERT(!"Unknown type");
	return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);
}


OP_STATUS
SqlValue::Set(sqlite3_value* value)
{
	switch (sqlite3_value_type(value))
	{
	case SQLITE_NULL:
		Clear();
		return OpStatus::OK;
	case SQLITE_INTEGER:
		SetInteger(static_cast<sqlite_int_t>(sqlite3_value_int64(value)));
		return OpStatus::OK;
	case SQLITE_FLOAT:
		SetDouble(sqlite3_value_double(value));
		return OpStatus::OK;
	case SQLITE_TEXT:
		return SetString(static_cast<const uni_char*>(sqlite3_value_text16(value)), UNICODE_DOWNSIZE(sqlite3_value_bytes16(value)));
	case SQLITE_BLOB:
		return SetBlob(static_cast<const byte*>(sqlite3_value_blob(value)), sqlite3_value_bytes(value));
	}
	OP_ASSERT(!"Unknown type");
	return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR);
}

OP_STATUS
SqlValue::SetBlob(const byte *value, unsigned value_length)
{
	if (m_type != TYPE_BLOB || m_packed.m_value_length < value_length || value_length == 0)
	{
		if (value_length != 0 && value != NULL) {
			byte* new_value = OP_NEWA(byte, value_length);
			if (new_value == NULL)
				return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);

			Clear();
			m_type = TYPE_BLOB;
			m_packed.m_blob_value = new_value;
			m_packed.m_value_length = value_length;
			op_memcpy(m_packed.m_blob_value, value, value_length);
		}
		else
		{
			Clear();
			m_type = TYPE_BLOB;
		}
	}
	else
	{
		//resuse the current buffer
		OP_ASSERT(m_packed.m_blob_value != NULL);
		op_memcpy(m_packed.m_blob_value, value, value_length);
		m_packed.m_value_length = value_length;
	}
	return OpStatus::OK;
}

OP_STATUS
SqlValue::SetString(const uni_char *value)
{
	return SetString(value, value != NULL ? uni_strlen(value) : 0);
}

OP_STATUS
SqlValue::SetString(const uni_char *value, unsigned value_length)
{
	if (m_type != TYPE_STRING || m_packed.m_value_length < value_length || value_length == 0)
	{
		if (value_length != 0 && value != NULL) {
			uni_char* new_value = OP_NEWA(uni_char, value_length + 1);
			if (new_value == NULL)
				return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY);

			Clear();
			m_type = TYPE_STRING;
			m_packed.m_string_value = new_value;
			m_packed.m_value_length = value_length;
			m_packed.m_string_value[value_length] = 0;
			op_memcpy(m_packed.m_string_value,value,UNICODE_SIZE(value_length));
		}
		else
		{
			Clear();
			m_type = TYPE_STRING;
			m_packed.m_string_value = NULL;
			m_packed.m_value_length = 0;
		}
	}
	else
	{
		OP_ASSERT(m_packed.m_string_value != NULL);
		op_memcpy(m_packed.m_string_value, value, UNICODE_SIZE(value_length));
		m_packed.m_string_value[value_length] = 0;
		m_packed.m_value_length = value_length;
	}
	return OpStatus::OK;
}

void
SqlValue::SetDouble(double value)
{
	Clear();
	m_type = TYPE_DOUBLE;
	m_double_value = value;
}

void
SqlValue::SetInteger(sqlite_int_t value)
{
	Clear();
	m_type = TYPE_INTEGER;
	m_integer_value = value;
}

void
SqlValue::SetBoolean(bool value)
{
	Clear();
	m_type = TYPE_BOOL;
	m_bool_value = value;
}

SQLiteErrorCode
SqlValue::BindToStatement(sqlite3_stmt *stmt, unsigned index) const
{
	OP_ASSERT(stmt != NULL);
	OP_ASSERT(index != 0 || !"indexes start at 1");

	// SQLITE_STATIC is used for strings because the SqlValue
	// is live together with the sqlite3_stmt object.
	switch(m_type)
	{
	case TYPE_NULL:
		return sqlite3_bind_null(stmt, index);
	case TYPE_STRING:
		OP_ASSERT(m_packed.m_string_value != NULL || m_packed.m_value_length == 0);
		return sqlite3_bind_text16(stmt, index,
				m_packed.m_string_value != NULL ? m_packed.m_string_value : UNI_L(""),
				UNICODE_SIZE(m_packed.m_value_length), SQLITE_STATIC);
	case TYPE_INTEGER:
		return sqlite3_bind_int64(stmt, index, m_integer_value);
	case TYPE_DOUBLE:
		return sqlite3_bind_double(stmt, index, m_double_value);
	case TYPE_BLOB:
		return sqlite3_bind_blob(stmt, index, m_packed.m_blob_value, m_packed.m_value_length, SQLITE_STATIC);
	case TYPE_BOOL:
		if (m_bool_value)
			return sqlite3_bind_text(stmt, index, "true", 4, SQLITE_STATIC);
		else
			return sqlite3_bind_text(stmt, index, "false", 5, SQLITE_STATIC);
	}
	OP_ASSERT(!"Forgot a type on SqlValue::BindToStatement(SqlValue)");
	return SQLITE_ERROR;
}

void
SqlValue::Clear()
{
	if (m_type == TYPE_STRING)
		OP_DELETEA(m_packed.m_string_value);
	if (m_type == TYPE_BLOB)
		OP_DELETEA(m_packed.m_blob_value);

	m_type = TYPE_NULL;
	m_packed.m_value_length = 0;
}

#define RETURN_FALSE_IF_TRUE(v) do{if(v)return FALSE;}while(0)
#define RETURN_TRUE_IF_TRUE(v) do{if(v)return TRUE;}while(0)
BOOL SqlValue::IsEqual(const SqlValue& value) const
{
	RETURN_TRUE_IF_TRUE(this == &value);
	RETURN_FALSE_IF_TRUE(m_type != value.m_type);
	switch(m_type)
	{
	case TYPE_NULL:
		return TRUE;
	case TYPE_INTEGER:
		return m_integer_value == value.m_integer_value;
	case TYPE_DOUBLE:
		return m_double_value == value.m_double_value;
	case TYPE_STRING:
		RETURN_FALSE_IF_TRUE(m_packed.m_value_length != value.m_packed.m_value_length);
		RETURN_TRUE_IF_TRUE (m_packed.m_string_value == value.m_packed.m_string_value);
		RETURN_FALSE_IF_TRUE(m_packed.m_string_value == NULL || value.m_packed.m_string_value == NULL);
		RETURN_TRUE_IF_TRUE (op_memcmp(m_packed.m_string_value, value.m_packed.m_string_value, UNICODE_SIZE(m_packed.m_value_length)) == 0);
		return FALSE;
	case TYPE_BLOB:
		RETURN_FALSE_IF_TRUE(m_packed.m_value_length != value.m_packed.m_value_length);
		RETURN_TRUE_IF_TRUE (m_packed.m_blob_value == value.m_packed.m_blob_value);
		RETURN_FALSE_IF_TRUE(m_packed.m_blob_value == NULL || value.m_packed.m_blob_value == NULL);
		RETURN_TRUE_IF_TRUE (op_memcmp(m_packed.m_blob_value, value.m_packed.m_blob_value, m_packed.m_value_length) == 0);
		return FALSE;
	default:
		OP_ASSERT(!"Forgot a type on SqlValue::IsEqual(ES_Value)");
	}
	return FALSE;
}


#ifdef DEBUG
void SqlValue::PrettyPrint(TempBuffer& buf)
{
	uni_char num[64] = {0}; /* ARRAY OK joaoe 2009-07-23 */
	buf.Clear();
	switch(m_type)
	{
	case TYPE_STRING:
		buf.Append("SqlValue:string \"");
		buf.Append(m_packed.m_string_value,m_packed.m_value_length);
		buf.Append('"');
		break;
	case TYPE_NULL:
		buf.Append("SqlValue:NULL");
		break;
	case TYPE_INTEGER:
		buf.Append("SqlValue:integer ");
		buf.AppendUnsignedLong(static_cast<unsigned long>(m_integer_value));
		break;
	case TYPE_DOUBLE:
		buf.Append("SqlValue:double ");
		uni_snprintf(num, 63, UNI_L("%lf"), m_double_value);
		buf.Append(num);
		break;
	case TYPE_BLOB:
	{
		buf.Append("SqlValue:blob ");
		for (unsigned j = 0; j < m_packed.m_value_length; j++)
		{
			uni_snprintf(num, 63, UNI_L("0x%2x,"), m_packed.m_blob_value[j]);
			if (num[2]==' ')num[2]='0';
			buf.Append(num);
		}
		break;
	}
	default:
		OP_ASSERT(!"Forgot a type on SqlValue::PrettyPrint(TempBuffer)");
	}
}
#endif


/**********************
 * SqlValueList
***********************/
/* static */ SqlValueList*
SqlValueList::Make(unsigned length)
{
	unsigned size = sizeof(SqlValueList) + (sizeof(SqlValue) * length - 1);
	void *p = op_malloc(size);
	RETURN_VALUE_IF_NULL(p, NULL);

	op_memset(p, 0, size);

	return new (p) SqlValueList(length);
}

void
SqlValueList::Delete()
{
	for (unsigned k = 0; k < length; k++)
		values[k].Clear();
	op_free(this);
}

/**********************
 * SqlResultSet
***********************/
SqlResultSet::SqlResultSet()
	: m_rs_type(INVALID_RESULT_SET)
	, m_sqlite_error(SQLITE_OK)
{
	op_memset(&m_data,0,sizeof(m_data));
}

SqlResultSet::~SqlResultSet()
{
	if (IsIterable())
	{
		Close();
		if (m_data.cursor_data.column_names != NULL)
		{
			for (UINT32 i = 0, count = GetColumnCount(); i < count; i++)
				OP_DELETEA(m_data.cursor_data.column_names[i]);
			OP_DELETEA(m_data.cursor_data.column_names);
		}
		if (m_data.cursor_data.row_list != NULL)
		{
			OpVector<SqlValue>*& row_list = m_data.cursor_data.row_list;
			for (unsigned k = 0, lim = row_list->GetCount(); k < lim; k++)
				OP_DELETEA(row_list->Get(k));
			OP_DELETE(row_list);
		}
	}
	if (m_sql_statement != NULL)
		m_sql_statement->m_result_set = NULL;
	m_rs_type = INVALID_RESULT_SET;
	m_sqlite_error = SQLITE_OK;
	op_memset(&m_data,0,sizeof(m_data));
}

void
SqlResultSet::Close()
{
	if (IsIterable() && GetStatementObj() != NULL)
	{
		BOOL sqlite_aborted = sqlite3_finalize(GetStatementObj()) == SQLITE_ABORT;
		m_data.cursor_data.sqlite_stmt_obj = NULL;
		if (m_sql_statement != NULL)
			m_sql_statement->OnResultSetClosed(sqlite_aborted);
	}
}

BOOL
SqlResultSet::GetColumnName(unsigned index, const uni_char** value) const
{
	OP_ASSERT(IsIterable());
	if (!IsIterable() || index >= GetColumnCount())
		return FALSE;
	OP_ASSERT(value != NULL);

	if (m_data.cursor_data.column_names != NULL)
	{
		*value = m_data.cursor_data.column_names[index];
	}
	else
	{
		OP_ASSERT(GetStatementObj() != NULL);
		*value = (const uni_char*)sqlite3_column_name16(GetStatementObj(), index);
	}
	return TRUE;
}

BOOL
SqlResultSet::GetColumnIndex(const uni_char* name, unsigned *value) const
{
	OP_ASSERT(name != NULL);
	OP_ASSERT(value != NULL);
	OP_ASSERT(IsIterable());
	if (!IsIterable())
		return FALSE;
	unsigned column_count = GetColumnCount();

	if (m_data.cursor_data.column_names != NULL)
	{
		for (unsigned k = 0; k < column_count; k++)
		{
			OP_ASSERT(m_data.cursor_data.column_names[k] != NULL);
			if (uni_str_eq(name, m_data.cursor_data.column_names[k]) )
			{
				*value = k;
				return TRUE;
			}
		}

	}
	else
	{
		OP_ASSERT(GetStatementObj() != NULL);
		for (unsigned k = 0; k < column_count; k++)
		{
			if (uni_str_eq(name, (const uni_char*)sqlite3_column_name16(GetStatementObj(), k)) )
			{
				*value = k;
				return TRUE;
			}
		}
	}
	return FALSE;
}

OP_STATUS
SqlResultSet::Step(BOOL *has_data)
{
	OP_ASSERT(has_data != NULL);
	TRAPD(status, *has_data = StepL());
	return status;
}

BOOL
SqlResultSet::StepL()
{
	OP_ASSERT(IsIterable());
	if (!IsIterable())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NOT_SUPPORTED));

	StoreColumnNamesL();

	//the 1st step is actually done by SqlTransaction, so this is just a dummy
	//to tell if it actually stepped
	if (!GetIterationFlag(DID_1ST_STEP))
	{
		StoreCurrentRowL();
		m_data.cursor_data.row_count++;
		SetIterationFlag(DID_1ST_STEP);
		return TRUE;
	}
	//no statement object means we've finished and cleaned it up
	if (GetStatementObj() == NULL)
		return FALSE;

	if (m_sql_statement != NULL)
	{
		m_sql_statement->m_sw.Continue();
		do{
			m_sqlite_error = GetIterationFlag(HAS_AT_LEAST_ONE_ROW) ? sqlite3_step(GetStatementObj()) : SQLITE_DONE;
		}
		while(!IsInterruptable() && !m_sql_statement->HasTimedOut() && m_sqlite_error == SQLITE_INTERRUPT);

		m_sql_statement->m_sw.Stop();
	}
	else
	{
		m_sqlite_error = GetIterationFlag(HAS_AT_LEAST_ONE_ROW) ? sqlite3_step(GetStatementObj()) : SQLITE_DONE;
	}

	switch(m_sqlite_error)
	{
		case SQLITE_ROW:
			StoreCurrentRowL();
			m_data.cursor_data.row_count++;
			return TRUE;
		case SQLITE_DONE:
			Close();
			return FALSE;
	}
	LEAVE(SIGNAL_OP_STATUS_ERROR(g_database_manager->SqliteErrorCodeToOpStatus(m_sqlite_error)));
	return FALSE;//for the compiler not to complain
}

OP_STATUS
SqlResultSet::EnableCaching(unsigned max_size_bytes)
{
	if (!IsIterable())
		return OpStatus::OK;
	if (GetIterationFlag(CACHING_ENABLED))
		return OpStatus::OK;

	//must not have parsed any row
	OP_ASSERT(m_data.cursor_data.row_count == 0);

	SetIterationFlag(CACHING_ENABLED);

	m_data.cursor_data.max_allowed_size = max_size_bytes;

	TRAPD(status, StoreColumnNamesL());

	return SIGNAL_OP_STATUS_ERROR(status);
}

OP_STATUS
SqlResultSet::GetValueAtIndex(unsigned index, SqlValue* value) const
{
	OP_ASSERT(IsIterable());

	if (!IsIterable())
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NOT_SUPPORTED);

	if (index >= GetColumnCount() || GetStatementObj() == NULL)
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE);

	OP_ASSERT(value != NULL);
	return value->Set(sqlite3_column_value(GetStatementObj(), index));
}

OP_STATUS
SqlResultSet::GetValueAtColumn(const uni_char* column_name, SqlValue* value) const
{
	OP_ASSERT(IsIterable());
	unsigned index;

	if (!IsIterable())
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NOT_SUPPORTED);

	if (GetStatementObj() == NULL || !GetColumnIndex(column_name,&index))
		return SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE);

	OP_ASSERT(value != NULL);
	return value->Set(sqlite3_column_value(GetStatementObj(), index));
}

unsigned
SqlResultSet::GetCachedLengthL()
{
	// Do not use this function without caching enabled
	// else the result set will be depleted
	OP_ASSERT(IsCachingEnabled());

	PrefetchRowL();
	return m_data.cursor_data.row_count;
}

OP_STATUS
SqlResultSet::GetCachedLength(unsigned *length)
{
	TRAPD(status, *length = GetCachedLengthL());
	return status;
}

const SqlValue*
SqlResultSet::GetCachedValueAtIndexL(unsigned row_index, unsigned col_index)
{
	OP_ASSERT(IsCachingEnabled());

	if (!IsCachingEnabled())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NOT_SUPPORTED));

	if (col_index >= GetColumnCount())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE));

	PrefetchRowL(row_index);

	if (GetRowCount() <= row_index)
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE));

	OP_ASSERT(m_data.cursor_data.row_list != NULL);
	OP_ASSERT(m_data.cursor_data.row_list->GetCount() == GetRowCount());
	OP_ASSERT(m_data.cursor_data.row_list->Get(row_index) != NULL);

	return m_data.cursor_data.row_list->Get(row_index) + col_index;
}

const SqlValue*
SqlResultSet::GetCachedValueAtColumnL(unsigned row_index, const uni_char* column_name)
{
	OP_ASSERT(IsCachingEnabled());

	if (!IsCachingEnabled())
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NOT_SUPPORTED));

	unsigned col_index = 0xffffffff;
	if (!GetColumnIndex(column_name, &col_index))
		LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_OUT_OF_RANGE));

	return GetCachedValueAtIndexL(row_index, col_index);
}

OP_STATUS
SqlResultSet::GetCachedValueAtIndex(unsigned row_index, unsigned col_index, const SqlValue*& value)
{
	TRAPD(status, value = GetCachedValueAtIndexL(row_index, col_index));
	return status;
}

OP_STATUS
SqlResultSet::GetCachedValueAtColumn(unsigned row_index, const uni_char* column_name, const SqlValue*& value)
{
	TRAPD(status, value = GetCachedValueAtColumnL(row_index, column_name));
	return status;
}

void
SqlResultSet::PrefetchRowL(unsigned row_index)
{
	if (STEP_BLOCK_SIZE > 1)
		row_index = (row_index / STEP_BLOCK_SIZE + 1) * STEP_BLOCK_SIZE;

	while(m_data.cursor_data.row_count <= row_index && StepL()) {}
}

void
SqlResultSet::StoreCurrentRowL()
{
	if (!IsCachingEnabled())
		return;

	OpVector<SqlValue>*& row_list = m_data.cursor_data.row_list;

	if (row_list == NULL)
	{
		row_list = OP_NEW_L(OpVector<SqlValue>, ());
		m_data.cursor_data.cached_size_bytes += sizeof(*row_list);
	}
	SqlValue* current_row = row_list->Get(m_data.cursor_data.row_count);
	if (current_row != NULL)
		//row exists -> quit
		return;

	if (HasSizeOverflown())
		LEAVE(PS_Status::ERR_RSET_TOO_BIG);

	unsigned column_count = GetColumnCount();

	current_row = OP_NEWA_L(SqlValue, column_count);

	OP_STATUS status = row_list->Insert(m_data.cursor_data.row_count, current_row);
	if (OpStatus::IsError(status))
	{
		OP_DELETEA(current_row);
		LEAVE(status);
	}

	for (unsigned k = 0; k < column_count; k++)
	{
		LEAVE_IF_ERROR(SIGNAL_OP_STATUS_ERROR(current_row[k].Set(sqlite3_column_value(GetStatementObj(), k))));
		m_data.cursor_data.cached_size_bytes += current_row[k].SizeOf();
		if (HasSizeOverflown())
			LEAVE(SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_RSET_TOO_BIG));
	}
}

void
SqlResultSet::StoreColumnNamesL()
{
	if (!IsCachingEnabled())
		return;

	uni_char**& column_names = m_data.cursor_data.column_names;
	if (column_names != NULL)
		return;

	if (HasSizeOverflown())
		LEAVE(PS_Status::ERR_RSET_TOO_BIG);

	unsigned acum_size = 0;
	unsigned column_count = GetColumnCount();

	column_names = OP_NEWA_L(uni_char*,column_count);
	acum_size += sizeof(uni_char*) * column_count;

	for (unsigned k = 0; k < column_count; k++)
	{
		column_names[k] = UniSetNewStr((uni_char*)sqlite3_column_name16(GetStatementObj(), k));
		if (column_names[k] == NULL)
		{
			for (; k > 0; k--)
				OP_DELETEA(column_names[k - 1]);
			OP_DELETEA(column_names);
			column_names = NULL;
			LEAVE(SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_MEMORY));
		}
		acum_size += uni_strlen(column_names[k]);
	}
	//this is an approximate size of the internal array necessary to hold all the pointers
	//not accurate, but good enough
	m_data.cursor_data.cached_size_bytes += acum_size + column_count * sizeof(uni_char*);
	if (HasSizeOverflown())
		LEAVE(SIGNAL_OP_STATUS_ERROR(PS_Status::ERR_RSET_TOO_BIG));
}

void
SqlResultSet::SetStatement(SqlStatement *stmt, sqlite3_stmt *sqlite_stmt, BOOL has_at_least_one_row)
{
	if (stmt == NULL)
	{
		//this is to clear the object
		m_rs_type = INVALID_RESULT_SET;
		m_sql_statement = stmt;
		m_data.cursor_data.sqlite_stmt_obj = NULL;
		m_data.cursor_data.cached_size_bytes = 0;
		return;
	}

	OP_ASSERT(m_rs_type == INVALID_RESULT_SET);

	m_rs_type = ITERABLE_RESULT_SET;
	m_sql_statement = stmt;
	m_data.cursor_data.sqlite_stmt_obj = sqlite_stmt;
	m_data.cursor_data.column_count = sqlite3_data_count(sqlite_stmt);
	if (has_at_least_one_row)
	{
		SetIterationFlag(HAS_AT_LEAST_ONE_ROW);
		UnsetIterationFlag(DID_1ST_STEP);
	}
	else
		SetIterationFlag(DID_1ST_STEP);
	m_data.cursor_data.cached_size_bytes = sizeof(*this);
}

void
SqlResultSet::SetDMLData(SqlStatement *stmt, unsigned rows_affected, sqlite_rowid_t last_rowid)
{
	OP_ASSERT(m_rs_type == INVALID_RESULT_SET);
	m_rs_type = DML_RESULT_SET;
	m_sql_statement = stmt;
	m_data.dml_data.last_rowid = last_rowid;
	m_data.dml_data.rows_affected = rows_affected;
}

#endif //SUPPORT_DATABASE_INTERNAL
