/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_OPDATAFRAGMENT_H
#define MODULES_OPDATA_OPDATAFRAGMENT_H

/** @file
 *
 * @brief OpDataFragment - fragments making up OpData buffers.
 *
 * This file is an internal/private part of the opdata module. Please do not
 * \#include this file from outside modules/opdata.
 *
 * See OpData.h for more extensive design notes on OpData and OpDataFragment.
 */


#include "modules/opdata/src/OpDataBasics.h"
#include "modules/opdata/src/OpDataChunk.h"


/**
 * See OpData.h for more documentation.
 */
class OpDataFragment
{
public:

	/**
	 * Create OpDataFragment object wrapping the given chunk.
	 *
	 * The caller is responsible for the given \c chunk having the
	 * appropriate reference count. chunk->Use() is NOT called by this
	 * method.
	 *
	 * @param chunk OpDataChunk to wrap
	 * @param offset Offset into \c chunk.
	 * @param length Lengh of data, starting from \c offset.
	 * @returns Pointer to OpDataFragment object wrapping the given chunk.
	 *          NULL on OOM.
	 */
	static OpDataFragment *Create(OpDataChunk *chunk, size_t offset, size_t length)
	{
		return OP_NEW(OpDataFragment, (chunk, offset, length));
	}

	/**
	 * Copy the given fragment list, starting from the given offset
	 * (\c first_offset) into the first fragment, and ending at the given
	 * offset (\c last_length) into the last fragment. If the first and
	 * last fragments are the same, \c last_length is interpreted as
	 * relative to the start of the fragment (and not relative to
	 * \c first_offset).
	 *
	 * The first/last fragments of the new list are stored in \c new_first
	 * and \c new_last. The new fragment list starts immediately at the
	 * given \c first_offset, and will end at the given \c last_length.
	 * If the caller replaces its existing fragment list with the new
	 * fragment list, it will likely have to adjust its \c first_offset (to
	 * 0) and \c last_length (to \c new_last->Length()) accordingly.
	 *
	 * Each copied fragment will have refcount == 1, and chunk->Use() will
	 * be called for each copied fragment's chunk. If the original fragment
	 * list is to be deleted, the caller is responsible for calling
	 * ->Release() on those fragments (after this method has successfully
	 * returned).
	 *
	 * @param first The first fragment in the list to be copied.
	 * @param last The last fragment in the list to be copied.
	 * @param first_offset The first copied fragment will refer to the bytes
	 *        following this offset relative to \c first.
	 * @param last_length The last copied fragment will refer to the bytes
	 *        preceding this offset relative to \c last.
	 * @param new_first The first copied fragment is stored here on success.
	 * @param new_last The last copied fragment is stored here on success.
	 * @returns OpStatus::OK on success, OpStatus::ERR_NO_MEMORY on failure.
	 */
	static OP_STATUS CopyList(
		const OpDataFragment *first,
		const OpDataFragment *last,
		size_t first_offset,
		size_t last_length,
		OpDataFragment **new_first,
		OpDataFragment **new_last);

	/**
	 * Return true if there is overlap between the two given fragment lists.
	 *
	 * Return true if and only if the two given lists overlap. This is
	 * checked by traversing from \c a_first to \c a_last and checking if
	 * any of the traversed fragments == \c b_last, followed by traversing
	 * from \c b_first to \c b_last and checking if any of the traversed
	 * fragments == \c a_last.
	 *
	 * @param a_first The first fragment in the first list.
	 * @param a_last The last fragment in the first list.
	 * @param b_first The first fragment in the second list.
	 * @param b_last The last fragment in the second list.
	 * @retval true if the two given fragment lists overlap.
	 * @retval false otherwise.
	 */
	static bool ListsOverlap(
		const OpDataFragment *a_first,
		const OpDataFragment *a_last,
		const OpDataFragment *b_first,
		const OpDataFragment *b_last);

	/**
	 * Destructor
	 */
	~OpDataFragment(void) { m_chunk->Release(); }

