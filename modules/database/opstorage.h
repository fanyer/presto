/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULE_DATABASE_OPSTORAGE_H
#define _MODULE_DATABASE_OPSTORAGE_H

#include "modules/database/src/opdatabase_base.h"

#ifdef OPSTORAGE_SUPPORT


#include "modules/database/webstorage_data_abstraction.h"
#include "modules/hardcore/mh/mh.h"

class OpStorage;
class OpStorageManager;
class WebStorageOperationCallbackOpStorageWrapper;


#include "modules/url/url2.h" // Because of URL

class OpStorageEventMessageHandler;
class OpStorageEventListener;
class DOM_Environment;

class OpStorageEventContext
{
public:
	OpStorageEventContext() : m_env(NULL){}
	OpStorageEventContext(const URL& url, DOM_Environment* env) : m_url(url), m_env(env){}
	~OpStorageEventContext(){}

	void Clear() { m_url = URL(); m_env = NULL; }

	BOOL IsSameDOMEnvironment(DOM_Environment* env) const { return m_env == env; }

	OpStorageEventContext& operator=(const OpStorageEventContext& o) { m_url = o.m_url; m_env = o.m_env; return *this; }

	URL              m_url; ///< URL from caller that is issuing the storage operation. Used only for StorageEvent.url.
private:
	DOM_Environment* m_env; ///< Shallow, never dereferenced, pointer to the dom env which issued the storage operation. Used for context
};

/**
 * This class holds a set of OpStorage object that belong to
 * a browsing context. This means that there will be many
 * instances of this class. When the object is destroyed all
 * the OpStorage objects owned are destroyed as well
 */
class OpStorageManager : private OpRefCounter
{
public:
	class OpStorageMgrIndexEntry;

	OpStorageManager();

	/**
	 * This methd must be called when a reference owner wants to
	 * dispose of it.
	 */
	void Release();

	/**
	 * from OpRefCounter
	 * Bypass private inheritance
	 */
	inline void IncRefCount() { OpRefCounter::IncRefCount(); }
	inline unsigned GetRefCount() { return OpRefCounter::GetRefCount(); }

	/**
	 * Factory method. Creates empty object which is readily usable
	 */
	static OpStorageManager* Create();

#ifdef SELFTEST

	/**
	 * Factory method. Can be used only on selftests
	 */
	static OpStorageManager* CreateForSelfTest();
#endif

	/**
	 * This method returns a new OpStorage object ready to be used.
	 * If the object does not exist, it is created.
	 * The object will always be owned by the OpStorageManager
	 *
	 * Note: OpStorage are shared among all the callees, therefore
	 * references are counted. After receiving the storage object by calling
	 * this function, the reference owner must dispose of the object
	 * using OpStorage::Release(). Check OpStorage documentation
	 * for more details
	 *
	 * @param type            type of webstorage to create. WEB_STORAGE_SESSION
	 *                        type only keep the data alive in the object itself,
	 *                        so when the manager is deleted, the data is deleted
	 *                        as well. If WEB_STORAGE_LOCAL is used, the data will
	 *                        be shared among all OpStorage object of the same type,
	 *                        origin, and persistency. The data will be stored on disk
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage object
	 * @param is_persistent   persistency flag. To be used to tell apart OpStorage
	 *                        objects between privacy mode and normal mode.
	 *                        Note: although WEB_STORAGE_SESSION is not persistent
	 *                        always this parameter needs to be respected because of
	 *                        privacy mode
	 * @return the new Opstorage object. Will never Be NULL. errors are reported by leaving .
	 *         If access to this storage area is forbidden then this will return OpStatus::ERR_NO_ACCESS.
	 */
	OP_STATUS GetStorage(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorage** ops);

