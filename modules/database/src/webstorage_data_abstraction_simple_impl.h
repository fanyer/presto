/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULE_DATABASE_WEBSTORAGE_DATA_ABSTRACTION_SIMPLE_IMPL_H_
#define _MODULE_DATABASE_WEBSTORAGE_DATA_ABSTRACTION_SIMPLE_IMPL_H_

#ifdef WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#include "modules/database/src/opdatabase_base.h"
#include "modules/database/opdatabasemanager.h"
#include "modules/database/sec_policy.h"
#include "modules/util/OpHashTable.h"
#include "modules/dochand/win.h"
#include "modules/database/src/filestorage.h"

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
# define IS_OP_SET_ITEM(op) ((op)->m_type == WebStorageOperation::SET_ITEM || \
		(op)->m_type == WebStorageOperation::SET_ITEM_RO || \
		(op)->m_type == WebStorageOperation::SET_NEW_ITEM_RO)
# define IS_OP_CLEAR(op)    ((op)->m_type == WebStorageOperation::CLEAR || (op)->m_type == WebStorageOperation::CLEAR_RO)
# define IS_OP_CLEAR_ALL(op)    ((op)->m_type == WebStorageOperation::CLEAR_RO)
#else
# define IS_OP_SET_ITEM(op) ((op)->m_type == WebStorageOperation::SET_ITEM)
# define IS_OP_CLEAR(op)    ((op)->m_type == WebStorageOperation::CLEAR)
# define IS_OP_CLEAR_ALL(op)    ((op)->m_type == WebStorageOperation::CLEAR)
#endif

class WebStorageBackend_SimpleImpl;
class WebStorageBackendOperation;
class MessageHandler;
class WebStorageBackend_SimpleImpl_QuotaCallback;

/**
 * Class that represents a web storage key/value pair
 */
class WebStorageValueInfo
{
public:
	WebStorageValueInfo()
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
		: m_is_readonly(FALSE)
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
	{}
	~WebStorageValueInfo() {}

	WebStorageValue m_key;   ///<* key. Never NULL
	WebStorageValue m_value; ///<* value. Never NULL

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
# define IS_WSVAL_RO(v) ((v) && (v)->m_is_readonly)
	BOOL m_is_readonly;      ///<* TRUE if pair is read-only and therefore cannot be changed using the regular Clear()/SetItem()
#else
# define IS_WSVAL_RO(v) (FALSE)
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
};

/**
 * Specialization of HashTable to contain the web storage pairs index.
 */
class WebStorageValueInfoTable : public OpPointerHashTable<WebStorageValue, WebStorageValueInfo>
{
public:
	WebStorageValueInfoTable() : OpPointerHashTable<WebStorageValue, WebStorageValueInfo>(this) {}

	//from OpHashFunctions
	virtual UINT32 Hash(const void* key);
	virtual BOOL KeysAreEqual(const void* key1, const void* key2);

	//from OpHashTable
	virtual void Delete(void* data);
};

/**
 * This struct saves allocation an extra WebStorageValue
 */
struct WebStorageOperationFixedValue : public WebStorageOperation
{
	WebStorageOperationFixedValue() {}
	~WebStorageOperationFixedValue();
	void SetValue();
	WebStorageValue m_value_holder;
};

/**
 * Internal class that represents one of the many
 * storage operations in the queue.
 */
class WebStorageBackendOperation : public ListElement<WebStorageBackendOperation>, public PS_Base
{
public:
	// Continuation of PS_Base::ObjectFlags.
	enum WebStorageBackendOperationFlags
	{
		SET_RO_FLAG_TRUE = 0x100,
		IS_SYNC_FLUSH = 0x400,
		// Operation fails immediately instead of prompting
		FAIL_IF_QUOTA_ERROR = 0x800,
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
		PAIR_IS_READONLY = 0x1000
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
	};

	//the missing action that is not part of WebStorageOperation
	enum
	{
		ENUMERATE_ITEMS = 30,
		FLUSH_DATA_TO_FILE
	};