	/**
	 * Get pointer to the following fragment in the fragment list.
	 *
	 * @returns A pointer to the next fragment in the fragment list, or
	 *          NULL if this is the last fragment in the fragment list.
	 */
	op_force_inline OpDataFragment *Next(void) const { return m_next; }

	/**
	 * Get pointer to the underlying data chunk.
	 *
	 * @returns The chunk holding the data referenced by this fragment.
	 */
	op_force_inline OpDataChunk *Chunk(void) const { return m_chunk; }

	/**
	 * Get offset into the underlying chunk, where this fragment starts.
	 *
	 * @returns The offset into the underlying chunk, where this fragment's
	 *          data starts.
	 */
	op_force_inline size_t Offset(void) const { return m_offset; }

	/**
	 * Get the number of bytes referenced by this fragment.
	 *
	 * @returns Length of this fragment.
	 */
	op_force_inline size_t Length(void) const { return m_length; }

	/**
	 * Get the number of OpData objects that refer to this fragment.
	 *
	 * @return Reference count for this fragment.
	 */
	op_force_inline size_t RC(void) const { return m_rc; }

	/**
	 * Return true iff fragment is shared between multiple OpData objects.
	 */
	op_force_inline bool IsShared(void) const { return RC() > 1; }

	/**
	 * Increment refcount of this fragment.
	 */
	op_force_inline void Use(void)
	{
		++m_rc;
		OP_ASSERT(RC() > 0); // i.e. it wasn't previously "-1"
	}

	/**
	 * Decrement refcount of this fragment.
	 *
	 * The caller should not access this fragment (as it may have been
	 * deleted) after this method's return.
	 */
	op_force_inline void Release(void)
	{
		OP_ASSERT(m_rc > 0);
		if (m_rc-- <= 1)
			delete this; // TODO Replace with Destroy()?
	}

	/**
	 * Return const pointer to this fragment's buffer.
	 */
	const char *Data(void) const { return Chunk()->Data() + Offset(); }

	/**
	 * Return mutable pointer to this fragment's buffer.
	 *
	 * MUST NOT be called if this fragment is shared.
	 */
	char *MutableData(void)
	{
		/* There should be an assertion here.  We used to assert !IsShared(), but
		 * that was wrong (see CORE-42302) and we haven't yet worked out a suitable
		 * replacement. */
		return Chunk()->MutableData() + Offset();
	}

	/**
	 * Return byte at given index.
	 *
	 * Caller MUST ensure 0 <= \c index < Length().
	 *
	 * @param index Return byte at this offset into Data().
	 * @returns The byte at \c index.
	 */
	const char& At(size_t index) const
	{
		OP_ASSERT(index < Length());
		return Data()[index];
	}

	/**
	 * Try to write the given byte \c c to the given \c offset.
	 *
	 * Return true if this fragment already contains a byte of values \c c
	 * at index \c offset, or if the byte can be safely written into that
	 * position without corrupting other OpData instances.
	 *
	 * @param offset The index of the byte to check/set
	 * @param c The byte value to check/set at \c offset
	 * @returns true if the byte at the given \c offset has the value \c c
	 *          when this method returns. false if the byte value is not
	 *          \c c, and \c c could not be written into that position.
	 */
	bool TrySetByte (size_t offset, char c)
	{
		// Handle out of bounds
		if (Offset() + offset >= Chunk()->Size() || offset > Length())
			return false;

		if (offset == Length())
		{
			// Can extend fragment to include byte
			if (CanExtend(1))
			{
#ifdef VALGRIND
				op_valgrind_set_defined(const_cast<void *>(static_cast<const void *>(Data() + offset)), 1);
#endif
				if (Chunk()->TrySetByte(Offset() + offset, c))
				{
					m_length = offset + 1;
					return true;
				}
			}
			return false;
		}

		// Byte is within fragment.
		// Maybe it already has the correct value?
		if (At(offset) == c)
			return true;

		// Otherwise, we need an unshared fragment
		if (IsShared())
			return false;

		// ...and a mutable chunk
		if (!Chunk()->IsMutable())
			return false;

		return Chunk()->TrySetByte(Offset() + offset, c);
	}