	/**
	 * This function clears the persistent storage area for the given type, context id and origin.
	 * It's just a convenient shorthand to avoid doing a GetStorage/Clear/Release.
	 * This method does not cover volatile storage areas because it would be useless considering
	 * that volatile storage areas do not storage data on disk and are cleared when their
	 * reference owners dispose of them
	 *
	 * @param type            type of webstorage to clear
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage objec
	 */
	OP_STATUS ClearStorage(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin);

	/**
	 * Returns the OpStorageMgrIndexEntry class associated with the storage for
	 * the given context id, origin and persistency. If such entry does not exist,
	 * it is not created and NULL is returned instead
	 *
	 * @param type            type of webstorage to create. Check GetStorage
	 *                        for details about this argument
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage object
	 * @param is_persistent   persistency flag. Check GetStorage for details about this argument
	 */
	OpStorageMgrIndexEntry* GetEntry(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent) const;


	/**
	 * Adds an event listener for storage events for the storage area
	 * identified by the given context id, origin and persistency.
	 * Implementation note: because the listeners are linked lists,
	 * they won't be added twice to the queue and called more than
	 * once during an event
	 *
	 * @param type            type of webstorage to create. Check GetStorage
	 *                        for details about this argument
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage object
	 * @param is_persistent   persistency flag. Check GetStorage for details about this argument
	 *
	 * @return status of the call, any error means that the listener was not added
	 */
	OP_STATUS AddStorageEventListener(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorageEventListener* listener);

	/**
	 * Removes the given event listener for the storage area
	 * identified by the given context id, origin and persistency
	 * if it has been previously registered
	 *
	 * @param type            type of webstorage to create. Check GetStorage
	 *                        for details about this argument
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage object
	 * @param is_persistent   persistency flag. Check GetStorage for details about this argument
	 *
	 */
	void RemoveStorageEventListener(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorageEventListener* listener);

	/**
	 * Tells if there are any event listeners registered for the storage area
	 * identified by the given context id, origin and persistency
	 *
	 * @param type            type of webstorage to create. Check GetStorage
	 *                        for details about this argument
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage object
	 * @param is_persistent   persistency flag. Check GetStorage for details about this argument
	 *
	 * @return TRUE if there are event listeners, FALSE if there aren't
	 */
	BOOL HasStorageEventListeners(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent) const;

	/**
	 * Returns number of event listeners registered for the storage area
	 * identified by the given context id, origin and persistency
	 *
	 * @param type            type of webstorage to create. Check GetStorage
	 *                        for details about this argument
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage object
	 * @param is_persistent   persistency flag. Check GetStorage for details about this argument
	 *
	 * @return number of registered event listeners. Obviously, 0 if none
	 */
	unsigned GetNumberOfListeners(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent) const;


	/**
	 * OpstorageManager stores an index of OpStorage objects
	 * given a set of meta data: origin, url context id and persistency.
	 * This class represents the data of a single OpStorage object
	 * and hold both the message queue for storage events,
	 * and the OpStorage object itself
	 */
	class OpStorageMgrIndexEntry : public PS_Base
	{
	private:
		OpStorageMgrIndexEntry()
			: m_type(WEB_STORAGE_START)
			, m_context_id(DB_NULL_CONTEXT_ID)
			, m_origin(NULL)
			, m_storage_obj(NULL)
			, m_message_handler(NULL)
			, m_storage_manager(NULL) {}
	public:
		enum OpStorageMgrIndexEntryFlags{
			IS_PERSISTENT_STORAGE = 0x100
		};
		~OpStorageMgrIndexEntry();
	public:
		/**
		 * Factory method. Creates a new OpStorageMgrIndexEntry
		 * instance.
		 *
		 * @param new_storage_manager
		 * @param type                 type of webstorage to create. Check GetStorage
		 *                             for details about this argument
		 * @param context_id           url context id is applicable
		 * @param origin               origin for the Storage object
		 * @param is_persistent        persistency flag. Check GetStorage for details about this argument
		 *
		 * @return the new instance. If NULL then it's OOM.
		 */
		static OpStorageMgrIndexEntry* Create(OpStorageManager* new_storage_manager, WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent);

