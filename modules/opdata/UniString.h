/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 2011-2012 Opera Software ASA.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */
#ifndef MODULES_OPDATA_UNISTRING_H
#define MODULES_OPDATA_UNISTRING_H

/** @file
 *
 * @brief UniString - Unicode string class with efficient data buffer management
 *
 *
 * @page UniString UniString - Unicode string class with efficient data buffer management
 *
 * @section UniString_design_requirements UniString design requirements
 *
 * Opera does a lot of string manipulation, and strings are often copied
 * between different parts of the code. In order to perform this manipulation
 * and copying as efficiently as possible, we need a string abstraction (a.k.a.
 * UniString) that meets the following requirements:
 *
 * -# <b>Low memory usage</b> (to run well on lower-end platforms with limited
 *    RAM, as well as increasing performance because of a smaller working set).
 * -# <b>Low memory churn</b> (on many platforms, heap allocation is not always
 *    cheap, hence we should minimize the number of unnecessary (re)allocations)
 * -# <b>Cheap copies</b> (to allow different parts of the code to maintain
 *    their own string instances with their own lifetime requirements).
 * -# <b>Cheap substrings</b> (to support dividing a string into smaller
 *    fragments).
 * -# <b>Cheap insert/append</b> (to support assembling fragments into larger
 *    strings).
 * -# <b>Cached length</b> (to minimize the number of uni_strlen() calls).
 * -# <b>Multiple deallocation strategies</b> (strings come from different
 *    sources, and we can not always control exactly how a given string is
 *    deleted).
 * -# <b>Low overhead</b> (string operations should be as quick and efficient as
 *    possible, and the overall performance of UniString must in most cases be
 *    better than using uni_char strings directly).
 * -# <b>Thread-safety</b> (the underlying data should be sharable across threads).
 * -# A disproportionately large portion of the strings in Opera are typically
 *    very short (e.g. < 10 characters). It would be nice if we could add some
 *    further optimizations for these short strings.
 *
 * Requirements 1, 2 and 3 suggest that the underlying data should be shared
 * between string instances. Requirement 4 suggests that the data sharing must
 * also support one instance referring to a \e subset of another instance.
 * Requirement 5 suggests that adding data to a string should not (always)
 * force a reallocation of the underlying data.
 *
 *
 * @section UniString_design_principles UniString design principles
 *
 * Since the above requirements are the same as for general data buffers, we
 * design UniString to reuse OpData internally to manage the underlying data
 * storage. Please read OpData's documentation for more information on how it
 * is designed.
 *
 * In short, we create UniString as a wrapper around an OpData object. Since
 * UniStrings hold 16-bit uni_char (UCS-2) data instead of 8-bit char data, we
 * require that the underlying OpData always be of even length, and we will do
 * extensive casting between OpData's char * type and UniString's uni_char *
 * type.
 *
 * @author Johan Herland, johanh@opera.com
 */

#include "modules/opdata/OpData.h"


/**
 * Efficient Unicode string abstraction that uses implicit sharing and
 * copy-on-write semantics to lower memory usage and increase performance.
 *
 * UniString objects are wrappers around OpData objects that provide an API
 * for manipulating uni_chars (instead of OpData's chars).
 */
class UniString
{
public:
	/**
	 * @name Static creator methods
	 *
	 * The following methods return newly created UniString objects with the
	 * given content. All of these methods will LEAVE() on OOM (failure to
	 * allocate internal data structures that package the given content).
	 * The provided methods are:
	 *
	 * - FromConstDataL() for wrapping a UniString object around constant
	 *   data that should not be deallocated. This is typically used to
	 *   wrap literal const uni_char * strings, ROM data, or other data
	 *   that is guaranteed to be constant for the lifetime of the process.
	 *
	 * - FromRawDataL() for wrapping a UniString object around a buffer that
	 *   should be automatically deallocated when no longer in use. The
	 *   appropriate mechanism for deallocating the data must be given to
	 *   FromRawDataL() in form of a #OpDataDeallocStrategy.
	 *
	 * - CopyDataL() for creating a UniString object holding its own copy of
	 *   the given data. This is typically used when you cannot transfer
	 *   ownership of the given data to UniString.
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
	 * Create UniString from const uni_char data.
	 *
	 * Use this creator to wrap constant string data in a UniString object.
	 *
	 * Constructs a UniString that may use the first \c length uni_chars of
	 * the \c data array. The UniString may refer to the actual memory
	 * referenced by the \c data pointer (i.e. without copying the memory at
	 * \c data). If \c length is not given (or #OpDataUnknownLength), the
	 * \c data array MUST be '\0'-terminated; the actual length of the
	 * \c data array will be found with uni_strlen().
	 *
	 * The caller \e promises that \c data will not be deleted or modified
	 * as long as this UniString and/or any copies (even of only fragments)
	 * of it exist. In other words, because UniString instances implicitly
	 * share the underlying data, the caller must not delete of modify
	 * \c data for as long as the returned UniString instance and/or any
	 * copies of it (or any sub-string of it) exist.
	 *
	 * UniString does not take ownership of \c data, so the UniString
	 * destructor will never delete \c data, even when the last UniString
	 * referring to \c data is destroyed.
	 *
	 * Warning: A UniString created with FromConstDataL() is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' uni_char at position \c length.
	 * While '\0'-termination does not matter for UniString, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from the UniString object.
	 *
	 * @param data The data array to wrap in a UniString object.
	 * @param length The number of uni_chars in the \c data array. If this
	 *        is #OpDataUnknownLength (the default), uni_strlen() will be
	 *        used to find the length of the \c data array. Hence, if you
	 *        want to embed '\0' uni_chars within a UniString, you \e must
	 *        specify \c length.
	 * @returns A UniString object wrapping the given \c data array.
	 *          LEAVEs on OOM.
	 */
	static UniString FromConstDataL(const uni_char *data, size_t length = OpDataUnknownLength)
	{
		ANCHORD(UniString, ret);
		LEAVE_IF_ERROR(ret.AppendConstData(data, length));
		return ret;
	}

	/**
	 * Create UniString from raw uni_char data with a given deallocation
	 * strategy.
	 *
	 * Use this creator to wrap allocated uni_char data in a UniString
	 * object, transferring ownership of the data to UniString in the
	 * process. Note that using this method with
	 * \c ds == #OPDATA_DEALLOC_NONE is equivalent to using the above
	 * FromConstDataL() creator, and hence subject to the same restrictions.
	 *
	 * Constructs a UniString that may use the first \c length uni_chars of
	 * the \c data array. The UniString may refer to the actual memory
	 * referenced by the \c data pointer (i.e. without copying the memory at
	 * \c data). If \c length is not given (or #OpDataUnknownLength), the
	 * \c data array MUST be '\0'-terminated; the actual length of the
	 * \c data array will be found with uni_strlen().
	 *
	 * If the actual allocated \c data array is larger than the given
	 * \c length, specify \c alloc to inform UniString of the length of the
	 * allocation. This enables UniString to use the uninitialized uni_chars
	 * between \c length and \c alloc, and thus potentially prevent
	 * (needless) reallocations.
	 *
	 * A successful call of this method transfers ownership of the \c data
	 * array to the UniString object; subsequently, the \c data array may
	 * only be accessed via the UniString object, and the caller should
	 * treat the memory at \c data as if it'd just been deallocated.
	 * UniString will automatically deallocate the \c data array when no
	 * longer needed, according to the given deallocation strategy \c ds,
	 * and may in fact have already done so before this method returns.
	 * If this method fails, the caller retains responsibility for
	 * deallocating the \c data array.
	 *
	 * Warning: A UniString created with FromRawDataL() is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' uni_char at position \c length.
	 * While '\0'-termination does not matter for UniString, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from the UniString object.
	 *
	 * @param ds Deallocation strategy. How to deallocate \c data.
	 * @param data The data array to wrap in a UniString object.
	 * @param length The number of valid/initialized uni_chars in the
	 *        \c data array. If this is #OpDataUnknownLength (the default),
	 *        uni_strlen() will be used to find the length of the \c data
	 *        array. Hence, if you want to embed '\0' uni_chars within a
	 *        UniString, you \e must specify \c length.
	 * @param alloc The total number of uni_chars in the allocation
	 *        starting at \c data. Defaults to \c length if not given.
	 * @returns A UniString object wrapping the given \c data array.
	 *          LEAVEs on OOM.
	 */
	static UniString FromRawDataL(OpDataDeallocStrategy ds, uni_char *data, size_t length = OpDataUnknownLength, size_t alloc = OpDataUnknownLength)
	{
		ANCHORD(UniString, ret);
		LEAVE_IF_ERROR(ret.AppendRawData(ds, data, length, alloc));
		return ret;
	}