	/**
	 * Return a writable ptr at the end of this fragment without allocation.
	 *
	 * This is a primarily a helper method for OpData's own
	 * GetAppendPtr_NoAlloc(), to handle the fragment and chunk layer of
	 * this operation
	 *
	 * Special case: If the given \c length is 0, this method will grab as
	 * much data as is available at the end of the buffer (without
	 * allocation), and update \c length accordingly.
	 *
	 * The returned pointer can be filled with bytes starting from index 0,
	 * and ending before index \c length. Using this pointer to access any
	 * OTHER bytes is forbidden and leads to undefined behavior.
	 *
	 * The returned pointer is always owned by OpDataFragment's internal
	 * data structures. Trying to free or delete the pointer is forbidden
	 * and leads to undefined behavior.
	 *
	 * This method adds \c length uninitialized bytes to the length of this
	 * fragment. If you do not write all \c length bytes into the pointer,
	 * you MUST truncate this fragment accordingly, to avoid including the
	 * uninitialized data.
	 *
	 * This method will rather return NULL, than allocate as necessary to
	 * get a writable memory of the given size.
	 *
	 * @param[in,out] length The returned pointer must be able to hold at
	 *                at least this many bytes. If and only if 0 is passed,
	 *                the maximum number of available bytes will be stored
	 *                into this parameter. If NULL is returned, nothing
	 *                will be stored here.
	 * @returns A writable pointer that can accomodate at least \c length
	 *          bytes, or NULL if no such pointer is available without
	 *          allocation.
	 */
	char *GetAppendPtr_NoAlloc(size_t& length)
	{
		size_t avail = CanExtend(length);
		if (!avail || !Chunk()->IsMutable())
			return NULL;
		if (!length)
			length = avail;

		char *ret = MutableData() + Length();
		m_length += length;
		return ret;
	}

	/**
	 * Make this fragment point to the given fragment.
	 *
	 * MUST only be called to clear this fragment's Next()
	 * (\c frag == NULL), or when Next() was previously NULL.
	 */
	void SetNext(OpDataFragment *frag)
	{
		OP_ASSERT(!frag || !Next());
		m_next = frag;
	}

	/**
	 * Increase the offset (and decrease the length) of this fragment.
	 *
	 * MUST NOT be called if this fragment is shared, and the given
	 * \c length MUST be <= the prior Length().
	 */
	void Consume(size_t length)
	{
		OP_ASSERT(!IsShared());
		OP_ASSERT(length <= m_length);
		m_offset += length;
		m_length -= length;
	}

	/**
	 * Decrease the length of this fragment to the given \c new_length.
	 *
	 * MUST NOT be called if this fragment is shared. Also, \c new_length
	 * MUST be strictly positive, and no more than the prior Length().
	 */
	void Trunc(size_t new_length)
	{
		OP_ASSERT(!IsShared());
		OP_ASSERT(new_length);
		OP_ASSERT(new_length <= m_length);
		m_length = new_length;
	}

	/**
	 * Traversal flags to Traverse() and Lookup().
	 *
	 * These flags can be given to modify the behaviour of the below
	 * Traverse() and Lookup() methods. Unless otherwise noted, flags can
	 * be combined using bitwise OR.
	 *
	 * Any TRAVERSE_ACTION_* flags passed to Traverse() or Lookup() specify
	 * actions (described in detail below) to perform on each fragment
	 * traversed by the method. By default, these actions are performed on
	 * the first fragment traversed (when it is not also the last) and all
	 * subsequent fragments up to \e but \e not \e including the last (which
	 * the method returns). See TRAVERSE_EXCLUDE_FIRST and
	 * TRAVERSE_INCLUDE_LAST for flags to modify this default.
	 */
	enum TraverseFlags {
		/**
		 * Call ->Use() on each fragment that is traversed.
		 *
		 * This flag is useful if you're processing a list of fragments
		 * that will be added to an OpData object.
		 *
		 * This flag cannot be combined with TRAVERSE_ACTION_RELEASE.
		 */
		TRAVERSE_ACTION_USE = (1 << 0),

