/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_OPDATACHUNK_H
#define MODULES_OPDATA_OPDATACHUNK_H

/** @file
 *
 * @brief OpDataChunk - managing the underlying data chunks for OpData buffers.
 *
 * This file is an internal/private part of the opdata module. Please do not
 * \#include this file from outside modules/opdata.
 *
 * See OpData.h for more extensive design notes on OpData and OpDataChunk.
 */


#include "modules/opdata/src/OpDataBasics.h"


/**
 * See OpData.h for more documentation.
 */
class OpDataChunk
{
public:
	/**
	 * @name Static creators
	 *
	 * @{
	 */

	/**
	 * Create OpDataChunk object wrapping the given const data.
	 *
	 * @param data Pointer to wrap.
	 * @param size Number of bytes available from \c data pointer.
	 * @returns Pointer to OpDataChunk object wrapping the given data.
	 *          NULL on OOM.
	 */
	static OpDataChunk *Create(const char *data, size_t size)
	{
		return OP_NEW(OpDataChunk, (OPDATA_DEALLOC_NONE, const_cast<char *>(data), size));
	}

	/**
	 * Create OpDataChunk object wrapping the given data.
	 *
	 * @param ds Deallocation strategy for the given \c data.
	 * @param data Pointer to wrap.
	 * @param size Number of bytes available from \c data pointer.
	 * @returns Pointer to OpDataChunk object wrapping the given data.
	 *          NULL on OOM.
	 */
	static OpDataChunk *Create(OpDataDeallocStrategy ds, char *data, size_t size)
	{
		return OP_NEW(OpDataChunk, (ds, data, size));
	}

	/**
	 * Allocate OpDataChunk object and associated buffer of the given size.
	 *
	 * Allocates a single memory area containing BOTH an OpDataChunk object
	 * AND the data buffer managed by that OpDataChunk object. This is done
	 * to reduce the number of allocations. The created OpDataChunk MUST
	 * use #OPDATA_DEALLOC_USER_DEFINED as its deallocation strategy, or
	 * else it won't be properly deallocated (and we'll most likely crash
	 * upon deallocation).
	 *
	 * The data buffer managed by the created OpDataChunk object will be of
	 * at least \c size bytes.
	 *
	 * @param size Size of data buffer to allocate, and reference from the
	 *        returned chunk. The actual allocation may be slightly larger.
	 * @returns Pointer to OpDataChunk object referencing a buffer of at
	 *          least \c size bytes. Returns NULL on OOM.
	 */
	static OpDataChunk *Allocate(size_t size)
	{
		size_t total = ROUND_UP_TO_NEXT_MALLOC_QUANTUM(
			sizeof(OpDataChunk) + size);
		if (total < size || total - sizeof(OpDataChunk) < size)
			return NULL; // detect overflow
		size = total - sizeof(OpDataChunk);

		void *mem = op_malloc(total);
		if (!mem)
			return NULL;

		char *data = static_cast<char *>(mem) + sizeof(OpDataChunk);
		// Placement-new the OpDataChunk object into mem.
		OpDataChunk *chunk = new (mem) OpDataChunk(
			OPDATA_DEALLOC_USER_DEFINED, data, size);

		OP_ASSERT(chunk); // placement-new should never fail
		OP_ASSERT(chunk == mem);
		OP_ASSERT(chunk->IsMutable());
		return chunk;
	}
	/** @} */

	/**
	 * Destroy this OpDataChunk and any associated storage.
	 *
	 * This is tricky: Since we are sometimes placement-new'ed into
	 * op_malloc()ed memory, we cannot always call the regular
	 * delete/OP_DELETE (since it would try to deallocate memory that
	 * belongs to a different allocation). In those cases, we instead
	 * need to explicitly invoke the destructor, followed by manually
	 * op_free()ing the chunk in which this object and its associated
	 * memory resides. Afterwards, we must immediately return and never
	 * ever touch this object again.
	 */
	void Destroy(void)
	{
		OP_ASSERT(GetRC() == 0);
		if (GetDS() == OPDATA_DEALLOC_USER_DEFINED)
		{
			// m_data should point to immediately following memory.
			OP_ASSERT(m_data == reinterpret_cast<const char *>(this + 1));
			this->~OpDataChunk();
			op_free(this);
			return;
		}
		else
		{
			// regular auto-deletion
			OpDataDealloc(m_data, GetDS());
			OP_DELETE(this); // must be last
			return;
		}
	}

