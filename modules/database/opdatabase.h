/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULE_DATABASE_OPDATABASE_H_
#define _MODULE_DATABASE_OPDATABASE_H_

#ifdef SUPPORT_DATABASE_INTERNAL

#include "modules/database/src/opdatabase_base.h"
#include "modules/sqlite/sqlite3.h"
#include "modules/database/opdatabasemanager.h"
#include "modules/database/sec_policy.h"
#include "modules/windowcommander/OpWindowCommander.h"

#ifdef HAVE_INT64
	typedef INT64 sqlite_rowid_t;
	typedef INT64 sqlite_int_t;
#else
	typedef int sqlite_rowid_t;
	typedef int sqlite_int_t;
#endif

#define SQLITE_MAX_PAGE_COUNT (2147483647)

struct sqlite3;
struct sqlite3_stmt;
class PS_IndexEntry;
class WSD_Database;
class SqlValue;
class SqlValueList;
class SqlStatement;
class SqlTransaction;
class SqlTransaction_QuotaCallback;
class SqlResultSet;

/**
 * Class which holds a sql query.
 */
class SqlText
{
public:
	enum SqlType {
		CONST_8BIT,
		HEAP_8BIT,
		CONST_16BIT,
		HEAP_16BIT
	};
	const SqlType m_sql_type;
	const union {
		const char *m_sql8;
		const uni_char *m_sql16;
	};
	/** Length of m_sql8/m_sql16 in chars. */
	const unsigned m_sql_length;

	BOOL Is8Bit()  const { return m_sql_type == CONST_8BIT  || m_sql_type == HEAP_8BIT; }
	BOOL Is16Bit() const { return m_sql_type == CONST_16BIT || m_sql_type == HEAP_16BIT; }

	SqlText(SqlType type, const char *sql, unsigned length) : m_sql_type(type), m_sql8(sql), m_sql_length(length) { OP_ASSERT(Is8Bit()); }
	SqlText(SqlType type, const uni_char *sql, unsigned length) : m_sql_type(type), m_sql16(sql), m_sql_length(length) { OP_ASSERT(Is16Bit()); }
};

/**
 * The class SqlStatement holds all the data needed to evaluate a sql query.
 * This class is used only internally by the SqlTransaction class, but API users
 * wanting to execute a query have to implement derived versions of SqlStatement::Callback
 */
class SqlStatement : public ListElement<SqlStatement> DB_INTEGRITY_CHK(0xabcd3456)
{
public:
	/**
	 * Helper class passed to SqlTransaction::executeSql for the
	 * callee to be notified when the statement completes and its
	 * result is available. The class is also used to pass some
	 * seldom needed information to the transaction.
	 *
	 * The callback object will not be owned by the underlying
	 * sql statement but the later will always keep a reference to
	 * the callback while the callback is not used to send the
	 * result back. After the callback is used, the database module
	 * disposes of the callback calling the Discard method. The
	 * callback can be reused by many statements.
	 * If the callback needs to be deleted prematurely, call
	 * SqlTransaction::CancelStatement().
	 */
	class Callback
	{
		friend class SqlStatement;
	public:
		Callback() {}

		/**
		 * Normal dtor. Can be called before the statement completes.
		 * the database code will be aware of it and will not use the
		 * deleted pointer.
		 *
		 * IMPORTANT: deleting the statement object does not cancel
		 * the underlying statement. It will execute normally and any
		 * output will simply not be returned. To cancel the statement
		 * call the Cancel() method explicitly.
		 */
		virtual ~Callback() {}

		/**
		 * Implemented by the callee. If this method returns TRUE then the
		 * transaction logic will fetch all rows into the result set prior
		 * to invoking the success callback. If the result set becomes too big
		 * then PS_Status::ERR_RSET_TOO_BIG will be returned. It's size
		 * is governed by the tweak TWEAK_DATABASE_MAX_RESULT_SET_SIZE.
		 * Else, the result set will be iterable and will only return one row
		 * at each time. To transverse the rows, call StepL
		 */
		virtual BOOL RequiresCaching() const { return FALSE; }

		/**
		 * Implemented by the callee to handle the result set. Note:
		 * the callee must OP_DELETE the result set when it no longer
		 * needs it
		 *
		 * @param result_set        result_set with query result
		 */
		virtual OP_STATUS HandleCallback(SqlResultSet *result_set) = 0;

		/**
		 * Implemented by the callee to handle any error that might have
		 * happened during the statement execution. After this method is called.
		 * the statement is not recoverable, meaning that if the implementation
		 * wants to try again, executeSQL must be called again.
		 * Note: the callee must OP_DELETEA the error_message when it no longer
		 * needs it
		 *
		 * @param error            error code, check PS_Status and OpStatus
		 * @param error_message    string with error detail. might be NULL. callee
		 *                         must delete it
		 * @param is_fatal         tells that the transaction is in a unrecoverable
		 *                         state and will terminate regardless
		 */
		virtual OP_STATUS HandleError(OP_STATUS error, const uni_char* error_message, BOOL is_fatal) = 0;

		/**
		 * Implemented by the callee so that the SqlTransaction logic can access
		 * the origining window of the executeSql request, if any. The Window
		 * will be used for the WindowCommander APIs and give special permissions
		 * to special windows
		 */
		virtual Window* GetWindow() = 0;

		/**
		 * Implemented by the callee to tells the foreseable size needed
		 * for the database when a quota limit is reached.
		 * This is the 4th parameter to window.openDatabase()
		 */
		virtual OpFileLength GetPreferredSize() = 0;

		/**
		 * Implemented by the called to return the display name to show
		 * to the user if quota exceeds and the user agent shows a dialog.
		 *
		 * @return   display name to use. If NULL is returned, then the database
		 *           name will be used as fallback
		 */
		virtual const uni_char* GetDisplayName() = 0;

		/**
		 * This method will be called when the SqlTransaction logic
		 * no longer needs this object. Cleanup should happen only
		 * during of after this call
		 */
		virtual void Discard() = 0;
	};

	/**
	 * Factory method to create SqlStatements.
	 * Because the object created can have an unforeseen life time, it
	 * will manage the data contained the following way:
	 *  -the transaction shall never be deleted before all statements, so the
	 *    pointer to the transaction is assumed to be != NULL, and the
	 *    transaction will not be owned by this object
	 *  - sql and sql_length will be duplicated, so the caller will still
	 *    own the sql string
	 *  - parameters will be taken ownership of, so after creating this
	 *    object, the caller must not keep references to the arguments
	 *    because they will be deleted when the statement terminated
	 *  - the callback obeys the description in the SqlStatement::Callback class
	 *
	 *
	 * @param transaction              SqlTransaction to which this statement will belong
	 * @param sql_text                 The sql query to execute. If the query is a heap string, then ownership
	 *                                 will be taken by the statement and deleted when not needed.
	 * @param parameters               array of SqlValues used as arguments for the query.
	 *                                 The SqlValue* must be NULL to delimit the array
	 * @param should_not_validate      if TRUE, then this query will not be validated by
	 *                                 a sql validator
	 * @param callback                 SqlStatement::Callback object that will control the query
	 *
	 * @return object if all OK, NULL on OOM
	 */
	static SqlStatement *Create(
			SqlTransaction *transaction,
			const SqlText &sql_text,
			SqlValueList *parameters,
			BOOL should_not_validate,
			BOOL no_version_check,
			Callback *callback);

	/**
	 * Normal dtor.
	 * Important: sql statements should be deleted in the same
	 * order they are in the statement queue for reliability sake
	 */
	~SqlStatement();

	/**
	 * Tells if SQL starts with the SELECT keyword
	 *
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsSelectStatement(const uni_char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, UNI_L("SELECT"), 6); }

	/**
	 * Tells if SQL starts with the COMMIT keyword
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsCommitStatement(const uni_char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, UNI_L("COMMIT"), 6); }

	/**
	 * Tells if SQL starts with the BEGIN keyword
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsBeginStatement(const uni_char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, UNI_L("BEGIN"), 5); }

	/**
	 * Tells if SQL starts with the ROLLBACK keyword
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsRollbackStatement(const uni_char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, UNI_L("ROLLBACK"), 8); }

	static BOOL IsStatement(const uni_char* sql, unsigned sql_length, const uni_char* statement_type, unsigned statement_type_length);

	/**
	 * Tells if SQL starts with the SELECT keyword
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsSelectStatement(const char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, "SELECT", 6); }

	/**
	 * Tells if SQL starts with the COMMIT keyword
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsCommitStatement(const char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, "COMMIT", 6); }

	/**
	 * Tells if SQL starts with the BEGIN keyword
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsBeginStatement(const char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, "BEGIN", 5); }

	/**
	 * Tells if SQL starts with the ROLLBACK keyword
	 * @param sql            sql statement to check
	 * @param sql_lengh      sql length
	 */
	inline static BOOL IsRollbackStatement(const char* sql, unsigned sql_length = (unsigned)-1)
	{ return IsStatement(sql, sql_length, "ROLLBACK", 8); }

