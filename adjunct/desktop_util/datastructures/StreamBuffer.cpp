/* -*- Mode: c++; tab-width: 4; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.
 * It may not be distributed under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#include "core/pch.h"

#include "adjunct/desktop_util/datastructures/StreamBuffer.h"


/***********************************************************************************
 ** Constructor
 **
 ** GenericStreamBuffer::GenericStreamBuffer
 ***********************************************************************************/
GenericStreamBuffer::GenericStreamBuffer()
  : m_buffer(0)
  , m_filled(0)
  , m_capacity(0)
{
}


/***********************************************************************************
 ** Destructor
 **
 ** GenericStreamBuffer::~GenericStreamBuffer
 ***********************************************************************************/
GenericStreamBuffer::~GenericStreamBuffer()
{
	OP_DELETEA(m_buffer);
}


/***********************************************************************************
 ** Append data to the buffer - data will be terminated with 0 after appending
 **
 ** GenericStreamBuffer::AppendInternal
 ** @param data Data to append
 ** @param datasize Size of one data item
 ** @param length Length of data to append
 ***********************************************************************************/
OP_STATUS GenericStreamBuffer::AppendInternal(const char* data, size_t datasize, size_t length)
{
	// Make sure there's enough space
	size_t needed = m_filled + datasize * (length + 1);

	if (m_capacity < needed)
	{
		// Be aggressive, reserve twice the size
		RETURN_IF_ERROR(ReserveInternal(needed * 2));
	}

	// Append to the buffer
	op_memcpy(m_buffer + m_filled, data, datasize * length);
	m_filled += datasize * length;

	// Terminate with a 0
	op_memset(m_buffer + m_filled, 0, datasize);

	return OpStatus::OK;
}


/***********************************************************************************
 ** Reserve data in the buffer
 **
 ** GenericStreamBuffer::AppendInternal
 ** @param data Data to append
 ** @param datasize Size of one data item
 ** @param length Length of data to append
 ***********************************************************************************/
OP_STATUS GenericStreamBuffer::ReserveInternal(size_t capacity)
{
	if (capacity <= m_capacity)
		return OpStatus::OK;

	// Create new buffer
	char* new_buffer = OP_NEWA(char, capacity);
	if (!new_buffer)
		return OpStatus::ERR_NO_MEMORY;

	// Copy contents of old buffer to new buffer
	if (m_buffer)
		op_memcpy(new_buffer, m_buffer, m_capacity);

	// Switch buffers
	OP_DELETEA(m_buffer);
	m_buffer   = new_buffer;
	m_capacity = capacity;

	return OpStatus::OK;
}


/***********************************************************************************
 ** Release the data in the buffer, and return the released data
 **
 ** GenericStreamBuffer::ReleaseInternal
 ** @return The data that was released
 ***********************************************************************************/
void* GenericStreamBuffer::ReleaseInternal()
{
	void* data = m_buffer;

	m_buffer   = 0;
	m_capacity = 0;
	m_filled   = 0;

	return data;
}


/***********************************************************************************
 ** Reset the buffer
 **
 ** GenericStreamBuffer::ResetInternal
 ***********************************************************************************/
void GenericStreamBuffer::ResetInternal()
{
	OP_DELETEA(m_buffer);
	ReleaseInternal();
}

/***********************************************************************************
 ** Take over the data of another buffer
 **
 ** GenericStreamBuffer::TakeOverInternal
 ***********************************************************************************/
void GenericStreamBuffer::TakeOverInternal(GenericStreamBuffer& buffer)
{
	ResetInternal();

	m_buffer = buffer.m_buffer;
	m_capacity = buffer.m_capacity;
	m_filled = buffer.m_filled;

	buffer.ReleaseInternal();
}

/***********************************************************************************
 ** Take over the data of a char buffer
 **
 ** GenericStreamBuffer::TakeOverInternal
 ***********************************************************************************/
void GenericStreamBuffer::TakeOverInternal(char*& buffer, size_t length)
{
	ResetInternal();
	m_buffer = buffer;
	m_capacity = length;
	m_filled = length;

	buffer = 0;
}


/***********************************************************************************
 ** OpString helper functions
 **
 **
 ***********************************************************************************/
template<class BufferType, class OpStringType>
void StreamBufferToOpStringTemplate(BufferType& buffer, OpStringType& string)
{
	class StringConverter : public OpStringType
	{
	public:
		StringConverter(BufferType& buffer)
		{
			this->iSize = buffer.GetCapacity();
			this->iBuffer = buffer.Release();
		}
	} converter(buffer);

	string.TakeOver(converter);
}

void StreamBufferToOpString(StreamBuffer<uni_char>& buffer, OpString& string)
{
	return StreamBufferToOpStringTemplate(buffer, string);
}

void StreamBufferToOpString8(StreamBuffer<char>& buffer, OpString8& string)
{
	return StreamBufferToOpStringTemplate(buffer, string);
}
