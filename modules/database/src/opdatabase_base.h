/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#ifndef _MODULES_DATABASE_OPDATABASE_BASE_H_
#define _MODULES_DATABASE_OPDATABASE_BASE_H_

#if defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT

#define DB_MAIN_CONTEXT_ID         0
#define DB_NULL_CONTEXT_ID         ((URL_CONTEXT_ID)-1)

#define DISABLE_EVIL_MEMBERS(type) \
	type(const type& a) {} \
	void operator=(const type& a) {}

//enable to get some database debugging macros working
//#define DATABASE_MODULE_DEBUG 1

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

/**
 * Types of databases to be used by core.
 *  - KLocalStorage and KSessionStorage - for webstorage
 *  - KWebDatabases for html5 databases
 *
 * IMPORTANT: Add types to the enumeration as you see fit,
 * add the string description of the new enumeration
 * values on opdatabasemanager.cpp, and add a proper policy
 * for the database type in sec_policy.cpp in the
 * PS_PolicyFactory class
 *
 */
class PS_ObjectTypes
{
public:
	enum PS_ObjectType
	{
		KDBTypeStart = -1,
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
			KLocalStorage,
			KSessionStorage,
#endif// WEBSTORAGE_ENABLE_SIMPLE_BACKEND
#ifdef DATABASE_STORAGE_SUPPORT
			KWebDatabases,
#endif
#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND
# ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
			KWidgetPreferences,
# endif //defined WEBSTORAGE_WIDGET_PREFS_SUPPORT
# ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
			KUserJsStorage,
# endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
#endif// WEBSTORAGE_ENABLE_SIMPLE_BACKEND
		KDBTypeEnd
	};

	inline static BOOL ValidateObjectType(int type) { return KDBTypeStart < type && type < KDBTypeEnd; }
};

#endif //DATABASE_MODULE_MANAGER_SUPPORT

/**
 * Custom error types used on the database module
 */
class PS_Status : public OpStatus
{
public:
	enum PSErrEnum
	{
		ERR_QUOTA_EXCEEDED       = USER_ERROR + 1, // when db tries to overflow available quota
		ERR_CORRUPTED_FILE       = USER_ERROR + 2, // data file got corrupted
		ERR_BAD_BIND_PARAMETERS  = USER_ERROR + 3, // wrong number of parameters when binding to a sql statement
		ERR_TIMED_OUT            = USER_ERROR + 4, // for when query times out
		ERR_AUTHORIZATION        = USER_ERROR + 5, // for when queries are not allowed to run
		ERR_READ_ONLY            = USER_ERROR + 6, // for read only transaction
		ERR_BAD_QUERY            = USER_ERROR + 7, // for bad/invalid queries
		ERR_SUSPENDED            = USER_ERROR + 8, // when waiting for user action
		ERR_RSET_TOO_BIG         = USER_ERROR + 9, // result set grew out of allowed limits
		ERR_MAX_DBS_PER_ORIGIN   = USER_ERROR +10, // maximum number of origins reached, used by DOM
		ERR_CANCELLED            = USER_ERROR +11, // operation was cancelled by API user
		ERR_VERSION_MISMATCH     = USER_ERROR +12, // database version meanwhile mutated
		ERR_CONSTRAINT_FAILED    = USER_ERROR +13, // when query causes a constraint error
		ERR_MAX_QUEUE_SIZE       = USER_ERROR +14, // someone or something tried to flood an API, so this error is returned to signal that
		ERR_INVALID_STATE        = USER_ERROR +15  // API used when it shouldn't, when API is in incorrect state (usually something asserts).
	};
};

#ifdef USE_DEBUGGING_OP_STATUS
op_force_inline BOOL operator ==(PS_Status::PSErrEnum val, const OP_STATUS_ARG status)
{ return status == val; }
op_force_inline BOOL operator ==(PS_Status::PSErrEnum val, const volatile OP_STATUS_ARG status)
{ return status == val; }
op_force_inline BOOL operator !=(PS_Status::PSErrEnum val, const OP_STATUS_ARG status)
{ return status != val; }
op_force_inline BOOL operator !=(PS_Status::PSErrEnum val, const volatile OP_STATUS_ARG status)
{ return status != val; }
#endif //USE_DEBUGGING_OP_STATUS

/**
 * Simple class to do ref counting. OpReferenceCounter does
 * not have the asserts so dearly needed to help debug the code
 * so we need a piece of code we can control
 */
class OpRefCounter
{
public:
	OpRefCounter() : m_ref_count(0) {}
	~OpRefCounter() { OP_ASSERT(m_ref_count == 0); }
	inline void IncRefCount() { m_ref_count++; }
	inline void DecRefCount() { OP_ASSERT(m_ref_count > 0); if(m_ref_count > 0) m_ref_count--; }
	inline unsigned GetRefCount() const { return m_ref_count; }
private:
	unsigned m_ref_count;
};

