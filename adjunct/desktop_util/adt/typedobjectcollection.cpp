/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#include "core/pch.h"

#include "adjunct/desktop_util/adt/typedobjectcollection.h"
#include "adjunct/desktop_util/adt/hashiterator.h"


TypedObjectCollection::~TypedObjectCollection()
{
	for (String8HashIterator<TypedObject> it(m_objects); it; it++)
	{
		op_free(const_cast<char*>(it.GetKey()));
	}
}

OP_STATUS TypedObjectCollection::Put(const OpStringC8& name,
		TypedObject* object)
{
	if (name.IsEmpty())
	{
		return OpStatus::ERR;
	}
	char* name_clone = op_strdup(name.CStr());
	RETURN_OOM_IF_NULL(name_clone);
	RETURN_IF_ERROR(m_objects.Add(name_clone, object));
	return OpStatus::OK;
}

TypedObject* TypedObjectCollection::GetUntyped(const OpStringC8& name) const
{
	TypedObject* result = NULL;
	if (name.HasContent())
	{
		RETURN_VALUE_IF_ERROR(m_objects.GetData(name.CStr(), &result), NULL);
	}
	return result;
}
