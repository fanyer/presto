/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2009 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef _MODULE_DATABASE_OPSTORAGE_DATA_ABSTRACTION_H_
#define _MODULE_DATABASE_OPSTORAGE_DATA_ABSTRACTION_H_

#ifdef OPSTORAGE_SUPPORT

#include "modules/database/src/opdatabase_base.h"

/**
 * This class just holds a string and its length
 * The string can have 0 chars in the middle and the
 * char at position m_value_length must be guaranteed to be
 * zero for safety.
 * If you need to allocate one of these objects in the stack
 * just for temporary use, use WebStorageValueTemp
 * Note that the string might not be valid UTF16, it might
 * have any random character from 0 to 0xffff because that's
 * possible to do from ecmascript. There is a selftest that
 * tests this behavior
 */
class WebStorageValue
{
public:
	WebStorageValue() : m_value(NULL), m_value_length(0) {}
	virtual ~WebStorageValue() { OP_DELETEA(const_cast<uni_char*>(m_value)); m_value = NULL; };

	const uni_char* m_value;
	unsigned m_value_length;

	static WebStorageValue* Create(const uni_char* value);
	static WebStorageValue* Create(const uni_char* value, unsigned value_length);
	static WebStorageValue* Create(const WebStorageValue* value);

	void Set(const uni_char* value, unsigned value_length)
	{ m_value = value; m_value_length = value_length; }

	OP_STATUS Copy(const uni_char* value, unsigned value_length);
	OP_STATUS Copy(const WebStorageValue* value);

	BOOL Equals(const WebStorageValue* v) const { return v == this || (v != NULL && Equals(*v)); }
	BOOL Equals(const WebStorageValue& v) const { return Equals(v.m_value, v.m_value_length); }

	BOOL Equals(const uni_char* value, unsigned value_length) const;
};

#define WSV_STR(v) ((v) != NULL ? (v)->m_value : NULL)
#define WSV_LENGTH(v) ((v) != NULL ? (v)->m_value_length : 0)

/**
 * Derived version of WebStorageValue that can be used to
 * create a temporary WebStorageValue which buffer isn't deleted
 * before passing to a function
 */
class WebStorageValueTemp : public WebStorageValue
{
public:
	WebStorageValueTemp() {}
	WebStorageValueTemp(const WebStorageValue& v) { Set(v); }
	WebStorageValueTemp(const uni_char* value, unsigned value_length) { Set(value, value_length); }

	void Set(const WebStorageValue* v)
	{
		if (v != NULL)
			Set(*v);
		m_value = NULL;
		m_value_length = 0;
	}

	void Set(const WebStorageValue& v)
	{ m_value = v.m_value; m_value_length = v.m_value_length; }

	void Set(const uni_char* value, unsigned value_length)
	{ m_value = value; m_value_length = value_length; }

	const WebStorageValueTemp& operator=(const WebStorageValue& v)
	{ Set(v); return *this; }

	virtual ~WebStorageValueTemp() { m_value = NULL; m_value_length = 0; };
};

/**
 * Set of custom errors that this API uses. Some of the error codes
 * can be returned to the web storage operation callback
 */
typedef PS_Status WebStorageStatus;

/**
 * This struct represents the use of one function on the WebStorageBackend API
 * and is used to return data asynchronously to the callee when an operation completes.
 */
struct WebStorageOperation
{
	WebStorageOperation() : m_type(NO_OP) { op_memset(&m_data, 0, sizeof(m_data)); op_memset(&m_error_data, 0, sizeof(m_error_data)); }
	virtual ~WebStorageOperation();

	/**
	 * Help method. Clones this object into 'dest'
	 *
	 * @param dest    object which will get a copy of
	 *                the data of this object.
	 */
	OP_STATUS CloneInto(WebStorageOperation* dest) const;

	/**
	 * Clears this object of all data, freeing all buffers,
	 * and therefore becoming an invalid operation
	 */
	void Clear();
	/**
	 * Clears error info, still keeps operation data, if any.
	 */
	void ClearErrorInfo();