	static BOOL IsStatement(const char* sql, unsigned sql_length, const char* statement_type, unsigned statement_type_length);

	/**
	 * After the statement has started to be executed, tells if it has
	 * overflown it's allowed execution time. Allowed execution time is
	 * set from the preference
	 */
	inline BOOL HasTimedOut() const { return m_avail_exec_time_ms != 0xffffffff && m_sw.GetEllapsedMS() > m_avail_exec_time_ms; }

	/**
	 * Returns the Window returned by the callback of this statement
	 */
	inline Window* GetWindow() const { return m_callback != NULL ? m_callback->GetWindow() : NULL; }

	/**
	 * Returns the preferred size returned by the callback of this statement
	 */
	inline OpFileLength GetPreferredSize() const { return m_callback != NULL ? m_callback->GetPreferredSize() : 0; }

	/**
	 * Returns the display name for this database if a quota dialog pops up
	 */
	inline const uni_char* GetDisplayName() const { return m_callback != NULL ? m_callback->GetDisplayName() : NULL; }

	/**
	 * Returns the callback object that has been assigned to this statement.
	 * It's the same that was passed to Sqltransaction::ExecuteSql
	 */
	inline Callback* GetCallback() const { return m_callback; }

	/**
	 * Sets a new callback for the statement
	 */
	inline void SetCallback(Callback* v) { m_callback = v; }

	/**
	 * Discards the callback used in this statement
	 */
	void DiscardCallback();

	/**
	 * In some cases, we will report an error back to the callback
	 * and sqlite will somehow cause an implicit rollback. Such thing happens
	 * if a statement object is terminated before completing (timeout case)
	 * or if we get a quota error.
	 * This function tells of such cases and therefore the transaction must be
	 * restarted, but it'll be completely transparent to the API user.
	 * Unrecoverable errors must fail the transaction though.
	 */
	BOOL MustRestartTransaction() const;

	/**
	 * If this statement errored, this function tells if the error can be
	 * recovered from. A non recoverable error is a fatal situation like OOM
	 * or OOD, or a timeout on a DDL/DML.
	 * Version mismatch means that the schema might no longer be in sync.
	 */
	BOOL IsErrorRecoverable() const
	{
		return
			!(!m_is_result_success &&
				(m_result_error_data.m_result_error == OpStatus::ERR_NO_MEMORY ||
				m_result_error_data.m_result_error == OpStatus::ERR_NO_ACCESS ||
				m_result_error_data.m_result_error == PS_Status::ERR_VERSION_MISMATCH));
	}

#ifdef _DEBUG
	BOOL HasExecutedOnce() const { return m_has_executed_once; }
#endif

private:

	friend class Callback;
	friend class WSD_Database;
	friend class SqlTransaction;
	friend class SqlResultSet;

	/**
	 * Instances of this struct will be passed to sqlite3_set_authorizer
	 * See also SqlTransaction::SqliteAuthorizerFlags
	 */
	struct SqliteAuthorizerData
	{
		SqliteAuthorizerData(): m_validator(NULL), m_statement_type(-1), m_flags(0) {}
		const SqlValidator* m_validator;
		int m_statement_type;
		unsigned m_flags;
		inline void SetFlag(unsigned int flag) { m_flags |= flag; }
		inline void UnsetFlag(unsigned int flag) { m_flags &= ~flag; }
		inline BOOL GetFlag(unsigned int flag) const { return (m_flags & flag)!=0; }
	};

	enum SqlStatementState
	{
		///start of enumeration, no special meaning
		STATEMENT_STATE_BEGIN = 0,

		///object just fresh, everything will
		///be processed from scratch
		BEFORE_EXECUTION,

		///m_stmt is created and m_result_set
		///is being filled in, or has m_stmt and
		///is waiting for a data file lock
		DURING_EXECUTION,

		///after the 1st step, this is set for queries which
		///return several rows, and have caching enabled,
		///because the transaction logic must fetch all rows
		///before calling the callback and such may take some
		///time to accomplish
		DURING_STEPPING,

		///result is calculated, just call success or error callback
		///if result set does not have caching, then the database will
		///wait before processing the next statement, until the
		///underlying sqlite3_stmt object is terminated.
		///Such happens when the result set is deleted or all rows are
		///retrieved
		AFTER_EXECUTION,

		///state after statement is cancelled
		///when on this state, the error callback will be called
		CANCELLED,

		///end of enumeration, no special meaning
		STATEMENT_STATE_END
	};

	/**
	 * Default ctor.
	 * See SqlStatement::Create for how data will be owned and managed
	 *
	 * @param transaction           SqlTransaction that owns this statement
	 * @param sql_text              The sql query to execute. If the query is a heap string, then ownership
	 *                              will be taken by the statement and deleted when not needed.
	 * @param parameters            SqlValueList with query parameters, can be NULL.
	 * @param should_not_validate   if statement should skip validation by a SqlValidator
	 * @param callback              callback object to control this statement
	 */
	SqlStatement(
			SqlTransaction *transaction,
			const SqlText &sql_text,
			SqlValueList *parameters,
			BOOL should_not_validate,
			BOOL no_version_check,
			Callback *callback) :
		m_state(BEFORE_EXECUTION),
		m_transaction(transaction),
		m_sql(sql_text),
		m_parameters(parameters),
		m_callback(callback),
		m_data_file_size(0),
		m_stmt(NULL),
		m_number_of_yields(0),
		m_avail_exec_time_ms(0xffffffff),
		m_should_not_validate(!!should_not_validate),
		m_no_version_check(!!no_version_check),
		m_is_result_success(TRUE),
		m_has_called_callbacks(FALSE),
#ifdef _DEBUG
		m_has_executed_once(FALSE),
#endif
		m_result_set(NULL)
	{ OP_ASSERT(transaction != NULL); }

	/**
	 * Sets this statement to cancelled state.
	 * Any pending operations are disregarded, the error callback is called
	 * with PS_Status::ERR_CANCELLED
	 */
	inline void Cancel() { if (!m_has_called_callbacks) m_state = CANCELLED; DiscardCallback(); }

	/**
	 * Sets new statement state. State determines in which phase of query execution the statement is at
	 */
	inline void SetState(SqlStatementState new_state) { OP_ASSERT(m_state<=new_state); m_state = new_state; };

	/**
	 * Resets statement to BEFORE_EXECUTION state and clears
	 * data meanwhile produced. If the statement is in the
	 * AFTER_EXECUTION state, then this is ignored
	 */
	void Reset();

	/**
	 * Tells if the statement has started processing
	 */
	inline BOOL IsStatementStarted() const { return m_state > BEFORE_EXECUTION; }

	/**
	 * Tells if the statement has completed, and only needs
	 * one extra call to SqlTransaction::ProcessNextStatement
	 * to return the results and be deleted
	 */
	BOOL IsStatementCompleted() const;

	/**
	 * Tells if the statement is known to be a select statement
	 */
	inline BOOL IsSelect() const { return m_statement_auth_data.m_statement_type == SQLITE_SELECT; }

	/**
	 * Clears the SqlResultSet and sqlite3_stmt objects
	 * if they're still attached to this object, for instance
	 * if an error happens
	 */
	void TerminateStatementAndResultSet();

	/**
	 * if this statement produces an error that should be notified, then
	 * the Window belonging to the statement is notified
	 */
	void RaiseError();

	/**
	 * Tells if this statement errored and the error should be raised
	 * to the window.
	 */
	BOOL IsRaisable() const
	{
		return !m_is_result_success &&
			(m_result_error_data.m_result_error == OpStatus::ERR_NO_MEMORY ||
			m_result_error_data.m_result_error == OpStatus::ERR_SOFT_NO_MEMORY ||
			m_result_error_data.m_result_error == OpStatus::ERR_NO_DISK);
	}

	/**
	 * Called when the result set is closed, to notify the transaction
	 * that it can proceed to the following statement.
	 *
	 * @param sqlite_aborted  If TRUE, the transaction assumes that sqlite aborted
	 *                        therefore killing the rollback segment, and can update
	 *                        its state.
	 */
	void OnResultSetClosed(BOOL sqlite_aborted);

	SqlStatementState m_state;      ///< statement state