	/**
	 * Create UniString holding a copy of the given uni_char array.
	 *
	 * Use this creator to copy data into a UniString object. The ownership
	 * of the original \c data is NOT transferred to UniString. This is
	 * useful when you cannot guarantee the lifetime of the given \c data.
	 *
	 * Constructs a UniString that copies the first \c length uni_chars from
	 * the \c data array. The UniString will have its own copy of the given
	 * data.
	 *
	 * @param data The data array to copy
	 * @param length The number of uni_chars to copy from \c data. If this
	 *        is #OpDataUnknownLength (the default), uni_strlen() will be
	 *        used to find the length of the \c data array. Hence, if you
	 *        want to embed '\0' uni_chars within a UniString, you \e must
	 *        specify \c length.
	 * @returns UniString object referring to a copy of \c data.
	 *          LEAVEs on OOM.
	 */
	static UniString CopyDataL(const uni_char *data, size_t length = OpDataUnknownLength)
	{
		ANCHORD(UniString, ret);
		LEAVE_IF_ERROR(ret.AppendCopyData(data, length));
		return ret;
	}

	/** @} */


	/**
	 * Default constructor.
	 */
	UniString(void) : m_data() {}

	/**
	 * Destructor.
	 */
	~UniString(void) { Clear(); }

	/**
	 * Copy constructor.
	 *
	 * Creates a shallow copy of the \c other UniString, referring to the
	 * uni_chars between the given indexes \c offset and \c length.
	 *
	 * @param other Copy this UniString object
	 * @param offset Refer to this offset into the \c other object.
	 * @param length Refer to this many uni_chars starting from \c offset.
	 * @returns A shallow copy of \c other, referring to the uni_chars
	 *          between \c offset and \c length.
	 */
	UniString(const UniString& other, size_t offset = 0, size_t length = OpDataFullLength);

	/**
	 * Assignment operator.
	 */
	UniString& operator=(const UniString& other);

	/**
	 * Returns the number of uni_chars in this object.
	 *
	 * @returns The number of valid/initialized uni_chars in this string.
	 */
	op_force_inline size_t Length(void) const { return UNICODE_DOWNSIZE(m_data.Length()); }

	/**
	 * Tests whether this object has any content.
	 *
	 * @returns true if this object has length 0; otherwise returns false.
	 */
	op_force_inline bool IsEmpty(void) const { return m_data.IsEmpty(); }

	/**
	 * @name Methods for setting the contents of a UniString object
	 *
	 * The following methods set the content of this UniString object.
	 * All of these methods return either OpStatus::OK (on success) or
	 * OpStatus::ERR_NO_MEMORY on OOM (failure to allocate internal data
	 * structures that package the given content). In case of OOM, the
	 * previous content of this object is left unchanged.
	 *
	 * These methods mirror the static creator methods above, in that they
	 * provide corresponding non-LEAVEing versions that return OP_STATUS
	 * instead of returning a new object.
	 *
	 * The provided methods are:
	 *
	 * - SetConstData() for wrapping a UniString object around constant data
	 *   that should not be deallocated. This is typically used to wrap
	 *   literal const uni_char * strings, ROM data, or other data that is
	 *   guaranteed to be constant for the lifetime of the process.
	 *
	 * - SetRawData() for wrapping a UniString object around a buffer that
	 *   should be automatically deallocated when no longer in use. The
	 *   appropriate mechanism for deallocating the data must be given to
	 *   SetRawData() in form of a #OpDataDeallocStrategy.
	 *
	 * - SetCopyData() for creating a UniString object holding its own copy
	 *   of the given data. This is typically used when you cannot transfer
	 *   ownership of the given data to UniString.
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
	 * Set this UniString from const uni_char * string.
	 *
	 * Make this object wrap the given constant uni_char data.
	 *
	 * This UniString object may use the first \c length uni_chars of the
	 * \c data array. This object may refer to the actual memory referenced
	 * by the \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with uni_strlen().
	 *
	 * The caller \e promises that \c data will not be deleted or modified
	 * as long as this UniString and/or any copies (even of only fragments)
	 * of it exist. In other words, because UniString instances implicitly
	 * share the underlying data, the caller must not delete of modify
	 * \c data for as long as this instance and/or any copies of it (or any
	 * sub-string of it) exist.
	 *
	 * UniString does not take ownership of \c data, so the UniString
	 * destructor will never delete \c data, even when the last UniString
	 * referring to \c data is destroyed.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' uni_char at position \c length.
	 * While '\0'-termination does not matter for UniString, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param data The data array to wrap with this UniString object.
	 * @param length The number of uni_chars in the \c data array. If this
	 *        is #OpDataUnknownLength (the default), uni_strlen() will be
	 *        used to find the length of the \c data array. Hence, if you
	 *        want to embed '\0' uni_chars within this object, you \e
	 *        must specify \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS SetConstData(const uni_char *data, size_t length = OpDataUnknownLength)
	{
		UniString d;
		RETURN_IF_ERROR(d.AppendConstData(data, length));
		*this = d;
		return OpStatus::OK;
	}

	/**
	 * Set this UniString from raw uni_char data with a given deallocation
	 * strategy.
	 *
	 * Use this method to make this object wrap the given allocated uni_char
	 * data, transferring ownership of the uni_char data to UniString in the
	 * process. Note that using this method with
	 * \c ds == #OPDATA_DEALLOC_NONE is equivalent to using the above
	 * SetConstData() method, and hence subject to the same restrictions.
	 *
	 * This UniString object may use the first \c length uni_chars of the
	 * \c data array. This object may refer to the actual memory referenced
	 * by the \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with uni_strlen().
	 *
	 * If the actual allocated \c data array is larger than the given
	 * \c length, specify \c alloc to inform UniString of the length of the
	 * allocation. This enables UniString to use the uninitialized uni_chars
	 * between \c length and \c alloc, and thus potentially prevent
	 * (needless) reallocations.
	 *
	 * A successful call of this method transfers ownership of the \c data
	 * array to this UniString object; subsequently, the \c data array may
	 * only be accessed via this UniString object, and the caller should
	 * treat the memory at \c data as if it'd just been deallocated.
	 * UniString will automatically deallocate the \c data array when no
	 * longer needed, according to the given deallocation strategy \c ds,
	 * and may in fact have already done so before this method returns.
	 * If this method fails, the caller retains responsibility for
	 * deallocating the \c data array.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' uni_char at position \c length.
	 * While '\0'-termination does not matter for UniString, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param ds Deallocation strategy. How to deallocate \c data.
	 * @param data The data array to wrap with this UniString object.
	 * @param length The number of valid/initialized uni_chars in the
	 *        \c data array. If this is #OpDataUnknownLength (the default),
	 *        uni_strlen() will be used to find the length of the \c data
	 *        array. Hence, if you want to embed '\0' uni_chars within this
	 *        object, you \e must specify \c length.
	 * @param alloc The total number of (initialized and uninitialized)
	 *        uni_chars in the allocation starting at \c data. Defaults to
	 *        \c length if not given.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS SetRawData(OpDataDeallocStrategy ds, uni_char *data, size_t length = OpDataUnknownLength, size_t alloc = OpDataUnknownLength)
	{
		UniString d;
		RETURN_IF_ERROR(d.AppendRawData(ds, data, length, alloc));
		*this = d;
		return OpStatus::OK;
	}

	/**
	 * Copy the given \c data array into this UniString object.
	 *
	 * Use this method to make this object refer to a copy of the given
	 * \c data. The ownership of the original data is \e not transferred
	 * to UniString. This is useful when you cannot guarantee the lifetime
	 * of the given \c data.
	 *
	 * Makes this UniString object wrap a copy the first \c length uni_chars
	 * from the \c data array. The UniString will have its own copy of the
	 * given data.
	 *
	 * @param data The data array to copy
	 * @param length The number of uni_chars to copy from \c data. If this
	 *        is #OpDataUnknownLength (the default), uni_strlen() will be
	 *        used to find the length of the \c data array. Hence, if you
	 *        want to embed '\0' uni_chars within this object, you \e must
	 *        specify \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS SetCopyData(const uni_char *data, size_t length = OpDataUnknownLength)
	{
		UniString d;
		RETURN_IF_ERROR(d.AppendCopyData(data, length));
		*this = d;
		return OpStatus::OK;
	}

	/** @} */