	enum WebStorageOperationEnum {
		/**
		 * Invalid operation, used only for cleanup
		 */
		NO_OP = 0,
		/**
		 * Used for GetItemByKey and GetItem
		 * The following three enums have m_value set to the value that is being returned,
		 * or NULL if the value does not exist. m_storage_mutated is FALSE
		 */
		GET_ITEM_BY_KEY = 1,
		GET_KEY_BY_INDEX,

		/**
		 * Used for GetNumberOfItems
		 * The following enum has m_item_count set to the number of stored items.
		 *  m_storage_mutated is FALSE
		 */
		GET_ITEM_COUNT,

		/**
		 * Used for SetItem
		 * The following enum has m_value set with the previous value
		 * before SetItem and m_storage_mutated set to TRUE. If the value
		 * did not exist, then m_value is NULL. If the previous value was
		 * equal to the new one being set, m_storage_mutated is FALSE
		 **/
		SET_ITEM,

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
		/**
		 * Used for SetItemReadOnly
		 *
		 * For the overload with value and is_readonly.
		 * The enum has m_value set with the previous value
		 * before SetItem and m_storage_mutated set to TRUE. If the
		 * value did not exist, then m_value is NULL. If the previous
		 * value was equal to the new one being set, m_storage_mutated
		 * is FALSE. If the value of the read-only flag changed,
		 * then m_storage_mutated is TRUE.
		 **/
		SET_ITEM_RO,
		/**
		 * Used for SetNewItemReadOnly
		 *
		 * For the overload with value and is_readonly. The enum
		 * has m_value set to NULL and m_storage_mutated set to
		 * TRUE. If a value did existed already with the same key,
		 * a ERR_READ_ONLY error is returned.
		 **/
		SET_NEW_ITEM_RO,
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

		/**
		 * The following enums does not store data on m_item_count nor m_value.
		 * m_storage_mutated is set to TRUE if data was cleared, or FALSE if the
		 * storage was empty.
		 * CLEAR is used for Clear(), CLEAR_RO for ClearAll()
		 */
		CLEAR
#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
		,CLEAR_RO
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

	};

	inline BOOL IsError() const { return OpStatus::IsError(m_error_data.m_error); }

	union {
		unsigned m_item_count;
		struct {
			const WebStorageValue* m_value;
			bool m_storage_mutated;
		} m_storage;
	} m_data;

	struct {
		const uni_char* m_error_message;
		OP_STATUS m_error;
	} m_error_data;

	WebStorageOperationEnum m_type;
};

/**
 * This class is used as callback when an operation on WebStorageBackend completes.
 * See documentation on WebStorageOperation for details
 */
class WebStorageOperationCallback
{
public:
	/**
	 * This method will be invoked when a operation completes.
	 * The passed WebStorageOperation must not be deleted by the callback
	 * and all data contained within will be owned by the object itself.
	 * If the callback needs to store data for an arbitrary amount of
	 * time, then it should clone the data it needs.
	 * The WebStorageOperation object must be kept alive until
	 * Discard is called
	 */
	virtual OP_STATUS HandleOperation(const WebStorageOperation* result) = 0;

	/**
	 * This method should return the Window to which the callee belongs.
	 * The Window will be used in case the backend needs to use the window
	 * commander API to request something to the user, like a quota increase.
	 * This method can return NULL.
	 */
	virtual Window* GetWindow() = 0;

	/**
	 * This method will be called when the backend code no longer needs
	 * the object, like after calling the HandleOperation or during
	 * forceful clean up. Cleanup should be implemented only inside this method
	 */
	virtual void Discard() = 0;
};

/**
 * This class is used as callback to enumerate all keys stored.
 * HandleKey can be called any number of times, but never after
 * Discard. Keys must be passed in index order starting at 0.
 * The order must be the same as the one supported by
 * WebStorageBackend::GetKeyAtIndex
 * The implementor is obliged to copy the values if it wishes to do so.
 * After returning from HandleKey, the buffers in key and value are not
 * guaranteed to be valid anymore.
 */
class WebStorageKeyEnumerator
{
public:
	virtual OP_STATUS HandleKey(unsigned index, const WebStorageValue* key, const WebStorageValue* value) = 0;

