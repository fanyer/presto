/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef MODULES_UTIL_BYTEARRAY_H
#define MODULES_UTIL_BYTEARRAY_H

/**
 * A class that maintains a dynamically growing sequence of bytes with
 * no limitations on content. It is intended for cases where the
 * data grows and will have a certain memory overhead to make
 * growth efficient.
 *
 * For a similar character array see TempBuffer. This class has intentionally
 * a very similar API and functions named the same.
 *
 */
class ByteArray
{
	/** The actual byte array. */
	char* m_byte_array;

	/** How much of m_byte_array that is used. */
	unsigned long m_length;

	/** How big m_byte_array is. */
	unsigned long m_allocated;

	/** Not implemented. Defined to prevent accidental object copying. */
	ByteArray(const ByteArray& other);

	/** Not implemented. Defined to prevent accidental object copying. */
	ByteArray& operator=(const ByteArray& other);

	/**
	 * Helper method to grow the buffer if needed.
	 */
	OP_STATUS EnsureCapacity(unsigned long new_len);

public:
	/**
	 * Constructor. Light weight. Will not allocate anything.
	 */
	ByteArray() : m_byte_array(NULL), m_length(0), m_allocated(0) {}

	/**
	 * Destructor. Will free the internal storage making all
	 * pointers returned from this array unusable.
	 */
	~ByteArray() { op_free(m_byte_array); }

	/**
	 * Returns the length of the array. Will start at 0 and grow
	 * with each call to Append.
	 */
	unsigned long Length() const { return m_length; }

	/**
	 * Return a pointer to buffer-internal storage containing the
	 * string.  This is valid only until the buffer is expanded or
	 * destroyed.
	 *
	 * The value might be NULL if GetStorage() is called when
	 * the array is empty.
	 */
	char* GetStorage() { return m_byte_array; }

	/**
	 * Return a pointer to buffer-internal storage containing the
	 * string.  This is valid only until the buffer is expanded or
	 * destroyed.
	 *
	 * The value might be NULL if GetStorage() is called when
	 * the array is empty.
	 */
	const char* GetStorage() const { return m_byte_array; }

	/**
	 * Adds a number of bytes to the end of the array.
	 *
	 * @param data The bytes to add.
	 * @param len The number of bytes to add from the data pointer.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY. If the
	 * function fails, then nothing in the array will have changed.
	 */
	OP_STATUS Append(const char *data, unsigned long len);

	/**
	 * Adds a number of zero bytes to the end of the array.
	 *
	 * @param len The number of zero bytes to add.
	 *
	 * @returns OpStatus::OK or OpStatus::ERR_NO_MEMORY. If the
	 * function fails, then nothing in the array will have changed.
	 */
	OP_STATUS AppendZeroes(unsigned long len);

	/**
	 * If the array is longer than new_length, then it will be
	 * be made new_length long. Otherwise nothing will happen.
	 */
	void Truncate(unsigned long new_length) { m_length = MIN(m_length, new_length); }

	/**
	 * Frees all external storage and sets the length to 0.
	 */
	void FreeStorage();

	/**
	 * Release external storage and sets the length to 0. The caller
	 * is assumed to already have taken ownership of the allocated
	 * storage and must eventually release it using 'op_free'.
	 */
	void ReleaseStorage();
};

#endif // !MODULES_UTIL_BYTEARRAY_H
