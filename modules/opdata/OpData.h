/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_OPDATA_H
#define MODULES_OPDATA_OPDATA_H

/** @file
 *
 * @brief OpData - efficient data buffer management
 *
 * @mainpage
 *
 * - @ref OpData_design_requirements
 * - @ref OpData_design_principles
 * - @ref UniString_design_requirements
 * - @ref UniString_design_principles
 *
 * @section OpData_design_requirements OpData design requirements
 *
 * Opera does a lot of buffer manipulation, and buffers are often copied
 * between different parts of the code. In order to perform this manipulation
 * and copying as efficiently as possible, we need a buffer abstraction (a.k.a.
 * OpData) that meets the following requirements:
 *
 * -# <b>Low memory usage</b> to run well on lower-end platforms with limited
 *    RAM, as well as increasing performance because of a smaller working set.
 * -# <b>Low memory churn</b> on many platforms, heap allocation is not always
 *    cheap, hence we should minimize the number of unnecessary (re)allocations.
 * -# <b>Cheap copies</b> to allow different parts of the code to maintain their
 *    own buffer instances with their own lifetime requirements.
 * -# <b>Cheap substrings</b> to support dividing a buffer into smaller fragments.
 * -# <b>Cheap insert/append</b> to support assembling fragments into larger buffers.
 * -# <b>Cached length</b> to minimize the number of strlen() calls.
 * -# <b>Multiple deallocation strategies</b> data come from different sources,
 *    and we can not always control exactly how a given piece of data is deleted.
 * -# <b>Low overhead</b> buffer operations should be as quick and efficient as
 *    possible, and the overall performance of OpData must in most cases be
 *    better than using buffers directly.
 * -# <b>Thread-safety</b> the underlying data should be sharable across threads.
 * -# A disproportionately large portion of the buffers in Opera are typically
 *    very short (e.g. < 16 bytes). It would be nice if we could add some
 *    further optimizations for these short buffers.
 *
 * Requirements 1, 2 and 3 suggest that the underlying data should be shared
 * between buffer instances. Requirement 4 suggests that the data sharing must
 * also support one instance referring to a <em>subset</em> of another instance.
 * Requirement 5 suggests that adding data to a buffer should not (always)
 * force a reallocation of the underlying data.
 *
 *
 * @section OpData_design_principles OpData design principles
 *
 * Based on the @ref OpData_design_requirements, we place the following
 * constraints on the OpData design:
 *
 *  -# The OpData object itself must be small enough to be used directly (not
 *     through a pointer). In function/method signatures we should prefer
 *     "(const) OpData& obj" to "(const) OpData *obj".
 *  -# (follows from #1) OpData must have a copy-constructor and an assignment
 *     operator. Both are expected to be used often (either explicitly, or
 *     implicitly by the compiler), so they must be as efficient as possible.
 *  -# Because of OOM safety, neither the copy constructor (or any other
 *     constructor) nor the assignment operator may perform dynamic memory
 *     allocation.
 *  -# (follows from #3) Any object referred to by OpData member pointers must
 *     be sharable (without requiring allocation) between OpData instances.
 *     We will use copy-on-write semantics to ensure that manipulating one
 *     instance does not affect other instances.
 *  -# We will use reference counting in the shared objects. Because of
 *     requirement <em>9. thread-safety</em>, the reference counting must be
 *     done in a thread-safe manner. We assume that there is a pi interface
 *     providing thread-safe reference counting.
 *  -# We define an OpDataChunk class for managing the underlying data chunks.
 *     An OpDataChunk instance manages (or "owns") a single, contiguous chunk
 *     of memory, and contains the following metadata about the chunk:
 *      - The start address of the chunk,
 *      - The total size of the chunk,
 *      - How the chunk should be properly deallocated,
 *      - The number of references to this chunk (a.k.a. reference count).
 *  -# In order to satisfy requirement <em>4. cheap substrings</em>, we need a
 *     higher level of objects "above" the OpDataChunk layer. Each of these
 *     higher-level objects refers to one underlying OpDataChunk instance, and
 *     maintains an offset and a length specifying which subset of this
 *     underlying chunk is in use. For now, we call these higher-level objects
 *     'fragments'.
 *  -# In order to satisfy requirement <em>5. cheap insert/append</em>, we must
 *     be able to specify an ordered sequence of the fragments (in #7). Since
 *     prepend/append is more common than insert, it is more important to have
 *     quick access to the first and last fragments, than to have quick access
 *     to a fragment in the middle.
 *  -# (follows from #7 and #8) Since the actual data referenced by a buffer is
 *     divided among multiple fragments referencing different underlying chunks,
 *     we can no longer inexpensively retrieve a single pointer to the entire
 *     buffer. Even if the buffer consists of a single fragment, #7 suggests
 *     that the fragment may not be '\0'-terminated. Therefore, when a single
 *     ('\0'-terminated) buffer pointer is needed, we need to consolidate the
 *     OpData fragments into a single ('\0'-terminated) buffer. In effect, this
 *     design \e transfers most of the buffer reallocation cost from the
 *     manipulation operation, to the consolidation operation. However, the
 *     cost of consolidation should never exceed the savings in the preceding
 *     manipulation, so the net effect should not be decreased performance.
 *  -# In order to minimize the consolidation described in #9, we must focus on
 *     eliminating code that requires a single ('\0'-terminated) buffer pointer
 *     to be created from an OpData object. In general, this means eliminating
 *     the usage of str/mem functions from the standard C library, and replace
 *     them with OpData methods that work efficiently on the OpData objects.
 *  -# (follows from #10) In order to successfully replace other kinds of
 *     buffer usage that require a single ('\0'-terminated) buffer pointer, we
 *     must design OpData with an API that is rich enough to allow all common
 *     buffer operations. Furthermore, the API should be simple and intuitive
 *     so as to give developers further incentives to use OpData instead of
 *     alternative buffer management strategies.
 *  -# At the highest level (the OpData object itself), we should attempt to
 *     allow for small buffers to be embedded directly inside the object
 *     itself, thus eliminating the need for the lower-level structures in the
 *     (common) case of very short buffers.
 *  -# (follows from #4) In order to make substrings very quick, we should be
 *     able to construct them without doing memory allocation. In other words,
 *     the method(s) that return a substring should return a stack-allocated
 *     OpData object that refers to a substring of another OpData object.
 *     This has the added advantage of making the substring operation always
 *     succeed. However, it requires that one OpData instance must be able to
 *     directly refer to a \e subset of the data used by another OpData
 *     instance. This means that the OpData instance itself must contain an
 *     offset and length relative to the underlying objects shared with other
 *     OpData instances.
 *  -# (follows from #3) Since we cannot use constructors to create OpData
 *     objects that actually wrap a given piece of raw data (because doing
 *     so involves allocation of fragment/chunk objects that may OOM),
 *     we must instead use static creator methods on OpData to create such
 *     OpData objects.
 *
 *
 * @section OpData_design_overview OpData design overview
 *
 * Based on the @ref OpData_design_principles, we arrive at the following
 * design for OpData:
 *
 * \dot
 * digraph example {
 *   node [shape=record, fontsize=10, rankdir=LR];
 *   edge [fontsize=10];
 *   { rank = same; OpData [ URL="\ref OpData" ]; }
 *   OpData -> frag1 [ label="first,\noffset" ];
 *   OpData -> frag3 [ label="last,\nlength" ];
 *
 *   { rank = same;
 *     frag1 [ label="OpDataFragment" URL="\ref OpDataFragment" ];
 *     frag2 [ label="OpDataFragment" URL="\ref OpDataFragment" ];
 *     frag3 [ label="OpDataFragment" URL="\ref OpDataFragment" ];
 *     NULL [ shape=plaintext ];
 *   }
 *   frag1 -> frag2 -> frag3 -> NULL
 *     [ label="next" URL="\ref OpDataFragment::Next()" ];
 *
 *   frag1 -> chunk1 [ label="chunk" URL="\ref OpDataFragment::Chunk()" ];
 *   frag2 -> chunk2 [ label="chunk" URL="\ref OpDataFragment::Chunk()" ];
 *   frag3 -> chunk2 [ label="chunk" URL="\ref OpDataFragment::Chunk()" ];
 *   { rank = same;
 *     chunk1 [ label="OpDataChunk" URL="\ref OpDataChunk" ];
 *     chunk2 [ label="OpDataChunk" URL="\ref OpDataChunk" ];
 *   }
 * }
 * \enddot
 *
 * Describing each level of this structure from the top down:
 *
 * OpData: This object identifies the buffer. It contains:
 *  - The pointer to the first and last fragments used by this buffer (see
 *    OpData::n.first and OpData::n.last).
 *  - An offset into the first fragment, where this buffer starts (see
 *    OpData::n.first_offset).
 *  - A length into the last fragment, where this buffer ends (see
 *    OpData::n.last_length).
 *  - A total length (\#bytes referenced by this buffer) (see OpData::n.length).
 *
 * OpDataFragment: This object identifies one fragment which is referenced by
 * one or more OpData objects. It contains:
 *  - A pointer to the following fragment (see OpDataFragment::Next()).
 *  - A pointer to an underlying data chunk (see OpDataFragment::Chunk()).
 *  - An offset into that chunk, where this fragment starts (see
 *    OpDataFragment::Offset()).
 *  - A length, starting from the offset, where this fragment ends (see
 *    OpDataFragment::Length()).
 *  - A reference count, the number of OpData objects that reference this
 *    fragment (see OpDataFragment::RC()).
 *
 * OpDataChunk: This object identifies one contiguous piece of underlying data.
 * It contains:
 *  - A pointer to the start address of the chunk (see OpDataChunk::Data()).
 *  - The total size of the chunk (see OpDataChunk::Size()).
 *  - A deallocation strategy identifying how the chunk should be deallocated
 *    (see OpDataChunk::DeallocStrategy()).
 *  - A reference count, the number of OpDataFragment objects that directly
 *    reference this chunk (see OpDataChunk::RefCount()).
 *
 * In the interest of keeping objects as small and efficient as possible, we
 * apply the following optimizations to the data structure (in the following,
 * we assume that addresses (pointers), sizes (size_t), and refcounts are 4
 * bytes on 32-bit systems and 8 bytes on 64-bit systems. Sizes are listed in
 * X/Y notation where X and Y refer to the size on 32-bit and 64-bit systems,
 * respectively):
 *
 * OpData:
 * The object consists of 2 pointers and 3 sizes, which makes it 20/40 bytes.
 * For short buffers (< 20/40 bytes), we fit the actual buffer data into the
 * object itself by repurposing the object as follows:
 *  - We need a bit located at a known place in the object that determines
 *    whether the object is in "normal" mode, or this new "embedded" mode.
 *  - This bit should be located in the very first or last byte of the object,
 *    so that we can use the remaining object bytes as a contiguous buffer.
 *  - In "embedded mode", The byte containing the "mode" bit can use its
 *    remaining bits to encode the length of the embedded buffer. Since the
 *    object is up to 20/40 bytes long, we need 5/6 bits to encode the length.
 *    This leaves 2/1 bits free for other purposes (see alignment notes below).
 *  - We can assume that the OpDataFragment objects referenced by our 'first'
 *    and 'last' pointers are pointer aligned (4/8-byte alignment), since these
 *    objects contain pointers of their own. (Furthermore, if we put all
 *    OpDataFragment objects in a memory pool, as suggested below, we can
 *    positively guarantee such alignment in all cases.)
 *  - Thus, we can use the least significant bit of the 'first' or 'last'
 *    fragment pointer to store the "mode" of this OpData object.
 *  - By placing the 'first' or 'last' fragment pointer at the start of the
 *    object in little-endian mode, and conversely at the end of the object in
 *    big-endian mode, we can ensure that the least significant byte ends up in
 *    the first or last byte of the object.
 *  - This leaves 19/39 bytes available for storing actual buffer data. For
 *    buffers within this size, we need no OpDataFragment or OpDataChunk
 *    objects.
 *  - At least one other class will use OpData for its internal data storage:
 *    UniString. UniString uses OpData to store strings of uni_char values, and
 *    this data MAY need to be aligned on sizeof(uni_char).
 *  - UniString objects will co-exist with other OpData objects. We therefore
 *    EITHER have to make \e all OpData objects obey uni_char alignment
 *    restrictions, OR store the alignment requirement per OpData object.
 *  - In embedded mode, we \e could store the alignment using the 1 free bit in
 *    the byte containing the mode bit (see above). Furthermore, when that bit
 *    is set, we \e could store additional alignment information in the next
 *    byte (the second byte in little-endian mode, or the next-to-last byte in
 *    big-endian mode), allowing OpData objects to handle \e any kind of
 *    alignment. In normal mode, we could use the low bits of the 'last'
 *    fragment pointer to store the alignment information. However, these tricks
 *    would add considerable complexity to the implementation.
 *  - Alternatively, we could go for the simpler, but less elegant solution,
 *    where we simply require any uni_char alignment restrictions to apply to
 *    \e all OpData objects. This would lose the use of one byte in embedded
 *    mode for non-UniString OpData objects, and we would also not be able to
 *    store data with \e different alignment requirements in OpData.
 *  - It is not yet clear which solution is more beneficial. For now, we're
 *    going to do the simple and less elegant solution, and waste an extra byte
 *    in embedded mode, in order to achieve uni_char alignment. Obviously we
 *    will only do this when uni_char alignment is truly required, so it will
 *    be controlled by a suitable \#define (OPDATA_UNI_CHAR_ALIGNMENT). Thus,
 *    when this \#define is enabled, we will only be able to store 18/38 bytes
 *    of actual buffer data in embedded mode.
 *
 * OpDataFragment:
 * The object consists of 2 pointers, 2 sizes and a reference count, which
 * makes it 20/40 bytes. Buffer manipulation will typically create and destroy
 * a lot of these objects, so we plan to allocate memory pools for managing
 * these objects. Organizing them into memory pools might also allow for
 * further optimizations at a later date (e.g. by traversing the memory pool
 * we can collect statistics on the fragments and chunks in-use, and based on
 * that we may be able to do targeted consolidation of chunks to decrease
 * and/or defragment the memory used by our buffers).
 *
 * OpDataChunk:
 * The object consists of a pointer, a size, a deallocation strategy
 * (enum value limited to 2 or 3 bits), and a reference count. If we're somehow
 * able to encode the deallocation strategy into the other members (e.g. if the
 * underlying data has alignment requirements, we could use the least
 * significant bits of the data pointer, or if we can guarantee a maximum chunk
 * size, or a maximum reference count (more likely), we could use the most
 * significant bits of one of those members), the resulting size of an
 * OpDataChunk object should be 12/24 bytes. Since there will be one of these
 * chunk objects for every underlying piece of data, we should try to make the
 * allocation and management of these chunk objects as efficient as possible.
 * If we expect that most underlying chunks of data can be allocated from
 * within the OpData code itself, we should be able to allocate a memory area
 * large enough to hold both the OpDataChunk object and the associated chunk of
 * data, then using placement-new to construct the OpDataChunk object at the
 * start of that memory area, and make it use the remainder of the area as its
 * underlying data. However, if we expect that we in most cases cannot control
 * the allocation of the underlying data chunk, we should instead consider
 * memory-pooling the OpDataChunk objects, in the same manner as the fragment
 * objects. As described above, that may open up for further optimizations in
 * the future, enabled by the ability to analyze all the OpDataChunk objects in
 * the memory pool.
 *
 * Some notes about OpDataChunk and the const-ness of data:
 *
 * The chunk's data pointer is nominally a char * pointer, indicating that the
 * data area itself is mutable. However, this is not always the case.
 * First, if the deallocation strategy is OPDATA_DEALLOC_NONE, we must assume
 * that we don't have total control of the data. (Since the data is not to be
 * deallocated by us, it may have an external owner, or it may even be located
 * in a static data area or ROM storage.) We must therefore never try to alter
 * the data in this area, but instead consider it read-only.
 * Second, if the chunk is shared by multiple OpData objects (i.e. the chunk's
 * reference count is > 1 or the fragment referring to the chunk has reference
 * count > 1) we must never attempt to change the data, since that would
 * silently corrupt the other OpData objects that refer to the same data.
 * Therefore, a chunk may only consider its data mutable if \e both of the
 * following conditions are met:
 *  - The data is \e owned by the chunk (i.e. the chunk is responsible for
 *    deallocating the data).
 *  - The chunk is \e not \e shared between multiple OpData objects (i.e. the
 *    chunk's reference count must be exactly 1, and the referring fragment's
 *    reference count must also be exactly 1).
 *
 * Some notes about the const OpData objects and the CreatePtr()/Data() methods:
 *
 * Since CreatePtr()/Data() may force reallocation of the underlying data and
 * considerable juggling of data structures, it may not usually be a const
 * method. However, users of OpData will definitely expect to be able to
 * retrieve a const pointer to the underlying data of a const object.
 * We therefore have to make all the OpData members \e mutable, in order to
 * make CreatePtr() and Data() into const methods.
 *
 * @author Johan Herland, johanh@opera.com
 */

