/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_OPHASHTABLE_H
#define MODULES_UTIL_OPHASHTABLE_H

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

class PointerHashFunctions : public OpHashFunctions
{
public:
	/**
	 * Creates a hashfunctions object that can hash and compare pointers.
	 */
	PointerHashFunctions() {}

	virtual UINT32 Hash(const void* key);
	virtual BOOL KeysAreEqual(const void* key1, const void* key2);
};

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

class HashBackend
{
public:
	HashBackend(OpHashFunctions* hashFunctions) : hash_functions(hashFunctions) {}

	virtual ~HashBackend() {}

	/**
	 * Initialize the HashBackend that can hold a number of elements, and that
	 * has a specified array size (the ratio defines the density of the hash table, performance related).
	 * Called from OpHashTable.
	 * @param array_size size of the array that is used for spreading the hash values.
	 * @param max_nr_of_elements nr of elements that the hash table can hold.
	 * @return OpStatus::OK, everything went well.
	 * @return OpStatus::ERR_NO_MEMORY, out of memory.
	 */
	virtual OP_STATUS Init(UINT32 array_size, UINT32 max_nr_of_elements) = 0;

	/**
	 * Adds a key and data to the HashBackend.
	 * Called from OpHashTable.
	 * @param key the key to add.
	 * @param data the data to add.
	 * @return OpStatus::OK, if the element was added.
	 * @return OpStatus::ERR, if the element was already added.
	 */
	virtual OP_STATUS Add(const void* key, void* data) = 0;

	/**
	 * Get the data corresponding to a key.
	 * Called from OpHashTable.
	 * @param key the key for the element to get.
	 * @param data where the found data is placed.
	 * @return OpStatus::OK, if the element was found.
	 * @return OpStatus::ERR, if the element was not in the HashBackend.
	 */
	virtual OP_STATUS GetData(const void* key, void** data) const = 0;

	/**
	 * Remove the key/value pair corresponding to the key.
	 * Called from OpHashTable.
	 * @param key the key in the key/value pair that should be removed.
	 * @param data where the removed data is placed.
	 * @return OpStatus::OK, if the element was removed.
	 * @return OpStatus::ERR, if the element was not in the list.
	 */
	virtual OP_STATUS Remove(const void* key, void** data) = 0;

	/**
	 * Get an iterator to iterate the HashBackend.
	 * The returned iterator is to be freed by the caller.
	 * Called from OpHashTable.
	 * @return the iterator for the HashBackend, or NULL if OOM.
	 */
	virtual OpHashIterator* GetIterator() const = 0;

	/**
	 * Call function for each (key, data) pair in the hash table.
	 */
	virtual void ForEach(void (*function)(const void *, void *)) = 0;

	virtual void ForEach(void (*function)(const void *, void *, OpHashTable *), OpHashTable* table) = 0;

	/**
	 * Add all data elements to an OpGenericVector (the order is random, ie. how they appear in hash table)
	 */
	virtual OP_STATUS CopyAllToVector(OpGenericVector& vector) const = 0;

	/**
	 * Add all data elements to another OpHashTable
	 */
	virtual OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const = 0;

protected:
	OpHashFunctions* hash_functions;
};

class ChainedHashBackend : public HashBackend
{
public:
	ChainedHashBackend(OpHashFunctions* hashFunctions);
	virtual ~ChainedHashBackend();

	virtual OP_STATUS Init(UINT32 array_size, UINT32 max_nr_of_elements);
	virtual OP_STATUS Add(const void* key, void* data);
	virtual OP_STATUS GetData(const void* key, void** data) const;
	virtual OP_STATUS Remove(const void* key, void** data);
	virtual OpHashIterator* GetIterator() const;
	virtual void ForEach(void (*function)(const void *, void *));
	virtual void ForEach(void (*function)(const void *, void *, OpHashTable *), OpHashTable* table);
	virtual OP_STATUS CopyAllToVector(OpGenericVector& vector) const;
	virtual OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const;

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

private:
	UINT32 Hash(const void* key, UINT32 tableSize) const;
	ChainedHashLink* GetNewLink();
	void FreeLink(ChainedHashLink* link);
	BOOL FindElm(UINT32 array_pos, const void* key, ChainedHashLink** link, ChainedHashLink** prev_link) const;
	OP_STATUS Iterate(ChainedHashIterator* iterator, UINT32 start_pos) const;

