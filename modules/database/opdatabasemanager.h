/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2011 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.
** It may not be distributed under any circumstances.
*/


#ifndef _MODULES_DATABASE_OPDATABASEMANAGER_H_
#define _MODULES_DATABASE_OPDATABASEMANAGER_H_

#ifdef DATABASE_MODULE_MANAGER_SUPPORT

#ifdef SQLITE_SUPPORT
typedef int SQLiteErrorCode;
#endif //SQLITE_SUPPORT

#include "modules/database/src/opdatabase_base.h"
#include "modules/hardcore/mh/mh.h"
#include "modules/database/sec_policy.h"
#include "modules/util/opfile/opfile.h"

/**
 * Var that holds the special name for the memory only file, used for sqlite
 * but got adopted by everything else
 */
extern const uni_char g_ps_memory_file_name[];

class PS_Object;
class PS_Manager;
class PS_IndexIterator;
class PS_IndexEntry;

/**
 * This class implements an iteration algorithm to transverse the index
 * of objects that the database manager stores
 */
class PS_IndexIterator : public PS_Base, public PS_ObjectTypes
{
public:
	/**
	 * Flags used to indicate iteration order
	 */
	enum IterationOrder
	{
		UNORDERED,
		ORDERED_ASCENDING,//key, origin, name
		ORDERED_DESCENDING//key origin, name
	};

	//continuation of PS_ObjectTypes::ObjectFlags
	enum PS_IndexIteratorFlags
	{
		//used by iterator to know if it references only ps_objects with persistent file on the HD
		PERSISTENT_DB_ITERATOR = 0x100
	};

	/**
	 * constructor
	 *
	 * @param mgr              PS_Manager which this iterator transverses
	 * @param context_id       URL_CONTEXT_ID if applicable
	 * @param use_case_type    type of objects to return. Check
	 *                         PS_ObjectTypes::PS_ObjectType. Use KDBTypeStart
	 *                         to get all objects regardless of type
	 * @param origin           string which represents the origin of the objects to return
	 *                         use NULL for all origins
	 * @param only_persistent  boolean that tells
	 * @param order            ordering. Note: requesting an ordered iterator requires more
	 *                         initialization time  because the iterator needs to create
	 *                         an ordered set in memory
	 */
	PS_IndexIterator(PS_Manager* mgr,
		URL_CONTEXT_ID context_id,
		PS_ObjectType use_case_type = KDBTypeStart,
		const uni_char* origin = NULL,
		BOOL only_persistent = TRUE,
		IterationOrder order = UNORDERED);

	virtual ~PS_IndexIterator();

	/**
	 * Initialization function to be callsed after constructing the object
	 */
	void BuildL();

	/**
	 * Reset iterator object to initial state
	 */
	void ResetL();

	/**
	 * Moves the iterator one item forward
	 *
	 * @return TRUE if there is another item to be used, FALSE otherwise
	 */
	BOOL MoveNextL();

	/**
	 * Tells if iterator has finished transversing
	 *
	 * @return TRUE if iterator has finished transversing, FALSE otherwise
	 */
	BOOL AtEndL() const;

	/**
	 * Returns the item at the current position.
	 * The return value is ensured NOT TO BE NULL unless
	 * AtEndL() returns true
	 *
	 * @return item at current position
	 */
	PS_IndexEntry* GetItemL() const;

	/**
	 * In case the database manager mutates, the iterator object becomes invalid.
	 * This method resynchronizes the iterator with the manager. Note however
	 * that because the index of objects might have changed, reusing this iterator
	 * object might yield less objects than existing, for instance, if a new object
	 * was created in a position that was already skipped by the iterator.
	 * Use with caution and if you know what you're doing
	 */
	void ReSync();

	/**
	 * @return TRUE if the bound database manager's index has mutated after this
	 *         iterator has been created
	 */
	BOOL IsDesynchronized() const;

	/**
	 * Tells if this iterator is sorted or not
	 */
	inline BOOL IsOrdered() const { return m_order_type!=UNORDERED; }

private:

	friend class PS_Manager;

	DISABLE_EVIL_MEMBERS(PS_IndexIterator)

	/**
	 * Returns the item at the current position.
	 * Might return NULL unlike GetItemL
	 */
	PS_IndexEntry* PeekCurrentL() const;

	//filtering fields
	IterationOrder m_order_type; ///< order type
	PS_ObjectType m_psobj_type;  ///< type of objects this iterator should return
	const uni_char* m_origin;    ///< origin of the objects this iterator should return
	URL_CONTEXT_ID m_context_id; ///< URL_CONTEXT_ID if applicable
	unsigned m_last_mod_count;   ///< modification time of the database manager, from
	                                      ///  the last time this object was synchronized

	//iteration fields
	unsigned m_1st_lvl_index; ///< current object type if UNORDERED or index in sorted vector if ORDERED
	unsigned m_2nd_lvl_index; ///< current origin hash (2nd depth level of the db manager index)
	unsigned m_3rd_lvl_index; ///< vector iteration
	OpVector<PS_IndexEntry>*
		m_sorted_index;       ///< cache of all objects sorted. NOTE: pointers
	                          ///  to PS_IndexEntry are owned by the database manager !!

	PS_Manager* m_mgr; //< database manager this iterator refers to
};

/**
 * Class that represents a data file for a persistent storage object.
 * File names are represented by a class because they need both
 * some business logic to calculate available file names,
 * a reference counter to know if a file is still being used so
 * the delete ps_object feature will work properly, and a way to
 * tell if a data file path is unaccessible
 */
class PS_DataFile : public OpRefCounter DB_INTEGRITY_CHK(0x88335522)
{
protected:
	/**
	 * Main constructor
	 *
	 * @param index_entry      database manager index entry to which this file refers to
	 * @param file_abs_path    absolute path to the file. If you do not have this path,
	 *                         then pass NULL. The absolute path will be calculated from
	 *                         the relative one. The file is considered accessible if the
	 *                         absolute path exists
	 * @param file_rel_path    relative path to file, relative to the profile folder root.
	 *                         This path will usually be read from the index file, and later
	 *                         the full absolute path will be calculated using the profile
	 *                         folder as base
	 */
	PS_DataFile(PS_IndexEntry* index_entry = NULL,
			const uni_char* file_abs_path = NULL,
			const uni_char* file_rel_path = NULL);

public:

	/**
	 * This function creates and set a newly created PS_DataFile
	 * instance in the index_entry::m_data_file_name property
	 *
	 * @param index_entry       database manager index entry to which this
	 *                          file refers to
	 * @param file_rel_path     relative path to file, starting at profile
	 *                          folder root. Can be NULL. If so, the object
	 *                          will generate a new unique file name using
	 *                          the naming rule:
	 *                          %profile%/pstorage/%db_type%/%origin_hash%/%hex_number%
	 */
	static OP_STATUS Create(PS_IndexEntry* index_entry, const uni_char* file_rel_path);

	/**
	 * This function generates an absolute file path to this data file,
	 * and also generates a new file name if it wasn't specified earlier.
	 * This needs to be called explicitly before saving data to the file,
	 * so a full file path is available.
	 */
	OP_STATUS MakeAbsFilePath();

	/**
	 * This function creates all the folders necessary to withold the
	 * file that is represented by this object because sqlite does not
	 * create the folders for itself.
	 * Note: the absolute file name must have been built
	 */
	OP_STATUS EnsureDataFileFolder();

	/**
	 * This function must be called when a reference owner no longer needs
	 * the object. The object cannot be deleted directly. This function
	 * decrements the ref count and tries a safe delete. After calling this
	 * function, the object is no more guaranteed to be valid
	 */
	void Release() { DecRefCount();SafeDelete(); }

	/**
	 * This function returns TRUE if this file meanwhile has been
	 * marked as bogus. A file is marked as bogus if it's not accessible.
	 */
	BOOL IsBogus() const { return m_bogus_file; }

	/**
	 * This function returns an error if this file meanwhile has been
	 * marked as bogus. A file is marked as bogus if it's not accessible.
	 */
	OP_STATUS CheckBogus() const { return IsBogus() ? SIGNAL_OP_STATUS_ERROR(OpStatus::ERR_NO_ACCESS) : OpStatus::OK; }

	/**
	 * Tells if this file has been marked for deletion
	 */
	BOOL WillBeDeleted() const { return m_should_delete; }

	/**
	 * Marks file as bogus so it won't be used
	 **/
	void SetBogus(bool v) { m_bogus_file = v; }

	/**
	 * Marks file for deletion from the file system
	 */
	void SetWillBeDeleted(BOOL v = TRUE) { m_should_delete = !!v; }

