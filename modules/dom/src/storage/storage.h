 /* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef _MODULES_DOM_SRC_STORAGE_STORAGE_H_
#define _MODULES_DOM_SRC_STORAGE_STORAGE_H_

#ifdef CLIENTSIDE_STORAGE_SUPPORT

#include "modules/dom/src/domobj.h"
#include "modules/database/opstorage.h"
#include "modules/ecmascript_utils/esthread.h"
#include "modules/util/simset.h"

class DOM_EnvironmentImpl;
class FramesDocument;
class Window;
#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
class DOM_UserJSThread;
#endif //WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

class DOM_Storage;

class DOM_Storage_OperationCallback;
class DOM_Storage_KeyEnumerator;
class ES_PropertyEnumerator;

/**
 * Restart object for storage objects. It keeps track of the database
 * operation and the thread blocked on its completion, resuming it
 * when either completed. Dually, if the thread is cancelled, the
 * database operation is disconnected.
 */
class DOM_Storage_OperationRestartObject
	: public ES_RestartObject
{
private:
	DOM_Storage_OperationRestartObject(DOM_Storage_OperationCallback *cb, DOM_Storage_KeyEnumerator *key_enumerator, DOM_Storage *storage);

	friend class DOM_Storage_OperationCallback;
	friend class DOM_Storage_KeyEnumerator;

	DOM_Storage_OperationCallback *m_cb;
	/**< The callback that this restart object is waiting for. */

	DOM_Storage_KeyEnumerator *m_enumerator;
	/**< The enumerator that this restart object is waiting for.
	     Non-NULL iff 'm_cb' is NULL. */

	DOM_Storage *m_dom_storage;
	/**< The current storage object being accessed. */

	ES_Thread *m_thread;
	/**< The thread to Block/Unblock before/after the operation is handled. */

public:
	static DOM_Storage_OperationRestartObject *Make(DOM_Storage *storage, ES_PropertyEnumerator *enumerator, DOM_Runtime *runtime);
	/**< Creates a restart object for an async storage operation. Returns NULL if
	     there wasn't memory to complete the creation. */

	~DOM_Storage_OperationRestartObject();

	DOM_Storage_OperationCallback *GetCallback() { return m_cb; }
	/**< Returns the callback this object is waiting for. */

	DOM_Storage_KeyEnumerator *GetEnumerator() { return m_enumerator; }
	/**< Returns the storage key enumerator this object is waiting for. */

	DOM_Storage *GetStorage() { return m_dom_storage; }

	void PrepareAndBlock(ES_Runtime *origining_runtime);
	/**< Prepare the callback for getting called back. This involves blocking
	 *   the current thread and setting the proper state so the object knows
	 *   what's happening. */

	void ResumeThread();
	/**< Resume blocked thread and remove our reference to it. */

	static DOM_Storage_OperationRestartObject *PopRestartObject(ES_Runtime *origining_runtime);
	/**< Upon restart from suspension, fetch the corresponding DOM
	 *   Storage restart object from the thread that blocked. */

	/* From ES_RestartObject. */
	virtual void GCTrace(ES_Runtime *runtime);
	virtual BOOL ThreadCancelled(ES_Thread *thread);
};

/**
 * Callback class for webstorage operations.
 * It mainly clones the result when it arrives so it's available when the suspended functions
 * restart.
 */
class DOM_Storage_OperationCallback : public WebStorageOperationCallback
{
private:
	friend class DOM_Storage_OperationRestartObject;

	DOM_Storage_OperationRestartObject *m_restart_object;
	/**< The restart object that has a pointer to this callback. If this is NULL then this object is not owned by anyone and must clean up itself. */

public:
	DOM_Storage_OperationCallback();
	/** < Creates a DOM_Storage_OperationCallback. */

	enum CallState
	{
		BEFORE = 1,  // Not waiting for any callback
		WAITING = 2, // The operation was launched and it's waiting for the callback
		AFTER = 3    // The callback was handled
	};