#include "modules/opdata/src/OpDataBasics.h"

class OpDataFragment;
template <class T> class OtlCountedList; // Needed by OpData::Split()

/**
 * Determine whether we need to uni_char align all our data buffers.
 * We only need to do this when all of these conditions hold:
 *  - We will use OpData to store uni_char data (e.g. we are using UniString).
 *  - The underlying platform requires that uni_chars are (16-bit) aligned.
 *
 * For now, we will assume that UniString is always in use, and that platforms
 * requiring uni_char alignment, have enabled SYSTEM_RISC_ALIGNMENT.
 */
#ifdef NEEDS_RISC_ALIGNMENT
# define OPDATA_UNI_CHAR_ALIGNMENT
#endif // NEEDS_RISC_ALIGNMENT


/**
 * Efficient data buffer abstraction that uses implicit sharing and
 * copy-on-write semantics to lower memory usage and increase performance.
 *
 * OpData objects exist in one of two modes - "embedded" or "normal". The mode
 * determines which union member (OpData::e or OpData::n, respectively) is used
 * to access the object's internal members. There is a mode bit (located in the
 * least significant bit of the first (or last) byte of the object) which
 * determines whether the object is in "embedded" mode (set) or "normal" mode
 * (unset):
 *
 * Embedded mode:
 *
 * - The referenced data is stored directly within the OpData object.
 * - The OpData object members are accessed through the OpData::e struct, and
 *   consist of one byte holding the meta information (OpData::meta: mode bit
 *   and data length), and the remaining EmbeddedDataLimit bytes holding the
 *   actual data (OpData::data).
 *
 * Normal mode:
 *
 * - The referenced data is stored as fragments (linked list of OpDataFragment
 *   objects, see OpData::n.first) each fragment referring to a subset of an
 *   underlying chunk (OpDataChunk object) where the actual data is stored (see
 *   the diagram in @ref OpData_design_principles).
 * - The OpData object members are accessed through the OpData::n struct, and
 *   consist of pointers to the first and last fragments in the fragment list
 *   (OpData::n.first and OpData::n.last, respectively), an offset into the
 *   first fragment (OpData::n.first_offset), and a length into the last
 *   fragment (OpData::n.last_length). Finally, the total length of the data
 *   referenced by the OpData instance is cached in the OpData::n.length member.
 * - There is always at least one fragment in the fragment list (in which case
 *   OpData::n.first == OpData::n.last). Neither OpData::n.first nor
 *   OpData::n.last may be NULL.
 * - In normal mode, the data starts at OpData::n.first_offset into the
 *   OpData::n.first fragment, and ends at OpData::n.last_length into the
 *   OpData::n.last fragment. Even if OpData::n.first and OpData::n.last are
 *   the same, OpData::n.last_length is still relative to the start of
 *   OpData::n.last (and not relative to OpData::n.first_offset). Otherwise,
 *   all data in intermediate fragments is referenced (i.e. part of the OpData
 *   data).
 *
 * Rules for choosing between normal and embedded mode:
 *
 * - An empty OpData instance must always be in embedded mode.
 * - An instance with Length > EmbeddedDataLimit must always be in normal mode.
 * - Instances with Length() <= EmbeddedDataLimit will usually be in embedded
 *   mode. Exceptions:
 *     - Split() may generate short instances in normal mode.
 *     - Append*() may convert an embedded mode object into a (short) normal
 *       mode object, if the other object (this/appender or other/appendee) is
 *       already in normal mode, and the entire result will not fit in embedded
 *       mode.
 */
class OpData
{
public:
	/**
	 * @name Static creator methods
	 *
	 * The following methods return newly created OpData objects with the
	 * given contents. All of these methods will LEAVE() on OOM (failure to
	 * allocate internal data structures that package the given content).
	 * The provided methods are:
	 *
	 * - FromConstDataL() for wrapping an OpData object around constant data
	 *   that should not be deallocated. This is typically used to wrap
	 *   literal const char * strings, ROM data, or other data that is
	 *   guaranteed to be constant for the lifetime of the process.
	 *
	 * - FromRawDataL() for wrapping an OpData object around a buffer that
	 *   should be automatically deallocated when no longer in use. The
	 *   appropriate mechanism for deallocating the data must be given to
	 *   FromRawDataL() in form of a #OpDataDeallocStrategy.
	 *
	 * - CopyDataL() for creating an OpData object holding its own copy of
	 *   the given data. This is typically used when you cannot transfer
	 *   ownership of the given data to OpData.
	 *
	 * For the non-LEAVE()ing versions of these methods, please see the
	 * following corresponding (non-static) setter methods:
	 *
	 * - SetConstData()
	 * - SetRawData()
	 * - SetCopyData()
	 *
	 * @{
	 */

	/**
	 * Create OpData from const char data.
	 *
	 * Use this creator to wrap constant byte data in an OpData object.
	 *
	 * Constructs an OpData that may use the first \c length bytes of the
	 * \c data array. The OpData may refer to the actual memory referenced
	 * by the \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with op_strlen().
	 *
	 * The caller \e promises that \c data will not be deleted or modified
	 * as long as this OpData and/or any copies (even of only fragments) of
	 * it exist. In other words, because OpData instances implicitly share
	 * the underlying data, the caller must not delete of modify \c data
	 * for as long as the returned OpData instance and/or any copies of it
	 * (or any sub-string of it) exist.
	 *
	 * OpData does not take ownership of \c data, so the OpData destructor
	 * will never delete \c data, even when the last OpData referring to
	 * \c data is destroyed.
	 *
	 * Warning: An OpData created with FromConstDataL() is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' byte at position \c length.
	 * While '\0'-termination does not matter for OpData, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from the OpData object.
	 *
	 * @param data The data array to wrap in an OpData object.
	 * @param length The number of bytes in the \c data array. If this is
	 *        #OpDataUnknownLength (the default), op_strlen() will be used
	 *        to find the length of the \c data array. Hence, if you want
	 *        to embed '\0' bytes within an OpData, you \e must specify
	 *        \c length.
	 * @returns An OpData object wrapping the given \c data array.
	 *          LEAVEs on OOM.
	 */
	static OpData FromConstDataL(const char *data, size_t length = OpDataUnknownLength)
	{
		ANCHORD(OpData, ret);
		LEAVE_IF_ERROR(ret.AppendConstData(data, length));
		return ret;
	}