	PS_IndexEntry* m_index_entry;    ///< db manager's index entry to which file belongs
	const uni_char* m_file_abs_path;           ///< absolute path of the file
	const uni_char* m_file_rel_path;           ///< path of file relative to the profile folder

	OpFile* GetOpFile()
	{
		if (m_file_abs_path != g_ps_memory_file_name && m_file_abs_path != NULL)
			return &m_file_obj;
		return NULL;
	}
	/**
	 * Tells whether the file represented by this object exists in the hard drive
	 */
	OP_STATUS FileExists(BOOL *exists);

#ifdef _DEBUG
	Head m_ref_owners;
# define PS_DataFile_AddOwner(f, o) (o)->m_PS_DataFile_ref.Into(&f->m_ref_owners);
# define PS_DataFile_ReleaseOwner(f, o) (o)->m_PS_DataFile_ref.Out();
# define PS_DataFile_DeclRefOwner Link m_PS_DataFile_ref;
#else
# define PS_DataFile_AddOwner(f, o)
# define PS_DataFile_ReleaseOwner(f, o)
# define PS_DataFile_DeclRefOwner
#endif //_DEBUG

protected:
	/**
	 * Deletes this object and the file on disk if there are no more
	 * reference owners to this object (ref count equals 0)
	 */
	void SafeDelete();

	OpFile m_file_obj;

	bool m_bogus_file:1;                    ///< Flag which tells if the file has been marked as bogus
	bool m_should_delete:1;                 ///< Flag which tells if the data file on disk
											///  should be deleted when the object is terminated

	virtual ~PS_DataFile();

	OP_STATUS InitFileObj();

	/**
	 * This function is called for the object to delete itself.
	 * If object is not allocated directly, or placed on the stack
	 * override with a stub.
	 */
	virtual void DeleteSelf() { OP_DELETE(this); }
};

/**
 * Basic iterator to enumerate all content stored
 * inside a database manager. Use this class to pass
 * to PS_Manager::Enumerate***
 *
 * If any of the callback functions return an error,
 * then that error is propagated to
 * PS_Manager::Enumerate* so this class itself
 * does not need any error handling
 *
 *
 * None of the methods is virtual so you won't be forced
 * to implemented them all always
 */
class PS_MgrContentIterator : protected PS_ObjectTypes
{
protected:
	PS_MgrContentIterator() {}
public:
	virtual ~PS_MgrContentIterator() {}

	/**
	 * Called for context ids enumerated by PS_Manager::EnumerateContextIds
	 * If context ids are disabled, the HandleContextId will
	 * be called once
	 */
	virtual OP_STATUS HandleContextId(URL_CONTEXT_ID context_id) { return OpStatus::OK; }

	/**
	 * Called for object types enumerated by PS_Manager::EnumerateTypes
	 */
	virtual OP_STATUS HandleType(URL_CONTEXT_ID context_id, PS_ObjectType type) { return OpStatus::OK; }

	/**
	 * Called for origins enumerated by PS_Manager::EnumerateOrigins
	 */
	virtual OP_STATUS HandleOrigin(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin) { return OpStatus::OK; }

	/**
	 * Called for objects enumerated by PS_Manager::EnumerateObjects
	 */
	virtual OP_STATUS HandleObject(PS_IndexEntry* object_entry) { return OpStatus::OK; }
};

/**
 * Listener  for change in persistent storage prefs.
 * Needs to be defined so that cached data sizes can be invalidated
 */
class PS_MgrPrefsListener : public OpPrefsListener
	, public PS_MgrContentIterator
{
public:
	PS_Manager* m_mgr;
	OpPrefsCollection* m_current_listenee;
	PS_ObjectType m_last_type;

	PS_MgrPrefsListener(PS_Manager* mgr) :
		m_mgr(mgr), m_current_listenee(NULL) { OP_ASSERT(m_mgr != NULL); }

	virtual ~PS_MgrPrefsListener()
	{
		//m_current_listenee must be cleared by PS_Manager::Destroy,
		//but we cleanup anyway just to be 100% safe
		OP_ASSERT(m_current_listenee == NULL);
		Clear();
	}

	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, int newvalue);

	virtual void PrefChanged(OpPrefsCollection::Collections id, int pref, const OpStringC& newvalue);

#ifdef PREFS_HOSTOVERRIDE
	virtual void HostOverrideChanged(OpPrefsCollection::Collections id, const uni_char * hostname);
#endif

	/**
	 * The following functions are used for enumerating all dbs so cached sizes are invalidated
	 */
	virtual OP_STATUS HandleContextId(URL_CONTEXT_ID context_id);

	virtual OP_STATUS HandleType(URL_CONTEXT_ID context_id, PS_ObjectType type);

	virtual OP_STATUS HandleOrigin(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin);

	/**
	 * Sets itself as listener of the passed collection
	 */
	OP_STATUS Setup(OpPrefsCollection* listenee);

	/**
	 * Clear itself from being registered as listener from the last collection
	 */
	void Clear();
};

/**
 * Main class of the entire database module.
 * This class holds the index of all ps_objects that exist. Each object is identified by:
 *  - url context id if applicable
 *  - type (local storage, user js storage, web pages web db, etc...)
 *  - origin
 *  - an optional name
 *  - persistency flag (to differentiate between privacy mode and normal mode)
 *
 * This class holds APIs to manage these objects.
 * Note: the usage of different context ids will make the data files and
 * database index file to be placed on the profile folder associated with that
 * context id.
 * Note: there might be extra PS_Manager instances for
 * selftests
 */
class PS_Manager : public PS_Base, protected PS_ObjectTypes, private MessageObject
{
	friend class DatabaseModule;

	//make constructor private, so we can control what creates instances
	PS_Manager();

public:

	~PS_Manager() { Destroy(); }

	/**
	 * Cleans up everything inside the object.
	 * That includes object index, all objects and
	 * any pending operations they might have
	 */
	void Destroy();

	/**
	 * Announces a new URL context so the id of its root folder is recorded.
	 * @param context_id      New URL_CONTEXT_ID id
	 * @param root_folder_id  ID of root folder of the context where all the profile data is placed.
	 *                        Shall be used to store the data within. If OPFILE_ABSOLUTE_FOLDER is
	 *                        used, then this context id shall be ignored for now.
	 * @return OpStatus::OK if registering the new context went ok, or if it already exists
	 *         OpStatus::ERR_NO_MEMORY if OOM.
	 */
	OP_STATUS AddContext(URL_CONTEXT_ID context_id, OpFileFolder root_folder_id, bool extension_context);

	/** TRUE if context was registered, FALSE otherwise. */
	bool IsContextRegistered(URL_CONTEXT_ID context_id) const;

#ifdef EXTENSION_SUPPORT
	/** TRUE if context was registered and belongs to an extension, FALSE otherwise. */
	bool IsContextExtension(URL_CONTEXT_ID context_id) const;
#endif // EXTENSION_SUPPORT

	/**
	 * Returns the root folder id for the given context.
	 * @param context_id  Context ID from which to knwo the root folder
	 * @param [out]root_folder_id Variable where the folder id will be stored.
	 * @return TRUE if the given context id was found and the value read, FALSE otherwise.
	 */
	BOOL GetContextRootFolder(URL_CONTEXT_ID context_id, OpFileFolder* root_folder_id);

	/**
	 * Removes the context id from the PS manager so it is no longer usable.
	 */
	void RemoveContext(URL_CONTEXT_ID context_id);

	/**
	 *
	 * This method returns a new object ready to be used.
	 * If the object does not exist, it is created.
	 * If there is no previous information on the object index
	 * for this object, a new entry is created and the index is
	 * flushed to disk.
	 * Note: PS_Objects are shared among all the callees, therefore
	 * references are counted. After receiving a object by calling
	 * this function, the reference owner must dispose of the
	 * object using PS_Object::Release(). Check PS_Object
	 * documentation for more details
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 * @param object        pointer to PS_Object* where the new
	 *                        object will be placed.
	 * @return status of call. If this function returns OK, then
	 *         the value on object is ensured not to be null,
	 *         else it'll be null. If access to this object is forbidden
	 *         then this will return OpStatus::ERR_NO_ACCESS.
	 */
	OP_STATUS GetObject(PS_ObjectType type, const uni_char* origin,
			const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id, PS_Object** object);

