/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef SCOPE_ID_TABLE_H
#define SCOPE_ID_TABLE_H
#ifdef SCOPE_SUPPORT

#include "modules/util/OpHashTable.h"

/**
 * OpScopeIDTable is a bidirectional mapping between an integral ID, and
 * a pointer. The ID range is 1 and up.
 *
 * This class does not own its objects, and so does not delete the table
 * entries in the destructor. For that, use OpScopeAutoIDTable.
 *
 * Each OpScopeIDTable has a reference count which refers to the permanence
 * of the IDs of the managed objects. The reference count does *not* refer
 * to the object itself. Whenever a Release causes the reference count to hit
 * zero, the hash tables will be cleared, and the object counter reset.
 *
 * Please note that the reference count is initially zero.
 */
template<class T>
class OpScopeIDTable
{
public:

	/**
	 * Constructor. Inits next_id to 1.
	 */
	OpScopeIDTable();

	/**
	 * Destructor. Does nothing currently.
	 */
	virtual ~OpScopeIDTable();

	/**
	 * Remove and/or delete all table elements, then set ID counter
	 * to @c 1.
	 */
	virtual void Purge();

	/**
	 * Call release, then retain immediately. This *may* cause a Purge, if
	 * the reference count hits zero after the first release.
	 */
	void Reset();

	/**
	 * Increase the reference count by one.
	 */
	OpScopeIDTable<T> *Retain();

	/**
	 * Decrease the reference count by one. This *may* cause a Purge, if
	 * it causes the reference count to hit zero.
	 */
	void Release();

	/**
	 * Get the integral ID for the object @c t, or create one if
	 * there is no ID yet.
	 *
	 * @param t [in] The object to create an integral ID for.
	 * @param id [out] The resulting ID.
	 * @return OpStatus::OK if an ID was found or created,
	 *         OpStatus::ERR_NO_MEMORY otherwise.
	 */
	OP_STATUS GetID(T *t, unsigned &id);

	/**
	 * Get the object associated with the specified ID.
	 *
	 * @param id [in] The ID of the object.
	 * @param t [out] A pointer to the object.
	 * @return OpStatus::OK if the object was found, or
	 *         OpStatus::ERR if the ID did not exist.
	 *         (Never OOM).
	 */
	OP_STATUS GetObject(unsigned id, T *&t) const;

	/**
	 * Checks if the object @a t is contained in the ID table.
	 *
	 * @param t [in] The object to check for.
	 * @return @c TRUE if object was found or @c FALSE otherwise.
	 */
	BOOL HasObject(T *t) const;

	/**
	 * Removes the contained object associated with the specified ID.
	 *
	 * @param id [in] The ID of the object.
	 * @return OpStatus::OK if the object was found and removed, or
	 *         OpStatus::ERR if the ID did not exist.
	 *         (Never OOM)
	 */
	OP_STATUS RemoveID(unsigned id);

protected:

	/**
	 * Add an object to the map.
	 *
	 * @param id The ID of the object.
	 * @param t A pointer to the object.
	 * @return OpStatus::OK if the object was added,
	 *         OpStatus::ERR if the object already existed, or
	 *         OpStatus::ERR_NO_MEMORY.
	 */
	OP_STATUS Add(unsigned id, T *t);

	// The next available ID.
	unsigned next_id;

	// Mapping between integral ID and the pointer.
	OpPointerHashTable<void, T> id_to_obj;

	// Mapping between the pointer and the integral ID.
	OpPointerIdHashTable<T, unsigned> obj_to_id;

	// The number of references to this object.
	unsigned ref_count;
};

/**
 * OpScopeAutoIDTable is a bidirectional mapping between an integral ID,
 * and a pointer. The ID range is 1 and up.
 *
 * This class will delete the pointers referenced in the table in the
 * destructor, and in the reset method. OpScopeIDTable is a similar class
 * which doesn't delete its table enties.
 */
template<class T>
class OpScopeAutoIDTable
	: public OpScopeIDTable<T>
{
public:

	/**
	 * Destructor. Deletes all pointers in the table.
	 */
	virtual ~OpScopeAutoIDTable();

	/**
	 * Removes all integral IDs, deletes all pointers, and resets
	 * ID count to @c 1.
	 */
	virtual void Purge();

	/**
	 * Deletes all objects and IDs, but does not reset the ID counter.
	 */
	void DeleteAll();

	/**
	 * Deletes the contained object associated with the specified ID.
	 *
	 * @param[in] id The ID of the object.
	 * @return OpStatus::OK if the object was found and removed, or
	 *         OpStatus::ERR if the ID did not exist. (Never OOM)
	 */
	OP_STATUS DeleteID(unsigned id);

	/**
	 * Deletes the specified object and its ID. If the object is not part
	 * of this ID table, OpStatus::ERR is returned.
	 *
	 * @param[in] t The object to delete.
	 * @return OpStatus::OK if the object was found and removed, or
	 *         OpStatus::ERR if the object did not exist. (Never OOM).
	 */
	OP_STATUS DeleteObject(T *t);
};