	/**
	 * Create OpData from raw char data with a given deallocation strategy.
	 *
	 * Use this creator to wrap allocated byte data in an OpData object,
	 * transferring ownership of the byte data to OpData in the process.
	 * Note that using this method with \c ds == #OPDATA_DEALLOC_NONE is
	 * equivalent to using the above FromConstDataL() creator, and hence
	 * subject to the same restrictions.
	 *
	 * Constructs an OpData that may use the first \c length bytes of the
	 * \c data array. The OpData may refer to the actual memory referenced
	 * by the \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with op_strlen().
	 *
	 * If the actual allocated \c data array is larger than the given
	 * \c length, specify \c alloc to inform OpData of the length of the
	 * allocation. This enables OpData to use the uninitialized bytes
	 * between \c length and \c alloc, and thus potentially prevent
	 * (needless) reallocations.
	 *
	 * A successful call of this method transfers ownership of the \c data
	 * array to the OpData object; subsequently, the \c data array may only
	 * be accessed via the OpData object, and the caller should treat the
	 * memory at \c data as if it'd just been deallocated.
	 * OpData will automatically deallocate the \c data array when no longer
	 * needed, according to the given deallocation strategy \c ds, and may
	 * in fact have already done so before this method returns. If this
	 * method fails, the caller retains responsibility for deallocating the
	 * \c data array.
	 *
	 * Warning: An OpData created with FromRawDataL() is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' byte at position \c length.
	 * While '\0'-termination does not matter for OpData, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from the OpData object.
	 *
	 * @param ds Deallocation strategy. How to deallocate \c data.
	 * @param data The data array to wrap in an OpData object.
	 * @param length The number of valid/initialized bytes in the \c data
	 *        array. If this is #OpDataUnknownLength (the default),
	 *        op_strlen() will be used to find the length of the \c data
	 *        array. Hence, if you want to embed '\0' bytes within an
	 *        OpData, you \e must specify \c length.
	 * @param alloc The total size of the allocation starting at \c data.
	 *        Defaults to \c length if not given.
	 * @returns An OpData object wrapping the given \c data array.
	 *          LEAVEs on OOM.
	 */
	static OpData FromRawDataL(OpDataDeallocStrategy ds, char *data, size_t length = OpDataUnknownLength, size_t alloc = OpDataUnknownLength)
	{
		ANCHORD(OpData, ret);
		LEAVE_IF_ERROR(ret.AppendRawData(ds, data, length, alloc));
		return ret;
	}

	/**
	 * Create OpData holding a copy of the given \c data array.
	 *
	 * Use this creator to copy data into an OpData object. The ownership
	 * of the original \c data is NOT transferred to OpData. This is useful
	 * when you cannot guarantee the lifetime of the given \c data.
	 *
	 * Constructs an OpData that copies the first \c length bytes from the
	 * \c data array. The OpData will have its own copy of the given data.
	 *
	 * @param data The data array to copy
	 * @param length The number of bytes to copy from \c data. If this is
	 *        #OpDataUnknownLength (the default), op_strlen() will be used
	 *        to find the length of the \c data array. Hence, if you want
	 *        to embed '\0' bytes within an OpData, you \e must specify
	 *        \c length.
	 * @returns OpData object referring to a copy of \c data.
	 *          LEAVEs on OOM.
	 */
	static OpData CopyDataL(const char *data, size_t length = OpDataUnknownLength)
	{
		ANCHORD(OpData, ret);
		LEAVE_IF_ERROR(ret.AppendCopyData(data, length));
		return ret;
	}

	/** @} */


	/**
	 * Default constructor
	 */
	OpData(void) { e.meta = EmptyMeta(); }

	/**
	 * Destructor
	 */
	~OpData(void) { Clear(); }

	/**
	 * Copy constructor
	 *
	 * Creates a shallow copy of the \c other OpData, referring to the
	 * bytes between the given indexes \c offset and \c length.
	 *
	 * @param other Copy this OpData object
	 * @param offset Refer to this offset into the \c other object.
	 * @param length Refer to this many bytes starting from \c offset.
	 * @returns A shallow copy of \c other, referring to the bytes between
	 *          \c offset and \c length.
	 */
	OpData(const OpData& other, size_t offset = 0, size_t length = OpDataFullLength);

	/**
	 * Assignment operator
	 */
	OpData& operator=(const OpData& other);

	/**
	 * Returns the number of data bytes in this object.
	 *
	 * @returns The number of valid/initialized bytes in this buffer.
	 */
	op_force_inline size_t Length(void) const { return IsEmbedded() ? EmbeddedLength() : n.length; }

	/**
	 * Tests whether this object has any content.
	 *
	 * @returns true if this object has length 0; otherwise returns false.
	 */
	op_force_inline bool IsEmpty(void) const { return Length() == 0; }

	/**
	 * @name Methods for setting the contents of an OpData object
	 *
	 * The following methods set the content of this OpData object.
	 * All of these methods return either OpStatus::OK (on success) or
	 * OpStatus::ERR_NO_MEMORY on OOM (failure to allocate internal data
	 * structures that package the given content). In case of OOM, the
	 * previous content of this object is left unchanged.
	 *
	 * These methods mirror the static creator methods above, in that they
	 * provide corresponding non-LEAVEing versions that return OP_STATUS
	 * instead of returning a new object.
	 *
	 * Also note that these methods mirror corresponding Append* methods,
	 * and that the SetFoo() methods declared here are equivalent to
	 * Clear() followed by the corresponding AppendFoo() method (except
	 * that SetFoo() will retain its original contents on error).
	 *
	 * The provided methods are:
	 *
	 * - SetConstData() for wrapping an OpData object around constant data
	 *   that should not be deallocated. This is typically used to wrap
	 *   literal const char * strings, ROM data, or other data that is
	 *   guaranteed to be constant for the lifetime of the process.
	 *
	 * - SetRawData() for wrapping an OpData object around a buffer that
	 *   should be automatically deallocated when no longer in use. The
	 *   appropriate mechanism for deallocating the data must be given to
	 *   SetRawData() in form of a #OpDataDeallocStrategy.
	 *
	 * - SetCopyData() for creating an OpData object holding its own copy of
	 *   the given data. This is typically used when you cannot transfer
	 *   ownership of the given data to OpData.
	 *
	 * For the LEAVE()ing versions of these methods, please see the
	 * following corresponding (static) creator methods:
	 *
	 * - FromConstDataL()
	 * - FromRawDataL()
	 * - CopyDataL()
	 *
	 * @{
	 */

	/**
	 * Set this OpData from const char * data.
	 *
	 * Make this object wrap the given constant byte data.
	 *
	 * This OpData object may use the first \c length bytes of the \c data
	 * array. This object may refer to the actual memory referenced by the
	 * \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with op_strlen().
	 *
	 * The caller \e promises that \c data will not be deleted or modified
	 * as long as this OpData and/or any copies (even of only fragments) of
	 * it exist. In other words, because OpData instances implicitly share
	 * the underlying data, the caller must not delete of modify \c data
	 * for as long as this instance and/or any copies of it (or any
	 * sub-string of it) exist.
	 *
	 * OpData does not take ownership of \c data, so the OpData destructor
	 * will never delete \c data, even when the last OpData referring to
	 * \c data is destroyed.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' byte at position \c length.
	 * While '\0'-termination does not matter for OpData, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param data The data array to wrap with this OpData object.
	 * @param length The number of bytes in the \c data array. If this is
	 *        #OpDataUnknownLength (the default), op_strlen() will be used
	 *        to find the length of the \c data array. Hence, if you want
	 *        to embed '\0' bytes within this object, you \e must specify
	 *        \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS SetConstData(const char *data, size_t length = OpDataUnknownLength)
	{
		OpData d;
		RETURN_IF_ERROR(d.AppendConstData(data, length));
		*this = d;
		return OpStatus::OK;
	}

	/**
	 * Set this OpData from raw char data with a given deallocation strategy.
	 *
	 * Use this method to make this object wrap the given allocated byte
	 * data, transferring ownership of the data to OpData in the process.
	 * Note that using this method with \c ds == #OPDATA_DEALLOC_NONE is
	 * equivalent to using the above SetConstData() method, and hence
	 * subject to the same restrictions.
	 *
	 * This OpData object may use the first \c length bytes of the \c data
	 * array. This object may refer to the actual memory referenced by the
	 * \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with op_strlen().
	 *
	 * If the actual allocated \c data array is larger than the given
	 * \c length, specify \c alloc to inform OpData of the length of the
	 * allocation. This enables OpData to use the uninitialized bytes
	 * between \c length and \c alloc, and thus potentially prevent
	 * (needless) reallocations.
	 *
	 * A successful call of this method transfers ownership of the \c data
	 * array to this OpData object; subsequently, the \c data array may only
	 * be accessed via this OpData object, and the caller should treat the
	 * memory at \c data as if it'd just been deallocated.
	 * OpData will automatically deallocate the \c data array when no longer
	 * needed, according to the given deallocation strategy \c ds, and may
	 * in fact have already done so before this method returns. If this
	 * method fails, the caller retains responsibility for deallocating the
	 * \c data array.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' byte at position \c length.
	 * While '\0'-termination does not matter for OpData, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param ds Deallocation strategy. How to deallocate \c data.
	 * @param data The data array to wrap with this OpData object.
	 * @param length The number of valid/initialized bytes in the \c data
	 *        array. If this is #OpDataUnknownLength (the default),
	 *        op_strlen() will be used to find the length of the \c data
	 *        array. Hence, if you want to embed '\0' bytes within this
	 *        object, you \e must specify \c length.
	 * @param alloc The total size of the allocation starting at \c data.
	 *        Defaults to \c length if not given.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS SetRawData(OpDataDeallocStrategy ds, char *data, size_t length = OpDataUnknownLength, size_t alloc = OpDataUnknownLength)
	{
		OpData d;
		RETURN_IF_ERROR(d.AppendRawData(ds, data, length, alloc));
		*this = d;
		return OpStatus::OK;
	}

	/**
	 * Copy the given \c data array into this OpData object.
	 *
	 * Use this method to make this object refer to a copy of the given
	 * \c data. The ownership of the original data is \e not transferred
	 * to OpData. This is useful when you cannot guarantee the lifetime
	 * of the given \c data.
	 *
	 * Constructs an OpData that copies the first \c length bytes from the
	 * \c data array. The OpData will have its own copy of the given data.
	 *
	 * @param data The data array to copy
	 * @param length The number of bytes to copy from \c data. If this is
	 *        #OpDataUnknownLength (the default), op_strlen() will be used
	 *        to find the length of the \c data array. Hence, if you want
	 *        to embed '\0' bytes within this object, you \e must specify
	 *        \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS SetCopyData(const char *data, size_t length = OpDataUnknownLength)
	{
		OpData d;
		RETURN_IF_ERROR(d.AppendCopyData(data, length));
		*this = d;
		return OpStatus::OK;
	}

	/** @} */

	/**
	 * Clear the contents of this object.
	 *
	 * This will release any storage not shared with other OpData objects.
	 */
	void Clear(void);