	SqlTransaction *m_transaction;  ///< transaction over which this statement will run
	SqlText m_sql;                  ///< sql to execute
	SqlValueList *m_parameters;     ///< SqlValueList with query parameters, can be NULL
	Callback *m_callback;           ///< Callback object for the query. Can be NULL and will be handled gracefully

	OpFileLength m_data_file_size;  ///< size of the data file before executing this statement

	SqliteAuthorizerData m_statement_auth_data; ///< authorizer object to be passed to sqlite3_set_authorizer

	sqlite3_stmt* m_stmt;           ///< underlying sqlite3_stmt object
	unsigned m_number_of_yields;    ///< number of times ERR_YIELD has been returned
	unsigned m_avail_exec_time_ms;  ///< time available for this query.
	OpStopWatch m_sw;               ///< stopwatch which times this statement

	bool m_should_not_validate:1;   ///< tells if this query should not be validated by the SqlValidator
	bool m_no_version_check:1;      ///< tells if this query should not be precedded by a database version check
	bool m_is_result_success:1;     ///< tells if the statement has succeded so far (no errors)
	bool m_has_called_callbacks:1;  ///< tells if callbacks have been called

#ifdef _DEBUG
	bool m_has_executed_once:1; ///< tells if this statement has completed at least once and then was saved
#endif

	union {
		SqlResultSet *m_result_set;                  ///< result set with query contents. Check SqlResultSet for more details
		struct {
			OP_STATUS        m_result_error;         ///< in case of query error, the OpStatus error
			SQLiteErrorCode  m_result_error_sqlite;  ///< in case of query error, the sqlite error code, if any
			uni_char*        m_result_error_message; ///< in case of query error, the error message produced by sqlite, if any
		} m_result_error_data;
	};
};

/**
 * This class represents in database session, or transaction as the
 * web database spec calls it. This is the class that provides the
 * interface to execute SQL statements. After usage, remember to call
 * SqlTransaction::Release(). If you do not wish to clutter your code
 * with calls to Release() use AutoReleaseSqlTransactionPtr.
 *
 * Important: during core shutdown, all pending statements or statements
 * executing that have not completed will simply be deleted and transactions
 * will be rolled back. To signal that the statement has aborted the
 * Discard() method of the callback is still called.
 * This is an assurance of the database API: the Discard() method on
 * callback object is ALWAYS called after the statement has been
 * successfully queued
 */
class SqlTransaction: public ListElement<SqlTransaction>, public PS_Base
	, public PS_ObjDelListener DB_INTEGRITY_CHK(0x12345678)
{
public:
	// Continuation of PS_Base::ObjectFlags.
	enum SqlTransactionFlags
	{
		///if it's a read only transaction
		READ_ONLY_TRANSACTION = 0x100,

		///if this transaction is synchronous or not
		///async transactions call callbacks after functions returning
		///and may operate in a separate thread
		SYNCHRONOUS_TRANSACTION = 0x200,

		///if this object is in the process of closing
		///the database session
		SHUTTING_DOWN = 0x400,

		///if a query is processing
		BUSY = 0x800,

		///this flags tells that after the transaction closes,
		///the object should delete itself - to be used only with Release()
		CLOSE_SHOULD_DELETE = 0x1000,

		///if the size of the data file is cached
		HAS_CACHED_DATA_SIZE = 0x2000,

		///if a begin has been issued which creates a rollback segment
		///and disabled auto commit
		HAS_ROLLBACK_SEGMENT = 0x4000,

		///if a message for this transaction to close has been posted
		BEEN_POSTED_CLOSE_MESSAGE = 0x8000,

		///if Release() was called, meaning, this object is on its own
		HAS_BEEN_RELEASED = 0x10000
	};

	/**
	 * Tells if this transaction is synchronous.
	 * Sychronous transactions currently are only allowed for selftests.
	 */
	BOOL IsSynchronous() const { return GetFlag(SYNCHRONOUS_TRANSACTION); }

	/**
	 * Tells if there is a rollback segment, meaning, a BEGIN has been issued
	 * but no COMMIT or RALLBACK. Note: if sqlite implicitly destroys the
	 * transaction, this will still be set so we can keep track of sqlite's
	 * unfortunate behavior
	 */
	BOOL HasRollbackSegment() const { return GetFlag(HAS_ROLLBACK_SEGMENT); }

	/**
	 * Tells if this is the only existent transaction for its database
	 */
	BOOL IsSingleTransaction() const { return Pred() == NULL && Suc() == NULL; }

	/**
	 * This method shall be used when the reference owner no longer
	 * needs this transaction and wants needs to dispose of it.
	 * This is different of simply deleting the object with OP_DELETE
	 * because a released transaction still executed all pending
	 * statements and calls the appropriate callbacks unless they delete
	 * themselves. A Release()d transaction closes the database session
	 * and deletes itself after completing all statements. During close
	 * an implicit ROLLBACK is issued, so make sure you queue a COMMIT
	 * before Release()ing.
	 */
	virtual OP_STATUS Release();

	/**
	 * To be called by the transaction user to cancel all statements.
	 * Note that calling SqlTransaction::Release() does not cancel any
	 * statements, it just tells the object that it's no longer owned by
	 * anyone. The database will still keep track of the transaction while it
	 * has pending statements.
	 *
	 * Each statement can be individually cancelled by calling
	 * CancelStatement(). Just as CancelStatement(), every callback
	 * is disposed by calling Discard() synchronously, and neither HandleError
	 * or HandleCallback are called.
	 */
	void CancelAllStatements();

	/**
	 * Cancels the statements linked to the given callback
	 * and immediately discards the callback.
	 * Neither HandleCallback nor HandleError is called.
	 */
	void CancelStatement(SqlStatement::Callback *callback);

	/**
	 * This method shall be used whenever a BEGIN is to be issued.
	 * ExecuteSql cannot be used directly for BEGIN, COMMIT and ROLLBACK
	 * statements, else these statements will be affected by the SqlValidator.
	 * By using this API to schedule a BEGIN the statement is flagged as
	 * non-validatable so it's allowed to execute, because it does not come
	 * from untrusted input. BEGIN statements will be subject to the expected
	 * version check.
	 * After issuing a BEGIN, the transaction creates a rollback segment.
	 * While this rollback segment is active, meaning neither COMMIT nor
	 * ROLLBACK are executed, the transaction gains a lock over the entire
	 * database, so other transactions wanting to execute will wait until
	 * this transaction either terminates its rollback segment or deletes itself
	 *
	 * WARNING: BEGIN is not allowed to be executed on a synchronous transaction
	 * because so would be harmful due to locking.
	 *
	 * @param callback         SqlStatement::Callback object to listen for
	 *                         the statement completion
	 **/
	OP_STATUS Begin(SqlStatement::Callback *callback = NULL);

	/**
	 * This method shall be used whenever a COMMIT is to be issued.
	 * ExecuteSql cannot be used directly for BEGIN, COMMIT and ROLLBACK
	 * statements, else these statements will be affected by the SqlValidator.
	 * By using this API to schedule a COMMIT the statement is flagged as
	 * non-validatable so it's allowed to execute, because it does not come
	 * from untrusted input. COMMIT statements will NOT be subject to the
	 * expected version check.
	 *
	 * @param callback         SqlStatement::Callback object to listen for
	 *                         the statement completion
	 */
	OP_STATUS Commit(SqlStatement::Callback *callback = NULL);
	/**
	 * This method shall be used whenever a ROLLBACK is to be issued.
	 * ExecuteSql cannot be used directly for BEGIN, COMMIT and ROLLBACK
	 * statements, else these statements will be affected by the SqlValidator.
	 * By using this API to schedule a ROLLBACK the statement is flagged as
	 * non-validatable so it's allowed to execute, because it does not come
	 * from untrusted input. ROLLBACK statements will NOT be subject to the
	 * expected version check. If this is the last statement the API uses to
	 * queue, then it can be bypassed because closing an uncommited transaction
	 * implicitly rolls back.
	 *
	 * @param callback         SqlStatement::Callback object to listen for
	 *                         the statement completion
	 */
	OP_STATUS Rollback(SqlStatement::Callback *callback = NULL);

	/**
	 * This is the method that is used to queue sql statements. To schedule
	 * BEGIN, COMMIT and ROLLBACK check Begin(), Commit() and Rollback().
	 *
	 * All statements passed to this method are validated using the SqlValidator.
	 * See SetSqlValidator() and GetSqlValidator(). The untrusted validator
	 * does not allow statements which control transactions nor pragmas.
	 *
	 * If this method returns OpStatus::OK, then the statement has been
	 * successfully queued.
	 *
	 * Regardless of the return status, this method takes immediate ownership of
	 * the sql_text and parameters. If the method fails, then these arguments
	 * are cleaned up.
	 *
	 * This method does not take ownership of the callback. It's up for the API
	 * user to keep the callback. If the new sql statement is successfully queued
	 * then then the statement finishes or is dropped, the callback's Discard()
	 * method will be called. Check the API documentation for SqlStatement::Callback.
	 *
	 * @param sql                  string with the sql statement
	 * @param sql_length           length of the sql argument
	 * @param parameters           SqlValue* NULL terminated array. If this
	 *                             method succeeds then the underlying statement
	 *                             will take ownership of this array
	 * @param callback             callback object to be notified of the statement
	 *                             results, or for the caller to cancel the statement
	 * @return OpStatus::OK, if statement was properly queued, else an error
	 */
	OP_STATUS ExecuteSql(
			const SqlText &sql_text,
			SqlValueList *parameters = NULL,
			SqlStatement::Callback *callback = NULL);

	/**
	 * This method executes all pending statements synchronously.
	 * If the database cannot execute statement, either due to data
	 * file locks, or waiting for the windowcommander API callback
	 * then it'll call g_opera->RequestRunSlice()
	 * This method only exists because FetchProperties in dom
	 * is not interruptible, because such thing beats the purpose
	 * of the asynchronous API.
	 * USE WITH EXTREME CAUTION !
	 * This method might disappear when FetchProperties() becomes
	 * suspendable.
	 */
	OP_STATUS Flush();

	/**
	 * Tells if this transaction only stores data in memory and not
	 * on the disk, typically, a transaction on a databae with privacy
	 * mode on
	 */
	BOOL IsMemoryOnly() const;

	/**
	 * Returns database on which this transaction operates
	 */
	inline WSD_Database* GetDatabase() const { return m_database; };

	/**
	 * Returns pointer to the underlying sqlite3 object
	 */
	inline sqlite3* GetDatabaseHandle() const { return m_sqlite_db; }

	/**
	 * Tells if this transaction is a read-only transaction.
	 * Read-only transactions cannot change anything on the data file,
	 * meaning, they're only good to do selects
	 */
	inline BOOL IsReadOnly() const { return GetFlag(READ_ONLY_TRANSACTION); }

	/**
	 * Tells if this transaction still has statements executing or waiting for execution
	 */
	inline BOOL HasPendingStatements() const { return !m_statement_queue.Empty(); }

	/**
	 * Returns the SqlValidator currently being used by this transaction.
	 * Might be NULL
	 */
	inline const SqlValidator* GetSqlValidator() { return m_sql_validator; }

	/**
	 * Sets a new SqlValidator to be used
	 *
	 * @param v        the new SqlValidator. might be NULL
	 */
	inline void SetSqlValidator(SqlValidator* v) { m_sql_validator = v; }

	/**
	 * Sets a new SqlValidator to be used given a type. The validator used
	 * will be access from g_database_policies. This way, it saved allocating
	 * a validator for each API user
	 *
	 * @param type       SqlValidator type. Accesses the validator with
	 *                   the given type on
	 */
	inline void SetSqlValidator(SqlValidator::Type type)
	{ m_sql_validator = (type == SqlValidator::TRUSTED ? NULL : g_database_policies->GetSqlValidator(type)); }

	/**
	 * Abstraction over g_opera->running. Tells if Opera is still running normally.
	 * otherwise it means shutdown.
	 */
	inline BOOL IsOperaRunning() const { return OpDbUtils::IsOperaRunning(); }

	/**
	 * Evaluates synchronously the number of objects that exist on the underlying database.
	 * Objects are tables, views, triggers, etc...
	 * Note: calling this method will initialize the transaction, if not done already
	 */
	OP_STATUS GetObjectCount(unsigned *object_count);

	/**
	 * Will be called when the window commander API replies to
	 * the previous OnIncreaseQuota request with the user's intention.
	 */
	void OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size);

	/**
	 * Will be called when the window commander API cancels
	 * a OnIncreaseQuota request like when the page is closed
	 * without a meaningful answer.
	 */
	void OnQuotaCancel();

