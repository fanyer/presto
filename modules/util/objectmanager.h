/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef UTIL_OBJECTMANAGER_H
#define UTIL_OBJECTMANAGER_H


#include "modules/util/OpHashTable.h"


/** A simple helper class for assigning unique IDs to objects
  * and search for them by those IDs. */
template <class ManagedObjectType>
class ObjectManager
{
public:
	/** Type on which to base managed objects.
	 *
	 * Objects to be managed by an ObjectManager must inherit from this
	 * base-class; for example:
	 * \code
	 * class Thing : public ObjectManager<Thing>::Object { ... }
	 * \endcode
	 *
	 * The manager for such objects must be passed to the constructor so that
	 * the new object and its manager can sort out registration.  This then
	 * ensures the new object's .GetId() is unique (provided the manager doesn't
	 * handle more instances than a UINT32 can count) and the object can be found
	 * by the manager's .FindItemById().
	 */
	class Object
	{
	public:
		Object(ObjectManager* manager)
				: m_manager(manager)
				, m_id(manager->RegisterItem(this))
		{
		}

		OP_STATUS Init() { return m_id ? OpStatus::OK : OpStatus::ERR_NO_MEMORY; }
		virtual ~Object() { if (m_manager) m_manager->UnregisterItem(this); }

		UINT32 GetId() { return m_id; }
		void OnManagerDestroyed() { m_manager = NULL; }
	private:
		ObjectManager* m_manager;
		const UINT32 m_id;
	};

	ObjectManager()
		: m_next_id(0)
	{}

	~ObjectManager()
	{
		DestructorCleanupIterator iterator;
		m_items.ForEach(&iterator);
	}

	/** Finds the item by id. */
	ManagedObjectType* FindItemById(UINT32 id)
	{
		Object* item = NULL;
		m_items.GetData(id, &item);
		return static_cast<ManagedObjectType*>(item);
	}
private:
	friend class Object;

	class DestructorCleanupIterator : public OpHashTableForEachListener
	{
	public:
		virtual void HandleKeyData(const void* key, void* data)
		{
			reinterpret_cast<Object*>(data)->OnManagerDestroyed();
		}
	};

	UINT32 RegisterItem(Object* item)
	{
		OP_ASSERT(item);
		// Casting m_items.GetCount() to unsigned as it realy is unsigned no matter what the return type says.
		OP_ASSERT(static_cast<UINT32>(m_items.GetCount()) < static_cast<UINT32>(-1));
		OP_STATUS err = OpStatus::ERR;
		while (err == OpStatus::ERR)
			if (++m_next_id)
				err = m_items.Add(m_next_id, item);

		return err == OpStatus::OK ? m_next_id : 0;
	}

	void UnregisterItem(Object* item)
	{
		OP_ASSERT(item);
		Object* unused;
		OpStatus::Ignore(m_items.Remove(item->GetId(), &unused));
	}

	INT32 m_next_id;
	OpINT32HashTable<Object> m_items;
};

#endif // UTIL_OBJECTMANAGER_H