	/**
	 * Append the given buffer to this buffer.
	 *
	 * @param other Buffer to add to this buffer.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Append(const OpData& other);

	/**
	 * Append the given byte to this buffer.
	 *
	 * @param c Byte to add to this buffer.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Append(char c);

	/**
	  * Append string to this buffer.
	  *
	  * The string will be copied. Equivalent to AppendCopyData.
	  * @param val '\0'-terminated string to append. If NULL, nothing is appended.
	  * @retval OpStatus::OK on success.
	  * @retval OpStatus::ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS AppendFormatted(const char* val)
	{
		return val ? AppendCopyData(val) : OpStatus::OK;
	}

	/**
	  * Append the data to this buffer.
	  *
	  * @param val Value to append.
	  * @param as_number If true, will append either "0" or "1", otherwise will
	  * append "false" or "true", depending on value of @a val.
	  * @retval OpStatus::OK on success.
	  * @retval OpStatus::ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS AppendFormatted(bool val, bool as_number = false);

	/** Append integer value.
	 *
	 * Will be formatted as with printf's %%d.
	 * @param val Value to append.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AppendFormatted(int val);

	/** Append unsigned integer value.
	 *
	 * Will be formatted as with printf's %%u.
	 * @param val Value to append.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AppendFormatted(unsigned int val);

	/** Append long integer value.
	 *
	 * Will be formatted as with printf's %%ld.
	 * @param val Value to append.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AppendFormatted(long val);

	/** Append long unsigned integer value.
	 *
	 * Will be formatted as with printf's %%lu.
	 * @param val Value to append.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AppendFormatted(unsigned long val);

	/** Append floating point value.
	 *
	 * Will be formatted as with printf's %%g.
	 * @param val Value to append.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AppendFormatted(double val);

	/** Append pointer address.
	 *
	 * Will be formatted as with printf's %%p.
	 * @param val Value to append.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS AppendFormatted(const void* val);

	/**
	 * Append const char * data to this OpData without copying.
	 *
	 * Wrap the given constant byte data in OpData magic, and append it to
	 * this OpData object.
	 *
	 * This OpData object may use the first \c length bytes of the \c data
	 * array. This object may refer to the actual memory referenced by the
	 * \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with op_strlen().
	 *
	 * The caller \e promises that \c data will not be deleted or modified
	 * as long as this OpData and/or any copies (even of only fragments) of
	 * it exist. In other words, because OpData instances implicitly share
	 * the underlying data, the caller must not delete of modify \c data
	 * for as long as this instance and/or any copies of it (or any
	 * sub-string of it) exist.
	 *
	 * OpData does not take ownership of \c data, so the OpData destructor
	 * will never delete \c data, even when the last OpData referring to
	 * \c data is destroyed.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' byte at position \c length.
	 * While '\0'-termination does not matter for OpData, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param data The data array to append to this OpData object without
	 *        copying.
	 * @param length The number of bytes in the \c data array to append. If
	 *        this is #OpDataUnknownLength (the default), op_strlen() will
	 *        be used to find the length of the \c data array. Hence, if you
	 *        want to append '\0' bytes from within \c data, you \e must
	 *        specify \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS AppendConstData(const char *data, size_t length = OpDataUnknownLength);

	/**
	 * Append raw char data with a given deallocation strategy to this object.
	 *
	 * Use this method to make this object wrap the given allocated byte
	 * data, transferring ownership of the data to OpData in the process.
	 * Note that using this method with \c ds == #OPDATA_DEALLOC_NONE is
	 * equivalent to using the above AppendConstData() method, and hence
	 * subject to the same restrictions.
	 *
	 * This OpData object may use the first \c length bytes of the \c data
	 * array. This object may refer to the actual memory referenced by the
	 * \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with op_strlen().
	 *
	 * If the actual allocated \c data array is larger than the given
	 * \c length, specify \c alloc to inform OpData of the length of the
	 * allocation. This enables OpData to use the uninitialized bytes
	 * between \c length and \c alloc, and thus potentially prevent
	 * (needless) reallocations.
	 *
	 * A successful call of this method transfers ownership of the \c data
	 * array to this OpData object; subsequently, the \c data array may only
	 * be accessed via this OpData object, and the caller should treat the
	 * memory at \c data as if it'd just been deallocated.
	 * OpData will automatically deallocate the \c data array when no longer
	 * needed, according to the given deallocation strategy \c ds, and may
	 * in fact have already done so before this method returns. If this
	 * method fails, the caller retains responsibility for deallocating the
	 * \c data array.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' byte at position \c length.
	 * While '\0'-termination does not matter for OpData, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param ds Deallocation strategy. How to deallocate \c data.
	 * @param data The data array to append to this OpData object,
	 *        transferring ownership to OpData in the process.
	 * @param length The number of valid/initialized bytes in the \c data
	 *        array to append to this object. If this is
	 *        #OpDataUnknownLength (the default), op_strlen() will be used
	 *        to find the length of the \c data array. Hence, if you want
	 *        to append '\0' bytes from within \c data, you \e must specify
	 *        \c length.
	 * @param alloc The total size of the allocation starting at \c data.
	 *        Defaults to \c length if not given.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS AppendRawData(OpDataDeallocStrategy ds, char *data, size_t length = OpDataUnknownLength, size_t alloc = OpDataUnknownLength);

	/**
	 * Append a copy of the given \c data array to this OpData object.
	 *
	 * The ownership of the original data is \e not transferred to OpData.
	 * This is useful when you cannot guarantee the lifetime of the given
	 * \c data.
	 *
	 * Copies the first \c length bytes from the \c data array into the end
	 * of this OpData object. This OpData object will have its own copy of
	 * the given data. If the append fails (because of OOM), this OpData
	 * object is left unchanged, and OpStatus::ERR_NO_MEMORY is returned.
	 *
	 * @param data The data array to copy.
	 * @param length The number of bytes to copy from \c data. If this is
	 *        #OpDataUnknownLength (the default), op_strlen() will be used
	 *        to find the length of the \c data array. Hence, if you want
	 *        to append '\0' bytes from within \c data, you \e must specify
	 *        \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS AppendCopyData(const char *data, size_t length = OpDataUnknownLength);

	/**
	 * Expand the given format string, and append it to this object.
	 *
	 * If the method fails for any reason (return value is not
	 * OpStatus::OK), the original contents of this object will be left
	 * unchanged.
	 *
	 * @param format The printf-style format string to expand.
	 * @param ... (resp. args) The list of arguments applied to the format
	 *        string.
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 * @retval OpStatus::ERR on formatting (op_vsnprintf()) error.
	 */
	OP_STATUS AppendFormat(const char *format, ...);
	OP_STATUS AppendVFormat(const char *format, va_list args);

	/**
	 * Retrieve a pointer to be used for appending to this buffer.
	 *
	 * The returned pointer is only valid until the next non-const method
	 * call on this OpData object.
	 *
	 * The returned pointer can be filled with bytes starting from index 0,
	 * and ending before index \c length. Using this pointer to access any
	 * OTHER bytes is forbidden and leads to undefined behavior.
	 *
	 * The OpData object always owns the returned pointer. Trying to free
	 * or delete the pointer is forbidden and leads to undefined behavior.
	 *
	 * IMPORTANT: This method immediately adds \c length bytes to the
	 * length of this buffer. If you do not write all \c length bytes into
	 * the pointer, you need to call #Trunc(Length() - num_unwritten_bytes)
	 * to remove the uninitialized bytes from the end of the buffer. The
	 * contents of the returned pointer is undefined until you fill it with
	 * data. If you forget to fill all \c length bytes (or call #Trunc()
	 * to remove the unfilled bytes), you will end up with uninitialized
	 * data in your buffer.
	 *
	 * This method will allocate as necessary to get a writable memory of
	 * the given size. Thus, you probably don't want to specify a large
	 * \c length value unless you are sure to use most of the allocated
	 * bytes.
	 *
	 * If \c length == 0 is given, this method will return a char * pointer
	 * to the byte following the last byte in this object. (This ensures
	 * that a successful request for space to copy \c length == 0 bytes is
	 * easily distinguished from the NULL return on OOM.) However, the
	 * caller is not allowed to access any bytes using that pointer.
	 *
	 * @param length The number of bytes you wish to append.
	 * @returns Writable pointer for you to fill with up to \c length bytes
	 *          of data to be appended to this buffer. NULL on OOM.
	 */
	char *GetAppendPtr(size_t length);

	/**
	 * Return true iff this OpData object is identical to the given object.
	 *
	 * @param other The other object to compare to this object.
	 * @retval true if the entire contents of \c other is identical to the
	 *         entire the entire contents of this object.
	 * @retval false otherwise.
	 */
	op_force_inline bool Equals(const OpData& other) const
	{
		return other.Length() == Length() && StartsWith(other);
	}

	/** Same as Equals(), but ignores the case of ASCII characters.
	 * @note Using this method makes only sense if both this instance and the
	 *       specified \p other object contain UTF-8 or ASCII strings. It
	 *       doesn't make sense to use this method on arbitrary binary data. */
	op_force_inline bool EqualsCase(const OpData& other) const
	{
		return other.Length() == Length() && StartsWithCase(other);
	}

	/**
	 * Return true iff this OpData object is identical to the given buffer.
	 *
	 * @param buf The buffer to compare to this object.
	 * @param length The length of \c buf. If not given, the length will
	 *        be determined with op_strlen().
	 * @retval true if the first \c length bytes of \c buf is identical to
	 *         the entire contents of this object.
	 * @retval false otherwise.
	 */
	bool Equals(const char *buf, size_t length = OpDataUnknownLength) const;

	/** Same as Equals(), but ignores the case of ASCII characters.
	 * @note Using this method makes only sense if both this instance and the
	 *       specified \p other object contain UTF-8 or ASCII strings. It
	 *       doesn't make sense to use this method on arbitrary binary data. */
	bool EqualsCase(const char* s, size_t length = OpDataUnknownLength) const;

	/**
	 * Comparison operators
	 *
	 * @{
	 */
	op_force_inline bool operator==(const OpData& other) const { return Equals(other); }
	op_force_inline bool operator==(const char *buf) const { return Equals(buf); }

	op_force_inline bool operator!=(const OpData& other) const { return !Equals(other); }
	op_force_inline bool operator!=(const char *buf) const { return !Equals(buf); }
	/** @} */

	/**
	 * Return true iff this object starts with the same contents as \c other.
	 *
	 * Compares the first \c length bytes of this object and \c other.
	 * Return true iff the first \c length bytes are identical.
	 *
	 * If \c length is not given, or greater than other.Length(), the
	 * entire length of \c other is used in the comparison.
	 *
	 * Note that if \e this object is \e shorter than \c length (or
	 * other.Length() when used in its place), this method will return
	 * false. In other words, the comparison is \e not auto-limited to the
	 * length of this object.
	 *
	 * The comparison is performed with op_memcmp(), so only identical byte
	 * values are considered equivalent.
	 *
	 * @param other The object to compare to the start of this object.
	 * @param length The number of bytes to compare.
	 *        If not given, or >= other.Length(), the comparison length will
	 *        be other.Length().
	 * @retval true if the first \c length bytes of \c other is identical to
	 *         the first \c length bytes of this object.
	 * @retval false otherwise.
	 */
	bool StartsWith(const OpData& other, size_t length = OpDataFullLength) const;

	/**
	 * Return true iff this object starts with the given data.
	 *
	 * The comparison is performed with op_memcmp(), so '\0'-bytes in \c buf
	 * (and in the OpData object) are allowed. Note, however, that you will
	 * need to specify \c length in that case, because the default \c length
	 * (using op_strlen()) will abort the comparison before the first
	 * '\0'-byte.
	 *
	 * @param buf The buffer to compare to the start of this object.
	 * @param length The length of \c buf. If not given, the length will
	 *        be determined with op_strlen().
	 * @retval true if the first \c length bytes of \c buf is identical to
	 *         the first \c length bytes of this object.
	 * @retval false otherwise.
	 */
	bool StartsWith(const char *buf, size_t length = OpDataUnknownLength) const;

	/** Same as StartsWith(), but ignores the case of ASCII characters.
	 * @note Using this method makes only sense if both this instance and the
	 *       specified \p other object contain UTF-8 or ASCII strings. It
	 *       doesn't make sense to use this method on arbitrary binary data. */
	bool StartsWithCase(const OpData& other, size_t length = OpDataFullLength) const;
	/** Same as StartsWith(), but ignores the case of ASCII characters.
	 * @note Using this method makes only sense if both this instance and the
	 *       specified \p other object contain UTF-8 or ASCII strings. It
	 *       doesn't make sense to use this method on arbitrary binary data. */
	bool StartsWithCase(const char* buf, size_t length = OpDataUnknownLength) const;

