/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2010 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#include "core/pch.h"

#include "modules/util/bytearray.h"

OP_STATUS
ByteArray::EnsureCapacity(unsigned long new_requested_len)
{
	if (new_requested_len <= m_allocated)
		return OpStatus::OK;

	// Allocating anything smaller than 16 bytes isn't likely
	// to save any memory. Otherwise make sure we at least
	// double the size to get good performance from growing.
	unsigned long new_size = MAX(m_allocated * 2, 16);
	if (new_size < new_requested_len)
		new_size = new_requested_len;

	char* new_array = static_cast<char*>(op_realloc(m_byte_array, new_size));
	if (!new_array)
		return OpStatus::ERR_NO_MEMORY;

	m_byte_array = new_array;
	m_allocated = new_size;

	return OpStatus::OK;
}

OP_STATUS
ByteArray::Append(const char* data, unsigned long len)
{
	if (m_length + len < m_length) // Overflow, wrapping.
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(EnsureCapacity(m_length + len));

	op_memcpy(m_byte_array + m_length, data, len);
	m_length += len;
	return OpStatus::OK;
}


OP_STATUS
ByteArray::AppendZeroes(unsigned long len)
{
	if (m_length + len < m_length) // Overflow, wrapping.
		return OpStatus::ERR_NO_MEMORY;

	RETURN_IF_ERROR(EnsureCapacity(m_length + len));

	op_memset(m_byte_array + m_length, 0, len);
	m_length += len;
	return OpStatus::OK;
}

void
ByteArray::FreeStorage()
{
	op_free(m_byte_array);
	m_byte_array = NULL;
	m_length = 0;
	m_allocated = 0;
}

void
ByteArray::ReleaseStorage()
{
	m_byte_array = NULL;
	m_length = 0;
	m_allocated = 0;
}