	/**
	 * HandleError will be called if an error happens before or during using
	 * this enumerator
	 **/
	virtual void HandleError(OP_STATUS status) = 0;

	/**
	 * This method will be called by the API when the enumerator object is not longer needed
	 * Note that it might be called before any item is enumerated IF the storage area is empty
	 * or if meanwhile core shuts down
	 */
	virtual void Discard() = 0;

	/**
	 * This method tells the backend logic that handles the enumerator
	 * that the callback method (HandleKey) wants also the values with the keys,
	 * and not only the keys. If WantsValues returns false, then HandleKey will always
	 * receive NULL for value, else it'll return the value that is stored.
	 * Opting in for values allows to optimize by not spending processing nor memory
	 * handling the values
	 *
	 * @return boolean which tells if values are wanted together with keys
	 */
	virtual BOOL WantsValues() { return FALSE; }
};


/**
 * This class allows the reference owner to keep track of internal modifications
 * of the WebStorageBackend object.
 *
 * The STORAGE_MUTATED happens when the internal storage mutates, either after
 * a SetItem, or after a Clear. This allows reference owners to keep track of
 * cached information to improve performance.
 *
 * The SHUTDOWN state change happens when the backend object is being
 * terminated. The implementation of WebStorageBackend must ensure that the
 * listeners are notified when the object is terminated so reference owners
 * can clear the references.
 *
 * IMPORTANT ! The listeners will be notified immediately BEFORE the event that
 * triggers their invocation. This means that the STORAGE_MUTATED state must be
 * notified BEFORE calling the WebStorageOperation callback, and SHUTDOWN must be
 * called before/while deleting the object, but after dropping all queued operations.
 * To make reference counting sane, during SHUTDOWN the listener's owner MUST
 * Release() the WebStorageBackend object.
 */
class WebStorageStateChangeListener
{
public:
	enum StateChange
	{
		STORAGE_MUTATED = 1,
		SHUTDOWN
	};
	WebStorageStateChangeListener() {}
	virtual ~WebStorageStateChangeListener() {}
	virtual void HandleStateChange(StateChange v) = 0;
};

/**
 * This enumeration differentiates between session storage
 * and local storage and other storage types that might exist
 */
enum WebStorageType
{
	WEB_STORAGE_START = 0,
	WEB_STORAGE_SESSION,
	WEB_STORAGE_LOCAL,
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	WEB_STORAGE_WGT_PREFS,
#endif //WEBSTORAGE_WIDGET_PREFS_SUPPORT
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	WEB_STORAGE_USERJS,
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	WEB_STORAGE_END
};

/**
 * This class specifies the interfaces for data backends for
 * localStorage and sessionStorage classes and other storage
 * types that might exist later on.
 * The backend must store either in disk or on memory, an
 * ordered set of key/value pairs
 *
 * IMPORTANT 1: all operations must be queued and the callbacks
 * must be invoked in the exact same order by which operations
 * are done. A callback function call is equivalent to one
 * operation. After a callback object is used, or no longer
 * needed, its Discard method MUST be called. After calling this,
 * the callback owner might cleanup the object, so the webstorage
 * backend should dispose of all references to the discarded callback.
 *
 * IMPORTANT 2: the callback argument for SetItem and Clear are
 * optional, meaning, that a NULL value must be handled gracefully
 * no crashes or side effects should happen, and the operation
 * must be properly completed, while respecting the queue order.
 * The callback might be enforced for read operations though
 * because it does not make any sense to do read operations
 * without a callback to pass the data on, although such
 * may happen due to internal implementation details
 *
 * IMPORTANT 3: the special APIs SetItemReadOnly, SetNewItemReadOnly
 * and ClearAll must be implemented for builds which have gadgets
 * enabled because of the widget.preferences object.
 * These APIs allow to create/delete items with a read-only
 * flag. If gadgets support is not enabled, then these APIs
 * might not need to be enabled, unless in the future a new
 * feature is developed which depends on this behavior.
 *
 * IMPORTANT 4: Release() might be called before all operations
 * are completed. After the operations complete, cleanup can
 * happen.
 *
 * IMPORTANT 5: Because of core shutdown, not all operations might
 * complete. It's advised for all pending operations to simply
 * be disregarded. Because of this HandleOperation for many
 * callback objects will not be called. However, Discard must
 * still be called as mandated by its specification.
 *
 * IMPORTANT 6: Beware that the strings held in WebStorageValues
 * might not be valid utf16, as mentioned in the class documentation
 * itself
 */