private:

	/**
	 * Processes the answer passed to OnQuotaReply previously
	 */
	OP_STATUS HandleQuotaReply(SqlStatement* stmt);

	/**
	 * This method is the main entry point for the statement execution
	 * and is called from the message loop code on WSD_Database, meaning
	 * that the WSD_Database chooses which transaction to execute. That
	 * choice is based on the locking policy where the transaction with
	 * an active rollback segment executes.
	 *
	 * This method returns multiple times when executing the same statement
	 * returning control back to the message queue. Each call of this
	 * method will evolve the state of the current statement until it's
	 * completed. Note that the current statement might be stuck on the
	 * same state for more than one call because if it takes too long then
	 * sqlite will be interrupted momentarily.
	 *
	 * If the current statement produces an error, that error is not returned,
	 * but saved with the SqlStatement to be reported to the callback. This
	 * method instead return one of the following status:
	 *  - OpStatus::OK if execution went normally and a new call of this
	 *    function should be scheduled
	 *  - OpStatus::ERR_SUSPENDED if the transaction cannot process because
	 *    it requires information provided from other APIs. This information
	 *    can be a reply of the window commander API, or if saved statements
	 *    are restored to the queue, but lack a last statement introduced by the
	 *    executeSql function to signal that the statements should be indeed
	 *    process and need a callback for errors that might happen-
	 *  - OpStatus::ERR_YIELD if the data file is locked so the next execution
	 *    should be delayed until the datafile is unlocked.
	 *
	 * After a statement executes, it is backed up if it there is a rollback
	 * segment, the transaction is not going to close, the statement did not
	 * produce a quota error, and it's not a select. Backed up statements will
	 * be reexecuted transparently later if sqlite aborts the transaction, which
	 * might happen when one gets quota errors, or the sqlite3_stmt object is
	 * finalized before completion.
	 */
	OP_STATUS ProcessNextStatement();

	/**
	 * Factory method.
	 * Creates a new  transaction object bound to the passed database
	 * and with the given metadata. The created object just holds the
	 * metadata, it has not initialized the sqlite3 backend. Such happens
	 * when necessary, immediately before the first statement is executed
	 *
	 * @param db                  database that will own this transaction
	 * @param read_only           if this transaction should only accept read
	 *                            only statements
	 * @param synchronous         if this transaction will be synchronous or not.
	 *                            Note that synchronous transactions are only used
	 *                            for selftests
	 * @param expected_version    optional. If this argument is not null, nor the
	 *                            empty string, before each new statement the
	 *                            database version will be validated against this
	 *                            argument, and if it's different it'll return
	 *                            the error PS_Status::ERR_VERSION_MISMATCH
	 * @return the new object, or NULL on OOM
	 */
	static SqlTransaction* Create(WSD_Database *db, BOOL read_only,
			BOOL synchronous, const uni_char* expected_version = NULL);

	/**
	 * Default ctor. Use SqlTransaction::Create instead
	 */
	SqlTransaction(WSD_Database *db, BOOL read_only, BOOL synchronous);

	/**
	 * Default dtor
	 */
	~SqlTransaction();

	/**
	 * This method terminates the underlying transaction if possible
	 * else it's delayed, except during core shutdown where it acts
	 * synchronously. The close will be delayed if there are pending
	 * statements
	 *
	 */
	OP_STATUS Close();

	/**
	 * Tells if this transaction can be closed safely, meaning
	 * Release() has been called and no pending statements
	 */
	BOOL CanCloseSafely() const { return GetFlag(HAS_BEEN_RELEASED) && !HasPendingStatements(); }

	/**
	 * Safely closes this transaction, meaning, if CanCloseSafely()
	 * returns TRUE then Close() is called
	 */
	OP_STATUS SafeClose() { return CanCloseSafely() ? Close() : OpStatus::OK; }

	/// These flags are used for the sqlite authorizer api
	enum SqliteAuthorizerFlags
	{
		/// tells if the current statement being executed
		/// affects the rollback segment
		AUTH_AFFECTS_ROLLBACK = 0x1,

		/// flag to tell out authorizer not to validate the
		/// query against the SqlValidator
		AUTH_DONT_VALIDATE = 0x2,

		/// tells if the current statement is a BEGIN
		AUTH_IS_BEGIN = 0x4,

		/// tells if the current statement is a ROLLBACK
		AUTH_IS_ROLLBACK = 0x8,

		/// tells if the current statement is a COMMIT
		AUTH_IS_COMMIT = 0x10
	};

	/**
	 * Callback for the sqlite3_set_authorizer API
	 */
	static int SqliteAuthorizerCallback(void* user_data, int sqlite_action,
			const char* d1, const char*d2, const char*d3, const char*d4);

	/**
	 * Callback for the sqlite3_progress_handler API
	 */
	static int SqliteProgressCallback(void *user_data);

	/**
	 * Invalidates the cached size of the data file.
	 * The size will be cached on a call to GetSize, so it
	 * can be reused
	 */
	inline void InvalidateCachedDataSize() { UnsetFlag(HAS_CACHED_DATA_SIZE); }

	/**
	 * This class represents a callback used internally for statements
	 * that are saved and later re-executed from backup. If the backed up
	 * statement later succeeds, then this callback class ignores its result
	 * else if it errors, that error is forwarded to the callback in the first
	 * statement queued which has not been backed up.
	 * In practice, if the current statement looks like ABC being AB restored
	 * from a backup, and C being an user statement still not executed, an error
	 * that could happen during the execution of A is forwarded to the callback
	 * in C
	 */
	class SavedStmtOverrideCallback : public SqlStatement::Callback
	{
	public:
		SavedStmtOverrideCallback(SqlTransaction* owner) :
			m_transaction(owner), m_original_statement(NULL), m_called_handle(FALSE) {};
		virtual ~SavedStmtOverrideCallback();

		void Set(SqlStatement* original_statement);
		virtual OP_STATUS HandleCallback(SqlResultSet *result_set);
		virtual OP_STATUS HandleError(OP_STATUS error, const uni_char* error_message, BOOL is_fatal);
		virtual Window* GetWindow();
		virtual OpFileLength GetPreferredSize();
		virtual const uni_char* GetDisplayName();
		virtual void Discard();

		SqlStatement::Callback* GetOverrideCallback() const { return m_original_statement != NULL ? m_original_statement->GetCallback() : NULL; }
		void DiscardCallback();

		SqlTransaction* m_transaction;
		SqlStatement* m_original_statement;
		BOOL m_called_handle;
	};

	/**
	 * Deletes all pending statements, without executing them further
	 * Any referenced callbacks are disposed by call ::Discard()
	 */
	void DiscardPendingStatements() { m_statement_queue.Clear(); }

	/**
	 * Deletes all saved statements, which are statements that already executed
	 * but were saved in case the transaction needs to be restarted
	 */
	void DiscardSavedStatements() { m_saved_statements.Clear(); }

	/**
	 * Restores all saved statements into the beginning of the statement
	 * queue of this transaction. Any pending statements are pushed after
	 * the saved statements, and therefore executed later
	 */
	void RestoreSavedStatements();

	/**
	 * Restores current statement being executed (the first on the queue)
	 */
	inline SqlStatement* GetCurrentStatement() const { return m_statement_queue.First(); }

	/**
	 * Tells if the current statement has began being executed
	 */
	BOOL IsCurrentStatementStarted() const
	{ return GetCurrentStatement() != NULL && GetCurrentStatement()->IsStatementStarted(); }

	/**
	 * Signals the termination of the current statement object.
	 * This function is to be used when a statement completes execution,
	 * whether successful or failed. The function Discards the callback
	 * and then assesses if the statement should be queued or not for later
	 * replay in case the transaction fails with a recoverable errors.
	 * If a statement has caused an unrecoverable error like OOM, the entire
	 * transaction rollbacks, and it's not restarted. If it fails with a recoverable
	 * error like a timeout on a select,then the statement is saved. If this statement
	 * has caused sqlite to abort, then all saved statements are requeued.
	 *
	 * @param stmt           the current SqlStatement
	 */
	void TerminateCurrentStatement(SqlStatement*& stmt);

	/**
	 * Function that ensures initialization on the underlying sqlite3 object
	 * that this transaction will use.
	 *
	 * @param error_code             output param with sqlite error code during
	 *                               initialization if any
	 */
	OP_STATUS EnsureInitialization(SQLiteErrorCode *error_code);

	/**
	 * Auxiliary function that deal with a corrupt file during initialization,
	 * or lack of proper read/write access.
	 *
	 * @param error_code        the error code produced during initialization
	 *                          that needs to be handled
	 */
	OP_STATUS HandleCorruptFile(OP_STATUS error_code);

	/**
	 * Posts a message to the owner WSD_Database to close this transaction
	 * later
	 */
	OP_STATUS ScheduleClose();

	/**
	 * Internal ExecuteSql. It's wrapped by ExecuteSql(sql,sql_length,params,callback),
	 * Begin, Rollback or Commit, and takes extra arguments.
	 *
	 * @param should_not_validate       this statement should not be validated by an SqlValidator.
	 *                                  used to let BEGIN/COMMIT/ROLLBACK pass through
	 * @param no_version_check          the transaction should not perform a version check of the
	 *                                  WSD_Database when exeucting this statement. This prevents the
	 *                                  edge case of a transaction going just fine to fail on the last
	 *                                  COMMIT.
	 */
	OP_STATUS ExecuteSql(
			const SqlText &sql_text,
			SqlValueList *parameters,
			BOOL should_not_validate,
			BOOL no_version_check,
			SqlStatement::Callback *callback);

	/**
	 * Auxiliary function to bind an array of SqlValue*s to a sqlite3_stmt
	 * object. Performs boundary check. Number of arguments must be the exact same
	 * number of placeholders in the sql query
	 */
	static SQLiteErrorCode BindParametersToStatement(sqlite3_stmt *stmt, SqlValueList* arguments);

	/**
	 * Checks the policy for this database and adjust the quota available for this transaction
	 * accordingly. Later the transaction during execution might fire an ERR_QUOTA_EXCEEDED
	 */
	OP_STATUS CheckQuotaPolicies();

	/**
	 * Checks if the expected version for this transaction matches the version of the database
	 * Used if some other transaction commits and therefore changes the schema.
	 */
	OP_STATUS CheckDbVersion() const;

	/**
	 * Tells if the data file being used by this transaction will be deleted later, due
	 * for instance, to delete private data
	 */
	BOOL WillFileBeDeleted() const;

	/**
	 * Counts the number of significant database objects that this database has.
	 * Significant object are objects created by the API user, which add significant
	 * data to the data file. Automatically created objects, like auxiliary tables or
	 * sequences are not significant.
	 */
	OP_STATUS FetchObjectCount();

	/**
	 * Quick evaluates a query synchronously. To be used
	 * only for trusted queries. Currently used only for internal
	 * pragmas and object count.
	 *
	 * @param sql          sql string
	 * @param sql_length   sql_string length
	 * @param result       SqlValue which will be set to the value returned if any
	 * @param error_code   error code from slqite if any. Note the function might fail
	 *                     not due to sqlite, so check for the return value
	 */
	OP_STATUS ExecQuickQuery(const char *sql, unsigned sql_length, SqlValue* result, SQLiteErrorCode *error_code);

