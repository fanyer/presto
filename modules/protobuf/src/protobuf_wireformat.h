/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_PROTOBUF_WIREFORMAT_H
#define OP_PROTOBUF_WIREFORMAT_H

#ifdef PROTOBUF_SUPPORT

class ByteBuffer;

// TODO: Use BOOL as return value for functions which either succeed or fail, e.g DecodeVarInt32
// TODO: Decide on "const char *" vs "const unsigned char *"
class OpProtobufWireFormat
{
public:
	enum
	{
		Bits = 3, //< Number of bits reserved for the wire type
		Mask = (1 << Bits) - 1 //< Bitmask which covers bits for the wire type
	};

	enum WireType
	{
		// The types supported by the wire format, the number is used directly in the binary format
		VarInt           = 0,
		Fixed64          = 1,
		LengthDelimited  = 2,
		StartGroup       = 3, // Not supported, deprecated in Protocol Buffers
		EndGroup         = 4, // Not supported, deprecated in Protocol Buffers
		Fixed32          = 5,
	};

	OpProtobufWireFormat() {}

	inline static int FieldHeaderSize(WireType wire_type, int number);
	inline static int VarIntSize32(UINT32 num);
	inline static int VarIntSize64(UINT64 num);

	inline static OP_STATUS EncodeVarInt32(char *data, int n, int &len, UINT32 num);
	inline static OP_STATUS EncodeVarInt64(char *data, int n, int &len, UINT64 num);
	inline static OP_STATUS EncodeFixed32(char *data, int n, UINT32 num);
	inline static OP_STATUS EncodeFixed64(char *data, int n, UINT64 num);

	inline static UINT32 EncodeField32(int field, int number);
	inline static UINT64 EncodeField64(int field, int number);
	inline static UINT32 ZigZag32(INT32 number);
	inline static UINT64 ZigZag64(INT64 number);

	inline static OP_STATUS DecodeVarInt32(const char *data, const char * const end, int &len, INT32 &num);
	inline static OP_STATUS DecodeVarInt32(const unsigned char *data, const unsigned char * const end, int &len, INT32 &num);
	inline static OP_STATUS DecodeVarInt64(const char *data, const char * const end, int &len, INT64 &num);
	inline static OP_STATUS DecodeVarInt64(const unsigned char *data, const unsigned char * const end, int &len, INT64 &num);
	inline static OP_STATUS DecodeFixed32(const char *data, UINT32 &num);
	inline static OP_STATUS DecodeFixed32(const unsigned char *data, UINT32 &num);
	inline static OP_STATUS DecodeFixed64(const char *data, UINT64 &num);
	inline static OP_STATUS DecodeFixed64(const unsigned char *data, UINT64 &num);

	inline static INT32 UnZigZag32(UINT32 number);
	inline static INT64 UnZigZag64(UINT64 number);
	inline static int DecodeFieldType32(UINT32 field);
	inline static int DecodeFieldType64(UINT64 field);
	inline static int DecodeFieldNumber32(UINT32 field);
	inline static int DecodeFieldNumber64(UINT64 field);

};

/*static*/ inline
int
OpProtobufWireFormat::FieldHeaderSize(WireType wire_type, int number)
{
	return VarIntSize64(EncodeField64(wire_type, number));
}

/*static*/ inline
int
OpProtobufWireFormat::VarIntSize32(UINT32 num)
{
	return VarIntSize64(num);
}