class WebStorageBackend
{
public:
	/**
	 * This is the main factory method, and must return a WebStorageBackend
	 * which will store the data for the given storage type, origin and url context id.
	 * IMPORTANT: Create might be called many times with the same set of arguments
	 * (type, origin, is_persistent, context_id). Such happens for each browsing context,
	 * so sessionStorage can keep track of many browsing contexts, while localStorage
	 * should not rely on this separation and just commit and read data from the same place.
	 *
	 * @param type          the type of web storage: sessionStorage or localStorage,
	 *                      or something else in the future
	 *
	 * @param origin        origin for this storage per html5, usually scheme://host:port
	 *                      MUST not be NULL.
	 *
	 * @param is_persistent tells if the callee is in privacy mode or not, so it will not
	 *                      store anything on disk and just keep a volatile memory storage
	 *
	 * @param context_id    url context id in case there are many users on a single core instance
	 *
	 * @param result        out parameter with the newly created WebStorageBackend object.
	 *                      if this function returns OpStatus::OK, then this value MUST NOT BE NULL.
	 *                      Multiple calls to this function with the same set of arguments CAN
	 *                      return a pointer to the exact same pointer, but calls with different sets
	 *                      of arguments should not. If the same object is returned many times, then
	 *                      the implementation must keep a reference count, and assume that the
	 *                      Release acts a decrementing the reference counter and do all cleanup
	 *
	 * @return              error status. If the function returns an error, then the
	 *                      implementation must ensure all cleanup, and set result to NULL.
	 *                      OpStatus::ERR_NO_ACCESS must be returned if the implementation
	 *                      does not allow access to this storage area
	 */
	static OP_STATUS Create(WebStorageType type, const uni_char* origin, BOOL is_persistent,
			URL_CONTEXT_ID context_id, WebStorageBackend** result);

