/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#ifdef UTIL_LISTENERS

#include "modules/util/adt/oplisteners.h"

/***********************************************************************************
**
**	OpListenersIterator
**
***********************************************************************************/

OpListenersIterator::OpListenersIterator(OpGenericListeners& listeners)
{
	m_current = 0;
	Into(&listeners.m_iterators);
}

OpListenersIterator::~OpListenersIterator()
{
	Out();
}

/***********************************************************************************
**
**	OpGenericListeners
**
***********************************************************************************/

OP_STATUS OpGenericListeners::Add(void* item)
{
	if (Find(item) == -1)
	{
		return OpGenericVector::Add(item);
	}
	
	return OpStatus::OK;
}

OP_STATUS OpGenericListeners::Remove(void* item)
{
	INT32 pos = Find(item);

	if (pos != -1)
	{
		OpGenericVector::Remove(pos);

		for (OpListenersIterator* iterator = (OpListenersIterator*) m_iterators.First(); iterator; iterator = (OpListenersIterator*) iterator->Suc())
		{
			if (pos < iterator->m_current)
			{
				iterator->m_current--;
			}
		}
	}
	
	return OpStatus::OK;
}

BOOL OpGenericListeners::HasNext(OpListenersIterator& iterator)
{
	return iterator.m_current < (INT32) GetCount();
}

void* OpGenericListeners::GetNext(OpListenersIterator& iterator)
{
	iterator.m_current++;
	return OpGenericVector::Get(iterator.m_current - 1);
}

#endif // UTIL_LISTENERS
