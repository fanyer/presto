/* -*- Mode: c++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*-
**
** Copyright (C) 2008 Opera Software AS.  All rights reserved.
**
** This file is part of the Opera web browser.  It may not be distributed
** under any circumstances.
*/

#ifndef OP_PROTOBUF_TYPES_H
#define OP_PROTOBUF_TYPES_H

#ifdef PROTOBUF_SUPPORT

#include "modules/protobuf/src/protobuf_wireformat.h"
#include "modules/util/opstring.h"
#include "modules/protobuf/src/opvaluevector.h"
#include "modules/util/adt/opvector.h"

// Typedefs for all types in OpProtobufFormat::FieldType (except Message)

struct OpProtobufRequiredType
{
	typedef double     Double;
	typedef float      Float;
	typedef INT32      Int32;
	typedef UINT32     Uint32;
	typedef Int32      Sint32;
	typedef Uint32     Fixed32;
	typedef Int32      Sfixed32;
	typedef BOOL       Bool;
	typedef OpString   String;
	typedef ByteBuffer Bytes;
#ifdef OP_PROTOBUF_64BIT_SUPPORT
	typedef INT64      Int64;
	typedef UINT64     Uint64;
	typedef Int64      Sint64;
	typedef Uint64     Fixed64;
	typedef Int64      Sfixed64;
#endif // OP_PROTOBUF_64BIT_SUPPORT
};

struct OpProtobufOptionalType
{
	typedef double     Double;
	typedef float      Float;
	typedef INT32      Int32;
	typedef UINT32     Uint32;
	typedef Int32      Sint32;
	typedef Uint32     Fixed32;
	typedef Int32      Sfixed32;
	typedef BOOL       Bool;
	typedef OpString   String;
	typedef ByteBuffer Bytes;
#ifdef OP_PROTOBUF_64BIT_SUPPORT
	typedef INT64      Int64;
	typedef UINT64     Uint64;
	typedef Int64      Sint64;
	typedef Uint64     Fixed64;
	typedef Int64      Sfixed64;
#endif // OP_PROTOBUF_64BIT_SUPPORT
};

struct OpProtobufRepeatedType
{
	typedef OpValueVector<double> Double;
	typedef OpValueVector<float>  Float;
	typedef OpValueVector<INT32>  Int32;
	typedef OpValueVector<UINT32> Uint32;
	typedef OpValueVector<Int32>  Sint32;
	typedef OpValueVector<Uint32> Fixed32;
	typedef OpValueVector<Int32>  Sfixed32;
	typedef OpINT32Vector         Bool;
	typedef OpAutoVector<OpString>    String;
	typedef OpAutoVector<ByteBuffer>  Bytes;
#ifdef OP_PROTOBUF_64BIT_SUPPORT
	typedef OpValueVector<INT64>  Int64;
	typedef OpValueVector<UINT64> Uint64;
	typedef OpValueVector<Int64>  Sint64;
	typedef OpValueVector<Uint64> Fixed64;
	typedef OpValueVector<Int64>  Sfixed64;
#endif // OP_PROTOBUF_64BIT_SUPPORT
};

#endif // PROTOBUF_SUPPORT

#endif // OP_PROTOBUF_TYPES_H