#ifdef SELFTEST
public:
#endif
	/**
	 * This function checks the policy of the current database, the used global and
	 * origin quotas and calculates the amount of available size for this transaction
	 *
	 * @param available_size       var where the available size will be stored
	 */
	OP_STATUS CalculateAvailableDataSize(OpFileLength* available_size);

	/**
	 * Returns bytes in use by this transaction
	 */
	OP_STATUS GetSize(OpFileLength *db_size);

#ifdef SELFTEST
private:
#endif
	/**
	 * Executes PRAGMA page_size;
	 * page_size times the number of pages give the amount of bytes used
	 */
	OP_STATUS GetPageSize(unsigned *page_size);

	/**
	 * Sets maximum amount of space in bytes that this database can use.
	 * Note that this function will be called during quota validation
	 * before each statement. Calling this elswhere has no use.
	 */
	OP_STATUS SetMaxSize(OpFileLength db_size);

	/**
	 * Executes PRAGMA page_count;
	 * page_size times page_count gives the amount of bytes used
	 */
	OP_STATUS GetPageCount(unsigned *page_count);

	/**
	 * Set maximum number of pages. A page has a fixed size, so
	 * number of pages times page size sets the maximum amounnt of bytes
	 * that this transaction can use
	 */
	OP_STATUS SetMaxPageCount(unsigned page_count);

	/**
	 * Auxiliary callback object that will be used for saved statements
	 */
	SavedStmtOverrideCallback m_saved_statement_callback;

	List<SqlStatement> m_statement_queue;    ///< statements waiting to be executed
	List<SqlStatement> m_saved_statements;   ///< statements saved for later replay
	OpFileLength m_cached_data_size;         ///< cached amount of data in used by
	                                         ///  this transaction. This value might
	                                         ///  not be in sync with the one in
	                                         ///  PS_IndexEntry due to uncommited data

	WSD_Database *m_database;                ///< WSD_Database that owns this transaction
	uni_char* m_used_db_version;             ///< expected version for this transaction. If the
	                                         ///< database version mutates meanwhile, then the transaction fails
	const SqlValidator* m_sql_validator;     ///< SqlValidator in use by this transaction
	PS_DataFile* m_used_datafile_name;       ///< DataFile used by this transaction. Note that PS_DataFile is ref counted
	PS_DataFile_DeclRefOwner
	sqlite3* m_sqlite_db;                    ///< our happy sqlite3 object. See sqlite documentation for details
	SqlTransaction_QuotaCallback* m_quota_callback; ///< Quota request callback object

	unsigned m_cached_page_size;             ///< cached page size, companion of m_cached_data_size
	                                         ///  although page size doesn't mutate that easily
	BOOL m_sqlite_aborted;                   ///< tells if sqlite has aborted and rolled back the transaction

	/**
	 * Used for OnQuotaReply
	 */
	BOOL         m_reply_cancelled;          ///> if OnCancel was called instead of OnQuotaReply
	BOOL         m_reply_allow_increase;     ///> answer passed to the last OnQuotaReply call
	OpFileLength m_reply_new_quota_size;     ///> answer passed to the last OnQuotaReply call

	friend class WSD_Database;
	friend class SqlStatement;
	friend class SqlResultSet;
	friend class SavedStmtOverrideCallback;
};