	WebStorageBackendOperation(WebStorageBackend_SimpleImpl* impl, unsigned type, WebStorageOperationCallback* callback);
	WebStorageBackendOperation(WebStorageBackend_SimpleImpl* impl, unsigned type, WebStorageKeyEnumerator* enumerator);
	virtual ~WebStorageBackendOperation();

	OP_STATUS Terminate(OP_STATUS status);

	OP_STATUS InvokeErrorCallback(OP_STATUS error, const uni_char* error_message);
	OP_STATUS InvokeCallback();

	void DiscardCallback();

	/**
	 * Deletes this operation without performing
	 * the actions after a successful operation
	 */
	void Drop();

	WebStorageKeyEnumerator* GetEnumerator() const { return IsEnumerate() ? m_enumerator : NULL; }

	WebStorageOperationCallback* GetCallback() const { return IsEnumerate() ? NULL : m_callback; }

	void SetCallback(WebStorageOperationCallback* cb) { OP_ASSERT(!IsEnumerate()); if (!IsEnumerate()) m_callback = cb; }

	inline BOOL EnumWantsValues()
	{ OP_ASSERT(IsEnumerate()); return m_enumerator->WantsValues(); }

	inline Window* GetWindow()
	{ return !IsEnumerate() && m_callback != NULL ? m_callback->GetWindow() : NULL; }

	inline MessageHandler* GetMessageHandler()
	{ return GetWindow() ? GetWindow()->GetMessageHandler() : OpDbUtils::GetMessageHandler(); }

	inline BOOL IsEnumerate() const
	{ return m_type == WebStorageBackendOperation::ENUMERATE_ITEMS; }

	inline BOOL RequiresInit() const;

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT

	inline void SetPairReadOnly(BOOL v)
	{ ToggleFlag(PAIR_IS_READONLY, v); }

	inline BOOL IsPairReadOnly()
	{ return GetFlag(PAIR_IS_READONLY); }

#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

	inline BOOL IsSyncFlush() const { return GetFlag(IS_SYNC_FLUSH); }
	inline void SetIsSyncFlush(BOOL v) { return ToggleFlag(IS_SYNC_FLUSH, v); }

	inline BOOL WillWriteData() const { return IS_OP_SET_ITEM(this) && m_value != NULL; }

	WebStorageBackend_SimpleImpl* m_impl;  ///<* WebStorageBackend object on which this operation will execute

	const WebStorageValue *m_key;
	const WebStorageValue *m_value;

private:
#ifdef _DEBUG
	BOOL m_has_called_callback; // Set to true if HandleOperation/HandleErrorHandleKey have been called and the callback dropped.
#endif // _DEBUG
	union{
		WebStorageKeyEnumerator* m_enumerator;    ///<* callback for enumerate operations
		WebStorageOperationCallback* m_callback;  ///<* callback for non-enumerate operations
	};
public:
	WebStorageOperationFixedValue m_result;  ///<* result object to be passed to callback

	unsigned m_index;  ///<* key/value index. Used in GET_ITEM_BY_INDEX
	unsigned m_type;   ///<* operation type, WebStorageOperation::WebStorageOperationEnum
};


/**
 * WebStorageBackend_SimpleImpl is our implementation of WebStorageBackend using the
 * xml file as backend
 */
