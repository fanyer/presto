/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
**
** Jan Borsodi
*/

#ifndef OP_PROTOBUF_H
#define OP_PROTOBUF_H

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_wireformat.h"

class OpString16;
typedef OpString16 OpString;

// Uncomment the next line to enable 64bit support in Protocol Buffer
#define OP_PROTOBUF_64BIT_SUPPORT
#define OP_PROTOBUF_64BIT_PROTO_SUPPORT

// Currently the 64bit code is not finished for JSON/XML
// The reason is parsing of 64bit unsigned values.
// #define OP_PROTOBUF_64BIT_JSON_SUPPORT
// #define OP_PROTOBUF_64BIT_XML_SUPPORT

// Check whether the compiler has issues with defining static const members
// which are initialized in the header file.
// This should be supported by all compilers but there is a problem on certain
// versions of Visual Studio
//
// Example of static const initialization
// .h
// struct X {
//     static const SomeEnum value = EnumValue;
// }
// .cpp
// const X::value;
//
// The line in the .cpp will not work for VS 9.0 or lower as it has already
// defined it when it was initialized.
#ifdef _MSC_VER
#else
#		define OP_PROTO_STATIC_CONST_DEFINITION
#endif

/**
 * Alternative to offsetof()/op_offsetof() which works for non-POD classes.
 * GCC gives warnings for usage of offsetof() on non-POD classes, it looks like:
 *   warning: invalid access to non-static data member ‘...::...’ of NULL object
 *   warning: (perhaps the ‘offsetof’ macro was used incorrectly)
 * 
 * The classes that we generate are quite simple and we don't use any advanced
 * inheritance so the field offset are constant.
 * We avoid the error this by using a non-NULL value for calculating the address.
 * A value of 16 should avoid any alignment issues.
 * This is a similar approach as in Google Protocol Buffer.
 */
#define OP_PROTO_OFFSETOF(TYPE, FIELD) \
	( reinterpret_cast<const char *>( &reinterpret_cast<const TYPE *>(16)->FIELD ) \
	- reinterpret_cast<const char *>(16))

class OpProtobufStringChunkRange;
class OpProtobufDataChunkRange;

struct OpProtobufFormat
{
	enum FieldType
	{
		// TODO: Float/double support should perphaps be discouraged since JSON does not support it, at least discourage it for multi-format setups
		            // Required   Optional   Repeated
		  Double=1	// double     double     OpValueVector<double>
		, Float		// float      float      OpValueVector<float>
		, Int32		// INT32      INT32      OpValueVector<INT32>
		, Uint32	// UINT32     UINT32     OpValueVector<UINT32>
		, Sint32	// INT32      INT32      OpValueVector<INT32>
		, Fixed32	// UINT32     UINT32     OpValueVector<UINT32>
		, Sfixed32	// INT32      INT32      OpValueVector<INT32>
		, Bool		// BOOL       BOOL       OpINT32Vector
		, String	// OpString   OpString   OpVector<OpString>
		, Bytes		// ByteBuffer ByteBuffer OpVector<ByteBuffer>
		, Message	// void *     void *     OpProtobufRepeatedItems
#ifdef OP_PROTOBUF_64BIT_SUPPORT
		, Int64		// INT64      INT64      OpValueVector<INT64>
		, Uint64	// UINT64     UINT64     OpValueVector<UINT64>
		, Sint64	// INT64      INT64      OpValueVector<INT64>
		, Fixed64	// UINT64     UINT64     OpValueVector<UINT64>
		, Sfixed64	// INT64      INT64      OpValueVector<INT64>
#endif // OP_PROTOBUF_64BIT_SUPPORT
	};

	/**
	 * Converts a FieldType to the type which will be used in the wire format.
	 */
	static inline OpProtobufWireFormat::WireType ToWireType(FieldType proto);

	enum TypeSize
	{
		// Byte count for FieldType types which have fixed sizes
		  Fixed32Size  = 4
		, Sfixed32Size = 4
		, DoubleSize   = 8
		, FloatSize    = 4
		, BoolSize     = 1
#ifdef OP_PROTOBUF_64BIT_SUPPORT
		, Fixed64Size  = 8
		, Sfixed64Size = 8
#endif // OP_PROTOBUF_64BIT_SUPPORT
	};

	static inline int Int32Size(INT32 num);
	static inline int Uint32Size(UINT32 num);
	static inline int Sint32Size(INT32 num);
#ifdef OP_PROTOBUF_64BIT_SUPPORT
	static inline int Int64Size(INT64 num);
	static inline int Uint64Size(UINT64 num);
	static inline int Sint64Size(INT64 num);
#endif // OP_PROTOBUF_64BIT_SUPPORT

	static int StringDataSize(OpProtobufStringChunkRange range);
	static int StringSize(OpProtobufStringChunkRange range);
	static int StringSize(const OpString &str, int char_count = -1);
	static int StringSize(const uni_char *str, int char_count = -1);
	static int BytesSize(const OpProtobufDataChunkRange &range);
};

/*static*/ inline
int
OpProtobufFormat::Int32Size(INT32 num)
{
	return OpProtobufWireFormat::VarIntSize32(num);
}

/*static*/ inline
int
OpProtobufFormat::Uint32Size(UINT32 num)
{
	return OpProtobufWireFormat::VarIntSize32(num);
}

/*static*/ inline
int
OpProtobufFormat::Sint32Size(INT32 num)
{
	return OpProtobufWireFormat::VarIntSize32(num);
}

#ifdef OP_PROTOBUF_64BIT_SUPPORT
/*static*/ inline
int
OpProtobufFormat::Int64Size(INT64 num)
{
	return OpProtobufWireFormat::VarIntSize64(num);
}

/*static*/ inline
int
OpProtobufFormat::Uint64Size(UINT64 num)
{
	return OpProtobufWireFormat::VarIntSize64(num);
}

/*static*/ inline
int
OpProtobufFormat::Sint64Size(INT64 num)
{
	return OpProtobufWireFormat::VarIntSize64(num);
}
#endif // OP_PROTOBUF_64BIT_SUPPORT

/*static*/ inline
OpProtobufWireFormat::WireType
OpProtobufFormat::ToWireType(FieldType type)
{
	switch (type)
	{
	case Double:
#ifdef OP_PROTOBUF_64BIT_SUPPORT
	case Fixed64:
	case Sfixed64:
#endif // OP_PROTOBUF_64BIT_SUPPORT
		return OpProtobufWireFormat::Fixed64;
	case Float:
	case Fixed32:
	case Sfixed32:
		return OpProtobufWireFormat::Fixed32;
	case Int32:
	case Uint32:
	case Sint32:
	case Bool:
#ifdef OP_PROTOBUF_64BIT_SUPPORT
	case Int64:
	case Uint64:
	case Sint64:
#endif // OP_PROTOBUF_64BIT_SUPPORT
		return OpProtobufWireFormat::VarInt;
	case String:
	case Bytes:
	case Message:
		return OpProtobufWireFormat::LengthDelimited;
	default:
		OP_ASSERT(!"Should not happen");
		return OpProtobufWireFormat::VarInt;
	}
}

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_H
