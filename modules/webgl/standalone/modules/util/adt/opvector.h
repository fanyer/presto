/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2007 Opera Software ASA.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

// ---------------------------------------------------------------------------------

#ifndef MODULES_UTIL_ADT_OPVECTOR_H
#define MODULES_UTIL_ADT_OPVECTOR_H

// ---------------------------------------------------------------------------------

class OpGenericVector
{
public:
	OP_STATUS Add(void* item);

	friend class OpGenericBinaryTree;

protected:

	OpGenericVector(UINT32 stepsize = 10);
	virtual ~OpGenericVector();

	void Clear();

	void SetAllocationStepSize(UINT32 step) { m_step = step; m_min_step = step; }

	OP_STATUS DuplicateOf(const OpGenericVector& vec);

	OP_STATUS Replace(UINT32 idx, void* item);

	OP_STATUS Insert(UINT32 idx, void* item);

	OP_STATUS RemoveByItem(void* item);

	void* Remove(UINT32 idx, UINT32 count = 1);

	INT32 Find(void* item) const;

	void* Get(UINT32 idx) const;

	UINT32 GetCount() const { return m_count; }

	OP_STATUS GrowInsert(UINT32 insertidx, void* item);
	void NormalInsert(UINT32 insertidx, void* item);
	void* ShrinkRemove(UINT32 removeidx, UINT32 count);
	void* NormalRemove(UINT32 removeidx, UINT32 count);

	UINT32 m_size;
	void** m_items;
	UINT32 m_count;
	UINT32 m_step;
	UINT32 m_min_step;

private:

	OP_STATUS Init();
	OpGenericVector(const OpGenericVector&);
	OpGenericVector& operator=(const OpGenericVector&);

};

// ---------------------------------------------------------------------------------

class OpINT32Vector : private OpGenericVector
{
public:

	/**
	 * Creates a vector, with a specified allocation step size.
	 */
	OpINT32Vector(UINT32 stepsize = 10) : OpGenericVector(stepsize) {}

	/**
	 * Clear vector, returning the vector to an empty state
	 */
	void Clear() { OpGenericVector::Clear(); }

	/**
	 * Sets the allocation step size
	 */
	void SetAllocationStepSize(UINT32 step) { OpGenericVector::SetAllocationStepSize(step); }

	/**
	 * Makes this a duplicate of vec.
	 */
	OP_STATUS DuplicateOf(const OpINT32Vector& vec) { return OpGenericVector::DuplicateOf(vec); }

	/**
	 * Replace the item at index with new item
	 */
	OP_STATUS Replace(UINT32 idx, INT32 item) { return OpGenericVector::Replace(idx, (void*) item); }

	/**
	 * Insert and add an item of type T at index idx. The index must be inside
	 * the current vector, or added to the end. This does not replace, the vector will grow.
	 */
	OP_STATUS Insert(UINT32 idx, INT32 item) { return OpGenericVector::Insert(idx, (void*) item); }

	/**
	 * Adds the item to the end of the vector.
	 */
	OP_STATUS Add(INT32 item) { return OpGenericVector::Add((void*) item); }

	/**
	 * Removes item if found in vector
	 */
	OP_STATUS RemoveByItem(INT32 item) { return OpGenericVector::RemoveByItem((void*) item); }

	/**
	 * Removes count items starting at index idx and returns the pointer to the first item.
	 */
	INT32 Remove(UINT32 idx, UINT32 count = 1) { return (INTPTR)(OpGenericVector::Remove(idx, count)); }

	/**
	 * Finds the index of a given item
	 */
	INT32 Find(INT32 item) const { return OpGenericVector::Find((void*) item); }

	/**
	 * Returns the item at index idx.
	 */
	INT32 Get(UINT32 idx) const { return (INTPTR)(OpGenericVector::Get(idx)); }

	/**
	 * Returns the number of items in the vector.
	 */
	UINT32 GetCount() const { return OpGenericVector::GetCount(); }

#if defined ADVANCED_OPVECTOR || defined ADVANCED_OPINT32VECTOR
	/**
	 * Merge sort the vector and make all the numbers unique
	 */
	OP_STATUS Sort();

	/**
	 * Binary search the position of the item in a sorted vector or, if not found, a position to insert it
	 */
	UINT32 Search(INT32 item) const {return Search(item, 0, m_count);}

	/**
	 * Search in a subset in range <start, end)
	 */
	UINT32 Search(INT32 item, UINT32 start, UINT32 end) const;