		/**
		 * Returns the type of webstorage object associated with this storage object.
		 */
		inline WebStorageType GetType() const { return m_type; }

		/**
		 * Returns the origin associated with this storage backend.
		 */
		inline const uni_char *GetOrigin() const { return m_origin; }

		/**
		 * Tells if the storage object contained is persistent or not
		 */
		inline BOOL IsPersistent() const { return GetFlag(IS_PERSISTENT_STORAGE); }

		/**
		 * Returns the url context id of this object in case multiple context are supported
		 */
		inline URL_CONTEXT_ID GetUrlContextId() const { return m_context_id; }

		/**
		 * Tells if the data withheld will be kept only in memory, therefore not being saved to disk
		 */
		inline BOOL IsVolatile() const { return GetType() == WEB_STORAGE_SESSION || !IsPersistent(); }

		/**
		 * Tells if this object can be safely deleted, meaning
		 * reference count at 0 and no event listeners
		 */
		BOOL CanBeDeleted();

		/**
		 * Safely deletes this object by previously checking if
		 * it can be deleted using CanBeDeleted
		 */
		void SafeDelete();

		/**
		 * Wrapper around the same function in the OpStorageManager
		 */
		WebStorageOperationCallbackOpStorageWrapper *GetUnusedCallbackObj()
		{ return m_storage_manager->GetUnusedCallbackObj(); }

		/**
		 * Wrapper around the same function in the OpStorageManager
		 */
		void DisposeCallbackObj(WebStorageOperationCallbackOpStorageWrapper* obj)
		{ m_storage_manager->DisposeCallbackObj(obj); }

		WebStorageType m_type;
		URL_CONTEXT_ID m_context_id;
		uni_char* m_origin;

		OpStorage* m_storage_obj;                         //<* the OpStorage object, if any
		OpStorageEventMessageHandler* m_message_handler;  //<* listener for storage events
		OpStorageManager* m_storage_manager;              //<* storage manager to which this object belongs
	};

private:

	~OpStorageManager();

	/**
	 * Similar to GetEntry. Returns the OpStorageMgrIndexEntry class associated
	 * with the storage for the given context id, origin and persistency. If
	 * such entry does not exist, it is created
	 *
	 * @param type            type of webstorage to create. Check GetStorage
	 *                        for details about this argument
	 * @param context_id      url context id is applicable
	 * @param origin          origin for the Storage object
	 * @param is_persistent   persistency flag. Check GetStorage for details about this argument
	 * @param st_entry        output arg, new OpStorageMgrIndexEntry instance
	 *
	 * @return status of the call
	 */
	OP_STATUS CreateEntry(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent, OpStorageManager::OpStorageMgrIndexEntry** st_entry);

	/**
	 * This function synchronously deletes the entry with the associated
	 * metadata. Only call if it can be called, which means, the owned
	 * OpStorage must have ref counter at 0 and no event listeners
	 */
	void DeleteEntry(WebStorageType type, URL_CONTEXT_ID context_id, const uni_char* origin, BOOL is_persistent);

	/**
	 * Simple overload to do auto deletion
	 */
	class StorageIndexMap : public OpStringHashTable<OpStorageMgrIndexEntry>
	{
	public:
		StorageIndexMap() : OpStringHashTable<OpStorageMgrIndexEntry>(TRUE) {}
		virtual ~StorageIndexMap() {DeleteAll();}
	};

	/**
	 * This method returns an unused callback object from the pool.
	 * If the pool is empty, then a new object is created.
	 * Note: the returned object is on its own, it's not owned by the pool.
	 * Object will be re-owned by the pool after calling DisposeCallbackObj
	 *
	 * @returns     NULL on OOM, else the new object
	 */
	WebStorageOperationCallbackOpStorageWrapper *GetUnusedCallbackObj();