class WebStorageBackend_SimpleImpl
	: public PS_Object
	, public WebStorageBackend
	, public MessageObject
	, public FileStorageLoadingCallback
	, public FileStorageSavingCallback
{
public:

	/**
	 * Factory method
	 * See WebStorageBackend::Create
	 */
	static OP_STATUS GetInstance(WebStorageType type, const uni_char* origin,
			BOOL is_persistent, URL_CONTEXT_ID context_id, WebStorageBackend_SimpleImpl** resul);

	/**
	 * Continuation of PS_Base::ObjectFlags.
	 * OBJ_INITIALIZED is used to tell when the data file was read into memory
	 */
	enum WebStorageBackend_SimpleImplFlags
	{
		/// If the message queue has been initialized
		INITED_MESSAGE_QUEUE = 0x100,

		/// If a MSG_WEBSTORAGE_EXECUTE_OPERATION has been posted
		HAS_POSTED_EXECUTION_MSG = 0x200,

		/// If the contents of this object have been modified
		HAS_BEEN_MODIFIED = 0x400,

		/// If a HAS_POSTED_DELAYED_FLUSH_MSG msg has been posted
		HAS_POSTED_DELAYED_FLUSH_MSG = 0x800,

		/// Waiting for data load
		WAITING_FOR_DATA_LOAD = 0x1000,

		/// Waiting for data write - note exclusive lock, don't run any operations
		WAITING_FOR_DATA_WRITE = 0x2000,

		/// Next time we have the chance, delete the data_loader
		/// because it can't be deleted from the OnLoading* call
		DELETE_DATA_LOADER = 0x4000
	};

	/**
	 * Does not use any kind of physical storage, just memory.
	 */
	inline BOOL IsVolatile() const { return IsMemoryOnly() || GetWebStorageType() == WEB_STORAGE_SESSION; }

	/**
	 * Synchronously evaluates the amount of data spent by this storage area
	 * OpStatus:ERR_YIELD if value can not be evaluated right now
	 */
	OP_STATUS EvalDataSizeSync(OpFileLength *result);

	/** From PS_Object. */
	virtual BOOL CanBeDropped() const { return IsEmpty(); }

	/**
	 * Refer to PS_Object::PreClearData
	 */
	virtual BOOL PreClearData();

	/**
	 * Refer to PS_Object::HasPendingActions
	 */
	virtual BOOL HasPendingActions() const;

	/**
	 * Defined because of ambiguity
	 */
	inline URL_CONTEXT_ID GetUrlContextId() const { return PS_Object::GetUrlContextId(); }

	/**
	 * Methods inherited from WebStorageBackend
	 */
	virtual OP_STATUS GetNumberOfItems(WebStorageOperationCallback* callback);
	virtual OP_STATUS GetKeyAtIndex(unsigned index, WebStorageOperationCallback* callback);
	virtual OP_STATUS GetItem(const WebStorageValue *key, WebStorageOperationCallback* callback);
	virtual OP_STATUS SetItem(const WebStorageValue *key, const WebStorageValue *value, WebStorageOperationCallback* callback);
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	virtual OP_STATUS SetItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback);
	virtual OP_STATUS SetNewItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback);
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
	virtual OP_STATUS EnumerateAllKeys(WebStorageKeyEnumerator* enumerator);
	virtual OP_STATUS Clear(WebStorageOperationCallback* callback);
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	virtual OP_STATUS ClearAll(WebStorageOperationCallback* callback);
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT
	virtual void Release(BOOL force);
#ifdef SELFTEST
	virtual void CancelAll();
#endif

	/**
	 * Methods inherited from PS_Object
	 */
	virtual OP_STATUS Release();

	/**
	 * Methods inherited from FileStorageLoadingCallback
	 */
	virtual void OnLoadingFinished();
	virtual void OnLoadingFailed(OP_STATUS);
	virtual OP_STATUS OnValueFound(WebStorageValueInfo*);

	/**
	 * Tells the opgadgetmanager to reapply the default preferences
	 * found in the config.xml file.
	 * To be used when unfortunatelly the data file is corrupted
	 * or empty, as a workaround for DSK-380695 (data loss).
	 */
	void RestoreExtensionDefaultPreferences();

	/**
	 * Methods inherited from FileStorageSavingCallback
	 */
	virtual void OnSavingFinished();
	virtual void OnSavingFailed(OP_STATUS);

	/**
	 * Schedules a task to flush this storage area to a file on disk
	 * @returns ERR_NO_MEMORY or Ok
	 */
	OP_STATUS SaveToDiskAsync();

	/**
	 * Synchronously flushes this storage area to a file on disk, by
	 * placing an operation in the queue and flushing the queue until
	 * that operation is completed. So, this function flushes everything
	 * in the queue as well. But won't work properly if the file is still
	 * loading.
	 *
	 * @returns OpStatus::ERR_OUT_OF_RANGE if object is not initialized yet because
	 *          synchronous file load is currently not implemented
	 */
	OP_STATUS SaveToDiskSync();

	/**
	 * Method which stores the data from a quota reply
	 * from WebStorageBackend_SimpleImpl_QuotaCallback
	 */
	void OnQuotaReply(BOOL allow_increase, OpFileLength new_quota_size);
	void OnQuotaCancel();

	/**
	 * Tells if this storage area does not have any key/value pair stored
	 */
	BOOL IsEmpty() const { return m_storage_area_sorted.GetCount() == 0; }