	/** Return true iff this object ends with the same contents as \p other.
	 *
	 * Compares the last \p length bytes of this object and the first \p length
	 * bytes of \p other. Return true iff the bytes are identical.
	 *
	 * If \p length is not given, or greater than other.Length(), the entire
	 * length of \p other is used in the comparison.
	 *
	 * @note If \e this object is \e shorter than \c length (or other.Length()
	 *       when used in its place), this method returns false. In other words,
	 *       the comparison is \e not auto-limited to the length of this object.
	 * @note If the specified \p length (or the length of the \p object) is 0,
	 *       then this method returns \c true - each buffer ends with an empty
	 *       string.
	 *
	 * The comparison is performed with op_memcmp(), so only identical byte
	 * values are considered equivalent.
	 *
	 * @param other The object to compare to the end of this object.
	 * @param length The number of bytes to compare.
	 *        If not given, or >= other.Length(), the comparison length will
	 *        be other.Length().
	 * @retval true if the first \p length bytes of \p other is identical to
	 *         the last \p length bytes of this object.
	 * @retval false otherwise.
	 */
	bool EndsWith(const OpData& other, size_t length = OpDataFullLength) const;

	/** Return true iff this object ends with the given data.
	 *
	 * The comparison is performed with op_memcmp(), so '\0'-bytes in \p buf
	 * (and in the OpData object) are allowed. Note, however, that you will
	 * need to specify \p length in that case, because the default \p length
	 * (using op_strlen()) will abort the comparison before the first
	 * '\0'-byte.
	 *
	 * @param buf The buffer to compare to the end of this object.
	 * @param length The length of \p buf. If not given, the length will
	 *        be determined with op_strlen().
	 * @retval true if the first \p length bytes of \p buf is identical to
	 *         the last \p length bytes of this object.
	 * @retval false otherwise.
	 */
	bool EndsWith(const char* buf, size_t length = OpDataUnknownLength) const;

	/**
	 * Get the byte at the given index into this buffer.
	 *
	 * Caller MUST ensure 0 <= \c index < Length().
	 *
	 * @param index Index into this buffer of byte to return.
	 * @returns Byte at the given \c index.
	 */
	const char& At(size_t index) const;

	/**
	 * Index operator
	 */
	op_force_inline const char& operator[](size_t index) const { return At(index); }