	/**
	 * This function tags a specific object for deletion. The
	 * object is not deleted immediately though because there might
	 * be other reference owners and open transactions. When all
	 * transactions close, the object will delete itself, unless
	 * a new call to GetObject is done, which will unmark the deletion
	 * flag. By deleting the object, its entry on the index file will
	 * be removed. When calling this function the data file is immediately
	 * removed, meaning that new transactions will use a new empty data
	 * faile. previously open transactions will continue to use the
	 * old data file until they close. When those transactions close
	 * the old data file will be deleted
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 *
	 * @return OpStatus::OK if entry found, OpStatus::ERR_OUT_OF_RANGE if entry not found
	 *         or other errors if processing fails, like OOM when loading the index in memory
	 */
	OP_STATUS DeleteObject(PS_ObjectType type, const uni_char* origin,
			const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id);

	/**
	 * This function tags all objects of the given type for the given
	 * origin for deletion. The behavior for deletion is the same as
	 * the one from DeleteObject.
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param context_id      url context id if applicable
	 * @param origin          origin for the object, per-html5, optional
	 */
	OP_STATUS DeleteObjects(PS_ObjectType type, URL_CONTEXT_ID context_id, const uni_char *origin, BOOL only_persistent = TRUE);
	OP_STATUS DeleteObjects(PS_ObjectType type, URL_CONTEXT_ID context_id, BOOL only_persistent = TRUE);

	/**
	 * This function loops through all objects of a given type, context id, and origin
	 * and calculates the accumulated size on the harddrive being used by the data files.
	 * The origin argument is optional, and in that case, it'll be calculated globally
	 * for the type and context id, and that value will be cached. This is used to
	 * calculate available global quotas.
	 * If an origin has been given unlimited quota, it will not be accounted into the final
	 * value because a object with a size over the global quota would prevent all the
	 * other objects from storing data, while this object would still be able to do so.
	 *
	 * @param data_size       output argument where the result will be placed
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param context_id      url context id if applicable
	 * @param origin          origin for the object, per-html5, optional
	 */
	OP_STATUS GetGlobalDataSize(OpFileLength* data_size, PS_ObjectType type,
			URL_CONTEXT_ID context_id, const uni_char* origin = NULL);
	OpFileLength GetGlobalDataSizeL(PS_ObjectType type, URL_CONTEXT_ID context_id,
			const uni_char* origin = NULL);

	/**
	 * Because the global data sizes returned by GetGlobalDataSize are cached
	 * this function allows then to be uncached, so when a object transaction
	 * changes data it can flag the manager that it's data size has changed
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param context_id      url context id if applicable
	 */
	void InvalidateCachedDataSize(PS_ObjectType type, URL_CONTEXT_ID context_id, const uni_char* origin = NULL);

	/**
	 * This function fills in the object index for the given context id
	 * with the information supplied in the file passed.
	 *
	 * @param file        preferences file from which information will be read.
	 *                    This file is supplied because there might not be any
	 *                    information at all regarding this context id
	 */
	void ReadIndexFromFileL(const OpFile& file, URL_CONTEXT_ID context_id);

	/**
	 * This function flushes the index to file. Unlike ReadIndexFromFileL
	 * this function does not accept a file argument because if there is
	 * no information on the index for this context id, it'll be ignored, else
	 * if there is information, the file path will be there already
	 */
	void FlushIndexToFileL(URL_CONTEXT_ID context_id);
	OP_STATUS FlushIndexToFile(URL_CONTEXT_ID context_id);

	/**
	 * This function posts a message to do an asynchronous call to
	 * FlushIndexToFileL
	 */
	OP_STATUS FlushIndexToFileAsync(URL_CONTEXT_ID context_id);

	/**
	 * Returns an iterator object to iterate easily over the index of objects
	 * that this manager holds.
	 * Refer to PS_IndexIterator for details on the iterator class
	 *
	 * @param context_id      url context id if applicable
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType. Pass
	 *                        KDBTypeStart for all
	 * @param origin          origin for the object, per-html5, optional.
	 *                        Pass NULL for all
	 * @param only_persistent boolean which tells if only objects with persistent
	 *                        storage should be returned. If FALSE, memory-only
	 *                        objects will also be returned
	 * @param order           iteration order. Note: choosing ordered iteration
	 *                        forces the creation of an ordered set
	 */
	PS_IndexIterator* GetIteratorL(URL_CONTEXT_ID context_id, PS_ObjectType type,
			const uni_char* origin, BOOL only_persistent = TRUE,
			PS_IndexIterator::IterationOrder order = PS_IndexIterator::UNORDERED);

	/**
	 * Returns an iterator object to iterate easily over the index of objects
	 * that this manager holds.
	 * Refer to PS_IndexIterator for details on the iterator class
	 *
	 * @param context_id      url context id if applicable
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param only_persistent boolean which tells if only objects with persistent
	 *                        storage should be returned. If FALSE, memory-only
	 *                        objects will also be returned
	 * @param order           iteration order. Note: choosing ordered iteration
	 *                        forces the creation of an ordered set
	 */
	PS_IndexIterator* GetIteratorL(URL_CONTEXT_ID context_id, PS_ObjectType type = KDBTypeStart,
			BOOL only_persistent = TRUE, PS_IndexIterator::IterationOrder order = PS_IndexIterator::UNORDERED)
	{ return GetIteratorL(context_id, type, NULL, only_persistent, order); }

	/**
	 * This function loops through all stored context ids and returns them to this enumerator class
	 * if context ids are disabled, then HandleContextid is called once
	 */
	OP_STATUS EnumerateContextIds(PS_MgrContentIterator* enumerator);

	/**
	 * This function loops through all the stored types of objects for the given context id.
	 * If there aren't any, then HandleType is not called
	 */
	OP_STATUS EnumerateTypes(PS_MgrContentIterator* enumerator, URL_CONTEXT_ID context_id);

	/**
	 * This function loops through all the stored origins of objects for the given context id and type
	 * If there aren't any, then HandleOrigin is not called
	 */
	OP_STATUS EnumerateOrigins(PS_MgrContentIterator* enumerator, URL_CONTEXT_ID context_id, PS_ObjectType type);

	/**
	 * This function loops through all the stored objects for the given context id, type and origin
	 * If there aren't any, then HandleObject is not called
	 */
	OP_STATUS EnumerateObjects(PS_MgrContentIterator* enumerator, URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin);

	/**
	 * Checks if a given object exists and return its index entry instance, or NULL if it does not exist
	 * Note: this function does not create new entries and does not open the object. Use GetObject
	 * for that use case
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 *
	 * @return NULL if the object does not exist, of the index entry that represents this object
	 */
	PS_IndexEntry* CheckObjectExists(PS_ObjectType type, const uni_char* origin,
			const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id) const;

	/**
	 * Returns number of objects for the given url context id, type and origin
	 *
	 * @param context_id      url context id if applicable
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 */
	unsigned GetNumberOfObjects(URL_CONTEXT_ID context_id, PS_ObjectType type, const uni_char* origin) const;

	/**
	 * @ return the number of modifications this object manager has suffered since it's creation.
	 * Used to keep iterators in sync
	 */
	unsigned GetModCount() const { return m_modification_counter; }

	/**
	 * @return the global message handler. spares one global access
	 */
	MessageHandler* GetMessageHandler() const { return OpDbUtils::GetMessageHandler(); };

	/**
	 * @return the MH_PARAM_1 of all messages this manager should handle
	 */
	MH_PARAM_1 GetMessageQueueId() const { return reinterpret_cast<MH_PARAM_1>(this); }

	/**
	 * Tells if Opera's message queue is running, If not then core is shutting down
	 */
	static BOOL IsOperaRunning() { return OpDbUtils::IsOperaRunning(); }

	/**
	 * @return the global TempBuffer used on this module
	 */
	TempBuffer& GetTempBuffer(BOOL clear = TRUE);

#ifdef DATABASE_MODULE_DEBUG
	/**
	 * Debug function. Prints the entire index to the standard output
	 */
	void DumpIndexL(URL_CONTEXT_ID context_id, const uni_char* msg = NULL);
#endif //DATABASE_MODULE_DEBUG

#ifdef SELFTEST

	/**
	 * Selftests need to create manager instances, but that instance
	 * should not access the index file on disk nor should it post messages
	 */
	static PS_Manager *NewForSelfTest();
	BOOL m_self_test_instance;

#endif

	/**
	 * Returns a human readable string that represents the passed object type.
	 * This string is used in the index file to prevent hardcoded numerical ids.
	 * Note: this string is not translated.
	 *
	 * @param type           type of object to get description
	 * @return the description
	 */
	const uni_char* GetTypeDesc(PS_ObjectType type) const;