	/**
	 * Clear the contents of this object.
	 *
	 * This will release any storage not shared with other UniString objects.
	 */
	void Clear(void) { m_data.Clear(); }

	/**
	 * Append the given string to this string.
	 *
	 * @param other String to add to this string.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Append(const UniString& other)
	{
		OP_ASSERT(IsUniCharAligned(m_data.Length()));
		OP_ASSERT(IsUniCharAligned(other.m_data.Length()));
		return m_data.Append(other.m_data);
	}

	/**
	 * Append the given uni_char to this string.
	 *
	 * @param c Uni_char to add to this string.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS Append(uni_char c)
	{
		uni_char *buf = GetAppendPtr(1);
		RETURN_OOM_IF_NULL(buf);
		*buf = c;
		return OpStatus::OK;
	}

	/**
	  * Append string to this buffer.
	  *
	  * The string will be copied. Equivalent to AppendCopyData.
	  * @param val '\0'-terminated string to append. If NULL, nothing is appended.
	  * @retval OpStatus::OK on success.
	  * @retval OpStatus::ERR_NO_MEMORY on OOM.
	  */
	OP_STATUS AppendFormatted(const uni_char* val)
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
	 * Append const uni_char * data to this UniString without copying.
	 *
	 * Wrap the given constant uni_char data in UniString magic, and append
	 * it to this UniString object.
	 *
	 * This UniString object may use the first \c length uni_chars of the
	 * \c data array. This object may refer to the actual memory referenced
	 * by the \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with uni_strlen().
	 *
	 * The caller \e promises that \c data will not be deleted or modified
	 * as long as this UniString and/or any copies (even of only fragments)
	 * of it exist. In other words, because UniString instances implicitly
	 * share the underlying data, the caller must not delete of modify
	 * \c data for as long as this instance and/or any copies of it (or any
	 * sub-string of it) exist.
	 *
	 * UniString does not take ownership of \c data, so the UniString
	 * destructor will never delete \c data, even when the last UniString
	 * referring to \c data is destroyed.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' uni_char at position \c length.
	 * While '\0'-termination does not matter for UniString, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param data The data array to append to this UniString object without
	 *        copying.
	 * @param length The number of uni_chars in the \c data array to append.
	 *        If this is #OpDataUnknownLength (the default), uni_strlen()
	 *        will be used to find the length of the \c data array. Hence,
	 *        if you want to append '\0' uni_chars from within \c data, you
	 *        \e must specify \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS AppendConstData(const uni_char *data, size_t length = OpDataUnknownLength);

	/**
	 * Append raw uni_char data with a given deallocation strategy to this
	 * object.
	 *
	 * Use this method to make this object wrap the given allocated uni_char
	 * data, transferring ownership of the uni_char data to UniString in the
	 * process. Note that using this method with
	 * \c ds == #OPDATA_DEALLOC_NONE is equivalent to using the above
	 * AppendConstData() method, and hence subject to the same restrictions.
	 *
	 * This UniString object may use the first \c length uni_chars of the
	 * \c data array. This object may refer to the actual memory referenced
	 * by the \c data pointer (i.e. without copying the memory at \c data).
	 * If \c length is not given (or #OpDataUnknownLength), the \c data
	 * array MUST be '\0'-terminated; the actual length of the \c data array
	 * will be found with uni_strlen().
	 *
	 * If the actual allocated \c data array is larger than the given
	 * \c length, specify \c alloc to inform UniString of the length of the
	 * allocation. This enables UniString to use the uninitialized uni_chars
	 * between \c length and \c alloc, and thus potentially prevent
	 * (needless) reallocations.
	 *
	 * A successful call of this method transfers ownership of the \c data
	 * array to this UniString object; subsequently, the \c data array may
	 * only be accessed via this UniString object, and the caller should
	 * treat the memory at \c data as if it'd just been deallocated.
	 * UniString will automatically deallocate the \c data array when no
	 * longer needed, according to the given deallocation strategy \c ds,
	 * and may in fact have already done so before this method returns.
	 * If this method fails, the caller retains responsibility for
	 * deallocating the \c data array.
	 *
	 * Warning: After successful return, this object is not necessarily
	 * '\0'-terminated, unless length == #OpDataUnknownLength (the default),
	 * or \c data happened to contain a '\0' uni_char at position \c length.
	 * While '\0'-termination does not matter for UniString, it might cause
	 * (expensive) reallocation if the caller later asks for a
	 * '\0'-terminated data pointer from this object.
	 *
	 * @param ds Deallocation strategy. How to deallocate \c data.
	 * @param data The data array to append to this UniString object,
	 *        transferring ownership to UniString in the process.
	 * @param length The number of valid/initialized uni_chars in the
	 *        \c data array to append to this object. If this is
	 *        #OpDataUnknownLength (the default), uni_strlen() will be used
	 *        to find the length of the \c data array. Hence, if you want
	 *        to append '\0' uni_chars from within \c data, you \e must
	 *        specify \c length.
	 * @param alloc The total number of (initialized and uninitialized)
	 *        uni_chars in the allocation starting at \c data. Defaults to
	 *        \c length if not given.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS AppendRawData(OpDataDeallocStrategy ds, uni_char *data, size_t length = OpDataUnknownLength, size_t alloc = OpDataUnknownLength);

	/**
	 * Append a copy of the given \c data array to this UniString object.
	 *
	 * The ownership of the original data is \e not transferred to
	 * UniString. This is useful when you cannot guarantee the lifetime
	 * of the given \c data.
	 *
	 * Copies the first \c length uni_chars from the \c data array into the
	 * end of this UniString object. This UniString object will have its own
	 * copy of the given data. If the append fails (because of OOM), this
	 * UniString object is left unchanged, and OpStatus::ERR_NO_MEMORY is
	 * returned.
	 *
	 * @param data The data array to copy.
	 * @param length The number of uni_chars to copy from \c data. If this
	 *        is #OpDataUnknownLength (the default), uni_strlen() will be
	 *        used to find the length of the \c data array. Hence, if you
	 *        want to append '\0' uni_chars from within \c data, you \e must
	 *        specify \c length.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this object is
	 *         unchanged.
	 */
	OP_STATUS AppendCopyData(const uni_char *data, size_t length = OpDataUnknownLength);