	/**
	 * This method must be called when a callback object is no longer necessary.
	 * The callback will be placed on the pool, of if the pool is too big, it'll
	 * be deleted
	 *
	 * @param obj      callback object to put on the pool
	 */
	void DisposeCallbackObj(WebStorageOperationCallbackOpStorageWrapper* obj);

	/**
	 * m_storage_index stores the index of OpStorage objects and its event listeners
	 */
	typedef OpPointerHashTable<void, StorageIndexMap> ContextIdToStorageIndexHashTable;
	ContextIdToStorageIndexHashTable* m_storage_index[2][WEB_STORAGE_END];

	/**
	 * Pool of callback wrapper objects, so they can be reused
	 */
	List<WebStorageOperationCallbackOpStorageWrapper> m_callback_wrapper_pool;
	unsigned m_callback_wrapper_pool_length;
	enum { MAX_POOL_SIZE = 4 };

#ifdef SELFTEST
	BOOL m_selftest_instance;
#endif

	friend class OpStorageMgrIndexEntry;
};

/**
 * This class is the main class for Web Storage implementation.
 * It wraps the WebStorageBackend APIs and adds events support,
 * caching, ref counting and can be owned by an OpStorageManager.
 * Important: storage events are only generated when content
 * changes not when it's set to the same value
 */
class OpStorage
	: public PS_Base
	, public OpRefCounter
	, public PS_ObjDelListener
	, public WebStorageStateChangeListener
	DB_INTEGRITY_CHK(0xbbccaa33)
{
public:

	enum OpStorageFlags
	{
		//cached length
		HAS_CACHED_NUM_ITEMS = 0x100
	};

	/**
	 * Must be called when a reference owner disposes of that
	 * reference, so the object can do cleanup if needed. After
	 * Calling this method, the reference to this object is no
	 * longer guaranteed to be valid.
	 */
	inline void Release() { DecRefCount(); SafeDelete(); }

	/**
	 * Returns the type of webstorage object associated with this storage backend
	 */
	inline WebStorageType  GetType() const { return m_storage_entry->GetType(); }

	/**
	 * Returns the origin per html5 associated with this storage backend
	 */
	inline const uni_char* GetOrigin() const { return m_storage_entry->GetOrigin(); }

	/**
	 * Returns the origin per html5 associated with this storage backend
	 */
	inline BOOL            IsPersistent() const { return m_storage_entry->IsPersistent(); }

	/**
	 * Returns the url context id of this object in case multiple context are supported
	 */
	inline URL_CONTEXT_ID  GetUrlContextId() const { return m_storage_entry->GetUrlContextId(); }

	/**
	 * Tells if the data withheld will be kept only in memory, therefore not being saved to disk
	 */
	inline BOOL IsVolatile() const { return m_storage_entry->IsVolatile(); }

	/**
	 * See WebStorageBackend::GetNumberOfItems
	 */
	OP_STATUS GetNumberOfItems(WebStorageOperationCallback* callback);

	/**
	 * See WebStorageBackend::GetKeyAtIndex
	 */
	OP_STATUS GetKeyAtIndex(unsigned index, WebStorageOperationCallback* callback);

	/**
	 * See WebStorageBackend::GetItem
	 */
	OP_STATUS GetItem(const WebStorageValue *key, WebStorageOperationCallback* callback);

	/**
	 * See WebStorageBackend::SetItem
	 * This method might generate storage events
	 */
	OP_STATUS SetItem(const WebStorageValue *key, const WebStorageValue *value, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx = OpStorageEventContext());

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	/**
	 * See WebStorageBackend::SetItemReadOnly
	 * This method might generate storage events
	 */
	OP_STATUS SetItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx = OpStorageEventContext());

	/**
	 * See WebStorageBackend::SetNewItemReadOnly
	 * This method might generate storage events
	 */
	OP_STATUS SetNewItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx = OpStorageEventContext());
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

	/**
	 * See WebStorageBackend::EnumerateAllKeys
	 */
	OP_STATUS EnumerateAllKeys(WebStorageKeyEnumerator* enumerator);

	/**
	 * See WebStorageBackend::Clear
	 * This method might generate storage events
	 *
	 * @param url        url of caller used for the storage event
	 */
	OP_STATUS Clear(WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx = OpStorageEventContext());

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	/**
	 * See WebStorageBackend::ClearAll
	 * This method might generate storage events
	 *
	 * @param url        url of caller used for the storage event
	 */
	OP_STATUS ClearAll(WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx = OpStorageEventContext());
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