	/**
	 * Converts a object type description as returned from GetUseCaseDesc()
	 * into the original type
	 * @param desc             object type description
	 * @return the object type
	 */
	PS_ObjectType GetDescType(const uni_char* desc) const;

private:
	/**
	 * Initializes this manager instance by registering the appropriate
	 * message and preference listeners, and doing a couple checks. This
	 * function should only be called on demand when the APIs of the
	 * manager are accessed to prevent loading and processing of data
	 * that might not be used later.
	 */
	OP_STATUS EnsureInitialization();

	/**
	 * Creates a new index entry on the index for the given object
	 * parameters. Note: the entry must not exist.
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param data_file_path  path to the data file as read from the index file
	 *                        can be relative or absolute
	 * @param db_version      db version for the openDatabase API
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 * @param created_entry   output argument, optional, for the newly created
	 *                        entry on the index
	 */
	OP_STATUS StoreObject(PS_ObjectType type, const uni_char* origin,
			const uni_char* name, const uni_char* data_file_path, const uni_char* db_version,
			BOOL is_persistent, URL_CONTEXT_ID context_id, PS_IndexEntry** created_entry);

	/**
	 * Internal function that deletes the index entry and the object immediately.
	 * Called when the object can be deleted
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 *
	 * @return  TRUE if it found something, FALSE otherwise
	 */
	BOOL DeleteEntryNow(PS_ObjectType type,
			const uni_char* origin,
			const uni_char* name,
			BOOL is_persistent,
			URL_CONTEXT_ID context_id);

	/**
	 * Checks if the origin is valid for a given type.
	 * If a NULL origin is passed, then the object
	 * will be treated as non-persistent
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5. If this
	 *                        value is NULL then it'll be replaced with opera:blank
	 * @param is_persistent   if the origin is NULL or empty, then is_persistent
	 *                        is overriden with false
	 */
	static OP_STATUS CheckOrigin(PS_ObjectType type, const uni_char* origin, BOOL is_persistent);

	/**
	 * Class that represents the memory-only datafile for memory
	 * only objects. Although the "file" is represented by a single
	 * class, sqlite will create many separate memory regions for
	 * each memory-only object
	 */
	class MemoryPS_DataFile: public PS_DataFile
	{
	public:
		MemoryPS_DataFile() : PS_DataFile(NULL,
				g_ps_memory_file_name,
				g_ps_memory_file_name) {}
		virtual ~MemoryPS_DataFile() { OP_ASSERT(!m_bogus_file); }
		virtual void DeleteSelf() {}
	};

	/**
	 * The single instance of the memory-only data file
	 */
	MemoryPS_DataFile m_non_persistent_file_name_obj;
public:

	/**
	 * @return the single instance of the memory-only data file
	 */
	MemoryPS_DataFile* GetNonPersistentFileName() { return &m_non_persistent_file_name_obj; }

	/**
	 * Constant: number of object types, used for array allocation
	 */
	enum { DATABASE_INDEX_LENGTH = KDBTypeEnd };

private:

	/**
	 * File names are generated using sequential numbers. This function
	 * parses a file name from the index file to store the number used
	 * for this file name, so the newly generated file names can be
	 * generated from the biggest number onwards. It's just an
	 * optimization because the filename generation code skips collisions.
	 *
	 * @param context_id      url context id
	 * @param filename        filename
	 */
	void ParseFileNameNumber(URL_CONTEXT_ID context_id, const uni_char* filename);

public:
	/**
	 * The object index is has several levels identified by:
	 * 1) url context id
	 * 2) object type
	 * 3) simple bitwise origin hash
	 * 4) vector with all objects
	 * This class represents the data on level 4, which is a vector of
	 * objects and the maximum number used for file names on this
	 * part of the index, because the the structure on the file system
	 * will resemble this one.
	 * This class also has cached information for a given origin
	 */
	class IndexEntryByOriginHash
	{
	public:
		IndexEntryByOriginHash() : m_highest_filename_number(0){}
		~IndexEntryByOriginHash();

		/**
		 * The cached info stored within
		 */
		struct OriginCachedInfo{
			unsigned m_number_of_dbs;
			OpFileLength m_cached_data_size;
			uni_char m_origin[1]; /*ARRAY OK joaoe 2009-07-27*/
		};
		/**
		 * Derivation of OpStringHashTable so Delete does free and not delete
		 */
		class OriginInfoTable : public OpStringHashTable<OriginCachedInfo>
		{
		public:
			OriginInfoTable() {}
			~OriginInfoTable() {}
			virtual void Delete(void* p) { OP_DELETEA((byte*)p); }
		};

		/**
		 * The following functions control the cached number of dbs
		 */
		unsigned GetNumberOfDbs(const uni_char* origin) const;

		OP_STATUS SetNumberOfDbs(const uni_char* origin, unsigned number_of_dbs);

		OP_STATUS IncNumberOfDbs(const uni_char* origin)
		{ return SetNumberOfDbs(origin, GetNumberOfDbs(origin)+1); }

		void DecNumberOfDbs(const uni_char* origin);

		/**
		 * The following function control the cached data size
		 */
		OP_STATUS SetCachedDataSize(const uni_char* origin, OpFileLength new_size);

		void UnsetCachedDataSize(const uni_char* origin);

		BOOL HasCachedDataSize(const uni_char* origin) const;

		OpFileLength GetCachedDataSize(const uni_char* origin) const;

		/**
		 * return entry of cached info if any, else NULL
		 */
		OriginCachedInfo* GetCachedEntry(const uni_char* origin) const;

		/**
		 * Makes a new entry in the hashmap of cached info
		 */
		OP_STATUS MakeCachedEntry(const uni_char* origin, OriginCachedInfo** ci);

		/**
		 * This vector contains all the objects
		 */
		OpVector<PS_IndexEntry> m_objects;
		/**
		 * This hash table contains cached information for a given origin
		 */
		OriginInfoTable m_origin_cached_info;

		/**
		 * Highest number used to generate a new data file name for the
		 * folder represented by this object.
		 */
		unsigned m_highest_filename_number;

		/*
		 * friend because of ADS
		 */
		friend class IndexByContext;
	};

	/**
	 * The object index is has several levels identified by:
	 * 1) url context id
	 * 2) object type
	 * 3) simple bitwise origin hash
	 * 4) vector with all objects
	 * This class represents the data on level 2.
	 * This class therefore contains an array of fixed size, being that size
	 * the number of different object types.
	 * Because this class stores the first level of data after the url
	 * context id, it also stores context information about the index file read
	 * and a global caching of data file sizes for global quota calculation
	 */
	class IndexByContext
	{
	public:
		IndexByContext(PS_Manager* mgr, URL_CONTEXT_ID context_id);
		~IndexByContext();
		//cached size values

		/**
		 * PS_Manager to which this data belongs
		 */
		PS_Manager* m_mgr;
		/**
		 * Array with index of objects, per-type
		 */
		IndexEntryByOriginHash** m_object_index[DATABASE_INDEX_LENGTH];
		/**
		 * Array with index of global cached data sizes, per-type
		 */
		OpFileLength m_cached_data_size[DATABASE_INDEX_LENGTH];
		/**
		 * url context id if applicable
		 */
		URL_CONTEXT_ID m_context_id;
		/**
		 * Folder id of the folder that contains this context's data.
		 */
		OpFileFolder m_context_folder_id;

		/**
		 * Tells if this part of the index is being destroyed (dtor called)
		 */
		BOOL IsIndexBeingDeleted() const { return m_bools.being_deleted; }

		/**
		 * Sets the being_delete flag which tells this part of the index of
		 * objects is being cleaned up.
		 * */
		void SetIndexBeingDeleted() { m_bools.being_deleted = true; }

#ifdef EXTENSION_SUPPORT
		/**
		 * Tells that this context id refers to an extension.
		 */
		bool IsExtensionContext() const { return m_bools.is_extension_context; }
#endif // EXTENSION_SUPPORT

		/**
		 * Get the IndexEntryByOriginHash instance for the given origin and
		 * context id, if any, else it returns NULL
		 *
		 * @param type            type of object. For possible values check
		 *                        PS_ObjectTypes::PS_ObjectType
		 * @param origin          origin for the object, per-html5, optional
		 */
		IndexEntryByOriginHash* GetIndexEntryByOriginHash(PS_ObjectTypes::PS_ObjectType type, const uni_char* origin) const;

		/**
		 * Sets flag that tells that this part of the index is being flushed to the index file
		 * Used to prevent reentrancy
		 */
		void SetIsFlushing(BOOL v) { m_bools.is_flushing_to_disk = !!v; }
		BOOL GetIsFlushing() const { return m_bools.is_flushing_to_disk; }

