/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2000-2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
**
*/

#include "core/pch.h"

#ifdef DATASTREAM_BITARRAY

#include "modules/datastream/fl_lib.h"
#include "modules/datastream/fl_bitarray.h"

#define ONES 0xffffffff

void DataStream_BitArray::AddBitsL(uint32 value, uint32 bits)
{
	OP_ASSERT(bits > 0 && bits <= 32);

	// mask out the overflowing bits
	value &= ONES >> (32 - bits);
	OP_ASSERT(bits == 32 || (value < (UINT32)(1 << bits)));

	if (bits > m_bits_left)
	{
		// first fill up the existing byte
		bits -= m_bits_left;
		uint32 fill_bits = value >> bits;
		value &= ONES >> (32 - bits);

		m_current_bytes[0] |= fill_bits;

		uint32 cur_byte = 1;
		// write all remaining full bytes
		while (bits >= 8)
		{
			bits -= 8;
			m_current_bytes[cur_byte++] = value >> bits;
			value &= ONES >> (32 - bits);
		}

		// flush all our full bytes and reset buffer
		AddContentL(m_current_bytes, cur_byte);

		m_bits_left = 8;
		m_initialize = 0;
	}

	// and put the remaining bits in the current_byte
	m_bits_left -= bits;
	value <<= m_bits_left;
	m_current_bytes[0] |= value;

	if (m_bits_left == 0)  // flush the byte
	{
		AddContentL(m_current_bytes, 1);
		m_initialize = 0;
		m_bits_left = 8;
	}
}

void DataStream_BitArray::Add1sL(uint32 number_of_ones)
{
	OP_ASSERT(number_of_ones <= 32);

	uint32 value = ONES >> (32 - number_of_ones);
	AddBitsL(value, number_of_ones);
}

void DataStream_BitArray::PadAndFlushL() {
	if (m_bits_left == 8)  // everything already flushed
		return;

	AddContentL(m_current_bytes, 1);
	m_initialize = 0;
	m_bits_left = 8;
}

#endif // DATASTREAM_BITARRAY
