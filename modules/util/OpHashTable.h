/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_OPHASHTABLE_H
#define MODULES_UTIL_OPHASHTABLE_H

/* NOTE: See modules/util/stringhash.h for a string-indexed hash table that
 * keeps copies of keys internally. */

/**
 * The OpHashFunctions object contains the hash and
 * compare functions used by a OpHashTable that do not
 * hash on ponter values.
 *
 */
class OpHashFunctions
{
public:
	virtual ~OpHashFunctions() {}

	/**
	 * Calculates the hash value for a key.
	 * the hash key.
	 */
	virtual UINT32	Hash(const void* key) = 0;

	/**
	 * Compares if to keys are equal.
	 */
	virtual BOOL	KeysAreEqual(const void* key1, const void* key2) = 0;

};

class ChainedHashBackend;
class OpHashTable;
class OpGenericVector;

class ChainedHashLink
{
public:
	const void* key;
	void* data;
	ChainedHashLink* next;
	BOOL used;
};

class OpHashIterator
{
public:
	virtual ~OpHashIterator() {}

	/**
	 * Makes sure that the first key and data
	 * can be read with GetKey() and GetData().
	 * @return OpStatus::OK, there was a first element.
	 * @return OpStatus::ERR, the hash table that is represented by the iterator is empty.
	 */
	virtual OP_STATUS First() = 0;

	/**
	 * Makes sure that the next key and data
	 * can be read with GetKey() and GetData().
	 * @return OpStatus::OK, there was a next element.
	 * @return OpStatus::ERR, the hash table that is represented by the iterator didn't have a next element.
	 */
	virtual OP_STATUS Next() = 0;

	/**
	 * Works only after a call to First() or Next()
	 * @return the key for the current element.
	 */
	virtual const void* GetKey() const = 0;

	/**
	 * Works only after a call to First() or Next()
	 * @return the data for the current element.
	 */
	virtual void* GetData() const = 0;
};

class ChainedHashIterator : public OpHashIterator
{
public:
	ChainedHashIterator(const ChainedHashBackend* backend);

	virtual OP_STATUS First();
	virtual OP_STATUS Next();
	virtual const void* GetKey() const;
	virtual void* GetData() const;

	/**
	 * Set key and data that is current in the iterator.
	 * Called from OpHashTable::Iterate().
	 * @param key the key to set.
	 * @param data the data to set.
	 */
	void SetKeyAndData(const void* key, void* data);

	/**
	 * Set hash link pos that is current in the iterator.
	 * Value will be -1 when we get an error, or before we have called First().
	 * Called from OpHashTable::Iterate().
	 */
	void SetHashLinkPos(INT32 pos);

	/**
	 * @return the hash link pos, -1 if First() has not been called, or if we have traversed the complete hash table.
	 */
	INT32 GetHashLinkPos() const;

private:
	const ChainedHashBackend* backend;
	const void* key;
	void* data;
	INT32 hash_link_pos;
};

/**
 * Class used to iterate over an HashBackend.
 * This way we can skip the iterator object allocation
 */
class OpHashTableForEachListener
{
public:
	virtual void HandleKeyData(const void* key, void* data) = 0;
};

class ChainedHashBackend
{
public:
	ChainedHashBackend();
	~ChainedHashBackend();

	/** Initialize the hash back-end.
	 *
	 * Specify the number of elements and array size (the ratio defines the
	 * density of the hash table, which affects performance).  Called from
	 * OpHashTable.
	 *
	 * @param hash_functions The OpHashFunctions object to use in computing hash values.
	 * @param array_size size of the array that is used for spreading the hash values.
	 * @param max_nr_of_elements nr of elements that the hash table can hold.
	 * @return OpStatus::OK, everything went well.
	 * @return OpStatus::ERR_NO_MEMORY, out of memory.
	 */
	OP_STATUS Init(OpHashFunctions* hash_functions, UINT32 array_size, UINT32 max_nr_of_elements);

	/**
	 * Adds a key and data to the HashBackend.
	 * Called from OpHashTable.
	 * @param key the key to add.
	 * @param data the data to add.
	 * @return OpStatus::OK, if the element was added.
	 * @return OpStatus::ERR, if the element was already added.
	 */
	OP_STATUS Add(const void* key, void* data);

	/**
	 * Like Add(), but updates the data associated with the key if it is
	 * already in the hash table. It is the caller's responsibility to make
	 * sure the old data value is not lost (if it is a pointer that needs to be
	 * deleted, for example).
	 *
	 * @param key      The key to update the data for.
	 * @param data     The new data value.
	 * @param old_data If non-null, receives the old data value at the key.
	 *                 Unmodified if the key is not in the table.
	 *
	 * @retval TRUE The key was already in the hash table.
	 * @retval FALSE Otherwise.
	 */
	BOOL Update(const void* key, void* data, void** old_data);