/**
 * Encapsulates some utility functions
 */
class OpDbUtils
{
public:

	static BOOL IsFilePathAbsolute(const uni_char*);

	//utility functions... will be moved to stdlib or util somewhere in time
	static OP_STATUS DuplicateString(const uni_char* src_value, uni_char** dest_value);
	static OP_STATUS DuplicateString(const uni_char* src_value, unsigned src_length,
			uni_char** dest_value, unsigned *dest_length);

	/**
	 * Tells if Opera's message queue is running, If not then core is shutting down
	 */
	static inline BOOL IsOperaRunning() { return !g_database_module.m_being_destroyed; }

	/**
	 * @return the global message handler. spares one global access
	 */
	static MessageHandler* GetMessageHandler() { return g_main_message_handler; };
	static void ReportCondition(OP_STATUS status);

	static BOOL StringsEqual(const uni_char* lhs_value, const uni_char* rhs_value)
	{ return (lhs_value == rhs_value) || (lhs_value != NULL &&  rhs_value != NULL && uni_str_eq(lhs_value, rhs_value)); }
	static BOOL StringsEqual(const uni_char* lhs_value, unsigned lhs_length,
			const uni_char* rhs_value, unsigned rhs_length);

	inline static OP_STATUS GetOpStatusError(OP_STATUS a, OP_STATUS b)
	{ return OpStatus::IsMemoryError(a) ? a : (OpStatus::IsSuccess(a) ? b: a); }

#ifdef DATABASE_MODULE_DEBUG
	static OP_STATUS SignalOpStatusError(OP_STATUS status, const char* msg, const char* file, int line);
#ifdef SUPPORT_DATABASE_INTERNAL
	static const char *DownsizeQuery(const class SqlText &sql_text);
#endif // SUPPORT_DATABASE_INTERNAL
#define SIGNAL_OP_STATUS_ERROR(err_num) OpDbUtils::SignalOpStatusError(err_num, #err_num, __FILE__, __LINE__)
#else
#define SIGNAL_OP_STATUS_ERROR(err_num) (err_num)
#endif
};

#ifdef DATABASE_MODULE_DEBUG
#  define DB_DBG(a) dbg_printf a;
#  if DATABASE_MODULE_DEBUG > 1
#    define DB_DBG_2(a) dbg_printf a;
#  else
#    define DB_DBG_2(a)
#  endif //DATABASE_MODULE_DEBUG
#  define DB_DBG_ENTRY_STR "%S,%S,%S,%d"
#  define DB_DBG_ENTRY_O(o) o->GetTypeDesc(),o->GetOrigin(),o->GetName(),o->IsPersistent()
#  define DB_DBG_BOOL(s) ((s)?"true":"false")
#  define DB_DBG_CHK(b,a) if(b) { DB_DBG(a); }
#  define DB_DBG_QUERY(s) OpDbUtils::DownsizeQuery(s)
#else
#  define DB_DBG(a)
#  define DB_DBG_2(a)
#  define DB_DBG_CHK(b, a)
#endif

#ifdef DEBUG_ENABLE_OPASSERT
# define DB_ASSERT_RET(expr, value) do { bool r;OP_ASSERT(r=!!(expr));if(!r) return value;} while(0)
# define DB_ASSERT_LEAVE(expr, value) do { bool r;OP_ASSERT(r=!!(expr));if(!r) LEAVE(value);} while(0)
#else
# define DB_ASSERT_RET(expr, value) do { if(!(expr)) return value;} while(0)
# define DB_ASSERT_LEAVE(expr, value) do { if(!(expr)) LEAVE(value);} while(0)
#endif // DEBUG_ENABLE_OPASSERT

/**
 * Helper class to define flags and flag API for classes
 */
class PS_Base
{
private:
	unsigned int m_flags;
protected:
	//flags generic to all objects
	//note ! these will be used as masks, so these can only have one bit at 1
	//the first 8 bit are reserved for ObjectFlags
	enum ObjectFlags
	{
		NO_FLAGS = 0,
		OBJ_INITIALIZED = 0x01,
		OBJ_INITIALIZED_ERROR = 0x02,
		///if the object has been OP_DELETEd meanwhile
		///used to prevent re-deletion
		BEING_DELETED = 0x04,
		//no value can pass this limit
		FLAGS_END_LIMIT = 0x100
	};

	/**
	 * Panic function !
	 */
	static void ReportCondition(OP_STATUS status) { OpDbUtils::ReportCondition(status); }