		struct UnsetFlushHelper {
			UnsetFlushHelper(IndexByContext* v) : p(v){}
			~UnsetFlushHelper() { if(p) p->SetIsFlushing(FALSE);}
			IndexByContext* p;
		};
		/**
		 * Tells if a message to flush this part of the index to disk has been posted
		 */
		BOOL HasPostedFlushMessage() const { return m_bools.has_posted_flush_msg; }
		/**
		 * Sets flag telling that a message to flush the index to disk has been posted
		 */
		void SetPostedFlushMessage() { m_bools.has_posted_flush_msg = true; }
		/**
		 * Clears the flush to disk flag
		 */
		void UnsetPostedFlushMessage() { m_bools.has_posted_flush_msg = false; }

		/**
		 * Tells if the this context was properly registered using AddContext()
		 * If so, it can be used for enumeration and storing data on disk.
		 */
		bool WasRegistered() const { return m_bools.was_registered; }

		/**
		 * Tells if the index file for this context was read.
		 */
		bool HasReadFile() const { return m_bools.file_was_read; }

		/**
		 * Initializes index file specific data, like calculating its location
		 * and reading its data into memory
		 */
		OP_STATUS InitializeFile();

		/**
		 * Returns the OpFile class that represents the index file used
		 * by this part of the object index
		 */
		const OpFile& GetFileFd() const { return m_index_file_fd; }

	private:

		OpFile m_index_file_fd;        ///< index file

	private:
		friend class PS_Manager;
		DISABLE_EVIL_MEMBERS(IndexByContext)
		union {
			struct {
				bool file_was_read:1;        ///< bool which tells if index file was read
				bool has_posted_flush_msg:1; ///< bool which tells if the flush to disk message was posted
				bool is_flushing_to_disk:1;  ///< if the index is currently being flushed to disk
				bool being_deleted:1;        ///< if this index is being deleted
				bool was_registered:1;       ///< if true, this was registered using AddContext
#ifdef EXTENSION_SUPPORT
				bool is_extension_context:1;   ///< If true, this context belongs to an extension.
#endif // EXTENSION_SUPPORT
			} m_bools;
			unsigned m_bools_init;
		};
	};

private:
	/**
	 * The object index is has several levels identified by:
	 * 1) url context id
	 * 2) object type
	 * 3) simple bitwise origin hash
	 * 4) vector with all objects
	 * This class represents the data on level 1.
	 * If url context ids are disabled, then this will hold the
	 * single instance of the class that holds the data on two
	 **/
	// TODO: make this into a data structure that has a fast path for context 0
	OpPointerHashTable<void, IndexByContext> m_object_index;

	/**
	 * Returns the IndexByContext class instance for the given
	 * url context id if any, or NULL instead
	 */
	IndexByContext* GetIndex(URL_CONTEXT_ID context_id) const
	{
		OP_ASSERT(context_id != DB_NULL_CONTEXT_ID);
		IndexByContext* index;
		if (OpStatus::IsSuccess(m_object_index.GetData(INT_TO_PTR(context_id), &index)))
			return index;
		return NULL;
	}

	/**
	 * Tells if the entire index of objects is being destroyed
	 */
	BOOL IsIndexBeingDeleted(URL_CONTEXT_ID context_id) const
	{
		IndexByContext* i = GetIndex(context_id);
		return i != NULL && i->IsIndexBeingDeleted();
	}

	/**
	 * Generates a new entry of IndexByContext for the given url
	 * context id if it does not exist already
	 *
	 * @param load_file       bool which tells if the index file should be read or not
	 * @param context_id      url context id if applicable
	 */
	OP_STATUS MakeIndex(BOOL load_file, URL_CONTEXT_ID context_id);

	/**
	 * Get the IndexEntryByOriginHash instance for the given origin and
	 * context id, if any, else it returns NULL
	 *
	 * @param context_id      url context id if applicable
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 */
	IndexEntryByOriginHash* GetIndexEntryByOriginHash(URL_CONTEXT_ID context_id, PS_ObjectTypes::PS_ObjectType type, const uni_char* origin) const;

	/**
	 * Deletes the entire index of objects.
	 *
	 * If the context_id is not DB_NULL_CONTEXT_ID, then only that part of the index is dropped.
	 */
	void DropEntireIndex(URL_CONTEXT_ID context_id = DB_NULL_CONTEXT_ID);

	/**
	 * Holds modification counter of this manager
	 */
	unsigned m_modification_counter;

	/**
	 * Holds mod count when the index was last saved to disk.
	 * This way we can prevent useless flushes
	 */
	unsigned m_last_save_mod_counter;

	/**
	 * Tells if the index was modified since the last time it was
	 * saved to disk
	 */
	BOOL IsModifiedFromDisk() const { return m_modification_counter != m_last_save_mod_counter; }

public:
	/**
	 * Vars used for the simple bitwise origin hash. The hashing function
	 * generates a 5bit number which will be used both for indexing
	 * and folder naming on the hard drive.
	 */
	enum {
		m_max_hash_chars = 16,
		m_hash_mask = 0x1f,  //5 bits -> 32 positions
		m_max_hash_amount = 0x20
	};

	/**
	 * Function calculates the simple bitwise origin hash by xoring
	 * the first 'm_max_hash_chars' chars of the string, or until 0 is found
	 */
	static unsigned HashOrigin(const uni_char* origin);
private:

#ifndef HAS_COMPLEX_GLOBALS

	/**
	 * This array holds the mapping from the numerical object type
	 * to string.
	 */
	const uni_char* database_module_mgr_psobj_types[DATABASE_INDEX_LENGTH + 1];

public:
	inline const uni_char* const* GetTypeDescTable() const { return database_module_mgr_psobj_types; }
private:
#endif

	friend class PS_IndexIterator;
	friend class PS_IndexEntry;
	friend class PS_DataFile;
	friend class PersistentStorageCommander;
	friend class IndexByContext;

	// While the manager is busy inside one of the many
	// EnumerateSomething calls this is incremented. If
	// this is non-zero this happens, the index cannot
	// be modified.
	unsigned m_inside_enumerate_count;

	/**
	 * Our friendly global TempBuffer for the database module
	 */
	TempBuffer m_global_buffer;

	/**
	 * Prefs listener to invalidate cached data sizes, when the qutoa exceeded handling changes
	 */
	PS_MgrPrefsListener m_prefs_listener;

	/**
	 * Callback function for the manager. So far it just flushes the index file inside
	 */
	virtual void HandleCallback(OpMessage msg, MH_PARAM_1 par1, MH_PARAM_2 par2);

	/**
	 * Posts a MSG_DB_MODULE_DELAYED_DELETE_DATA message to this same object
	 * so data can be cleared later
	 */
	void PostDeleteDataMessage(URL_CONTEXT_ID context_id, unsigned type_flags, const uni_char* origin, bool is_extension = false);

	/**
	 * Helper struct to store temporary information about delete requests
	 * posted by PersistentStorageCommander().
	 */
	struct DeleteDataInfo : public ListElement<DeleteDataInfo> {
		URL_CONTEXT_ID m_context_id;
		unsigned m_type_flags;
#ifdef EXTENSION_SUPPORT
		bool m_is_extension;
#endif // EXTENSION_SUPPORT
		uni_char m_origin[1]; /* ARRAY OK 2012-03-27 joaoe */
		static DeleteDataInfo* Create(URL_CONTEXT_ID context_id, unsigned type_flags, bool is_extension, const uni_char* origin = NULL);
	};
	List<DeleteDataInfo> m_to_delete;

#ifdef SELFTEST
public:
#endif // SELFTEST
	/**
	 * Flushes and clears the queue of clear private data requests posted by the
	 * PersistentStorageCommander().
	 *
	 * @param apply If TRUE, the delete requests are done,
	 *              If FALSE the queue is cleared without processing any delete request.
	 */
	OP_STATUS FlushDeleteQueue(BOOL apply);

	/**
	 * Loops the contents of the storage folder for the given context id
	 * and clears files which are not referenced by the storage index.
	 */
	OP_STATUS DeleteOrphanFiles(URL_CONTEXT_ID context_id);
public:
	/**
	 * Tells if this object has been destroyed/cleaned up
	 */
	inline BOOL IsBeingDestroyed() const { return GetFlag(BEING_DELETED); }

#ifdef SQLITE_SUPPORT
	/*
	 * The header file at "modules/sqlite/sqlite3.h" defines the error codes, up to value 101
	 * This declares an array of int instead of OP_STATUS because if USE_DEBUGGING_OP_STATUS is
	 * defined then OP_STATUS will be a class.
	 */
	static OP_STATUS SqliteErrorCodeToOpStatus(SQLiteErrorCode err_code);
#endif //SQLITE_SUPPORT
};

