/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4; c-file-style:"stroustrup" -*-
 *
 * Copyright (C) 1995-2010 Opera Software AS.  All rights reserved.
 *
 * This file is part of the Opera web browser.  It may not be distributed
 * under any circumstances.
 *
 * @author Anders Karlsson (pugo)
 */

#ifndef SFNT_DATA_TYPES_H
#define SFNT_DATA_TYPES_H

#ifdef VEGA_POSTSCRIPT_PRINTING


class SFNTDataTypes
{
public:
	struct Fixed
	{
		Fixed() : integer(0), fraction(0) {}
		Fixed(UINT16 _integer, UINT16 _fraction) : integer(_integer), fraction(_fraction) {}
		Fixed(UINT32 _value) { Set(_value); }
		void Set(UINT32 _value) { integer = _value >> 16; fraction = _value & 65535; }
		UINT16 integer;
		UINT16 fraction;
	};

	struct LongHorizontalMetric
	{
		UINT16	advance_width;
		INT16 left_side_bearing;
	};

	static UINT8 getByte(const UINT8* source) { return *source; }
	static UINT16 getUShort(const UINT8* source) { return getValue16(source); }
	static INT16 getShort(const UINT8* source) { return getValue16(source); }
	static UINT32 getUInt24(const UINT8* source) { return getValue32(source, 3); }
	static UINT32 getULong(const UINT8* source) { return getValue32(source); }
	static INT32 getLong(const UINT8* source) { return getValue32(source); }
	static Fixed getFixed(const UINT8* source) { return Fixed(getValue16(source), getValue16(source + 2)); }

	static void setUShort(UINT8* source, UINT16 value) { return setValue16(source, value); }

private:
	static UINT32 getValue32(const UINT8* src, const size_t src_len = 4)
	{
		UINT32 value = 0;
		op_memcpy(&value + (4 - src_len), src, src_len);
		return op_ntohl(value);
	}

	static UINT16 getValue16(const UINT8* src)
	{
		UINT16 value;
		op_memcpy(&value, src, 2);
		return op_ntohs(value);
	}

	static void setValue16(UINT8* src, UINT16 value)
	{
		UINT16 _value = op_htons(value);
		op_memcpy(src, &_value, 2);
	}
};

#endif // VEGA_POSTSCRIPT_PRINTING

#endif // SFNT_DATA_TYPES_H