	/**
	 * Get the data corresponding to a key.
	 * Called from OpHashTable.
	 * @param key the key for the element to get.
	 * @param data where the found data is placed.
	 * @return OpStatus::OK, if the element was found.
	 * @return OpStatus::ERR, if the element was not in the HashBackend.
	 */
	OP_STATUS GetData(const void* key, void** data) const;

	/**
	 * Remove the key/value pair corresponding to the key.
	 * Called from OpHashTable.
	 * @param key the key in the key/value pair that should be removed.
	 * @param data where the removed data is placed.
	 * @return OpStatus::OK, if the element was removed.
	 * @return OpStatus::ERR, if the element was not in the list.
	 */
	OP_STATUS Remove(const void* key, void** data);

	/**
	 * Get an iterator to iterate the HashBackend.
	 * The returned iterator is to be freed by the caller.
	 * Called from OpHashTable.
	 * @return the iterator for the HashBackend, or NULL if OOM.
	 */
	OpHashIterator* GetIterator() const;

	/**
	 * Call function for each (key, data) pair in the hash table.
	 */
	void ForEach(void (*function)(const void *, void *));

	void ForEach(void (*function)(const void *, void *, OpHashTable *), OpHashTable* table);

	void ForEach(OpHashTableForEachListener* listener);

	/**
	 * Add all data elements to an OpGenericVector (the order is random, ie. how they appear in hash table)
	 */
	OP_STATUS CopyAllToVector(OpGenericVector& vector) const;

	/**
	 * Add all data elements to another OpHashTable
	 */
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const;

	/**
	 * Modifies the iterator to point to the first element in
	 * the ChainedHashBackend.
	 * Called from ChainedHashIterator::First().
	 * @param iterator the iterator that is modified.
	 * @return OpStatus::OK if the backend had an element.
	 * @return OpStatus::ERR if the backend was empty.
	 */
	OP_STATUS First(ChainedHashIterator* iterator) const;

	/**
	 * Modifies the iterator to point to the next element in
	 * the ChainedHashBackend.
	 * Called from ChainedHashIterator::Next().
	 * @param iterator the iterator that is modified.
	 * @return OpStatus::OK if the backend had a next element.
	 * @return OpStatus::ERR if the backend did not have a next element.
	 */
	OP_STATUS Next(ChainedHashIterator* iterator) const;

	/**
	 * Bring this object to its default constructed stated, destroying/deallocating its substructures as necessary.
	 */
	void  CleanUp(BOOL destroy_substructures=TRUE);

private:
	UINT32 Hash(const void* key, UINT32 tableSize) const;
	ChainedHashLink* GetNewLink();
	void FreeLink(ChainedHashLink* link);
	BOOL FindElm(UINT32 array_pos, const void* key, ChainedHashLink** link, ChainedHashLink** prev_link) const;
	OP_STATUS Iterate(ChainedHashIterator* iterator, UINT32 start_pos) const;

	OpHashFunctions* hash_functions;
	UINT32 table_size;
	UINT32 nr_of_hash_links;
	ChainedHashLink** hash_array;
	ChainedHashLink* hash_links;
	ChainedHashLink* first_free_link;
};

class OpHashTable : public OpHashFunctions
{
public:
	OpHashTable(OpHashFunctions* hashFunctions = NULL,
				BOOL useChainedHashing = TRUE);

	virtual ~OpHashTable();

	void SetHashFunctions(OpHashFunctions* hashFunctions) {hash_functions = hashFunctions;}

	/**
	 * Adds a key with data to the hash table.
	 *
	 * Return/Leave values:
	 * OpStatus::OK if we could add the key and data.
	 * OpStatus::ERR the key was already in the hash table.
	 * OpStatus::ERR_NO_MEMORY if we could not add because of OOM.
	 */
	OP_STATUS Add(const void* key, void* data);
	void AddL(const void* key, void* data);

	/**
	 * Like Add(), but updates the data associated with the key if it is
	 * already in the hash table. It is the caller's responsibility to make
	 * sure the old data value is not lost (if it is a pointer that needs to be
	 * deleted, for example). The DeleteAll() method will not delete a pointer
	 * value that has been replaced via Update().
	 *
	 * @param key      The key to update the data for.
	 * @param data     The new data value.
	 * @param old_data If non-null, receives the old data value at the key.
	 *                 Unmodified if the key is not in the table.
	 * @param had_key  If non-null, receives TRUE if the key was already in
	 *                 the table and FALSE otherwise. This is needed as
	 *                 old_data might be 0.
	 *
	 * @retval OpStatus::OK The key was successfully inserted/updated.
	 * @retval OpStatus::ERR_NO_MEMORY The update failed because of OOM.
	 */
	OP_STATUS Update(const void* key,
	                 void* data,
	                 void** old_data = NULL,
	                 BOOL* had_key   = NULL);

