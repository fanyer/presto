/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 1995-2004 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
*/

#ifndef UTIL_OP_BUFFER_H
#define UTIL_OP_BUFFER_H

//********************************************************************
//
//	OpBuffer
//
//********************************************************************

/*! A growable buffer class for storing data of any new-able type. */

template <class T>
class OpBuffer
{
public:
	// Constructor / destructor.
	OpBuffer();
	virtual ~OpBuffer() { Empty(); }

	OP_STATUS Init(UINT initial_size = 0);

	// Methods.
	OP_STATUS Append(T *data, UINT data_size);
	OP_STATUS Remove(UINT data_size);		// remove bytes from the beginning of the buffer
	T *Buffer() { return m_buffer; }
	T const *Buffer() const { return m_buffer; }
	void Empty();
	BOOL HasContent() const { return m_data_size > 0; }
	BOOL IsEmpty() const { return m_data_size == 0; }
	OP_STATUS Resize(UINT new_size);
	OP_STATUS ShrinkToFit();
	UINT BufferSize() const { return m_buffer_size; } // Sice of the internal buffer.
	UINT DataSize() const { return m_data_size; } // Size of the data we have stored with Append().

	OP_STATUS SetDataSize(UINT new_size);	// will resize and modify m_data_size too, so data can be copied directly into the buffer without using Append().

private:
	// Methods.
	OP_STATUS GrowIfNeeded(UINT desired_size);

	// Members.
	T *m_buffer;
	UINT m_buffer_size;
	UINT m_data_size;
};


//********************************************************************
//
//	OpByteBuffer
//
//********************************************************************

class OpByteBuffer
:	public OpBuffer<unsigned char>
{
public:
	OpByteBuffer() : OpBuffer<unsigned char>() { }
};


//********************************************************************
//
//	OpBuffer
//
//********************************************************************

template <class T>
OpBuffer<T>::OpBuffer()
:	m_buffer(0),
	m_buffer_size(0),
	m_data_size(0)
{
}


template <class T>
OP_STATUS OpBuffer<T>::Init(UINT initial_size)
{
	return GrowIfNeeded(initial_size);
}


template <class T>
OP_STATUS OpBuffer<T>::Append(T *data, UINT data_size)
{
	RETURN_IF_ERROR(GrowIfNeeded(m_data_size + data_size));

	memcpy(m_buffer + m_data_size, data, data_size);
	m_data_size += data_size;

	return OpStatus::OK;
}

template <class T>
OP_STATUS OpBuffer<T>::Remove(UINT data_size)
{
	// remove data at the _beginning_ of the buffer
	
	if (data_size > m_data_size || data_size == 0 ) return OpStatus::OK;	// silently ignore this
	m_data_size -= data_size;
	memmove(m_buffer, m_buffer + data_size, m_data_size);

	return OpStatus::OK;
}


template <class T>
void OpBuffer<T>::Empty()
{
	if (m_buffer != 0)
		OP_DELETEA(m_buffer);

	m_buffer = 0;
	m_buffer_size = 0;
	m_data_size = 0;
}


template <class T>
OP_STATUS OpBuffer<T>::Resize(UINT new_size)
{
	// Create a new buffer of the size we need.
	T* new_buffer = OP_NEWA(T, new_size);
	if (new_buffer == 0)
		return OpStatus::ERR_NO_MEMORY;

	m_buffer_size = new_size;

	// Copy the old content into the new buffer, then delete it.
	if (m_data_size > 0)
	{
		memcpy(new_buffer, m_buffer, m_data_size);
		OP_DELETEA(m_buffer);
	}

	m_buffer = new_buffer;
	return OpStatus::OK;
}

template <class T>
OP_STATUS OpBuffer<T>::SetDataSize(UINT new_size)
{
	OP_STATUS s = Resize(new_size);
	if(OpStatus::IsSuccess(s))
	{
		m_data_size = m_buffer_size;
	}
	return s;
}

template <class T>
OP_STATUS OpBuffer<T>::ShrinkToFit()
{
	if (m_buffer_size > m_data_size)
		return Resize(m_data_size);

	return OpStatus::OK;
}


template <class T>
OP_STATUS OpBuffer<T>::GrowIfNeeded(UINT desired_size)
{
	if (desired_size > m_buffer_size)
	{
		const UINT new_size = max(m_buffer_size * 2, desired_size);
		return Resize(new_size);
	}

	return OpStatus::OK;
}

#endif
