/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
** psmaas - Patricia Aas
*/

#ifndef CORE_HISTORY_MODEL_PREFIX_FOLDER_H
#define CORE_HISTORY_MODEL_PREFIX_FOLDER_H

#ifdef HISTORY_SUPPORT

#include "modules/history/OpHistoryFolder.h"

class PrefixMatchNode
	: public OpTypedObject
{
public:
	PrefixMatchNode() :
		m_id(OpTypedObject::GetUniqueID()) {}
	
	void Init(INT index,
			  HistoryKey * key)
		{
			m_index = index;
			m_key   = 0;
			SetKey(key);
		}
	
	virtual ~PrefixMatchNode()
		{
			SetKey(0);
		}
	
	void SetKey(HistoryKey * key)
		{
			if(key)
				key->Increment();
			
			if(m_key)
				m_key->Decrement();
			
			if(m_key && !m_key->InUse())
				OP_DELETE(m_key);
			
			m_key = key;
		}
	
	virtual const LexSplayKey * GetKey()   const { return m_key; }
	virtual Type                GetType()  { return FOLDER_TYPE; }
	virtual INT32               GetID()    { return m_id; }
	INT                         GetIndex() { return m_index; }
	
	INT m_index;
	INT32 m_id;
	HistoryKey* m_key;
};

//___________________________________________________________________________
//                         CORE HISTORY MODEL PREFIX FOLDER
//___________________________________________________________________________

class HistoryPrefixFolder
	: public HistoryFolder
{
    friend class HistoryModel;
	friend class HistoryPage;
	friend class HistoryFolder;

public:

	/*
	 * Public static Create method - will return 0 if page could not be created.
	 *
	 * @param title
	 * @param index
	 * @param key
	 * @param prefix_key
	 * @param suffix_key
	 *
	 * @return
	 **/
    static HistoryPrefixFolder* Create(const OpStringC& title,
									   INT32 index,
									   HistoryKey * key,
									   HistoryKey * prefix_key,
									   HistoryKey * suffix_key);

	/*
	 * @return
	 **/
    INT32 GetIndex() const { return m_index; }

	/*
	 * @return
	 **/
    BOOL GetContained() const { return m_contained; }

	/*
	 * @return
	 **/
    virtual BOOL IsDeletable() const { return FALSE; }

	/*
	 * @return
	 **/
    virtual HistoryFolderType GetFolderType() const { return PREFIX_FOLDER; }

	/*
	 * @param value
	 **/
    void SetContained(BOOL value) { m_contained = value; }

	/*
	 * @return
	 **/
	PrefixMatchNode & GetPrefixNode() { return m_prefix_node; }

	/*
	 * @return
	 **/
	PrefixMatchNode & GetSuffixNode() { return m_suffix_node; }

#ifdef HISTORY_DEBUG
	virtual int GetSize() { return sizeof(HistoryPrefixFolder) + GetTitleLength();}
#endif //HISTORY_DEBUG

    virtual ~HistoryPrefixFolder() {}

private:

	/*
	 * Private constructor - use public static Create method for memory safety
	 **/
    HistoryPrefixFolder(INT32 index,
						HistoryKey * key,
						HistoryKey * prefix_key,
						HistoryKey * suffix_key);

	const uni_char * GetPrefix();

    INT32  m_index;
    BOOL   m_contained;
	PrefixMatchNode m_prefix_node;
	PrefixMatchNode m_suffix_node;
};

typedef HistoryPrefixFolder CoreHistoryModelPrefixFolder;  // Temorary typedef to old class name, so old code continues to compile

#endif // HISTORY_SUPPORT
#endif // CORE_HISTORY_MODEL_PREFIX_FOLDER_H