	/**
	 * Gets data in the pointer pointer data.
	 *
	 * Return/Leave values:
	 * @return OpStatus::OK, we get some data from the table.
	 * @return OpStatus::ERR, the key was not in the hash table.
	 */
	OP_STATUS GetData(const void* key, void** data) const;

	/**
	 * Remove an element from the hash table.
	 * @param key the key for the element to remove.
	 * @param data here will the data be added.
	 * @return OpStatus::OK, if everything went well.
	 * @return OpStatus::ERR, if element was not in hash table.
	 */
	OP_STATUS Remove(const void* key, void** data);

	/**
	 * Get an iterator for the hash table.
	 * @return the iterator, or NULL if OOM.
	 */
	OpHashIterator*	GetIterator();

	/**
	 * Call function for each (key, data) pair in the hash table.
	 */
	void ForEach(void (*function)(const void *, void *));

	void ForEach(void (*function)(const void *, void *, OpHashTable *));

	void ForEach(OpHashTableForEachListener* listener);

	/**
	 * Remove all elements
	 */
	void RemoveAll();

	/**
	 * Delete all elements. To use this, you must overload Delete(void* data).
	 * Note that this method will not delete pointers that have been replaced
	 * via calls to Update().
	 */
	void DeleteAll();

	/**
	 * Add all data elements to an OpGenericVector (the order is random, ie. how they appear in hash table)
	 */

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const;

	/**
	 * Add all data elements to another OpHashTable
	 */

	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const;

	/**
	 * Any item with that key?
	 */

	BOOL Contains(const void* key) const {void* data; return GetData(key, &data) == OpStatus::OK;}

	/**
	 * Any item with that key and exactsame data?
	 */

	BOOL Contains(const void* key, void* data) const {void* other_data; return GetData(key, &other_data) == OpStatus::OK && data == other_data;}

	/**
	 * Get number of items in hashtable
	 */

	INT32 GetCount() const {return nr_of_elements;}

	/**
	 * Set minimum nr of elements (to speed up Adding lots of items where that number of items is known)
	 */

	void SetMinimumCount(UINT32 minimum_count);

	/* Default OpHashFunctions */
	virtual UINT32	Hash(const void* key);
	virtual BOOL	KeysAreEqual(const void* key1, const void* key2);

protected:

	virtual void Delete(void* /*data*/) {OP_ASSERT(0);}  // must be overloaded to be able to delete

private:
	OP_STATUS Init();

	BOOL IsInitialized() const;
	BOOL IsChained() const;
	void SetInitialized(BOOL initialized);
	void SetChained(BOOL chained);

	OP_STATUS Rehash(UINT16 size_index);
	OP_STATUS GrowIfNeeded();

	OpHashFunctions*   hash_functions;
	ChainedHashBackend hash_backend;
	UINT32 nr_of_elements;
	UINT32 minimum_nr_of_elements;
	UINT16 hash_size_index;
	UINT16 flags;

	static void DeleteFunc(const void *key, void *data, OpHashTable* table);
};

class OpGenericStringHashTable : protected OpHashTable
{
	public:

		OpGenericStringHashTable(BOOL case_sensitive = FALSE) : OpHashTable(), m_case_sensitive(case_sensitive) {SetHashFunctions(this);}

		OP_STATUS Add(const uni_char* key, void* data) {return OpHashTable::Add((void*)key, data);}
		void AddL(const uni_char* key, void* data) {OpHashTable::AddL((void*)key, data);}

		OP_STATUS GetData(const uni_char* key, void** data) const {return OpHashTable::GetData((void*)key, data);}

		OP_STATUS Remove(const uni_char* key, void** data) {return OpHashTable::Remove((void*)key, data);}

		OpHashIterator*	GetIterator() {return OpHashTable::GetIterator();}

		void ForEach(void (*function)(const void *, void *)) {OpHashTable::ForEach(function);}
		void ForEach(void (*function)(const void *, void *, OpHashTable *)) {OpHashTable::ForEach(function);}

		void ForEach(OpHashTableForEachListener* listener) {OpHashTable::ForEach(listener);}

		void RemoveAll() {OpHashTable::RemoveAll();}
		void DeleteAll() {OpHashTable::DeleteAll();}

		BOOL Contains(const uni_char* key) const {return OpHashTable::Contains((void*)key);}

		OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpHashTable::CopyAllToVector(vector);}

		INT32 GetCount() const {return OpHashTable::GetCount();}

		// expose this to the outside world
		static UINT32 HashString(const uni_char* key, BOOL case_sensitive);
		static UINT32 HashString(const uni_char* key, unsigned str_length, BOOL case_sensitive);

		// OpHashFunctions interface

		virtual UINT32 Hash(const void* key);
		virtual BOOL KeysAreEqual(const void* key1, const void* key2);

		void SetMinimumCount(UINT32 minimum_count){ return OpHashTable::SetMinimumCount(minimum_count); }

	private:

		BOOL m_case_sensitive;
};