	CallState m_state; /**< The state of this callback */
	WebStorageOperation m_op; /**< Holds a copy of the results for later usage. */
	BOOL m_must_succeed; /**< Tells if the GetValue function must forcefully return OpStatus::OK. */

	void Delete();
	/**< This function shall be called when the object has been released
	     by its runtime, so the object can clean itself up. If the object
	     is waiting for the completion of an operation, then cleanup will be
	     postponed. */

	virtual OP_STATUS HandleOperation(const WebStorageOperation *result);
	/**< Handle the callback result, namely, just copy it so restarting functions can use
	 *   the result accordingly.
	 *   The operation is marked as done and the thread unblocked if it was blocked. */

	virtual Window *GetWindow();

	virtual void Discard();
	/**< From WebStorageOperationCallback, called after the callback object is not longer used */

	void Reset();
	/**< Reset the callback after the results have been used.
	 *   This function should always be called if Prepare() was called before. */

	OP_STATUS GetValue(ES_Value *value);
	/**< Convenience function to set an ES value from the callback results.
	 *
	 *   @param[out] value The respective result value. Can be NULL.
	 *
	 *   Returns error if the item requested is not in storage or the operation was a SET_ITEM
	 *   and had errors.
	 */
private:
	void SetState(CallState state);
	virtual ~DOM_Storage_OperationCallback();
};


/**
 * Key enumeration class for a webstorage object, used when asynchronously
 * generating the key set supported by the underlying storage object.
 */
class DOM_Storage_KeyEnumerator
	: public WebStorageKeyEnumerator
{
public:
	DOM_Storage_KeyEnumerator(ES_PropertyEnumerator *enumerator)
		: m_restart_object(NULL)
		, m_enumerator(enumerator)
		, m_state(BEFORE)
	{
	}

	virtual ~DOM_Storage_KeyEnumerator();

	void Delete();
	/**< This function shall be called when the object has been released
	     by its runtime, so the object can clean itself up. If the object
	     is waiting for the completion of an operation, then cleanup will be
	     postponed. */

	virtual void HandleError(OP_STATUS status);
	virtual void Discard();
	virtual OP_STATUS HandleKey(unsigned index, const WebStorageValue* key, const WebStorageValue* value);

	enum State
	{
		BEFORE = 1,  // Not started enumerating.
		WAITING = 2, // Enumeration is ongoing.
		AFTER = 3    // The enumeration has completed.
	};

private:

	friend class DOM_Storage_OperationRestartObject;

	DOM_Storage_OperationRestartObject *m_restart_object;
	ES_PropertyEnumerator *m_enumerator;
	State m_state;

	void SetState(State state);
};

/**
 * Main class that represents Storage objects
 *
 * Specification:
 * - http://dev.w3.org/html5/webstorage/
 * Wiki:
 * - https://wiki.oslo.opera.com/developerwiki/Core/projects/clisto
 *
 * About User JS Storage (CORE-24184), the following requirements apply:
 * - Firstly, the user must enable a preference for user script to have access
 *    to storage areas on opera:config.
 * - User scripts get no default quota, to ensure explicit opt-in by the user
 * - Each user script, begin a user script a single javascript file on the user
 *   scripts' folder, gets a completely sandboxed storage area. Storage areas are
 *   differentiated by origin (per html5) in the core implementation. To reuse
 *   the implementation with no extra work, the storage area origin's is the scripts'
 *   full path on the file system. Note that this file path is not visible at all
 *   to the user script, nor the storage area's origin.
 * - The storage area shall not be accessible by nothing else but its owner script.
 *   This means that the webpage will have completely separate storage areas for
 *   localStorage and sessionStorage.
 * - The storage area is only available during the "User JS" thread through the
 *   opera.scriptStorage property. When a user script accesses this property it
 *   gets its own storage area. Consequence, opera.scriptStorage will return different
 *   objects in different scripts. In practice, the storage object is bound to the
 *   DOM_UserJsThread object in core. Any attempt to access opera.scriptStorage
 *   outside the User JS thread yields undefined. Scripts can keep a live reference
 *   to the object within their scope for later use, even after the user js thread..
 * - If the script keeps a reference to the object, then it might accidentally leak
 *   onto the webpage, so any function invocation over the storage object or property
 *   access will first validate if the current ecmascript context belongs to it's owner
 *   script. If so, operation completes, else security error is thrown.
 * - Because a webpage might try to trick the user script by doing
 *   "window.Storage.prototype.setItem = badfunction", the user script storage object
 *   will not inherit Storage.prototype, meaning, it will still have the default
 *   Storage API, but that API will be unreachable to anyone but the script itself,
 *   and not prototypable. Doing "window.Object.prototype.setItem = badfunction" will
 *   not work because that property will be shadowed by the objects native properties.
 * - No storage events generated for the script storage areas. Such would require an
 *   unfortunate extra amount of work.
 * - The opera:webstorage webpage will list all user script storage objects, and their
 *   origins (script file's full path)
 */