	/**
	 * Expand the given format string, and append it to this object.
	 *
	 * If the method fails for any reason (return value is not
	 * OpStatus::OK), the original contents of this object will be left
	 * unchanged.
	 *
	 * @param format The printf-style format string to expand
	 * @param ... (resp. args) The list of arguments applied to the format
	 *        string
	 *
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 * @retval OpStatus::ERR on formatting (uni_vsnprintf()) error.
	 */
	OP_STATUS AppendFormat(const uni_char *format, ...);
	OP_STATUS AppendVFormat(const uni_char *format, va_list args);

	/**
	 * Retrieve a pointer to be used for appending to this string.
	 *
	 * The returned pointer is only valid until the next non-const method
	 * call on this UniString object.
	 *
	 * The returned pointer can be filled with uni_chars starting from index
	 * 0, and ending before index \c length. Using this pointer to access
	 * any OTHER uni_chars is forbidden and leads to undefined behavior.
	 *
	 * The UniString object always owns the returned pointer. Trying to free
	 * or delete the pointer is forbidden and leads to undefined behavior.
	 *
	 * This method immediately adds \c length uni_chars to the length of
	 * this string. If you do not write all \c length uni_chars into the
	 * pointer, you need to call #Trunc(Length() - num_unwritten_uni_chars)
	 * to remove the uninitialized uni_chars from the end of the string.
	 * IMPORTANT: This method immediately adds \c length uni_chars to the
	 * length of this string. If you do not write all \c length uni_chars
	 * into the pointer, you need to call #Trunc(Length() -
	 * num_unwritten_uni_chars) to remove the uninitialized uni_chars from
	 * the end of the string. The contents of the returned pointer is
	 * undefined until you fill it with data. If you forget to fill all
	 * \c length uni_chars (or call #Trunc() to remove the unfilled
	 * uni_chars), you will end up with uninitialized uni_chars in your
	 * string.
	 *
	 * This method will allocate as necessary to get a writable memory of
	 * the given size. Thus, you probably don't want to specify a large
	 * \c length value unless you are sure to use most of the allocated
	 * uni_chars.
	 *
	 * If \c length == 0 is given, this method will return a uni_char *
	 * pointer to the uni_char following the last uni_char in this object.
	 * (This ensures that a successful request for space to copy \c length
	 * == 0 uni_chars is easily distinguished from the NULL return on OOM.)
	 * However, the caller is not allowed to access any uni_chars using
	 * that pointer.
	 *
	 * @param length The number of uni_chars you wish to append.
	 * @returns Writable pointer for you to fill with up to \c length
	 *          uni_chars of data to be appended to this buffer. NULL on OOM.
	 */
	uni_char *GetAppendPtr(size_t length);

	/**
	 * Return true iff this UniString object is identical to the given object.
	 *
	 * @param other The other object to compare to this object.
	 * @retval true if the entire contents of \c other is identical to the
	 *         entire the entire contents of this object.
	 * @retval false otherwise.
	 */
	op_force_inline bool Equals(const UniString& other) const
	{
		return other.Length() == Length() && StartsWith(other);
	}

	/** Same as Equals(), but ignores the case of characters. */
	op_force_inline bool EqualsCase(const UniString& other) const
	{
		return other.Length() == Length() && StartsWithCase(other);
	}

	/**
	 * Return true iff this UniString object is identical to the given
	 * string.
	 *
	 * @param s The string to compare to this object.
	 * @param length The length of \c s. If not given, the length will
	 *        be determined with uni_strlen().
	 * @retval true if the first \c length uni_chars of \c s is identical to
	 *         the entire contents of this object.
	 * @retval false otherwise.
	 */
	bool Equals(const uni_char *s, size_t length = OpDataUnknownLength) const;

	/** Same as Equals(), but ignores the case of characters. */
	bool EqualsCase(const uni_char* s, size_t length = OpDataUnknownLength) const;

	/**
	 * Comparison operators.
	 *
	 * @{
	 */
	op_force_inline bool operator==(const UniString& other) const { return Equals(other); }
	op_force_inline bool operator==(const uni_char *s) const { return Equals(s); }

	op_force_inline bool operator!=(const UniString& other) const { return !Equals(other); }
	op_force_inline bool operator!=(const uni_char *s) const { return !Equals(s); }
	/** @} */

	/**
	 * Return true iff this object starts with the same contents as \c other.
	 *
	 * Compares the first \c length uni_chars of this object and \c other.
	 * Return true iff the first \c length uni_chars are identical.
	 *
	 * If \c length is not given, or greater than other.Length(), the
	 * entire length of \c other is used in the comparison.
	 *
	 * Note that if \e this object is \e shorter than \c length (or
	 * other.Length() when used in its place), this method will return
	 * false. In other words, the comparison is \e not auto-limited to the
	 * length of this object.
	 *
	 * The comparison is performed with op_memcmp(), so only identical
	 * uni_char values are considered equivalent - Unicode-equivalent UCS-2
	 * (or UTF-16) sequences won't be recognised as such, only identical
	 * memory contents will be recognised. See modules/unicode for APIs
	 * that allow more Unicode-aware string processing.
	 *
	 * @param other The object to compare to the start of this object.
	 * @param length The number of uni_chars to compare.
	 *        If not given, or >= other.Length(), the comparison length will
	 *        be other.Length().
	 * @retval true if the first \c length uni_chars of \c other is
	 *         identical to the first \c length uni_chars of this object.
	 * @retval false otherwise.
	 */
	bool StartsWith(const UniString& other, size_t length = OpDataFullLength) const;

	/**
	 * Return true iff this object starts with the given string.
	 *
	 * The comparison is performed with op_memcmp(), so '\0'-characters in
	 * \c buf (and in the UniString object) is allowed. Note, however, that
	 * you will need to specify \c length in that case, because the default
	 * \c length (using uni_strlen()) will abort the comparison before the
	 * first '\0'-character.
	 *
	 * @param buf The string to compare to the start of this object.
	 * @param length The length of \c buf. If not given, the length will
	 *        be determined with uni_strlen().
	 * @retval true if the first \c length uni_chars of \c buf is identical
	 *         to the first \c length uni_chars of this object.
	 * @retval false otherwise.
	 */
	bool StartsWith(const uni_char *buf, size_t length = OpDataUnknownLength) const;

	/** Same as StartsWith(), but ignores the case of characters. */
	bool StartsWithCase(const UniString& other, size_t length = OpDataFullLength) const;
	/** Same as StartsWith(), but ignores the case of characters. */
	bool StartsWithCase(const uni_char* buf, size_t length = OpDataUnknownLength) const;

