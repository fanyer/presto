/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#ifndef FL_BITARRAY_H
#define FL_BITARRAY_H

#ifdef DATASTREAM_BITARRAY
/** Class for writing a stream of bits.  The bytes in the stream will
 *  be written in network byte order (big endian).
 * 
 *  Since only full bytes can be eventually returned the caller should make
 *  sure that the total number of bits added is a multiple of 8, or call
 *  PadAndFlushL at the end to pad the last byte with 0 bits.
 */
class DataStream_BitArray : public DataStream_ByteArray_Base
{
public:
	DataStream_BitArray() : m_initialize(0), m_bits_left(8) {}
	~DataStream_BitArray() {}

	/** Adds the value to the buffer using the specified number of bits.
	 *  It is important that the value actually fits in the specified number
	 *  of bits.  */
	void AddBitsL(uint32 value, uint32 bits);
	
	/** Add the specified number of zero bits to the buffer */
	void Add0sL(uint32 number_of_zeros) { AddBitsL(0, number_of_zeros); }
	
	/** Add the specified number of one bits to the buffer */
	void Add1sL(uint32 number_of_ones);

	// Pads remaining bits with 0 and flush all bits to the byte array
	void PadAndFlushL();
private:
	union {
		byte		m_current_bytes[4];  //< byte buffer used to contruct bytes from the incoming bits // ARRAY OK 2009-04-22 yngve
		uint32		m_initialize;        //< just to ease initalizing of the entire byte array
	};

	uint32 			m_bits_left;    ///< bits left unused in m_current_bytes[0]
};
#endif // DATASTREAM_BITARRAY
#endif /*FL_BITARRAY_H */