	PS_Base() : m_flags(NO_FLAGS) {}
	~PS_Base() { m_flags = NO_FLAGS; }

public:
	inline BOOL GetFlag(unsigned int flag) const { return (m_flags & flag)!=0; }
	inline void SetFlag(unsigned int flag) { m_flags |= flag; }
	inline void UnsetFlag(unsigned int flag) { m_flags &= ~flag; }
	inline void ToggleFlag(unsigned int flag, BOOL enable) { if(enable) {SetFlag(flag);}else{UnsetFlag(flag);} }

	inline BOOL IsBeingDeleted() const { return GetFlag(BEING_DELETED); }
	inline BOOL IsInitialized() const { return GetFlag(OBJ_INITIALIZED) && !GetFlag(OBJ_INITIALIZED_ERROR); }
	inline BOOL IsInitializedError() const { return GetFlag(OBJ_INITIALIZED) && GetFlag(OBJ_INITIALIZED_ERROR); }
};

/**
 * Listener class for objects in the database module to notify
 * between themselves when they are deleted
 */
class PS_ObjDelListener
{
public:
	class ResourceShutdownCallback
	{
		class ResourceShutdownCallbackLinkElem : public ListElement<ResourceShutdownCallbackLinkElem> {
		public:
			ResourceShutdownCallbackLinkElem(ResourceShutdownCallback* c) : m_cb(c) {}
			~ResourceShutdownCallbackLinkElem() { Out(); }
			ResourceShutdownCallback* m_cb;
		};
		ResourceShutdownCallbackLinkElem m_link;
		friend class PS_ObjDelListener;
	public:
		ResourceShutdownCallback() : m_link(this) {}
		virtual ~ResourceShutdownCallback() { }
		virtual void HandleResourceShutdown() = 0;
	};

	PS_ObjDelListener() {}
	~PS_ObjDelListener()
	{
		//notifying the callbacks at this stage will probably be harmful because the
		//object type they added the listener to at this time are already destroyed
		//so we're only left with the base class
		OP_ASSERT(m_shutdown_listeners.Empty() || !"Forgot to call ::FireShutdownCallbacks()");
		m_shutdown_listeners.RemoveAll();
	}

	void AddShutdownCallback(ResourceShutdownCallback* cb) { if(cb)cb->m_link.Into(&m_shutdown_listeners); }
	void RemoveShutdownCallback(ResourceShutdownCallback* cb) { if(cb != NULL)cb->m_link.Out(); }

protected:
	void FireShutdownCallbacks();
private:
	List<ResourceShutdownCallback::ResourceShutdownCallbackLinkElem> m_shutdown_listeners;
};

/**
 * Class that implements a stop watch, and returns the run time in miliseconds.
 * Run time rolls over after 2^32 milliseconds, which is about 49 days total runtime.
 */
class OpStopWatch
{
public:
	OpStopWatch() : m_last_start(0), m_accumulated(0), m_is_running(FALSE) {}
	~OpStopWatch() {}

	void Start();
	void Continue();
	void Stop();
	void Reset() { OP_ASSERT(!m_is_running); m_last_start = 0; m_accumulated = 0; }
	unsigned GetEllapsedMS() const;

	BOOL IsRunning() const { return m_is_running; }
private:
	double   m_last_start;
	unsigned m_accumulated;
	BOOL     m_is_running;
};

#ifdef _DEBUG
/**
 * Helper class to find out if reinterpert_casts don't
 * mess up or objects aren't deleted prematurely
 */
template<int INTEGRITY_KEY>
class OpDBIntegrityCheck
{
	int m_integrity_key;
protected:
	//debug code to make sure the object is properly passed to the event handler
	OpDBIntegrityCheck() : m_integrity_key(INTEGRITY_KEY) {}
	~OpDBIntegrityCheck() {VerifyIntegrity();}
public:
	void VerifyIntegrity() const { OP_ASSERT(m_integrity_key==INTEGRITY_KEY); }
};

#define DB_INTEGRITY_CHK(c) ,public OpDBIntegrityCheck<c>
#define DB_INTEGRITY_CHK_F(c) :public OpDBIntegrityCheck<c>

#define INTEGRITY_CHECK() VerifyIntegrity()
#define INTEGRITY_CHECK_P(p) (p)->VerifyIntegrity()

#else

#define DB_INTEGRITY_CHK(c)
#define DB_INTEGRITY_CHK_F(c)

#define INTEGRITY_CHECK() ((void)0)
#define INTEGRITY_CHECK_P(p) ((void)0)

#endif //_DEBUG

#endif //defined DATABASE_MODULE_MANAGER_SUPPORT || defined OPSTORAGE_SUPPORT

#include "modules/database/autoreleaseptrs.h"

#endif /* _MODULES_DATABASE_OPDATABASE_BASE_H_ */