class OpGenericString8HashTable : protected OpHashTable
{
	public:

		OpGenericString8HashTable(BOOL case_sensitive = FALSE) : OpHashTable(), m_case_sensitive(case_sensitive) {SetHashFunctions(this);}

		OP_STATUS Add(const char* key, void* data) {return OpHashTable::Add((void*)key, data);}
		void AddL(const char* key, void* data) {OpHashTable::AddL((void*)key, data);}

		OP_STATUS GetData(const char* key, void** data) const {return OpHashTable::GetData((void*)key, data);}

		OP_STATUS Remove(const char* key, void** data) {return OpHashTable::Remove((void*)key, data);}

		OpHashIterator*	GetIterator() {return OpHashTable::GetIterator();}

		void ForEach(void (*function)(const void *, void *)) {OpHashTable::ForEach(function);}
		void ForEach(void (*function)(const void *, void *, OpHashTable *)) {OpHashTable::ForEach(function);}

		void ForEach(OpHashTableForEachListener* listener) {OpHashTable::ForEach(listener);}

		void RemoveAll() {OpHashTable::RemoveAll();}
		void DeleteAll() {OpHashTable::DeleteAll();}

		BOOL Contains(const char* key) const {return OpHashTable::Contains((void*)key);}

		INT32 GetCount() const {return OpHashTable::GetCount();}

		// expose this to the outside world
		static UINT32 HashString(const char* key, BOOL case_sensitive);
		static UINT32 HashString(const char* key, unsigned str_length, BOOL case_sensitive);

		// OpHashFunctions interface

		virtual UINT32 Hash(const void* key);
		virtual BOOL KeysAreEqual(const void* key1, const void* key2);

		void SetMinimumCount(UINT32 minimum_count){ return OpHashTable::SetMinimumCount(minimum_count); }

	private:

		BOOL m_case_sensitive;
};

template<class T>
class OpStringHashTable : private OpGenericStringHashTable
{
	public:

		OpStringHashTable(BOOL case_sensitive = FALSE) : OpGenericStringHashTable(case_sensitive) {}

		OP_STATUS Add(const uni_char* key, T* data) {return OpGenericStringHashTable::Add(key, (void*)data);}
		void AddL(const uni_char* key, T* data) {OpGenericStringHashTable::AddL(key, (void*)data);}

		OP_STATUS GetData(const uni_char* key, T** data) const {return OpGenericStringHashTable::GetData(key, (void**)data);}

		BOOL Contains(const uni_char* key) const {return OpGenericStringHashTable::Contains(key);}

		OP_STATUS Remove(const uni_char* key, T** data) {return OpGenericStringHashTable::Remove(key, (void**)data);}

		OpHashIterator*	GetIterator() {return OpGenericStringHashTable::GetIterator();}

		void ForEach(void (*function)(const void *, void *)) {OpGenericStringHashTable::ForEach(function);}
		void ForEach(void (*function)(const void *, void *, OpHashTable *)) {OpGenericStringHashTable::ForEach(function);}

		void ForEach(OpHashTableForEachListener* listener) {OpGenericStringHashTable::ForEach(listener);}

		void RemoveAll() {OpGenericStringHashTable::RemoveAll();}
		void DeleteAll() {OpGenericStringHashTable::DeleteAll();}

		virtual void Delete(void* data) {OP_DELETE((T*) data );}

		INT32 GetCount() const {return OpGenericStringHashTable::GetCount();}

		OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpGenericStringHashTable::CopyAllToVector(vector);}

		void SetMinimumCount(UINT32 minimum_count){ OpGenericStringHashTable::SetMinimumCount(minimum_count); }
};

template<class T>
class OpString8HashTable : private OpGenericString8HashTable
{
	public:

		OpString8HashTable(BOOL case_sensitive = FALSE) : OpGenericString8HashTable(case_sensitive) {}

		OP_STATUS Add(const char* key, T* data) {return OpGenericString8HashTable::Add(key, (void*)data);}
		void AddL(const char* key, T* data) {OpGenericString8HashTable::AddL(key, (void*)data);}

		OP_STATUS GetData(const char* key, T** data) const {return OpGenericString8HashTable::GetData(key, (void**)data);}

		BOOL Contains(const char* key) const {return OpGenericString8HashTable::Contains(key);}

		OP_STATUS Remove(const char* key, T** data) {return OpGenericString8HashTable::Remove(key, (void**)data);}

		OpHashIterator*	GetIterator() {return OpGenericString8HashTable::GetIterator();}