		/**
		 * Call ->Release() on each fragment that is traversed.
		 *
		 * This flag is useful if you're processing a list of fragments
		 * that are being removed from an OpData object.
		 *
		 * This flag cannot be combined with TRAVERSE_ACTION_USE.
		 *
		 * Be careful if combining this flag with TRAVERSE_INCLUDE_LAST
		 * since that could result in returning a deleted fragment.
		 */
		TRAVERSE_ACTION_RELEASE = (1 << 1),

		/**
		 * Skip invoking the specified action(s) on the first fragment.
		 *
		 * Use this flag to avoid invoking the specified action(s) on
		 * the first fragment in the fragment list. This flag does not
		 * change the traversal itself, only whether or not the
		 * specified action(s) are invoked.
		 *
		 * You must combine this flag with at least one of the above
		 * actions in order for it to be useful.
		 *
		 * This flag may also be combined with TRAVERSE_INCLUDE_LAST,
		 * although TRAVERSE_INCLUDE_LAST takes precedence if both are
		 * given and the first and final fragments coincide.
		 */
		TRAVERSE_EXCLUDE_FIRST = (1 << 2),

		/**
		 * Force invoking the specified action(s) on the last fragment.
		 *
		 * Use this flag to force invoking the specified action(s) on
		 * the last fragment traversed (i.e. the fragment returned from
		 * Traverse()/Lookup()). This flag does not change the traversal
		 * itself, only whether or not the specified action(s) are
		 * invoked.
		 *
		 * You must combine this flag with at least one of the above
		 * actions in order for it to be useful.
		 *
		 * This flag may also be combined with TRAVERSE_EXCLUDE_FIRST,
		 * although TRAVERSE_INCLUDE_LAST takes precedence if both are
		 * given and the first and last fragments coincide.
		 */
		TRAVERSE_INCLUDE_LAST = (1 << 3),

		/**
		 * Prefer Length() of previous fragment to index 0 in current.
		 *
		 * When traversing fragments, if the given index happens to
		 * occur on a fragment boundary, Lookup() will by default
		 * return the fragment after the boundary (and set index to 0).
		 *
		 * Use this flag to instead make Lookup() return the previous
		 * fragment (with index set to the length of that fragment).
		 *
		 * Note: This flag is only valid for Lookup().
		 */
		LOOKUP_PREFER_LENGTH = (1 << 4)
	};

	/**
	 * Helper base class for Traverse().
	 *
	 * Implement ProcessFragment() in a subclass, and pass an instance of
	 * that subclass to Traverse(). The instance's ProcessFragment() will
	 * be invoked for each fragment traversed by Traverse(). See Traverse()
	 * documentation for more details.
	 */
	class TraverseHelper
	{
	public:
		/**
		 * Process each fragment traversed by Traverse().
		 *
		 * This method is invoked once for each fragment traversed by
		 * Traverse(), with the fragment currently being traversed
		 * given in \c frag.
		 *
		 * This method may return true to stop Traverse(). Traverse()
		 * will then return the current fragment. If this method
		 * returns false, Traverse() continues traversing as normal
		 * (see Traverse() documentation for the other ways in which
		 * Traverse() completes traversal).
		 *
		 * There are two versions of ProcessFragment(), taking a const
		 * or non-const fragment, corresponding to the const and non-
		 * const versions of Traverse(). You should only implement one
		 * of them. If your helper does not alter the traversed
		 * fragments, you should implement the const version (the
		 * default implementation of the non-const version will auto-
		 * redirect to your const implementation). Otherwise, if your
		 * helper _does_ alter the traversed fragments, you should
		 * implement the non-const version, and make sure to never
		 * Traverse() a const fragment list (the default implementation
		 * of the const version will assert that it should not be called
		 * and abort traversal immediately).
		 *
		 * @{
		 */
		virtual bool ProcessFragment(const OpDataFragment *frag)
		{
			OP_ASSERT(!"Not implemented!");
			return true;
		}