	/**
	 * Invokes callback asynchronously with the number of items stored in this storage
	 * See documentation on WebStorageOperation for details.
	 * Implementations are encouraged to cache the value returned by this call if possible
	 * although the callback always needs to be called asynchronously
	 *
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS GetNumberOfItems(WebStorageOperationCallback* callback) = 0;

	/**
	 * Invokes callback asynchronously passing the key at the specified index, or
	 * NULL if not existent or out of range.
	 * See documentation on WebStorageOperation for details
	 *
	 * @param index index of the wanted key/value
	 *
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS GetKeyAtIndex(unsigned index, WebStorageOperationCallback* callback) = 0;

	/**
	 * Invokes callback asynchronously passing the value with the specified key, or
	 * NULL if not existent
	 * See documentation on WebStorageOperation for details
	 *
	 * @param key  key part of the key/value pair. Must never be null. The function does not
	 *             take ownership of this argument, it'll be cloned, so the callee must still
	 *             handle the value after being passed to this function
	 *
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS GetItem(const WebStorageValue *key, WebStorageOperationCallback* callback) = 0;

	/**
	 * Invokes callback asynchronously with the status of the SetItem operation.
	 * See documentation on WebStorageOperation for details.
	 * Before the callback is used, the new value must be set for the specified key.
	 * The following must be respected:
	 *  - If the key already exists, its value is overridden, and its index in the
	 *    collection kept intact
	 *  - If the key does not exist, its added to the collection and its index must be
	 *    ensured to be the last
	 *  - If the key exists and value is NULL, the key/value pair must be removed and no
	 *    longer returned by any of the methods that do read operations, unless a new
	 *    SetItem with a value is done
	 *
	 * Implementations are advised to limit the available quota for each WebStorageBackend
	 * and if quota exceeded as result of a SetItem, then the error PS_Status::
	 * ERR_QUOTA_EXCEEDED must be returned.
	 *
	 * If the implementation supports key/value pairs marked as read-only, then this
	 * method DOES NOT affect pairs marked as read-only and instead must return the
	 * error ERR_READ_ONLY! See SetItemReadOnly as well.
	 *
	 * @param key    key part of the key/value pair. Must never be null. The function does not
	 *               take ownership of key, it'll be cloned, so the callee must still handle the value
	 *               after being passed to this function
	 * @param value  value part of the key/value pair. If it is null then the value will be deleted
	 *               else it'll be inserted. The function does not take ownership of this argument,
	 *               it'll be cloned, so the callee must still handle the value after being passed
	 *               to this function
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS SetItem(const WebStorageValue *key, const WebStorageValue *value, WebStorageOperationCallback* callback) = 0;

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
	/**
	 * This method behaves EXACTLY like SetItem with the sole difference that it allows to
	 * set/unset the read-only flag of key/value pairs, and change the pairs themselves.
	 * This method will be used for widget.preferences, which means that it might not need to
	 * be implemented in platforms that have gadgets disabled
	 *
	 * @param key          key part of the key/value pair. Must never be null. The function does not
	 *                     take ownership of key, it'll be cloned, so the callee must still handle the value
	 *                     after being passed to this function
	 * @param value        value part of the key/value pair. If it is null then the value will be deleted
	 *                     else it'll be inserted. The function does not take ownership of this argument,
	 *                     it'll be cloned, so the callee must still handle the value after being passed
	 *                     to this function
	 * @param is_readonly  new value of the read-only flag
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS SetItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback) = 0;

	/**
	 * This method behaves EXACTLY like SetItemReadOnly with the difference that it does not
	 * modify the stored value if it already exists. Instead, it results in a PS_Status::ERR_READ_ONLY error.
	 * This method will be used for widget.preferences, which means that it might not need to
	 * be implemented in platforms that have gadgets disabled
	 *
	 * @param key          key part of the key/value pair. Must never be null. The function does not
	 *                     take ownership of key, it'll be cloned, so the callee must still handle the value
	 *                     after being passed to this function
	 * @param value        value part of the key/value pair. If it is null then the value will be deleted
	 *                     else it'll be inserted. The function does not take ownership of this argument,
	 *                     it'll be cloned, so the callee must still handle the value after being passed
	 *                     to this function
	 * @param is_readonly  new value of the read-only flag
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS SetNewItemReadOnly(const WebStorageValue *key, const WebStorageValue *value, BOOL is_readonly, WebStorageOperationCallback* callback) = 0;

#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

	/**
	 * Invokes enumerator asynchronously enumerating all the keys in index order.
	 * See WebStorageKeyEnumerator for details.
	 *
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS EnumerateAllKeys(WebStorageKeyEnumerator* enumerator) = 0;

	/**
	 * Empties this storage completely. All key/value pairs are deleted, therefore the next SetItem
	 * will start again at 0.
	 * See WebStorageKeyEnumerator for details.
	 *
	 * If the implementation supports key/value pairs marked as read-only, then this
	 * method DOES NOT affect pairs marked as read-only! See SetItemReadOnly as well.
	 *
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS Clear(WebStorageOperationCallback* callback) = 0;

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT

	/**
	 * Same as Clear, but this function clears also readonly entries
	 *
	 * @return status of call. if an error is returned, then the callback object will not be used
	 */
	virtual OP_STATUS ClearAll(WebStorageOperationCallback* callback) = 0;
#endif //WEBSTORAGE_RO_PAIRS_SUPPORT