/**
 * This class represents a object in the index of databases
 * held by the database manager. The information encapsulated is read
 * and written to the object index file.
 * This class does not implement the Database API itself, just holds metadata
 * and is kept always live while the object is not explicitly deleted.
 * Instances of this class will always be owned by the database manager
 */
class PS_IndexEntry
	: public PS_Base
	, public OpRefCounter
	, protected PS_ObjectTypes
{
public:

	~PS_IndexEntry() { OP_ASSERT(!GetFlag(BEING_DELETED)); SetFlag(BEING_DELETED); Destroy(); }

	//continuation of PS_ObjectTypes::ObjectFlags
	enum PS_IndexEntryFlags
	{
		//opdatabasekeyidentifier - memory only db
		MEMORY_ONLY_DB = 0x100,
		//if the size of the datafile is cached
		HAS_CACHED_DATA_SIZE = 0x200,
		//if this object has been marked for deletion
		MARKED_FOR_DELETION = 0x400,
		//this entry is going to be deleted anytime soon
		PURGE_ENTRY = 0x800
	};

	/**
	 * Flags this index entry for deletion. If the object is
	 * not owned by anyone (ref count at 0), and if the entry
	 * does not represent any data on disk at the time this is
	 * called, the entry is deleted synchronously.
	 * Note, if a PS_Manager::GetObject which returns
	 * this object is done after this entry has been flagged
	 * for deletion, the deletion flag is cleared.
	 *
	 * @return TRUE if it was immediately deleted, so clear pointers,
	 *         or FALSE if it was postponed, because there might be
	 *         still open objects
	 **/
	BOOL Delete();

	/**
	 * Flags the data file to be deleted as soon as possible, but
	 * does not delete the entry itself. The object transactions
	 * that still keep references to the current file, will continue
	 * to do so until they are closed. Meanwhile, this method clears
	 * the reference to the current file so the next time a new
	 * transaction is open it'll access a new fresh file
	 *
	 * @return TRUE if it actually deleted something, of FALSE if nothing happened
	 */
	BOOL DeleteDataFile();

	/**
	 * Tells if this object has been marked for deletion
	 */
	BOOL WillBeDeleted() const { return GetFlag(MARKED_FOR_DELETION); }

	/**
	 * Returns the size occupied by the data file of this object
	 *
	 * @param file_size          output arg, pointer were file size will be placed
	 */
	OP_STATUS GetDataFileSize(OpFileLength* file_size);

	/**
	 * Compares this object with the argument and returns true if they represent the
	 * same object
	 *
	 * @param rhs      object to compare to
	 * @return true if objects represent the same object
	 */
	inline BOOL IsEqual(const PS_IndexEntry* rhs) const { return rhs != NULL && IsEqual(*rhs); }
	BOOL IsEqual(const PS_IndexEntry& rhs) const;
	BOOL IsEqual(PS_ObjectType type, const uni_char* origin,
			const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id) const;

	/**
	 * Sets a resources policy to be used by this object
	 * See PS_Policy for details
	 */
	void SetPolicy(PS_Policy* policy) { m_res_policy = policy; }
	/**
	 * Returns the policy being used by this object.
	 * Although unlikely, a NULL pointer can be guessed asOOM but should be
	 * handled gracefully
	 */
	PS_Policy* GetPolicy() const;

	/**
	 * The following functions return many of the attributes that identify this object
	 */
	PS_ObjectType GetType() const { return m_type; }
	const uni_char* GetTypeDesc() const { return m_manager->GetTypeDesc(m_type); }
	const uni_char* GetDomain() const { return m_domain; }
	const uni_char* GetOrigin() const { return m_origin; }
	const uni_char* GetName() const { return m_name; }
	const uni_char* GetVersion() const { return m_db_version; }
	BOOL IsMemoryOnly() const { return GetFlag(MEMORY_ONLY_DB); }
	BOOL IsPersistent() const { return !GetFlag(MEMORY_ONLY_DB); }
	URL_CONTEXT_ID GetUrlContextId() const { return m_context_id; }

	OpFile* GetOpFile() const { return m_data_file_name ? m_data_file_name->GetOpFile() : NULL; }
	OP_STATUS FileExists(BOOL *exists) const { if (m_data_file_name) return m_data_file_name->FileExists(exists); *exists = FALSE; return OpStatus::OK; }

	/**
	 * Compares the given version to string to the one on this object.
	 * NULL is handled as well.
	 *
	 * @return TRUE if equal, FALSE if different
	 */
	BOOL CompareVersion(const uni_char* other) const;

	/**
	 * Returns the PS_Object object associated with this index entry.
	 * Note: does not create the object if it does nto exist. For that
	 * use PS_Manager::GetObject
	 */
	inline PS_Object* GetObject()const { return m_ps_obj; }

	/**
	 * Generates a file name for this object IF it does not have one, and calculates
	 * the absolute path to the data file
	 */
	OP_STATUS MakeAbsFilePath();

	/**
	 * Tells if this object has an associated data file
	 */
	inline BOOL HasDataFile() const { return m_data_file_name != NULL; }

	/**
	 * Returns the PS_DataFile instance associated with this object
	 */
	inline PS_DataFile* GetFileNameObj() const { return m_data_file_name; }

	/**
	 * This method should be called immediately before sqlite accessing the
	 * data file, so it can be ensured that the structure of folders exists in
	 * the file system, because sqlite does not create folders
	 * that are missing recursively
	 */
	OP_STATUS EnsureDataFileFolder() const { return m_data_file_name != NULL ? m_data_file_name->EnsureDataFileFolder() : OpStatus::OK; }

	/**
	 * Returns this object's absolute data file path.
	 * Return value might be NULL. If the callee needs to
	 * have an absolute path forcefully, then it should call
	 * MakeAbsFilePath before.
	 */
	const uni_char* GetFileAbsPath() const { return m_data_file_name != NULL ? m_data_file_name->m_file_abs_path : NULL; }

	/**
	 * Returns this object's relative data file path.
	 * If this entry was read from the index file, then there
	 * must be a relative path. Return value might be NULL
	 * if there is no data file yet. It's created lazily.
	 * If the callee needs to have an relative path forcefully,
	 * then it should call MakeAbsFilePath before, which will
	 * generate a new file path if it does not exist
	 */
	const uni_char* GetFileRelPath() const { return m_data_file_name != NULL ? m_data_file_name->m_file_rel_path : NULL; }

	/**
	 * Returns the PS_Manager that owns this object.
	 */
	PS_Manager* GetPSManager() const { return m_manager; }

	/**
	 * Returns PS_Manager::IndexByContext instance of the
	 * database manager that holds this object entry
	 */
	PS_Manager::IndexByContext* GetIndex() const { return m_manager->GetIndex(GetUrlContextId()); }

#ifdef EXTENSION_SUPPORT
	/**
	 * Tells if this object belongs to an extension.
	 */
	BOOL IsExtensionContext() const { return GetIndex()->IsExtensionContext(); }
#endif // EXTENSION_SUPPORT

	/**
	 * This function tell if this object has data that should be written to the index file
	 */
	BOOL HasWritableData() const { return !IsMemoryOnly() && GetFileRelPath() != NULL; }

	/**
	 * Sets new value for the object version
	 */
	OP_STATUS SetVersion(const uni_char* db_version)
	{
		GetPSManager()->FlushIndexToFileAsync(m_context_id);
		return OpDbUtils::DuplicateString(db_version, &m_db_version);
	}

	/**
	 * If this object has meanwhile been tagged for deletion,
	 * this function clears the flags to prevent the deletion
	 */
	void UnmarkDeletion() { UnsetFlag(MARKED_FOR_DELETION | PURGE_ENTRY); }

	/**
	 * Comparison functions to be used by OpVector<*>::QSort
	 */
	static int CompareAsc(const PS_IndexEntry** lhs, const PS_IndexEntry** rhs)
	{ return Compare(*lhs,*rhs,1); }
	static int CompareDesc(const PS_IndexEntry** lhs, const PS_IndexEntry** rhs)
	{ return Compare(*lhs,*rhs,-1); }

	/**
	 * Shorthand to access policy attributes
	 */
	OpFileLength GetPolicyAttribute(PS_Policy::SecAttrUint64 attr, const Window* window = NULL) const
	{ return GetPolicy() != NULL ? GetPolicy()->GetAttribute(attr, GetUrlContextId(), GetDomain(), window) : PS_Policy::UINT64_ATTR_INVALID; }


	/**
	 * Shorthand to access policy attributes
	 */
	unsigned GetPolicyAttribute(PS_Policy::SecAttrUint   attr, const Window* window = NULL) const
	{ return GetPolicy() != NULL ? GetPolicy()->GetAttribute(attr, GetUrlContextId(), GetDomain(), window) : PS_Policy::UINT_ATTR_INVALID; }

	/**
	 * Shorthand to access policy attributes
	 */
	const uni_char* GetPolicyAttribute(PS_Policy::SecAttrUniStr attr, const Window* window = NULL) const
	{ return GetPolicy() != NULL ? GetPolicy()->GetAttribute(attr, GetUrlContextId(), GetDomain(), window) : NULL; }

	/**
	 * Shorthand to access policy attributes
	 */
	OP_STATUS SetPolicyAttribute(PS_Policy::SecAttrUint64 attr, OpFileLength new_value, const Window* window = NULL)
	{ return GetPolicy() != NULL ? GetPolicy()->SetAttribute(attr, GetUrlContextId(), new_value, GetDomain(), window) : OpStatus::OK; }

	/**
	 * Shorthand to access policy attributes
	 */
	OP_STATUS SetPolicyAttribute(PS_Policy::SecAttrUint   attr, unsigned     new_value, const Window* window = NULL)
	{ return GetPolicy() != NULL ? GetPolicy()->SetAttribute(attr, GetUrlContextId(), new_value, GetDomain(), window) : OpStatus::OK; }

	/**
	 * Shorthand to access policy attributes
	 */
	BOOL IsPolicyConfigurable(PS_Policy::SecAttrUint64 attr) const
	{ return GetPolicy() && GetPolicy()->IsConfigurable(attr); }

	/**
	 * Shorthand to access policy attributes
	 */
	BOOL IsPolicyConfigurable(PS_Policy::SecAttrUint   attr) const
	{ return GetPolicy() && GetPolicy()->IsConfigurable(attr); }

	/**
	 * Shorthand to access policy attributes
	 */
	BOOL IsPolicyConfigurable(PS_Policy::SecAttrUniStr attr) const
	{ return GetPolicy() && GetPolicy()->IsConfigurable(attr); }

	/**
	 * State used to indicate if we are waiting for user reply from
	 * a quota exceeded dialog or not, and what the reply was.
	 */
	enum QuotaStatus
	{
		QS_DEFAULT,
		QS_WAITING_FOR_USER,
		QS_USER_REPLIED
	};

	void        SetQuotaHandlingStatus(QuotaStatus q) { m_quota_status = q; }
	QuotaStatus GetQuotaHandlingStatus() const { return m_quota_status; }

private:

	DISABLE_EVIL_MEMBERS(PS_IndexEntry)

	/**
	 * Tells if Opera's message queue is running, If not then core is shutting down
	 */
	static BOOL IsOperaRunning() { return OpDbUtils::IsOperaRunning(); }

	/**
	 * Listener called by the underlying PS_Object when it is deleted
	 *
	 * @param should_delete        the underlying PS_Object might tell this listener
	 *                             to delete the object entry and data file, if the
	 *                             later does not have interesting data
	 *
	 */
	void HandleObjectShutdown(BOOL should_delete);

	/**
	 * Sets a new value for the cached data size of this object.
	 * Trusted users only. Could breaks quotas if used wrongly
	 *
	 * @param file_size    new data size
	 **/
	void SetDataFileSize(const OpFileLength& file_size);

	/**
	 * Invalidates the cached data size for this object
	 * Trusted users only. Could breaks quotas if used wrongly
	 */
	inline void InvalidateCachedDataSize() { UnsetFlag(HAS_CACHED_DATA_SIZE); }

	/**
	 * Auxiliary function used by CompareAsc and CompareDesc
	 */
	static int Compare(const PS_IndexEntry *lhs, const PS_IndexEntry *rhs, int order);

	/**
	 * Factory function that creates an object and stored all meta data of this object
	 *
	 * @param type                type of object. For possible values check
	 *                            PS_ObjectTypes::PS_ObjectType
	 * @param origin              origin for the object, per-html5, optional
	 * @param name                name for the object, optional. This way
	 *                            a single origin can have many different web databases
	 * @param data_file_name      path to file, optional, might be relative or absolute
	 * @param db_version          version of the database, used for html5 databases,
	 *                            will be read and stored on the index file
	 * @param is_persistent       bool which tells if the data should be
	 *                            stored in the harddrive, or kept only in
	 *                            memory. Used to implement privacy mode
	 * @param context_id          url context id if applicable
	 * @param ps_obj              PS_Object object that belongs to this index entry if any
	 */
	static PS_IndexEntry* Create(PS_Manager* mgr, URL_CONTEXT_ID context_id,
			PS_ObjectType type, const uni_char* origin,
			const uni_char* name, const uni_char* data_file_name,
			const uni_char* db_version, BOOL is_persistent,
			PS_Object* ps_obj);

	PS_IndexEntry(PS_Manager* mgr);

	/**
	 * Cleans up the object entirely, also deletes the associated PS_Object
	 */
	void Destroy();

	/**
	 * Tells if the entire index of objects is being destroyed
	 */
	BOOL IsIndexBeingDeleted() const { return GetPSManager()->IsIndexBeingDeleted(m_context_id); }

	OpFileLength         m_cached_file_size; ///< cached data file size
	PS_Manager*          m_manager;          ///< PS_Manager that owns this object
	PS_ObjectType        m_type;
	uni_char*            m_origin;
	uni_char*            m_name;
	uni_char*            m_db_version;
	URL_CONTEXT_ID       m_context_id;
	uni_char*            m_domain;           ///< server name component of origin, or the
	                                         ///  full origin if it does not have a servername component
	PS_DataFile*         m_data_file_name;   ///< object that represents the data file
	PS_DataFile_DeclRefOwner
	PS_Object*           m_ps_obj;           ///< the ps object
	PS_Policy*           m_res_policy;       ///< policy object
	QuotaStatus          m_quota_status;     ///< last answer given to the call to WindowCommander::OnQuotaIncrease

	friend class PS_Object;
	friend class PS_Manager;
	friend class PS_DataFile;
};