	/**
	 * Find the first occurrence of the given byte (sequence) in this buffer.
	 *
	 * Return the smallest index i in \c offset <= i < \c length for which
	 * the given \c needle is found at index i (i.e. this->[i] == \c needle
	 * for the char version of this method, or op_memcmp(this->Data() + i,
	 * \c needle, \c needle_len) == 0 for the const char * version). In the
	 * const char * version of this method, the \e entire \c needle must be
	 * fully contained within the search area (haystack) in order to be
	 * found. If no such i is found, return #OpDataNotFound.
	 *
	 * If \c offset is not given, the search starts at the beginning of this
	 * buffer. If \c length is not given, the search stops at the end of
	 * this buffer.
	 *
	 * In the const char * version of this method, if \c needle_len ==
	 * #OpDataUnknownLength (the default), the length of \c needle is
	 * computed with op_strlen(). Hence, if a '\0' byte is part of your
	 * needle, you must specify \c needle_len. If \c needle_len is 0 (or
	 * becomes 0 after calling op_strlen()), the search is always successful
	 * (even in an empty buffer) unless \c offset exceeds Length(). In other
	 * words, the empty \c needle matches at every index in the range from
	 * \c offset to \c length (or Length()), even if there are no bytes.
	 *
	 * @param needle The byte or byte sequence to search for.
	 * @param needle_len The length of the \c needle. If not given, the
	 *          length of the needle will be found with op_strlen().
	 * @param offset Index into this buffer, indicating start of search
	 *          area (haystack). Defaults to start of buffer if not given.
	 * @param length The length (in bytes) of the haystack, starting from
	 *          \c offset. Defaults to the rest of this buffer if not given.
	 * @returns The buffer index (i.e. relative to the start of this buffer,
	 *          \e not relative to \c offset) of (the start of) the first
	 *          occurrence of the \c needle, or #OpDataNotFound if not found.
	 *
	 * @{
	 */
	size_t FindFirst(
		const char *needle, size_t needle_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	size_t FindFirst(char needle, size_t offset = 0, size_t length = OpDataFullLength) const;
	/** @} */

	/**
	 * Find the first occurrence of any byte in the given \c accept set.
	 *
	 * Return the smallest index i in \c offset <= i < \c length for which
	 * any byte in the given \c accept set is found at index i (i.e.
	 * this->[i] is a byte in \c accept). If no such i is found, return
	 * #OpDataNotFound.
	 *
	 * If \c offset is not given, the search starts at the beginning of this
	 * buffer. If \c length is not given, the search stops at the end of
	 * this buffer.
	 *
	 * If \c accept_len == #OpDataUnknownLength (the default), the length
	 * of \c accept (i.e. the number of bytes in the \c accept set) is
	 * computed with op_strlen(). Hence, if a '\0' byte is part of your
	 * \c accept set, you must specify \c accept_len.
	 *
	 * @param accept The set of bytes to search for.
	 * @param accept_len The number of bytes in the \c accept set. If not
	 *          given, the size of the set will be found with op_strlen().
	 * @param offset Index into this buffer, indicating start of search
	 *          area (haystack). Defaults to start of buffer if not given.
	 * @param length The length (in bytes) of the haystack, starting from
	 *          \c offset. Defaults to the rest of this buffer if not given.
	 * @returns The buffer index (i.e. relative to the start of this buffer,
	 *          \e not relative to \c offset) of the first occurrence of any
	 *          byte in \c accept, or #OpDataNotFound if not found.
	 */
	size_t FindFirstOf(
		const char *accept, size_t accept_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	/**
	 * Find the first occurrence of any byte \e not in the given \c ignore
	 * set.
	 *
	 * Return the smallest index i in \c offset <= i < \c length for which
	 * any byte \e not in the given \c ignore set is found at index i (i.e.
	 * this->[i] is not a byte in \c ignore). If no such i is found (i.e.
	 * if all the bytes searched are in \c ignore), return #OpDataNotFound.
	 *
	 * If \c offset is not given, the search starts at the beginning of this
	 * buffer. If \c length is not given, the search stops at the end of
	 * this buffer.
	 *
	 * If \c ignore_len == #OpDataUnknownLength (the default), the length
	 * of \c ignore (i.e. the number of bytes in the \c ignore set) is
	 * computed with op_strlen(). Hence, if a '\0' byte is part of your
	 * \c ignore set, you must specify \c ignore_len.
	 *
	 * @param ignore The set of bytes to ignore while searching.
	 * @param ignore_len The number of bytes in the \c ignore set. If not
	 *          given, the size of the set will be found with op_strlen().
	 * @param offset Index into this buffer, indicating start of search
	 *          area (haystack). Defaults to start of buffer if not given.
	 * @param length The length (in bytes) of the haystack, starting from
	 *          \c offset. Defaults to the rest of this buffer if not given.
	 * @returns The buffer index (i.e. relative to the start of this buffer,
	 *          \e not relative to \c offset) of the first occurrence of any
	 *          byte \e not in \c ignore, or #OpDataNotFound if none found.
	 */
	size_t FindEndOf(
		const char *ignore, size_t ignore_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	/**
	 * Find the last occurrence of the given byte (sequence) in this buffer.
	 *
	 * Return the highest index i in \c offset <= i < \c length for which
	 * the given \c needle is found at index i (i.e. this->[i] == \c needle
	 * for the char version of this method, or op_memcmp(this->Data() + i,
	 * \c needle, \c needle_len) == 0 for the const char * version). In the
	 * const char * version of this method, the \e entire \c needle must be
	 * fully contained within the search area (haystack) in order to be
	 * found. If no such i is found, return #OpDataNotFound.
	 *
	 * If \c offset is not given, the search area starts at the beginning of
	 * this buffer. If \c length is not given, the search area stops at the
	 * end of this buffer. Note that the search is reversed, and proceeds
	 * backwards from \c offset + \c length towards \c offset.
	 *
	 * In the const char * version of this method, if \c needle_len ==
	 * #OpDataUnknownLength (the default), the length of \c needle is
	 * computed with op_strlen(). Hence, if a '\0' byte is part of your
	 * needle, you must specify \c needle_len. If \c needle_len is 0 (or
	 * becomes 0 after calling op_strlen()), the search is always successful
	 * (even in an empty buffer) unless \c offset exceeds Length(). In other
	 * words, the empty \c needle matches at every index in the range from
	 * \c offset to \c length (or Length()), even if there are no bytes.
	 *
	 * @param needle The byte or byte sequence to search for.
	 * @param needle_len The length of the \c needle. If not given, the
	 *          length of the needle will be found with op_strlen().
	 * @param offset Index into this buffer, indicating start of search
	 *          area (haystack). Defaults to start of buffer if not given.
	 * @param length The length (in bytes) of the haystack, starting from
	 *          \c offset. Defaults to the rest of this buffer if not given.
	 * @returns The buffer index (i.e. relative to the start of this buffer,
	 *          \e not relative to \c offset) of (the start of) the last
	 *          occurrence of the \c needle, or #OpDataNotFound if not found.
	 *
	 * @{
	 */
	size_t FindLast(
		const char *needle, size_t needle_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	size_t FindLast(char needle, size_t offset = 0, size_t length = OpDataFullLength) const;
	/** @} */

	/**
	 * Consume the given number of bytes from the start of this buffer.
	 *
	 * Remove \c length bytes from the start of the buffer.
	 * If \c length is the current buffer length or greater, the buffer will
	 * become empty.
	 *
	 * @param length Consume this many bytes from the start of the buffer.
	 */
	void Consume(size_t length);

	/**
	 * Truncate this buffer to the given \c new_length.
	 *
	 * Remove all bytes starting from index \c new_length, and to the end
	 * of the buffer. If \c new_length is the current buffer length or
	 * greater, nothing is truncated. Passing \c new_length == 0 is
	 * equivalent to calling Clear().
	 *
	 * @param new_length Truncate buffer to this length.
	 */
	void Trunc(size_t new_length);

	/**
	 * Delete \c length bytes starting at \c offset from this buffer.
	 *
	 * @param offset The index of the first byte to delete.
	 * @param length The number of bytes to delete.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this OpData
	 *         shall remain unchanged.
	 */
	OP_STATUS Delete(size_t offset, size_t length)
	{
		OpData result(*this, 0, offset);
		RETURN_IF_ERROR(result.Append(OpData(*this, offset + length)));
		*this = result;
		return OpStatus::OK;
	}

	/**
	 * Remove the given byte (sequence) from this buffer.
	 *
	 * Remove one or more instances of the given byte (or byte sequence)
	 * from this OpData object. By default, all instances of the given byte
	 * (sequence) will be removed from this buffer. Specify \c maxremove to
	 * limit the number of instances to be removed. Removal proceeds from
	 * the start of this buffer, so specifying e.g. \c maxremove == 5 will
	 * remove the first 5 instances occurring in the buffer.
	 *
	 * @param remove The byte or byte sequence to remove from this buffer.
	 * @param length The length of \c remove. If not given, the length of
	 *        \c remove will be determined by op_strlen().
	 * @param maxremove The maximum number of removals to perform.
	 *        Unlimited by default.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this OpData
	 *         shall remain unchanged.
	 *
	 * @{
	 */
	OP_STATUS Remove(const char *remove, size_t length = OpDataUnknownLength, size_t maxremove = OpDataFullLength);

	OP_STATUS Remove(char remove, size_t maxremove = OpDataFullLength);
	/** @} */

	/** Replace the \p character with the specified \p replacement in this
	 * buffer.
	 *
	 * Replaces one or more instances of the given \c character in this
	 * OpData object with the specified \c replacement. By default, all
	 * instances of the \p character will be replaced in this buffer.
	 * Specify \p maxreplace to limit the number of instances to be replaced.
	 * Replacement proceeds from the start of this string, so specifying e.g.
	 * \p maxreplace == 5 will replace the first 5 instances occurring in this
	 * string.
	 *
	 * @param character The character to replace in this buffer.
	 * @param replacement The character to replace \p character.
	 * @param maxreplace The maximum number of replacements to perform.
	 *        Unlimited by default.
	 * @param[out] replaced May point to a size_t. If such a pointer is
	 *        specified, the size_t receives on success the number of
	 *        replacements that were performed.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this OpData
	 *         shall remain unchanged.
	 */
	OP_STATUS Replace(char character, char replacement, size_t maxreplace = OpDataFullLength, size_t* replaced = NULL);

	/** Replace \p substring with the specified \p replacement in this buffer.
	 *
	 * Replaces one or more instances of the given \p substring in this
	 * OpData object with the specified \p replacement. By default, all
	 * instances of the \p substring will be replaced in this buffer.
	 * Specify \p maxreplace to limit the number of instances to be replaced.
	 * Replacement proceeds from the start of this buffer, so specifying e.g.
	 * \p maxreplace == 5 will replace the first 5 instances occurring in this
	 * buffer.
	 *
	 * @param substring The sub-string to replace in this buffer. If the
	 *        \p substring is NULL or an empty string, then this implementation
	 *        does not modify this buffer and OpStatus::ERR is returned.
	 * @param replacement The string which replaces the \p substring. If the
	 *        \p replacement is NULL or an empty string, the sub-string is
	 *        removed.
	 * @param maxreplace The maximum number of replacements to perform.
	 *        Unlimited by default.
	 * @param[out] replaced May point to a size_t. If such a pointer is
	 *        specified, the size_t receives on success the number of
	 *        replacements that were performed.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR if substring is NULL or an empty string.
	 *         OpStatus::ERR_NO_MEMORY on OOM. On returning an error status this
	 *         UniString shall remain unchanged.
	 */
	OP_STATUS Replace(const char* substring, const char* replacement, size_t maxreplace = OpDataFullLength, size_t* replaced = NULL);

	/**
	 * Strip leading/trailing whitespace from this object.
	 *
	 * Whitespace means any character for which the op_isspace() function
	 * returns true. This includes the ASCII characters '\\t', '\\n', '\\v',
	 * '\\f', '\\r', and ' '.
	 *
	 * @{
	 */
	void LStrip(void); ///< Strip leading whitespace
	void RStrip(void); ///< Strip trailing whitespace
	void Strip(void); ///< Strip both leading and trailing whitespace
	/** @} */

	/**
	 * Split the data into segments wherever \c sep occurs.
	 *
	 * Returns a list of those segments (as OpData objects). If \c sep does
	 * not match anywhere in the data, Split() returns a single-element
	 * list containing the entire data. Each of the returned segments refers
	 * to one or more fragments of the source data.
	 *
	 * If \c maxsplit is given, do not split more than that number of times
	 * (yielding up to \c maxsplit + 1 elements in the returned list).
	 * Split() proceeds from the start of the buffer. If there are more
	 * than \c maxsplit separators present, the remaining separators will
	 * be part of the last element of the returned list.
	 *
	 * Examples:
	 *
	 *   @code
	 *   OpData d = OpData::FromConstDataL("12|34|567|8||9|0|");
	 *   d.Split('|');
	 *   @endcode
	 *   will return: ["12", "34", "567", "8", "", "9", "0", ""]
	 *
	 *   @code
	 *   d.Split('|', 3);
	 *   @endcode
	 *   will return: ["12", "34", "567", "8||9|0|"]
	 *
	 *   @code
	 *   d.Split('|', 6);
	 *   @endcode
	 *   will return: ["12", "34", "567", "8", "", "9", "0|"]
	 *
	 *   @code
	 *   d.Split('|', 0);
	 *   @endcode
	 *   will return: ["12|34|567|8||9|0|"]
	 *
	 *   @code
	 *   d.Split('X', 6);
	 *   @endcode
	 *   will return: ["12|34|567|8||9|0|"]
	 *
	 * If the given separator is empty (i.e. \c length == 0, or \c length
	 * == OpDataUnknownLength and \c sep[0] == '\0'), Split() will split
	 * between every byte (limited by \c maxsplit, if given), and the
	 * resulting list will have N elements, each one byte long, where N ==
	 * this->Length().
	 *
	 * Examples:
	 *
	 *   @code
	 *   OpData d = OpData::FromConstDataL("abcdefghij");
	 *   d.Split("ef")
	 *   @endcode
	 *   will return: ["abcd", "ghij"]
	 *
	 *   @code
	 *   d.Split("");
	 *   @endcode
	 *   will return: ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"]
	 *
	 *   @code
	 *   d.Split("foo", 0, 3);
	 *   @endcode
	 *   will return: ["a", "b", "c", "defghij"]
	 *
	 * The returned OtlCountedList object is heap-allocated and must be
	 * deleted by the caller, using OP_DELETE(). On OOM, NULL is returned.
	 *
	 * @param sep The byte or byte sequence on which to split this data.
	 * @param length The length of \c sep. If not given, the length of
	 *        \c sep will be determined by op_strlen().
	 * @param maxsplit The maximum number of splits to perform. The
	 *        returned list will have up to (\c maxsplit + 1) elements.
	 *        If not given, split until there are no separators left.
	 * @returns List of segments (substrings of this OpData instance) after
	 *          splitting on \c sep. Individual segments may be empty (e.g.
	 *          when \c sep immediately follows another \c sep). Returned
	 *          list must be OP_DELETE()d by caller. Returns NULL on OOM.
	 *
	 * @{
	 */
	OtlCountedList<OpData> *Split(const char *sep, size_t length = OpDataUnknownLength, size_t maxsplit = OpDataFullLength) const;

	OtlCountedList<OpData> *Split(char sep, size_t maxsplit = OpDataFullLength) const;
	/** @} */

	/**
	 * Split the data into segments wherever any character in \p sep occurs.
	 *
	 * Returns a list of split segments (as OpData objects). If the data
	 * does not contain any character found in \p sep, SplitAtAnyOf() returns a
	 * single-element list containing the entire string. Each of the returned
	 * segments refers to one or more fragments of the source data.
	 *
	 * If \p maxsplit is given, do not split more than that number of times
	 * (yielding up to \p maxsplit + 1 elements in the returned list).
	 * SplitAtAnyOf() proceeds from the start of the data. If there are
	 * more than \p maxsplit separators present, the remaining separators will
	 * be part of the last element of the returned list.
	 *
	 * Examples:
	 *
	 *   @code
	 *   OpData d = OpData::FromConstDataL("12;34,567,8;,9,0,");
	 *   d.SplitAtAnyOf(";,");
	 *   @endcode
	 *   will return: ["12", "34", "567", "8", "", "9", "0", ""]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(";,", 3);
	 *   @endcode
	 *   will return: ["12", "34", "567", "8;,9,0,"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(";,", 6);
	 *   @endcode
	 *   will return: ["12", "34", "567", "8", "", "9", "0,"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(";,", 0);
	 *   @endcode
	 *   will return: ["12;34,567,8;,9,0,"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf("X", 6);
	 *   @endcode
	 *   will return: ["12;34,567,8;,9,0,"]
	 *
	 * If the given separator is empty (i.e. \p length == 0, or \p length ==
	 * OpDataUnknownLength and \c sep[0] == '\0'), SplitAtAnyOf() will not split
	 * the data. The resulting list will have one element containing the
	 * complete data.
	 *
	 *   @code
	 *   OpData d = OpData::FromConstDataL("abcdefghij");
	 *   d.SplitAtAnyOf("ef");
	 *   @endcode
	 *   will return: ["abcd", "", "ghij"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf("");
	 *   @endcode
	 *   will return: ["abcdefghij"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf("foo", 0, 3);
	 *   @endcode
	 *   will return: ["abcdefghij"]
	 *
	 * The returned OtlCountedList object is heap-allocated and must be
	 * deleted by the caller, using OP_DELETE(). On OOM, NULL is returned.
	 *
	 * @param sep The set of characters on which to split this data.
	 * @param length The length of \p sep. If not given, the length of \p sep
	 *        will be determined by op_strlen().
	 * @param maxsplit The maximum number of splits to perform. The returned
	 *        list will have up to (\c maxsplit + 1) elements. If not given,
	 *        split until there are no separators left.
	 * @returns List of segments (substrings of this OpData instance) after
	 *          splitting on \c sep. Individual segments may be empty (e.g. when
	 *          characters in \p sep occur consecutively). Returned list must be
	 *          OP_DELETE()d by caller. Returns NULL on OOM.
	 */
	OtlCountedList<OpData>* SplitAtAnyOf(const char* sep, size_t length = OpDataUnknownLength, size_t maxsplit = OpDataFullLength) const;

	/**
	 * Copy \c length bytes of data from this object into the given \c ptr.
	 *
	 * If this object holds fewer than the given \c length bytes, only the
	 * bytes availble (== Length() - offset) will be copied.
	 *
	 * The caller maintains full ownership of the given \c ptr, and
	 * promises that the given \c ptr can hold the given \c length number
	 * of bytes,
	 *
	 * Note that no '\0'-termination will be performed (unless this OpData
	 * object happens to contain '\0' bytes).
	 *
	 * @param ptr The pointer into which data from this object is copied.
	 * @param length The maximum number of bytes to copy into \c ptr.
	 * @param offset Start copying at this offset into this object's data.
	 * @returns The actual number of bytes copied into \c ptr. This number
	 *          is guaranteed to be <= \c length.
	 */
	size_t CopyInto(char *ptr, size_t length, size_t offset = 0) const;

	/**
	 * Return a const pointer to the raw data kept within this object.
	 *
	 * The returned pointer will be non-NULL even if this OpData is empty.
	 * Only if OOM is encountered while preparing the pointer will NULL be
	 * returned.
	 *
	 * The returned pointer is only valid until the next non-const method
	 * call on this OpData object.
	 *
	 * Use the \c nul_terminated argument to control whether the returned
	 * data must be '\0'-terminated or not.
	 *
	 * @param nul_terminated Set to true if you need the return data array
	 *        to be '\0'-terminated.
	 * @retval Pointer to const data contents of this OpData object. Even
	 *         if the OpData is empty, the returned pointer will be
	 *         non-NULL.
	 * @retval NULL on OOM.
	 */
	const char *Data(bool nul_terminated = false) const;

	/**
	 * Create a const pointer to the raw data kept within this object.
	 *
	 * This will consolidate the underlying data into a single contiguous
	 * memory area, and store a pointer to that memory in \c ptr, before
	 * returning OpStatus::OK.
	 *
	 * Use the \c nul_terminators argument to control whether the resulting
	 * pointer must be '\0'-terminated (and if so, how many '\0'-terminators
	 * are needed).
	 *
	 * If OOM occurs while consolidating the underlying data,
	 * OpStatus::ERR_NO_MEMORY will be returned, and \c ptr will not be
	 * touched.
	 *
	 * IMPORTANT: The pointer stored in \c ptr is only valid until the next
	 * non-const method call (or CreatePtr() call with a larger
	 * \c nul_terminators value) on this OpData object. OpData guarantees
	 * that repeated calls to CreatePtr() (with the same or smaller
	 * \c nul_terminators value, and with no intervening non-const method
	 * calls) will result in the same pointer being stored into \c ptr.
	 *
	 * @param ptr The pointer to the consolidated data is stored here.
	 * @param nul_terminators Set to 0 if you don't need '\0'-termination,
	 *        1 if you need a single '\0'-terminator byte, or more if you
	 *        for some reason need more trailing '\0' bytes.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS CreatePtr(const char **ptr, size_t nul_terminators = 0) const;

	/**
	 * Interpret ASCII data as a signed/unsigned short/int/long integer.
	 *
	 * OpData wrapper around the op_strto*() family of functions.
	 *
	 * Converts the initial bytes - starting at \c offset (the \e start of
	 * this OpData object by default) - from ASCII characters to the
	 * requested integer type. The parsed integer is returned in \c ret, and
	 * the number of bytes parsed (not including \c offset) is returned in
	 * \c length. The value stored in \c length is guaranteed to be
	 * <= Length() - \c offset.
	 *
	 * As with the op_strto*() functions, leading ASCII whitespace is
	 * ignored, and parsing stops at the first byte not part of the number.
	 * See the op_strto*() documentation for more information on the rules
	 * regarding the formatting of the given integer ASCII string.
	 *
	 * If (and only if) \c length is NULL (the default), this entire object
	 * (modulo leading/trailing whitespace) must consist only of the parsed
	 * number, or OpStatus::ERR shall be returned. In other words, the data
	 * between the index that would otherwise be stored into \c length, and
	 * the end of this object MUST consist entirely of whitespace (according
	 * to op_isspace()). (Note that trailing whitespace is not ignored if
	 * \c length is given.)
	 *
	 * If no number can be parsed, an appropriate OpStatus error code is
	 * returned, and the value pointed to by \c ret shall be undefined.
	 * If the part of the string that is parsed for a number is empty (or
	 * contains only whitespace characters or a "-"), then no number can be
	 * parsed and OpStatus::ERR is returned.
	 *
	 * @param[out] ret The parsed integer is stored here. On failure, the
	 *        value is undefined and should be ignored. (This parameter is
	 *        never read, only written to.)
	 * @param[out] length The number of bytes parsed (relative to \c offset)
	 *        is stored here. On failure, the current parsing position
	 *        (typically 0 unless ERR_OUT_OF_RANGE is returned) will be
	 *        stored here. If NULL (the default), an error will be returned
	 *        unless this \e entire object (starting from \c offset) is
	 *        successfully parsed. (This parameter is never read, only
	 *        written to.)
	 * @param base The base to use while parsing. Must be 0 or
	 *        2 <= \c base <= 36. If 0, the base is determined automatically
	 *        using the the following rules: If the data starts with "0x",
	 *        assume hexadecimal (base 16); if it starts with "0", assume
	 *        octal (base 8); otherwise assume decimal (base 10).
	 * @param offset The index into this object where parsing should start.
	 *        Defaults to the start of this object.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if parsing failed because of OOM.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the parsed integer cannot be
	 *         represented by \c ret.
	 * @retval OpStatus::ERR if parsing failed for some other reason.
	 * @{
	 */
	OP_STATUS ToShort(short *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	OP_STATUS ToInt(int *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	OP_STATUS ToLong(long *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	OP_STATUS ToUShort(unsigned short *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	OP_STATUS ToUInt(unsigned int *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	OP_STATUS ToULong(unsigned long *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	/*
	 * Import API_STD_64BIT_STRING_CONVERSIONS to get the following methods,
	 * as they depend on op_strtoi64() and op_strtoui64(), respectively.
	 * Also, see CORE-40606.
	 */
#ifdef STDLIB_64BIT_STRING_CONVERSIONS
	OP_STATUS ToINT64(INT64 *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	OP_STATUS ToUINT64(UINT64 *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;
#endif // STDLIB_64BIT_STRING_CONVERSIONS
	/** @} */

	/**
	 * Interpret ASCII data as a double.
	 *
	 * OpData wrapper around the op_strtod() function.
	 *
	 * Converts the initial bytes - starting at \c offset (the \e start of
	 * this OpData object by default) - from ASCII characters to a floating-
	 * point double value. The parsed double value is returned in \c ret,
	 * and the number of bytes parsed (not including \c offset) is returned
	 * in \c length. The value stored in \c length is guaranteed to be
	 * <= Length() - \c offset.
	 *
	 * As with the op_strtod() function, leading ASCII whitespace is
	 * ignored, and parsing stops at the first byte not part of the number.
	 * See the op_strtod() documentation for more information on the rules
	 * regarding the formatting of the given floating-point number ASCII
	 * string.
	 *
	 * If (and only if) \c length is NULL (the default), this entire object
	 * (modulo leading/trailing whitespace) must consist only of the parsed
	 * number, or OpStatus::ERR shall be returned. In other words, the data
	 * between the index that would otherwise be stored into \c length, and
	 * the end of this object MUST consist entirely of whitespace (according
	 * to op_isspace()). (Note that trailing whitespace is not ignored if
	 * \c length is given.)
	 *
	 * If no number can be parsed, an appropriate OpStatus error code is
	 * returned, and the value pointed to by \c ret shall be undefined.
	 *
	 * @param[out] ret The parsed double is stored here. On failure, the
	 *        value is undefined and should be ignored. (This parameter is
	 *        never read, only written to.)
	 * @param[out] length The number of bytes parsed (relative to \c offset)
	 *        is stored here. On failure, the current parsing position
	 *        (typically 0 unless ERR_OUT_OF_RANGE is returned) will be
	 *        stored here. If NULL (the default), an error will be returned
	 *        unless this \e entire object (starting from \c offset) is
	 *        successfully parsed. (This parameter is never read, only
	 *        written to.)
	 * @param offset The index into this object where parsing should start.
	 *        Defaults to the start of this object.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY if parsing failed because of OOM.
	 * @retval OpStatus::ERR_OUT_OF_RANGE if the parsed floating-point
	 *         number cannot be represented by \c ret.
	 * @retval OpStatus::ERR if parsing failed for some other reason.
	 */
	OP_STATUS ToDouble(double *ret, size_t *length = NULL, size_t offset = 0) const;

#ifdef OPDATA_DEBUG
	/**
	 * Produce a debug-friendly view of this OpData instance.
	 *
	 * Return an ASCII string containing the data stored in this instance,
	 * but also make the internal structure visible. Non-printable or
	 * non-ASCII characters are shown using escape codes of the form "\x##",
	 * where "##" indicate the byte value in hexadecimal notation. E.g.:
	 * "\xde\xad\xbe\xef".
	 *
	 * Instances in embedded mode are formatted like this:
	 * @code
	 *   {'DATA'}
	 * @endcode
	 * where the '{}' curly brackets indicate embedded mode.
	 *
	 * Instances in normal mode are formatted like this:
	 * @code
	 *   [2+'DATA'] -> ['MORE DATA'] -> ['FINAL DATA'+17]
	 * @endcode
	 * where the '[]' square brackets delimit each referenced fragment
	 * in normal mode, a leading '#+' (a decimal number followed by plus
	 * sign) within specifies the \#bytes skipped from the start of the
	 * first fragment (== OpData::n.first_offset), and a trailing '+#' (plus
	 * sign followed by a decimal number) specifies the \#bytes skipped
	 * from the end of the last fragment
	 * (== fragment->Length() - OpData::n.last_length).
	 *
	 * Thus, the bytes that are shown, are exactly those that are part of
	 * this OpData object, and the leading/trailing numbers indicate the
	 * remaining bytes in the first/last fragments that are unused by this
	 * instance (but may be in use by other OpData instances).
	 *
	 * The returned string has been allocated by this method, using
	 * op_malloc(). It is the responsibility of the caller to deallocate it
	 * using op_free().
	 *
	 * @param escape_nonprintables Whether or not non-printable and
	 *        non-ASCII bytes should be escaped in the output like this:
	 *        "\xde\xad\xbe\xef".
	 *        The following bytes are considered non-printable:
	 *         - bytes >= 0x80 (outside ASCII).
	 *         - any byte for which op_isprint() returns false.
	 *        Also, the single quote and backslash characters will be
	 *        escaped as "\'" and "\\", respectively.
	 *        You would usually set this to false only if you know that the
	 *        bytes stored in the OpData object are compatible with your
	 *        console encoding, e.g. if you have a UTF-8 console and you're
	 *        debugging an OpData object conatining UTF-8 data.
	 * @returns A string containing a debug-friendly view of this object.
	 *          NULL on OOM. Note that an empty OpData object necessarily
	 *          is embedded, hence displays as "{''}".
	 */
	char *DebugView(bool escape_nonprintables = true) const;
#endif // OPDATA_DEBUG

private:
	/**
	 * Internal constructor for normal mode objects
	 *
	 * The caller is responsible for calling ->Use() on each fragment in
	 * the given fragment list.
	 */
	OpData(
		OpDataFragment *first,
		size_t first_offset,
		OpDataFragment *last,
		size_t last_length,
		size_t length);

	/**
	 * Internal constructor for embedded mode object.
	 */
	OpData(const char *data, size_t length)
	{
		OP_ASSERT(CanEmbed(length));
		SetMeta(length);
		op_memcpy(e.data, data, length);
	}

	/**
	 * Return a writable ptr at the end of this buffer without allocation.
	 *
	 * This is a primarily a helper method for GetAppendPtr(), to handle
	 * the cases where we can produce an append-pointer without actually
	 * allocating any data.
	 *
	 * Special case: If the given \c length is 0, this method will grab as
	 * much data as is available at the end of the buffer (without
	 * allocation), and update \c length accordingly.
	 *
	 * The returned pointer can be filled with bytes starting from index 0,
	 * and ending before index \c length. Using this pointer to access any
	 * OTHER bytes is forbidden and leads to undefined behavior.
	 *
	 * The OpData object always owns the returned pointer. Trying to free
	 * or delete the pointer is forbidden and leads to undefined behavior.
	 *
	 * This method adds \c length uninitialized bytes to the length of this
	 * buffer. If you do not write all \c length bytes into the pointer,
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
	char *GetAppendPtr_NoAlloc(size_t& length);

	/**
	 * Frontend for Consolidate(), for retrieving a pointer into a single,
	 * possibly '\0'-terminated chunk of this object's data.
	 *
	 * Ensures that repeated calls to this method with the same \c nuls
	 * value will return the same pointer, and not cause reallocation.
	 *
	 * @param nuls How many '\0'-bytes to use in termination. Usually 0 or
	 *        1, but see UniString for a different use case.
	 * @returns A pointer to the consolidated data buffer, or NULL on OOM.
	 */
	const char *CreatePtr(size_t nuls) const;

	/**
	 * Consolidate this instance into a single '\0'-terminated chunk.
	 *
	 * This is expensive, but often necessary when a single pointer to the
	 * referenced data is required.
	 *
	 * @param nuls How many '\0'-bytes to use in termination. Usually 0 or
	 *        1, but see UniString for a different use case.
	 * @returns A pointer to the consolidated data buffer, or NULL on OOM.
	 */
	char *Consolidate(size_t nuls) const;

	/**
	 * Add the data between \c offset and \c length in \c frag to this
	 * OpData object without requiring allocation of memory.
	 *
	 * This method handles the special case where we're iterating over an
	 * existing fragment list, and progressively extend this object to
	 * comprise a run of the iterated data.
	 *
	 * In order for this to work without allocating any new fragments, the
	 * data to be added must already follow immediately after the data
	 * already referenced by this object. In other words, at least one of
	 * the following conditions must hold:
	 *
	 *  1.     \c frag == \c this->n.last
	 *     AND \c offset == \c this->n.last_length
	 *     (i.e. we extend the data referenced in this object's last
	 *      fragment).
	 *
	 *  2.     \c frag == \c this->n.last->Next() (or we can make it so
	 *                             because \c this->n.last->Next() == NULL)
	 *     AND \c this->n.last_length == \c this->n.last->Length()
	 *     AND \c offset == 0
	 *     (i.e. we extend from the end of our last fragment into the start
	 *      of the given fragment).
	 *
	 *  3. \c this->IsEmpty()
	 *     (i.e. the trivial case of initializing this object with a new
	 *      run of data).
	 *
	 * This method will test each condition, and OP_ASSERT() if none of them
	 * are met. It is therefore the caller's responsibility to ensure that
	 * at least one of the conditions are met, and that this object can be
	 * safely extended into the given fragment.
	 *
	 * @param frag The fragment containing the data to extend into.
	 * @param offset The offset into \c frag indicating the start of the
	 *        data to extend into.
	 * @param length The number of bytes, starting from \c offset to extend
	 *        into this object.
	 *        Required: \c offset + \c length <= \c frag->Length().
	 */
	void ExtendIntoFragment(OpDataFragment *frag, size_t offset, size_t length);

	/**
	 * Append the given fragment list to this object.
	 *
	 * This method will call Use() on the fragments in the given list.
	 *
	 * If this object's current fragments need reallocation in order to
	 * append the given fragment list, then that will be done (with
	 * Release() being called on the current fragments, accordingly).
	 *
	 * This method may also be called on an object in embedded mode, in
	 * which case the embedded data will be reallocated into a chunk, and
	 * this object converted into normal mode, before the given fragments
	 * are appended.
	 *
	 * @param first The first fragment to append. This fragment may need to
	 *        be copied, in which case, Use() will \e not be called
	 *        for this fragment (although Use() \e will be called for its
	 *        copy, and for all subsequent fragments until \c last.)
	 * @param first_offset The offset into the \c first fragment where the
	 *        data-to-append starts.
	 * @param last The last fragment to append. OpData::n.last will be set
	 *        to this before we return.
	 * @param last_length The offset into the \c last fragment where the
	 *        data-to-append ends. OpData::n.last_length will be set to this
	 *        before we return. Note that in the case where the \c first
	 *        and \c last fragments are the same, the \c last_length is
	 *        relative to the \e start of the fragment, and NOT relative to
	 *        \c first_offset.
	 * @param length The total number of bytes being appended. This must
	 *        match the number of bytes between offset \c first_offset into
	 *        the \c first fragment, and offset \c last_length into the
	 *        \c last fragment.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this OpData
	 *         shall remain unchanged.
	 */
	OP_STATUS AppendFragments(
		OpDataFragment *first,
		size_t first_offset,
		OpDataFragment *last,
		size_t last_length,
		size_t length);

#ifdef OPDATA_DEBUG
	/**
	 * Perform the duties of DebugView(), with the given debug data printer
	 * function, and the given unit size divisor (1 for bytes, 2 for
	 * uni_chars, etc.).
	 *
	 * See modules/opdata/src/DebugViewFormatter.h for the class declaration
	 * of DebugViewFormatter.
	 */
	char *DebugView(class DebugViewFormatter *formatter, size_t unit_size = 1) const;
#endif // OPDATA_DEBUG

	/**
	 * Find a needle in this buffer.
	 *
	 * Uses \c nf->FindNeedle() to find the first occurrence of some needle
	 * in this OpData buffer, starting at \c offset and ending \c length
	 * bytes later.
	 *
	 * Traverses the fragments of this buffer, invoking \c nf->FindNeedle()
	 * on each fragment successively, until either non-NULL is returned from
	 * \c nf->FindNeedle(), or the \c length bytes have been traversed.
	 *
	 * Returns the index of the start of the first needle found by
	 * \c nf->FindNeedle(), or #OpDataNotFound if no needle was found in the
	 * \c length bytes following \c offset. The returned index is relative
	 * to the start of the object, \e not relative to \c offset.
	 *
	 * See modules/opdata/src/NeedleFinder.h for the class declaration of
	 * NeedleFinder (\c nf).
	 *
	 * @param nf The #NeedleFinder instance to use for finding the first
	 *        occurrence of some needle, starting at \c offset.
	 * @param[in,out] offset The index into this OpData buffer where the
	 *        search should start.
	 * @param[in,out] length The length of the search area, starting from
	 *        \c offset.
	 * @returns The index (relative to the start of this object) of the
	 *          first match returned from \c nf->FindNeedle(),
	 *          or #OpDataNotFound if no match was found.
	 */
	size_t Find(class NeedleFinder *nf, size_t offset, size_t length) const;

	/**
	 * Split the data into segments.
	 *
	 * Uses \c sepf->FindNeedle() to identify where to split; the positions
	 * it identifies, along with the start and end of the OpData object,
	 * delimit a succession of segments (sub-strings of the OpData object)
	 * each of which is passed, in turn, to \c coll->AddSegment().
	 * If \c sepf finds no separators, there is just one segment (the whole
	 * OpData object); generally, the number of segments is one more than
	 * the number of splits found. Each segment may, like any sub-string of
	 * the OpData object, span several fragments of the source data.
	 *
	 * If \c maxsplit is given, do not split more than that number of times.
	 * Split() proceeds from the start of the buffer. If there are more
	 * than \c maxsplit separators present, the remaining separators will
	 * be part of the last segment passed to \c coll->AddSegment().
	 *
	 * See modules/opdata/src/NeedleFinder.h for the class declarations of
	 * NeedleFinder (\c sepf) and modules/opdata/src/SegmentCollector.h for
	 * the class declarations of SegmentCollector (\c coll).
	 *
	 * @param sepf The #NeedleFinder instance to use for finding separators
	 *        in the data.
	 * @param coll The #SegmentCollector instance to use for collecting
	 *        the found segments.
	 * @param maxsplit The maximum number of splits to perform.
	 *        \c coll->AddSegment() will be invoked up to (\c maxsplit + 1)
	 *        times. If not given, split until \c sepf->FindNeedle()
	 *        returns NULL and there are no more fragments.
	 * @returns OP_STATUS code propagated from \c coll->AddSegment().
	 *          OpStatus::OK is returned only when the entire splitting
	 *          process has finished successfully.
	 */
	OP_STATUS Split(
		class NeedleFinder *sepf,
		class SegmentCollector *coll,
		size_t maxsplit = OpDataFullLength) const;

	/**
	 * @name Object layout.
	 *
	 * As explained above in the @ref OpData_design_requirements section, the
	 * OpData object has two different "modes", In normal mode, the object
	 * references a list of #OpDataFragment:s, while in embedded mode the actual
	 * data bytes are embedded directly within the object, with a one byte
	 * "header" at the start or end of the objects (in little/big-endian mode,
	 * respectively).
	 *
	 * The two different modes are encoded as two structs - named 'n' for normal
	 * mode and 'e' for embedded mode - within an anonymous union.
	 *
	 * Since OpData objects are also used to hold uni_char data, we require
	 * all internal data to be uni_char-aligned (only if
	 * OPDATA_UNI_CHAR_ALIGNMENT is enabled). This is going to waste an
	 * additional byte in embedded mode (assuming sizeof(uni_char) == 2).
	 *
	 * @{
	 */

public:
	/// Maximum number of data bytes that can be stored in embedded mode.
	static const size_t EmbeddedDataLimit =
#ifdef OPDATA_UNI_CHAR_ALIGNMENT
		2 * sizeof(OpDataFragment *) + 3 * sizeof(size_t) - sizeof(uni_char);
#else // OPDATA_UNI_CHAR_ALIGNMENT
		2 * sizeof(OpDataFragment *) + 3 * sizeof(size_t) - sizeof(char);
#endif // OPDATA_UNI_CHAR_ALIGNMENT

private:
	/// Shared storage for "normal" and "embedded" modes.
	union {
#ifdef OPERA_BIG_ENDIAN
	/// Normal mode object structure.
	struct {
		/// Offset into the \c first fragment where our data starts.
		mutable size_t first_offset;

		/// Offset into the \c last fragment where our data ends.
		mutable size_t last_length;

		/// Sum of lengths of fragments.
		mutable size_t length;

		/// Last fragment in this buffer.
		mutable OpDataFragment *last;

		/// First fragment in this buffer.
		mutable OpDataFragment *first;
	} n;

	/// Embedded mode object structure.
	struct {
		/// Array of data bytes.
		mutable char data[EmbeddedDataLimit]; // ARRAY OK 2011-08-23 johanh

# ifdef OPDATA_UNI_CHAR_ALIGNMENT
		/// Wasted byte, to achieve uni_char alignment for OpData::data.
		char padding;
# endif // OPDATA_UNI_CHAR_ALIGNMENT

		/**
		 * Embedded mode metadata.
		 *
		 * IMPORTANT: This byte MUST always overlap the least
		 * significant byte of the n.first fragment pointer.
		 *
		 * The least significant bit in this byte is always set (this
		 * is the 'mode' bit indicating whether or not we're in
		 * embedded mode). The top 7 bits encode the number of valid
		 * bytes  found in the \c data array.
		 */
		mutable char meta;
	} e;
#else // !OPERA_BIG_ENDIAN
	/// Normal mode object structure.
	struct {
		/// First fragment in this buffer.
		mutable OpDataFragment *first;

		/// Last fragment in this buffer.
		mutable OpDataFragment *last;

		/// Offset into the \c first fragment where our data starts.
		mutable size_t first_offset;

		/// Offset into the \c last fragment where our data ends.
		mutable size_t last_length;

		/// Sum of lengths of fragments.
		mutable size_t length;
	} n;

	/// Embedded mode object structure.
	struct {
		/**
		 * Embedded mode metadata.
		 *
		 * IMPORTANT: This byte MUST always overlap the least
		 * significant byte of the OpData::n.first fragment pointer.
		 *
		 * The least significant bit in this byte is always set (this
		 * is the 'mode' bit indicating whether or not we're in
		 * embedded mode). The top 7 bits encode the number of valid
		 * bytes  found in the \c data array.
		 */
		mutable char meta;

# ifdef OPDATA_UNI_CHAR_ALIGNMENT
		/// Wasted byte, to achieve uni_char alignment for OpData::data.
		char padding;
# endif // OPDATA_UNI_CHAR_ALIGNMENT

		/// Array of data bytes.
		mutable char data[EmbeddedDataLimit]; // ARRAY OK 2011-08-23 johanh
	} e;
#endif // OPERA_BIG_ENDIAN
	};
	/** @} */


	/**
	 * @name Convenience methods for handling OpData::meta.
	 *
	 * The OpData::meta member contains the mode flag in the least significant
	 * bit, and - if in embedded mode - the length of the embedded data
	 * in the top 7 bits.
	 *
	 * The following methods handle encoding/decoding between OpData::meta and
	 * the mode bit + embedded data length.
	 *
	 * @{
	 */
	static op_force_inline char EmptyMeta() { return 0x1; }
	static op_force_inline bool CanEmbed(size_t size) { return size <= EmbeddedDataLimit; }
	op_force_inline bool IsEmbedded() const { return (e.meta & 0x1) != 0; }
	op_force_inline size_t EmbeddedLength() const { return e.meta >> 1; }
	op_force_inline void SetMeta(size_t length) const { e.meta = (static_cast<char>(length) << 1) | 0x1; }
	/** @} */

friend class SplitHelper; // needs access to ExtendIntoFragment().
friend class OpDataFragmentIterator; // needs access to data members.
friend class UniString; // needs access to private helper methods.
};

/**
 * Convenience macros for printing/formatting OpData objects.
 *
 * Example usage:
 * @code
 *   OpData s = OpData::FromConstDataL("Hello World!");
 *   printf("Contents of s: " OPDATA_FORMAT "\n", OPDATA_FORMAT_PARAM(s));
 * @endcode
 * @{
 */
#define OPDATA_FORMAT "%.*s"
#define OPDATA_FORMAT_PARAM(obj) static_cast<int>((obj).Length()), obj.Data(false)
/** @} */

#ifdef OPDATA_DEBUG
/**
 * This operator can be used to print the OpData instance in human readable form
 * in an OP_DBG() statement. See OpData::DebugView().
 *
 * Example:
 * @code
 * OpData data = ...;
 * OP_DBG(("data:") << data);
 * @endcode
 */
inline Debug& operator<<(Debug& d, const OpData& data)
{
	char* v = data.DebugView();
	d << v;
	op_free(v);
	return d;
}
#endif // OPDATA_DEBUG

#endif /* MODULES_OPDATA_OPDATA_H */
