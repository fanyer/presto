/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2012 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef UTIL_STRINGHASH_H
#define UTIL_STRINGHASH_H

#include "modules/util/hash.h"

/** Simple char* -> T hash table.
 *
 * This hash table keeps copies of key strings internally, eliminating messy
 * ownership issues. It is used in (and tailored for size-wise) the WebGL
 * implementation to keep track of data for various types of identifiers.
 *
 * I'd be happy to replace this class with some "Opera STL" equivalent if that
 * ever comes along. It was introduced mainly to solve key string ownership
 * issues and get a less clunky hash table interface. /Ulf
 *
 * Limitations:
 *
 *  - No selective deletion of individual key/value pairs.
 *  - Modification during iteration is not safe in general.
 *
 */
template<class T>
class StringHash
{
public:
	StringHash() : cnt(0)
	{
		for (unsigned i = 0; i < N_BUCKETS; ++i)
			buckets[i] = NULL;
	}

	~StringHash() { clear(); }

	/** Get the data associated with a key.
	 *
	 * @param key The key string to look up.
	 * @param data Receives the data if @p key is in the table; unmodified
	 *             otherwise.
	 *
	 * @return @c true if @p key is in the table, otherwise @c false.
	 */
	bool get(const char *key, T& data) const
	{
		for (Node *node = buckets[djb2hash(key) % N_BUCKETS]; node; node = node->next)
			if (op_strcmp(node->key, key) == 0)
			{
				data = node->data;
				return true;
			}
		return false;
	}

	/** Set the data associated with a key.
	 *
	 * A copy of the key string is stored internally in the hash table, so the
	 * caller is free to deallocate the string afterwards. @p data is copied
	 * internally using its cctor and operator=().
	 *
	 * @param key The key whose associated data is to be set.
	 * @param data The associated data to set.
	 *
	 * @retval OpStatus::OK The key was successfully inserted.
	 * @retval OpStatus::ERR_NO_MEMORY The insert operation failed due to OOM.
	 *         The hash table will still be in a consistent state (without the
	 *         new key and associated data) if this happens.
	 */
	OP_STATUS set(const char *key, const T& data)
	{
		Node **nextpp;
		for (nextpp = &buckets[djb2hash(key) % N_BUCKETS]; *nextpp; nextpp = &(*nextpp)->next)
		{
			Node *node = *nextpp;
			if (op_strcmp(node->key, key) == 0)
			{
				node->data = data;
				return OpStatus::OK;
			}
		}
		RETURN_IF_ERROR(add_node(nextpp, key, data));
		++cnt;
		return OpStatus::OK;
	}

	/** Check if a string is a key of the hash table.
	 *
	 * @param key The key string to look up.
	 *
	 * @return @c true if @p key is in the table, otherwise @c false.
	 */
	bool contains(const char *key) const
	{
		for (Node *node = buckets[djb2hash(key) % N_BUCKETS]; node; node = node->next)
			if (op_strcmp(node->key, key) == 0)
				return true;
		return false;
	}

	/** Create a copy of the hash table.
	 *
	 * New copies of the key strings are allocated, so deallocating the source
	 * table afterwards is safe.
	 *
	 * @param new_sh Receives the new hash table. The prior value of the
	 *               pointer is ignored.
	 *
	 * @retval OpStatus::OK The table was successfully copied.
	 * @retval OpStatus::ERR_NO_MEMORY The copy operation failed due to OOM.
	 *         @p new_sh will not be modified if this happens.
	 */
	OP_STATUS copy(StringHash *&new_sh) const
	{
		StringHash *new_sh_local = new StringHash;
		RETURN_OOM_IF_NULL(new_sh_local);
		for (unsigned bucket = 0; bucket < N_BUCKETS; ++bucket)
		{
			Node *read_node;
			Node **write_node;
			for (read_node = buckets[bucket], write_node = &new_sh_local->buckets[bucket];
			     read_node;
			     read_node = read_node->next, write_node = &(*write_node)->next)
			{
				if (OpStatus::IsMemoryError(add_node(write_node, read_node->key, read_node->data)))
				{
					OP_DELETE(new_sh_local);
					return OpStatus::ERR_NO_MEMORY;
				}
			}
		}
		new_sh_local->cnt = cnt;

		new_sh = new_sh_local;
		return OpStatus::OK;
	}

	/** Get the number of entries in the hash table.
	 *
	 * @return The number of keys with associated data that appear in the table.
	 */
	unsigned count() const { return cnt; }

	/** Remove all key/value pairs from the hash table.
	 *
	 * Leaves the hash table in the same state as a newly created hash table.
	 */
	void clear()
	{
		for (unsigned i = 0; i < N_BUCKETS; ++i)
		{
			for (Node *node = buckets[i]; node;)
			{
				Node *next = node->next;
				op_free(node->key);
				OP_DELETE(node);
				node = next;
			}
			buckets[i] = NULL;
		}
		cnt = 0;
	}

private:
	enum { N_BUCKETS = 31 };

	struct Node
	{
		char *const key;
		T data;
		Node *next;

		Node(char *key, const T& data)
		    : key(key)
		    , data(data)
		    , next(NULL) {}
	};

	Node *buckets[N_BUCKETS];

	static OP_STATUS add_node(Node **location, const char *key, const T& data)
	{
		char *key_copy = op_strdup(key);
		RETURN_OOM_IF_NULL(key_copy);
		Node *new_node = OP_NEW(Node, (key_copy, data));
		if (!new_node)
		{
			op_free(key_copy);
			return OpStatus::ERR_NO_MEMORY;
		}
		// Make sure we are overwriting a null pointer
		OP_ASSERT(!*location);
		*location = new_node;
		return OpStatus::OK;
	}

	unsigned cnt;

public:
	class Iterator;
	friend class Iterator;

	/** StringHash iterator.
	 *
	 * Iterator class for stepping through all key/value pairs in a StringHash.
	 * Modifying the hash table during iteration is not safe in general.
	 *
	 * Example usage:
	 *
	 * StringHash<int> hash;
	 * <populate hash>
	 * StringHash<int>::Iterator iter = hash.get_iterator();
	 * const char *key;
	 * int value;
	 * while (iter.get_next(key, value))
	 *     <do something with 'key' and 'value'>
	 */
	class Iterator
	{
		friend class StringHash;
	private:
		explicit Iterator(Node **buckets)
		     : buckets(buckets)
		     , current_bucket(-1)
		     , next_node(NULL) {}

	public:

		/** Get the next key/value pair from the table.
		 *
		 * @param key Receives the key. Unmodified if no more key/value pairs exist
		 *            in the table.
		 *
		 * @param data Receives the associated data. Unmodified if no more
		 *             key/value pairs exist in the table.
		 *
		 * @retval true key/data received the next key/value pair from the table.
		 * @retval false No more key/value pairs exist in the table.
		 */
		bool get_next(const char *&key, T& data)
		{
			while (!next_node)
			{
				if (current_bucket == N_BUCKETS - 1)
					return false;
				++current_bucket;
				next_node = buckets[current_bucket];
			}

			key = next_node->key;
			data = next_node->data;
			next_node = next_node->next;

			return true;
		}
	private:
		Node **buckets;
		int current_bucket;
		typename StringHash::Node *next_node;
	};

	/** Get an iterator for stepping through all key/value pairs in the table.
	 *
	 * Modifying the table during iteration is not safe in general.
	 */
	Iterator get_iterator() { return Iterator(buckets); }
};

#endif // UTIL_STRINGHASH_H