	UINT32 table_size;
	UINT32 nr_of_hash_links;
	ChainedHashLink** hash_array;
	ChainedHashLink* hash_links;
	ChainedHashLink* first_free_link;
};

class OpHashTable
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

	/**
	 * Remove all elements
	 */
	void RemoveAll();

	/**
	 * Delete all elements.. to use this, you must overload Delete(void* data)
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

protected:

	virtual void Delete(void* /*data*/) {OP_ASSERT(0);}  // must be overloaded to be able to delete

private:
	OP_STATUS Init();

	BOOL IsInitialized() const;
	BOOL IsChained() const;
	void SetInitialized(BOOL initialized);
	void SetChained(BOOL chained);

	HashBackend* CreateBackend(UINT16 size_index);
	OP_STATUS Rehash(UINT16 size_index);

	OpHashFunctions* hash_functions;
	HashBackend* hash_backend;
	UINT32 nr_of_elements;
	UINT32 minimum_nr_of_elements;
	UINT16 hash_size_index;
	UINT16 flags;

	static void DeleteFunc(const void *key, void *data, OpHashTable* table);
};

class OpGenericStringHashTable : protected OpHashTable, public OpHashFunctions
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
 
		void RemoveAll() {OpHashTable::RemoveAll();}
		void DeleteAll() {OpHashTable::DeleteAll();}

		BOOL Contains(const uni_char* key) const {return OpHashTable::Contains((void*)key);}

		INT32 GetCount() const {return OpHashTable::GetCount();}

		// expose this to the outside world
		static UINT32 HashString(const uni_char* key, BOOL case_sensitive);

		// OpHashFunctions interface

		virtual UINT32 Hash(const void* key);
		virtual BOOL KeysAreEqual(const void* key1, const void* key2);

	private:

		BOOL m_case_sensitive;
};

class OpGenericString8HashTable : protected OpHashTable, public OpHashFunctions
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
 
		void RemoveAll() {OpHashTable::RemoveAll();}
		void DeleteAll() {OpHashTable::DeleteAll();}

		BOOL Contains(const char* key) const {return OpHashTable::Contains((void*)key);}

		INT32 GetCount() const {return OpHashTable::GetCount();}

		// expose this to the outside world
		static UINT32 HashString(const char* key, BOOL case_sensitive);

		// OpHashFunctions interface

		virtual UINT32 Hash(const void* key);
		virtual BOOL KeysAreEqual(const void* key1, const void* key2);

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
		void RemoveAll() {OpGenericStringHashTable::RemoveAll();}
		void DeleteAll() {OpGenericStringHashTable::DeleteAll();}

		virtual void Delete(void* data) {delete (T*) data;}

		INT32 GetCount() const {return OpGenericStringHashTable::GetCount();}
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
		void RemoveAll() {OpGenericString8HashTable::RemoveAll();}
		void DeleteAll() {OpGenericString8HashTable::DeleteAll();}

		virtual void Delete(void* data) {delete (T*) data;}

		INT32 GetCount() const {return OpGenericString8HashTable::GetCount();}
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
class OpINT32HashTable : private OpHashTable
{
public:

	OpINT32HashTable() : OpHashTable() {}

	OP_STATUS Add(INT32 key, T* data) {return OpHashTable::Add((void*)key, (void*)data);}

	OP_STATUS GetData(INT32 key, T** data) const {return OpHashTable::GetData((void*)key, (void**)data);}

	BOOL Contains(INT32 key) const {return OpHashTable::Contains((void*)key);}

	OP_STATUS Remove(INT32 key, T** data) {return OpHashTable::Remove((void*)key, (void**)data);}