typedef AutoReleaseTypePtr<SqlTransaction> AutoReleaseSqlTransactionPtr;

/***************************
* WSD_Database
*
* This is the main Web Database underlying class. In practice it's just a bridge between the
* index of ps objects and the underlying transactions which are the ones that manipulate sqlte.
* The WSD_Database object manages time sharing between its transactions, depending whether they
* have active roolback segments or not.
*/
class WSD_Database : public PS_Object, public MessageObject
{
public:

	/**
	 * Wrapper for PS_Manager::GetObject
	 * This method returns a new WSD_Database ready to be used.
	 * If the object does not exist, it is created.
	 * If there is no previous information on the object index
	 * for this object, a new entry is created and the index is
	 * flushed to disk.
	 * Note: WSD_Database are shared among all the callees, therefore
	 * references are counted. After receiving a object by calling
	 * this function, the reference owner must dispose of the
	 * object using PS_Object::Release(). Check PS_Object
	 * documentation for more details
	 *
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 * @param result          pointer to WSD_Database* where the new
	 *                        database will be placed.
	 * @return status of call. If this function returns OK, then
	 *         the value on result is ensured not to be null,
	 *         else it'll be null. If access to this database is forbidden
	 *         then this will return OpStatus::ERR_NO_ACCESS.
	 */
	static OP_STATUS GetInstance(const uni_char* origin, const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id, WSD_Database** result);

	/**
	 * This function tags a specific database for deletion. The
	 * database is not deleted immediately though because there might
	 * be other reference owners and open transactions. When all
	 * transactions close, the database will delete itself, unless
	 * a new call to GetObject is done, which will unmark the deletion
	 * flag. By deleting the database, its entry on the index file will
	 * be removed. When calling this function the data file is immediately
	 * removed, meaning that new transactions will use a new empty data
	 * file. Previously open transactions will continue to use the
	 * old data file until they close. When those transactions close
	 * the old data file will be deleted
	 *
	 * @param origin          origin for the database, per-html5, optional
	 * @param name            name for the database, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 *
	 * @return OpStatus::OK if entry found, OpStatus::ERR_OUT_OF_RANGE if entry not found
	 *         or other errors if processing fails, like OOM when loading the index in memory
	 */
	static OP_STATUS DeleteInstance(const uni_char* origin,
			const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id);

	/**
	 * This function tags all objects of the given type for the given
	 * origin for deletion. The behavior for deletion is the same as
	 * the one from DeleteObject.
	 *
	 * @param context_id      url context id if applicable
	 * @param origin          origin for the database, per-html5, optional
	 */
	static OP_STATUS DeleteInstances(URL_CONTEXT_ID context_id, const uni_char *origin, BOOL only_persistent = TRUE);
	static OP_STATUS DeleteInstances(URL_CONTEXT_ID context_id, BOOL only_persistent = TRUE);

	// Continuation of PS_Base::ObjectFlags.
	enum WSD_DatabaseFlags
	{
		/// Tells if this database has been requested to be closed
		SHUTTING_DOWN = 0x100,

		/// If the message listeners have been added
		INITED_MESSAGE_QUEUE = 0x200,

		/// If a MSG_DATABASE_EXECUTE_TRANSACTION has been posted. Prevents useless messages
		HAS_POSTED_EXECUTION_MSG = 0x400
	};

	/**
	 * Inherited from PS_Objct::Release
	 * This method shall be used when a reference owner
	 * no longer needs an WSD_Database and needs to dispose of it.
	 * During normal execution, calling Release will cause the
	 * database to do delayed shutdown, which means that any running
	 * statements will continue to execute until they finish.
	 * To cancel statements, check the interface for SqlTransaction
	 * After all statements finish, if there are no more reference
	 * owners, the database will delete itself.
	 */
	virtual OP_STATUS Release();

	/**
	 * Creates a new transaction, on which statement can be executed.
	 * See Sql Transaction for what a transaction can do.
	 * All statement execution is asynchronous
	 *
	 * @param read_only        if this transaction is read-only. read-only
	 *                         transaction is not allowed to modify data on disk
	 * @param expected_version override for the database version this transaction expects
	 *                         when executing. If the database version meanwhile mistaches
	 *                         then subsequent statements fail with ERR_VERSION_MISMATCH
	 * @returns NULL if OOM, else the transaction object
	 */
	SqlTransaction* CreateTransactionAsync(BOOL read_only, const uni_char* expected_version = NULL);

#ifdef SELFTEST
	/**
	 * Just like CreateTransactionAsync but the transaction is synchronous
	 * Only for selftests for enormous convenience !
	 */
	SqlTransaction* CreateTransactionSync(BOOL read_only, const uni_char* expected_version = NULL);
#endif // SELFTEST

	/**
	 * Tells if this database has open transactions
	 */
	inline BOOL IsOpened() const { return m_transactions.First() != NULL; }

	/**
	 * From PS_Object. Tells if the underlying data storage holds any significant data
	 * and therefore the database can't be deleted automatically
	 */
	virtual BOOL CanBeDropped() const { return GetCachedObjectCount() == 0; }

	/**
	 * Synchronously evaluates the amount of data spent by this db.
	 * Must not use cached info ! This result WILL be used to cache
	 * a result
	 */
	virtual OP_STATUS EvalDataSizeSync(OpFileLength *result);

	/**
	 * From PS_Object. Used to clear data for the delete private feature
	 * in case the database can't be deleted right now
	 */
	virtual BOOL PreClearData();

	/** From PS_Object. */
	virtual BOOL HasPendingActions() const;

private:
	friend class PS_Object;
	friend class SqlTransaction;
	friend class SqlStatement;

	/**
	 * Factory method
	 */
	static WSD_Database* Create(PS_IndexEntry* key) { return OP_NEW(WSD_Database,(key)); }

	WSD_Database(PS_IndexEntry* entry);
	WSD_Database(const WSD_Database& o) : PS_Object(o) { OP_ASSERT(!"Don't call this"); }
	WSD_Database() : PS_Object() { OP_ASSERT(!"Don't call this"); }

	virtual ~WSD_Database();

	/**
	 * From MessageObject
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * Returns the database's MH_PARAM_1 for its messages
	 */
	inline MH_PARAM_1 GetMessageQueueId() const { return reinterpret_cast<MH_PARAM_1>(this); }

	/**
	 * Posts a message to execute another step of the current transaction
	 */
	OP_STATUS ScheduleTransactionExecute(unsigned timeout);

	/**
	 * Returns cached number of database objects: tables and views.
	 * If 0, the database will be deleted.
	 */
	inline unsigned GetCachedObjectCount() const { return m_cached_object_count; }

	/**
	 * Setups up message queue listeners
	 */
	OP_STATUS InitMessageQueue();

	/**
	 * Returns the next transaction to execute a statement from.
	 * The priority is defined as:
	 *  - a transaction with a statement executing
	 *  - a transaction will a rollback segment
	 *  - a transaction with statements in the queue
	 * else returns NULL
	 */
	SqlTransaction* GetNextTransactionToExecute() const;

	/**
	 * Listener that a transaction calls when it terminates itself.
	 * If the transaction is the only one in the database, and the
	 * database has been released, the database also cleans up itself.
	 */
	void OnTransactionTermination(SqlTransaction* transaction);