// OpScopeIDTable

template<class T>
OpScopeIDTable<T>::OpScopeIDTable()
	: next_id(1)
	, ref_count(0)
{
}

template<class T>
OpScopeIDTable<T>::~OpScopeIDTable()
{
}

template<class T> /* virtual */ void
OpScopeIDTable<T>::Purge()
{
	id_to_obj.RemoveAll();
	obj_to_id.RemoveAll();
	next_id = 1;
}

template<class T> void
OpScopeIDTable<T>::Reset()
{
	Release();
	Retain();
}

template<class T> OpScopeIDTable<T>*
OpScopeIDTable<T>::Retain()
{
	++ref_count;
	return this;
}

template<class T> void
OpScopeIDTable<T>::Release()
{
	OP_ASSERT(ref_count);

	if (ref_count != 0)
		--ref_count;

	if (ref_count == 0)
		this->Purge();
}

template<class T> OP_STATUS
OpScopeIDTable<T>::GetID(T *t, unsigned &id)
{
	if (OpStatus::IsError(obj_to_id.GetData(t, &id)))
	{
		id = next_id++;
		return Add(id, t);
	}

	return OpStatus::OK;
}

template<class T> BOOL
OpScopeIDTable<T>::HasObject(T *t) const
{
	unsigned id;
	if (OpStatus::IsError(obj_to_id.GetData(t, &id)))
		return FALSE;

	return TRUE;
}

template<class T> OP_STATUS
OpScopeIDTable<T>::GetObject(unsigned id, T *&t) const
{
	void *id_ptr = reinterpret_cast<void*>(static_cast<UINTPTR>(id));
	return id_to_obj.GetData(id_ptr, &t);
}


template<class T> OP_STATUS
OpScopeIDTable<T>::Add(unsigned id, T *t)
{
	OP_ASSERT(t);

	void *id_ptr = reinterpret_cast<void*>(static_cast<UINTPTR>(id));

	RETURN_IF_ERROR(id_to_obj.Add(id_ptr, t));

	OP_STATUS status = obj_to_id.Add(t, id);

	if (OpStatus::IsError(status))
		id_to_obj.Remove(id_ptr, &t);

	return status;
}

template<class T> OP_STATUS
OpScopeIDTable<T>::RemoveID(unsigned id)
{
	void *id_ptr = reinterpret_cast<void*>(static_cast<UINTPTR>(id));

	T *tmp = NULL;
	if (OpStatus::IsError(id_to_obj.GetData(id_ptr, &tmp)))
		return OpStatus::ERR;

	RETURN_IF_ERROR(id_to_obj.Remove(id_ptr, &tmp));
	unsigned removed;
	return obj_to_id.Remove(tmp, &removed);
}

// OpScopeAutoIDTable

template<class T>
OpScopeAutoIDTable<T>::~OpScopeAutoIDTable()
{
	// Explicit |this| so that the calls depend on the template type
	this->id_to_obj.DeleteAll();
}

template<class T> /* virtual */ void
OpScopeAutoIDTable<T>::Purge()
{
	// Explicit |this| so that the calls depend on the template type
	this->DeleteAll();
	this->next_id = 1;
}

template<class T> /* virtual */ void
OpScopeAutoIDTable<T>::DeleteAll()
{
	// Explicit |this| so that the calls depend on the template type.
	this->id_to_obj.DeleteAll();
	this->obj_to_id.RemoveAll();
}

template<class T> OP_STATUS
OpScopeAutoIDTable<T>::DeleteID(unsigned id)
{
	void *id_ptr = reinterpret_cast<void*>(static_cast<UINTPTR>(id));

	T *tmp = NULL;
	if (OpStatus::IsError(this->id_to_obj.GetData(id_ptr, &tmp)))
		return OpStatus::ERR;

	OP_DELETE(tmp);

	RETURN_IF_ERROR(this->id_to_obj.Remove(id_ptr, &tmp));
	unsigned removed;
	return this->obj_to_id.Remove(tmp, &removed);
}

template<class T> OP_STATUS
OpScopeAutoIDTable<T>::DeleteObject(T *t)
{
	unsigned removed;
	RETURN_IF_ERROR(this->obj_to_id.Remove(t, &removed));

	OP_DELETE(t);

	void *id_ptr = reinterpret_cast<void*>(static_cast<UINTPTR>(removed));
	return this->id_to_obj.Remove(id_ptr, &t);
}

#endif // SCOPE_SUPPORT
#endif // SCOPE_ID_TABLE_H