/**
 * Base class that represents a persistent storage object.
 * Either a web database or a web storage area
 */
class PS_Object : public PS_Base, public PS_ObjDelListener, public PS_ObjectTypes DB_INTEGRITY_CHK(0x99ddeba6)
{
public:

	/**
	 * Creates a new PS_Object suited for the type specified
	 * by the entry object.
	 * See PS_Manager::GetObject
	 */
	static PS_Object* Create(PS_IndexEntry* key);

	/**
	 * This function tags a specific object for deletion. The
	 * object is not deleted immediately though because there might
	 * be other reference owners and open transactions. When all
	 * transactions close, the object will delete itself, unless
	 * a new call to GetObject is done, which will unmark the deletion
	 * flag. By deleting the object, its entry on the index file will
	 * be removed. When calling this function the data file is immediately
	 * removed, meaning that new transactions will use a new empty data
	 * faile. previously open transactions will continue to use the
	 * old data file until they close. When those transactions close
	 * the old data file will be deleted
	 * See PS_Manager::DeleteObject
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 *
	 * @return OpStatus::OK if entry found, OpStatus::ERR_OUT_OF_RANGE if entry not found
	 *         or other errors if processing fails, like OOM when loading the index in memory
	 */
	static OP_STATUS DeleteInstance(PS_ObjectType type, const uni_char* origin,
		const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id);

	/**
	 * This function tags all objects of the given type for the given
	 * origin for deletion. The behavior for deletion is the same as
	 * the one from DeleteObject.
	 * See PS_Manager::DeleteObjects
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param context_id      url context id if applicable
	 * @param origin          origin for the object, per-html5, optional
	 */
	static OP_STATUS DeleteInstances(PS_ObjectType type, URL_CONTEXT_ID context_id, const uni_char *origin, BOOL only_persistent = TRUE);
	static OP_STATUS DeleteInstances(PS_ObjectType type, URL_CONTEXT_ID context_id, BOOL only_persistent = TRUE);