	OpHashIterator*	GetIterator() {return OpHashTable::GetIterator();}

	void ForEach(void (*function)(const void *, void *)) {OpHashTable::ForEach(function);}
	void ForEach(void (*function)(const void *, void *, OpHashTable *)) {OpHashTable::ForEach(function);}
	void RemoveAll() {OpHashTable::RemoveAll();}
	void DeleteAll() {OpHashTable::DeleteAll();}

	virtual void Delete(void* data) {delete (T*) data;}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpHashTable::CopyAllToVector(vector);}
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const {return OpHashTable::CopyAllToHashTable(hash_table);}
	INT32 GetCount() const {return OpHashTable::GetCount();}
	void SetMinimumCount(UINT32 minimum_count) {OpHashTable::SetMinimumCount(minimum_count);}
};

template <class T>
class OpAutoINT32HashTable : public OpINT32HashTable<T>
{
public:
	OpAutoINT32HashTable() : OpINT32HashTable<T>() { }
	virtual ~OpAutoINT32HashTable() { OpINT32HashTable<T>::DeleteAll(); }
};

template <class KeyType, class DataType>
class OpPointerHashTable : private OpHashTable
{
public:
	OpPointerHashTable() : OpHashTable() {}

	OP_STATUS Add(KeyType* key, DataType* data) { return OpHashTable::Add((void *)(key), (void *)(data)); }
	OP_STATUS Remove(KeyType* key, DataType** data) { return OpHashTable::Remove((void *)(key), (void **)(data)); }

	OP_STATUS GetData(KeyType* key, DataType** data) const { return OpHashTable::GetData((void *)(key), (void **)(data)); }
	BOOL Contains(KeyType* key) const { return OpHashTable::Contains((void *)(key)); }

	OpHashIterator*	GetIterator() { return OpHashTable::GetIterator(); }

	void ForEach(void (*function)(const void *, void *)) { OpHashTable::ForEach(function); }
	void ForEach(void (*function)(const void *, void *, OpHashTable *)) { OpHashTable::ForEach(function); }
	void RemoveAll() { OpHashTable::RemoveAll(); }
	void DeleteAll() { OpHashTable::DeleteAll(); }

	virtual void Delete(void* data) { delete (DataType *)(data); }

	INT32 GetCount() const { return OpHashTable::GetCount(); }
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
	void RemoveAll() {OpGenericPointerSet::RemoveAll();}
	void DeleteAll() {OpGenericPointerSet::DeleteAll();}

	virtual void Delete(void* data) {delete (T*) data;}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpGenericPointerSet::CopyAllToVector(vector);}
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const {return OpGenericPointerSet::CopyAllToHashTable(hash_table);}

	INT32 GetCount() const {return OpHashTable::GetCount();}
};

class OpINT32Set : private OpGenericPointerSet
{
public:

	OpINT32Set() : OpGenericPointerSet() {}

	OP_STATUS Add(INT32 data) {return OpGenericPointerSet::Add((void*)data);}
	BOOL Contains(INT32 data) const {return OpGenericPointerSet::Contains((void*)data);}
	OP_STATUS Remove(INT32 data) {return OpGenericPointerSet::Remove((void*)data);}

	OpHashIterator*	GetIterator() {return OpGenericPointerSet::GetIterator();}
	void RemoveAll() {OpGenericPointerSet::RemoveAll();}

	OP_STATUS CopyAllToVector(OpGenericVector& vector) const {return OpGenericPointerSet::CopyAllToVector(vector);}
	OP_STATUS CopyAllToHashTable(OpHashTable& hash_table) const {return OpGenericPointerSet::CopyAllToHashTable(hash_table);}

	INT32 GetCount() const {return OpGenericPointerSet::GetCount();}
	void SetMinimumCount(UINT32 minimum_count) {OpGenericPointerSet::SetMinimumCount(minimum_count);}
};

typedef OpINT32Set OpIntegerHashTable;

#endif // !MODULES_UTIL_OPHASHTABLE_H