	/** Deletes this object is it's not opened and not being referenced
	 *  nor already deleted.
	 *  @retval true if the object was deleted
	 *  @retval false if the object was not deleted. */
	bool SafeDelete();

	/**
	 * Function that cleans up the database and all its child transactions
	 */
	OP_STATUS Close(BOOL synchronous);

	List<SqlTransaction> m_transactions;   ///<* Transaction queue
	unsigned m_cached_object_count;        ///<* Cached count of objects in the database, something different from 0 if unkown
	SqlTransaction* m_current_transaction; ///<* Current or last transaction that executed
};

typedef AutoReleaseTypePtr<WSD_Database> AutoReleaseWSDDatabasePtr;

/**
 * Class that represents one value in a data set, and maps to
 * one of the many datatypes in the database
 *  - http://www.sqlite.org/datatypes.html
 */
class SqlValue
{
public:

	enum ValueType
	{
		TYPE_NULL = 0,
		TYPE_STRING,
		TYPE_INTEGER,
		TYPE_DOUBLE,
		TYPE_BLOB,
		TYPE_BOOL, // Not supported by sqlite but used to simplify passing values from dom to sqlite
		//last type to check range
		TYPE_END
	};

	SqlValue() : m_type(TYPE_NULL) { Clear(); }
	~SqlValue() { Clear(); }

	unsigned SizeOf() const
	{ return sizeof(SqlValue)+(m_type == TYPE_STRING ? UNICODE_SIZE(m_packed.m_value_length+1) : ( m_type == TYPE_BLOB ? m_packed.m_value_length : 0 ));}

	void Clear();
	OP_STATUS Set(const SqlValue& value);
	OP_STATUS Set(sqlite3_value* value);
	OP_STATUS SetBlob(const byte *value, unsigned value_length);
	OP_STATUS SetString(const uni_char *value);
	OP_STATUS SetString(const uni_char *value, unsigned value_length);
	void SetDouble(double value);
	void SetInteger(sqlite_int_t value);
	void SetBoolean(bool value);

	/**
	 * Calls sqlite3_bind_* with the data this SqlValue stores on the passed statement
	 *
	 * @param stmt       sqlite3_stmt object to bind the data to
	 * @param index      index of the variable, starts at 1
	 */
	SQLiteErrorCode BindToStatement(sqlite3_stmt *stmt, unsigned index) const;

	ValueType       Type() const { return m_type; }

	const uni_char* StringValue() const { OP_ASSERT(m_type == TYPE_STRING); return m_packed.m_string_value; }
	unsigned        StringValueLength() const { OP_ASSERT(m_type == TYPE_STRING); return m_packed.m_value_length; }
	sqlite_int_t    IntegerValue() const { OP_ASSERT(m_type == TYPE_INTEGER); return m_integer_value; }
	double          DoubleValue() const { OP_ASSERT(m_type == TYPE_DOUBLE); return m_double_value; }
	const byte*     BlobValue() const { OP_ASSERT(m_type == TYPE_BLOB); return m_packed.m_blob_value; }
	unsigned        BlobValueLength() const { OP_ASSERT(m_type == TYPE_BLOB); return m_packed.m_value_length; }
	bool            BoolValue() const { OP_ASSERT(m_type == TYPE_BOOL); return m_bool_value; }

	BOOL operator==(const SqlValue& value) const { return IsEqual(value); }
	BOOL IsEqual(const SqlValue* value) const { return value != NULL&&IsEqual(*value); }
	BOOL IsEqual(const SqlValue& value) const;

	/**
	 * This method releases any buffer it might have allocated internally so a callee
	 * can keep a reference to the buffer while deleting the SqlValue object. This
	 * saves the callee from allocating an extra buffer.
	 * Because the buffer is no longer owned by this object, therefore trusted, the object is set to TYPE_NULL
	 * Use with caution though, else you might leak memory!
	 */
	void ReleaseBuffers() { m_type = TYPE_NULL; }

#ifdef DEBUG
	void PrettyPrint(TempBuffer& buf);
#endif

private:

	DISABLE_EVIL_MEMBERS(SqlValue)

	ValueType m_type;
	union {
		struct{
			union {
				uni_char* m_string_value;
				byte*     m_blob_value;
			};
			unsigned m_value_length;
		} m_packed;
		sqlite_int_t m_integer_value;
		double       m_double_value;
		bool         m_bool_value;
	};
};

class SqlValueList
{
	SqlValueList(unsigned l) : length(l){}
	~SqlValueList(){}
public:
	const unsigned length;
	SqlValue values[1];

	void Delete();
	static SqlValueList *Make(unsigned length);
};

template<> inline void OpAutoPtr<SqlValueList>::DeletePtr(SqlValueList *p) { p->Delete(); }

/**
 * Class that represents the result of a SQL query.
 * Either a table from a SELECT, or some info like
 * number of rows affected from DML, or an error.
 */
class SqlResultSet
{
public:

	~SqlResultSet();

	/**
	 * In case this result set does not have caching enabled, it keeps
	 * the sqlte3_stmt object live internally so it can iterate it
	 * for each line, therefore saving memory that caching does not save.
	 */
	void Close();

	/**
	 * Returns number of columns in an iterable result set (from a SELECT)
	 */
	inline unsigned GetColumnCount() const { OP_ASSERT(IsIterable()); return m_data.cursor_data.column_count; }

	/*
	 * Gets the column name at a given position.
	 * Indexes start at 0.
	 *
	 * @param index         column index
	 * @param value         string where to store the column name.
	 *                      NOTE: column name is owned by the result set.
	 *                      Callee must not deallocate the string.
	 * @returns TRUE if column index valid or FALSE otherwise
	 */
	BOOL GetColumnName(unsigned index, const uni_char** value) const;

	/*
	 * Gets the index of the column with the given name.
	 * Indexes start at 0.
	 *
	 * @param name          column name
	 * @param value         integer where to store the column index.
	 * @returns TRUE if column name valid or FALSE otherwise
	 */
	BOOL GetColumnIndex(const uni_char* name, unsigned *value) const;

	/**
	 * Steps one row if this result set is iterable.
	 * If caching is enabled, the fetched row will be saved
	 * and can later be retrieved using GetCachedValueAt*.
	 * If IS_INTERRUPTABLE is not set, StepL will throw
	 * PS_Status::ERR_TIMED_OUT when the query time execution
	 * expires due to the query execution timeout preference
	 * else PS_Status::ERR_TIMED_OUT will be returned periodically
	 * to allow control to return to the main message loop.
	 * Check the return value of HasTimedOut() to know if the
	 * query has indeed timed out, and isn't just allowing the
	 * called to return to the message loop.
	 *
	 * @returns TRUE if a new row was found, FALSE otherwise
	 */
	BOOL StepL();

	/**
	 * Steps one row if this result set is iterable.
	 * If caching is enabled, the fetched row will be saved
	 * and can later be retrieved using GetCachedValueAt*.
	 * If IS_INTERRUPTABLE is not set, StepL will throw
	 * PS_Status::ERR_TIMED_OUT when the query time execution
	 * expires due to the query execution timeout preference
	 * else PS_Status::ERR_TIMED_OUT will be returned periodically
	 * to allow control to return to the main message loop.
	 * Check the return value of HasTimedOut() to know if the
	 * query has indeed timed out, and isn't just allowing the
	 * called to return to the message loop.
	 *
	 * @param has_data    stores TRUE if a new row was found, FALSE otherwise
	 */
	OP_STATUS Step(BOOL *has_data);

	/**
	 * Tells if this result set is iterable, which means, the result of a SELECT
	 */
	inline BOOL IsIterable() const { return m_rs_type==ITERABLE_RESULT_SET; }

	/**
	 * Tells this result set to enable row caching, so stepped
	 * rows can be fetched again later on. Do note though that
	 * saving rows uses memory
	 */
	inline OP_STATUS EnableCaching(unsigned max_size_bytes);

	/**
	 * Tells if row caching is enabled, which can only happen if the result set is iterable.
	 */
	inline BOOL IsCachingEnabled() const { return IsIterable() && GetIterationFlag(CACHING_ENABLED); }

	/**
	 * Sets IS_INTERRUPTABLE flag which tells this result set that it should
	 * yield periodically from Step/StepL to allow control to fallback to the
	 * message loop if the query is taking too long to execute
	 */
	inline void SetIsInterruptable() { SetIterationFlag(IS_INTERRUPTABLE); }

	/**
	 * Tells if IS_INTERRUPTABLE is set
	 */
	inline BOOL IsInterruptable() { return GetIterationFlag(IS_INTERRUPTABLE); }

