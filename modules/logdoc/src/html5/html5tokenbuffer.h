/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style: "stroustrup" -*-
 *
 * Copyright (C) 1995-2011 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 */

#ifndef HTML5TOKENBUFFER_H
#define HTML5TOKENBUFFER_H

#include "modules/logdoc/src/html5/html5base.h"

class HTML5TokenBuffer
{
public:
	HTML5TokenBuffer(unsigned size) : m_buffer(NULL), m_used(0), m_buffer_length(0), m_initial_size(size), m_hash_value(0), m_hash_base(0) {}
	~HTML5TokenBuffer() { OP_DELETEA(m_buffer); }

	void	FreeBuffer()
	{
		m_buffer_length = m_used = 0;
		OP_DELETEA(m_buffer);
		m_buffer = NULL;
	}

	/**
	 * Clear buffer and reset buffer size.
	 */
	void	ResetL();

	/**
	 * Clear the buffer content but leave the buffer as-is
	 */
	void	Clear()
	{
		m_used = 0;
		if (m_buffer)
			*m_buffer = 0;
		m_hash_value = m_hash_base;
	}

	/**
	 * Check if the buffer is large enough or try to allocate enough
	 */
	void	EnsureCapacityL(unsigned added_length)
	{
		if (m_used + added_length > m_buffer_length)
			GrowL(added_length);
	}

	/**
	 * Takes over the memory of the other buffer (and sets its buffers to NULL)
	 * Used when resizing attribute array and we just want to transfer the attributes
	 * from one array to the other
	 */
	void	TakeOver(HTML5TokenBuffer &take_over_from);

	/**
	 * Deep copy
	 */
	void	CopyL(const HTML5TokenBuffer *copy_from);

	/**
	 * Append the character c without a size check. Caller needs to
	 * call EnsureCapacityL() before appending.
	 */
	void	Append(const uni_char c)
	{
		m_buffer[m_used++] = c;
	}

	/**
	 * Append the character c with a size check.
	 */
	void	AppendL(const uni_char c)
	{
		EnsureCapacityL(1);
		m_buffer[m_used++] = c;
	}

	/**
	 * As Append() above but will also calculate the running
	 * hash value for the string.
	 */
	void	AppendAndHashL(const uni_char c)
	{
		EnsureCapacityL(1);
		m_buffer[m_used++] = c;
		HTML5_HASH_FUNCTION(m_hash_value, c);
	}

	/**
	 * Append a string.
	 */
	void	AppendL(const uni_char *str, unsigned str_len);

	/**
	 * Append a string and calculate the hash value.
	 */
	void	AppendAndHashL(const uni_char *str, unsigned str_len)
	{
		EnsureCapacityL(str_len);
		for (unsigned i = 0; i < str_len; i++)
		{
			m_buffer[m_used++] = *str;
			HTML5_HASH_FUNCTION(m_hash_value, *str);
			str++;
		}
	}

	/**
	 * Zero terminate the value.
	 */
	void	TerminateL()
	{
		EnsureCapacityL(1);
		m_buffer[m_used] = 0;
	}

	/**
	 * Checks if a given zero-terminated string is equal to the
	 * contents of this buffer.
	 * @param str Pointer to the start of the string.
	 * @param str_len The length, in uni_chars, of the string.
	 * @returns TRUE if the buffer value is equal to the zero
	 * terminated str
	 */
	BOOL	IsEqualTo(const uni_char *str, unsigned str_len) const
	{
		OP_ASSERT(str);
		return str_len == m_used && uni_strncmp(str, m_buffer, m_used) == 0;
	}

	unsigned	Length() const { return m_used; }

	const uni_char*	GetBuffer() const { return m_buffer; }

	void	SetHashBase(UINT32 base) { m_hash_value = m_hash_base = base; }
	void	SetHashValue(UINT32 value) { m_hash_value = value; }
	UINT32	GetHashValue() const { return m_hash_value; }

private:
	static const unsigned kChunkSize = 1024;

	uni_char*	m_buffer; /// buffer to store the text
	unsigned	m_used; /// length of text
	unsigned	m_buffer_length; /// total length of buffer
	unsigned	m_initial_size; /// initial buffer length
	UINT32		m_hash_value; /// pre-calculated hash value
	UINT32		m_hash_base; /// base value for hashing

	void	GrowL(unsigned added_length);

	/**
	 * HACK!! DO NOT USE UNLESS YOU ARE ABSOLUTELY SURE OF WHAT YOU DO!!
	 * Used by a swapping method to avoid deleting the buffer
	 * of the swapped values when the temporary variable is deleted.
	 */
	void	ReleaseBuffer()
	{
		Clear();
		m_buffer = NULL;
	}
};

#endif // HTML5TOKENBUFFER_H