		void ForEach(void (*function)(const void *, void *)) {OpGenericString8HashTable::ForEach(function);}
		void ForEach(void (*function)(const void *, void *, OpHashTable *)) {OpGenericString8HashTable::ForEach(function);}

		void ForEach(OpHashTableForEachListener* listener) {OpGenericString8HashTable::ForEach(listener);}

		void RemoveAll() {OpGenericString8HashTable::RemoveAll();}
		void DeleteAll() {OpGenericString8HashTable::DeleteAll();}

		virtual void Delete(void* data) {OP_DELETE((T*) data );}

		INT32 GetCount() const {return OpGenericString8HashTable::GetCount();}

		void SetMinimumCount(UINT32 minimum_count){ OpGenericString8HashTable::SetMinimumCount(minimum_count); }
};

template<class T>
class OpAutoStringHashTable : public OpStringHashTable<T>
{
	public:

		OpAutoStringHashTable(BOOL case_sensitive = FALSE) : OpStringHashTable<T>(case_sensitive) {}
		virtual ~OpAutoStringHashTable() {this->DeleteAll();}
};

template<class T>
class OpAutoString8HashTable : public OpString8HashTable<T>
{
	public:

		OpAutoString8HashTable(BOOL case_sensitive = FALSE) : OpString8HashTable<T>(case_sensitive) {}
		virtual ~OpAutoString8HashTable() {this->DeleteAll();}
};

template<class T>
class OpINT32HashTable : protected OpHashTable
{
public:

	OpINT32HashTable() : OpHashTable() {}

	OP_STATUS Add(INT32 key, T* data) {return OpHashTable::Add(reinterpret_cast<void*>(key), (void*)data);}

	OP_STATUS GetData(INT32 key, T** data) const {return OpHashTable::GetData(reinterpret_cast<void*>(key), (void**)data);}

	BOOL Contains(INT32 key) const {return OpHashTable::Contains(reinterpret_cast<void*>(key));}

	OP_STATUS Remove(INT32 key, T** data) {return OpHashTable::Remove(reinterpret_cast<void*>(key), (void**)data);}

	OpHashIterator*	GetIterator() {return OpHashTable::GetIterator();}

	void ForEach(void (*function)(const void *, void *)) {OpHashTable::ForEach(function);}
	void ForEach(void (*function)(const void *, void *, OpHashTable *)) {OpHashTable::ForEach(function);}

	void ForEach(OpHashTableForEachListener* listener) {OpHashTable::ForEach(listener);}

	void RemoveAll() {OpHashTable::RemoveAll();}
	void DeleteAll() {OpHashTable::DeleteAll();}

	virtual void Delete(void* data) {OP_DELETE((T*) data );}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpHashTable::CopyAllToVector(vector);}
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const {return OpHashTable::CopyAllToHashTable(hash_table);}
	INT32 GetCount() const {return OpHashTable::GetCount();}
	void SetMinimumCount(UINT32 minimum_count) {OpHashTable::SetMinimumCount(minimum_count);}
};

/* Helper template for storing non-pointer POD types as either
   the keys or the values (or both) of a hash table. Note that it is still okay
   to use a pointer type for either the keys or the values (or both, though
   there might be better alternatives for that).

   Note: DeleteAll() is a no-op for this hash table type. Be careful not to
   leak memory if you store pointers as values.

   Note: KeyType and ValueType need to fit in a void*. Rather than
   instantiating this template yourself, prefer to use one of the
   specializations below the class definition.

   The methods in this template class call through to the base class
   versions; see OpHashTable for documentation. */

template<class KeyType, class ValueType>
class OpGenericHashTable : protected OpHashTable
{
public:

	OpGenericHashTable() : OpHashTable()
	{
		/* Make sure KeyType and ValueType fit in a void*, which is
		   what the underlying table class (OpHashTable) uses. */
		OP_STATIC_ASSERT(sizeof(KeyType)   <= sizeof(void*) &&
		                 sizeof(ValueType) <= sizeof(void*));
	}

	OP_STATUS Add(KeyType key, ValueType data)
	{
		return OpHashTable::Add(KeyToConstVoidP(key), ValueToVoidP(data));
	}

	OP_STATUS Update(KeyType key,
	                 ValueType data,
	                 ValueType *old_data = NULL,
	                 BOOL *key_was_in    = NULL)
	{
		/* Use of 'conv' ensures we can safely write sizeof(void*) bytes, even
		   when sizeof(ValueType) < sizeof(void*) */
		union { ValueType value; void *voidp; } conv;
		conv.voidp = NULL;
		conv.value = ValueType();
		RETURN_IF_ERROR(OpHashTable::Update(KeyToConstVoidP(key),
		                                    ValueToVoidP(data),
		                                    old_data ? &conv.voidp : NULL,
		                                    key_was_in));
		if (old_data)
			*old_data = conv.value;
		return OpStatus::OK;
	}

