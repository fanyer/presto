/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "oplist.h"

class OpGenericListItem
{
public:

	void* m_object;
	OpGenericListItem* m_next;
};

/***********************************************************************************
**
** OpListItemFreeStore - Constructor
**
***********************************************************************************/
OpListItemFreeStore::OpListItemFreeStore() : m_items(NULL) {}


/***********************************************************************************
**
** OpListItemFreeStore::Alloc
**
***********************************************************************************/
OpGenericListItem* OpListItemFreeStore::Alloc()
{
	return OP_NEW(OpGenericListItem, ());
}


/***********************************************************************************
**
** OpListItemFreeStore::Free
**
***********************************************************************************/
void OpListItemFreeStore::Free(OpGenericListItem* item)
{
	OP_DELETE(item);
}


OpListItemFreeStore OpGenericList::m_listitems;

/***********************************************************************************
**
** OpGenericList - Constructor
**
***********************************************************************************/
OpGenericList::OpGenericList()
		: m_first(NULL),
		  m_last(NULL)
{}


/***********************************************************************************
**
** OpGenericList - Destructor
**
***********************************************************************************/
OpGenericList::~OpGenericList()
{
	while (GetFirst() != NULL)
	{
		OP_ASSERT(m_last == NULL || m_last->m_next == NULL);
		RemoveFirst();
		OP_ASSERT(m_last == NULL || m_last->m_next == NULL);
	}
}


/***********************************************************************************
**
** OpGenericList::RemoveFirst
**
***********************************************************************************/
void* OpGenericList::RemoveFirst()
{
	OP_ASSERT(m_last == NULL || m_last->m_next == NULL);

	if (m_first == NULL)
	{
		return NULL;
	}

	OpGenericListItem* candidate = m_first;
	void* object = candidate->m_object;

	m_first = m_first->m_next;

	m_listitems.Free(candidate);
	m_last = (m_first == NULL ? NULL : m_last);

	OP_ASSERT(m_last == NULL || m_last->m_next == NULL);

	return object;
}


/***********************************************************************************
**
** OpGenericList::AddLast
**
***********************************************************************************/
OP_STATUS OpGenericList::AddLast(void* object)
{
	OP_NEW_DBG("OpGenericList::AddLast", "util.adt");

	OP_ASSERT(m_last == NULL || m_last->m_next == NULL);

	OpGenericListItem* item = m_listitems.Alloc();

	if (item == NULL)
	{
		return OpStatus::ERR;
	}

	item->m_next = NULL;
	item->m_object = object;

	if (m_last != NULL)
	{
		m_last->m_next = item;
	}

	m_last  = item;
	m_first = (m_first == NULL ? item : m_first);

	OP_DBG(("added last: %x", m_last));

	return OpStatus::OK;
}


/***********************************************************************************
**
** OpGenericList::IsEmpty
**
***********************************************************************************/
bool OpGenericList::IsEmpty() const
{
	OP_NEW_DBG("OpGenericList::IsEmpty", "util.adt");
	OP_DBG(("m_first= %x", m_first));
	return m_first == NULL;
}


/***********************************************************************************
**
** OpGenericIterableList - Constructor
**
***********************************************************************************/
OpGenericIterableList::OpGenericIterableList()
		: m_current(NULL),
		  m_prev(NULL)
{}


/***********************************************************************************
**
** OpGenericIterableList::Begin
**
***********************************************************************************/
void* OpGenericIterableList::Begin()
{
	m_current = GetFirst();
	m_prev    = NULL;

	return GetCurrentItem();
}

/***********************************************************************************
**
** OpGenericIterableList::Next
**
***********************************************************************************/
void* OpGenericIterableList::Next()
{
	m_prev    = m_current;
	m_current = m_current == NULL ? NULL : m_current->m_next;

	return GetCurrentItem();
}

/***********************************************************************************
**
** OpGenericIterableList::RemoveCurrentItem
**
***********************************************************************************/
void* OpGenericIterableList::RemoveCurrentItem()
{
	OP_NEW_DBG("OpGenericIterableList::RemoveCurrentItem", "util.adt");

	if (m_current == NULL)
	{
		return NULL;
	}

	OpGenericListItem* candidate = m_current;
	OpGenericListItem* next      = candidate->m_next;
	OpGenericListItem* prev      = m_prev;

	if (prev != NULL)
	{
		prev->m_next = next;
	}

	if (candidate == m_first)
	{
		m_first = next;
	}

	if (candidate == m_last)
	{
		m_last = prev;
	}

	/* what is the definied value for m_current after
	   this operation?

	   either:

	   1) m_current = NULL
	   2) m_current = next item if exists, else last item

	*/

	m_current = NULL;

	/*
	  if (next != NULL)
	  {
	  m_current = next;
	  }
	  else
	  {
	  m_current = last;
	  }
	*/

	void* result = candidate->m_object;

	OP_DBG(("removing item: %x", candidate));

	m_listitems.Free(candidate);

	return result;
}

/***********************************************************************************
**
** OpGenericIterableList::RemoveItem
**
***********************************************************************************/
void* OpGenericIterableList::RemoveItem(void* item)
{
	for (Begin(); m_current; Next())
	{
		if (item == m_current->m_object)
		{
			return RemoveCurrentItem();
		}
	}

	return NULL;
}

/***********************************************************************************
**
** OpGenericIterableList::GetCurrentItem
**
***********************************************************************************/
void* OpGenericIterableList::GetCurrentItem()
{
	return m_current == NULL ? NULL : m_current->m_object;
}