	/**
	 * Renounce the ownership of the associated data.
	 *
	 * This method MUST ONLY be used in the case where a chunk was created
	 * to own/manage some data, but the caller now wants to take back
	 * ownership of the data.
	 *
	 * This method MUST NOT be called if chunk was created by #Allocate()
	 * (in which case the chunk's data is co-located with the chunk and the
	 * two can not be separately deallocated).
	 *
	 * The data will remain referenced by this chunk, but its deallocation
	 * strategy will be changed to OPDATA_DEALLOC_NONE, meaning that the
	 * data will be considered immutable, and will not be deallocated when
	 * the chunk is destroyed.
	 *
	 * This method will return the deallocation strategy previously
	 * associated with its data, so that the caller, upon assuming
	 * ownership of the data can learn how to properly deallocate it.
	 */
	OpDataDeallocStrategy AbdicateOwnership(void)
	{
		// MUST NOT be co-located with the data
		OP_ASSERT(GetDS() != OPDATA_DEALLOC_USER_DEFINED);

		OpDataDeallocStrategy ds = GetDS();
		SetMeta(OPDATA_DEALLOC_NONE, GetRC());
		return ds;
	}

	/**
	 * Return the total size of this chunk.
	 */
	op_force_inline size_t Size(void) const { return m_size; }

	/**
	 * Return true iff this chunk is shared between multiple OpData objects.
	 */
	op_force_inline bool IsShared(void) const { return GetRC() > 1; }

	/**
	 * Return true is this chunk must be considered immutable.
	 *
	 * A chunk is immutable as long as it is shared, or if its deallocation
	 * strategy is OPDATA_DEALLOC_NONE (i.e. it refers to constant data).
	 */
	op_force_inline bool IsConst(void) const { return IsShared() || GetDS() == OPDATA_DEALLOC_NONE; }

	/**
	 * Opposite of IsConst().
	 */
	op_force_inline bool IsMutable(void) const { return !IsConst(); }

	/**
	 * Return the deallocation strategy of this chunk.
	 *
	 * This method should not be needed in most cases.
	 * Prefer IsConst()/IsMutable() if possible.
	 */
	op_force_inline OpDataDeallocStrategy DeallocStrategy(void) const { return GetDS(); }

	/**
	 * Return the number of references to this chunk.
	 *
	 * This method should not be needed in most cases.
	 * Prefer IsConst()/IsMutable() if possible.
	 */
	op_force_inline size_t RefCount(void) const { return GetRC(); }

	/**
	 * Return const pointer to this chunk's buffer.
	 *
	 * The caller should use #Size() to establish the length of the
	 * returned buffer.
	 */
	op_force_inline const char *Data(void) const { return m_data; }

	/**
	 * Return non-const pointer to this chunk's buffer.
	 *
	 * MUST NOT be called unless #IsMutable() is true.
	 * The caller should use #Size() to establish the length of the
	 * returned buffer.
	 */
	op_force_inline char *MutableData(void)
	{
		OP_ASSERT(IsMutable());
		return m_data;
	}

	/**
	 * Return byte at given offset.
	 *
	 * The caller MUST ensure that \c offset < #Size()
	 *
	 * @param offset Return byte at this offset of m_data.
	 * @returns The byte at \c offset.
	 */
	op_force_inline const char& At(size_t offset) const
	{
		OP_ASSERT(offset < m_size);
		return m_data[offset];
	}

	/**
	 * Set the byte at the given offset.
	 *
	 * Preconditions:
	 * - This chunk MUST be mutable (unshared and holding non-const data).
	 * - The given offset MUST be < the size of this chunk.
	 *
	 * @param offset Offset at which to set the byte.
	 * @param c The new value of the byte at \c offset.
	 */
	op_force_inline void SetByte(size_t offset, char c)
	{
		OP_ASSERT(IsMutable());
		OP_ASSERT(offset < m_size);
		m_data[offset] = c;
	}

