/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 */

#ifndef TYPED_OBJECT_COLLECTION_H
#define TYPED_OBJECT_COLLECTION_H

#include "adjunct/desktop_util/adt/typedobject.h"

/**
 * A name -> TypedObject pointer map.
 *
 * Since the intended usage is mapping from names that come from outside of
 * code, this class maintains its own copies of the names.
 *
 * @author Wojciech Dzierzanowski (wdzierzanowski)
 */
class TypedObjectCollection
{
public:
	~TypedObjectCollection();

	/**
	 * Creates a mapping from a name to an object.  Fails if a mapping
	 * from @a name has already been created.
	 *
	 * @param name the name.  It is safe to destroy the argument afterwards --
	 * 		a private copy of the string is created.
	 * @param object the object
	 * @return @c OpStatus::OK if mapping created successfully
	 */
	OP_STATUS Put(const OpStringC8& name, TypedObject* object);

	/**
	 * Gets an object by name.
	 *
	 * @param name the name
	 * @return pointer to the object @a name maps to, or @c NULL if there is no
	 * 		mapping for @a name
	 */
	TypedObject* Get(OpStringC8 name) const
		{ return GetUntyped(name); }

	/**
	 * Gets an object by name and type. This method should be called when there
	 * is certainty that object exists (it can be checked by method Contains).
	 *
	 * @param name the name
	 * @param W the object type
	 * @return pointer to the object @a name maps to, or @c NULL if there is no
	 * 		mapping for @a name or the type @a W doesn't match
	 */
	template <typename W>
	inline W* Get(OpStringC8 name) const;

	/**
	 * Gets an object by name and type. Note: this method will LEAVE with
	 * OpStatus::ERR if widget is not found.
	 *
	 * @param name the name
	 * @param W the object type
	 * @return pointer to the object @a name maps to
	 */
	template <typename W>
	inline W* GetL(OpStringC8 name) const;

	/**
	 * Checks if object given by name and type exists in the collection.
	 *
	 * @param name the name
	 * @param W the object type
	 * @return true if object is found, false otherwise
	 */
	template <typename W>
	bool Contains(OpStringC8 name) const;

private:
	typedef OpString8HashTable<TypedObject> NameToWidgetMap;

	TypedObject* GetUntyped(const OpStringC8& name) const;

	NameToWidgetMap m_objects;
};



template <typename W>
W* TypedObjectCollection::Get(OpStringC8 name) const
{
	TypedObject* object = GetUntyped(name);
	W* result = object != NULL ? object->GetTypedObject<W>() : NULL;
	OP_ASSERT(result != NULL);
	return result;
}

template <typename W>
W* TypedObjectCollection::GetL(OpStringC8 name) const
{
	TypedObject* object = GetUntyped(name);
	W* result = object != NULL ? object->GetTypedObject<W>() : NULL;
	if (object == NULL)
	{
		LEAVE(OpStatus::ERR);
	}
	return result;
}

template <typename W>
bool  TypedObjectCollection::Contains(OpStringC8 name) const
{
	TypedObject* result = GetUntyped(name);
	return result != NULL && result->GetTypedObject<W>() != NULL;
}

#endif // TYPED_OBJECT_COLLECTION_H
