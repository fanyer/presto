/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2009 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 */

#ifndef DEVICEAPI_OPSTRINGHASHMULTIMAP_H
#define DEVICEAPI_OPSTRINGHASHMULTIMAP_H

/** This class implements a MultiMap - map with repetition.
 *
 * Internally it uses OpGenericStringHashTable as implementation
 * but exposes a slightly different interface which is customised
 * more for storing multiple values mapped to one key
 */
template<class ElementType>
class OpStringHashMultiMap : private OpGenericStringHashTable
{
public:
  // Treat as if it was private, BREW doesn't allow it to be private :(
	struct DataElement
	{
		OpString key;
		OpVector<ElementType> elements;
	};

	OpStringHashMultiMap(BOOL case_sensitive)
		: OpGenericStringHashTable(case_sensitive)
	{
	}

	/**
	 * Adds a key with data to the hash table. If the key already existed it gets another value.
	 *
	 * @return
	 * - OpStatus::OK if we could add the key and data.
	 * - OpStatus::ERR_NO_MEMORY if we could not add because of OOM.
	 */
	OP_STATUS Add(const uni_char* key, ElementType* data)
	{
		DataElement* element;
		OP_STATUS status = OpGenericStringHashTable::GetData(key, reinterpret_cast<void**>(&element));
		if (status == OpStatus::ERR)
		{
			OpAutoPtr<DataElement> element_deleter;
			RETURN_OOM_IF_NULL(element = OP_NEW(DataElement, ()));
			element_deleter.reset(element);
			RETURN_IF_ERROR(element->key.Set(key));
			RETURN_IF_ERROR(OpGenericStringHashTable::Add(element->key.CStr(), element));
			element_deleter.release();
		}
		return element->elements.Add(data);
	}

	/**
	 * Gets a number of elements mapped to a one key. If a key is not present in map 0 is returned.
	 */
	UINT32 GetElementCount(const uni_char* key) const
	{
		DataElement* element;
		OP_STATUS status = OpGenericStringHashTable::GetData(key, reinterpret_cast<void**>(&element));
		if (status == OpStatus::ERR)
			return 0;
		else
			return element->elements.GetCount();
	}

	/**
	 * Gets a number of key in the map.
	 */
	UINT32 GetKeyCount() const
	{
		return OpGenericStringHashTable::GetCount();
	}

	/**
	 * Gets a n-th value mapped to a key.
	 *
	 * @param key - key at which data is stored.
	 * @param index - index of the value stored at a key.
	 * @param [out]data - requested data will be set here
	 * @return
	 *  - OK - if succeeded
	 *  - ERR - there is no such key or the index was higher
	 *          than the last index for data at thet key.
	 */
	OP_STATUS GetData(const uni_char* key, UINT32 index, ElementType** data) const
	{
		DataElement* element;
		RETURN_IF_ERROR(OpGenericStringHashTable::GetData(key, reinterpret_cast<void**>(&element)));
		*data = element->elements.Get(index);
		if (!*data)
			return OpStatus::ERR;
		return OpStatus::OK;
	}

	/**
	 * Gets all the values mapped to a key.
	 *
	 * @param key - key at which data is stored.
	 * @param [out]data - requested data pointers will be put.
	 *   The data which was stored in this vector will be discarded.
	 * @return
	 *  - OK - if succeeded
	 *  - ERR - there is no such key.
	 *  - ERR_NO_MEMORY - copying data pointers to the vector oom-ed.
	 */
	OP_STATUS GetData(const uni_char* key, OpVector<ElementType>* data) const
	{
		DataElement* element;
		RETURN_IF_ERROR(OpGenericStringHashTable::GetData(key, reinterpret_cast<void**>(&element)));
		return data->DuplicateOf(element->elements);
	}