		virtual bool ProcessFragment(OpDataFragment *frag)
		{
			return ProcessFragment(const_cast<const OpDataFragment *>(frag));
		}
		/** @} */

		/**
		 * The following method is called by Traverse() before traversal
		 * starts, to let the helper object know which fragments shall
		 * be first and last and to indicate which flags were passed to
		 * Traverse().
		 *
		 * The flags are nominally a combination of the above
		 * TraverseFlags, and their associated actions will be performed
		 * by Traverse() (after invoking ProcessFragments()). However,
		 * the helper is free to specify additional flags (and implement
		 * appropriate handling of such flags), as long as those flags
		 * do not conflict with the above TraverseFlags. All the flags
		 * given to Traverse() (both the ones handled by Traverse(),
		 * and any additional flags) are passed to this method.
		 *
		 * @param first The first fragment to be traversed. This is the
		 *        fragment on which Traverse() was called.
		 * @param last The last fragment to be traversed. This may be
		 *        NULL, in which case traversal will continue until
		 *        either ProcessFragments() returns \c true, or the
		 *        end of the fragment list is encountered.
		 * @param flags The flags value given to Traverse(). You may use
		 *        this to pick up helper-specific flags passed by the
		 *        user, or simply monitor the TraverseFlags passed to
		 *        (and handled by) Traverse().
		 */
		virtual void SetTraverseProperties(
			const OpDataFragment *first,
			const OpDataFragment *last,
			unsigned int flags)
		{}
	};

	/**
	 * Traverse fragments, invoking the given callback for each fragment
	 * traversed.
	 *
	 * The given \c helper object's ProcessFragment() method is invoked
	 * once for each fragment in the fragment list starting here, until
	 * one of the following conditions occurs:
	 *  - \c helper->ProcessFragment() returns \c true.
	 *  - Traverse() reaches the \c last fragment.
	 *  - Traverse() reaches the end of the fragment list.
	 *
	 * When the traversal ends, Traverse() returns a pointer to the last
	 * fragment traversed.
	 *
	 * Any actions specified in the \c flags parameter are performed
	 * \e after the call to \c helper->ProcessFragment().
	 *
	 * For convenience, when doing read-only traversal of fragments, there
	 * is also a const version of Traverse() which will invoke the const
	 * version of \c helper->ProcessFragment() during traversal. Because of
	 * the const-ness, it MUST NOT be used with the TRAVERSE_ACTION_USE and
	 * TRAVERSE_ACTION_RELEASE flags, and (because there can be no side-
	 * effects while traversing) the given \c helper MUST be non-NULL
	 * (otherwise, a NULL \c helper would simply be an expensive no-op).
	 *
	 * @param helper TraverseHelper instance whose ->ProcessFragment() method
	 *        will be invoked for each traversed fragment. Traversal stops
	 *        if \c helper->ProcessFragment() returns \c true. May be NULL
	 *        in the non-const version, to perform nothing but the
	 *        side-effects indicated by the given \c flags.
	 * @param last The last fragment to be traversed. Traversal stops after
	 *        this fragment has been traversed. If NULL (the default),
	 *        traversal stops at the end of the fragment list.
	 * @param flags Bitwise OR combination of #TraverseFlags values.
	 *        This allows the caller to modify this method's behaviour.
	 *        See the #TraverseFlags documentation for more information.
	 * @returns The last fragment traversed, or NULL if the end of the
	 *          fragment list was reached.
	 *
	 * @{
	 */
	OpDataFragment *Traverse(TraverseHelper *helper, OpDataFragment *last = NULL, unsigned int flags = 0);

	const OpDataFragment *Traverse(TraverseHelper *helper, const OpDataFragment *last = NULL, unsigned int flags = 0) const;
	/** @} */