	/** Return true iff this object ends with the same contents as \p other.
	 *
	 * Compares the last \p length uni_chars of this object and the first \p
	 * length uni_chars of \p other. Return true iff these are identical.
	 *
	 * If \p length is not given, or greater than other.Length(), the entire
	 * length of \p other is used in the comparison.
	 *
	 * @note If \e this object is \e shorter than \c length (or other.Length()
	 *       when used in its place), this method returns false. In other words,
	 *       the comparison is \e not auto-limited to the length of this object.
	 * @note If the specified \p length (or the length of the \p object) is 0,
	 *       then this method returns \c true - each string ends with an empty
	 *       string.
	 *
	 * The comparison is performed with op_memcmp(), so only identical uni_char
	 * values are considered equivalent - Unicode-equivalent UCS-2 (or UTF-16)
	 * sequences won't be recognised as such, only identical memory contents
	 * will be recognised. See modules/unicode for APIs that allow more
	 * Unicode-aware string processing.
	 *
	 * @param other The object to compare to the end of this object.
	 * @param length The number of uni_chars to compare.
	 *        If not given, or >= other.Length(), the comparison length will
	 *        be other.Length().
	 * @retval true if the first \p length uni_chars of \c other is identical to
	 *         the last \c length uni_chars of this object.
	 * @retval false otherwise.
	 */
	bool EndsWith(const UniString& other, size_t length = OpDataFullLength) const;

	/** Return true iff this object ends with the given string.
	 *
	 * The comparison is performed with op_memcmp(), so '\0'-characters in
	 * \p buf (and in the UniString object) are allowed. Note, however, that
	 * you will need to specify \p length in that case, because the default
	 * \p length (using uni_strlen()) will abort the comparison before the
	 * first '\0'-character.
	 *
	 * @param buf The string to compare to the end of this object.
	 * @param length The length of \p buf. If not given, the length will
	 *        be determined with uni_strlen().
	 * @retval true if the first \p length uni_chars of \p buf is identical
	 *         to the last \p length uni_chars of this object.
	 * @retval false otherwise.
	 */
	bool EndsWith(const uni_char* buf, size_t length = OpDataUnknownLength) const;

	/**
	 * Get the uni_char at the given index into this string.
	 *
	 * @param index Index into this string of uni_char to return.
	 * @returns uni_char at the given \c index.
	 */
	const uni_char& At(size_t index) const;

	/**
	 * Index operator.
	 */
	op_force_inline const uni_char& operator[](size_t index) const { return At(index); }