	OP_STATUS GetData(KeyType key, ValueType *data) const
	{
		/* Use of 'conv' ensures we can safely write sizeof(void*) bytes, even
		   when sizeof(ValueType) < sizeof(void*) */
		union { ValueType value; void *voidp; } conv;
		RETURN_IF_ERROR(OpHashTable::GetData(KeyToConstVoidP(key), &conv.voidp));
		*data = conv.value;
		return OpStatus::OK;
	}

	BOOL Contains(KeyType key) const
	{
		return OpHashTable::Contains(KeyToConstVoidP(key));
	}

	OP_STATUS Remove(KeyType key, ValueType *data)
	{
		/* Use of 'conv' ensures we can safely write sizeof(void*) bytes, even
		   when sizeof(ValueType) < sizeof(void*) */
		union { ValueType value; void *voidp; } conv;
		RETURN_IF_ERROR(OpHashTable::Remove(KeyToConstVoidP(key), &conv.voidp));
		*data = conv.value;
		return OpStatus::OK;
	}

	OpHashIterator*	GetIterator() { return OpHashTable::GetIterator(); }

	void ForEach( void (*function)(const void*, void*) )               { OpHashTable::ForEach(function); }
	void ForEach( void (*function)(const void*, void*, OpHashTable*) ) { OpHashTable::ForEach(function); }

	void ForEach(OpHashTableForEachListener* listener) { OpHashTable::ForEach(listener); }

	void RemoveAll() { OpHashTable::RemoveAll(); }
	void DeleteAll() { OpHashTable::DeleteAll(); }

	virtual void Delete(void* data)
	{
		/* This method will be called if DeleteAll() is ever called on the
		   OpGenericHashTable instance. As the intent of such a kill is likely
		   to free pointers, and as this class is not designed to hold pointer
		   values (there are better alternatives), flag this with an assert. */
		OP_ASSERT(!"OpGenericHashTable is not meant to hold pointer values");
	}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const { return OpHashTable::CopyAllToVector(vector); }
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const { return OpHashTable::CopyAllToHashTable(hash_table); }
	INT32 GetCount() const { return OpHashTable::GetCount(); }
	void SetMinimumCount(UINT32 minimum_count) { OpHashTable::SetMinimumCount(minimum_count); }

private:
	/* Helper functions for storing arbitrary data inside pointers. Strictly
	   speaking, writing one member of a union and reading the value back via
	   another results in undefined behavior, but enough programs depend on
	   this working "as expected" that it'll probably keep working. */

	static const void *KeyToConstVoidP(KeyType key)
	{
		union { KeyType key; const void *cvoidp; } conv;
		conv.cvoidp = 0; // Needed in case sizeof(const void*) > sizeof(KeyType)
		conv.key = key;
		return conv.cvoidp;
	}

	static void *ValueToVoidP(ValueType value)
	{
		union { ValueType value; void *voidp; } conv;
		conv.voidp = 0; // Needed in case sizeof(void*) > sizeof(ValueType)
		conv.value = value;
		return conv.voidp;
	}
};

// Specializations of the helper template OpGenericHashTable

typedef OpGenericHashTable<UINT32, UINT32> OpUINT32ToUINT32HashTable;


template <class T>
class OpAutoINT32HashTable : public OpINT32HashTable<T>
{
public:
	OpAutoINT32HashTable() : OpINT32HashTable<T>() { }
	virtual ~OpAutoINT32HashTable() { OpINT32HashTable<T>::DeleteAll(); }
};

template <class KeyType, class DataType>
class OpPointerHashTable : protected OpHashTable
{
public:
	OpPointerHashTable() : OpHashTable() {}

	OpPointerHashTable(OpHashFunctions* hashFunctions, BOOL useChainedHashing = TRUE) :
		OpHashTable(hashFunctions,useChainedHashing)
		{}

	OP_STATUS Add(const KeyType* key, DataType* data) { return OpHashTable::Add((const void *)(key), (void *)(data)); }
	OP_STATUS Remove(const KeyType* key, DataType** data) { return OpHashTable::Remove((const void *)(key), (void **)(data)); }

	OP_STATUS GetData(const KeyType* key, DataType** data) const { return OpHashTable::GetData((const void *)(key), (void **)(data)); }
	BOOL Contains(const KeyType* key) const { return OpHashTable::Contains((const void *)(key)); }

	OpHashIterator*	GetIterator() { return OpHashTable::GetIterator(); }

	void ForEach(void (*function)(const void *, void *)) { OpHashTable::ForEach(function); }
	void ForEach(void (*function)(const void *, void *, OpHashTable *)) { OpHashTable::ForEach(function); }

	void ForEach(OpHashTableForEachListener* listener) {OpHashTable::ForEach(listener);}