protected:

	friend class PS_Object;
	friend class PS_Manager;
	friend class WebStorageBackendOperation;
	friend class WebStorageBackend_SimpleImpl_QuotaCallback;

	/**
	 * Factory method: called by the PS_Manager.
	 */
	static WebStorageBackend_SimpleImpl* Create(PS_IndexEntry* entry);

	/**
	 * Constructors
	 */
	WebStorageBackend_SimpleImpl(PS_IndexEntry* entry);
	WebStorageBackend_SimpleImpl(const WebStorageBackend_SimpleImpl& o) : PS_Object(o) { OP_ASSERT(!"Don't call this"); }
	WebStorageBackend_SimpleImpl() : PS_Object() { OP_ASSERT(!"Don't call this"); }

	virtual ~WebStorageBackend_SimpleImpl();

	/**
	 * Internal helper function for EnumerateAllKeys().
	 */
	OP_STATUS EnumerateAllKeys_Internal(WebStorageKeyEnumerator* enumerator);

	/**
	 * Loads all data into memory from the data file.
	 * Return value tells what to do:
	 *  - OpStatus::OK object is already initialized and all data is readily available, or there is no data file, so assume nothing is to be done.
	 *  - OpStatus::ERR_YIELD means that we need to wait until all data is loaded/saved
	 *  - PS_Status::ERR_CORRUPTED_FILE means that the data is corrupted and therefore disregarded
	 *  - everything else, panic
	 */
	OP_STATUS EnsureInitialization();

	/**
	 * Deletes all data contained within, including data file.
	 *
	 * @returns TRUE if something was indeed modified
	 */
	BOOL DestroyData();

	/** Marks that the object has modifications to be saved to the disk. */
	void SetHasModifications() { if (!IsVolatile()) SetFlag(HAS_BEEN_MODIFIED); }
	/** Reverts a call to SetHasModifications(). */
	void UnsetHasModifications() { UnsetFlag(HAS_BEEN_MODIFIED); }
	/** Tells if there are modifications to be saved to the disk. */
	BOOL HasModifications() { return GetFlag(HAS_BEEN_MODIFIED); }

	/**
	 * Initializes the message listeners, and posts a message
	 * to process one storage operation.
	 */
	OP_STATUS PostExecutionMessage(unsigned timeout = 0);

	/**
	 * Adds the operation to the queue and posts a message to handle it
	 * later.
	 */
	OP_STATUS ScheduleOperation(WebStorageBackendOperation* op);

	/**
	 * Initializes the message listeners, and posts a message
	 * to flush the data to file after a timeout.
	 */
	OP_STATUS PostDelayedFlush(unsigned timeout = WEBSTORAGE_DELAYED_FLUSH_TIMEOUT);

	/**
	 * Sets the callbacks in the message queue for the messages
	 * this object handles
	 */
	OP_STATUS InitMessageListeners();

	/**
	 * Messages receiver id.
	 */
	inline MH_PARAM_1 GetMessageQueueId() const { return reinterpret_cast<MH_PARAM_1>(this); }

	/**
	 * MessageHandler that this object should use.
	 */
	inline MessageHandler* GetMessageHandler() const { return OpDbUtils::GetMessageHandler(); };

	/**
	 * From MessageObject.
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * Picks up the current storage operation from the queue
	 * and processes it.
	 *
	 * @returns OpStatus::ERR_YIELD in case the current operation cannot run because it
	 *          waiting for data to be loaded or written, or waiting for a user
	 *          reply to a quota increase request.
	 *          OpStatus::ERR_NO_MEMORY in case of OOM error.
	 *          OpStatus::OK if execution went normally and a new call of this
	 *          function should be scheduled
	 */
	OP_STATUS ProcessOneOperation();

	/**
	 * Returns the first operation in the queue.
	 */
	WebStorageBackendOperation* GetCurrentOperation() const;

	/**
	 * Tells if operation queue is empty
	 */
	BOOL IsQueueEmpty() const;

	/**
	 * Callback to be called when a storage operation is completed.
	 */
	void OnOperationCompleted();

	/** Deletes this object is it's not opened and not being referenced
	 *  nor already deleted.
	 *  @retval true if the object was deleted
	 *  @retval false if the object was not deleted. */
	bool SafeDelete();

	/**
	 * Processes the data received from a quota reply during
	 * the processing of a storage operation.
	 *
	 * @param op      the storage operation being processed
	 */
	OP_STATUS HandleQuotaReply(WebStorageBackendOperation* op);

	/**
	 * Get storage type.
	 */
	WebStorageType GetWebStorageType() const { return WebStorageBackend::GetType(); }

	/**
	 * Converts a simple value size into what it would occupy on disk.
	 */
	unsigned ConvertValueSize(const WebStorageValue* v) const;

	/**
	 * Converts a pair size into what it would occupy on disk.
	 */
	unsigned ConvertPairSize(const WebStorageValueInfo* v) const;

	/**
	 * Check if by replacing old_value by new_value for a given key,
	 * the quota will overflow. Also updated cached values for quota.
	 * After this function is called, operations cannot fail, because
	 * there is rollback mechanism !
	 *
	 * @param key        key of the key/value pair
	 * @param old_value  old value that will be replaced. NULL if non-existent
	 * @param new_value  new value that will replace the former.
	 *                   NULL if it's being deleted
	 *
	 * @returns ERR_QUOTA_EXCEEDED if quota exceeds
	 */
	OP_STATUS CheckQuotaOverflow(const WebStorageValue* key, const WebStorageValue* old_value, const WebStorageValue* new_value);

	/**
	 * Calculates the available quota that this DB can use in total,
	 * including the space it already occupies now.
	 */
	OP_STATUS CalculateAvailableDataSize(OpFileLength* available_size);

	enum { HASH_TABLE_MIN_SIZE = 7 };

	WebStorageValueInfoTable m_storage_area;               ///<* hash table with key/value pairs accessible by key
	OpVector<WebStorageValueInfo> m_storage_area_sorted;   ///<* vector with key/value pairs sorted by index
	List<WebStorageBackendOperation> m_pending_operations; ///<* queue of pending operations

	double m_last_operation_time;                          ///<* wall clock ms when the last operation completed
	unsigned m_executed_operation_count;                   ///<* number of operations that completed

	FileStorageLoader *m_data_loader;
	FileStorageSaver *m_data_saver;

	OP_STATUS m_init_error;     ///<* error that happened during initialization
	unsigned m_errors_delayed;  ///<* number of errors ignored during file initialization. See MAX_ACCEPTABLE_ERRORS.

	enum { MAX_ACCEPTABLE_ERRORS = 2 };

	/**
	 * Used for OnQuotaReply
	 */
	WebStorageBackend_SimpleImpl_QuotaCallback *m_quota_callback;
	BOOL m_reply_cancelled;
	BOOL m_reply_allow_increase;
	OpFileLength m_reply_new_quota_size;
};
#endif //WEBSTORAGE_ENABLE_SIMPLE_BACKEND

#endif //_MODULE_DATABASE_WEBSTORAGE_DATA_ABSTRACTION_SIMPLE_IMPL_H_