	/**
	 * Tells if this result set's query took too long to execute and has
	 * therefore timed out.
	 */
	BOOL HasTimedOut() const { OP_ASSERT(IsIterable()); return m_sql_statement != NULL && m_sql_statement->HasTimedOut();}

	/**
	 * Returns number of rows processed by this result set.
	 * If caching is enabled, this is the number of rows stored.
	 * Note: this is NOT the full length of the result set. For
	 * that use GetCachedLength
	 */
	inline unsigned GetRowCount() const {return IsIterable() ? m_data.cursor_data.row_count : 0;}

	/**
	 * Returns the value in the current row in the given column
	 * with the requested index. GetRowCount() returns the row
	 * index + 1. If Step() has returned FALSE, meaning the result
	 * set has been fully traversed, then there are no more rows,
	 * and this method returns ERR_OUT_OF_RANGE.
	 *
	 * @param index     column index
	 * @param value     SqlValue in which a copy of the value will be stored
	 */
	OP_STATUS GetValueAtIndex(unsigned index, SqlValue* value) const;

	/**
	 * Returns the value in the current row in the given column
	 * with the requested column. GetRowCount() returns the row
	 * index + 1. If Step() has returned FALSE, meaning the result
	 * set has been fully traversed, then there are no more rows,
	 * and this method returns ERR_OUT_OF_RANGE.
	 *
	 * @param column_name  name of the column
	 * @param value        SqlValue in which a copy of the value will be stored
	 */
	OP_STATUS GetValueAtColumn(const uni_char* column_name, SqlValue* value) const;

	/**
	 * Returns total number of rows in the result set, if caching is enabled.
	 * If necessary, this function traverses the entire result set to get the
	 * full length. this means that a result set without caching will be iterated
	 * to the end and then all data is lost.
	 */
	unsigned GetCachedLengthL();
	OP_STATUS GetCachedLength(unsigned *length);

	/**
	 * When caching is enabled, these functions return the value
	 * saved for the given column index or name and row index.
	 * If the row has not yet been traversed, then the result set
	 * is traversed and that row and all before it are stored.
	 * Note that the values returned are owned by the result set
	 * and therefore the callee MUST NOT do any kind of cleanup.
	 *
	 * @throws   ERR_RSET_TOO_BIG if result set grows to a size bigger
	 *          than defined by the tweak TWEAK_DATABASE_MAX_RESULT_SET_SIZE
	 *           ERR_OUT_OF_RANGE is the requested row or column are out
	 *          of range
	 *           ERR_NO_MEMORY if OOM
	 * @returns the SqlValue saved inside the result set, never NULL.
	 */
	const SqlValue* GetCachedValueAtIndexL(unsigned row_index, unsigned col_index);
	const SqlValue* GetCachedValueAtColumnL(unsigned row_index, const uni_char* column_name);

	/**
	 * When caching is enabled, these functions return the value
	 * saved for the given column index or name and row index.
	 * If the row has not yet been traversed, then the result set
	 * is traversed and that row and all before it are stored.
	 * Note that the values returned are owned by the result set
	 * and therefore the callee MUST NOT do any kind of cleanup.
	 *
	 * @returns  ERR_RSET_TOO_BIG if result set grows to a size bigger
	 *          than defined by the tweak TWEAK_DATABASE_MAX_RESULT_SET_SIZE
	 *           ERR_OUT_OF_RANGE is the requested row or column are out
	 *          of range
	 *           ERR_NO_MEMORY if OOM
	 */
	OP_STATUS GetCachedValueAtIndex(unsigned row_index, unsigned col_index, const SqlValue*& value);
	OP_STATUS GetCachedValueAtColumn(unsigned row_index, const uni_char* column_name, const SqlValue*& value);

	/**
	 * In case this result set represents an INSERT, this returns the
	 * last affected row id. It's fairly useless but it's part of the web db spec
	 */
	sqlite_rowid_t LastInsertId() const { return IsIterable() ? 0 : m_data.dml_data.last_rowid; }

	/**
	 * Returns the number of rows affected by this query, which needs to be a DML obviously
	 */
	unsigned GetRowsAffected() const { return IsIterable() ? 0 : m_data.dml_data.rows_affected; }

	/**
	 * Returns the last error the sqlite API returned if any
	 */
	SQLiteErrorCode GetLastError() const { return m_sqlite_error; }

	/**
	 * Tells if this result set does not have an open sqlite result set,
	 * meaning it's not a select or it has traversed all rows.
	 */
	BOOL IsClosed() const { return !IsIterable() || GetStatementObj() == NULL; }

	/**
	 * Returns size in bytes spent by this result set and all owned data
	 */
	unsigned SizeOf() const { return IsCachingEnabled() ? m_data.cursor_data.cached_size_bytes : 0; }

	/**
	 * Tells if the size in memory spent by this result set has overflown its allowed limit
	 */
	BOOL HasSizeOverflown() const { return SizeOf() > m_data.cursor_data.max_allowed_size; }

private:

	/// This is the number of rows that are going to be fetched when
	/// StepL on a result set with caching enabled needs to read more rows
	/// instead of reading just one, it'll read a block
	enum { STEP_BLOCK_SIZE = 8 };

	SqlResultSet();
	DISABLE_EVIL_MEMBERS(SqlResultSet)

	/**
	 * Prefetches all rows at least until the row with index
	 * row_index. If the row is already cached, then nothing is
	 * done
	 */
	void PrefetchRowL(unsigned row_index = 0xffffffff);

	/**
	 * Internal helper function. Reads the current row and
	 * saves it so it can later be accessed using the cached
	 * values API
	 */
	void StoreCurrentRowL();

	/**
	 * Saves all column names, so they can be reused later, even
	 * if the underlying sqlite statement object is terminated
	 */
	void StoreColumnNamesL();

	/**
	 * Fills in data in this result set relative to a SELECT
	 */
	void SetStatement(SqlStatement *stmt, sqlite3_stmt *sqlite_stmt, BOOL has_at_least_one_row);

	/**
	 * Fills in data in this result set relative to any SQL
	 * statement that isn't a SELECT
	 */
	void SetDMLData(SqlStatement *stmt, unsigned rows_affected, sqlite_rowid_t last_rowid);

	/**
	 * Returns the underlying sqlite3_stmt object
	 */
	inline sqlite3_stmt* GetStatementObj() const { return m_data.cursor_data.sqlite_stmt_obj; }

	friend class SqlStatement;
	friend class SqlTransaction;

	enum ResultSetType
	{
		INVALID_RESULT_SET = 0,
		ITERABLE_RESULT_SET,
		DML_RESULT_SET
	};
	enum IterationFlags{
		DID_1ST_STEP = 0x01,
		CACHING_ENABLED = 0x02,
		HAS_AT_LEAST_ONE_ROW = 0x04,
		/// If this is set, then the result set will periodically return PS_Status::ERR_TIMED_OUT from StepL
		IS_INTERRUPTABLE = 0x10
	};

	/**
	 * These 3 funtions manipulate a couple of flags to be
	 * used when the result set is iterable
	 */
	inline void SetIterationFlag(unsigned int flag) { m_data.cursor_data.iteration_flags |= flag; }
	inline void UnsetIterationFlag(unsigned int flag) { m_data.cursor_data.iteration_flags &= ~flag; }
	inline BOOL GetIterationFlag(unsigned int flag) const { return (m_data.cursor_data.iteration_flags & flag)!=0; }

	union
	{
		//for SELECT
		struct
		{
			sqlite3_stmt *sqlite_stmt_obj;  ///<* underlying sqlite statement object.
			unsigned row_count;             ///<* number of rows processed.
			unsigned column_count;          ///<* cached column count.
			unsigned iteration_flags;       ///<* flags used during iteration. See IterationFlags.
			OpVector<SqlValue> *row_list;  ///<* vector with cached rows. Only used if caching is enabled.
			uni_char** column_names;        ///<* cached column names.
			unsigned cached_size_bytes;     ///<* size spent by this result set so far. Saves populating it too often
			unsigned max_allowed_size;      ///<* max memory size allowed
		} cursor_data;

		//for the others
		struct
		{
			sqlite_rowid_t last_rowid;   ///<* last row id returned by sqlite, if DML
			unsigned rows_affected;   ///<* number of rows affected by this query
		} dml_data;
	} m_data;

	ResultSetType m_rs_type;         ///<* result set type: iterable, or DML/DDL
	SQLiteErrorCode m_sqlite_error;  ///<* last sqlite error
	SqlStatement *m_sql_statement;   ///<* pointer back to the SqlStatement that produced this result set
};

#endif //SUPPORT_DATABASE_INTERNAL

#endif //_MODULE_DATABASE_OPDATABASE_H_