	/**
	 * Look up the given \c index in the fragment list starting here.
	 *
	 * Traverse the fragment list to find the given \c index (which is
	 * given relative to the start of this fragment), and return the
	 * fragment containing the \c index, along with updating \c index to be
	 * relative to the returned fragment.
	 *
	 * If the given \c index is within this fragment, this fragment will be
	 * returned, and \c index will be unchanged.
	 *
	 * If \c index occurs at a fragment boundary, return the fragment after
	 * the boundary, and set \c index to 0. Use the LOOKUP_PREFER_LENGTH
	 * flag to change this behaviour.
	 *
	 * If \c index is beyond the end of the fragment list starting at this
	 * fragment, NULL will be returned, and the number of bytes in the
	 * fragment list will be subtracted from \c index.
	 *
	 * @param[in,out] index Index into the fragment list starting at this
	 *                fragment. This parameter is updated to store the
	 *                index relative to the _returned_ fragment.
	 * @param flags Bitwise OR combination of TraverseFlags values.
	 *        This allows the caller to modify this method's behaviour.
	 *        See the TraverseFlags documentation for more information.
	 * @returns Pointer to fragment containing the given \c index. NULL if
	 *          the given \c index is beyond the end of this fragment list.
	 */
	OpDataFragment *Lookup(size_t *index, unsigned int flags = 0);

private:
	/// Constructor.
	OpDataFragment(OpDataChunk *chunk, size_t offset, size_t length)
		: m_next(NULL), m_chunk(chunk), m_offset(offset), m_length(length), m_rc(1) {}

	/**
	 * Return \#bytes that this fragment can be extended by.
	 *
	 * Extending a fragment means increasing its length to reference a
	 * larger portion of the underlying chunk. Obviously, this can only
	 * be done if more bytes are available from the underlying chunk.
	 *
	 * Also, a fragment cannot always be extended without corrupting other
	 * OpData instances that reference the same fragment. Specifically, if
	 * the fragment being extended is a non-last fragment in any OpData
	 * instance, the fragment extension will become part of that OpData
	 * instance's data, thus corrupting those instances. However, even if a
	 * fragment is shared, we can guarantee that it is the last fragment of
	 * all those instances, if (and only if) its Next() pointer is NULL.
	 *
	 * Therefore, a shared fragment can be extended if (a) the next pointer
	 * is NULL and (b) there are additional bytes available at the end of
	 * the underlying chunk. For an unshared fragment, only (b) is required.
	 *
	 * This method checks whether the fragment can be extended by the given
	 * \c extend_length number of bytes. If the answer is no (either
	 * because no extension can be done, or because the number of
	 * extendable bytes is < \c extend_length), 0 is returned. However, if
	 * this fragment can be extended by at least \c extend_length bytes,
	 * the total number of extendable bytes is returned. This number is
	 * guaranteed to be >= \c extend_length.
	 *
	 * If non-zero is returned, the caller is free to add any value <= the
	 * return value to m_length, thereby increasing the number of bytes in
	 * the underlying chunk referenced by this fragment.
	 *
	 * @param extend_length The number of bytes that we want to add to this
	 *        fragment.
	 * @returns 0 if the requested extension cannot be performed, otherwise
	 *          a value >= \c extend_length indicating the maximum possible
	 *          length of extension.
	 */
	size_t CanExtend(size_t extend_length = 1) const
	{
		size_t chunk_length = Offset() + Length();
		if (Next() && IsShared())
			return 0;
		if (chunk_length + extend_length > Chunk()->Size())
			return 0;
		OP_ASSERT(Chunk()->Size() - chunk_length >= extend_length);
		return Chunk()->Size() - chunk_length;
	}

	/// Pointer to the following fragment.
	OpDataFragment *m_next;

	/// Pointer to the underlying data chunk.
	OpDataChunk *m_chunk;

	/// Offset into the underlying data chunk, where this fragment starts.
	size_t m_offset;

	/// Length of fragment, starting from \c m_offset.
	size_t m_length;

	/// Reference count, #OpData objects that reference this fragment.
	size_t m_rc;
};

#endif /* MODULES_OPDATA_OPDATAFRAGMENT_H */