	void RemoveAll() { OpHashTable::RemoveAll(); }
	void DeleteAll() { OpHashTable::DeleteAll(); }

	virtual void Delete(void* data) {OP_DELETE((DataType *)(data) ); }

	INT32 GetCount() const { return OpHashTable::GetCount(); }

	void SetMinimumCount(UINT32 minimum_count){ OpHashTable::SetMinimumCount(minimum_count); }

	void SetHashFunctions(OpHashFunctions* hashFunctions) {OpHashTable::SetHashFunctions(hashFunctions);}
};


template <class KeyType, typename IdType>
class OpPointerIdHashTable : protected OpHashTable
{
public:
	OpPointerIdHashTable() : OpHashTable() {}

	OpPointerIdHashTable(OpHashFunctions* hashFunctions, BOOL useChainedHashing = TRUE) :
		OpHashTable(hashFunctions,useChainedHashing)
		{}

	OP_STATUS Add(const KeyType* key, IdType id) { return OpHashTable::Add((const void *)(key), (void *)(INTPTR)(id)); }

	OP_STATUS Remove(const KeyType* key, IdType* id)
	{
		void* value;
		OP_STATUS status = OpHashTable::Remove((const void *)(key), &value);
		if (OpStatus::IsSuccess(status))
			*id = static_cast<IdType>(reinterpret_cast<UINTPTR>(value));
		return status;
	}

	OP_STATUS GetData(const KeyType* key, IdType* id) const
	{
		void* value;
		OP_STATUS status = OpHashTable::GetData((const void *)(key), &value);
		if (OpStatus::IsSuccess(status))
			*id = static_cast<IdType>(reinterpret_cast<UINTPTR>(value));
		return status;
	}

	BOOL Contains(const KeyType* key) const { return OpHashTable::Contains((const void *)(key)); }

	OpHashIterator*	GetIterator() { return OpHashTable::GetIterator(); }

	void ForEach(void (*function)(const void *, void *)) { OpHashTable::ForEach(function); }
	void ForEach(void (*function)(const void *, void *, OpHashTable *)) { OpHashTable::ForEach(function); }

	void ForEach(OpHashTableForEachListener* listener) {OpHashTable::ForEach(listener);}

	void RemoveAll() { OpHashTable::RemoveAll(); }
	void DeleteAll() { OpHashTable::DeleteAll(); }

	INT32 GetCount() const { return OpHashTable::GetCount(); }

	void SetMinimumCount(UINT32 minimum_count){ OpHashTable::SetMinimumCount(minimum_count); }

	void SetHashFunctions(OpHashFunctions* hashFunctions) {OpHashTable::SetHashFunctions(hashFunctions);}
};


template <class KeyType, class DataType>
class OpAutoPointerHashTable : public OpPointerHashTable<KeyType, DataType>
{
public:
	OpAutoPointerHashTable() : OpPointerHashTable<KeyType, DataType>() { }
	virtual ~OpAutoPointerHashTable() { OpPointerHashTable<KeyType, DataType>::DeleteAll(); }
};


class OpGenericPointerSet : public OpHashTable
{
public:
	OpGenericPointerSet() : OpHashTable() {}

	OP_STATUS Add(void* data) {return OpHashTable::Add(data, data);}
	BOOL Contains(void* data) const {return OpHashTable::Contains(data);}
	OP_STATUS Remove(void* data) {return OpHashTable::Remove(data, &data);}

	OpHashIterator*	GetIterator() {return OpHashTable::GetIterator();}
	void ForEach(OpHashTableForEachListener* listener) {OpHashTable::ForEach(listener);}

	void RemoveAll() {OpHashTable::RemoveAll();}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpHashTable::CopyAllToVector(vector);}
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const {return OpHashTable::CopyAllToHashTable(hash_table);}

	INT32 GetCount() const {return OpHashTable::GetCount();}
	void SetMinimumCount(UINT32 minimum_count) {OpHashTable::SetMinimumCount(minimum_count);}
};

template<class T>
class OpPointerSet : private OpGenericPointerSet
{
public:
	OpPointerSet() : OpGenericPointerSet() {}

	OP_STATUS Add(T* data) {return OpGenericPointerSet::Add(data);}
	BOOL Contains(T* data) const {return OpGenericPointerSet::Contains(data);}
	OP_STATUS Remove(T* data) {return OpGenericPointerSet::Remove(data);}

	OpHashIterator*	GetIterator() {return OpGenericPointerSet::GetIterator();}
	void ForEach(OpHashTableForEachListener* listener) {OpGenericPointerSet::ForEach(listener);}

	void RemoveAll() {OpGenericPointerSet::RemoveAll();}
	void DeleteAll() {OpGenericPointerSet::DeleteAll();}