/*static*/ inline
int
OpProtobufWireFormat::VarIntSize64(UINT64 num)
{
	if (num <= 0x7f)
		return 1;
	int size = 0;
	do
	{
		++size;
		num >>= 7;
		OP_ASSERT(size <= 10);
	}
	while (num > 0);
	return size;
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::EncodeVarInt32(char *data, int n, int &len, UINT32 num)
{
	return EncodeVarInt64(data, n, len, num);
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::EncodeVarInt64(char *data, int n, int &len, UINT64 num)
{
	if (num == 0)
	{
		if (n < 1)
			return OpStatus::ERR;
		data[0] = '\0';
		len = 1;
		return OpStatus::OK;
	}
	int i = 0;
	do
	{
		if (i >= 10 || i >= n)
			return OpStatus::ERR;
		UINT8 b = (UINT8)(num & 0x7f);
		num >>= 7;
		if (num > 0)
			b |= 0x80;
		data[i] = b;
		++i;
	}
	while (num > 0);
	len = i;
	return OpStatus::OK;
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeVarInt32(const char *data, const char * const data_end, int &len, INT32 &num)
{
	return DecodeVarInt32(reinterpret_cast<const unsigned char *>(data), reinterpret_cast<const unsigned char * const>(data_end), len, num);
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeVarInt32(const unsigned char *start, const unsigned char * const end, int &len, INT32 &num)
{
	const unsigned char *it = start;
	if (it == end)
	{
		len = 0;
		return OpStatus::ERR;
	}
	unsigned char c = *it++;
	if ((c & 0x80) != 0x80)
	{
		num = static_cast<int>(c);
		len = 1;
		return OpStatus::OK;
	}
	int v = static_cast<int>(c & 0x7f), shift = 7;
	for (; it != end; shift += 7)
	{
		if (shift > 31)
			break;
		c = *it++;
		if (c & 0x80)
			v |= static_cast<int>(c & 0x7f) << shift;
		else
		{
			v |= static_cast<int>(c) << shift;
			num = v;
			len = it - start;
			return OpStatus::OK;
		}
	}
	// We have received a varint larger than 5 bytes, read and discard remaining bytes (up to 5 more).
	for (; it != end; shift += 7)
	{
		if (shift > 63) // Must not exceed 64 bit boundary
			break;
		c = *it++;
		if (!(c & 0x80))
		{
			num = v;
			len = it - start;
			return OpStatus::OK;
		}
	}
	len = it - start;
	return OpStatus::ERR;
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeVarInt64(const char *data, const char * const data_end, int &len, INT64 &num)
{
	return DecodeVarInt64(reinterpret_cast<const unsigned char *>(data), reinterpret_cast<const unsigned char * const>(data_end), len, num);
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeVarInt64(const unsigned char *start, const unsigned char * const end, int &len, INT64 &num)
{
	const unsigned char *it = start;
	if (it == end)
	{
		len = 0;
		return OpStatus::ERR;
	}
	unsigned char c = *it++;
	if ((c & 0x80) != 0x80)
	{
		num = static_cast<INT64>(c);
		len = 1;
		return OpStatus::OK;
	}
	INT64 v = static_cast<INT64>(c & 0x7f), shift = 7;
	for (; it != end; shift += 7)
	{
		if (shift > 63)
			break;
		c = *it++;
		if (c & 0x80)
			v |= static_cast<INT64>(c & 0x7f) << shift;
		else
		{
			v |= static_cast<INT64>(c) << shift;
			num = v;
			len = it - start;
			return OpStatus::OK;
		}
	}
	len = it - start;
	return OpStatus::ERR;
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::EncodeFixed32(char *data, int n, UINT32 num)
{
	// TODO: Optimize with proper le/be code.
	if (n < 4)
		return OpStatus::ERR;
	unsigned char *d = reinterpret_cast<unsigned char *>(data);
	d[0] = (num&0xff);
	d[1] = ((num>>8)&0xff);
	d[2] = ((num>>16)&0xff);
	d[3] = ((num>>24)&0xff);
	return OpStatus::OK;
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::EncodeFixed64(char *data, int n, UINT64 num)
{
	// TODO: Optimize with proper le/be code.
	if (n < 8)
		return OpStatus::ERR;
	unsigned char *d = reinterpret_cast<unsigned char *>(data);
	UINT32 part_a = static_cast<UINT32>(num);
	UINT32 part_b = static_cast<UINT32>(num >> 32);
	d[0] = (part_a&0xff);
	d[1] = ((part_a>>8)&0xff);
	d[2] = ((part_a>>16)&0xff);
	d[3] = ((part_a>>24)&0xff);
	d[4] = (part_b&0xff);
	d[5] = ((part_b>>8)&0xff);
	d[6] = ((part_b>>16)&0xff);
	d[7] = ((part_b>>24)&0xff);
	return OpStatus::OK;
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeFixed32(const char *data, UINT32 &num)
{
	// TODO: Optimize with proper le/be code.
	return DecodeFixed32(reinterpret_cast<const unsigned char *>(data), num);
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeFixed32(const unsigned char *udata, UINT32 &num)
{
	// TODO: Optimize with proper le/be code.
	num =    udata[0]
		  | (udata[1] << 8)
		  | (udata[2] << 16)
		  | (udata[3] << 24);
	return OpStatus::OK;
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeFixed64(const char *data, UINT64 &num)
{
	// TODO: Optimize with proper le/be code.
	return DecodeFixed64(reinterpret_cast<const unsigned char *>(data), num);
}

/*static*/ inline
OP_STATUS
OpProtobufWireFormat::DecodeFixed64(const unsigned char *udata, UINT64 &num)
{
	// TODO: Optimize with proper le/be code.
	UINT32 a =    udata[0]
			   | (udata[1] << 8)
			   | (udata[2] << 16)
			   | (udata[3] << 24);
	UINT32 b =    udata[4]
			   | (udata[5] << 8)
			   | (udata[6] << 16)
			   | (udata[7] << 24);
	num = (static_cast<UINT64>(b) << 32) | a;
	return OpStatus::OK;
}

/*static*/ inline
UINT32
OpProtobufWireFormat::EncodeField32(int field, int number)
{
	return number << Bits | field;
}

/*static*/ inline
UINT64
OpProtobufWireFormat::EncodeField64(int field, int number)
{
	return number << Bits | field;
}

/*static*/ inline
UINT32
OpProtobufWireFormat::ZigZag32(INT32 number)
{
	if (number < 0)
		return (-number) << 1 | 1;
	else
		return number << 1;
}

/*static*/ inline
UINT64
OpProtobufWireFormat::ZigZag64(INT64 number)
{
	if (number < 0)
		return (-number) << 1 | 1;
	else
		return number << 1;
}

/*static*/ inline
INT32
OpProtobufWireFormat::UnZigZag32(UINT32 number)
{
	if (number & 1)
		return -static_cast<INT32>(number >> 1);
	else
		return number >> 1;
}

/*static*/ inline
INT64
OpProtobufWireFormat::UnZigZag64(UINT64 number)
{
	if (number & 1)
		return -static_cast<INT64>(number >> 1);
	else
		return number >> 1;
}

/*static*/ inline
int
OpProtobufWireFormat::DecodeFieldType32(UINT32 field)
{
	return field & Mask;
}

/*static*/ inline
int
OpProtobufWireFormat::DecodeFieldType64(UINT64 field)
{
	return static_cast<int>(field & Mask);
}

/*static*/ inline
int
OpProtobufWireFormat::DecodeFieldNumber32(UINT32 field)
{
	return field >> Bits;
}

/*static*/ inline
int
OpProtobufWireFormat::DecodeFieldNumber64(UINT64 field)
{
	return static_cast<int>(field >> Bits);
}

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_WIREFORMAT_H