#ifdef SELFTEST
	/**
	 * See WebStorageBackend::CancelAll
	 * No storage events are generated
	 */
	void CancelAll();
#endif

	/**
	 * Returns the storage entry that owns this object
	 */
	inline OpStorageManager::OpStorageMgrIndexEntry* GetEntry() const { OP_ASSERT(m_storage_entry != NULL); return m_storage_entry; }

	/**
	 * Returns the storage manager that owns this object
	 */
	inline OpStorageManager* GetManager() const { return m_storage_entry->m_storage_manager; }

	/**
	 * These functions control the cached value of the number of items
	 */
	BOOL HasCachedNumberOfItems() const { return GetFlag(HAS_CACHED_NUM_ITEMS); }
	unsigned GetCachedNumberOfItems() const { return HasCachedNumberOfItems() ? m_cached_num_items : -1; }
	void SetCachedNumberOfItems(unsigned new_count) { SetFlag(HAS_CACHED_NUM_ITEMS); m_cached_num_items = new_count; }
	void UnsetCachedNumberOfItems() { UnsetFlag(HAS_CACHED_NUM_ITEMS); }

	/**
	 * Proxy to GetEntry()->m_message_handler->HasListeners()
	 */
	BOOL HasStorageEventListeners() const;

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	/**
	 * Notification method for the OpGadgetManager to tell the storage object
	 * that the default widget.prefs found in config.xml have been set.
	 */
	void OnDefaultWidgetPrefsSet();
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

protected:

	friend class OpStorageManager;
	friend class OpStorageManager::OpStorageMgrIndexEntry;

	virtual ~OpStorage();
	OpStorage(OpStorageManager::OpStorageMgrIndexEntry* storage_entry, WebStorageBackend* wsb);

	/**
	 * Internal to SetItem, SetItemReadOnly and SetnewItemReadonly.
	 */
	OP_STATUS SetItemAux(WebStorageOperation::WebStorageOperationEnum type, const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback, const OpStorageEventContext& event_ctx);

	/**
	 * Deletes this object if it's safe to do so.
	 * Such condition expect ref count at 0 and the object
	 * not being deleted already
	 */
	void SafeDelete();

	/**
	 * From WebStorageStateChangeListener::HandleStateChange
	 * State listener which listens for mutation in the underlying
	 * WebStorageBackend object
	 */
	void HandleStateChange(WebStorageStateChangeListener::StateChange v);

	/**
	 * Releases the underlying WebStorageBackend object so it can be cleaned
	 * up. The argument force is passed to WebStorageBackend::Release so
	 * the behavior is the same
	 */
	void ReleaseWSB(BOOL force);

	OpStorageManager::OpStorageMgrIndexEntry* m_storage_entry;    //*< storage entry that owns this storage object
	WebStorageBackend*                        m_wsb;              //*< associated webstorage backend
	unsigned                                  m_cached_num_items; //*< cached number of items
};

/**
 * This class represents the data of a storage event. Storage events
 * happen when an OpStorage object contents mutate. Events can be single
 * key/value pair events or clear events which erase everything. The
 * object contains the key, previous and new value, and the url
 * passed to SetItem, SetItemReadOnly, Clear and ClearAll
 */
class OpStorageValueChangedEvent : protected OpRefCounter
{
public:
	/**
	 * Struct that holds the values of the storage event
	 * for mutated key/value pairs
	 */
	struct ValueChangedData
	{
		uni_char* m_key;
		unsigned  m_key_length;