	/**
	 * Try to write the given byte \c c to the given \c offset.
	 *
	 * This function returns true if the byte (at \c offset) is what we
	 * want (i.e. == \c c), false if we can't make it so (because our data
	 * is either const or is shared with other OpData instances).
	 *
	 * @param offset The offset of the byte to check/set.
	 * @param c The byte value to check/set at \c offset.
	 * @retval true if the byte is \c c (or we made it so).
	 * @retval false if the byte is not \c c (and we could not change it).
	 */
	bool TrySetByte (size_t offset, char c)
	{
		if (offset > Size())
			return false;
		if (At(offset) == c)
			return true;
		if (!IsMutable())
			return false;
		SetByte(offset, c);
		return true;
	}

	/**
	 * Increment refcount of this chunk.
	 */
	op_force_inline void Use(void)
	{
		OP_ASSERT(GetRC() < RefCountMax);
		++m_meta; // shortcut
	}

	/**
	 * Decrement refcount of this chunk.
	 */
	op_force_inline void Release(void)
	{
		OP_ASSERT(GetRC() > 0);
		--m_meta; // shortcut
		if (GetRC() == 0)
			Destroy();
	}

private:
	/**
	 * Constructors. Should only be called by the static creators.
	 *
	 * @{
	 */
	OpDataChunk(OpDataDeallocStrategy ds, char *data, size_t size)
		: m_data(data), m_size(size), m_meta(EncodeMeta(ds, 1)) {}

	OpDataChunk(const char *data, size_t size)
		: m_data(const_cast<char *>(data))
		, m_size(size)
		, m_meta(EncodeMeta(OPDATA_DEALLOC_NONE, 1)) {}
	/** @} */

	/**
	 * Destructor. Should only be called by Destroy().
	 *
	 * See Destroy() for important details about destruction/deallocation.
	 */
	~OpDataChunk(void) {}


	/**
	 * Pointer to actual buffer of bytes.
	 *
	 * This is a non-const pointer, but it refers to const data whenever
	 * m_ds == #OPDATA_DEALLOC_NONE. In other words, it is safe to
	 * <code>const_cast\<char *>(data)</code> as long as we have set
	 * m_ds to #OPDATA_DEALLOC_NONE. (The implementation shall never do
	 * non-const things to the pointer when m_ds == #OPDATA_DEALLOC_NONE.)
	 */
	char *m_data;

	/**
	 * Total number of bytes in chunk starting from \c m_data.
	 */
	size_t m_size;

	/**
	 * Combined field: reference count and deallocation strategy.
	 *
	 * The deallocation strategy (accessible through GetDS()) specifies how
	 * \c m_data should be deallocated when this chunk is no longer needed.
	 *
	 * The reference count (accessible through GetRC()) specifies how many
	 * OpData objects are currently referring to this chunk.
	 */
	size_t m_meta;

	/**
	 * Convenience methods for handling m_meta.
	 *
	 * The m_meta member variable encodes the deallocation strategy in the
	 * top (most significant) #OPDATA_DEALLOC_NUMBITS bits of m_meta, while
	 * the remaining bits store the chunk's reference count (how many
	 * OpData objects currently refers to this chunk).
	 *
	 * The following methods handle encoding/decoding between m_meta and
	 * the deallocation strategy + refcount.
	 *
	 * @{
	 */
	static const size_t RefCountNumBits = (sizeof(size_t) * CHAR_BIT - OPDATA_DEALLOC_NUMBITS);
	static const size_t RefCountMax = ((~static_cast<size_t>(0)) >> OPDATA_DEALLOC_NUMBITS);

	static op_force_inline OpDataDeallocStrategy DecodeDS(size_t meta)
	{
		return static_cast<OpDataDeallocStrategy>(
			(meta & ~RefCountMax) >> RefCountNumBits);
	}
	static op_force_inline size_t DecodeRC(size_t meta) { return meta & RefCountMax; }

	static op_force_inline size_t EncodeMeta(OpDataDeallocStrategy ds, size_t rc)
	{
		return (static_cast<size_t>(ds) << RefCountNumBits) | rc;
	}

	op_force_inline OpDataDeallocStrategy GetDS(void) const
	{
		return DecodeDS(m_meta);
	}

	op_force_inline size_t GetRC(void) const
	{
		return DecodeRC(m_meta);
	}

	op_force_inline void SetMeta(OpDataDeallocStrategy ds, size_t rc)
	{
		m_meta = EncodeMeta(ds, rc);
	}
	/** @} */
};

#endif /* MODULES_OPDATA_OPDATACHUNK_H */