	/**
	 * Add the elements to a sorted vector which aren't present already
	 */
	OP_STATUS Add(OpINT32Vector &vec);
	static OP_STATUS Add(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2);

#ifdef ADVANCED_OPINT32VECTOR
	/**
	 * Remove all the elements from a sorted vector present in vec
	 */
	OP_STATUS Subtract(OpINT32Vector &vec);
	static OP_STATUS Subtract(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2);
#else // old API with incorrect spelling
	/**
	 * Remove all the elements from a sorted vector present in vec
	 */
	OP_STATUS Substract(OpINT32Vector &vec);
	static OP_STATUS Substract(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2);
#endif

	/**
	 * Keep just the elements present in both
	 */
	OP_STATUS Intersect(OpINT32Vector &vec);
	static OP_STATUS Intersect(OpINT32Vector &result, const OpINT32Vector &vec1, const OpINT32Vector &vec2);

protected:
	/**
	 * Shrink m_items after substraction or intersection
	 */
	void Refit(void);
#endif  // ADVANCED_OPVECTOR
};


// ---------------------------------------------------------------------------------

template<class T>
class OpVector : private OpGenericVector
{
public:

	/**
	 * Creates a vector, with a specified allocation step size.
	 */
	OpVector(UINT32 stepsize = 10) : OpGenericVector(stepsize) {}

	/**
	 * Clear vector, returning the vector to an empty state
	 */
	void Clear() { OpGenericVector::Clear(); }

	/**
	 * Sets the allocation step size
	 */
	void SetAllocationStepSize(UINT32 step) { OpGenericVector::SetAllocationStepSize(step); }

	/**
	 * Makes this a duplicate of vec.
	 */
	OP_STATUS DuplicateOf(const OpVector& vec) { return OpGenericVector::DuplicateOf(vec); }

	/**
	 * Replace the item at index with new item
	 */
	OP_STATUS Replace(UINT32 idx, T* item) { return OpGenericVector::Replace(idx, item); }

	/**
	 * Insert and add an item of type T at index idx. The index must be inside
	 * the current vector, or added to the end. This does not replace, the vector will grow.
	 */
	OP_STATUS Insert(UINT32 idx, T* item) { return OpGenericVector::Insert(idx, item); }

	/**
	 * Adds the item to the end of the vector.
	 */
	OP_STATUS Add(T* item) { return OpGenericVector::Add(item); }

	/**
	 * Removes item if found in vector
	 */
	OP_STATUS RemoveByItem(T* item) { return OpGenericVector::RemoveByItem(item); }

	/**
	 * Removes count items starting at index idx and returns the pointer to the first item.
	 */
	T* Remove(UINT32 idx, UINT32 count = 1) { return static_cast<T*>(OpGenericVector::Remove(idx, count)); }

	/**
	 * Removes AND Delete (call actual delete operator) item if found
	 */

	OP_STATUS Delete(T* item)
	{
		RETURN_IF_ERROR(RemoveByItem(item));

		OP_DELETE(item);

		return OpStatus::OK;
	}

	/**
	 * Removes AND Delete (call actual delete operator) count items starting at index idx
	 */

	void Delete(UINT32 idx, UINT32 count = 1)
	{
		// delete items

		for (UINT32 i = 0; i < count; i++)
		{
			OP_DELETE(Get(idx + i));
		}

		// clear nodes

		Remove(idx, count);
	}

	/**
	 * Removes AND Delete (call actual delete operator) all items.
	 */

	void DeleteAll()
	{
		Delete(0, GetCount());
	}

	/**
	 * Finds the index of a given item
	 */
	INT32 Find(T* item) const { return OpGenericVector::Find(item); }

	/**
	 * Returns the item at index idx.
	 */
	T* Get(UINT32 idx) const { return static_cast<T*>(OpGenericVector::Get(idx)); }

	/**
	 * Returns the number of items in the vector.
	 */
	UINT32 GetCount() const { return OpGenericVector::GetCount(); }
};

/**
 * OpAutoVector works exactly like OpVector but it deletes the elements automatically.
 */
template<class T>
class OpAutoVector : public OpVector<T>
{
public:
	OpAutoVector(UINT32 stepsize = 10) : OpVector<T>(stepsize) {}

	virtual ~OpAutoVector()
	{
		// Explicit |this| so that the calls depend on the template type
		UINT32 count = this->GetCount();
		for (UINT32 i = 0; i < count; i++)
		{
			OP_DELETE(this->Get(i));
		}
	};
};

// ---------------------------------------------------------------------------------

#endif // !MODULES_UTIL_ADT_OPVECTOR_H

// ---------------------------------------------------------------------------------