		uni_char* m_old_value;
		unsigned  m_old_value_length;

		uni_char* m_new_value;
		unsigned  m_new_value_length;
	};

	/**
	 * Factory method. Creates a new object which refers to a
	 * single key/value pair mutation
	 *
	 * @param storage_type       type of the OpStorage that mutated
	 * @param key                key of the key/value pair that changed.
	 *                           Can have 0 chars
	 * @param key_length         length of key
	 * @param old_value          value before the change. Can have 0
	 *                           chars. NULL if it did not exist before
	 * @param old_value_length   length of old_value
	 * @param new_value          value after the change. Can have 0 chars
	 *                           NULL if value was erased
	 * @param new_value_length   length of new_value
	 * @param url                url of caller used for the storage event
	 * @param data               output pointer where the new object will be placed
	 */
	static OP_STATUS Create(WebStorageType storage_type,
			const uni_char* key, unsigned key_length, const uni_char* old_value,
			unsigned old_value_length, const uni_char* new_value,
			unsigned new_value_length, const OpStorageEventContext& event_ctx,
			OpStorageValueChangedEvent*& data);

	/**
	 * Factory method. Creates a new object which refers to a
	 * clear event, therefore not bound to any specific key/value pair
	 * so m_event_data will be NULL.
	 *
	 * @param storage_type       type of the OpStorage that mutated
	 * @param url                url of caller used for the storage event
	 * @param data               output pointer where the new object will be placed
	 */
	static OP_STATUS Create(WebStorageType storage_type,
			const OpStorageEventContext& event_ctx, OpStorageValueChangedEvent*& data_obj);

	/**
	 * Tells if this object is from a clear event
	 */
	inline BOOL IsClearEvent() const { return m_is_clear;}

	/**
	 * The same Storage event object will be shared among all event
	 * listeners so it'll have a ref counter to decide when to clean up.
	 * This method must be called by the event listeners when they no
	 * longer need the object. After calling this method, pointer might
	 * no longer be valid so they must be disregarded.
	 */
	inline void Release() { OP_ASSERT(GetRefCount()>0); DecRefCount(); SafeDelete(); }

	/**
	 * Type of OpStorage affected by ths event
	 */
	inline WebStorageType GetType() const { return m_storage_type; }

	/**
	 * Wrapper due to protected inheritance
	 */
	inline void IncRefCount() { OpRefCounter::IncRefCount(); }

	/**
	 * Wrapper due to protected inheritance
	 */
	inline unsigned GetRefCount() const { return OpRefCounter::GetRefCount(); }

	ValueChangedData*     m_event_data;  ///<* NULL if clear event else key/value pair event data
	OpStorageEventContext m_context;     ///<* context of caller that generated the event

private:

	friend class OpStorageEventMessageHandler;
	friend class OpStorageGlobals;

	OpStorageValueChangedEvent() {}
	~OpStorageValueChangedEvent() {}

	/**
	 * Safely deletes this object if ref count is at 0
	 */
	inline void SafeDelete() { if (GetRefCount() == 0) { m_context.~OpStorageEventContext(); OP_DELETEA((byte*)this); } }

	bool           m_is_clear;      ///<* tells if this is a clear event or not
	WebStorageType m_storage_type;  ///<* type of storage affected by this event
};

/**
 * This interface must be implemented by whatever needs to listen
 * for storage events. Event listeners are registered in the
 * OpStorageManager
 */
class OpStorageEventListener : public ListElement<OpStorageEventListener>
{
public:
	OpStorageEventListener() {}
	virtual ~OpStorageEventListener() { Out(); }

	/**
	 * This method shall be called for a storage event.
	 * The implementation can keep the event object for an indefinite
	 * amount of time and must dispose of it calling Release() when
	 * no longer needed
	 * See also OpStorageValueChangedEvent
	 *
	 * @param event            object with event information, never NULL
	 */
	virtual OP_STATUS HandleEvent(OpStorageValueChangedEvent *event) = 0;

