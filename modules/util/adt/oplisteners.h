/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2006 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// ---------------------------------------------------------------------------------

#ifndef MODULES_UTIL_ADT_OPLISTENERS_H
#define MODULES_UTIL_ADT_OPLISTENERS_H

#ifdef UTIL_LISTENERS

#include "modules/util/adt/opvector.h"
#include "modules/util/simset.h"

/**
 * OpListeners uses OpGenericVector, and is specialized for
 * holding a list of listeners. It only exposes Add, Remove,
 * HasNext, GetNext
 *
 * The reason is that it is desired to support that a listener
 * removes itself from the listener list while inside a listener call,
 *
 * typical broadcasting:
 *
 * for (OpListenersIterator iterator(m_listeners); m_listeners.HasNext(iterator);)
 * {
 *		m_listeners.GetNext(iterator)->OnEvent();
 * }
 *
 */

class OpGenericListeners;

class OpListenersIterator : public Link
{
	friend class OpGenericListeners;

	public:
								OpListenersIterator(OpGenericListeners& listeners);
		virtual					~OpListenersIterator();

	private:
	
		INT32					m_current;
};

class OpGenericListeners : private OpGenericVector
{
	friend class OpListenersIterator;

	public:

		OP_STATUS				Add(void* item);
		OP_STATUS				Remove(void* item);
		BOOL					HasNext(OpListenersIterator& iterator);
		void*					GetNext(OpListenersIterator& iterator);

	private:

		Head					m_iterators;
};

template<class T>
class OpListeners : public OpGenericListeners
{
	public:

		OP_STATUS				Add(T* item) { return OpGenericListeners::Add(item); }
		OP_STATUS				Remove(T* item) { return OpGenericListeners::Remove(item); }
		BOOL					HasNext(OpListenersIterator& iterator) {return OpGenericListeners::HasNext(iterator);}
		T*						GetNext(OpListenersIterator& iterator) {return (T*) OpGenericListeners::GetNext(iterator);}
};

#endif // UTIL_LISTENERS

// ---------------------------------------------------------------------------------

#endif // !MODULES_UTIL_ADT_OPLISTENERS_H

// ---------------------------------------------------------------------------------