	/**
	 * This method allows the object owner to notify that it will no longer be using this object.
	 * This method should do all necessary clean up associated with this object.
	 * If there are operations which have not completed, those should be handled after the 'force'
	 * argument, and the Discard method of the callbacks should be invoked.
	 *
	 * Important: because of core shutdown, the underlying implementation may delete this object before
	 * Release is called by the reference owners, due to module destruction order. If so, then
	 * InvokeShutdownListeners() must be called so reference owners will know that the object is going
	 * to be deleted. Therefore, reference owners will call Release for consistency with the rest of
	 * the business logic but also because of ref counting if such is used. So, the implementation must
	 * expect Release to be called during InvokeShutdownListeners
	 *
	 * @param force If this argument has value FALSE, then the object can do async clean up. A typical
	 *              use case if for when a document is unloaded and the garbage collector cleans up
	 *              DOM_Objects that might keep references to this object.
	 *              If this argument has value TRUE, then the implementation must do all necessary
	 *              clean up synchronously if possible. This will be used during core shutdown
	 *              If the implementation can keep track of core shutdown, then this argument can be
	 *              ignored as long as the cleanup is done, but beware that the database module needs
	 *              to cleanup first because it could hold pointers to these objects
	 */
	virtual void Release(BOOL force) = 0;

	/**
	 * Returns the type of webstorage object associated with this storage backend
	 */
	inline WebStorageType  GetType() const { return m_type; }
	/**
	 * Returns the origin per html5 associated with this storage backend
	 */
	inline const uni_char* GetOrigin() const { return m_origin; }
	/**
	 * Returns the origin per html5 associated with this storage backend
	 */
	inline BOOL            IsPersistent() const { return m_is_persistent; }
	/**
	 * Returns the url context id of this object in case multiple context are supported
	 */
	inline URL_CONTEXT_ID  GetUrlContextId() const { return m_context_id; }

	/**
	 * Add a shutdown notifier
	 */
	OP_STATUS AddListener(WebStorageStateChangeListener* cb);

	/**
	 * Removes a shutdown notifier
	 */
	void RemoveListener(WebStorageStateChangeListener* cb);

#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	/**
	 * Notification method for the OpGadgetManager to tell the storage object
	 * that the default widget.prefs found in config.xml have been set.
	 */
	inline void OnDefaultWidgetPrefsSet() { m_default_widget_prefs_set = TRUE; }

	/**
	 * Tells if the default widget.prefs have been set since this object was created.
	 */
	BOOL HasSetDefaultWidgetPrefs() const { return m_default_widget_prefs_set; }
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT

#ifdef SELFTEST
	/**
	 * This is a helper method for selftests to cancel and cleanup
	 * everything after a failed test.
	 * The Discard callback on the pending callback objects should be called
	 */
	virtual void CancelAll() = 0;
#endif

protected:
	WebStorageBackend();
	virtual ~WebStorageBackend();

	/**
	 * This method must be called by the implementation
	 * then the backend object is detroyed. This allows for reference
	 * owners to be notified so they can clear the references
	 */
	void InvokeMutationListeners();
	/**
	 * This method must be called by the implementation
	 * then the backend object is detroyed. This allows for reference
	 * owners to be notified so they can clear the references
	 */
	void InvokeShutdownListeners();

	/**
	 * Object initialization. Only initializes the class members.
	 * Derived classes might call this method to save some code
	 */
	virtual OP_STATUS Init(WebStorageType type, const uni_char* origin,
			BOOL is_persistent, URL_CONTEXT_ID context_id);

private:
	//Forward declare helper class. Not relevant to
	class WebStorageStateChangeListenerLink;
	List<WebStorageStateChangeListenerLink> m_state_listeners;
	WebStorageType m_type;
	uni_char *m_origin;
	URL_CONTEXT_ID m_context_id;
	BOOL m_is_persistent;
#ifdef WEBSTORAGE_WIDGET_PREFS_SUPPORT
	BOOL m_default_widget_prefs_set;
#endif // WEBSTORAGE_WIDGET_PREFS_SUPPORT
};

#ifdef WEBSTORAGE_RO_PAIRS_SUPPORT
# define WEBSTORAGE_CLEAR_ALL(storage_obj, callback) (storage_obj)->ClearAll(callback)
#else
# define WEBSTORAGE_CLEAR_ALL(storage_obj, callback) (storage_obj)->Clear(callback)
#endif // WEBSTORAGE_RO_PAIRS_SUPPORT


#endif //OPSTORAGE_SUPPORT

#endif //_MODULE_DATABASE_OPSTORAGE_DATA_ABSTRACTION_H_