class DOM_Storage : public DOM_BuiltInConstructor, public Link
{
private:

	/** Saved reference to the document's url, in case the document is detached from the
	    runtime. Used for context when issuing storage operations. */
	URL						m_document_url;

	/** Pointer to the underlying OpStorage object. */
	AutoReleaseOpStoragePtr	m_storage;

	/** Type of the storage area. */
	WebStorageType 			m_type;

	/** If TRUE, a message to the console about file IO errors regarding this storage
	    area has been posted. This way the number of messages is controlled.*/
	BOOL					m_has_posted_err_msg;

#if defined(WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT) || defined(EXTENSION_SUPPORT)
	/**
	 * This origin allows the internal OpStorage object to be initialized to
	 * an origin different from the main document. It'll be used by user scripts
	 * to create multiple storage objects on the same webpage environment.
	 * Extensions also do this to make widget.preferences available to extensionjs
	 * scripts.
	 */
	uni_char* m_override_origin;
	URL_CONTEXT_ID m_override_context_id;
	friend class DOM_UserJSThread;
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT || EXTENSION_SUPPORT

	DOM_Storage(DOM_Runtime::Prototype proto, WebStorageType type);

	/**
	 * Method used to do the initialization of the underlying storage area.
	 * This shall be called before any access to the data, like reading, writing,
	 * removing or enumerating keys and values.
	 *
	 * @param origining_runtime  Runtime from which the request to initialize this object came.
	 * @return OpStatus::OK if the initialization was OK, or if the object is initialized already
	 *         OpStatus::ERR_NULL_POINTER if it was not possible to get a reference to the Window
	 *         which prevents this object from initializing properly.
	 *         OpStatus::ERR_NO_MEMORY if OOM.
	 *         OpStatus::ERR_NO_ACCESS if there was a restriction which prevents this storage
	 *         area from being accessed.
	 */
	OP_STATUS EnsureStorageObj(ES_Runtime *origining_runtime);

	/**
	 * Uninitializes this object, allowing the underlying resources to be cleaned.
	 * Should be called when the document is unloaded. If the document is reactivated,
	 * the object will reinitialize.
	 */
	void DropStorageObj();

public:
	static void FreeStorageResources(Head *);
	/**< Loops linked list of DOM_Storage objects and releases the
	 *   underlying OpStorage object to save memory. If the DOM_Storage
	 *   objects are reused later, the OpStorage object will be refetched.
	 */

	static BOOL CanRuntimeAccessObject(WebStorageType type, ES_Runtime *origining_runtime, DOM_Runtime *object_runtime);
	/**< Verifies if the given runtime can access a DOM_Storage object of the given type
	 *
	 *   @param origining_runtime   runtime of calling thread. Used to check for user script/extension storage later
	 *   @param object_runtime      used to check of document.domain has changed
	 */

	virtual				~DOM_Storage();