	/**
	 * Find the first occurrence of the given character (sequence) in this
	 * string.
	 *
	 * Return the smallest index i in \c offset <= i < \c length for which
	 * the given \c needle is found at index i (i.e. this->[i] == \c needle
	 * for the uni_char version of this method, or 0 ==
	 * op_memcmp(this->Data() + i, \c needle, UNICODE_SIZE(\c needle_len))
	 * for the const uni_char * version). In the const uni_char * version of
	 * this method, the \e entire \c needle must be fully contained within
	 * the search area (haystack) in order to be found. If no such i is
	 * found, return #OpDataNotFound.
	 *
	 * If \c offset is not given, the search starts at the beginning of this
	 * string. If \c length is not given, the search stops at the end of
	 * this string.
	 *
	 * In the const uni_char * version of this method, if \c needle_len ==
	 * #OpDataUnknownLength (the default), the length of \c needle is
	 * computed with uni_strlen(). Hence, if a '\0' character is part of
	 * your needle, you must specify \c needle_len. If \c needle_len is 0
	 * (or becomes 0 after calling uni_strlen()), the search is always
	 * successful (even in an empty string) unless \c offset exceeds
	 * Length(). In other words, the empty \c needle matches at every index
	 * in the range from \c offset to \c length (or Length()), even if there
	 * are no characters.
	 *
	 * @param needle The character or character sequence to search for.
	 * @param needle_len The length of the \c needle. If not given, the
	 *          length of the needle will be found with uni_strlen().
	 * @param offset Index into this string, indicating start of search
	 *          area (haystack). Defaults to start of string if not given.
	 * @param length The length (in uni_chars) of the haystack, starting
	 *          from \c offset. Defaults to the rest of this string if not
	 *          given.
	 * @returns The string index (i.e. relative to the start of this string,
	 *          \e not relative to \c offset) of (the start of) the first
	 *          occurrence of the \c needle, or #OpDataNotFound if not found.
	 *
	 * @{
	 */
	size_t FindFirst(
		const uni_char *needle, size_t needle_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	size_t FindFirst(uni_char needle, size_t offset = 0, size_t length = OpDataFullLength) const;
	/** @} */

	/**
	 * Find the first occurrence of any character in the given \c accept set.
	 *
	 * Return the smallest index i in \c offset <= i < \c length for which
	 * any uni_char in the given \c accept set is found at index i (i.e.
	 * this->[i] is a uni_char in \c accept). If no such i is found, return
	 * #OpDataNotFound.
	 *
	 * If \c offset is not given, the search starts at the beginning of this
	 * string. If \c length is not given, the search stops at the end of
	 * this string.
	 *
	 * If \c accept_len == #OpDataUnknownLength (the default), the length
	 * of \c accept (i.e. the number of uni_chars in the \c accept set) is
	 * computed with uni_strlen(). Hence, if a '\0' character is part of
	 * your \c accept set, you must specify \c accept_len.
	 *
	 * @param accept The set of characters to search for.
	 * @param accept_len The number of characters in the \c accept set. If
	 *          not given, the size of the set will be found with
	 *          uni_strlen().
	 * @param offset Index into this string, indicating start of search
	 *          area (haystack). Defaults to start of string if not given.
	 * @param length The length (in uni_chars) of the haystack, starting
	 *          from \c offset. Defaults to the rest of this string if not
	 *          given.
	 * @returns The string index (i.e. relative to the start of this string,
	 *          \e not relative to \c offset) of the first occurrence of any
	 *          character in \c accept, or #OpDataNotFound if not found.
	 */
	size_t FindFirstOf(
		const uni_char *accept, size_t accept_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	/**
	 * Find the first occurrence of any character \e not in the given
	 * \c ignore set.
	 *
	 * Return the smallest index i in \c offset <= i < \c length for which
	 * any uni_char \e not in the given \c ignore set is found at index i
	 * (i.e. this->[i] is not a uni_char in \c ignore). If no such i is
	 * found (i.e. if all the uni_chars searched are in \c ignore), return
	 * #OpDataNotFound.
	 *
	 * If \c offset is not given, the search starts at the beginning of this
	 * string. If \c length is not given, the search stops at the end of
	 * this string.
	 *
	 * If \c ignore_len == #OpDataUnknownLength (the default), the length
	 * of \c ignore (i.e. the number of uni_chars in the \c ignore set) is
	 * computed with uni_strlen(). Hence, if a '\0' character is part of
	 * your \c ignore set, you must specify \c ignore_len.
	 *
	 * @param ignore The set of characters to ignore while searching.
	 * @param ignore_len The number of characters in the \c ignore set. If
	 *          not given, the size of the set will be found with
	 *          uni_strlen().
	 * @param offset Index into this string, indicating start of search
	 *          area (haystack). Defaults to start of string if not given.
	 * @param length The length (in uni_chars) of the haystack, starting
	 *          from \c offset. Defaults to the rest of this string if not
	 *          given.
	 * @returns The string index (i.e. relative to the start of this string,
	 *          \e not relative to \c offset) of the first occurrence of any
	 *          character \e not in \c ignore, or #OpDataNotFound if none
	 *          found.
	 */
	size_t FindEndOf(
		const uni_char *ignore, size_t ignore_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	/**
	 * Find the last occurrence of the given character (sequence) in this
	 * string.
	 *
	 * Return the highest index i in \c offset <= i < \c length for which
	 * the given \c needle is found at index i (i.e. this->[i] == \c needle
	 * for the uni_char version of this method, or 0 ==
	 * op_memcmp(this->Data() + i, \c needle, UNICODE_SIZE(\c needle_len))
	 * for the const uni_char * version). In the const uni_char * version of
	 * this method, the \e entire \c needle must be fully contained within
	 * the search area (haystack) in order to be found. If no such i is
	 * found, return #OpDataNotFound.
	 *
	 * If \c offset is not given, the search area starts at the beginning of
	 * this string. If \c length is not given, the search area stops at the
	 * end of this string. Note that the search is reversed, and proceeds
	 * backwards from \c offset + \c length towards \c offset.
	 *
	 * In the const uni_char * version of this method, if \c needle_len ==
	 * #OpDataUnknownLength (the default), the length of \c needle is
	 * computed with uni_strlen(). Hence, if a '\0' character is part of
	 * your needle, you must specify \c needle_len. If \c needle_len is 0
	 * (or becomes 0 after calling uni_strlen()), the search is always
	 * successful (even in an empty string) unless \c offset exceeds
	 * Length(). In other words, the empty \c needle matches at every index
	 * in the range from \c offset to \c length (or Length()), even if there
	 * are no characters.
	 *
	 * @param needle The character or character sequence to search for.
	 * @param needle_len The length of the \c needle. If not given, the
	 *          length of the needle will be found with uni_strlen().
	 * @param offset Index into this string, indicating start of search
	 *          area (haystack). Defaults to start of string if not given.
	 * @param length The length (in uni_chars) of the haystack, starting
	 *          from \c offset. Defaults to the rest of this string if not
	 *          given.
	 * @returns The string index (i.e. relative to the start of this string,
	 *          \e not relative to \c offset) of (the start of) the last
	 *          occurrence of the \c needle, or #OpDataNotFound if not found.
	 *
	 * @{
	 */
	size_t FindLast(
		const uni_char *needle, size_t needle_len = OpDataUnknownLength,
		size_t offset = 0, size_t length = OpDataFullLength) const;

	size_t FindLast(uni_char needle, size_t offset = 0, size_t length = OpDataFullLength) const;
	/** @} */

	/**
	 * Consume the given number of uni_chars from the start of this string.
	 *
	 * Remove \c length uni_chars from the start of the string.
	 * If \c length is the current string length or greater, the string will
	 * become empty.
	 *
	 * @param length Consume this many uni_chars from the start of the
	 *        string.
	 */
	void Consume(size_t length);

	/**
	 * Truncate this string to the given length.
	 *
	 * Remove all uni_chars starting from index \c length, and to the end
	 * of the string. If \c length is the current string length or greater,
	 * nothing is truncated. Passing \c length == 0 is equivalent to calling
	 * Clear().
	 *
	 * @param length Truncate string to this length.
	 */
	void Trunc(size_t length);

	/**
	 * Delete \c length characters starting at \c offset from this string.
	 *
	 * @param offset The index of the first character to delete.
	 * @param length The number of characters to delete.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this UniString
	 *         shall remain unchanged.
	 */
	OP_STATUS Delete(size_t offset, size_t length)
	{
		return m_data.Delete(UNICODE_SIZE(offset), UNICODE_SIZE(length));
	}

	/**
	 * Remove the given character (sequence) from this string.
	 *
	 * Remove one or more instances of the given uni_char (or uni_char
	 * sequence) from this UniString object. By default, all instances of
	 * the given uni_char (sequence) will be removed from this string.
	 * Specify \c maxremove to limit the number of instances to be removed.
	 * Removal proceeds from the start of this string, so specifying e.g.
	 * \c maxremove == 5 will remove the first 5 instances occuring in this
	 * string.
	 *
	 * @param remove The uni_char or uni_char sequence to remove from this
	 *        string.
	 * @param length The length of \c remove. If not given, the length of
	 *        \c remove will be determined by uni_strlen().
	 * @param maxremove The maximum number of removals to perform.
	 *        Unlimited by default.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this UniString
	 *         shall remain unchanged.
	 *
	 * @{
	 */
	OP_STATUS Remove(const uni_char *remove, size_t length = OpDataUnknownLength, size_t maxremove = OpDataFullLength);

	OP_STATUS Remove(uni_char remove, size_t maxremove = OpDataFullLength);
	/** @} */

	/** Replace the \p character with the specified \p replacement in this
	 * string.
	 *
	 * Replaces one or more instances of the given \c character in this
	 * UniString object with the specified \c replacement. By default, all
	 * instances of the \p character will be replaced in this string.
	 * Specify \p maxreplace to limit the number of instances to be replaced.
	 * Replacement proceeds from the start of this string, so specifying e.g.
	 * \p maxreplace == 5 will replace the first 5 instances occuring in this
	 * string.
	 *
	 * @param character The uni_char character to replace in this string.
	 * @param replacement The uni_char which replaces \p character.
	 * @param maxreplace The maximum number of replacements to perform.
	 *        Unlimited by default.
	 * @param[out] replaced May point to a size_t. If such a pointer is
	 *        specified, the size_t receives on success the number of
	 *        replacements that were performed.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM, in which case this UniString
	 *         shall remain unchanged.
	 */
	OP_STATUS Replace(uni_char character, uni_char replacement, size_t maxreplace = OpDataFullLength, size_t* replaced = NULL);

	/** Replace \p substring with the specified \p replacement in this string.
	 *
	 * Replaces one or more instances of the given \p substring in this
	 * UniString object with the specified \p replacement. By default, all
	 * instances of the \p substring will be replaced in this string.
	 * Specify \p maxreplace to limit the number of instances to be replaced.
	 * Replacement proceeds from the start of this string, so specifying e.g.
	 * \p maxreplace == 5 will replace the first 5 instances occuring in this
	 * string.
	 *
	 * @param substring The sub-string to replace in this string. If the
	 *        \p substring is NULL or an empty string, then this implementation
	 *        does not modify this string and OpStatus::ERR is returned.
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
	OP_STATUS Replace(const uni_char* substring, const uni_char* replacement, size_t maxreplace = OpDataFullLength, size_t* replaced = NULL);

	/**
	 * Strip leading/trailing whitespace from this object.
	 *
	 * Whitespace means any character for which the uni_isspace() function
	 * returns true. This includes at least the ASCII characters '\\t', '\\n',
	 * '\\v', '\\f', '\\r', and ' '. See the Unicode::IsSpace() documentation
	 * for more details of which characters are considered whitespace.
	 *
	 * @{
	 */
	void LStrip(void); ///< Strip leading whitespace.
	void RStrip(void); ///< Strip trailing whitespace.
	void Strip(void); ///< Strip both leading and trailing whitespace.
	/** @} */

	/**
	 * Split the string into segments wherever \c sep occurs.
	 *
	 * Returns a list of those segments (as UniString objects). If \c sep
	 * does not match anywhere in the string, Split() returns a single-
	 * element list containing the entire string. Each of the returned
	 * segments refers to one or more fragments of the source string.
	 *
	 * If \c maxsplit is given, do not split more than that number of times
	 * (yielding up to \c maxsplit + 1 elements in the returned list).
	 * Split() proceeds from the start of the string. If there are more
	 * than \c maxsplit separators present, the remaining separators will
	 * be part of the last element of the returned list.
	 *
	 * Examples:
	 *
	 *   @code
	 *   UniString d = UniString::FromConstDataL(UNI_L("12|34|567|8||9|0|"));
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
	 * between every uni_char (limited by \c maxsplit, if given), and the
	 * resulting list will have N elements, each one uni_char long, where
	 * N == this->Length().
	 *
	 * Examples:
	 *
	 *   @code
	 *   UniString d = UniString::FromConstDataL(UNI_L("abcdefghij"));
	 *   d.Split(UNI_L("ef"))
	 *   @endcode
	 *   will return: ["abcd", "ghij"]
	 *
	 *   @code
	 *   d.Split(UNI_L(""));
	 *   @endcode
	 *   will return: ["a", "b", "c", "d", "e", "f", "g", "h", "i", "j"]
	 *
	 *   @code
	 *   d.Split(UNI_L("foo"), 0, 3);
	 *   @endcode
	 *   will return: ["a", "b", "c", "defghij"]
	 *
	 * The returned OtlCountedList object is heap-allocated and must be
	 * deleted by the caller, using OP_DELETE(). On OOM, NULL is returned.
	 *
	 * @param sep The uni_char or uni_char sequence on which to split this
	 *        data.
	 * @param length The length of \c sep. If not given, the length of
	 *        \c sep will be determined by uni_strlen().
	 * @param maxsplit The maximum number of splits to perform. The
	 *        returned list will have up to (\c maxsplit + 1) elements.
	 *        If not given, split until there are no separators left.
	 * @returns List of segments (substrings of this UniString instance)
	 *          after splitting on \c sep. Individual segments may be empty
	 *          (e.g. when \c sep immediately follows another \c sep).
	 *          Returned list must be OP_DELETE()d by caller. Returns NULL
	 *          on OOM.
	 *
	 * @{
	 */
	OtlCountedList<UniString> *Split(const uni_char *sep, size_t length = OpDataUnknownLength, size_t maxsplit = OpDataFullLength) const;

	OtlCountedList<UniString> *Split(uni_char sep, size_t maxsplit = OpDataFullLength) const;
	/** @} */

	/**
	 * Split the string into segments wherever any character in \p sep occurs.
	 *
	 * Returns a list of split segments (as UniString objects). If the string
	 * does not contain any character found in \p sep, SplitAtAnyOf() returns a
	 * single-element list containing the entire string. Each of the returned
	 * segments refers to one or more fragments of the source string.
	 *
	 * If \p maxsplit is given, do not split more than that number of times
	 * (yielding up to \p maxsplit + 1 elements in the returned list).
	 * SplitAtAnyOf() proceeds from the start of the string. If there are
	 * more than \p maxsplit separators present, the remaining separators will
	 * be part of the last element of the returned list.
	 *
	 * Examples:
	 *
	 *   @code
	 *   UniString d = UniString::FromConstDataL(UNI_L("12;34,567,8;,9,0,"));
	 *   d.SplitAtAnyOf(UNI_L(";,"));
	 *   @endcode
	 *   will return: ["12", "34", "567", "8", "", "9", "0", ""]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(UNI_L(";,"), OpDataUnknownLength, 3);
	 *   @endcode
	 *   will return: ["12", "34", "567", "8;,9,0,"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(UNI_L(";,"), OpDataUnknownLength, 6);
	 *   @endcode
	 *   will return: ["12", "34", "567", "8", "", "9", "0,"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(UNI_L(";,"), OpDataUnknownLength, 0);
	 *   @endcode
	 *   will return: ["12;34,567,8;,9,0,"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(UNI_L("X"), OpDataUnknownLength, 6);
	 *   @endcode
	 *   will return: ["12;34,567,8;,9,0,"]
	 *
	 * If the given separator is empty (i.e. \p length == 0, or \p length ==
	 * OpDataUnknownLength and \c sep[0] == '\0'), SplitAtAnyOf() will not split
	 * the string. The resulting list will have one element containing the
	 * complete string.
	 *
	 *   @code
	 *   UniString d = UniString::FromConstDataL(UNI_L("abcdefghij"));
	 *   d.SplitAtAnyOf(UNI_L("ef"));
	 *   @endcode
	 *   will return: ["abcd", "", "ghij"]
	 *
	 *   @code
	 *   d.SplitAtAnyOf(UNI_L(""));
	 *   @endcode
	 *   will return: ["abcdefghij"]
	 *
	 *   @code
	 *   d.Split(UNI_L("foo"), 0, 3);
	 *   @endcode
	 *   will return: ["abcdefghij"]
	 *
	 * The returned OtlCountedList object is heap-allocated and must be
	 * deleted by the caller, using OP_DELETE(). On OOM, NULL is returned.
	 *
	 * @param sep The set of uni_char on which to split this data.
	 * @param length The length of \p sep. If not given, the length of \p sep
	 *        will be determined by uni_strlen().
	 * @param maxsplit The maximum number of splits to perform. The returned
	 *        list will have up to (\c maxsplit + 1) elements. If not given,
	 *        split until there are no separators left.
	 * @returns List of segments (substrings of this UniString instance) after
	 *          splitting on \c sep. Individual segments may be empty (e.g. when
	 *          characters in \p sep occur consecutively). Returned list must be
	 *          OP_DELETE()d by caller. Returns NULL on OOM.
	 */
	OtlCountedList<UniString>* SplitAtAnyOf(const uni_char* sep, size_t length = OpDataUnknownLength, size_t maxsplit = OpDataFullLength) const;

	/**
	 * Copy \c length uni_chars of data from this object into the given
	 * \c ptr.
	 *
	 * If this object holds fewer than the given \c length uni_char, only
	 * the uni_char available (== Length() - offset) will be copied.
	 *
	 * The caller maintains full ownership of the given \c ptr, and
	 * promises that the given \c ptr can hold the given \c length number
	 * of uni_chars,
	 *
	 * Note that no '\0'-termination will be performed (unless this
	 * UniString object happens to contain '\0' uni_chars).
	 *
	 * @param ptr The pointer into which data from this object is copied.
	 * @param length The maximum number of uni_chars to copy into \c ptr.
	 * @param offset Start copying at this offset into this object's data.
	 * @returns The actual number of uni_chars copied into \c ptr. This
	 *          number is guaranteed to be <= \c length.
	 */
	size_t CopyInto(uni_char *ptr, size_t length, size_t offset = 0) const;

	/**
	 * Return a const pointer to the raw data kept within this object.
	 *
	 * The returned pointer will be non-NULL even if this UniString is
	 * empty. Only if OOM is encountered while preparing the pointer will
	 * NULL be returned.
	 *
	 * The returned pointer is only valid until the next non-const method
	 * call on this UniString object.
	 *
	 * Use the \c nul_terminated argument to control whether the returned
	 * data must be '\0'-terminated or not.
	 *
	 * @param nul_terminated Set to true if you need the return data array
	 *        to be '\0'-terminated.
	 * @retval Pointer to const data contents of this UniString object. Even
	 *         if the UniString is empty, the returned pointer will be
	 *         non-NULL.
	 * @retval NULL on OOM.
	 */
	const uni_char *Data(bool nul_terminated = false) const;

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
	 * \c nul_terminators value) on this UniString object. UniString
	 * guarantees that repeated calls to CreatePtr() (with the same or
	 * smaller \c nul_terminators value, and with no intervening non-const
	 * method calls) will result in the same pointer being stored into
	 * \c ptr.
	 *
	 * @param ptr The pointer to the consolidated data is stored here.
	 * @param nul_terminators Set to 0 if you don't need '\0'-termination,
	 *        1 if you need a single '\0'-terminator uni_char, or more if
	 *        you for some reason need more trailing '\0' uni_chars.
	 * @retval OpStatus::OK on success.
	 * @retval OpStatus::ERR_NO_MEMORY on OOM.
	 */
	OP_STATUS CreatePtr(const uni_char **ptr, size_t nul_terminators = 0) const;

	/**
	 * Interpret Unicode data as a signed/unsigned short/int/long integer.
	 *
	 * UniString wrapper around the uni_strto*() family of functions.
	 *
	 * Converts the initial characters - starting at \c offset (the \e start
	 * of this UniString object by default) - from Unicode characters to the
	 * requested integer type. The parsed integer is returned in \c ret, and
	 * the number of uni_chars parsed (not including \c offset) is returned
	 * in \c length. The value stored in \c length is guaranteed to be
	 * <= Length() - \c offset.
	 *
	 * As with the uni_strto*() functions, leading whitespace is ignored,
	 * and parsing stops at the first uni_char not part of the number.
	 * See the uni_strto*() documentation for more information on the rules
	 * regarding the formatting of the given integer string.
	 *
	 * If (and only if) \c length is NULL (the default), this entire object
	 * (modulo leading/trailing whitespace) must consist only of the parsed
	 * number, or OpStatus::ERR shall be returned. In other words, the data
	 * between the index that would otherwise be stored into \c length, and
	 * the end of this object MUST consist entirely of whitespace (according
	 * to uni_isspace()). (Note that trailing whitespace is not ignored if
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
	 * @param[out] length The number of uni_chars parsed (relative to
	 *        \c offset) is stored here. On failure, the current parsing
	 *        position (typically 0 unless ERR_OUT_OF_RANGE is returned)
	 *        will be stored here. If NULL (the default), an error will be
	 *        returned unless this \e entire object (starting from
	 *        \c offset) is successfully parsed. (This parameter is never
	 *        read, only written to.)
	 * @param base The base to use while parsing. Must be 0 or
	 *        2 <= \c base <= 36. If 0, the base is determined automatically
	 *        using the the following rules: If the string starts with "0x",
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
	 * as they depend on uni_strtoi64() and uni_strtoui64(), respectively.
	 * Also, see CORE-40606.
	 */
#ifdef STDLIB_64BIT_STRING_CONVERSIONS
	OP_STATUS ToINT64(INT64 *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;

	OP_STATUS ToUINT64(UINT64 *ret, size_t *length = NULL, int base = 10, size_t offset = 0) const;
#endif // STDLIB_64BIT_STRING_CONVERSIONS
	/** @} */

	/**
	 * Interpret Unicode data as a double.
	 *
	 * UniString wrapper around the uni_strtod() function.
	 *
	 * Converts the initial characters - starting at \c offset (the \e start
	 * of this OpData object by default) - from Unicode characters to a
	 * floating-point double value. The parsed double value is returned in
	 * \c ret, and the number of uni_chars parsed (not including \c offset)
	 * is returned in \c length. The value stored in \c length is guaranteed
	 * to be <= Length() - \c offset.
	 *
	 * As with the uni_strtod() function, leading whitespace is ignored,
	 * and parsing stops at the first uni_char not part of the number.
	 * See the uni_strtod() documentation for more information on the rules
	 * regarding the formatting of the given floating-point number string.
	 *
	 * If (and only if) \c length is NULL (the default), this entire object
	 * (modulo leading/trailing whitespace) must consist only of the parsed
	 * number, or OpStatus::ERR shall be returned. In other words, the data
	 * between the index that would otherwise be stored into \c length, and
	 * the end of this object MUST consist entirely of whitespace (according
	 * to uni_isspace()). (Note that trailing whitespace is not ignored if
	 * \c length is given.)
	 *
	 * If no number can be parsed, an appropriate OpStatus error code is
	 * returned, and the value pointed to by \c ret shall be undefined.
	 *
	 * @param[out] ret The parsed double is stored here. On failure, the
	 *        value is undefined and should be ignored. (This parameter is
	 *        never read, only written to.)
	 * @param[out] length The number of uni_chars parsed (relative to
	 *        \c offset) is stored here. On failure, the current parsing
	 *        position (typically 0 unless ERR_OUT_OF_RANGE is returned)
	 *        will be stored here. If NULL (the default), an error will be
	 *        returned unless this \e entire object (starting from
	 *        \c offset) is successfully parsed. (This parameter is never
	 *        read, only written to.)
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
	 * Produce a debug-friendly view of this UniString instance.
	 *
	 * Return an ASCII string containing the data stored in this instance,
	 * but also make the internal structure visible. Non-printable or
	 * non-ASCII characters are shown using escape codes of the form
	 * "\x####", where "####" indicate the uni_char value in hexadecimal
	 * notation. E.g.: "\xdead\xbeef". The following character values are
	 * considered non-printable:
	 *  - >= 0x0080 (outside ASCII).
	 *  - any character for which op_isprint() returns false.
	 * Also, the single quote and backslash characters will be escaped as
	 * "\'" and "\\", respectively.
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
	 * sign) within specifies the \#uni_chars skipped from the start of the
	 * first fragment, and a trailing '+#' (plus sign followed by a decimal
	 * number) specifies the \#uni_chars skipped from the end of the last
	 * fragment.
	 *
	 * Thus, the characters that are shown, are exactly those that are part
	 * of this UniString object, and the leading/trailing numbers indicate
	 * the remaining characters in the first/last fragments that are unused
	 * by this instance (but may be in use by other UniString instances).
	 *
	 * The returned string has been allocated by this method, using
	 * op_malloc(). It is the responsibility of the caller to deallocate it
	 * using op_free().
	 *
	 * @returns A string containing a debug-friendly view of this object.
	 *          NULL on OOM. Note that an empty UniString object necessarily
	 *          is embedded, hence displays as "{''}".
	 */
	char *DebugView(void) const;
#endif // OPDATA_DEBUG

	/// Return true iff the given size/ptr is uni_char aligned.
	static op_force_inline bool IsUniCharAligned(size_t size) { return size % UNICODE_SIZE(1) == 0; }
#ifdef OPDATA_UNI_CHAR_ALIGNMENT
	static op_force_inline bool IsUniCharAligned(const void *ptr) { return reinterpret_cast<UINTPTR>(ptr) % UNICODE_SIZE(1) == 0; }
#else // OPDATA_UNI_CHAR_ALIGNMENT
	static op_force_inline bool IsUniCharAligned(const void *ptr) { return true; }
#endif // OPDATA_UNI_CHAR_ALIGNMENT

private:
	/**
	 * Internal constructor taking an OpData object.
	 */
	UniString(const OpData& d) : m_data(d) { OP_ASSERT(IsUniCharAligned(d.Length())); }

	/**
	 * Return a writable ptr at the end of this string without allocation.
	 *
	 * This is the uni_char version of OpData::GetAppendPtr_NoAlloc().
	 * Please consult its documentation for more details on the proper use
	 * of this method.
	 *
	 * IMPORTANT: The \c length parameter is given in uni_char (two-byte)
	 * units, instead of the OpData version, which uses byte units.
	 */
	uni_char *GetAppendPtr_NoAlloc(size_t& length);


	/**
	 * Internal data storage.
	 *
	 * Stores an even number of bytes, that are cast to uni_char when
	 * needed.
	 */
	OpData m_data;

friend class CountedUniStringCollector; // needs UniString(const OpData& d).
friend class UniStringFragmentIterator; // needs access to data members.
};

#ifdef OPDATA_DEBUG
/**
 * This operator can be used to print the UniString instance in human readable
 * form in an OP_DBG() statement. See UniString::DebugView().
 *
 * Example:
 * @code
 * OpData data = ...;
 * OP_DBG(("data:") << data);
 * @endcode
 */
op_force_inline Debug& operator<<(Debug& d, const UniString& data)
{
	char* v = data.DebugView();
	d << v;
	op_free(v);
	return d;
}
#endif // OPDATA_DEBUG

#endif /* MODULES_OPDATA_UNISTRING_H */