	/**
	 * Removes a n-th value mapped to a key(doesn't delete it).
	 *
	 * @param key - key at which data is stored.
	 * @param index - index of the value stored at a key.
	 * @param [out]data - removed data will be set here.
	 * @return
	 *  - OK - if succeeded
	 *  - ERR - there is no such key or the index was higher
	 *          than the last index for data at thet key.
	 */
	OP_STATUS Remove(const uni_char* key, UINT32 index, ElementType** data)
	{
		DataElement* element;
		RETURN_IF_ERROR(OpGenericStringHashTable::GetData(key, reinterpret_cast<void**>(&element)));
		*data = element->elements.Remove(index);
		if (!*data)
			return OpStatus::ERR;
		return OpStatus::OK;
	}

	/**
	 * Deletes a n-th value mapped to a key.
	 *
	 * @param key - key at which data is stored.
	 * @param index - index of the value stored at a key.
	 * @return
	 *  - OK - if succeeded
	 *  - ERR - there is no such key or the index was higher
	 *          than the last index for data at thet key.
	 */
	OP_STATUS Delete(const uni_char* key, UINT32 index)
	{
		ElementType* data;
		RETURN_IF_ERROR(Remove(key, index, &data));
		OP_DELETE(data);
		return OpStatus::OK;
	}

	/** Deletes all data in the map */
	void DeleteAll()
	{
		OpGenericStringHashTable::ForEach(FullDeleteFunc);
		OpGenericStringHashTable::RemoveAll();
	}

	/** Removes all data in the map */
	void RemoveAll()
	{
		OpGenericStringHashTable::ForEach(ShallowDeleteFunc);
		OpGenericStringHashTable::RemoveAll();
	}

	~OpStringHashMultiMap()
	{
		RemoveAll();
	}

	class MultiMapIterator
	{
		friend class OpStringHashMultiMap;
	public:
		~MultiMapIterator(){ OP_DELETE(m_iterator); }
		/** Sets the iterator to the first element in the map
		 * @return
		 * - OK - if suceeded.
		 * - ERR - no elements in the map
		 */
		OP_STATUS First() { return m_iterator ? m_iterator->First() : OpStatus::ERR_NO_MEMORY; }
		/** Sets the iterator to the next element in the map
		 * @return
		 * - OK - if suceeded.
		 * - ERR - no elements in the map
		 */
		OP_STATUS Next() { return m_iterator ? m_iterator->Next() : OpStatus::ERR_NO_MEMORY; }
		/** Gets the current key */
		const uni_char* GetKey() { return m_iterator ? reinterpret_cast<DataElement*>(m_iterator->GetData())->key.CStr() : NULL; }
		/** Gets all the the values at current key. the vector returned IS the vector
		 *stored in the map so any modifications to it will affect also contents of the map
		 */
		OpVector<ElementType>* GetValues() { return m_iterator ? &reinterpret_cast<DataElement*>(m_iterator->GetData())->elements : NULL; }
	private:
		MultiMapIterator(OpHashIterator* iterator) : m_iterator(iterator) { OP_ASSERT(m_iterator); }
		OpHashIterator* m_iterator;
		// private copy operators to prevent copying
		MultiMapIterator(MultiMapIterator &iterator);
		MultiMapIterator& operator=(MultiMapIterator& MultiMapIterator);
	};

	/** Gets the iterator for multi map.
	 * It's use is analogous to to normal OpHashIterator
	 */
	MultiMapIterator* GetIterator()
	{
		OpHashIterator* iterator = OpGenericStringHashTable::GetIterator();
		if (!iterator)
			return NULL;
		MultiMapIterator* retval = OP_NEW(MultiMapIterator, (iterator));
		if (!retval)
			OP_DELETE(iterator);
		return retval;
	}
private:
	virtual void Delete(void* /*data*/) {OP_ASSERT(0);}  // must be overloaded to be able to delete

	static void FullDeleteFunc(const void * key, void * elem, OpHashTable * table)
	{
		DataElement* data_element = reinterpret_cast<DataElement*>(elem);
		data_element->elements.DeleteAll();
		OP_DELETE(data_element);
	}

	static void ShallowDeleteFunc(const void * key, void * elem, OpHashTable * table)
	{
		DataElement* data_element = reinterpret_cast<DataElement*>(elem);
		OP_DELETE(data_element);
	}
};

#endif // DEVICE_API_OP_STRING_HASH_MULTIMAP_H