	/**
	 * Implementations of this class will typically never be
	 * the last destination of storage events. Rather they'll proxy
	 * events to other destinations like DOM events, which the web storage
	 * spec specifies. So, this method allows the underlying events
	 * logic to skip event objects which HasListener() return FALSE
	 * meaning that this object, although added as listener,
	 * as of the time of the HasListeners call will not proxy the event
	 * to no other user. If this method returns FALSE, then HandleEvent
	 * will not be called.
	 */
	virtual BOOL HasListeners() = 0;
};

/********************
 * This class implements the message queue and events handling for a given origin
 * Instances of this class will be stored in the OpStorageManager and have an
 * associated OpStorage object, although this object has separate use cases and API
 * from OpStorage
 *
 * Important: the logic that dispatches events to listeners caps events at a maximum
 * of TWEAK_OPSTORAGE_EVENTS_MAX_AMOUNT_EVENTS concurrent events. Events must be consumed
 * before more can be dispatched. Also, event messages have minimum timeout between
 * them of TWEAK_OPSTORAGE_EVENTS_MIN_INTERVAL_MS milliseconds.
 * These heuristics exist to minimize the effects of fork bombing
 */
class OpStorageEventMessageHandler
	: public PS_Base
	, public MessageObject
	DB_INTEGRITY_CHK(0x88776655)
{
public:

	enum OpStorageFlags
	{
		//if the object has setup its message queue
		INITIALIZED_MSG_QUEUE = 0x0100
	};

	/**
	 * Constructor
	 * @param storage_entry        storage entry from the opstoragemanager that will own this object
	 */
	OpStorageEventMessageHandler(OpStorageManager::OpStorageMgrIndexEntry* storage_entry) :
		m_storage_entry(storage_entry) {}
	virtual ~OpStorageEventMessageHandler();

	/**
	 * Dispatches a storage event for a single key/valOpStorageEventMessageHandlerue pair mutation to all
	 * registered listeners. Note: listeners are notified asynchronously in a
	 * separate message.
	 *
	 * @param key            key that mutated
	 * @param old_value      previous value before change. NULL if it did not exist
	 * @param new_value      new value after the change. NULL if it was erased
	 * @param event_ctx      event_ctx of caller used for the storage event
	 */
	OP_STATUS FireValueChangedListeners(const WebStorageValue* key,
			const WebStorageValue* old_value, const WebStorageValue* new_value,
			const OpStorageEventContext& event_ctx);
	/**
	 * Dispatches a storage event for a clear operation to all registered
	 * listeners. Note: listeners are notified asynchronously in a separate
	 * message.
	 *
	 * @param event_ctx        event_ctx of caller used for the storage event
	 */
	OP_STATUS FireClearListeners(const OpStorageEventContext& event_ctx);

	/**OpStorageEventMessageHandler
	 * Registers a new event listener.
	 * Note: calling many times with the same argument will not add
	 * the listener many times to the queue.
	 * Currently, this requirement is enforced by the implementation using
	 * a linked list
	 */
	inline void AddStorageEventListener(OpStorageEventListener* listener)
	{ if(listener != NULL && !listener->InList())listener->Into(&m_storage_event_listeners); }

	/**
	 * Remove as event listener. Although this method is of little use because
	 * the listener itself is just a linked list element, it should still
	 * be used in case the implementation changes in the future.
	 */
	inline void RemoveStorageEventListener(OpStorageEventListener* listener)
	{ if(listener != NULL)listener->Out(); }

	/**
	 * Tells if this object has listener registered, which themselves
	 * return TRUE from their HasListeners() implementation.
	 * This means that there are event listeners and therefore events
	 * should be generated
	 */
	BOOL HasStorageEventListeners();

	/**
	 * Tells the number of listeners registered, which themselves
	 * return TRUE from their HasListeners() implementation.
	 * If HasStorageEventListeners() returns TRUE then this
	 * is ensured to return a value greater than 0
	 */
	unsigned GetNumberOfListeners() const;

private:
	friend class OpStorageGlobals;
	friend class OpStorageManager;

	/**
	 * Puts a storage event object in the end of the queue of events to dispatch
	 * @param event      event object to go into the queue
	 */

	OP_STATUS EnqueueEventObject(OpStorageValueChangedEvent* event);
	/**
	 * Removed a storage event from the 1st place of the queue to be
	 * passed on to listeners.
	 *
	 * @return the event object that was removed or NULL if non-existent
	 */
	OpStorageValueChangedEvent* DequeueEventObject();

	/**
	 * Removes a storage event object from the end of the queue.
	 * This shall be used when the event somehow should not be dispatched
	 * and is therefore ignored, like during an error situation
	 *
	 * @return the event object that was removed or NULL if non-existent
	 */
	OpStorageValueChangedEvent* PopEventObject();

	/**
	 * From MessageObject.
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * Message's PARAM_1 to be used for this object
	 */
	inline MH_PARAM_1 GetMessageQueueId() const { return reinterpret_cast<MH_PARAM_1>(this); }

	/**
	 * Object specific initialization.
	 */
	OP_STATUS Init();

	/**
	 * Helper class to allow storage event object to be placeable in a linked list
	 */
	struct EventQueueElement: public ListElement<EventQueueElement>{
		OpStorageValueChangedEvent* m_event;
		EventQueueElement(OpStorageValueChangedEvent* e) : m_event(e) {}
	};
	List<EventQueueElement> m_storage_event_objects;   //<* queue of event objects to process
	List<OpStorageEventListener> m_storage_event_listeners; //<* list of storage event listeners
	OpStorageManager::OpStorageMgrIndexEntry* m_storage_entry; //<* storage enty that owns this object
};

