/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef	REG_KEY_ITERATOR_H
#define REG_KEY_ITERATOR_H

#ifdef MSWIN

/**
 * For iterating over the subkeys of a registry key.
 *
 * Iteration has no particular order.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class RegKeyIterator
{
public:
	RegKeyIterator();
	~RegKeyIterator();
	
	/**
	 * Initializes the iterator to point to the first subkey of the given key,
	 * or to be AtEnd() if the given key has no subkeys.
	 *
	 * @param ancestor_handle a handle to an ancestor of the key.  The ancestor
	 * 		key must have been opened previously.
	 * @param key_name the name of the key, relative to @a root_handle
	 * @param key_handle on success, receives the handle to the key once it's
	 * 		opened.  Probably only useful in tests.
	 * @return an error code indicates failure to open the key, etc.
	 * 		Specifically, the return value is OpStatus::OK if the key is
	 * 		accessed successfully, but has no subkeys.
	 */
	OP_STATUS Init(HKEY ancestor_handle,
			const OpStringC& key_name = OpStringC(),
			HKEY* key_handle = NULL);

	/**
	 * Determines if the iterator has reached the end of the subkey list.
	 *
	 * @return @c TRUE iff the iterator is at the end of the subkey list
	 */
	BOOL AtEnd() const;

	/**
	 * Retrieves the name of the subkey at the current iteration point.  The
	 * subkey name is relative to the ancestor used to initialize the iterator.
	 *
	 * @param subkey_name on success, receives the subkey name
	 * @return status
	 */
	OP_STATUS GetCurrent(OpString& subkey_name) const;

	/**
	 * Advances the iterator to the next subkey in the list.  Does nothing if
	 * there are no more subkeys.
	 */
	void Next();

private:
	// Non-copyable.
	RegKeyIterator(const RegKeyIterator&);
	RegKeyIterator& operator=(const RegKeyIterator&);

	BOOL AtSubkey() const;
	void CleanUp();

	static const HKEY NO_HANDLE;
	static const INT32 NO_INDEX = -1;
	static const INT32 END_INDEX = -2;
	static const size_t MAX_KEY_NAME_LENGTH = 255;

	HKEY m_ancestor_handle;
	HKEY m_key_handle;
	OpString m_key_name;
	INT32 m_subkey_index;
	OpString m_subkey_name;
};

#endif // MSWIN
#endif // REG_KEY_ITERATOR_H
