/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
 *
 * Copyright (C) 1995-2007 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.	It may not be distributed
 * under any circumstances.
 *
 * @author Arjan van Leeuwen (arjanl)
 */

#ifndef STREAM_BUFFER_H
#define STREAM_BUFFER_H

/**
 * @brief A generic stream buffer class
 *
 * Use this if you want to append data to a buffer multiple times.
 *
 * Example for a uni_char* buffer:
 *
 * Declaration: StreamBuffer<uni_char> buffer
 * Append data: buffer.Append(string, uni_strlen(string))
 *
 */

class GenericStreamBuffer
{
protected:
	GenericStreamBuffer();
	~GenericStreamBuffer();

	OP_STATUS AppendInternal(const char* data, size_t datasize, size_t length);
	OP_STATUS ReserveInternal(size_t capacity);
	void*	  ReleaseInternal();
	void	  ResetInternal();
	void	  TakeOverInternal(GenericStreamBuffer& buffer);
	void	  TakeOverInternal(char*& buffer, size_t length);

	char*  m_buffer;
	size_t m_filled;
	size_t m_capacity;
};

template<class T>
class StreamBuffer : public GenericStreamBuffer
{
public:
	StreamBuffer() {}

	/** Append data to the buffer - data will be terminated with 0 after appending
	  * @param data Data to append
	  * @param length Length of data to append
	  */
	OP_STATUS Append(const T* data, size_t length)
		{ return AppendInternal(reinterpret_cast<const char*>(data), sizeof(T), length); }

	/** Get the data that is in the buffer
	  */
	const T*  GetData() const
		{ return reinterpret_cast<T*>(m_buffer); }

	/** Reserve a number of items in the buffer
	  * Call this if you expect the total length of your buffer to not exceed a certain size
	  * NB: There is no need to reserve an extra item for string terminators!
	  * @param capacity Number of items to reserve
	  */
	OP_STATUS Reserve(size_t capacity)
		{ return ReserveInternal(sizeof(T) * (capacity + 1)); }

	/** Release the data from the buffer (data no longer owned by this buffer)
	  * @return The released data
	  */
	T*		  Release()
		{ return static_cast<T*>(ReleaseInternal()); }

	/** Get the current capacity of the buffer
	  */
	size_t	  GetCapacity() const
		{ return (m_capacity / sizeof(T)) - 1; }

	/** Get the current fill-rate of the buffer
	  */
	size_t	  GetFilled() const
		{ return m_filled / sizeof(T); }

	/** Reset the buffer
	  */
	void	  Reset()
		{ return ResetInternal(); }

	/** Take over the data contained in another buffer
	  * @param buffer Buffer to steal data from. This buffer will be empty
	  * after execution of this function.
	  */
	void	  TakeOver(StreamBuffer<T>& buffer)
		{ TakeOverInternal(buffer); }

	/** Take over the data contained in another buffer
	  * @param buffer Buffer to steal data from. The buffer should be allocated
	  * using NEWA(). The 'buffer' pointer will be invalid and should not be
	  * dereferenced after this method returns.  The variable will also be set
	  * to 0 by this method.
	  * @param length size of buffer in bytes
	  */
	void	  TakeOver(char*& buffer, size_t length)
		{ TakeOverInternal(buffer, length); }

private:
	StreamBuffer(const StreamBuffer<T>& nocopy);
	StreamBuffer& operator=(const StreamBuffer<T>& nocopy);
};

/** Make an OpString from a string-based StreamBuffer
  * Executes in constant time (ie. not dependent on string length)
  * @param buffer Buffer to convert (will release its data)
  * @param string Where to put the converted string
  */
void StreamBufferToOpString(StreamBuffer<uni_char>& buffer, OpString& string);
void StreamBufferToOpString8(StreamBuffer<char>& buffer, OpString8& string);

#endif // STREAM_BUFFER_H