	/**
	 * Wrapper for PS_Manager::GetObject
	 * This method returns a new object ready to be used.
	 * If the object does not exist, it is created.
	 * If there is no previous information on the object index
	 * for this object, a new entry is created and the index is
	 * flushed to disk.
	 * Note: PS_Objects are shared among all the callees, therefore
	 * references are counted. After receiving a object by calling
	 * this function, the reference owner must dispose of the
	 * object using PS_Object::Release().
	 *
	 * @param type            type of object. For possible values check
	 *                        PS_ObjectTypes::PS_ObjectType
	 * @param origin          origin for the object, per-html5, optional
	 * @param name            name for the object, optional. This way
	 *                        a single origin can have many different objects
	 * @param is_persistent   bool which tells if the data should be
	 *                        stored in the harddrive, or kept only in
	 *                        memory. Used to implement privacy mode
	 * @param context_id      url context id if applicable
	 * @param object          pointer to PS_Object* where the new
	 *                        object will be placed.
	 * @return status of call. If this function returns OK, then
	 *         the value on object is ensured not to be null,
	 *         else it'll be null. If access to this object is forbidden
	 *         then this will return OpStatus::ERR_NO_ACCESS.
	 */
	static OP_STATUS GetInstance(PS_ObjectType type, const uni_char* origin,
			const uni_char* name, BOOL is_persistent, URL_CONTEXT_ID context_id, PS_Object** object);

	/**
	 * Synchronously evaluates the amount of data spent by this object in
	 * bytes. Must not use cached info ! This result WILL be used to cache
	 * the result.
	 *
	 * @param result      pointer to number where the size should be stored
	 * @returns any status code representing the success or failure of the operations
	 */
	virtual OP_STATUS EvalDataSizeSync(OpFileLength *result) = 0;

	/**
	 * This method shall be used when a reference owner
	 * no longer needs an PS_Object and needs to dispose of it.
	 * During normal execution, calling Release will cause the
	 * object to do delayed shutdown, which means that any running
	 * operations will continue to execute until they finish.
	 * After all operations finish, if there are no more reference
	 * owners, the object will delete itself.
	 *
	 * @returns this methods normally will not fail, but it might
	            return OOM is really rare cases
	 */
	virtual OP_STATUS Release() = 0;

	/**
	 * This method should tell if this object does not hold any
	 * data, and therefore can be deleted automatically by the
	 * PS_Manager when clearing the index.
	 */
	virtual BOOL CanBeDropped() const = 0;

	/**
	 * This method will be called in case one of the Delete
	 * functions of the PersistentStorageCommander is called that
	 * affects this object, but however due to ref counting, the object
	 * can't be disposed just yet, so the object is notified to clear
	 * any data it might have in memory. The file on disk is deleted
	 * regardless.
	 * The request does not need to be respected immediately,
	 * but further accesses to the data need to run after.
	 *
	 * @return TRUE if data was cleared, FALSE otherwise.
	  *         In case of doubt return FALSE
	 */
	virtual BOOL PreClearData() = 0;

	/**
	 * An object might have reference count set at zero, but still
	 * have pending actions which have been left by the API
	 * user, after dropping the reference. While these actions have
	 * not run, the object needs to be held for a while longer.
	 * Note that the object will be deleted forcefully, even if
	 * there are pending actions, during shutdown.
	 */
	virtual BOOL HasPendingActions() const = 0;

public:

	/**
	 * All the following methods are just shorthands for the methods in PS_IndexEntry
	 */
	typedef PS_IndexEntry::QuotaStatus QuotaStatus;

	inline OP_STATUS GetDataFileSize(OpFileLength* file_size) { return m_index_entry->GetDataFileSize(file_size); }
	inline void InvalidateCachedDataSize() { m_index_entry->InvalidateCachedDataSize(); }
	inline void SetDataFileSize(const OpFileLength& file_size) { m_index_entry->SetDataFileSize(file_size); }

	//stubs
	inline PS_IndexEntry* GetIndexEntry() const { return m_index_entry; }
	inline PS_Manager* GetPSManager() const { return m_index_entry->GetPSManager(); }
	inline PS_ObjectTypes::PS_ObjectType GetType() const { return m_index_entry->GetType(); }
	inline const uni_char* GetTypeDesc() const { return m_index_entry->GetTypeDesc(); }
	inline const uni_char* GetDomain() const { return m_index_entry->GetDomain(); };
	inline const uni_char* GetOrigin() const { return m_index_entry->GetOrigin(); };
	inline const uni_char* GetName() const { return m_index_entry->GetName(); };
	inline const uni_char* GetVersion() const { return m_index_entry->GetVersion(); };
	OP_STATUS SetVersion(const uni_char* db_version) { return m_index_entry->SetVersion(db_version); };
	inline URL_CONTEXT_ID GetUrlContextId() const { return m_index_entry->m_context_id; }
	inline BOOL IsMemoryOnly() const { return m_index_entry->IsMemoryOnly(); }
	inline BOOL IsPersistent() const { return m_index_entry->IsPersistent(); }

	inline OP_STATUS MakeAbsFilePath() { return m_index_entry->MakeAbsFilePath(); }
	inline BOOL HasDataFile() const { return m_index_entry->HasDataFile(); }
	inline PS_DataFile* GetFileNameObj() const { return m_index_entry->GetFileNameObj(); }
	inline OP_STATUS EnsureDataFileFolder() const { return m_index_entry->EnsureDataFileFolder(); }
	inline const uni_char* GetFileAbsPath() const { return m_index_entry->GetFileAbsPath(); }
	inline const uni_char* GetFileRelPath() const { return m_index_entry->GetFileRelPath(); }

	MessageHandler* GetMessageHandler() const { return GetPSManager()->GetMessageHandler(); };
	inline BOOL IsOperaRunning() const { return GetPSManager()->IsOperaRunning(); }
	inline BOOL IsIndexBeingDeleted() const { return m_index_entry->IsIndexBeingDeleted(); }
	inline unsigned GetRefCount() const { return m_index_entry->GetRefCount(); }
	inline OpFile* GetOpFile() const { return m_index_entry->GetOpFile(); }
	inline OP_STATUS FileExists(BOOL *exists) const { return m_index_entry->FileExists(exists); }

	void SetPolicy(PS_Policy* policy) { m_index_entry->SetPolicy(policy); }
	PS_Policy* GetPolicy() const { return m_index_entry->GetPolicy(); }

	OpFileLength GetPolicyAttribute(PS_Policy::SecAttrUint64 attr, const Window* window = NULL) const
	{ return GetIndexEntry()->GetPolicyAttribute(attr, window); }
	unsigned GetPolicyAttribute(PS_Policy::SecAttrUint   attr, const Window* window = NULL) const
	{ return GetIndexEntry()->GetPolicyAttribute(attr, window); }
	const uni_char* GetPolicyAttribute(PS_Policy::SecAttrUniStr attr, const Window* window = NULL) const
	{ return GetIndexEntry()->GetPolicyAttribute(attr, window); }
	OP_STATUS SetPolicyAttribute(PS_Policy::SecAttrUint64 attr, OpFileLength new_value, const Window* window = NULL)
	{ return GetIndexEntry()->SetPolicyAttribute(attr, new_value, window);}
	OP_STATUS SetPolicyAttribute(PS_Policy::SecAttrUint   attr, unsigned     new_value, const Window* window = NULL)
	{ return GetIndexEntry()->SetPolicyAttribute(attr, new_value, window);}

	inline void        SetQuotaHandlingStatus(QuotaStatus q) { m_index_entry->SetQuotaHandlingStatus(q); }
	inline QuotaStatus GetQuotaHandlingStatus() const { return m_index_entry->GetQuotaHandlingStatus(); }

protected:
	PS_Object(PS_IndexEntry* entry);
	virtual ~PS_Object() {}

	void HandleObjectShutdown(BOOL should_delete) { if (m_index_entry != NULL) m_index_entry->HandleObjectShutdown(should_delete); }

	PS_Object(const PS_Object&) { OP_ASSERT(!"Don't call this"); }
	PS_Object() { OP_ASSERT(!"Don't call this"); }

	friend class PS_Manager;
	friend class PS_IndexEntry;

private:

	PS_IndexEntry* m_index_entry;
};

typedef AutoReleaseTypePtr<PS_Object> AutoReleasePSObjectPtr;

#ifdef OPSTORAGE_SUPPORT

#include "modules/database/webstorage_data_abstraction.h"

PS_ObjectTypes::PS_ObjectType WebStorageTypeToPSObjectType(WebStorageType ws_t);
WebStorageType PSObjectTypeToWebStorageType(PS_ObjectTypes::PS_ObjectType db_t);

#endif //OPSTORAGE_SUPPORT

#endif //DATABASE_MODULE_MANAGER_SUPPORT

#endif /* _MODULES_DATABASE_OPDATABASEMANAGER_H_ */