	/**
	 * Constructs and returns a new DOM_Storage object.
	 *
	 * @param storage  Output parameter, will receive the newly created storage object.
	 *                 If the call succeeds this is ensured not to be NULL.
	 * @param type     Type of the web storage area. @see WebStorageType
	 * @param runtime  Runtime which will own the object
	 * @return OpStatus::OK if call succeeds
	 *         OpStatus::ERR_NO_MEMORY in case of OOM
	 */
	static OP_STATUS	Make(DOM_Storage *&storage, WebStorageType type, DOM_Runtime *runtime);

#if defined(WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT) || defined(EXTENSION_SUPPORT)
	/**
	 * Sets the origin used by this storage area, if it need to be different from
	 * the one used for the scripting environment, for instance, user js storage objects
	 * which have a dedicated storage area and widget.preferences for opera extensions.
	 * Note: this method must be called only and immediately after DOM_Storage::Make(),
	 * so the underlying storage area can then be initialized using the correct values.
	 * @param origin      Origin of storage area, like widget://<hash>/
	 * @param context_id  Url context id of the origin.
	 */
	OP_STATUS			SetOverrideOrigin(const uni_char *origin, URL_CONTEXT_ID context_id);
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT || EXTENSION_SUPPORT

#ifdef WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT
	/**
	 * Constructs and returns a new DOM_Storage object used for user js storage.
	 * The object will have the type WEB_STORAGE_USERJS.
	 *
	 * @param storage            Output parameter, will receive the newly created storage object.
	 *                           If the call succeeds this is ensured not to be NULL.
	 * @param origin             Origin used for this storage area, like D:\folder\foo.js
	 *                           or /home/foo/bar.js. Should be an absolute path to the script,
	 *                           or anything else that uniquely identifies the user script.
	 * @param runtime            Runtime which will own the object
	 * @return OpStatus::OK if call succeeds
	 *         OpStatus::ERR_NO_MEMORY in case of OOM
	 */
	static OP_STATUS	MakeScriptStorage(DOM_Storage *&storage, const uni_char* origin, DOM_Runtime *runtime);
	virtual BOOL		AllowOperationOnProperty(const uni_char *property_name, PropertyOperation op);
#endif // WEBSTORAGE_USER_SCRIPT_STORAGE_SUPPORT

	virtual ES_GetState GetName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetName(OpAtom property_name, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual ES_GetState GetIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_GetState GetIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual ES_PutState PutName(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutNameRestart(const uni_char* property_name, int property_code, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual ES_PutState PutIndex(int property_index, ES_Value* value, ES_Runtime* origining_runtime);
	virtual ES_PutState PutIndexRestart(int property_index, ES_Value* value, ES_Runtime* origining_runtime, ES_Object* restart_object);

	virtual ES_DeleteStatus DeleteName(const uni_char* property_name, ES_Runtime* origining_runtime);

	virtual ES_GetState FetchPropertiesL(ES_PropertyEnumerator *enumerator, ES_Runtime *origining_runtime);

	virtual BOOL IsA(int type) { return type == DOM_TYPE_STORAGE || DOM_Object::IsA(type); }

	OpStorage *GetOpStorage() { return m_storage.operator->(); }
	const OpStorage *GetOpStorage() const { return m_storage.operator->(); }
	WebStorageType GetStorageType() const { return m_type; }
	BOOL HasPostedErrMsg() const { return m_has_posted_err_msg; }
	void SetHasPostedErrMsg(BOOL v) { m_has_posted_err_msg = v; }

	DOM_DECLARE_FUNCTION(key);
	DOM_DECLARE_FUNCTION(getItem);
	DOM_DECLARE_FUNCTION(setItem);
	DOM_DECLARE_FUNCTION(removeItem);
	DOM_DECLARE_FUNCTION(clear);

	enum { FUNCTIONS_ARRAY_SIZE = 6 };
};

#endif // CLIENTSIDE_STORAGE_SUPPORT
#endif // _MODULES_DOM_SRC_STORAGE_STORAGE_H_
