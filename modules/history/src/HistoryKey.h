/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 * psmaas - Patricia Aas
 */

#ifndef CORE_HISTORY_MODEL_KEY_H
#define CORE_HISTORY_MODEL_KEY_H

#ifdef HISTORY_SUPPORT

#include "modules/history/src/OpSplayTree.h"

class HistoryKey 
	: public LexSplayKey
{
public:
    /**
       @param key that this object should store
       @return a stripped key if it could be created correctly, 
       returns 0 otherwise
    */
    static HistoryKey * Create(const OpStringC& key);
	
    /**
       Should only be called when InUse returns FALSE
    */
    virtual ~HistoryKey();

	/**
       @return pointer to start of key buffer
    */
	virtual const uni_char * GetKey() const { return m_key.CStr(); }

#ifdef HISTORY_DEBUG
	static INT m_number_of_keys;
	static OpVector<HistoryKey> keys;
	static OpVector<HistoryKey> & GetKeys() { return keys; }
	virtual int GetSize() { return GetKeyLength() + sizeof(int) + sizeof(void*);}
#endif //HISTORY_DEBUG

private:

    HistoryKey(); 
	int GetKeyLength() const { return m_key.Length() * sizeof(uni_char); }

    OpString   m_key;
};

template<class InternalType, class LeafType>
class InternalKeyGuard
{
public:

	InternalKeyGuard(OpSplayTree<InternalType, LeafType>* tree)
		: m_key(0),
		  m_tree(tree), 
		  m_created(FALSE) {}

	~InternalKeyGuard()
	{
		if(m_created && m_tree && m_key)
		{
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL item_was_removed_from_tree =
#endif // DEBUG_ENABLE_OPASSERT
				m_tree->RemoveInternalNode(m_key->GetKey());
			OP_ASSERT(item_was_removed_from_tree == FALSE);
			OP_DELETE(m_key);
		}
	}

	OP_STATUS createKey(const OpStringC & stripped)
	{
		m_key = HistoryKey::Create(stripped);

		if(!m_key)
			return OpStatus::ERR_NO_MEMORY;

		m_created = TRUE;
		return OpStatus::OK;
	}

	void findKey(const OpStringC & stripped)
	{
	    m_key = static_cast<HistoryKey *>(m_tree->GetKey(stripped.CStr()));
		m_created = FALSE;
	}

	HistoryKey* getKey()
	{
		return m_key;
	}

	InternalType** insertKey()	
	{
		return m_tree->InsertInternalNode(m_key);
	}

	void release()
	{
		m_key  = 0;
		m_tree = 0;
	}

private:

	HistoryKey* m_key;
	OpSplayTree<InternalType, LeafType>* m_tree;
	BOOL m_created;
};

template<class InternalType, class LeafType>
class LeafKeyGuard
{
public:

	LeafKeyGuard(OpSplayTree<InternalType, LeafType>* tree, INT32 prefix_num)
		: m_key(0),
		  m_tree(tree), 
		  m_prefix_num(prefix_num),
		  m_created(FALSE) { }

	~LeafKeyGuard()
	{
		if(m_created && m_tree && m_key)
		{
#ifdef DEBUG_ENABLE_OPASSERT
			BOOL item_was_removed_from_tree = 
#endif // DEBUG_ENABLE_OPASSERT
				m_tree->RemoveLeafNode(m_key->GetKey(), m_prefix_num);
			OP_ASSERT(item_was_removed_from_tree == FALSE);
			OP_DELETE(m_key);
		}
	}

	void release()
	{
		m_key        = 0;
		m_tree       = 0;
		m_prefix_num = 0;
		m_created    = FALSE;
	}

	OP_STATUS createKey(const OpStringC & stripped)
	{
		m_key = HistoryKey::Create(stripped);

		if(!m_key)
			return OpStatus::ERR_NO_MEMORY;

		m_created = TRUE;
		return OpStatus::OK;
	}

	void findKey(const OpStringC & stripped)
	{
	    m_key = static_cast<HistoryKey *>(m_tree->GetKey(stripped.CStr()));
		m_created = FALSE;
	}

	HistoryKey* getKey()
	{
		return m_key;
	}

	LeafType** insertKey()	
	{
		return m_tree->InsertLeafNode(m_key, m_prefix_num);
	}

private:

	HistoryKey* m_key;
	OpSplayTree<InternalType, LeafType>* m_tree;
	INT32 m_prefix_num;
	BOOL m_created;
};

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_KEY_H