/**
 * Helper class that handles the global vars to control the number of events and
 * timeout between them, and posts messages to dispatch storage events, in a way
 * that minimizes the effects of fork bombing
 */
class OpStorageGlobals
{
public:
	/**
	 * Posts a message to dispatch a storage event on the passed message_handler.
	 *
	 * @param message_handler      object that will handle the event and notify its listeners
	 * @param event                event object
	 *
	 * @returns OpStatus::OK is message was posted, OpStatus::ERR is message was dropped
	 */
	OP_STATUS PostStorageEventMessage(OpStorageEventMessageHandler *message_handler, OpStorageValueChangedEvent* event);

	/**
	 * Get message handler to which to dispatch messages
	 */
	MessageHandler* GetMessageHandler() const { return OpDbUtils::GetMessageHandler(); };

	/**
	 * The following functions control the number of unprocessed
	 * storage event messages
	 */
	inline unsigned GetNumberOfUnprocessedEventMessages() const
	{ return m_number_of_unprocessed_storage_event_messages; }

	inline void IncNumberOfUnprocessedEventMessages()
	{ m_number_of_unprocessed_storage_event_messages++; }

	inline void DecNumberOfUnprocessedEventMessages()
	{
		if (m_number_of_unprocessed_storage_event_messages>0)
			m_number_of_unprocessed_storage_event_messages--;
	}

private:
	OpStorageGlobals() : m_last_event_time(0), m_number_of_unprocessed_storage_event_messages(0) {}

	double m_last_event_time; //<* time as returned from g_op_time_info->GetWallClockMS() when the last event message was queued
	unsigned m_number_of_unprocessed_storage_event_messages; //<* var name is explicit

	friend class DatabaseModule;
};


typedef AutoReleaseTypePtr<OpStorage> AutoReleaseOpStoragePtr;

#endif // OPSTORAGE_SUPPORT

#endif //_MODULE_DATABASE_OPSTORAGE_H