	virtual void Delete(void* data) {OP_DELETE((T*) data );}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpGenericPointerSet::CopyAllToVector(vector);}
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const {return OpGenericPointerSet::CopyAllToHashTable(hash_table);}

	INT32 GetCount() const {return OpHashTable::GetCount();}
};

class OpINT32Set : private OpGenericPointerSet
{
public:
	/** Callback class for ForEach(). */
	class ForEachIterator{
	public:
		virtual void HandleInteger(INT32 data) = 0;
	};

	OpINT32Set() : OpGenericPointerSet() {}

	OP_STATUS Add(INT32 data) {return OpGenericPointerSet::Add(INT_TO_PTR(data));}
	BOOL Contains(INT32 data) const {return OpGenericPointerSet::Contains(INT_TO_PTR(data));}
	OP_STATUS Remove(INT32 data) {return OpGenericPointerSet::Remove(INT_TO_PTR(data));}

	OpHashIterator*	GetIterator() {return OpGenericPointerSet::GetIterator();}
	void RemoveAll() {OpGenericPointerSet::RemoveAll();}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpGenericPointerSet::CopyAllToVector(vector);}
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const {return OpGenericPointerSet::CopyAllToHashTable(hash_table);}

	INT32 GetCount() const {return OpGenericPointerSet::GetCount();}
	void SetMinimumCount(UINT32 minimum_count) {OpGenericPointerSet::SetMinimumCount(minimum_count);}

	void ForEach(ForEachIterator* itr) {
		struct LocalItr : public OpHashTableForEachListener {
			ForEachIterator* m_itr;
			LocalItr(ForEachIterator* itr) : m_itr(itr) {}
			virtual void HandleKeyData(const void* key, void* data)
			{ m_itr->HandleInteger(PTR_TO_INTEGRAL(INT32, key)); }
		} local_itr(itr);
		OpGenericPointerSet::ForEach(&local_itr);
	}
};

typedef OpINT32Set OpIntegerHashTable;

class OpStringHashSet : private OpStringHashTable<uni_char>
{
public:
	OpStringHashSet() : OpStringHashTable<uni_char>() {}

	OP_STATUS Add(uni_char* data) {return OpStringHashTable<uni_char>::Add(data, data);}
	BOOL Contains(const uni_char* data) const {return OpStringHashTable<uni_char>::Contains(data);}
	OP_STATUS Remove(uni_char* data) {return OpStringHashTable<uni_char>::Remove(data, &data);}

	OpHashIterator*	GetIterator() {return OpStringHashTable<uni_char>::GetIterator();}
	void RemoveAll() {OpStringHashTable<uni_char>::RemoveAll();}
	OP_STATUS CopyAllToVector(OpVector<uni_char>& vector) const {return OpStringHashTable<uni_char>::CopyAllToVector(*reinterpret_cast<OpGenericVector*>(&vector));}
	INT32 GetCount() const {return OpStringHashTable<uni_char>::GetCount();}
	void SetMinimumCount(UINT32 minimum_count) {OpStringHashTable<uni_char>::SetMinimumCount(minimum_count);}
	virtual void Delete(void* data) { OP_DELETEA(reinterpret_cast<uni_char*>(data));}
	void DeleteAll() { OpStringHashTable<uni_char>::DeleteAll();}
};

class OpConstStringHashSet : private OpStringHashTable<const uni_char>
{
public:
	OpConstStringHashSet() : OpStringHashTable<const uni_char>() {}

	OP_STATUS Add(const uni_char* data) {return OpStringHashTable<const uni_char>::Add(data, data);}
	BOOL Contains(const uni_char* data) const {return OpStringHashTable<const uni_char>::Contains(data);}
	OP_STATUS Remove(const uni_char* data) {return OpStringHashTable<const uni_char>::Remove(data, &data);}

	OpHashIterator*	GetIterator() {return OpStringHashTable<const uni_char>::GetIterator();}
	void RemoveAll() {OpStringHashTable<const uni_char>::RemoveAll();}
	OP_STATUS CopyAllToVector(OpVector<const uni_char>& vector) const {return OpStringHashTable<const uni_char>::CopyAllToVector(*reinterpret_cast<OpGenericVector*>(&vector));}
	INT32 GetCount() const {return OpStringHashTable<const uni_char>::GetCount();}
	void SetMinimumCount(UINT32 minimum_count) {OpStringHashTable<const uni_char>::SetMinimumCount(minimum_count);}
	virtual void Delete(void* data) { OP_DELETEA(reinterpret_cast<uni_char*>(data));}
	void DeleteAll() { OpStringHashTable<const uni_char>::DeleteAll();}
};

class OpAutoStringHashSet : public OpStringHashSet
{
public:
	~OpAutoStringHashSet() { DeleteAll(); }
};

#endif // !MODULES_UTIL_OPHASHTABLE_H
