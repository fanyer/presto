/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef HASHITERATOR_H
#define HASHITERATOR_H

/**
 * @brief An iterator for OpHashTables
 * @author Arjan van Leeuwen
 *
 * This is a safer iterator for an OpHashTable (no risk of leaking memory,
 * gets correct type for data and key).
 *
 * Typical usage:
 *
 * OpStringHashTable<T> table;
 * for (StringHashIterator<T> it(table); it; it++)
 * {
 *     // Do stuff with it.GetData() or it.GetKey()
 * }
 *
 */
template<class Data, class Table>
class GenericHashIterator
{
	public:
		/** Destructor
		  */
		~GenericHashIterator()
			{ OP_DELETE(m_iterator); }

		/** Get data for current position of iterator
		  */
		Data* GetData() const
			{ return static_cast<Data*>(m_iterator->GetData()); }

		/** Check if iterator is valid, call before accessing other functions
		  */
		operator BOOL() const
			{ return m_status == OpStatus::OK; }

		/** Increase iterator
		  */
		void operator++(int)
			{ m_status = m_iterator->Next(); }

	protected:
		GenericHashIterator(Table& table)
		  : m_table(table)
		  , m_iterator(table.GetIterator())
		  , m_status(m_iterator ? m_iterator->First() : OpStatus::ERR_NO_MEMORY) {}
		GenericHashIterator(const GenericHashIterator<Data, Table>& to_copy)
		  : m_table(to_copy.m_table)
		  , m_iterator(to_copy.m_table.GetIterator())
		  , m_status(m_iterator ? m_iterator->First() : OpStatus::ERR_NO_MEMORY) {}

		Table&			m_table;
		OpHashIterator* m_iterator;
		OP_STATUS		m_status;

	private:
		GenericHashIterator& operator=(const GenericHashIterator<Data, Table>&);
};


template<class K, class D, class Table>
class HashIterator : public GenericHashIterator<D, Table>
{
	public:
		/** Get key for current position of iterator
		  */
		const K* GetKey() const
			{ return static_cast<const K*>(this->m_iterator->GetKey()); }

	protected:
		HashIterator(Table& table) : GenericHashIterator<D, Table>(table) {}
		HashIterator(const HashIterator<K, D, Table>& to_copy) : GenericHashIterator<D, Table>(to_copy) {}
};


template<class K, class D>
class PointerHashIterator : public HashIterator<K, D, OpPointerHashTable<K, D> >
{
	public:
		/** Constructor
		  * @param table Table to iterate through
		  */
		PointerHashIterator(OpPointerHashTable<K, D>& table) : HashIterator<K, D, OpPointerHashTable<K, D> >(table) {}
		PointerHashIterator(const PointerHashIterator<K, D>& to_copy) : HashIterator<K, D, OpPointerHashTable<K, D> >(to_copy) {}
};


template<class T>
class StringHashIterator : public HashIterator<uni_char, T, OpStringHashTable<T> >
{
	public:
		/** Constructor
	 	  * @param table Table to iterate through
		  */
		StringHashIterator(OpStringHashTable<T>& table) : HashIterator<uni_char, T, OpStringHashTable<T> >(table) {}
		StringHashIterator(const StringHashIterator<T>& to_copy) : HashIterator<uni_char, T, OpStringHashTable<T> >(to_copy) {}
};


template<class T>
class String8HashIterator : public HashIterator<char, T, OpString8HashTable<T> >
{
	public:
		/** Constructor
		  * @param table Table to iterate through
		  */
		String8HashIterator(OpString8HashTable<T>& table) : HashIterator<char, T, OpString8HashTable<T> >(table) {}
		String8HashIterator(const String8HashIterator<T>& to_copy) : HashIterator<char, T, OpString8HashTable<T> >(to_copy) {}
};


template<class T>
class INT32HashIterator : public GenericHashIterator<T, OpINT32HashTable<T> >
{
	public:
		/** Constructor
		  * @param table Table to iterate through
		  */
		INT32HashIterator(OpINT32HashTable<T>& table) : GenericHashIterator<T, OpINT32HashTable<T> >(table) {}
		INT32HashIterator(const INT32HashIterator<T>& to_copy) : GenericHashIterator<T, OpINT32HashTable<T> >(to_copy) {}

		/** Get key for current position of iterator
		 */
		INT32 GetKey() const
			{ return (INT32)reinterpret_cast<INTPTR>(this->m_iterator->GetKey()); }
};


template<class T>
class PointerSetIterator : public GenericHashIterator<T, OpPointerSet<T> >
{
	public:
		PointerSetIterator(OpPointerSet<T>& table) : GenericHashIterator<T, OpPointerSet<T> >(table) {}
		PointerSetIterator(const PointerSetIterator<T>& to_copy) : GenericHashIterator<T, OpPointerSet<T> >(to_copy) {}
};


class INT32SetIterator : public GenericHashIterator<void, OpINT32Set>
{
	public:
		INT32SetIterator(OpINT32Set& table) : GenericHashIterator<void, OpINT32Set>(table) {}
		INT32SetIterator(const INT32SetIterator& to_copy) : GenericHashIterator<void, OpINT32Set>(to_copy) {}

		INT32 GetData() const
			{ return (INT32)reinterpret_cast<INTPTR>(this->m_iterator->GetData()); }
};


#endif // HASHITERATOR_H
