/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef OP_FILTERED_PROPERTY_H
#define OP_FILTERED_PROPERTY_H

#include "adjunct/desktop_util/adt/opproperty.h"


/**
 * An OpProperty that doesn't necessarily receive the exact value that is Set().
 *
 * Instead, it performs some filtering, e.g., whitespace stripping.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
template <typename T>
class OpFilteredProperty : public OpProperty<T>
{
public:
	typedef OpDelegateBase<T&> Filter;

	/**
	 * @param filter the filter.  Ownership is transfered.
	 */
	explicit OpFilteredProperty(Filter* filter) : m_filter(filter)  {}

	virtual ~OpFilteredProperty()
		{ OP_DELETE(m_filter); }

	OP_STATUS Init()
		{ return m_filter != NULL ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }

protected:
	virtual BOOL Assign(typename OpProperty<T>::ArgType value)
	{
		OP_ASSERT(m_filter != NULL
				|| !"Check init status with OpFilteredProperty::Init()");
		T filtered_value;
		OpPropertyFunctions::Assign(filtered_value, value);
		(*m_filter)(filtered_value);
		return OpProperty<T>::Assign(filtered_value);
	}

private:
	Filter* m_filter;
};


#endif // OP_FILTERED_PROPERTY_H
